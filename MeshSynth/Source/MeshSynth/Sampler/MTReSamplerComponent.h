// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CesiumCartographicPolygon.h"
#include "CoreMinimal.h"
#include "MTSamplerComponentBase.h"
#include "MTSamplingFunctionLibrary.h"
#include <CesiumGeospatial/CartographicPolygon.h>

#include "MTReSamplerComponent.generated.h"

UENUM()
enum class EMTDatasetType
{
    SFXL_CSV,
    PITTS_TXT,
    TOKYO_TXT,
};

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MESHSYNTH_API UMTReSamplerComponent : public UMTSamplerComponentBase
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;
    
    virtual int32 GetEstimatedSampleCount() override;

protected:
    virtual TOptional<FTransform> SampleNextLocation() override;

    virtual TOptional<FTransform> ValidateSampleLocation() override;

    virtual FMTSample CollectSampleMetadata() override;

private:
    UPROPERTY(EditAnywhere)
    FString FilePath;

    UPROPERTY(EditAnywhere)
    EMTDatasetType DatasetType;

    TArray<UMTSamplingFunctionLibrary::FLocationPathPair> Locations;

    int32 CurrentSampleIndex = 0;
    int32 InitialLocationCount = 0;
    int32 NextSampleIndex = 0;
    
    UPROPERTY(EditAnywhere)
    TObjectPtr<ACesiumCartographicPolygon> BoundingPolygon;
    
    TArray<FVector> BoundingPolyline;
    TArray<FVector2D> BoundingPolyline2D;
    FBox2d BoundingBox2D;
};
