// Fill out your copyright notice in the Description page of Project Settings.

#include "MTSamplingFunctionLibrary.h"

#include "CesiumGeoreference.h"
#include "CubemapUnwrapUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "MTSample.h"

FRandomStream UMTSamplingFunctionLibrary::RandomStream;

const FRandomStream& UMTSamplingFunctionLibrary::GetRandomStream()
{
    return RandomStream;
}

void UMTSamplingFunctionLibrary::SetRandomSeed(const int32 NewSeed)
{
    RandomStream.Initialize(NewSeed);
}

FVector UMTSamplingFunctionLibrary::SampleUniformDirOnPositiveHemisphere(const float Bias)
{
    // uniform random direction in positive Unit hemisphere
    const auto RotationXi1 = GetRandomStream().FRand();
    const auto RotationXi2 = GetRandomStream().FRand();

    // For full hemisphere we would use 2*Xi1
    const auto Theta = FMath::Acos(FMath::Pow(1. - RotationXi1, Bias));
    const auto Phi = 2. * PI * RotationXi2;

    double ThetaSin;
    double ThetaCos;
    FMath::SinCos(&ThetaSin, &ThetaCos, Theta);

    double PhiSin;
    double PhiCos;
    FMath::SinCos(&PhiSin, &PhiCos, Phi);

    return FVector(ThetaSin * PhiCos, ThetaSin * PhiSin, ThetaCos);
}

bool UMTSamplingFunctionLibrary::IsViewObstructed(
    const UWorld* World,
    const FTransform& SampleTransform,
    const float MinClearance,
    const float SweepSphereRadius)
{
    FHitResult OutHit;
    World->SweepSingleByChannel(
        OutHit,
        SampleTransform.GetLocation(),
        SampleTransform.GetLocation() + SampleTransform.Rotator().Vector() * UE_FLOAT_HUGE_DISTANCE,
        FQuat::Identity,
        ECC_Visibility,
        FCollisionShape::MakeSphere(SweepSphereRadius));

    if (OutHit.bStartPenetrating)
    {
        return true;
    }

    if (OutHit.Distance < MinClearance)
    {
        return true;
    }

    if (!OutHit.bBlockingHit)
    {
        return true;
    }

    return false;
}

bool UMTSamplingFunctionLibrary::ReadPixelsFromRenderTarget(
    UTextureRenderTarget2D* RenderTarget2D,
    TArray<FColor>& PixelBuffer)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(UMTSamplingFunctionLibrary::ReadPixelsFromRenderTarget);

    FTextureRenderTargetResource* RenderTargetResource =
        RenderTarget2D->GameThread_GetRenderTargetResource();

    FReadSurfaceDataFlags ReadSurfaceDataFlags(RCM_UNorm);
    // ReadSurfaceDataFlags.SetLinearToGamma(true);

    PixelBuffer.SetNum(RenderTarget2D->SizeX * RenderTarget2D->SizeY);
    ensure(RenderTargetResource->ReadPixelsPtr(PixelBuffer.GetData(), ReadSurfaceDataFlags));

    return true;
}

TFuture<UMTSamplingFunctionLibrary::FImageWriteResult>
UMTSamplingFunctionLibrary::WritePixelBufferToFile(
    const FString& FilePath,
    const TArray<FColor>& PixelBuffer,
    const FIntVector2& Size)
{
    return Async(
        EAsyncExecution::Thread,
        [PixelBufferCopy = PixelBuffer, FilePath, Size]()
        {
            IImageWrapperModule& ImageWrapperModule =
                FModuleManager::GetModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

            TArray64<uint8> ImgData;
            ImageWrapperModule.CompressImage(
                ImgData,
                EImageFormat::JPEG,
                FImageView(
                    (uint8*)(PixelBufferCopy.GetData()), Size.X, Size.Y, ERawImageFormat::BGRA8),
                85);
            FFileHelper::SaveArrayToFile(ImgData, *FilePath);

            return FImageWriteResult{FilePath, ImgData};
        });
}

TFuture<UMTSamplingFunctionLibrary::FImageWriteResult>
UMTSamplingFunctionLibrary::WriteCubeMapPixelBufferToFile(
    const FString& FilePath,
    const TArray<FColor>& PixelBuffer,
    const FIntVector2& Size,
    const FIntVector2& TopAndBottomCrop)
{
    return Async(
        EAsyncExecution::Thread,
        [PixelBufferCopy = PixelBuffer, FilePath, Size, TopAndBottomCrop]()
        {
            IImageWrapperModule& ImageWrapperModule =
                FModuleManager::GetModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));

            TArray64<uint8> ImgData;
            ImageWrapperModule.CompressImage(
                ImgData,
                EImageFormat::JPEG,
                FImageView(
                    (uint8*)(PixelBufferCopy.GetData() + TopAndBottomCrop.X * Size.X),
                    Size.X,
                    Size.Y - TopAndBottomCrop.X - TopAndBottomCrop.Y,
                    ERawImageFormat::BGRA8),
                85);
            FFileHelper::SaveArrayToFile(ImgData, *FilePath);

            return FImageWriteResult{FilePath, ImgData};
        });
}

TArray<UMTSamplingFunctionLibrary::FLocationPathPair>
UMTSamplingFunctionLibrary::PanoramaLocationsFromCosPlaceCSV(
    const FString& FilePath,
    const ACesiumGeoreference* Georeference)
{
    TArray<FString> CSVLines;
    FFileHelper::LoadFileToStringArray(CSVLines, *FilePath);

    TArray<FLocationPathPair> Result;

    for (const auto& Line : MakeArrayView(CSVLines).Slice(1, CSVLines.Num() - 1))
    {
        TArray<FString> CSVRowEntries;
        Line.ParseIntoArray(CSVRowEntries, TEXT(","));
        // PATH,UTM_EAST,UTM_NORTH,UTM_ZONE_NUMBER,UTM_ZONE_LETTER,LAT,LON,PANO_ID,HEADING,YEAR,MONTH,ALTITUDE,PITCH,ROLL
        const auto Path = CSVRowEntries[0];
        const auto Lat = FCString::Atod(*CSVRowEntries[5]);
        const auto Lon = FCString::Atod(*CSVRowEntries[6]);
        const auto Alt = FCString::Atod(*CSVRowEntries[11]) - 27.5;
        const auto Heading = FCString::Atod(*CSVRowEntries[8]);
        const auto Pitch = FCString::Atod(*CSVRowEntries[12]);
        const auto Roll = FCString::Atod(*CSVRowEntries[13]);

        const FVector UnrealLocation =
            Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector{Lon, Lat, Alt});
        const FRotator HeadingRotation = FRotator{Pitch - 90, Heading - 90, Roll};

        const FTransform Transform{HeadingRotation, UnrealLocation};

        Result.Add({Transform, Path});
    }

    return Result;
}

TArray<UMTSamplingFunctionLibrary::FLocationPathPair>
UMTSamplingFunctionLibrary::SliceLocationsFromPittsTXT(
    const FString& FilePath,
    const ACesiumGeoreference* Georeference)
{
    TArray<FString> CSVLines;
    FFileHelper::LoadFileToStringArray(CSVLines, *FilePath);

    TArray<FLocationPathPair> Result;

    for (const auto& Line : MakeArrayView(CSVLines).Slice(1, CSVLines.Num() - 1))
    {
        if (!Line.Contains(TEXT("database")))
        {
            continue;
        }

        const auto PathWithoutExtension = FPaths::GetBaseFilename(Line);

        // @ UTM_east @ UTM_north @ UTM_zone_number @ UTM_zone_letter @ latitude @ longitude @
        // pano_id @ tile_num @ heading @ pitch @ roll @ height @ timestamp @ note @ extension
        TArray<FString> PathParts;
        PathWithoutExtension.ParseIntoArray(PathParts, TEXT("@"), false);

        // get long lat alt heading pitch roll
        const auto Lat = FCString::Atod(*PathParts[5]);
        const auto Lon = FCString::Atod(*PathParts[6]);

        const FVector UnrealLocation =
            Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector{Lon, Lat, 0.});

        const auto PitchYawString = PathParts[14];
        FString PitchString;
        FString YawString;
        PitchYawString.Split(TEXT("_"), &PitchString, &YawString);
        PitchString.RemoveFromStart(TEXT("pitch"));
        YawString.RemoveFromStart(TEXT("yaw"));

        const auto Pitch = FCString::Atoi(*PitchString) == 1 ? 4 : 26.5;
        const auto Yaw = FCString::Atoi(*YawString) * 30.;

        FRotator HeadingRotation = FRotator{Pitch, Yaw - 180., 0.};

        const FTransform Transform{HeadingRotation, UnrealLocation};

        Result.Add({Transform, Line});
    }

    return Result;
}

TArray<UMTSamplingFunctionLibrary::FLocationPathPair>
UMTSamplingFunctionLibrary::SliceLocationsFromTokyoTXT(
    const FString& FilePath,
    const ACesiumGeoreference* Georeference)
{
    TArray<FString> CSVLines;
    FFileHelper::LoadFileToStringArray(CSVLines, *FilePath);

    TArray<FLocationPathPair> Result;

    for (const auto& Line : MakeArrayView(CSVLines).Slice(1, CSVLines.Num() - 1))
    {
        if (!Line.Contains(TEXT("database")))
        {
            continue;
        }

        const auto PathWithoutExtension = FPaths::GetBaseFilename(Line);

        // @ UTM_east @ UTM_north @ UTM_zone_number @ UTM_zone_letter @ latitude @ longitude @
        // pano_id @ tile_num @ heading @ pitch @ roll @ height @ timestamp @ note @ extension
        TArray<FString> PathParts;
        PathWithoutExtension.ParseIntoArray(PathParts, TEXT("@"), false);

        // get long lat alt heading pitch roll
        const auto Lat = FCString::Atod(*PathParts[5]);
        const auto Lon = FCString::Atod(*PathParts[6]);

        const FVector UnrealLocation =
            Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector{Lon, Lat, 0.});

        const auto TileNum = PathParts[8];
        
        const auto Pitch = 12.;
        const auto Yaw = FCString::Atoi(*TileNum) * 30.;

        FRotator HeadingRotation = FRotator{Pitch, Yaw - 90., 0.};

        const FTransform Transform{HeadingRotation, UnrealLocation};

        Result.Add({Transform, Line});
    }

    return Result;
}

TArray<UMTSamplingFunctionLibrary::FExtraViewData>
UMTSamplingFunctionLibrary::PredictionsLocationsFromFile(
    const FString& FilePath,
    const ACesiumGeoreference* Georeference)
{
    TArray<FExtraViewData> Result;

    FString JSONRaw;
    FFileHelper::LoadFileToString(JSONRaw, *FilePath);

    const TSharedRef<TJsonReader<>> Reader = FJsonStringReader::Create(MoveTemp(JSONRaw));

    TArray<TSharedPtr<FJsonValue>> QueryInfos;
    if (!FJsonSerializer::Deserialize(Reader, QueryInfos))
    {
        check(false);
    }

    for (const auto& QueryInfo : QueryInfos)
    {
        const auto& QueryInfoObject = QueryInfo->AsObject();
        const auto& QueryInfoPredications = QueryInfoObject->GetArrayField(TEXT("predictions"));
        const auto& SharedViewOutDir = QueryInfoObject->GetStringField(TEXT("shared_extra_view_dir"));
        const auto& QueryTempDatabaseImageList = QueryInfoObject->GetStringField(TEXT("database_image_list"));

        const int32 Width = FCString::Atoi(*QueryInfoObject->GetStringField(TEXT("width")));
        const int32 Height = FCString::Atoi(*QueryInfoObject->GetStringField(TEXT("height")));

        for (const auto& PredictionJsonValue : QueryInfoPredications)
        {
            const auto PredictionPath = PredictionJsonValue->AsString();

            const auto PredictionPathFileName = FPaths::GetCleanFilename(PredictionPath);

            // @ UTM_east @ UTM_north @ UTM_zone_number @ UTM_zone_letter @ latitude @ longitude @
            // pano_id @ tile_num @ heading @ pitch @ roll @ height @ timestamp @ note @ extension
            TArray<FString> PredictionPathParts;
            PredictionPathFileName.ParseIntoArray(PredictionPathParts, TEXT("@"), false);

            // get long lat alt heading pitch roll
            const auto Lat = FCString::Atod(*PredictionPathParts[5]);
            const auto Lon = FCString::Atod(*PredictionPathParts[6]);

            const FVector UnrealLocation =
                Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(
                    FVector{Lon, Lat, 0.});

            const auto PitchYawString = PredictionPathParts[14];
            FString PitchString;
            FString YawString;
            PitchYawString.Split(TEXT("_"), &PitchString, &YawString);
            PitchString.RemoveFromStart(TEXT("pitch"));
            YawString.RemoveFromStart(TEXT("yaw"));

            const auto Pitch = FCString::Atod(*PitchString);
            const auto Yaw = FCString::Atod(*YawString);

            FRotator HeadingRotation = FRotator{Pitch, Yaw, 0.};

            const FTransform Transform{HeadingRotation, UnrealLocation};

            Result.Add(
                {Transform, PredictionPathFileName, SharedViewOutDir, QueryTempDatabaseImageList, {Width, Height}});
        }
    }

    return Result;
}

FString UMTSamplingFunctionLibrary::CreateImageNameFromSample(const FMTSample& Sample)
{
    // @ UTM_east @ UTM_north @ UTM_zone_number @ UTM_zone_letter @ latitude @ longitude @
    // pano_id @ tile_num @ heading @ pitch @ roll @ height @ timestamp @ note @ extension
    double UTMEast;
    double UTMNorth;
    TCHAR ZoneLetter;
    int32 ZoneNumber;
    UTM::LLtoUTM(
        Sample.LonLatAltitude.X,
        Sample.LonLatAltitude.Y,
        UTMNorth,
        UTMEast,
        ZoneNumber,
        ZoneLetter);

    // const auto TimeStamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));

    return FString::Printf(
        TEXT("@%.2f@%.2f@%d@%s@%.5f@%.5f@%d@%s@%.2f@%.2f@%.2f@%.2f@%s@art_prob_%.4f@.jpg"),
        UTMEast,
        UTMNorth,
        ZoneNumber,
        *FString().AppendChar(ZoneLetter),
        Sample.LonLatAltitude.Y + 0.,
        Sample.LonLatAltitude.X + 0.,
        Sample.ImageID,
        TEXT(""),
        Sample.HeadingAngle + 0.,
        Sample.Pitch + 0.,
        Sample.Roll + 0.,
        Sample.LonLatAltitude.Z + 0.,
        TEXT(""),  // TimeStamp
        Sample.ArtifactProbability + 0.);
}
