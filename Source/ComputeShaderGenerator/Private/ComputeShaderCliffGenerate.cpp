#include "ComputeShaderCliffGenerate.h"
#include "ComputeShaderMeshFill.h"
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
#include "ComputeShaderBasicFunction.h"
#include "Engine/Texture2DArray.h"

DECLARE_STATS_GROUP(TEXT("CSCliffGenerate"), STATGROUP_CSCliffGenerate, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("CS Execute"), STAT_CSCliffGenerate_Execute, STATGROUP_CSCliffGenerate)
DECLARE_CYCLE_STAT(TEXT("CS Capture"), STAT_CSCliffGenerate_Capture, STATGROUP_CSCliffGenerate)
DECLARE_CYCLE_STAT(TEXT("CS Tatal"), STAT_CSCliffGenerate_Tatal, STATGROUP_CSCliffGenerate);

using namespace CSHepler;

ACSCliffGenerateCapture::ACSCliffGenerateCapture()
: Super()
{

}

void ACSCliffGenerateCapture::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

}


inline void ACSCliffGenerateCapture::GenerateCliffVertical(int32 NumIteration, float InSpawnSize)
{
	SCOPE_CYCLE_COUNTER(STAT_CSCliffGenerate_Tatal);
	if (!IsParameterValidMult()) return;


	SpawnSize = InSpawnSize;
	TArray<FCSCliffGenerateData> GenerateDatas;
	GenerateDatas.Reserve(NumIteration);
	for (int32 i = 0; i < NumIteration; i++)
	{
		int32 SelectIndex = FMath::RandRange(0, MeshDataAssets.Num() - 1);
		// float RandomRotate = FMath::FRandRange(0.0, 7.0);
		// float RandomHeightOffset = FMath::FRandRange(-350.0, 550.0) * (SelectIndex > 1);
		
		FCSCliffGenerateData GenerateData;
		GenerateData.SelectIndex = SelectIndex;
		GenerateData.RandomScale = MeshDataAssets[SelectIndex]->RandomScale;
		GenerateData.RandomRotate = MeshDataAssets[SelectIndex]->RandomRotate;
		GenerateData.RandomHeightOffset = MeshDataAssets[SelectIndex]->RandomHeightOffset;
		GenerateDatas.Add(GenerateData);
	}
	// MeshDataAsset.
	

	{
		SCOPE_CYCLE_COUNTER(STAT_CSCliffGenerate_Capture);
		CaptureMeshsInBox();
		FlushRenderingCommands();
		GenerateTargetHeightCal();
		FlushRenderingCommands();
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_CSCliffGenerate_Execute);
		GenerateCliffVerticalCal(GenerateDatas);
		FlushRenderingCommands();
	}
	
	// void* Data = RHILockBuffer(DebugBuffer->GetRHI(), ;
	TArray<FLinearColor> LinearSamples;
	
	FRenderTarget* RT_Result = InResult->GameThread_GetRenderTargetResource();
	FIntRect SampleRect(0, 0, InResult->SizeX - 1, InResult->SizeY - 1);
	FReadSurfaceDataFlags ReadSurfaceDataFlags = FReadSurfaceDataFlags(RCM_MinMax);
	RT_Result->ReadLinearColorPixels(LinearSamples, ReadSurfaceDataFlags, SampleRect);
	
	// switch (EPixelFormat ReadRenderTargetHelper(Samples, LinearSamples, WorldContextObject, TextureRenderTarget, X, Y, 1, 1))
	// {
	// case PF_B8G8R8A8:
	// 	check(Samples.Num() == 1 && LinearSamples.Num() == 0);
	// 	return Samples[0];
	// case PF_FloatRGBA:
	// 	check(Samples.Num() == 0 && LinearSamples.Num() == 1);
	// 	return LinearSamples[0].ToFColor(true);
	float ResultSize = InResult->SizeX;
	TArray<FLinearColor> Colors;
	UKismetRenderingLibrary::ReadRenderTargetRaw(GWorld, InResult, Colors, false);
	int32 MaxGenerate = FMath::RoundToInt(Colors[Colors.Num() - 1].R);
	if (MaxGenerate == 0)
		return;

	for (int32 i = 0; i < MaxGenerate; i++)
	{
		FLinearColor LocationResultColor = Colors[i];
		float LocationX = LocationResultColor.R;
		float LocationY = LocationResultColor.G;
		float LocationZ = LocationResultColor.B;
		
		FLinearColor RSIResultColor = Colors[i + InResult->SizeX];
		float ResultRotate = RSIResultColor.R;
		float ResultScale = RSIResultColor.G;
		int32 SelectIndex = RSIResultColor.B;
		
		UStaticMesh* GenerateStaticMesh = MeshDataAssets[SelectIndex]->CSStaticMesh;
		float MeshSize = MeshDataAssets[SelectIndex]->CSMeshSize;
		
		FActorSpawnParameters SpawnParameters;
		AStaticMeshActor *SpawnMesh = GWorld->SpawnActor<AStaticMeshActor>(SpawnParameters);
		// AStaticMeshActor *SpawnMeshCopy = GWorld->SpawnActor<AStaticMeshActor>(SpawnParameters);

		float RandomRotate = FMath::RandRange(0, 1) * 180;
		FVector SpawnLocation = FVector(LocationX * CaptureSize, LocationY * CaptureSize, LocationZ) + GetActorLocation() - FVector(1, 1, 0) * CaptureSize / 2;
		FRotator SpawnRotation = FRotator(0, FMath::RadiansToDegrees(ResultRotate) + RandomRotate, 0);
		FVector SpawnScale = FVector::OneVector * ResultScale / MeshSize * CaptureSize;
		
		FTransform SpawnTransform(SpawnRotation, SpawnLocation, SpawnScale);
		
		SpawnMesh->SetActorTransform(SpawnTransform);
		SpawnMesh->GetStaticMeshComponent()->SetStaticMesh(GenerateStaticMesh);
		SpawnTransform.SetLocation(FVector(SpawnTransform.GetLocation().X, SpawnTransform.GetLocation().Y, SpawnTransform.GetLocation().Z - MaxHeight * 4));
		// SpawnMeshCopy->SetActorTransform(SpawnTransform);
		// SpawnMeshCopy->GetStaticMeshComponent()->SetStaticMesh(GenerateStaticMesh);
		// SpawnMeshCopy->Tags = {Tag};
		SpawnMesh->Tags = {Tag};
	}
	
	TArray<AActor*> StaticMeshActors;
	for (TActorIterator<AActor> It(GWorld, AStaticMeshActor::StaticClass()); It; ++It)
	{
		AActor* Actor = *It;
		if (IsValid(Actor) && Actor->ActorHasTag(Tag))
		{
			StaticMeshActors.Add(Actor);
		}
	}
	CaptureObjNormal->ShowOnlyActors = StaticMeshActors;
	CaptureMeshsInBox();
}

void ACSCliffGenerateCapture::GenerateTargetHeightCal()
{

	if (!IsParameterValidMult()) return;

	InObjectDepth->ResizeTarget(TextureSize, TextureSize);
	InObjectNormal->ResizeTarget(TextureSize, TextureSize);
	InSceneNormal->ResizeTarget(TextureSize, TextureSize);
	InSceneDepth->ResizeTarget(TextureSize, TextureSize);
	InDebugView->ResizeTarget(TextureSize, TextureSize);
	InCurrentSceneDepth->ResizeTarget(TextureSize, TextureSize);
	
	InTargetHeight->ResizeTarget(TextureSize, TextureSize);

	FRenderTarget* ObjectNormal = InObjectNormal->GameThread_GetRenderTargetResource();
	FRenderTarget* SceneNormal = InSceneNormal->GameThread_GetRenderTargetResource();
	FRenderTarget* SceneDepth = InSceneDepth->GameThread_GetRenderTargetResource();
	FRenderTarget* DebugView = InDebugView->GameThread_GetRenderTargetResource();
	FRenderTarget* CurrentSceneDepth = InCurrentSceneDepth->GameThread_GetRenderTargetResource();
	FRenderTarget* TargetHeight = InTargetHeight->GameThread_GetRenderTargetResource();
	FRenderTarget* ObjectDepth = InObjectDepth->GameThread_GetRenderTargetResource();
	float SizeX = SceneDepth->GetSizeXY().X;
	float SizeY = SceneDepth->GetSizeXY().Y;
	
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=, this ](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			DECLARE_GPU_STAT(CSMeshFill)
			RDG_EVENT_SCOPE(GraphBuilder, "CSMeshFill");
			RDG_GPU_STAT_SCOPE(GraphBuilder, CSMeshFill);

			TShaderMapRef<FMeshFillMult> ComputeShader_Init = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_Init);
			TShaderMapRef<FMeshFillMult> ComputeShader_TargetHeight = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_TargetHeight);


			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(SizeX, SizeY, 1), FComputeShaderUtils::kGolden2DGroupSize);
			FMeshFillMult::FParameters* PassParameters = GraphBuilder.AllocParameters<FMeshFillMult::FParameters>();
			
			FRDGTextureRef TmpTexture_DebugView = ConvertToUVATextureFormat(GraphBuilder, DebugView, PF_FloatRGBA, TEXT("DebugView_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);

			FRDGTextureRef TmpTexture_CurrentSceneDepth = ConvertToUVATextureFormat(GraphBuilder, CurrentSceneDepth, PF_FloatRGBA, TEXT("CurrentSceneDepth_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_CurrentSceneDepth = GraphBuilder.CreateUAV(TmpTexture_CurrentSceneDepth);
			

			FRDGTextureRef TmpTexture_TargetHeight = ConvertToUVATextureFormat(GraphBuilder, TargetHeight, PF_FloatRGBA, TEXT("TargetHeight_Texture"));
			FRDGTextureUAVRef TmpTextureUAV_TargetHeight = GraphBuilder.CreateUAV(TmpTexture_TargetHeight);
			FRDGTextureRef TmpTexture_A = ConvertToUVATextureFormat(GraphBuilder, TargetHeight, PF_FloatRGBA, TEXT("Temp_A"));
			FRDGTextureUAVRef TmpTextureUAV_A = GraphBuilder.CreateUAV(TmpTexture_A);
			FRDGTextureRef TmpTexture_B = ConvertToUVATextureFormat(GraphBuilder, TargetHeight, PF_FloatRGBA, TEXT("Temp_B"));
			FRDGTextureUAVRef TmpTextureUAV_B = GraphBuilder.CreateUAV(TmpTexture_B);
			
			
			FRDGTextureRef CurrentSceneDepthTexture =  RegisterExternalTexture(GraphBuilder, CurrentSceneDepth->GetRenderTargetTexture(), TEXT("CurrentSceneDepth_RT"));
			FRDGTextureRef SceneDepthTexture = RegisterExternalTexture(GraphBuilder, SceneDepth->GetRenderTargetTexture(), TEXT("SceneDepth_RT"));
			FRDGTextureRef SceneNormalTexture = RegisterExternalTexture(GraphBuilder, SceneNormal->GetRenderTargetTexture(), TEXT("SceneNormal_RT"));
			FRDGTextureRef ObjectNormalTexture = RegisterExternalTexture(GraphBuilder, ObjectNormal->GetRenderTargetTexture(), TEXT("ObjectNormal_RT"));
			FRDGTextureRef TargetHeightTexture = RegisterExternalTexture(GraphBuilder, TargetHeight->GetRenderTargetTexture(), TEXT("TargetHeight_T"));
			FRDGTextureRef ObjectDepthTexture = RegisterExternalTexture(GraphBuilder, ObjectDepth->GetRenderTargetTexture(), TEXT("ObjectDepth_T"));
			
			PassParameters->T_ObjectDepth = ObjectDepthTexture;
			PassParameters->T_TargetHeight = TargetHeightTexture;
			PassParameters->T_SceneDepth = SceneDepthTexture;
			PassParameters->T_SceneNormal = SceneNormalTexture;
			PassParameters->T_CurrentSceneDepth = CurrentSceneDepthTexture;
			PassParameters->T_ObjectNormal = ObjectNormalTexture;
			PassParameters->RW_CurrentSceneDepth = TmpTextureUAV_CurrentSceneDepth;
			PassParameters->RW_DebugView = TmpTextureUAV_DebugView;
			PassParameters->RW_TargetHeight = TmpTextureUAV_TargetHeight;
			PassParameters->RW_TempA = TmpTextureUAV_A;
			PassParameters->RW_TempB = TmpTextureUAV_B;
			PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
			
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("MeshFillVerticalRock"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader_Init, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Init, *PassParameters, GroupCount);
				});
			AddCopyTexturePass(GraphBuilder, TmpTexture_CurrentSceneDepth, CurrentSceneDepthTexture, FRHICopyTextureInfo());
			AddCopyTexturePass(GraphBuilder, TmpTexture_TargetHeight, TargetHeightTexture, FRHICopyTextureInfo());
			AddCopyTexturePass(GraphBuilder, TmpTexture_TargetHeight, TmpTexture_A, FRHICopyTextureInfo());
			AddCopyTexturePass(GraphBuilder, TmpTexture_TargetHeight, TmpTexture_B, FRHICopyTextureInfo());
			
			for (int32 i = 0; i < GroupCount.X * 2; i++)
			{
				GraphBuilder.AddPass(
				RDG_EVENT_NAME("GenerateTargetHeight"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader_TargetHeight, GroupCount, i, TmpTextureUAV_A, TmpTextureUAV_B](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_TargetHeight, *PassParameters, GroupCount);
					if ( i % 2 == 0)
					{
						PassParameters->RW_TempA = TmpTextureUAV_B;
						PassParameters->RW_TempB = TmpTextureUAV_A;
					}
					else
					{
						PassParameters->RW_TempA = TmpTextureUAV_A;
						PassParameters->RW_TempB = TmpTextureUAV_B;
					}
				});
			}
			
			AddCopyTexturePass(GraphBuilder, TmpTexture_A, TargetHeightTexture, FRHICopyTextureInfo());
			
			typename FBlurTexture::FPermutationDomain PermutationVector;
			PermutationVector.Set<FBlurTexture::FBlurFunctionSet>(FBlurTexture::EBlurType::BT_BLUR15X15);
			TShaderMapRef<FBlurTexture> ComputeShader_Blur(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
			
			FBlurTexture::FParameters* BlurPassParameters = GraphBuilder.AllocParameters<FBlurTexture::FParameters>();
			
			BlurPassParameters->T_BlurTexture = TargetHeightTexture;
			BlurPassParameters->RW_BlurTexture = TmpTextureUAV_TargetHeight ;
			BlurPassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
			BlurPassParameters->BlurScale = 1;
			
			for (int32 i = 0; i < 1; i++)
			{
				GraphBuilder.AddPass(
				RDG_EVENT_NAME("Blur"),
				BlurPassParameters,
				ERDGPassFlags::AsyncCompute,
				[&BlurPassParameters, ComputeShader_Blur, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Blur, *BlurPassParameters, GroupCount);
				});
				AddCopyTexturePass(GraphBuilder,TmpTexture_TargetHeight , TargetHeightTexture, FRHICopyTextureInfo());
			}
			
			FRDGTextureRef DebugViewTexture = RegisterExternalTexture(GraphBuilder, DebugView->GetRenderTargetTexture(), TEXT("DebugView_RT"));
			AddCopyTexturePass(GraphBuilder, TmpTexture_DebugView, DebugViewTexture, FRHICopyTextureInfo());
		}
		GraphBuilder.Execute();
	});
}


void ACSCliffGenerateCapture::GenerateCliffVerticalCal(TArray<FCSCliffGenerateData> GenerateDatas)
{
	if (!IsParameterValidMult() || GenerateDatas.Num() == 0) return;
	
	InObjectDepth->ResizeTarget(TextureSize, TextureSize);
	InObjectNormal->ResizeTarget(TextureSize, TextureSize);
	InSceneNormal->ResizeTarget(TextureSize, TextureSize);
	InSceneDepth->ResizeTarget(TextureSize, TextureSize);
	InDebugView->ResizeTarget(TextureSize, TextureSize);
	InCurrentSceneDepth->ResizeTarget(TextureSize, TextureSize);
	
	
	//InConectivityClassifiy->ResizeTarget(TextureSize, TextureSize);
	InTargetHeight->ResizeTarget(TextureSize, TextureSize);
	int32 ResultSize = GenerateTextureSize(GenerateDatas.Num());
	InResult->ResizeTarget(ResultSize, 2);
	
	FRenderTarget* ObjectNormal = InObjectNormal->GameThread_GetRenderTargetResource();
	FRenderTarget* SceneNormal = InSceneNormal->GameThread_GetRenderTargetResource();
	FRenderTarget* SceneDepth = InSceneDepth->GameThread_GetRenderTargetResource();
	FRenderTarget* DebugView = InDebugView->GameThread_GetRenderTargetResource();
	//FRenderTarget* ConectivityClassifiy = InConectivityClassifiy->GameThread_GetRenderTargetResource();
	FRenderTarget* Result = InResult->GameThread_GetRenderTargetResource();
	FRenderTarget* CurrentSceneDepth = InCurrentSceneDepth->GameThread_GetRenderTargetResource();
	FRenderTarget* TargetHeight = InTargetHeight->GameThread_GetRenderTargetResource();
	FRenderTarget* ObjectDepth = InObjectDepth->GameThread_GetRenderTargetResource();
	float SizeX = SceneDepth->GetSizeXY().X;
	float SizeY = SceneDepth->GetSizeXY().Y;

	InMeshHeightArray->SourceTextures.Empty();
	//MeshHeightArray->SourceTextures.Add();
	for (int32 i = 0; i < MeshDataAssets.Num(); i++)
	{
		InMeshHeightArray->SourceTextures.Add(MeshDataAssets[i]->CSMeshHeightTexture);
	}
	InMeshHeightArray->UpdateSourceFromSourceTextures();
	
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=, this ](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			DECLARE_GPU_STAT(CSMeshFill)
			RDG_EVENT_SCOPE(GraphBuilder, "CSMeshFill");
			RDG_GPU_STAT_SCOPE(GraphBuilder, CSMeshFill);

			TShaderMapRef<FMeshFillMult> ComputeShader_Init = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_Init);
			TShaderMapRef<FMeshFillMult> ComputeShader_FVR = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_FillVerticalRock);
			TShaderMapRef<FMeshFillMult> ComputeShader_FindPixel = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_FindBestPixel);
			TShaderMapRef<FMeshFillMult> ComputeShader_FindPixelRW = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_FindBestPixelRW_256);
			TShaderMapRef<FMeshFillMult> ComputeShader_Update = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_UpdateCurrentHeight);
			TShaderMapRef<FMeshFillMult> ComputeShader_ExtentGenerateMask = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_ExtentGenerateMask);
			TShaderMapRef<FMeshFillMult> ComputeShader_FilterResultExtent = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_FilterResultExtent);
			TShaderMapRef<FMeshFillMult> ComputeShader_FilterResultReduce = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_FilterResultReduce);
			TShaderMapRef<FMeshFillMult> ComputeShader_Deduplication = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_Deduplication);
			TShaderMapRef<FMeshFillMult> ComputeShader_FilterResultSelect = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_FilterResultSelect);


			FIntPoint TextureSizeXY = SceneDepth->GetSizeXY();
			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(SizeX, SizeY, 1), FComputeShaderUtils::kGolden2DGroupSize);
			FIntVector ReductionGroupSize = FIntVector(SizeX * SizeY / SHAREGROUP_FINDEXT_SIZE, 1, 1);
			FMeshFillMult::FParameters* PassParameters = GraphBuilder.AllocParameters<FMeshFillMult::FParameters>();
			
			FRDGTextureRef TmpTexture_DebugView = ConvertToUVATextureFormat(GraphBuilder, DebugView, PF_FloatRGBA, TEXT("DebugView_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);
			
			FRDGTextureRef TmpTexture_CurrentSceneDepth = ConvertToUVATextureFormat(GraphBuilder, CurrentSceneDepth, PF_FloatRGBA, TEXT("CurrentSceneDepth_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_CurrentSceneDepth = GraphBuilder.CreateUAV(TmpTexture_CurrentSceneDepth);
			
			FRDGTextureRef TmpTexture_CurrentSceneDepthA = ConvertToUVATextureFormat(GraphBuilder, CurrentSceneDepth, PF_FloatRGBA, TEXT("CurrentSceneDepthA_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_CurrentSceneDepthA = GraphBuilder.CreateUAV(TmpTexture_CurrentSceneDepthA);
			FRDGTextureRef TmpTexture_CurrentSceneDepthB = ConvertToUVATextureFormat(GraphBuilder, CurrentSceneDepth, PF_FloatRGBA, TEXT("CurrentSceneDepthB_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_CurrentSceneDepthB = GraphBuilder.CreateUAV(TmpTexture_CurrentSceneDepthB);
			
			FRDGTextureRef TmpTexture_Result = ConvertToUVATextureFormat(GraphBuilder, Result, PF_FloatRGBA, TEXT("Result_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_Result = GraphBuilder.CreateUAV(TmpTexture_Result);
			
			FRDGTextureRef TmpTexture_ResultA = ConvertToUVATextureFormat(GraphBuilder, Result, PF_A32B32G32R32F, TEXT("ResultA_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_ResultA = GraphBuilder.CreateUAV(TmpTexture_ResultA);
			FRDGTextureRef TmpTexture_ResultB = ConvertToUVATextureFormat(GraphBuilder, Result, PF_A32B32G32R32F, TEXT("ResultB_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_ResultB = GraphBuilder.CreateUAV(TmpTexture_ResultB);

			FRDGTextureRef TmpTexture_FilterResult = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_A32B32G32R32F, TEXT("Result_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_FilterResult = GraphBuilder.CreateUAV(TmpTexture_FilterResult);

			FRDGTextureRef TmpTexture_TargetHeight = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_FloatRGBA, TEXT("TargetHeight_Texture"));
			FRDGTextureUAVRef TmpTextureUAV_TargetHeight = GraphBuilder.CreateUAV(TmpTexture_TargetHeight);

			FRDGTextureRef TmpTexture_SaveRotateScale = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_FloatRGBA, TEXT("SaveRotateSacle_Texture"));
			FRDGTextureUAVRef TmpTextureUAV_SaveRotateScale = GraphBuilder.CreateUAV(TmpTexture_SaveRotateScale);

			FRDGTextureRef TmpTexture_FilterResulteMult = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_FloatRGBA, TEXT("FilterResulteMult_Texture"));
			FRDGTextureUAVRef TmpTextureUAV_FilterResulteMult = GraphBuilder.CreateUAV(TmpTexture_FilterResulteMult);
			
			FRDGTextureRef TmpTexture_Deduplication = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_FloatRGBA, TEXT("Deduplication_Texture"));
			FRDGTextureUAVRef TmpTextureUAV_Deduplication = GraphBuilder.CreateUAV(TmpTexture_Deduplication);
			
			FRDGTextureRef CurrentSceneDepthTexture =  RegisterExternalTexture(GraphBuilder, CurrentSceneDepth->GetRenderTargetTexture(), TEXT("CurrentSceneDepth_RT"));
			FRDGTextureRef SceneDepthTexture = RegisterExternalTexture(GraphBuilder, SceneDepth->GetRenderTargetTexture(), TEXT("SceneDepth_RT"));
			FRDGTextureRef SceneNormalTexture = RegisterExternalTexture(GraphBuilder, SceneNormal->GetRenderTargetTexture(), TEXT("SceneNormal_RT"));
			FRDGTextureRef ObjectNormalTexture = RegisterExternalTexture(GraphBuilder, ObjectNormal->GetRenderTargetTexture(), TEXT("ObjectNormal_RT"));
			FRDGTextureRef ResultTexture = RegisterExternalTexture(GraphBuilder, Result->GetRenderTargetTexture(), TEXT("Result_RT"));
			FRDGTextureRef TMeshDepthTextureArray = RegisterExternalTexture(GraphBuilder, InMeshHeightArray->GetResource()->GetTextureRHI(), TEXT("MeshHeight_T"));
			
			FRDGTextureRef TargetHeightTexture = RegisterExternalTexture(GraphBuilder, TargetHeight->GetRenderTargetTexture(), TEXT("TargetHeight_T"));
			FRDGTextureRef ObjectDepthTexture = RegisterExternalTexture(GraphBuilder, ObjectDepth->GetRenderTargetTexture(), TEXT("ObjectDepth_T"));
			
			TArray<FRDGTextureRef> MeshHeightTextures;
			MeshHeightTextures.Reserve(GenerateDatas.Num());
			for (int32 i = 0 ; i < GenerateDatas.Num() ; i++)
			{
				UTexture2D* MeshHeightMap = MeshDataAssets[GenerateDatas[i].SelectIndex]->CSMeshHeightTexture;
				FRDGTextureRef TMeshDepthTexture = RegisterExternalTexture(GraphBuilder, MeshHeightMap->GetResource()->GetTextureRHI(), TEXT("MeshHeight_T"));
				MeshHeightTextures.Add(TMeshDepthTexture);
			}

			
			const uint32 NumElements = SizeX * SizeY;
			const uint32 BytesPerElement = sizeof(FVector4f);
			FRDGBufferRef Tmp_CountBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(BytesPerElement, NumElements), TEXT("FindPixelBuffer"));
			FRDGBufferUAVRef Tmp_CountBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Tmp_CountBuffer, EPixelFormat::PF_FloatRGBA));
			AddClearUAVPass(GraphBuilder,Tmp_CountBufferUAV, 0.0);
			
			FRDGBufferRef Tmp_FilterResult_Number = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(BytesPerElement, 256), TEXT("FindPixelBuffer"));
			FRDGBufferUAVRef Tmp_FilterResult_Number_UAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Tmp_CountBuffer, EPixelFormat::PF_R32G32_UINT));
			AddClearUAVPass(GraphBuilder,Tmp_FilterResult_Number_UAV, -1.0);

			FRDGBufferRef Tmp_FilterResult_NumberCount = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(BytesPerElement, 1), TEXT("FindPixelBuffer"));
			FRDGBufferUAVRef Tmp_FilterResult_NumberCount_UAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Tmp_CountBuffer, EPixelFormat::PF_R32_UINT));
			AddClearUAVPass(GraphBuilder,Tmp_FilterResult_NumberCount_UAV, 0.0);

			PassParameters->T_ObjectDepth = ObjectDepthTexture;
			PassParameters->T_TargetHeight = TargetHeightTexture;
			PassParameters->T_Result = ResultTexture;
			PassParameters->T_SceneDepth = SceneDepthTexture;
			PassParameters->T_SceneNormal = SceneNormalTexture;
			PassParameters->T_CurrentSceneDepth = CurrentSceneDepthTexture;
			PassParameters->T_ObjectNormal = ObjectNormalTexture;
			PassParameters->T_TMeshDepth = MeshHeightTextures[0];
			PassParameters->TA_MeshHeight = TMeshDepthTextureArray;
			PassParameters->RW_SaveRotateScale = TmpTextureUAV_SaveRotateScale;
			PassParameters->RW_CurrentSceneDepth = TmpTextureUAV_CurrentSceneDepth;
			PassParameters->RW_CurrentSceneDepthA = TmpTextureUAV_CurrentSceneDepthA;
			PassParameters->RW_CurrentSceneDepthB = TmpTextureUAV_CurrentSceneDepthB;
			PassParameters->RW_DebugView = TmpTextureUAV_DebugView;
			PassParameters->RW_Result = TmpTextureUAV_Result;
			PassParameters->RW_ResultA = TmpTextureUAV_ResultA;
			PassParameters->RW_ResultB = TmpTextureUAV_ResultB;
			PassParameters->RW_FilterResult = TmpTextureUAV_FilterResult;
			PassParameters->RW_TargetHeight = TmpTextureUAV_TargetHeight;
			PassParameters->RW_Deduplication = TmpTextureUAV_Deduplication;
			PassParameters->RW_FilterResulteMult = TmpTextureUAV_FilterResulteMult;

			PassParameters->RW_FindPixelBuffer = Tmp_CountBufferUAV;
			PassParameters->RW_FindPixelBufferResult_Number = Tmp_FilterResult_Number_UAV;
			PassParameters->RW_FindPixelBufferResult_NumberCount = Tmp_FilterResult_NumberCount_UAV;

			
			PassParameters->SelectIndex = -1;

			PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
			
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("Init"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader_Init, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Init, *PassParameters, GroupCount);
				});
			AddCopyTexturePass(GraphBuilder, TmpTexture_CurrentSceneDepth, CurrentSceneDepthTexture, FRHICopyTextureInfo());

			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExtentNormalMask"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader_ExtentGenerateMask, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_ExtentGenerateMask, *PassParameters, GroupCount);
				});
			AddCopyTexturePass(GraphBuilder, TmpTexture_CurrentSceneDepth, TmpTexture_CurrentSceneDepthA, FRHICopyTextureInfo());
			
			
			for (int32 i = 0; i < GenerateDatas.Num(); i++)
			{
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("MeshFillVerticalRock"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_FVR, GroupCount, i, GenerateDatas, &MeshHeightTextures, this](FRHIComputeCommandList& RHICmdList)
					{
						float MeshSize = MeshDataAssets[GenerateDatas[i].SelectIndex]->CSMeshSize;
						float DrawSize = MeshSize / CaptureSize * SpawnSize ;
						PassParameters->Size = DrawSize;
						PassParameters->RandomRotator = FMath::FRandRange(GenerateDatas[i].RandomRotate.X, GenerateDatas[i].RandomRotate.Y);
						PassParameters->HeightOffset = FMath::FRandRange(GenerateDatas[i].RandomHeightOffset.X, GenerateDatas[i].RandomRotate.Y);
						PassParameters->SelectIndex = GenerateDatas[i].SelectIndex;

						// PassParameters->T_TMeshDepth = MeshHeightTextures[GenerateDatas[i].SelectIndex];
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_FVR, *PassParameters, GroupCount);

					});
				
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("FindBestPiexel"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_FindPixel, ReductionGroupSize](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_FindPixel, *PassParameters, ReductionGroupSize);
					});
				
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("FindBestPiexelRW"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_FindPixelRW, i, TmpTextureUAV_ResultA, TmpTextureUAV_ResultB](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_FindPixelRW, *PassParameters, FIntVector(1, 1, 1));
						if ( i % 2 == 0)
						{
							PassParameters->RW_ResultA = TmpTextureUAV_ResultB;
							PassParameters->RW_ResultB = TmpTextureUAV_ResultA;
						}
						else
						{
							PassParameters->RW_ResultA = TmpTextureUAV_ResultA;
							PassParameters->RW_ResultB = TmpTextureUAV_ResultB;
						}
					});
				
				
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("Update"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_Update, GroupCount, i, TmpTextureUAV_ResultA, TmpTextureUAV_ResultB, TmpTextureUAV_CurrentSceneDepthA, TmpTextureUAV_CurrentSceneDepthB](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Update, *PassParameters, GroupCount);
						if ( i % 2 == 0)
						{
							PassParameters->RW_ResultA = TmpTextureUAV_ResultB;
							PassParameters->RW_ResultB = TmpTextureUAV_ResultA;
							PassParameters->RW_CurrentSceneDepthA = TmpTextureUAV_CurrentSceneDepthB;
							PassParameters->RW_CurrentSceneDepthB = TmpTextureUAV_CurrentSceneDepthA;
						}
						else
						{
							PassParameters->RW_ResultA = TmpTextureUAV_ResultA;
							PassParameters->RW_ResultB = TmpTextureUAV_ResultB;
							PassParameters->RW_CurrentSceneDepthA = TmpTextureUAV_CurrentSceneDepthA;
							PassParameters->RW_CurrentSceneDepthB = TmpTextureUAV_CurrentSceneDepthB;
						}
					});
				// for(int32 n = 0; n < 4; n++)
				// {
				// 	GraphBuilder.AddPass(
				// 		RDG_EVENT_NAME("FilterResultExtent"),
				// 		PassParameters,
				// 		ERDGPassFlags::AsyncCompute,
				// 		[&PassParameters, ComputeShader_FilterResultExtent](FRHIComputeCommandList& RHICmdList)
				// 		{
				// 			FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_FilterResultExtent, *PassParameters, FIntVector(256, 1, 1));
				// 		});
				// }
				//
				//
				// GraphBuilder.AddPass(
				// 	RDG_EVENT_NAME("FilterResultReduce"),
				// 	PassParameters,
				// 	ERDGPassFlags::AsyncCompute,
				// 	[&PassParameters, ComputeShader_FilterResultReduce](FRHIComputeCommandList& RHICmdList)
				// 	{
				// 		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_FilterResultReduce, *PassParameters, FIntVector(256, 1, 1));
				// 	});
				// GraphBuilder.AddPass(
				// 	RDG_EVENT_NAME("Deduplication"),
				// 	PassParameters,
				// 	ERDGPassFlags::AsyncCompute,
				// 	[&PassParameters, ComputeShader_Deduplication](FRHIComputeCommandList& RHICmdList)
				// 	{
				// 		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Deduplication, *PassParameters, FIntVector(256, 1, 1));
				// 	});
				// GraphBuilder.AddPass(
				// 	RDG_EVENT_NAME("FilterResultSelect"),
				// 	PassParameters,
				// 	ERDGPassFlags::AsyncCompute,
				// 	[&PassParameters, ComputeShader_FilterResultSelect, i, TmpTextureUAV_ResultB, TmpTextureUAV_ResultA, TmpTextureUAV_CurrentSceneDepthB, TmpTextureUAV_CurrentSceneDepthA](FRHIComputeCommandList& RHICmdList)
				// 	{
				// 		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_FilterResultSelect, *PassParameters, FIntVector(256, 1, 1));
				// 		if ( i % 2 == 0)
				// 		{
				// 			PassParameters->RW_ResultA = TmpTextureUAV_ResultB;
				// 			PassParameters->RW_ResultB = TmpTextureUAV_ResultA;
				// 			PassParameters->RW_CurrentSceneDepthA = TmpTextureUAV_CurrentSceneDepthB;
				// 			PassParameters->RW_CurrentSceneDepthB = TmpTextureUAV_CurrentSceneDepthA;
				// 		}
				// 		else
				// 		{
				// 			PassParameters->RW_ResultA = TmpTextureUAV_ResultA;
				// 			PassParameters->RW_ResultB = TmpTextureUAV_ResultB;
				// 			PassParameters->RW_CurrentSceneDepthA = TmpTextureUAV_CurrentSceneDepthA;
				// 			PassParameters->RW_CurrentSceneDepthB = TmpTextureUAV_CurrentSceneDepthB;
				// 		}
				// 	});
			}
			if (GenerateDatas.Num() % 2 == 0)
			{
				AddCopyTexturePass(GraphBuilder, TmpTexture_CurrentSceneDepthA, CurrentSceneDepthTexture, FRHICopyTextureInfo());
				// AddCopyTexturePass(GraphBuilder, TmpTexture_ResultA, ResultTexture, FRHICopyTextureInfo());
			}
			else
			{
				AddCopyTexturePass(GraphBuilder, TmpTexture_CurrentSceneDepthB, CurrentSceneDepthTexture, FRHICopyTextureInfo());
				// AddCopyTexturePass(GraphBuilder, TmpTexture_ResultB, ResultTexture, FRHICopyTextureInfo());
			}
			
			
			AddCopyTexturePass(GraphBuilder, TmpTexture_ResultA, ResultTexture, FRHICopyTextureInfo());
			FRDGTextureRef DebugViewTexture = RegisterExternalTexture(GraphBuilder, DebugView->GetRenderTargetTexture(), TEXT("DebugView_RT"));
			AddCopyTexturePass(GraphBuilder, TmpTexture_DebugView, DebugViewTexture, FRHICopyTextureInfo());
			
			//GraphBuilder.QueueBufferUpload(Tmp_CountBuffer, DebugDrawData->StaticLines.GetData(), BufferNumUint32 * sizeof(uint32));
			//GraphBuilder.QueueBufferExtraction(Tmp_CountBuffer, &DebugBuffer);
			
		}
		
		GraphBuilder.Execute();
	});
}
