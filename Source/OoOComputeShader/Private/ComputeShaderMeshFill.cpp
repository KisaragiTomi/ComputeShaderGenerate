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

DECLARE_STATS_GROUP(TEXT("CSMeshFill"), STATGROUP_CSGenerate, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("CS Execute"), STAT_CSGenerate_Execute, STATGROUP_CSGenerate)
DECLARE_CYCLE_STAT(TEXT("CS Capture"), STAT_CSGenerate_Capture, STATGROUP_CSGenerate)
DECLARE_CYCLE_STAT(TEXT("CS Tatal"), STAT_CSGenerate_Tatal, STATGROUP_CSGenerate);


#define NUM_THREADS_PER_GROUP_DIMENSION_X 32
#define NUM_THREADS_PER_GROUP_DIMENSION_Y 32
#define NUM_THREADS_PER_GROUP_DIMENSION_Z 1
/// <summary>
///// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
/// </summary>
///

using namespace CSHepler;

class FMeshFill : public FGlobalShader
{
public:
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FMeshFill);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FMeshFill, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_SceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_SceneNormal)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_TMeshDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_CurrentSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_CurrentSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_DebugView)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_Result)
		SHADER_PARAMETER(float, RandomRotator)
		SHADER_PARAMETER(float, Size)
		// SHADER_PARAMETER_UAV(RWBuffer<float4>, OutBounds)
		// SHADER_PARAMETER_SRV(Buffer<float>, InParticleIndices)
		SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
	END_SHADER_PARAMETER_STRUCT()
public:
	//Called by the engine to determine which permutations to compile for this shader
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		

		return true;
	}
	//Modifies the compilations environment of the shader
	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code 
		// when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
	
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), NUM_THREADS_PER_GROUP_DIMENSION_X);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), NUM_THREADS_PER_GROUP_DIMENSION_Y);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), NUM_THREADS_PER_GROUP_DIMENSION_Z);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMeshFill, "/OoOShaders/MeshFill.usf", "MainComputeShader", SF_Compute);

class FMeshFillMult : public FGlobalShader
{
public:
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FMeshFillMult);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FMeshFillMult, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_SceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_SceneNormal)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_TMeshDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_Result)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_CurrentSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_CurrentSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_DebugView)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_Result)
		SHADER_PARAMETER(float, RandomRotator)
		SHADER_PARAMETER(float, Size)
		// SHADER_PARAMETER_UAV(RWBuffer<float4>, OutBounds)
		// SHADER_PARAMETER_SRV(Buffer<float>, InParticleIndices)
		SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
	END_SHADER_PARAMETER_STRUCT()
public:
	//Called by the engine to determine which permutations to compile for this shader
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		

		return true;
	}
	//Modifies the compilations environment of the shader
	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code 
		// when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
	
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), NUM_THREADS_PER_GROUP_DIMENSION_X);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), NUM_THREADS_PER_GROUP_DIMENSION_Y);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), NUM_THREADS_PER_GROUP_DIMENSION_Z);
	}
};

IMPLEMENT_GLOBAL_SHADER(FMeshFillMult, "/OoOShaders/MeshFill.usf", "MeshFillMult", SF_Compute);

class FDrawHeightMap : public FGlobalShader
{
public:
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FDrawHeightMap);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FDrawHeightMap, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
	
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_Result)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_TMeshDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_CurrentSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_CurrentSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_Result)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_DebugView)
		// SHADER_PARAMETER(float, RandomRotator)
		// SHADER_PARAMETER(float, Size)
		
		SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
	END_SHADER_PARAMETER_STRUCT()
public:
	//Called by the engine to determine which permutations to compile for this shader
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		

		return true;
	}
	//Modifies the compilations environment of the shader
	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		//We're using it here to add some preprocessor defines. That way we don't have to change both C++ and HLSL code 
		// when we change the value for NUM_THREADS_PER_GROUP_DIMENSION
	
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), NUM_THREADS_PER_GROUP_DIMENSION_X);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), NUM_THREADS_PER_GROUP_DIMENSION_Y);
		OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Z"), NUM_THREADS_PER_GROUP_DIMENSION_Z);
	}
};

IMPLEMENT_GLOBAL_SHADER(FDrawHeightMap, "/OoOShaders/MeshFill.usf", "DrawCurrentHeightMap", SF_Compute);


void FMeshFillCSInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FMeshFillCSParameters Params)
{
	// FRDGBuilder GraphBuilder(RHICmdList);
	// {
	// 	// SCOPE_CYCLE_COUNTER(STAT_OoOComputeShader_Execute);
	// 	// DECLARE_GPU_STAT(OoOComputeShader)
	// 	// RDG_EVENT_SCOPE(GraphBuilder, "OoOComputeShader");
	// 	// RDG_GPU_STAT_SCOPE(GraphBuilder, OoOComputeShader);
	// 	
	// 	
	// 	typename FMeshFill::FPermutationDomain PermutationVector;
	//
	// 	TShaderMapRef<FMeshFill> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
	//
	// 	bool bIsShaderValid = ComputeShader.IsValid();
	//
	// 	if (bIsShaderValid)
	// 	{
	// 		FMeshFill::FParameters* PassParameters = GraphBuilder.AllocParameters<FMeshFill::FParameters>();
	//
	// 		const uint32 NumElements = Params.Sizes.Num();
	// 		const uint32 BytesPerElement = sizeof(float);
	// 		const uint32 TotalBytes = NumElements * BytesPerElement;
	//
	// 		// 创建结构化缓冲区
	// 		FRHIResourceCreateInfo CreateSizeInfo(TEXT("SizeBuffer"));
	// 		FBufferRHIRef StructuredBuffer = RHICreateStructuredBuffer(
	// 			BytesPerElement,            // 每个元素的大小
	// 			TotalBytes,                 // 总字节数
	// 			BUF_Static | BUF_ShaderResource,         // 允许着色器访问
	// 			CreateSizeInfo
	// 		);
	//
	// 		// 将数据复制到缓冲区
	// 		void* BufferData = RHICmdList.LockBuffer(StructuredBuffer, 0, TotalBytes, RLM_WriteOnly);
	// 		FMemory::Memcpy(BufferData, Params.Sizes.GetData(), TotalBytes);
	// 		RHICmdList.UnlockBuffer(StructuredBuffer);
	//
	// 		// 创建Shader Resource View (SRV)
	// 		FShaderResourceViewRHIRef ShaderResourceView = RHICreateShaderResourceView(StructuredBuffer);
	// 		
	// 		const int32 BufferSize = 64;
	// 		FRHIResourceCreateInfo CreateInfo(TEXT("BoundsVertexBuffer"));
	// 		FBufferRHIRef BoundsVertexBufferRHI = RHICmdList.CreateVertexBuffer(
	// 			BufferSize,
	// 			BUF_UnorderedAccess | BUF_KeepCPUAccessible,
	// 			CreateInfo);
	// 		FUnorderedAccessViewRHIRef BoundsVertexBufferUAV = RHICmdList.CreateUnorderedAccessView(
	// 			BoundsVertexBufferRHI,
	// 			PF_A32B32G32R32F );
	// 		PassParameters->OutBounds = BoundsVertexBufferUAV;
	//
	// 		//FRHIShaderResourceView* VertexBufferSRV;
	//
	// 		FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(Params.ColorRT->GetSizeXY(), PF_FloatRGBA, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
	// 		FRDGTextureRef TmpTexture = GraphBuilder.CreateTexture(Desc, TEXT("OoOComputeShader_TempTexture"));
	// 		FRDGTextureRef TargetTexture = RegisterExternalTexture(GraphBuilder, Params.ColorRT->GetRenderTargetTexture(), TEXT("OoOComputeShader_RT"));
	// 		PassParameters->ColorRT = TargetTexture;
	// 		PassParameters->DrawRT = GraphBuilder.CreateUAV(TmpTexture);
	// 		
	// 		//PassParameters->Seed = 1;
	// 		
	// 		auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Params.X, Params.Y, Params.Z), FComputeShaderUtils::kGolden2DGroupSize);
	// 		GraphBuilder.AddPass(
	// 			RDG_EVENT_NAME("ExecuteExampleComputeShader"),
	// 			PassParameters,
	// 			ERDGPassFlags::AsyncCompute,
	// 			[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
	// 			{
	// 				FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
	// 			});
	// 		
	// 		FRDGTextureRef RDGSourceTexture = RegisterExternalTexture(GraphBuilder, Params.DrawRT->GetRenderTargetTexture(), TEXT("DrawHOLDTexture_Gradient"));
	// 		AddCopyTexturePass(GraphBuilder, TmpTexture, RDGSourceTexture, FRHICopyTextureInfo());
	//
	// 		BoundsVertexBufferUAV.SafeRelease();
	// 		ShaderResourceView.SafeRelease();
	// 	}
	// }
	// GraphBuilder.Execute();
	
}

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
		Capturer->CaptureSceneNormal->CaptureScene();
		FlushRenderingCommands();
	}
	
	FCSGenerateParameter Parameter;
	Parameter.SceneDepth = Capturer->CaptureSceneDepth->TextureTarget;
	Parameter.SceneNormal =Capturer->CaptureSceneNormal->TextureTarget;
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
	Capturer->CaptureSceneNormal->ShowOnlyActors = StaticMeshActors;
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
		Capturer->CaptureSceneNormal->CaptureScene();
		FlushRenderingCommands();
	}
	
	FCSGenerateParameter Parameter;
	Parameter.SceneDepth = Capturer->CaptureSceneDepth->TextureTarget;
	Parameter.SceneNormal =Capturer->CaptureSceneNormal->TextureTarget;
	Parameter.Result = Result;
	Parameter.DebugView = DubugView;
	Parameter.CurrentSceneDepth = CurrentSceneDepth;
	Parameter.TMeshDepth = TMeshDepth;
	Parameter.Size = DrawSize * TestSizeScale;
	Parameter.RandomRoation = FMath::FRandRange(0.0, 1.0);

	{
		SCOPE_CYCLE_COUNTER(STAT_CSGenerate_Execute);
		UComputeShaderMeshFillFunctions::CalculateMeshLoctionAndRotationMult(Parameter, 1);
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
	Capturer->CaptureSceneNormal->ShowOnlyActors = StaticMeshActors;
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
	[NumIteraion, SizeX, SizeY, SceneDepth, TMeshDepth, DebugView, Result, SceneNormal, CurrentSceneDepth, Params](FRHICommandListImmediate& RHICmdList)
	{
		FRDGBuilder GraphBuilder(RHICmdList);
		{
			DECLARE_GPU_STAT(CSMeshFill)
			RDG_EVENT_SCOPE(GraphBuilder, "CSMeshFill");
			RDG_GPU_STAT_SCOPE(GraphBuilder, CSMeshFill);
			
			TShaderMapRef<FMeshFillMult> ComputeShaderTestMesh(GetGlobalShaderMap(GMaxRHIFeatureLevel), typename FMeshFillMult::FPermutationDomain());
			TShaderMapRef<FDrawHeightMap> ComputeShaderDrawMesh(GetGlobalShaderMap(GMaxRHIFeatureLevel), typename FDrawHeightMap::FPermutationDomain());
		
			bool bIsShaderValid = ComputeShaderTestMesh.IsValid() && ComputeShaderDrawMesh.IsValid();
		
			if (bIsShaderValid && SceneDepth != nullptr)
			{
				auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(SizeX, SizeY, 1), FComputeShaderUtils::kGolden2DGroupSize);
				
				FMeshFillMult::FParameters* PassParameters_MF = GraphBuilder.AllocParameters<FMeshFillMult::FParameters>();
				FDrawHeightMap::FParameters* PassParameters_DH = GraphBuilder.AllocParameters<FDrawHeightMap::FParameters>();
				
				FRDGTextureDesc Desc_DebugView(FRDGTextureDesc::Create2D(SceneDepth->GetSizeXY(), PF_FloatRGBA, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
				FRDGTextureRef TmpTexture_DebugView = GraphBuilder.CreateTexture(Desc_DebugView, TEXT("DebugView_TempTexture"));
				AddClearRenderTargetPass(GraphBuilder, TmpTexture_DebugView, FLinearColor::Black);
				FRDGTextureUAVRef TmpTextureUAV_DebugView = GraphBuilder.CreateUAV(TmpTexture_DebugView);
				
				FRDGTextureDesc Desc_CurrentSceneDepth(FRDGTextureDesc::Create2D(SceneDepth->GetSizeXY(), PF_FloatRGBA, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
				FRDGTextureRef TmpTexture_CurrentSceneDepth = GraphBuilder.CreateTexture(Desc_CurrentSceneDepth, TEXT("CurrentSceneDepth_TempTexture"));
				AddClearRenderTargetPass(GraphBuilder, TmpTexture_CurrentSceneDepth, FLinearColor::Black);
				FRDGTextureUAVRef TmpTextureUAV_CurrentSceneDepth = GraphBuilder.CreateUAV(TmpTexture_CurrentSceneDepth);
				
				FRDGTextureDesc Desc_Result(FRDGTextureDesc::Create2D(Result->GetSizeXY(), PF_FloatRGBA, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
				FRDGTextureRef TmpTexture_Result = GraphBuilder.CreateTexture(Desc_Result, TEXT("Result_TempTexture"));
				AddClearRenderTargetPass(GraphBuilder, TmpTexture_Result, FLinearColor(0, 0, 0, 0));
				FRDGTextureUAVRef TmpTextureUAV_Result = GraphBuilder.CreateUAV(TmpTexture_Result);
				
				FRDGTextureRef CurrentSceneDepthTexture =  RegisterExternalTexture(GraphBuilder, CurrentSceneDepth->GetRenderTargetTexture(), TEXT("CurrentSceneDepth_RT"));
				FRDGTextureRef SceneDepthTexture = RegisterExternalTexture(GraphBuilder, SceneDepth->GetRenderTargetTexture(), TEXT("SceneDepth_RT"));
				FRDGTextureRef SceneNormalTexture = RegisterExternalTexture(GraphBuilder, SceneNormal->GetRenderTargetTexture(), TEXT("SceneNormal_RT"));
				FRDGTextureRef ResultTexture = RegisterExternalTexture(GraphBuilder, Result->GetRenderTargetTexture(), TEXT("Result_RT"));
				
				FRDGTextureRef TMeshDepthTexture = RegisterExternalTexture(GraphBuilder, TMeshDepth->GetResource()->GetTextureRHI(), TEXT("TMeshDepth_T"));
				
				PassParameters_MF->T_Result = ResultTexture;
				PassParameters_MF->T_SceneDepth = SceneDepthTexture;
				PassParameters_MF->T_SceneNormal = SceneNormalTexture;
				PassParameters_MF->T_CurrentSceneDepth = CurrentSceneDepthTexture;
				PassParameters_MF->RW_CurrentSceneDepth = TmpTextureUAV_CurrentSceneDepth;
				PassParameters_MF->RW_DebugView = TmpTextureUAV_DebugView;
				PassParameters_MF->RW_Result = TmpTextureUAV_Result;
				PassParameters_MF->Sampler	= TStaticSamplerState<SF_Bilinear>::GetRHI();

				PassParameters_DH->T_Result = ResultTexture;
				PassParameters_DH->T_CurrentSceneDepth = CurrentSceneDepthTexture;
				PassParameters_DH->RW_Result = TmpTextureUAV_Result;
				PassParameters_DH->RW_CurrentSceneDepth = TmpTextureUAV_CurrentSceneDepth;
				PassParameters_DH->RW_DebugView = TmpTextureUAV_DebugView;
				PassParameters_DH->Sampler = TStaticSamplerState<SF_Bilinear>::GetRHI();
				
				
				AddCopyTexturePass(GraphBuilder, SceneDepthTexture, TmpTexture_CurrentSceneDepth, FRHICopyTextureInfo());
				AddCopyTexturePass(GraphBuilder, SceneDepthTexture, CurrentSceneDepthTexture, FRHICopyTextureInfo());
				AddCopyTexturePass(GraphBuilder, TmpTexture_Result, ResultTexture, FRHICopyTextureInfo());
				for (int32 i = 0; i < NumIteraion; i++)
				{
					PassParameters_DH->T_TMeshDepth = TMeshDepthTexture;
					PassParameters_MF->T_TMeshDepth = TMeshDepthTexture;
					PassParameters_MF->Size = Params.Size;
					PassParameters_MF->RandomRotator = Params.RandomRoation;
								
					GraphBuilder.AddPass(
						RDG_EVENT_NAME("ExecuteExampleComputeShader"),
						PassParameters_MF,
						ERDGPassFlags::AsyncCompute,
						[&PassParameters_MF, ComputeShaderTestMesh, GroupCount](FRHIComputeCommandList& RHICmdList)
						{
							FComputeShaderUtils::Dispatch(RHICmdList, ComputeShaderTestMesh, *PassParameters_MF, GroupCount);
						});

					AddCopyTexturePass(GraphBuilder, TmpTexture_Result, ResultTexture, FRHICopyTextureInfo());
					
					GraphBuilder.AddPass(
						RDG_EVENT_NAME("ExecuteExampleComputeShader"),
						PassParameters_DH,
						ERDGPassFlags::AsyncCompute,
						[&PassParameters_DH, ComputeShaderDrawMesh, GroupCount](FRHIComputeCommandList& RHICmdList)
						{
							FComputeShaderUtils::Dispatch(RHICmdList, ComputeShaderDrawMesh, *PassParameters_DH, GroupCount);
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

void UComputeShaderMeshFillFunctions::DrawHeightMap(UTextureRenderTarget2D* InHeight, UTexture2D* InTMeshDepth, float Size, float Rotator)
{

}
