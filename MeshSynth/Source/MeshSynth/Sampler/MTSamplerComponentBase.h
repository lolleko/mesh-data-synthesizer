// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/SceneCaptureComponent2D.h"
#include "CoreMinimal.h"
#include "Engine/SceneCapture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "JsonDomBuilder.h"
#include "MTSample.h"
#include "MTSceneCapture.h"
#include "MTSceneCaptureCube.h"
#include "MTWayGraphSamplerConfig.h"
#include "MTSamplingFunctionLibrary.h"

#include "MTSamplerComponentBase.generated.h"

USTRUCT()
struct FMTCaptureImagePathPair
{
    GENERATED_BODY()

    ASceneCapture* Capture;
    FString AbsoluteImagePath;
};

UCLASS(ClassGroup = (Custom), Abstract)
class MESHSYNTH_API UMTSamplerComponentBase : public USceneComponent
{
    GENERATED_BODY()

public:
    UMTSamplerComponentBase();

    void BeginPlay() override;

    void BeginSampling();

    UFUNCTION(BlueprintCallable)
    bool IsSampling();

    UFUNCTION(BlueprintCallable)
    int32 GetCurrentSampleCount();

    UFUNCTION(BlueprintCallable)
    virtual int32 GetEstimatedSampleCount()
    {
        PURE_VIRTUAL(ValidateSampleLocation)
        return 0;
    }

    UFUNCTION(BlueprintCallable)
    UMTWayGraphSamplerConfig* GetActiveConfig() const;

    UFUNCTION(BlueprintCallable)
    virtual TArray<FColor> GetPanoramaPixelBuffer();

    UFUNCTION(BlueprintCallable)
    virtual FIntPoint GetPanoramaPixelBufferSize();


protected:
    virtual TOptional<FTransform> ValidateSampleLocation()
    {
        PURE_VIRTUAL(ValidateSampleLocation)
        return GetComponentTransform();
    }

    virtual TOptional<FTransform> SampleNextLocation()
    {
        PURE_VIRTUAL(SampleNextLocation)
        return {};
    }

    virtual FMTSample CollectSampleMetadata()
    {
        PURE_VIRTUAL(CollectSampleMetadata)
        return {};
    };

    bool ShouldUseToneCurve() const
    {
        return GetActiveConfig()->bShouldUseToneCurve;
    }

    void IncrementCurrentSampleCount()
    {
        CurrentSampleCount++;
    }

    void EndSampling();

    bool ShouldSampleOnBeginPlay() const;

    FString GetSessionDir() const;

    TOptional<FTransform>
    ValidateGroundAndObstructions(const bool bIgnoreObstructions = false) const;

    void TogglePanoramaCapture(const bool bEnable)
    {
        bCapturePanorama = bEnable;
    }

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;

    void ChangeCapture2DResolution(const FIntVector2& Resolution, const double FOVAngle = 65., bool bResizeTo512 = false)
    {
        Capture2D->GetCaptureComponent2D()->FOVAngle = 68;

        // mimics torch.resize(512)
        FIntVector2 ResizedResolution = Resolution;

        if (bResizeTo512)
        {
            constexpr auto MinSize = 512;
            if (Resolution.Y > Resolution.X)
            {
                ResizedResolution = {MinSize, MinSize * Resolution.Y / Resolution.X};
            }
            else
            {
                ResizedResolution = {MinSize * Resolution.X / Resolution.Y, MinSize};
            }
        }

        Capture2D->GetCaptureComponent2D()->FOVAngle = FOVAngle;
        Capture2D->GetCaptureComponent2D()->TextureTarget->InitCustomFormat(
            ResizedResolution.X, ResizedResolution.Y, EPixelFormat::PF_B8G8R8A8, false);
    }

    FString CreateImagePathForSample(const FMTSample& Sample);

private:
    UPROPERTY()
    TObjectPtr<AMTSceneCaptureCube> PanoramaCapture;

    UPROPERTY()
    TObjectPtr<AMTSceneCapture> Capture2D;

    UPROPERTY(EditAnywhere)
    bool bShouldSampleOnBeginPlay = false;

    UPROPERTY(EditAnywhere)
    TObjectPtr<UMTWayGraphSamplerConfig> Config;

    UPROPERTY(EditAnywhere)
    bool bDrawObstructionDetectionDebug = false;

    int32 CurrentSampleCount;

    int32 CapturedImageCount;

    bool bIsSampling = false;

    bool bCapturePanorama = true;

    int32 CesiumGroundLoaderCameraID;

    int32 CesiumSkyLoaderCameraID;

    TStaticArray<int32, 4> CesiumPanoramaLoaderCameraIDs;

    double SampleArtifactProbability = 0.;

    TArray<FMTCaptureImagePathPair> CaptureQueue;

    TArray<TFuture<UMTSamplingFunctionLibrary::FImageWriteResult>> ImageWriteTaskFutures;
    TArray<TPromise<void>> InferenceTaskFutures;

    FRenderCommandFence CaptureFence;

    enum class ENextSampleStep
    {
        InitSampling,
        FindNextSampleLocation,
        PreCaptureSample,
        CaptureSample,
    };

    ENextSampleStep NextSampleStep = ENextSampleStep::InitSampling;

    int32 StepFrameSkips = 0;

    UFUNCTION()
    void InitSampling();

    UFUNCTION()
    virtual void FindNextSampleLocation();

    UFUNCTION()
    void PreCapture();

    UFUNCTION()
    void CaptureSample();

    void GotoNextSampleStep(const ENextSampleStep NextStep, const int32 StepWaitFrames = 0);

    void EnqueueCapture(const FMTCaptureImagePathPair& CaptureImagePathPair);

    void UpdateCesiumCameras();

    FString GetImageDir();

    void WaitForRenderThreadReadSurfaceAndWriteImages();
};
