// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "../WayGraph/MTWayGraph.h"
#include "CesiumCartographicPolygon.h"
#include "CoreMinimal.h"
#include "MeshSynth/WayGraph/MTChinesePostMan.h"
#include "MeshSynth/WayGraph/MTWayGraphStreetData.h"
#include "MeshSynth/WayGraph/MTWayGraphVisualizerComponent.h"
#include "MTSample.h"
#include "MTSamplerComponentBase.h"

#include "MTWayGraphSamplerComponent.generated.h"

UCLASS(Blueprintable, ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MESHSYNTH_API UMTWayGraphSamplerComponent : public UMTSamplerComponentBase
{
    GENERATED_BODY()

public:
    UMTWayGraphSamplerComponent();

    virtual int32 GetEstimatedSampleCount() override;

    virtual void BeginPlay() override;

    UFUNCTION(BlueprintCallable)
    void SamplePolygon(ACesiumCartographicPolygon* Polygon, const bool bIgnoreCache);
    
protected:
    virtual TOptional<FTransform> SampleNextLocation() override;

    virtual TOptional<FTransform> ValidateSampleLocation() override;

    virtual FMTSample CollectSampleMetadata() override;

private:
    UPROPERTY(EditAnywhere, Category = "WayGraphSampler")
    TObjectPtr<ACesiumCartographicPolygon> BoundingPolygon;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UMTWayGraphVisualizerComponent> WayGraphVisualizerComponent;

    int32 EstimatedSampleCount;
    
    FMTStreetData StreetData;
    
    int32 CurrentImageCount;
    
    int32 CurrentPathIndex;

    int32 CurrentPathSegmentIndex;

    double CurrentPathSegmentStartDistance;

    double CurrentPathDistance;

    FVector CurrentSampleLocation;

    int32 CurrentWayIndex;

    int32 PrevWayIndex;
    
    FVector PrevSampleLocation;
    
    struct FSampledLocationWithGraphEdgeID
    {
        FVector Location;
        int32 WayID;

        bool operator==(const FSampledLocationWithGraphEdgeID& Other) const
        {
            return Location == Other.Location && WayID == Other.WayID;
        }

        friend uint32 GetTypeHash(const FSampledLocationWithGraphEdgeID& InPath)
        {
            return GetTypeHash(InPath.Location) ^ GetTypeHash(InPath.WayID);
        }
    };
    TSet<FSampledLocationWithGraphEdgeID> SampledLocationsLSH;

    FMTOverPassQueryCompletionDelegate OverpassQueryCompletedDelegate;
    
    void InitSamplingParameters();
    
    UFUNCTION()
    void OverpassQueryCompleted(const FOverPassQueryResult& Result, const bool bSuccess);

    UFUNCTION()
    void EulerTourAnimationCompleted();

    UFUNCTION()
    void FlyToLocationCompleted();

    UFUNCTION()
    void AfterFlyPauseIsOver();
    
    TConstArrayView<int32> ViewCurrentPath();

    FString GetStreetDataCacheFilePath() const;

    UFUNCTION(CallInEditor)
    void CopyBoundingPolygonToClipboard();
};
