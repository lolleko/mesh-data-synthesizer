// Fill out your copyright notice in the Description page of Project Settings.


#include "MTPlayerCursorHandlerComponent.h"

#include "EnhancedInputComponent.h"
#include "InputTriggers.h"
#include "MTPlayerController.h"

void UMTPlayerCursorHandlerComponent::SetupPlayerInputComponent(
	UInputComponent* InputComponent)
{
	auto* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);

	EnhancedInputComponent->BindAction(PrimaryAction, ETriggerEvent::Triggered, this, &ThisClass::PrimaryActionHandler);
	EnhancedInputComponent->BindAction(SecondaryAction, ETriggerEvent::Triggered, this, &ThisClass::SecondaryActionHandler);
}

FHitResult UMTPlayerCursorHandlerComponent::TraceCursor() const
{
	const auto* Controller = GetPlayerController();

	FHitResult OutHit;
	Controller->GetHitResultUnderCursor(ECollisionChannel::ECC_Visibility, false, OutHit);
	return OutHit;
}

FMTPlayerCursorActionDelegate& UMTPlayerCursorHandlerComponent::OnPrimaryCursorAction()
{
	return PrimaryActionDelegate;
}

FMTPlayerCursorActionDelegate& UMTPlayerCursorHandlerComponent::OnSecondaryCursorAction()
{
	return SecondaryActionDelegate;
}

void UMTPlayerCursorHandlerComponent::PrimaryActionHandler()
{
	OnPrimaryCursorAction().Broadcast(GetPlayerController());
}

void UMTPlayerCursorHandlerComponent::SecondaryActionHandler()
{
	OnSecondaryCursorAction().Broadcast(GetPlayerController());
}
