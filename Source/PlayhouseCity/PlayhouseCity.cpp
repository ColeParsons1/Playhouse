// Copyright PlayhouseCity. All Rights Reserved.

#include "PlayhouseCity.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionCameraPositionWS.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionScreenPosition.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialExpressionTime.h"
#include "MaterialDomain.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "HAL/PlatformFileManager.h"
#endif

DEFINE_LOG_CATEGORY(LogPlayhouseCity);

IMPLEMENT_PRIMARY_GAME_MODULE(FPlayhouseCityModule, PlayhouseCity, "PlayhouseCity");

void FPlayhouseCityModule::StartupModule()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PlayhouseCity module starting up"));

#if WITH_EDITOR
	// ---------------------------------------------------------------------------
	// Animal Crossing-style world curvature — adds WorldPositionOffset nodes
	// that bend vertices downward based on XY distance from camera.
	// ---------------------------------------------------------------------------
	auto AddCurvatureWPO = [](UMaterial* Mat, UMaterialEditorOnlyData* EditorData)
	{
		// Vertex absolute world position
		auto* WorldPos = NewObject<UMaterialExpressionWorldPosition>(Mat);
		EditorData->ExpressionCollection.Expressions.Add(WorldPos);

		// Camera world position
		auto* CamPos = NewObject<UMaterialExpressionCameraPositionWS>(Mat);
		EditorData->ExpressionCollection.Expressions.Add(CamPos);

		// Subtract: vertex - camera = offset vector
		auto* Sub = NewObject<UMaterialExpressionSubtract>(Mat);
		Sub->A.Expression = WorldPos;
		Sub->B.Expression = CamPos;
		EditorData->ExpressionCollection.Expressions.Add(Sub);

		// Mask to XY only (ignore Z difference)
		auto* MaskXY = NewObject<UMaterialExpressionComponentMask>(Mat);
		MaskXY->Input.Expression = Sub;
		MaskXY->R = true;  MaskXY->G = true;
		MaskXY->B = false; MaskXY->A = false;
		EditorData->ExpressionCollection.Expressions.Add(MaskXY);

		// Dot product XY . XY = distance squared
		auto* Dot = NewObject<UMaterialExpressionDotProduct>(Mat);
		Dot->A.Expression = MaskXY;
		Dot->B.Expression = MaskXY;
		EditorData->ExpressionCollection.Expressions.Add(Dot);

		// Curvature constant (negative = curve downward)
		auto* CurveK = NewObject<UMaterialExpressionConstant>(Mat);
		CurveK->R = -0.000020f;
		EditorData->ExpressionCollection.Expressions.Add(CurveK);

		// Multiply: dist squared * curvature = Z drop (scalar)
		auto* MulDrop = NewObject<UMaterialExpressionMultiply>(Mat);
		MulDrop->A.Expression = Dot;
		MulDrop->B.Expression = CurveK;
		EditorData->ExpressionCollection.Expressions.Add(MulDrop);

		// Unit Z vector (0, 0, 1)
		auto* UnitZ = NewObject<UMaterialExpressionConstant3Vector>(Mat);
		UnitZ->Constant = FLinearColor(0.f, 0.f, 1.f, 0.f);
		EditorData->ExpressionCollection.Expressions.Add(UnitZ);

		// Multiply scalar drop * (0,0,1) = (0, 0, Z_drop) offset vector
		auto* MulFinal = NewObject<UMaterialExpressionMultiply>(Mat);
		MulFinal->A.Expression = UnitZ;
		MulFinal->B.Expression = MulDrop;
		EditorData->ExpressionCollection.Expressions.Add(MulFinal);

		// Connect to WorldPositionOffset
		EditorData->WorldPositionOffset.Expression = MulFinal;
	};

	// ---------------------------------------------------------------------------
	// Patch existing materials: disconnect WorldPositionOffset (remove curvature)
	// ---------------------------------------------------------------------------
	auto RemoveCurvatureFromMaterial = [](const FString& PackagePath, const FString& AssetName)
	{
		const FString ObjectPath = PackagePath + TEXT(".") + AssetName;
		UMaterial* Mat = LoadObject<UMaterial>(nullptr, *ObjectPath);
		if (!Mat) return;
		UMaterialEditorOnlyData* EditorData = Mat->GetEditorOnlyData();
		if (!EditorData || !EditorData->WorldPositionOffset.Expression) return;

		EditorData->WorldPositionOffset.Expression = nullptr;
		Mat->PreEditChange(nullptr);
		Mat->PostEditChange();

		const FString FilePath = FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Mat->GetPackage(), Mat, *FilePath, SaveArgs);

		UE_LOG(LogPlayhouseCity, Log, TEXT("PHC: Removed curvature WPO from %s"), *AssetName);
	};

	RemoveCurvatureFromMaterial(TEXT("/Game/Materials/M_PHCColor"), TEXT("M_PHCColor"));
	RemoveCurvatureFromMaterial(TEXT("/Game/Materials/M_PHCGlass"), TEXT("M_PHCGlass"));
	RemoveCurvatureFromMaterial(TEXT("/Game/Materials/M_PHCClearCoat"), TEXT("M_PHCClearCoat"));
	RemoveCurvatureFromMaterial(TEXT("/Game/Materials/M_PHCSkin"), TEXT("M_PHCSkin"));

	// ---------------------------------------------------------------------------
	// M_PHCColor -- opaque PBR material with BaseColor, Metallic, Roughness,
	// and EmissiveColor parameters.  Only created if the .uasset does not exist.
	// ---------------------------------------------------------------------------
	{
		const FString PackagePath = TEXT("/Game/Materials/M_PHCColor");
		const FString FilePath = FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension());
		if (!FPaths::FileExists(FilePath))
		{

		// Ensure the Content/Materials directory exists.
		const FString DirPath = FPaths::GetPath(FilePath);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*DirPath);

		UPackage* Package = CreatePackage(*PackagePath);
		UMaterial* Mat = NewObject<UMaterial>(Package, TEXT("M_PHCColor"),
			RF_Public | RF_Standalone);

		UMaterialEditorOnlyData* EditorData = Mat->GetEditorOnlyData();

		// BaseColor (Vector)
		UMaterialExpressionVectorParameter* BaseColorParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		BaseColorParam->ParameterName = TEXT("BaseColor");
		BaseColorParam->DefaultValue = FLinearColor(1.f, 1.f, 1.f, 1.f);
		EditorData->ExpressionCollection.Expressions.Add(BaseColorParam);

		// Metallic (Scalar)
		UMaterialExpressionScalarParameter* MetallicParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		MetallicParam->ParameterName = TEXT("Metallic");
		MetallicParam->DefaultValue = 0.0f;
		EditorData->ExpressionCollection.Expressions.Add(MetallicParam);
		EditorData->Metallic.Expression = MetallicParam;

		// Roughness (Scalar)
		UMaterialExpressionScalarParameter* RoughnessParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		RoughnessParam->ParameterName = TEXT("Roughness");
		RoughnessParam->DefaultValue = 0.5f;
		EditorData->ExpressionCollection.Expressions.Add(RoughnessParam);

		// EmissiveColor (Vector)
		UMaterialExpressionVectorParameter* EmissiveParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		EmissiveParam->ParameterName = TEXT("EmissiveColor");
		EmissiveParam->DefaultValue = FLinearColor::Black;
		EditorData->ExpressionCollection.Expressions.Add(EmissiveParam);
		EditorData->EmissiveColor.Expression = EmissiveParam;

		// --- World Position node (input to procedural detail shader) ---
		auto* WorldPosNode = NewObject<UMaterialExpressionWorldPosition>(Mat);
		EditorData->ExpressionCollection.Expressions.Add(WorldPosNode);

		// --- Procedural surface detail (Custom HLSL) ---
		// Pure math — no texture dependencies, iOS Metal compatible
		// Outputs: RGB = modified BaseColor, A unused
		// Normal output via separate additional output
		auto* DetailNode = NewObject<UMaterialExpressionCustom>(Mat);
		DetailNode->OutputType = CMOT_Float3;
		DetailNode->Description = TEXT("ProceduralSurfaceDetail");

		// Inputs
		DetailNode->Inputs.Empty();
		{
			FCustomInput In;
			In.InputName = TEXT("WorldPos");
			In.Input.Expression = WorldPosNode;
			DetailNode->Inputs.Add(In);
		}
		{
			FCustomInput In;
			In.InputName = TEXT("BaseCol");
			In.Input.Expression = BaseColorParam;
			DetailNode->Inputs.Add(In);
		}
		{
			FCustomInput In;
			In.InputName = TEXT("Rough");
			In.Input.Expression = RoughnessParam;
			DetailNode->Inputs.Add(In);
		}

		// Additional outputs: Normal (index 1), ModRoughness (index 2)
		DetailNode->AdditionalOutputs.Empty();
		{
			FCustomOutput Out;
			Out.OutputName = TEXT("Normal");
			Out.OutputType = CMOT_Float3;
			DetailNode->AdditionalOutputs.Add(Out);
		}
		{
			FCustomOutput Out;
			Out.OutputName = TEXT("ModRoughness");
			Out.OutputType = CMOT_Float1;
			DetailNode->AdditionalOutputs.Add(Out);
		}

		DetailNode->Code = TEXT(
			"// Hash function (Metal-safe, no texture lookups)\n"
			"float3 WP = WorldPos * 0.01;\n"
			"\n"
			"// Multi-octave hash noise for surface micro-detail\n"
			"float2 p1 = floor(WP.xy * 6.0);\n"
			"float2 p2 = floor(WP.xy * 14.0);\n"
			"float2 p3 = floor(WP.xy * 30.0);\n"
			"float2 f1 = frac(WP.xy * 6.0);\n"
			"float2 f2 = frac(WP.xy * 14.0);\n"
			"float2 f3 = frac(WP.xy * 30.0);\n"
			"f1 = f1 * f1 * (3.0 - 2.0 * f1);\n"
			"f2 = f2 * f2 * (3.0 - 2.0 * f2);\n"
			"f3 = f3 * f3 * (3.0 - 2.0 * f3);\n"
			"\n"
			"float h00 = frac(sin(dot(p1, float2(127.1, 311.7))) * 43758.5453);\n"
			"float h10 = frac(sin(dot(p1 + float2(1,0), float2(127.1, 311.7))) * 43758.5453);\n"
			"float h01 = frac(sin(dot(p1 + float2(0,1), float2(127.1, 311.7))) * 43758.5453);\n"
			"float h11 = frac(sin(dot(p1 + float2(1,1), float2(127.1, 311.7))) * 43758.5453);\n"
			"float n1 = lerp(lerp(h00, h10, f1.x), lerp(h01, h11, f1.x), f1.y);\n"
			"\n"
			"float m00 = frac(sin(dot(p2, float2(269.5, 183.3))) * 43758.5453);\n"
			"float m10 = frac(sin(dot(p2 + float2(1,0), float2(269.5, 183.3))) * 43758.5453);\n"
			"float m01 = frac(sin(dot(p2 + float2(0,1), float2(269.5, 183.3))) * 43758.5453);\n"
			"float m11 = frac(sin(dot(p2 + float2(1,1), float2(269.5, 183.3))) * 43758.5453);\n"
			"float n2 = lerp(lerp(m00, m10, f2.x), lerp(m01, m11, f2.x), f2.y);\n"
			"\n"
			"float s00 = frac(sin(dot(p3, float2(419.2, 371.9))) * 43758.5453);\n"
			"float s10 = frac(sin(dot(p3 + float2(1,0), float2(419.2, 371.9))) * 43758.5453);\n"
			"float s01 = frac(sin(dot(p3 + float2(0,1), float2(419.2, 371.9))) * 43758.5453);\n"
			"float s11 = frac(sin(dot(p3 + float2(1,1), float2(419.2, 371.9))) * 43758.5453);\n"
			"float n3 = lerp(lerp(s00, s10, f3.x), lerp(s01, s11, f3.x), f3.y);\n"
			"\n"
			"// Normal perturbation (tangent space)\n"
			"float nx = (n1 - 0.5) * 0.25 + (n2 - 0.5) * 0.15 + (n3 - 0.5) * 0.08;\n"
			"float ny = (frac(n1 * 7.13) - 0.5) * 0.25 + (frac(n2 * 13.37) - 0.5) * 0.15;\n"
			"Normal = float3(nx, ny, 1.0);\n"
			"\n"
			"// Roughness micro-variation\n"
			"ModRoughness = Rough + (n2 - 0.5) * 0.12;\n"
			"\n"
			"// Color micro-variation (subtle per-pixel noise breaks flat look)\n"
			"float colorNoise = (n1 - 0.5) * 0.06 + (n3 - 0.5) * 0.03;\n"
			"float3 modColor = BaseCol + float3(colorNoise, colorNoise * 1.2, colorNoise * 0.5);\n"
			"return max(modColor, float3(0.0, 0.0, 0.0));\n"
		);

		EditorData->ExpressionCollection.Expressions.Add(DetailNode);

		// Connect outputs
		EditorData->BaseColor.Expression = DetailNode;  // output 0: modified color
		EditorData->Normal.Expression = DetailNode;
		EditorData->Normal.OutputIndex = 1;  // additional output "Normal"
		EditorData->Roughness.Expression = DetailNode;
		EditorData->Roughness.OutputIndex = 2;  // additional output "ModRoughness"

		AddCurvatureWPO(Mat, EditorData);

		Mat->PreEditChange(nullptr);
		Mat->PostEditChange();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, Mat, *FilePath, SaveArgs);

		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHC: Created and saved M_PHCColor material to %s"), *FilePath);
		}
	}

	// ---------------------------------------------------------------------------
	// M_PHCGlass -- translucent material for glass and water surfaces.
	// ---------------------------------------------------------------------------
	{
		const FString PackagePath = TEXT("/Game/Materials/M_PHCGlass");
		const FString FilePath = FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension());
		if (!FPaths::FileExists(FilePath))
		{
		// Ensure the Content/Materials directory exists.
		const FString DirPath = FPaths::GetPath(FilePath);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*DirPath);

		UPackage* Package = CreatePackage(*PackagePath);
		UMaterial* Mat = NewObject<UMaterial>(Package, TEXT("M_PHCGlass"),
			RF_Public | RF_Standalone);

		Mat->BlendMode = BLEND_Translucent;
		Mat->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;

		UMaterialEditorOnlyData* EditorData = Mat->GetEditorOnlyData();

		// BaseColor (Vector)
		UMaterialExpressionVectorParameter* BaseColorParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		BaseColorParam->ParameterName = TEXT("BaseColor");
		BaseColorParam->DefaultValue = FLinearColor(0.1f, 0.15f, 0.2f, 1.f);
		EditorData->ExpressionCollection.Expressions.Add(BaseColorParam);
		EditorData->BaseColor.Expression = BaseColorParam;

		// Opacity (Scalar)
		UMaterialExpressionScalarParameter* OpacityParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		OpacityParam->ParameterName = TEXT("Opacity");
		OpacityParam->DefaultValue = 0.3f;
		EditorData->ExpressionCollection.Expressions.Add(OpacityParam);
		EditorData->Opacity.Expression = OpacityParam;

		// Metallic (Scalar)
		UMaterialExpressionScalarParameter* MetallicParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		MetallicParam->ParameterName = TEXT("Metallic");
		MetallicParam->DefaultValue = 0.0f;
		EditorData->ExpressionCollection.Expressions.Add(MetallicParam);
		EditorData->Metallic.Expression = MetallicParam;

		// Roughness (Scalar)
		UMaterialExpressionScalarParameter* RoughnessParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		RoughnessParam->ParameterName = TEXT("Roughness");
		RoughnessParam->DefaultValue = 0.05f;
		EditorData->ExpressionCollection.Expressions.Add(RoughnessParam);
		EditorData->Roughness.Expression = RoughnessParam;

		// EmissiveColor (Vector)
		UMaterialExpressionVectorParameter* EmissiveParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		EmissiveParam->ParameterName = TEXT("EmissiveColor");
		EmissiveParam->DefaultValue = FLinearColor::Black;
		EditorData->ExpressionCollection.Expressions.Add(EmissiveParam);
		EditorData->EmissiveColor.Expression = EmissiveParam;

		AddCurvatureWPO(Mat, EditorData);

		Mat->PreEditChange(nullptr);
		Mat->PostEditChange();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, Mat, *FilePath, SaveArgs);

		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHC: Created and saved M_PHCGlass material to %s"), *FilePath);
		}
	}

	// ---------------------------------------------------------------------------
	// M_PHCClearCoat -- automotive clear coat paint for vehicle bodies.
	// ClearCoat shading model: glossy topcoat over metallic paint.
	// ---------------------------------------------------------------------------
	{
		const FString PackagePath = TEXT("/Game/Materials/M_PHCClearCoat");
		const FString FilePath = FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension());
		if (!FPaths::FileExists(FilePath))
		{
		const FString DirPath = FPaths::GetPath(FilePath);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*DirPath);

		UPackage* Package = CreatePackage(*PackagePath);
		UMaterial* Mat = NewObject<UMaterial>(Package, TEXT("M_PHCClearCoat"),
			RF_Public | RF_Standalone);

		// Set ClearCoat shading model for automotive paint
		Mat->SetShadingModel(MSM_ClearCoat);

		UMaterialEditorOnlyData* EditorData = Mat->GetEditorOnlyData();

		// BaseColor (Vector) - paint color
		UMaterialExpressionVectorParameter* BaseColorParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		BaseColorParam->ParameterName = TEXT("BaseColor");
		BaseColorParam->DefaultValue = FLinearColor(0.8f, 0.1f, 0.1f, 1.f); // default red
		EditorData->ExpressionCollection.Expressions.Add(BaseColorParam);
		EditorData->BaseColor.Expression = BaseColorParam;

		// Metallic (Scalar) - high for metallic paint
		UMaterialExpressionScalarParameter* MetallicParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		MetallicParam->ParameterName = TEXT("Metallic");
		MetallicParam->DefaultValue = 0.9f;
		EditorData->ExpressionCollection.Expressions.Add(MetallicParam);
		EditorData->Metallic.Expression = MetallicParam;

		// Roughness (Scalar) - base layer roughness
		UMaterialExpressionScalarParameter* RoughnessParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		RoughnessParam->ParameterName = TEXT("Roughness");
		RoughnessParam->DefaultValue = 0.35f;
		EditorData->ExpressionCollection.Expressions.Add(RoughnessParam);
		EditorData->Roughness.Expression = RoughnessParam;

		// ClearCoat (Scalar) - strength of clear coat layer
		UMaterialExpressionScalarParameter* ClearCoatParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		ClearCoatParam->ParameterName = TEXT("ClearCoat");
		ClearCoatParam->DefaultValue = 1.0f;
		EditorData->ExpressionCollection.Expressions.Add(ClearCoatParam);
		EditorData->ClearCoat.Expression = ClearCoatParam;

		// ClearCoatRoughness (Scalar) - ultra-smooth topcoat
		UMaterialExpressionScalarParameter* ClearCoatRoughnessParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		ClearCoatRoughnessParam->ParameterName = TEXT("ClearCoatRoughness");
		ClearCoatRoughnessParam->DefaultValue = 0.03f;
		EditorData->ExpressionCollection.Expressions.Add(ClearCoatRoughnessParam);
		EditorData->ClearCoatRoughness.Expression = ClearCoatRoughnessParam;

		// EmissiveColor (Vector) - for headlight/taillight glow
		UMaterialExpressionVectorParameter* EmissiveParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		EmissiveParam->ParameterName = TEXT("EmissiveColor");
		EmissiveParam->DefaultValue = FLinearColor::Black;
		EditorData->ExpressionCollection.Expressions.Add(EmissiveParam);
		EditorData->EmissiveColor.Expression = EmissiveParam;

		AddCurvatureWPO(Mat, EditorData);

		Mat->PreEditChange(nullptr);
		Mat->PostEditChange();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, Mat, *FilePath, SaveArgs);

		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHC: Created and saved M_PHCClearCoat material to %s"), *FilePath);
		}
	}

	// ---------------------------------------------------------------------------
	// M_PHCSkin -- Subsurface skin material for realistic character rendering.
	// Warm SSS glow when backlit. Same parameters as M_PHCColor plus
	// SubsurfaceColor and Opacity for SSS control.
	// ---------------------------------------------------------------------------
	{
		const FString PackagePath = TEXT("/Game/Materials/M_PHCSkin");
		const FString FilePath = FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension());
		if (!FPaths::FileExists(FilePath))
		{
		const FString DirPath = FPaths::GetPath(FilePath);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*DirPath);

		UPackage* Package = CreatePackage(*PackagePath);
		UMaterial* Mat = NewObject<UMaterial>(Package, TEXT("M_PHCSkin"),
			RF_Public | RF_Standalone);

		Mat->SetShadingModel(MSM_Subsurface);

		UMaterialEditorOnlyData* EditorData = Mat->GetEditorOnlyData();

		UMaterialExpressionVectorParameter* BaseColorParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		BaseColorParam->ParameterName = TEXT("BaseColor");
		BaseColorParam->DefaultValue = FLinearColor(0.55f, 0.38f, 0.26f);
		EditorData->ExpressionCollection.Expressions.Add(BaseColorParam);
		EditorData->BaseColor.Expression = BaseColorParam;

		UMaterialExpressionScalarParameter* MetallicParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		MetallicParam->ParameterName = TEXT("Metallic");
		MetallicParam->DefaultValue = 0.0f;
		EditorData->ExpressionCollection.Expressions.Add(MetallicParam);
		EditorData->Metallic.Expression = MetallicParam;

		UMaterialExpressionScalarParameter* RoughnessParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		RoughnessParam->ParameterName = TEXT("Roughness");
		RoughnessParam->DefaultValue = 0.45f;
		EditorData->ExpressionCollection.Expressions.Add(RoughnessParam);
		EditorData->Roughness.Expression = RoughnessParam;

		UMaterialExpressionVectorParameter* SSParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		SSParam->ParameterName = TEXT("SubsurfaceColor");
		SSParam->DefaultValue = FLinearColor(0.9f, 0.2f, 0.1f);
		EditorData->ExpressionCollection.Expressions.Add(SSParam);
		EditorData->SubsurfaceColor.Expression = SSParam;

		UMaterialExpressionScalarParameter* OpacityParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		OpacityParam->ParameterName = TEXT("Opacity");
		OpacityParam->DefaultValue = 0.35f;
		EditorData->ExpressionCollection.Expressions.Add(OpacityParam);
		EditorData->Opacity.Expression = OpacityParam;

		UMaterialExpressionVectorParameter* EmissiveParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		EmissiveParam->ParameterName = TEXT("EmissiveColor");
		EmissiveParam->DefaultValue = FLinearColor::Black;
		EditorData->ExpressionCollection.Expressions.Add(EmissiveParam);
		EditorData->EmissiveColor.Expression = EmissiveParam;

		AddCurvatureWPO(Mat, EditorData);

		Mat->PreEditChange(nullptr);
		Mat->PostEditChange();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, Mat, *FilePath, SaveArgs);

		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHC: Created and saved M_PHCSkin material to %s"), *FilePath);
		}
	}

	// ---------------------------------------------------------------------------
	// M_PHCChromaAberration -- Post-process material for custom-colored chromatic
	// aberration.  Samples the scene at 3 radially-offset UVs and tints each
	// sample with a mandala palette color (magenta / blue / teal).
	// ---------------------------------------------------------------------------
	{
		const FString PackagePath = TEXT("/Game/Materials/M_PHCChromaAberration");
		const FString FilePath = FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension());
		if (!FPaths::FileExists(FilePath))
		{
		const FString DirPath = FPaths::GetPath(FilePath);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*DirPath);

		UPackage* Package = CreatePackage(*PackagePath);
		UMaterial* Mat = NewObject<UMaterial>(Package, TEXT("M_PHCChromaAberration"),
			RF_Public | RF_Standalone);

		Mat->MaterialDomain = MD_PostProcess;
		Mat->BlendableLocation = BL_AfterTonemapping;

		UMaterialEditorOnlyData* EditorData = Mat->GetEditorOnlyData();

		// --- Node 1: Screen UV ---
		auto* ScreenPos = NewObject<UMaterialExpressionScreenPosition>(Mat);
		EditorData->ExpressionCollection.Expressions.Add(ScreenPos);

		// --- Node 2: 0.5 constant (center offset) ---
		auto* Half = NewObject<UMaterialExpressionConstant>(Mat);
		Half->R = 0.5f;
		EditorData->ExpressionCollection.Expressions.Add(Half);

		// --- Node 3: Direction from center = UV - 0.5 ---
		auto* DirFromCenter = NewObject<UMaterialExpressionSubtract>(Mat);
		DirFromCenter->A.Expression = ScreenPos;
		DirFromCenter->B.Expression = Half;
		EditorData->ExpressionCollection.Expressions.Add(DirFromCenter);

		// --- Node 4: ChromaOffset scalar parameter (default 0 = no effect) ---
		auto* OffsetParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
		OffsetParam->ParameterName = TEXT("ChromaOffset");
		OffsetParam->DefaultValue = 0.0f;
		EditorData->ExpressionCollection.Expressions.Add(OffsetParam);

		// --- Node 5: UV shift = direction * offset ---
		auto* UVShift = NewObject<UMaterialExpressionMultiply>(Mat);
		UVShift->A.Expression = DirFromCenter;
		UVShift->B.Expression = OffsetParam;
		EditorData->ExpressionCollection.Expressions.Add(UVShift);

		// --- Node 6: UV_A = UV + shift (outer) ---
		auto* UV_A = NewObject<UMaterialExpressionAdd>(Mat);
		UV_A->A.Expression = ScreenPos;
		UV_A->B.Expression = UVShift;
		EditorData->ExpressionCollection.Expressions.Add(UV_A);

		// --- Node 7: UV_C = UV - shift (inner) ---
		auto* UV_C = NewObject<UMaterialExpressionSubtract>(Mat);
		UV_C->A.Expression = ScreenPos;
		UV_C->B.Expression = UVShift;
		EditorData->ExpressionCollection.Expressions.Add(UV_C);

		// --- Nodes 8-10: Scene texture samples at 3 UV positions ---
		auto* SampleA = NewObject<UMaterialExpressionSceneTexture>(Mat);
		SampleA->SceneTextureId = PPI_PostProcessInput0;
		SampleA->Coordinates.Expression = UV_A;
		EditorData->ExpressionCollection.Expressions.Add(SampleA);

		auto* SampleB = NewObject<UMaterialExpressionSceneTexture>(Mat);
		SampleB->SceneTextureId = PPI_PostProcessInput0;
		SampleB->Coordinates.Expression = ScreenPos;
		EditorData->ExpressionCollection.Expressions.Add(SampleB);

		auto* SampleC = NewObject<UMaterialExpressionSceneTexture>(Mat);
		SampleC->SceneTextureId = PPI_PostProcessInput0;
		SampleC->Coordinates.Expression = UV_C;
		EditorData->ExpressionCollection.Expressions.Add(SampleC);

		// --- Nodes 11-13: Tint color parameters ---
		auto* TintA = NewObject<UMaterialExpressionVectorParameter>(Mat);
		TintA->ParameterName = TEXT("TintA");
		TintA->DefaultValue = FLinearColor(1.f, 1.f, 1.f, 1.f);
		EditorData->ExpressionCollection.Expressions.Add(TintA);

		auto* TintB = NewObject<UMaterialExpressionVectorParameter>(Mat);
		TintB->ParameterName = TEXT("TintB");
		TintB->DefaultValue = FLinearColor(1.f, 1.f, 1.f, 1.f);
		EditorData->ExpressionCollection.Expressions.Add(TintB);

		auto* TintC = NewObject<UMaterialExpressionVectorParameter>(Mat);
		TintC->ParameterName = TEXT("TintC");
		TintC->DefaultValue = FLinearColor(1.f, 1.f, 1.f, 1.f);
		EditorData->ExpressionCollection.Expressions.Add(TintC);

		// --- Nodes 14-16: Multiply each sample by its tint ---
		auto* TintedA = NewObject<UMaterialExpressionMultiply>(Mat);
		TintedA->A.Expression = SampleA;
		TintedA->B.Expression = TintA;
		EditorData->ExpressionCollection.Expressions.Add(TintedA);

		auto* TintedB = NewObject<UMaterialExpressionMultiply>(Mat);
		TintedB->A.Expression = SampleB;
		TintedB->B.Expression = TintB;
		EditorData->ExpressionCollection.Expressions.Add(TintedB);

		auto* TintedC = NewObject<UMaterialExpressionMultiply>(Mat);
		TintedC->A.Expression = SampleC;
		TintedC->B.Expression = TintC;
		EditorData->ExpressionCollection.Expressions.Add(TintedC);

		// --- Nodes 17-18: Sum all three ---
		auto* SumAB = NewObject<UMaterialExpressionAdd>(Mat);
		SumAB->A.Expression = TintedA;
		SumAB->B.Expression = TintedB;
		EditorData->ExpressionCollection.Expressions.Add(SumAB);

		auto* SumABC = NewObject<UMaterialExpressionAdd>(Mat);
		SumABC->A.Expression = SumAB;
		SumABC->B.Expression = TintedC;
		EditorData->ExpressionCollection.Expressions.Add(SumABC);

		// --- Nodes 19-20: Average (divide by 3) ---
		auto* Third = NewObject<UMaterialExpressionConstant>(Mat);
		Third->R = 0.333333f;
		EditorData->ExpressionCollection.Expressions.Add(Third);

		auto* FinalColor = NewObject<UMaterialExpressionMultiply>(Mat);
		FinalColor->A.Expression = SumABC;
		FinalColor->B.Expression = Third;
		EditorData->ExpressionCollection.Expressions.Add(FinalColor);

		// --- Connect to emissive (post-process output) ---
		EditorData->EmissiveColor.Expression = FinalColor;

		Mat->PreEditChange(nullptr);
		Mat->PostEditChange();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, Mat, *FilePath, SaveArgs);

		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHC: Created and saved M_PHCChromaAberration material to %s"), *FilePath);
		}
	}

	// ---------------------------------------------------------------------------
	// M_PHCDecal -- Deferred decal material for projecting patterns onto surfaces.
	// ---------------------------------------------------------------------------
	{
		const FString PackagePath = TEXT("/Game/Materials/M_PHCDecal");
		const FString FilePath = FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension());
		if (!FPaths::FileExists(FilePath))
		{
		const FString DirPath = FPaths::GetPath(FilePath);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*DirPath);

		UPackage* Package = CreatePackage(*PackagePath);
		UMaterial* Mat = NewObject<UMaterial>(Package, TEXT("M_PHCDecal"),
			RF_Public | RF_Standalone);

		Mat->MaterialDomain = MD_DeferredDecal;
		Mat->DecalBlendMode = DBM_Translucent;

		UMaterialEditorOnlyData* EditorData = Mat->GetEditorOnlyData();

		// BaseColor
		UMaterialExpressionVectorParameter* BaseColorParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		BaseColorParam->ParameterName = TEXT("BaseColor");
		BaseColorParam->DefaultValue = FLinearColor(0.02f, 0.02f, 0.03f, 1.f);
		EditorData->ExpressionCollection.Expressions.Add(BaseColorParam);
		EditorData->BaseColor.Expression = BaseColorParam;

		// Opacity
		UMaterialExpressionScalarParameter* OpacityParam =
			NewObject<UMaterialExpressionScalarParameter>(Mat);
		OpacityParam->ParameterName = TEXT("Opacity");
		OpacityParam->DefaultValue = 0.85f;
		EditorData->ExpressionCollection.Expressions.Add(OpacityParam);
		EditorData->Opacity.Expression = OpacityParam;

		// EmissiveColor
		UMaterialExpressionVectorParameter* EmissiveParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		EmissiveParam->ParameterName = TEXT("EmissiveColor");
		EmissiveParam->DefaultValue = FLinearColor::Black;
		EditorData->ExpressionCollection.Expressions.Add(EmissiveParam);
		EditorData->EmissiveColor.Expression = EmissiveParam;

		Mat->PreEditChange(nullptr);
		Mat->PostEditChange();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, Mat, *FilePath, SaveArgs);

		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHC: Created and saved M_PHCDecal material to %s"), *FilePath);
		}
	}

	// ---------------------------------------------------------------------------
	// M_PHCTripSky -- Unlit Neon Circuitry fractal sky dome for mushroom trip.
	// Mandelbox fractal iteration (box fold + sphere fold), dual scene modes
	// (Alien Synthesis neon glow + Neon Glyphs ornamental grid), Persian paisley
	// background, post-processing (contrast, saturation 2.2x, vignette).
	// Rendered on inside of a sphere surrounding the player.
	// ---------------------------------------------------------------------------
	{
		const FString PackagePath = TEXT("/Game/Materials/M_PHCTripSky");
		const FString FilePath = FPackageName::LongPackageNameToFilename(
			PackagePath, FPackageName::GetAssetPackageExtension());
		if (!FPaths::FileExists(FilePath))
		{
		const FString DirPath = FPaths::GetPath(FilePath);
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.CreateDirectoryTree(*DirPath);

		UPackage* Package = CreatePackage(*PackagePath);
		UMaterial* Mat = NewObject<UMaterial>(Package, TEXT("M_PHCTripSky"),
			RF_Public | RF_Standalone);

		Mat->SetShadingModel(MSM_Unlit);
		Mat->TwoSided = true;

		UMaterialEditorOnlyData* EditorData = Mat->GetEditorOnlyData();

		// --- Node 1: Texture Coordinate (sphere UVs) ---
		auto* TexCoord = NewObject<UMaterialExpressionTextureCoordinate>(Mat);
		EditorData->ExpressionCollection.Expressions.Add(TexCoord);

		// --- Node 2: Time ---
		auto* TimeNode = NewObject<UMaterialExpressionTime>(Mat);
		EditorData->ExpressionCollection.Expressions.Add(TimeNode);

		// --- Scalar parameters for simulated audio ---
		auto* BassParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
		BassParam->ParameterName = TEXT("Bass");
		BassParam->DefaultValue = 0.0f;
		EditorData->ExpressionCollection.Expressions.Add(BassParam);

		auto* HighParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
		HighParam->ParameterName = TEXT("High");
		HighParam->DefaultValue = 0.0f;
		EditorData->ExpressionCollection.Expressions.Add(HighParam);

		auto* LevelParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
		LevelParam->ParameterName = TEXT("Level");
		LevelParam->DefaultValue = 0.5f;
		EditorData->ExpressionCollection.Expressions.Add(LevelParam);

		auto* MidParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
		MidParam->ParameterName = TEXT("Mid");
		MidParam->DefaultValue = 0.0f;
		EditorData->ExpressionCollection.Expressions.Add(MidParam);

		auto* SceneIndexParam = NewObject<UMaterialExpressionScalarParameter>(Mat);
		SceneIndexParam->ParameterName = TEXT("SceneIndex");
		SceneIndexParam->DefaultValue = 0.0f;
		EditorData->ExpressionCollection.Expressions.Add(SceneIndexParam);

		// --- Custom HLSL Neon Circuitry shader ---
		auto* CustomNode = NewObject<UMaterialExpressionCustom>(Mat);
		CustomNode->OutputType = CMOT_Float3;
		CustomNode->Description = TEXT("NeonCircuitryShader");

		// Define 7 inputs
		CustomNode->Inputs.Empty();
		{
			FCustomInput In;
			In.InputName = TEXT("UV");
			In.Input.Expression = TexCoord;
			CustomNode->Inputs.Add(In);
		}
		{
			FCustomInput In;
			In.InputName = TEXT("Time");
			In.Input.Expression = TimeNode;
			CustomNode->Inputs.Add(In);
		}
		{
			FCustomInput In;
			In.InputName = TEXT("Bass");
			In.Input.Expression = BassParam;
			CustomNode->Inputs.Add(In);
		}
		{
			FCustomInput In;
			In.InputName = TEXT("High");
			In.Input.Expression = HighParam;
			CustomNode->Inputs.Add(In);
		}
		{
			FCustomInput In;
			In.InputName = TEXT("Level");
			In.Input.Expression = LevelParam;
			CustomNode->Inputs.Add(In);
		}
		{
			FCustomInput In;
			In.InputName = TEXT("Mid");
			In.Input.Expression = MidParam;
			CustomNode->Inputs.Add(In);
		}
		{
			FCustomInput In;
			In.InputName = TEXT("SceneIndex");
			In.Input.Expression = SceneIndexParam;
			CustomNode->Inputs.Add(In);
		}

		// Full HLSL — Neon Circuitry translation from three.js
		// Mandelbox fractal (box fold + sphere fold), dual scenes, Persian BG
		CustomNode->Code = TEXT(
			"float time = Time * 0.5;\n"
			"float2 uv = UV * 2.0 - 1.0;\n"
			"uv.x *= 2.0;\n"
			"\n"
			"// --- Noise functions ---\n"
			"float h1 = frac(sin(dot(uv * 200.0, float2(12.9898, 78.233))) * 43758.5453);\n"
			"float h2 = frac(sin(dot(uv * 400.0 + 7.0, float2(39.346, 11.135))) * 43758.5453);\n"
			"\n"
			"// --- Layered nebula clouds (FBM-style) ---\n"
			"float3 nebula = float3(0.0, 0.0, 0.0);\n"
			"float nebAmp = 0.5;\n"
			"float2 nebUV = uv * 1.2;\n"
			"for (int n = 0; n < 5; n++)\n"
			"{\n"
			"    float fn = float(n);\n"
			"    float2 p = nebUV + time * 0.02 * float2(sin(fn), cos(fn * 0.7));\n"
			"    float2 ip = floor(p); float2 fp = frac(p);\n"
			"    fp = fp * fp * (3.0 - 2.0 * fp);\n"
			"    float a = frac(sin(dot(ip, float2(127.1, 311.7))) * 43758.5453);\n"
			"    float b = frac(sin(dot(ip + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);\n"
			"    float c = frac(sin(dot(ip + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);\n"
			"    float d = frac(sin(dot(ip + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);\n"
			"    float noise = lerp(lerp(a, b, fp.x), lerp(c, d, fp.x), fp.y);\n"
			"    // Galaxy palette: deep purple, teal, magenta, warm gold\n"
			"    float3 nebCol = 0.5 + 0.5 * cos(6.28318 * (noise * 0.8 + fn * 0.15\n"
			"        + float3(0.6, 0.35, 0.75) + time * 0.02));\n"
			"    nebula += nebCol * noise * nebAmp;\n"
			"    nebAmp *= 0.55;\n"
			"    nebUV *= 2.1;\n"
			"}\n"
			"nebula *= 0.03;\n"
			"\n"
			"// --- Mandelbox fractal (primary glow structure) ---\n"
			"float2 suv = abs(uv);\n"
			"suv -= 0.5;\n"
			"float ra0 = time * 0.04;\n"
			"float rs0 = sin(ra0); float rc0 = cos(ra0);\n"
			"suv = float2(rc0 * suv.x - rs0 * suv.y, rs0 * suv.x + rc0 * suv.y);\n"
			"float2 uv0 = suv;\n"
			"float sc = 2.0 + 0.08 * sin(time * 0.08);\n"
			"\n"
			"float3 fractalCol = float3(0.0, 0.0, 0.0);\n"
			"float2 suv1 = suv;\n"
			"for (int i = 0; i < 7; i++)\n"
			"{\n"
			"    suv1 = clamp(suv1, -1.0, 1.0) * 2.0 - suv1;\n"
			"    float r2 = dot(suv1, suv1);\n"
			"    float sphereFold = lerp(1.0, 1.0 / max(r2, 0.001), step(0.25, r2) - step(1.0, r2));\n"
			"    sphereFold = lerp(sphereFold, 4.0, 1.0 - step(0.25, r2));\n"
			"    suv1 *= sphereFold * sc;\n"
			"    float fi = float(i);\n"
			"    float ira = 0.15 + fi * 0.02 * sin(time * 0.08);\n"
			"    float irs = sin(ira); float irc = cos(ira);\n"
			"    suv1 = float2(irc * suv1.x - irs * suv1.y, irs * suv1.x + irc * suv1.y);\n"
			"    suv1 += float2(0.4 * sin(time * 0.08), 0.4 * cos(time * 0.11));\n"
			"    float d = length(suv1);\n"
			"    float pt = length(uv0) + fi * 0.3 + time * 0.06;\n"
			"    // Galaxy palette: blue-violet core, teal edges, warm highlights\n"
			"    float3 col1 = 0.5 + 0.5 * cos(6.28318 * (pt * 0.8\n"
			"        + float3(0.55, 0.35, 0.75)));\n"
			"    // Soft volumetric glow (wider falloff, less harsh)\n"
			"    float pat = sin(d * (5.0 + Level * 5.0) + time * 0.06) / 8.0;\n"
			"    float brightness = 0.005 + Bass * 0.002;\n"
			"    float glow = brightness / (pat * pat + 0.003);\n"
			"    glow = min(glow, 3.0);\n"
			"    fractalCol += col1 * glow * exp(-fi * 0.22);\n"
			"}\n"
			"fractalCol *= 0.08;\n"
			"\n"
			"// --- Star field layer ---\n"
			"float stars = 0.0;\n"
			"float2 starUV = uv * 30.0;\n"
			"float2 starID = floor(starUV);\n"
			"float2 starF = frac(starUV) - 0.5;\n"
			"float starRnd = frac(sin(dot(starID, float2(12.9898, 78.233))) * 43758.5453);\n"
			"float starBright = step(0.92, starRnd);\n"
			"float2 starOff = float2(frac(starRnd * 127.1) - 0.5, frac(starRnd * 311.7) - 0.5) * 0.7;\n"
			"float starDist = length(starF - starOff);\n"
			"float starGlow = starBright * (0.003 / (starDist * starDist + 0.003));\n"
			"starGlow *= 0.5 + 0.5 * sin(time * 2.0 + starRnd * 100.0);\n"
			"float3 starCol = lerp(float3(0.8, 0.85, 1.0), float3(1.0, 0.9, 0.7), starRnd);\n"
			"\n"
			"// --- Second star layer (finer) ---\n"
			"float2 starUV2 = uv * 80.0;\n"
			"float2 starID2 = floor(starUV2);\n"
			"float2 starF2 = frac(starUV2) - 0.5;\n"
			"float starRnd2 = frac(sin(dot(starID2, float2(39.346, 11.135))) * 43758.5453);\n"
			"float starBright2 = step(0.95, starRnd2);\n"
			"float2 starOff2 = float2(frac(starRnd2 * 127.1) - 0.5, frac(starRnd2 * 311.7) - 0.5) * 0.6;\n"
			"float starDist2 = length(starF2 - starOff2);\n"
			"float starGlow2 = starBright2 * (0.001 / (starDist2 * starDist2 + 0.001));\n"
			"\n"
			"// --- Combine all layers ---\n"
			"float3 col = nebula;\n"
			"col += fractalCol;\n"
			"col += starCol * starGlow * 0.2;\n"
			"col += float3(0.7, 0.8, 1.0) * starGlow2 * 0.08;\n"
			"\n"
			"// Subtle dithering to eliminate banding\n"
			"col += (h1 - 0.5) * 0.008;\n"
			"\n"
			"// Filmic tone mapping (ACES-like)\n"
			"float3 x = max(col, 0.0);\n"
			"col = (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14);\n"
			"\n"
			"// Slight warm shift in shadows, cool in highlights\n"
			"float lum = dot(col, float3(0.299, 0.587, 0.114));\n"
			"col = lerp(col * float3(1.05, 0.95, 1.1), col, smoothstep(0.0, 0.3, lum));\n"
			"\n"
			"// Final output\n"
			"col *= 0.35;\n"
			"return max(col, float3(0.0, 0.0, 0.0));\n"
		);

		EditorData->ExpressionCollection.Expressions.Add(CustomNode);

		// Connect custom node output → EmissiveColor
		EditorData->EmissiveColor.Expression = CustomNode;

		Mat->PreEditChange(nullptr);
		Mat->PostEditChange();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, Mat, *FilePath, SaveArgs);

		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHC: Created and saved M_PHCTripSky material to %s"), *FilePath);
		}
	}

#endif
}

void FPlayhouseCityModule::ShutdownModule()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PlayhouseCity module shutting down"));
}
