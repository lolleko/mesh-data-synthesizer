// Fill out your copyright notice in the Description page of Project Settings.


#include "MTPlayerCameraMovementComponent.h"

#include "EnhancedInputComponent.h"

#include "Camera/CameraComponent.h"

#include "Kismet/KismetMathLibrary.h"


UMTPlayerCameraMovementComponent::UMTPlayerCameraMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UMTPlayerCameraMovementComponent::SetupPlayerInputComponent(
    UInputComponent* InputComponent)
{
    auto* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(InputComponent);

    EnhancedInputComponent->BindAction(CameraZoomAction, ETriggerEvent::Triggered, this, &ThisClass::CameraZoomHandler);
    EnhancedInputComponent->BindAction(CameraRotateAction, ETriggerEvent::Triggered, this, &ThisClass::CameraRotateHandler);
    EnhancedInputComponent->BindAction(CameraMoveAction, ETriggerEvent::Triggered, this, &ThisClass::PlayerPawnMoveHandler);
}

void UMTPlayerCameraMovementComponent::SetUpdateAndCameraComponent(
    USceneComponent* NewUpdateComponent,
    UCameraComponent* NewCameraComponent)
{
    UpdatedComponent = NewUpdateComponent;
    CameraComponent = NewCameraComponent;
}

void UMTPlayerCameraMovementComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    TickUpdatedComponentMovement(static_cast<double>(DeltaTime));
    TickCameraMovement(static_cast<double>(DeltaTime));

    // DrawDebugSphere(GetWorld(), UpdatedComponent->GetComponentLocation(), 32.F, 8, FColor::Red);
    // DrawDebugDirectionalArrow(GetWorld(), CameraComponent->GetComponentLocation(), UpdatedComponent->GetComponentLocation(), 100.F, FColor::Orange);

    ConsumeInput();
}

void UMTPlayerCameraMovementComponent::CameraZoomHandler(const FInputActionValue& Value)
{
    // Retrieve as vector so we can keep double precision
    // Even though we only are using X
    CurrentInputValues.CameraZoom = Value.Get<FVector>().X;
}

void UMTPlayerCameraMovementComponent::CameraRotateHandler(const FInputActionValue& Value)
{
    CurrentInputValues.CameraRotate = Value.Get<FVector2D>();
}

void UMTPlayerCameraMovementComponent::PlayerPawnMoveHandler(const FInputActionValue& Value)
{
    CurrentInputValues.CameraMove = Value.Get<FVector2D>();
}

void UMTPlayerCameraMovementComponent::TickUpdatedComponentMovement(const double DeltaTime)
{
    const auto InputVelocity = FVector(CurrentInputValues.CameraMove * DeltaTime * CameraMoveSpeed, 0.);

    const auto Velocity = InputVelocity.X * UpdatedComponent->GetRightVector() + InputVelocity.Y * UpdatedComponent->GetForwardVector();

    const auto RotationYawDelta = CurrentInputValues.CameraRotate.X * CameraRotationSpeed * DeltaTime;

    const auto NewRotation = UpdatedComponent->GetRelativeRotation().Add(0., RotationYawDelta, 0.);
    
    UpdatedComponent->SetRelativeRotation(NewRotation);
    UpdatedComponent->ComponentVelocity = Velocity;
    UpdatedComponent->SetRelativeLocation(UpdatedComponent->GetComponentLocation() + Velocity);
}

void UMTPlayerCameraMovementComponent::TickCameraMovement(const double DeltaTime)
{
    const auto ZoomVelocity = CurrentInputValues.CameraZoom * CameraZoomSpeed * DeltaTime;
    
    CameraCurrentZoom = FMath::Clamp(CameraCurrentZoom + ZoomVelocity, CameraZoomDistanceRange.X, CameraZoomDistanceRange.Y);

    const auto CurrentZoomNormalized = UKismetMathLibrary::NormalizeToRange(CameraCurrentZoom, CameraZoomDistanceRange.X, CameraZoomDistanceRange.Y);

    const FVector CameraPositionAfterZoom = {-FMath::Lerp(CameraZoomDistanceRange.X, CameraZoomDistanceRange.Y, CurrentZoomNormalized), 0., 0.};

    const auto RotationPitchDelta = CurrentInputValues.CameraRotate.Y * CameraRotationSpeed * DeltaTime;

    CameraCurrentPitch =  FMath::Clamp(CameraCurrentPitch + RotationPitchDelta, CameraPitchRange.X, CameraPitchRange.Y);

    // We handle Zoom by moving on the X axis so we have to rotate around Y for pitch;
    const FVector CameraPositionAfterZoomAndPitch  = CameraPositionAfterZoom.RotateAngleAxis(CameraCurrentPitch, FVector::YAxisVector);

    CameraComponent->SetRelativeLocation(CameraPositionAfterZoomAndPitch);
    CameraComponent->SetRelativeRotation(FRotator(-CameraCurrentPitch, 0., 0.));
}

void UMTPlayerCameraMovementComponent::ConsumeInput()
{
    CurrentInputValues = {};
}
