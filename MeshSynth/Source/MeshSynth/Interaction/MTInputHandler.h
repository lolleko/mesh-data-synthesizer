// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "MTInputHandler.generated.h"

UINTERFACE()
class UMTInputHandler : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class MESHSYNTH_API IMTInputHandler
{
	GENERATED_BODY()

    virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) = 0;
public:
};
