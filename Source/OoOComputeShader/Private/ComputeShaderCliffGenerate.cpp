#include "ComputeShaderCliffGenerate.h"

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

DECLARE_STATS_GROUP(TEXT("CSCliffGenerate"), STATGROUP_CSCliffGenerate, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("CS Execute"), STAT_CSCliffGenerate_Execute, STATGROUP_CSCliffGenerate)
DECLARE_CYCLE_STAT(TEXT("CS Capture"), STAT_CSCliffGenerate_Capture, STATGROUP_CSCliffGenerate)
DECLARE_CYCLE_STAT(TEXT("CS Tatal"), STAT_CSCliffGenerate_Tatal, STATGROUP_CSCliffGenerate);


#define NUM_THREADS_PER_GROUP_DIMENSION_X 32
#define NUM_THREADS_PER_GROUP_DIMENSION_Y 32
#define NUM_THREADS_PER_GROUP_DIMENSION_Z 1
/// <summary>
///// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
/// </summary>
///

using namespace CSHepler;

class FCliffGenerate : public FGlobalShader
{
public:
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FCliffGenerate);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FCliffGenerate, FGlobalShader);
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



void UComputeShaderCliffGenerateFunctions::CSCliffGenerate(ACSGenerateCaptureScene* Capturer, UStaticMesh* StaticMesh,
	UTextureRenderTarget2D* DubugView, UTextureRenderTarget2D* Result, UTexture2D* TMeshDepth, float SpawnSize,
	float TestSizeScale, FName Tag)
{
	
}
