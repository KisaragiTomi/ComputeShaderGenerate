#pragma once
//
#include "CoreMinimal.h"
#include "Engine/TextureRenderTarget2D.h"
#include "ComputerShaderSceneCapture.h"
//
#include "ComputerShaderCliffGenerate.generated.h"
//
//
// //This struct act as a container for all the parameters that the client needs to pass to the Compute Shader Manager.

USTRUCT(BlueprintType)
struct OOOCOMPUTESHADER_API FCSCliffGenerateParameter
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTexture2D* TMeshDepth;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* Mask;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* SceneDepth;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* DebugView;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* Result;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* SceneNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* CurrentSceneDepth;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Size = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float RandomRoation = 0;

	bool IsValid()
	{
		return TMeshDepth != nullptr && SceneDepth != nullptr &&
			SceneNormal != nullptr && DebugView != nullptr && Result != nullptr;
	}
	bool IsValidMult()
	{
		return TMeshDepth != nullptr && SceneDepth != nullptr &&
			SceneNormal != nullptr && DebugView != nullptr && Result != nullptr &&
			CurrentSceneDepth != nullptr;
	}
};

UCLASS()
class OOOCOMPUTESHADER_API UComputerShaderCliffGenerateFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void CSCliffGenerate(ACSGenerateCaptureScene* Capturer, UStaticMesh* StaticMesh, UTextureRenderTarget2D* DubugView, UTextureRenderTarget2D*
	                       Result, UTexture2D* TMeshDepth, float SpawnSize = 1, float TestSizeScale = 1, FName Tag = FName(TEXT("Auto")));


};

