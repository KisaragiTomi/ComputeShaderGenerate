#pragma once
//
#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Components/BoxComponent.h"
#include "ComputeShaderSceneCapture.h"
#include "ShaderParameterStruct.h"

#include "ComputeShaderMeshFill.generated.h"
//
//
#define NUM_THREADS_PER_GROUP_DIMENSION_X 32
#define NUM_THREADS_PER_GROUP_DIMENSION_Y 32
#define NUM_THREADS_PER_GROUP_DIMENSION_Z 1
#define NUM_SAMPLE_MESH_THREADS_PER_GROUP_DIMENSION_X 16
#define NUM_SAMPLE_MESH_THREADS_PER_GROUP_DIMENSION_Y 16
#define NUM_SAMPLE_MESH_THREADS_PER_GROUP_DIMENSION_Z 1
#define SHAREGROUP_FINDEXT_SIZE 256



class FMeshFillMult : public FGlobalShader
{
public:
		
	enum class EMeshFillFunction : uint8
	{
		MF_Init,
		MF_InitTargetHeight,
		MF_General,
		MF_FillVerticalRock,
		MF_FillCappingRock,
		MF_UpdateCurrentHeight,
		MF_FindBestPixel,
		MF_FindBestPixelRW_512,
		MF_FindBestPixelRW_256,
		MF_TargetHeight,
		MF_ExtentGenerateMask,
		MF_FilterResultExtent,
		MF_FilterResultReduce,
		MF_Deduplication,
		MF_UpdateCurrentHeightMult,
		MAX
	};
	class FMeshFillFunction : SHADER_PERMUTATION_ENUM_CLASS("FMESHFILL", EMeshFillFunction);
	using FPermutationDomain = TShaderPermutationDomain<FMeshFillFunction>;

	static TShaderMapRef<FMeshFillMult> CreateMeshFillPermutation(EMeshFillFunction Permutation)
	{
		typename FMeshFillMult::FPermutationDomain PermutationVector;
		PermutationVector.Set<FMeshFillMult::FMeshFillFunction>(Permutation);
		TShaderMapRef<FMeshFillMult> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);
		return ComputeShader;
	}
	//Declare this class as a global shader
	DECLARE_GLOBAL_SHADER(FMeshFillMult);
	//Tells the engine that this shader uses a structure for its parameters
	SHADER_USE_PARAMETER_STRUCT(FMeshFillMult, FGlobalShader);
	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_SceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_SceneNormal)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_ObjectNormal)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_TMeshDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_Result)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_TargetHeight)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_CurrentSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_ObjectDepth)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, T_HeightNormal)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2DArray, TA_MeshHeight)
		
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_CurrentSceneDepth)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_CurrentSceneDepthA)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_CurrentSceneDepthB)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_TargetHeight)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_TempA)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_TempB)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_DebugView)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_Result)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_ResultA)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_ResultB)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_FilterResult)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_SaveRotateScale)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_Deduplication)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RW_FilterResulteMult)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWTexture2D<float4>, RW_FindPixelBuffer)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWTexture2D<uint2>, RW_FindPixelBufferResult_Number)
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWTexture2D<uint>, RW_FindPixelBufferResult_NumberCount)

	
		SHADER_PARAMETER(float, RandomRotator)
		SHADER_PARAMETER(float, Size)
		SHADER_PARAMETER(float, HeightOffset)
		SHADER_PARAMETER(int, SelectIndex)
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
			TEXT("FMESHFILL_INIT"),
			TEXT("FMESHFILL_INITTARGETHEIGHT"),
			TEXT("FMESHFILL_GENERAL"),
			TEXT("FMESHFILL_FILLVERTICALROCK"),
			TEXT("FMESHFILL_FILLCAPPINGROCK"),
			TEXT("FMESHFILL_UPDATECURRENTHEIGHT"),
			TEXT("FMESHFILL_FINDBESTPIXEL"),
			TEXT("FMESHFILL_FINDBESTPIXELRW_512"),
			TEXT("FMESHFILL_FINDBESTPIXELRW_256"),
			TEXT("FMESHFILL_TARGETHEIGHT"),
			TEXT("FMESHFILL_EXTENTGENERATEMASK"),
			TEXT("FMESHFILL_FILTERRESULTEXTENT"),
			TEXT("FMESHFILL_FILTERRESULTREDUCE"),
			TEXT("FMESHFILL_DEDUPLICATION"),
			TEXT("FMESHFILL_UPDATECURRENTHEIGHTMULT"),
		}; 
		static_assert(UE_ARRAY_COUNT(ShaderSourceModeDefineName) == (uint32)EMeshFillFunction::MAX, "Enum doesn't match define table.");

		const FPermutationDomain PermutationVector(Parameters.PermutationId);
		const uint32 SourceModeIndex = static_cast<uint32>(PermutationVector.Get<FMeshFillFunction>());
		OutEnvironment.SetDefine(ShaderSourceModeDefineName[SourceModeIndex], 1u);

		OutEnvironment.SetDefine(TEXT("MAX_HEIGHT"), 10000);
		//OutEnvironment.SetDefine(TEXT("FINDPIXELTHREADSIZE"), 256);

		if (PermutationVector.Get<FMeshFillFunction>() == EMeshFillFunction::MF_FillVerticalRock)
		{
			OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), NUM_SAMPLE_MESH_THREADS_PER_GROUP_DIMENSION_X);
			OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), NUM_SAMPLE_MESH_THREADS_PER_GROUP_DIMENSION_Y);
		}
		if (PermutationVector.Get<FMeshFillFunction>() == EMeshFillFunction::MF_FindBestPixel)
		{
			OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), SHAREGROUP_FINDEXT_SIZE);
			OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), 1);
		}
		if (PermutationVector.Get<FMeshFillFunction>() == EMeshFillFunction::MF_FindBestPixelRW_512)
		{
			OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), 512 * 512 / SHAREGROUP_FINDEXT_SIZE);
			OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), 1);
		}
		if (PermutationVector.Get<FMeshFillFunction>() == EMeshFillFunction::MF_FindBestPixelRW_256)
		{
			OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_X"), 16);
			OutEnvironment.SetDefine(TEXT("THREADGROUPSIZE_Y"), 16);
		}
		if (PermutationVector.Get<FMeshFillFunction>() == EMeshFillFunction::MF_UpdateCurrentHeightMult)
		{
			OutEnvironment.SetDefine(TEXT("SHARETEXTURESIZE"), 32);
		}
	}
};

IMPLEMENT_GLOBAL_SHADER(FMeshFillMult, "/OoOShaders/MeshFill.usf", "MeshFillMult", SF_Compute);

USTRUCT(BlueprintType)
struct COMPUTESHADERGENERATOR_API FCSGenerateParameter
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTexture2D* TMeshDepth;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* Mask;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* SceneDepth;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* DebugView;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* Result;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* SceneNormal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	UTextureRenderTarget2D* CurrentSceneDepth;


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float Size = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Options)
	float RandomRoation = 0;

	bool IsValid()
	{
		return TMeshDepth != nullptr && SceneDepth != nullptr &&
			SceneNormal != nullptr && DebugView != nullptr && Result != nullptr;
	}
	bool IsValidMult()
	{
		return TMeshDepth != nullptr && SceneDepth != nullptr &&
			SceneNormal != nullptr && DebugView != nullptr && Result != nullptr ;
	}
};

UCLASS()
class COMPUTESHADERGENERATOR_API UComputeShaderMeshFillFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	// UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	// static void CSMeshFill(ACSGenerateCaptureScene* Capturer, UStaticMesh* StaticMesh, UTextureRenderTarget2D* DubugView, UTextureRenderTarget2D*
	//                        Result, UTexture2D* TMeshDepth, float SpawnSize = 1, float TestSizeScale = 1, FName Tag = FName(TEXT("Auto")));

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void CSMeshFillMult(ACSGenerateCaptureScene* Capturer, UStaticMesh* StaticMesh, UTextureRenderTarget2D* DubugView, UTextureRenderTarget2D*
	                           Result, UTextureRenderTarget2D* CurrentSceneDepth, UTexture2D* TMeshDepth, int32 Iteration = 100, float SpawnSize = 1, float
	                           TestSizeScale = 1, FName Tag = FName(TEXT("Auto")));

	
	// UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	// static void CalculateMeshLoctionAndRotation(FCSGenerateParameter Params);

	UFUNCTION(BlueprintCallable, Category = "ComputeShader")
	static void CalculateMeshLoctionAndRotationMult(FCSGenerateParameter Params, int32 NumIteraion);


};

