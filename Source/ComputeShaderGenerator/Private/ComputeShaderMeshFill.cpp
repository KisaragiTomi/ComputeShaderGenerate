#include "ComputeShaderMeshFill.h"

#include "ClearQuad.h"
#include "ComputeShaderGenerateHepler.h"
#include "GlobalShader.h"
#include "MaterialShader.h"

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

DECLARE_STATS_GROUP(TEXT("CSMeshFill"), STATGROUP_CSGenerate, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("CS Execute"), STAT_CSGenerate_Execute, STATGROUP_CSGenerate)
DECLARE_CYCLE_STAT(TEXT("CS Capture"), STAT_CSGenerate_Capture, STATGROUP_CSGenerate)
DECLARE_CYCLE_STAT(TEXT("CS Tatal"), STAT_CSGenerate_Tatal, STATGROUP_CSGenerate);



/// <summary>
///// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
/// </summary>
///

using namespace CSHepler;



void UComputeShaderMeshFillFunctions::CSMeshFill(ACSGenerateCaptureScene* Capturer, UStaticMesh* StaticMesh, UTextureRenderTarget2D* DubugView, UTextureRenderTarget2D* Result, UTexture2D* TMeshDepth, float SpawnSize, float TestSizeScale, FName Tag)
{
	SCOPE_CYCLE_COUNTER(STAT_CSGenerate_Tatal);
	float CapturerSize = Capturer->CaptureSize;
	float MaxHeight = Capturer->MaxHeight;
	float MeshSize = FMath::Max(StaticMesh->GetBounds().BoxExtent.X, StaticMesh->GetBounds().BoxExtent.Y) * 2;
	float DrawSize = MeshSize / CapturerSize * SpawnSize;
	
	{
		SCOPE_CYCLE_COUNTER(STAT_CSGenerate_Capture);
		Capturer->CaptureSceneDepth->CaptureScene();
		Capturer->CaptureObjNormal->CaptureScene();
		FlushRenderingCommands();
	}
	
	FCSGenerateParameter Parameter;
	Parameter.SceneDepth = Capturer->CaptureSceneDepth->TextureTarget;
	Parameter.SceneNormal =Capturer->CaptureObjNormal->TextureTarget;
	Parameter.Result = Result;
	Parameter.DebugView = DubugView;
	Parameter.TMeshDepth = TMeshDepth;
	Parameter.Size = DrawSize * TestSizeScale;
	Parameter.RandomRoation = FMath::FRandRange(0.0, 1.0);

	{
		SCOPE_CYCLE_COUNTER(STAT_CSGenerate_Execute);
		UComputeShaderMeshFillFunctions::CalculateMeshLoctionAndRotation(Parameter);
		FlushRenderingCommands();
	}
	
	TArray<FLinearColor> Colors;
	UKismetRenderingLibrary::ReadRenderTargetRaw(GWorld, Result, Colors);
	if (Colors[0].A == 0)
		return;
	
	FActorSpawnParameters SpawnParameters;
	AStaticMeshActor *SpawnMesh = GWorld->SpawnActor<AStaticMeshActor>(SpawnParameters);
	
	FVector SpawnLocation = FVector(Colors[0].R * CapturerSize, Colors[0].G * CapturerSize, Colors[0].B * MaxHeight) + Capturer->GetActorLocation() - FVector(1, 1, 0) * CapturerSize / 2;
	FRotator SpawnRotation = FRotator(0, Colors[0].A * 360, 0);
	FVector SpawnScale = FVector(1, 1, 1) * SpawnSize;
	FTransform SpawnTransform(SpawnRotation, SpawnLocation, SpawnScale);
	
	SpawnMesh->SetActorTransform(SpawnTransform);
	SpawnMesh->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
	SpawnMesh->Tags = {Tag};
	
	TArray<AActor*> StaticMeshActors;
	for (TActorIterator<AActor> It(GWorld, AStaticMeshActor::StaticClass()); It; ++It)
	{
		AActor* Actor = *It;
		if (IsValid(Actor) && Actor->ActorHasTag(Tag))
		{
			StaticMeshActors.Add(Actor);
		}
	}
	Capturer->CaptureObjNormal->ShowOnlyActors = StaticMeshActors;
}

void UComputeShaderMeshFillFunctions::CSMeshFillMult(ACSGenerateCaptureScene* Capturer, UStaticMesh* StaticMesh,
                                              UTextureRenderTarget2D* DubugView, UTextureRenderTarget2D* Result, UTextureRenderTarget2D* CurrentSceneDepth, UTexture2D* TMeshDepth, int32 Iteration,
                                              float SpawnSize, float TestSizeScale, FName Tag)
{
		SCOPE_CYCLE_COUNTER(STAT_CSGenerate_Tatal);
	float CapturerSize = Capturer->CaptureSize;
	float MaxHeight = Capturer->MaxHeight;
	float MeshSize = FMath::Max(StaticMesh->GetBounds().BoxExtent.X, StaticMesh->GetBounds().BoxExtent.Y) * 2;
	float DrawSize = MeshSize / CapturerSize * SpawnSize;
	
	{
		SCOPE_CYCLE_COUNTER(STAT_CSGenerate_Capture);
		Capturer->CaptureSceneDepth->CaptureScene();
		Capturer->CaptureObjNormal->CaptureScene();
		FlushRenderingCommands();
	}
	
	FCSGenerateParameter Parameter;
	Parameter.SceneDepth = Capturer->CaptureSceneDepth->TextureTarget;
	Parameter.SceneNormal =Capturer->CaptureObjNormal->TextureTarget;
	Parameter.Result = Result;
	Parameter.DebugView = DubugView;
	Parameter.CurrentSceneDepth = CurrentSceneDepth;
	Parameter.TMeshDepth = TMeshDepth;
	Parameter.Size = DrawSize * TestSizeScale;
	Parameter.RandomRoation = FMath::FRandRange(0.0, 1.0);

	{
		SCOPE_CYCLE_COUNTER(STAT_CSGenerate_Execute);
		UComputeShaderMeshFillFunctions::CalculateMeshLoctionAndRotationMult(Parameter, Iteration);
		FlushRenderingCommands();
	}
	TArray<FLinearColor> LinearSamples;
	
	FRenderTarget* RT_Result = Result->GameThread_GetRenderTargetResource();
	FIntRect SampleRect(0, 0, Result->SizeX - 1, Result->SizeY - 1);
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
	float ResultSize = Result->SizeX;
	TArray<FLinearColor> Colors;
	UKismetRenderingLibrary::ReadRenderTargetRaw(GWorld, Result, Colors);
	int32 MaxGenerate = FMath::RoundToInt(Colors[Colors.Num() - 1].R);
	if (MaxGenerate == 0)
		return;

	for (int32 i = 0; i < MaxGenerate; i++)
	{
		FActorSpawnParameters SpawnParameters;
		AStaticMeshActor *SpawnMesh = GWorld->SpawnActor<AStaticMeshActor>(SpawnParameters);
	
		FVector SpawnLocation = FVector(Colors[i].R * CapturerSize, Colors[i].G * CapturerSize, Colors[i].B * MaxHeight) + Capturer->GetActorLocation() - FVector(1, 1, 0) * CapturerSize / 2;
		FRotator SpawnRotation = FRotator(0, Colors[i + Result->SizeX].R * 360, 0);
		FVector SpawnScale = FVector(1, 1, 1) * Colors[i + Result->SizeX].G  / MeshSize * CapturerSize;
		FTransform SpawnTransform(SpawnRotation, SpawnLocation, SpawnScale);
	
		SpawnMesh->SetActorTransform(SpawnTransform);
		SpawnMesh->GetStaticMeshComponent()->SetStaticMesh(StaticMesh);
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
	Capturer->CaptureObjNormal->ShowOnlyActors = StaticMeshActors;
}

void UComputeShaderMeshFillFunctions::CalculateMeshLoctionAndRotation(FCSGenerateParameter Params)
{
	if (!Params.IsValid())
		return;
	Params.SceneNormal->ResizeTarget(512, 512);
	Params.SceneDepth->ResizeTarget(512, 512);
	Params.DebugView->ResizeTarget(512, 512);
	Params.Result->ResizeTarget(2, 1);
	UTexture2D* TMeshDepth = Params.TMeshDepth;
	FRenderTarget* SceneNormal = Params.SceneNormal->GameThread_GetRenderTargetResource();
	FRenderTarget* SceneDepth = Params.SceneDepth->GameThread_GetRenderTargetResource();
	
	FRenderTarget* DebugView = Params.DebugView->GameThread_GetRenderTargetResource();
	FRenderTarget* Result = Params.Result->GameThread_GetRenderTargetResource();

	
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[SceneDepth, TMeshDepth, DebugView, Result, SceneNormal, Params](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			
			DECLARE_GPU_STAT(CSMeshFill)
			RDG_EVENT_SCOPE(GraphBuilder, "CSMeshFill");
			RDG_GPU_STAT_SCOPE(GraphBuilder, CSMeshFill);
			
			
			typename FMeshFill::FPermutationDomain PermutationVector;
		
			TShaderMapRef<FMeshFill> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
		
			bool bIsShaderValid = ComputeShader.IsValid();
		
			if (bIsShaderValid && SceneDepth)
			{
				FMeshFill::FParameters* PassParameters = GraphBuilder.AllocParameters<FMeshFill::FParameters>();
				auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(SceneDepth->GetSizeXY().X, SceneDepth->GetSizeXY().Y, 1), FComputeShaderUtils::kGolden2DGroupSize);
				
				FRDGTextureDesc Desc_DebugView(FRDGTextureDesc::Create2D(SceneDepth->GetSizeXY(), PF_FloatRGBA, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
				FRDGTextureRef TmpTexture_DebugView = GraphBuilder.CreateTexture(Desc_DebugView, TEXT("DebugView_TempTexture"));
				AddClearRenderTargetPass(GraphBuilder, TmpTexture_DebugView, FLinearColor::Black);
				
				FRDGTextureDesc Desc_CurrentSceneDepth(FRDGTextureDesc::Create2D(SceneDepth->GetSizeXY(), PF_FloatRGBA, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
				FRDGTextureRef TmpTexture_CurrentSceneDepth = GraphBuilder.CreateTexture(Desc_CurrentSceneDepth, TEXT("CurrentSceneDepth_TempTexture"));
				AddClearRenderTargetPass(GraphBuilder, TmpTexture_CurrentSceneDepth, FLinearColor::Black);
				
				FRDGTextureDesc Desc_Result(FRDGTextureDesc::Create2D(Result->GetSizeXY(), PF_FloatRGBA, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
				FRDGTextureRef TmpTexture_Result = GraphBuilder.CreateTexture(Desc_Result, TEXT("Result_TempTexture"));
				AddClearRenderTargetPass(GraphBuilder, TmpTexture_Result, FLinearColor(0, 0, 0, 0));
				
				//FRDGTextureRef TmpTexture_SceneNormal = ConvertToUVATexture(SceneNormal, GraphBuilder, FLinearColor::Black);

				FRDGTextureRef CurrentSceneDepth =  RegisterExternalTexture(GraphBuilder, SceneDepth->GetRenderTargetTexture(), TEXT("CurrentSceneDepth_RT"));
				FRDGTextureRef SceneDepthTexture = RegisterExternalTexture(GraphBuilder, SceneDepth->GetRenderTargetTexture(), TEXT("SceneDepth_RT"));
				FRDGTextureRef SceneNormalTexture = RegisterExternalTexture(GraphBuilder, SceneNormal->GetRenderTargetTexture(), TEXT("SceneNormal_RT"));
				FRDGTextureRef TMeshDepthTexture = RegisterExternalTexture(GraphBuilder, TMeshDepth->GetResource()->GetTextureRHI(), TEXT("TMeshDepth_T"));
				
				PassParameters->T_SceneDepth = SceneDepthTexture;
				PassParameters->T_SceneNormal = SceneNormalTexture;
				PassParameters->T_TMeshDepth = TMeshDepthTexture;
				PassParameters->T_CurrentSceneDepth = CurrentSceneDepth;
				PassParameters->RW_CurrentSceneDepth = GraphBuilder.CreateUAV(TmpTexture_CurrentSceneDepth);
				PassParameters->RW_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);
				PassParameters->RW_Result = GraphBuilder.CreateUAV(TmpTexture_Result);
				PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
				PassParameters->Size = Params.Size;
				PassParameters->RandomRotator = Params.RandomRoation;
				
				AddCopyTexturePass(GraphBuilder, SceneDepthTexture, CurrentSceneDepth, FRHICopyTextureInfo());
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("ExecuteExampleComputeShader"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
					});
				
				FRDGTextureRef ResultTexture = RegisterExternalTexture(GraphBuilder, Result->GetRenderTargetTexture(), TEXT("Result_RT"));
				AddCopyTexturePass(GraphBuilder, TmpTexture_Result, ResultTexture, FRHICopyTextureInfo());

				FRDGTextureRef DebugViewTexture = RegisterExternalTexture(GraphBuilder, DebugView->GetRenderTargetTexture(), TEXT("DebugView_RT"));
				AddCopyTexturePass(GraphBuilder, TmpTexture_DebugView, DebugViewTexture, FRHICopyTextureInfo());
				
			}
		}
		GraphBuilder.Execute();
	});
}

void UComputeShaderMeshFillFunctions::CalculateMeshLoctionAndRotationMult(FCSGenerateParameter Params, int32 NumIteraion)
{
	if (!Params.IsValidMult())
		return;
	
	int32 ResultSize = GenerateTextureSize(NumIteraion);
	if (ResultSize == -1)
		return;
	
	Params.SceneNormal->ResizeTarget(512, 512);
	Params.SceneDepth->ResizeTarget(512, 512);
	Params.DebugView->ResizeTarget(512, 512);
	Params.CurrentSceneDepth->ResizeTarget(512, 512);
	Params.Result->ResizeTarget(ResultSize, 2);
	UTexture2D* TMeshDepth = Params.TMeshDepth;
	FRenderTarget* SceneNormal = Params.SceneNormal->GameThread_GetRenderTargetResource();
	FRenderTarget* SceneDepth = Params.SceneDepth->GameThread_GetRenderTargetResource();
	
	FRenderTarget* DebugView = Params.DebugView->GameThread_GetRenderTargetResource();
	FRenderTarget* Result = Params.Result->GameThread_GetRenderTargetResource();
	FRenderTarget* CurrentSceneDepth = Params.CurrentSceneDepth->GameThread_GetRenderTargetResource();
	float SizeX = SceneDepth->GetSizeXY().X;
	float SizeY = SceneDepth->GetSizeXY().Y;
	
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			DECLARE_GPU_STAT(CSMeshFill)
			RDG_EVENT_SCOPE(GraphBuilder, "CSMeshFill");
			RDG_GPU_STAT_SCOPE(GraphBuilder, CSMeshFill);
			
			TShaderMapRef<FMeshFillMult> ComputeShader_General = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_General);
			TShaderMapRef<FMeshFillMult> ComputeShader_Update = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_UpdateCurrentHeight);
			TShaderMapRef<FMeshFillMult> ComputeShader_TargetMap = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_TargetHeight);

			bool bIsShaderValid = ComputeShader_General.IsValid() && ComputeShader_Update.IsValid();
		
			if (bIsShaderValid && SceneDepth != nullptr)
			{
				FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(SizeX, SizeY, 1), FComputeShaderUtils::kGolden2DGroupSize);
				
				FMeshFillMult::FParameters* PassParameters = GraphBuilder.AllocParameters<FMeshFillMult::FParameters>();
				
				FRDGTextureRef TmpTexture_DebugView = ConvertToUVATextureFormat(GraphBuilder, DebugView, PF_FloatRGBA, TEXT("DebugView_Texture")); 
				FRDGTextureUAVRef TmpTextureUAV_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);

				FRDGTextureRef TmpTexture_CurrentSceneDepth = ConvertToUVATextureFormat(GraphBuilder, CurrentSceneDepth, PF_FloatRGBA, TEXT("CurrentSceneDepth_Texture")); 
				FRDGTextureUAVRef TmpTextureUAV_CurrentSceneDepth = GraphBuilder.CreateUAV(TmpTexture_CurrentSceneDepth);
				
				FRDGTextureRef TmpTexture_Result = ConvertToUVATextureFormat(GraphBuilder, Result, PF_FloatRGBA, TEXT("Result_Texture")); 
				FRDGTextureUAVRef TmpTextureUAV_Result = GraphBuilder.CreateUAV(TmpTexture_Result);
				
				FRDGTextureRef CurrentSceneDepthTexture =  RegisterExternalTexture(GraphBuilder, CurrentSceneDepth->GetRenderTargetTexture(), TEXT("CurrentSceneDepth_RT"));
				FRDGTextureRef SceneDepthTexture = RegisterExternalTexture(GraphBuilder, SceneDepth->GetRenderTargetTexture(), TEXT("SceneDepth_RT"));
				FRDGTextureRef SceneNormalTexture = RegisterExternalTexture(GraphBuilder, SceneNormal->GetRenderTargetTexture(), TEXT("SceneNormal_RT"));
				FRDGTextureRef ResultTexture = RegisterExternalTexture(GraphBuilder, Result->GetRenderTargetTexture(), TEXT("Result_RT"));
				FRDGTextureRef TMeshDepthTexture = RegisterExternalTexture(GraphBuilder, TMeshDepth->GetResource()->GetTextureRHI(), TEXT("TMeshDepth_T"));

				PassParameters->T_TMeshDepth = TMeshDepthTexture;
				PassParameters->T_Result = ResultTexture;
				PassParameters->T_SceneDepth = SceneDepthTexture;
				PassParameters->T_SceneNormal = SceneNormalTexture;
				PassParameters->T_CurrentSceneDepth = CurrentSceneDepthTexture;
				PassParameters->T_TargetHeight = CurrentSceneDepthTexture;
				PassParameters->RW_CurrentSceneDepth = TmpTextureUAV_CurrentSceneDepth;
				PassParameters->RW_TargetHeight = TmpTextureUAV_CurrentSceneDepth;
				PassParameters->RW_DebugView = TmpTextureUAV_DebugView;
				PassParameters->RW_Result = TmpTextureUAV_Result;
				PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
				
				AddCopyTexturePass(GraphBuilder, SceneDepthTexture, TmpTexture_CurrentSceneDepth, FRHICopyTextureInfo());
				AddCopyTexturePass(GraphBuilder, SceneDepthTexture, CurrentSceneDepthTexture, FRHICopyTextureInfo());
				AddCopyTexturePass(GraphBuilder, TmpTexture_Result, ResultTexture, FRHICopyTextureInfo());
				for (int32 i = 0; i < NumIteraion; i++)
				{
					// PassParameters->T_TMeshDepth = TMeshDepthTexture;
					// PassParameters->T_TMeshDepth = TMeshDepthTexture;
					PassParameters->Size = Params.Size;
					GraphBuilder.AddPass(
						RDG_EVENT_NAME("CheckFill"),
						PassParameters,
						ERDGPassFlags::AsyncCompute,
						[&PassParameters, ComputeShader_General, GroupCount, i](FRHIComputeCommandList& RHICmdList)
						{
							PassParameters->RandomRotator = FMath::FRandRange(0.0, 1.0);
							FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_General, *PassParameters, GroupCount);
						});
					GraphBuilder.AddPass(
						RDG_EVENT_NAME("CheckFill"),
						PassParameters,
						ERDGPassFlags::AsyncCompute,
						[&PassParameters, ComputeShader_General, GroupCount, i](FRHIComputeCommandList& RHICmdList)
						{
							PassParameters->RandomRotator = FMath::FRandRange(0.0, 1.0);
							FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_General, *PassParameters, GroupCount);
						});
					AddCopyTexturePass(GraphBuilder, TmpTexture_Result, ResultTexture, FRHICopyTextureInfo());
					
					GraphBuilder.AddPass(
						RDG_EVENT_NAME("UpdateCurrentScene"),
						PassParameters,
						ERDGPassFlags::AsyncCompute,
						[&PassParameters, ComputeShader_Update, GroupCount, i](FRHIComputeCommandList& RHICmdList)
						{
							
							FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Update, *PassParameters, GroupCount);
						});
					
					AddCopyTexturePass(GraphBuilder, TmpTexture_CurrentSceneDepth, CurrentSceneDepthTexture, FRHICopyTextureInfo());
					AddCopyTexturePass(GraphBuilder, TmpTexture_Result, ResultTexture, FRHICopyTextureInfo());
				}
				
				FRDGTextureRef DebugViewTexture = RegisterExternalTexture(GraphBuilder, DebugView->GetRenderTargetTexture(), TEXT("DebugView_RT"));
				AddCopyTexturePass(GraphBuilder, TmpTexture_DebugView, DebugViewTexture, FRHICopyTextureInfo());
			}
		}
		GraphBuilder.Execute();
	});
}

