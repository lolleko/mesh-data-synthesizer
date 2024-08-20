// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "GameFramework/HUD.h"

#include "MeshSynth/Sampler/MTSamplerComponentBase.h"

#include "MTHUD.generated.h"

UCLASS()
class MESHSYNTH_API AMTHUD : public AHUD
{
	GENERATED_BODY()

public:
	AMTHUD();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UUserWidget> MenuWidget;

};
