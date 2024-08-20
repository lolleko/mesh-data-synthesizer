// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "MTWayGraph.h"

#include "MTChinesePostMan.generated.h"

USTRUCT()
struct FMTWayGraphPath
{
    GENERATED_BODY()
    
    UPROPERTY()
    TArray<int32> Nodes;
};

/**
 *
 */
class MESHSYNTH_API FMTChinesePostMan
{
public:
    static TArray<FMTWayGraphPath>
    CalculatePathsThatContainAllEdges(const FMTWayGraph& Graph, const ACesiumGeoreference* GeoRef);
};