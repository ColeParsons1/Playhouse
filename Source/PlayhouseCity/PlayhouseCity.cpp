// Copyright PlayhouseCity. All Rights Reserved.

#include "PlayhouseCity.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
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

		Mat->PreEditChange(nullptr);
		Mat->PostEditChange();

		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		UPackage::SavePackage(Package, Mat, *FilePath, SaveArgs);

		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHC: Created and saved M_PHCSkin material to %s"), *FilePath);
		}
	}
#endif
}

void FPlayhouseCityModule::ShutdownModule()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PlayhouseCity module shutting down"));
}
