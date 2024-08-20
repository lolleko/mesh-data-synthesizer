// Fill out your copyright notice in the Description page of Project Settings.

#include "MTWayGraphVisualizerComponent.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "MTChinesePostMan.h"
#include "MeshSynth/OSM/MTOverpassConverter.h"
#include "MeshSynth/OSM/MTOverpassQuery.h"

UMTWayGraphVisualizerComponent::UMTWayGraphVisualizerComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    PathISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PathISMComponent"));
    PathISMComponent->SetupAttachment(this);
    PathISMComponent->NumCustomDataFloats = 3;

    BoundaryISMComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BoundaryISMComponent"));
    BoundaryISMComponent->SetupAttachment(this);
    BoundaryISMComponent->NumCustomDataFloats = 3;

    EulerTourCursorMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EulerTourCursorMeshComponent"));
    EulerTourCursorMeshComponent->SetupAttachment(this);
    EulerTourCursorMeshComponent->SetHiddenInGame(true);
    EulerTourCursorMeshComponent->SetWorldScale3D({100, 100, 100});
}

void UMTWayGraphVisualizerComponent::ShowOverview()
{
    const auto* Georeference = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());

    const FBoxSphereBounds StaticMeshBounds = PathISMComponent->GetStaticMesh()->GetBounds();
    const auto MeshSizeInX = StaticMeshBounds.BoxExtent.Dot(FVector::XAxisVector) * 1.95;

    TArray<int32> EdgeMeshInstanceIds;
    EdgeMeshInstanceIds.Reserve(StreetData.Graph.EdgeNum());

    TArray<FTransform> EdgeMeshTransforms;
    EdgeMeshTransforms.Reserve(StreetData.Graph.EdgeNum());

    TArray<FTransform> EdgeMeshPrevTransforms;
    EdgeMeshPrevTransforms.Reserve(StreetData.Graph.EdgeNum());

    TArray<float> EdgeMeshCustomFloats;
    EdgeMeshPrevTransforms.Reserve(StreetData.Graph.EdgeNum() * PathISMComponent->NumCustomDataFloats);

    for (int32 Node1 = 0; Node1 < StreetData.Graph.NodeNum(); Node1++)
    {
        for (int32 Node2 = Node1 + 1; Node2 < StreetData.Graph.NodeNum(); Node2++)
        {
            if (StreetData.Graph.AreNodesConnected(Node1, Node2))
            {
                const auto EdgeIndex = StreetData.Graph.NodePairToEdgeIndex(Node1, Node2);
                const auto EdgeWayIndex = 0;  // WayGraph.GetEdgeWay(EdgeIndex);

                const auto EdgeStartVector = StreetData.Graph.GetNodeLocationUnreal(Node1, Georeference);
                const auto EdgeEndVector = StreetData.Graph.GetNodeLocationUnreal(Node2, Georeference);

                const auto EdgeSize = FVector::Dist(EdgeStartVector, EdgeEndVector);
                const auto EdgeRotation =
                    (EdgeEndVector - EdgeStartVector).GetSafeNormal().Rotation();

                const auto MeshScale = EdgeSize / MeshSizeInX;

                const auto Transform = FTransform(
                    EdgeRotation,
                    EdgeStartVector,
                    FVector(
                        MeshScale,
                        EMTWayToScale(StreetData.Graph.GetWayKind(EdgeWayIndex)) * 2.F,
                        1.F));
                const auto InstanceID = EdgeMeshTransforms.Add(Transform);
                EdgeMeshPrevTransforms.Add(Transform);

                EdgeMeshInstanceIds.Add(InstanceID);

                FRandomStream StreetRandom(
                    0 /*WayGraph.GetEdgeWay(InstanceIndexToEdgeIndex[InstanceIndex])*/);

                const auto Hue = StreetRandom.RandRange(0, 255);
                const auto StreetColor =
                    FColor(240, 159, 0)
                        .ReinterpretAsLinear();  // FLinearColor::MakeFromHSV8(Hue, 255, 255);

                EdgeMeshCustomFloats.Append({StreetColor.R, StreetColor.G, StreetColor.B});
            }
        }
    }

    PathISMComponent->UpdateInstances(
        EdgeMeshInstanceIds,
        EdgeMeshTransforms,
        EdgeMeshPrevTransforms,
        PathISMComponent->NumCustomDataFloats,
        EdgeMeshCustomFloats);

    PathISMComponent->MarkRenderStateDirty();
}

void UMTWayGraphVisualizerComponent::ShowEulerTour()
{
    const auto* Georeference = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());

    const FBoxSphereBounds StaticMeshBounds = PathISMComponent->GetStaticMesh()->GetBounds();
    const auto MeshSizeInX = StaticMeshBounds.BoxExtent.Dot(FVector::XAxisVector) * 2.;

    TArray<int32> EdgeMeshInstanceIds;
    EdgeMeshInstanceIds.Reserve(StreetData.Graph.EdgeNum());

    TArray<FTransform> EdgeMeshTransforms;
    EdgeMeshTransforms.Reserve(StreetData.Graph.EdgeNum());

    TArray<FTransform> EdgeMeshPrevTransforms;
    EdgeMeshPrevTransforms.Reserve(StreetData.Graph.EdgeNum());

    TArray<float> EdgeMeshCustomFloats;
    EdgeMeshPrevTransforms.Reserve(StreetData.Graph.EdgeNum() * PathISMComponent->NumCustomDataFloats);

    FVector ZOffset = FVector::ZeroVector;
    for (int32 PathIndex = 0; PathIndex < StreetData.Paths.Num(); ++PathIndex)
    {
        for (int32 PathNodeIndex = 0; PathNodeIndex < StreetData.Paths[PathIndex].Nodes.Num() - 1;
             ++PathNodeIndex)
        {
            const auto Node1 = StreetData.Paths[PathIndex].Nodes[PathNodeIndex];
            const auto Node2 = StreetData.Paths[PathIndex].Nodes[PathNodeIndex + 1];

            const auto EdgeIndex = StreetData.Graph.NodePairToEdgeIndex(Node1, Node2);
            const auto EdgeWayIndex = 0;  // WayGraph.GetEdgeWay(EdgeIndex);

            const auto EdgeStartVector = StreetData.Graph.GetNodeLocationUnreal(Node1, Georeference);
            const auto EdgeEndVector = StreetData.Graph.GetNodeLocationUnreal(Node2, Georeference);

            const auto EdgeSize = FVector::Dist(EdgeStartVector, EdgeEndVector);
            const auto EdgeRotation = (EdgeEndVector - EdgeStartVector).GetSafeNormal().Rotation();

            const auto MeshScale = EdgeSize / MeshSizeInX;

            const auto Transform = FTransform(
                EdgeRotation,
                EdgeStartVector + ZOffset,
                FVector(MeshScale, EMTWayToScale(StreetData.Graph.GetWayKind(EdgeWayIndex)) * 2.F, 1.F));
            const auto InstanceID = EdgeMeshTransforms.Add(Transform);
            EdgeMeshPrevTransforms.Add(Transform);

            EdgeMeshInstanceIds.Add(InstanceID);

            const auto ColorRange = FMath::GetRangePct(
                0.,
                static_cast<double>(StreetData.Paths[PathIndex].Nodes.Num() - 1),
                static_cast<double>(PathNodeIndex));
            const auto StreetColor = FLinearColor(2.0f * ColorRange, 2.0f * (1 - ColorRange), 0);

            EdgeMeshCustomFloats.Append({StreetColor.R, StreetColor.G, StreetColor.B});

            ZOffset += FVector(0., 0., 2.);
        }
    }

    PathISMComponent->UpdateInstances(
        EdgeMeshInstanceIds,
        EdgeMeshTransforms,
        EdgeMeshPrevTransforms,
        PathISMComponent->NumCustomDataFloats,
        EdgeMeshCustomFloats);

    PathISMComponent->MarkRenderStateDirty();
}

void UMTWayGraphVisualizerComponent::ShowEulerTourAnimated()
{
    const auto* Georeference = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());

    EulerTourCursorMeshComponent->SetHiddenInGame(false);

    AddNextEulerTourMesh();
}

void UMTWayGraphVisualizerComponent::ShowBoundary()
{
    const FBoxSphereBounds StaticMeshBounds = PathISMComponent->GetStaticMesh()->GetBounds();
    const auto MeshSizeInX = StaticMeshBounds.BoxExtent.Dot(FVector::XAxisVector) * 2.;

    // Create spline mesh components for BoundingPolygon->Polygon
    for (int32 SplineSegmentIndex = 0;
         SplineSegmentIndex < BoundingPolygon->Polygon->GetNumberOfSplineSegments();
         ++SplineSegmentIndex)
    {
        const auto EdgeStartVector = BoundingPolygon->Polygon->GetLocationAtSplinePoint(
            SplineSegmentIndex, ESplineCoordinateSpace::World);
        const auto EdgeEndVector = BoundingPolygon->Polygon->GetLocationAtSplinePoint(
            SplineSegmentIndex + 1, ESplineCoordinateSpace::World);

        const auto EdgeSize = FVector::Dist(EdgeStartVector, EdgeEndVector);
        const auto EdgeRotation = (EdgeEndVector - EdgeStartVector).GetSafeNormal().Rotation();

        const auto MeshScale = EdgeSize / MeshSizeInX;

        const auto Transform =
            FTransform(EdgeRotation, EdgeStartVector, FVector(MeshScale, 5.F, 1.F));

        const auto StreetColor = FLinearColor::Blue;

        const int32 Instance = BoundaryISMComponent->AddInstance(Transform, true);
        BoundaryISMComponent->SetCustomData(Instance, {StreetColor.R, StreetColor.G, StreetColor.B});
    }
}

void UMTWayGraphVisualizerComponent::ClearVisualization()
{
    PathISMComponent->ClearInstances();
    BoundaryISMComponent->ClearInstances();
}

void UMTWayGraphVisualizerComponent::StartVisualization(
    ACesiumCartographicPolygon* InBoundingPolygon,
    const FMTStreetData& InStreetData,
    const double Duration,
    const EMTWaygraphVisualizerMode Mode,
    const EMTWaygraphVisualizerBoundaryMode BoundaryMode)
{
    BoundingPolygon = InBoundingPolygon;
    StreetData = InStreetData;

    PathISMComponent->SetStaticMesh(PathMesh);
    BoundaryISMComponent->SetStaticMesh(BoundaryMesh);
    EulerTourCursorMeshComponent->SetStaticMesh(CursorMesh);

    if (BoundaryMode == EMTWaygraphVisualizerBoundaryMode::Visible)
    {
        ShowBoundary();
    }

    if (Mode == EMTWaygraphVisualizerMode::Animated)
    {
        EulerTourSegmentsPerTick = StreetData.Graph.EdgeNum() / Duration / 60; // Assume 60 fps
        FTimerHandle TimerHandle;
        GetWorld()->GetTimerManager().SetTimer(
            TimerHandle, this, &UMTWayGraphVisualizerComponent::ShowEulerTourAnimated, 3.f);
    }
    else
    {
        ShowOverview();
    }
}


void UMTWayGraphVisualizerComponent::BeginPlay()
{
    Super::BeginPlay();
}


void UMTWayGraphVisualizerComponent::AddNextEulerTourMesh()
{
    for (int32 I = 0; EulerAnimationCurrentNodeIndex <
                          StreetData.Paths[EulerAnimationCurrentPathIndex].Nodes.Num() - 1 &&
                      I < EulerTourSegmentsPerTick;
         ++I)
    {
        const auto* Georeference = ACesiumGeoreference::GetDefaultGeoreference(GetWorld());

        const FBoxSphereBounds StaticMeshBounds = PathISMComponent->GetStaticMesh()->GetBounds();
        const auto MeshSizeInX = StaticMeshBounds.BoxExtent.Dot(FVector::XAxisVector) * 2.;

        const auto Node1 =
            StreetData.Paths[EulerAnimationCurrentPathIndex].Nodes[EulerAnimationCurrentNodeIndex];
        const auto Node2 = StreetData.Paths[EulerAnimationCurrentPathIndex]
                               .Nodes[EulerAnimationCurrentNodeIndex + 1];

        const auto EdgeIndex = StreetData.Graph.NodePairToEdgeIndex(Node1, Node2);
        const auto EdgeWayIndex = 0;  // WayGraph.GetEdgeWay(EdgeIndex);

        const auto EdgeStartVector = StreetData.Graph.GetNodeLocationUnreal(Node1, Georeference);
        const auto EdgeEndVector = StreetData.Graph.GetNodeLocationUnreal(Node2, Georeference);

        const auto EdgeSize = FVector::Dist(EdgeStartVector, EdgeEndVector);
        const auto EdgeRotation = (EdgeEndVector - EdgeStartVector).GetSafeNormal().Rotation();

        const auto MeshScale = EdgeSize / MeshSizeInX;

        const auto Height = BoundingPolygon->Polygon->Bounds.Origin.Z + 1000.;
        const auto HeightOffset = FVector(0., 0., Height);

        const auto EdgeStartVectorWithHeight =
            FVector(EdgeStartVector.X, EdgeStartVector.Y, HeightOffset.Z + EulerAnimationZOffset.Z);

        const auto Transform = FTransform(
            EdgeRotation,
            EdgeStartVectorWithHeight,
            FVector(MeshScale, EMTWayToScale(StreetData.Graph.GetWayKind(EdgeWayIndex)) * 5.F, 1.F));

        const auto ColorRange = FMath::GetRangePct(
            0.,
            static_cast<double>(StreetData.Paths[EulerAnimationCurrentPathIndex].Nodes.Num() - 1),
            static_cast<double>(EulerAnimationCurrentNodeIndex));
        const auto StreetColor = FLinearColor(2.0f * ColorRange, 2.0f * (1 - ColorRange), 0);

        const int32 Instance = PathISMComponent->AddInstance(Transform, true);
        PathISMComponent->SetCustomData(Instance, {StreetColor.R, StreetColor.G, StreetColor.B});

        EulerAnimationCurrentNodeIndex++;

        EulerAnimationZOffset += FVector(0., 0., 0.001);

        const auto EdgeEndVectorWithHeight =
            FVector(EdgeEndVector.X, EdgeEndVector.Y, HeightOffset.Z + EulerAnimationZOffset.Z);

        EulerTourCursorMeshComponent->SetWorldLocation(EdgeEndVectorWithHeight);
    }

    if (EulerAnimationCurrentNodeIndex >=
        StreetData.Paths[EulerAnimationCurrentPathIndex].Nodes.Num() - 1)
    {
        EulerAnimationCurrentNodeIndex = 0;
        EulerAnimationCurrentPathIndex++;
        if (EulerAnimationCurrentPathIndex < StreetData.Paths.Num())
        {
            GetWorld()->GetTimerManager().SetTimerForNextTick(
                this, &UMTWayGraphVisualizerComponent::AddNextEulerTourMesh);
        } else
        {
            // Done
            EulerTourCursorMeshComponent->SetHiddenInGame(true);
            OnEulerTourAnimationCompleted().Broadcast();
        }
    }
    else
    {
        GetWorld()->GetTimerManager().SetTimerForNextTick(
            this, &UMTWayGraphVisualizerComponent::AddNextEulerTourMesh);
    }
}