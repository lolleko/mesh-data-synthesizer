// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MTWayGraphSamplerConfig.generated.h"

/**
 * 
 */
UCLASS(BlueprintType)
class MESHSYNTH_API UMTWayGraphSamplerConfig : public UDataAsset
{
    GENERATED_BODY()

public:
    // Roughly the distance between street view captures
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double SampleDistance = 800.;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PanoramaWidth =  3328;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PanoramaTopCrop =  512;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PanoramaBottomCrop =  640;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bShouldUseToneCurve = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString DatasetName = TEXT("Default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString OutputDirectory = TEXT("Default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bShouldSkipInference = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString InferenceEndpointAddress = TEXT("http://localhost:80/process-panos");
    
    FString GetConfigName() const;

    double GetMinDistanceBetweenSamples() const;
};
