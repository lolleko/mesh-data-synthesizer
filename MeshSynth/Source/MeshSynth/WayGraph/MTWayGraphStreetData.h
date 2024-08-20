// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MeshSynth/Waygraph/MTWayGraph.h"
#include "MeshSynth/Waygraph/MTChinesePostMan.h"

#include "MTWayGraphStreetData.generated.h"

USTRUCT()
struct FMTStreetData
{
    GENERATED_BODY()

    UPROPERTY()
    FMTWayGraph Graph;

    UPROPERTY()
    TArray<FMTWayGraphPath> Paths;

    UPROPERTY()
    double TotalPathLength = 0.;
};
