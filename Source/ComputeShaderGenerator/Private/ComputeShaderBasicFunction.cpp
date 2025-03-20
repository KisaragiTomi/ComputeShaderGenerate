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
#include "EngineModule.h"
#include "LandscapeExtra.h"
#include "../../../../../../../../UnrealEngine-5.4.4-release/Engine/Plugins/Runtime/nDisplay/Source/DisplayClusterConfiguration/Public/DisplayClusterConfigurationStrings.h"
#include "Components/SplineComponent.h"
#include "Engine/Texture2DArray.h"
#include "GeometryScript/PolyPathFunctions.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"
#include "Slate/SceneViewport.h"

DECLARE_STATS_GROUP(TEXT("TestTime"), STATGROUP_CSTest, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("CS Execute"), STAT_CSTest_Execute, STATGROUP_CSTest)
using namespace CSHepler;

void UComputeShaderBasicFunction::DrawLinearColorsToRenderTarget(UTextureRenderTarget2D* InTextureTarget,
	TArray<FLinearColor> Colors)
{
	// if (Colors.Num() > InTextureTarget->SizeX * InTextureTarget->SizeY)
	// 	return;
	int32 TextureSize = FMath::CeilToInt(FMath::Pow(Colors.Num(), .5));
	InTextureTarget->ResizeTarget(TextureSize, TextureSize); 
	// TArray<FLinearColor> TmpColors;
	// TmpColors.Init(FLinearColor::Black, InTextureTarget->SizeX * InTextureTarget->SizeY);
	
	// FMemory::Memcpy(TmpColors.GetData(), Colors.GetData(), Colors.Num() * sizeof(FLinearColor));
	FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=](FRHICommandListImmediate& RHICmdList)
	{
		FTexture2DRHIRef TextureRHI = TextureTarget->GetRenderTargetTexture();
		uint32 DestStride;
		void* DestData = RHILockTexture2D(TextureRHI, 0, RLM_WriteOnly, DestStride, false);
		 if (!DestStride)
		 	return;
		
		FMemory::Memcpy(DestData, Colors.GetData(), TextureTarget->GetSizeXY().X * TextureTarget->GetSizeXY().Y * sizeof(FLinearColor));
		RHIUnlockTexture2D(TextureRHI, 0 ,false);
	});
	FlushRenderingCommands();
}

void UComputeShaderBasicFunction::ConnectivityPixel(UTextureRenderTarget2D* InTextureTarget,
                                                    UTextureRenderTarget2D* InConnectivityMap, UTextureRenderTarget2D* InDebugView, int32 Channel, int32 TextureSize)
{
	if (InTextureTarget == nullptr || InConnectivityMap == nullptr || InDebugView == nullptr)
		return;
	
	// int32 TextureSize = 409;
	InTextureTarget->ResizeTarget(TextureSize, TextureSize);
	InConnectivityMap->ResizeTarget(TextureSize, TextureSize);
	InDebugView->ResizeTarget(TextureSize, TextureSize);
	FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
	FRenderTarget* ConnectivityMap = InConnectivityMap->GameThread_GetRenderTargetResource();
	FRenderTarget* DebugView = InDebugView->GameThread_GetRenderTargetResource();
	{
		SCOPE_CYCLE_COUNTER(STAT_CSTest_Execute);
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[=](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			{
				
				TShaderMapRef<FConnectivityPixel> ComputeShader_Init = FConnectivityPixel::CreateConnectivityPermutation(FConnectivityPixel::EConnectivityStep::CP_Init);
				TShaderMapRef<FConnectivityPixel> ComputeShader_FindIslands = FConnectivityPixel::CreateConnectivityPermutation(FConnectivityPixel::EConnectivityStep::CP_FindIslands);
				TShaderMapRef<FConnectivityPixel> ComputeShader_Count = FConnectivityPixel::CreateConnectivityPermutation(FConnectivityPixel::EConnectivityStep::CP_Count);
				TShaderMapRef<FConnectivityPixel> ComputeShader_NormalizeResult = FConnectivityPixel::CreateConnectivityPermutation(FConnectivityPixel::EConnectivityStep::CP_NormalizeResult);
				TShaderMapRef<FConnectivityPixel> ComputeShader_DrawTexture = FConnectivityPixel::CreateConnectivityPermutation(FConnectivityPixel::EConnectivityStep::CP_DrawTexture);
				
				FConnectivityPixel::FParameters* PassParameters = GraphBuilder.AllocParameters<FConnectivityPixel::FParameters>();
				FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(TextureTarget->GetSizeXY().X, TextureTarget->GetSizeXY().Y, 1), 32);
				FIntPoint TextureSizeXY = ConnectivityMap->GetSizeXY();
				
				FRDGTextureRef TmpTexture_ConnectivityMap = ConvertToUVATexture(ConnectivityMap, GraphBuilder);
				FRDGTextureRef TmpTexture_DebugView = ConvertToUVATextureFormat(GraphBuilder,DebugView);
				FRDGTextureRef TextureTargetTexture = RegisterExternalTexture(GraphBuilder, TextureTarget->GetRenderTargetTexture(), TEXT("Input_RT"));
				
				FRDGTextureRef TmpTexture_LabelBufferA = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_A32B32G32R32F, TEXT("LabelBufferA"));
				FRDGTextureUAVRef TmpTextureUAV_LabelBufferA = GraphBuilder.CreateUAV(TmpTexture_LabelBufferA);
				FRDGTextureRef TmpTexture_LabelBufferB = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_A32B32G32R32F, TEXT("LabelBufferB"));
				FRDGTextureUAVRef TmpTextureUAV_LabelBufferB = GraphBuilder.CreateUAV(TmpTexture_LabelBufferB);
				FRDGTextureRef TmpTexture_CountBuffer = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_R32_UINT, TEXT("CountBuffer"));
				FRDGTextureUAVRef TmpTextureUAV_CountBuffer = GraphBuilder.CreateUAV(TmpTexture_CountBuffer);


				const uint32 NumElements = TextureTarget->GetSizeXY().X * TextureTarget->GetSizeXY().Y;
				const uint32 BytesPerElement = sizeof(uint32);
				FRDGBufferRef Tmp_NormalizeCounterBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(BytesPerElement, 1), TEXT("NormalizeCountBuffer"));
				FRDGBufferUAVRef Tmp_NormalizeCounterBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Tmp_NormalizeCounterBuffer, EPixelFormat::PF_R32_UINT));
				AddClearUAVPass(GraphBuilder,Tmp_NormalizeCounterBufferUAV, 0);

				const uint32 BytesPerElementFloat4 = sizeof(FVector4f);
				FRDGBufferRef Tmp_ResultBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(BytesPerElement, NumElements), TEXT("ResultBuffer"));
				FRDGBufferUAVRef Tmp_ResultBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Tmp_ResultBuffer, EPixelFormat::PF_A32B32G32R32F));
				AddClearUAVPass(GraphBuilder,Tmp_ResultBufferUAV, 0);
				// FRHIBuffer* brhi = Tmp_CountBuffer->GetRHI();
				// brhi->GetType();
				// GraphBuilder.QueueBufferUpload(Tmp_CountBuffer, test.GetData(), 4);
				
				
				PassParameters->InputTexture = TextureTargetTexture;
				PassParameters->RW_ConnectivityPixel = GraphBuilder.CreateUAV(TmpTexture_ConnectivityMap);
				PassParameters->RW_LabelBufferA = TmpTextureUAV_LabelBufferA;
				PassParameters->RW_LabelBufferB = TmpTextureUAV_LabelBufferB;
				PassParameters->RW_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);
				PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();
				PassParameters->RW_LabelCounters = TmpTextureUAV_CountBuffer;
				PassParameters->RW_ResultBuffer = Tmp_ResultBufferUAV;
				PassParameters->RW_NormalizeCounter = Tmp_NormalizeCounterBufferUAV;
				PassParameters->Channel = Channel;
				PassParameters->PieceNum = 0;
				
				//Init
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("Init"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_Init, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Init, *PassParameters, GroupCount);
					});
				
				int32 Iteration = GroupCount.X * 2 ;
				// Iteration = 1;
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
				// ERDGPassFlags::Raster
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("Count"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_Count, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_Count, *PassParameters, GroupCount);
					});
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("NormalizeResult"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader_NormalizeResult, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader_NormalizeResult, *PassParameters, GroupCount);
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
		FlushRenderingCommands();
	}
	
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
	UGameViewportClient* ViewportClient = GWorld->GetGameViewport();
	// ViewportClient->GetMousePosition();
	ViewportClient->Viewport ;
	FSceneViewport* SceneViewport = ViewportClient->GetGameViewport();
	// FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(nullptr, nullptr, FEngineShowFlags(ESFIM_Game))
	// 							.SetTime(FGameTime()));
	// FSceneView* View = ViewportClient->CalcSceneView()
	APlayerController* PC = GWorld->GetFirstPlayerController();
	FVector CameraLocation;
	FRotator CameraRotation;
	PC->PlayerCameraManager->GetCameraViewPoint(CameraLocation, CameraRotation);
    
	// 设置视图参数
	// FSceneViewInitOptions ViewInitOptions;
	// ViewInitOptions.ViewFamily = &ViewFamily;
	// ViewInitOptions.SetViewRectangle(ViewRect);
	// ViewInitOptions.ViewOrigin = -Origin;
	// ViewInitOptions.ViewRotationMatrix = ViewRotationMatrix;
	// ViewInitOptions.ProjectionMatrix = ProjectionMatrix;
	// ViewInitOptions.BackgroundColor = FLinearColor::Black;
	//
	// FSceneView* NewView = new FSceneView(ViewInitOptions);
	



	
	// SceneViewport->view
	// GWorld->EditorViews
	FRenderTarget* DebugView = InDebugView->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			TShaderMapRef<FGlobalDistanceFieldForCS> ComputeShader = FGlobalDistanceFieldForCS::CreateTempShaderPermutation(FGlobalDistanceFieldForCS::ESDFShader::GDF_DistanceToNearestSurface);
			FGlobalDistanceFieldForCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGlobalDistanceFieldForCS::FParameters>();
			
			FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(TestArray->GetSizeX(), TestArray->GetSizeY(), 1), 32);
			
			FRDGTextureRef TmpTexture_DebugView = CSHepler::ConvertToUVATexture(DebugView, GraphBuilder);
			FRDGTextureRef DebugViewTexture = RegisterExternalTexture(GraphBuilder, DebugView->GetRenderTargetTexture(), TEXT("DebugView_RT"));
			FRDGTextureRef TextureArray = RegisterExternalTexture(GraphBuilder, TestArray->GetResource()->GetTextureRHI(), TEXT("Input_TA"));

			PassParameters->RW_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);
			
			PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();

			

			
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteExampleComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{

				});
			
			AddCopyTexturePass(GraphBuilder, TmpTexture_DebugView, DebugViewTexture, FRHICopyTextureInfo());
		}
		GraphBuilder.Execute();

	});
	
}

void UComputeShaderBasicFunction::ConvertHeightDataToTexture(
	UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* InHeightData, FVector Center, FVector Extent)
{
	FVector MapMin = FVector::ZeroVector;
	FVector MapMax = FVector::ZeroVector;
	TArray<FLinearColor> Colors = ULandscapeExtra::CreateLandscapeTextureData(MapMin, MapMax, Center, Extent, 256, 1);
	int32 TextureSize = FMath::CeilToInt(FMath::Pow(Colors.Num(), .5));
	// InTextureTarget->ResizeTarget(TextureSize, TextureSize); 
	InHeightData->ResizeTarget(TextureSize, TextureSize);

	FVector TextureMin = Center - Extent;
	FVector TextureMax = Center + Extent;
	
	FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
	FRenderTarget* HeightDataRT = InHeightData->GameThread_GetRenderTargetResource();

	FVector Range = MapMax - MapMin + FVector(0, 0, 1);
	FVector MinUV = (TextureMin - MapMin) / Range;
	FVector MaxUV = (TextureMax - MapMin) / Range;
	FVector UVRange = MaxUV - MinUV;
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			FTexture2DRHIRef TextureRHI = HeightDataRT->GetRenderTargetTexture();
			uint32 DestStride;
			void* DestData = RHILockTexture2D(TextureRHI, 0, RLM_WriteOnly, DestStride, false);
			 if (!DestStride)
			 	return;
		
			FMemory::Memcpy(DestData, Colors.GetData(), HeightDataRT->GetSizeXY().X * HeightDataRT->GetSizeXY().Y * sizeof(FLinearColor));
			RHIUnlockTexture2D(TextureRHI, 0 ,false);
			
			FIntVector GroupSize = FIntVector(TextureTarget->GetSizeXY().X, TextureTarget->GetSizeXY().Y, 1);
			TShaderMapRef<FGeneralFunctionShader> ComputeShader = FGeneralFunctionShader::CreateTempShaderPermutation(FGeneralFunctionShader::ETempShader::GTS_ConvertHeightDataToTexture);
			FGeneralFunctionShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FGeneralFunctionShader::FParameters>();
			FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(GroupSize, 32);
			
			FIntPoint TextureSizeXY = TextureTarget->GetSizeXY();
			
			FRDGTextureRef TmpTexture_HeightNormal = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_FloatRGBA, TEXT("HeightNormal_Texture")); 
			FRDGTextureUAVRef TmpTextureUAV_HeightNormal = GraphBuilder.CreateUAV(TmpTexture_HeightNormal);
			
			
			FRDGTextureRef HeightDataTexture =  RegisterExternalTexture(GraphBuilder, HeightDataRT->GetRenderTargetTexture(), TEXT("HeightData_RT"));
			
			PassParameters->T_ProcssTexture0 = HeightDataTexture;
			PassParameters->RW_ProcssTexture0 = TmpTextureUAV_HeightNormal;
			PassParameters->InputVectorData0 = FVector3f(UVRange.X, UVRange.Y, UVRange.Z);
			PassParameters->InputVectorData1 = FVector3f(MinUV.X, MinUV.Y, MinUV.Z);
			PassParameters->Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
			
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ConvertHeightDataToTexture"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});
			FRDGTextureRef OutTexture = RegisterExternalTexture(GraphBuilder, TextureTarget->GetRenderTargetTexture(), TEXT("OutTexture_RT"));
			AddCopyTexturePass(GraphBuilder, TmpTexture_HeightNormal, OutTexture, FRHICopyTextureInfo());

			
			
		}
		GraphBuilder.Execute();
	});
	FlushRenderingCommands();
}

void UComputeShaderBasicFunction::ExtentMaskFast(UTextureRenderTarget2D* InTextureTarget, UTextureRenderTarget2D* InDebugView, int32 Channel, int32 NumExtend)
{

	FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
	FRenderTarget* DebugView = InDebugView->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			FIntVector GroupSize = FIntVector(TextureTarget->GetSizeXY().X, TextureTarget->GetSizeXY().Y, 1);
			FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(GroupSize, 32);
			FIntPoint TextureSizeXY = TextureTarget->GetSizeXY();
			TShaderMapRef<FGeneralFunctionShader> ComputeShader = FGeneralFunctionShader::CreateTempShaderPermutation(FGeneralFunctionShader::ETempShader::GTS_MaskExtendFast);
			FGeneralFunctionShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FGeneralFunctionShader::FParameters>();
			
			
			
			FRDGTextureRef TmpTexture_HeightNormal = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_FloatRGBA, TEXT("InTexture_RWTexture")); 
			FRDGTextureUAVRef TmpTextureUAV_HeightNormal = GraphBuilder.CreateUAV(TmpTexture_HeightNormal);
			FRDGTextureRef TmpTexture_DebugView = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_FloatRGBA, TEXT("DebugView_RWTexture")); 
			FRDGTextureUAVRef TmpTextureUAV_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);
			
			
			FRDGTextureRef InTexture =  RegisterExternalTexture(GraphBuilder, TextureTarget->GetRenderTargetTexture(), TEXT("InTexture_RT"));
			
			PassParameters->T_ProcssTexture0 = InTexture;
			PassParameters->RW_ProcssTexture0 = TmpTextureUAV_HeightNormal;
			PassParameters->RW_DebugView = TmpTextureUAV_DebugView;
			PassParameters->InputIntData0 = Channel;
			PassParameters->InputIntData1 = NumExtend;
			PassParameters->Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
			
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("MaskExtendFast"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});

			FRDGTextureRef DebugViewTexture = RegisterExternalTexture(GraphBuilder, DebugView->GetRenderTargetTexture(), TEXT("OutTexture_RT"));
			AddCopyTexturePass(GraphBuilder, TmpTexture_DebugView, DebugViewTexture, FRHICopyTextureInfo());
			AddCopyTexturePass(GraphBuilder, TmpTexture_HeightNormal, DebugViewTexture, FRHICopyTextureInfo());

			
			
		}
		GraphBuilder.Execute();
	});
	FlushRenderingCommands();
}

TArray<FTransform> UComputeShaderBasicFunction::SampleSpline(UTextureRenderTarget2D* InTextureTarget,
                                                             UTextureRenderTarget2D* InDebugView,
                                                             TArray<USplineComponent*> InSplineComponents,
                                                             FBoxSphereBounds& Bounds,
                                                             int32 TextureSize, float Interval)
{
	TArray<FTransform> OutTransforms;
	if (InTextureTarget == nullptr || InDebugView == nullptr || InSplineComponents.Num() == 0) return OutTransforms;

	
	int32 ThreadGroupSize = 32;
	int32 GroupCountX = TextureSize / ThreadGroupSize;
	int32 SplineMaxElement = 1024;
	int32 NumSpline = InSplineComponents.Num();
	// int32 MaxNumPointPerCell = 128;
	int32 BufferNumElement = SplineMaxElement * InSplineComponents.Num();
	int32 ElementSize = sizeof(FLinearColor);

	
	
	FBoxSphereBounds MaxBounds = InSplineComponents[0]->Bounds;
	TArray<TArray<FTransform>> ResampleTransform;
	for (USplineComponent* SplineComponent : InSplineComponents)
	{
		MaxBounds = Union(MaxBounds, SplineComponent->Bounds) ;
		float SplineLength = SplineComponent->GetSplineLength();
		// int32 ExpectSamples = FMath::Min(SplineLength / FMath::Max(1, Interval), SplineMaxElement);
		TArray<FTransform> Frames;
		TArray<double> FrameTimes;
		FGeometryScriptSplineSamplingOptions SamplingOptions;
		SamplingOptions.NumSamples = SplineMaxElement;
		if (!UGeometryScriptLibrary_PolyPathFunctions::SampleSplineToTransforms(SplineComponent, Frames, FrameTimes, SamplingOptions, FTransform::Identity)) continue;
		ResampleTransform.Add(Frames);
		
	}
	MaxBounds.BoxExtent =  FVector(1, 1, 0) * FMath::Max(MaxBounds.BoxExtent.X, MaxBounds.BoxExtent.Y);
	FVector BoundMin = MaxBounds.Origin - MaxBounds.BoxExtent;
	FVector BoundMax = MaxBounds.Origin + MaxBounds.BoxExtent;
	FVector BoundSize = BoundMax - BoundMin;
	// FVector Boundsize = FVector(1, 1, 0) * FMath::Max(BoundSize.X, BoundSize.Y);
	float SizeTatal = FMath::Max(BoundSize.X, BoundSize.Y);
	float SizePerCell = SizeTatal / GroupCountX;

	TArray<int32> SplinePointCount;
	TArray<FLinearColor> SplinePoints;
	SplinePoints.AddZeroed(NumSpline * SplineMaxElement);
	SplinePointCount.AddZeroed(NumSpline);
	OutTransforms.Reserve(NumSpline * SplineMaxElement);
	for (int32 i = 0; i < NumSpline; i++)
	{
		FTransform SplineTransform = InSplineComponents[i]->GetComponentTransform();
		SplinePointCount[i] = ResampleTransform[i].Num();
		for (int32 j = 0; j < ResampleTransform[i].Num(); j++)
		{
			FTransform Transform = ResampleTransform[i][j] * SplineTransform;
			float Rotate = Transform.GetRotation().Rotator().Yaw;
			FVector Location = Transform.GetLocation();
			FVector LocationUVW = (Location - BoundMin) / BoundSize;
			FLinearColor Color = FLinearColor(Location.X, Location.Y, Location.Z, Rotate);
			SplinePoints[i * SplineMaxElement + j] = Color;
			OutTransforms.Add(Transform);
		}
	}
	{
		SCOPE_CYCLE_COUNTER(STAT_CSTest_Execute);
		InTextureTarget->ResizeTarget(TextureSize, TextureSize);
		InDebugView->ResizeTarget(TextureSize, TextureSize);
		
		FRenderTarget* TextureTarget = InTextureTarget->GameThread_GetRenderTargetResource();
		FRenderTarget* DebugView = InDebugView->GameThread_GetRenderTargetResource();
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[=](FRHICommandListImmediate& RHICmdList)
		{
			FRDGBuilder GraphBuilder(RHICmdList);
			{
				
				FIntVector GroupSize = FIntVector(TextureTarget->GetSizeXY().X, TextureTarget->GetSizeXY().Y, 1);
				FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(GroupSize, 32);
				// GroupCount = FIntVector(1, 1, 1);
				FIntPoint TextureSizeXY = TextureTarget->GetSizeXY();
				
				TShaderMapRef<FSampleSpline> ComputeShader = FSampleSpline::CreateTempShaderPermutation(FSampleSpline::ESampleStep::SS_SampleSpline);
				FSampleSpline::FParameters* PassParameters = GraphBuilder.AllocParameters<FSampleSpline::FParameters>();
				
				FRDGTextureRef TmpTexture_InTexture = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_FloatRGBA, TEXT("InTexture_RWTexture")); 
				FRDGTextureUAVRef TmpTextureUAV_InTexture = GraphBuilder.CreateUAV(TmpTexture_InTexture);
				FRDGTextureRef TmpTexture_DebugView = ConvertToUVATextureFormat(GraphBuilder, TextureSizeXY, PF_FloatRGBA, TEXT("DebugView_RWTexture")); 
				FRDGTextureUAVRef TmpTextureUAV_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);
				
				FRDGBufferRef Tmp_SplineDataBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(ElementSize, BufferNumElement), TEXT("SplineDataBuffer"));
				GraphBuilder.QueueBufferUpload(Tmp_SplineDataBuffer, SplinePoints.GetData(), SplinePoints.Num() * ElementSize);
				FRDGBufferUAVRef Tmp_SplineDataBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Tmp_SplineDataBuffer, EPixelFormat::PF_A32B32G32R32F));
				// GraphBuilder.QueueBufferUpload(Tmp_SplineDataBufferUAV, SplinePoints.GetData(), SplinePoints.Num());
				FRDGBufferRef Tmp_SplinePointCountBuffer = GraphBuilder.CreateBuffer(FRDGBufferDesc::CreateBufferDesc(sizeof(int32), NumSpline), TEXT("SplinePointCountBuffer"));
				GraphBuilder.QueueBufferUpload(Tmp_SplinePointCountBuffer, SplinePointCount.GetData(), SplinePointCount.Num() * sizeof(int32));
				FRDGBufferUAVRef Tmp_SplinePointCountBufferUAV = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(Tmp_SplinePointCountBuffer, EPixelFormat::PF_R32_UINT));
				
				
				PassParameters->RW_OutTexture = TmpTextureUAV_InTexture;
				PassParameters->RW_DebugView = TmpTextureUAV_DebugView;
				PassParameters->RW_PointsToSampleBuffer = Tmp_SplineDataBufferUAV;
				PassParameters->RW_SplinePointCount = Tmp_SplinePointCountBufferUAV;
				PassParameters->NumSpline = NumSpline;
				PassParameters->BoundsMin = FVector3f(BoundMin.X, BoundMin.Y, BoundMin.Z);
				PassParameters->BoundsSize = FVector3f(BoundSize.X, BoundSize.Y, BoundSize.Z);
				
				PassParameters->Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
				
				GraphBuilder.AddPass(
					RDG_EVENT_NAME("SampleSpline"),
					PassParameters,
					ERDGPassFlags::AsyncCompute,
					[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
					{
						FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
					});

				FRDGTextureRef InTexture =  RegisterExternalTexture(GraphBuilder, TextureTarget->GetRenderTargetTexture(), TEXT("InTexture_RT"));
				FRDGTextureRef DebugViewTexture = RegisterExternalTexture(GraphBuilder, DebugView->GetRenderTargetTexture(), TEXT("DebugView_RT"));
				AddCopyTexturePass(GraphBuilder, TmpTexture_DebugView, DebugViewTexture, FRHICopyTextureInfo());
				AddCopyTexturePass(GraphBuilder, TmpTexture_InTexture, InTexture, FRHICopyTextureInfo());
			}
			GraphBuilder.Execute();
		});
		FlushRenderingCommands();
	}
	
	
	Bounds = MaxBounds;
	return OutTransforms;
}

void UComputeShaderBasicFunction::CalDistanceToNearestSurface(FSceneView* SceneView, UTextureRenderTarget2D* InDebugView)
{
	// FScene* Scene;
	// Scene->ViewStates.view
	// Scene->DistanceFieldSceneData.
	// scene
	// GlobalDistanceFieldInfo()
	
	FRenderTarget* DebugView = InDebugView->GameThread_GetRenderTargetResource();
	ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
	[=](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			TShaderMapRef<FGlobalDistanceFieldForCS> ComputeShader = FGlobalDistanceFieldForCS::CreateTempShaderPermutation(FGlobalDistanceFieldForCS::ESDFShader::GDF_DistanceToNearestSurface);
			FGlobalDistanceFieldForCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FGlobalDistanceFieldForCS::FParameters>();
			
			FIntVector GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(DebugView->GetSizeXY().X, DebugView->GetSizeXY().Y, 1), 32);
			
			FRDGTextureRef TmpTexture_DebugView = CSHepler::ConvertToUVATexture(DebugView, GraphBuilder);
			FRDGTextureRef DebugViewTexture = RegisterExternalTexture(GraphBuilder, DebugView->GetRenderTargetTexture(), TEXT("DebugView_RT"));
			FRDGTextureRef TextureArray = RegisterExternalTexture(GraphBuilder, InDebugView->GetResource()->GetTextureRHI(), TEXT("Input_TA"));

			PassParameters->RW_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);
			
			PassParameters->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();

			
			TUniformBufferRef<FViewUniformShaderParameters> ViewUniformBuffer = SceneView->ViewUniformBuffer;
			const FGlobalDistanceFieldParameterData* GlobalDistanceFieldParameterData = GetRendererModule().GetGlobalDistanceFieldParameterData(*SceneView);
			TSet<FSceneInterface*> FSISet = GetRendererModule().GetAllocatedScenes();
			FSceneInterface* FSI;
			FSI->get
			// GetRendererModule().renderer
			// FScene* Scene;
			// Scene->DistanceFieldSceneData
// Scene->DistanceFieldSceneData.distance
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteExampleComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount, ViewUniformBuffer, GlobalDistanceFieldParameterData](FRHIComputeCommandList& RHICmdList)
				{
					PassParameters->View = ViewUniformBuffer;
					PassParameters->GlobalDistanceFieldParameters = SetupGlobalDistanceFieldParameters_Minimal(*GlobalDistanceFieldParameterData);
					PassParameters->GlobalDistanceFieldParameters.GlobalDistanceFieldCoverageAtlasTextureSampler = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
					PassParameters->GlobalDistanceFieldParameters.GlobalDistanceFieldPageAtlasTextureSampler = TStaticSamplerState<SF_Trilinear, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();
					PassParameters->GlobalDistanceFieldParameters.GlobalDistanceFieldMipTextureSampler = TStaticSamplerState<SF_Trilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});
			
			AddCopyTexturePass(GraphBuilder, TmpTexture_DebugView, DebugViewTexture, FRHICopyTextureInfo());
		}
		GraphBuilder.Execute();

	});
	
}

