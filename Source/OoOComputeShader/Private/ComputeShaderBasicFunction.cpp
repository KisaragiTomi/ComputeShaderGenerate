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
	UTextureRenderTarget2D* InConnectivityMap, int32 Channel, UTextureRenderTarget2D* InDebugView)
{
	if (InTextureTarget == nullptr || InConnectivityMap == nullptr || InDebugView == nullptr)
		return;
	InTextureTarget->ResizeTarget(128, 128);
	InConnectivityMap->ResizeTarget(128, 128);
	InDebugView->ResizeTarget(128, 128);
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
		
			bool bIsShaderValid = ComputeShader_Init.IsValid();
		
			if (bIsShaderValid)
			{
				FConnectivityPixel::FParameters* PassParameters = GraphBuilder.AllocParameters<FConnectivityPixel::FParameters>();
				auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(TextureTarget->GetSizeXY().X, TextureTarget->GetSizeXY().Y, 1), FComputeShaderUtils::kGolden2DGroupSize);
				
				FRDGTextureRef TmpTexture_ConnectivityMap = ConvertToUVATexture(ConnectivityMap, GraphBuilder);
				FRDGTextureRef TmpTexture_DebugView = ConvertToUVATexture(DebugView, GraphBuilder);
				FRDGTextureRef TextureTargetTexture = RegisterExternalTexture(GraphBuilder, TextureTarget->GetRenderTargetTexture(), TEXT("Input_RT"));
				
				FRDGTextureRef TmpTexture_LabelBufferA = ConvertToUVATextureFormat(ConnectivityMap, GraphBuilder, PF_A32B32G32R32F);
				FRDGTextureRef TmpTexture_LabelBufferB = ConvertToUVATextureFormat(ConnectivityMap, GraphBuilder, PF_A32B32G32R32F);

				const uint32 NumElements = TextureTarget->GetSizeXY().X * TextureTarget->GetSizeXY().Y;
				const uint32 BytesPerElement = sizeof(uint32);
				FRDGBufferRef Tmp_CountBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(BytesPerElement, NumElements), TEXT("CountBuffer"));
				FRDGBufferUAVRef Tmp_CountBufferUAV = GraphBuilder.CreateUAV(Tmp_CountBuffer);
				AddClearUAVPass(GraphBuilder,Tmp_CountBufferUAV, 0);

				
				PassParameters->InputTexture = TextureTargetTexture;
				PassParameters->RW_ConnectivityPixel = GraphBuilder.CreateUAV(TmpTexture_ConnectivityMap);
				PassParameters->RW_LabelBufferA = GraphBuilder.CreateUAV(TmpTexture_LabelBufferA);
				PassParameters->RW_LabelBufferB = GraphBuilder.CreateUAV(TmpTexture_LabelBufferB);
				PassParameters->RW_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);
				PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
				PassParameters->RW_LabelCounters = Tmp_CountBufferUAV;
				PassParameters->Channel = Channel;
				
				//Init
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("Init"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_Init, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Init, *PassParameters, GroupCount);
					});

				int32 Iteration = TextureTarget->GetSizeXY().X / 2;
				//Iteration = 2;
				for (int32 i = 0; i < Iteration; i++)
				{
					GraphBuilder.AddPass(
					RDG_EVENT_NAME("FindIslands"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_FindIslands, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_FindIslands, *PassParameters, GroupCount);
					});
					AddCopyTexturePass(GraphBuilder, TmpTexture_LabelBufferB, TmpTexture_LabelBufferA, FRHICopyTextureInfo());
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

				//CountBufferUAV->Release();
				FRDGTextureRef ConnectivityMapTexture = RegisterExternalTexture(GraphBuilder, ConnectivityMap->GetRenderTargetTexture(), TEXT("Connectivity_RT"));
				AddCopyTexturePass(GraphBuilder, TmpTexture_ConnectivityMap, ConnectivityMapTexture, FRHICopyTextureInfo());

				FRDGTextureRef DebugViewTexture = RegisterExternalTexture(GraphBuilder, DebugView->GetRenderTargetTexture(), TEXT("Connectivity_RT"));
				AddCopyTexturePass(GraphBuilder, TmpTexture_DebugView, DebugViewTexture, FRHICopyTextureInfo());
			}
		}
		GraphBuilder.Execute();
	});
}

void UComputeShaderBasicFunction::BlurTexture(UTextureRenderTarget2D* InTextureTarget,
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
			PermutationVector.Set<FBlurTexture::FBlurVector4Texture>(true);
			
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
			// for (int32 PermutationId = 0; PermutationId < FBlurTexture::FPermutationDomain::PermutationCount; PermutationId++) {
			// 	FBlurTexture::FPermutationDomain PermutationVector;
			// 	PermutationVector.FromPermutationId(PermutationId);
			//
			// 	// 获取当前变体的参数状态
			// 	bool bUseHorizontal = PermutationVector.Get<FBlurTexture::FUseHorizontalBlur>();
			// 	bool bUseHighQuality = PermutationVector.Get<FBlurTexture::FUseHighQuality>();
			//
			// 	// 调度对应变体的 Shader
			// 	TShaderMapRef<FBlurTexture> Shader(GetGlobalShaderMap(..., PermutationVector));
			// 	DispatchComputeShader(...);
			// }
			typename FBlurTexture::FPermutationDomain PermutationVector;
			PermutationVector.Set<FBlurTexture::FBlurNormalTexture>(true);
			
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

