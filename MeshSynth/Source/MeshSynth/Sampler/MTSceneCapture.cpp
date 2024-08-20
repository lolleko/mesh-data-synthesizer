// Fill out your copyright notice in the Description page of Project Settings.

#include "MTSceneCapture.h"

#include "Components/SceneCaptureComponent2D.h"

AMTSceneCapture::AMTSceneCapture()
{
    PrimaryActorTick.bCanEverTick = true;
    
    GetCaptureComponent2D()->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    GetCaptureComponent2D()->bCaptureEveryFrame = false;
    GetCaptureComponent2D()->bCaptureOnMovement = false;
    GetCaptureComponent2D()->FOVAngle = 80;
    GetCaptureComponent2D()->ShowFlags.SetPostProcessing(true);
    GetCaptureComponent2D()->ShowFlags.SetAntiAliasing(true);
    GetCaptureComponent2D()->ShowFlags.SetTemporalAA(false);
    GetCaptureComponent2D()->ShowFlags.SetSkyLighting(false);
    GetCaptureComponent2D()->ShowFlags.SetToneCurve(false);
    GetCaptureComponent2D()->ShowFlags.SetTonemapper(false);
    GetCaptureComponent2D()->ShowFlags.SetEyeAdaptation(false);
}

TArray<FColor>& AMTSceneCapture::GetMutableImageDataRef()
{
    return ImageData;
}

FIntVector2& AMTSceneCapture::GetMutableImageSize()
{
    return ImageSize;
}
