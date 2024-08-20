// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MTGridAccessor.h"
#include "MTSamplerComponentBase.h"
#include "MTSamplingFunctionLibrary.h"

#include "MTExtraViewSamplerComponent.generated.h"

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class MESHSYNTH_API UMTExtraViewSamplerComponent : public UMTSamplerComponentBase
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
    FString TestPredictionFile;
    
    TArray<UMTSamplingFunctionLibrary::FExtraViewData> Locations;

    int32 CurrentPredictionIndex = 0;

    int32 SampleGridRadius = 2;
    
    TMTNDGridAccessor<3> SampleGrid;

    TMap<FString, TArray<FString>> PredictionToExtraViews;

    FVector SampleGridCellSize;

    int32 CurrentSampleGridCellIndex;
    
    int32 InitialLocationCount = 0;

    void WriteToImageListTXT(const FString& ImageListTXTPath, const FString& ImagePath);
};
