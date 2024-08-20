// Fill out your copyright notice in the Description page of Project Settings.

#include "MTSceneCaptureCube.h"

#include "Components/SceneCaptureComponentCube.h"
#include "Engine/TextureRenderTarget2D.h"

// Sets default values
AMTSceneCaptureCube::AMTSceneCaptureCube()
{
    PrimaryActorTick.bCanEverTick = true;

    GetCaptureComponentCube()->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    GetCaptureComponentCube()->bCaptureEveryFrame = false;
    GetCaptureComponentCube()->bCaptureOnMovement = false;
    GetCaptureComponentCube()->ShowFlags.SetPostProcessing(true);
    GetCaptureComponentCube()->ShowFlags.SetAntiAliasing(true);
    GetCaptureComponentCube()->ShowFlags.SetTemporalAA(false);
    GetCaptureComponentCube()->ShowFlags.SetSkyLighting(false);
    GetCaptureComponentCube()->ShowFlags.SetToneCurve(false);
    GetCaptureComponentCube()->ShowFlags.SetTonemapper(false);
    GetCaptureComponentCube()->ShowFlags.SetEyeAdaptation(false);
    GetCaptureComponentCube()->bCaptureRotation = true;

    RenderTargetLongLat = NewObject<UTextureRenderTarget2D>();
    check(RenderTargetLongLat);
    RenderTargetLongLat->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
    RenderTargetLongLat->TargetGamma = 0;
}

TArray<FColor>& AMTSceneCaptureCube::GetMutableImageDataRef()
{
    return ImageData;
}

UTextureRenderTarget2D* AMTSceneCaptureCube::GetRenderTargetLongLat()
{
    return RenderTargetLongLat;
}


