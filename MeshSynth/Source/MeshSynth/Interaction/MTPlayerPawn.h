// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MTPlayerPawn.generated.h"

class UMTCaptureComponent;
class USphereComponent;
class UMTPlayerCameraMovementComponent;
class UCameraComponent;
class UInputMappingContext;
class UMTPlayerCursorHandlerComponent;
class UInputComponent;

UCLASS()
class MESHSYNTH_API AMTPlayerPawn : public APawn
{
	GENERATED_BODY()

public:
	AMTPlayerPawn();
	
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	virtual void PawnClientRestart() override;

	UCameraComponent* GetOverviewCameraComponent();

    UCameraComponent* GetCaptureCameraComponent();
    
private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMTPlayerCameraMovementComponent> PlayerMovementComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UMTPlayerCursorHandlerComponent> PlayerCursorHandlerComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> PlayerSphereComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UCameraComponent> OverviewCameraComponent;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> CaptureCameraComponent;
	
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<UInputMappingContext> DefaultInputMapping;

	int32 CesiumPlayerSurroundingsCameraID;
};
