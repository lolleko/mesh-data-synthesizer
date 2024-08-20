// Fill out your copyright notice in the Description page of Project Settings.


#include "MTWayGraphSamplerConfig.h"

FString UMTWayGraphSamplerConfig::GetConfigName() const
{
    return FString::Printf(TEXT("Config_%s"), *GetName());
}

double UMTWayGraphSamplerConfig::GetMinDistanceBetweenSamples() const
{
    return SampleDistance * 0.5;
}
