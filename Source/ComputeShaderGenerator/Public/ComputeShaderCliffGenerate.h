#pragma once
//
#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ComputeShaderSceneCapture.h"
//
#include "ComputeShaderCliffGenerate.generated.h"
//
//
// //This struct act as a container for all the parameters that the client needs to pass to the Compute Shader Manager.

USTRUCT()
struct COMPUTESHADERGENERATOR_API FCSCliffGenerateData
{
	GENERATED_USTRUCT_BODY()
public:
	
	FVector2D RandomRotate = FVector2D(0, 0);
	FVector2D RandomScale = FVector2D(1, 1);
	FVector2D RandomHeightOffset = FVector2D(0, 0);
	float DrawScale = 1;
	float SpawnScaleMult = 1;
	int32 SelectIndex = -1;

};

UCLASS()
class COMPUTESHADERGENERATOR_API ACSCliffGenerateCapture : public ACSGenerateCaptureScene
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InMask;

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
		if (InMeshHeightArray == nullptr) Check = false; 
		return MeshDataAssets.Num() > 0 && InSceneDepth != nullptr &&
			InSceneNormal != nullptr && InDebugView != nullptr && InResult != nullptr &&
			InCurrentSceneDepth != nullptr;
	}

	void GenerateCliffVerticalCal(TArray<FCSCliffGenerateData> GenerateDatas);

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	void GenerateCliffVertical(int32 NumIteration = 1, float InSpawnSize = 1);

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	void GenerateTargetHeightCal();
	
	
};


