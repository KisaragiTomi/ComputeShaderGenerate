#include "ComputerShaderSceneCapture.h"

#include "ClearQuad.h"
#include "ComputerShaderGenerateHepler.h"
#include "GlobalShader.h"
#include "MaterialShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "ComputerShaderGenerateHepler.h"
#include "EngineUtils.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "ComputerShaderGeneral.h"

ACSGenerateCaptureScene::ACSGenerateCaptureScene()
{
	SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("CaptureRoot"));
	//SceneComponent->SetupAttachment(GetRootComponent(), TEXT("CaptureRoot"));
	SetRootComponent(SceneComponent);
	
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->SetupAttachment(SceneComponent, TEXT("Box"));
	Box->SetBoxExtent(FVector(50,50,50));
	
	CaptureSceneDepth = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureSceneDepth"));
	CaptureSceneDepth->OrthoWidth = CaptureSize;
	CaptureSceneDepth->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureSceneDepth->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
	CaptureSceneDepth->SetRelativeRotation(FRotator(-90, -90, 0));
	CaptureSceneDepth->SetRelativeLocation(FVector(0, 0, 10000));
	CaptureSceneDepth->bCaptureEveryFrame = false;
	CaptureSceneDepth->SetupAttachment(SceneComponent, TEXT("CaptureSceneDepth"));
	
	CaptureSceneNormal = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureSceneNormal"));
	CaptureSceneNormal->OrthoWidth = CaptureSize;
	CaptureSceneNormal->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureSceneNormal->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureSceneNormal->CaptureSource = ESceneCaptureSource::SCS_Normal;
	CaptureSceneNormal->bCaptureEveryFrame = false;
	CaptureSceneNormal->SetupAttachment(CaptureSceneDepth, TEXT("CaptureSceneNormal"));
	
	CaptureBaseColor = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureBaseColor"));
	CaptureBaseColor->OrthoWidth = CaptureSize;
	CaptureBaseColor->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureBaseColor->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
	CaptureBaseColor->bCaptureEveryFrame = false;
	CaptureBaseColor->SetupAttachment(CaptureSceneDepth, TEXT("CaptureBaseColor"));
}

void ACSGenerateCaptureScene::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	Box->SetRelativeScale3D(FVector(CaptureSceneDepth->OrthoWidth / 100, CaptureSceneDepth->OrthoWidth / 100, MaxHeight / 100));
	Box->SetRelativeLocation(FVector(0, 0, Scale3DZ * 50));
	Box->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	CaptureSceneDepth->SetRelativeLocation(FVector(0, 0, MaxHeight));
	CaptureSceneDepth->OrthoWidth = CaptureSize;
	CaptureSceneNormal->OrthoWidth = CaptureSize;
	CaptureBaseColor->OrthoWidth = CaptureSize;
	
}

inline void ACSGenerateCaptureScene::Capture()
{
	CaptureSceneDepth->CaptureScene();
	CaptureSceneNormal->CaptureScene();
	CaptureBaseColor->CaptureScene();
}