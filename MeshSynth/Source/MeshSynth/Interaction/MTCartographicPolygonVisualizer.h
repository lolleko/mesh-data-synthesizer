// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CesiumCartographicPolygon.h"

#include "Components/InstancedStaticMeshComponent.h"

#include "MTCartographicPolygonVisualizer.generated.h"

UCLASS(Blueprintable)
class MESHSYNTH_API AMTCartographicPolygonVisualizer : public ACesiumCartographicPolygon
{
	GENERATED_BODY()
	
public:
	// Sets default values for this actor's properties
	AMTCartographicPolygonVisualizer();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

private:

	UPROPERTY(EditAnywhere, Category = "Visualizer")
    TObjectPtr<UStaticMesh> LineMesh;

    UPROPERTY(EditAnywhere, Category = "Visualizer")
    TObjectPtr<UStaticMesh> PointMesh;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> LineISMComponent;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> PointISMComponent;
};
