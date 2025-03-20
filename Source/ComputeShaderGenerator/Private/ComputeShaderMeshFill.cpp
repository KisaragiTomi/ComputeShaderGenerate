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
#include "Engine/Texture2DArray.h"

DECLARE_STATS_GROUP(TEXT("CSMeshFill"), STATGROUP_CSGenerate, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("CS Execute"), STAT_CSGenerate_Execute, STATGROUP_CSGenerate)
DECLARE_CYCLE_STAT(TEXT("CS Capture"), STAT_CSGenerate_Capture, STATGROUP_CSGenerate)
DECLARE_CYCLE_STAT(TEXT("CS Tatal"), STAT_CSGenerate_Tatal, STATGROUP_CSGenerate);



/// <summary>
///// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
/// </summary>
///

using namespace CSHepler;


ACSFillTarget::ACSFillTarget()
{
	
}

void ACSFillTarget::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
}

void ACSFillTarget::FillTargetCal(TArray<FCSMeshFillData> GenerateDatas)
{
	if (!IsParameterValidMult() || GenerateDatas.Num() == 0) return;
	
	InTargetHeight->ResizeTarget(TextureSize, TextureSize);
	int32 ResultSize = GenerateTextureSize(GenerateDatas.Num());
	InResult->ResizeTarget(1024, 2);
	
	FRenderTarget* ObjectNormal = InObjectNormal->GameThread_GetRenderTargetResource();
	FRenderTarget* SceneNormal = InSceneNormal->GameThread_GetRenderTargetResource();
	FRenderTarget* SceneDepth = InSceneDepth->GameThread_GetRenderTargetResource();
	FRenderTarget* DebugView = InDebugView->GameThread_GetRenderTargetResource();
	FRenderTarget* HeightNormal = InHeightNormal->GameThread_GetRenderTargetResource();
	FRenderTarget* Result = InResult->GameThread_GetRenderTargetResource();
	FRenderTarget* CurrentSceneDepth = InCurrentSceneDepth->GameThread_GetRenderTargetResource();
	FRenderTarget* TargetHeight = InTargetHeight->GameThread_GetRenderTargetResource();
	FRenderTarget* ObjectDepth = InObjectDepth->GameThread_GetRenderTargetResource();
	float SizeX = SceneDepth->GetSizeXY().X;
	float SizeY = SceneDepth->GetSizeXY().Y;

	InMeshHeightArray->SourceTextures.Empty();
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
			TShaderMapRef<FMeshFillMult> ComputeShader_InitTargetHeight = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_InitTargetHeight);
			TShaderMapRef<FMeshFillMult> ComputeShader_FVR = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_FillVerticalRock);
			TShaderMapRef<FMeshFillMult> ComputeShader_FindPixel = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_FindBestPixel);
			TShaderMapRef<FMeshFillMult> ComputeShader_FindPixelRW = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_FindBestPixelRW_256);
			TShaderMapRef<FMeshFillMult> ComputeShader_Update = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_UpdateCurrentHeight);
			TShaderMapRef<FMeshFillMult> ComputeShader_ExtentGenerateMask = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_ExtentGenerateMask);
			TShaderMapRef<FMeshFillMult> ComputeShader_FilterResultExtent = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_FilterResultExtent);
			TShaderMapRef<FMeshFillMult> ComputeShader_FilterResultReduce = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_FilterResultReduce);
			TShaderMapRef<FMeshFillMult> ComputeShader_Deduplication = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_Deduplication);
			TShaderMapRef<FMeshFillMult> ComputeShader_UpdateCurrentHeightMult = FMeshFillMult::CreateMeshFillPermutation(FMeshFillMult::EMeshFillFunction::MF_UpdateCurrentHeightMult);
			
			FIntVector MeshCheckGroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(SizeX, SizeY, 1), 16);
			FIntVector GeneralGroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(SizeX, SizeY, 1), 32);
			FIntPoint TextureSizeXY = SceneDepth->GetSizeXY();
			FIntPoint FilterResultSizeXY = FIntPoint(MeshCheckGroupCount.X, MeshCheckGroupCount.Y);
			
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

			
			FRDGTextureRef TmpTexture_TargetHeight = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_FloatRGBA, TEXT("TargetHeight_Texture"));
			FRDGTextureUAVRef TmpTextureUAV_TargetHeight = GraphBuilder.CreateUAV(TmpTexture_TargetHeight);
			
			FRDGTextureRef TmpTexture_FilterResult = ConvertToUVATextureFormat(GraphBuilder, FilterResultSizeXY, PF_A32B32G32R32F, TEXT("FilterResult_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_FilterResult = GraphBuilder.CreateUAV(TmpTexture_FilterResult);

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
			FRDGTextureRef HeightNormalTexture = RegisterExternalTexture(GraphBuilder, HeightNormal->GetRenderTargetTexture(), TEXT("NormalHeight_T"));

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


			PassParameters->T_HeightNormal = HeightNormalTexture;
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
				[&PassParameters, ComputeShader_Init, GeneralGroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Init, *PassParameters, GeneralGroupCount);
				});
			AddCopyTexturePass(GraphBuilder, TmpTexture_CurrentSceneDepth, CurrentSceneDepthTexture, FRHICopyTextureInfo());
			
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExtentNormalMask"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader_ExtentGenerateMask, GeneralGroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_ExtentGenerateMask, *PassParameters, GeneralGroupCount);
				});
			AddCopyTexturePass(GraphBuilder, TmpTexture_CurrentSceneDepth, TmpTexture_CurrentSceneDepthA, FRHICopyTextureInfo());
			
			
			for (int32 i = 0; i < GenerateDatas.Num(); i++)
			{
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("MeshFillVerticalRock"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_FVR, MeshCheckGroupCount, i, GenerateDatas, &MeshHeightTextures, this](FRHIComputeCommandList& RHICmdList)
					{
						float MeshSize = MeshDataAssets[GenerateDatas[i].SelectIndex]->CSMeshSize;
						float DrawSize = MeshSize / CaptureSize * SpawnSize ;
						PassParameters->Size = DrawSize;
						PassParameters->RandomRotator = FMath::FRandRange(GenerateDatas[i].RandomRotate.X, GenerateDatas[i].RandomRotate.Y);
						PassParameters->HeightOffset = FMath::FRandRange(GenerateDatas[i].RandomHeightOffset.X, GenerateDatas[i].RandomRotate.Y);
						PassParameters->SelectIndex = GenerateDatas[i].SelectIndex;

						// PassParameters->T_TMeshDepth = MeshHeightTextures[GenerateDatas[i].SelectIndex];
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_FVR, *PassParameters, MeshCheckGroupCount);
					});
				
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("FindBestPiexelRW"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_FindPixelRW, i, TmpTextureUAV_ResultA, TmpTextureUAV_ResultB, FilterResultSizeXY](FRHIComputeCommandList& RHICmdList)
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
					[&PassParameters, ComputeShader_UpdateCurrentHeightMult, GeneralGroupCount, i, TmpTextureUAV_ResultA, TmpTextureUAV_ResultB, TmpTextureUAV_CurrentSceneDepthA, TmpTextureUAV_CurrentSceneDepthB](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_UpdateCurrentHeightMult, *PassParameters, GeneralGroupCount);
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
			
		}
		
		GraphBuilder.Execute();
	});
}

void ACSFillTarget::FillTarget(int32 NumIteration, float InSpawnSize)
{
	
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

