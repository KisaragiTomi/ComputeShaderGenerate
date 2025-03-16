#pragma once

#include "GlobalShader.h"
#include "MaterialShader.h"
#include "ShaderParameterStruct.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "ComputeShaderGenerateHepler.h"
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
	enum class EConnectivityStep : uint8
	{
		CP_Init,
		CP_FindIslands,
		CP_Count,
		CP_NormalizeResult,
		CP_DrawTexture,
		MAX
	};
	class FConnectivityPiexlStep : SHADER_PERMUTATION_ENUM_CLASS("CONNECTIVITYSTEP", EConnectivityStep);
	using FPermutationDomain = TShaderPermutationDomain<FConnectivityPiexlStep>;

	static TShaderMapRef<FConnectivityPixel> CreateConnectivityPermutation(EConnectivityStep Permutation)
	{
		typename FConnectivityPixel::FPermutationDomain PermutationVector;
		PermutationVector.Set<FConnectivityPixel::FConnectivityPiexlStep>(Permutation);
		TShaderMapRef<FConnectivityPixel> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
		return ComputeShader;
	}
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FConnectivityPixel);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FConnectivityPixel, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_ConnectivityPixel)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_DebugView)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_LabelBufferA)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_LabelBufferB)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<int32>, RW_LabelCounters)
		SHADER_PARAMETER(int, Channel)
		SHADER_PARAMETER(int, PieceNum)
		// SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RW_LabelCounters)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<uint>, RW_NormalizeCounter)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWBuffer<float4>, RW_ResultBuffer)

		SHADER_PARAMETER_SAMPLER(SamplerState, Sampler)
	END_SHADER_PARAMETER_STRUCT()
public:
	//Called by the engine to determine which permutations to compile for this shader
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		FPermutationDomain PermutationVector(Parameters.PermutationId);
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
		static const TCHAR* ShaderSourceModeDefineName[] =
		{
			TEXT("CONNECTIVITYSTEP_CP_Init"),
			TEXT("CONNECTIVITYSTEP_CP_FindIslands"),
			TEXT("CONNECTIVITYSTEP_CP_Count"),
			TEXT("CONNECTIVITYSTEP_CP_NormalizeResult"),
			TEXT("CONNECTIVITYSTEP_CP_DrawTexture")
		};
		static_assert(UE_ARRAY_COUNT(ShaderSourceModeDefineName) == (uint32)EConnectivityStep::MAX, "Enum doesn't match define table.");
		
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 SourceModeIndex = static_cast<uint32>(PermutationVector.Get<FConnectivityPiexlStep>());
		OutEnvironment.SetDefine(ShaderSourceModeDefineName[SourceModeIndex], 1u);
		
	}
};

IMPLEMENT_GLOBAL_SHADER(FConnectivityPixel, "/OoOShaders/Connectivity.usf", "ConnectivityPixel", SF_Compute);


class FBlurTexture : public FGlobalShader
{
public:

	enum class EBlurType : uint8
	{
		BT_BLUR3X3,
		BT_BLUR7X7,
		BT_BLUR9X9,
		BT_BLUR15X15,
		BT_BLURNORMAL3X3,
		BT_BLURNORMAL7X7,
		BT_BLURNORMAL9X9,
		BT_BLURNORMAL15X15,
		MAX
	};
	class FBlurFunctionSet : SHADER_PERMUTATION_ENUM_CLASS("BLURTEXTURE", EBlurType);
	using FPermutationDomain = TShaderPermutationDomain<FBlurFunctionSet>;
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
		FPermutationDomain PermutationVector(Parameters.PermutationId);
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
		static const TCHAR* ShaderSourceModeDefineName[] =
		{
			TEXT("BLURTEXTURE_BT_BLUR3X3"),
			TEXT("BLURTEXTURE_BT_BLUR7X7"),
			TEXT("BLURTEXTURE_BT_BLUR9X9"),
			TEXT("BLURTEXTURE_BT_BLUR15X15"),
			TEXT("BLURTEXTURE_BT_BLURNORMAL3X3"),
			TEXT("BLURTEXTURE_BT_BLURNORMAL7X7"),
			TEXT("BLURTEXTURE_BT_BLURNORMAL9X9"),
			TEXT("BLURTEXTURE_BT_BLURNORMAL15X15"),
		};
		static_assert(UE_ARRAY_COUNT(ShaderSourceModeDefineName) == (uint32)EBlurType::MAX, "Enum doesn't match define table.");
		
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 SourceModeIndex = static_cast<uint32>(PermutationVector.Get<FBlurFunctionSet>());
		OutEnvironment.SetDefine(ShaderSourceModeDefineName[SourceModeIndex], 1u);

		EBlurType BlurType = PermutationVector.Get<FBlurFunctionSet>();
		if (BlurType == EBlurType::BT_BLUR3X3 || BlurType == EBlurType::BT_BLURNORMAL3X3)
		{
			OutEnvironment.SetDefine(TEXT("BT_SHAREFIND"), 1);
		}
		if (BlurType == EBlurType::BT_BLUR7X7 || BlurType == EBlurType::BT_BLURNORMAL7X7)
		{
			OutEnvironment.SetDefine(TEXT("BT_SHAREFIND"), 3);
		}
		if (BlurType == EBlurType::BT_BLUR9X9 || BlurType == EBlurType::BT_BLURNORMAL9X9)
		{
			OutEnvironment.SetDefine(TEXT("BT_SHAREFIND"), 4);
		}
		if (BlurType == EBlurType::BT_BLUR15X15 || BlurType == EBlurType::BT_BLURNORMAL15X15)
		{
			OutEnvironment.SetDefine(TEXT("BT_SHAREFIND"), 7);
		}
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
		SHADER_PARAMETER(int, Channel)

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



class FGeneralFunctionShader : public FGlobalShader
{
public:
	enum class ETempShader : uint8
	{
		GTS_ProcessMeshHeightTexture,
		GTS_TextureArrayTest,
		GTS_ConvertHeightDataToTexture,
		GTS_MaskExtendFast,
		MAX
	};
	class FGeneralFunctionSet : SHADER_PERMUTATION_ENUM_CLASS("GeneralFunction", ETempShader);
	using FPermutationDomain = TShaderPermutationDomain<FGeneralFunctionSet>;

	static TShaderMapRef<FGeneralFunctionShader> CreateTempShaderPermutation(ETempShader Permutation)
	{
		typename FGeneralFunctionShader::FPermutationDomain PermutationVector;
		PermutationVector.Set<FGeneralFunctionShader::FGeneralFunctionSet>(Permutation);
		TShaderMapRef<FGeneralFunctionShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
		return ComputeShader;
	}
	
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FGeneralFunctionShader);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FGeneralFunctionShader, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_ProcssTexture0)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_ProcssTexture1)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, TA_ProcssTexture0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_ProcssTexture0)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_ProcssTexture1)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_DebugView)
		SHADER_PARAMETER(float, InputData0)
		SHADER_PARAMETER(int32, InputIntData0)
		SHADER_PARAMETER(int32, InputIntData1)
		SHADER_PARAMETER(FVector3f, InputVectorData0)
		SHADER_PARAMETER(FVector3f, InputVectorData1)

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
		
		static const TCHAR* ShaderSourceModeDefineName[] =
		{
			TEXT("GTS_PROCESSMESHHEIGHTTEXTURE"),
			TEXT("GTS_TEXTUREARRAYTEST"),
			TEXT("GTS_CONVERTHEIGHTDATATEXTURE"),
			TEXT("GTS_MASKEXTENDFAST"),
		};
		static_assert(UE_ARRAY_COUNT(ShaderSourceModeDefineName) == (uint32)ETempShader::MAX, "Enum doesn't match define table.");
		
		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 SourceModeIndex = static_cast<uint32>(PermutationVector.Get<FGeneralFunctionSet>());
		OutEnvironment.SetDefine(ShaderSourceModeDefineName[SourceModeIndex], 1u);
	}
};

IMPLEMENT_GLOBAL_SHADER(FGeneralFunctionShader, "/OoOShaders/BasicFunction.usf", "GeneralFunctionSet", SF_Compute);