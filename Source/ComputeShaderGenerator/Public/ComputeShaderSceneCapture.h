#pragma once
//
#include "CoreMinimal.h"
#include "DynamicMeshActor.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/BoxComponent.h"
//
#include "ComputeShaderSceneCapture.generated.h"


USTRUCT(BlueprintType)
struct COMPUTESHADERGENERATOR_API FCSMeshData
{
	GENERATED_USTRUCT_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	UStaticMesh* CSStaticMesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	UTexture* CSMeshHeight = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	FVector CSMeshPivot = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	float CSMeshMaxHeight = 0;

};


//
// //This struct act as a container for all the parameters that the client needs to pass to the Compute Shader Manager.
UCLASS()
class COMPUTESHADERGENERATOR_API ACSGenerateCaptureScene : public AActor
{
	GENERATED_BODY()
public:
	ACSGenerateCaptureScene();
	
	UBoxComponent* Box;
	USceneComponent* SceneComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InSceneDepth;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InSceneNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InObjectNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InObjectBaseColor;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CliffGenerate)
	UTextureRenderTarget2D* InObjectDepth;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capturer")
	TArray<UCSMeshAsset*> MeshDataAssets;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Capturer")
	USceneCaptureComponent2D* CaptureObjectDepth;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Capturer")
	USceneCaptureComponent2D* CaptureSceneDepth;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Capturer")
	USceneCaptureComponent2D* CaptureObjNormal;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Capturer")
	USceneCaptureComponent2D* CaptureObjBaseColor;
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Capturer")
	USceneCaptureComponent2D* CaptureSceneNormal;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<class UDynamicMeshComponent> DynamicMeshComponent;
	
	TRefCountPtr<FRDGPooledBuffer> DebugBuffer;
	
	virtual void OnConstruction(const FTransform& Transform) override;
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	void CaptureAll();
	
	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	bool CreateLandscapeMesh();
	
	UTextureRenderTarget2D* CreateRenderTarget(float TextureSize = 512);
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capturer")
	float CaptureSize = 2048;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capturer")
	float MaxHeight = 10000;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capturer")
	float Scale3DZ = 100;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Capturer")
	float TextureSize = 256;
	
	FVector PreCenter = FVector::ZeroVector;
	FVector PreExtent = FVector::ZeroVector;
	
};



UCLASS()
class COMPUTESHADERGENERATOR_API ACSMeshConverter : public AActor
{
	GENERATED_BODY()
public:
	ACSMeshConverter();

	USceneComponent* SceneComponent;

	
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Container")
	UStaticMeshComponent* MeshVisualize;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Container")
	UStaticMesh* MeshToCapture;
	

	
	FVector Center = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;


	virtual void OnConstruction(const FTransform& Transform) override;
};

UCLASS(BlueprintType)
class COMPUTESHADERGENERATOR_API UCSMeshAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	UStaticMesh* CSStaticMesh = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	UTexture2D* CSMeshHeightTexture = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	FVector CSMeshPivot = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	float CSMeshMaxHeight = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	float CSMeshSize = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	FVector2D RandomHeightOffset = FVector2D(0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	FVector2D RandomRotate = FVector2D(0, 0);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CSMeshData")
	FVector2D RandomScale = FVector2D(1, 1);
};

