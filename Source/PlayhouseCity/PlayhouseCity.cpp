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
			"float2 rawUV = UV * 2.0 - 1.0;\n"
			"\n"
			"// --- Persian paisley background ---\n"
			"float2 bgp = rawUV * 2.5 + time * 0.05;\n"
			"float2 bgID = floor(bgp);\n"
			"float2 bgF = frac(bgp) - 0.5;\n"
			"float bgRipple = sin(length(rawUV) * 5.0 - time * 0.5);\n"
			"bgF += bgF * bgRipple * 0.05;\n"
			"float bgR = length(bgF);\n"
			"float bgA = atan2(bgF.y, bgF.x);\n"
			"float bgTwist = sin(bgR * 8.0 - time * 0.5);\n"
			"float bgShape = sin(bgA + bgR * 6.0 + bgTwist);\n"
			"float bgPat = smoothstep(0.1, 0.0, abs(bgShape * bgR - 0.1));\n"
			"float bgT = length(bgID) * 0.1 + time * 0.1;\n"
			"float3 bgCol = (0.5 + 0.5 * cos(6.28318 * (float3(1.0, 1.0, 0.5) * bgT\n"
			"    + float3(0.0, 0.15, 0.20)))) * bgPat * 0.25;\n"
			"\n"
			"// --- Mandelbox fractal (shared by both scenes) ---\n"
			"float2 suv = abs(uv);\n"
			"suv -= 0.5;\n"
			"float ra0 = time * 0.05;\n"
			"float rs0 = sin(ra0); float rc0 = cos(ra0);\n"
			"suv = float2(rc0 * suv.x - rs0 * suv.y, rs0 * suv.x + rc0 * suv.y);\n"
			"float2 uv0 = suv;\n"
			"float sc = 2.0 + 0.1 * sin(time * 0.1);\n"
			"\n"
			"float3 sceneCol = float3(0.0, 0.0, 0.0);\n"
			"float2 suv1 = suv;\n"
			"\n"
			"// Neon Circuitry: Alien Synthesis — Mandelbox fractal\n"
			"for (int i = 0; i < 5; i++)\n"
			"{\n"
			"    suv1 = clamp(suv1, -1.0, 1.0) * 2.0 - suv1;\n"
			"    float r2 = dot(suv1, suv1);\n"
			"    float sphereFold = lerp(1.0, 1.0 / max(r2, 0.001), step(0.25, r2) - step(1.0, r2));\n"
			"    sphereFold = lerp(sphereFold, 4.0, 1.0 - step(0.25, r2));\n"
			"    suv1 *= sphereFold;\n"
			"    suv1 *= sc;\n"
			"    float fi = float(i);\n"
			"    float ira = 0.2 + fi * 0.03 * sin(time * 0.1);\n"
			"    float irs = sin(ira); float irc = cos(ira);\n"
			"    suv1 = float2(irc * suv1.x - irs * suv1.y, irs * suv1.x + irc * suv1.y);\n"
			"    suv1 += float2(0.5 * sin(time * 0.1), 0.5 * cos(time * 0.13));\n"
			"    float d = length(suv1);\n"
			"    float pt = length(uv0) + fi * 0.4 + time * 0.1;\n"
			"    float3 col1 = 0.5 + 0.5 * cos(6.28318 * (pt + float3(0.263, 0.416, 0.657)));\n"
			"    float sinP = sin(d * (4.0 + Level * 6.0) + time * 0.1) / 8.0;\n"
			"    float sharpP = abs(sin(d * 8.0 + time * 0.1)) / 8.0;\n"
			"    float dMixed = lerp(sinP, sharpP, 0.5);\n"
			"    float brightness = 0.008 + Bass * 0.004;\n"
			"    float glow1 = pow(brightness / (abs(dMixed) + 0.01), 1.15);\n"
			"    sceneCol += col1 * glow1 * exp(-fi * 0.25);\n"
			"}\n"
			"sceneCol *= 0.8;\n"
			"\n"
			"// Blend scene with BG based on scene luminance\n"
			"float scLum = dot(sceneCol, float3(0.299, 0.587, 0.114));\n"
			"float blendA = clamp(smoothstep(0.05, 0.4, scLum) + Bass * 0.05, 0.0, 1.0);\n"
			"float3 col = lerp(bgCol, sceneCol, blendA);\n"
			"\n"
			"// Depth layering — push darks deeper, lift mids to reveal fine detail\n"
			"float lum = dot(col, float3(0.299, 0.587, 0.114));\n"
			"float depthContrast = smoothstep(0.0, 0.35, lum);\n"
			"col *= depthContrast * 1.4 + 0.05;\n"
			"\n"
			"// Desaturate peak colors — softer, more ethereal\n"
			"float grayVal = dot(col, float3(0.299, 0.587, 0.114));\n"
			"col = lerp(float3(grayVal, grayVal, grayVal), col, 1.4);\n"
			"\n"
			"// Clamp peak intensity\n"
			"col = min(col, float3(0.85, 0.85, 0.85));\n"
			"\n"
			"// Final brightness\n"
			"col *= 0.336;\n"
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
