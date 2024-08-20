 #include "MTCartographicPolygonVisualizer.h"

// Sets default values
AMTCartographicPolygonVisualizer::AMTCartographicPolygonVisualizer()
{
	// Set this actor to call Tick() every frame. You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	LineISMComponent =
        CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LineISMComponent"));
    LineISMComponent->SetupAttachment(RootComponent);
    LineISMComponent->NumCustomDataFloats = 3;

    PointISMComponent =
        CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PointISMComponent"));
    PointISMComponent->SetupAttachment(RootComponent);
}

// Called when the game starts or when spawned
void AMTCartographicPolygonVisualizer::BeginPlay()
{
	Super::BeginPlay();

	LineISMComponent->SetStaticMesh(LineMesh);
    PointISMComponent->SetStaticMesh(PointMesh);
}

// Called every frame
void AMTCartographicPolygonVisualizer::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    LineISMComponent->ClearInstances();
    PointISMComponent->ClearInstances();

    const FBoxSphereBounds StaticMeshBounds = LineISMComponent->GetStaticMesh()->GetBounds();
    const auto MeshSizeInX = StaticMeshBounds.BoxExtent.Dot(FVector::XAxisVector) * 2.;

    // Create spline mesh components for BoundingPolygon->Polygon
    for (int32 SplineSegmentIndex = 0;
         SplineSegmentIndex < Polygon->GetNumberOfSplineSegments();
         ++SplineSegmentIndex)
    {
        const auto EdgeStartVector = Polygon->GetLocationAtSplinePoint(
            SplineSegmentIndex, ESplineCoordinateSpace::World);
        const auto EdgeEndVector = Polygon->GetLocationAtSplinePoint(
            SplineSegmentIndex + 1, ESplineCoordinateSpace::World);

        const auto EdgeSize = FVector::Dist(EdgeStartVector, EdgeEndVector);
        const auto EdgeRotation = (EdgeEndVector - EdgeStartVector).GetSafeNormal().Rotation();

        const auto MeshScale = EdgeSize / MeshSizeInX;

        const auto LineTransform =
            FTransform(EdgeRotation, EdgeStartVector, FVector(MeshScale, 5.F, 1.F));

        const auto LineColor = FColor::Orange.ReinterpretAsLinear();

        const int32 LineInstance = LineISMComponent->AddInstance(LineTransform, true);
        LineISMComponent->SetCustomData(
            LineInstance, {LineColor.R, LineColor.G, LineColor.B});

        const auto PointTransform = FTransform(EdgeStartVector);

        const int32 PointInstance = PointISMComponent->AddInstance(PointTransform, true);
    }
}
