// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CesiumGeoreference.h"
#include "CoreMinimal.h"
#include "MeshSynth/OSM/MTOverpassSchema.h"

#include "MTWayGraph.generated.h"

UENUM()
enum class EMTWay : int32
{
    None = INDEX_NONE,
    Motorway,
    Trunk,
    Primary,
    Secondary,
    Tertiary,
    Unclassified,
    Residential,
    Motorway_Link,
    Trunk_Link,
    Primary_Link,
    Secondary_Link,
    Tertiary_Link,
    LivingStreet,
    Pedestrian,
    Cycleway,
    Footway,
    Path,
    Service,
    Max
};

double EMTWayToScale(const EMTWay& Way);

UENUM()
enum class EMTEdgeDirection
{
    Forward = 1,
    Reverse = -1
};

USTRUCT()
struct FMTWayGraphNode
{
    GENERATED_BODY()
    
    UPROPERTY()
    FOverpassCoordinates Coords;
};

USTRUCT()
struct FMTWayGraphEdge
{
    GENERATED_BODY()
    
    UPROPERTY()
    int32 WayIndex;
};

USTRUCT()
struct FMTWayGraphWay
{
    GENERATED_BODY()
    
    UPROPERTY()
    FString Name;
    UPROPERTY()
    EMTWay Kind;
};

USTRUCT()
struct FMTWayGraphAdjacentNodesArrayWrapper
{
    GENERATED_BODY()
    
    UPROPERTY()
    TArray<int32> AdjacentNodes;
};

USTRUCT()
struct FMTWayGraph
{
    GENERATED_BODY()
    
    int64 NodePairToEdgeIndex(const int32 Node1, const int32 Node2) const;

    int32 AddWay(const FString& Name, const EMTWay Kind);

    EMTWay GetWayKind(const int32 WayIndex) const;

    void UpdateWayKind(const int32 WayIndex, const EMTWay Kind);

    FString GetWayName(const int32 WayIndex) const;

    int32 AddNode(const FOverpassCoordinates& Coords);

    void ConnectNodes(const int32 Node1, const int32 Node2, const int32 WayIndex);

    int32 GetEdgeWay(int64 EdgeIndex) const;

    int32 EdgeNum() const;

    int32 WayNum() const;

    TConstArrayView<int32> ViewNodesConnectedToNode(const int32 NodeIndex) const;

    FOverpassCoordinates GetNodeLocation(const int32 NodeIndex) const;

    FVector GetNodeLocationUnreal(const int32 NodeIndex, const ACesiumGeoreference* GeoRef) const;

    bool AreNodesConnected(const int32 NodeIndex1, const int32 NodeIndex2) const;

    int32 NodeNum() const;

private:
    UPROPERTY()
    TArray<FMTWayGraphNode> Nodes;

    UPROPERTY()
    TArray<FMTWayGraphAdjacentNodesArrayWrapper> AdjacencyList;
    
    UPROPERTY()
    TMap<int64, FMTWayGraphEdge> EdgeData;
    
    UPROPERTY()
    TArray<FMTWayGraphWay> Ways;

    friend void DrawDebugStreetGraph(
        const UWorld* World,
        const FMTWayGraph& WayGraph,
        const ACesiumGeoreference* Georeference);
};

void DrawDebugStreetGraph(
    const UWorld* World,
    const FMTWayGraph& WayGraph,
    const ACesiumGeoreference* Georeference);