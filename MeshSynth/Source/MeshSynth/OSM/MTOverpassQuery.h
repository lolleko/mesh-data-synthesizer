// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "JsonObjectConverter.h"

#include "MTOverpassSchema.h"

namespace MTOverpass
{
    void AsyncQuery(const FString& OverpassQueryString, const FMTOverPassQueryCompletionDelegate& CompletionDelegate);
}

