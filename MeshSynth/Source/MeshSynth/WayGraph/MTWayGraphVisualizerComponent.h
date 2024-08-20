// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CesiumCartographicPolygon.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MeshSynth/OSM/MTOverpassSchema.h"
#include "MeshSynth/WayGraph/MTWayGraphStreetData.h"

#include "MTWayGraphVisualizerComponent.generated.h"

UENUM()
enum class EMTWaygraphVisualizerMode
{
    Static,
    Animated
};

UENUM()
enum class EMTWaygraphVisualizerBoundaryMode
{
    Visible,
    Hidden
};


UCLASS(Blueprintable)
class MESHSYNTH_API UMTWayGraphVisualizerComponent : public USceneComponent
{
    GENERATED_BODY()

public:
    UMTWayGraphVisualizerComponent();

    DECLARE_EVENT( UMTWayGraphVisualizerComponent, FOnEulerTourAnimationCompletedEvent )
    FOnEulerTourAnimationCompletedEvent& OnEulerTourAnimationCompleted() { return OnEulerTourAnimationCompletedEvent; }

    void ClearVisualization();

    void StartVisualization(
        ACesiumCartographicPolygon* InBoundingPolygon,
        const FMTStreetData& InStreetData,
        const double Duration,
        const EMTWaygraphVisualizerMode Mode,
        const EMTWaygraphVisualizerBoundaryMode BoundaryMode);

protected:
    virtual void BeginPlay() override;


private:
    // Should be a a mesh with Forward X Axis and Pivot at the beginning of X
    UPROPERTY(EditAnywhere, Category = "Visualizer")
    TObjectPtr<UStaticMesh> PathMesh;

    UPROPERTY(EditAnywhere, Category = "Visualizer")
    TObjectPtr<UStaticMesh> BoundaryMesh;

    UPROPERTY(EditAnywhere, Category = "Visualizer")
    TObjectPtr<UStaticMesh> CursorMesh;
    
    UPROPERTY()
    TObjectPtr<ACesiumCartographicPolygon> BoundingPolygon;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> PathISMComponent;
    
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> BoundaryISMComponent;

    UPROPERTY()
    TObjectPtr<UStaticMeshComponent> EulerTourCursorMeshComponent;

    FMTOverPassQueryCompletionDelegate OverpassQueryCompletedDelegate;

    FMTStreetData StreetData;

    int32 EulerAnimationCurrentPathIndex = 0;
    int32 EulerAnimationCurrentNodeIndex = 0;
    int32 EulerTourSegmentsPerTick = 0;
    FVector EulerAnimationZOffset = FVector(0, 0, 0);

    FOnEulerTourAnimationCompletedEvent OnEulerTourAnimationCompletedEvent;

    void ShowOverview();

    void ShowEulerTour();

    void ShowEulerTourAnimated();

    void ShowBoundary();

    void AddNextEulerTourMesh();
};