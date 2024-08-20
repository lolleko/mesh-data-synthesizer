// Fill out your copyright notice in the Description page of Project Settings.


#include "MTPlayerPawnComponent.h"

#include "MTPlayerController.h"
#include "MTPlayerPawn.h"

AMTPlayerPawn* UMTPlayerPawnComponent::GetPlayerPawn() const
{
	return CastChecked<AMTPlayerPawn>(GetOwner());
}

AMTPlayerController* UMTPlayerPawnComponent::GetPlayerController() const
{
	return CastChecked<AMTPlayerController>(GetPlayerPawn()->GetController());
}
