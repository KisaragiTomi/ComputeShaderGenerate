#pragma once
//
#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/BoxComponent.h"
//
#include "ComputeShaderSceneCapture.generated.h"
//
//
// //This struct act as a container for all the parameters that the client needs to pass to the Compute Shader Manager.
UCLASS()
class OOOCOMPUTESHADER_API ACSGenerateCaptureScene : public AActor
{
	GENERATED_BODY()
public:
	ACSGenerateCaptureScene();
	
	UBoxComponent* Box;
	USceneComponent* SceneComponent;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capturer")
	USceneCaptureComponent2D* CaptureSceneDepth;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capturer")
	USceneCaptureComponent2D* CaptureSceneNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capturer")
	USceneCaptureComponent2D* CaptureBaseColor;
	
	virtual void OnConstruction(const FTransform& Transform) override;

	void Capture();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capturer")
	float CaptureSize = 2048;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capturer")
	float MaxHeight = 10000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capturer")
	float Scale3DZ = 100;
};


