// Fill out your copyright notice in the Description page of Project Settings.


#include "MTOverpassConverter.h"

#include "CesiumCartographicPolygon.h"
#include "GeomTools.h"

namespace MTOverpass
{
    EMTWay WayStringToEnum(const FString& Way)
    {
        return static_cast<EMTWay>(FindObject<UEnum>(ANY_PACKAGE, TEXT("EMTWay"))->GetValueByName(FName(*Way)));
    };

    FMTWayGraph CreateStreetGraphFromQuery(const FOverPassQueryResult& Query, const ACesiumCartographicPolygon* BoundingPolygon)
    {
        TArray<FVector> BoundingPolyline;
        TArray<FVector2D> BoundingPolyline2D;
        FBox2d BoundingBox2D;
        
        if (BoundingPolygon)
        {

            
            BoundingPolygon->Polygon->ConvertSplineToPolyLine(ESplineCoordinateSpace::World, 1., BoundingPolyline);
            BoundingPolyline2D.Reserve(BoundingPolyline.Num());
            for (const auto& Vert : BoundingPolyline)
            {
                BoundingPolyline2D.Add(FVector2D(Vert.X, Vert.Y));
            }

            BoundingPolyline2D.RemoveAtSwap(BoundingPolyline2D.Num() - 1);

            BoundingBox2D = FBox2d(BoundingPolyline2D);
        }

        TMap<int64, int32> OSMNodeIDToNodeIndex;
        TMap<FString, int32> OSMWayNameToWayIndex;

        int32 CurrentWayIndex = 0;
        
        FMTWayGraph Result;
    
        for (const auto& Element : Query.Elements)
        {
            for (int32 NodeIndex = 0; NodeIndex < Element.Nodes.Num(); NodeIndex++)
            {
                const auto NodeID  = Element.Nodes[NodeIndex];
                
                if (!OSMNodeIDToNodeIndex.Contains(Element.Nodes[NodeIndex]))
                {
                    if (!BoundingPolygon) {
                        OSMNodeIDToNodeIndex.Add(NodeID, Result.AddNode(Element.Geometry[NodeIndex]));
                    }
                    else
                    {
                        const auto TestPoint3D = ACesiumGeoreference::GetDefaultGeoreference(BoundingPolygon)->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(Element.Geometry[NodeIndex].Lon, Element.Geometry[NodeIndex].Lat, 0.));
                        const auto TestPoint2D = FVector2D(TestPoint3D.X, TestPoint3D.Y);
                        if (BoundingBox2D.IsInsideOrOn(TestPoint2D) && FGeomTools2D::IsPointInPolygon(TestPoint2D, BoundingPolyline2D))
                        {
                            OSMNodeIDToNodeIndex.Add(NodeID, Result.AddNode(Element.Geometry[NodeIndex]));
                        }
                    }
                }
            }
        }
        
        for (const auto& Element : Query.Elements)
        {
            for (int32 NodeIndex = 0; NodeIndex < Element.Nodes.Num() - 1; NodeIndex++)
            {
                const auto NodeID  = Element.Nodes[NodeIndex];

                const auto NextNodeIndex = NodeIndex + 1;
                const auto NextNodeID  = Element.Nodes[NextNodeIndex];
                
                const auto WayName = Element.Tags.Contains("name") ? Element.Tags["name"] : FString();
                const auto WayType = Element.Tags["highway"];
                const auto WayKind = static_cast<int32>(WayStringToEnum(*WayType)) != -1 ? WayStringToEnum(WayType) : EMTWay::Unclassified;

                if (!OSMWayNameToWayIndex.Contains(WayName))
                {
                    Result.AddWay(WayName, WayKind);
                    OSMWayNameToWayIndex.Add(WayName, CurrentWayIndex);
                    CurrentWayIndex++;
                }

                const auto WayIndex = OSMWayNameToWayIndex[WayName];
                if (WayKind != EMTWay::Unclassified && Result.GetWayKind(WayIndex) == EMTWay::Unclassified)
                {
                    Result.UpdateWayKind(WayIndex, WayKind);
                }

                if(OSMNodeIDToNodeIndex.Contains(NodeID) && OSMNodeIDToNodeIndex.Contains(NextNodeID) && OSMNodeIDToNodeIndex[NodeID] != OSMNodeIDToNodeIndex[NextNodeID] && !Result.AreNodesConnected(OSMNodeIDToNodeIndex[NodeID], OSMNodeIDToNodeIndex[NextNodeID]))
                {
                    Result.ConnectNodes(OSMNodeIDToNodeIndex[NodeID], OSMNodeIDToNodeIndex[NextNodeID], WayIndex);
                }
            }
        }

        return Result;
    }

    FString BuildQueryStringFromBoundingPolygon(const ACesiumCartographicPolygon* BoundingPolygon)
    {
        const auto PolygonGlobalSpaceVertices = BoundingPolygon->CreateCartographicPolygon(FTransform::Identity).getVertices();

        FStringBuilderBase PolygonStringBuilder;
        PolygonStringBuilder << TEXT("(poly:'");
        for (const auto& Vertex : PolygonGlobalSpaceVertices)
        {
            PolygonStringBuilder << FString::SanitizeFloat(FMath::RadiansToDegrees(Vertex.y));
            PolygonStringBuilder << TEXT(" ");
            PolygonStringBuilder << FString::SanitizeFloat(FMath::RadiansToDegrees(Vertex.x));
            PolygonStringBuilder << TEXT(" ");

        }
        // Remove last space
        PolygonStringBuilder.RemoveSuffix(1);
        PolygonStringBuilder << TEXT("');");

        return FString::Printf(TEXT("[out:json][timeout:600];way['highway'~'motorway|motorway_link|trunk|trunk_link|primary|primary_link|secondary|secondary_link|tertiary|tertiary_link|service|residential|living_street|pedestrian|unclassified|cycleway|footway|path']['covered'!~'yes'][!'tunnel']%sout geom;"), PolygonStringBuilder.ToString());
    }
}
