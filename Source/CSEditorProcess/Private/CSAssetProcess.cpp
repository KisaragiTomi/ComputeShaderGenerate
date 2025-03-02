// Fill out your copyright notice in the Description page of Project Settings.


#include "CSAssetProcess.h"

#include "ClearQuad.h"
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
#include "AssetRegistry/AssetRegistryHelpers.h"
#include "Engine/SceneCapture.h"
#include "Engine/SceneCapture2D.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ComputeShaderSceneCapture.h"
#include "EditorAssetLibrary.h"


#include "Engine/StaticMeshActor.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"



class FAssetRegistryModule;

void UCSAssetProcess::CaptureMeshHeight(ACSMeshConverter* InConverter, UTextureRenderTarget2D*& OutRenderTarget2D, TArray<AActor*>& OutActors, int32 InTextureSize)
{
	if (InConverter == nullptr) return;
	UStaticMesh* ContainerMesh = InConverter->MeshToCapture;
	if (ContainerMesh == nullptr) return;

	FBoxSphereBounds Bounds = ContainerMesh->GetBounds();

	float MaxHeight = 10000;

	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	FActorSpawnParameters SpawnParameters;
	AStaticMeshActor *SpawnMeshTargetMesh = GWorld->SpawnActor<AStaticMeshActor>(SpawnParameters);
	AStaticMeshActor *SpawnMeshPlane = GWorld->SpawnActor<AStaticMeshActor>(SpawnParameters);
	SpawnMeshTargetMesh->GetStaticMeshComponent()->SetStaticMesh(ContainerMesh);
	SpawnMeshPlane->GetStaticMeshComponent()->SetStaticMesh(PlaneMesh);
	SpawnMeshPlane->SetActorScale3D(FVector(9999, 9999, 1));
	TArray<AActor*> TargetActors;
	TargetActors.Add(SpawnMeshPlane);
	TargetActors.Add(SpawnMeshTargetMesh);
	
	
	UTextureRenderTarget2D* NewRenderTarget2D = NewObject<UTextureRenderTarget2D>();
	check(NewRenderTarget2D);
	NewRenderTarget2D->RenderTargetFormat = ETextureRenderTargetFormat::RTF_RGBA16f;
	NewRenderTarget2D->ClearColor = FLinearColor::Black;
	NewRenderTarget2D->bAutoGenerateMips = false;
	NewRenderTarget2D->bCanCreateUAV = true;
	NewRenderTarget2D->InitAutoFormat(InTextureSize, InTextureSize);
	
	ASceneCapture2D * CaptureTarget = GWorld->SpawnActor<ASceneCapture2D>(SpawnParameters);
	FTransform CaptureTransform(FRotator(0, -90, -90), FVector(Bounds.Origin.X, Bounds.Origin.Y, MaxHeight), FVector::OneVector);
	
	
	CaptureTarget->SetActorTransform(CaptureTransform);
	CaptureTarget->GetCaptureComponent2D()->TextureTarget = NewRenderTarget2D;
	CaptureTarget->GetCaptureComponent2D()->OrthoWidth = FMath::Max(Bounds.BoxExtent.X, Bounds.BoxExtent.Y) * 2;
	CaptureTarget->GetCaptureComponent2D()->ShowOnlyActors = TargetActors;
	CaptureTarget->GetCaptureComponent2D()->ProjectionType = ECameraProjectionMode::Orthographic;
	CaptureTarget->GetCaptureComponent2D()->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
	CaptureTarget->GetCaptureComponent2D()->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;

	OutActors.Add(SpawnMeshPlane);
	OutActors.Add(SpawnMeshTargetMesh);
	OutActors.Add(CaptureTarget);
}

void UCSAssetProcess::CalculateMeshHeight(ACSMeshConverter* InConverter,  UTextureRenderTarget2D* NewRenderTarget2D)
{
	
	float MaxHeight = 10000;
	if (InConverter == nullptr || NewRenderTarget2D == nullptr) return;
	UStaticMesh* ContainerMesh = InConverter->MeshToCapture; 
	if (ContainerMesh == nullptr) return;

	FBoxSphereBounds Bounds = ContainerMesh->GetBounds();
	ULevel* CurrentLevel = InConverter->GetLevel();
	FString LevelPathName = GetPathNameSafe(CurrentLevel);
	FString FileName;
	FString LevelPath;
	FString Extension;
	FPaths::Split(LevelPathName, LevelPath, FileName, Extension);
	FString AssetPath = LevelPath.Append("/MeshHeightData");
	IAssetRegistry &AssetRegistry = FModuleManager::GetModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> OutAssetData;
	if (!AssetRegistry.GetAssetsByPath(FName(AssetPath), OutAssetData)) return;
	
	FRenderTarget* NewTextureTarget = NewRenderTarget2D->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[NewTextureTarget, MaxHeight](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			TShaderMapRef<FGeneralTempShader> ComputeShader = FGeneralTempShader::CreateTempShaderPermutation(FGeneralTempShader::ETempShader::GTS_ProcessMeshHeightTexture);

			FGeneralTempShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FGeneralTempShader::FParameters>();
			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(NewTextureTarget->GetSizeXY().X, NewTextureTarget->GetSizeXY().Y, 1), FComputeShaderUtils::kGolden2DGroupSize);
			
			FRDGTextureRef TmpTexture_ProcssTexture = CSHepler::ConvertToUVATexture(NewTextureTarget, GraphBuilder);
			FRDGTextureRef ProcssTexture = RegisterExternalTexture(GraphBuilder, NewTextureTarget->GetRenderTargetTexture(), TEXT("Input_RT"));
			
			PassParameters->T_ProcssTexture = ProcssTexture;
			PassParameters->RW_ProcssTexture = GraphBuilder.CreateUAV(TmpTexture_ProcssTexture);
			PassParameters->InputData = MaxHeight;
			PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();

			
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteExampleComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});
			
			AddCopyTexturePass(GraphBuilder, TmpTexture_ProcssTexture, ProcssTexture, FRHICopyTextureInfo());
		}
		GraphBuilder.Execute();

	});
	
	FlushRenderingCommands();
	
	
	TArray<FAssetData> AssetDatas;
	TArray<UTexture2D*> AssetTextures;
	for (int32 i = 0; i < OutAssetData.Num(); i++)
	{
		UCSMeshAsset* MeshAsset = Cast<UCSMeshAsset>(OutAssetData[i].GetAsset());
		if (MeshAsset != nullptr)
		{
			AssetDatas.Add(MeshAsset);
			continue;
		}

		UTexture2D* Texture = Cast<UTexture2D>(OutAssetData[i].GetAsset());
		if (Texture != nullptr)
		{
			AssetTextures.Add(Texture);
		}
	}

	

	
	FString ContainerMeshPathName = GetPathNameSafe(ContainerMesh);
	FString ContainerMeshFileName;
	FString ContainerMeshPath;
	FString ContainerMeshExtension;
	FPaths::Split(ContainerMeshPathName, ContainerMeshPath, ContainerMeshFileName, ContainerMeshExtension);


	IAssetTools &AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	// TScriptInterface<IAssetTools> AssetTools =  UAssetToolsHelpers::GetAssetTools();

	FString TextureAssetName = "T_";
	
	TextureAssetName.Append(ContainerMeshFileName);
	TextureAssetName.Append("_Height");
	
	FString TextureAssetPath = AssetPath.Append("/");
	TextureAssetPath.Append(TextureAssetName);

	UTexture2D* CurrentTextureObject = nullptr;
	bool FindTexture = false;
	if (UEditorAssetLibrary::DoesAssetExist(TextureAssetPath))
	{
		UObject* LoadObject = UEditorAssetLibrary::LoadAsset(TextureAssetPath);
		UTexture2D* Texture = Cast<UTexture2D>(LoadObject);
		if (Texture != nullptr)
		{
			CurrentTextureObject = Texture;
			FindTexture = true;
		}
	}
	if (!FindTexture)
	{
		UObject* NewTextureObject = AssetTools.CreateAsset(TextureAssetName, AssetPath, UTexture2D::StaticClass(), nullptr);
		UTexture2D* NewTexture = Cast<UTexture2D>(NewTextureObject);
		if (NewTexture != nullptr)
		{
			CurrentTextureObject = NewTexture;
		}
	}


	FString MeshAssetName = "MA_";
	
	MeshAssetName.Append(ContainerMeshFileName);
	MeshAssetName.Append("_MeshData");
	
	FString MeshAssetPath = AssetPath.Append("/");
	MeshAssetPath.Append(MeshAssetName);
	
	UCSMeshAsset* CurrentMeshAssetObject = nullptr;
	bool FindMeshAsset = false;
	if (UEditorAssetLibrary::DoesAssetExist(MeshAssetPath))
	{
		UObject* LoadObject = UEditorAssetLibrary::LoadAsset(MeshAssetPath);
		UCSMeshAsset* MeshAsset = Cast<UCSMeshAsset>(LoadObject);
		if (MeshAsset != nullptr)
		{
			CurrentMeshAssetObject = MeshAsset;
			FindMeshAsset = true;
		}
	}
	if (!FindMeshAsset)
	{
		
		UObject* NewMeshAssetObject = AssetTools.CreateAsset(MeshAssetName, AssetPath, UCSMeshAsset::StaticClass(), nullptr);
		UCSMeshAsset* NewMeshAsset = Cast<UCSMeshAsset>(NewMeshAssetObject);
		if (NewMeshAsset != nullptr)
		{
			CurrentMeshAssetObject = NewMeshAsset;
		}
	}
	
	CurrentTextureObject->CompressionNoAlpha = true;
	CurrentTextureObject->MipGenSettings = TMGS_NoMipmaps;
	CurrentTextureObject->SRGB = false;
	CurrentTextureObject->DeferCompression = true;
	CurrentTextureObject->AddressX = TextureAddress::TA_Clamp;
	CurrentTextureObject->AddressY = TextureAddress::TA_Clamp;
	
	UKismetRenderingLibrary::ConvertRenderTargetToTexture2DEditorOnly(GWorld, NewRenderTarget2D, CurrentTextureObject);
	
	CurrentTextureObject->CompressionSettings = TextureCompressionSettings::TC_HDR;
	
	CurrentMeshAssetObject->CSStaticMesh = ContainerMesh;
	CurrentMeshAssetObject->CSMeshHeightTexture = CurrentTextureObject;
	CurrentMeshAssetObject->CSMeshPivot = FVector(-Bounds.Origin.X, -Bounds.Origin.Y, Bounds.BoxExtent.Z - Bounds.Origin.Z);
	CurrentMeshAssetObject->CSMeshMaxHeight = Bounds.BoxExtent.Z * 2;
	CurrentMeshAssetObject->CSMeshSize =  FMath::Max(ContainerMesh->GetBounds().BoxExtent.X, ContainerMesh->GetBounds().BoxExtent.Y) * 2;

	UEditorAssetLibrary::SaveLoadedAsset(CurrentTextureObject, false);
	UEditorAssetLibrary::SaveLoadedAsset(CurrentMeshAssetObject, false);

	
	NewRenderTarget2D = nullptr;
}
