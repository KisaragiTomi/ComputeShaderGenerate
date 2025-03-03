#include "ComputeShaderSceneCapture.h"

#include "ClearQuad.h"
#include "ComputeShaderGenerateHepler.h"
#include "GlobalShader.h"
#include "MaterialShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "ComputeShaderGenerateHepler.h"
#include "EngineUtils.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "ComputeShaderGeneral.h"
#include "IAssetTools.h"
#include "Landscape.h"
#include "LandscapeExtra.h"
#include "Engine/SceneCapture.h"
#include "Engine/SceneCapture2D.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "Kismet/KismetSystemLibrary.h"

ACSGenerateCaptureScene::ACSGenerateCaptureScene()
: Super()
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
	CaptureSceneDepth->SetRelativeLocation(FVector(0, 0, -10000));
	CaptureSceneDepth->bCaptureEveryFrame = false;
	CaptureSceneDepth->SetupAttachment(SceneComponent, TEXT("CaptureSceneDepth"));

	CaptureSceneNormal = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureSceneNormal"));
	CaptureSceneNormal->OrthoWidth = CaptureSize;
	CaptureSceneNormal->ProjectionType = ECameraProjectionMode::Orthographic;
	// CaptureSceneNormal->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureSceneNormal->CaptureSource = ESceneCaptureSource::SCS_Normal;
	CaptureSceneNormal->bCaptureEveryFrame = false;
	CaptureSceneNormal->SetupAttachment(CaptureSceneDepth, TEXT("CaptureSceneNormal"));

	CaptureObjectDepth = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureObjectDepth"));
	CaptureObjectDepth->OrthoWidth = CaptureSize;
	CaptureObjectDepth->ProjectionType = ECameraProjectionMode::Orthographic;
	// CaptureObjectDepth->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureObjectDepth->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
	CaptureObjectDepth->SetRelativeRotation(FRotator(-90, -90, 0));
	CaptureObjectDepth->SetWorldLocation(FVector(0, 0, MaxHeight));
	CaptureObjectDepth->bCaptureEveryFrame = false;
	CaptureObjectDepth->SetupAttachment(SceneComponent, TEXT("CaptureObjectDepth"));
	
	CaptureObjNormal = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureObjNormal"));
	CaptureObjNormal->OrthoWidth = CaptureSize;
	CaptureObjNormal->ProjectionType = ECameraProjectionMode::Orthographic;
	// CaptureObjNormal->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureObjNormal->CaptureSource = ESceneCaptureSource::SCS_Normal;
	CaptureObjNormal->bCaptureEveryFrame = false;
	CaptureObjNormal->SetupAttachment(CaptureObjectDepth, TEXT("CaptureObjNormal"));
	
	CaptureObjBaseColor = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("CaptureObjBaseColor"));
	CaptureObjBaseColor->OrthoWidth = CaptureSize;
	CaptureObjBaseColor->ProjectionType = ECameraProjectionMode::Orthographic;
	// CaptureObjBaseColor->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureObjBaseColor->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
	CaptureObjBaseColor->bCaptureEveryFrame = false;
	CaptureObjBaseColor->SetupAttachment(CaptureObjectDepth, TEXT("CaptureObjBaseColor"));

	DynamicMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMeshComponent"));
	DynamicMeshComponent->SetupAttachment(RootComponent);
}

void ACSGenerateCaptureScene::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	
	Box->SetRelativeScale3D(FVector(CaptureSceneDepth->OrthoWidth / 100, CaptureSceneDepth->OrthoWidth / 100, MaxHeight / 100));
	Box->SetRelativeLocation(FVector(0, 0, Scale3DZ * 50));
	Box->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);


	DynamicMeshComponent->SetWorldLocation(FVector(0, 0, -MaxHeight * 2));
	DynamicMeshComponent->AttachToComponent(RootComponent, FAttachmentTransformRules::KeepRelativeTransform);

	//CaptureSceneDepth->SetRelativeLocation(FVector(0, 0, MaxHeight));
	CaptureSceneDepth->SetRelativeLocation(FVector(0, 0, -MaxHeight));
	CaptureObjectDepth->SetRelativeLocation(FVector(0, 0, MaxHeight));
	CaptureSceneDepth->OrthoWidth = CaptureSize;
	CaptureObjNormal->OrthoWidth = CaptureSize;
	CaptureObjBaseColor->OrthoWidth = CaptureSize;
	CaptureSceneNormal->OrthoWidth = CaptureSize;
	CaptureObjectDepth->OrthoWidth = CaptureSize;
	
}



inline void ACSGenerateCaptureScene::CaptureAll()
{
	if (CaptureSceneDepth->TextureTarget != nullptr)	CaptureSceneDepth->CaptureScene();
	if (CaptureSceneNormal->TextureTarget != nullptr)	CaptureSceneNormal->CaptureScene();
	if (CaptureObjNormal->TextureTarget != nullptr)		CaptureObjNormal->CaptureScene();
	if (CaptureObjBaseColor->TextureTarget != nullptr)	CaptureObjBaseColor->CaptureScene();
	if (CaptureObjectDepth->TextureTarget != nullptr)	CaptureObjectDepth->CaptureScene();
}

void ACSGenerateCaptureScene::CaptureMeshsInBox()
{
	// ALandscape* Landscape = nullptr;
	// if (UWorld* World = GEngine->GetWorldFromContextObject(GWorld, EGetWorldErrorMode::LogAndReturnNull))
	// {
	// 	for (TActorIterator<ALandscape> It(World, ALandscape::StaticClass()); It; ++It)
	// 	{
	// 		Landscape = *It;
	// 		break;
	// 	}
	// }
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypes{ UEngineTypes::ConvertToObjectType(ECollisionChannel::ECC_WorldStatic) };
	TArray<AActor*> ActorsToIgnore ;
	// if (Landscape) ActorsToIgnore.Add(Landscape);
	TArray<AActor*> OverlapOutActors;
	UKismetSystemLibrary::ComponentOverlapActors(Box, Box->GetComponentTransform(), ObjectTypes, AStaticMeshActor::StaticClass(), ActorsToIgnore, OverlapOutActors);

	CaptureObjectDepth->ShowOnlyActors = OverlapOutActors;
	CaptureObjNormal->ShowOnlyActors = OverlapOutActors;
	CaptureObjBaseColor->ShowOnlyActors = OverlapOutActors;
	if (CaptureObjNormal->TextureTarget != nullptr)		CaptureObjNormal->CaptureScene();
	if (CaptureObjBaseColor->TextureTarget != nullptr)	CaptureObjBaseColor->CaptureScene();
	if (CaptureObjectDepth->TextureTarget != nullptr)	CaptureObjectDepth->CaptureScene();
	if (CaptureSceneDepth->TextureTarget != nullptr)	CaptureSceneDepth->CaptureScene();
	if (CaptureSceneNormal->TextureTarget != nullptr)	CaptureSceneNormal->CaptureScene();
}

bool ACSGenerateCaptureScene::CreateLandscapeMesh()
{
	UDynamicMesh* Mesh = DynamicMeshComponent->GetDynamicMesh();
	FVector Center = Box->Bounds.Origin;
	FVector Extent = Box->Bounds.BoxExtent;
	if(PreCenter == Center && PreExtent == Extent && Mesh->GetTriangleCount() > 0) return false;
	
	PreCenter = Center;
	PreExtent = Extent;
	
	ULandscapeExtra::CreateProjectPlane(Mesh, PreCenter, PreExtent, 3);
	FGeometryScriptCalculateNormalsOptions CalculateOptions;
	UGeometryScriptLibrary_MeshNormalsFunctions::RecomputeNormals(Mesh, CalculateOptions);
	return true;
}

UTextureRenderTarget2D* ACSGenerateCaptureScene::CreateRenderTarget(float InTextureSize)
{
	UTextureRenderTarget2D* NewRenderTarget2D = NewObject<UTextureRenderTarget2D>();
	check(NewRenderTarget2D);
	NewRenderTarget2D->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
	NewRenderTarget2D->ClearColor = FLinearColor::Black;
	NewRenderTarget2D->bAutoGenerateMips = false;
	NewRenderTarget2D->bCanCreateUAV = true;
	NewRenderTarget2D->InitAutoFormat(InTextureSize, InTextureSize);
	return NewRenderTarget2D;
}
