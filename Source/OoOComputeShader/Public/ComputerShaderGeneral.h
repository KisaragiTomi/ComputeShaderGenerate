#pragma once

#include "GlobalShader.h"
#include "MaterialShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "ComputerShaderGenerateHepler.h"
#include "EngineUtils.h"


#define NUM_THREADS_PER_GROUP_DIMENSION_X 32
#define NUM_THREADS_PER_GROUP_DIMENSION_Y 32
#define NUM_THREADS_PER_GROUP_DIMENSION_Z 1
/// <summary>
///// This class carries our parameter declarations and acts as the bridge between cpp and HLSL.
/// </summary>
///

class FCalculateGradient : public FGlobalShader
{
public:
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FCalculateGradient);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FCalculateGradient, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_Height)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_Gradient)

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

		// if (Parameters.PermutationId == 0)
		// {
		// 	OutEnvironment.SetDefine(TEXT("ENTRY_FUNCTION"), TEXT("Init"));
		// }
		
	}

	
};

IMPLEMENT_GLOBAL_SHADER(FCalculateGradient, "/OoOShaders/BasicFunction.usf", "CalculateGradient", SF_Compute);

class FConnectivityPixel : public FGlobalShader
{
public:
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FConnectivityPixel);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FConnectivityPixel, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_ConnectivityPixel)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RW_LabelBufferA)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<uint>, RW_LabelBufferB)
		SHADER_PARAMETER_UAV(RWBuffer<uint>, RW_LabelCounters)
		SHADER_PARAMETER(int, Step)

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

		if (Parameters.PermutationId == 0)
		{
			OutEnvironment.SetDefine(TEXT("ENTRY_FUNCTION"), TEXT("CP_Init"));
		}
		if (Parameters.PermutationId == 1)
		{
			OutEnvironment.SetDefine(TEXT("ENTRY_FUNCTION"), TEXT("CP_FindIslands"));
		}
		if (Parameters.PermutationId == 2)
		{
			OutEnvironment.SetDefine(TEXT("ENTRY_FUNCTION"), TEXT("CP_Count"));
		}
		if (Parameters.PermutationId == 3)
		{
			OutEnvironment.SetDefine(TEXT("ENTRY_FUNCTION"), TEXT("CP_DrawTexture"));
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FConnectivityPixel, "/OoOShaders/Connectivity.usf", "ConnectivityPixel", SF_Compute);


class FBlurTexture : public FGlobalShader
{
public:
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FBlurTexture);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FBlurTexture, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_BlurTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_BlurTexture)
		SHADER_PARAMETER(float, BlurScale)

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

IMPLEMENT_GLOBAL_SHADER(FBlurTexture, "/OoOShaders/BasicFunction.usf", "BlurTexture", SF_Compute);

class FUpPixelsMask : public FGlobalShader
{
public:
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FUpPixelsMask);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FUpPixelsMask, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_UpPixel)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_UpPixel)
		SHADER_PARAMETER(float, UpPixelThreshold)

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

IMPLEMENT_GLOBAL_SHADER(FUpPixelsMask, "/OoOShaders/BasicFunction.usf", "UpPixel", SF_Compute);