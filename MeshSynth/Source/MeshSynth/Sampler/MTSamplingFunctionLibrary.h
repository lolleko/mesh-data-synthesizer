// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CanvasItem.h"
#include "CanvasTypes.h"
#include "CesiumGeoreference.h"
#include "CoreMinimal.h"
#include "CubemapUnwrapUtils.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/TextureRenderTargetCube.h"
#include "MTSample.h"
#include "UObject/Object.h"

#include "MTSamplingFunctionLibrary.generated.h"

/**
 * 
 */
UCLASS()
class MESHSYNTH_API UMTSamplingFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	static const FRandomStream& GetRandomStream();

	static void SetRandomSeed(const int32 NewSeed);

	// Bias = 1/N to prefer vertical directions, upper part of hemisphere (e.g. Bias = 1/15)
	// Bias = N to prefer horizontal directions, lower part of hemisphere (e.g. Bias = 2)
	static FVector SampleUniformDirOnPositiveHemisphere(const float Bias = 1.F);
    
	static bool IsViewObstructed(const UWorld* World, const FTransform& SampleTransform, const float MinClearance, const float SweepSphereRadius);

    static const inline FDateTime SessionStartTime = FDateTime::Now();
    static const inline TCHAR* TimeFormat = TEXT("%Y%m%dT%H%M%S%sZ");

    static bool ReadPixelsFromRenderTarget(UTextureRenderTarget2D* RenderTarget2D, TArray<FColor>& PixelBuffer);
    
    struct FImageWriteResult
    {
        FString FilePath;
        TArray64<uint8> Data;
    };
    static TFuture<FImageWriteResult> WritePixelBufferToFile(const FString& FilePath, const TArray<FColor>& PixelBuffer, const FIntVector2& Size);

    static TFuture<FImageWriteResult> WriteCubeMapPixelBufferToFile(
        const FString& FilePath,
        const TArray<FColor>& PixelBuffer,
        const FIntVector2& Size,
        const FIntVector2& TopAndBottomCrop);

    struct FLocationPathPair
    {
        FTransform Location;
        FString Path;
    };
    static TArray<FLocationPathPair> PanoramaLocationsFromCosPlaceCSV(const FString& FilePath, const ACesiumGeoreference* Georeference);

    static TArray<FLocationPathPair> SliceLocationsFromPittsTXT(const FString& FilePath, const ACesiumGeoreference* Georeference);

    static TArray<FLocationPathPair> SliceLocationsFromTokyoTXT(const FString& FilePath, const ACesiumGeoreference* Georeference);

    struct FExtraViewData
    {
        FTransform Location;
        FString PredictionFileName;
        FString SharedViewOutDir;
        FString QueryTempDatabaseImageList;
        FIntVector2 Resolution;
    };
    static TArray<FExtraViewData> PredictionsLocationsFromFile(const FString& FilePath, const ACesiumGeoreference* Georeference);

    static FString CreateImageNameFromSample(const FMTSample& Sample);

private:
	static FRandomStream RandomStream;
};

// copied from CubemapUnwrrapUtils
// modified to work on the RenderThread

namespace CubemapHelpers
{
	/**
     * Helper function to create an unwrapped 2D image of the cube map ( longitude/latitude )
     * This version takes explicitly passed properties of the source object, as the sources have
     * different APIs.
     * @param	TextureResource		Source FTextureResource object.
     * @param	AxisDimenion		axis length of the cube.
     * @param	SourcePixelFormat	pixel format of the source.
     * @param	BitsOUT				Raw bits of the 2D image bitmap.
     * @param	SizeXOUT			Filled with the X dimension of the output bitmap.
     * @param	SizeYOUT			Filled with the Y dimension of the output bitmap.
     * @return						true on success.
     * @param	FormatOUT			Filled with the pixel format of the output bitmap.
     */
    inline bool GenerateLongLatUnwrap(
        const FTextureResource* TextureResource,
        const uint32 AxisDimenion,
        const EPixelFormat SourcePixelFormat,
        TArray<FColor>& PixelsOUT,
        FIntPoint& SizeOUT,
        EPixelFormat& FormatOUT,
        UTextureRenderTarget2D* RenderTargetLongLatCache,
        FRHICommandListImmediate& RHICmdList)
    {
        TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;
        BatchedElementParameters = new FMipLevelBatchedElementParameters(
            (float)0, (float)-1, false, FMatrix44f::Identity, true, true, false);
        const FIntPoint LongLatDimensions(AxisDimenion * 2, AxisDimenion);

        // If the source format is 8 bit per channel or less then select a LDR target format.
        const EPixelFormat TargetPixelFormat =
            CalculateImageBytes(1, 1, 0, SourcePixelFormat) <= 4 ? PF_B8G8R8A8 : PF_FloatRGBA;
        
        FTextureRenderTargetResource* RenderTarget = RenderTargetLongLatCache->GetRenderTargetResource();

        FCanvas* Canvas = new FCanvas(RenderTarget, NULL, FGameTime(), GMaxRHIFeatureLevel);

        // Clear the render target to black
        Canvas->Clear(FLinearColor(0, 0, 0, 0));

        FCanvasTileItem TileItem(
            FVector2D(0.0f, 0.0f),
            TextureResource,
            FVector2D(LongLatDimensions.X, LongLatDimensions.Y),
            FLinearColor::White);
        TileItem.BatchedElementParameters = BatchedElementParameters;
        TileItem.BlendMode = SE_BLEND_Opaque;
        Canvas->DrawItem(TileItem);

        Canvas->Flush_RenderThread(RHICmdList);

        PixelsOUT.SetNumUninitialized(LongLatDimensions.X * LongLatDimensions.Y);

        switch (TargetPixelFormat)
        {
            case PF_B8G8R8A8:
                RHICmdList.ReadSurfaceData(
                    RenderTarget->GetRenderTargetTexture(),
                    FIntRect(0, 0, RenderTarget->GetSizeX(), RenderTarget->GetSizeY()),
                    PixelsOUT,
                    FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX));
                break;
            case PF_FloatRGBA:
            {
                check(false)
            }
            break;
        }
        
        // Clean up.
        delete Canvas;

        SizeOUT = LongLatDimensions;
        FormatOUT = TargetPixelFormat;

        return true;
    }

    inline bool GenerateLongLatUnwrap(UTextureRenderTargetCube* CubeTarget, TArray<FColor>& PixelsOUT, FIntPoint& SizeOUT, EPixelFormat& FormatOUT, UTextureRenderTarget2D* RenderTargetLongLatCache, FRHICommandListImmediate& RHICmdList)
	{
		check(CubeTarget != NULL);
		return GenerateLongLatUnwrap(CubeTarget->GetRenderTargetResource(), CubeTarget->SizeX, CubeTarget->GetFormat(), PixelsOUT, SizeOUT, FormatOUT, RenderTargetLongLatCache, RHICmdList);
	}
}

namespace UTM
{
    // Grid granularity for rounding UTM coordinates to generate MapXY.
    const double grid_size = 100000.0;    ///< 100 km grid

// WGS84 Parameters
#define WGS84_A		6378137.0		///< major axis
#define WGS84_B		6356752.31424518	///< minor axis
#define WGS84_F		0.0033528107		///< ellipsoid flattening
#define WGS84_E		0.0818191908		///< first eccentricity
#define WGS84_EP	0.0820944379		///< second eccentricity

    // UTM Parameters
#define UTM_K0		0.9996			///< scale factor
#define UTM_FE		500000.0		///< false easting
#define UTM_FN_N	0.0           ///< false northing, northern hemisphere
#define UTM_FN_S	10000000.0    ///< false northing, southern hemisphere
#define UTM_E2		(WGS84_E*WGS84_E)	///< e^2
#define UTM_E4		(UTM_E2*UTM_E2)		///< e^4
#define UTM_E6		(UTM_E4*UTM_E2)		///< e^6
#define UTM_EP2		(UTM_E2/(1-UTM_E2))	///< e'^2

    /**
     * Determine the correct UTM letter designator for the
     * given latitude
     *
     * @returns 'Z' if latitude is outside the UTM limits of 84N to 80S
     *
     * Written by Chuck Gantz- chuck.gantz@globalstar.com
     */
    static inline char UTMLetterDesignator(double Lat)
    {
        char LetterDesignator;

        if     ((84 >= Lat) && (Lat >= 72))  LetterDesignator = 'X';
        else if ((72 > Lat) && (Lat >= 64))  LetterDesignator = 'W';
        else if ((64 > Lat) && (Lat >= 56))  LetterDesignator = 'V';
        else if ((56 > Lat) && (Lat >= 48))  LetterDesignator = 'U';
        else if ((48 > Lat) && (Lat >= 40))  LetterDesignator = 'T';
        else if ((40 > Lat) && (Lat >= 32))  LetterDesignator = 'S';
        else if ((32 > Lat) && (Lat >= 24))  LetterDesignator = 'R';
        else if ((24 > Lat) && (Lat >= 16))  LetterDesignator = 'Q';
        else if ((16 > Lat) && (Lat >= 8))   LetterDesignator = 'P';
        else if (( 8 > Lat) && (Lat >= 0))   LetterDesignator = 'N';
        else if (( 0 > Lat) && (Lat >= -8))  LetterDesignator = 'M';
        else if ((-8 > Lat) && (Lat >= -16)) LetterDesignator = 'L';
        else if((-16 > Lat) && (Lat >= -24)) LetterDesignator = 'K';
        else if((-24 > Lat) && (Lat >= -32)) LetterDesignator = 'J';
        else if((-32 > Lat) && (Lat >= -40)) LetterDesignator = 'H';
        else if((-40 > Lat) && (Lat >= -48)) LetterDesignator = 'G';
        else if((-48 > Lat) && (Lat >= -56)) LetterDesignator = 'F';
        else if((-56 > Lat) && (Lat >= -64)) LetterDesignator = 'E';
        else if((-64 > Lat) && (Lat >= -72)) LetterDesignator = 'D';
        else if((-72 > Lat) && (Lat >= -80)) LetterDesignator = 'C';
        // 'Z' is an error flag, the Latitude is outside the UTM limits
        else LetterDesignator = 'Z';
        return LetterDesignator;
    }

    /**
     * Convert lat/long to UTM coords.  Equations from USGS Bulletin 1532
     *
     * East Longitudes are positive, West longitudes are negative.
     * North latitudes are positive, South latitudes are negative
     * Lat and Long are in fractional degrees
     *
     * Written by Chuck Gantz- chuck.gantz@globalstar.com
     */
    static inline void LLtoUTM(const double Long, const double Lat,
                               double &UTMNorthing, double &UTMEasting,
                               int32& ZoneNumber, TCHAR& UTMZoneLetter)
    {
        double a = WGS84_A;
        double eccSquared = UTM_E2;
        double k0 = UTM_K0;

        double LongOrigin;
        double eccPrimeSquared;
        double N, T, C, A, M;

        //Make sure the longitude is between -180.00 .. 179.9
        double LongTemp = (Long+180)-int((Long+180)/360)*360-180;

        double LatRad = FMath::DegreesToRadians(Lat);
        double LongRad = FMath::DegreesToRadians(LongTemp);
        double LongOriginRad;

        ZoneNumber = int((LongTemp + 180)/6) + 1;

        if( Lat >= 56.0 && Lat < 64.0 && LongTemp >= 3.0 && LongTemp < 12.0 )
            ZoneNumber = 32;

        // Special zones for Svalbard
        if( Lat >= 72.0 && Lat < 84.0 )
        {
            if(      LongTemp >= 0.0  && LongTemp <  9.0 ) ZoneNumber = 31;
            else if( LongTemp >= 9.0  && LongTemp < 21.0 ) ZoneNumber = 33;
            else if( LongTemp >= 21.0 && LongTemp < 33.0 ) ZoneNumber = 35;
            else if( LongTemp >= 33.0 && LongTemp < 42.0 ) ZoneNumber = 37;
        }
        // +3 puts origin in middle of zone
        LongOrigin = (ZoneNumber - 1)*6 - 180 + 3;
        LongOriginRad = FMath::DegreesToRadians(LongOrigin);

        //compute the UTM Zone from the latitude and longitude
        UTMZoneLetter = UTMLetterDesignator(Lat);

        eccPrimeSquared = (eccSquared)/(1-eccSquared);

        N = a/sqrt(1-eccSquared*sin(LatRad)*sin(LatRad));
        T = tan(LatRad)*tan(LatRad);
        C = eccPrimeSquared*cos(LatRad)*cos(LatRad);
        A = cos(LatRad)*(LongRad-LongOriginRad);

        M = a*((1 - eccSquared/4 - 3*eccSquared*eccSquared/64
                - 5*eccSquared*eccSquared*eccSquared/256) * LatRad
               - (3*eccSquared/8 + 3*eccSquared*eccSquared/32
                  + 45*eccSquared*eccSquared*eccSquared/1024)*sin(2*LatRad)
               + (15*eccSquared*eccSquared/256
                  + 45*eccSquared*eccSquared*eccSquared/1024)*sin(4*LatRad)
               - (35*eccSquared*eccSquared*eccSquared/3072)*sin(6*LatRad));

        UTMEasting = (double)
        (k0*N*(A+(1-T+C)*A*A*A/6
               + (5-18*T+T*T+72*C-58*eccPrimeSquared)*A*A*A*A*A/120)
         + 500000.0);

        UTMNorthing = (double)
        (k0*(M+N*tan(LatRad)
             *(A*A/2+(5-T+9*C+4*C*C)*A*A*A*A/24
               + (61-58*T+T*T+600*C-330*eccPrimeSquared)*A*A*A*A*A*A/720)));

        if(Lat < 0)
        {
            //10000000 meter offset for southern hemisphere
            UTMNorthing += 10000000.0;
        }
    }

    /**
     * Converts UTM coords to lat/long.  Equations from USGS Bulletin 1532
     *
     * East Longitudes are positive, West longitudes are negative.
     * North latitudes are positive, South latitudes are negative
     * Lat and Long are in fractional degrees.
     *
     * Written by Chuck Gantz- chuck.gantz@globalstar.com
     */
    static inline void UTMtoLL(const double UTMNorthing, const double UTMEasting,
                               const char* UTMZone, double& Lat,  double& Long )
    {
        double k0 = UTM_K0;
        double a = WGS84_A;
        double eccSquared = UTM_E2;
        double eccPrimeSquared;
        double e1 = (1-sqrt(1-eccSquared))/(1+sqrt(1-eccSquared));
        double N1, T1, C1, R1, D, M;
        double LongOrigin;
        double mu, phi1Rad;
        double x, y;
        int ZoneNumber;
        char* ZoneLetter;

        x = UTMEasting - 500000.0; //remove 500,000 meter offset for longitude
        y = UTMNorthing;

        ZoneNumber = strtoul(UTMZone, &ZoneLetter, 10);
        if((*ZoneLetter - 'N') < 0)
        {
            //remove 10,000,000 meter offset used for southern hemisphere
            y -= 10000000.0;
        }

        //+3 puts origin in middle of zone
        LongOrigin = (ZoneNumber - 1)*6 - 180 + 3;
        eccPrimeSquared = (eccSquared)/(1-eccSquared);
        
        M = y / k0;
        mu = M/(a*(1-eccSquared/4-3*eccSquared*eccSquared/64
                   -5*eccSquared*eccSquared*eccSquared/256));
        
        phi1Rad = mu + ((3*e1/2-27*e1*e1*e1/32)*sin(2*mu) 
                        + (21*e1*e1/16-55*e1*e1*e1*e1/32)*sin(4*mu)
                        + (151*e1*e1*e1/96)*sin(6*mu));
        
        N1 = a/sqrt(1-eccSquared*sin(phi1Rad)*sin(phi1Rad));
        T1 = tan(phi1Rad)*tan(phi1Rad);
        C1 = eccPrimeSquared*cos(phi1Rad)*cos(phi1Rad);
        R1 = a*(1-eccSquared)/pow(1-eccSquared*sin(phi1Rad)*sin(phi1Rad), 1.5);
        D = x/(N1*k0);
        
        Lat = phi1Rad - ((N1*tan(phi1Rad)/R1)
                         *(D*D/2
                           -(5+3*T1+10*C1-4*C1*C1-9*eccPrimeSquared)*D*D*D*D/24
                           +(61+90*T1+298*C1+45*T1*T1-252*eccPrimeSquared
                             -3*C1*C1)*D*D*D*D*D*D/720));
        
        Lat = FMath::RadiansToDegrees(Lat);
        
        Long = ((D-(1+2*T1+C1)*D*D*D/6
                 +(5-2*C1+28*T1-3*C1*C1+8*eccPrimeSquared+24*T1*T1)
                 *D*D*D*D*D/120)
                / cos(phi1Rad));
        Long = LongOrigin + FMath::RadiansToDegrees(Long);
        
    }
} // end namespace UTM
