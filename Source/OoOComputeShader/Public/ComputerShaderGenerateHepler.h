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
	FRDGTextureRef ConvertToUVATexture(FRenderTarget* RenderTarget, FRDGBuilder& GraphBuilder, FLinearColor ClearColor = FLinearColor::Black, const TCHAR* Name = TEXT("TempTexture") )
	{
		FRDGTextureDesc Desc_View(FRDGTextureDesc::Create2D(RenderTarget->GetSizeXY(), PF_FloatRGBA, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
		FRDGTextureRef TmpTexture = GraphBuilder.CreateTexture(Desc_View, Name);
		AddClearRenderTargetPass(GraphBuilder, TmpTexture, ClearColor);
		return TmpTexture;
	}

	FRDGTextureRef ConvertToUVATextureFormat(FRenderTarget* RenderTarget, FRDGBuilder& GraphBuilder, EPixelFormat Format = PF_FloatRGBA, FLinearColor ClearColor = FLinearColor::Black, const TCHAR* Name = TEXT("TempTexture") )
	{
		FRDGTextureDesc Desc_View(FRDGTextureDesc::Create2D(RenderTarget->GetSizeXY(), PF_FloatRGBA, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
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
	
}
