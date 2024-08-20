// Fill out your copyright notice in the Description page of Project Settings.

#include "MTChinesePostMan.h"

#include "Algo/Count.h"
#include "Algo/MinElement.h"
#include "Algo/Unique.h"

namespace
{
    struct FOddToOddPath
    {
        int32 StartOddIndex;
        int32 EndOddIndex;
        double Distance;

        bool operator<(const FOddToOddPath& Other) const
        {
            return Distance < Other.Distance;
        }
    };
    
    void DFSUtil(
        const FMTWayGraph& Graph,
        int32 NodeIndex,
        TArray<bool>& InOutVisited,
        TArray<int32>& OutIsland,
        TArray<int32>& OutIslandOddNodes)
    {
        InOutVisited[NodeIndex] = true;
        OutIsland.Add(NodeIndex);

        const auto ConnectedNodes = Graph.ViewNodesConnectedToNode(NodeIndex);

        if (ConnectedNodes.Num() % 2 != 0)
        {
            OutIslandOddNodes.Add(NodeIndex);
        }

        for (const auto& ConnectedNodeIndex : ConnectedNodes)
        {
            if (!InOutVisited[ConnectedNodeIndex])
            {
                DFSUtil(Graph, ConnectedNodeIndex, InOutVisited, OutIsland, OutIslandOddNodes);
            }
        }
    }

    void FindIslands(
        const FMTWayGraph& Graph,
        TArray<TArray<int32>>& OutIslands,
        TArray<TArray<int32>>& OutIslandOddNodes)
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(FindIslands);

        TArray<bool> Visited;
        Visited.SetNumZeroed(Graph.NodeNum());

        for (int NodeIndex = 0; NodeIndex < Graph.NodeNum(); NodeIndex++)
        {
            if (Visited[NodeIndex] == false)
            {
                DFSUtil(
                    Graph,
                    NodeIndex,
                    Visited,
                    OutIslands.Emplace_GetRef(),
                    OutIslandOddNodes.Emplace_GetRef());
            }
        }
    }

    // Returns path in resever sicne we dotn really care about path direction in CchinesPP
    bool Dijsktra(
        const FMTWayGraph& Graph,
        const int32 StartNode,
        TArray<double>& DistanceCache,
        const TMap<int64, double>& WeightCache,
        TArray<int32>& PrevCache)
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Dijsktra);

        TArray<int32> MinQueue;

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(InitStorage);
            DistanceCache.SetNumUninitialized(Graph.NodeNum(), false);
            PrevCache.SetNumUninitialized(Graph.NodeNum(), false);

            for (int32 I = 0; I < Graph.NodeNum(); ++I)
            {
                DistanceCache[I] = DBL_MAX;
                PrevCache[I] = INDEX_NONE;
            }
        }

        DistanceCache[StartNode] = 0.;
        PrevCache[StartNode] = StartNode;

        const auto HeapPredicate = [&DistanceCache](const int32 A, const int32 B)
        { return DistanceCache[A] < DistanceCache[B]; };

        MinQueue.HeapPush(StartNode, HeapPredicate);

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Pathing);
            while (!MinQueue.IsEmpty())
            {
                int32 MinNode;

                {
                    TRACE_CPUPROFILER_EVENT_SCOPE(PopQueue);
                    MinQueue.HeapPop(MinNode, HeapPredicate, false);
                }

                for (const auto& ConnectedNode : Graph.ViewNodesConnectedToNode(MinNode))
                {
                    const auto EdgeIndex = Graph.NodePairToEdgeIndex(MinNode, ConnectedNode);
                    const auto DistCandidate =
                        DistanceCache[MinNode] +
                        WeightCache[EdgeIndex];

                    if (MinQueue.Contains(ConnectedNode) &&
                        DistCandidate < DistanceCache[ConnectedNode])
                    {
                        TRACE_CPUPROFILER_EVENT_SCOPE(UpdateDistance);

                        DistanceCache[ConnectedNode] = DistCandidate;
                        PrevCache[ConnectedNode] = MinNode;
                        for (int32& NodeIndex : MinQueue)
                        {
                            if (NodeIndex == ConnectedNode)
                            {
                                AlgoImpl::HeapSiftUp(
                                    MinQueue.GetData(),
                                    0,
                                    UE_PTRDIFF_TO_INT32(&NodeIndex - MinQueue.GetData()),
                                    FIdentityFunctor(),
                                    HeapPredicate);
                                break;
                            }
                        }
                    }
                    else if (PrevCache[ConnectedNode] == INDEX_NONE)
                    {
                        TRACE_CPUPROFILER_EVENT_SCOPE(PushDistance);

                        DistanceCache[ConnectedNode] = DistCandidate;
                        PrevCache[ConnectedNode] = MinNode;
                        MinQueue.HeapPush(ConnectedNode, HeapPredicate);
                    }
                }
            }
        }

        return true;
    }

    bool IsIsolated(
    const FMTWayGraph& Graph,
    const int32 Node,
    TMap<int64, int32>& EdgeCounts)
    {
        for (const auto& ConnectedNode : Graph.ViewNodesConnectedToNode(Node))
        {
            ensure(
                Graph.NodePairToEdgeIndex(Node, ConnectedNode) ==
                Graph.NodePairToEdgeIndex(ConnectedNode, Node));
            ensure(Graph.AreNodesConnected(Node, ConnectedNode));
            ensure(Graph.AreNodesConnected(ConnectedNode, Node));
            const auto EdgeIndex = Graph.NodePairToEdgeIndex(Node, ConnectedNode);
            const auto EdgeCount = EdgeCounts[EdgeIndex];
            if (EdgeCount > 0)
            {
                return false;
            }
        }

        return true;
    }

    
    int32 GotoNextNodeAndRemoveEdge(
        const FMTWayGraph& Graph,
        const int32 Node,
        TMap<int64, int32>& EdgeCounts)
    {
        for (const auto& ConnectedNode : Graph.ViewNodesConnectedToNode(Node))
        {
            ensure(
                Graph.NodePairToEdgeIndex(Node, ConnectedNode) ==
                Graph.NodePairToEdgeIndex(ConnectedNode, Node));
            ensure(Graph.AreNodesConnected(Node, ConnectedNode));
            ensure(Graph.AreNodesConnected(ConnectedNode, Node));
            
            const auto EdgeIndex = Graph.NodePairToEdgeIndex(Node, ConnectedNode);
            const auto EdgeCount = EdgeCounts[EdgeIndex];
            if (EdgeCount > 0)
            {
                EdgeCounts[EdgeIndex]--;
                return ConnectedNode;
            }
        }

        check(false);
        return -1;
    }

    void FindEulerPath(
        const FMTWayGraph& Graph,
        const int32 StartNode,
        TMap<int64, int32>& EdgeCounts,
        TArray<int32>& OutTour)
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(FindEulerPath);

        OutTour.Reset();
        
        TArray CurrentPathStack = {StartNode};
        auto CurrentNode = StartNode;
        while (!CurrentPathStack.IsEmpty())
        {
            if (!IsIsolated(Graph, CurrentNode, EdgeCounts))
            {
                CurrentPathStack.Push(CurrentNode);
                const auto Next = GotoNextNodeAndRemoveEdge(Graph, CurrentNode, EdgeCounts);
                CurrentNode = Next;
            }
            else
            {
                if (!OutTour.IsEmpty())
                {
                    ensureAlwaysMsgf(Graph.AreNodesConnected(OutTour.Last(), CurrentNode), TEXT("Prev = %d, Curr = %d"), OutTour.Last(), CurrentNode);
                }
                OutTour.Push(CurrentNode);
                CurrentNode = CurrentPathStack.Pop();
            }
        }
    }
}  // namespace


TArray<FMTWayGraphPath> FMTChinesePostMan::CalculatePathsThatContainAllEdges(
    const FMTWayGraph& Graph,
    const ACesiumGeoreference* GeoRef)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(ChinesePostMan::CalculatePathsThatContainAllEdges);

    TArray<TArray<int32>> Islands;
    TArray<TArray<int32>> IslandsOddNodes;

    FindIslands(Graph, Islands, IslandsOddNodes);

    check(Islands.Num() == IslandsOddNodes.Num())
    TMap<int64, int32> EdgeCounts;
    TMap<int64, double> EdgeWeights;

    {
        TRACE_CPUPROFILER_EVENT_SCOPE(Init);

        for (int32 Node1 = 0; Node1 < Graph.NodeNum(); Node1++)
        {
            for (int32 Node2 = Node1 + 1; Node2 < Graph.NodeNum(); Node2++)
            {
                if (Graph.AreNodesConnected(Node1, Node2))
                {
                    const auto EdgeIndex = Graph.NodePairToEdgeIndex(Node1, Node2);
                    EdgeCounts.Add(EdgeIndex, 1);
                    const auto EdgeWeight = FVector::Distance(
                        Graph.GetNodeLocationUnreal(Node1, GeoRef),
                        Graph.GetNodeLocationUnreal(Node2, GeoRef));
                    EdgeWeights.Add(EdgeIndex, EdgeWeight);
                }
            }
        }
    }

    TArray<FMTWayGraphPath> Result;

    struct FDijsktraContext
    {
        TArray<double> DistanceCache = {};
    };
    TArray<FDijsktraContext> DijsktraContexts;
    TArray<TArray<int32>> OddPrevPaths;

    TArray<FOddToOddPath> OddToOddPaths;
    
    TSet<int32> MatchedIndices;
    TArray<FOddToOddPath> MatchedEdges;

    FCriticalSection OddToOddLock;

    for (int32 IslandIndex = 0; IslandIndex < Islands.Num(); ++IslandIndex)
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(ProcessIsland);
        check(Islands[IslandIndex].Num() > 0)
        
        const auto& IslandOddNodes = IslandsOddNodes[IslandIndex];
        check(IslandOddNodes.Num() % 2 == 0);
        
        OddPrevPaths.SetNum(IslandOddNodes.Num());
        DijsktraContexts.Reset();
        OddToOddPaths.Reset();

        ParallelForWithTaskContext(
            DijsktraContexts,
            IslandOddNodes.Num(),
            [](int32 ContextIndex, int32 NumContexts) { return FDijsktraContext{}; },
            [&IslandOddNodes, &Graph, &EdgeWeights, &OddPrevPaths, &OddToOddPaths, &OddToOddLock](
                FDijsktraContext& Context, int32 OddIndex)
            {
                const auto OddStart = IslandOddNodes[OddIndex];

                const auto SearchStatus = Dijsktra(
                    Graph, OddStart, Context.DistanceCache, EdgeWeights, OddPrevPaths[OddIndex]);
                check(SearchStatus);

                OddToOddLock.Lock();
                for (int32 OtherOddIndex = OddIndex + 1; OtherOddIndex < IslandOddNodes.Num();
                     ++OtherOddIndex)
                {
                    OddToOddPaths.Push(
                        {OddIndex,
                         OtherOddIndex,
                         Context.DistanceCache[IslandOddNodes[OtherOddIndex]]});
                }
                OddToOddLock.Unlock();
            });

        // Greedy matching
        // 2-Approximation
        MatchedIndices.Reset();
        MatchedEdges.Reset();

        {
            TRACE_CPUPROFILER_EVENT_SCOPE(Sort);
            OddToOddPaths.Sort();
        }

        for (const auto& OddToOddPath : OddToOddPaths)
        {
            if (OddToOddPath.StartOddIndex != OddToOddPath.EndOddIndex)
            {
                if (!MatchedIndices.Contains(OddToOddPath.StartOddIndex) &&
                    !MatchedIndices.Contains(OddToOddPath.EndOddIndex))
                {
                    MatchedIndices.Add(OddToOddPath.StartOddIndex);
                    MatchedIndices.Add(OddToOddPath.EndOddIndex);
                    MatchedEdges.Add(
                        {OddToOddPath.StartOddIndex,
                         OddToOddPath.EndOddIndex,
                         OddToOddPath.Distance});
                }
            }
        }
        
        // for matched oddtooddpath insert additional edges

        for (const auto& MatchedEdge : MatchedEdges)
        {
            const auto StartNode = IslandOddNodes[MatchedEdge.StartOddIndex];
            // StartNodeOddIndex is used to identify prevPath
            const auto& PrevPath = OddPrevPaths[MatchedEdge.StartOddIndex];
            auto CurrentPathNode = IslandOddNodes[MatchedEdge.EndOddIndex];
            while (CurrentPathNode != StartNode)
            {
                const auto NextPathNode = PrevPath[CurrentPathNode];

                ensure(Graph.AreNodesConnected(CurrentPathNode, NextPathNode));
                EdgeCounts[Graph.NodePairToEdgeIndex(CurrentPathNode, NextPathNode)]++;

                CurrentPathNode = NextPathNode;
            }
        }

        // Pick a random node and calculate a euler Cycle for island
        const auto& IslandNodes = Islands[IslandIndex];
        if(IslandNodes.Num() > 1)
        {
            auto StartNode = IslandNodes[0];
            //check(Graph.ViewNodesConnectedToNode(StartNode).Num() > 1);
            
            {
                TRACE_CPUPROFILER_EVENT_SCOPE(Validation);
                // Validate euler path & that each node exists as start node
                for (const auto& IslandNode : IslandNodes)
                {
                    int32 NodeDegree = 0;
                    for (const auto& ConnectedNode : Graph.ViewNodesConnectedToNode(IslandNode))
                    {
                        ensure(
                            Graph.NodePairToEdgeIndex(IslandNode, ConnectedNode) ==
                            Graph.NodePairToEdgeIndex(ConnectedNode, IslandNode));
                        ensure(Graph.AreNodesConnected(IslandNode, ConnectedNode));
                        ensure(Graph.AreNodesConnected(ConnectedNode, IslandNode));

                        const auto EdgeIndex = Graph.NodePairToEdgeIndex(IslandNode, ConnectedNode);
                        NodeDegree += EdgeCounts[EdgeIndex];
                    }

                    FString DbgNeighbourString = TEXT("");
                    for (const int32 Neighbour : Graph.ViewNodesConnectedToNode(IslandNode))
                    {
                        DbgNeighbourString += FString::FromInt(Neighbour) + TEXT(",");
                    }
                    
                    ensureAlwaysMsgf(NodeDegree % 2 == 0, TEXT("Node = %d, NodeDegree = %d, ConnectedNodes = %d, Neighbours = %s, StartNode = %d"), IslandNode, NodeDegree, Graph.ViewNodesConnectedToNode(IslandNode).Num(), *DbgNeighbourString, StartNode);
                }
            }
            auto& NextCycle = Result.Emplace_GetRef();
            FindEulerPath(Graph, StartNode, EdgeCounts, NextCycle.Nodes);

            {
                TRACE_CPUPROFILER_EVENT_SCOPE(Validation);

                check(NextCycle.Nodes.Num() > 1);
                check(NextCycle.Nodes.Num() >= IslandNodes.Num());

                TRACE_CPUPROFILER_EVENT_SCOPE(Validation);
                // Validate euler path
                for (const auto& IslandNode : IslandNodes)
                {
                    ensure(NextCycle.Nodes.Contains(IslandNode));
                }

                for (int32 I = 0; I < NextCycle.Nodes.Num() - 1; ++I)
                {
                    ensureMsgf(Graph.AreNodesConnected(NextCycle.Nodes[I + 1], NextCycle.Nodes[I]), TEXT("I == %d; NextCycle.Num() == %d"), I, NextCycle.Nodes.Num());
                    ensureMsgf(Graph.AreNodesConnected(NextCycle.Nodes[I], NextCycle.Nodes[I + 1]), TEXT("I == %d; NextCycle.Num() == %d"), I, NextCycle.Nodes.Num());
                }
            }
        }
    }

    return Result;
}
