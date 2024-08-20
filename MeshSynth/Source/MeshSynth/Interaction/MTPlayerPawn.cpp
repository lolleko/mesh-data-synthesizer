// Fill out your copyright notice in the Description page of Project Settings.


#include "MTPlayerPawn.h"

#include "CesiumCameraManager.h"
#include "EnhancedInputSubsystems.h"
#include "MTPlayerCameraMovementComponent.h"
#include "MTPlayerCursorHandlerComponent.h"

#include "Blueprint/UserWidget.h"

#include "Camera/CameraComponent.h"

#include "Components/SphereComponent.h"

AMTPlayerPawn::AMTPlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	PlayerSphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("PlayerSphere"));
	RootComponent = PlayerSphereComponent;

	OverviewCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	OverviewCameraComponent->SetupAttachment(RootComponent);

    CaptureCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("CaptureCamera"));
    CaptureCameraComponent->SetupAttachment(RootComponent);
    CaptureCameraComponent->SetActive(false);
	
	PlayerMovementComponent = CreateDefaultSubobject<UMTPlayerCameraMovementComponent>(TEXT("PlayerCameraMovement"));
	PlayerMovementComponent->SetUpdateAndCameraComponent(PlayerSphereComponent, OverviewCameraComponent);

	PlayerCursorHandlerComponent = CreateDefaultSubobject<UMTPlayerCursorHandlerComponent>(TEXT("PlayerCursorHandler"));
}

void AMTPlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	PlayerMovementComponent->SetupPlayerInputComponent(PlayerInputComponent);
	PlayerCursorHandlerComponent->SetupPlayerInputComponent(PlayerInputComponent);
}

void AMTPlayerPawn::PawnClientRestart()
{
	Super::PawnClientRestart();

	const auto* PC = CastChecked<APlayerController>(GetController());
	auto* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
	check(IsValid(Subsystem));
    
	Subsystem->ClearAllMappings();

	Subsystem->AddMappingContext(DefaultInputMapping, 0);
}

UCameraComponent* AMTPlayerPawn::GetOverviewCameraComponent()
{
	return OverviewCameraComponent;
}

UCameraComponent* AMTPlayerPawn::GetCaptureCameraComponent()
{
    return CaptureCameraComponent;
}


