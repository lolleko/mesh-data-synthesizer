// Fill out your copyright notice in the Description page of Project Settings.

#include "MTWayGraphSamplerComponent.h"

#include "CesiumFlyToComponent.h"
#include "MeshSynth/OSM/MTOverpassConverter.h"
#include "MeshSynth/OSM/MTOverpassQuery.h"
#include "MeshSynth/WayGraph/MTChinesePostMan.h"
#include "GlobeAwareDefaultPawn.h"
#include "JsonDomBuilder.h"
#include "MTSample.h"
#include "Kismet/GameplayStatics.h"
#include "GenericPlatform/GenericPlatformApplicationMisc.h"

UMTWayGraphSamplerComponent::UMTWayGraphSamplerComponent()
{
    WayGraphVisualizerComponent =
        CreateDefaultSubobject<UMTWayGraphVisualizerComponent>(TEXT("WayGraphVisualizer"));
    WayGraphVisualizerComponent->SetupAttachment(this);
}

int32 UMTWayGraphSamplerComponent::GetEstimatedSampleCount()
{
    return EstimatedSampleCount;
}

void UMTWayGraphSamplerComponent::BeginPlay()
{
    Super::BeginPlay();

    WayGraphVisualizerComponent->OnEulerTourAnimationCompleted().AddUObject(
        this, &UMTWayGraphSamplerComponent::EulerTourAnimationCompleted);
    
    if (ShouldSampleOnBeginPlay())
    {
        if (!ensure(IsValid(BoundingPolygon)))
        {
            return;
        }
        SamplePolygon(BoundingPolygon, false);
    }
}

void UMTWayGraphSamplerComponent::SamplePolygon(ACesiumCartographicPolygon* Polygon, const bool bIgnoreCache)
{
    check(Polygon);

    BoundingPolygon = Polygon;

    auto ControlRotation =
        GetWorld()->GetGameInstance()->GetPrimaryPlayerController()->GetControlRotation();

    // Pitch looking donwn for path visualization TODO snap view to poly
    ControlRotation.Pitch = -90.;
    GetWorld()->GetGameInstance()->GetPrimaryPlayerController()->SetControlRotation(
        ControlRotation);

    const auto PlayerFOV = UGameplayStatics::GetPlayerCameraManager(this, 0)->GetFOVAngle();

    FBoxSphereBounds PolygonBounds = BoundingPolygon->Polygon->Bounds;
    FVector PlayerPosition = PolygonBounds.Origin;
    const auto CameraDistance =
        PolygonBounds.SphereRadius * 2. / FMath::Tan(FMath::DegreesToRadians(PlayerFOV) / 2.);
    PlayerPosition.Z = CameraDistance + PolygonBounds.Origin.Z + PolygonBounds.BoxExtent.Z / 2.;

    GetWorld()->GetGameInstance()->GetPrimaryPlayerController()->GetPawn()->SetActorLocation(
        PlayerPosition);

    if (!bIgnoreCache && FPaths::FileExists(GetStreetDataCacheFilePath()))
    {
        FString StreetDataJSONString;
        FFileHelper::LoadFileToString(StreetDataJSONString, *GetStreetDataCacheFilePath());
        FJsonObjectConverter::JsonObjectStringToUStruct(StreetDataJSONString, &StreetData);

        WayGraphVisualizerComponent->StartVisualization(
            BoundingPolygon,
            StreetData,
            8.,
            EMTWaygraphVisualizerMode::Animated,
            EMTWaygraphVisualizerBoundaryMode::Visible);
    }
    else
    {
        OverpassQueryCompletedDelegate.BindUFunction(this, TEXT("OverpassQueryCompleted"));
        const auto OverpassQuery = MTOverpass::BuildQueryStringFromBoundingPolygon(BoundingPolygon);
        MTOverpass::AsyncQuery(OverpassQuery, OverpassQueryCompletedDelegate);
    }
}

TOptional<FTransform> UMTWayGraphSamplerComponent::SampleNextLocation()
{
    const auto* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());

    if (CurrentPathSegmentIndex == ViewCurrentPath().Num() - 1)
    {
        // GOTO next path or finish
        CurrentPathIndex++;
        CurrentPathSegmentIndex = 0;
        CurrentPathSegmentStartDistance = 0.;
        CurrentPathDistance = 0.;

        if (CurrentPathIndex > StreetData.Paths.Num() - 1)
        {
            EndSampling();
            return {};
        }

        CurrentSampleLocation = StreetData.Graph.GetNodeLocationUnreal(
            ViewCurrentPath()[CurrentPathSegmentIndex], GeoRef);
    }

    PrevSampleLocation = CurrentSampleLocation;
    PrevWayIndex = CurrentWayIndex;

    FQuat CurrentEdgeDir;

    auto SegmentEndDistance = CurrentPathSegmentStartDistance;

    const auto NextSampleDistance = CurrentPathDistance + GetActiveConfig()->SampleDistance;

    for (; CurrentPathSegmentIndex < ViewCurrentPath().Num() - 1; ++CurrentPathSegmentIndex)
    {
        const auto StartPoint = StreetData.Graph.GetNodeLocationUnreal(
            ViewCurrentPath()[CurrentPathSegmentIndex], GeoRef);

        const auto EndPoint = StreetData.Graph.GetNodeLocationUnreal(
            ViewCurrentPath()[CurrentPathSegmentIndex + 1], GeoRef);

        const auto SegmentLength = FVector::Dist(StartPoint, EndPoint);

        CurrentPathSegmentStartDistance = SegmentEndDistance;
        SegmentEndDistance += SegmentLength;

        if (NextSampleDistance <= SegmentEndDistance)
        {
            const auto Alpha = (SegmentEndDistance - NextSampleDistance) /
                               (SegmentEndDistance - CurrentPathSegmentStartDistance);

            CurrentSampleLocation = FMath::Lerp(StartPoint, EndPoint, 1 - Alpha);

            CurrentWayIndex = StreetData.Graph.GetEdgeWay(StreetData.Graph.NodePairToEdgeIndex(
                ViewCurrentPath()[CurrentPathSegmentIndex],
                ViewCurrentPath()[CurrentPathSegmentIndex + 1]));

            CurrentEdgeDir = (EndPoint - StartPoint).Rotation().Quaternion();

            break;
        }

        CurrentSampleLocation = EndPoint;
    }

    CurrentPathDistance = NextSampleDistance;

    const auto SampleLocationDuplicationGrid = FVector(
        FMath::Floor(CurrentSampleLocation.X / GetActiveConfig()->GetMinDistanceBetweenSamples()),
        FMath::Floor(CurrentSampleLocation.Y / GetActiveConfig()->GetMinDistanceBetweenSamples()),
        0.);

    if (SampledLocationsLSH.Contains({SampleLocationDuplicationGrid, CurrentWayIndex}))
    {
        return {};
    }
    else
    {
        const auto PrevSampleLocationDuplicationGrid = FVector(
            FMath::Floor(PrevSampleLocation.X / GetActiveConfig()->GetMinDistanceBetweenSamples()),
            FMath::Floor(PrevSampleLocation.Y / GetActiveConfig()->GetMinDistanceBetweenSamples()),
            0.);

        SampledLocationsLSH.Add({SampleLocationDuplicationGrid, CurrentWayIndex});
        SampledLocationsLSH.Add({PrevSampleLocationDuplicationGrid, CurrentWayIndex});
        SampledLocationsLSH.Add({SampleLocationDuplicationGrid, PrevWayIndex});

        // TODO deduplicate with CollectSampleMetadata
        FString SampleName = FString::Format(
            TEXT("{0}-{1}-{2}-{3}"),
            {FMath::RoundToInt64(CurrentSampleLocation.X * 1000),
             FMath::RoundToInt64(CurrentSampleLocation.Y * 1000),
             FMath::RoundToInt64(CurrentSampleLocation.Z * 1000),
             CurrentWayIndex});

        const auto RelativeFileName =
            FPaths::Combine("./Images", FString::Format(TEXT("{0}.jpg"), {SampleName}));

        const auto AbsoluteImageFilePath =
            FPaths::ConvertRelativePathToFull(GetSessionDir(), RelativeFileName);

        // Assume we are resuming previous run and don't overwrite image or metadata
        if (FPaths::FileExists(AbsoluteImageFilePath))
        {
            return {};
        }

        return {FTransform(CurrentEdgeDir, CurrentSampleLocation)};
    }
}

TOptional<FTransform> UMTWayGraphSamplerComponent::ValidateSampleLocation()
{
    return ValidateGroundAndObstructions();
}

FMTSample UMTWayGraphSamplerComponent::CollectSampleMetadata()
{
    const auto* Georeference = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());

    const auto SampleLonLat =
        Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(GetComponentLocation());
    const auto EastSouthUp = GetComponentRotation();
    const auto HeadingAngle = FRotator::ClampAxis(EastSouthUp.Yaw + 90.);
    const auto Pitch = FRotator::ClampAxis(EastSouthUp.Pitch + 90.);

    const auto StreetName = StreetData.Graph.GetWayName(CurrentWayIndex);
    
    return {GetActiveConfig()->OutputDirectory, {}, {}, HeadingAngle, Pitch, EastSouthUp.Roll, SampleLonLat, StreetName, 0.};
}

void UMTWayGraphSamplerComponent::InitSamplingParameters()
{
    EstimatedSampleCount = (StreetData.TotalPathLength / GetActiveConfig()->SampleDistance);

    const auto* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());

    CurrentPathSegmentIndex = 0;
    CurrentPathSegmentStartDistance = 0.;
    CurrentPathDistance = 0.;
    CurrentPathIndex = 0;
    CurrentWayIndex = 0;
    SampledLocationsLSH.Reset();
    CurrentSampleLocation = StreetData.Graph.GetNodeLocationUnreal(ViewCurrentPath()[0], GeoRef);
}

void UMTWayGraphSamplerComponent::OverpassQueryCompleted(
    const FOverPassQueryResult& Result,
    const bool bSuccess)
{
    if (!bSuccess)
    {
        return;
    }

    StreetData.Graph = MTOverpass::CreateStreetGraphFromQuery(Result, BoundingPolygon);

    const auto* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());
    StreetData.Paths =
        FMTChinesePostMan::CalculatePathsThatContainAllEdges(StreetData.Graph, GeoRef);

    for (int32 PathIndex = 0; PathIndex < StreetData.Paths.Num(); ++PathIndex)
    {
        for (int32 PathNodeIndex = 0; PathNodeIndex < StreetData.Paths[PathIndex].Nodes.Num() - 1;
             ++PathNodeIndex)
        {
            const auto StartLocation = StreetData.Graph.GetNodeLocationUnreal(
                StreetData.Paths[PathIndex].Nodes[PathNodeIndex], GeoRef);
            const auto EndLocation = StreetData.Graph.GetNodeLocationUnreal(
                StreetData.Paths[PathIndex].Nodes[PathNodeIndex + 1], GeoRef);

            StreetData.TotalPathLength += FVector::Dist(StartLocation, EndLocation);
        }
    }

    FString StreetDataJSONString;
    FJsonObjectConverter::UStructToJsonObjectString(StreetData, StreetDataJSONString);
    FFileHelper::SaveStringToFile(StreetDataJSONString, *GetStreetDataCacheFilePath());

    WayGraphVisualizerComponent->StartVisualization(
        BoundingPolygon,
        StreetData,
        8.,
        EMTWaygraphVisualizerMode::Animated,
        EMTWaygraphVisualizerBoundaryMode::Visible);
}

void UMTWayGraphSamplerComponent::EulerTourAnimationCompleted()
{
    WayGraphVisualizerComponent->ClearVisualization();

    const auto PlayerPawn = CastChecked<AGlobeAwareDefaultPawn>(
        GetWorld()->GetGameInstance()->GetPrimaryPlayerController()->GetPawn());

    const auto StartPointCoords = StreetData.Graph.GetNodeLocation(StreetData.Paths[0].Nodes[0]);

    const auto* GeoRef = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());

    const auto StartPoint = StreetData.Graph.GetNodeLocationUnreal(StreetData.Paths[0].Nodes[0], GeoRef);

    const auto EndPoint = StreetData.Graph.GetNodeLocationUnreal(StreetData.Paths[0].Nodes[1], GeoRef);

    const auto EdgeRotation = (EndPoint - StartPoint).Rotation();

    auto* FlyTo = PlayerPawn->FindComponentByClass<UCesiumFlyToComponent>();
    FlyTo->Duration = 5.;
    PlayerPawn->FlyToLocationLongitudeLatitudeHeight(
        FVector{StartPointCoords.Lon, StartPointCoords.Lat, 83.}, EdgeRotation.Yaw, 10., false);

    FlyTo->OnFlightComplete.AddDynamic(
        this, &UMTWayGraphSamplerComponent::FlyToLocationCompleted);
}

void UMTWayGraphSamplerComponent::FlyToLocationCompleted()
{
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(
        TimerHandle, this, &UMTWayGraphSamplerComponent::AfterFlyPauseIsOver, 3.f);
}

void UMTWayGraphSamplerComponent::AfterFlyPauseIsOver()
{
    InitSamplingParameters();
    BeginSampling();
}

TConstArrayView<int32> UMTWayGraphSamplerComponent::ViewCurrentPath()
{
    return StreetData.Paths[CurrentPathIndex].Nodes;
}

FString UMTWayGraphSamplerComponent::GetStreetDataCacheFilePath() const
{
    return FPaths::Combine(GetSessionDir(), TEXT("StreetDataCache.json"));
}

void UMTWayGraphSamplerComponent::CopyBoundingPolygonToClipboard()
{
    FJsonDomBuilder::FArray PolygonArray;

    if (BoundingPolygon)
    {
        const auto SampledPolygon =
            BoundingPolygon->CreateCartographicPolygon(FTransform::Identity);
        for (const auto& VertexCoord : SampledPolygon.getVertices())
        {
            FJsonDomBuilder::FObject CordObj;
            CordObj.Set(TEXT("Lon"), FMath::RadiansToDegrees(VertexCoord.x));
            CordObj.Set(TEXT("Lat"), FMath::RadiansToDegrees(VertexCoord.y));

            PolygonArray.Add(CordObj);
        }
    }

    const auto BoundingPolygonStr = PolygonArray.ToString<>();

    FGenericPlatformApplicationMisc::ClipboardCopy(*BoundingPolygonStr);
}
