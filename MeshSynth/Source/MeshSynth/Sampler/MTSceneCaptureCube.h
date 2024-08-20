// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/SceneCaptureCube.h"

#include "MTSceneCaptureCube.generated.h"

UCLASS()
class MESHSYNTH_API AMTSceneCaptureCube : public ASceneCaptureCube
{
    GENERATED_BODY()

public:
    AMTSceneCaptureCube();

    virtual TArray<FColor>& GetMutableImageDataRef();

    UTextureRenderTarget2D* GetRenderTargetLongLat();

private:
    // In the rare case that our capture gets deleted while beeing used on render thread
    TArray<FColor> ImageData;

    UPROPERTY()
    UTextureRenderTarget2D* RenderTargetLongLat;
};
