#include "ComputerShaderBasicFunction.h"

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


using namespace CSHepler;

void UComputerShaderBasicFunction::DrawLinearColorsToRenderTarget(UTextureRenderTarget2D* InTextureTarget,
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

void UComputerShaderBasicFunction::ConnectivityPixel(UTextureRenderTarget2D* InTextureTarget,
	UTextureRenderTarget2D* InConnectivityMap)
{
	if (InTextureTarget == nullptr || InConnectivityMap == nullptr)
		return;

	FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
	FRenderTarget* ConnectivityMap = InConnectivityMap->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[TextureTarget, ConnectivityMap](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			
			typename FConnectivityPixel::FPermutationDomain PermutationVector_Init(0);
			TShaderMapRef<FConnectivityPixel> ComputeShader_Init(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector_Init);
			typename FConnectivityPixel::FPermutationDomain PermutationVector_FindIslands(1);
			TShaderMapRef<FConnectivityPixel> ComputeShader_FindIslands(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector_FindIslands);
			typename FConnectivityPixel::FPermutationDomain PermutationVector_Count(2);
			TShaderMapRef<FConnectivityPixel> ComputeShader_Count(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector_Count);
			typename FConnectivityPixel::FPermutationDomain PermutationVector_DrawTexture(3);
			TShaderMapRef<FConnectivityPixel> ComputeShader_DrawTexture(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector_DrawTexture);
		
			bool bIsShaderValid = ComputeShader_Init.IsValid();
		
			if (bIsShaderValid)
			{
				FConnectivityPixel::FParameters* PassParameters = GraphBuilder.AllocParameters<FConnectivityPixel::FParameters>();
				auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(TextureTarget->GetSizeXY().X, TextureTarget->GetSizeXY().Y, 1), FComputeShaderUtils::kGolden2DGroupSize);
				
				FRDGTextureRef TmpTexture_ConnectivityMap = ConvertToUVATexture(ConnectivityMap, GraphBuilder);
				FRDGTextureRef TextureTargetTexture = RegisterExternalTexture(GraphBuilder, TextureTarget->GetRenderTargetTexture(), TEXT("Input_RT"));
				
				FRDGTextureRef TmpTexture_LabelBufferA = ConvertToUVATextureFormat(ConnectivityMap, GraphBuilder, PF_R32_UINT);
				FRDGTextureRef TmpTexture_LabelBufferB = ConvertToUVATextureFormat(ConnectivityMap, GraphBuilder, PF_R32_UINT);


				const uint32 NumElements = TextureTarget->GetSizeXY().X * TextureTarget->GetSizeXY().Y;
				const uint32 BytesPerElement = sizeof(uint32);
				const uint32 TotalBytes = NumElements * BytesPerElement;
				// 创建结构化缓冲区
				FRHIResourceCreateInfo CreateSizeInfo(TEXT("SizeBuffer"));
				FBufferRHIRef StructuredBuffer = RHICreateStructuredBuffer(
					BytesPerElement,            // 每个元素的大小
					TotalBytes,                 // 总字节数
					BUF_Static | BUF_ShaderResource,         // 允许着色器访问
					CreateSizeInfo
				);
		
				// 创建Shader Resource View (SRV)
				FShaderResourceViewRHIRef ShaderResourceView = RHICreateShaderResourceView(StructuredBuffer);
				
				const int32 BufferSize = NumElements;
				FRHIResourceCreateInfo CreateInfo(TEXT("CountBuffer"));
				FBufferRHIRef CountBufferRHI = RHICmdList.CreateVertexBuffer(
					BufferSize,
					BUF_UnorderedAccess | BUF_KeepCPUAccessible,
					CreateInfo);
				FUnorderedAccessViewRHIRef CountBufferUAV = RHICmdList.CreateUnorderedAccessView(
					CountBufferRHI,
					PF_R32_UINT );
				RHICmdList.ClearUAVUint(CountBufferUAV, FUintVector4(0));
				
				PassParameters->InputTexture = TextureTargetTexture;
				PassParameters->RW_ConnectivityPixel = GraphBuilder.CreateUAV(TmpTexture_ConnectivityMap);
				PassParameters->RW_LabelBufferA = GraphBuilder.CreateUAV(TmpTexture_LabelBufferA);
				PassParameters->RW_LabelBufferB = GraphBuilder.CreateUAV(TmpTexture_LabelBufferB);
				PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
				PassParameters->RW_LabelCounters = CountBufferUAV;
				PassParameters->Step = 0;

				//Init
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("ExecuteExampleComputeShader"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_Init, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Init, *PassParameters, GroupCount);
					});

				//PassParameters->Step = 1;
				for (int32 i = 0; i < TextureTarget->GetSizeXY().X / 2; i++)
				{
					GraphBuilder.AddPass(
					RDG_EVENT_NAME("ExecuteExampleComputeShader"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_FindIslands, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_FindIslands, *PassParameters, GroupCount);
					});
					AddCopyTexturePass(GraphBuilder, TmpTexture_LabelBufferB, TmpTexture_LabelBufferA, FRHICopyTextureInfo());
				}
				
				//PassParameters->Step = 2;

				GraphBuilder.AddPass(
					RDG_EVENT_NAME("ExecuteExampleComputeShader"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_Count, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Count, *PassParameters, GroupCount);
					});
				//PassParameters->Step = 3;
				
				GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteExampleComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader_DrawTexture, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_DrawTexture, *PassParameters, GroupCount);
				});

				CountBufferUAV->Release();
				FRDGTextureRef ConnectivityMapTexture = RegisterExternalTexture(GraphBuilder, ConnectivityMap->GetRenderTargetTexture(), TEXT("Connectivity_RT"));
				AddCopyTexturePass(GraphBuilder, TmpTexture_ConnectivityMap, ConnectivityMapTexture, FRHICopyTextureInfo());
			}
		}
		GraphBuilder.Execute();

	});
}

void UComputerShaderBasicFunction::BlurTexture(UTextureRenderTarget2D* InTextureTarget,
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
			
			typename FBlurTexture::FPermutationDomain PermutationVector(0);
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

void UComputerShaderBasicFunction::BlurNormalTexture(UTextureRenderTarget2D* InTextureTarget,
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
			
			typename FBlurTexture::FPermutationDomain PermutationVector(1);
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

void UComputerShaderBasicFunction::UpPixelsMask(UTextureRenderTarget2D* InTextureTarget,
                                                UTextureRenderTarget2D* OutUpTexture, float Threshould)
{
			if (InTextureTarget == nullptr || OutUpTexture == nullptr)
		return;

	FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
	FRenderTarget* UpTexture = OutUpTexture->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[TextureTarget, UpTexture, Threshould](FRHICommandListImmediate& RHICmdList)
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
				
				PassParameters->T_UpPixel = TextureTargetTexture;
				PassParameters->RW_UpPixel = GraphBuilder.CreateUAV(TmpTexture_UpTexture);
				PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
				PassParameters->UpPixelThreshold = Threshould;
				
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("ExecuteExampleComputeShader"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
					});
				
				FRDGTextureRef UpTextureTexture = RegisterExternalTexture(GraphBuilder, UpTexture->GetRenderTargetTexture(), TEXT("Connectivity_RT"));
				AddCopyTexturePass(GraphBuilder, TmpTexture_UpTexture, UpTextureTexture, FRHICopyTextureInfo());
			}
		}
		GraphBuilder.Execute();

	});
}

