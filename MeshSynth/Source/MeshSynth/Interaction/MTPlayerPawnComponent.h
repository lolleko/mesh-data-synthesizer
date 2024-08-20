// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MTPlayerPawnComponent.generated.h"


class AMTPlayerPawn;
class AMTPlayerController;

UCLASS(ClassGroup=(Custom), Within=MTPlayerPawn, meta=(BlueprintSpawnableComponent))
class MESHSYNTH_API UMTPlayerPawnComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	AMTPlayerPawn* GetPlayerPawn() const;
	
	AMTPlayerController* GetPlayerController() const;
};
