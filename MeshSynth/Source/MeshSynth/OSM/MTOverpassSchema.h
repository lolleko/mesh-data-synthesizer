// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MTOverpassSchema.generated.h"

USTRUCT()
struct FOverpassCoordinates
{
    GENERATED_BODY()

    UPROPERTY()
    double Lat;

    UPROPERTY()
    double Lon;
};

/**
 *
 */
USTRUCT()
struct FOverpassElement
{
    GENERATED_BODY()

    UPROPERTY()
    int64 ID;

    UPROPERTY()
    TArray<int64> Nodes;

    UPROPERTY()
    TArray<FOverpassCoordinates> Geometry;

    UPROPERTY()
    TMap<FString, FString> Tags;
};

/**
 *
 */
USTRUCT()
struct FOverPassQueryResult
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<FOverpassElement> Elements;
};

DECLARE_DELEGATE_TwoParams(FMTOverPassQueryCompletionDelegate, const FOverPassQueryResult&, const bool);
