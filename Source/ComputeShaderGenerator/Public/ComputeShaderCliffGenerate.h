#pragma once
//
#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ComputeShaderMeshFill.h"
#include "ComputeShaderSceneCapture.h"
//
#include "ComputeShaderCliffGenerate.generated.h"
//
//
// //This struct act as a container for all the parameters that the client needs to pass to the Compute Shader Manager.



UCLASS()
class COMPUTESHADERGENERATOR_API ACSCliffGenerateCapture : public ACSGenerateCaptureScene
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InMask;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InHeightData;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InHeightNormal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InDebugView;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InResult;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTexture2DArray* InMeshHeightArray;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InCurrentSceneDepth;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InConectivityClassifiy;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InTargetHeight;

	

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	float SpawnSize = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	float RandomRoation = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	FName Tag = FName(TEXT("Auto"));




	ACSCliffGenerateCapture();

	virtual void OnConstruction(const FTransform& Transform) override;
	
	bool IsParameterValidMult()
	{
		bool Check = true;
		if (MeshDataAssets.Num() == 0) Check = false;
		if (InHeightNormal == nullptr) Check = false;
		if (InMeshHeightArray == nullptr) Check = false;
		if (InHeightData == nullptr) Check = false;
		if (InSceneDepth == nullptr) Check = false;
		if (InSceneNormal == nullptr) Check = false;
		if (InDebugView == nullptr) Check = false;
		if (InResult == nullptr) Check = false;
		if (InCurrentSceneDepth == nullptr) Check = false;
		return Check;
	}

	void GenerateCliffVerticalCal(TArray<FCSMeshFillData> GenerateDatas);

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	void GenerateCliffVertical(int32 NumIteration = 1, float InSpawnSize = 1);

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	void GenerateTargetHeightCal();
	
	
};


