// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "JsonObjectWrapper.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "MeshSynth/Sampler/MTSample.h"
#include "MeshSynth/Sampler/MTSceneCapture.h"
#include "NNERuntimeCPU.h"
#include "UObject/Object.h"

#include "MTInteractionUtils.generated.h"

/**
 *
 */
UCLASS()
class MESHSYNTH_API UMTInteractionFunctionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

    // UFUNCTION(BlueprintCallable, Category = "Utilities")
    // static FString SelectFileFromFileDialog();

    UFUNCTION(BlueprintCallable, Category = "Utilities")
    static UTexture2D* LoadImageFromFile(const FString& FilePath);

    UFUNCTION(BlueprintCallable, Category = "Utilities")
    static UTexture2DDynamic* CreateTexture2DDynamic();

    UFUNCTION(BlueprintPure, Category = "Utilities")
    static FString GetBaseDir();

    UFUNCTION(BlueprintCallable, Category = "Utilities")
    static TArray<FMTSample> ResultsToSampleArray(const FString& Results);

    UFUNCTION(BlueprintCallable, Category = "Utilities")
    static FString  EncodeCHWImageBase64(const TArray<float>& ImagePixels);
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
    FMTOnImageLoadCompleted,
    const int32,
    SizeX,
    const int32,
    SizeY,
    const FString&,
    Base64Image,
    const bool,
    bSuccess);

UCLASS()  // Change the _API to match your project
class MESHSYNTH_API UMTImageLoadAsyncAction : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    /** Execute the actual load */
    virtual void Activate() override;

    UFUNCTION(
        BlueprintCallable,
        meta =
            (BlueprintInternalUseOnly = "true",
             Category = "HTTP",
             WorldContext = "WorldContextObject"))
    static UMTImageLoadAsyncAction* AsyncImageLoad(
        UObject* WorldContextObject,
        const FString& FilePath,
        UTexture2DDynamic* TargetImageTexture);

    UPROPERTY(BlueprintAssignable)
    FMTOnImageLoadCompleted Completed;

private:
    FString FilePath;

    UPROPERTY()
    UTexture2DDynamic* TargetImageTexture;
};


// TODO concert to base64 input
UCLASS()  // Change the _API to match your project
class MESHSYNTH_API UMTLoadImageFromBase64AsyncAction : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    /** Execute the actual load */
    virtual void Activate() override;

    UFUNCTION(
        BlueprintCallable,
        meta =
            (BlueprintInternalUseOnly = "true",
             Category = "HTTP",
             WorldContext = "WorldContextObject"))
    static UMTLoadImageFromBase64AsyncAction* AsyncLoadImageFromBase64(
        UObject* WorldContextObject,
        const FString& EncodedImage,
        UTexture2DDynamic* TargetImageTexture);

    UPROPERTY(BlueprintAssignable)
    FMTOnImageLoadCompleted Completed;

private:
    FString EncodedImage;

    UPROPERTY()
    UTexture2DDynamic* TargetImageTexture;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTOnImageCaptureCompleted, const bool, bSuccess);

UCLASS()
class MESHSYNTH_API UMTCapturePreviewImageAction : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    /** Execute the actual load */
    virtual void Activate() override;

    UFUNCTION(
        BlueprintCallable,
        meta =
            (BlueprintInternalUseOnly = "true",
             Category = "HTTP",
             WorldContext = "WorldContextObject"))
    static UMTCapturePreviewImageAction* CapturePreviewImage(
        UObject* WorldContextObject,
        const FVector& LongLatHeight,
        double YawAtLocation,
        double PitchAtLocation,
        UTexture2DDynamic* TargetImageTexture);

    UPROPERTY(BlueprintAssignable)
    FMTOnImageCaptureCompleted Completed;

private:
    FVector LongLatHeight;

    double YawAtLocation;

    double PitchAtLocation;

    UPROPERTY()
    AMTSceneCapture* CaptureActor;

    UPROPERTY()
    UObject* WorldContextObject;

    UPROPERTY()
    UTexture2DDynamic* TargetImageTexture;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FMTOnHttpRequestCompleted,
    const FString&,
    Json,
    bool,
    bSuccess);

UCLASS()  // Change the _API to match your project
class MESHSYNTH_API UMTHttpJsonRequestAsyncAction : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    /** Execute the actual load */
    virtual void Activate() override;

    UFUNCTION(
        BlueprintCallable,
        meta =
            (BlueprintInternalUseOnly = "true",
             Category = "HTTP",
             WorldContext = "WorldContextObject"))
    static UMTHttpJsonRequestAsyncAction* AsyncJSONRequestHTTP(
        UObject* WorldContextObject,
        const FString& URL,
        const FString& Verb = TEXT("GET"),
        const FString& Body = TEXT(""));

    UPROPERTY(BlueprintAssignable)
    FMTOnHttpRequestCompleted Completed;

protected:
    void HandleRequestCompleted(const FString& ResponseString, bool bSuccess);

private:
    /* URL to send GET request to */
    FString URL;

    FString Verb;

    FString Body;
};
