// Fill out your copyright notice in the Description page of Project Settings.

#include "MTInteractionUtils.h"

#undef max
#include "Cesium3DTileset.h"
#include "CesiumGeoreference.h"
#include "CesiumSunSky.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Texture2DDynamic.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "HttpModule.h"
#include "ImageUtils.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "JsonDomBuilder.h"
#include "MeshSynth/MTShowcaseGameMode.h"

// FString UMTInteractionFunctionLibrary::SelectFileFromFileDialog()
// {
//     IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
//
//     TArray<FString> path;
//
//     FString FilePath;
//     TArray<FString> OutFiles;
//
//     TSharedPtr<SWindow> ParentWindow = FGlobalTabmanager::Get()->GetRootWindow();
//
//     if (DesktopPlatform->OpenFileDialog(
//             FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr),
//             TEXT("Select Query Image..."),
//             TEXT(""),
//             TEXT(""),
//             ImageWrapperHelper::GetImageFilesFilterString(false).GetData(),
//             EFileDialogFlags::None,
//             OutFiles))
//     {
//         FilePath = OutFiles[0];
//     }
//
//     return FilePath;
// }

UTexture2D* UMTInteractionFunctionLibrary::LoadImageFromFile(const FString& FilePath)
{
    return FImageUtils::ImportFileAsTexture2D(FilePath);
}
UTexture2DDynamic* UMTInteractionFunctionLibrary::CreateTexture2DDynamic()
{
    return UTexture2DDynamic::Create(512, 512);
}

FString UMTInteractionFunctionLibrary::GetBaseDir()
{
    return FPlatformProcess::BaseDir();
}

TArray<FMTSample> UMTInteractionFunctionLibrary::ResultsToSampleArray(const FString& Results)
{
    TArray<FMTSample> Samples;

    FJsonObjectWrapper Wrapper;
    Wrapper.JsonObjectFromString(Results);

    const auto ResultsArr = Wrapper.JsonObject->GetArrayField(TEXT("result"));
    for (const auto& Result : ResultsArr)
    {
        const auto ResultObj = Result->AsObject();

        FMTSample Sample;

        Sample.HeadingAngle = ResultObj->GetNumberField(TEXT("heading_angle")) - 90;
        Sample.Region = ResultObj->GetStringField(TEXT("dataset_name"));

        Sample.LonLatAltitude = FVector(
            ResultObj->GetNumberField(TEXT("lon")),
            ResultObj->GetNumberField(TEXT("lat")),
            ResultObj->GetNumberField(TEXT("altitude")));

        Sample.Score = ResultObj->GetNumberField(TEXT("score"));

        Sample.AbsoluteImagePath = ResultObj->GetStringField(TEXT("absolute_image_path"));

        Sample.Thumbnail = ResultObj->GetStringField(TEXT("thumbnail"));

        Samples.Add(Sample);
    }

    return Samples;
}

FString UMTInteractionFunctionLibrary::EncodeCHWImageBase64(const TArray<float>& ImagePixels)
{
    return FBase64::Encode((uint8*)ImagePixels.GetData(), ImagePixels.Num() * sizeof(float));
}

void UMTImageLoadAsyncAction::Activate()
{
    AsyncTask(
        ENamedThreads::AnyBackgroundThreadNormalTask,
        [this]()
        {
            TArray64<uint8> Buffer;
            if (FFileHelper::LoadFileToArray(Buffer, *FilePath))
            {
                FImage Image;
                if (FImageUtils::DecompressImage(Buffer.GetData(), Buffer.Num(), Image))
                {
                    FImage ImageForInference;

                    Image.ResizeTo(
                        ImageForInference, 512, 512, ERawImageFormat::BGRA8, EGammaSpace::sRGB);

                    TArray64<uint8> CompressedImage;
                    FImageUtils::CompressImage(
                        CompressedImage, TEXT(".jpg"), ImageForInference, 85);
                    FString Base64Image = FBase64::Encode(
                        CompressedImage.GetData(), CompressedImage.Num(), EBase64Mode::Standard);

                    AsyncTask(
                        ENamedThreads::GameThread,
                        [this, Image = MoveTemp(Image), Base64Image]() mutable
                        {
                            TargetImageTexture->Init(
                                Image.GetWidth(), Image.GetHeight(), PF_B8G8R8A8);

                            FTexture2DDynamicResource* TextureResource =
                                static_cast<FTexture2DDynamicResource*>(
                                    TargetImageTexture->GetResource());
                            if (TextureResource)
                            {
                                ENQUEUE_RENDER_COMMAND(FWriteRawDataToTexture)
                                (
                                    [this,
                                     TextureResource,
                                     RawData = MoveTemp(Image.RawData),
                                     Width = Image.GetWidth(),
                                     Height = Image.GetHeight(),
                                     Base64Image](
                                        FRHICommandListImmediate& RHICmdList)
                                    {
                                        TextureResource->WriteRawToTexture_RenderThread(RawData);
                                        AsyncTask(
                                            ENamedThreads::GameThread,
                                            [this, Width, Height, Base64Image]() {
                                                Completed.Broadcast(
                                                    Width,
                                                    Height,
                                                    Base64Image,
                                                    true);
                                            });
                                    });
                            }
                            else
                            {
                                Completed.Broadcast(0, 0, {}, false);
                            }
                        });
                    return;  // success
                }
            }
            AsyncTask(
                ENamedThreads::GameThread, [this]() { Completed.Broadcast(0, 0, {}, false); });
        });
}

UMTImageLoadAsyncAction* UMTImageLoadAsyncAction::AsyncImageLoad(
    UObject* WorldContextObject,
    const FString& FilePath,
    UTexture2DDynamic* TargetImageTexture)
{
    auto* Action = NewObject<UMTImageLoadAsyncAction>();
    Action->FilePath = FilePath;
    Action->TargetImageTexture = TargetImageTexture;
    Action->RegisterWithGameInstance(WorldContextObject);

    return Action;
}

void UMTLoadImageFromBase64AsyncAction::Activate()
{
    AsyncTask(
        ENamedThreads::GameThread,
        [this]() mutable
        {
            TArray64<uint8> DecodedImageData;

            DecodedImageData.SetNum(FBase64::GetDecodedDataSize(EncodedImage));
            FBase64::Decode(
                *EncodedImage,
                EncodedImage.Len(),
                DecodedImageData.GetData(),
                EBase64Mode::Standard);

            FImage UncompressedImage;
            if (FImageUtils::DecompressImage(
                    DecodedImageData.GetData(), DecodedImageData.Num(), UncompressedImage))
            {
                TargetImageTexture->Init(
                    UncompressedImage.SizeX, UncompressedImage.SizeY, PF_B8G8R8A8);

                FTexture2DDynamicResource* TextureResource =
                    static_cast<FTexture2DDynamicResource*>(TargetImageTexture->GetResource());
                if (TextureResource)
                {
                    ENQUEUE_RENDER_COMMAND(FWriteRawDataToTexture)
                    (
                        [this, TextureResource, Image = MoveTemp(UncompressedImage)](
                            FRHICommandListImmediate& RHICmdList)
                        {
                            TextureResource->WriteRawToTexture_RenderThread(Image.RawData);
                            AsyncTask(
                                ENamedThreads::GameThread,
                                [this, SizeX = Image.SizeX, SizeY = Image.SizeY]()
                                { Completed.Broadcast(SizeX, SizeY, {}, true); });
                        });
                }
                else
                {
                    Completed.Broadcast(0, 0, {}, false);
                }
            }
            else
            {
                Completed.Broadcast(0, 0, {}, false);
            }
        });
}

UMTLoadImageFromBase64AsyncAction* UMTLoadImageFromBase64AsyncAction::AsyncLoadImageFromBase64(
    UObject* WorldContextObject,
    const FString& EncodedImage,
    UTexture2DDynamic* TargetImageTexture)
{
    auto* Action = NewObject<UMTLoadImageFromBase64AsyncAction>();
    Action->EncodedImage = EncodedImage;
    Action->TargetImageTexture = TargetImageTexture;
    Action->RegisterWithGameInstance(WorldContextObject);

    return Action;
}

void UMTCapturePreviewImageAction::Activate()
{
    AsyncTask(
        ENamedThreads::GameThread,
        [this]()
        {
            CaptureActor = WorldContextObject->GetWorld()->SpawnActor<AMTSceneCapture>();
            CaptureActor->GetCaptureComponent2D()->TextureTarget =
                NewObject<UTextureRenderTarget2D>(CaptureActor->GetCaptureComponent2D());
            CaptureActor->GetCaptureComponent2D()->TextureTarget->CompressionSettings =
                TextureCompressionSettings::TC_Default;
            CaptureActor->GetCaptureComponent2D()->TextureTarget->TargetGamma = 2.2F;
            CaptureActor->GetCaptureComponent2D()->TextureTarget->bAutoGenerateMips = false;
            CaptureActor->GetCaptureComponent2D()->TextureTarget->CompressionSettings =
                TextureCompressionSettings::TC_Default;
            CaptureActor->GetCaptureComponent2D()->TextureTarget->AddressX =
                TextureAddress::TA_Clamp;
            CaptureActor->GetCaptureComponent2D()->TextureTarget->AddressY =
                TextureAddress::TA_Clamp;
            CaptureActor->GetCaptureComponent2D()->TextureTarget->RenderTargetFormat = RTF_RGBA8;
            CaptureActor->GetCaptureComponent2D()->TextureTarget->bGPUSharedFlag = true;
            CaptureActor->GetCaptureComponent2D()->TextureTarget->InitCustomFormat(
                512, 512, EPixelFormat::PF_B8G8R8A8, true);
            TargetImageTexture->Init(512, 512, PF_B8G8R8A8);

            auto* GeoRef =
                ACesiumGeoreference::GetDefaultGeoreference(WorldContextObject->GetWorld());

            GeoRef->SetOriginLongitudeLatitudeHeight(LongLatHeight);
            TActorIterator<ACesiumSunSky>(WorldContextObject->GetWorld())->UpdateSun();

            auto Location = GeoRef->TransformLongitudeLatitudeHeightPositionToUnreal(LongLatHeight);
            FRotator Rotation = GeoRef->TransformEastSouthUpRotatorToUnreal(
                FRotator(0, YawAtLocation, 0), Location);
            Rotation.Pitch = PitchAtLocation;

            CaptureActor->SetActorLocation(Location);
            CaptureActor->SetActorRotation(Rotation);

            WorldContextObject->GetWorld()
                ->GetFirstPlayerController()
                ->GetPawnOrSpectator()
                ->SetActorLocation(Location);
            WorldContextObject->GetWorld()
                ->GetFirstPlayerController()
                ->GetPawnOrSpectator()
                ->SetActorRotation(Rotation);
            WorldContextObject->GetWorld()->GetFirstPlayerController()->SetControlRotation(
                Rotation);

            WorldContextObject->GetWorld()->GetTimerManager().SetTimerForNextTick(
                [this]()
                {
                    FTimerHandle TimerHandle;
                    WorldContextObject->GetWorld()->GetTimerManager().SetTimer(
                        TimerHandle,
                        [this]()
                        {
                            WorldContextObject->GetWorld()->SendAllEndOfFrameUpdates();

                            CaptureActor->GetCaptureComponent2D()->UpdateSceneCaptureContents(
                                WorldContextObject->GetWorld()->Scene);

                            ENQUEUE_RENDER_COMMAND(FREadSurfaceDataAndWriteToDyanmicTexture)
                            (
                                [this](FRHICommandListImmediate& RHICmdList)
                                {
                                    TRACE_CPUPROFILER_EVENT_SCOPE(MTSampling::ReadSurface);

                                    const auto* Resource =
                                        CaptureActor->GetCaptureComponent2D()
                                            ->TextureTarget->GetRenderTargetResource();
                                    RHICmdList.ReadSurfaceData(
                                        Resource->GetRenderTargetTexture(),
                                        FIntRect(0, 0, Resource->GetSizeX(), Resource->GetSizeY()),
                                        CaptureActor->GetMutableImageDataRef(),
                                        FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX));

                                    FTexture2DDynamicResource* TextureResource =
                                        static_cast<FTexture2DDynamicResource*>(
                                            TargetImageTexture->GetResource());
                                    if (TextureResource)
                                    {
                                        const auto View = TArrayView64<uint8>(
                                            (uint8*)CaptureActor->GetMutableImageDataRef()
                                                .GetData(),
                                            512 * 512 * 4);
                                        TextureResource->WriteRawToTexture_RenderThread(View);
                                        AsyncTask(
                                            ENamedThreads::GameThread,
                                            [this]()
                                            {
                                                Completed.Broadcast(true);
                                                CaptureActor->Destroy();
                                            });
                                    }
                                    else
                                    {
                                        check(false);
                                        Completed.Broadcast(false);
                                    }
                                });
                        },
                        3,
                        false);
                });
        });
}

UMTCapturePreviewImageAction* UMTCapturePreviewImageAction::CapturePreviewImage(
    UObject* WorldContextObject,
    const FVector& LongLatHeight,
    double YawAtLocation,
    double PitchAtLocation,
    UTexture2DDynamic* TargetImageTexture)
{
    auto* Action = NewObject<UMTCapturePreviewImageAction>();
    Action->LongLatHeight = LongLatHeight;
    Action->YawAtLocation = YawAtLocation;
    Action->PitchAtLocation = PitchAtLocation;
    Action->TargetImageTexture = TargetImageTexture;
    Action->WorldContextObject = WorldContextObject;

    Action->RegisterWithGameInstance(WorldContextObject);

    return Action;
}

void UMTHttpJsonRequestAsyncAction::Activate()
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest = FHttpModule::Get().CreateRequest();
    HttpRequest->SetVerb(Verb);
    HttpRequest->SetHeader("Content-Type", "application/json");
    HttpRequest->SetURL(URL);
    HttpRequest->SetContentAsString(Body);

    HttpRequest->OnProcessRequestComplete().BindLambda(
        [this](FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
        {
            FString ResponseString = "";
            if (bSuccess)
            {
                ResponseString = Response->GetContentAsString();
            }

            this->HandleRequestCompleted(ResponseString, bSuccess);
        });

    HttpRequest->ProcessRequest();
}

UMTHttpJsonRequestAsyncAction* UMTHttpJsonRequestAsyncAction::AsyncJSONRequestHTTP(
    UObject* WorldContextObject,
    const FString& URL,
    const FString& Verb,
    const FString& Body)
{
    // Create Action Instance for Blueprint System
    auto* Action = NewObject<UMTHttpJsonRequestAsyncAction>();
    Action->URL = URL;
    Action->Verb = Verb;
    Action->Body = Body;
    Action->RegisterWithGameInstance(WorldContextObject);

    return Action;
}

void UMTHttpJsonRequestAsyncAction::HandleRequestCompleted(
    const FString& ResponseString,
    bool bSuccess)
{
    Completed.Broadcast(ResponseString, bSuccess);
}
