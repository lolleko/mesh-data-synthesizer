// Fill out your copyright notice in the Description page of Project Settings.

#include "MTReSamplerComponent.h"

#include "CesiumCartographicPolygon.h"
#include "GeomTools.h"
#include "MTSamplingFunctionLibrary.h"

void UMTReSamplerComponent::BeginPlay()
{
    Super::BeginPlay();

    if (ShouldSampleOnBeginPlay())
    {
        switch (DatasetType)
        {
            case EMTDatasetType::SFXL_CSV:
                Locations = UMTSamplingFunctionLibrary::PanoramaLocationsFromCosPlaceCSV(
                    FilePath, ACesiumGeoreference::GetDefaultGeoreference(GetWorld()));
                break;
            case EMTDatasetType::PITTS_TXT:
                Locations = UMTSamplingFunctionLibrary::SliceLocationsFromPittsTXT(
                    FilePath, ACesiumGeoreference::GetDefaultGeoreference(GetWorld()));
                break;
            case EMTDatasetType::TOKYO_TXT:
                Locations = UMTSamplingFunctionLibrary::SliceLocationsFromTokyoTXT(
                    FilePath, ACesiumGeoreference::GetDefaultGeoreference(GetWorld()));
                break;
        }

        if (IsValid(BoundingPolygon))
        {
            BoundingPolygon->Polygon->ConvertSplineToPolyLine(
                ESplineCoordinateSpace::World, 1., BoundingPolyline);
            BoundingPolyline2D.Reserve(BoundingPolyline.Num());
            for (const auto& Vert : BoundingPolyline)
            {
                BoundingPolyline2D.Add(FVector2D(Vert.X, Vert.Y));
            }

            BoundingPolyline2D.RemoveAtSwap(BoundingPolyline2D.Num() - 1);

            BoundingBox2D = FBox2d(BoundingPolyline2D);
        }

        CurrentSampleIndex = INDEX_NONE;

        // Initial data cleanup
        // Remove locations that are outside of the bounding polygon
        // Also remove locations that already have been rendered
        for (int32 I = 0; I < Locations.Num(); ++I)
        {
            const auto SampleLocation = Locations[I].Location;

            const auto TestPoint2D =
                FVector2D(SampleLocation.GetLocation().X, SampleLocation.GetLocation().Y);
            if (IsValid(BoundingPolygon) &&
                (!BoundingBox2D.IsInsideOrOn(TestPoint2D) ||
                 !FGeomTools2D::IsPointInPolygon(TestPoint2D, BoundingPolyline2D)))
            {
                Locations.RemoveAtSwap(I);
                I--;
                continue;
            }

            const auto AbsoluteFileName =
                FPaths::Combine(GetSessionDir(), "Images", Locations[I].Path);

            if (FPaths::FileExists(AbsoluteFileName))
            {
                Locations.RemoveAtSwap(I);
                I--;
                continue;
            }
        }

        switch (DatasetType)
        {
            case EMTDatasetType::PITTS_TXT:
                TogglePanoramaCapture(false);
                ChangeCapture2DResolution({640, 480}, 60);
                break;
            case EMTDatasetType::TOKYO_TXT:
                TogglePanoramaCapture(false);
                ChangeCapture2DResolution({1280, 960}, 60);
                break;
        }

        InitialLocationCount = Locations.Num();

        BeginSampling();
    }
}
int32 UMTReSamplerComponent::GetEstimatedSampleCount()
{
    return InitialLocationCount;
}

TOptional<FTransform> UMTReSamplerComponent::SampleNextLocation()
{
    if (CurrentSampleIndex != INDEX_NONE)
    {
        Locations.RemoveAtSwap(CurrentSampleIndex);
    }

    if (Locations.IsEmpty())
    {
        EndSampling();
        return {};
    }

    double ClosestSampleDistance = MAX_dbl;

    for (int32 I = 0; I < Locations.Num(); ++I)
    {
        const auto SampleLocation = Locations[I].Location;

        const auto Distance =
            FVector::DistSquared(GetOwner()->GetActorLocation(), SampleLocation.GetLocation());
        if (Distance < ClosestSampleDistance)
        {
            ClosestSampleDistance = Distance;
            CurrentSampleIndex = I;
        }
    }

    return Locations[CurrentSampleIndex].Location;
}

TOptional<FTransform> UMTReSamplerComponent::ValidateSampleLocation()
{
    switch (DatasetType)
    {
        case EMTDatasetType::SFXL_CSV:
            return Locations[CurrentSampleIndex].Location;
        case EMTDatasetType::PITTS_TXT:
        case EMTDatasetType::TOKYO_TXT:
            return ValidateGroundAndObstructions(true);
    }
    return {};
}

FMTSample UMTReSamplerComponent::CollectSampleMetadata()
{
    const auto* Georeference = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());

    const auto SampleLonLat =
        Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(GetComponentLocation());
    const auto EastSouthUp = GetComponentRotation();
    const auto HeadingAngle = FRotator::ClampAxis(EastSouthUp.Yaw + 90.);
    const auto Pitch = FRotator::ClampAxis(EastSouthUp.Pitch + 90.);

    return {
        {},
        Locations[CurrentSampleIndex].Path,
        {},
        HeadingAngle,
        Pitch,
        EastSouthUp.Roll,
        SampleLonLat,
        TEXT("")};
}
