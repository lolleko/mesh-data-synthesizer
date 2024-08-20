// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/SceneCapture2D.h"

#include "MTSceneCapture.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MESHSYNTH_API AMTSceneCapture : public ASceneCapture2D
{
	GENERATED_BODY()
 
public:
	AMTSceneCapture();

    virtual TArray<FColor>& GetMutableImageDataRef();

    virtual FIntVector2& GetMutableImageSize();

private:
    // In the rare case that our capture gets deleted while beeing used on render thread
    UPROPERTY(Transient)
    TArray<FColor> ImageData;

    UPROPERTY(Transient)
    FIntVector2 ImageSize;
};
