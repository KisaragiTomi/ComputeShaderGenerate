// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CSAssetProcess.generated.h"

/**
 * 
 */
UCLASS()
class CSEDITORPROCESS_API UCSAssetProcess : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void CaptureMeshHeight(AStaticMeshActor* InMeshActor, UTextureRenderTarget2D*& OutRenderTarget2D, TArray<AActor*>& OutActors, int32
	                              InTextureSize = 256);

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void CalculateMeshHeight(AStaticMeshActor* InMeshActor, UTextureRenderTarget2D* NewRenderTarget2D);

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void GetDistanceToNearestSurface(UTextureRenderTarget2D* InDebugView);
};
