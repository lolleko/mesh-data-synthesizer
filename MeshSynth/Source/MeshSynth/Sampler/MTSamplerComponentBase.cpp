// Fill out your copyright notice in the Description page of Project Settings.

#include "MTSamplerComponentBase.h"

#include "Cesium3DTileset.h"
#include "CesiumCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/SceneCaptureComponentCube.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "EngineUtils.h"
#include "MeshSynth/Interaction/MTInteractionUtils.h"
#include "HttpModule.h"
#include "JsonDomBuilder.h"
#include "MTSample.h"
#include "MTSceneCaptureCube.h"
#include "MTWayGraphSamplerConfig.h"

UMTSamplerComponentBase::UMTSamplerComponentBase()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UMTSamplerComponentBase::BeginPlay()
{
    Super::BeginPlay();
    
    PanoramaCapture = GetWorld()->SpawnActor<AMTSceneCaptureCube>();
    PanoramaCapture->GetCaptureComponentCube()->TextureTarget =
        NewObject<UTextureRenderTargetCube>(this);
    PanoramaCapture->GetCaptureComponentCube()->TextureTarget->CompressionSettings =
        TextureCompressionSettings::TC_Default;
    PanoramaCapture->GetCaptureComponentCube()->TextureTarget->TargetGamma = 0.F;
    PanoramaCapture->GetCaptureComponentCube()->TextureTarget->Init(
        GetActiveConfig()->PanoramaWidth / 2, EPixelFormat::PF_B8G8R8A8);
    PanoramaCapture->GetCaptureComponentCube()->ShowFlags.SetToneCurve(ShouldUseToneCurve());

    PanoramaCapture->GetRenderTargetLongLat()->InitCustomFormat(
        PanoramaCapture->GetCaptureComponentCube()->TextureTarget->SizeX * 2,
        PanoramaCapture->GetCaptureComponentCube()->TextureTarget->SizeX,
        EPixelFormat::PF_B8G8R8A8,
        false);

    Capture2D = GetWorld()->SpawnActor<AMTSceneCapture>();
    Capture2D->GetCaptureComponent2D()->FOVAngle = 65;
    Capture2D->GetCaptureComponent2D()->TextureTarget = NewObject<UTextureRenderTarget2D>(this);
    Capture2D->GetCaptureComponent2D()->TextureTarget->bAutoGenerateMips = false;
    Capture2D->GetCaptureComponent2D()->TextureTarget->CompressionSettings =
        TextureCompressionSettings::TC_Default;
    Capture2D->GetCaptureComponent2D()->TextureTarget->AddressX = TextureAddress::TA_Clamp;
    Capture2D->GetCaptureComponent2D()->TextureTarget->AddressY = TextureAddress::TA_Clamp;
    Capture2D->GetCaptureComponent2D()->TextureTarget->RenderTargetFormat = RTF_RGBA8;
    Capture2D->GetCaptureComponent2D()->TextureTarget->TargetGamma = 2.2F;
    Capture2D->GetCaptureComponent2D()->TextureTarget->bGPUSharedFlag = true;
    Capture2D->GetCaptureComponent2D()->ShowFlags.SetToneCurve(ShouldUseToneCurve());

    DepthPerspectivePostProcessMaterial = LoadObject<UMaterial>(
    nullptr,
    TEXT("/Game/Geolocator/Materials/PP_DepthPerspective.PP_DepthPerspective"),
    nullptr,
    LOAD_None,
    nullptr);

    Capture2DDepth = GetWorld()->SpawnActor<AMTSceneCapture>();
    Capture2DDepth->GetCaptureComponent2D()->bAlwaysPersistRenderingState = true;
    Capture2DDepth->GetCaptureComponent2D()->TextureTarget = NewObject<UTextureRenderTarget2D>(this);
    Capture2DDepth->GetCaptureComponent2D()->TextureTarget->bAutoGenerateMips = false;
    Capture2DDepth->GetCaptureComponent2D()->TextureTarget->CompressionSettings =
        TextureCompressionSettings::TC_Default;
    Capture2DDepth->GetCaptureComponent2D()->TextureTarget->AddressX = TextureAddress::TA_Clamp;
    Capture2DDepth->GetCaptureComponent2D()->TextureTarget->AddressY = TextureAddress::TA_Clamp;
    Capture2DDepth->GetCaptureComponent2D()->TextureTarget->RenderTargetFormat = RTF_RGBA8;
    Capture2DDepth->GetCaptureComponent2D()->TextureTarget->TargetGamma = 2.2F;
    Capture2DDepth->GetCaptureComponent2D()->TextureTarget->bGPUSharedFlag = false;
    Capture2DDepth->GetCaptureComponent2D()->ShowFlags.SetToneCurve(false);
    Capture2DDepth->GetCaptureComponent2D()->ShowFlags.SetAntiAliasing(false);
    
    ChangeCapture2DResolution({512, 512}, 65.);
}

void UMTSamplerComponentBase::BeginSampling()
{
    if (!ensure(IsValid(GetActiveConfig())))
    {
        return;
    }
    
    DepthPerspectivePostProcessMaterialInstance = UMaterialInstanceDynamic::Create(
        DepthPerspectivePostProcessMaterial,
        this);

    DepthPerspectivePostProcessMaterialInstance->SetScalarParameterValue(
        FName(TEXT("MaxZ")), MaxDepth);

    Capture2DDepth->GetCaptureComponent2D()->AddOrUpdateBlendable(
        DepthPerspectivePostProcessMaterial, 1);

    bIsSampling = true;
    CurrentSampleCount = 0;

    GotoNextSampleStep(ENextSampleStep::InitSampling, 4);
}

bool UMTSamplerComponentBase::IsSampling()
{
    return bIsSampling;
}

int32 UMTSamplerComponentBase::GetCurrentSampleCount()
{
    return CurrentSampleCount;
}

UMTWayGraphSamplerConfig* UMTSamplerComponentBase::GetActiveConfig() const
{
    return Config;
}

TArray<FColor> UMTSamplerComponentBase::GetPanoramaPixelBuffer()
{
    return PanoramaCapture->GetMutableImageDataRef();
}

FIntPoint UMTSamplerComponentBase::GetPanoramaPixelBufferSize()
{
    return {
        PanoramaCapture->GetRenderTargetLongLat()->SizeX,
        PanoramaCapture->GetRenderTargetLongLat()->SizeY};
}

void UMTSamplerComponentBase::InitSampling()
{
    auto* CameraManager = ACesiumCameraManager::GetDefaultCameraManager(GetWorld());
    CesiumGroundLoaderCameraID = CameraManager->AddCamera(
        {{128, 128}, GetComponentLocation() + FVector(0, 0, 500), {-90., 0., 0.}, 50.});

    CesiumSkyLoaderCameraID = CameraManager->AddCamera(
        {{128, 128}, GetComponentLocation() - FVector(0, 0, 500), {90., 0., 0.}, 50.});

    for (auto* Tileset : TActorRange<ACesium3DTileset>(GetWorld()))
    {
        Tileset->PlayMovieSequencer();
    }

    for (int32 I = 0; I < CesiumPanoramaLoaderCameraIDs.Num(); ++I)
    {
        CesiumPanoramaLoaderCameraIDs[I] = CameraManager->AddCamera(
            {{static_cast<double>(GetActiveConfig()->PanoramaWidth / 2.),
              GetActiveConfig()->PanoramaWidth / 2.},
             GetComponentLocation(),
             {0., I * 90., 0.},
             110.});
    }

    // TODO refactor into GetPriamryPlayerPawn
    // const auto PlayerPawn = CastChecked<AMTPlayerPawn>(
    //     GetWorld()->GetGameInstance()->GetPrimaryPlayerController()->GetPawn());
    // PlayerPawn->GetCaptureCameraComponent()->SetActive(true);
    // PlayerPawn->GetOverviewCameraComponent()->SetActive(false);

    GotoNextSampleStep(ENextSampleStep::FindNextSampleLocation);
}

void UMTSamplerComponentBase::FindNextSampleLocation()
{
    CurrentSampleCount++;

    auto PossibleSampleLocation = SampleNextLocation();

    while (!PossibleSampleLocation)
    {
        CurrentSampleCount++;

        PossibleSampleLocation = SampleNextLocation();

        if (!bIsSampling)
        {
            return;
        }
    }

    const FTransform SampleTransform = PossibleSampleLocation.GetValue();

    GetOwner()->SetActorTransform(SampleTransform);

    // make sure ground and surroudnigs are loaded
    UpdateCesiumCameras();

    GotoNextSampleStep(ENextSampleStep::PreCaptureSample);
}

void UMTSamplerComponentBase::PreCapture()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(MTSampling::PreCapture);

    const auto PossibleUpdatedTransform = ValidateSampleLocation();

    if (!PossibleUpdatedTransform)
    {
        GotoNextSampleStep(ENextSampleStep::FindNextSampleLocation);
        return;
    }

    const auto UpdatedTransform = PossibleUpdatedTransform.GetValue();

    const auto MaximumHeightDifference = FVector(0., 0., 10000.);

    TStaticArray<FVector, 9> TraceOffsets;
    TraceOffsets[0] = {-75, 0, 0};
    TraceOffsets[1] = {0, 0, 0};
    TraceOffsets[2] = {75, 0, 0};
    TraceOffsets[3] = {-75, -75, 0};
    TraceOffsets[4] = {0, -75, 0};
    TraceOffsets[5] = {75, -75, 0};
    TraceOffsets[6] = {-75, 75, 0};
    TraceOffsets[7] = {0, 75, 0};
    TraceOffsets[8] = {75, 75, 0};

    double TraceMissCounter = 0;

    for (const auto& TraceOffset : TraceOffsets)
    {
        FHitResult TopDownTrace;
        GetWorld()->LineTraceSingleByObjectType(
            TopDownTrace,
            UpdatedTransform.GetLocation() + TraceOffset,
            UpdatedTransform.GetLocation() + TraceOffset - MaximumHeightDifference,
            FCollisionObjectQueryParams::AllStaticObjects);

        // if both traces miss we are most likely inside the ground
        if (!TopDownTrace.bBlockingHit)
        {
            TraceMissCounter++;
        }
    }

    SampleArtifactProbability = TraceMissCounter / TraceOffsets.Num();

    GetOwner()->SetActorTransform(UpdatedTransform);

    const auto PlayerPawn = CastChecked<APawn>(
        GetWorld()->GetGameInstance()->GetPrimaryPlayerController()->GetPawn());
    PlayerPawn->SetActorTransform(UpdatedTransform);// + FTransform(FVector(0, 0, 3000)));
    
    // auto Rotas = UpdatedTransform.GetRotation().Rotator();
    // Rotas.Pitch -= 90;
    // GetWorld()->GetGameInstance()->GetPrimaryPlayerController()->SetControlRotation(
    //     Rotas);
    // TODO rotate camera to look in direciton of travel

    PanoramaCapture->SetActorTransform(UpdatedTransform);
    PanoramaCapture->AddActorWorldRotation(FRotator(0., 90., 0.));
    Capture2D->SetActorTransform(UpdatedTransform);
    Capture2DDepth->SetActorTransform(UpdatedTransform);

    UpdateCesiumCameras();

    GotoNextSampleStep(ENextSampleStep::CaptureSample);
}

void UMTSamplerComponentBase::CaptureSample()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(MTSampling::CaptureSample);

    // Write images from previous run doing this here gives them a couple of frames to complete
    // during this time we can already go to the next sample etc.
    // now we are about to capture again and therefore need to make sure the images are stored

    WaitForRenderThreadReadSurfaceAndWriteImages();

    FMTSample Sample = CollectSampleMetadata();
    
    const auto AbsoluteImageFilePath = CreateImagePathForSample(Sample);

    CapturedImageCount++;

    // Assume we are resuming previous run and don't overwrite image or metadata
    if (FPaths::FileExists(AbsoluteImageFilePath))
    {
        GotoNextSampleStep(ENextSampleStep::FindNextSampleLocation);
        return;
    }

    Sample.ArtifactProbability = SampleArtifactProbability;

    if (bCapturePanorama)
    {
        EnqueueCapture({PanoramaCapture, AbsoluteImageFilePath});
    }
    else
    {
        EnqueueCapture({Capture2D, AbsoluteImageFilePath});

        if (bCaptureDepth)
        {
            // replace extension with _depth.png
            EnqueueCapture({Capture2DDepth, FPaths::ChangeExtension(AbsoluteImageFilePath, TEXT("depth")) + TEXT(".png")});
        }
    }

    if (!CaptureQueue.IsEmpty())
    {
        // Basically the same as in CaptureScene()
        // But we only call SendAllEndOfFrameUpdates once
        GetWorld()->SendAllEndOfFrameUpdates();
        for (const auto& CaptureData : CaptureQueue)
        {
            if (CaptureData.Capture->IsA<AMTSceneCaptureCube>())
            {
                const auto* CubeCapture = CastChecked<AMTSceneCaptureCube>(CaptureData.Capture);

                CubeCapture->GetCaptureComponentCube()->UpdateSceneCaptureContents(
                    GetWorld()->Scene);
            }
            else if (CaptureData.Capture->IsA<AMTSceneCapture>())
            {
                const auto* Capture = CastChecked<AMTSceneCapture>(CaptureData.Capture);

                Capture->GetCaptureComponent2D()->UpdateSceneCaptureContents(GetWorld()->Scene);
            }
        }

        ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)
        (
            [this](FRHICommandListImmediate& RHICmdList)
            {
                TRACE_CPUPROFILER_EVENT_SCOPE(MTSampling::ReadSurface);

                for (const auto& CaptureData : CaptureQueue)
                {
                    if (CaptureData.Capture->IsA<AMTSceneCaptureCube>())
                    {
                        auto* CubeCapture = CastChecked<AMTSceneCaptureCube>(CaptureData.Capture);

                        FIntPoint SizeOUT;
                        EPixelFormat FormatOUT;
                        CubemapHelpers::GenerateLongLatUnwrap(
                            CubeCapture->GetCaptureComponentCube()->TextureTarget,
                            CubeCapture->GetMutableImageDataRef(),
                            SizeOUT,
                            FormatOUT,
                            CubeCapture->GetRenderTargetLongLat(),
                            RHICmdList);
                    }
                    else if (CaptureData.Capture->IsA<AMTSceneCapture>())
                    {
                        auto* CurrentCapture2D = CastChecked<AMTSceneCapture>(CaptureData.Capture);

                        const auto* Resource = CurrentCapture2D->GetCaptureComponent2D()
                                                   ->TextureTarget->GetRenderTargetResource();
                        CurrentCapture2D->GetMutableImageSize() = {(int32)Resource->GetSizeX(), (int32)Resource->GetSizeY()};
                        FReadSurfaceDataFlags SurfaceFlags(RCM_UNorm, CubeFace_MAX);
                        RHICmdList.ReadSurfaceData(
                            Resource->GetRenderTargetTexture(),
                            FIntRect(0, 0, Resource->GetSizeX(), Resource->GetSizeY()),
                            CurrentCapture2D->GetMutableImageDataRef(),
                            SurfaceFlags);
                    }
                }
            });

        CaptureFence.BeginFence();
    }

    // Call Capture function on remaining samples
    GotoNextSampleStep(ENextSampleStep::FindNextSampleLocation);
}

void UMTSamplerComponentBase::GotoNextSampleStep(
    const ENextSampleStep NextStep,
    const int32 StepWaitFrames)
{
    NextSampleStep = NextStep;
    StepFrameSkips = StepWaitFrames;
}

void UMTSamplerComponentBase::EnqueueCapture(const FMTCaptureImagePathPair& CaptureImagePathPair)
{
    CaptureQueue.Add(CaptureImagePathPair);
}

void UMTSamplerComponentBase::UpdateCesiumCameras()
{
    auto* CameraManager = ACesiumCameraManager::GetDefaultCameraManager(GetWorld());
    CameraManager->UpdateCamera(
        CesiumGroundLoaderCameraID,
        {{128, 128}, GetComponentLocation() + FVector(0, 0, 500), {-90., 0., 0.}, 50.});
    CameraManager->UpdateCamera(
        CesiumSkyLoaderCameraID,
        {{128, 128}, GetComponentLocation() - FVector(0, 0, 500), {90., 0., 0.}, 50.});

    for (int32 I = 0; I < CesiumPanoramaLoaderCameraIDs.Num(); ++I)
    {
        CameraManager->UpdateCamera(
            CesiumPanoramaLoaderCameraIDs[I],
            {{static_cast<double>(GetActiveConfig()->PanoramaWidth / 2.),
              GetActiveConfig()->PanoramaWidth / 2.},
             GetComponentLocation(),
             {0., I * 90., 0.},
             110.});
    }
}

FString UMTSamplerComponentBase::GetImageDir()
{
    return FPaths::Combine(GetSessionDir(), "Images");
}

void UMTSamplerComponentBase::WaitForRenderThreadReadSurfaceAndWriteImages()
{
    TRACE_CPUPROFILER_EVENT_SCOPE(MTSampling::WaitForReadSurface);

    //for (auto& InferenceTaskFuture : InferenceTaskFutures)
    //{
    //    InferenceTaskFuture.GetFuture().Wait();
    //}
    //InferenceTaskFutures.Reset();

    for (auto& ImageWriteFuture : ImageWriteTaskFutures)
    {
        ImageWriteFuture.Wait();

        if (!Config->bShouldSkipInference)
        {
            TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
            HttpRequest->SetVerb(TEXT("POST"));
            HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
            HttpRequest->SetURL(FString::Printf(TEXT("http://%s/process-pano"), *Config->InferenceEndpointAddress));

            FJsonDomBuilder::FObject RequestPayload;
            RequestPayload.Set(
                TEXT("image"),
                FBase64::Encode(
                    ImageWriteFuture.Get().Data.GetData(),
                    ImageWriteFuture.Get().Data.Num(),
                    EBase64Mode::Standard));
            RequestPayload.Set("image_path", ImageWriteFuture.Get().FilePath);
            RequestPayload.Set(TEXT("dataset_name"), GetActiveConfig()->DatasetName);
                
            HttpRequest->SetContentAsString(RequestPayload.ToString());
            HttpRequest->SetDelegateThreadPolicy(
                EHttpRequestDelegateThreadPolicy::CompleteOnHttpThread);

            //TPromise<void>& InferenceTaskFuture = InferenceTaskFutures.Emplace_GetRef();

            //HttpRequest->OnProcessRequestComplete().BindLambda(
            //    [this, &InferenceTaskFuture](
            //        FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess) 
            //    { 
            //        InferenceTaskFuture.SetValue();
            //    });

            HttpRequest->ProcessRequest();   
        }
    }

    ImageWriteTaskFutures.Reset();

    if (!CaptureQueue.IsEmpty())
    {
        // Allow other tasks to execute, we only care that out captures are not modified
        CaptureFence.Wait(false);

        for (const auto& CaptureData : CaptureQueue)
        {
            if (CaptureData.Capture->IsA<AMTSceneCaptureCube>())
            {
                auto* CubeCapture = CastChecked<AMTSceneCaptureCube>(CaptureData.Capture);
                auto ImageWriteFuture = UMTSamplingFunctionLibrary::WriteCubeMapPixelBufferToFile(
                    CaptureData.AbsoluteImagePath,
                    CubeCapture->GetMutableImageDataRef(),
                    {GetActiveConfig()->PanoramaWidth,
                     CubeCapture->GetCaptureComponentCube()->TextureTarget->SizeX},
                    {GetActiveConfig()->PanoramaTopCrop, GetActiveConfig()->PanoramaBottomCrop});
                ImageWriteTaskFutures.Emplace(MoveTemp(ImageWriteFuture));
            }
            else if (CaptureData.Capture->IsA<AMTSceneCapture>())
            {
                auto* Capture = CastChecked<AMTSceneCapture>(CaptureData.Capture);

                auto ImageWriteFuture = UMTSamplingFunctionLibrary::WritePixelBufferToFile(
                    CaptureData.AbsoluteImagePath,
                    Capture->GetMutableImageDataRef(),
                    Capture->GetMutableImageSize());
                ImageWriteTaskFutures.Emplace(MoveTemp(ImageWriteFuture));
            }
        }
    }

    CaptureQueue.Reset();
}

void UMTSamplerComponentBase::EndSampling()
{
    // Wait for last batch
    WaitForRenderThreadReadSurfaceAndWriteImages();

    PanoramaCapture->Destroy();
    Capture2D->Destroy();
    Capture2DDepth->Destroy();

    // TODO refactor into GetPriamryPlayerPawn
    // const auto PlayerPawn = CastChecked<AMTPlayerPawn>(
    //     GetWorld()->GetGameInstance()->GetPrimaryPlayerController()->GetPawn());
    // PlayerPawn->GetCaptureCameraComponent()->SetActive(false);
    // PlayerPawn->GetOverviewCameraComponent()->SetActive(true);

    bIsSampling = false;
}

bool UMTSamplerComponentBase::ShouldSampleOnBeginPlay() const
{
    return bShouldSampleOnBeginPlay;
}

FString UMTSamplerComponentBase::GetSessionDir() const
{
    FString SessionDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("WorldIndex"),
        GetWorld()->GetMapName(),
        GetActiveConfig()->GetConfigName()));

    return SessionDir;
}

TOptional<FTransform>
UMTSamplerComponentBase::ValidateGroundAndObstructions(const bool bIgnoreObstructions) const
{
    const auto MaximumHeightDifference = FVector(0., 0., 1000000.);

    FHitResult GroundHit;
    GetWorld()->LineTraceSingleByObjectType(
        GroundHit,
        GetComponentLocation() + MaximumHeightDifference,
        GetComponentLocation() - MaximumHeightDifference,
        FCollisionObjectQueryParams::AllStaticObjects);

    if (!GroundHit.bBlockingHit)
    {
        return {};
    }

    // ground offset of street view car probably ~250, we attempt to stick close to street view liek
    // scenarios because there is precedent in many papers e.g.
    // @torii24PlaceRecognition2015 & @bertonRethinkingVisualGeolocalization2022
    const auto GroundOffset = FVector(0., 0., 250.);

    auto UpdatedSampleLocation = GroundHit.Location + GroundOffset;

    if (!bIgnoreObstructions)
    {
        TArray<FHitResult> Hits;

        constexpr auto TraceCount = 64;
        constexpr auto ClearanceDistance = 1000.;

        // Line trace in a sphere around the updated location
        for (int32 TraceIndex = 0; TraceIndex < TraceCount; ++TraceIndex)
        {
            const auto DegreeInterval = 360. / TraceCount;
            FHitResult Hit;

            GetWorld()->LineTraceSingleByObjectType(
                Hit,
                UpdatedSampleLocation,
                UpdatedSampleLocation +
                    FRotator(0., TraceIndex * DegreeInterval, 0.).Vector() * ClearanceDistance,
                FCollisionObjectQueryParams::AllStaticObjects);

            if (bDrawObstructionDetectionDebug)
            {
                DrawDebugLine(
                    GetWorld(),
                    UpdatedSampleLocation,
                    UpdatedSampleLocation +
                        FRotator(0., TraceIndex * DegreeInterval, 0.).Vector() * ClearanceDistance,
                    FColor::Orange,
                    false,
                    2,
                    0,
                    6);
            }

            if (Hit.bBlockingHit)
            {
                if (bDrawObstructionDetectionDebug)
                {
                    DrawDebugSphere(GetWorld(), Hit.Location, 50, 6, FColor::Red, false, 2, 0, 6);
                }
                Hits.Add(Hit);
            }
        }

        if (bDrawObstructionDetectionDebug)
        {
            DrawDebugSphere(GetWorld(), UpdatedSampleLocation, 80, 12, FColor::Red, false, 2, 0, 6);
        }

        // Calcualte poitn with clearance form all hits
        if (Hits.Num() > 0)
        {
            FVector Center = FVector::ZeroVector;
            for (const auto& Hit : Hits)
            {
                const auto HitToCam = (Hit.TraceStart - Hit.TraceEnd).GetSafeNormal();
                const auto ClearPoint = Hit.Location + HitToCam * ClearanceDistance;
                //DrawDebugSphere(GetWorld(), ClearPoint, 50, 6, FColor::Yellow, false, 2, 0, 6);
                Center += ClearPoint;
            }
            Center /= Hits.Num();
            UpdatedSampleLocation = Center;
        }

        GetWorld()->LineTraceSingleByObjectType(
            GroundHit,
            UpdatedSampleLocation + MaximumHeightDifference,
            UpdatedSampleLocation - MaximumHeightDifference,
            FCollisionObjectQueryParams::AllStaticObjects);


        if (bDrawObstructionDetectionDebug)
        {
            DrawDebugSphere(GetWorld(), UpdatedSampleLocation, 80, 12, FColor::Green, false, 2, 0, 6);
        }
        
        if (!GroundHit.bBlockingHit)
        {
            return {};
        }

        UpdatedSampleLocation = GroundHit.Location + GroundOffset;
    }

    return {FTransform(GetOwner()->GetActorRotation(), UpdatedSampleLocation)};
}

void UMTSamplerComponentBase::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (StepFrameSkips > 0)
    {
        StepFrameSkips--;
        return;
    }

    if (!bIsSampling)
    {
        return;
    }

    switch (NextSampleStep)
    {
        case ENextSampleStep::InitSampling:
            // skip one frame for initialization
            InitSampling();
            break;
        case ENextSampleStep::FindNextSampleLocation:
            FindNextSampleLocation();
            break;
        case ENextSampleStep::PreCaptureSample:
            PreCapture();
            break;
        case ENextSampleStep::CaptureSample:
            CaptureSample();
            break;
        default:
            check(false);
    }
}

FString UMTSamplerComponentBase::CreateImagePathForSample(const FMTSample& Sample)
{
    FMTSample SampleWithImageID  = Sample;
    SampleWithImageID.ImageID = CapturedImageCount;

    FString ImageDir = SampleWithImageID.ImageDir.IsSet() ? SampleWithImageID.ImageDir.GetValue() : GetImageDir();

    FString ImageName = SampleWithImageID.ImageName.IsSet()
                            ? SampleWithImageID.ImageName.GetValue()
                            : UMTSamplingFunctionLibrary::CreateImageNameFromSample(SampleWithImageID);
    
    if (ImageDir.Contains(TEXT("\\"))) {
        return ImageDir + TEXT("\\") + ImageName;
    }
    return FPaths::Combine(ImageDir, ImageName);
}
