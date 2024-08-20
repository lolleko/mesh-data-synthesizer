// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IHttpRequest.h"
#include "MTGameModeBase.h"

#include "MTShowcaseGameMode.generated.h"

/**
 * 
 */
UCLASS()
class MESHSYNTH_API AMTShowcaseGameMode : public AMTGameModeBase
{
    GENERATED_BODY()


protected:
    virtual void BeginPlay() override;
    
};
