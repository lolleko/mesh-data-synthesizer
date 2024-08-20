// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "../WayGraph/MTWayGraph.h"


class ACesiumCartographicPolygon;

namespace MTOverpass
{
    EMTWay WayStringToEnum(const FString& Way);
    
    FMTWayGraph CreateStreetGraphFromQuery(const FOverPassQueryResult& Query, const ACesiumCartographicPolygon* BoundingPolygon = nullptr);

    FString BuildQueryStringFromBoundingPolygon(const ACesiumCartographicPolygon* BoundingPolygon);
}
