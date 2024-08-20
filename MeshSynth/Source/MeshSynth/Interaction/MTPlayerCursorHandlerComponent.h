// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MTInputHandler.h"
#include "MTPlayerPawnComponent.h"

#include "UObject/Object.h"
#include "MTPlayerCursorHandlerComponent.generated.h"

class UInputAction;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMTPlayerCursorActionDelegate, AMTPlayerController*, Instigator);

/**
 * 
 */
UCLASS()
class MESHSYNTH_API UMTPlayerCursorHandlerComponent : public UMTPlayerPawnComponent, public IMTInputHandler
{
	GENERATED_BODY()

public:
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	
	FHitResult TraceCursor() const;
	
	FMTPlayerCursorActionDelegate& OnPrimaryCursorAction();
	FMTPlayerCursorActionDelegate& OnSecondaryCursorAction();

private:
	UPROPERTY(EditAnywhere, Category=Input)
	TObjectPtr<UInputAction> PrimaryAction;

	UPROPERTY(EditAnywhere, Category=Input)
	TObjectPtr<UInputAction> SecondaryAction;

	UPROPERTY(BlueprintAssignable)
	FMTPlayerCursorActionDelegate PrimaryActionDelegate;

	UPROPERTY(BlueprintAssignable)
	FMTPlayerCursorActionDelegate SecondaryActionDelegate;
    
	void PrimaryActionHandler();

	void SecondaryActionHandler();
};
