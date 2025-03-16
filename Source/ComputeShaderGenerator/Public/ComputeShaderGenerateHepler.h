#pragma once

#include "GlobalShader.h"
#include "MaterialShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"

namespace CSHepler
{
	FRDGTextureRef ConvertToUVATexture(FRenderTarget* RenderTarget, FRDGBuilder& GraphBuilder, FLinearColor ClearColor = FLinearColor(0 ,0 ,0, 0), const TCHAR* Name = TEXT("TempTexture") )
	{
		FRDGTextureDesc Desc_View(FRDGTextureDesc::Create2D(RenderTarget->GetSizeXY(), PF_FloatRGBA, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
		FRDGTextureRef TmpTexture = GraphBuilder.CreateTexture(Desc_View, Name);
		AddClearRenderTargetPass(GraphBuilder, TmpTexture, ClearColor);
		return TmpTexture;
	}

	FRDGTextureRef ConvertToUVATextureFormat(FRDGBuilder& GraphBuilder, FRenderTarget* RenderTarget,  EPixelFormat Format = PF_FloatRGBA, const TCHAR* Name = TEXT("TempTexture"), FLinearColor ClearColor = FLinearColor(0 ,0 ,0, 0) )
	{
		FRDGTextureDesc Desc_View(FRDGTextureDesc::Create2D(RenderTarget->GetSizeXY(), Format, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
		FRDGTextureRef TmpTexture = GraphBuilder.CreateTexture(Desc_View, Name);
		AddClearRenderTargetPass(GraphBuilder, TmpTexture, ClearColor);
		return TmpTexture;
	}

	FRDGTextureRef ConvertToUVATextureFormat(FRDGBuilder& GraphBuilder, FIntPoint Size,  EPixelFormat Format = PF_FloatRGBA, const TCHAR* Name = TEXT("TempTexture"), FLinearColor ClearColor = FLinearColor(0 ,0 ,0, 0) )
	{
		FRDGTextureDesc Desc_View(FRDGTextureDesc::Create2D(Size, Format, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
		FRDGTextureRef TmpTexture = GraphBuilder.CreateTexture(Desc_View, Name);
		AddClearRenderTargetPass(GraphBuilder, TmpTexture, ClearColor);
		return TmpTexture;
	}
	
	int32 GenerateTextureSize(int32 Iteration)
	{
		for (int i = 0; i < 12; i ++)
		{
			if (FMath::Pow(2.0, i) - 2 > Iteration)
				return FMath::Pow(2.0, i);
		}
		return -1;
	}

	FUnorderedAccessViewRHIRef CreateRWBuffer(FRHICommandListImmediate& RHICmdList, uint32 NumElements, uint32 BytesPerElement, EPixelFormat Format = PF_A16B16G16R16)
	{
		// const uint32 TotalBytes = NumElements * BytesPerElement;
		// // 创建结构化缓冲区
		// FRHIResourceCreateInfo CreateSizeInfo(TEXT("SizeBuffer"));
		// FBufferRHIRef StructuredBuffer = RHICreateStructuredBuffer(
		// 	BytesPerElement,            // 每个元素的大小
		// 	TotalBytes,                 // 总字节数
		// 	BUF_Static | BUF_ShaderResource,         // 允许着色器访问
		// 	CreateSizeInfo
		// );
		//
		// // 创建Shader Resource View (SRV)
		// FShaderResourceViewRHIRef ShaderResourceView = RHICreateShaderResourceView(StructuredBuffer);
		// 		
		// const int32 BufferSize = NumElements;
		// FRHIResourceCreateInfo CreateInfo(TEXT("CountBuffer"));
		// FBufferRHIRef CountBufferRHI = RHICmdList.CreateVertexBuffer(
		// 	BufferSize,
		// 	BUF_UnorderedAccess | BUF_KeepCPUAccessible,
		// 	CreateInfo);
		// FUnorderedAccessViewRHIRef BufferUAV = RHICmdList.CreateUnorderedAccessView(
		// 	CountBufferRHI,
		// 	Format );
		// RHICmdList.ClearUAVUint(BufferUAV, FUintVector4(0));
		return nullptr;
	}


				
}
