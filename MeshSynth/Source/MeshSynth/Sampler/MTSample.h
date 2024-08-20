// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MTSample.generated.h"

/**
 * 
 */
USTRUCT(BlueprintType)
struct MESHSYNTH_API FMTSample
{
    GENERATED_BODY()

    TOptional<FString> ImageDir; 
    
    TOptional<FString> ImageName;
    
    TArray<ANSICHAR> Descriptor;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double HeadingAngle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Pitch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Roll;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector LonLatAltitude;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString StreetName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double ArtifactProbability;
    
    // Only present during inference
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    double Score;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Region;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString AbsoluteImagePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString Thumbnail;

    int32 ImageID;
};
