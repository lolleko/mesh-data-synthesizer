// Fill out your copyright notice in the Description page of Project Settings.

#include "MTHUD.h"

#include "Algo/Copy.h"

#include "Blueprint/UserWidget.h"


AMTHUD::AMTHUD()
{
	PrimaryActorTick.bCanEverTick = true;
}

void AMTHUD::BeginPlay()
{
	Super::BeginPlay();

	auto* Menu = CreateWidget(GetWorld(), MenuWidget);
	
	Menu->AddToViewport();
}


