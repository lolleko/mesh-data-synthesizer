// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MTInputHandler.h"
#include "MTPlayerPawnComponent.h"

#include "MTPlayerCameraMovementComponent.generated.h"

struct FInputActionValue;
class UCameraComponent;
class UInputAction;

/**
 * 
 */
UCLASS()
class MESHSYNTH_API UMTPlayerCameraMovementComponent : public UMTPlayerPawnComponent, public IMTInputHandler
{
	GENERATED_BODY()

public:

	UMTPlayerCameraMovementComponent();

	void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

	void SetUpdateAndCameraComponent(USceneComponent* NewUpdateComponent, UCameraComponent* NewCameraComponent);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(EditAnywhere, Category=Input)
	TObjectPtr<UInputAction> CameraZoomAction;

	UPROPERTY(EditAnywhere, Category=Input)
	TObjectPtr<UInputAction> CameraRotateAction;
    
	UPROPERTY(EditAnywhere, Category=Input)
	TObjectPtr<UInputAction> CameraMoveAction;
    
	// in cm per second
	UPROPERTY(EditAnywhere, Category=Camera)
	double CameraZoomSpeed = 500.;

	UPROPERTY(EditAnywhere, Category=Camera)
	FVector2D CameraZoomDistanceRange = {100., 3000.};
    
	UPROPERTY(EditAnywhere, Category=Camera)
	FVector2D CameraPitchRange = {20., 85.};

	UPROPERTY(EditAnywhere, Category=Camera)
	double CameraRotationSpeed = 200.;

	UPROPERTY(EditAnywhere, Category=Camera)
	double CameraMoveSpeed = 1000.;
    
	double CameraCurrentZoom = CameraZoomDistanceRange.X;

	double CameraCurrentPitch = CameraPitchRange.X;
    
	struct FRawInputValues
	{
		double CameraZoom = 0.;
		FVector2D CameraRotate = FVector2D::ZeroVector;
		FVector2D CameraMove = FVector2D::ZeroVector;
	};
	FRawInputValues CurrentInputValues;

	TObjectPtr<USceneComponent> UpdatedComponent;
    
	TObjectPtr<UCameraComponent> CameraComponent;
     
	void CameraZoomHandler(const FInputActionValue& Value);

	void CameraRotateHandler(const FInputActionValue& Value);

	void PlayerPawnMoveHandler(const FInputActionValue& Value);

	void TickUpdatedComponentMovement(const double DeltaTime);
    
	void TickCameraMovement(const double DeltaTime);

	void ConsumeInput();
};
