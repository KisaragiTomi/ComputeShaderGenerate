#include "ComputeShaderBasicFunction.h"

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
#include "Engine/Texture2DArray.h"

DECLARE_STATS_GROUP(TEXT("TestTime"), STATGROUP_CSTest, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("CS Execute"), STAT_CSTest_Execute, STATGROUP_CSTest)
using namespace CSHepler;

void UComputeShaderBasicFunction::DrawLinearColorsToRenderTarget(UTextureRenderTarget2D* InTextureTarget,
	TArray<FLinearColor> Colors)
{
	
	if (Colors.Num() > InTextureTarget->SizeX * InTextureTarget->SizeY)
		return;
	
	TArray<FLinearColor> TmpColors;
	TmpColors.Init(FLinearColor::Black, InTextureTarget->SizeX * InTextureTarget->SizeY);
	
	FMemory::Memcpy(TmpColors.GetData(), Colors.GetData(), Colors.Num() * sizeof(FLinearColor));
	FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[TextureTarget, TmpColors](FRHICommandListImmediate& RHICmdList)
	{
		FTexture2DRHIRef TextureRHI = TextureTarget->GetRenderTargetTexture();
		uint32 DestStride;
		void* DestData = RHILockTexture2D(TextureRHI, 0, RLM_WriteOnly, DestStride, false);
		 if (!DestStride)
		 	return;
		
		FMemory::Memcpy(DestData, TmpColors.GetData(), TextureTarget->GetSizeXY().X * TextureTarget->GetSizeXY().Y * sizeof(FLinearColor));
		RHIUnlockTexture2D(TextureRHI, 0 ,false);
	});
}

void UComputeShaderBasicFunction::ConnectivityPixel(UTextureRenderTarget2D* InTextureTarget,
                                                    UTextureRenderTarget2D* InConnectivityMap, UTextureRenderTarget2D* InDebugView, int32 Channel)
{
	if (InTextureTarget == nullptr || InConnectivityMap == nullptr || InDebugView == nullptr)
		return;

	int32 TextureSize = 256;
	InTextureTarget->ResizeTarget(TextureSize, TextureSize);
	InConnectivityMap->ResizeTarget(TextureSize, TextureSize);
	InDebugView->ResizeTarget(TextureSize, TextureSize);
	FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
	FRenderTarget* ConnectivityMap = InConnectivityMap->GameThread_GetRenderTargetResource();
	FRenderTarget* DebugView = InDebugView->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			
			typename FConnectivityPixel::FPermutationDomain PermutationVector_Init;
			PermutationVector_Init.Set<FConnectivityPixel::FConnectivityPiexlStep>(FConnectivityPixel::EConnectivityStep::CP_Init);
			TShaderMapRef<FConnectivityPixel> ComputeShader_Init(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector_Init);
			typename FConnectivityPixel::FPermutationDomain PermutationVector_FindIslands;
			PermutationVector_FindIslands.Set<FConnectivityPixel::FConnectivityPiexlStep>(FConnectivityPixel::EConnectivityStep::CP_FindIslands);
			TShaderMapRef<FConnectivityPixel> ComputeShader_FindIslands(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector_FindIslands);
			typename FConnectivityPixel::FPermutationDomain PermutationVector_Count;
			PermutationVector_Count.Set<FConnectivityPixel::FConnectivityPiexlStep>(FConnectivityPixel::EConnectivityStep::CP_Count);
			TShaderMapRef<FConnectivityPixel> ComputeShader_Count(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector_Count);
			typename FConnectivityPixel::FPermutationDomain PermutationVector_DrawTexture;
			PermutationVector_DrawTexture.Set<FConnectivityPixel::FConnectivityPiexlStep>(FConnectivityPixel::EConnectivityStep::CP_DrawTexture);
			TShaderMapRef<FConnectivityPixel> ComputeShader_DrawTexture(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector_DrawTexture);

			FConnectivityPixel::FParameters* PassParameters = GraphBuilder.AllocParameters<FConnectivityPixel::FParameters>();
			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(TextureTarget->GetSizeXY().X, TextureTarget->GetSizeXY().Y, 1), FComputeShaderUtils::kGolden2DGroupSize);
			
			FRDGTextureRef TmpTexture_ConnectivityMap = ConvertToUVATexture(ConnectivityMap, GraphBuilder);
			FRDGTextureRef TmpTexture_DebugView = ConvertToUVATextureFormat(GraphBuilder,DebugView);
			FRDGTextureRef TextureTargetTexture = RegisterExternalTexture(GraphBuilder, TextureTarget->GetRenderTargetTexture(), TEXT("Input_RT"));
			
			FRDGTextureRef TmpTexture_LabelBufferA = ConvertToUVATextureFormat(GraphBuilder, ConnectivityMap, PF_A32B32G32R32F, TEXT("LabelBufferA"));
			FRDGTextureUAVRef TmpTextureUAV_LabelBufferA = GraphBuilder.CreateUAV(TmpTexture_LabelBufferA);
			FRDGTextureRef TmpTexture_LabelBufferB = ConvertToUVATextureFormat(GraphBuilder, ConnectivityMap, PF_A32B32G32R32F, TEXT("LabelBufferB"));
			FRDGTextureUAVRef TmpTextureUAV_LabelBufferB = GraphBuilder.CreateUAV(TmpTexture_LabelBufferB);

			const uint32 NumElements = TextureTarget->GetSizeXY().X * TextureTarget->GetSizeXY().Y;
			const uint32 BytesPerElement = sizeof(uint32);
			FRDGBufferRef Tmp_CountBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(BytesPerElement, NumElements), TEXT("CountBuffer"));
			FRDGBufferUAVRef Tmp_CountBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Tmp_CountBuffer, EPixelFormat::PF_R32_UINT));
			AddClearUAVPass(GraphBuilder,Tmp_CountBufferUAV, 0);
			// FRHIBuffer* brhi = Tmp_CountBuffer->GetRHI();
			// brhi->GetType();
			// GraphBuilder.QueueBufferUpload(Tmp_CountBuffer, test.GetData(), 4);
			
			
			PassParameters->InputTexture = TextureTargetTexture;
			PassParameters->RW_ConnectivityPixel = GraphBuilder.CreateUAV(TmpTexture_ConnectivityMap);
			PassParameters->RW_LabelBufferA = TmpTextureUAV_LabelBufferA;
			PassParameters->RW_LabelBufferB = TmpTextureUAV_LabelBufferB;
			PassParameters->RW_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);
			PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
			PassParameters->RW_LabelCounters = Tmp_CountBufferUAV;
			PassParameters->Channel = Channel;
			PassParameters->PieceNum = -1;
			
			//Init
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("Init"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader_Init, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Init, *PassParameters, GroupCount);
				});
			
			// int32 Iteration = GroupCount.X * 2 ;
			int32 Iteration = TextureTarget->GetSizeXY().X / 16 ;
			for (int32 i = 0; i < Iteration; i++)
			{
				GraphBuilder.AddPass(
				RDG_EVENT_NAME("FindIslands"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[i, PassParameters, ComputeShader_FindIslands, GroupCount, TmpTextureUAV_LabelBufferB, TmpTextureUAV_LabelBufferA](FRHIComputeCommandList& RHICmdList)
				{

					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_FindIslands, *PassParameters, GroupCount);
					if ( i % 2 == 0)
					{
						PassParameters->RW_LabelBufferA = TmpTextureUAV_LabelBufferB;
						PassParameters->RW_LabelBufferB = TmpTextureUAV_LabelBufferA;
					}
					else
					{
						PassParameters->RW_LabelBufferA = TmpTextureUAV_LabelBufferA;
						PassParameters->RW_LabelBufferB = TmpTextureUAV_LabelBufferB;
					}

				});
			}
			
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("Count"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader_Count, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Count, *PassParameters, GroupCount);
				});
			
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("DrawTexture"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader_DrawTexture, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_DrawTexture, *PassParameters, GroupCount);
				});
			
			FRDGTextureRef ConnectivityMapTexture = RegisterExternalTexture(GraphBuilder, ConnectivityMap->GetRenderTargetTexture(), TEXT("Connectivity_RT"));
			AddCopyTexturePass(GraphBuilder, TmpTexture_ConnectivityMap, ConnectivityMapTexture, FRHICopyTextureInfo());
			
			FRDGTextureRef DebugViewTexture = RegisterExternalTexture(GraphBuilder, DebugView->GetRenderTargetTexture(), TEXT("DebugView_RT"));
			AddCopyTexturePass(GraphBuilder, TmpTexture_DebugView, DebugViewTexture, FRHICopyTextureInfo());

			// TRefCountPtr<FRDGPooledBuffer> test = GraphBuilder.ConvertToExternalBuffer(Tmp_CountBuffer);
			// FRHIBuffer* Buffer = test->GetRHI();
			// void* DestData = RHILockBuffer(Buffer, )
			
		}
		GraphBuilder.Execute();
	});
}

void UComputeShaderBasicFunction::BlurTexture(UTextureRenderTarget2D* InTextureTarget,
	UTextureRenderTarget2D* OutBlurTexture, float BlurScale)
{
	if (InTextureTarget == nullptr || OutBlurTexture == nullptr)	return;

	SCOPE_CYCLE_COUNTER(STAT_CSTest_Execute);
	FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
	FRenderTarget* BlurTexture = OutBlurTexture->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[TextureTarget, BlurTexture, BlurScale](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			
			typename FBlurTexture::FPermutationDomain PermutationVector;
			PermutationVector.Set<FBlurTexture::FBlurFunctionSet>(FBlurTexture::EBlurType::BT_BLUR3X3);
			TShaderMapRef<FBlurTexture> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
			
			bool bIsShaderValid = ComputeShader.IsValid();
		
			if (bIsShaderValid)
			{
				FBlurTexture::FParameters* PassParameters = GraphBuilder.AllocParameters<FBlurTexture::FParameters>();
				auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(TextureTarget->GetSizeXY().X, TextureTarget->GetSizeXY().Y, 1), FComputeShaderUtils::kGolden2DGroupSize);
				
				FRDGTextureRef TmpTexture_BlurTexture = ConvertToUVATexture(BlurTexture, GraphBuilder);
				FRDGTextureRef TextureTargetTexture = RegisterExternalTexture(GraphBuilder, TextureTarget->GetRenderTargetTexture(), TEXT("Input_RT"));
				FRDGTextureRef BlurTextureTexture = RegisterExternalTexture(GraphBuilder, BlurTexture->GetRenderTargetTexture(), TEXT("Blur_RT"));
				
				PassParameters->T_BlurTexture = TextureTargetTexture;
				PassParameters->RW_BlurTexture = GraphBuilder.CreateUAV(TmpTexture_BlurTexture);
				PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
				PassParameters->BlurScale = BlurScale;
				

				GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteExampleComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});
				
				
				
				AddCopyTexturePass(GraphBuilder, TmpTexture_BlurTexture, BlurTextureTexture, FRHICopyTextureInfo());
			}
		}
		GraphBuilder.Execute();
	});
	FlushRenderingCommands();
}

void UComputeShaderBasicFunction::BlurTextureRDG(FRDGBuilder& GraphBuilder, FRDGTextureRef& InTexture, FRDGTextureUAVRef& InTextureUAV, FRDGTextureRef& OutTexture, FIntVector GroupCount, FBlurTexture::EBlurType Type,float BlurScale)
{
	typename FBlurTexture::FPermutationDomain PermutationVector;
	PermutationVector.Set<FBlurTexture::FBlurFunctionSet>(FBlurTexture::EBlurType::BT_BLUR15X15);
	TShaderMapRef<FBlurTexture> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

	FBlurTexture::FParameters* PassParameters = GraphBuilder.AllocParameters<FBlurTexture::FParameters>();
			
	PassParameters->T_BlurTexture = OutTexture;
	PassParameters->RW_BlurTexture = InTextureUAV ;
	PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
	PassParameters->BlurScale = BlurScale;
			

	GraphBuilder.AddPass(
	RDG_EVENT_NAME("Blur"),
	PassParameters,
	ERDGPassFlags::AsyncCompute,
	[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
	{
		FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
	});
	AddCopyTexturePass(GraphBuilder,InTexture , OutTexture, FRHICopyTextureInfo());
}

void UComputeShaderBasicFunction::BlurNormalTexture(UTextureRenderTarget2D* InTextureTarget,
                                                    UTextureRenderTarget2D* OutBlurTexture, float BlurScale)
{
			if (InTextureTarget == nullptr || OutBlurTexture == nullptr)
		return;

	FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
	FRenderTarget* BlurTexture = OutBlurTexture->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[TextureTarget, BlurTexture, BlurScale](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			typename FBlurTexture::FPermutationDomain PermutationVector;
			PermutationVector.Set<FBlurTexture::FBlurFunctionSet>(FBlurTexture::EBlurType::BT_BLURNORMAL3X3);
			TShaderMapRef<FBlurTexture> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
			
			bool bIsShaderValid = ComputeShader.IsValid();
		
			if (bIsShaderValid)
			{
				FBlurTexture::FParameters* PassParameters = GraphBuilder.AllocParameters<FBlurTexture::FParameters>();
				auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(TextureTarget->GetSizeXY().X, TextureTarget->GetSizeXY().Y, 1), FComputeShaderUtils::kGolden2DGroupSize);
				
				FRDGTextureRef TmpTexture_BlurTexture = ConvertToUVATexture(BlurTexture, GraphBuilder);
				FRDGTextureRef TextureTargetTexture = RegisterExternalTexture(GraphBuilder, TextureTarget->GetRenderTargetTexture(), TEXT("Input_RT"));
				
				PassParameters->T_BlurTexture = TextureTargetTexture;
				PassParameters->RW_BlurTexture = GraphBuilder.CreateUAV(TmpTexture_BlurTexture);
				PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
				PassParameters->BlurScale = BlurScale;
				
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("ExecuteExampleComputeShader"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
					});
				
				FRDGTextureRef BlurTextureTexture = RegisterExternalTexture(GraphBuilder, BlurTexture->GetRenderTargetTexture(), TEXT("Connectivity_RT"));
				AddCopyTexturePass(GraphBuilder, TmpTexture_BlurTexture, BlurTextureTexture, FRHICopyTextureInfo());
			}
		}
		GraphBuilder.Execute();

	});
}

void UComputeShaderBasicFunction::UpPixelsMask(UTextureRenderTarget2D* InTextureTarget,
                                                UTextureRenderTarget2D* OutUpTexture, float Threshould, int32 Channel)
{
	if (InTextureTarget == nullptr || OutUpTexture == nullptr)
		return;

	FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
	FRenderTarget* UpTexture = OutUpTexture->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			
			typename FUpPixelsMask::FPermutationDomain PermutationVector;
			TShaderMapRef<FUpPixelsMask> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
			
			bool bIsShaderValid = ComputeShader.IsValid();
		
			if (bIsShaderValid)
			{
				FUpPixelsMask::FParameters* PassParameters = GraphBuilder.AllocParameters<FUpPixelsMask::FParameters>();
				auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(TextureTarget->GetSizeXY().X, TextureTarget->GetSizeXY().Y, 1), FComputeShaderUtils::kGolden2DGroupSize);
				
				FRDGTextureRef TmpTexture_UpTexture = ConvertToUVATexture(UpTexture, GraphBuilder);
				FRDGTextureRef TextureTargetTexture = RegisterExternalTexture(GraphBuilder, TextureTarget->GetRenderTargetTexture(), TEXT("Input_RT"));
				FRDGTextureRef UpTextureTexture = RegisterExternalTexture(GraphBuilder, UpTexture->GetRenderTargetTexture(), TEXT("Connectivity_RT"));
				
				PassParameters->T_UpPixel = TextureTargetTexture;
				PassParameters->RW_UpPixel = GraphBuilder.CreateUAV(TmpTexture_UpTexture);
				PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
				PassParameters->UpPixelThreshold = Threshould;
				PassParameters->Channel = Channel;

				AddCopyTexturePass(GraphBuilder, UpTextureTexture, TmpTexture_UpTexture, FRHICopyTextureInfo());
				
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("ExecuteExampleComputeShader"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
					});
				
				AddCopyTexturePass(GraphBuilder, TmpTexture_UpTexture, UpTextureTexture, FRHICopyTextureInfo());
			}
		}
		GraphBuilder.Execute();

	});
}

void UComputeShaderBasicFunction::DrawTextureOut(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* OutTextureTarget)
{
	if (InTextureTarget == nullptr || OutTextureTarget == nullptr) return;
		
	
	
	FRenderTarget* TextureTargetIn = InTextureTarget->GameThread_GetRenderTargetResource();
	FRenderTarget* TextureTargetOut = OutTextureTarget->GameThread_GetRenderTargetResource();

	if (TextureTargetIn->GetSizeXY() != TextureTargetOut->GetSizeXY()) return;
	
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			FRDGTextureRef TextureTargetInTexture = RegisterExternalTexture(GraphBuilder, TextureTargetIn->GetRenderTargetTexture(), TEXT("Input_RT"));
			FRDGTextureRef TextureTextureOutTexture = RegisterExternalTexture(GraphBuilder, TextureTargetOut->GetRenderTargetTexture(), TEXT("Output_RT"));
			
			AddCopyTexturePass(GraphBuilder, TextureTargetInTexture, TextureTextureOutTexture, FRHICopyTextureInfo());
		
		}
		GraphBuilder.Execute();

	});
}

void UComputeShaderBasicFunction::Test(UTexture2DArray* InArray, UTexture2D* InTexture, UTextureRenderTarget2D* InDebugView)
{
	UTexture2DArray* TestArray = NewObject<UTexture2DArray>();
	InArray->SourceTextures.Empty();
	TestArray->SourceTextures.Add(InTexture);
	TestArray->UpdateSourceFromSourceTextures();
		
	FRenderTarget* DebugView = InDebugView->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			TShaderMapRef<FGeneralTempShader> ComputeShader = FGeneralTempShader::CreateTempShaderPermutation(FGeneralTempShader::ETempShader::GTS_TextureArrayTest);

			FGeneralTempShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FGeneralTempShader::FParameters>();
			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(TestArray->GetSizeX(), TestArray->GetSizeY(), 1), FComputeShaderUtils::kGolden2DGroupSize);
			
			FRDGTextureRef TmpTexture_DebugView = CSHepler::ConvertToUVATexture(DebugView, GraphBuilder);
			FRDGTextureRef DebugViewTexture = RegisterExternalTexture(GraphBuilder, DebugView->GetRenderTargetTexture(), TEXT("DebugView_RT"));
			FRDGTextureRef TextureArray = RegisterExternalTexture(GraphBuilder, TestArray->GetResource()->GetTextureRHI(), TEXT("Input_TA"));
			PassParameters->TA_ProcssTexture = TextureArray;
			PassParameters->RW_ProcssTexture = GraphBuilder.CreateUAV(TmpTexture_DebugView);
			// PassParameters->InputData = 1;
			PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();

			
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteExampleComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});
			
			AddCopyTexturePass(GraphBuilder, TmpTexture_DebugView, DebugViewTexture, FRHICopyTextureInfo());
		}
		GraphBuilder.Execute();

	});
	
}

