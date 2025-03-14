#include "ComputeShaderGeneratorDeclaration.h"

#include "GlobalShader.h"
#include "MaterialShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"

#define NUM_THREADS_PER_GROUP_DIMENSION_X 32
#define NUM_THREADS_PER_GROUP_DIMENSION_Y 32
#define NUM_THREADS_PER_GROUP_DIMENSION_Z 1

DECLARE_STATS_GROUP(TEXT("ComputeShaderGenerator"), STATGROUP_ComputeShaderGenerator, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("ComputeShaderGenerator Execute"), STAT_ComputeShaderGenerator_Execute, STATGROUP_ComputeShaderGenerator);

class FComputeShaderGenerator : public FGlobalShader
{
public:
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FComputeShaderGenerator);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FComputeShaderGenerator, FGlobalShader);
	/// <summary>
	/// DECLARATION OF THE PARAMETER STRUCTURE
	/// The parameters must match the parameters in the HLSL code
	/// For each parameter, provide the C++ type, and the name (Same name used in HLSL code)
	/// </summary>
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D,  RenderTarget)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER(float, Seed)
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

// This will tell the engine to create the shader and where the shader entry point is.
//												 ShaderType              ShaderPath             Shader function name    Type
IMPLEMENT_GLOBAL_SHADER(FComputeShaderGenerator, "/OoOShaders/OoOShader.usf", "MainComputeShader", SF_Compute);
// IMPLEMENT_GLOBAL_SHADER(FDrawHLODTexture, "/OoOShaders/DrawHLODTexture.usf", "MainComputeShader", SF_Compute);

void FComputeShaderGeneratorInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FOoOCSParameters Params)
{
	FRDGBuilder GraphBuilder(RHICmdList);
	{
		SCOPE_CYCLE_COUNTER(STAT_ComputeShaderGenerator_Execute);
		DECLARE_GPU_STAT(ComputeShaderGenerator)
		RDG_EVENT_SCOPE(GraphBuilder, "ComputeShaderGenerator");
		RDG_GPU_STAT_SCOPE(GraphBuilder, ComputeShaderGenerator);
		
		
		typename FComputeShaderGenerator::FPermutationDomain PermutationVector;

		TShaderMapRef<FComputeShaderGenerator> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

		bool bIsShaderValid = ComputeShader.IsValid();

		if (bIsShaderValid)
		{
			FComputeShaderGenerator::FParameters* PassParameters = GraphBuilder.AllocParameters<FComputeShaderGenerator::FParameters>();


			FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(Params.RenderTarget->GetSizeXY(), PF_B8G8R8A8, FClearValueBinding::White, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
			FRDGTextureRef TmpTexture = GraphBuilder.CreateTexture(Desc, TEXT("ComputeShaderGenerator_TempTexture"));
			FRDGTextureRef TargetTexture = RegisterExternalTexture(GraphBuilder, Params.RenderTarget->GetRenderTargetTexture(), TEXT("ComputeShaderGenerator_RT"));
			PassParameters->RenderTarget = GraphBuilder.CreateUAV(TmpTexture);
			
			FRHITexture* InputTexture = Params.InputTexture->GetResource()->GetTextureRHI();
			FRDGTextureRef RDGSourceTexture = RegisterExternalTexture(GraphBuilder, InputTexture, TEXT("ComputeShaderGenerator_InputTexture"));
			PassParameters->InputTexture = RDGSourceTexture;
			
			
			PassParameters->Seed = Params.Seed;

			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Params.X, Params.Y, Params.Z), FComputeShaderUtils::kGolden2DGroupSize);
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteExampleComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});
			AddCopyTexturePass(GraphBuilder, TmpTexture, TargetTexture, FRHICopyTextureInfo());
		}
	}
	GraphBuilder.Execute();
	
}

