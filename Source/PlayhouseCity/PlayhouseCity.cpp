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

		// BaseColor (Vector)
		UMaterialExpressionVectorParameter* BaseColorParam =
			NewObject<UMaterialExpressionVectorParameter>(Mat);
		BaseColorParam->ParameterName = TEXT("BaseColor");
		BaseColorParam->DefaultValue = FLinearColor(1.f, 1.f, 1.f, 1.f);

		UMaterialEditorOnlyData* EditorData = Mat->GetEditorOnlyData();
		EditorData->ExpressionCollection.Expressions.Add(BaseColorParam);
		EditorData->BaseColor.Expression = BaseColorParam;

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

#endif
}

void FPlayhouseCityModule::ShutdownModule()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PlayhouseCity module shutting down"));
}
