#pragma once
//
#include "CoreMinimal.h"
#include "ComputeShaderGeneral.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"

#include "ComputeShaderBasicFunction.generated.h"


struct FCSSPlinePointData
{
	TArray<TArray<FLinearColor>> PointData;
};

UCLASS()
class COMPUTESHADERGENERATOR_API UComputeShaderBasicFunction : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void DrawLinearColorsToRenderTarget(UTextureRenderTarget2D* InTextureTarget, TArray<FLinearColor> Colors);
	
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void ConnectivityPixel(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* InConnectivityMap, UTextureRenderTarget2D
	                              * InDebugView, int32 Channel = 2, int32 TextureSize = 256);

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void BlurTexture(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* OutBlurTexture, float BlurScale = 1);

	static void BlurTextureRDG(FRDGBuilder& GraphBuilder, FRDGTextureRef& InTexture, FRDGTextureUAVRef& InTextureUAV, FRDGTextureRef& OutTexture, FIntVector
	                           GroupCount, FBlurTexture::EBlurType Type, float
	                           BlurScale = 1);
	
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void BlurNormalTexture(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* OutBlurTexture, float BlurScale = .5);
	
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void UpPixelsMask(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* OutUpTexture, float Threshould = .8, int32 Channel = 0);
	
	// UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	// static void UpPixelsMask(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* OutUpTexture, float Threshould = .8, int32 Channel = 0);
	//
	
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void DrawTextureOut(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* OutTextureTarget);
	
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void Test(UTexture2DArray* InArray, UTexture2D* InTexture, UTextureRenderTarget2D* InDebugView);
	
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void ConvertHeightDataToTexture(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* InHeightData, FVector
	                                       Center, FVector Extent);

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void ExtentMaskFast(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* InDebugView, int32 Channel = 0, int32 NumExtend = 1);
	
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static TArray<FTransform> SampleSpline(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* InDebugView,
	                                       TArray<USplineComponent*>
	                                       InSplineComponents, FBoxSphereBounds& Bounds, int32 TextureSize = 256, float Interval = 50);

	// UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void CalDistanceToNearestSurface(FSceneView* SceneView, UTextureRenderTarget2D* InDebugView);
};

