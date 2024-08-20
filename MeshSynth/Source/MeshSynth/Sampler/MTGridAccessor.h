// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

template <int32 Dimensions>
class TMTNDGridAccessor
{
public:
    using IndexType = decltype(Dimensions);
    using Coords = Chaos::TVector<IndexType, Dimensions>;

    TMTNDGridAccessor()
        : TMTNDGridAccessor(Coords(0))
    {
    }

    TMTNDGridAccessor(const Coords& InitialDimensionSizes)
    {
        Resize(InitialDimensionSizes);
    }

    struct FCGNeighbours
    {
        IndexType Count = 0;
        Chaos::TVector<IndexType, Dimensions * 2> Cells;
        Chaos::TVector<IndexType, Dimensions * 2> Directions;
    };

    FCGNeighbours GetNeighbors(const IndexType CellIndex) const
    {
        FCGNeighbours Result;
        for (IndexType Dim = 0; Dim < Dimensions; ++Dim)
        {
            AddNeighboursFromDimension(CellIndex, Dim, Result);
        }
        return Result;
    }

    struct FCGDimensionNeighbours
    {
        TOptional<IndexType> NegativeNeighbour{};
        TOptional<IndexType> PositiveNeighbour{};
    };

    FCGDimensionNeighbours
    GetNeighboursInDimension(const IndexType CellIndex, const IndexType Dim) const
    {
        FCGDimensionNeighbours Result;

        IndexType Candidate1 = CellIndex - CumulatedCellCount[Dim];
        IndexType Candidate2 = CellIndex + CumulatedCellCount[Dim];
        if ((Candidate1 >= 0) &&
            (Candidate1 / CumulatedCellCount[Dim + 1] == CellIndex / CumulatedCellCount[Dim + 1]))
        {
            Result.NegativeNeighbour = Candidate1;
        }
        if (Candidate2 / CumulatedCellCount[Dim + 1] == CellIndex / CumulatedCellCount[Dim + 1])
        {
            Result.PositiveNeighbour = Candidate2;
        }

        return Result;
    }

    TArray<IndexType> GetCellsInRadius(const IndexType CellIndex, const IndexType Radius) const
    {
        TArray<IndexType> Result;
        // TODO can overflow not sure what do to maybe custom pow func?
        Result.Reserve(FMath::Pow(Radius * 2.F + 1, Dimensions));

        AddCellsInRadius(CellIndex, 0, Radius, Result);
        return Result;
    }

    Coords GetDimensionSizes() const
    {
        return DimensionSizes;
    }

    void Resize(const Coords& NewDimensionSizes)
    {
        DimensionSizes = NewDimensionSizes;
        CumulatedCellCount = decltype(CumulatedCellCount)(0.F);
        CalculateCumulatedCellCounts();
    }

    /**
     * Try to use cell indices directly if possible.
     * If it can't be avoided use CoordToIndex() instead since it's cheaper.
     */
    Coords IndexToCoords(const IndexType Index) const
    {
        Coords Result;

        // unrolled from loop
        Result[Dimensions - 1] = Index / CumulatedCellCount[Dimensions - 1];
        int Acc = Index - Result[Dimensions - 1] * CumulatedCellCount[Dimensions - 1];
        for (IndexType Dim = Dimensions - 2; Dim > 0; --Dim)
        {
            Result[Dim] = Acc / CumulatedCellCount[Dim];
            Acc -= Result[Dim] * CumulatedCellCount[Dim];
        }
        // unrolled from loop
        Result[0] = Acc;

        return Result;
    }

    /**
     * Try to use cell indices directly if possible.
     * Cheaper than IndexToCoords()
     */
    IndexType CoordToIndex(const Coords& Coords) const
    {
        check(Coords[0] >= 0);

        IndexType Result = Coords[0];

        for (IndexType Dim = 1; Dim < Dimensions; ++Dim)
        {
            check(Coords[Dim] >= 0);
            Result += Coords[Dim] * CumulatedCellCount[Dim];
        }
        return Result;
    }

    IndexType CellCount() const
    {
        return TotalCellCount;
    }

    static constexpr IndexType DimensionNum()
    {
        return Dimensions;
    }

    bool IsValidCellIndex(const IndexType CellIndex) const
    {
        return CellIndex >= 0 && CellIndex < TotalCellCount;
    }

private:
    Coords DimensionSizes;
    IndexType TotalCellCount;
    Chaos::TVector<IndexType, Dimensions + 1> CumulatedCellCount;

    void CalculateCumulatedCellCounts()
    {
        TotalCellCount = 1;

        CumulatedCellCount[0] = 1;
        for (IndexType Dim = 0; Dim < Dimensions; ++Dim)
        {
            TotalCellCount *= DimensionSizes[Dim];
            CumulatedCellCount[Dim + 1] = TotalCellCount;
        }
    }

    void AddNeighboursFromDimension(
        const IndexType CellIndex,
        const IndexType Dim,
        FCGNeighbours& OutNeighbours) const
    {
        const IndexType Candidate1 = CellIndex - CumulatedCellCount[Dim];
        const IndexType Candidate2 = CellIndex + CumulatedCellCount[Dim];
        if ((Candidate1 >= 0) &&
            (Candidate1 / CumulatedCellCount[Dim + 1] == CellIndex / CumulatedCellCount[Dim + 1]))
        {
            OutNeighbours.Cells[OutNeighbours.Count] = Candidate1;
            OutNeighbours.Directions[OutNeighbours.Count] = Dim * 2;
            ++OutNeighbours.Count;
        }
        if (Candidate2 / CumulatedCellCount[Dim + 1] == CellIndex / CumulatedCellCount[Dim + 1])
        {
            OutNeighbours.Cells[OutNeighbours.Count] = Candidate2;
            OutNeighbours.Directions[OutNeighbours.Count] = Dim * 2 + 1;
            ++OutNeighbours.Count;
        }
    }

    /**
     * Add all cells in range recursing over the dimensions
     */
    void AddCellsInRadius(
        const IndexType CellIndex,
        const IndexType Dim,
        const IndexType Range,
        TArray<IndexType>& OutCells) const
    {
        if (Dim >= Dimensions)
        {
            if (IsValidCellIndex(CellIndex))
            {
                OutCells.Emplace(CellIndex);
            }
            return;
        }

        for (IndexType i = CellIndex - Range * CumulatedCellCount[Dim];
             i <= CellIndex + Range * CumulatedCellCount[Dim];
             i += CumulatedCellCount[Dim])
        {
            AddCellsInRadius(i, Dim + 1, Range, OutCells);
        }
    }
};