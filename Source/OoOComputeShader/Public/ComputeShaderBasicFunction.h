#pragma once
//
#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/BoxComponent.h"

#include "ComputeShaderBasicFunction.generated.h"

UCLASS()
class OOOCOMPUTESHADER_API UComputeShaderBasicFunction : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void DrawLinearColorsToRenderTarget(UTextureRenderTarget2D* InTextureTarget, TArray<FLinearColor> Colors);
	
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void ConnectivityPixel(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* InConnectivityMap, int32 Channel, UTextureRenderTarget2D
	                              * InDebugView);

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void BlurTexture(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* OutBlurTexture, float BlurScale = .5);

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void BlurNormalTexture(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* OutBlurTexture, float BlurScale = .5);
	
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void UpPixelsMask(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* OutUpTexture, float Threshould = .8, int32 Channel = 0);
	
	
};

