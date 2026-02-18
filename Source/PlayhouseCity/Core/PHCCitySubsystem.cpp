// Copyright PlayhouseCity. All Rights Reserved.

#include "Core/PHCCitySubsystem.h"
#include "Core/PHCGameState.h"
#include "PlayhouseCity.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/DirectionalLight.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"
#include "Engine/Engine.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Engine/PostProcessVolume.h"

// ---------------------------------------------------------------------------
// Centralized city-grid constants (used across multiple Spawn* functions).
// ---------------------------------------------------------------------------
static constexpr int32 NumRoads = 25;
static constexpr float RoadSpacing = 2800.f;
static constexpr float RoadWidth = 700.f;
static constexpr float SidewalkWidth = 180.f;
static constexpr float CityHalfExtent = (NumRoads - 1) * RoadSpacing * 0.5f + 1400.f; // ~35000
static constexpr float FirstRoad = -(NumRoads - 1) * RoadSpacing * 0.5f; // ~-33600

// ---- PBR Material Helpers ------------------------------------------------

static void ApplyPBR(UStaticMeshComponent* MC, const FLinearColor& Color,
	float Metallic = 0.f, float Roughness = 0.5f,
	const FLinearColor& Emissive = FLinearColor::Black)
{
	if (!MC) return;
	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
	if (!Mat) return;
	Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
	if (Metallic != 0.f)
		Mat->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	if (Roughness != 0.5f)
		Mat->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	if (Emissive != FLinearColor::Black)
		Mat->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
	MC->SetMaterial(0, Mat);
}

static UMaterialInstanceDynamic* ApplyPBRGlass(UStaticMeshComponent* MC,
	UMaterial* GlassMat, const FLinearColor& Color,
	float Opacity = 0.3f, float Metallic = 0.f, float Roughness = 0.05f,
	const FLinearColor& Emissive = FLinearColor::Black)
{
	if (!MC || !GlassMat) return nullptr;
	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(GlassMat, MC);
	if (!Mat) return nullptr;
	Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
	Mat->SetScalarParameterValue(TEXT("Opacity"), Opacity);
	if (Metallic != 0.f)
		Mat->SetScalarParameterValue(TEXT("Metallic"), Metallic);
	if (Roughness != 0.05f)
		Mat->SetScalarParameterValue(TEXT("Roughness"), Roughness);
	if (Emissive != FLinearColor::Black)
		Mat->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
	MC->SetMaterial(0, Mat);
	return Mat;
}

// ---------------------------------------------------------------------------

TStatId UPHCCitySubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UPHCCitySubsystem, STATGROUP_Tickables);
}

bool UPHCCitySubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UPHCCitySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (bCityGenerated) return;
	bCityGenerated = true;

	CityRandom.Initialize(42);
	DetailRandom.Initialize(137);

	CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	CylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	SphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));

	// Load the color material with a "BaseColor" vector parameter.
	// The .uasset is created and saved by FPlayhouseCityModule::StartupModule()
	// in editor/cook builds and then cooked for iOS. On editor, fall back to
	// creating it inline if the asset hasn't been saved yet.
	ColorMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));
	GlassMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCGlass.M_PHCGlass"));

#if WITH_EDITORONLY_DATA
	if (!ColorMaterial)
	{
		// First editor run before the .uasset exists -- create inline.
		ColorMaterial = NewObject<UMaterial>(GetTransientPackage(), FName(TEXT("PHC_ColorMaterial")));

		UMaterialExpressionVectorParameter* ColorParam =
			NewObject<UMaterialExpressionVectorParameter>(ColorMaterial);
		ColorParam->ParameterName = TEXT("BaseColor");
		ColorParam->DefaultValue = FLinearColor(1.f, 1.f, 1.f, 1.f);

		UMaterialEditorOnlyData* EditorData = ColorMaterial->GetEditorOnlyData();
		EditorData->ExpressionCollection.Expressions.Add(ColorParam);
		EditorData->BaseColor.Expression = ColorParam;

		UMaterialExpressionScalarParameter* MetParam = NewObject<UMaterialExpressionScalarParameter>(ColorMaterial);
		MetParam->ParameterName = TEXT("Metallic");
		MetParam->DefaultValue = 0.0f;
		EditorData->ExpressionCollection.Expressions.Add(MetParam);
		EditorData->Metallic.Expression = MetParam;

		UMaterialExpressionScalarParameter* RghParam = NewObject<UMaterialExpressionScalarParameter>(ColorMaterial);
		RghParam->ParameterName = TEXT("Roughness");
		RghParam->DefaultValue = 0.5f;
		EditorData->ExpressionCollection.Expressions.Add(RghParam);
		EditorData->Roughness.Expression = RghParam;

		UMaterialExpressionVectorParameter* EmParam = NewObject<UMaterialExpressionVectorParameter>(ColorMaterial);
		EmParam->ParameterName = TEXT("EmissiveColor");
		EmParam->DefaultValue = FLinearColor::Black;
		EditorData->ExpressionCollection.Expressions.Add(EmParam);
		EditorData->EmissiveColor.Expression = EmParam;

		ColorMaterial->PreEditChange(nullptr);
		ColorMaterial->PostEditChange();
	}

	if (!GlassMaterial)
	{
		GlassMaterial = NewObject<UMaterial>(GetTransientPackage(), FName(TEXT("PHC_GlassMaterial")));
		GlassMaterial->BlendMode = BLEND_Translucent;
		GlassMaterial->TranslucencyLightingMode = TLM_SurfacePerPixelLighting;

		// BaseColor
		UMaterialExpressionVectorParameter* GBC = NewObject<UMaterialExpressionVectorParameter>(GlassMaterial);
		GBC->ParameterName = TEXT("BaseColor");
		GBC->DefaultValue = FLinearColor(0.1f, 0.15f, 0.2f, 1.f);

		// Opacity
		UMaterialExpressionScalarParameter* GOp = NewObject<UMaterialExpressionScalarParameter>(GlassMaterial);
		GOp->ParameterName = TEXT("Opacity");
		GOp->DefaultValue = 0.3f;

		// Metallic
		UMaterialExpressionScalarParameter* GMet = NewObject<UMaterialExpressionScalarParameter>(GlassMaterial);
		GMet->ParameterName = TEXT("Metallic");
		GMet->DefaultValue = 0.0f;

		// Roughness
		UMaterialExpressionScalarParameter* GRgh = NewObject<UMaterialExpressionScalarParameter>(GlassMaterial);
		GRgh->ParameterName = TEXT("Roughness");
		GRgh->DefaultValue = 0.05f;

		// EmissiveColor
		UMaterialExpressionVectorParameter* GEm = NewObject<UMaterialExpressionVectorParameter>(GlassMaterial);
		GEm->ParameterName = TEXT("EmissiveColor");
		GEm->DefaultValue = FLinearColor::Black;

		UMaterialEditorOnlyData* GED = GlassMaterial->GetEditorOnlyData();
		GED->ExpressionCollection.Expressions = { GBC, GOp, GMet, GRgh, GEm };
		GED->BaseColor.Expression = GBC;
		GED->Opacity.Expression = GOp;
		GED->Metallic.Expression = GMet;
		GED->Roughness.Expression = GRgh;
		GED->EmissiveColor.Expression = GEm;

		GlassMaterial->PreEditChange(nullptr);
		GlassMaterial->PostEditChange();
	}
#endif

	if (ColorMaterial)
	{
		// Engine BasicShapes are "Disregard for GC" objects -- they can only
		// reference root-set materials.  Add ours so GC verification passes.
		ColorMaterial->AddToRoot();

		// Override all basic shape default materials so GetMaterial(0) returns ours.
		auto OverrideMeshMat = [&](UStaticMesh* Mesh)
		{
			if (Mesh && Mesh->GetStaticMaterials().Num() > 0)
			{
				Mesh->GetStaticMaterials()[0].MaterialInterface = ColorMaterial;
			}
		};
		OverrideMeshMat(CubeMesh);
		OverrideMeshMat(PlaneMesh);
		OverrideMeshMat(CylinderMesh);
		OverrideMeshMat(SphereMesh);
		OverrideMeshMat(ConeMesh);

		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Loaded ColorMaterial and overrode basic shape materials."));
	}
	else
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("PHCCityGameMode: ColorMaterial not found! Shapes will be uncolored."));
	}

	if (GlassMaterial)
	{
		GlassMaterial->AddToRoot();
	}

	// Disable ALL on-screen debug/log text (yellow messages).
	GAreScreenMessagesEnabled = false;
	if (GEngine)
	{
		GEngine->bEnableOnScreenDebugMessages = false;
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode::BeginPlay -- Spawning city."));
	SpawnCityEnvironment();
}

void UPHCCitySubsystem::Deinitialize()
{
	if (ColorMaterial)
	{
		ColorMaterial->RemoveFromRoot();
	}
	if (GlassMaterial)
	{
		GlassMaterial->RemoveFromRoot();
	}
	Super::Deinitialize();
}

// ---------------------------------------------------------------------------
// Day/Night Cycle -- 20 minute full cycle (10 min day + 10 min night)
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// On remote clients, sync day/night time from the server's GameState
	UWorld* World = GetWorld();
	if (World)
	{
		APHCGameState* GS = Cast<APHCGameState>(World->GetGameState());
		if (GS && !World->GetAuthGameMode())
		{
			// Remote client: use server's time instead of computing locally
			DayNightTime = GS->ServerDayNightTime;
		}
		else if (GS && World->GetAuthGameMode())
		{
			// Server/host: push our time to GameState for replication
			GS->ServerDayNightTime = DayNightTime;
		}
	}

	// Full cycle: 20 minutes = 1200 seconds.
	// T=0.0 sunrise, T=0.25 noon, T=0.5 sunset, T=0.75 midnight, T=1.0 sunrise.
	constexpr float FullCycleDuration = 1200.f;

	if (!bNightModeLocked)
	{
		DayNightTime += DeltaTime / FullCycleDuration;
		if (DayNightTime >= 1.f) DayNightTime -= 1.f;
	}

	const float T = DayNightTime;
	const float SunSine = FMath::Sin(T * 2.f * UE_PI); // +1 at noon, -1 at midnight

	// ---- Sun rotation ----
	if (SunLightComp)
	{
		// Pitch: -80 at noon (high in sky), +80 at midnight (below horizon)
		const float SunPitch = -80.f * SunSine;
		// Yaw: rotates east (90) -> south (0) -> west (-90) through the day
		const float SunYaw = 90.f - T * 360.f;
		SunLightComp->SetWorldRotation(FRotator(SunPitch, SunYaw, 0.f));

		// Intensity scales with elevation; zero when below horizon.
		const float Elevation = FMath::Max(0.f, SunSine);
		SunLightComp->SetIntensity(Elevation * 4.5f);

		// Color: orange at low sun, warm white at high sun.
		if (Elevation > 0.01f)
		{
			const float OrangeFactor = 1.f - FMath::Pow(Elevation, 0.4f);
			const FLinearColor NoonColor(1.f, 0.97f, 0.92f);
			const FLinearColor HorizonColor(1.f, 0.45f, 0.12f);
			const FLinearColor SunColor = FMath::Lerp(NoonColor, HorizonColor, OrangeFactor);
			SunLightComp->SetLightColor(SunColor.ToFColor(true));
		}
	}

	// ---- Moon (opposite sun, visible at night) ----
	if (MoonLightComp)
	{
		const float MoonElevation = FMath::Max(0.f, -SunSine);
		MoonLightComp->SetIntensity(MoonElevation * 0.08f);
		const float MoonPitch = -60.f * MoonElevation;
		const float MoonYaw = 90.f - T * 360.f + 180.f;
		MoonLightComp->SetWorldRotation(FRotator(MoonPitch, MoonYaw, 0.f));
	}

	// ---- Sky light (ambient fill) ----
	if (SkyLightComp)
	{
		// Bright during day, very dim at night.
		const float DayFactor = FMath::Max(0.f, SunSine);
		SkyLightComp->SetIntensity(FMath::Lerp(0.03f, 1.5f, DayFactor));

		// Tint: blue-white during day, very deep blue at night.
		const float NightFactor = FMath::Max(0.f, -SunSine);
		const FLinearColor DayTint(0.6f, 0.7f, 1.0f);
		const FLinearColor NightTint(0.04f, 0.01f, 0.06f);
		SkyLightComp->SetLightColor(FMath::Lerp(DayTint, NightTint, NightFactor));

		// RecaptureSky is extremely expensive (full cubemap capture).
		// Throttle to once every 10 seconds to avoid GPU memory exhaustion on mobile.
		SkyRecaptureTimer += DeltaTime;
		if (SkyRecaptureTimer >= 10.f)
		{
			SkyRecaptureTimer = 0.f;
			SkyLightComp->RecaptureSky();
		}
	}

	// ---- Fog (thicker at night, directional inscattering for god rays) ----
	if (FogComp)
	{
		const float NightFactor = FMath::Max(0.f, -SunSine);
		FogComp->SetFogDensity(FMath::Lerp(0.0008f, 0.003f, NightFactor));
		FogComp->SetFogMaxOpacity(FMath::Lerp(0.45f, 0.05f, NightFactor));
		FogComp->SetVolumetricFogDistance(FMath::Lerp(20000.f, 6000.f, NightFactor));

		// GTA V-style directional inscattering -- warm haze at dawn/dusk, near-black at night
		const float DawnDusk = 1.f - FMath::Abs(SunSine);
		FogComp->SetDirectionalInscatteringExponent(8.f);
		FogComp->SetDirectionalInscatteringStartDistance(5000.f);
		const FLinearColor NeutralFog(0.6f, 0.6f, 0.7f);
		const FLinearColor WarmHaze(1.0f, 0.7f, 0.3f);
		const FLinearColor NightFog(0.02f, 0.005f, 0.03f);
		FLinearColor InscatterColor = FMath::Lerp(NeutralFog, WarmHaze, DawnDusk);
		InscatterColor = FMath::Lerp(InscatterColor, NightFog, NightFactor);
		FogComp->SetFogInscatteringColor(InscatterColor);
	}

	// ---- Post-process day/night transition (GTA V color grading) ----
	if (PostProcessVolume)
	{
		FPostProcessSettings& PP = PostProcessVolume->Settings;
		const float NF = FMath::Max(0.f, -SunSine);
		const float DawnDuskFactor = 1.f - FMath::Abs(SunSine);
		const float Elevation = FMath::Max(0.f, SunSine);

		PP.BloomThreshold = FMath::Lerp(1.0f, 0.2f, NF);
		PP.BloomIntensity = FMath::Lerp(0.8f, 1.2f, NF);

		// GTA V 4-phase color grading (ColorGain + ColorOffset)
		// Determine time-of-day phase: dawn (T~0), day (T~0.25), sunset (T~0.5), night (T~0.75)
		FVector4 TargetGain;
		FVector4 TargetOffset;
		if (T < 0.125f || T >= 0.875f)
		{
			// Dawn: warm pink-amber
			TargetGain = FVector4(1.15f, 0.95f, 0.85f, 1.0f);
			TargetOffset = FVector4(0.02f, 0.005f, -0.01f, 0.0f);
		}
		else if (T < 0.375f)
		{
			// Day: subtle warm
			TargetGain = FVector4(1.02f, 1.00f, 0.96f, 1.0f);
			TargetOffset = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		}
		else if (T < 0.625f)
		{
			// Sunset: golden amber
			TargetGain = FVector4(1.20f, 0.90f, 0.70f, 1.0f);
			TargetOffset = FVector4(0.03f, 0.01f, -0.02f, 0.0f);
		}
		else
		{
			// Night: very dark purple
			TargetGain = FVector4(0.42f, 0.32f, 0.48f, 1.0f);
			TargetOffset = FVector4(-0.01f, -0.04f, 0.01f, 0.0f);
		}
		// Smooth interpolation to target
		PP.ColorGain = FMath::Lerp(PP.ColorGain, TargetGain, FMath::Min(1.f, DeltaTime * 2.f));
		PP.ColorOffset = FMath::Lerp(PP.ColorOffset, TargetOffset, FMath::Min(1.f, DeltaTime * 2.f));

		// Legacy ColorGamma still used for base warmth
		const float WarmR = FMath::Lerp(1.0f, 1.08f, NF);
		const float WarmB = FMath::Lerp(0.92f, 0.90f, NF);
		const float WarmG = FMath::Lerp(0.97f, 0.88f, NF);
		PP.ColorGamma = FVector4(WarmR, WarmG, WarmB, 1.0f);

		PP.VignetteIntensity = FMath::Lerp(0.25f, 0.45f, DawnDuskFactor);

		PP.AutoExposureMinBrightness = FMath::Lerp(0.5f, 0.01f, NF);
		PP.AutoExposureMaxBrightness = FMath::Lerp(3.0f, 0.48f, NF);

		// ---- Depth of Field: sharper when walking, blurry background when driving ----
		// Check if any local player is currently possessing a vehicle
		bool bDriving = false;
		if (UWorld* W = GetWorld())
		{
			if (APlayerController* PC = W->GetFirstPlayerController())
			{
				bDriving = (PC->GetPawn() && PC->GetPawn()->IsA(APawn::StaticClass()) &&
					PC->GetPawn()->GetClass()->GetName().Contains(TEXT("Vehicle")));
			}
		}
		const float TargetFstop = bDriving ? 4.0f : 22.0f;
		PP.DepthOfFieldFstop = FMath::FInterpTo(PP.DepthOfFieldFstop, TargetFstop, DeltaTime, 3.f);
	}

	// ---- God ray toggle (disable when sun below horizon) ----
	if (SunLightComp)
	{
		const float Elevation = FMath::Max(0.f, SunSine);
		SunLightComp->bEnableLightShaftBloom = (Elevation > 0.05f);
	}

	// ---- Starfield visibility ----
	if (StarfieldActor)
	{
		const bool bShowStars = (SunSine < -0.10f);
		if (StarfieldActor->IsHidden() != !bShowStars)
		{
			StarfieldActor->SetActorHiddenInGame(!bShowStars);
		}
	}

	// ---- Cloud layer drift + sunset tinting ----
	{
		const float CloudDriftSpeed = 500.f; // UU per minute eastward
		const float CloudDrift = DeltaTime * (CloudDriftSpeed / 60.f);
		const float DawnDusk = 1.f - FMath::Abs(SunSine);

		if (CloudLayerActor)
		{
			CloudLayerActor->AddActorWorldOffset(FVector(CloudDrift, CloudDrift * 0.3f, 0.f));
		}
		if (CloudLayerActor2)
		{
			// Second layer drifts slower for parallax
			CloudLayerActor2->AddActorWorldOffset(FVector(CloudDrift * 0.6f, CloudDrift * 0.15f, 0.f));
		}
	}

	// ---- Ocean wave heave ----
	if (OceanActor)
	{
		WaterTime += DeltaTime;
		const float WaveZ = -8.f + FMath::Sin(WaterTime * 0.15f * 2.f * UE_PI) * 3.f;
		FVector OceanLoc = OceanActor->GetActorLocation();
		OceanLoc.Z = WaveZ;
		OceanActor->SetActorLocation(OceanLoc);
	}

	// ---- Wet roads at night (dew/humidity effect) ----
	if (RoadMaterials.Num() > 0)
	{
		const float NightFactor = FMath::Max(0.f, -SunSine);
		const float TargetWetness = NightFactor * 0.4f; // up to 40% wetness at midnight
		if (FMath::Abs(TargetWetness - GlobalWetness) > 0.02f)
		{
			GlobalWetness = FMath::FInterpTo(GlobalWetness, TargetWetness, DeltaTime, 0.5f);
			const float WetRoughness = FMath::Lerp(0.85f, 0.15f, GlobalWetness);
			const float DarkenFactor = FMath::Lerp(1.0f, 0.6f, GlobalWetness);
			for (auto& RoadMat : RoadMaterials)
			{
				if (RoadMat)
				{
					RoadMat->SetScalarParameterValue(TEXT("Roughness"), WetRoughness);
					RoadMat->SetVectorParameterValue(TEXT("BaseColor"),
						FLinearColor(0.015f * DarkenFactor, 0.015f * DarkenFactor, 0.018f * DarkenFactor));
				}
			}
		}
	}

	// ---- Night-lit windows (offices, apartments, condos) ----
	if (NightWindowMaterials.Num() > 0)
	{
		// Target brightness: ramps up at dusk (SunSine goes negative), full at night.
		// Starts turning on when sun is ~10 above horizon, fully on once well below.
		float TargetBrightness = 0.f;
		if (SunSine < -0.15f)
		{
			TargetBrightness = 1.f;
		}
		else if (SunSine < 0.08f)
		{
			// Smooth transition during dusk/dawn.
			TargetBrightness = FMath::GetMappedRangeValueClamped(
				FVector2D(0.08f, -0.15f), FVector2D(0.f, 1.f), SunSine);
		}

		// Only update materials when brightness changes meaningfully.
		const bool bBrightnessChanged = FMath::Abs(TargetBrightness - NightWindowBrightness) > 0.015f;

		// Intermittent toggling: every ~45 seconds at night, some windows flip on/off.
		bool bToggled = false;
		if (TargetBrightness > 0.5f)
		{
			WindowToggleTimer += DeltaTime;
			if (WindowToggleTimer >= 45.f)
			{
				WindowToggleTimer = 0.f;
				bToggled = true;
				// ~8% of windows toggle state (some go to bed, some come home late).
				for (int32 i = 0; i < NightWindowLitStates.Num(); ++i)
				{
					if (CityRandom.FRand() < 0.08f)
					{
						NightWindowLitStates[i] = !NightWindowLitStates[i];
					}
				}
			}
		}
		else
		{
			WindowToggleTimer = 0.f;
		}

		if (bBrightnessChanged || bToggled)
		{
			NightWindowBrightness = TargetBrightness;
			const FLinearColor DarkGlass(0.06f, 0.07f, 0.10f);

			for (int32 i = 0; i < NightWindowMaterials.Num(); ++i)
			{
				if (!NightWindowMaterials[i]) continue;

				if (NightWindowLitStates[i] && NightWindowBrightness > 0.01f)
				{
					NightWindowMaterials[i]->SetVectorParameterValue(TEXT("BaseColor"), DarkGlass);
					const FLinearColor EmissiveGlow = NightWindowLitColors[i] * NightWindowBrightness * 2.0f;
					NightWindowMaterials[i]->SetVectorParameterValue(TEXT("EmissiveColor"), EmissiveGlow);
				}
				else
				{
					NightWindowMaterials[i]->SetVectorParameterValue(TEXT("BaseColor"), DarkGlass);
					NightWindowMaterials[i]->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
				}
			}
		}
	}

	// ---- Giant Crab NPC (night only) ----
	if (GiantCrabActor)
	{
		const float CrabNF = FMath::Max(0.f, -SunSine);
		const bool bShouldShow = (CrabNF > 0.3f) || bNightModeLocked;

		if (bShouldShow != bCrabVisible)
		{
			bCrabVisible = bShouldShow;
			GiantCrabActor->SetActorHiddenInGame(!bShouldShow);
		}

		if (bCrabVisible)
		{
			// --- Movement: roam toward target ---
			FVector CPos = GiantCrabActor->GetActorLocation();
			FVector ToTarget = CrabRoamTarget - CPos;
			ToTarget.Z = 0;

			if (ToTarget.Size2D() < 1500.f)
			{
				// Pick new random roam target
				CrabRoamTarget = FVector(
					FMath::FRandRange(-25000.f, 25000.f),
					FMath::FRandRange(-25000.f, 25000.f),
					CPos.Z);
			}

			const float CrabSpeed = 2235.f;
			FVector MoveDir = ToTarget.GetSafeNormal2D();
			GiantCrabActor->AddActorWorldOffset(MoveDir * CrabSpeed * DeltaTime);

			// Face movement direction (head is at local +Y)
			FRotator DesiredRot(0, MoveDir.Rotation().Yaw - 90.f, 0);
			FRotator CurRot = GiantCrabActor->GetActorRotation();
			GiantCrabActor->SetActorRotation(
				FMath::RInterpTo(CurRot, DesiredRot, DeltaTime, 1.5f));

			// --- Leg animation ---
			CrabAnimTime += DeltaTime;
			const float GaitFreq = 3.0f;

			for (int i = 0; i < CrabHipJoints.Num() && i < CrabHipBaseRot.Num(); i++)
			{
				int LegInSide = i % 4;
				int Side = i / 4;

				// Alternating gait: even/odd legs on opposite sides in sync
				float Phase = (LegInSide % 2 == 0) ? 0.f : UE_PI;
				if (Side == 1) Phase += UE_PI;
				Phase += LegInSide * 0.25f;

				float A = CrabAnimTime * GaitFreq * UE_TWO_PI + Phase;

				// Hip: oscillate Roll for forward/back swing
				float HipSwing = FMath::Sin(A) * 12.f;
				FRotator HR = CrabHipBaseRot[i];
				HR.Roll += HipSwing;
				CrabHipJoints[i]->SetRelativeRotation(HR);

				// Knee: oscillate Pitch for flex (more bend when lifted)
				if (i < CrabKneeJoints.Num() && i < CrabKneeBaseRot.Num())
				{
					float KneeFlex = FMath::Sin(A + UE_HALF_PI) * 15.f;
					FRotator KR = CrabKneeBaseRot[i];
					KR.Pitch += KneeFlex;
					CrabKneeJoints[i]->SetRelativeRotation(KR);
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
void UPHCCitySubsystem::SpawnCityEnvironment()
{
	SpawnGround();
	SpawnOceanAndBeach();
	SpawnSkyAndLighting();
	SpawnCityGrid();
	SpawnBlockStreets();
	SpawnSkyscrapers();
	SpawnStreetLights();
	SpawnRamps();
	SpawnOutskirts();
	SpawnHills();
	SpawnStarfield();
	SpawnCloudLayer();
	SpawnMountainRange();
	SpawnSandCastle();
	SpawnLakeAndPirateShip();
	SpawnUndergroundCavern();
	SpawnGiantCrab();
}

// ---------------------------------------------------------------------------
// Ground
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnGround()
{
	UWorld* World = GetWorld();
	// Use CubeMesh instead of PlaneMesh — the Plane has zero-thickness
	// collision that confuses the CharacterMovementComponent's overlap
	// resolution, launching the capsule to Z≈90000.  A thin Cube (2 UU)
	// gives proper box collision the CMC can resolve correctly.
	if (!World || !CubeMesh) return;

	const FLinearColor GrassColor(0.12f, 0.18f, 0.08f);

	// TopZ: desired Z of the top surface.
	// Cube default is 100×100×100 centered at origin.
	// ScaleZ=0.02 → 2 UU thick, half-height=1.  Center at TopZ-1.
	auto SpawnGroundSlab = [&](float TopZ, const FVector& XYLoc,
		const FVector& XYScale, const TCHAR* Label)
	{
		const FVector Loc(XYLoc.X, XYLoc.Y, TopZ - 1.f);
		AActor* GA = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!GA) return;
#if WITH_EDITOR
		GA->SetActorLabel(Label);
#endif
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(GA, TEXT("GroundMesh"));
		MC->SetStaticMesh(CubeMesh);
		MC->SetWorldScale3D(FVector(XYScale.X, XYScale.Y, 0.02f));
		MC->SetWorldLocation(Loc);
		// NoCollision — the ground is visual only.  Characters walk on
		// road / sidewalk slabs which have proper box collision.
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->RegisterComponent();
		GA->SetRootComponent(MC);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
		if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), GrassColor); Mat->SetScalarParameterValue(TEXT("Roughness"), 0.9f); MC->SetMaterial(0, Mat); }
		GA->AddInstanceComponent(MC);
	};

	// Main city ground: doubled map — covers ±126,000 UU.
	// Coastline zone starts at 126k where ocean ramps begin.
	SpawnGroundSlab(0.f, FVector(0.f, 0.f, 0.f), FVector(2520.f, 2520.f, 1.f), TEXT("CityGround"));

	// Northwest fill: extends ground beyond north edge
	SpawnGroundSlab(0.f, FVector(-30000.f, 75000.f, 0.f), FVector(700.f, 200.f, 1.f), TEXT("NorthwestFill"));

	// Outskirts ground: west of city extending along highway
	SpawnGroundSlab(0.f, FVector(-130000.f, 0.f, 0.f), FVector(2000.f, 1400.f, 1.f), TEXT("OutskirtsMain"));

	// Far outskirts: single solid ground plane (no cavern hole)
	SpawnGroundSlab(0.f, FVector(-280000.f, 0.f, 0.f), FVector(1000.f, 1000.f, 1.f), TEXT("OutskirtsFar"));
}

// ---------------------------------------------------------------------------
// Ocean and Beach
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnOceanAndBeach()
{
	UWorld* World = GetWorld();
	if (!World || !PlaneMesh) return;

	AActor* LastSpawnedWater = nullptr;
	auto SpawnWater = [&](const FVector& Loc, const FVector& Scale, const FLinearColor& Color, const TCHAR* Label, bool bIsWater = false)
	{
		AActor* WA = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!WA) return;
		LastSpawnedWater = WA;
#if WITH_EDITOR
		WA->SetActorLabel(Label);
#endif
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(WA, TEXT("WaterMesh"));
		MC->SetStaticMesh(PlaneMesh);
		MC->SetWorldScale3D(Scale);
		MC->SetWorldLocation(Loc);
		if (bIsWater)
		{
			MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		else
		{
			MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MC->SetCollisionResponseToAllChannels(ECR_Block);
		}
		MC->RegisterComponent();
		WA->SetRootComponent(MC);
		if (bIsWater && GlassMaterial)
		{
			UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(GlassMaterial, MC);
			if (Mat)
			{
				Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
				Mat->SetScalarParameterValue(TEXT("Opacity"), 0.85f);
				Mat->SetScalarParameterValue(TEXT("Roughness"), 0.02f);
				Mat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.01f, 0.03f, 0.06f));
				MC->SetMaterial(0, Mat);
			}
		}
		else
		{
			UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
			if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), Color); Mat->SetScalarParameterValue(TEXT("Roughness"), bIsWater ? 0.02f : 0.85f); MC->SetMaterial(0, Mat); }
		}
		WA->AddInstanceComponent(MC);
	};

	const FLinearColor DeepOcean(0.02f, 0.06f, 0.14f);
	const FLinearColor ShallowWater(0.04f, 0.12f, 0.22f);
	const FLinearColor Sand(0.76f, 0.70f, 0.55f);

	// Global ocean floor — massive, covers everything below land
	SpawnWater(FVector(0.f, 0.f, -8.f), FVector(4000.f, 4000.f, 1.f), DeepOcean, TEXT("OceanFloor"), true);
	OceanActor = LastSpawnedWater; // Store for wave animation in Tick()

	// Bay inlet (NE corner) — shallow water sitting above ocean
	SpawnWater(FVector(45000.f, 60000.f, -3.f), FVector(250.f, 150.f, 1.f), ShallowWater, TEXT("BayInlet"), true);

	// ---- Beach strips (sandy shores around the peninsula) ----

	// East coast beach — long strip running N-S (doubled map)
	SpawnWater(FVector(130000.f, 0.f, -1.f), FVector(40.f, 2800.f, 1.f), Sand, TEXT("EastBeach"));

	// South coast beach — long strip running E-W (doubled map)
	SpawnWater(FVector(0.f, -130000.f, -1.f), FVector(2800.f, 40.f, 1.f), Sand, TEXT("SouthBeach"));

	// North coast beach (doubled map)
	SpawnWater(FVector(0.f, 128000.f, -1.f), FVector(2800.f, 40.f, 1.f), Sand, TEXT("NorthBeach"));

	// West coast beach (doubled map)
	SpawnWater(FVector(-130000.f, 0.f, -1.f), FVector(40.f, 2800.f, 1.f), Sand, TEXT("WestBeach"));

	// Bay south shoreline (kept in original position within old city area)
	SpawnWater(FVector(45000.f, 52000.f, -1.f), FVector(250.f, 14.f, 1.f), Sand, TEXT("BaySouthShore"));

	// Bay west shoreline
	SpawnWater(FVector(32000.f, 60000.f, -1.f), FVector(14.f, 100.f, 1.f), Sand, TEXT("BayWestShore"));

	// ========================================================================
	// Ocean floor: sloped ramps from shore down to deep seabed
	// ========================================================================
	if (!CubeMesh) return;

	const float SeabedZ = -3000.f;          // deep ocean floor — way below sea level
	const float RampLen = 10000.f;           // shorter run = steeper slope
	const float RampDrop = FMath::Abs(SeabedZ);
	const float RampAngle = FMath::RadiansToDegrees(FMath::Atan2(RampDrop, RampLen)); // ~16.7°
	const float RampHyp = FMath::Sqrt(RampLen * RampLen + RampDrop * RampDrop);
	const float RampCenterZ = SeabedZ * 0.5f; // midpoint of slope
	const float GroundEdge = 126000.f;       // matches doubled ground plane (2520 scale)
	const float RampCenterOffset = GroundEdge + RampLen * 0.5f; // 68000

	const FLinearColor SeabedColor(0.08f, 0.10f, 0.07f); // dark sandy ocean floor

	// Helper to spawn a collision ramp or floor slab
	auto SpawnOceanFloor = [&](const FVector& Loc, const FVector& Scale, const FRotator& Rot, const TCHAR* Label)
	{
		AActor* RA = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!RA) return;
#if WITH_EDITOR
		RA->SetActorLabel(Label);
#endif
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(RA, TEXT("FloorMesh"));
		MC->SetStaticMesh(CubeMesh);
		MC->SetWorldScale3D(Scale);
		MC->SetWorldLocation(Loc);
		MC->SetWorldRotation(Rot);
		MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MC->SetCollisionResponseToAllChannels(ECR_Block);
		MC->RegisterComponent();
		RA->SetRootComponent(MC);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
		if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), SeabedColor); Mat->SetScalarParameterValue(TEXT("Roughness"), 0.92f); MC->SetMaterial(0, Mat); }
		RA->AddInstanceComponent(MC);
	};

	// Deep seabed — massive flat floor beneath all water
	SpawnOceanFloor(
		FVector(0.f, 0.f, SeabedZ),
		FVector(4000.f, 4000.f, 0.5f),
		FRotator::ZeroRotator,
		TEXT("Seabed"));

	// East coast ramp (slopes down toward +X)
	SpawnOceanFloor(
		FVector(RampCenterOffset, 0.f, RampCenterZ),
		FVector(RampHyp / 100.f, 2000.f, 0.5f),
		FRotator(-RampAngle, 0.f, 0.f),
		TEXT("EastRamp"));

	// West coast ramp (slopes down toward -X)
	SpawnOceanFloor(
		FVector(-RampCenterOffset, 0.f, RampCenterZ),
		FVector(RampHyp / 100.f, 2000.f, 0.5f),
		FRotator(RampAngle, 0.f, 0.f),
		TEXT("WestRamp"));

	// South coast ramp (slopes down toward -Y)
	SpawnOceanFloor(
		FVector(0.f, -RampCenterOffset, RampCenterZ),
		FVector(2000.f, RampHyp / 100.f, 0.5f),
		FRotator(0.f, 0.f, RampAngle),
		TEXT("SouthRamp"));

	// North coast ramp (slopes down toward +Y)
	SpawnOceanFloor(
		FVector(0.f, RampCenterOffset, RampCenterZ),
		FVector(2000.f, RampHyp / 100.f, 0.5f),
		FRotator(0.f, 0.f, -RampAngle),
		TEXT("NorthRamp"));
}

// ---------------------------------------------------------------------------
// Sky and Lighting -- NIGHT TIME
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnSkyAndLighting()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Start at sunrise (T=0).
	DayNightTime = 0.f;

	// ---- Sun (directional light driving the sky atmosphere) ----
	{
		AActor* SunActor = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector::ZeroVector, FRotator(-5.f, 90.f, 0.f));
		if (SunActor)
		{
#if WITH_EDITOR
			SunActor->SetActorLabel(TEXT("Sun"));
#endif
			SunLightComp = NewObject<UDirectionalLightComponent>(
				SunActor, TEXT("SunLight"));
			SunLightComp->SetIntensity(0.5f); // sunrise intensity -- Tick will animate
			SunLightComp->SetLightColor(FColor(255, 140, 50)); // sunrise orange
			SunLightComp->SetWorldRotation(FRotator(-5.f, 90.f, 0.f));
			SunLightComp->SetCastShadows(true);
			SunLightComp->bAtmosphereSunLight = true;

			// ---- God Rays / Light Shafts ----
			SunLightComp->bEnableLightShaftBloom = true;
			SunLightComp->BloomScale = 1.2f;
			SunLightComp->BloomThreshold = 0.5f;

			SunLightComp->RegisterComponent();
			SunActor->SetRootComponent(SunLightComp);
			SunActor->AddInstanceComponent(SunLightComp);
		}
	}

	// ---- Sky Atmosphere (responds automatically to sun direction) ----
	{
		AActor* SkyActor = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector::ZeroVector, FRotator::ZeroRotator);
		if (SkyActor)
		{
#if WITH_EDITOR
			SkyActor->SetActorLabel(TEXT("CitySkyAtmosphere"));
#endif
			USkyAtmosphereComponent* SkyAtmo = NewObject<USkyAtmosphereComponent>(
				SkyActor, TEXT("SkyAtmosphere"));
			SkyAtmo->RegisterComponent();
			SkyActor->SetRootComponent(SkyAtmo);
			SkyActor->AddInstanceComponent(SkyAtmo);
		}
	}

	// ---- Sky Light (ambient fill -- real-time capture reflects sky color) ----
	{
		AActor* SkyLightActor = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector::ZeroVector, FRotator::ZeroRotator);
		if (SkyLightActor)
		{
#if WITH_EDITOR
			SkyLightActor->SetActorLabel(TEXT("CitySkyLight"));
#endif
			SkyLightComp = NewObject<USkyLightComponent>(
				SkyLightActor, TEXT("SkyLight"));
			SkyLightComp->SetIntensity(0.3f); // sunrise ambient
			SkyLightComp->SetLightColor(FLinearColor(0.6f, 0.7f, 1.0f));
			SkyLightComp->bRealTimeCapture = true;
			SkyLightComp->RegisterComponent();
			SkyLightActor->SetRootComponent(SkyLightComp);
			SkyLightActor->AddInstanceComponent(SkyLightComp);
		}
	}

	// ---- Exponential Height Fog ----
	{
		AActor* FogActor = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector::ZeroVector, FRotator::ZeroRotator);
		if (FogActor)
		{
#if WITH_EDITOR
			FogActor->SetActorLabel(TEXT("CityFog"));
#endif
			FogComp = NewObject<UExponentialHeightFogComponent>(
				FogActor, TEXT("HeightFog"));
			FogComp->SetFogDensity(0.0008f);
			FogComp->SetFogHeightFalloff(0.08f);
			FogComp->SetFogMaxOpacity(0.45f);
			FogComp->SetStartDistance(15000.f);
			FogComp->SetVolumetricFog(true);
			FogComp->SetVolumetricFogScatteringDistribution(0.2f);
			FogComp->SetVolumetricFogExtinctionScale(1.0f);
			FogComp->SetVolumetricFogDistance(20000.f);
			FogComp->RegisterComponent();
			FogActor->SetRootComponent(FogComp);
			FogActor->AddInstanceComponent(FogComp);
		}
	}

	// ---- Moon (dim directional light, blue-white, no atmosphere drive) ----
	{
		AActor* MoonActor = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector::ZeroVector, FRotator(-40.f, -90.f, 0.f));
		if (MoonActor)
		{
#if WITH_EDITOR
			MoonActor->SetActorLabel(TEXT("Moon"));
#endif
			MoonLightComp = NewObject<UDirectionalLightComponent>(
				MoonActor, TEXT("MoonLight"));
			MoonLightComp->SetIntensity(0.f);
			MoonLightComp->SetLightColor(FColor(180, 200, 255));
			MoonLightComp->SetWorldRotation(FRotator(-40.f, -90.f, 0.f));
			MoonLightComp->SetCastShadows(false);
			MoonLightComp->RegisterComponent();
			MoonActor->SetRootComponent(MoonLightComp);
			MoonActor->AddInstanceComponent(MoonLightComp);
		}
	}

	// ---- Global Post-Process Volume (cinematic look) ----
	{
		PostProcessVolume = World->SpawnActor<APostProcessVolume>(
			APostProcessVolume::StaticClass(),
			FVector::ZeroVector, FRotator::ZeroRotator);
		if (PostProcessVolume)
		{
#if WITH_EDITOR
			PostProcessVolume->SetActorLabel(TEXT("CityPostProcess"));
#endif
			PostProcessVolume->bUnbound = true;

			FPostProcessSettings& PP = PostProcessVolume->Settings;

			// Bloom
			PP.bOverride_BloomIntensity = true;
			PP.BloomIntensity = 0.8f;
			PP.bOverride_BloomThreshold = true;
			PP.BloomThreshold = 1.0f;

			// Ambient Occlusion
			PP.bOverride_AmbientOcclusionIntensity = true;
			PP.AmbientOcclusionIntensity = 0.5f;
			PP.bOverride_AmbientOcclusionRadius = true;
			PP.AmbientOcclusionRadius = 200.f;

			// Vignette
			PP.bOverride_VignetteIntensity = true;
			PP.VignetteIntensity = 0.25f;

			// Auto Exposure
			PP.bOverride_AutoExposureMinBrightness = true;
			PP.AutoExposureMinBrightness = 0.5f;
			PP.bOverride_AutoExposureMaxBrightness = true;
			PP.AutoExposureMaxBrightness = 3.0f;
			PP.bOverride_AutoExposureSpeedUp = true;
			PP.AutoExposureSpeedUp = 2.0f;
			PP.bOverride_AutoExposureSpeedDown = true;
			PP.AutoExposureSpeedDown = 1.0f;

			// Color Grading (warm cinematic tone)
			PP.bOverride_ColorGamma = true;
			PP.ColorGamma = FVector4(1.0f, 0.97f, 0.92f, 1.0f);
			PP.bOverride_ColorSaturation = true;
			PP.ColorSaturation = FVector4(1.05f, 1.05f, 1.05f, 1.0f);

			// ---- GTA V Filmic Tone Mapping ----
			PP.bOverride_FilmSlope = true;
			PP.FilmSlope = 0.88f;
			PP.bOverride_FilmToe = true;
			PP.FilmToe = 0.55f;
			PP.bOverride_FilmShoulder = true;
			PP.FilmShoulder = 0.26f;
			PP.bOverride_FilmBlackClip = true;
			PP.FilmBlackClip = 0.0f;
			PP.bOverride_FilmWhiteClip = true;
			PP.FilmWhiteClip = 0.04f;

			// ---- Chromatic Aberration (subtle lens fringing) ----
			PP.bOverride_SceneFringeIntensity = true;
			PP.SceneFringeIntensity = 0.4f;

			// ---- Film Grain (cinematic texture) ----
			PP.bOverride_FilmGrainIntensity = true;
			PP.FilmGrainIntensity = 0.06f;

			// ---- Depth of Field (cinematic background blur) ----
			PP.bOverride_DepthOfFieldFocalDistance = true;
			PP.DepthOfFieldFocalDistance = 3000.f;
			PP.bOverride_DepthOfFieldFstop = true;
			PP.DepthOfFieldFstop = 22.f; // Sharp by default, f/4 when driving
			PP.bOverride_DepthOfFieldSensorWidth = true;
			PP.DepthOfFieldSensorWidth = 24.576f; // APS-C sensor

			// ---- Color Gain/Offset for GTA V time-of-day grading ----
			PP.bOverride_ColorGain = true;
			PP.ColorGain = FVector4(1.02f, 1.00f, 0.96f, 1.0f);
			PP.bOverride_ColorOffset = true;
			PP.ColorOffset = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	}
}

// ---------------------------------------------------------------------------
// City grid -- roads, sidewalks, and buildings (Brookhaven suburban style)
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnCityGrid()
{
	// ---------------------------------------------------------------
	// 1. Landmark table & lookup set
	// ---------------------------------------------------------------
	struct FLandmark { int32 bx, by; float W, D, H; FLinearColor Wall, Accent; };
	const FLandmark Landmarks[] = {
		{ 3,  14, 900,  700, 800, FLinearColor(0.95f, 0.95f, 0.95f), FLinearColor(0.8f, 0.1f, 0.1f)   },  // Hospital
		{ 3,  20, 650,  550, 600, FLinearColor(0.3f, 0.35f, 0.55f),  FLinearColor(0.85f, 0.75f, 0.2f)  },  // Police
		{ 20,  3, 550,  480, 500, FLinearColor(0.8f, 0.2f, 0.15f),   FLinearColor(0.85f, 0.85f, 0.2f)  },  // Fire
		{ 15, 12, 850,  650, 600, FLinearColor(0.75f, 0.6f, 0.4f),   FLinearColor(0.4f, 0.5f, 0.3f)    },  // School
		{ 6,  13, 480,  400, 400, FLinearColor(0.6f, 0.55f, 0.45f),  FLinearColor(0.3f, 0.6f, 0.25f)   },  // Grocery
		{ 8,   9, 350,  300, 350, FLinearColor(0.45f, 0.3f, 0.2f),   FLinearColor(0.9f, 0.85f, 0.7f)   },  // Coffee
		{ 20, 20, 700,  550, 500, FLinearColor(0.9f, 0.9f, 0.88f),   FLinearColor(0.2f, 0.4f, 0.7f)    },  // Marina
		{ 2,   3, 800,  650, 700, FLinearColor(0.5f, 0.5f, 0.48f),   FLinearColor(0.6f, 0.4f, 0.15f)   },  // Factory
		{ 16,  6, 1100, 900, 500, FLinearColor(0.6f, 0.6f, 0.58f),   FLinearColor(0.15f, 0.35f, 0.6f)  },  // Stadium
		{ 1,  12, 420,  350, 350, FLinearColor(0.85f, 0.82f, 0.78f), FLinearColor(0.7f, 0.15f, 0.1f)   },  // Gas Station
	};

	const int32 NumBlocks = NumRoads - 1; // 24

	TSet<int32> LandmarkKeys;
	for (const FLandmark& L : Landmarks)
	{
		LandmarkKeys.Add(L.by * NumBlocks + L.bx);
	}

	// ---------------------------------------------------------------
	// 2. District classification lambda
	// ---------------------------------------------------------------
	auto GetDistrict = [](int32 bx, int32 by) -> ECityDistrict
	{
		if ((bx >= 7 && bx <= 8 && by >= 18 && by <= 19) ||
			(bx >= 13 && bx <= 14 && by >= 12 && by <= 13))
		{
			return ECityDistrict::Park;
		}
		if (bx >= 7 && bx <= 10 && by >= 10 && by <= 15)
		{
			return ECityDistrict::DowntownCore;
		}
		if (bx >= 5 && bx <= 10 && by >= 8 && by <= 17)
		{
			return ECityDistrict::Midtown;
		}
		if (bx >= 18 && by >= 17)
		{
			return ECityDistrict::Waterfront;
		}
		if (bx >= 1 && bx <= 4 && by >= 1 && by <= 6)
		{
			return ECityDistrict::Industrial;
		}
		if (bx >= 14 && bx <= 17 && by >= 7 && by <= 10)
		{
			return ECityDistrict::ArtsDistrict;
		}
		if (bx == 0 || bx == 23 || by == 0 || by == 23)
		{
			return ECityDistrict::Edge;
		}
		return ECityDistrict::Residential;
	};

	// ---------------------------------------------------------------
	// 3. Road network — 25 roads in each direction
	// ---------------------------------------------------------------
	// Full road length: 2 × CityHalfExtent so roads span the entire grid
	// (from FirstRoad − margin to LastRoad + margin).
	const float FullRoadLen = 2.f * CityHalfExtent;

	for (int32 i = 0; i < NumRoads; ++i)
	{
		const float RoadPos = FirstRoad + i * RoadSpacing;

		// X-direction road (runs east-west at Y = RoadPos)
		SpawnRoadStrip(FVector(0.f, RoadPos, 2.f), FullRoadLen, RoadWidth * 0.5f);
		SpawnSidewalk(FVector(0.f, RoadPos + RoadWidth * 0.5f + SidewalkWidth * 0.5f, 1.5f), FullRoadLen, SidewalkWidth * 0.5f);
		SpawnSidewalk(FVector(0.f, RoadPos - RoadWidth * 0.5f - SidewalkWidth * 0.5f, 1.5f), FullRoadLen, SidewalkWidth * 0.5f);

		// Y-direction road (runs north-south at X = RoadPos)
		SpawnRoadStrip(FVector(RoadPos, 0.f, 2.f), RoadWidth * 0.5f, FullRoadLen);
		SpawnSidewalk(FVector(RoadPos + RoadWidth * 0.5f + SidewalkWidth * 0.5f, 0.f, 1.5f), SidewalkWidth * 0.5f, FullRoadLen);
		SpawnSidewalk(FVector(RoadPos - RoadWidth * 0.5f - SidewalkWidth * 0.5f, 0.f, 1.5f), SidewalkWidth * 0.5f, FullRoadLen);
	}

	// ---------------------------------------------------------------
	// 4. Color palettes
	// ---------------------------------------------------------------
	const FLinearColor DowntownColors[] = {
		FLinearColor(0.78f, 0.78f, 0.78f),  // light gray
		FLinearColor(0.55f, 0.60f, 0.70f),  // blue-gray
		FLinearColor(0.65f, 0.67f, 0.70f),  // steel
		FLinearColor(0.82f, 0.76f, 0.65f),  // sandstone
		FLinearColor(0.92f, 0.90f, 0.88f),  // off-white
		FLinearColor(0.45f, 0.62f, 0.78f),  // glass blue
		FLinearColor(0.30f, 0.30f, 0.32f),  // charcoal
		FLinearColor(0.50f, 0.52f, 0.55f),  // slate
	};
	const int32 NumDowntownColors = UE_ARRAY_COUNT(DowntownColors);

	const FLinearColor MidtownColors[] = {
		FLinearColor(0.96f, 0.94f, 0.88f),  // cream
		FLinearColor(0.87f, 0.80f, 0.70f),  // warm beige
		FLinearColor(0.80f, 0.45f, 0.35f),  // terracotta
		FLinearColor(0.60f, 0.70f, 0.55f),  // sage
		FLinearColor(0.78f, 0.60f, 0.62f),  // dusty rose
	};
	const int32 NumMidtownColors = UE_ARRAY_COUNT(MidtownColors);

	const FLinearColor ResidentialColors[] = {
		FLinearColor(0.45f, 0.22f, 0.12f),  // classic red brick
		FLinearColor(0.52f, 0.28f, 0.15f),  // warm red brick
		FLinearColor(0.38f, 0.18f, 0.10f),  // dark red brick
		FLinearColor(0.55f, 0.35f, 0.22f),  // orange brick
		FLinearColor(0.40f, 0.25f, 0.18f),  // brown brick
		FLinearColor(0.60f, 0.42f, 0.28f),  // tan brick
		FLinearColor(0.35f, 0.20f, 0.12f),  // deep burgundy brick
		FLinearColor(0.48f, 0.30f, 0.20f),  // terracotta brick
		FLinearColor(0.62f, 0.78f, 0.92f),  // sky blue
		FLinearColor(0.94f, 0.92f, 0.90f),  // warm white
		FLinearColor(0.82f, 0.52f, 0.32f),  // terracotta
		FLinearColor(0.68f, 0.90f, 0.78f),  // mint
		FLinearColor(0.95f, 0.78f, 0.62f),  // peach
		FLinearColor(0.55f, 0.60f, 0.75f),  // slate blue
		FLinearColor(0.95f, 0.90f, 0.55f),  // butter yellow
		FLinearColor(0.80f, 0.80f, 0.78f),  // light gray
	};
	const int32 NumResidentialColors = UE_ARRAY_COUNT(ResidentialColors);

	const FLinearColor RoofColors[] = {
		FLinearColor(0.25f, 0.25f, 0.28f),  // dark gray
		FLinearColor(0.30f, 0.18f, 0.10f),  // dark brown
		FLinearColor(0.55f, 0.22f, 0.15f),  // brick red
		FLinearColor(0.15f, 0.30f, 0.15f),  // forest green
		FLinearColor(0.18f, 0.18f, 0.20f),  // charcoal
		FLinearColor(0.40f, 0.42f, 0.45f),  // slate
	};
	const int32 NumRoofColors = UE_ARRAY_COUNT(RoofColors);

	const FLinearColor WaterfrontWallColors[] = {
		FLinearColor(0.95f, 0.95f, 0.95f),  // white
		FLinearColor(0.78f, 0.88f, 0.95f),  // light blue
		FLinearColor(0.72f, 0.68f, 0.62f),  // driftwood gray
		FLinearColor(0.88f, 0.83f, 0.72f),  // sandy beige
	};
	const int32 NumWaterfrontWallColors = UE_ARRAY_COUNT(WaterfrontWallColors);

	const FLinearColor WaterfrontAwningColors[] = {
		FLinearColor(0.15f, 0.45f, 0.75f),  // ocean blue
		FLinearColor(0.90f, 0.50f, 0.40f),  // coral
		FLinearColor(0.85f, 0.75f, 0.35f),  // sandy gold
		FLinearColor(0.40f, 0.80f, 0.65f),  // seafoam
		FLinearColor(0.92f, 0.72f, 0.75f),  // shell pink
	};
	const int32 NumWaterfrontAwningColors = UE_ARRAY_COUNT(WaterfrontAwningColors);

	const FLinearColor IndustrialColors[] = {
		FLinearColor(0.55f, 0.55f, 0.55f),  // corrugated gray
		FLinearColor(0.60f, 0.35f, 0.20f),  // rust brown
		FLinearColor(0.40f, 0.50f, 0.38f),  // muted green
		FLinearColor(0.35f, 0.35f, 0.35f),  // dark gray
		FLinearColor(0.45f, 0.52f, 0.60f),  // weathered blue
	};
	const int32 NumIndustrialColors = UE_ARRAY_COUNT(IndustrialColors);

	const FLinearColor ArtsColors[] = {
		FLinearColor(0.95f, 0.25f, 0.55f),  // hot pink
		FLinearColor(0.15f, 0.85f, 0.75f),  // teal
		FLinearColor(0.95f, 0.55f, 0.10f),  // vibrant orange
		FLinearColor(0.55f, 0.20f, 0.85f),  // purple
		FLinearColor(0.95f, 0.90f, 0.15f),  // bright yellow
		FLinearColor(0.20f, 0.65f, 0.90f),  // sky blue
		FLinearColor(0.90f, 0.15f, 0.15f),  // bold red
		FLinearColor(0.30f, 0.85f, 0.30f),  // lime green
		FLinearColor(0.85f, 0.40f, 0.70f),  // magenta
		FLinearColor(0.10f, 0.10f, 0.10f),  // black (for contrast walls)
	};
	const int32 NumArtsColors = UE_ARRAY_COUNT(ArtsColors);

	// ---------------------------------------------------------------
	// 5. Block loop — 24 x 24 blocks
	// ---------------------------------------------------------------
	const float BlockW = RoadSpacing - RoadWidth - 2.f * SidewalkWidth; // ~1740

	for (int32 by = 0; by < NumBlocks; ++by)
	{
		for (int32 bx = 0; bx < NumBlocks; ++bx)
		{
			// Bay cutout
			if (bx >= 22 && by >= 21)
			{
				continue;
			}

			const float BlockCenterX = FirstRoad + (bx + 0.5f) * RoadSpacing;
			const float BlockCenterY = FirstRoad + (by + 0.5f) * RoadSpacing;
			const float BlockMinX = BlockCenterX - BlockW * 0.5f;
			const float BlockMinY = BlockCenterY - BlockW * 0.5f;
			const float BlockH = BlockW; // square blocks

			// ------- Landmark check (institutions removed) -------
			const int32 BlockKey = by * NumBlocks + bx;
			if (LandmarkKeys.Contains(BlockKey))
			{
				continue;
			}

			ECityDistrict District = GetDistrict(bx, by);

			switch (District)
			{
			// =======================================================
			// PARK
			// =======================================================
			case ECityDistrict::Park:
			{
				const bool bAddFountain = (bx == 7 && by == 18) || (bx == 13 && by == 12);
				SpawnParkBlock(FVector(BlockCenterX, BlockCenterY, 0.f), BlockW, bAddFountain);
				break;
			}

			// =======================================================
			// DOWNTOWN CORE — 2-4 tall buildings per block
			// =======================================================
			case ECityDistrict::DowntownCore:
			{
				const int32 Count = CityRandom.RandRange(2, 4);
				for (int32 n = 0; n < Count; ++n)
				{
					if (CityRandom.FRand() > 0.40f) continue;
					const float BldgW = CityRandom.FRandRange(180.f, 320.f);
					const float BldgD = CityRandom.FRandRange(180.f, 320.f);
					// Tight 2x2 grid placement
					const int32 Col = n % 2;
					const int32 Row = n / 2;
					const float OffX = BlockW * (0.28f + 0.44f * Col);
					const float OffY = BlockH * (0.28f + 0.44f * Row);
					const FVector Loc(BlockMinX + OffX, BlockMinY + OffY, 0.f);
					const FLinearColor& Color = DowntownColors[CityRandom.RandRange(0, NumDowntownColors - 1)];

					const float Roll = CityRandom.FRandRange(0.f, 1.f);
					if (Roll < 0.70f)
					{
						const float Height = CityRandom.FRandRange(1500.f, 4000.f);
						SpawnBuilding(Loc, FVector(BldgW, BldgD, Height), Color, true);
					}
					else
					{
						const int32 Stories = CityRandom.RandRange(2, 3);
						SpawnShop(Loc, BldgW, BldgD, Stories, Color,
							DowntownColors[CityRandom.RandRange(0, NumDowntownColors - 1)]);
					}
				}
				// Warm downtown ground light
				{
					AActor* DtActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
						FVector(BlockCenterX, BlockCenterY, 200.f), FRotator::ZeroRotator);
					if (DtActor)
					{
						UPointLightComponent* DtLight = NewObject<UPointLightComponent>(DtActor, TEXT("DtLight"));
						DtLight->SetWorldLocation(FVector(BlockCenterX, BlockCenterY, 200.f));
						DtLight->SetIntensity(5000.f);
						DtLight->SetAttenuationRadius(2000.f);
						DtLight->SetLightColor(FColor(255, 210, 150));
						DtLight->SetCastShadows(false);
						DtLight->RegisterComponent();
						DtActor->SetRootComponent(DtLight);
						DtActor->AddInstanceComponent(DtLight);
					}
				}
				break;
			}

			// =======================================================
			// MIDTOWN — removed (rendering performance)
			// =======================================================
			case ECityDistrict::Midtown:
				break;

			// =======================================================
			// RESIDENTIAL — removed (rendering performance)
			// =======================================================
			case ECityDistrict::Residential:
				break;

			case ECityDistrict::ArtsDistrict:
				break;

			case ECityDistrict::Waterfront:
				break;

			case ECityDistrict::Industrial:
				break;

			case ECityDistrict::Edge:
				break;

			default:
				break;
			}
		}
	}

	// ---------------------------------------------------------------
	// 6. Bus stops — ~20 shelters along major roads
	// ---------------------------------------------------------------
	UWorld* World = GetWorld();
	for (int32 s = 0; s < 20; ++s)
	{
		const int32 ri = CityRandom.RandRange(4, 20);
		const float RoadY = FirstRoad + ri * RoadSpacing;
		const float StopX = FirstRoad + CityRandom.RandRange(2, 21) * RoadSpacing
			+ CityRandom.FRandRange(-1500.f, 1500.f);

		const FVector StopLoc(StopX, RoadY + RoadWidth * 0.5f + SidewalkWidth + 50.f, 0.f);

		AActor* Stop = World->SpawnActor<AActor>(AActor::StaticClass(), StopLoc, FRotator::ZeroRotator);
		if (!Stop)
		{
			continue;
		}

		// Bench
		UStaticMeshComponent* Bench = NewObject<UStaticMeshComponent>(Stop, TEXT("Bench"));
		Bench->SetStaticMesh(CubeMesh);
		Bench->SetWorldScale3D(FVector(1.2f, 0.4f, 0.5f));
		Bench->SetWorldLocation(FVector(StopX, RoadY + RoadWidth * 0.5f + SidewalkWidth + 50.f, 40.f));
		Bench->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bench->RegisterComponent();
		Stop->SetRootComponent(Bench);
		Stop->AddInstanceComponent(Bench);

		// Shelter roof
		UStaticMeshComponent* Roof = NewObject<UStaticMeshComponent>(Stop, TEXT("Roof"));
		Roof->SetStaticMesh(CubeMesh);
		Roof->SetWorldScale3D(FVector(1.6f, 0.8f, 0.04f));
		Roof->SetWorldLocation(FVector(StopX, RoadY + RoadWidth * 0.5f + SidewalkWidth + 50.f, 200.f));
		Roof->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Roof->RegisterComponent();
		Stop->AddInstanceComponent(Roof);
	}
}

// ---------------------------------------------------------------------------
// Interior Block Streets — narrow cross streets inside each city block
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnBlockStreets()
{
	const int32 NumBlocks = NumRoads - 1; // 24
	const float BlockW = RoadSpacing - RoadWidth - 2.f * SidewalkWidth; // ~1740
	const float HalfSpan = BlockW * 0.5f;
	const float InteriorHalfW = 100.f; // 200 UU wide interior streets

	// Park block coordinates (same as SpawnCityGrid)
	auto IsPark = [](int32 bx, int32 by) -> bool
	{
		return (bx >= 7 && bx <= 8 && by >= 18 && by <= 19) ||
			   (bx >= 13 && bx <= 14 && by >= 12 && by <= 13);
	};

	for (int32 by = 0; by < NumBlocks; ++by)
	{
		for (int32 bx = 0; bx < NumBlocks; ++bx)
		{
			// Bay cutout
			if (bx >= 22 && by >= 21) continue;

			// Parks have walking paths, not streets
			if (IsPark(bx, by)) continue;

			const float CenterX = FirstRoad + (bx + 0.5f) * RoadSpacing;
			const float CenterY = FirstRoad + (by + 0.5f) * RoadSpacing;

			// East-west interior street
			SpawnRoadStrip(FVector(CenterX, CenterY, 2.f), HalfSpan, InteriorHalfW);

			// North-south interior street
			SpawnRoadStrip(FVector(CenterX, CenterY, 2.f), InteriorHalfW, HalfSpan);
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Interior block streets spawned."));
}

// ---------------------------------------------------------------------------
// Street Lights
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnStreetLights()
{
	// Inner zone: roads 3-21 (inclusive), full density
	for (int32 i = 3; i <= 21; ++i)
	{
		for (int32 j = 3; j <= 21; ++j)
		{
			const float IX = FirstRoad + i * RoadSpacing;
			const float IY = FirstRoad + j * RoadSpacing;
			const float Off = RoadWidth * 0.5f + 300.f;
			SpawnStreetLight(FVector(IX + Off, IY + Off, 0.f));
			SpawnStreetLight(FVector(IX - Off, IY - Off, 0.f));
		}
	}

	// Outer zone: sparse (every other intersection, 1 light only)
	for (int32 i = 0; i < NumRoads; ++i)
	{
		for (int32 j = 0; j < NumRoads; ++j)
		{
			if (i >= 3 && i <= 21 && j >= 3 && j <= 21) continue; // already done
			if ((i + j) % 2 != 0) continue;
			const float IX = FirstRoad + i * RoadSpacing;
			const float IY = FirstRoad + j * RoadSpacing;
			SpawnStreetLight(FVector(IX + RoadWidth * 0.5f + 300.f, IY + RoadWidth * 0.5f + 300.f, 0.f));
		}
	}

	// Midway lights on downtown/midtown avenues (roads 5-11)
	for (int32 i = 5; i <= 11; i += 2)
	{
		for (int32 j = 0; j < NumRoads - 1; ++j)
		{
			const float MX = FirstRoad + i * RoadSpacing;
			const float MY = FirstRoad + (j + 0.5f) * RoadSpacing;
			SpawnStreetLight(FVector(MX + RoadWidth * 0.5f + 300.f, MY, 0.f));
		}
	}
}

// ---------------------------------------------------------------------------
// Palm Trees
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnPalmTrees()
{
	// ---- City sidewalk palm trees ----
	// Every avenue, both sides — dense palm forest feel
	for (int32 i = 0; i < NumRoads; i += 1)
	{
		const float RoadX = FirstRoad + i * RoadSpacing;
		const float SidewalkOff = RoadWidth * 0.5f + SidewalkWidth * 0.5f;

		// Trees along the length of this avenue
		float Y = FirstRoad + 500.f;
		while (Y < -FirstRoad - 500.f)
		{
			const bool bInner = (i >= 5 && i <= 11);
			const float SkipChance = bInner ? 0.35f : 0.45f;

			if (CityRandom.FRand() > SkipChance)
			{
				const float H = CityRandom.FRandRange(600.f, 1200.f);
				SpawnPalmTree(FVector(RoadX, Y, 0.f), H, bInner);
			}
			if (CityRandom.FRand() > SkipChance)
			{
				const float H = CityRandom.FRandRange(600.f, 1200.f);
				SpawnPalmTree(FVector(RoadX + SidewalkOff * 2.f, Y, 0.f), H, bInner);
			}

			Y += CityRandom.FRandRange(800.f, 1800.f);
		}
	}

	// ---- East coast palm forest (deep band of trees) ----
	{
		// 8 rows from X=58000 to X=72000 (14000 UU deep)
		for (int32 row = 0; row < 8; ++row)
		{
			const float RowX = 58000.f + row * 2000.f;
			float Y = -70000.f;
			while (Y < 70000.f)
			{
				const float TX = RowX + CityRandom.FRandRange(-800.f, 800.f);
				const float H = CityRandom.FRandRange(600.f, 1400.f);
				SpawnPalmTree(FVector(TX, Y, 0.f), H, false);
				Y += CityRandom.FRandRange(400.f, 900.f);
			}
		}
	}

	// ---- South coast palm forest ----
	{
		for (int32 row = 0; row < 8; ++row)
		{
			const float RowY = -58000.f - row * 2000.f;
			float X = -65000.f;
			while (X < 65000.f)
			{
				const float TY = RowY + CityRandom.FRandRange(-800.f, 800.f);
				const float H = CityRandom.FRandRange(500.f, 1300.f);
				SpawnPalmTree(FVector(X, TY, 0.f), H, false);
				X += CityRandom.FRandRange(400.f, 900.f);
			}
		}
	}

	// ---- North coast palm forest (west of bay) ----
	{
		for (int32 row = 0; row < 6; ++row)
		{
			const float RowY = 58000.f + row * 2000.f;
			float X = -65000.f;
			while (X < 5000.f)
			{
				const float TY = RowY + CityRandom.FRandRange(-800.f, 800.f);
				const float H = CityRandom.FRandRange(500.f, 1200.f);
				SpawnPalmTree(FVector(X, TY, 0.f), H, false);
				X += CityRandom.FRandRange(400.f, 1000.f);
			}
		}
	}

	// ---- Bay shoreline palm forest ----
	{
		// South edge of bay — thicker
		for (int32 row = 0; row < 4; ++row)
		{
			float X = 30000.f;
			while (X < 58000.f)
			{
				const float TY = 51000.f + row * 1500.f + CityRandom.FRandRange(-500.f, 500.f);
				const float H = CityRandom.FRandRange(500.f, 1100.f);
				SpawnPalmTree(FVector(X, TY, 0.f), H, false);
				X += CityRandom.FRandRange(500.f, 1000.f);
			}
		}
		// West edge of bay — thicker
		for (int32 row = 0; row < 4; ++row)
		{
			float Y2 = 51000.f;
			while (Y2 < 70000.f)
			{
				const float TX = 30000.f + row * 1500.f + CityRandom.FRandRange(-500.f, 500.f);
				const float H = CityRandom.FRandRange(500.f, 1000.f);
				SpawnPalmTree(FVector(TX, Y2, 0.f), H, false);
				Y2 += CityRandom.FRandRange(500.f, 1000.f);
			}
		}
	}

	// ---- Waterfront district bonus palms ----
	{
		const float WFMinX = FirstRoad + 18 * RoadSpacing;
		const float WFMinY = FirstRoad + 17 * RoadSpacing;
		const float WFMaxX = FirstRoad + 22 * RoadSpacing;
		const float WFMaxY = FirstRoad + 21 * RoadSpacing;

		for (int32 t = 0; t < 80; ++t)
		{
			const float TX = CityRandom.FRandRange(WFMinX, WFMaxX);
			const float TY = CityRandom.FRandRange(WFMinY, WFMaxY);
			const float H = CityRandom.FRandRange(600.f, 1100.f);
			SpawnPalmTree(FVector(TX, TY, 0.f), H, false);
		}
	}

	// ---- Large palm forest patches near city edges ----
	// Southeast forest
	for (int32 t = 0; t < 200; ++t)
	{
		const float TX = CityRandom.FRandRange(45000.f, 58000.f);
		const float TY = CityRandom.FRandRange(-58000.f, -45000.f);
		const float H = CityRandom.FRandRange(500.f, 1300.f);
		SpawnPalmTree(FVector(TX, TY, 0.f), H, false);
	}
	// Northeast forest
	for (int32 t = 0; t < 200; ++t)
	{
		const float TX = CityRandom.FRandRange(45000.f, 58000.f);
		const float TY = CityRandom.FRandRange(45000.f, 58000.f);
		const float H = CityRandom.FRandRange(500.f, 1300.f);
		SpawnPalmTree(FVector(TX, TY, 0.f), H, false);
	}
	// Southwest forest
	for (int32 t = 0; t < 150; ++t)
	{
		const float TX = CityRandom.FRandRange(-58000.f, -45000.f);
		const float TY = CityRandom.FRandRange(-58000.f, -45000.f);
		const float H = CityRandom.FRandRange(500.f, 1200.f);
		SpawnPalmTree(FVector(TX, TY, 0.f), H, false);
	}
	// Northwest forest
	for (int32 t = 0; t < 150; ++t)
	{
		const float TX = CityRandom.FRandRange(-58000.f, -45000.f);
		const float TY = CityRandom.FRandRange(45000.f, 58000.f);
		const float H = CityRandom.FRandRange(500.f, 1200.f);
		SpawnPalmTree(FVector(TX, TY, 0.f), H, false);
	}
}

// ---------------------------------------------------------------------------
// Vehicles
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnVehicles()
{
	return;
}

// ---------------------------------------------------------------------------
// Launch ramps at city edges
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnRamps()
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh) return;

	const FLinearColor PadColor(0.25f, 0.25f, 0.28f);
	const FLinearColor StripeColor(0.85f, 0.75f, 0.1f);
	const FLinearColor ArrowColor(1.0f, 0.3f, 0.1f);

	const FVector RampLocations[] = {
		// Cardinal at outer edge (60000)
		FVector( 60000.f,      0.f, 3.f),
		FVector(-60000.f,      0.f, 3.f),
		FVector(     0.f,  60000.f, 3.f),
		FVector(     0.f, -60000.f, 3.f),
		// Diagonal at outer corners (42000, 42000)
		FVector( 42000.f,  42000.f, 3.f),
		FVector(-42000.f,  42000.f, 3.f),
		FVector( 42000.f, -42000.f, 3.f),
		FVector(-42000.f, -42000.f, 3.f),
		// Cardinal at inner ring (30000)
		FVector( 30000.f,      0.f, 3.f),
		FVector(-30000.f,      0.f, 3.f),
		FVector(     0.f,  30000.f, 3.f),
		FVector(     0.f, -30000.f, 3.f),
	};

	auto ApplyColor = [&](UStaticMeshComponent* MC, const FLinearColor& Color)
	{
		if (!MC) return;
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			MC->SetMaterial(0, Mat);
		}
	};

	for (int32 i = 0; i < UE_ARRAY_COUNT(RampLocations); ++i)
	{
		const FVector& Loc = RampLocations[i];

		AActor* RampActor = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!RampActor) continue;

		RampActor->Tags.Add(TEXT("LaunchPad"));
#if WITH_EDITOR
		RampActor->SetActorLabel(*FString::Printf(TEXT("LaunchPad_%d"), i));
#endif

		// Pad: CubeMesh scaled to 900x900x6 UU
		UStaticMeshComponent* PadMC = NewObject<UStaticMeshComponent>(RampActor, *FString::Printf(TEXT("Pad_%d"), i));
		PadMC->SetStaticMesh(CubeMesh);
		PadMC->SetWorldLocation(Loc);
		PadMC->SetWorldScale3D(FVector(9.0f, 9.0f, 0.06f));
		PadMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PadMC->SetCollisionResponseToAllChannels(ECR_Block);
		PadMC->RegisterComponent();
		RampActor->SetRootComponent(PadMC);
		ApplyColor(PadMC, PadColor);
		RampActor->AddInstanceComponent(PadMC);

		// Warning stripes: 2 yellow bands
		for (int32 s = 0; s < 2; ++s)
		{
			FVector Dir2D = FVector(Loc.X, Loc.Y, 0.f);
			Dir2D.Normalize();

			float StripeOffset = (s == 0) ? 200.f : -200.f;
			FVector StripeLoc = Loc + Dir2D * StripeOffset;
			StripeLoc.Z = Loc.Z + 2.f;

			float YawDeg = FMath::Atan2(Dir2D.Y, Dir2D.X) * (180.f / PI);

			UStaticMeshComponent* StripeMC = NewObject<UStaticMeshComponent>(RampActor, *FString::Printf(TEXT("Stripe_%d_%d"), i, s));
			StripeMC->SetStaticMesh(CubeMesh);
			StripeMC->SetWorldLocation(StripeLoc);
			StripeMC->SetWorldScale3D(FVector(0.3f, 8.0f, 0.04f));
			StripeMC->SetWorldRotation(FRotator(0.f, YawDeg, 0.f));
			StripeMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			StripeMC->RegisterComponent();
			ApplyColor(StripeMC, StripeColor);
			RampActor->AddInstanceComponent(StripeMC);
		}

		// Direction pointing away from city center
		FVector Dir2DRamp = FVector(Loc.X, Loc.Y, 0.f);
		Dir2DRamp.Normalize();
		float RampYaw = FMath::Atan2(Dir2DRamp.Y, Dir2DRamp.X) * (180.f / PI);

		// Arrow indicator pointing away from origin
		FVector ArrowLoc = Loc + Dir2DRamp * 300.f;
		ArrowLoc.Z = Loc.Z + 130.f;

		UStaticMeshComponent* ArrowMC = NewObject<UStaticMeshComponent>(RampActor, *FString::Printf(TEXT("Arrow_%d"), i));
		ArrowMC->SetStaticMesh(CubeMesh);
		ArrowMC->SetWorldLocation(ArrowLoc);
		ArrowMC->SetWorldScale3D(FVector(3.0f, 0.4f, 0.15f));
		ArrowMC->SetWorldRotation(FRotator(0.f, RampYaw, 0.f));
		ArrowMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ArrowMC->RegisterComponent();
		ApplyColor(ArrowMC, ArrowColor);
		RampActor->AddInstanceComponent(ArrowMC);

		// Angled ramp surface — CubeMesh tilted 25° pointing outward from city center
		// Ramp: 800 long, 700 wide, 30 thick, pitched up 25° in the launch direction
		const float RampPitch = 25.f;
		const float RampLen = 800.f;
		const float RampH = RampLen * FMath::Sin(FMath::DegreesToRadians(RampPitch)); // ~338 UU
		FVector RampCenter = Loc - Dir2DRamp * 100.f; // slightly behind pad center
		RampCenter.Z = Loc.Z + RampH * 0.5f;

		UStaticMeshComponent* RampMC = NewObject<UStaticMeshComponent>(RampActor, *FString::Printf(TEXT("Ramp_%d"), i));
		RampMC->SetStaticMesh(CubeMesh);
		RampMC->SetWorldLocation(RampCenter);
		RampMC->SetWorldScale3D(FVector(8.0f, 7.0f, 0.30f));
		// Pitch up in the launch direction: rotate around the perpendicular axis
		RampMC->SetWorldRotation(FRotator(RampPitch, RampYaw, 0.f));
		RampMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		RampMC->SetCollisionResponseToAllChannels(ECR_Block);
		RampMC->RegisterComponent();
		ApplyColor(RampMC, PadColor * 1.15f);
		RampActor->AddInstanceComponent(RampMC);

		// Side rails on the ramp
		for (int32 Rail = 0; Rail < 2; ++Rail)
		{
			const float RailSide = (Rail == 0) ? 350.f : -350.f;
			FVector RailOfs = FVector::CrossProduct(Dir2DRamp, FVector::UpVector) * RailSide;
			FVector RailLoc = RampCenter + RailOfs;

			UStaticMeshComponent* RailMC = NewObject<UStaticMeshComponent>(RampActor, *FString::Printf(TEXT("Rail_%d_%d"), i, Rail));
			RailMC->SetStaticMesh(CubeMesh);
			RailMC->SetWorldLocation(RailLoc);
			RailMC->SetWorldScale3D(FVector(8.0f, 0.2f, 0.6f));
			RailMC->SetWorldRotation(FRotator(RampPitch, RampYaw, 0.f));
			RailMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			RailMC->SetCollisionResponseToAllChannels(ECR_Block);
			RailMC->RegisterComponent();
			ApplyColor(RailMC, StripeColor);
			RampActor->AddInstanceComponent(RailMC);
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Spawned 12 launch pads at city edges."));
}

// ---------------------------------------------------------------------------
// SpawnHouse -- Brookhaven-style suburban house with pitched roof and lawn
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnHouse(const FVector& Loc, float W, float D, int32 Stories,
	const FLinearColor& WallColor, const FLinearColor& RoofColor)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh || !PlaneMesh) return;

	const float StoryH = 280.f;
	const float BodyH = Stories * StoryH;
	const float BodyZ = BodyH * 0.5f;

	AActor* House = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
	if (!House) return;
#if WITH_EDITOR
	House->SetActorLabel(TEXT("House"));
#endif

	// ---- Body cube ----
	UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(House, TEXT("HouseBody"));
	Body->SetStaticMesh(CubeMesh);
	Body->SetWorldScale3D(FVector(W / 100.f, D / 100.f, BodyH / 100.f));
	Body->SetWorldLocation(FVector(Loc.X, Loc.Y, BodyZ));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->RegisterComponent();
	House->SetRootComponent(Body);
	UMaterialInstanceDynamic* BodyMat = UMaterialInstanceDynamic::Create(Body->GetMaterial(0), Body);
	if (BodyMat) { BodyMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor); BodyMat->SetScalarParameterValue(TEXT("Roughness"), 0.92f); Body->SetMaterial(0, BodyMat); }
	House->AddInstanceComponent(Body);

	// ---- Brick corner pilasters (rounded cylinders soften sharp corners) ----
	if (CylinderMesh)
	{
		const float PilR = 8.f;
		const float PilH = BodyH;
		const FLinearColor PilColor = WallColor * 1.08f; // slightly lighter
		for (int32 Ci = 0; Ci < 4; ++Ci)
		{
			const float CXSign = (Ci & 1) ? 1.f : -1.f;
			const float CYSign = (Ci & 2) ? 1.f : -1.f;
			FString PName = FString::Printf(TEXT("HousePilaster_%d"), Ci);
			UStaticMeshComponent* Pil = NewObject<UStaticMeshComponent>(House, *PName);
			Pil->SetStaticMesh(CylinderMesh);
			Pil->SetWorldScale3D(FVector(PilR / 50.f, PilR / 50.f, PilH / 100.f));
			Pil->SetWorldLocation(FVector(Loc.X + CXSign * W * 0.5f, Loc.Y + CYSign * D * 0.5f, PilH * 0.5f));
			Pil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Pil->RegisterComponent();
			UMaterialInstanceDynamic* PilMat = UMaterialInstanceDynamic::Create(Pil->GetMaterial(0), Pil);
			if (PilMat) { PilMat->SetVectorParameterValue(TEXT("BaseColor"), PilColor); PilMat->SetScalarParameterValue(TEXT("Roughness"), 0.90f); Pil->SetMaterial(0, PilMat); }
			House->AddInstanceComponent(Pil);
		}
	}

	// ---- Pitched roof (two sloped halves forming a gable) ----
	const float RoofOverhang = 40.f;
	const float RoofW = W + RoofOverhang * 2.f;
	const float RoofSlabD = (D * 0.5f + RoofOverhang) / FMath::Cos(FMath::DegreesToRadians(30.f));
	const float RoofThick = 18.f;
	const float RoofPeakZ = BodyH + D * 0.5f * FMath::Tan(FMath::DegreesToRadians(30.f));
	const float RoofMidZ = (BodyH + RoofPeakZ) * 0.5f;

	for (int32 Side = 0; Side < 2; ++Side)
	{
		const float Sign = (Side == 0) ? 1.f : -1.f;
		FString RName = FString::Printf(TEXT("Roof_%d"), Side);
		UStaticMeshComponent* Roof = NewObject<UStaticMeshComponent>(House, *RName);
		Roof->SetStaticMesh(CubeMesh);
		Roof->SetWorldScale3D(FVector(RoofW / 100.f, RoofSlabD / 100.f, RoofThick / 100.f));
		Roof->SetWorldLocation(FVector(Loc.X, Loc.Y + Sign * D * 0.25f, RoofMidZ));
		Roof->SetWorldRotation(FRotator(Sign * 30.f, 0.f, 0.f));
		Roof->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Roof->SetCollisionResponseToAllChannels(ECR_Block);
		Roof->RegisterComponent();
		UMaterialInstanceDynamic* RMat = UMaterialInstanceDynamic::Create(Roof->GetMaterial(0), Roof);
		if (RMat) { RMat->SetVectorParameterValue(TEXT("BaseColor"), RoofColor); RMat->SetScalarParameterValue(TEXT("Roughness"), 0.65f); Roof->SetMaterial(0, RMat); }
		House->AddInstanceComponent(Roof);
	}

	// Ridge cap along peak.
	UStaticMeshComponent* Ridge = NewObject<UStaticMeshComponent>(House, TEXT("RoofRidge"));
	Ridge->SetStaticMesh(CubeMesh);
	Ridge->SetWorldScale3D(FVector(RoofW / 100.f, 0.15f, 0.12f));
	Ridge->SetWorldLocation(FVector(Loc.X, Loc.Y, RoofPeakZ));
	Ridge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Ridge->RegisterComponent();
	UMaterialInstanceDynamic* RidgeMat = UMaterialInstanceDynamic::Create(Ridge->GetMaterial(0), Ridge);
	if (RidgeMat) { RidgeMat->SetVectorParameterValue(TEXT("BaseColor"), RoofColor * 0.8f); RidgeMat->SetScalarParameterValue(TEXT("Roughness"), 0.6f); Ridge->SetMaterial(0, RidgeMat); }
	House->AddInstanceComponent(Ridge);

	// ---- Windows (front and back faces) ----
	const FLinearColor DarkGlass(0.06f, 0.07f, 0.10f);
	const FLinearColor WarmColors[] = {
		FLinearColor(0.95f, 0.85f, 0.40f),
		FLinearColor(0.90f, 0.65f, 0.30f),
		FLinearColor(0.85f, 0.82f, 0.72f),
		FLinearColor(0.45f, 0.55f, 0.82f),
	};
	const float WinW = 60.f;
	const float WinH = 70.f;
	const int32 WinsPerFloor = FMath::Clamp(FMath::FloorToInt(W / 220.f), 2, 3);
	int32 WIdx = 0;

	for (int32 Floor = 0; Floor < Stories; ++Floor)
	{
		const float FloorZ = Floor * StoryH + StoryH * 0.6f;
		for (int32 Face = 0; Face < 2; ++Face)
		{
			const float FaceY = Loc.Y + (Face == 0 ? D * 0.5f + 3.f : -D * 0.5f - 3.f);
			for (int32 Wi = 0; Wi < WinsPerFloor; ++Wi)
			{
				const float WX = Loc.X + (Wi - (WinsPerFloor - 1) * 0.5f) * (W / (WinsPerFloor + 1));
				FString WName = FString::Printf(TEXT("HWin_%d"), WIdx++);
				UStaticMeshComponent* Win = NewObject<UStaticMeshComponent>(House, *WName);
				Win->SetStaticMesh(CubeMesh);
				Win->SetWorldScale3D(FVector(WinW / 100.f, 3.f / 100.f, WinH / 100.f));
				Win->SetWorldLocation(FVector(WX, FaceY, FloorZ));
				Win->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Win->RegisterComponent();
				House->AddInstanceComponent(Win);

				UMaterialInstanceDynamic* WMat = UMaterialInstanceDynamic::Create(Win->GetMaterial(0), Win);
				if (WMat)
				{
					WMat->SetVectorParameterValue(TEXT("BaseColor"), DarkGlass);
					WMat->SetScalarParameterValue(TEXT("Metallic"), 0.1f);
					WMat->SetScalarParameterValue(TEXT("Roughness"), 0.05f);
					Win->SetMaterial(0, WMat);
					NightWindowMaterials.Add(WMat);
					NightWindowLitColors.Add(WarmColors[CityRandom.RandRange(0, 3)]);
					NightWindowLitStates.Add(CityRandom.FRand() < 0.7f);
				}
			}
		}
	}

	// ---- Front door ----
	{
		UStaticMeshComponent* Door = NewObject<UStaticMeshComponent>(House, TEXT("FrontDoor"));
		Door->SetStaticMesh(CubeMesh);
		Door->SetWorldScale3D(FVector(0.55f, 3.f / 100.f, 1.2f));
		Door->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + 3.f, 60.f));
		Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Door->RegisterComponent();
		UMaterialInstanceDynamic* DMat = UMaterialInstanceDynamic::Create(Door->GetMaterial(0), Door);
		if (DMat) { DMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.18f, 0.12f, 0.06f)); DMat->SetScalarParameterValue(TEXT("Roughness"), 0.7f); Door->SetMaterial(0, DMat); }
		House->AddInstanceComponent(Door);

		// Doorway arch (half-cylinder above door)
		if (CylinderMesh)
		{
			UStaticMeshComponent* Arch = NewObject<UStaticMeshComponent>(House, TEXT("DoorArch"));
			Arch->SetStaticMesh(CylinderMesh);
			Arch->SetWorldScale3D(FVector(0.35f, 0.08f, 0.04f));
			Arch->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + 4.f, 128.f));
			Arch->SetWorldRotation(FRotator(0.f, 0.f, 90.f));
			Arch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Arch->RegisterComponent();
			UMaterialInstanceDynamic* ArMat = UMaterialInstanceDynamic::Create(Arch->GetMaterial(0), Arch);
			if (ArMat) { ArMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor * 0.85f); ArMat->SetScalarParameterValue(TEXT("Roughness"), 0.85f); Arch->SetMaterial(0, ArMat); }
			House->AddInstanceComponent(Arch);
		}
	}

	// ---- Lawn (visual only — character walks on roads/sidewalks) ----
	{
		UStaticMeshComponent* Lawn = NewObject<UStaticMeshComponent>(House, TEXT("Lawn"));
		Lawn->SetStaticMesh(CubeMesh);
		Lawn->SetWorldScale3D(FVector((W + 200.f) / 100.f, (D + 200.f) / 100.f, 0.02f));
		Lawn->SetWorldLocation(FVector(Loc.X, Loc.Y, 3.f)); // top face at Z=4
		Lawn->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Lawn->RegisterComponent();
		UMaterialInstanceDynamic* LMat = UMaterialInstanceDynamic::Create(Lawn->GetMaterial(0), Lawn);
		if (LMat) { LMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.15f, 0.35f, 0.08f)); LMat->SetScalarParameterValue(TEXT("Roughness"), 0.9f); Lawn->SetMaterial(0, LMat); }
		House->AddInstanceComponent(Lawn);
	}

	// ---- Foundation bushes (sphere clusters at base of walls) ----
	if (SphereMesh && DetailRandom.FRand() < 0.60f)
	{
		const int32 NumBushes = DetailRandom.RandRange(2, 4);
		for (int32 Bi = 0; Bi < NumBushes; ++Bi)
		{
			const float BushX = Loc.X + DetailRandom.FRandRange(-W * 0.4f, W * 0.4f);
			const float BushY = Loc.Y + (DetailRandom.FRand() < 0.5f ? D * 0.48f : -D * 0.48f);
			const float BushScale = DetailRandom.FRandRange(0.4f, 0.7f);
			FString BName = FString::Printf(TEXT("HouseBush_%d"), Bi);
			UStaticMeshComponent* Bush = NewObject<UStaticMeshComponent>(House, *BName);
			Bush->SetStaticMesh(SphereMesh);
			Bush->SetWorldScale3D(FVector(BushScale, BushScale, BushScale * 0.7f));
			Bush->SetWorldLocation(FVector(BushX, BushY, BushScale * 20.f));
			Bush->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Bush->RegisterComponent();
			UMaterialInstanceDynamic* BuMat = UMaterialInstanceDynamic::Create(Bush->GetMaterial(0), Bush);
			if (BuMat) { BuMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.10f, 0.28f + DetailRandom.FRandRange(-0.05f, 0.05f), 0.06f)); BuMat->SetScalarParameterValue(TEXT("Roughness"), 0.92f); Bush->SetMaterial(0, BuMat); }
			House->AddInstanceComponent(Bush);
		}
	}

	// ---- Optional garage (30% chance) ----
	if (CityRandom.FRand() < 0.30f)
	{
		const float GW = W * 0.35f;
		const float GD = D * 0.55f;
		const float GH = StoryH * 0.85f;
		const float GX = Loc.X + W * 0.5f + GW * 0.5f + 10.f;

		UStaticMeshComponent* Garage = NewObject<UStaticMeshComponent>(House, TEXT("Garage"));
		Garage->SetStaticMesh(CubeMesh);
		Garage->SetWorldScale3D(FVector(GW / 100.f, GD / 100.f, GH / 100.f));
		Garage->SetWorldLocation(FVector(GX, Loc.Y, GH * 0.5f));
		Garage->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Garage->SetCollisionResponseToAllChannels(ECR_Block);
		Garage->RegisterComponent();
		UMaterialInstanceDynamic* GMat = UMaterialInstanceDynamic::Create(Garage->GetMaterial(0), Garage);
		if (GMat) { GMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor); GMat->SetScalarParameterValue(TEXT("Roughness"), 0.7f); Garage->SetMaterial(0, GMat); }
		House->AddInstanceComponent(Garage);

		// Garage door face.
		UStaticMeshComponent* GDoor = NewObject<UStaticMeshComponent>(House, TEXT("GarageDoor"));
		GDoor->SetStaticMesh(CubeMesh);
		GDoor->SetWorldScale3D(FVector(GW * 0.8f / 100.f, 3.f / 100.f, GH * 0.7f / 100.f));
		GDoor->SetWorldLocation(FVector(GX, Loc.Y + GD * 0.5f + 3.f, GH * 0.35f));
		GDoor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GDoor->RegisterComponent();
		UMaterialInstanceDynamic* GDMat = UMaterialInstanceDynamic::Create(GDoor->GetMaterial(0), GDoor);
		if (GDMat) { GDMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.22f, 0.20f, 0.18f)); GDMat->SetScalarParameterValue(TEXT("Roughness"), 0.6f); GDoor->SetMaterial(0, GDMat); }
		House->AddInstanceComponent(GDoor);
	}

	// ---- Chimney (35% chance) -- round cylinder with cone cap ----
	if (DetailRandom.FRand() < 0.35f && CylinderMesh && ConeMesh)
	{
		const float ChR = 16.f;
		const float ChH = 120.f;
		const float ChX = Loc.X + W * 0.25f;
		const float ChZ = BodyH + D * 0.5f * FMath::Tan(FMath::DegreesToRadians(30.f)) * 0.5f;
		UStaticMeshComponent* Chimney = NewObject<UStaticMeshComponent>(House, TEXT("Chimney"));
		Chimney->SetStaticMesh(CylinderMesh);
		Chimney->SetWorldScale3D(FVector(ChR / 50.f, ChR / 50.f, ChH / 100.f));
		Chimney->SetWorldLocation(FVector(ChX, Loc.Y - D * 0.2f, ChZ + ChH * 0.5f));
		Chimney->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Chimney->RegisterComponent();
		UMaterialInstanceDynamic* ChMat = UMaterialInstanceDynamic::Create(Chimney->GetMaterial(0), Chimney);
		if (ChMat) { ChMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.45f, 0.25f, 0.18f)); ChMat->SetScalarParameterValue(TEXT("Roughness"), 0.8f); Chimney->SetMaterial(0, ChMat); }
		House->AddInstanceComponent(Chimney);

		// Cone cap
		UStaticMeshComponent* Cap = NewObject<UStaticMeshComponent>(House, TEXT("ChimneyCap"));
		Cap->SetStaticMesh(ConeMesh);
		Cap->SetWorldScale3D(FVector((ChR + 5.f) / 50.f, (ChR + 5.f) / 50.f, 0.35f));
		Cap->SetWorldLocation(FVector(ChX, Loc.Y - D * 0.2f, ChZ + ChH + 12.f));
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cap->RegisterComponent();
		UMaterialInstanceDynamic* CapMat = UMaterialInstanceDynamic::Create(Cap->GetMaterial(0), Cap);
		if (CapMat) { CapMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.3f, 0.3f, 0.28f)); CapMat->SetScalarParameterValue(TEXT("Roughness"), 0.8f); Cap->SetMaterial(0, CapMat); }
		House->AddInstanceComponent(Cap);
	}

	// ---- Front porch (40% chance) ----
	if (DetailRandom.FRand() < 0.40f)
	{
		const float PorchW = W * 0.6f;
		const float PorchD = 80.f;
		const float PorchH = 8.f;
		// Platform
		UStaticMeshComponent* Porch = NewObject<UStaticMeshComponent>(House, TEXT("Porch"));
		Porch->SetStaticMesh(CubeMesh);
		Porch->SetWorldScale3D(FVector(PorchW / 100.f, PorchD / 100.f, PorchH / 100.f));
		Porch->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + PorchD * 0.5f, PorchH * 0.5f));
		Porch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Porch->RegisterComponent();
		UMaterialInstanceDynamic* PchMat = UMaterialInstanceDynamic::Create(Porch->GetMaterial(0), Porch);
		if (PchMat) { PchMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.40f, 0.30f, 0.20f)); PchMat->SetScalarParameterValue(TEXT("Roughness"), 0.7f); Porch->SetMaterial(0, PchMat); }
		House->AddInstanceComponent(Porch);

		// Two support posts
		if (CylinderMesh)
		{
			for (int32 Ps = 0; Ps < 2; ++Ps)
			{
				const float Sign = (Ps == 0) ? -1.f : 1.f;
				FString PName = FString::Printf(TEXT("PorchPost_%d"), Ps);
				UStaticMeshComponent* Post = NewObject<UStaticMeshComponent>(House, *PName);
				Post->SetStaticMesh(CylinderMesh);
				Post->SetWorldScale3D(FVector(0.08f, 0.08f, 1.4f));
				Post->SetWorldLocation(FVector(Loc.X + Sign * PorchW * 0.4f, Loc.Y + D * 0.5f + PorchD, 70.f));
				Post->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Post->RegisterComponent();
				UMaterialInstanceDynamic* PostMat = UMaterialInstanceDynamic::Create(Post->GetMaterial(0), Post);
				if (PostMat) { PostMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.85f, 0.85f, 0.82f)); PostMat->SetScalarParameterValue(TEXT("Roughness"), 0.7f); Post->SetMaterial(0, PostMat); }
				House->AddInstanceComponent(Post);
			}
		}

		// Porch roof
		UStaticMeshComponent* PRoof = NewObject<UStaticMeshComponent>(House, TEXT("PorchRoof"));
		PRoof->SetStaticMesh(CubeMesh);
		PRoof->SetWorldScale3D(FVector((PorchW + 20.f) / 100.f, (PorchD + 20.f) / 100.f, 0.06f));
		PRoof->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + PorchD * 0.5f, 145.f));
		PRoof->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PRoof->RegisterComponent();
		UMaterialInstanceDynamic* PRMat = UMaterialInstanceDynamic::Create(PRoof->GetMaterial(0), PRoof);
		if (PRMat) { PRMat->SetVectorParameterValue(TEXT("BaseColor"), RoofColor); PRMat->SetScalarParameterValue(TEXT("Roughness"), 0.65f); PRoof->SetMaterial(0, PRMat); }
		House->AddInstanceComponent(PRoof);
	}
}

// ---------------------------------------------------------------------------
// SpawnShop -- Brookhaven-style commercial storefront with awning
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnShop(const FVector& Loc, float W, float D, int32 Stories,
	const FLinearColor& WallColor, const FLinearColor& AwningColor)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh || !PlaneMesh) return;

	const float StoryH = 300.f;
	const float BodyH = Stories * StoryH;
	const float BodyZ = BodyH * 0.5f;

	AActor* Shop = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
	if (!Shop) return;
#if WITH_EDITOR
	Shop->SetActorLabel(TEXT("Shop"));
#endif

	// ---- Body cube ----
	UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(Shop, TEXT("ShopBody"));
	Body->SetStaticMesh(CubeMesh);
	Body->SetWorldScale3D(FVector(W / 100.f, D / 100.f, BodyH / 100.f));
	Body->SetWorldLocation(FVector(Loc.X, Loc.Y, BodyZ));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->RegisterComponent();
	Shop->SetRootComponent(Body);
	UMaterialInstanceDynamic* BodyMat = UMaterialInstanceDynamic::Create(Body->GetMaterial(0), Body);
	if (BodyMat) { BodyMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor); BodyMat->SetScalarParameterValue(TEXT("Roughness"), 0.55f); Body->SetMaterial(0, BodyMat); }
	Shop->AddInstanceComponent(Body);

	// ---- Front face corner trim (decorative vertical cylinders) ----
	if (CylinderMesh)
	{
		for (int32 Ti = 0; Ti < 2; ++Ti)
		{
			const float TSign = (Ti == 0) ? -1.f : 1.f;
			FString TName = FString::Printf(TEXT("ShopTrim_%d"), Ti);
			UStaticMeshComponent* Trim = NewObject<UStaticMeshComponent>(Shop, *TName);
			Trim->SetStaticMesh(CylinderMesh);
			Trim->SetWorldScale3D(FVector(0.06f, 0.06f, BodyH / 100.f));
			Trim->SetWorldLocation(FVector(Loc.X + TSign * W * 0.5f, Loc.Y + D * 0.5f + 2.f, BodyH * 0.5f));
			Trim->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Trim->RegisterComponent();
			UMaterialInstanceDynamic* TrMat = UMaterialInstanceDynamic::Create(Trim->GetMaterial(0), Trim);
			if (TrMat) { TrMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor * 0.85f); TrMat->SetScalarParameterValue(TEXT("Roughness"), 0.5f); Trim->SetMaterial(0, TrMat); }
			Shop->AddInstanceComponent(Trim);
		}
	}

	// ---- Roof parapet (thin strip along top) ----
	{
		UStaticMeshComponent* Parapet = NewObject<UStaticMeshComponent>(Shop, TEXT("Parapet"));
		Parapet->SetStaticMesh(CubeMesh);
		Parapet->SetWorldScale3D(FVector((W + 10.f) / 100.f, (D + 10.f) / 100.f, 0.25f));
		Parapet->SetWorldLocation(FVector(Loc.X, Loc.Y, BodyH + 12.f));
		Parapet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Parapet->RegisterComponent();
		UMaterialInstanceDynamic* PMat = UMaterialInstanceDynamic::Create(Parapet->GetMaterial(0), Parapet);
		if (PMat) { PMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor * 0.9f); PMat->SetScalarParameterValue(TEXT("Roughness"), 0.55f); Parapet->SetMaterial(0, PMat); }
		Shop->AddInstanceComponent(Parapet);
	}

	// ---- Storefront windows (large glass on ground floor front face) ----
	const FLinearColor DarkGlass(0.06f, 0.07f, 0.10f);
	const FLinearColor WarmColors[] = {
		FLinearColor(0.95f, 0.85f, 0.40f),
		FLinearColor(0.90f, 0.65f, 0.30f),
		FLinearColor(0.85f, 0.82f, 0.72f),
		FLinearColor(0.45f, 0.55f, 0.82f),
	};
	int32 WIdx = 0;

	{
		const float SFW = W * 0.80f;
		const float SFH = StoryH * 0.55f;
		UStaticMeshComponent* SF = NewObject<UStaticMeshComponent>(Shop, TEXT("Storefront"));
		SF->SetStaticMesh(CubeMesh);
		SF->SetWorldScale3D(FVector(SFW / 100.f, 3.f / 100.f, SFH / 100.f));
		SF->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + 3.f, SFH * 0.5f + 30.f));
		SF->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SF->RegisterComponent();
		Shop->AddInstanceComponent(SF);

		UMaterialInstanceDynamic* SFMat = UMaterialInstanceDynamic::Create(SF->GetMaterial(0), SF);
		if (SFMat)
		{
			SFMat->SetVectorParameterValue(TEXT("BaseColor"), DarkGlass);
			SFMat->SetScalarParameterValue(TEXT("Metallic"), 0.1f);
			SFMat->SetScalarParameterValue(TEXT("Roughness"), 0.05f);
			SF->SetMaterial(0, SFMat);
			NightWindowMaterials.Add(SFMat);
			NightWindowLitColors.Add(WarmColors[CityRandom.RandRange(0, 3)]);
			NightWindowLitStates.Add(CityRandom.FRand() < 0.8f);
		}
	}

	// ---- Awning (colored cube protruding from front) ----
	{
		const float AwningProtrude = 90.f;
		const float AwningZ = StoryH * 0.75f;
		UStaticMeshComponent* Awning = NewObject<UStaticMeshComponent>(Shop, TEXT("Awning"));
		Awning->SetStaticMesh(CubeMesh);
		Awning->SetWorldScale3D(FVector(W * 0.85f / 100.f, AwningProtrude / 100.f, 0.12f));
		Awning->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + AwningProtrude * 0.5f, AwningZ));
		Awning->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Awning->RegisterComponent();
		UMaterialInstanceDynamic* AMat = UMaterialInstanceDynamic::Create(Awning->GetMaterial(0), Awning);
		if (AMat) { AMat->SetVectorParameterValue(TEXT("BaseColor"), AwningColor); AMat->SetScalarParameterValue(TEXT("Roughness"), 0.6f); Awning->SetMaterial(0, AMat); }
		Shop->AddInstanceComponent(Awning);

		// Awning support posts (cylinders)
		if (CylinderMesh)
		{
			for (int32 Ai = 0; Ai < 2; ++Ai)
			{
				const float ASign = (Ai == 0) ? -1.f : 1.f;
				FString AName = FString::Printf(TEXT("AwningPost_%d"), Ai);
				UStaticMeshComponent* APost = NewObject<UStaticMeshComponent>(Shop, *AName);
				APost->SetStaticMesh(CylinderMesh);
				APost->SetWorldScale3D(FVector(0.04f, 0.04f, AwningZ / 100.f));
				APost->SetWorldLocation(FVector(Loc.X + ASign * W * 0.38f, Loc.Y + D * 0.5f + AwningProtrude, AwningZ * 0.5f));
				APost->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				APost->RegisterComponent();
				UMaterialInstanceDynamic* APMat = UMaterialInstanceDynamic::Create(APost->GetMaterial(0), APost);
				if (APMat) { APMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.20f, 0.20f, 0.22f)); APMat->SetScalarParameterValue(TEXT("Metallic"), 0.7f); APMat->SetScalarParameterValue(TEXT("Roughness"), 0.3f); APost->SetMaterial(0, APMat); }
				Shop->AddInstanceComponent(APost);
			}
		}
	}

	// ---- Sign strip (above awning) ----
	{
		const float SignZ = StoryH * 0.85f;
		UStaticMeshComponent* Sign = NewObject<UStaticMeshComponent>(Shop, TEXT("SignStrip"));
		Sign->SetStaticMesh(CubeMesh);
		Sign->SetWorldScale3D(FVector(W * 0.70f / 100.f, 3.f / 100.f, 0.30f));
		Sign->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + 5.f, SignZ));
		Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Sign->RegisterComponent();
		UMaterialInstanceDynamic* SMat = UMaterialInstanceDynamic::Create(Sign->GetMaterial(0), Sign);
		if (SMat) { SMat->SetVectorParameterValue(TEXT("BaseColor"), AwningColor * 1.15f); SMat->SetScalarParameterValue(TEXT("Roughness"), 0.3f); Sign->SetMaterial(0, SMat); }
		Shop->AddInstanceComponent(Sign);
	}

	// ---- Upper windows (if 2-story) ----
	if (Stories >= 2)
	{
		const float WinW = 55.f;
		const float WinH = 65.f;
		const int32 NumWins = FMath::Clamp(FMath::FloorToInt(W / 200.f), 2, 4);
		for (int32 Wi = 0; Wi < NumWins; ++Wi)
		{
			const float WX = Loc.X + (Wi - (NumWins - 1) * 0.5f) * (W / (NumWins + 1));
			const float WZ = StoryH + StoryH * 0.5f;
			FString WName = FString::Printf(TEXT("SWin_%d"), WIdx++);
			UStaticMeshComponent* Win = NewObject<UStaticMeshComponent>(Shop, *WName);
			Win->SetStaticMesh(CubeMesh);
			Win->SetWorldScale3D(FVector(WinW / 100.f, 3.f / 100.f, WinH / 100.f));
			Win->SetWorldLocation(FVector(WX, Loc.Y + D * 0.5f + 3.f, WZ));
			Win->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Win->RegisterComponent();
			Shop->AddInstanceComponent(Win);

			UMaterialInstanceDynamic* WMat = UMaterialInstanceDynamic::Create(Win->GetMaterial(0), Win);
			if (WMat)
			{
				WMat->SetVectorParameterValue(TEXT("BaseColor"), DarkGlass);
				WMat->SetScalarParameterValue(TEXT("Metallic"), 0.1f);
				WMat->SetScalarParameterValue(TEXT("Roughness"), 0.05f);
				Win->SetMaterial(0, WMat);
				NightWindowMaterials.Add(WMat);
				NightWindowLitColors.Add(WarmColors[CityRandom.RandRange(0, 3)]);
				NightWindowLitStates.Add(CityRandom.FRand() < 0.65f);
			}
		}
	}

	// ---- Sidewalk pad (visual only — character walks on roads/sidewalks) ----
	{
		UStaticMeshComponent* Pad = NewObject<UStaticMeshComponent>(Shop, TEXT("SidewalkPad"));
		Pad->SetStaticMesh(CubeMesh);
		Pad->SetWorldScale3D(FVector(W / 100.f, 1.2f, 0.02f));
		Pad->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + 60.f, 3.f)); // top face at Z=4
		Pad->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Pad->RegisterComponent();
		UMaterialInstanceDynamic* PMat = UMaterialInstanceDynamic::Create(Pad->GetMaterial(0), Pad);
		if (PMat) { PMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.55f, 0.53f, 0.50f)); Pad->SetMaterial(0, PMat); }
		Shop->AddInstanceComponent(Pad);
	}

	// ---- Front door ----
	{
		UStaticMeshComponent* Door = NewObject<UStaticMeshComponent>(Shop, TEXT("ShopDoor"));
		Door->SetStaticMesh(CubeMesh);
		Door->SetWorldScale3D(FVector(0.55f, 3.f / 100.f, 1.2f));
		Door->SetWorldLocation(FVector(Loc.X - W * 0.15f, Loc.Y + D * 0.5f + 3.f, 60.f));
		Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Door->RegisterComponent();
		UMaterialInstanceDynamic* DoorMat = UMaterialInstanceDynamic::Create(Door->GetMaterial(0), Door);
		if (DoorMat) { DoorMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.18f, 0.12f, 0.06f)); Door->SetMaterial(0, DoorMat); }
		Shop->AddInstanceComponent(Door);
	}

	// ---- Neon sign (50% chance, glows at night via NightWindowMaterials) ----
	if (DetailRandom.FRand() < 0.50f)
	{
		const FLinearColor NeonColors[] = {
			FLinearColor(3.0f, 0.3f, 0.5f),   // hot pink
			FLinearColor(0.3f, 0.5f, 3.0f),   // electric blue
			FLinearColor(3.0f, 1.5f, 0.2f),   // amber gold
			FLinearColor(0.2f, 3.0f, 0.5f),   // neon green
			FLinearColor(2.5f, 0.2f, 2.5f),   // purple
		};
		const FLinearColor& NeonCol = NeonColors[DetailRandom.RandRange(0, 4)];
		const float NeonW = W * 0.45f;
		const float NeonH = 28.f;
		const float NeonZ = StoryH * 0.92f;

		UStaticMeshComponent* Neon = NewObject<UStaticMeshComponent>(Shop, TEXT("NeonSign"));
		Neon->SetStaticMesh(CylinderMesh ? CylinderMesh : CubeMesh);
		if (CylinderMesh)
		{
			// Horizontal tube neon sign
			Neon->SetWorldScale3D(FVector(NeonW / 100.f, NeonH * 0.5f / 50.f, NeonH * 0.5f / 50.f));
			Neon->SetWorldRotation(FRotator(0.f, 0.f, 90.f));
		}
		else
		{
			Neon->SetWorldScale3D(FVector(NeonW / 100.f, 3.f / 100.f, NeonH / 100.f));
		}
		Neon->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + 6.f, NeonZ));
		Neon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Neon->RegisterComponent();
		Shop->AddInstanceComponent(Neon);

		UMaterialInstanceDynamic* NMat = UMaterialInstanceDynamic::Create(Neon->GetMaterial(0), Neon);
		if (NMat)
		{
			NMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.08f, 0.08f, 0.10f));
			NMat->SetScalarParameterValue(TEXT("Roughness"), 0.3f);
			Neon->SetMaterial(0, NMat);
			NightWindowMaterials.Add(NMat);
			NightWindowLitColors.Add(NeonCol);
			NightWindowLitStates.Add(true); // neon always on at night
		}
	}
}

// ---------------------------------------------------------------------------
// SpawnInstitution -- Brookhaven-style institutional building
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnInstitution(const FVector& Loc, float W, float D, float H,
	const FLinearColor& WallColor, const FLinearColor& AccentColor)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh) return;

	const float BodyZ = H * 0.5f;

	AActor* Inst = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
	if (!Inst) return;
#if WITH_EDITOR
	Inst->SetActorLabel(TEXT("Institution"));
#endif

	// ---- Body cube ----
	UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(Inst, TEXT("InstBody"));
	Body->SetStaticMesh(CubeMesh);
	Body->SetWorldScale3D(FVector(W / 100.f, D / 100.f, H / 100.f));
	Body->SetWorldLocation(FVector(Loc.X, Loc.Y, BodyZ));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->RegisterComponent();
	Inst->SetRootComponent(Body);
	UMaterialInstanceDynamic* BodyMat = UMaterialInstanceDynamic::Create(Body->GetMaterial(0), Body);
	if (BodyMat) { BodyMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor); BodyMat->SetScalarParameterValue(TEXT("Roughness"), 0.6f); Body->SetMaterial(0, BodyMat); }
	Inst->AddInstanceComponent(Body);

	// ---- Accent stripe (colored horizontal band at ground floor) ----
	{
		const float StripeH = 80.f;
		UStaticMeshComponent* Stripe = NewObject<UStaticMeshComponent>(Inst, TEXT("AccentStripe"));
		Stripe->SetStaticMesh(CubeMesh);
		Stripe->SetWorldScale3D(FVector((W + 6.f) / 100.f, (D + 6.f) / 100.f, StripeH / 100.f));
		Stripe->SetWorldLocation(FVector(Loc.X, Loc.Y, StripeH * 0.5f));
		Stripe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Stripe->RegisterComponent();
		UMaterialInstanceDynamic* SMat = UMaterialInstanceDynamic::Create(Stripe->GetMaterial(0), Stripe);
		if (SMat) { SMat->SetVectorParameterValue(TEXT("BaseColor"), AccentColor); SMat->SetScalarParameterValue(TEXT("Metallic"), 0.3f); SMat->SetScalarParameterValue(TEXT("Roughness"), 0.4f); Stripe->SetMaterial(0, SMat); }
		Inst->AddInstanceComponent(Stripe);
	}

	// ---- Entrance overhang (protruding cube above front door) ----
	{
		const float OvW = W * 0.35f;
		const float OvD = 80.f;
		UStaticMeshComponent* Overhang = NewObject<UStaticMeshComponent>(Inst, TEXT("Overhang"));
		Overhang->SetStaticMesh(CubeMesh);
		Overhang->SetWorldScale3D(FVector(OvW / 100.f, OvD / 100.f, 0.15f));
		Overhang->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + OvD * 0.5f, 260.f));
		Overhang->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Overhang->RegisterComponent();
		UMaterialInstanceDynamic* OMat = UMaterialInstanceDynamic::Create(Overhang->GetMaterial(0), Overhang);
		if (OMat) { OMat->SetVectorParameterValue(TEXT("BaseColor"), AccentColor); OMat->SetScalarParameterValue(TEXT("Roughness"), 0.5f); Overhang->SetMaterial(0, OMat); }
		Inst->AddInstanceComponent(Overhang);
	}

	// ---- Windows (regular grid, larger than residential) ----
	const FLinearColor DarkGlass(0.06f, 0.07f, 0.10f);
	const FLinearColor WarmColors[] = {
		FLinearColor(0.95f, 0.85f, 0.40f),
		FLinearColor(0.90f, 0.65f, 0.30f),
		FLinearColor(0.85f, 0.82f, 0.72f),
	};
	const float WinW = 80.f;
	const float WinH = 90.f;
	const float FloorH = 280.f;
	const int32 NumFloors = FMath::Max(1, FMath::FloorToInt(H / FloorH));
	const int32 WinsPerFloor = FMath::Clamp(FMath::FloorToInt(W / 200.f), 2, 5);
	int32 WIdx = 0;

	for (int32 Floor = 0; Floor < NumFloors; ++Floor)
	{
		const float FloorZ = Floor * FloorH + FloorH * 0.55f;
		for (int32 Face = 0; Face < 2; ++Face)
		{
			const float FaceY = Loc.Y + (Face == 0 ? D * 0.5f + 3.f : -D * 0.5f - 3.f);
			for (int32 Wi = 0; Wi < WinsPerFloor; ++Wi)
			{
				const float WX = Loc.X + (Wi - (WinsPerFloor - 1) * 0.5f) * (W / (WinsPerFloor + 1));
				FString WName = FString::Printf(TEXT("IWin_%d"), WIdx++);
				UStaticMeshComponent* Win = NewObject<UStaticMeshComponent>(Inst, *WName);
				Win->SetStaticMesh(CubeMesh);
				Win->SetWorldScale3D(FVector(WinW / 100.f, 3.f / 100.f, WinH / 100.f));
				Win->SetWorldLocation(FVector(WX, FaceY, FloorZ));
				Win->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Win->RegisterComponent();
				Inst->AddInstanceComponent(Win);

				UMaterialInstanceDynamic* WMat = UMaterialInstanceDynamic::Create(Win->GetMaterial(0), Win);
				if (WMat)
				{
					WMat->SetVectorParameterValue(TEXT("BaseColor"), DarkGlass);
					WMat->SetScalarParameterValue(TEXT("Metallic"), 0.1f);
					WMat->SetScalarParameterValue(TEXT("Roughness"), 0.05f);
					Win->SetMaterial(0, WMat);
					NightWindowMaterials.Add(WMat);
					NightWindowLitColors.Add(WarmColors[CityRandom.RandRange(0, 2)]);
					NightWindowLitStates.Add(CityRandom.FRand() < 0.75f);
				}
			}
		}
	}

	// ---- 4 entrance columns ----
	if (CylinderMesh)
	{
		const float ColH = 250.f;
		const float ColRadius = 0.12f;
		const float Spacing = W * 0.2f;
		for (int32 c = 0; c < 4; ++c)
		{
			const float CX = Loc.X + (c - 1.5f) * Spacing;
			FString CName = FString::Printf(TEXT("InstCol_%d"), c);
			UStaticMeshComponent* Col = NewObject<UStaticMeshComponent>(Inst, *CName);
			Col->SetStaticMesh(CylinderMesh);
			Col->SetWorldScale3D(FVector(ColRadius, ColRadius, ColH / 100.f));
			Col->SetWorldLocation(FVector(CX, Loc.Y + D * 0.5f + 40.f, ColH * 0.5f));
			Col->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Col->RegisterComponent();
			UMaterialInstanceDynamic* CMat = UMaterialInstanceDynamic::Create(Col->GetMaterial(0), Col);
			if (CMat) { CMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.80f, 0.78f, 0.75f)); CMat->SetScalarParameterValue(TEXT("Roughness"), 0.5f); Col->SetMaterial(0, CMat); }
			Inst->AddInstanceComponent(Col);
		}
	}

	// ---- Front door ----
	{
		UStaticMeshComponent* Door = NewObject<UStaticMeshComponent>(Inst, TEXT("InstDoor"));
		Door->SetStaticMesh(CubeMesh);
		Door->SetWorldScale3D(FVector(0.70f, 3.f / 100.f, 1.5f));
		Door->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + 3.f, 75.f));
		Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Door->RegisterComponent();
		UMaterialInstanceDynamic* DMat = UMaterialInstanceDynamic::Create(Door->GetMaterial(0), Door);
		if (DMat) { DMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.15f, 0.10f, 0.05f)); DMat->SetScalarParameterValue(TEXT("Roughness"), 0.5f); Door->SetMaterial(0, DMat); }
		Inst->AddInstanceComponent(Door);
	}

	// ---- Dome (hemisphere on roof -- classical government look) ----
	if (SphereMesh)
	{
		const float DomeR = FMath::Min(W, D) * 0.3f;
		UStaticMeshComponent* Dome = NewObject<UStaticMeshComponent>(Inst, TEXT("InstDome"));
		Dome->SetStaticMesh(SphereMesh);
		Dome->SetWorldScale3D(FVector(DomeR / 50.f, DomeR / 50.f, DomeR * 0.5f / 50.f));
		Dome->SetWorldLocation(FVector(Loc.X, Loc.Y, H + DomeR * 0.25f));
		Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Dome->RegisterComponent();
		UMaterialInstanceDynamic* DmMat = UMaterialInstanceDynamic::Create(Dome->GetMaterial(0), Dome);
		if (DmMat) { DmMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.55f, 0.55f, 0.52f)); DmMat->SetScalarParameterValue(TEXT("Metallic"), 0.6f); DmMat->SetScalarParameterValue(TEXT("Roughness"), 0.35f); Dome->SetMaterial(0, DmMat); }
		Inst->AddInstanceComponent(Dome);
	}

	// ---- Flagpole (thin tall cylinder rising from rooftop) ----
	if (CylinderMesh)
	{
		const float FlagH = 200.f;
		UStaticMeshComponent* Flag = NewObject<UStaticMeshComponent>(Inst, TEXT("Flagpole"));
		Flag->SetStaticMesh(CylinderMesh);
		Flag->SetWorldScale3D(FVector(0.03f, 0.03f, FlagH / 100.f));
		Flag->SetWorldLocation(FVector(Loc.X + W * 0.35f, Loc.Y + D * 0.5f + 20.f, H + FlagH * 0.5f));
		Flag->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Flag->RegisterComponent();
		UMaterialInstanceDynamic* FpMat = UMaterialInstanceDynamic::Create(Flag->GetMaterial(0), Flag);
		if (FpMat) { FpMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.70f, 0.70f, 0.68f)); FpMat->SetScalarParameterValue(TEXT("Metallic"), 0.8f); FpMat->SetScalarParameterValue(TEXT("Roughness"), 0.25f); Flag->SetMaterial(0, FpMat); }
		Inst->AddInstanceComponent(Flag);
	}
}


// ---------------------------------------------------------------------------
// Warehouse (industrial, large footprint, no windows)
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnWarehouse(const FVector& Loc, float W, float D, float H,
	const FLinearColor& Color)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh) return;

	const float HalfH = H * 0.5f;

	AActor* Bldg = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
	if (!Bldg) return;
#if WITH_EDITOR
	Bldg->SetActorLabel(TEXT("Warehouse"));
#endif

	// Main body
	UStaticMeshComponent* Body = NewObject<UStaticMeshComponent>(Bldg, TEXT("Body"));
	Body->SetStaticMesh(CubeMesh);
	Body->SetWorldScale3D(FVector(W / 100.f, D / 100.f, H / 100.f));
	Body->SetWorldLocation(FVector(Loc.X, Loc.Y, HalfH));
	Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Body->SetCollisionResponseToAllChannels(ECR_Block);
	Body->RegisterComponent();
	Bldg->SetRootComponent(Body);
	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Body->GetMaterial(0), Body);
	if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), Color); Mat->SetScalarParameterValue(TEXT("Roughness"), 0.7f); Body->SetMaterial(0, Mat); }
	Bldg->AddInstanceComponent(Body);

	// Loading dock (30% chance) — flat platform on one side
	if (CityRandom.FRand() < 0.3f)
	{
		const float DockW = W * 0.4f;
		const float DockD = 150.f;
		const float DockH = 80.f;
		UStaticMeshComponent* Dock = NewObject<UStaticMeshComponent>(Bldg, TEXT("Dock"));
		Dock->SetStaticMesh(CubeMesh);
		Dock->SetWorldScale3D(FVector(DockW / 100.f, DockD / 100.f, DockH / 100.f));
		Dock->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + DockD * 0.5f, DockH * 0.5f));
		Dock->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Dock->RegisterComponent();
		UMaterialInstanceDynamic* DM = UMaterialInstanceDynamic::Create(Dock->GetMaterial(0), Dock);
		if (DM) { DM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.4f, 0.4f, 0.38f)); DM->SetScalarParameterValue(TEXT("Metallic"), 0.6f); DM->SetScalarParameterValue(TEXT("Roughness"), 0.4f); Dock->SetMaterial(0, DM); }
		Bldg->AddInstanceComponent(Dock);
	}

	// Smokestack (35% chance) — cylinder on roof with cone cap
	if (CityRandom.FRand() < 0.35f && CylinderMesh)
	{
		UStaticMeshComponent* Stack = NewObject<UStaticMeshComponent>(Bldg, TEXT("Stack"));
		Stack->SetStaticMesh(CylinderMesh);
		Stack->SetWorldScale3D(FVector(0.4f, 0.4f, 2.0f));
		Stack->SetWorldLocation(FVector(Loc.X + W * 0.3f, Loc.Y, H + 100.f));
		Stack->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Stack->RegisterComponent();
		UMaterialInstanceDynamic* SM = UMaterialInstanceDynamic::Create(Stack->GetMaterial(0), Stack);
		if (SM) { SM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.35f, 0.35f, 0.33f)); SM->SetScalarParameterValue(TEXT("Metallic"), 0.6f); SM->SetScalarParameterValue(TEXT("Roughness"), 0.4f); Stack->SetMaterial(0, SM); }
		Bldg->AddInstanceComponent(Stack);

		// Cone cap on smokestack
		if (ConeMesh)
		{
			UStaticMeshComponent* StackCap = NewObject<UStaticMeshComponent>(Bldg, TEXT("StackCap"));
			StackCap->SetStaticMesh(ConeMesh);
			StackCap->SetWorldScale3D(FVector(0.55f, 0.55f, 0.5f));
			StackCap->SetWorldLocation(FVector(Loc.X + W * 0.3f, Loc.Y, H + 225.f));
			StackCap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			StackCap->RegisterComponent();
			UMaterialInstanceDynamic* SCMat = UMaterialInstanceDynamic::Create(StackCap->GetMaterial(0), StackCap);
			if (SCMat) { SCMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.30f, 0.30f, 0.28f)); SCMat->SetScalarParameterValue(TEXT("Metallic"), 0.6f); SCMat->SetScalarParameterValue(TEXT("Roughness"), 0.4f); StackCap->SetMaterial(0, SCMat); }
			Bldg->AddInstanceComponent(StackCap);
		}
	}

	// ---- Roll-up bay doors (1-3 on front face) ----
	{
		const int32 NumDoors = DetailRandom.RandRange(1, 3);
		const float DoorW = FMath::Min(W / (NumDoors + 1), 200.f);
		const float DoorH = FMath::Min(H * 0.6f, 250.f);
		for (int32 d = 0; d < NumDoors; ++d)
		{
			const float DX = Loc.X + (d - (NumDoors - 1) * 0.5f) * (W / (NumDoors + 1));
			FString DName = FString::Printf(TEXT("BayDoor_%d"), d);
			UStaticMeshComponent* Bay = NewObject<UStaticMeshComponent>(Bldg, *DName);
			Bay->SetStaticMesh(CubeMesh);
			Bay->SetWorldScale3D(FVector(DoorW / 100.f, 3.f / 100.f, DoorH / 100.f));
			Bay->SetWorldLocation(FVector(DX, Loc.Y + D * 0.5f + 3.f, DoorH * 0.5f));
			Bay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Bay->RegisterComponent();
			UMaterialInstanceDynamic* BDMat = UMaterialInstanceDynamic::Create(Bay->GetMaterial(0), Bay);
			if (BDMat) { BDMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.25f, 0.25f, 0.23f)); BDMat->SetScalarParameterValue(TEXT("Metallic"), 0.6f); BDMat->SetScalarParameterValue(TEXT("Roughness"), 0.4f); Bay->SetMaterial(0, BDMat); }
			Bldg->AddInstanceComponent(Bay);

			// Accent strip above door
			FString AName = FString::Printf(TEXT("BayAccent_%d"), d);
			UStaticMeshComponent* Accent = NewObject<UStaticMeshComponent>(Bldg, *AName);
			Accent->SetStaticMesh(CubeMesh);
			Accent->SetWorldScale3D(FVector((DoorW + 10.f) / 100.f, 3.f / 100.f, 0.10f));
			Accent->SetWorldLocation(FVector(DX, Loc.Y + D * 0.5f + 4.f, DoorH + 5.f));
			Accent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Accent->RegisterComponent();
			UMaterialInstanceDynamic* AMat = UMaterialInstanceDynamic::Create(Accent->GetMaterial(0), Accent);
			if (AMat) { AMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.60f, 0.35f, 0.10f)); AMat->SetScalarParameterValue(TEXT("Metallic"), 0.6f); AMat->SetScalarParameterValue(TEXT("Roughness"), 0.4f); Accent->SetMaterial(0, AMat); }
			Bldg->AddInstanceComponent(Accent);
		}
	}
}

// ---------------------------------------------------------------------------
// Park block (green overlay, trees, paths, benches, optional fountain)
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnParkBlock(const FVector& ParkCenter, float ParkSize, bool bAddFountain)
{
	UWorld* World = GetWorld();
	if (!World || !PlaneMesh || !CubeMesh) return;

	const float Half = ParkSize * 0.5f;

	// Green grass overlay
	AActor* Park = World->SpawnActor<AActor>(AActor::StaticClass(), ParkCenter, FRotator::ZeroRotator);
	if (!Park) return;
#if WITH_EDITOR
	Park->SetActorLabel(TEXT("ParkBlock"));
#endif

	// Park grass (brighter green than city ground)
	UStaticMeshComponent* Grass = NewObject<UStaticMeshComponent>(Park, TEXT("ParkGrass"));
	Grass->SetStaticMesh(PlaneMesh);
	Grass->SetWorldScale3D(FVector(ParkSize / 100.f, ParkSize / 100.f, 1.f));
	Grass->SetWorldLocation(FVector(ParkCenter.X, ParkCenter.Y, 3.f));
	Grass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Grass->RegisterComponent();
	Park->SetRootComponent(Grass);
	UMaterialInstanceDynamic* GrassMat = UMaterialInstanceDynamic::Create(Grass->GetMaterial(0), Grass);
	if (GrassMat) { GrassMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.15f, 0.30f, 0.10f)); Grass->SetMaterial(0, GrassMat); }
	Park->AddInstanceComponent(Grass);

	// Walking paths (cross-shaped)
	auto SpawnPath = [&](const FVector& PLoc, const FVector& PScale)
	{
		static int32 PathIdx = 0;
		FString PName = FString::Printf(TEXT("Path_%d"), PathIdx++);
		UStaticMeshComponent* Path = NewObject<UStaticMeshComponent>(Park, *PName);
		Path->SetStaticMesh(PlaneMesh);
		Path->SetWorldScale3D(PScale);
		Path->SetWorldLocation(PLoc);
		Path->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Path->RegisterComponent();
		UMaterialInstanceDynamic* PM = UMaterialInstanceDynamic::Create(Path->GetMaterial(0), Path);
		if (PM) { PM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.55f, 0.52f, 0.46f)); Path->SetMaterial(0, PM); }
		Park->AddInstanceComponent(Path);
	};

	// N-S path
	SpawnPath(FVector(ParkCenter.X, ParkCenter.Y, 4.f), FVector(1.5f, ParkSize / 100.f, 1.f));
	// E-W path
	SpawnPath(FVector(ParkCenter.X, ParkCenter.Y, 4.f), FVector(ParkSize / 100.f, 1.5f, 1.f));

	// ---- Hedge rows (sphere clusters along path edges) ----
	if (SphereMesh)
	{
		const int32 NumHedges = 8;
		for (int32 Hi = 0; Hi < NumHedges; ++Hi)
		{
			const float Frac = (Hi + 0.5f) / (float)NumHedges;
			// Alternate along N-S and E-W paths
			FVector HLoc;
			if (Hi < NumHedges / 2)
				HLoc = FVector(ParkCenter.X + 50.f, ParkCenter.Y + ParkSize * (Frac * 2.f - 0.5f), 0.f);
			else
				HLoc = FVector(ParkCenter.X + ParkSize * ((Frac - 0.5f) * 2.f - 0.5f), ParkCenter.Y + 50.f, 0.f);

			const float HScale = DetailRandom.FRandRange(0.3f, 0.5f);
			FString HName = FString::Printf(TEXT("Hedge_%d"), Hi);
			UStaticMeshComponent* Hedge = NewObject<UStaticMeshComponent>(Park, *HName);
			Hedge->SetStaticMesh(SphereMesh);
			Hedge->SetWorldScale3D(FVector(HScale, HScale, HScale * 0.6f));
			Hedge->SetWorldLocation(FVector(HLoc.X, HLoc.Y, HScale * 18.f));
			Hedge->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Hedge->RegisterComponent();
			UMaterialInstanceDynamic* HM = UMaterialInstanceDynamic::Create(Hedge->GetMaterial(0), Hedge);
			if (HM) { HM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.08f, 0.25f, 0.05f)); HM->SetScalarParameterValue(TEXT("Roughness"), 0.92f); Hedge->SetMaterial(0, HM); }
			Park->AddInstanceComponent(Hedge);
		}
	}

	// ---- Topiary cones (cone-shaped trees at path intersections) ----
	if (ConeMesh)
	{
		for (int32 Ti = 0; Ti < 2; ++Ti)
		{
			const float TSign = (Ti == 0) ? -1.f : 1.f;
			FString TName = FString::Printf(TEXT("Topiary_%d"), Ti);
			UStaticMeshComponent* Topiary = NewObject<UStaticMeshComponent>(Park, *TName);
			Topiary->SetStaticMesh(ConeMesh);
			Topiary->SetWorldScale3D(FVector(0.5f, 0.5f, 2.5f));
			Topiary->SetWorldLocation(FVector(ParkCenter.X + TSign * 120.f, ParkCenter.Y + TSign * 120.f, 125.f));
			Topiary->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Topiary->RegisterComponent();
			UMaterialInstanceDynamic* TM = UMaterialInstanceDynamic::Create(Topiary->GetMaterial(0), Topiary);
			if (TM) { TM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.05f, 0.22f, 0.04f)); TM->SetScalarParameterValue(TEXT("Roughness"), 0.90f); Topiary->SetMaterial(0, TM); }
			Park->AddInstanceComponent(Topiary);
		}
	}

	// Benches (4-6 along paths)
	const int32 NumBenches = CityRandom.RandRange(4, 6);
	for (int32 b = 0; b < NumBenches; ++b)
	{
		const float BX = ParkCenter.X + CityRandom.FRandRange(-Half * 0.6f, Half * 0.6f);
		const float BY = ParkCenter.Y + CityRandom.FRandRange(-Half * 0.6f, Half * 0.6f);
		FString BName = FString::Printf(TEXT("Bench_%d"), b);
		UStaticMeshComponent* Bench = NewObject<UStaticMeshComponent>(Park, *BName);
		Bench->SetStaticMesh(CubeMesh);
		Bench->SetWorldScale3D(FVector(1.0f, 0.35f, 0.4f));
		Bench->SetWorldLocation(FVector(BX, BY, 30.f));
		Bench->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Bench->RegisterComponent();
		UMaterialInstanceDynamic* BM = UMaterialInstanceDynamic::Create(Bench->GetMaterial(0), Bench);
		if (BM) { BM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.35f, 0.25f, 0.15f)); Bench->SetMaterial(0, BM); }
		Park->AddInstanceComponent(Bench);
	}

	// Scattered trees (6-10)
	const int32 NumTrees = CityRandom.RandRange(6, 10);
	for (int32 t = 0; t < NumTrees; ++t)
	{
		const float TX = ParkCenter.X + CityRandom.FRandRange(-Half * 0.8f, Half * 0.8f);
		const float TY = ParkCenter.Y + CityRandom.FRandRange(-Half * 0.8f, Half * 0.8f);
		const float TH = CityRandom.FRandRange(500.f, 1000.f);
		SpawnPalmTree(FVector(TX, TY, 0.f), TH, false);
	}

	// Fountain (optional — central water feature)
	if (bAddFountain && SphereMesh && CylinderMesh)
	{
		// Basin (cylinder)
		UStaticMeshComponent* Basin = NewObject<UStaticMeshComponent>(Park, TEXT("FountainBasin"));
		Basin->SetStaticMesh(CylinderMesh);
		Basin->SetWorldScale3D(FVector(3.0f, 3.0f, 0.5f));
		Basin->SetWorldLocation(FVector(ParkCenter.X, ParkCenter.Y, 25.f));
		Basin->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Basin->RegisterComponent();
		UMaterialInstanceDynamic* BsMat = UMaterialInstanceDynamic::Create(Basin->GetMaterial(0), Basin);
		if (BsMat) { BsMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.55f, 0.55f, 0.52f)); Basin->SetMaterial(0, BsMat); }
		Park->AddInstanceComponent(Basin);

		// Water surface
		UStaticMeshComponent* Water = NewObject<UStaticMeshComponent>(Park, TEXT("FountainWater"));
		Water->SetStaticMesh(CylinderMesh);
		Water->SetWorldScale3D(FVector(2.8f, 2.8f, 0.1f));
		Water->SetWorldLocation(FVector(ParkCenter.X, ParkCenter.Y, 40.f));
		Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Water->RegisterComponent();
		UMaterialInstanceDynamic* WMat = UMaterialInstanceDynamic::Create(Water->GetMaterial(0), Water);
		if (WMat) { WMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.1f, 0.2f, 0.35f)); Water->SetMaterial(0, WMat); }
		Park->AddInstanceComponent(Water);

		// Center spout (sphere)
		UStaticMeshComponent* Spout = NewObject<UStaticMeshComponent>(Park, TEXT("FountainSpout"));
		Spout->SetStaticMesh(SphereMesh);
		Spout->SetWorldScale3D(FVector(0.8f, 0.8f, 1.5f));
		Spout->SetWorldLocation(FVector(ParkCenter.X, ParkCenter.Y, 80.f));
		Spout->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Spout->RegisterComponent();
		UMaterialInstanceDynamic* SMat = UMaterialInstanceDynamic::Create(Spout->GetMaterial(0), Spout);
		if (SMat) { SMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.7f, 0.7f, 0.68f)); Spout->SetMaterial(0, SMat); }
		Park->AddInstanceComponent(Spout);
	}
}

// ---------------------------------------------------------------------------
// Building spawner (with window lights for night)
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnBuilding(const FVector& Location, const FVector& Size,
	const FLinearColor& Color, bool bAddWindows)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh) return;

	// Size.X/Y = half-widths, Size.Z = full height.
	// Place the cube center at half-height so the bottom rests at ground.
	const float HalfH = Size.Z * 0.5f;
	const FVector SpawnLoc(Location.X, Location.Y, HalfH);

	AActor* Building = World->SpawnActor<AActor>(AActor::StaticClass(),
		SpawnLoc, FRotator::ZeroRotator);
	if (!Building) return;
#if WITH_EDITOR
	Building->SetActorLabel(TEXT("CityBuilding"));
#endif

	UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Building, TEXT("BuildingMesh"));
	MC->SetStaticMesh(CubeMesh);
	MC->SetWorldScale3D(FVector(Size.X / 100.f, Size.Y / 100.f, Size.Z / 100.f));
	MC->SetWorldLocation(SpawnLoc);
	MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MC->SetCollisionResponseToAllChannels(ECR_Block);
	MC->RegisterComponent();
	Building->SetRootComponent(MC);

	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
	if (Mat)
	{
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Mat->SetScalarParameterValue(TEXT("Roughness"), 0.55f);
		MC->SetMaterial(0, Mat);
	}
	Building->AddInstanceComponent(MC);

	// ---- Corner edge cylinders (soften sharp 90-degree corners) ----
	if (CylinderMesh && Size.Z > 400.f)
	{
		const float CornerR = 6.f;
		const float HExtX = Size.X * 0.5f;
		const float HExtY = Size.Y * 0.5f;
		for (int32 Ci = 0; Ci < 4; ++Ci)
		{
			const float CXSign = (Ci & 1) ? 1.f : -1.f;
			const float CYSign = (Ci & 2) ? 1.f : -1.f;
			FString CName = FString::Printf(TEXT("BldgCorner_%d"), Ci);
			UStaticMeshComponent* Corner = NewObject<UStaticMeshComponent>(Building, *CName);
			Corner->SetStaticMesh(CylinderMesh);
			Corner->SetWorldScale3D(FVector(CornerR / 50.f, CornerR / 50.f, Size.Z / 100.f));
			Corner->SetWorldLocation(FVector(Location.X + CXSign * HExtX, Location.Y + CYSign * HExtY, Size.Z * 0.5f));
			Corner->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Corner->RegisterComponent();
			UMaterialInstanceDynamic* CMat = UMaterialInstanceDynamic::Create(Corner->GetMaterial(0), Corner);
			if (CMat) { CMat->SetVectorParameterValue(TEXT("BaseColor"), Color * 0.92f); CMat->SetScalarParameterValue(TEXT("Roughness"), 0.55f); Corner->SetMaterial(0, CMat); }
			Building->AddInstanceComponent(Corner);
		}
	}

	// ---- Ground-floor storefront awning (adds street-level character) ----
	if (bAddWindows && Size.Z > 400.f && CityRandom.FRand() < 0.35f)
	{
		// Colored awning jutting out from one face at ground level.
		const FLinearColor AwningColors[] = {
			FLinearColor(0.65f, 0.12f, 0.10f),  // red restaurant
			FLinearColor(0.10f, 0.35f, 0.15f),  // green grocery
			FLinearColor(0.12f, 0.18f, 0.55f),  // blue shop
			FLinearColor(0.60f, 0.45f, 0.10f),  // gold/yellow cafe
			FLinearColor(0.55f, 0.20f, 0.45f),  // purple boutique
			FLinearColor(0.70f, 0.35f, 0.10f),  // orange deli
			FLinearColor(0.15f, 0.45f, 0.50f),  // teal pharmacy
			FLinearColor(0.50f, 0.50f, 0.48f),  // gray office supply
		};
		const FLinearColor& AwningColor = AwningColors[CityRandom.RandRange(0, 7)];
		const float AwningWidth = FMath::Min(Size.X, Size.Y) * CityRandom.FRandRange(0.5f, 0.9f);
		const float AwningDepth = 120.f;
		const float AwningZ = 200.f; // just above ground level

		// Pick a random face.  Size.X/Y are full scale extents; actual half-extent = Size/2.
		const float HalfX = Size.X * 0.5f;
		const float HalfY = Size.Y * 0.5f;
		const int32 Face = CityRandom.RandRange(0, 3);
		FVector AwningLoc = Location;
		FVector AwningScale;
		switch (Face)
		{
		case 0: AwningLoc.Y += HalfY + AwningDepth * 0.5f; AwningScale = FVector(AwningWidth / 50.f, AwningDepth / 50.f, 0.08f); break;
		case 1: AwningLoc.Y -= HalfY + AwningDepth * 0.5f; AwningScale = FVector(AwningWidth / 50.f, AwningDepth / 50.f, 0.08f); break;
		case 2: AwningLoc.X += HalfX + AwningDepth * 0.5f; AwningScale = FVector(AwningDepth / 50.f, AwningWidth / 50.f, 0.08f); break;
		default: AwningLoc.X -= HalfX + AwningDepth * 0.5f; AwningScale = FVector(AwningDepth / 50.f, AwningWidth / 50.f, 0.08f); break;
		}
		AwningLoc.Z = AwningZ;

		UStaticMeshComponent* Awning = NewObject<UStaticMeshComponent>(Building, TEXT("Awning"));
		Awning->SetStaticMesh(CubeMesh);
		Awning->SetWorldScale3D(AwningScale);
		Awning->SetWorldLocation(AwningLoc);
		Awning->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Awning->RegisterComponent();
		UMaterialInstanceDynamic* AwMat = UMaterialInstanceDynamic::Create(Awning->GetMaterial(0), Awning);
		if (AwMat) { AwMat->SetVectorParameterValue(TEXT("BaseColor"), AwningColor); Awning->SetMaterial(0, AwMat); }
		Building->AddInstanceComponent(Awning);
	}

	// ---- Window bands on all 4 faces (lightweight lit strips) ----
	if (bAddWindows && Size.Z > 400.f && CityRandom.FRand() < 0.72f)
	{
		const float FullHeight = Size.Z;
		const float BandD = 4.f;
		const float BandH = FMath::Clamp(FullHeight * 0.07f, 20.f, 70.f);

		// Window glass colors (darker than facade -- sky reflection).
		const FLinearColor BandColors[] = {
			FLinearColor(0.10f, 0.15f, 0.25f),  // sky blue reflection
			FLinearColor(0.08f, 0.10f, 0.16f),  // dark tinted glass
			FLinearColor(0.15f, 0.18f, 0.22f),  // light gray glass
			FLinearColor(0.05f, 0.08f, 0.14f),  // deep tinted
			FLinearColor(0.12f, 0.14f, 0.20f),  // medium blue glass
		};
		const FLinearColor& BandColor = BandColors[CityRandom.RandRange(0, 4)];

		// Adaptive band count based on height.
		TArray<float> BandHeights;
		BandHeights.Add(FullHeight * 0.12f); // ground floor lobby
		if (FullHeight > 1200.f)
			BandHeights.Add(FullHeight * 0.48f); // mid floors
		if (FullHeight > 2800.f)
			BandHeights.Add(FullHeight * 0.78f); // upper floors

		// Actual half-extents of the cube mesh after scaling.
		const float HExtX = Size.X * 0.5f;
		const float HExtY = Size.Y * 0.5f;

		struct FBandFace { float OffX, OffY; bool bAlongX; float HW; };
		const FBandFace Faces[] = {
			{  0.f,  HExtY + 3.f, true,  HExtX },
			{  0.f, -HExtY - 3.f, true,  HExtX },
			{  HExtX + 3.f, 0.f,  false, HExtY },
			{ -HExtX - 3.f, 0.f,  false, HExtY },
		};

		int32 BIdx = 0;
		for (const FBandFace& F : Faces)
		{
			for (float BZ : BandHeights)
			{
				FString N = FString::Printf(TEXT("WB_%d"), BIdx++);
				UStaticMeshComponent* WB = NewObject<UStaticMeshComponent>(Building, *N);
				WB->SetStaticMesh(CubeMesh);
				if (F.bAlongX)
				{
					WB->SetWorldScale3D(FVector(F.HW * 2.f / 100.f, BandD / 100.f, BandH / 100.f));
					WB->SetWorldLocation(FVector(Location.X, Location.Y + F.OffY, BZ));
				}
				else
				{
					WB->SetWorldScale3D(FVector(BandD / 100.f, F.HW * 2.f / 100.f, BandH / 100.f));
					WB->SetWorldLocation(FVector(Location.X + F.OffX, Location.Y, BZ));
				}
				WB->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				WB->RegisterComponent();
				UMaterialInstanceDynamic* WM = UMaterialInstanceDynamic::Create(WB->GetMaterial(0), WB);
				if (WM) { WM->SetVectorParameterValue(TEXT("BaseColor"), BandColor); WM->SetScalarParameterValue(TEXT("Metallic"), 0.1f); WM->SetScalarParameterValue(TEXT("Roughness"), 0.05f); WB->SetMaterial(0, WM); }
				Building->AddInstanceComponent(WB);
			}
		}
	}

	// ---- Individual lit windows (glow at night -- people in offices/apartments) ----
	if (bAddWindows && Size.Z > 500.f)
	{
		const float FullHeight = Size.Z;
		const float FloorHeight = 320.f;
		const float WindowW = 65.f;
		const float WindowH = 80.f;
		const float WindowSpacing = 210.f;
		const float StartZ = 350.f; // above ground-floor storefronts

		// Warm interior glow colors -- variety makes the city feel alive.
		const FLinearColor WarmColors[] = {
			FLinearColor(0.95f, 0.85f, 0.40f),  // office fluorescent
			FLinearColor(0.90f, 0.65f, 0.30f),  // warm home lamp
			FLinearColor(0.85f, 0.82f, 0.72f),  // neutral white room
			FLinearColor(0.45f, 0.55f, 0.82f),  // TV/screen blue glow
			FLinearColor(0.95f, 0.55f, 0.20f),  // orange cozy reading light
			FLinearColor(0.70f, 0.88f, 0.65f),  // green-tinted kitchen
		};
		const FLinearColor DarkGlass(0.06f, 0.07f, 0.10f);

		// Only place lit windows on 2 faces (saves components; street-facing).
		struct FWinFace { float OffX, OffY; bool bAlongX; float FaceW; };
		FWinFace WFaces[2];
		int32 NumFaces = 0;
		// Actual half-extents after cube scaling.
		const float WExtX = Size.X * 0.5f;
		const float WExtY = Size.Y * 0.5f;
		// Pick the two wider faces for more visible windows.
		if (Size.X >= Size.Y)
		{
			WFaces[NumFaces++] = {  0.f,  WExtY + 6.f, true,  Size.X };
			WFaces[NumFaces++] = {  0.f, -WExtY - 6.f, true,  Size.X };
		}
		else
		{
			WFaces[NumFaces++] = {  WExtX + 6.f, 0.f, false, Size.Y };
			WFaces[NumFaces++] = { -WExtX - 6.f, 0.f, false, Size.Y };
		}

		int32 WinIdx = 0;
		for (int32 Fi = 0; Fi < NumFaces; ++Fi)
		{
			const FWinFace& WF = WFaces[Fi];
			const int32 NumCols = FMath::Clamp(FMath::FloorToInt(WF.FaceW / WindowSpacing), 1, 6);
			const int32 NumRows = FMath::Clamp(FMath::FloorToInt((FullHeight - StartZ) / FloorHeight), 1, 10);
			const float ColStart = -(NumCols - 1) * WindowSpacing * 0.5f;

			for (int32 Row = 0; Row < NumRows; ++Row)
			{
				for (int32 Col = 0; Col < NumCols; ++Col)
				{
					// Only ~35% of window positions actually get a lit window.
					if (CityRandom.FRand() > 0.35f)
						continue;

					const float WZ = StartZ + Row * FloorHeight;
					const float WOff = ColStart + Col * WindowSpacing;

					FVector WinLoc;
					FVector WinScale;
					if (WF.bAlongX)
					{
						WinLoc = FVector(Location.X + WOff, Location.Y + WF.OffY, WZ);
						WinScale = FVector(WindowW / 50.f, 3.f / 50.f, WindowH / 50.f);
					}
					else
					{
						WinLoc = FVector(Location.X + WF.OffX, Location.Y + WOff, WZ);
						WinScale = FVector(3.f / 50.f, WindowW / 50.f, WindowH / 50.f);
					}

					FString WName = FString::Printf(TEXT("NWin_%d"), WinIdx++);
					UStaticMeshComponent* WinMC = NewObject<UStaticMeshComponent>(Building, *WName);
					WinMC->SetStaticMesh(CubeMesh);
					WinMC->SetWorldScale3D(WinScale);
					WinMC->SetWorldLocation(WinLoc);
					WinMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					WinMC->RegisterComponent();
					Building->AddInstanceComponent(WinMC);

					UMaterialInstanceDynamic* WinMat = UMaterialInstanceDynamic::Create(
						WinMC->GetMaterial(0), WinMC);
					if (WinMat)
					{
						// Start dark (game begins at sunrise).
						WinMat->SetVectorParameterValue(TEXT("BaseColor"), DarkGlass);
						WinMat->SetScalarParameterValue(TEXT("Metallic"), 0.1f);
						WinMat->SetScalarParameterValue(TEXT("Roughness"), 0.05f);
						WinMC->SetMaterial(0, WinMat);

						NightWindowMaterials.Add(WinMat);
						NightWindowLitColors.Add(WarmColors[CityRandom.RandRange(0, 5)]);
						NightWindowLitStates.Add(CityRandom.FRand() < 0.7f); // 70% start on at first nightfall
					}
				}
			}
		}
	}

	// ---- Ground-floor entrance door ----
	if (bAddWindows && Size.Z > 400.f)
	{
		const float HalfX2 = Size.X * 0.5f;
		const float HalfY2 = Size.Y * 0.5f;
		UStaticMeshComponent* Door = NewObject<UStaticMeshComponent>(Building, TEXT("BldgDoor"));
		Door->SetStaticMesh(CubeMesh);
		Door->SetWorldScale3D(FVector(0.60f, 3.f / 100.f, 1.4f));
		Door->SetWorldLocation(FVector(Location.X, Location.Y + HalfY2 + 3.f, 70.f));
		Door->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Door->RegisterComponent();
		UMaterialInstanceDynamic* DoorMat = UMaterialInstanceDynamic::Create(Door->GetMaterial(0), Door);
		if (DoorMat) { DoorMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.12f, 0.10f, 0.08f)); Door->SetMaterial(0, DoorMat); }
		Building->AddInstanceComponent(Door);
	}

	// ---- Rooftop detail (35% -- AC unit, water tank, spire, or dome) ----
	if (DetailRandom.FRand() < 0.35f && Size.Z > 600.f)
	{
		const float RoofZ = Size.Z + 2.f;
		const float RoofRoll = DetailRandom.FRand();
		if (RoofRoll < 0.25f)
		{
			// AC unit (cube)
			UStaticMeshComponent* AC = NewObject<UStaticMeshComponent>(Building, TEXT("RooftopAC"));
			AC->SetStaticMesh(CubeMesh);
			AC->SetWorldScale3D(FVector(0.6f, 0.4f, 0.3f));
			AC->SetWorldLocation(FVector(Location.X + Size.X * 0.2f, Location.Y - Size.Y * 0.2f, RoofZ + 15.f));
			AC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			AC->RegisterComponent();
			UMaterialInstanceDynamic* ACMat = UMaterialInstanceDynamic::Create(AC->GetMaterial(0), AC);
			if (ACMat) { ACMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.50f, 0.50f, 0.48f)); ACMat->SetScalarParameterValue(TEXT("Metallic"), 0.5f); ACMat->SetScalarParameterValue(TEXT("Roughness"), 0.4f); AC->SetMaterial(0, ACMat); }
			Building->AddInstanceComponent(AC);
		}
		else if (RoofRoll < 0.50f && CylinderMesh)
		{
			// Water tank (cylinder)
			UStaticMeshComponent* Tank = NewObject<UStaticMeshComponent>(Building, TEXT("RooftopTank"));
			Tank->SetStaticMesh(CylinderMesh);
			Tank->SetWorldScale3D(FVector(0.5f, 0.5f, 0.8f));
			Tank->SetWorldLocation(FVector(Location.X - Size.X * 0.15f, Location.Y + Size.Y * 0.15f, RoofZ + 40.f));
			Tank->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Tank->RegisterComponent();
			UMaterialInstanceDynamic* TkMat = UMaterialInstanceDynamic::Create(Tank->GetMaterial(0), Tank);
			if (TkMat) { TkMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.40f, 0.40f, 0.38f)); TkMat->SetScalarParameterValue(TEXT("Metallic"), 0.5f); TkMat->SetScalarParameterValue(TEXT("Roughness"), 0.4f); Tank->SetMaterial(0, TkMat); }
			Building->AddInstanceComponent(Tank);
		}
		else if (RoofRoll < 0.75f && ConeMesh)
		{
			// Spire (cone pointing up)
			UStaticMeshComponent* Spire = NewObject<UStaticMeshComponent>(Building, TEXT("RooftopSpire"));
			Spire->SetStaticMesh(ConeMesh);
			Spire->SetWorldScale3D(FVector(0.3f, 0.3f, 1.8f));
			Spire->SetWorldLocation(FVector(Location.X, Location.Y, RoofZ + 90.f));
			Spire->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Spire->RegisterComponent();
			UMaterialInstanceDynamic* SpMat = UMaterialInstanceDynamic::Create(Spire->GetMaterial(0), Spire);
			if (SpMat) { SpMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.55f, 0.55f, 0.52f)); SpMat->SetScalarParameterValue(TEXT("Metallic"), 0.6f); SpMat->SetScalarParameterValue(TEXT("Roughness"), 0.3f); Spire->SetMaterial(0, SpMat); }
			Building->AddInstanceComponent(Spire);
		}
		else if (SphereMesh)
		{
			// Dome (flattened sphere)
			UStaticMeshComponent* Dome = NewObject<UStaticMeshComponent>(Building, TEXT("RooftopDome"));
			Dome->SetStaticMesh(SphereMesh);
			Dome->SetWorldScale3D(FVector(FMath::Min(Size.X, Size.Y) * 0.004f, FMath::Min(Size.X, Size.Y) * 0.004f, 0.4f));
			Dome->SetWorldLocation(FVector(Location.X, Location.Y, RoofZ + 20.f));
			Dome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Dome->RegisterComponent();
			UMaterialInstanceDynamic* DmMat = UMaterialInstanceDynamic::Create(Dome->GetMaterial(0), Dome);
			if (DmMat) { DmMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.48f, 0.48f, 0.45f)); DmMat->SetScalarParameterValue(TEXT("Metallic"), 0.5f); DmMat->SetScalarParameterValue(TEXT("Roughness"), 0.35f); Dome->SetMaterial(0, DmMat); }
			Building->AddInstanceComponent(Dome);
		}
	}
}

// ---------------------------------------------------------------------------
// Road strip
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnRoadStrip(const FVector& Center, float LengthX, float LengthY)
{
	UWorld* World = GetWorld();
	// CubeMesh slab with real thickness for robust capsule collision.
	if (!World || !CubeMesh) return;

	// 50 UU thick slab.  Top face at Z=2, center at Z=−23.
	const float SlabThick = 50.f;
	const float TopZ = 2.f;
	const float SlabCenter = TopZ - SlabThick * 0.5f;

	AActor* Road = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector(Center.X, Center.Y, SlabCenter), FRotator::ZeroRotator);
	if (!Road) return;
#if WITH_EDITOR
	Road->SetActorLabel(TEXT("CityRoad"));
#endif

	UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Road, TEXT("RoadMesh"));
	MC->SetStaticMesh(CubeMesh);
	MC->SetWorldScale3D(FVector(LengthX / 100.f, LengthY / 100.f, SlabThick / 100.f));
	MC->SetWorldLocation(FVector(Center.X, Center.Y, SlabCenter));
	MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MC->SetCollisionResponseToAllChannels(ECR_Block);
	MC->RegisterComponent();
	Road->SetRootComponent(MC);

	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
	if (Mat)
	{
		// Black asphalt road surface.
		Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.015f, 0.015f, 0.018f));
		Mat->SetScalarParameterValue(TEXT("Roughness"), 0.85f);
		MC->SetMaterial(0, Mat);
		RoadMaterials.Add(Mat); // Store for wet road effect in Tick()
	}
	Road->AddInstanceComponent(MC);

	// ---- Road markings (yellow center line + white edge lines) ----
	if (!CubeMesh) return;

	const bool bAlongX = (LengthX > LengthY);
	const float RoadLen = bAlongX ? LengthX : LengthY;
	const float RoadWid = bAlongX ? LengthY : LengthX;

	auto SpawnLine = [&](const FVector& LineLoc, const FLinearColor& LineColor, const FLinearColor& Emissive = FLinearColor::Black)
	{
		UStaticMeshComponent* LineMC = NewObject<UStaticMeshComponent>(Road);
		LineMC->SetStaticMesh(CubeMesh);
		if (bAlongX)
			LineMC->SetWorldScale3D(FVector(RoadLen / 100.f, 0.12f, 0.015f));
		else
			LineMC->SetWorldScale3D(FVector(0.12f, RoadLen / 100.f, 0.015f));
		LineMC->SetWorldLocation(LineLoc);
		LineMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		LineMC->RegisterComponent();
		UMaterialInstanceDynamic* LMat = UMaterialInstanceDynamic::Create(LineMC->GetMaterial(0), LineMC);
		if (LMat)
		{
			LMat->SetVectorParameterValue(TEXT("BaseColor"), LineColor);
			LMat->SetScalarParameterValue(TEXT("Metallic"), 0.3f); // retroreflective paint
			LMat->SetScalarParameterValue(TEXT("Roughness"), 0.25f);
			if (Emissive != FLinearColor::Black)
				LMat->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
			LineMC->SetMaterial(0, LMat);
		}
		Road->AddInstanceComponent(LineMC);
	};

	const FLinearColor YellowLine(1.2f, 1.0f, 0.0f);
	const FLinearColor WhiteLine(0.7f, 0.7f, 0.7f);
	const float LineZ = 3.5f;

	// Center line (boosted emissive for GTA V night glow).
	SpawnLine(FVector(Center.X, Center.Y, LineZ), YellowLine, FLinearColor(0.6f, 0.5f, 0.0f));

	// Edge lines (brighter retroreflective).
	const FLinearColor WhiteEmissive(0.2f, 0.2f, 0.2f);
	if (bAlongX)
	{
		SpawnLine(FVector(Center.X, Center.Y - RoadWid * 0.5f + 50.f, LineZ), WhiteLine, WhiteEmissive);
		SpawnLine(FVector(Center.X, Center.Y + RoadWid * 0.5f - 50.f, LineZ), WhiteLine, WhiteEmissive);
	}
	else
	{
		SpawnLine(FVector(Center.X - RoadWid * 0.5f + 50.f, Center.Y, LineZ), WhiteLine, WhiteEmissive);
		SpawnLine(FVector(Center.X + RoadWid * 0.5f - 50.f, Center.Y, LineZ), WhiteLine, WhiteEmissive);
	}
}

// ---------------------------------------------------------------------------
// Sidewalk strip
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnSidewalk(const FVector& Center, float LengthX, float LengthY)
{
	UWorld* World = GetWorld();
	// CubeMesh slab with real thickness for robust capsule collision.
	if (!World || !CubeMesh) return;

	// 50 UU thick slab.  Top face at Z=3, center at Z=−22.
	const float SlabThick = 50.f;
	const float TopZ = 3.f;
	const float SlabCenter = TopZ - SlabThick * 0.5f;

	AActor* Walk = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector(Center.X, Center.Y, SlabCenter), FRotator::ZeroRotator);
	if (!Walk) return;
#if WITH_EDITOR
	Walk->SetActorLabel(TEXT("CitySidewalk"));
#endif

	UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Walk, TEXT("SidewalkMesh"));
	MC->SetStaticMesh(CubeMesh);
	MC->SetWorldScale3D(FVector(LengthX / 100.f, LengthY / 100.f, SlabThick / 100.f));
	MC->SetWorldLocation(FVector(Center.X, Center.Y, SlabCenter));
	MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MC->SetCollisionResponseToAllChannels(ECR_Block);
	MC->RegisterComponent();
	Walk->SetRootComponent(MC);

	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
	if (Mat)
	{
		// Concrete sidewalk.
		Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.40f, 0.38f, 0.35f));
		Mat->SetScalarParameterValue(TEXT("Roughness"), 0.75f);
		MC->SetMaterial(0, Mat);
	}
	Walk->AddInstanceComponent(MC);
}

// ---------------------------------------------------------------------------
// Street light (pole + point light)
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnStreetLight(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World || !CylinderMesh || !SphereMesh) return;

	AActor* LampActor = World->SpawnActor<AActor>(AActor::StaticClass(),
		Location, FRotator::ZeroRotator);
	if (!LampActor) return;
#if WITH_EDITOR
	LampActor->SetActorLabel(TEXT("StreetLight"));
#endif

	// ---- Pole (very thin cylinder) ----
	constexpr float PoleHeight = 450.f;
	constexpr float PoleRadius = 1.5f;

	UStaticMeshComponent* PoleMC = NewObject<UStaticMeshComponent>(LampActor, TEXT("Pole"));
	PoleMC->SetStaticMesh(CylinderMesh);
	// Cylinder: 100 tall, 50 radius. Scale to PoleHeight tall, PoleRadius radius.
	PoleMC->SetWorldScale3D(FVector(PoleRadius / 50.f, PoleRadius / 50.f, PoleHeight / 100.f));
	PoleMC->SetWorldLocation(FVector(Location.X, Location.Y, PoleHeight * 0.5f));
	PoleMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PoleMC->RegisterComponent();
	LampActor->SetRootComponent(PoleMC);

	UMaterialInstanceDynamic* PoleMat = UMaterialInstanceDynamic::Create(PoleMC->GetMaterial(0), PoleMC);
	if (PoleMat)
	{
		PoleMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.15f, 0.15f, 0.16f));
		PoleMat->SetScalarParameterValue(TEXT("Metallic"), 0.7f);
		PoleMat->SetScalarParameterValue(TEXT("Roughness"), 0.35f);
		PoleMC->SetMaterial(0, PoleMat);
	}
	LampActor->AddInstanceComponent(PoleMC);

	// Lamp fixture (warm emissive sphere at top of pole)
	UStaticMeshComponent* FixtureMC = NewObject<UStaticMeshComponent>(LampActor, TEXT("LampFixture"));
	FixtureMC->SetStaticMesh(SphereMesh);
	FixtureMC->SetWorldScale3D(FVector(0.35f, 0.35f, 0.25f));
	FixtureMC->SetWorldLocation(FVector(Location.X, Location.Y, PoleHeight));
	FixtureMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FixtureMC->RegisterComponent();
	LampActor->AddInstanceComponent(FixtureMC);
	{
		UMaterialInstanceDynamic* FM = UMaterialInstanceDynamic::Create(FixtureMC->GetMaterial(0), FixtureMC);
		if (FM)
		{
			FM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.9f, 0.8f, 0.5f));
			FM->SetScalarParameterValue(TEXT("Metallic"), 0.6f);
			FM->SetScalarParameterValue(TEXT("Roughness"), 0.3f);
			FM->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(1.5f, 1.2f, 0.7f));
			FixtureMC->SetMaterial(0, FM);
		}
	}

	// ---- Lamp shade (inverted cone above the globe) ----
	if (ConeMesh)
	{
		UStaticMeshComponent* Shade = NewObject<UStaticMeshComponent>(LampActor, TEXT("LampShade"));
		Shade->SetStaticMesh(ConeMesh);
		Shade->SetWorldScale3D(FVector(0.45f, 0.45f, 0.25f));
		Shade->SetWorldLocation(FVector(Location.X, Location.Y, PoleHeight + 18.f));
		Shade->SetWorldRotation(FRotator(180.f, 0.f, 0.f)); // inverted, pointing down
		Shade->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shade->RegisterComponent();
		UMaterialInstanceDynamic* ShMat = UMaterialInstanceDynamic::Create(Shade->GetMaterial(0), Shade);
		if (ShMat) { ShMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.15f, 0.15f, 0.16f)); ShMat->SetScalarParameterValue(TEXT("Metallic"), 0.7f); ShMat->SetScalarParameterValue(TEXT("Roughness"), 0.35f); Shade->SetMaterial(0, ShMat); }
		LampActor->AddInstanceComponent(Shade);
	}

	// ---- Spot light (GTA V-style downward cone, realistic street pool) ----
	USpotLightComponent* SL = NewObject<USpotLightComponent>(LampActor, TEXT("LampLight"));
	SL->SetWorldLocation(FVector(Location.X, Location.Y, PoleHeight));
	SL->SetWorldRotation(FRotator(-85.f, 0.f, 0.f)); // pointing down
	SL->SetIntensity(20000.f);
	SL->SetAttenuationRadius(2000.f);
	SL->SetOuterConeAngle(55.f);
	SL->SetInnerConeAngle(30.f);
	SL->SetLightColor(FColor(255, 200, 120)); // warm sodium
	SL->SetCastShadows(false); // Performance: no shadows from street lights
	SL->RegisterComponent();
	LampActor->AddInstanceComponent(SL);
}

// ---------------------------------------------------------------------------
// Palm tree — GTA V-quality pinnate palms from basic shapes
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnPalmTree(const FVector& Location, float Height, bool bDetailed)
{
	UWorld* World = GetWorld();
	if (!World || !CylinderMesh || !CubeMesh) return;

	Height *= 4.8f;

	AActor* Tree = World->SpawnActor<AActor>(AActor::StaticClass(),
		Location, FRotator::ZeroRotator);
	if (!Tree) return;
#if WITH_EDITOR
	Tree->SetActorLabel(TEXT("PalmTree"));
#endif

	int32 CompIdx = 0;

	// Helper: attach a mesh component, apply PBR material, register
	auto AddComp = [&](UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot,
		const FVector& Scale, const FLinearColor& Color, float Roughness,
		float Metallic = 0.f) -> UStaticMeshComponent*
	{
		FString Name = FString::Printf(TEXT("P_%d"), CompIdx++);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Tree, *Name);
		C->SetStaticMesh(Mesh);
		C->SetWorldLocation(Loc);
		C->SetWorldRotation(Rot);
		C->SetWorldScale3D(Scale);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->RegisterComponent();
		if (CompIdx == 1) Tree->SetRootComponent(C);

		UMaterialInstanceDynamic* M = UMaterialInstanceDynamic::Create(
			C->GetMaterial(0), C);
		if (M)
		{
			M->SetVectorParameterValue(TEXT("BaseColor"), Color);
			M->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			M->SetScalarParameterValue(TEXT("Metallic"), Metallic);
			C->SetMaterial(0, M);
		}
		Tree->AddInstanceComponent(C);
		return C;
	};

	// ====================================================================
	// TRUNK — multi-segment taper with natural S-curve and bark ring scars
	// ====================================================================

	const float BaseRadius = Height * 0.014f;
	const float TopRadius  = Height * 0.005f;
	const int32 TrunkSegs  = bDetailed ? 8 : 5;
	const float SegH       = Height / TrunkSegs;

	// S-curve: gentle sine wave creates organic lean
	const float CurveAmp  = Height * 0.012f;
	const float CurveYaw  = CityRandom.FRandRange(0.f, 360.f);
	const float CurveYawR = FMath::DegreesToRadians(CurveYaw);

	// Color gradient: gray-brown at base → warm tan at crown
	const FLinearColor TrunkBase(
		CityRandom.FRandRange(0.18f, 0.24f),
		CityRandom.FRandRange(0.12f, 0.16f),
		CityRandom.FRandRange(0.05f, 0.08f));
	const FLinearColor TrunkTop(
		CityRandom.FRandRange(0.30f, 0.38f),
		CityRandom.FRandRange(0.24f, 0.30f),
		CityRandom.FRandRange(0.12f, 0.18f));

	// Ring scar color (lighter, slightly yellowish)
	const FLinearColor RingScarColor(0.40f, 0.34f, 0.20f);

	// Track crown position (accounts for S-curve offset)
	FVector CrownPos = Location;
	CrownPos.Z = Height;

	for (int32 S = 0; S < TrunkSegs; ++S)
	{
		const float T = static_cast<float>(S) / TrunkSegs;
		float Radius = FMath::Lerp(BaseRadius, TopRadius, T);
		if (S == 0) Radius *= 1.25f; // slight base bulge

		// S-curve offset at this height
		const float CurveOff = CurveAmp * FMath::Sin(T * PI);
		const float SegCX = Location.X + CurveOff * FMath::Cos(CurveYawR);
		const float SegCY = Location.Y + CurveOff * FMath::Sin(CurveYawR);
		const float SegZ  = S * SegH + SegH * 0.5f;

		// Slight tilt to follow the curve tangent
		const float dCurve = CurveAmp * PI * FMath::Cos(T * PI) / Height;
		const float TiltDeg = FMath::RadiansToDegrees(FMath::Atan(dCurve));

		const FLinearColor SegColor = FMath::Lerp(TrunkBase, TrunkTop, T);

		AddComp(CylinderMesh,
			FVector(SegCX, SegCY, SegZ),
			FRotator(TiltDeg * FMath::Cos(CurveYawR),
					 CurveYaw,
					 TiltDeg * FMath::Sin(CurveYawR)),
			FVector(Radius / 50.f, Radius / 50.f, SegH / 100.f),
			SegColor, 0.88f);

		// Ring scars — thin discs at segment joints (every 2 segments)
		const int32 RingFreq = bDetailed ? 2 : 3;
		if (S > 0 && S % RingFreq == 0)
		{
			const float RingRadius = Radius * 1.18f;
			AddComp(CylinderMesh,
				FVector(SegCX, SegCY, S * SegH),
				FRotator::ZeroRotator,
				FVector(RingRadius / 50.f, RingRadius / 50.f, 0.015f),
				RingScarColor, 0.75f);
		}

		// Update crown position to account for S-curve
		if (S == TrunkSegs - 1)
		{
			const float TopT = 1.f;
			const float TopOff = CurveAmp * FMath::Sin(TopT * PI);
			CrownPos.X = Location.X + TopOff * FMath::Cos(CurveYawR);
			CrownPos.Y = Location.Y + TopOff * FMath::Sin(CurveYawR);
		}
	}

	// Crownshaft — bright green cylinder at top of trunk (detailed only)
	if (bDetailed)
	{
		const float CSHeight = SegH * 0.6f;
		const float CSRadius = TopRadius * 1.4f;
		AddComp(CylinderMesh,
			FVector(CrownPos.X, CrownPos.Y, Height - CSHeight * 0.3f),
			FRotator::ZeroRotator,
			FVector(CSRadius / 50.f, CSRadius / 50.f, CSHeight / 100.f),
			FLinearColor(0.06f, 0.28f, 0.04f), 0.5f);
	}

	// ====================================================================
	// FRONDS — pinnate compound leaves: rachis spine + V-folded leaf blades
	// ====================================================================

	const float FrondLen       = Height * 0.28f;
	const float FrondWidth     = Height * 0.04f;
	const float SpineThickness = Height * 0.003f;
	const float BladeThickness = Height * 0.0015f;
	const float VFoldAngle     = 18.f; // V-fold half-angle for leaf cross-section

	// Frond tier definition
	struct FFrondTier
	{
		int32 Count;
		float MinDroop;
		float MaxDroop;
		FLinearColor Color;
		float LengthMult;
		float WidthMult;
		float Roughness;
		bool bDead;
	};

	TArray<FFrondTier> Tiers;
	if (bDetailed)
	{
		// Upper tier — young, upright, bright green, waxy sheen
		Tiers.Add({ CityRandom.RandRange(4, 5), 8.f, 22.f,
			FLinearColor(
				CityRandom.FRandRange(0.03f, 0.06f),
				CityRandom.FRandRange(0.28f, 0.38f),
				CityRandom.FRandRange(0.02f, 0.05f)),
			0.8f, 0.9f, 0.45f, false });
		// Middle tier — mature, drooping, deeper green
		Tiers.Add({ CityRandom.RandRange(4, 6), 28.f, 50.f,
			FLinearColor(
				CityRandom.FRandRange(0.02f, 0.04f),
				CityRandom.FRandRange(0.18f, 0.26f),
				CityRandom.FRandRange(0.01f, 0.03f)),
			1.0f, 1.0f, 0.55f, false });
		// Dead tier — brown, hanging down along trunk
		Tiers.Add({ CityRandom.RandRange(2, 3), 55.f, 78.f,
			FLinearColor(
				CityRandom.FRandRange(0.25f, 0.35f),
				CityRandom.FRandRange(0.18f, 0.24f),
				CityRandom.FRandRange(0.06f, 0.10f)),
			0.65f, 0.6f, 0.80f, true });
	}
	else
	{
		// Single tier for simple trees
		Tiers.Add({ CityRandom.RandRange(6, 8), 18.f, 48.f,
			FLinearColor(
				CityRandom.FRandRange(0.02f, 0.05f),
				CityRandom.FRandRange(0.20f, 0.30f),
				CityRandom.FRandRange(0.01f, 0.04f)),
			1.0f, 1.0f, 0.55f, false });
	}

	float GlobalYawOffset = CityRandom.FRandRange(0.f, 360.f);

	for (const FFrondTier& Tier : Tiers)
	{
		for (int32 F = 0; F < Tier.Count; ++F)
		{
			const float Yaw = GlobalYawOffset
				+ (360.f / Tier.Count) * F
				+ CityRandom.FRandRange(-12.f, 12.f);
			const float Droop = CityRandom.FRandRange(Tier.MinDroop, Tier.MaxDroop);
			const float FL = FrondLen * Tier.LengthMult;
			const float FW = FrondWidth * Tier.WidthMult;

			// Direction the frond extends outward from crown
			const float YawR   = FMath::DegreesToRadians(Yaw);
			const float DroopR = FMath::DegreesToRadians(Droop);
			const FVector FrondDir(
				FMath::Cos(DroopR) * FMath::Cos(YawR),
				FMath::Cos(DroopR) * FMath::Sin(YawR),
				-FMath::Sin(DroopR));
			const FVector FrondCenter = CrownPos + FrondDir * (FL * 0.5f);

			// Spine color — slightly darker than blade
			const FLinearColor SpineColor(
				Tier.Color.R * 0.6f,
				Tier.Color.G * 0.7f,
				Tier.Color.B * 0.5f);

			// -- Rachis (spine): thin elongated cube, darker green --
			AddComp(CubeMesh,
				FrondCenter,
				FRotator(-Droop, Yaw, 0.f),
				FVector(FL / 100.f, SpineThickness / 100.f, SpineThickness / 100.f),
				SpineColor, Tier.Roughness);

			// -- Left leaf blade: flat wide section, angled up by -VFold --
			const FRotator LeftRot(-Droop, Yaw, -VFoldAngle);
			const FVector LeftRight = FRotationMatrix(FRotator(-Droop, Yaw, 0.f))
				.GetUnitAxis(EAxis::Y);
			const FVector LeftCenter = FrondCenter - LeftRight * (FW * 0.22f);

			AddComp(CubeMesh,
				LeftCenter,
				LeftRot,
				FVector(FL * 0.88f / 100.f, FW * 0.44f / 100.f, BladeThickness / 100.f),
				Tier.Color, Tier.Roughness);

			// -- Right leaf blade: mirror of left --
			const FRotator RightRot(-Droop, Yaw, VFoldAngle);
			const FVector RightCenter = FrondCenter + LeftRight * (FW * 0.22f);

			AddComp(CubeMesh,
				RightCenter,
				RightRot,
				FVector(FL * 0.88f / 100.f, FW * 0.44f / 100.f, BladeThickness / 100.f),
				Tier.Color, Tier.Roughness);
		}

		// Rotate base offset so tiers don't overlap
		GlobalYawOffset += CityRandom.FRandRange(15.f, 30.f);
	}

	// ====================================================================
	// COCONUT CLUSTER — small brown-green spheres just below crown
	// ====================================================================

	if (bDetailed && SphereMesh && CityRandom.FRand() < 0.65f)
	{
		const int32 NumCocos = CityRandom.RandRange(3, 5);
		const float CocoR = Height * 0.007f;
		for (int32 C = 0; C < NumCocos; ++C)
		{
			const float CYaw = CityRandom.FRandRange(0.f, 360.f);
			const float CDist = TopRadius + CocoR * 2.5f;
			AddComp(SphereMesh,
				FVector(
					CrownPos.X + CDist * FMath::Cos(FMath::DegreesToRadians(CYaw)),
					CrownPos.Y + CDist * FMath::Sin(FMath::DegreesToRadians(CYaw)),
					Height - CocoR * 4.f),
				FRotator::ZeroRotator,
				FVector(CocoR / 50.f),
				FLinearColor(0.22f, 0.16f, 0.05f), 0.65f);
		}
	}
}

// ---------------------------------------------------------------------------
// Outskirts (extended highway + side roads + mountains + roadside content)
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnOutskirts()
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh) return;

	const float HwyStartX = -64000.f;
	const float HwyEndX = -280000.f;
	const float HwyY = 0.f;
	const float HwyHalfW = 700.f;

	// ---- Main Highway ----
	const float HwyLen = FMath::Abs(HwyEndX - HwyStartX);
	const float HwyCenterX = (HwyStartX + HwyEndX) * 0.5f;
	SpawnRoadStrip(FVector(HwyCenterX, HwyY, 2.f), HwyLen * 0.5f, HwyHalfW);

	// ---- Side Roads ----
	struct FSideRoad { float X; float DirY; float Len; };
	const FSideRoad SideRoads[] = {
		{ -80000.f,   1.f,  8000.f },
		{ -100000.f, -1.f, 10000.f },
		{ -130000.f,  1.f,  6000.f },
		{ -160000.f, -1.f, 12000.f },
		{ -200000.f,  1.f,  8000.f },
		{ -240000.f, -1.f, 10000.f },
		// Mirror side roads (opposite direction, shorter)
		{ -80000.f,  -1.f,  5000.f },
		{ -100000.f,  1.f,  6000.f },
		{ -160000.f,  1.f,  6000.f },
		{ -200000.f, -1.f,  5000.f },
		{ -240000.f,  1.f,  5000.f },
	};
	for (const auto& SR : SideRoads)
	{
		const float SRCenterY = HwyY + SR.DirY * SR.Len * 0.5f;
		SpawnRoadStrip(FVector(SR.X, SRCenterY + SR.DirY * HwyHalfW, 2.f), 400.f, SR.Len * 0.5f);
	}

	// ---- Cross Streets connecting side road clusters ----
	struct FCrossStreet { float CX; float CY; float HalfLenX; float HalfLenY; };
	const FCrossStreet CrossStreets[] = {
		{ -80000.f,   4500.f,  5000.f, 300.f },
		{ -100000.f, -5000.f,  5000.f, 300.f },
		{ -145000.f,  4000.f,  8000.f, 300.f },
		{ -195000.f, -4000.f,  6000.f, 300.f },
		{ -252000.f,  3000.f,  8000.f, 300.f },
	};
	for (const auto& CS : CrossStreets)
	{
		SpawnRoadStrip(FVector(CS.CX, CS.CY, 2.f), CS.HalfLenX, CS.HalfLenY);
	}

	// ---- Mountain Ranges ----
	auto SpawnMountain = [&](const FVector& Loc, const FVector& Scale, const FLinearColor& Color, const TCHAR* Label)
	{
		AActor* Mt = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!Mt) return;
#if WITH_EDITOR
		Mt->SetActorLabel(Label);
#endif
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Mt, TEXT("MountainMesh"));
		MC->SetStaticMesh(CubeMesh);
		MC->SetWorldScale3D(Scale);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MC->SetCollisionResponseToAllChannels(ECR_Block);
		MC->RegisterComponent();
		Mt->SetRootComponent(MC);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
		if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), Color); MC->SetMaterial(0, Mat); }
		Mt->AddInstanceComponent(MC);
	};

	const FLinearColor MtColors[] = {
		FLinearColor(0.25f, 0.22f, 0.18f),
		FLinearColor(0.30f, 0.28f, 0.22f),
		FLinearColor(0.20f, 0.18f, 0.15f),
		FLinearColor(0.28f, 0.25f, 0.20f),
	};

	// North ridge
	for (int32 m = 0; m < 8; ++m)
	{
		const float MX = -80000.f - m * 18000.f + CityRandom.FRandRange(-3000.f, 3000.f);
		const float MY = 18000.f + CityRandom.FRandRange(-5000.f, 8000.f);
		const float MH = CityRandom.FRandRange(2000.f, 6000.f);
		const float MW = CityRandom.FRandRange(3000.f, 8000.f);
		const float MD = CityRandom.FRandRange(2000.f, 5000.f);
		SpawnMountain(FVector(MX, MY, MH * 0.5f), FVector(MW / 100.f, MD / 100.f, MH / 100.f),
			MtColors[CityRandom.RandRange(0, 3)], TEXT("NorthMt"));
	}

	// South ridge
	for (int32 m = 0; m < 7; ++m)
	{
		const float MX = -85000.f - m * 17000.f + CityRandom.FRandRange(-3000.f, 3000.f);
		const float MY = -18000.f + CityRandom.FRandRange(-8000.f, 5000.f);
		const float MH = CityRandom.FRandRange(1800.f, 5500.f);
		const float MW = CityRandom.FRandRange(2500.f, 7000.f);
		const float MD = CityRandom.FRandRange(2000.f, 4500.f);
		SpawnMountain(FVector(MX, MY, MH * 0.5f), FVector(MW / 100.f, MD / 100.f, MH / 100.f),
			MtColors[CityRandom.RandRange(0, 3)], TEXT("SouthMt"));
	}

	// Far west mountains
	for (int32 m = 0; m < 4; ++m)
	{
		const float MX = -250000.f + CityRandom.FRandRange(-20000.f, 20000.f);
		const float MY = CityRandom.FRandRange(-15000.f, 15000.f);
		const float MH = CityRandom.FRandRange(3000.f, 8000.f);
		const float MW = CityRandom.FRandRange(5000.f, 12000.f);
		const float MD = CityRandom.FRandRange(4000.f, 8000.f);
		SpawnMountain(FVector(MX, MY, MH * 0.5f), FVector(MW / 100.f, MD / 100.f, MH / 100.f),
			MtColors[CityRandom.RandRange(0, 3)], TEXT("FarWestMt"));
	}

	// ---- Roadside Buildings ----
	struct FRoadsideBuilding { float X, Y, W, D, H; FLinearColor Color; };
	const FRoadsideBuilding Buildings[] = {
		{ -75000.f,  3000.f, 400.f, 300.f, 350.f, FLinearColor(0.7f, 0.65f, 0.55f) },   // Motel
		{ -90000.f, -2500.f, 350.f, 250.f, 300.f, FLinearColor(0.8f, 0.3f, 0.2f) },      // Diner
		{ -105000.f, 4000.f, 500.f, 400.f, 250.f, FLinearColor(0.6f, 0.6f, 0.58f) },     // Gas Station
		{ -115000.f,-3500.f, 300.f, 250.f, 280.f, FLinearColor(0.75f, 0.7f, 0.6f) },     // Roadside bar
		{ -135000.f, 2000.f, 600.f, 450.f, 350.f, FLinearColor(0.65f, 0.6f, 0.5f) },     // Trading post
		{ -150000.f,-4000.f, 400.f, 350.f, 300.f, FLinearColor(0.5f, 0.5f, 0.48f) },     // Mechanic
		{ -170000.f, 3500.f, 350.f, 300.f, 250.f, FLinearColor(0.7f, 0.5f, 0.3f) },      // BBQ shack
		{ -185000.f,-2000.f, 500.f, 400.f, 300.f, FLinearColor(0.4f, 0.45f, 0.5f) },     // Warehouse
		{ -205000.f, 3000.f, 400.f, 300.f, 280.f, FLinearColor(0.75f, 0.72f, 0.65f) },   // General store
		{ -220000.f,-3000.f, 300.f, 250.f, 250.f, FLinearColor(0.6f, 0.55f, 0.45f) },    // Outpost
		{ -235000.f, 2500.f, 350.f, 300.f, 300.f, FLinearColor(0.8f, 0.75f, 0.6f) },     // Saloon
		{ -250000.f,-4500.f, 500.f, 400.f, 350.f, FLinearColor(0.55f, 0.55f, 0.52f) },   // Truck stop
		{ -265000.f, 3500.f, 400.f, 350.f, 280.f, FLinearColor(0.7f, 0.6f, 0.45f) },     // Country store
		{ -275000.f,-2000.f, 350.f, 300.f, 250.f, FLinearColor(0.65f, 0.6f, 0.55f) },    // Last chance gas
	};
	for (const auto& B : Buildings)
	{
		SpawnBuilding(FVector(B.X, B.Y, 0.f), FVector(B.W * 0.5f, B.D * 0.5f, B.H), B.Color, false);
	}

	// ---- Building Driveways (short road from highway to each building) ----
	for (const auto& B : Buildings)
	{
		const float HwyEdgeY = (B.Y > 0.f) ? HwyHalfW : -HwyHalfW;
		const float DrivewayLen = FMath::Abs(B.Y - HwyEdgeY);
		const float DrivewayCenterY = (HwyEdgeY + B.Y) * 0.5f;
		SpawnRoadStrip(FVector(B.X, DrivewayCenterY, 2.f), 100.f, DrivewayLen * 0.5f);
	}

	// ---- Scattered Trees ----
	for (int32 t = 0; t < 80; ++t)
	{
		const float TX = CityRandom.FRandRange(HwyEndX + 5000.f, HwyStartX - 5000.f);
		const float TY = CityRandom.FRandRange(-35000.f, 35000.f);
		// Skip if too close to highway
		if (FMath::Abs(TY) < 3000.f) continue;
		const float TH = CityRandom.FRandRange(500.f, 1200.f);
		SpawnPalmTree(FVector(TX, TY, 0.f), TH, false);
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Outskirts spawned (extended highway X=-64k to -280k + mountains + roadside content)."));
}

// ---------------------------------------------------------------------------
// Rolling hills and mountains — drivable terrain around and within the city
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnHills()
{
	UWorld* World = GetWorld();
	if (!World || !SphereMesh || !CubeMesh) return;

	auto SpawnHill = [&](const FVector& Loc, float RadiusX, float RadiusY, float Height,
		const FLinearColor& Color, const TCHAR* Label, bool bCollision = true)
	{
		AActor* Hill = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!Hill) return;
#if WITH_EDITOR
		Hill->SetActorLabel(Label);
#endif

		// Visual mesh — sphere flattened to hill shape (no collision on mesh)
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Hill, TEXT("HillMesh"));
		MC->SetStaticMesh(SphereMesh);
		MC->SetWorldScale3D(FVector(RadiusX * 2.f / 100.f, RadiusY * 2.f / 100.f, Height * 2.f / 100.f));
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);  // Visual only
		MC->RegisterComponent();
		Hill->SetRootComponent(MC);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
		if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), Color); MC->SetMaterial(0, Mat); }
		Hill->AddInstanceComponent(MC);

		if (bCollision)
		{
			// Collision box — sized to cover the drivable surface of the hill
			// Use a box that covers the top half of the hill (where vehicles drive)
			UBoxComponent* HillBox = NewObject<UBoxComponent>(Hill, TEXT("HillCollision"));
			HillBox->SetupAttachment(MC);
			HillBox->SetBoxExtent(FVector(RadiusX * 0.85f, RadiusY * 0.85f, Height * 0.5f));
			HillBox->SetWorldLocation(FVector(Loc.X, Loc.Y, Loc.Z));
			HillBox->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			HillBox->SetCollisionObjectType(ECC_WorldStatic);
			HillBox->SetCollisionResponseToAllChannels(ECR_Block);
			HillBox->SetHiddenInGame(true);
			HillBox->RegisterComponent();
			Hill->AddInstanceComponent(HillBox);
		}
	};

	// Color palette for hills
	const FLinearColor GrassGreen(0.18f, 0.35f, 0.10f);
	const FLinearColor DarkGrass(0.12f, 0.28f, 0.08f);
	const FLinearColor DryGrass(0.35f, 0.32f, 0.15f);
	const FLinearColor RockyBrown(0.30f, 0.25f, 0.18f);
	const FLinearColor EarthBrown(0.25f, 0.20f, 0.12f);
	const FLinearColor MountainGray(0.35f, 0.33f, 0.30f);

	// ---- Rolling hills within city edges (between city and outskirts) ----
	// These are gentle, drivable slopes around the city perimeter

	// Northwest cluster
	SpawnHill(FVector(-55000.f, 30000.f, 0.f), 3000.f, 2500.f, 400.f, GrassGreen, TEXT("Hill_NW1"));
	SpawnHill(FVector(-58000.f, 35000.f, 0.f), 2000.f, 2200.f, 250.f, DarkGrass, TEXT("Hill_NW2"));
	SpawnHill(FVector(-52000.f, 38000.f, 0.f), 2500.f, 1800.f, 350.f, DryGrass, TEXT("Hill_NW3"));

	// Northeast cluster
	SpawnHill(FVector(55000.f, 35000.f, 0.f), 3500.f, 3000.f, 500.f, GrassGreen, TEXT("Hill_NE1"));
	SpawnHill(FVector(58000.f, 28000.f, 0.f), 2200.f, 2000.f, 300.f, DarkGrass, TEXT("Hill_NE2"));
	SpawnHill(FVector(52000.f, 42000.f, 0.f), 2800.f, 2400.f, 450.f, GrassGreen, TEXT("Hill_NE3"));

	// Southwest cluster (near highway)
	SpawnHill(FVector(-60000.f, -25000.f, 0.f), 2500.f, 2000.f, 350.f, DryGrass, TEXT("Hill_SW1"));
	SpawnHill(FVector(-65000.f, -30000.f, 0.f), 3000.f, 2800.f, 500.f, GrassGreen, TEXT("Hill_SW2"));
	SpawnHill(FVector(-55000.f, -35000.f, 0.f), 2000.f, 1800.f, 280.f, DarkGrass, TEXT("Hill_SW3"));

	// Southeast cluster
	SpawnHill(FVector(55000.f, -30000.f, 0.f), 3200.f, 2600.f, 450.f, GrassGreen, TEXT("Hill_SE1"));
	SpawnHill(FVector(60000.f, -25000.f, 0.f), 2500.f, 2200.f, 380.f, DryGrass, TEXT("Hill_SE2"));

	// ---- Larger hills / small mountains along the highway corridor ----
	SpawnHill(FVector(-90000.f, 15000.f, 0.f), 5000.f, 4000.f, 800.f, RockyBrown, TEXT("Hill_Hwy1"));
	SpawnHill(FVector(-110000.f, -12000.f, 0.f), 4500.f, 3500.f, 700.f, EarthBrown, TEXT("Hill_Hwy2"));
	SpawnHill(FVector(-140000.f, 18000.f, 0.f), 6000.f, 5000.f, 1200.f, MountainGray, TEXT("Mtn_Hwy1"));
	SpawnHill(FVector(-170000.f, -15000.f, 0.f), 5500.f, 4500.f, 1000.f, RockyBrown, TEXT("Mtn_Hwy2"));
	SpawnHill(FVector(-200000.f, 20000.f, 0.f), 7000.f, 5500.f, 1500.f, MountainGray, TEXT("Mtn_Hwy3"));
	SpawnHill(FVector(-230000.f, -18000.f, 0.f), 6000.f, 5000.f, 1300.f, EarthBrown, TEXT("Mtn_Hwy4"));

	// ---- Large mountain backdrop behind the highway ----
	SpawnHill(FVector(-260000.f, 0.f, 0.f), 12000.f, 8000.f, 3000.f, MountainGray, TEXT("Mtn_Backdrop1"));
	SpawnHill(FVector(-250000.f, 25000.f, 0.f), 10000.f, 7000.f, 2500.f, RockyBrown, TEXT("Mtn_Backdrop2"));
	SpawnHill(FVector(-255000.f, -22000.f, 0.f), 11000.f, 9000.f, 2800.f, EarthBrown, TEXT("Mtn_Backdrop3"), false);

	// ---- Northern transition zone: hills from city edge to mountain base ----
	// (fill the 50k gap between city perimeter hills at Y=42k and mountains at Y=96k)
	SpawnHill(FVector(-40000.f, 55000.f, 0.f), 4000.f, 3500.f, 600.f, GrassGreen, TEXT("Hill_NT1"));
	SpawnHill(FVector(-10000.f, 60000.f, 0.f), 3500.f, 3000.f, 450.f, DarkGrass, TEXT("Hill_NT2"));
	SpawnHill(FVector( 25000.f, 58000.f, 0.f), 4500.f, 3800.f, 700.f, DryGrass, TEXT("Hill_NT3"));
	SpawnHill(FVector( 60000.f, 55000.f, 0.f), 3800.f, 3200.f, 520.f, GrassGreen, TEXT("Hill_NT4"));
	SpawnHill(FVector(-60000.f, 68000.f, 0.f), 5000.f, 4000.f, 900.f, RockyBrown, TEXT("Hill_NT5"));
	SpawnHill(FVector(-20000.f, 72000.f, 0.f), 4500.f, 4000.f, 800.f, EarthBrown, TEXT("Hill_NT6"));
	SpawnHill(FVector( 15000.f, 75000.f, 0.f), 5500.f, 4500.f, 1100.f, MountainGray, TEXT("Hill_NT7"));
	SpawnHill(FVector( 50000.f, 70000.f, 0.f), 4200.f, 3500.f, 650.f, DarkGrass, TEXT("Hill_NT8"));
	SpawnHill(FVector(-45000.f, 82000.f, 0.f), 6000.f, 5000.f, 1400.f, MountainGray, TEXT("Hill_NT9"));
	SpawnHill(FVector(  5000.f, 85000.f, 0.f), 5500.f, 4800.f, 1200.f, RockyBrown, TEXT("Hill_NT10"));
	SpawnHill(FVector( 40000.f, 83000.f, 0.f), 5000.f, 4200.f, 1000.f, EarthBrown, TEXT("Hill_NT11"));
	SpawnHill(FVector( 75000.f, 78000.f, 0.f), 4500.f, 3800.f, 850.f, RockyBrown, TEXT("Hill_NT12"));
	SpawnHill(FVector(-75000.f, 75000.f, 0.f), 5000.f, 4500.f, 1050.f, EarthBrown, TEXT("Hill_NT13"));

	// ---- Southern transition zone: hills toward southern mountains ----
	SpawnHill(FVector(-45000.f, -55000.f, 0.f), 4000.f, 3500.f, 550.f, GrassGreen, TEXT("Hill_ST1"));
	SpawnHill(FVector(-10000.f, -60000.f, 0.f), 3500.f, 3200.f, 480.f, DarkGrass, TEXT("Hill_ST2"));
	SpawnHill(FVector( 30000.f, -58000.f, 0.f), 4200.f, 3600.f, 650.f, DryGrass, TEXT("Hill_ST3"));
	SpawnHill(FVector( 65000.f, -55000.f, 0.f), 3800.f, 3000.f, 500.f, GrassGreen, TEXT("Hill_ST4"));
	SpawnHill(FVector(-55000.f, -70000.f, 0.f), 5000.f, 4200.f, 950.f, RockyBrown, TEXT("Hill_ST5"));
	SpawnHill(FVector(-15000.f, -75000.f, 0.f), 4800.f, 4000.f, 850.f, EarthBrown, TEXT("Hill_ST6"));
	SpawnHill(FVector( 20000.f, -72000.f, 0.f), 5200.f, 4500.f, 1050.f, MountainGray, TEXT("Hill_ST7"));
	SpawnHill(FVector( 55000.f, -68000.f, 0.f), 4500.f, 3800.f, 700.f, DarkGrass, TEXT("Hill_ST8"));
	SpawnHill(FVector(-40000.f, -82000.f, 0.f), 5500.f, 4800.f, 1300.f, MountainGray, TEXT("Hill_ST9"));
	SpawnHill(FVector(  0000.f, -85000.f, 0.f), 5000.f, 4500.f, 1100.f, RockyBrown, TEXT("Hill_ST10"));
	SpawnHill(FVector( 45000.f, -80000.f, 0.f), 5200.f, 4200.f, 1000.f, EarthBrown, TEXT("Hill_ST11"));
	SpawnHill(FVector( 75000.f, -75000.f, 0.f), 4500.f, 3800.f, 800.f, RockyBrown, TEXT("Hill_ST12"));
	SpawnHill(FVector(-70000.f, -78000.f, 0.f), 4800.f, 4200.f, 950.f, EarthBrown, TEXT("Hill_ST13"));
	// ---- Southern far transition (bridge gap to mountains now at Y~-130k) ----
	SpawnHill(FVector(-60000.f, -100000.f, 0.f), 5500.f, 5000.f, 1500.f, MountainGray, TEXT("Hill_ST14"));
	SpawnHill(FVector(-20000.f, -105000.f, 0.f), 5200.f, 4500.f, 1400.f, RockyBrown, TEXT("Hill_ST15"));
	SpawnHill(FVector( 25000.f, -102000.f, 0.f), 5800.f, 5000.f, 1600.f, MountainGray, TEXT("Hill_ST16"));
	SpawnHill(FVector( 60000.f,  -98000.f, 0.f), 5000.f, 4200.f, 1200.f, EarthBrown, TEXT("Hill_ST17"));
	SpawnHill(FVector(-45000.f, -115000.f, 0.f), 6000.f, 5500.f, 1800.f, MountainGray, TEXT("Hill_ST18"));
	SpawnHill(FVector(  5000.f, -118000.f, 0.f), 5500.f, 5000.f, 1700.f, RockyBrown, TEXT("Hill_ST19"));
	SpawnHill(FVector( 50000.f, -112000.f, 0.f), 5800.f, 5200.f, 1600.f, MountainGray, TEXT("Hill_ST20"));
	SpawnHill(FVector( 80000.f, -108000.f, 0.f), 5200.f, 4500.f, 1400.f, EarthBrown, TEXT("Hill_ST21"));

	// ---- Eastern coastal hills: backdrop visible from city looking east ----
	SpawnHill(FVector( 80000.f,  50000.f, 0.f), 5000.f, 4000.f, 800.f, GrassGreen, TEXT("Hill_EC1"));
	SpawnHill(FVector( 90000.f,  25000.f, 0.f), 4500.f, 3800.f, 650.f, DarkGrass, TEXT("Hill_EC2"));
	SpawnHill(FVector( 85000.f,   0000.f, 0.f), 5500.f, 4500.f, 950.f, RockyBrown, TEXT("Hill_EC3"));
	SpawnHill(FVector( 95000.f, -20000.f, 0.f), 4000.f, 3500.f, 550.f, DryGrass, TEXT("Hill_EC4"));
	SpawnHill(FVector( 88000.f, -45000.f, 0.f), 5000.f, 4200.f, 750.f, GrassGreen, TEXT("Hill_EC5"));
	SpawnHill(FVector(100000.f,  60000.f, 0.f), 6000.f, 5000.f, 1200.f, MountainGray, TEXT("Hill_EC6"));
	SpawnHill(FVector(105000.f, -55000.f, 0.f), 5500.f, 4800.f, 1100.f, MountainGray, TEXT("Hill_EC7"));
	SpawnHill(FVector(110000.f,  10000.f, 0.f), 7000.f, 5500.f, 1500.f, MountainGray, TEXT("Hill_EC8"));
	SpawnHill(FVector(108000.f, -35000.f, 0.f), 6000.f, 5000.f, 1300.f, RockyBrown, TEXT("Hill_EC9"));
	SpawnHill(FVector(105000.f,  40000.f, 0.f), 5500.f, 4500.f, 1000.f, EarthBrown, TEXT("Hill_EC10"));

	// ---- Inner scatter: fill empty flat zones within the city outskirts ----
	SpawnHill(FVector(-35000.f, 45000.f, 0.f), 2800.f, 2200.f, 300.f, GrassGreen, TEXT("Hill_IS1"));
	SpawnHill(FVector( 40000.f, 45000.f, 0.f), 3000.f, 2500.f, 350.f, DarkGrass, TEXT("Hill_IS2"));
	SpawnHill(FVector(-40000.f, -42000.f, 0.f), 2500.f, 2000.f, 280.f, DryGrass, TEXT("Hill_IS3"));
	SpawnHill(FVector( 45000.f, -40000.f, 0.f), 2800.f, 2400.f, 320.f, GrassGreen, TEXT("Hill_IS4"));
	SpawnHill(FVector( 70000.f,  15000.f, 0.f), 3500.f, 2800.f, 420.f, DarkGrass, TEXT("Hill_IS5"));
	SpawnHill(FVector( 68000.f, -15000.f, 0.f), 3200.f, 2600.f, 380.f, DryGrass, TEXT("Hill_IS6"));

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Spawned 77 hills — city perimeter, transition zones, eastern coastal, inner scatter, highway corridor, and backdrop mountains."));
}

// ---------------------------------------------------------------------------
// Underground Cavern — removed
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnUndergroundCavern()
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh || !CylinderMesh || !ConeMesh || !SphereMesh || !PlaneMesh) return;

	int32 Idx = 0;
	(void)Idx; // suppress warning — used in lambdas below

	// ================================================================
	// Helper lambdas
	// ================================================================
	auto AP = [&](AActor* O, UStaticMesh* Mesh, const FVector& Loc, const FVector& S,
		const FRotator& Rot, const FLinearColor& Col, float Rough, float Metal,
		bool bColl = false) -> UStaticMeshComponent*
	{
		FString N = FString::Printf(TEXT("AP_%d"), Idx++);
		auto* MC = NewObject<UStaticMeshComponent>(O, *N);
		MC->SetStaticMesh(Mesh);
		MC->SetWorldScale3D(S); MC->SetWorldLocation(Loc); MC->SetWorldRotation(Rot);
		if (bColl)
		{
			MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MC->SetCollisionObjectType(ECC_WorldStatic);
			MC->SetCollisionResponseToAllChannels(ECR_Block);
		}
		else { MC->SetCollisionEnabled(ECollisionEnabled::NoCollision); }
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		ApplyPBR(MC, Col, Metal, Rough);
		O->AddInstanceComponent(MC);
		return MC;
	};

	auto APE = [&](AActor* O, UStaticMesh* Mesh, const FVector& Loc, const FVector& S,
		const FRotator& Rot, const FLinearColor& Col, float Rough, float Metal,
		const FLinearColor& Emissive) -> UStaticMeshComponent*
	{
		auto* MC = AP(O, Mesh, Loc, S, Rot, Col, Rough, Metal);
		if (MC) ApplyPBR(MC, Col, Metal, Rough, Emissive);
		return MC;
	};

	auto APGlass = [&](AActor* O, const FVector& Loc, const FVector& S,
		const FRotator& Rot, const FLinearColor& Col, float Opacity) -> UStaticMeshComponent*
	{
		FString N = FString::Printf(TEXT("AP_%d"), Idx++);
		auto* MC = NewObject<UStaticMeshComponent>(O, *N);
		MC->SetStaticMesh(CubeMesh);
		MC->SetWorldScale3D(S); MC->SetWorldLocation(Loc); MC->SetWorldRotation(Rot);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		if (GlassMaterial)
		{
			UMaterialInstanceDynamic* MI = UMaterialInstanceDynamic::Create(GlassMaterial, MC);
			if (MI) { MI->SetVectorParameterValue(TEXT("BaseColor"), Col); MI->SetScalarParameterValue(TEXT("Opacity"), Opacity); MI->SetScalarParameterValue(TEXT("Roughness"), 0.1f); MC->SetMaterial(0, MI); }
		}
		O->AddInstanceComponent(MC);
		return MC;
	};

	auto APLight = [&](AActor* O, const FVector& Loc, FColor Col, float Intensity, float Radius)
	{
		FString N = FString::Printf(TEXT("APL_%d"), Idx++);
		auto* PL = NewObject<UPointLightComponent>(O, *N);
		PL->SetWorldLocation(Loc);
		PL->SetLightColor(Col);
		PL->SetIntensity(Intensity);
		PL->SetAttenuationRadius(Radius);
		PL->SetCastShadows(false);
		PL->RegisterComponent();
		O->AddInstanceComponent(PL);
	};

	// ================================================================
	// Constants & Colors
	// ================================================================
	// Airport depth and cavern extents
	const float AirportZ = -8000.f;         // floor level
	const float CeilingZ = -3000.f;         // ceiling
	// Cavern: X 100000..200000, Y -10000..70000
	const float CavXMin = 100000.f, CavXMax = 200000.f;
	const float CavYMin = -10000.f, CavYMax = 70000.f;
	const float CavCX = (CavXMin + CavXMax) * 0.5f; // 150000
	const float CavCY = (CavYMin + CavYMax) * 0.5f;  // 30000
	const float CavW = CavXMax - CavXMin;             // 100000
	const float CavH = CavYMax - CavYMin;             // 80000

	// Hole entrance
	const float HX = 112000.f;
	const float HY = 30000.f;

	// Luxury palette
	const FLinearColor MarbleWhite(0.88f, 0.86f, 0.82f);
	const FLinearColor MarbleCream(0.82f, 0.78f, 0.70f);
	const FLinearColor PolishedGranite(0.22f, 0.22f, 0.24f);
	const FLinearColor DarkSteel(0.12f, 0.12f, 0.15f);
	const FLinearColor BrushedSteel(0.55f, 0.55f, 0.58f);
	const FLinearColor Gold(0.90f, 0.75f, 0.30f);
	const FLinearColor DarkGold(0.70f, 0.55f, 0.20f);
	const FLinearColor Asphalt(0.06f, 0.06f, 0.08f);
	const FLinearColor RwyWhite(0.92f, 0.92f, 0.92f);
	const FLinearColor TermGlass(0.08f, 0.15f, 0.30f);
	const FLinearColor CyanGlow(0.0f, 2.0f, 3.0f);
	const FLinearColor WarmGlow(3.0f, 2.0f, 1.0f);
	const FLinearColor GoldGlow(4.0f, 3.0f, 1.0f);
	const FLinearColor RedGlow(4.0f, 0.3f, 0.3f);
	const FLinearColor GreenGlow(0.3f, 3.0f, 0.3f);
	const FLinearColor BlueGlow(0.3f, 0.8f, 4.0f);

	// ================================================================
	// MAIN ACTOR — holds the cavern shell
	// ================================================================
	AActor* CA = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector(CavCX, CavCY, AirportZ), FRotator::ZeroRotator);
	if (!CA) return;
#if WITH_EDITOR
	CA->SetActorLabel(TEXT("SecretAirport"));
#endif

	// ================================================================
	// SECTION 1: CAVERN SHELL
	// ================================================================
	// Floor — COLLISION (this is what the player walks on!)
	auto* FloorMC = AP(CA, CubeMesh, FVector(CavCX, CavCY, AirportZ - 1.f),
		FVector(CavW / 100.f, CavH / 100.f, 0.02f), FRotator::ZeroRotator,
		PolishedGranite, 0.30f, 0.15f, /*bColl=*/true);
	CA->SetRootComponent(FloorMC);

	// Luxury floor tiles — alternating marble/granite checkerboard
	for (float tx = CavXMin + 2500.f; tx < CavXMax; tx += 5000.f)
	{
		for (float ty = CavYMin + 2500.f; ty < CavYMax; ty += 5000.f)
		{
			const bool bLight = ((int32)((tx - CavXMin) / 5000.f) + (int32)((ty - CavYMin) / 5000.f)) % 2 == 0;
			AP(CA, CubeMesh, FVector(tx, ty, AirportZ + 1.f),
				FVector(50.f, 50.f, 0.01f), FRotator::ZeroRotator,
				bLight ? MarbleWhite : MarbleCream, bLight ? 0.20f : 0.25f, 0.05f);
		}
	}

	// Ceiling
	AP(CA, CubeMesh, FVector(CavCX, CavCY, CeilingZ),
		FVector(CavW / 100.f, CavH / 100.f, 0.5f), FRotator::ZeroRotator,
		DarkSteel, 0.85f, 0.20f);

	// Walls (4)
	AP(CA, CubeMesh, FVector(CavCX, CavYMax, (AirportZ + CeilingZ) * 0.5f),
		FVector(CavW / 100.f, 0.5f, (CeilingZ - AirportZ) / 100.f), FRotator::ZeroRotator,
		DarkSteel, 0.70f, 0.30f);
	AP(CA, CubeMesh, FVector(CavCX, CavYMin, (AirportZ + CeilingZ) * 0.5f),
		FVector(CavW / 100.f, 0.5f, (CeilingZ - AirportZ) / 100.f), FRotator::ZeroRotator,
		DarkSteel, 0.70f, 0.30f);
	AP(CA, CubeMesh, FVector(CavXMax, CavCY, (AirportZ + CeilingZ) * 0.5f),
		FVector(0.5f, CavH / 100.f, (CeilingZ - AirportZ) / 100.f), FRotator::ZeroRotator,
		DarkSteel, 0.70f, 0.30f);
	AP(CA, CubeMesh, FVector(CavXMin, CavCY, (AirportZ + CeilingZ) * 0.5f),
		FVector(0.5f, CavH / 100.f, (CeilingZ - AirportZ) / 100.f), FRotator::ZeroRotator,
		DarkSteel, 0.70f, 0.30f);

	// Ceiling structural beams (gold-accented steel I-beams)
	for (float bx = CavXMin + 10000.f; bx <= CavXMax - 5000.f; bx += 20000.f)
	{
		AP(CA, CubeMesh, FVector(bx, CavCY, CeilingZ + 50.f),
			FVector(2.f, CavH / 100.f, 3.f), FRotator::ZeroRotator,
			BrushedSteel, 0.35f, 0.60f);
		// Gold accent strip on beam bottom
		APE(CA, CubeMesh, FVector(bx, CavCY, CeilingZ + 10.f),
			FVector(1.f, CavH / 100.f, 0.2f), FRotator::ZeroRotator,
			Gold, 0.25f, 0.80f, GoldGlow * 0.3f);
	}

	// ================================================================
	// LIGHTING — flood the space with light
	// ================================================================
	// Massive overhead grid (4x3)
	for (int32 lx = 0; lx < 4; ++lx)
		for (int32 ly = 0; ly < 3; ++ly)
			APLight(CA, FVector(CavXMin + 15000.f + lx * 25000.f, CavYMin + 15000.f + ly * 25000.f, CeilingZ + 200.f),
				FColor(255, 245, 230), 40000.f, 30000.f);

	// Gold wall-wash uplights around perimeter
	for (float wx = CavXMin + 10000.f; wx <= CavXMax - 5000.f; wx += 15000.f)
	{
		APLight(CA, FVector(wx, CavYMin + 500.f, AirportZ + 100.f), FColor(255, 200, 100), 5000.f, 8000.f);
		APLight(CA, FVector(wx, CavYMax - 500.f, AirportZ + 100.f), FColor(255, 200, 100), 5000.f, 8000.f);
	}

	// ================================================================
	// SECTION 2: TUNNEL ENTRANCE + STAIRCASE
	// ================================================================
	// Single clear tunnel entrance at beach.  The tunnel is a 2000 UU wide
	// corridor heading WEST (negative X) from the beacon location.
	// A staircase inside descends to the airport.
	//
	// CHARACTER GROUND-FOLLOW RULES (from PHCCityCharacter.cpp):
	//   LineTraceMulti from Pos.Z+5000 to Pos.Z-2000
	//   Snaps to hit surface IF TargetZ >= Pos.Z - 100
	//   Otherwise: gravity → floor clamp Z=58
	// Therefore each stair step must drop ≤ 80 UU from the previous one,
	// and the FIRST step must be a raised platform the trace will find.

	AActor* HA = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector(HX, HY, 0.f), FRotator::ZeroRotator);
	if (!HA) return;
#if WITH_EDITOR
	HA->SetActorLabel(TEXT("AirportEntrance"));
#endif

	// --- Beacon tower (visible from far away) ---
	APE(HA, CylinderMesh, FVector(HX, HY, 2500.f), FVector(8.f, 8.f, 50.f), FRotator::ZeroRotator,
		Gold, 0.20f, 0.90f, GoldGlow * 2.f);
	APE(HA, SphereMesh, FVector(HX, HY, 5200.f), FVector(20.f, 20.f, 20.f), FRotator::ZeroRotator,
		FLinearColor(1.f, 0.8f, 0.2f), 0.15f, 0.90f, FLinearColor(8.f, 6.f, 2.f));
	APLight(HA, FVector(HX, HY, 5500.f), FColor(255, 220, 80), 80000.f, 50000.f);

	// --- Tunnel dimensions ---
	const float TunW = 2000.f;   // tunnel width (Y extent)
	const float TunH = 1500.f;   // tunnel height (Z extent above floor)
	const float TunWall = 100.f;  // wall thickness

	// --- Helper: spawn a standalone collision slab as its own actor ---
	// Components added to distant actors don't reliably register collision.
	// Each stair step is its own actor with the mesh as root → guaranteed
	// to appear in LineTraceMultiByChannel.
	auto SpawnCollisionSlab = [&](const FVector& Loc, const FVector& Scale,
		const FLinearColor& Col, float Rough, float Metal)
	{
		AActor* SA = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!SA) return;
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(SA, TEXT("SlabMesh"));
		MC->SetStaticMesh(CubeMesh);
		MC->SetWorldLocation(Loc);
		MC->SetWorldScale3D(Scale);
		MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MC->SetCollisionObjectType(ECC_WorldStatic);
		MC->SetCollisionResponseToAllChannels(ECR_Block);
		MC->RegisterComponent();
		SA->SetRootComponent(MC);
		ApplyPBR(MC, Col, Metal, Rough);
	};

	// --- Entry platform: walk onto this to begin descent ---
	const float PlatZ = 60.f;
	const float PlatLen = 4000.f;
	const float PlatCX = HX + PlatLen * 0.5f;
	SpawnCollisionSlab(FVector(PlatCX, HY, PlatZ),
		FVector(PlatLen / 100.f, TunW / 100.f, 0.5f),
		PolishedGranite, 0.30f, 0.15f);

	// Bright emissive floor strip
	APE(HA, CubeMesh, FVector(PlatCX, HY, PlatZ + 27.f),
		FVector(PlatLen / 100.f, 3.f, 0.05f), FRotator::ZeroRotator,
		Gold, 0.25f, 0.80f, GoldGlow);
	// Platform walls
	AP(HA, CubeMesh, FVector(PlatCX, HY - TunW * 0.5f - TunWall * 0.5f, PlatZ + TunH * 0.5f),
		FVector(PlatLen / 100.f, TunWall / 100.f, TunH / 100.f), FRotator::ZeroRotator,
		DarkSteel, 0.70f, 0.30f);
	AP(HA, CubeMesh, FVector(PlatCX, HY + TunW * 0.5f + TunWall * 0.5f, PlatZ + TunH * 0.5f),
		FVector(PlatLen / 100.f, TunWall / 100.f, TunH / 100.f), FRotator::ZeroRotator,
		DarkSteel, 0.70f, 0.30f);
	// Platform ceiling
	AP(HA, CubeMesh, FVector(PlatCX, HY, PlatZ + TunH),
		FVector(PlatLen / 100.f, (TunW + TunWall * 2.f) / 100.f, 0.5f), FRotator::ZeroRotator,
		DarkSteel, 0.85f, 0.20f);
	// Back wall
	AP(HA, CubeMesh, FVector(HX + PlatLen + TunWall * 0.5f, HY, PlatZ + TunH * 0.5f),
		FVector(TunWall / 100.f, (TunW + TunWall * 2.f) / 100.f, TunH / 100.f), FRotator::ZeroRotator,
		DarkSteel, 0.70f, 0.30f);
	// Emissive sign
	APE(HA, CubeMesh, FVector(HX - 50.f, HY, PlatZ + TunH - 200.f),
		FVector(0.1f, 15.f, 3.f), FRotator::ZeroRotator,
		FLinearColor(0.0f, 0.0f, 0.0f), 0.30f, 0.50f, FLinearColor(0.f, 5.f, 0.f));
	APLight(HA, FVector(PlatCX, HY, PlatZ + TunH - 100.f), FColor(255, 240, 200), 10000.f, 5000.f);

	// --- STAIRCASE: each step is a standalone collision actor ---
	const float StairTopZ = PlatZ;
	const float StepDrop = 80.f;
	const int32 NumSteps = (int32)FMath::CeilToFloat((StairTopZ - AirportZ) / StepDrop);
	const float StairStartX = HX;
	const float StairEndX = CavXMin + 2000.f;
	const float StairDX = (StairEndX - StairStartX) / (float)NumSteps;

	for (int32 s = 0; s < NumSteps; ++s)
	{
		const float SX = StairStartX + (s + 0.5f) * StairDX;
		const float SZ = StairTopZ - (s + 0.5f) * StepDrop;

		// Each stair step = standalone collision actor
		SpawnCollisionSlab(FVector(SX, HY, SZ),
			FVector(FMath::Abs(StairDX) / 100.f + 0.5f, TunW / 100.f, 0.5f),
			PolishedGranite, 0.30f, 0.15f);

		// Tunnel walls + lights (visual only, on HA actor)
		if (s % 10 == 0)
		{
			AP(HA, CubeMesh, FVector(SX, HY - TunW * 0.5f - TunWall * 0.5f, SZ + TunH * 0.5f),
				FVector(FMath::Abs(StairDX) * 10.f / 100.f + 1.f, TunWall / 100.f, TunH / 100.f), FRotator::ZeroRotator,
				DarkSteel, 0.70f, 0.30f);
			AP(HA, CubeMesh, FVector(SX, HY + TunW * 0.5f + TunWall * 0.5f, SZ + TunH * 0.5f),
				FVector(FMath::Abs(StairDX) * 10.f / 100.f + 1.f, TunWall / 100.f, TunH / 100.f), FRotator::ZeroRotator,
				DarkSteel, 0.70f, 0.30f);
		}
		if (s % 15 == 0)
			APLight(HA, FVector(SX, HY, SZ + TunH - 50.f), FColor(255, 220, 160), 8000.f, 5000.f);
	}

	// Tunnel ceiling over staircase
	AP(HA, CubeMesh, FVector((StairStartX + StairEndX) * 0.5f, HY, -2000.f),
		FVector(FMath::Abs(StairEndX - StairStartX) / 100.f + 5.f, (TunW + TunWall * 2.f) / 100.f, 0.5f), FRotator::ZeroRotator,
		DarkSteel, 0.85f, 0.20f);

	// ================================================================
	// SECTION 3: RUNWAYS (2 main + 1 taxiway)
	// ================================================================
	const float Rwy1Y = 15000.f;
	const float Rwy2Y = 40000.f;
	const float TaxiY = 55000.f;
	const float RwyLen = 80000.f;  // huge runway
	const float RwyW = 3000.f;
	const float RwyXC = CavCX;

	// Runway 1
	AP(CA, CubeMesh, FVector(RwyXC, Rwy1Y, AirportZ + 2.f),
		FVector(RwyLen / 100.f, RwyW / 100.f, 0.03f), FRotator::ZeroRotator,
		Asphalt, 0.70f, 0.0f, /*bColl=*/true);
	// Runway 2
	AP(CA, CubeMesh, FVector(RwyXC, Rwy2Y, AirportZ + 2.f),
		FVector(RwyLen / 100.f, RwyW / 100.f, 0.03f), FRotator::ZeroRotator,
		Asphalt, 0.70f, 0.0f, /*bColl=*/true);
	// Taxiway
	AP(CA, CubeMesh, FVector(RwyXC, TaxiY, AirportZ + 2.f),
		FVector(60000.f / 100.f, 2000.f / 100.f, 0.03f), FRotator::ZeroRotator,
		FLinearColor(0.10f, 0.10f, 0.12f), 0.75f, 0.0f, /*bColl=*/true);

	// Centerline dashes on runways
	for (int32 rwy = 0; rwy < 2; ++rwy)
	{
		const float RY = (rwy == 0) ? Rwy1Y : Rwy2Y;
		for (int32 d = 0; d < 25; ++d)
		{
			const float DX = RwyXC - RwyLen * 0.45f + d * (RwyLen * 0.9f / 24.f);
			AP(CA, CubeMesh, FVector(DX, RY, AirportZ + 4.f),
				FVector(12.f, 1.f, 0.02f), FRotator::ZeroRotator,
				RwyWhite, 0.60f, 0.0f);
		}
	}

	// Runway edge lights
	for (int32 rwy = 0; rwy < 2; ++rwy)
	{
		const float RY = (rwy == 0) ? Rwy1Y : Rwy2Y;
		for (int32 li = 0; li < 16; ++li)
		{
			const float LX = RwyXC - RwyLen * 0.45f + li * (RwyLen * 0.9f / 15.f);
			APLight(CA, FVector(LX, RY - RwyW * 0.5f - 100.f, AirportZ + 50.f),
				FColor(100, 200, 255), 1500.f, 1200.f);
			APLight(CA, FVector(LX, RY + RwyW * 0.5f + 100.f, AirportZ + 50.f),
				FColor(100, 200, 255), 1500.f, 1200.f);
		}
		// Threshold lights
		APLight(CA, FVector(RwyXC - RwyLen * 0.48f, RY, AirportZ + 50.f), FColor(50, 255, 50), 3000.f, 2000.f);
		APLight(CA, FVector(RwyXC + RwyLen * 0.48f, RY, AirportZ + 50.f), FColor(255, 50, 50), 3000.f, 2000.f);
	}

	// Emissive cyan taxiway guide lines
	for (int32 m = 0; m < 12; ++m)
	{
		const float MX = CavXMin + 5000.f + m * 8000.f;
		APE(CA, CubeMesh, FVector(MX, (Rwy1Y + Rwy2Y) * 0.5f, AirportZ + 1.f),
			FVector(1.5f, 200.f, 0.01f), FRotator::ZeroRotator,
			FLinearColor(0.02f, 0.04f, 0.06f), 0.50f, 0.0f, CyanGlow);
	}

	// ================================================================
	// SECTION 4: LUXURY TERMINAL
	// ================================================================
	AActor* TA = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector(CavXMin + 5000.f, CavCY, AirportZ), FRotator::ZeroRotator);
	if (!TA) return;
#if WITH_EDITOR
	TA->SetActorLabel(TEXT("LuxuryTerminal"));
#endif

	const float TermX = CavXMin + 8000.f; // 108000
	const float TermW = 12000.f;
	const float TermD = 40000.f;
	const float TermH = 4000.f;

	// Main terminal body — white marble
	auto* TR = AP(TA, CubeMesh, FVector(TermX, CavCY, AirportZ + TermH * 0.5f),
		FVector(TermW / 100.f, TermD / 100.f, TermH / 100.f), FRotator::ZeroRotator,
		MarbleWhite, 0.20f, 0.05f);
	TA->SetRootComponent(TR);

	// Glass curtain wall (runway side)
	APGlass(TA, FVector(TermX + TermW * 0.5f + 10.f, CavCY, AirportZ + TermH * 0.5f),
		FVector(0.5f, (TermD - 200.f) / 100.f, (TermH - 200.f) / 100.f), FRotator::ZeroRotator,
		TermGlass, 0.30f);

	// Glass curtain wall (entrance side)
	APGlass(TA, FVector(TermX - TermW * 0.5f - 10.f, CavCY, AirportZ + TermH * 0.5f),
		FVector(0.5f, (TermD - 200.f) / 100.f, (TermH - 200.f) / 100.f), FRotator::ZeroRotator,
		TermGlass, 0.30f);

	// Roof with gold edge trim
	AP(TA, CubeMesh, FVector(TermX, CavCY, AirportZ + TermH + 50.f),
		FVector((TermW + 1000.f) / 100.f, (TermD + 1000.f) / 100.f, 2.f), FRotator::ZeroRotator,
		BrushedSteel, 0.35f, 0.50f);
	APE(TA, CubeMesh, FVector(TermX, CavCY, AirportZ + TermH + 10.f),
		FVector((TermW + 800.f) / 100.f, (TermD + 800.f) / 100.f, 0.3f), FRotator::ZeroRotator,
		Gold, 0.25f, 0.80f, GoldGlow * 0.2f);

	// 6 marble columns along glass facade
	for (int32 col = 0; col < 6; ++col)
	{
		const float CY = CavCY - TermD * 0.4f + col * (TermD * 0.8f / 5.f);
		AP(TA, CylinderMesh, FVector(TermX + TermW * 0.5f + 200.f, CY, AirportZ + TermH * 0.5f),
			FVector(2.f, 2.f, TermH / 100.f), FRotator::ZeroRotator,
			MarbleCream, 0.20f, 0.05f);
		// Gold column capital
		AP(TA, CylinderMesh, FVector(TermX + TermW * 0.5f + 200.f, CY, AirportZ + TermH - 100.f),
			FVector(3.f, 3.f, 1.5f), FRotator::ZeroRotator,
			Gold, 0.25f, 0.80f);
	}

	// Terminal interior lights (warm luxury glow)
	for (int32 ti = 0; ti < 4; ++ti)
		APLight(TA, FVector(TermX, CavCY - 12000.f + ti * 8000.f, AirportZ + TermH - 200.f),
			FColor(255, 230, 200), 20000.f, 10000.f);

	// Emissive departure/arrival signs
	APE(TA, CubeMesh, FVector(TermX + TermW * 0.5f + 20.f, CavCY + 8000.f, AirportZ + TermH - 500.f),
		FVector(0.3f, 25.f, 5.f), FRotator::ZeroRotator,
		FLinearColor(0.02f, 0.05f, 0.02f), 0.50f, 0.0f, GreenGlow);
	APE(TA, CubeMesh, FVector(TermX + TermW * 0.5f + 20.f, CavCY - 8000.f, AirportZ + TermH - 500.f),
		FVector(0.3f, 25.f, 5.f), FRotator::ZeroRotator,
		FLinearColor(0.05f, 0.04f, 0.01f), 0.50f, 0.0f, FLinearColor(3.0f, 2.0f, 0.5f));

	// 4 Jet bridges — gold-trimmed walkways to gates
	for (int32 jb = 0; jb < 4; ++jb)
	{
		const float JBY = CavCY - 15000.f + jb * 10000.f;
		AP(TA, CubeMesh, FVector(TermX + TermW * 0.5f + 2500.f, JBY, AirportZ + 1500.f),
			FVector(30.f, 5.f, 4.f), FRotator::ZeroRotator,
			BrushedSteel, 0.40f, 0.40f);
		APE(TA, CubeMesh, FVector(TermX + TermW * 0.5f + 2500.f, JBY, AirportZ + 1300.f),
			FVector(30.f, 5.5f, 0.2f), FRotator::ZeroRotator,
			Gold, 0.25f, 0.80f, GoldGlow * 0.15f);
	}

	// ================================================================
	// SECTION 5: CONTROL TOWER
	// ================================================================
	AActor* TW = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector(CavXMax - 15000.f, CavYMax - 10000.f, AirportZ), FRotator::ZeroRotator);
	if (!TW) return;
#if WITH_EDITOR
	TW->SetActorLabel(TEXT("ControlTower"));
#endif
	const float TWX = CavXMax - 15000.f, TWY = CavYMax - 10000.f;
	auto* TWR = AP(TW, CylinderMesh, FVector(TWX, TWY, AirportZ + 2000.f),
		FVector(6.f, 6.f, 40.f), FRotator::ZeroRotator,
		BrushedSteel, 0.35f, 0.50f);
	TW->SetRootComponent(TWR);
	// Cab
	AP(TW, CylinderMesh, FVector(TWX, TWY, CeilingZ - 400.f),
		FVector(14.f, 14.f, 5.f), FRotator::ZeroRotator, DarkSteel, 0.30f, 0.40f);
	APGlass(TW, FVector(TWX, TWY, CeilingZ - 400.f),
		FVector(14.5f, 14.5f, 4.5f), FRotator::ZeroRotator, TermGlass, 0.25f);
	// Gold band
	APE(TW, CylinderMesh, FVector(TWX, TWY, CeilingZ - 600.f),
		FVector(15.f, 15.f, 0.5f), FRotator::ZeroRotator, Gold, 0.25f, 0.80f, GoldGlow * 0.3f);
	APLight(TW, FVector(TWX, TWY, CeilingZ - 200.f), FColor(200, 255, 200), 20000.f, 20000.f);

	// ================================================================
	// SECTION 6: HANGARS (3 luxury hangars)
	// ================================================================
	const float HangarX = CavXMax - 12000.f;
	const float HangarYs[] = { 5000.f, 25000.f, 48000.f };
	const FLinearColor HCols[] = { BrushedSteel, DarkSteel, FLinearColor(0.18f, 0.20f, 0.28f) };

	for (int32 h = 0; h < 3; ++h)
	{
		AP(CA, CubeMesh, FVector(HangarX, HangarYs[h], AirportZ + 1500.f),
			FVector(80.f, 100.f, 30.f), FRotator::ZeroRotator, HCols[h], 0.55f, 0.40f);
		// Door
		AP(CA, CubeMesh, FVector(HangarX - 4050.f, HangarYs[h], AirportZ + 800.f),
			FVector(0.3f, 60.f, 16.f), FRotator::ZeroRotator, FLinearColor(0.02f, 0.02f, 0.03f), 0.95f, 0.0f);
		// Gold trim over door
		APE(CA, CubeMesh, FVector(HangarX - 4050.f, HangarYs[h], AirportZ + 1650.f),
			FVector(0.5f, 65.f, 1.f), FRotator::ZeroRotator, Gold, 0.25f, 0.80f, GoldGlow * 0.2f);
		// Interior lights
		APLight(CA, FVector(HangarX - 1000.f, HangarYs[h], AirportZ + 2800.f), FColor(255, 240, 210), 12000.f, 6000.f);
		APLight(CA, FVector(HangarX + 2000.f, HangarYs[h], AirportZ + 2800.f), FColor(255, 240, 210), 12000.f, 6000.f);
	}

	// ================================================================
	// SECTION 7: LUXURY AIRCRAFT (5 private jets + 2 airliners)
	// ================================================================
	struct FJetDef { FVector Pos; float Yaw; FLinearColor Body; FLinearColor Accent; float Scale; };
	const FJetDef Jets[] = {
		// Private jets on runway 1
		{ FVector(130000.f, Rwy1Y, AirportZ + 100.f),  0.f,   MarbleWhite, Gold, 1.0f },
		{ FVector(155000.f, Rwy1Y, AirportZ + 100.f),  0.f,   FLinearColor(0.05f, 0.05f, 0.08f), FLinearColor(0.80f, 0.10f, 0.10f), 1.0f },
		// Airliners on runway 2
		{ FVector(140000.f, Rwy2Y, AirportZ + 100.f),  0.f,   FLinearColor(0.90f, 0.90f, 0.92f), FLinearColor(0.10f, 0.25f, 0.65f), 1.8f },
		{ FVector(170000.f, Rwy2Y, AirportZ + 100.f), 180.f,  FLinearColor(0.90f, 0.90f, 0.92f), FLinearColor(0.65f, 0.15f, 0.15f), 1.8f },
		// In hangars
		{ FVector(HangarX, 5000.f,  AirportZ + 100.f),  90.f, FLinearColor(0.90f, 0.88f, 0.80f), DarkGold, 1.0f },
		{ FVector(HangarX, 25000.f, AirportZ + 100.f),  90.f, FLinearColor(0.02f, 0.02f, 0.04f), FLinearColor(0.60f, 0.60f, 0.65f), 1.2f },
		{ FVector(HangarX, 48000.f, AirportZ + 100.f),  90.f, FLinearColor(0.85f, 0.85f, 0.88f), Gold, 1.0f },
	};

	for (int32 j = 0; j < 7; ++j)
	{
		const FJetDef& JD = Jets[j];
		AActor* JA = World->SpawnActor<AActor>(AActor::StaticClass(), JD.Pos, FRotator(0.f, JD.Yaw, 0.f));
		if (!JA) continue;
#if WITH_EDITOR
		JA->SetActorLabel(*FString::Printf(TEXT("Jet_%d"), j));
#endif
		const FRotator JR(0.f, JD.Yaw, 0.f);
		const FVector Fwd = JR.RotateVector(FVector(1.f, 0.f, 0.f));
		const FVector Rt = JR.RotateVector(FVector(0.f, 1.f, 0.f));
		const FVector Up(0.f, 0.f, 1.f);
		const float Sc = JD.Scale;

		// Fuselage
		auto* JRoot = AP(JA, CubeMesh, JD.Pos,
			FVector(30.f * Sc, 3.5f * Sc, 3.5f * Sc), JR, JD.Body, 0.30f, 0.25f);
		JA->SetRootComponent(JRoot);

		const float HLen = 1500.f * Sc;
		// Nose
		AP(JA, ConeMesh, JD.Pos + Fwd * (HLen + 300.f * Sc),
			FVector(3.5f * Sc, 3.5f * Sc, 10.f * Sc), FRotator(-90.f, JD.Yaw, 0.f),
			JD.Body, 0.30f, 0.25f);
		// Wings
		const float WO = 250.f * Sc + 600.f * Sc;
		AP(JA, CubeMesh, JD.Pos + Fwd * (-100.f * Sc) + Rt * (-WO) + Up * (-50.f),
			FVector(10.f * Sc, 18.f * Sc, 0.5f), JR, JD.Accent, 0.35f, 0.20f);
		AP(JA, CubeMesh, JD.Pos + Fwd * (-100.f * Sc) + Rt * (WO) + Up * (-50.f),
			FVector(10.f * Sc, 18.f * Sc, 0.5f), JR, JD.Accent, 0.35f, 0.20f);
		// Vertical tail
		AP(JA, CubeMesh, JD.Pos + Fwd * (-HLen + 100.f * Sc) + Up * 250.f * Sc,
			FVector(6.f * Sc, 0.5f, 6.f * Sc), JR, JD.Accent, 0.35f, 0.20f);
		// Horizontal tail
		AP(JA, CubeMesh, JD.Pos + Fwd * (-HLen + 100.f * Sc) + Rt * (-350.f * Sc) + Up * 80.f,
			FVector(5.f * Sc, 6.f * Sc, 0.3f), JR, JD.Accent, 0.35f, 0.20f);
		AP(JA, CubeMesh, JD.Pos + Fwd * (-HLen + 100.f * Sc) + Rt * (350.f * Sc) + Up * 80.f,
			FVector(5.f * Sc, 6.f * Sc, 0.3f), JR, JD.Accent, 0.35f, 0.20f);
		// Engines
		AP(JA, CylinderMesh, JD.Pos + Fwd * (-200.f * Sc) + Rt * (-WO * 0.55f) + Up * (-120.f),
			FVector(2.f * Sc, 2.f * Sc, 5.f * Sc), FRotator(-90.f, JD.Yaw, 0.f),
			BrushedSteel, 0.35f, 0.50f);
		AP(JA, CylinderMesh, JD.Pos + Fwd * (-200.f * Sc) + Rt * (WO * 0.55f) + Up * (-120.f),
			FVector(2.f * Sc, 2.f * Sc, 5.f * Sc), FRotator(-90.f, JD.Yaw, 0.f),
			BrushedSteel, 0.35f, 0.50f);
		// Engine exhaust glow
		APE(JA, SphereMesh, JD.Pos + Fwd * (-200.f * Sc - 300.f * Sc) + Rt * (-WO * 0.55f) + Up * (-120.f),
			FVector(1.5f * Sc, 1.5f * Sc, 1.5f * Sc), FRotator::ZeroRotator,
			FLinearColor(0.90f, 0.50f, 0.15f), 0.20f, 0.0f, FLinearColor(4.0f, 2.0f, 0.4f));
		APE(JA, SphereMesh, JD.Pos + Fwd * (-200.f * Sc - 300.f * Sc) + Rt * (WO * 0.55f) + Up * (-120.f),
			FVector(1.5f * Sc, 1.5f * Sc, 1.5f * Sc), FRotator::ZeroRotator,
			FLinearColor(0.90f, 0.50f, 0.15f), 0.20f, 0.0f, FLinearColor(4.0f, 2.0f, 0.4f));
		// Cockpit glass
		APGlass(JA, JD.Pos + Fwd * (HLen * 0.6f) + Up * 120.f * Sc,
			FVector(4.f * Sc, 2.f * Sc, 2.f * Sc), JR, TermGlass, 0.25f);
		// Gold stripe on fuselage
		APE(JA, CubeMesh, JD.Pos + Up * 20.f,
			FVector(28.f * Sc, 0.2f, 0.2f), JR, Gold, 0.25f, 0.80f, GoldGlow * 0.1f);
	}

	// ================================================================
	// SECTION 8: VIP LOUNGE & DETAILS
	// ================================================================
	// Luxury seating areas along terminal side
	for (int32 seat = 0; seat < 8; ++seat)
	{
		const float SY = CavCY - 14000.f + seat * 4000.f;
		// Bench (dark leather)
		AP(CA, CubeMesh, FVector(TermX - 3000.f, SY, AirportZ + 200.f),
			FVector(8.f, 15.f, 4.f), FRotator::ZeroRotator,
			FLinearColor(0.12f, 0.08f, 0.05f), 0.50f, 0.0f);
		// Gold armrest
		AP(CA, CubeMesh, FVector(TermX - 3000.f, SY - 700.f, AirportZ + 350.f),
			FVector(8.f, 0.5f, 2.f), FRotator::ZeroRotator, Gold, 0.25f, 0.80f);
	}

	// Luxury carpet runners (red velvet along terminal approach)
	AP(CA, CubeMesh, FVector(TermX - 5000.f, CavCY, AirportZ + 5.f),
		FVector(40.f, 30.f, 0.01f), FRotator::ZeroRotator,
		FLinearColor(0.45f, 0.05f, 0.05f), 0.85f, 0.0f);

	// Potted plants (gold pots with green sphere topiary)
	for (int32 pl = 0; pl < 6; ++pl)
	{
		const float PY = CavCY - 10000.f + pl * 4000.f;
		AP(CA, CylinderMesh, FVector(TermX + TermW * 0.5f + 500.f, PY, AirportZ + 200.f),
			FVector(2.f, 2.f, 4.f), FRotator::ZeroRotator, DarkGold, 0.30f, 0.70f);
		AP(CA, SphereMesh, FVector(TermX + TermW * 0.5f + 500.f, PY, AirportZ + 600.f),
			FVector(3.f, 3.f, 3.f), FRotator::ZeroRotator,
			FLinearColor(0.15f, 0.30f, 0.10f), 0.80f, 0.0f);
	}

	// Large luxury fountain at terminal entrance
	AP(CA, CylinderMesh, FVector(TermX - 4000.f, CavCY, AirportZ + 100.f),
		FVector(10.f, 10.f, 2.f), FRotator::ZeroRotator, MarbleCream, 0.20f, 0.05f);
	AP(CA, CylinderMesh, FVector(TermX - 4000.f, CavCY, AirportZ + 400.f),
		FVector(4.f, 4.f, 8.f), FRotator::ZeroRotator, MarbleWhite, 0.20f, 0.05f);
	APE(CA, SphereMesh, FVector(TermX - 4000.f, CavCY, AirportZ + 1000.f),
		FVector(3.f, 3.f, 3.f), FRotator::ZeroRotator,
		FLinearColor(0.20f, 0.40f, 0.70f), 0.10f, 0.0f, BlueGlow);
	APLight(CA, FVector(TermX - 4000.f, CavCY, AirportZ + 600.f), FColor(100, 180, 255), 8000.f, 5000.f);

	// Cargo/fuel area near hangars
	FRandomStream Rng(42);
	for (int32 c = 0; c < 25; ++c)
	{
		const float CX = HangarX - 6000.f + Rng.FRandRange(-3000.f, 3000.f);
		const float CY2 = 2000.f + Rng.FRandRange(0.f, 55000.f);
		const float CS = Rng.FRandRange(2.f, 6.f);
		AP(CA, CubeMesh, FVector(CX, CY2, AirportZ + CS * 50.f),
			FVector(CS, CS * 0.7f, CS), FRotator(0.f, Rng.FRandRange(0.f, 360.f), 0.f),
			FLinearColor(0.25f + Rng.FRandRange(-0.05f, 0.05f), 0.22f, 0.18f), 0.85f, 0.0f);
	}

	// Fuel tanks
	for (int32 ft = 0; ft < 4; ++ft)
	{
		AP(CA, CylinderMesh, FVector(HangarX + 5000.f, 8000.f + ft * 4000.f, AirportZ + 300.f),
			FVector(4.f, 4.f, 12.f), FRotator(90.f, 0.f, 0.f),
			BrushedSteel, 0.25f, 0.70f);
	}

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCCitySubsystem: Underground luxury airport spawned (%d components)."), Idx);
}


// ---------------------------------------------------------------------------
// Giant Crab NPC — spawns hidden, revealed at night, roams the city
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnGiantCrab()
{
	UWorld* W = GetWorld();
	if (!W || !SphereMesh || !CylinderMesh || !CubeMesh || !ConeMesh || !ColorMaterial) return;

	GiantCrabActor = W->SpawnActor<AActor>(AActor::StaticClass(),
		FVector(0.f, 0.f, 1200.f), FRotator::ZeroRotator);
	if (!GiantCrabActor) return;

	CrabRoamTarget = FVector(0.f, 0.f, 1200.f);

	// --- Root ---
	USceneComponent* Root = NewObject<USceneComponent>(GiantCrabActor, TEXT("CrabRoot"));
	Root->RegisterComponent();
	GiantCrabActor->SetRootComponent(Root);
	GiantCrabActor->AddInstanceComponent(Root);

	// --- Mesh helper ---
	auto CM = [&](UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Loc,
		const FRotator& Rot, const FVector& S, const FLinearColor& Col,
		float Metal, float Rough, const FName& Name,
		const FLinearColor& Emis = FLinearColor(0.0f, 3.0f, 0.2f)) -> UStaticMeshComponent*
	{
		auto* C = NewObject<UStaticMeshComponent>(GiantCrabActor, Name);
		C->SetStaticMesh(Mesh);
		C->SetRelativeLocation(Loc);
		C->SetRelativeRotation(Rot);
		C->SetRelativeScale3D(S);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		auto* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, C);
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Col);
		Mat->SetScalarParameterValue(TEXT("Metallic"), Metal);
		Mat->SetScalarParameterValue(TEXT("Roughness"), Rough);
		if (Emis.R > 0.001f || Emis.G > 0.001f || Emis.B > 0.001f)
			Mat->SetVectorParameterValue(TEXT("EmissiveColor"), Emis);
		C->SetMaterial(0, Mat);
		C->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		C->RegisterComponent();
		GiantCrabActor->AddInstanceComponent(C);
		return C;
	};

	auto MakeJoint = [&](USceneComponent* Parent, const FVector& Loc,
		const FRotator& Rot, const FName& Name) -> USceneComponent*
	{
		auto* SC = NewObject<USceneComponent>(GiantCrabActor, Name);
		SC->SetRelativeLocation(Loc);
		SC->SetRelativeRotation(Rot);
		SC->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
		SC->RegisterComponent();
		GiantCrabActor->AddInstanceComponent(SC);
		return SC;
	};

	// --- Colors ---
	const FLinearColor Shell(0.55f, 0.15f, 0.08f);
	const FLinearColor ShellDark(0.40f, 0.10f, 0.05f);
	const FLinearColor Leg(0.45f, 0.12f, 0.06f);
	const FLinearColor Claw(0.50f, 0.10f, 0.05f);
	const FLinearColor Belly(0.60f, 0.35f, 0.20f);
	const FLinearColor Glow(0.0f, 3.0f, 0.2f); // neon green emissive

	// ========== BODY ==========
	// Carapace — wide flat ellipsoid
	CM(SphereMesh, Root, FVector(0, 0, 250), FRotator::ZeroRotator,
		FVector(15, 10, 5), Shell, 0.15f, 0.7f, TEXT("Carapace"), Glow);

	// Head bump
	CM(SphereMesh, Root, FVector(0, 450, 300), FRotator::ZeroRotator,
		FVector(10, 7, 3.5f), Shell * 0.9f, 0.15f, 0.65f, TEXT("HeadBump"));

	// Belly (lighter)
	CM(SphereMesh, Root, FVector(0, 0, 130), FRotator::ZeroRotator,
		FVector(12, 8, 2.5f), Belly, 0.05f, 0.8f, TEXT("Belly"));

	// Shell ridges (3 bumps along back)
	for (int R = 0; R < 3; R++)
	{
		float Y = -200.f + R * 200.f;
		CM(SphereMesh, Root, FVector(0, Y, 420), FRotator::ZeroRotator,
			FVector(6, 4, 2), ShellDark, 0.2f, 0.6f,
			FName(*FString::Printf(TEXT("Ridge%d"), R)));
	}

	// Rear shell edge
	CM(SphereMesh, Root, FVector(0, -350, 200), FRotator::ZeroRotator,
		FVector(12, 5, 3), ShellDark, 0.15f, 0.7f, TEXT("RearEdge"));

	// ========== EYES ==========
	for (int E = 0; E < 2; E++)
	{
		float SX = (E == 0) ? -200.f : 200.f;
		float Tilt = (E == 0) ? 15.f : -15.f;
		FString Side = (E == 0) ? TEXT("L") : TEXT("R");

		// Stalk
		CM(CylinderMesh, Root,
			FVector(SX, 500, 430), FRotator(20, 0, Tilt),
			FVector(0.6f, 0.6f, 2.5f), Leg, 0.1f, 0.75f,
			FName(*(TEXT("EyeStalk_") + Side)));

		// Eyeball with red emissive
		CM(SphereMesh, Root,
			FVector(SX * 1.3f, 550, 620), FRotator::ZeroRotator,
			FVector(1.5f, 1.5f, 1.5f),
			FLinearColor(0.02f, 0.01f, 0.01f), 0.8f, 0.2f,
			FName(*(TEXT("Eye_") + Side)),
			FLinearColor(0.0f, 4.0f, 0.3f));
	}

	// ========== MOUTH PARTS ==========
	// Mandibles — two small angled pieces under the head
	for (int M = 0; M < 2; M++)
	{
		float SX = (M == 0) ? -80.f : 80.f;
		CM(CubeMesh, Root,
			FVector(SX, 600, 150), FRotator(25, (M == 0) ? -10.f : 10.f, 0),
			FVector(0.8f, 2.5f, 0.5f), ShellDark, 0.1f, 0.7f,
			FName(*FString::Printf(TEXT("Mandible%d"), M)));
	}

	// ========== CLAWS ==========
	for (int C = 0; C < 2; C++)
	{
		float SS = (C == 0) ? -1.f : 1.f; // side sign
		FString Side = (C == 0) ? TEXT("L") : TEXT("R");

		// Arm — cylinder extending forward and outward
		CM(CylinderMesh, Root,
			FVector(SS * 550, 650, 200),
			FRotator(30, SS * 25, SS * -40),
			FVector(2.0f, 2.0f, 4.5f), Claw, 0.15f, 0.65f,
			FName(*(TEXT("ClawArm_") + Side)));

		// Hand base
		FVector HP(SS * 900, 1100, 130);
		CM(CubeMesh, Root, HP,
			FRotator(0, SS * 30, 0),
			FVector(3.5f, 5.f, 2.f), Claw * 0.85f, 0.2f, 0.6f,
			FName(*(TEXT("ClawHand_") + Side)));

		// Lower pincer (fixed)
		CM(CubeMesh, Root,
			HP + FVector(SS * 40, 300, -40),
			FRotator(-5, SS * 25, 0),
			FVector(2.2f, 4.5f, 1.0f), Claw * 0.8f, 0.2f, 0.55f,
			FName(*(TEXT("ClawLower_") + Side)));

		// Upper pincer
		CM(CubeMesh, Root,
			HP + FVector(SS * 40, 280, 90),
			FRotator(8, SS * 25, 0),
			FVector(2.0f, 4.0f, 0.8f), Claw * 0.8f, 0.2f, 0.55f,
			FName(*(TEXT("ClawUpper_") + Side)));

		// Pincer tip (pointed)
		CM(ConeMesh, Root,
			HP + FVector(SS * 40, 600, 20),
			FRotator(90, SS * 25, 0),
			FVector(1.5f, 1.0f, 2.5f), Claw * 0.75f, 0.2f, 0.5f,
			FName(*(TEXT("ClawTip_") + Side)));
	}

	// ========== WALKING LEGS (8 total) ==========
	struct FLegDef { float HipX, HipY, HipZ, FwdAngle; };
	const FLegDef Legs[4] = {
		{ 600, 300, 200, 15 },   // front
		{ 680, 100, 180, 5 },    // mid-front
		{ 680, -100, 180, -5 },  // mid-back
		{ 600, -300, 200, -15 }, // back
	};

	const float UpperLen = 400.f;
	const float LowerLen = 350.f;

	for (int Side = 0; Side < 2; Side++)
	{
		float SS = (Side == 0) ? 1.f : -1.f;
		FString SN = (Side == 0) ? TEXT("L") : TEXT("R");

		for (int L = 0; L < 4; L++)
		{
			const FLegDef& D = Legs[L];
			FVector HipPos(D.HipX * SS, D.HipY, D.HipZ);

			// Hip base rotation: Pitch tilts outward+down, Roll fans fwd/back
			FRotator HipBase(SS * 120.f, 0.f, -D.FwdAngle);

			USceneComponent* Hip = MakeJoint(Root, HipPos, HipBase,
				FName(*FString::Printf(TEXT("Hip_%s%d"), *SN, L)));
			CrabHipJoints.Add(Hip);
			CrabHipBaseRot.Add(HipBase);

			// Upper leg mesh — pivot at hip, extends along joint Z
			CM(CylinderMesh, Hip,
				FVector(0, 0, UpperLen * 0.5f), FRotator::ZeroRotator,
				FVector(1.0f, 1.0f, UpperLen / 50.f),
				Leg, 0.1f, 0.7f,
				FName(*FString::Printf(TEXT("UpperLeg_%s%d"), *SN, L)));

			// Knee joint at end of upper leg
			FRotator KneeBase(25, 0, 0);
			USceneComponent* Knee = MakeJoint(Hip, FVector(0, 0, UpperLen), KneeBase,
				FName(*FString::Printf(TEXT("Knee_%s%d"), *SN, L)));
			CrabKneeJoints.Add(Knee);
			CrabKneeBaseRot.Add(KneeBase);

			// Lower leg mesh
			CM(CylinderMesh, Knee,
				FVector(0, 0, LowerLen * 0.5f), FRotator::ZeroRotator,
				FVector(0.7f, 0.7f, LowerLen / 50.f),
				Leg * 0.9f, 0.1f, 0.7f,
				FName(*FString::Printf(TEXT("LowerLeg_%s%d"), *SN, L)));

			// Foot tip — small pointed sphere
			CM(SphereMesh, Knee,
				FVector(0, 0, LowerLen), FRotator::ZeroRotator,
				FVector(1.2f, 1.2f, 0.7f),
				Leg * 0.8f, 0.1f, 0.75f,
				FName(*FString::Printf(TEXT("Foot_%s%d"), *SN, L)));
		}
	}

	// ========== GLOW LIGHTS ==========
	auto AddGlow = [&](const FVector& Loc, float Inten, float Rad, const FColor& Col, const FName& Name)
	{
		auto* PL = NewObject<UPointLightComponent>(GiantCrabActor, Name);
		PL->SetRelativeLocation(Loc);
		PL->SetIntensity(Inten);
		PL->SetAttenuationRadius(Rad);
		PL->SetLightColor(Col);
		PL->SetCastShadows(false);
		PL->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		PL->RegisterComponent();
		GiantCrabActor->AddInstanceComponent(PL);
	};

	AddGlow(FVector(0, 0, 50), 8000.f, 5000.f, FColor(0, 255, 40), TEXT("CrabGlow1"));
	AddGlow(FVector(0, 400, 350), 5000.f, 4000.f, FColor(0, 255, 30), TEXT("CrabGlow2"));
	AddGlow(FVector(0, -300, 350), 4000.f, 3500.f, FColor(0, 255, 20), TEXT("CrabGlow3"));

	// Start hidden — revealed at night
	GiantCrabActor->SetActorHiddenInGame(true);
}


// ---------------------------------------------------------------------------
// Starfield -- Milky Way night sky with scattered stars
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnStarfield()
{
	UWorld* World = GetWorld();
	if (!World || !SphereMesh) return;

	FRandomStream StarRandom(999);

	StarfieldActor = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator);
	if (!StarfieldActor) return;
#if WITH_EDITOR
	StarfieldActor->SetActorLabel(TEXT("Starfield"));
#endif
	StarfieldActor->SetActorHiddenInGame(true); // starts hidden, Tick toggles

	const float SkyRadius = 160000.f;

	// Star color palette (emissive -- BaseColor > 1.0 causes bloom)
	struct FStarColor { FLinearColor Color; float Weight; };
	const FStarColor Palette[] = {
		{ FLinearColor(2.5f, 2.5f, 2.5f),   0.40f }, // white
		{ FLinearColor(2.0f, 2.2f, 3.0f),   0.25f }, // blue-white
		{ FLinearColor(3.0f, 2.7f, 1.8f),   0.20f }, // yellow
		{ FLinearColor(3.0f, 2.0f, 1.2f),   0.10f }, // orange
		{ FLinearColor(2.5f, 1.2f, 1.0f),   0.05f }, // red
	};

	auto PickStarColor = [&](FRandomStream& Rng) -> FLinearColor
	{
		float Roll = Rng.FRand();
		float Accum = 0.f;
		for (const auto& P : Palette)
		{
			Accum += P.Weight;
			if (Roll <= Accum) return P.Color;
		}
		return Palette[0].Color;
	};

	int32 StarIdx = 0;

	// --- 180 background stars scattered across full hemisphere ---
	for (int32 i = 0; i < 180; ++i)
	{
		const float Azimuth = StarRandom.FRandRange(0.f, 360.f);
		const float Elevation = StarRandom.FRandRange(5.f, 85.f);
		const float AzRad = FMath::DegreesToRadians(Azimuth);
		const float ElRad = FMath::DegreesToRadians(Elevation);

		const FVector Pos(
			SkyRadius * FMath::Cos(ElRad) * FMath::Cos(AzRad),
			SkyRadius * FMath::Cos(ElRad) * FMath::Sin(AzRad),
			SkyRadius * FMath::Sin(ElRad)
		);

		const float Scale = StarRandom.FRandRange(0.15f, 0.40f);
		const FLinearColor Col = PickStarColor(StarRandom) * StarRandom.FRandRange(0.6f, 1.0f);

		FString Name = FString::Printf(TEXT("Star_%d"), StarIdx++);
		UStaticMeshComponent* SC = NewObject<UStaticMeshComponent>(StarfieldActor, *Name);
		SC->SetStaticMesh(SphereMesh);
		SC->SetWorldScale3D(FVector(Scale));
		SC->SetWorldLocation(Pos);
		SC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SC->RegisterComponent();
		UMaterialInstanceDynamic* SM = UMaterialInstanceDynamic::Create(SC->GetMaterial(0), SC);
		if (SM)
		{
			SM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.1f, 0.1f, 0.1f));
			SM->SetScalarParameterValue(TEXT("Roughness"), 0.f);
			SM->SetVectorParameterValue(TEXT("EmissiveColor"), Col);
			SC->SetMaterial(0, SM);
		}
		StarfieldActor->AddInstanceComponent(SC);
	}

	// --- 250 Milky Way band stars (diagonal arc across sky) ---
	for (int32 i = 0; i < 250; ++i)
	{
		// Milky Way arc: azimuth centered at 120 degrees, spread ±75
		const float Azimuth = 120.f + StarRandom.FRandRange(-75.f, 75.f);
		// Elevation follows a parabolic arc: peaks at center, low at edges
		const float AzNorm = (Azimuth - 120.f) / 75.f; // -1 to 1
		const float BaseElev = 15.f + (85.f - 15.f) * (1.f - AzNorm * AzNorm);
		const float Elevation = FMath::Clamp(BaseElev + StarRandom.FRandRange(-15.f, 15.f), 5.f, 88.f);

		const float AzRad = FMath::DegreesToRadians(Azimuth);
		const float ElRad = FMath::DegreesToRadians(Elevation);

		const FVector Pos(
			SkyRadius * FMath::Cos(ElRad) * FMath::Cos(AzRad),
			SkyRadius * FMath::Cos(ElRad) * FMath::Sin(AzRad),
			SkyRadius * FMath::Sin(ElRad)
		);

		const float Scale = StarRandom.FRandRange(0.10f, 0.30f);
		const float Dimming = StarRandom.FRandRange(0.3f, 0.7f);
		const FLinearColor Col = PickStarColor(StarRandom) * Dimming;

		FString Name = FString::Printf(TEXT("Star_%d"), StarIdx++);
		UStaticMeshComponent* SC = NewObject<UStaticMeshComponent>(StarfieldActor, *Name);
		SC->SetStaticMesh(SphereMesh);
		SC->SetWorldScale3D(FVector(Scale));
		SC->SetWorldLocation(Pos);
		SC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SC->RegisterComponent();
		UMaterialInstanceDynamic* SM = UMaterialInstanceDynamic::Create(SC->GetMaterial(0), SC);
		if (SM)
		{
			SM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.1f, 0.1f, 0.1f));
			SM->SetScalarParameterValue(TEXT("Roughness"), 0.f);
			SM->SetVectorParameterValue(TEXT("EmissiveColor"), Col);
			SC->SetMaterial(0, SM);
		}
		StarfieldActor->AddInstanceComponent(SC);
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Spawned %d stars (180 bg + 250 Milky Way)."), StarIdx);
}

// ---------------------------------------------------------------------------
// Cloud Layer -- two overlapping planes at high altitude for parallax depth
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnCloudLayer()
{
	UWorld* World = GetWorld();
	if (!World || !PlaneMesh || !GlassMaterial) return;

	auto SpawnCloudPlane = [&](float Z, float Opacity, const TCHAR* Label) -> AActor*
	{
		AActor* CA = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(0.f, 0.f, Z), FRotator::ZeroRotator);
		if (!CA) return nullptr;
#if WITH_EDITOR
		CA->SetActorLabel(Label);
#endif
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(CA, TEXT("CloudMesh"));
		MC->SetStaticMesh(PlaneMesh);
		MC->SetWorldScale3D(FVector(5000.f, 5000.f, 1.f)); // massive sky plane (doubled map)
		MC->SetWorldLocation(FVector(0.f, 0.f, Z));
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->RegisterComponent();
		CA->SetRootComponent(MC);

		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(GlassMaterial, MC);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.85f, 0.85f, 0.88f));
			Mat->SetScalarParameterValue(TEXT("Opacity"), Opacity);
			Mat->SetScalarParameterValue(TEXT("Roughness"), 1.0f);
			MC->SetMaterial(0, Mat);
		}
		CA->AddInstanceComponent(MC);
		return CA;
	};

	CloudLayerActor = SpawnCloudPlane(15000.f, 0.10f, TEXT("CloudLayer1"));
	CloudLayerActor2 = SpawnCloudPlane(16500.f, 0.08f, TEXT("CloudLayer2"));
}

// ---------------------------------------------------------------------------
// Skyscrapers -- 20 detailed, architecturally distinct towers in downtown
// Block range: (7-10, 10-15) in the 24x24 grid
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnSkyscrapers()
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh || !CylinderMesh || !ColorMaterial) return;
	// Helper: block center from grid indices (uses file-scope FirstRoad/RoadSpacing).
	auto BlockCenter = [](int32 bx, int32 by) -> FVector
	{
		return FVector(
			FirstRoad + (bx + 0.5f) * RoadSpacing,
			FirstRoad + (by + 0.5f) * RoadSpacing,
			0.f
		);
	};

	// Running indices for unique component naming.
	int32 WinIdx = 0;
	int32 BandIdx = 0;

	// ---------------------------------------------------------------
	// Helper lambdas
	// ---------------------------------------------------------------

	// Note: Size parameter is in UU; BasicShapes are 100 UU, so divide by 100 for scale.
	auto MakeCubeMC = [&](AActor* Owner, const FVector& Loc, const FVector& Size,
		const FLinearColor& Color, const TCHAR* Name,
		float Metallic = 0.f, float Roughness = 0.5f,
		const FLinearColor& Emissive = FLinearColor::Black) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, Name);
		MC->SetStaticMesh(CubeMesh);
		MC->SetWorldScale3D(Size / 100.f);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->RegisterComponent();
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, MC);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			if (Metallic != 0.f)
				Mat->SetScalarParameterValue(TEXT("Metallic"), Metallic);
			if (Roughness != 0.5f)
				Mat->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			if (Emissive != FLinearColor::Black)
				Mat->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
			MC->SetMaterial(0, Mat);
		}
		Owner->AddInstanceComponent(MC);
		return MC;
	};

	auto MakeCylMC = [&](AActor* Owner, const FVector& Loc, const FVector& Size,
		const FLinearColor& Color, const TCHAR* Name,
		float Metallic = 0.f, float Roughness = 0.5f,
		const FLinearColor& Emissive = FLinearColor::Black) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, Name);
		MC->SetStaticMesh(CylinderMesh);
		MC->SetWorldScale3D(Size / 100.f);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->RegisterComponent();
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, MC);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			if (Metallic != 0.f)
				Mat->SetScalarParameterValue(TEXT("Metallic"), Metallic);
			if (Roughness != 0.5f)
				Mat->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			if (Emissive != FLinearColor::Black)
				Mat->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
			MC->SetMaterial(0, Mat);
		}
		Owner->AddInstanceComponent(MC);
		return MC;
	};

	auto MakeSphereMC = [&](AActor* Owner, const FVector& Loc, const FVector& Size,
		const FLinearColor& Color, const TCHAR* Name,
		float Metallic = 0.f, float Roughness = 0.5f,
		const FLinearColor& Emissive = FLinearColor::Black) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, Name);
		MC->SetStaticMesh(SphereMesh);
		MC->SetWorldScale3D(Size / 100.f);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->RegisterComponent();
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, MC);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			if (Metallic != 0.f)
				Mat->SetScalarParameterValue(TEXT("Metallic"), Metallic);
			if (Roughness != 0.5f)
				Mat->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			if (Emissive != FLinearColor::Black)
				Mat->SetVectorParameterValue(TEXT("EmissiveColor"), Emissive);
			MC->SetMaterial(0, Mat);
		}
		Owner->AddInstanceComponent(MC);
		return MC;
	};

	// Helper: add lit window rows to a tower (two wider faces).
	auto AddWindows = [&](AActor* Owner, const FVector& BaseLoc,
		float TowerW, float TowerD, float TowerH, int32& WIdx)
	{
		const float FloorH = 320.f;
		const float WinW = 65.f;
		const float WinH = 80.f;
		const float WinSpacing = 210.f;
		const float StartZ = 350.f;
		const FLinearColor DarkWinGlass(0.06f, 0.07f, 0.10f);
		const FLinearColor WarmColors[] = {
			FLinearColor(0.95f, 0.85f, 0.40f),
			FLinearColor(0.90f, 0.65f, 0.30f),
			FLinearColor(0.85f, 0.82f, 0.72f),
			FLinearColor(0.45f, 0.55f, 0.82f),
			FLinearColor(0.95f, 0.55f, 0.20f),
			FLinearColor(0.80f, 0.75f, 0.55f),
		};

		const int32 NumRows = FMath::Clamp(FMath::FloorToInt((TowerH - StartZ) / FloorH), 1, 14);

		// Two wider faces.
		struct FWF { float OffX, OffY; bool bAlongX; float FW; };
		FWF Faces[2];
		if (TowerW >= TowerD)
		{
			Faces[0] = { 0.f,  TowerD * 0.5f + 5.f, true, TowerW };
			Faces[1] = { 0.f, -TowerD * 0.5f - 5.f, true, TowerW };
		}
		else
		{
			Faces[0] = {  TowerW * 0.5f + 5.f, 0.f, false, TowerD };
			Faces[1] = { -TowerW * 0.5f - 5.f, 0.f, false, TowerD };
		}

		for (int32 Fi = 0; Fi < 2; ++Fi)
		{
			const FWF& F = Faces[Fi];
			const int32 NumCols = FMath::Clamp(FMath::FloorToInt(F.FW / WinSpacing), 1, 8);
			const float ColStart = -(NumCols - 1) * WinSpacing * 0.5f;

			for (int32 Row = 0; Row < NumRows; ++Row)
			{
				for (int32 Col = 0; Col < NumCols; ++Col)
				{
					if (CityRandom.FRand() > 0.35f) continue;

					const float WZ = BaseLoc.Z + StartZ + Row * FloorH;
					const float WOff = ColStart + Col * WinSpacing;

					FVector WLoc;
					FVector WScale;
					if (F.bAlongX)
					{
						WLoc = FVector(BaseLoc.X + WOff, BaseLoc.Y + F.OffY, WZ);
						WScale = FVector(WinW, 3.f, WinH);
					}
					else
					{
						WLoc = FVector(BaseLoc.X + F.OffX, BaseLoc.Y + WOff, WZ);
						WScale = FVector(3.f, WinW, WinH);
					}

					FString WN = FString::Printf(TEXT("Win_%d"), WIdx++);
					UStaticMeshComponent* WMC = MakeCubeMC(Owner, WLoc, WScale, DarkWinGlass, *WN);
					UMaterialInstanceDynamic* WMat = Cast<UMaterialInstanceDynamic>(WMC->GetMaterial(0));
					if (WMat)
					{
						WMat->SetScalarParameterValue(TEXT("Metallic"), 0.1f);
						WMat->SetScalarParameterValue(TEXT("Roughness"), 0.05f);
						NightWindowMaterials.Add(WMat);
						NightWindowLitColors.Add(WarmColors[CityRandom.RandRange(0, 5)]);
						NightWindowLitStates.Add(CityRandom.FRand() < 0.5f);
					}
				}
			}
		}
	};

	// Helper: add glass curtain wall bands (horizontal strips across all 4 faces).
	auto AddGlassBands = [&](AActor* Owner, const FVector& BaseLoc,
		float W, float D, float H, const FLinearColor& GlassColor, int32& BIdx)
	{
		const int32 NumBands = FMath::Clamp(FMath::FloorToInt(H / 1000.f), 2, 8);
		const float BandH = FMath::Clamp(H * 0.04f, 25.f, 60.f);
		const float BandD = 5.f;

		for (int32 b = 0; b < NumBands; ++b)
		{
			const float BZ = BaseLoc.Z + H * (0.15f + 0.7f * b / FMath::Max(1, NumBands - 1));
			FString NY = FString::Printf(TEXT("Band_%d"), BIdx++);
			FString NnY = FString::Printf(TEXT("Band_%d"), BIdx++);
			FString NX = FString::Printf(TEXT("Band_%d"), BIdx++);
			FString NnX = FString::Printf(TEXT("Band_%d"), BIdx++);
			// +Y face
			MakeCubeMC(Owner, FVector(BaseLoc.X, BaseLoc.Y + D * 0.5f + 3.f, BZ),
				FVector(W, BandD, BandH), GlassColor, *NY, 0.1f, 0.05f);
			// -Y face
			MakeCubeMC(Owner, FVector(BaseLoc.X, BaseLoc.Y - D * 0.5f - 3.f, BZ),
				FVector(W, BandD, BandH), GlassColor, *NnY, 0.1f, 0.05f);
			// +X face
			MakeCubeMC(Owner, FVector(BaseLoc.X + W * 0.5f + 3.f, BaseLoc.Y, BZ),
				FVector(BandD, D, BandH), GlassColor, *NX, 0.1f, 0.05f);
			// -X face
			MakeCubeMC(Owner, FVector(BaseLoc.X - W * 0.5f - 3.f, BaseLoc.Y, BZ),
				FVector(BandD, D, BandH), GlassColor, *NnX, 0.1f, 0.05f);
		}
	};

	// Helper: add vertical corner columns.
	auto AddCornerColumns = [&](AActor* Owner, const FVector& BaseLoc,
		float W, float D, float H, float ColW, const FLinearColor& Color)
	{
		const float HalfW = W * 0.5f;
		const float HalfD = D * 0.5f;
		const FVector Corners[] = {
			{ BaseLoc.X + HalfW, BaseLoc.Y + HalfD, BaseLoc.Z + H * 0.5f },
			{ BaseLoc.X - HalfW, BaseLoc.Y + HalfD, BaseLoc.Z + H * 0.5f },
			{ BaseLoc.X + HalfW, BaseLoc.Y - HalfD, BaseLoc.Z + H * 0.5f },
			{ BaseLoc.X - HalfW, BaseLoc.Y - HalfD, BaseLoc.Z + H * 0.5f },
		};
		for (int32 ci = 0; ci < 4; ++ci)
		{
			FString CN = FString::Printf(TEXT("Col_%d_%d"), BandIdx++, ci);
			MakeCubeMC(Owner, Corners[ci], FVector(ColW, ColW, H), Color, *CN, 0.85f, 0.3f);
		}
	};

	// Helper: spawn a skyscraper actor.
	auto MakeTower = [&](const FVector& Loc, const TCHAR* Label) -> AActor*
	{
		AActor* T = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
#if WITH_EDITOR
		if (T) T->SetActorLabel(Label);
#endif
		return T;
	};

	int32 SkyCount = 0;

	// ===================================================================
	// Colors
	// ===================================================================
	const FLinearColor SteelGray(0.42f, 0.44f, 0.46f);
	const FLinearColor DarkSteel(0.20f, 0.22f, 0.24f);
	const FLinearColor LightConcrete(0.72f, 0.70f, 0.68f);
	const FLinearColor WarmConcrete(0.65f, 0.58f, 0.50f);
	const FLinearColor CoolWhite(0.82f, 0.84f, 0.88f);
	const FLinearColor MidnightBlue(0.08f, 0.10f, 0.18f);
	const FLinearColor TintedGlass(0.10f, 0.14f, 0.22f);
	const FLinearColor DarkGlass(0.05f, 0.08f, 0.14f);
	const FLinearColor SkyGlass(0.12f, 0.18f, 0.28f);
	const FLinearColor GoldAccent(0.75f, 0.55f, 0.15f);
	const FLinearColor CopperAccent(0.62f, 0.35f, 0.12f);
	const FLinearColor RedAccent(0.55f, 0.08f, 0.08f);
	const FLinearColor AntennaRed(0.85f, 0.10f, 0.10f);
	const FLinearColor LightGray(0.58f, 0.56f, 0.54f);
	const FLinearColor SandStone(0.72f, 0.65f, 0.52f);
	const FLinearColor CharcoalGray(0.15f, 0.15f, 0.17f);
	const FLinearColor WhiteMarble(0.90f, 0.88f, 0.85f);
	const FLinearColor CreamColor(0.88f, 0.82f, 0.70f);
	const FLinearColor NavyBlue(0.06f, 0.08f, 0.20f);
	const FLinearColor Terracotta(0.68f, 0.38f, 0.18f);
	const FLinearColor EmeraldGreen(0.05f, 0.32f, 0.15f);
	const FLinearColor CrystalBlue(0.18f, 0.28f, 0.42f);
	const FLinearColor GrayGreen(0.35f, 0.40f, 0.35f);

	// ===================================================================
	// 1. THE PINNACLE -- Art Deco stepped tower with antenna spire
	//    Block (8,11), downtown core
	// ===================================================================
	{
		const FVector Loc = BlockCenter(8, 11) + FVector(500.f, 500.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Pinnacle"));
		if (T)
		{
			// Wide podium base (3 stories) -- sandstone concrete
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 500.f),
				FVector(900.f, 800.f, 1000.f), SandStone, TEXT("Pinnacle_Pod"), 0.f, 0.6f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Main tower -- concrete
			const float MainH = 5000.f;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1000.f + MainH * 0.5f),
				FVector(550.f, 500.f, MainH), WarmConcrete, TEXT("Pinnacle_Main"), 0.f, 0.6f);

			// First setback -- concrete
			const float Set1H = 1800.f;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1000.f + MainH + Set1H * 0.5f),
				FVector(400.f, 380.f, Set1H), WarmConcrete * 0.95f, TEXT("Pinnacle_Set1"), 0.f, 0.6f);

			// Second setback (crown) -- sandstone concrete
			const float Set2H = 900.f;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1000.f + MainH + Set1H + Set2H * 0.5f),
				FVector(250.f, 240.f, Set2H), SandStone, TEXT("Pinnacle_Set2"), 0.f, 0.6f);

			// Decorative crown ledges -- gold trim
			const float CrownZ = 1000.f + MainH + Set1H + Set2H;
			for (int32 i = 0; i < 4; ++i)
			{
				const float LZ = CrownZ - i * 100.f;
				const float LW = 260.f - i * 15.f;
				FString LN = FString::Printf(TEXT("Pinnacle_Ledge_%d"), i);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y, LZ),
					FVector(LW + 30.f, LW + 20.f, 25.f), GoldAccent, *LN, 0.95f, 0.2f);
			}

			// Antenna spire -- steel
			MakeCylMC(T, FVector(Loc.X, Loc.Y, CrownZ + 600.f),
				FVector(15.f, 15.f, 1200.f), LightGray, TEXT("Pinnacle_Spire"), 0.85f, 0.3f);
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, CrownZ + 1200.f),
				FVector(25.f, 25.f, 25.f), AntennaRed, TEXT("Pinnacle_Light"), 0.f, 0.5f,
				FLinearColor(2.0f, 0.2f, 0.2f));

			AddCornerColumns(T, FVector(Loc.X, Loc.Y, 1000.f), 560.f, 510.f, MainH, 35.f, SandStone * 0.8f);
			AddGlassBands(T, FVector(Loc.X, Loc.Y, 1000.f), 550.f, 500.f, MainH, TintedGlass, BandIdx);
			AddWindows(T, FVector(Loc.X, Loc.Y, 1000.f), 550.f, 500.f, MainH + Set1H, WinIdx);

			// Ground floor canopy -- gold trim
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 410.f, 280.f),
				FVector(400.f, 120.f, 15.f), GoldAccent, TEXT("Pinnacle_Canopy"), 0.95f, 0.2f);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 2. GLASS TOWER -- Sleek modern blue-glass curtain wall
	//    Block (10,11)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(10, 11) + FVector(-400.f, 400.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_GlassTower"));
		if (T)
		{
			const float H = 5500.f;
			// Glass tower facade
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(500.f, 450.f, H), MidnightBlue, TEXT("Glass_Main"), 0.1f, 0.02f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			AddGlassBands(T, Loc, 500.f, 450.f, H, SkyGlass, BandIdx);

			// Mechanical penthouse -- steel
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 200.f),
				FVector(300.f, 250.f, 400.f), DarkSteel, TEXT("Glass_Pent"), 0.85f, 0.3f);

			// Rooftop antenna array -- steel
			for (int32 i = -1; i <= 1; ++i)
			{
				FString AN = FString::Printf(TEXT("Glass_Ant_%d"), i + 1);
				MakeCylMC(T, FVector(Loc.X + i * 80.f, Loc.Y, H + 400.f + 300.f),
					FVector(8.f, 8.f, 600.f), LightGray, *AN, 0.85f, 0.3f);
			}

			// Ground floor glass lobby
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 230.f, 200.f),
				FVector(480.f, 20.f, 380.f), SkyGlass * 1.5f, TEXT("Glass_Lobby"), 0.1f, 0.02f);

			AddWindows(T, Loc, 500.f, 450.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 3. CROWN PLAZA -- Wide base tapering to narrow crown with gold cap
	//    Block (8,14)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(8, 14) + FVector(400.f, -300.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_CrownPlaza"));
		if (T)
		{
			// Wide base (6 stories) -- concrete
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1000.f),
				FVector(1000.f, 900.f, 2000.f), LightConcrete, TEXT("Crown_Base"), 0.f, 0.6f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Main tower rising from base -- concrete
			const float TH = 4500.f;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2000.f + TH * 0.5f),
				FVector(600.f, 550.f, TH), CoolWhite, TEXT("Crown_Tower"), 0.f, 0.6f);

			// Gold crown cap
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2000.f + TH + 100.f),
				FVector(650.f, 600.f, 200.f), GoldAccent, TEXT("Crown_Cap1"), 0.95f, 0.2f);
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2000.f + TH + 250.f),
				FVector(550.f, 500.f, 100.f), GoldAccent, TEXT("Crown_Cap2"), 0.95f, 0.2f);

			AddCornerColumns(T, FVector(Loc.X, Loc.Y, 2000.f), 610.f, 560.f, TH, 40.f, LightConcrete * 0.85f);

			// Center vertical spines -- steel structural
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 280.f, 2000.f + TH * 0.5f),
				FVector(40.f, 8.f, TH), LightConcrete * 0.8f, TEXT("Crown_SpineA"), 0.85f, 0.3f);
			MakeCubeMC(T, FVector(Loc.X, Loc.Y - 280.f, 2000.f + TH * 0.5f),
				FVector(40.f, 8.f, TH), LightConcrete * 0.8f, TEXT("Crown_SpineB"), 0.85f, 0.3f);

			AddGlassBands(T, FVector(Loc.X, Loc.Y, 2000.f), 600.f, 550.f, TH, TintedGlass, BandIdx);
			AddWindows(T, FVector(Loc.X, Loc.Y, 2000.f), 600.f, 550.f, TH, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 4. THE CYLINDER -- Round glass tower with observation deck ring
	//    Block (10,14)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(10, 14) + FVector(-300.f, -400.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Cylinder"));
		if (T)
		{
			const float H = 5000.f;
			const float R = 280.f;

			// Steel cylinder body
			auto* Body = MakeCylMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(R, R, H), SteelGray, TEXT("Cyl_Main"), 0.85f, 0.3f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Observation deck ring -- glass
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H * 0.82f),
				FVector(R + 80.f, R + 80.f, 180.f), DarkGlass, TEXT("Cyl_ObsDeck"), 0.1f, 0.02f);
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H * 0.82f - 80.f),
				FVector(R + 90.f, R + 90.f, 20.f), LightGray, TEXT("Cyl_ObsFloor"), 0.85f, 0.3f);

			// Roof disc -- steel
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H + 10.f),
				FVector(R + 20.f, R + 20.f, 20.f), DarkSteel, TEXT("Cyl_Roof"), 0.85f, 0.3f);

			// Antenna -- steel
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H + 600.f),
				FVector(12.f, 12.f, 1200.f), LightGray, TEXT("Cyl_Ant"), 0.85f, 0.3f);
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, H + 1200.f),
				FVector(20.f, 20.f, 20.f), AntennaRed, TEXT("Cyl_AntLight"), 0.f, 0.5f,
				FLinearColor(2.0f, 0.2f, 0.2f));

			// Ground floor entrance -- glass
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + R + 50.f, 200.f),
				FVector(300.f, 100.f, 380.f), SkyGlass * 1.5f, TEXT("Cyl_Entrance"), 0.1f, 0.02f);

			AddWindows(T, Loc, R * 2.f, R * 2.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 5. STEPPED MONOLITH -- Ultra-tall art deco with 3 dramatic setbacks
	//    Block (9,11)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(9, 11) + FVector(0.f, 600.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Monolith"));
		if (T)
		{
			// Tier 1: wide base -- steel
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1200.f),
				FVector(800.f, 700.f, 2400.f), SteelGray, TEXT("Mono_T1"), 0.85f, 0.3f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Tier 2 -- steel
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2400.f + 1500.f),
				FVector(600.f, 520.f, 3000.f), SteelGray * 0.95f, TEXT("Mono_T2"), 0.85f, 0.3f);
			// Tier 3 -- steel
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 5400.f + 1000.f),
				FVector(420.f, 380.f, 2000.f), SteelGray * 0.9f, TEXT("Mono_T3"), 0.85f, 0.3f);
			// Tier 4: crown -- dark steel
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 7400.f + 400.f),
				FVector(250.f, 230.f, 800.f), DarkSteel, TEXT("Mono_T4"), 0.85f, 0.3f);

			// Setback ledges -- copper accents
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2400.f),
				FVector(820.f, 720.f, 40.f), CopperAccent, TEXT("Mono_L1"), 0.9f, 0.25f);
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 5400.f),
				FVector(620.f, 540.f, 40.f), CopperAccent, TEXT("Mono_L2"), 0.9f, 0.25f);
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 7400.f),
				FVector(440.f, 400.f, 40.f), CopperAccent, TEXT("Mono_L3"), 0.9f, 0.25f);

			// Spire -- steel
			MakeCylMC(T, FVector(Loc.X, Loc.Y, 8200.f + 700.f),
				FVector(18.f, 18.f, 1400.f), LightGray, TEXT("Mono_Spire"), 0.85f, 0.3f);
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, 9600.f),
				FVector(28.f, 28.f, 28.f), AntennaRed, TEXT("Mono_Light"), 0.f, 0.5f,
				FLinearColor(2.0f, 0.2f, 0.2f));

			AddGlassBands(T, Loc, 800.f, 700.f, 2400.f, DarkGlass, BandIdx);
			AddGlassBands(T, FVector(Loc.X, Loc.Y, 2400.f), 600.f, 520.f, 3000.f, DarkGlass, BandIdx);
			AddWindows(T, Loc, 800.f, 700.f, 7800.f, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 6. TWIN TOWERS -- Two matching towers connected by skybridge
	//    Block (9,12)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(9, 12) + FVector(0.f, 500.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Twins"));
		if (T)
		{
			const float H = 4800.f;
			const float Sep = 450.f;

			auto* Body = MakeCubeMC(T, FVector(Loc.X - Sep, Loc.Y, H * 0.5f),
				FVector(400.f, 380.f, H), CoolWhite, TEXT("Twin_A"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			auto* BodyB = MakeCubeMC(T, FVector(Loc.X + Sep, Loc.Y, H * 0.5f),
				FVector(400.f, 380.f, H), CoolWhite, TEXT("Twin_B"));
			BodyB->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			BodyB->SetCollisionResponseToAllChannels(ECR_Block);

			// Skybridge at 60% height
			const float BridgeZ = H * 0.6f;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, BridgeZ),
				FVector(Sep * 2.f - 200.f, 180.f, 250.f), SkyGlass * 1.5f, TEXT("Twin_Bridge"));

			MakeCubeMC(T, FVector(Loc.X - 100.f, Loc.Y, BridgeZ - 200.f),
				FVector(30.f, 30.f, 400.f), LightGray, TEXT("Twin_BridgeColA"));
			MakeCubeMC(T, FVector(Loc.X + 100.f, Loc.Y, BridgeZ - 200.f),
				FVector(30.f, 30.f, 400.f), LightGray, TEXT("Twin_BridgeColB"));

			// Mechanical penthouses
			MakeCubeMC(T, FVector(Loc.X - Sep, Loc.Y, H + 150.f),
				FVector(250.f, 230.f, 300.f), DarkSteel, TEXT("Twin_PentA"));
			MakeCubeMC(T, FVector(Loc.X + Sep, Loc.Y, H + 150.f),
				FVector(250.f, 230.f, 300.f), DarkSteel, TEXT("Twin_PentB"));

			// Antennas
			MakeCylMC(T, FVector(Loc.X - Sep, Loc.Y, H + 300.f + 400.f),
				FVector(10.f, 10.f, 800.f), LightGray, TEXT("Twin_AntA"));
			MakeCylMC(T, FVector(Loc.X + Sep, Loc.Y, H + 300.f + 400.f),
				FVector(10.f, 10.f, 800.f), LightGray, TEXT("Twin_AntB"));

			AddGlassBands(T, FVector(Loc.X - Sep, Loc.Y, 0.f), 400.f, 380.f, H, TintedGlass, BandIdx);
			AddGlassBands(T, FVector(Loc.X + Sep, Loc.Y, 0.f), 400.f, 380.f, H, TintedGlass, BandIdx);
			AddWindows(T, FVector(Loc.X - Sep, Loc.Y, 0.f), 400.f, 380.f, H, WinIdx);
			AddWindows(T, FVector(Loc.X + Sep, Loc.Y, 0.f), 400.f, 380.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 7. THE SLAB -- Super-thin ultra-tall residential tower
	//    Block (8,13)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(8, 13) + FVector(0.f, -500.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Slab"));
		if (T)
		{
			const float H = 6500.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(200.f, 600.f, H), CoolWhite, TEXT("Slab_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Balcony ledges
			for (int32 i = 0; i < 10; ++i)
			{
				const float BZ = 500.f + i * 600.f;
				FString BNA = FString::Printf(TEXT("Slab_BalA_%d"), i);
				FString BNB = FString::Printf(TEXT("Slab_BalB_%d"), i);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y + 310.f, BZ),
					FVector(180.f, 30.f, 12.f), LightGray, *BNA);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y - 310.f, BZ),
					FVector(180.f, 30.f, 12.f), LightGray, *BNB);
			}

			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 40.f),
				FVector(220.f, 620.f, 15.f), LightGray, TEXT("Slab_Roof"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 200.f),
				FVector(160.f, 400.f, 300.f), SkyGlass * 1.5f, TEXT("Slab_Pent"));

			AddGlassBands(T, Loc, 200.f, 600.f, H, SkyGlass, BandIdx);
			AddWindows(T, Loc, 200.f, 600.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 8. BRUTALIST TOWER -- Chunky concrete with horizontal ledges
	//    Block (7,12)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(7, 12) + FVector(600.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Brutalist"));
		if (T)
		{
			const float H = 4000.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(650.f, 600.f, H), WarmConcrete, TEXT("Brut_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			for (int32 i = 0; i < 8; ++i)
			{
				const float LZ = 400.f + i * 480.f;
				FString LN = FString::Printf(TEXT("Brut_Ledge_%d"), i);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y, LZ),
					FVector(680.f, 630.f, 30.f), WarmConcrete * 0.85f, *LN);
			}

			for (int32 i = 0; i < 5; ++i)
			{
				const float WX = Loc.X - 250.f + i * 125.f;
				FString WNA = FString::Printf(TEXT("Brut_SlotA_%d"), i);
				FString WNB = FString::Printf(TEXT("Brut_SlotB_%d"), i);
				MakeCubeMC(T, FVector(WX, Loc.Y + 305.f, H * 0.5f),
					FVector(50.f, 10.f, H * 0.85f), CharcoalGray, *WNA);
				MakeCubeMC(T, FVector(WX, Loc.Y - 305.f, H * 0.5f),
					FVector(50.f, 10.f, H * 0.85f), CharcoalGray, *WNB);
			}

			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 150.f),
				FVector(500.f, 450.f, 300.f), CharcoalGray, TEXT("Brut_Mech"));

			AddWindows(T, FVector(Loc.X, Loc.Y, 0.f), 650.f, 600.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 9. COPPER SPIRE -- Hexagonal profile with spire
	//    Block (10,12)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(10, 12) + FVector(-500.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_CopperSpire"));
		if (T)
		{
			const float H = 5800.f;

			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(450.f, 400.f, H), LightConcrete, TEXT("Copper_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			MakeCubeMC(T, FVector(Loc.X + 60.f, Loc.Y + 60.f, H * 0.5f),
				FVector(350.f, 350.f, H * 0.97f), LightConcrete * 0.95f, TEXT("Copper_Facet"));

			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.75f + H * 0.125f),
				FVector(380.f, 340.f, H * 0.25f), CopperAccent * 0.8f, TEXT("Copper_Upper"));

			const float CrownBase = H;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, CrownBase + 200.f),
				FVector(350.f, 310.f, 400.f), CopperAccent, TEXT("Copper_Crown1"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, CrownBase + 500.f),
				FVector(250.f, 220.f, 200.f), CopperAccent * 1.1f, TEXT("Copper_Crown2"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, CrownBase + 700.f),
				FVector(150.f, 130.f, 200.f), CopperAccent * 1.2f, TEXT("Copper_Crown3"));

			MakeCylMC(T, FVector(Loc.X, Loc.Y, CrownBase + 800.f + 500.f),
				FVector(14.f, 14.f, 1000.f), CopperAccent * 0.7f, TEXT("Copper_Spire"));

			AddGlassBands(T, Loc, 450.f, 400.f, H * 0.7f, TintedGlass, BandIdx);
			AddWindows(T, Loc, 450.f, 400.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 10. THE OBELISK -- Tapered rectangular tower
	//     Block (7,13)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(7, 13) + FVector(500.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Obelisk"));
		if (T)
		{
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1500.f),
				FVector(700.f, 650.f, 3000.f), DarkSteel, TEXT("Obel_Lower"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 3000.f + 1250.f),
				FVector(550.f, 500.f, 2500.f), DarkSteel * 1.05f, TEXT("Obel_Mid"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 5500.f + 750.f),
				FVector(380.f, 340.f, 1500.f), SteelGray, TEXT("Obel_Upper"));

			// Helipad
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 7000.f + 10.f),
				FVector(400.f, 360.f, 20.f), LightGray, TEXT("Obel_RoofPad"));
			MakeCylMC(T, FVector(Loc.X, Loc.Y, 7020.f + 5.f),
				FVector(120.f, 120.f, 5.f), FLinearColor(0.8f, 0.8f, 0.0f), TEXT("Obel_Heli"));

			// H marking
			MakeCubeMC(T, FVector(Loc.X - 40.f, Loc.Y, 7028.f),
				FVector(8.f, 60.f, 3.f), CoolWhite, TEXT("Obel_H1"));
			MakeCubeMC(T, FVector(Loc.X + 40.f, Loc.Y, 7028.f),
				FVector(8.f, 60.f, 3.f), CoolWhite, TEXT("Obel_H2"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 7028.f),
				FVector(80.f, 8.f, 3.f), CoolWhite, TEXT("Obel_H3"));

			AddGlassBands(T, Loc, 700.f, 650.f, 3000.f, SkyGlass, BandIdx);
			AddGlassBands(T, FVector(Loc.X, Loc.Y, 3000.f), 550.f, 500.f, 2500.f, SkyGlass, BandIdx);
			AddWindows(T, Loc, 700.f, 650.f, 5500.f, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 11. PRISMATIC TOWER -- Offset stacked boxes
	//     Block (10,13)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(10, 13) + FVector(-400.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Prismatic"));
		if (T)
		{
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 900.f),
				FVector(800.f, 700.f, 1800.f), CoolWhite, TEXT("Prism_T1"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			MakeCubeMC(T, FVector(Loc.X + 80.f, Loc.Y - 50.f, 1800.f + 900.f),
				FVector(650.f, 580.f, 1800.f), CoolWhite * 0.97f, TEXT("Prism_T2"));
			MakeCubeMC(T, FVector(Loc.X + 150.f, Loc.Y - 100.f, 3600.f + 750.f),
				FVector(500.f, 450.f, 1500.f), CoolWhite * 0.94f, TEXT("Prism_T3"));
			MakeCubeMC(T, FVector(Loc.X + 200.f, Loc.Y - 130.f, 5100.f + 500.f),
				FVector(350.f, 320.f, 1000.f), CoolWhite * 0.91f, TEXT("Prism_T4"));

			MakeCubeMC(T, FVector(Loc.X - 400.f, Loc.Y, 900.f),
				FVector(15.f, 700.f, 1800.f), SkyGlass, TEXT("Prism_Stripe"));

			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1800.f),
				FVector(820.f, 720.f, 25.f), LightGray, TEXT("Prism_L1"));
			MakeCubeMC(T, FVector(Loc.X + 80.f, Loc.Y - 50.f, 3600.f),
				FVector(670.f, 600.f, 25.f), LightGray, TEXT("Prism_L2"));
			MakeCubeMC(T, FVector(Loc.X + 150.f, Loc.Y - 100.f, 5100.f),
				FVector(520.f, 470.f, 25.f), LightGray, TEXT("Prism_L3"));

			MakeCubeMC(T, FVector(Loc.X + 200.f, Loc.Y - 130.f, 6100.f + 120.f),
				FVector(200.f, 180.f, 240.f), DarkSteel, TEXT("Prism_Mech"));

			AddGlassBands(T, Loc, 800.f, 700.f, 1800.f, TintedGlass, BandIdx);
			AddWindows(T, Loc, 800.f, 700.f, 5600.f, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 12. BEACON TOWER -- Slender tower with dome observatory
	//     Block (9,14)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(9, 14) + FVector(0.f, -500.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Beacon"));
		if (T)
		{
			const float H = 4200.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(380.f, 350.f, H), SteelGray, TEXT("Beacon_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			MakeSphereMC(T, FVector(Loc.X, Loc.Y, H + 100.f),
				FVector(320.f, 320.f, 250.f), SkyGlass * 2.f, TEXT("Beacon_Dome"));
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H),
				FVector(200.f, 200.f, 40.f), LightGray, TEXT("Beacon_Ring"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H - 30.f),
				FVector(420.f, 390.f, 30.f), LightGray, TEXT("Beacon_Plat"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 350.f),
				FVector(500.f, 470.f, 700.f), SteelGray * 1.05f, TEXT("Beacon_Pod"));

			AddCornerColumns(T, Loc, 390.f, 360.f, H, 30.f, DarkSteel);
			AddGlassBands(T, Loc, 380.f, 350.f, H, TintedGlass, BandIdx);
			AddWindows(T, Loc, 380.f, 350.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 13. FORTRESS TOWER -- L-shaped footprint with courtyard
	//     Block (7,15)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(7, 15) + FVector(300.f, -300.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Fortress"));
		if (T)
		{
			const float H = 3800.f;

			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y - 200.f, H * 0.5f),
				FVector(800.f, 350.f, H), LightConcrete, TEXT("Fort_WingA"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			auto* WB = MakeCubeMC(T, FVector(Loc.X - 250.f, Loc.Y + 200.f, H * 0.5f),
				FVector(350.f, 600.f, H), LightConcrete * 0.97f, TEXT("Fort_WingB"));
			WB->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			WB->SetCollisionResponseToAllChannels(ECR_Block);

			MakeCubeMC(T, FVector(Loc.X - 250.f, Loc.Y - 200.f, H * 0.5f + 500.f),
				FVector(380.f, 380.f, H + 1000.f), LightConcrete * 0.93f, TEXT("Fort_Corner"));
			MakeCubeMC(T, FVector(Loc.X - 250.f, Loc.Y - 200.f, H + 1000.f + 100.f),
				FVector(400.f, 400.f, 50.f), CopperAccent, TEXT("Fort_Crown"));
			MakeCubeMC(T, FVector(Loc.X + 100.f, Loc.Y + 200.f, 5.f),
				FVector(350.f, 350.f, 5.f), FLinearColor(0.08f, 0.25f, 0.06f), TEXT("Fort_Court"));

			AddGlassBands(T, FVector(Loc.X, Loc.Y - 200.f, 0.f), 800.f, 350.f, H, TintedGlass, BandIdx);
			AddWindows(T, FVector(Loc.X, Loc.Y - 200.f, 0.f), 800.f, 350.f, H, WinIdx);
			AddWindows(T, FVector(Loc.X - 250.f, Loc.Y + 200.f, 0.f), 350.f, 600.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 14. RED STRIPE TOWER -- Dark tower with bold red accent bands
	//     Block (10,10)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(10, 10) + FVector(-400.f, 400.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_RedStripe"));
		if (T)
		{
			const float H = 4600.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(480.f, 440.f, H), CharcoalGray, TEXT("Red_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			for (int32 i = 0; i < 5; ++i)
			{
				const float BZ = 600.f + i * 900.f;
				FString RN1 = FString::Printf(TEXT("Red_BandA_%d"), i);
				FString RN2 = FString::Printf(TEXT("Red_BandB_%d"), i);
				FString RN3 = FString::Printf(TEXT("Red_BandC_%d"), i);
				FString RN4 = FString::Printf(TEXT("Red_BandD_%d"), i);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y + 225.f, BZ),
					FVector(490.f, 10.f, 50.f), RedAccent, *RN1);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y - 225.f, BZ),
					FVector(490.f, 10.f, 50.f), RedAccent, *RN2);
				MakeCubeMC(T, FVector(Loc.X + 245.f, Loc.Y, BZ),
					FVector(10.f, 450.f, 50.f), RedAccent, *RN3);
				MakeCubeMC(T, FVector(Loc.X - 245.f, Loc.Y, BZ),
					FVector(10.f, 450.f, 50.f), RedAccent, *RN4);
			}

			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 50.f),
				FVector(500.f, 460.f, 40.f), RedAccent, TEXT("Red_Crown"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 120.f),
				FVector(350.f, 310.f, 100.f), CharcoalGray, TEXT("Red_Mech"));

			MakeCylMC(T, FVector(Loc.X, Loc.Y, H + 170.f + 400.f),
				FVector(10.f, 10.f, 800.f), LightGray, TEXT("Red_Ant"));

			AddWindows(T, Loc, 480.f, 440.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 15. GOLD CREST TOWER -- Elegant tower with gold-framed facade
	//     Block (7,10)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(7, 10) + FVector(-500.f, -300.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_GoldCrest"));
		if (T)
		{
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 600.f),
				FVector(750.f, 650.f, 1200.f), SandStone, TEXT("Gold_Base"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			const float H = 5200.f;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1200.f + H * 0.5f),
				FVector(500.f, 460.f, H), CoolWhite, TEXT("Gold_Tower"));

			for (int32 i = 0; i < 4; ++i)
			{
				const float MX = Loc.X - 180.f + i * 120.f;
				FString MNA = FString::Printf(TEXT("Gold_MullA_%d"), i);
				FString MNB = FString::Printf(TEXT("Gold_MullB_%d"), i);
				MakeCubeMC(T, FVector(MX, Loc.Y + 235.f, 1200.f + H * 0.5f),
					FVector(12.f, 8.f, H), GoldAccent, *MNA);
				MakeCubeMC(T, FVector(MX, Loc.Y - 235.f, 1200.f + H * 0.5f),
					FVector(12.f, 8.f, H), GoldAccent, *MNB);
			}
			for (int32 i = 0; i < 8; ++i)
			{
				const float SZ = 1200.f + 500.f + i * 600.f;
				FString SNA = FString::Printf(TEXT("Gold_SpanA_%d"), i);
				FString SNB = FString::Printf(TEXT("Gold_SpanB_%d"), i);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y + 235.f, SZ),
					FVector(500.f, 8.f, 15.f), GoldAccent, *SNA);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y - 235.f, SZ),
					FVector(500.f, 8.f, 15.f), GoldAccent, *SNB);
			}

			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1200.f + H + 80.f),
				FVector(520.f, 480.f, 60.f), GoldAccent, TEXT("Gold_CrownA"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1200.f + H + 160.f),
				FVector(450.f, 410.f, 60.f), GoldAccent * 0.9f, TEXT("Gold_CrownB"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1200.f + H + 240.f),
				FVector(380.f, 340.f, 60.f), GoldAccent * 0.8f, TEXT("Gold_CrownC"));

			MakeCylMC(T, FVector(Loc.X, Loc.Y, 1200.f + H + 270.f + 450.f),
				FVector(16.f, 16.f, 900.f), GoldAccent * 0.7f, TEXT("Gold_Spire"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 340.f, 250.f),
				FVector(500.f, 80.f, 15.f), GoldAccent, TEXT("Gold_Canopy"));

			AddGlassBands(T, FVector(Loc.X, Loc.Y, 1200.f), 500.f, 460.f, H, TintedGlass, BandIdx);
			AddWindows(T, FVector(Loc.X, Loc.Y, 1200.f), 500.f, 460.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 16. MARBLE SPIRE -- White marble clad tower with emerald bands
	//     Block (8,10)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(8, 10) + FVector(200.f, 400.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_MarbleSpire"));
		if (T)
		{
			const float H = 5400.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(480.f, 440.f, H), WhiteMarble, TEXT("Marble_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Emerald accent bands
			for (int32 i = 0; i < 6; ++i)
			{
				const float BZ = 800.f + i * 800.f;
				FString EN = FString::Printf(TEXT("Marble_EBand_%d"), i);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y + 225.f, BZ),
					FVector(490.f, 8.f, 35.f), EmeraldGreen, *EN);
			}

			// Stepped crown
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 100.f),
				FVector(400.f, 360.f, 200.f), WhiteMarble * 0.95f, TEXT("Marble_Crown1"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 250.f),
				FVector(300.f, 260.f, 100.f), WhiteMarble * 0.9f, TEXT("Marble_Crown2"));

			MakeCylMC(T, FVector(Loc.X, Loc.Y, H + 300.f + 500.f),
				FVector(12.f, 12.f, 1000.f), LightGray, TEXT("Marble_Spire"));
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, H + 1300.f),
				FVector(22.f, 22.f, 22.f), AntennaRed, TEXT("Marble_Light"));

			AddGlassBands(T, Loc, 480.f, 440.f, H, CrystalBlue, BandIdx);
			AddWindows(T, Loc, 480.f, 440.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 17. NAVY TOWER -- Dark navy blue tower with cream horizontal bands
	//     Block (9,10)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(9, 10) + FVector(-200.f, -400.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_NavyTower"));
		if (T)
		{
			const float H = 4800.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(520.f, 480.f, H), NavyBlue, TEXT("Navy_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Cream horizontal bands every 4 floors
			for (int32 i = 0; i < 7; ++i)
			{
				const float BZ = 500.f + i * 650.f;
				FString BN = FString::Printf(TEXT("Navy_Cream_%d"), i);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y, BZ),
					FVector(540.f, 500.f, 25.f), CreamColor, *BN);
			}

			// Penthouse with terrace
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 150.f),
				FVector(380.f, 340.f, 300.f), NavyBlue * 1.2f, TEXT("Navy_Pent"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 15.f),
				FVector(540.f, 500.f, 30.f), CreamColor, TEXT("Navy_Roof"));

			AddWindows(T, Loc, 520.f, 480.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 18. TERRACOTTA TOWER -- Warm terracotta clad with arched details
	//     Block (8,15)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(8, 15) + FVector(-300.f, 200.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Terracotta"));
		if (T)
		{
			const float H = 3600.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(550.f, 500.f, H), Terracotta, TEXT("Terra_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Arched entrance facade (approximated with stacked cubes)
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 255.f, 250.f),
				FVector(200.f, 15.f, 500.f), Terracotta * 0.85f, TEXT("Terra_Arch1"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 255.f, 520.f),
				FVector(220.f, 15.f, 40.f), CreamColor, TEXT("Terra_ArchCap"));

			// Cream cornice at mid-height and top
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(570.f, 520.f, 30.f), CreamColor, TEXT("Terra_MidCornice"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 15.f),
				FVector(570.f, 520.f, 30.f), CreamColor, TEXT("Terra_TopCornice"));

			// Rooftop water tank (typical old-style)
			MakeCylMC(T, FVector(Loc.X + 100.f, Loc.Y, H + 200.f),
				FVector(60.f, 60.f, 200.f), GrayGreen, TEXT("Terra_Tank"));

			AddWindows(T, Loc, 550.f, 500.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 19. CRYSTAL TOWER -- All-glass tower with crystalline crown
	//     Block (9,15)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(9, 15) + FVector(300.f, -200.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Crystal"));
		if (T)
		{
			const float H = 6000.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(460.f, 420.f, H), CrystalBlue, TEXT("Crystal_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Crystalline crown -- angled shards
			MakeCubeMC(T, FVector(Loc.X - 80.f, Loc.Y, H + 300.f),
				FVector(200.f, 200.f, 600.f), CrystalBlue * 1.3f, TEXT("Crystal_ShardA"));
			MakeCubeMC(T, FVector(Loc.X + 80.f, Loc.Y, H + 200.f),
				FVector(180.f, 180.f, 400.f), CrystalBlue * 1.5f, TEXT("Crystal_ShardB"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y - 60.f, H + 400.f),
				FVector(150.f, 150.f, 800.f), SkyGlass * 2.f, TEXT("Crystal_ShardC"));

			// Glass fins on facade
			for (int32 i = 0; i < 6; ++i)
			{
				const float FZ = 800.f + i * 900.f;
				FString FN = FString::Printf(TEXT("Crystal_Fin_%d"), i);
				MakeCubeMC(T, FVector(Loc.X + 235.f, Loc.Y, FZ),
					FVector(15.f, 380.f, 60.f), SkyGlass * 1.5f, *FN);
			}

			AddGlassBands(T, Loc, 460.f, 420.f, H, SkyGlass, BandIdx);
			AddWindows(T, Loc, 460.f, 420.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 20. GREEN TOWER -- Eco tower with setback terraces and greenery
	//     Block (10,15)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(10, 15) + FVector(-200.f, 300.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_GreenTower"));
		if (T)
		{
			const float H = 4400.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(600.f, 550.f, H), GrayGreen, TEXT("Green_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Setback terraces with green strips
			const FLinearColor PlantGreen(0.08f, 0.30f, 0.08f);
			for (int32 i = 0; i < 4; ++i)
			{
				const float TZ = 1000.f + i * 900.f;
				const float TW = 620.f - i * 40.f;
				FString TN = FString::Printf(TEXT("Green_Terrace_%d"), i);
				FString GN = FString::Printf(TEXT("Green_Plant_%d"), i);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y, TZ),
					FVector(TW, TW - 50.f, 25.f), LightGray, *TN);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y + (TW - 50.f) * 0.5f - 20.f, TZ + 30.f),
					FVector(TW - 40.f, 30.f, 40.f), PlantGreen, *GN);
			}

			// Wind turbine on roof (decorative)
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H + 300.f),
				FVector(20.f, 20.f, 600.f), LightGray, TEXT("Green_TurbPole"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 600.f),
				FVector(200.f, 15.f, 15.f), CoolWhite, TEXT("Green_Blade"));

			AddGlassBands(T, Loc, 600.f, 550.f, H, SkyGlass, BandIdx);
			AddWindows(T, Loc, 600.f, 550.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 21. THE HELIX -- Twisted glass tower with 3 rotated sections
	//     Block (7,10)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(7, 10) + FVector(0.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Helix"));
		if (T)
		{
			const float H = 2800.f;
			const float SecH = 933.f;

			// Section 1: base section (0-933)
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, SecH * 0.5f),
				FVector(300.f, 300.f, SecH), TintedGlass, TEXT("Helix_Sec1"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Section 2: offset 60 UU (933-1866)
			MakeCubeMC(T, FVector(Loc.X + 60.f, Loc.Y + 60.f, SecH + SecH * 0.5f),
				FVector(300.f, 300.f, SecH), TintedGlass * 0.97f, TEXT("Helix_Sec2"));

			// Section 3: offset 120 UU (1866-2800)
			MakeCubeMC(T, FVector(Loc.X + 120.f, Loc.Y + 120.f, 2.f * SecH + SecH * 0.5f),
				FVector(300.f, 300.f, SecH), TintedGlass * 0.94f, TEXT("Helix_Sec3"));

			// Steel frame edges per section
			MakeCubeMC(T, FVector(Loc.X - 150.f, Loc.Y, SecH * 0.5f),
				FVector(12.f, 300.f, SecH), SteelGray, TEXT("Helix_Frame1L"));
			MakeCubeMC(T, FVector(Loc.X + 150.f, Loc.Y, SecH * 0.5f),
				FVector(12.f, 300.f, SecH), SteelGray, TEXT("Helix_Frame1R"));
			MakeCubeMC(T, FVector(Loc.X + 60.f - 150.f, Loc.Y + 60.f, SecH + SecH * 0.5f),
				FVector(12.f, 300.f, SecH), SteelGray, TEXT("Helix_Frame2L"));
			MakeCubeMC(T, FVector(Loc.X + 60.f + 150.f, Loc.Y + 60.f, SecH + SecH * 0.5f),
				FVector(12.f, 300.f, SecH), SteelGray, TEXT("Helix_Frame2R"));
			MakeCubeMC(T, FVector(Loc.X + 120.f - 150.f, Loc.Y + 120.f, 2.f * SecH + SecH * 0.5f),
				FVector(12.f, 300.f, SecH), SteelGray, TEXT("Helix_Frame3L"));
			MakeCubeMC(T, FVector(Loc.X + 120.f + 150.f, Loc.Y + 120.f, 2.f * SecH + SecH * 0.5f),
				FVector(12.f, 300.f, SecH), SteelGray, TEXT("Helix_Frame3R"));

			// Crown: flat glass disc on top
			MakeCylMC(T, FVector(Loc.X + 120.f, Loc.Y + 120.f, H + 15.f),
				FVector(180.f, 180.f, 30.f), TintedGlass * 1.3f, TEXT("Helix_Crown"));

			AddGlassBands(T, Loc, 300.f, 300.f, H, TintedGlass, BandIdx);
			AddWindows(T, Loc, 300.f, 300.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 22. THE OBELISK -- Black monolith tapering to a point
	//     Block (10,10)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(10, 10) + FVector(0.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Obelisk"));
		if (T)
		{
			const float TotalH = 9000.f;
			const float TierH = 2250.f;

			// Tier 1: base (0-2250), 800x700
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, TierH * 0.5f),
				FVector(800.f, 700.f, TierH), CharcoalGray, TEXT("Obelisk_T1"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Tier 2: (2250-4500), 700x610
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, TierH + TierH * 0.5f),
				FVector(700.f, 610.f, TierH), CharcoalGray * 0.97f, TEXT("Obelisk_T2"));

			// Tier 3: (4500-6750), 600x520
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2.f * TierH + TierH * 0.5f),
				FVector(600.f, 520.f, TierH), CharcoalGray * 0.94f, TEXT("Obelisk_T3"));

			// Tier 4: (6750-9000), 500x430
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 3.f * TierH + TierH * 0.5f),
				FVector(500.f, 430.f, TierH), CharcoalGray * 0.91f, TEXT("Obelisk_T4"));

			// Gold accent trim bands between tiers
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, TierH),
				FVector(820.f, 720.f, 30.f), GoldAccent, TEXT("Obelisk_Band1"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2.f * TierH),
				FVector(720.f, 630.f, 30.f), GoldAccent, TEXT("Obelisk_Band2"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 3.f * TierH),
				FVector(620.f, 540.f, 30.f), GoldAccent, TEXT("Obelisk_Band3"));

			// Pointed crown: narrow cylinder on top
			MakeCylMC(T, FVector(Loc.X, Loc.Y, TotalH + 150.f),
				FVector(80.f, 80.f, 300.f), CharcoalGray * 0.85f, TEXT("Obelisk_Point"));

			AddWindows(T, Loc, 800.f, 700.f, TierH, WinIdx);
			AddWindows(T, FVector(Loc.X, Loc.Y, TierH), 700.f, 610.f, TierH, WinIdx);
			AddWindows(T, FVector(Loc.X, Loc.Y, 2.f * TierH), 600.f, 520.f, TierH, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 23. HARBOR TOWER -- Nautical glass tower with observation deck
	//     Block (8,15)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(8, 15) + FVector(0.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Harbor"));
		if (T)
		{
			const float H = 2200.f;

			// Main body — short, compact waterfront tower
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(350.f, 300.f, H), CoolWhite, TEXT("Harbor_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Blue glass accent band at 60% height
			const float BandZ = H * 0.6f;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, BandZ),
				FVector(370.f, 320.f, 80.f), CrystalBlue, TEXT("Harbor_BlueBand"));

			// Observation deck: wider cylinder ring at 80% height
			const float DeckZ = H * 0.8f;
			MakeCylMC(T, FVector(Loc.X, Loc.Y, DeckZ),
				FVector(220.f, 220.f, 30.f), LightGray, TEXT("Harbor_ObsDeck"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, DeckZ + 20.f),
				FVector(440.f, 440.f, 12.f), LightGray, TEXT("Harbor_DeckPlat"));

			// Lighthouse-style beacon sphere on top
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, H + 60.f),
				FVector(70.f, 70.f, 70.f), AntennaRed, TEXT("Harbor_Beacon"));
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H + 15.f),
				FVector(30.f, 30.f, 30.f), LightGray, TEXT("Harbor_BeaconBase"));

			// Ground floor: glass lobby entrance
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 160.f, 150.f),
				FVector(280.f, 50.f, 300.f), SkyGlass, TEXT("Harbor_Lobby"));

			AddGlassBands(T, Loc, 350.f, 300.f, H, CrystalBlue, BandIdx);
			AddWindows(T, Loc, 350.f, 300.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 24. THE PRISM -- Triangular-ish tower with overlapping cubes
	//     Block (9,14)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(9, 14) + FVector(200.f, 200.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Prism"));
		if (T)
		{
			const float H = 5800.f;

			// Primary cube — tall, angular
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(420.f, 420.f, H), SkyGlass, TEXT("Prism_Primary"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Secondary cube offset 150 UU diagonally
			MakeCubeMC(T, FVector(Loc.X + 150.f, Loc.Y + 150.f, H * 0.5f),
				FVector(370.f, 370.f, H), DarkGlass, TEXT("Prism_Secondary"));

			// Steel frame columns on all corners of primary cube
			AddCornerColumns(T, Loc, 420.f, 420.f, H, 25.f, SteelGray);

			// Steel frame columns on all corners of secondary cube
			AddCornerColumns(T, FVector(Loc.X + 150.f, Loc.Y + 150.f, 0.f), 370.f, 370.f, H, 25.f, SteelGray);

			// Antenna spire on top
			MakeCylMC(T, FVector(Loc.X + 75.f, Loc.Y + 75.f, H + 500.f),
				FVector(15.f, 15.f, 1000.f), SteelGray, TEXT("Prism_Antenna"));
			MakeSphereMC(T, FVector(Loc.X + 75.f, Loc.Y + 75.f, H + 1010.f),
				FVector(30.f, 30.f, 30.f), AntennaRed, TEXT("Prism_AntennaTop"));

			AddGlassBands(T, Loc, 420.f, 420.f, H, SkyGlass, BandIdx);
			AddWindows(T, Loc, 420.f, 420.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 25. METRO CENTER -- Wide L-shaped tower with skybridge
	//     Block (7,13)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(7, 13) + FVector(0.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Metro"));
		if (T)
		{
			const float H = 2500.f;

			// Wing A: 650x300 — wide but squat
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(650.f, 300.f, H), LightConcrete, TEXT("Metro_WingA"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Wing B: 300x500, perpendicular
			MakeCubeMC(T, FVector(Loc.X + 175.f, Loc.Y + 400.f, H * 0.5f),
				FVector(300.f, 500.f, H), LightConcrete, TEXT("Metro_WingB"));

			// Corner columns in DarkSteel on Wing A
			AddCornerColumns(T, Loc, 650.f, 300.f, H, 25.f, DarkSteel);

			// Corner columns in DarkSteel on Wing B
			AddCornerColumns(T, FVector(Loc.X + 175.f, Loc.Y + 400.f, 0.f), 300.f, 500.f, H, 25.f, DarkSteel);

			// Rooftop garden: green platform on Wing A
			const FLinearColor GardenGreen(0.1f, 0.35f, 0.1f);
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 15.f),
				FVector(630.f, 280.f, 30.f), GardenGreen, TEXT("Metro_Garden"));

			// Glass skybridge connecting tops of both wings
			MakeCubeMC(T, FVector(Loc.X + 87.f, Loc.Y + 200.f, H - 80.f),
				FVector(180.f, 420.f, 70.f), SkyGlass, TEXT("Metro_Skybridge"));

			AddGlassBands(T, Loc, 650.f, 300.f, H, TintedGlass, BandIdx);
			AddWindows(T, Loc, 650.f, 300.f, H, WinIdx);
			AddWindows(T, FVector(Loc.X + 175.f, Loc.Y + 400.f, 0.f), 300.f, 500.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 26. THE SPIRE -- Ultra-thin needle tower, tallest in the city
	//     Block (10,13)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(10, 13) + FVector(400.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_Spire"));
		if (T)
		{
			const float H = 10000.f;

			// Main ultra-thin body — tallest tower in the city
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(180.f, 180.f, H), CoolWhite, TEXT("Spire_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Observation sphere at 8500 UU height
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, 8500.f),
				FVector(280.f, 280.f, 280.f), DarkGlass, TEXT("Spire_ObsSphere"));

			// Antenna extending 1000 UU above the top
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H + 500.f),
				FVector(10.f, 10.f, 1000.f), SteelGray, TEXT("Spire_Antenna"));
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, H + 1010.f),
				FVector(22.f, 22.f, 22.f), AntennaRed, TEXT("Spire_AntennaTop"));

			// Reinforcement rings every 1200 UU
			for (int32 i = 1; i <= 8; ++i)
			{
				const float RZ = i * 1200.f;
				FString RN = FString::Printf(TEXT("Spire_Ring_%d"), i);
				MakeCylMC(T, FVector(Loc.X, Loc.Y, RZ),
					FVector(120.f, 120.f, 20.f), LightGray, *RN);
			}

			// Ground plaza: large flat platform around the base
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 10.f),
				FVector(700.f, 700.f, 20.f), LightGray, TEXT("Spire_Plaza"));

			AddWindows(T, Loc, 180.f, 180.f, H, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 27. COPPER RIDGE -- Stepped copper tower with Art Nouveau details
	//     Block (9,10)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(9, 10) + FVector(0.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("Skyscraper_CopperRidge"));
		if (T)
		{
			const float TotalH = 3200.f;

			// Tier 1: base (0-1000), 600 wide
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 500.f),
				FVector(600.f, 600.f, 1000.f), CopperAccent, TEXT("Copper_T1"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Tier 2: (1000-2000), 460 wide
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1000.f + 500.f),
				FVector(460.f, 460.f, 1000.f), CopperAccent * 0.97f, TEXT("Copper_T2"));

			// Tier 3: (2000-2700), 340 wide
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2000.f + 350.f),
				FVector(340.f, 340.f, 700.f), CopperAccent * 0.94f, TEXT("Copper_T3"));

			// Tier 4: (2700-3200), 240 wide
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2700.f + 250.f),
				FVector(240.f, 240.f, 500.f), CopperAccent * 0.91f, TEXT("Copper_T4"));

			// Gold decorative bands at each setback
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1000.f),
				FVector(620.f, 620.f, 25.f), GoldAccent, TEXT("Copper_Band1"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2000.f),
				FVector(480.f, 480.f, 25.f), GoldAccent, TEXT("Copper_Band2"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2700.f),
				FVector(360.f, 360.f, 25.f), GoldAccent, TEXT("Copper_Band3"));

			// Art Nouveau ornamental spheres at each setback corner
			const float SphR = 30.f;
			for (int32 tier = 0; tier < 3; ++tier)
			{
				const float SZ = (tier == 0) ? 1000.f : (tier == 1) ? 2000.f : 2700.f;
				const float SW = (tier == 0) ? 600.f : (tier == 1) ? 460.f : 340.f;
				const float HalfW = SW * 0.5f;
				FString SA = FString::Printf(TEXT("Copper_Orb_%d_A"), tier);
				FString SB = FString::Printf(TEXT("Copper_Orb_%d_B"), tier);
				FString SC = FString::Printf(TEXT("Copper_Orb_%d_C"), tier);
				FString SD = FString::Printf(TEXT("Copper_Orb_%d_D"), tier);
				MakeSphereMC(T, FVector(Loc.X - HalfW, Loc.Y - HalfW, SZ + SphR),
					FVector(SphR * 2.f, SphR * 2.f, SphR * 2.f), GoldAccent, *SA);
				MakeSphereMC(T, FVector(Loc.X + HalfW, Loc.Y - HalfW, SZ + SphR),
					FVector(SphR * 2.f, SphR * 2.f, SphR * 2.f), GoldAccent, *SB);
				MakeSphereMC(T, FVector(Loc.X - HalfW, Loc.Y + HalfW, SZ + SphR),
					FVector(SphR * 2.f, SphR * 2.f, SphR * 2.f), GoldAccent, *SC);
				MakeSphereMC(T, FVector(Loc.X + HalfW, Loc.Y + HalfW, SZ + SphR),
					FVector(SphR * 2.f, SphR * 2.f, SphR * 2.f), GoldAccent, *SD);
			}

			// Mechanical penthouse on top
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, TotalH + 60.f),
				FVector(150.f, 150.f, 120.f), DarkSteel, TEXT("Copper_Mech"));
			MakeCylMC(T, FVector(Loc.X, Loc.Y, TotalH + 120.f + 50.f),
				FVector(12.f, 12.f, 100.f), SteelGray, TEXT("Copper_MechVent"));

			AddGlassBands(T, Loc, 600.f, 600.f, 1000.f, GoldAccent, BandIdx);
			AddWindows(T, Loc, 600.f, 600.f, TotalH, WinIdx);

			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Spawned %d skyscrapers (%d windows)."), SkyCount, WinIdx);
}

// ---------------------------------------------------------------------------
// Mountain Range — dramatic peaks in the northern expansion area
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnMountainRange()
{
	UWorld* World = GetWorld();
	if (!World || !SphereMesh || !CubeMesh || !CylinderMesh || !ConeMesh) return;

	int32 CompIdx = 0;

	// ================================================================
	// Mesh helpers
	// ================================================================
	auto MC_Cube = [&](AActor* O, const FVector& Loc, const FVector& S,
		const FRotator& Rot, const FLinearColor& Col, float Rough, float Metal, bool bCollision = false) -> UStaticMeshComponent*
	{
		FString N = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(O, *N);
		MC->SetStaticMesh(CubeMesh);
		MC->SetWorldScale3D(S); MC->SetWorldLocation(Loc); MC->SetWorldRotation(Rot);
		if (bCollision)
		{
			MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MC->SetCollisionObjectType(ECC_WorldStatic);
			MC->SetCollisionResponseToAllChannels(ECR_Block);
		}
		else
		{
			MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		ApplyPBR(MC, Col, Metal, Rough);
		O->AddInstanceComponent(MC);
		return MC;
	};

	auto MC_Cone = [&](AActor* O, const FVector& Loc, const FVector& S,
		const FRotator& Rot, const FLinearColor& Col, float Rough, float Metal, bool bCollision = false) -> UStaticMeshComponent*
	{
		FString N = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(O, *N);
		MC->SetStaticMesh(ConeMesh);
		MC->SetWorldScale3D(S); MC->SetWorldLocation(Loc); MC->SetWorldRotation(Rot);
		if (bCollision)
		{
			MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MC->SetCollisionObjectType(ECC_WorldStatic);
			MC->SetCollisionResponseToAllChannels(ECR_Block);
		}
		else
		{
			MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		ApplyPBR(MC, Col, Metal, Rough);
		O->AddInstanceComponent(MC);
		return MC;
	};

	auto MC_Cyl = [&](AActor* O, const FVector& Loc, const FVector& S,
		const FRotator& Rot, const FLinearColor& Col, float Rough, float Metal, bool bCollision = false) -> UStaticMeshComponent*
	{
		FString N = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(O, *N);
		MC->SetStaticMesh(CylinderMesh);
		MC->SetWorldScale3D(S); MC->SetWorldLocation(Loc); MC->SetWorldRotation(Rot);
		if (bCollision)
		{
			MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MC->SetCollisionObjectType(ECC_WorldStatic);
			MC->SetCollisionResponseToAllChannels(ECR_Block);
		}
		else
		{
			MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		ApplyPBR(MC, Col, Metal, Rough);
		O->AddInstanceComponent(MC);
		return MC;
	};

	// ================================================================
	// Peak definitions — 23 peaks in two rows
	// ================================================================
	struct FPeakData { float X; float Y; float Height; float RadX; float RadY; };

	const FPeakData Peaks[] = {
		{ -92000.f, 100000.f, 14500.f, 8000.f, 7000.f },
		{ -80000.f,  97000.f, 10200.f, 6500.f, 5800.f },
		{ -68000.f, 102000.f, 16800.f, 9000.f, 8000.f },
		{ -55000.f,  96000.f, 11500.f, 7000.f, 6200.f },
		{ -43000.f, 101000.f, 17500.f, 9500.f, 8500.f },
		{ -31000.f,  98000.f,  8800.f, 6000.f, 5500.f },
		{ -20000.f, 103000.f, 15200.f, 8500.f, 7500.f },
		{  -8000.f,  97500.f, 12000.f, 7200.f, 6500.f },
		{   5000.f, 101500.f, 17000.f, 9200.f, 8200.f },
		{  17000.f,  96500.f,  9500.f, 6200.f, 5800.f },
		{  28000.f, 100500.f, 15800.f, 8800.f, 7800.f },
		{  39000.f,  98500.f,  7200.f, 5500.f, 5000.f },
		{  50000.f, 102500.f, 13500.f, 7500.f, 6800.f },
		{  61000.f,  97000.f, 16200.f, 9000.f, 8000.f },
		{  72000.f, 100000.f, 10800.f, 6800.f, 6200.f },
		{  82000.f,  96000.f, 14800.f, 8200.f, 7200.f },
		{  93000.f, 101000.f,  5800.f, 5000.f, 4500.f },
		{ -85000.f, 108000.f,  6500.f, 5200.f, 4800.f },
		{ -50000.f, 110000.f,  9000.f, 6500.f, 5500.f },
		{ -15000.f, 109000.f,  7800.f, 5800.f, 5200.f },
		{  15000.f, 111000.f, 10500.f, 7000.f, 6000.f },
		{  48000.f, 108500.f,  8200.f, 6000.f, 5500.f },
		{  78000.f, 107000.f,  7500.f, 5500.f, 5000.f },
		// ---- Southern front row (pushed back ~30k to clear lake/pirate ship at Y=-95k) ----
		{ -85000.f, -130000.f, 13500.f, 7500.f, 6500.f },
		{ -65000.f, -127000.f,  9800.f, 6200.f, 5500.f },
		{ -45000.f, -132000.f, 15500.f, 8500.f, 7500.f },
		{ -25000.f, -128000.f, 11000.f, 7000.f, 6200.f },
		{  -8000.f, -131000.f, 16500.f, 9000.f, 8000.f },
		{  10000.f, -127500.f, 12500.f, 7500.f, 6800.f },
		{  28000.f, -130500.f, 14800.f, 8200.f, 7200.f },
		{  48000.f, -128000.f, 10000.f, 6500.f, 5800.f },
		{  65000.f, -132000.f, 15000.f, 8500.f, 7500.f },
		{  82000.f, -127000.f, 11500.f, 7000.f, 6200.f },
		// ---- Southern back row ----
		{ -60000.f, -139000.f,  7500.f, 5500.f, 5000.f },
		{ -15000.f, -140000.f,  9200.f, 6200.f, 5500.f },
		{  25000.f, -138000.f,  8000.f, 5800.f, 5200.f },
		{  60000.f, -139500.f,  7000.f, 5500.f, 4800.f },
	};
	const int32 NumPeaks = UE_ARRAY_COUNT(Peaks);

	// Snow peaks (northern tallest only — southern peaks are volcanic)
	const int32 SnowIdx[] = { 4, 8, 2, 13, 10, 6, 15, 0 };
	const int32 NumSnow = UE_ARRAY_COUNT(SnowIdx);

	// Southern front row = indices 23-32, back row = 33-36
	const int32 VolcanoStartIdx = 23;
	const int32 VolcanoEndIdx = 36; // inclusive

	// ================================================================
	// Color palette — layered geological look
	// ================================================================
	const FLinearColor RockDark(0.25f, 0.23f, 0.21f);    // deep shadow rock
	const FLinearColor RockMid(0.38f, 0.36f, 0.33f);     // mid-tone granite
	const FLinearColor RockLight(0.48f, 0.45f, 0.42f);   // lighter exposed face
	const FLinearColor CliffFace(0.22f, 0.20f, 0.19f);   // near-vertical dark cliff
	const FLinearColor Talus(0.32f, 0.30f, 0.27f);       // scree/debris color
	const FLinearColor Alpine(0.22f, 0.28f, 0.15f);      // green alpine meadow
	const FLinearColor TreeLine(0.12f, 0.18f, 0.08f);    // dark forest green
	const FLinearColor SnowFresh(0.94f, 0.95f, 0.97f);   // bright fresh snow
	const FLinearColor SnowOld(0.82f, 0.84f, 0.88f);     // packed/old snow
	const FLinearColor RidgeRock(0.30f, 0.28f, 0.26f);   // ridge connector

	// Volcanic palette
	const FLinearColor VolcanicRock(0.10f, 0.08f, 0.07f);    // dark basalt
	const FLinearColor VolcanicDark(0.06f, 0.05f, 0.04f);    // obsidian
	const FLinearColor LavaBright(1.0f, 0.35f, 0.02f);       // bright molten lava
	const FLinearColor LavaGlow(0.9f, 0.18f, 0.01f);         // deep magma
	const FLinearColor LavaEmissive(12.f, 3.f, 0.3f);        // emissive glow
	const FLinearColor LavaCrustEmissive(4.f, 0.8f, 0.1f);   // cooler crust glow
	const FLinearColor AshGray(0.18f, 0.17f, 0.16f);         // volcanic ash
	const FLinearColor SmokeColor(0.25f, 0.23f, 0.22f);      // eruption smoke

	// ================================================================
	// BUILD EACH MOUNTAIN
	// ================================================================
	for (int32 P = 0; P < NumPeaks; ++P)
	{
		const FPeakData& PD = Peaks[P];
		AActor* M = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(PD.X, PD.Y, 0.f), FRotator::ZeroRotator);
		if (!M) continue;
#if WITH_EDITOR
		M->SetActorLabel(*FString::Printf(TEXT("Mountain_%d"), P));
#endif

		const float H = PD.Height;
		const float RX = PD.RadX;
		const float RY = PD.RadY;

		const bool bVolcanic = (P >= VolcanoStartIdx && P <= VolcanoEndIdx);

		// Per-peak color jitter
		const float CJ = CityRandom.FRandRange(-0.03f, 0.03f);
		auto Jitter = [&](const FLinearColor& C) {
			return FLinearColor(
				FMath::Clamp(C.R + CJ, 0.f, 1.f),
				FMath::Clamp(C.G + CJ, 0.f, 1.f),
				FMath::Clamp(C.B + CJ, 0.f, 1.f));
		};

		// -------------------------------------------------------
		// A. BASE MASS — wide tapered cone, NOT a sphere
		// Cone mesh: 100 UU diameter at base, 100 UU tall, point at top
		// Scale: X/Y = diameter/100, Z = height/100
		// -------------------------------------------------------
		const float BaseDiamX = RX * 2.2f;
		const float BaseDiamY = RY * 2.2f;
		const FLinearColor BaseCol = bVolcanic ? Jitter(VolcanicRock) : Jitter(RockMid);
		UStaticMeshComponent* Root = MC_Cone(M,
			FVector(PD.X, PD.Y, 0.f),
			FVector(BaseDiamX / 100.f, BaseDiamY / 100.f, H * 0.85f / 100.f),
			FRotator::ZeroRotator,
			BaseCol, bVolcanic ? 0.95f : 0.92f, 0.05f, /*bCollision=*/true);
		M->SetRootComponent(Root);

		// -------------------------------------------------------
		// B. SUMMIT SPIRE — narrow steep cone on top for a sharp peak
		// -------------------------------------------------------
		const float SpireH = H * CityRandom.FRandRange(0.30f, 0.45f);
		const float SpireDiam = RX * CityRandom.FRandRange(0.5f, 0.7f);
		MC_Cone(M,
			FVector(PD.X + CityRandom.FRandRange(-RX * 0.05f, RX * 0.05f),
					PD.Y + CityRandom.FRandRange(-RY * 0.05f, RY * 0.05f),
					H * 0.65f),
			FVector(SpireDiam / 100.f, SpireDiam * 0.8f / 100.f, SpireH / 100.f),
			FRotator(CityRandom.FRandRange(-3.f, 3.f), CityRandom.FRandRange(0.f, 360.f), 0.f),
			Jitter(RockLight), 0.88f, 0.08f, /*bCollision=*/true);

		// -------------------------------------------------------
		// C. CLIFF FACES — 4-6 tall narrow cubes angled steeply
		// These break the smooth cone silhouette with jagged planes
		// -------------------------------------------------------
		const int32 NumCliffs = 4 + CityRandom.RandRange(0, 2);
		for (int32 C = 0; C < NumCliffs; ++C)
		{
			const float Yaw = CityRandom.FRandRange(0.f, 360.f);
			const float Rad = FMath::DegreesToRadians(Yaw);

			// Cliff positioned on the mountain face
			const float CDist = RX * CityRandom.FRandRange(0.2f, 0.55f);
			const float CX = PD.X + CDist * FMath::Cos(Rad);
			const float CY = PD.Y + CDist * FMath::Sin(Rad);
			const float CZ = H * CityRandom.FRandRange(0.15f, 0.55f);

			// Tall, thin slab — like an exposed rock face
			const float CW = RX * CityRandom.FRandRange(0.25f, 0.55f);
			const float CD = RX * CityRandom.FRandRange(0.04f, 0.12f); // very thin
			const float CH = H * CityRandom.FRandRange(0.30f, 0.60f);

			// Steep pitch to look like a cliff face leaning against the peak
			const float Pitch = CityRandom.FRandRange(8.f, 25.f);
			const float Roll = CityRandom.FRandRange(-5.f, 5.f);

			MC_Cube(M,
				FVector(CX, CY, CZ),
				FVector(CW / 100.f, CD / 100.f, CH / 100.f),
				FRotator(Pitch, Yaw + 90.f, Roll),
				Jitter(CliffFace), 0.95f, 0.08f);
		}

		// -------------------------------------------------------
		// D. ROCK OUTCROPS — angular cubes jutting out at various angles
		// Creates the craggy, irregular silhouette
		// -------------------------------------------------------
		const int32 NumOutcrops = 5 + CityRandom.RandRange(0, 3);
		for (int32 O = 0; O < NumOutcrops; ++O)
		{
			const float OYaw = CityRandom.FRandRange(0.f, 360.f);
			const float ORad = FMath::DegreesToRadians(OYaw);
			const float ODist = RX * CityRandom.FRandRange(0.3f, 0.8f);
			const float OX = PD.X + ODist * FMath::Cos(ORad);
			const float OY = PD.Y + ODist * FMath::Sin(ORad);
			const float OZ = H * CityRandom.FRandRange(0.05f, 0.65f);

			const float OW = RX * CityRandom.FRandRange(0.12f, 0.35f);
			const float OD = RY * CityRandom.FRandRange(0.10f, 0.30f);
			const float OH = H * CityRandom.FRandRange(0.10f, 0.30f);

			MC_Cube(M,
				FVector(OX, OY, OZ),
				FVector(OW / 100.f, OD / 100.f, OH / 100.f),
				FRotator(CityRandom.FRandRange(-20.f, 20.f),
						 OYaw,
						 CityRandom.FRandRange(-15.f, 15.f)),
				Jitter(O < 3 ? RockDark : RockLight), 0.93f, 0.06f);
		}

		// -------------------------------------------------------
		// E. SECONDARY PEAKS — 2-3 smaller cones flanking the main peak
		// Prevents the single-cone look, creates a rugged cluster
		// -------------------------------------------------------
		const int32 NumSecondary = 2 + CityRandom.RandRange(0, 1);
		for (int32 S = 0; S < NumSecondary; ++S)
		{
			const float SYaw = CityRandom.FRandRange(0.f, 360.f);
			const float SRad = FMath::DegreesToRadians(SYaw);
			const float SDist = RX * CityRandom.FRandRange(0.4f, 0.8f);
			const float SX = PD.X + SDist * FMath::Cos(SRad);
			const float SY = PD.Y + SDist * FMath::Sin(SRad);
			const float SH = H * CityRandom.FRandRange(0.35f, 0.65f);
			const float SDiam = RX * CityRandom.FRandRange(0.4f, 0.7f);

			MC_Cone(M,
				FVector(SX, SY, 0.f),
				FVector(SDiam / 100.f, SDiam * 0.85f / 100.f, SH / 100.f),
				FRotator(CityRandom.FRandRange(-4.f, 4.f), SYaw, CityRandom.FRandRange(-3.f, 3.f)),
				Jitter(RockMid), 0.90f, 0.05f, /*bCollision=*/true);

			// Sharp tip on secondary peaks
			MC_Cone(M,
				FVector(SX, SY, SH * 0.6f),
				FVector(SDiam * 0.35f / 100.f, SDiam * 0.30f / 100.f, SH * 0.35f / 100.f),
				FRotator(CityRandom.FRandRange(-6.f, 6.f), CityRandom.FRandRange(0.f, 360.f), 0.f),
				Jitter(RockLight), 0.88f, 0.07f);
		}

		// -------------------------------------------------------
		// F. RIDGELINE SPINES — thin vertical cubes along the peak crest
		// These create the jagged ridgeline silhouette when viewed from the side
		// -------------------------------------------------------
		const int32 NumSpines = 3 + CityRandom.RandRange(0, 2);
		const float SpineYaw = CityRandom.FRandRange(0.f, 180.f); // ridge direction
		for (int32 Sp = 0; Sp < NumSpines; ++Sp)
		{
			const float SpFrac = ((float)Sp / (float)(NumSpines - 1)) - 0.5f; // -0.5 to +0.5
			const float SpRad = FMath::DegreesToRadians(SpineYaw);
			const float SpDist = RX * 0.3f * SpFrac;
			const float SpX = PD.X + SpDist * FMath::Cos(SpRad);
			const float SpY = PD.Y + SpDist * FMath::Sin(SpRad);
			const float SpZ = H * CityRandom.FRandRange(0.50f, 0.80f);
			const float SpH = H * CityRandom.FRandRange(0.15f, 0.30f);
			const float SpW = RX * CityRandom.FRandRange(0.08f, 0.18f);
			const float SpD = RX * CityRandom.FRandRange(0.02f, 0.06f);

			MC_Cube(M,
				FVector(SpX, SpY, SpZ),
				FVector(SpW / 100.f, SpD / 100.f, SpH / 100.f),
				FRotator(CityRandom.FRandRange(-10.f, 10.f), SpineYaw, CityRandom.FRandRange(-8.f, 8.f)),
				Jitter(RockDark), 0.94f, 0.07f);
		}

		// -------------------------------------------------------
		// G. TALUS/SCREE SLOPES — angled debris fields at the base
		// Flattened cubes fanning out from the mountain base
		// -------------------------------------------------------
		const int32 NumTalus = 3 + CityRandom.RandRange(0, 2);
		for (int32 T = 0; T < NumTalus; ++T)
		{
			const float TYaw = CityRandom.FRandRange(0.f, 360.f);
			const float TRad = FMath::DegreesToRadians(TYaw);
			const float TDist = RX * CityRandom.FRandRange(0.7f, 1.2f);
			const float TX = PD.X + TDist * FMath::Cos(TRad);
			const float TY = PD.Y + TDist * FMath::Sin(TRad);

			const float TW = RX * CityRandom.FRandRange(0.4f, 0.8f);
			const float TD = RY * CityRandom.FRandRange(0.3f, 0.6f);
			const float TH = H * CityRandom.FRandRange(0.03f, 0.08f);

			// Tilted to slope away from mountain
			MC_Cube(M,
				FVector(TX, TY, TH * 0.5f),
				FVector(TW / 100.f, TD / 100.f, TH / 100.f),
				FRotator(CityRandom.FRandRange(5.f, 15.f), TYaw, 0.f),
				Jitter(Talus), 0.94f, 0.03f);
		}

		// -------------------------------------------------------
		// H. ALPINE MEADOWS — green patches at mid-low elevations
		// -------------------------------------------------------
		const int32 NumMeadows = 2 + CityRandom.RandRange(0, 2);
		for (int32 G = 0; G < NumMeadows; ++G)
		{
			const float GYaw = CityRandom.FRandRange(0.f, 360.f);
			const float GRad = FMath::DegreesToRadians(GYaw);
			const float GDist = RX * CityRandom.FRandRange(0.5f, 0.9f);
			const float GX = PD.X + GDist * FMath::Cos(GRad);
			const float GY = PD.Y + GDist * FMath::Sin(GRad);
			const float GZ = H * CityRandom.FRandRange(0.05f, 0.25f);

			const float GW = RX * CityRandom.FRandRange(0.3f, 0.6f);
			const float GD = RY * CityRandom.FRandRange(0.3f, 0.5f);

			MC_Cube(M,
				FVector(GX, GY, GZ),
				FVector(GW / 100.f, GD / 100.f, 0.15f),
				FRotator(CityRandom.FRandRange(5.f, 20.f), GYaw, 0.f),
				Jitter(Alpine), 0.92f, 0.f);
		}

		// -------------------------------------------------------
		// I. TREELINE PATCHES — dark forest green at lowest elevations
		// -------------------------------------------------------
		const int32 NumForest = 3 + CityRandom.RandRange(0, 2);
		for (int32 F = 0; F < NumForest; ++F)
		{
			const float FYaw = CityRandom.FRandRange(0.f, 360.f);
			const float FRad = FMath::DegreesToRadians(FYaw);
			const float FDist = RX * CityRandom.FRandRange(0.8f, 1.3f);
			const float FX = PD.X + FDist * FMath::Cos(FRad);
			const float FY = PD.Y + FDist * FMath::Sin(FRad);

			// Cone-shaped tree cluster
			const float FDiam = RX * CityRandom.FRandRange(0.2f, 0.45f);
			const float FH = H * CityRandom.FRandRange(0.04f, 0.12f);

			MC_Cone(M,
				FVector(FX, FY, 0.f),
				FVector(FDiam / 100.f, FDiam / 100.f, FH / 100.f),
				FRotator::ZeroRotator,
				Jitter(TreeLine), 0.90f, 0.f);
		}

		// -------------------------------------------------------
		// J. SNOW (northern tallest peaks only) or VOLCANO (southern peaks)
		// -------------------------------------------------------
		if (bVolcanic)
		{
			// ===== ACTIVE VOLCANO: crater, lava pool, lava flows, eruption =====

			// Crater rim — ring of dark rock at summit (hollow the top)
			const float CraterRim = RX * 0.55f;
			const float CraterH = H * 0.08f;
			MC_Cyl(M,
				FVector(PD.X, PD.Y, H * 0.78f),
				FVector(CraterRim / 50.f, CraterRim / 50.f, CraterH / 100.f),
				FRotator::ZeroRotator,
				Jitter(VolcanicDark), 0.96f, 0.03f);

			// Lava pool inside crater — bright glowing disc
			const float LavaPoolR = RX * 0.35f;
			UStaticMeshComponent* LavaPool = MC_Cyl(M,
				FVector(PD.X, PD.Y, H * 0.76f),
				FVector(LavaPoolR / 50.f, LavaPoolR / 50.f, 0.02f),
				FRotator::ZeroRotator,
				LavaBright, 0.20f, 0.1f);
			if (LavaPool)
			{
				ApplyPBR(LavaPool, LavaBright, 0.1f, 0.20f, LavaEmissive);
			}

			// 3-4 lava flows cascading down the mountain face into pools
			const int32 NumFlows = 3 + CityRandom.RandRange(0, 1);
			for (int32 LF = 0; LF < NumFlows; ++LF)
			{
				const float LFYaw = CityRandom.FRandRange(0.f, 360.f);
				const float LFRad = FMath::DegreesToRadians(LFYaw);

				// Flow starts near crater and runs down the slope
				const int32 NumSegments = 5 + CityRandom.RandRange(0, 2);
				for (int32 Seg = 0; Seg < NumSegments; ++Seg)
				{
					const float Frac = (float)Seg / (float)(NumSegments - 1);
					const float SegDist = RX * (0.15f + Frac * 0.90f);
					const float SegX = PD.X + SegDist * FMath::Cos(LFRad);
					const float SegY = PD.Y + SegDist * FMath::Sin(LFRad);
					const float SegZ = H * (0.70f - Frac * 0.68f); // flows all the way to base

					// Wider near the bottom (lava spreads as it cools)
					const float WidthMult = 1.f + Frac * 1.5f;
					const float SegW = RX * CityRandom.FRandRange(0.04f, 0.08f) * WidthMult;
					const float SegL = RX * CityRandom.FRandRange(0.10f, 0.20f);
					const float SegH = H * 0.015f;

					// Hotter near summit, cooler crust farther down
					const FLinearColor FlowEmissive = FLinearColor(
						FMath::Lerp(LavaEmissive.R, LavaCrustEmissive.R, Frac),
						FMath::Lerp(LavaEmissive.G, LavaCrustEmissive.G, Frac),
						FMath::Lerp(LavaEmissive.B, LavaCrustEmissive.B, Frac));
					const FLinearColor FlowCol = FLinearColor(
						FMath::Lerp(LavaBright.R, LavaGlow.R, Frac),
						FMath::Lerp(LavaBright.G, LavaGlow.G, Frac),
						FMath::Lerp(LavaBright.B, LavaGlow.B, Frac));

					UStaticMeshComponent* FlowSeg = MC_Cube(M,
						FVector(SegX, SegY, SegZ),
						FVector(SegL / 100.f, SegW / 100.f, SegH / 100.f),
						FRotator(CityRandom.FRandRange(10.f, 25.f), LFYaw + 90.f, 0.f),
						FlowCol, 0.25f, 0.05f);
					if (FlowSeg)
					{
						ApplyPBR(FlowSeg, FlowCol, 0.05f, 0.25f, FlowEmissive);
					}
				}

				// ---- Lava collection pool at the base of each flow ----
				const float PoolDist = RX * 1.05f + CityRandom.FRandRange(0.f, RX * 0.15f);
				const float PoolX = PD.X + PoolDist * FMath::Cos(LFRad);
				const float PoolY = PD.Y + PoolDist * FMath::Sin(LFRad);
				const float PoolZ = H * 0.02f; // at ground level
				const float PoolR = RX * CityRandom.FRandRange(0.25f, 0.40f);

				// Main pool disc — bright molten lava
				UStaticMeshComponent* FlowPool = MC_Cyl(M,
					FVector(PoolX, PoolY, PoolZ),
					FVector(PoolR / 50.f, PoolR / 50.f, 0.03f),
					FRotator::ZeroRotator,
					LavaBright, 0.15f, 0.05f);
				if (FlowPool)
				{
					ApplyPBR(FlowPool, LavaBright, 0.05f, 0.15f, LavaEmissive);
				}

				// Dark crusted rim around pool edge
				const float RimR = PoolR * 1.25f;
				UStaticMeshComponent* PoolRim = MC_Cyl(M,
					FVector(PoolX, PoolY, PoolZ - 2.f),
					FVector(RimR / 50.f, RimR / 50.f, 0.02f),
					FRotator::ZeroRotator,
					LavaGlow, 0.80f, 0.02f);
				if (PoolRim)
				{
					ApplyPBR(PoolRim, FLinearColor(0.15f, 0.06f, 0.02f), 0.02f, 0.80f, LavaCrustEmissive * 0.5f);
				}

				// Inner hot spot — smaller, brighter emissive center
				const float HotR = PoolR * 0.5f;
				const float HotJX = CityRandom.FRandRange(-PoolR * 0.15f, PoolR * 0.15f);
				const float HotJY = CityRandom.FRandRange(-PoolR * 0.15f, PoolR * 0.15f);
				UStaticMeshComponent* HotSpot = MC_Cyl(M,
					FVector(PoolX + HotJX, PoolY + HotJY, PoolZ + 1.f),
					FVector(HotR / 50.f, HotR / 50.f, 0.015f),
					FRotator::ZeroRotator,
					FLinearColor(1.f, 0.55f, 0.05f), 0.10f, 0.03f);
				if (HotSpot)
				{
					ApplyPBR(HotSpot, FLinearColor(1.f, 0.55f, 0.05f), 0.03f, 0.10f,
						FLinearColor(18.f, 5.f, 0.5f)); // extra bright hot center
				}

				// Pool light — strong ground-level glow that illuminates sky
				AActor* PoolLightActor = World->SpawnActor<AActor>(
					AActor::StaticClass(), FVector(PoolX, PoolY, PoolZ + 200.f), FRotator::ZeroRotator);
				if (PoolLightActor)
				{
					FString PLN = FString::Printf(TEXT("LavaPoolLight_%d_%d"), P, LF);
					UPointLightComponent* PLL = NewObject<UPointLightComponent>(PoolLightActor, *PLN);
					PLL->SetWorldLocation(FVector(PoolX, PoolY, PoolZ + 200.f));
					PLL->SetIntensity(80000.f);
					PLL->SetAttenuationRadius(25000.f);
					PLL->SetLightColor(FColor(255, 90, 15));
					PLL->SetCastShadows(false);
					PLL->RegisterComponent();
					PoolLightActor->SetRootComponent(PLL);
					PoolLightActor->AddInstanceComponent(PLL);
				}

				// Mid-flow light — boosted for sky visibility
				const float MidDist = RX * 0.5f;
				const FVector FlowLightPos(
					PD.X + MidDist * FMath::Cos(LFRad),
					PD.Y + MidDist * FMath::Sin(LFRad),
					H * 0.35f);
				AActor* FlowLightActor = World->SpawnActor<AActor>(
					AActor::StaticClass(), FlowLightPos, FRotator::ZeroRotator);
				if (FlowLightActor)
				{
					FString FLN = FString::Printf(TEXT("LavaFlowLight_%d_%d"), P, LF);
					UPointLightComponent* FLL = NewObject<UPointLightComponent>(FlowLightActor, *FLN);
					FLL->SetWorldLocation(FlowLightPos);
					FLL->SetIntensity(35000.f);
					FLL->SetAttenuationRadius(15000.f);
					FLL->SetLightColor(FColor(255, 100, 20));
					FLL->SetCastShadows(false);
					FLL->RegisterComponent();
					FlowLightActor->SetRootComponent(FLL);
					FlowLightActor->AddInstanceComponent(FLL);
				}
			}

			// ---- Large communal lava lake at volcano base ----
			// All flows converge into a larger molten lake on the downhill side
			{
				const float LakeYaw = CityRandom.FRandRange(90.f, 270.f); // generally south-facing (toward pirate ship)
				const float LakeRad = FMath::DegreesToRadians(LakeYaw);
				const float LakeDist = RX * 1.3f;
				const float LakeX = PD.X + LakeDist * FMath::Cos(LakeRad);
				const float LakeY = PD.Y + LakeDist * FMath::Sin(LakeRad);
				const float LakeZ = 5.f; // just above ground

				// Primary lake — large bright lava disc
				const float LakeR = RX * CityRandom.FRandRange(0.55f, 0.75f);
				UStaticMeshComponent* LakeMC = MC_Cyl(M,
					FVector(LakeX, LakeY, LakeZ),
					FVector(LakeR / 50.f, LakeR / 50.f, 0.04f),
					FRotator::ZeroRotator,
					LavaBright, 0.12f, 0.05f);
				if (LakeMC)
				{
					ApplyPBR(LakeMC, LavaBright, 0.05f, 0.12f, LavaEmissive);
				}

				// Darker crusted shoreline ring
				const float ShoreR = LakeR * 1.3f;
				UStaticMeshComponent* ShoreMC = MC_Cyl(M,
					FVector(LakeX, LakeY, LakeZ - 3.f),
					FVector(ShoreR / 50.f, ShoreR / 50.f, 0.03f),
					FRotator::ZeroRotator,
					FLinearColor(0.12f, 0.05f, 0.02f), 0.85f, 0.02f);
				if (ShoreMC)
				{
					ApplyPBR(ShoreMC, FLinearColor(0.12f, 0.05f, 0.02f), 0.02f, 0.85f, LavaCrustEmissive * 0.4f);
				}

				// 2-3 bright hotspots within the lake
				for (int32 HS = 0; HS < 2 + CityRandom.RandRange(0, 1); ++HS)
				{
					const float HSR = LakeR * CityRandom.FRandRange(0.15f, 0.30f);
					const float HSAngle = CityRandom.FRandRange(0.f, 360.f);
					const float HSDist = CityRandom.FRandRange(0.f, LakeR * 0.5f);
					const float HSX = LakeX + HSDist * FMath::Cos(FMath::DegreesToRadians(HSAngle));
					const float HSY = LakeY + HSDist * FMath::Sin(FMath::DegreesToRadians(HSAngle));

					UStaticMeshComponent* HSMC = MC_Cyl(M,
						FVector(HSX, HSY, LakeZ + 2.f),
						FVector(HSR / 50.f, HSR / 50.f, 0.015f),
						FRotator::ZeroRotator,
						FLinearColor(1.f, 0.6f, 0.08f), 0.08f, 0.02f);
					if (HSMC)
					{
						ApplyPBR(HSMC, FLinearColor(1.f, 0.6f, 0.08f), 0.02f, 0.08f,
							FLinearColor(20.f, 6.f, 0.6f)); // blazing hot
					}
				}

				// Solidified lava rock islands in the lake
				for (int32 RI = 0; RI < 2; ++RI)
				{
					const float RIAngle = CityRandom.FRandRange(0.f, 360.f);
					const float RIDist = CityRandom.FRandRange(LakeR * 0.2f, LakeR * 0.7f);
					const float RIX = LakeX + RIDist * FMath::Cos(FMath::DegreesToRadians(RIAngle));
					const float RIY = LakeY + RIDist * FMath::Sin(FMath::DegreesToRadians(RIAngle));
					const float RIR = LakeR * CityRandom.FRandRange(0.06f, 0.12f);

					FString RIN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
					UStaticMeshComponent* RockMC = NewObject<UStaticMeshComponent>(M, *RIN);
					RockMC->SetStaticMesh(SphereMesh);
					RockMC->SetWorldScale3D(FVector(RIR / 50.f, RIR / 50.f, RIR * 0.5f / 50.f));
					RockMC->SetWorldLocation(FVector(RIX, RIY, LakeZ + 8.f));
					RockMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					RockMC->SetBoundsScale(10.f);
					RockMC->RegisterComponent();
					ApplyPBR(RockMC, VolcanicDark, 0.02f, 0.95f);
					M->AddInstanceComponent(RockMC);
				}

				// Lake main light — massive intensity to illuminate the night sky
				AActor* LakeLightActor = World->SpawnActor<AActor>(
					AActor::StaticClass(), FVector(LakeX, LakeY, LakeZ + 500.f), FRotator::ZeroRotator);
				if (LakeLightActor)
				{
					FString LLN = FString::Printf(TEXT("LavaLakeLight_%d"), P);
					UPointLightComponent* LLL = NewObject<UPointLightComponent>(LakeLightActor, *LLN);
					LLL->SetWorldLocation(FVector(LakeX, LakeY, LakeZ + 500.f));
					LLL->SetIntensity(120000.f);
					LLL->SetAttenuationRadius(40000.f);
					LLL->SetLightColor(FColor(255, 85, 12));
					LLL->SetCastShadows(false);
					LLL->RegisterComponent();
					LakeLightActor->SetRootComponent(LLL);
					LakeLightActor->AddInstanceComponent(LLL);
				}
			}

			// Main crater light — strong orange-red glow illuminating the sky
			const FVector CraterLightPos(PD.X, PD.Y, H * 0.85f);
			AActor* CraterLightActor = World->SpawnActor<AActor>(
				AActor::StaticClass(), CraterLightPos, FRotator::ZeroRotator);
			if (CraterLightActor)
			{
				FString CLN = FString::Printf(TEXT("VolcanoCraterLight_%d"), P);
				UPointLightComponent* CL = NewObject<UPointLightComponent>(CraterLightActor, *CLN);
				CL->SetWorldLocation(CraterLightPos);
				CL->SetIntensity(100000.f);
				CL->SetAttenuationRadius(35000.f);
				CL->SetLightColor(FColor(255, 80, 10));
				CL->SetCastShadows(false);
				CL->RegisterComponent();
				CraterLightActor->SetRootComponent(CL);
				CraterLightActor->AddInstanceComponent(CL);
			}

			// Sky-glow light — elevated high above the volcano to cast warm light on clouds/sky
			AActor* SkyGlowActor = World->SpawnActor<AActor>(
				AActor::StaticClass(), FVector(PD.X, PD.Y, H * 1.5f), FRotator::ZeroRotator);
			if (SkyGlowActor)
			{
				FString SGN = FString::Printf(TEXT("VolcanoSkyGlow_%d"), P);
				UPointLightComponent* SGL = NewObject<UPointLightComponent>(SkyGlowActor, *SGN);
				SGL->SetWorldLocation(FVector(PD.X, PD.Y, H * 1.5f));
				SGL->SetIntensity(60000.f);
				SGL->SetAttenuationRadius(50000.f);
				SGL->SetLightColor(FColor(255, 65, 8));
				SGL->SetCastShadows(false);
				SGL->RegisterComponent();
				SkyGlowActor->SetRootComponent(SGL);
				SkyGlowActor->AddInstanceComponent(SGL);
			}

			// Eruption plume — stacked spheres above crater (smoke + lava bombs)
			// Only on front-row volcanic peaks (taller ones)
			if (P >= VolcanoStartIdx && P <= 32)
			{
				// Smoke column — translucent dark spheres rising above crater
				for (int32 Sm = 0; Sm < 5; ++Sm)
				{
					const float SmZ = H * 0.85f + Sm * H * 0.12f;
					const float SmR = RX * (0.2f + Sm * 0.08f); // expands upward
					const float SmJX = CityRandom.FRandRange(-RX * 0.1f, RX * 0.1f);
					const float SmJY = CityRandom.FRandRange(-RY * 0.1f, RY * 0.1f);

					// Use sphere mesh for smoke puffs
					FString SmN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
					UStaticMeshComponent* SmokeMC = NewObject<UStaticMeshComponent>(M, *SmN);
					SmokeMC->SetStaticMesh(SphereMesh);
					SmokeMC->SetWorldScale3D(FVector(SmR / 50.f, SmR / 50.f, SmR * 0.6f / 50.f));
					SmokeMC->SetWorldLocation(FVector(PD.X + SmJX, PD.Y + SmJY, SmZ));
					SmokeMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					SmokeMC->SetBoundsScale(10.f);
					SmokeMC->RegisterComponent();
					// Dark ash color with subtle warm emissive from below
					const float SmEmissive = FMath::Max(0.f, 1.f - Sm * 0.25f);
					ApplyPBR(SmokeMC, AshGray, 0.f, 0.95f,
						FLinearColor(SmEmissive * 2.f, SmEmissive * 0.4f, SmEmissive * 0.05f));
					M->AddInstanceComponent(SmokeMC);
				}

				// Lava bombs — small bright spheres scattered in eruption column
				for (int32 LB = 0; LB < 4; ++LB)
				{
					const float LBZ = H * CityRandom.FRandRange(0.90f, 1.30f);
					const float LBR = RX * CityRandom.FRandRange(0.03f, 0.06f);
					const float LBX = PD.X + CityRandom.FRandRange(-RX * 0.25f, RX * 0.25f);
					const float LBY = PD.Y + CityRandom.FRandRange(-RY * 0.25f, RY * 0.25f);

					FString LBN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
					UStaticMeshComponent* BombMC = NewObject<UStaticMeshComponent>(M, *LBN);
					BombMC->SetStaticMesh(SphereMesh);
					BombMC->SetWorldScale3D(FVector(LBR / 50.f));
					BombMC->SetWorldLocation(FVector(LBX, LBY, LBZ));
					BombMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					BombMC->SetBoundsScale(10.f);
					BombMC->RegisterComponent();
					ApplyPBR(BombMC, LavaBright, 0.1f, 0.15f, LavaEmissive);
					M->AddInstanceComponent(BombMC);
				}
			}

			// Ash deposits at base — dark gray patches instead of green
			const int32 NumAsh = 3 + CityRandom.RandRange(0, 2);
			for (int32 A = 0; A < NumAsh; ++A)
			{
				const float AYaw = CityRandom.FRandRange(0.f, 360.f);
				const float ARad = FMath::DegreesToRadians(AYaw);
				const float ADist = RX * CityRandom.FRandRange(0.8f, 1.3f);

				MC_Cube(M,
					FVector(PD.X + ADist * FMath::Cos(ARad),
							PD.Y + ADist * FMath::Sin(ARad),
							H * 0.02f),
					FVector(RX * 0.4f / 100.f, RY * 0.3f / 100.f, 0.10f),
					FRotator(CityRandom.FRandRange(3.f, 12.f), AYaw, 0.f),
					Jitter(AshGray), 0.96f, 0.f);
			}
		}
		else
		{
		bool bSnow = false;
		for (int32 Si = 0; Si < NumSnow; ++Si)
		{
			if (SnowIdx[Si] == P) { bSnow = true; break; }
		}
		if (bSnow)
		{
			// Main snow mantle — cone draped over the peak
			const float SnowH = H * 0.30f;
			const float SnowDiam = RX * 0.9f;
			MC_Cone(M,
				FVector(PD.X, PD.Y, H * 0.60f),
				FVector(SnowDiam / 100.f, SnowDiam * 0.85f / 100.f, SnowH / 100.f),
				FRotator::ZeroRotator,
				SnowFresh, 0.55f, 0.f);

			// 3-4 irregular snow patches cascading down different faces
			const int32 NumSnowPatches = 3 + CityRandom.RandRange(0, 1);
			for (int32 SP = 0; SP < NumSnowPatches; ++SP)
			{
				const float SPYaw = CityRandom.FRandRange(0.f, 360.f);
				const float SPRad = FMath::DegreesToRadians(SPYaw);
				const float SPDist = RX * CityRandom.FRandRange(0.15f, 0.4f);
				const float SPX = PD.X + SPDist * FMath::Cos(SPRad);
				const float SPY = PD.Y + SPDist * FMath::Sin(SPRad);
				const float SPZ = H * CityRandom.FRandRange(0.45f, 0.70f);

				const float SPW = RX * CityRandom.FRandRange(0.2f, 0.4f);
				const float SPD = RY * CityRandom.FRandRange(0.15f, 0.35f);
				const float SPH = H * CityRandom.FRandRange(0.08f, 0.18f);

				MC_Cube(M,
					FVector(SPX, SPY, SPZ),
					FVector(SPW / 100.f, SPD / 100.f, SPH / 100.f),
					FRotator(CityRandom.FRandRange(10.f, 30.f), SPYaw, 0.f),
					(SP % 2 == 0) ? SnowFresh : SnowOld, 0.55f, 0.f);
			}

			// Snow-covered summit tip
			MC_Cone(M,
				FVector(PD.X, PD.Y, H * 0.82f),
				FVector(RX * 0.25f / 100.f, RY * 0.22f / 100.f, H * 0.18f / 100.f),
				FRotator::ZeroRotator,
				SnowFresh, 0.50f, 0.f);
		}
		} // end snow vs volcano
	}

	// ================================================================
	// RIDGE CONNECTORS — jagged ridgeline between adjacent front-row peaks
	// ================================================================
	for (int32 R = 0; R < 16; ++R)
	{
		const FPeakData& PA = Peaks[R];
		const FPeakData& PB = Peaks[R + 1];

		const float MidX = (PA.X + PB.X) * 0.5f;
		const float MidY = (PA.Y + PB.Y) * 0.5f;
		const float MinH = FMath::Min(PA.Height, PB.Height);
		const float Dist = FMath::Sqrt(FMath::Square(PB.X - PA.X) + FMath::Square(PB.Y - PA.Y));
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(PB.Y - PA.Y, PB.X - PA.X));

		AActor* RA = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(MidX, MidY, 0.f), FRotator::ZeroRotator);
		if (!RA) continue;
#if WITH_EDITOR
		RA->SetActorLabel(*FString::Printf(TEXT("Ridge_%d_%d"), R, R + 1));
#endif

		// Main ridge body — elongated cone
		const float RH = MinH * CityRandom.FRandRange(0.25f, 0.40f);
		const float RW = FMath::Min(PA.RadY, PB.RadY) * 0.5f;
		UStaticMeshComponent* RidgeRoot = MC_Cone(RA,
			FVector(MidX, MidY, 0.f),
			FVector(Dist * 0.55f / 100.f, RW / 100.f, RH / 100.f),
			FRotator(0.f, Yaw, 0.f),
			RidgeRock, 0.93f, 0.04f, /*bCollision=*/true);
		RA->SetRootComponent(RidgeRoot);

		// 3-5 jagged teeth along the ridge crest
		const int32 NumTeeth = 3 + CityRandom.RandRange(0, 2);
		for (int32 T = 0; T < NumTeeth; ++T)
		{
			const float TFrac = ((float)T / (float)(NumTeeth - 1)) - 0.5f;
			const float TRad = FMath::DegreesToRadians(Yaw);
			const float TDist = Dist * 0.4f * TFrac;
			const float TX = MidX + TDist * FMath::Cos(TRad);
			const float TY = MidY + TDist * FMath::Sin(TRad);
			const float TH = RH * CityRandom.FRandRange(0.4f, 0.9f);
			const float TW = RW * CityRandom.FRandRange(0.3f, 0.6f);

			MC_Cube(RA,
				FVector(TX, TY, RH * 0.3f + TH * 0.3f),
				FVector(TW * 0.6f / 100.f, TW * 0.15f / 100.f, TH / 100.f),
				FRotator(CityRandom.FRandRange(-10.f, 10.f), Yaw + CityRandom.FRandRange(-20.f, 20.f),
						 CityRandom.FRandRange(-8.f, 8.f)),
				FLinearColor(RidgeRock.R + CityRandom.FRandRange(-0.04f, 0.04f),
							 RidgeRock.G + CityRandom.FRandRange(-0.04f, 0.04f),
							 RidgeRock.B + CityRandom.FRandRange(-0.04f, 0.04f)),
				0.94f, 0.05f);
		}

		// Talus at ridge base
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float SideDir = (Side == 0) ? 1.f : -1.f;
			const float PerpdRad = FMath::DegreesToRadians(Yaw + 90.f);
			const float SX = MidX + SideDir * RW * 0.8f * FMath::Cos(PerpdRad);
			const float SY = MidY + SideDir * RW * 0.8f * FMath::Sin(PerpdRad);

			MC_Cube(RA,
				FVector(SX, SY, RH * 0.1f),
				FVector(Dist * 0.3f / 100.f, RW * 0.5f / 100.f, RH * 0.15f / 100.f),
				FRotator(SideDir * 8.f, Yaw, 0.f),
				Talus, 0.94f, 0.03f);
		}
	}

	// ================================================================
	// RIDGE CONNECTORS — southern mountain range
	// ================================================================
	for (int32 R = 23; R < 32; ++R)
	{
		const FPeakData& PA = Peaks[R];
		const FPeakData& PB = Peaks[R + 1];

		const float MidX = (PA.X + PB.X) * 0.5f;
		const float MidY = (PA.Y + PB.Y) * 0.5f;
		const float MinH = FMath::Min(PA.Height, PB.Height);
		const float Dist = FMath::Sqrt(FMath::Square(PB.X - PA.X) + FMath::Square(PB.Y - PA.Y));
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(PB.Y - PA.Y, PB.X - PA.X));

		AActor* RA = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(MidX, MidY, 0.f), FRotator::ZeroRotator);
		if (!RA) continue;
#if WITH_EDITOR
		RA->SetActorLabel(*FString::Printf(TEXT("SouthRidge_%d_%d"), R, R + 1));
#endif

		const float RH = MinH * CityRandom.FRandRange(0.25f, 0.40f);
		const float RW = FMath::Min(PA.RadY, PB.RadY) * 0.5f;
		UStaticMeshComponent* RidgeRoot = MC_Cone(RA,
			FVector(MidX, MidY, 0.f),
			FVector(Dist * 0.55f / 100.f, RW / 100.f, RH / 100.f),
			FRotator(0.f, Yaw, 0.f),
			RidgeRock, 0.93f, 0.04f, /*bCollision=*/true);
		RA->SetRootComponent(RidgeRoot);

		const int32 NumTeeth = 3 + CityRandom.RandRange(0, 2);
		for (int32 T = 0; T < NumTeeth; ++T)
		{
			const float TFrac = ((float)T / (float)(NumTeeth - 1)) - 0.5f;
			const float TRad = FMath::DegreesToRadians(Yaw);
			const float TDist = Dist * 0.4f * TFrac;
			const float TX = MidX + TDist * FMath::Cos(TRad);
			const float TY = MidY + TDist * FMath::Sin(TRad);
			const float TH = RH * CityRandom.FRandRange(0.4f, 0.9f);
			const float TW = RW * CityRandom.FRandRange(0.3f, 0.6f);

			MC_Cube(RA,
				FVector(TX, TY, RH * 0.3f + TH * 0.3f),
				FVector(TW * 0.6f / 100.f, TW * 0.15f / 100.f, TH / 100.f),
				FRotator(CityRandom.FRandRange(-10.f, 10.f), Yaw + CityRandom.FRandRange(-20.f, 20.f),
						 CityRandom.FRandRange(-8.f, 8.f)),
				FLinearColor(RidgeRock.R + CityRandom.FRandRange(-0.04f, 0.04f),
							 RidgeRock.G + CityRandom.FRandRange(-0.04f, 0.04f),
							 RidgeRock.B + CityRandom.FRandRange(-0.04f, 0.04f)),
				0.94f, 0.05f);
		}

		for (int32 Side = 0; Side < 2; ++Side)
		{
			const float SideDir = (Side == 0) ? 1.f : -1.f;
			const float PerpdRad = FMath::DegreesToRadians(Yaw + 90.f);
			const float SX = MidX + SideDir * RW * 0.8f * FMath::Cos(PerpdRad);
			const float SY = MidY + SideDir * RW * 0.8f * FMath::Sin(PerpdRad);

			MC_Cube(RA,
				FVector(SX, SY, RH * 0.1f),
				FVector(Dist * 0.3f / 100.f, RW * 0.5f / 100.f, RH * 0.15f / 100.f),
				FRotator(SideDir * 8.f, Yaw, 0.f),
				Talus, 0.94f, 0.03f);
		}
	}

	// ================================================================
	// SNOWFALL & GROUND ACCUMULATION — northern mountain range
	// Visible snowfall (suspended white spheres) and snow ground cover
	// ================================================================
	{
		const float SnowMinX = -95000.f;
		const float SnowMaxX =  96000.f;
		const float SnowMinY =  85000.f;  // extends south of peaks for snow field
		const float SnowMaxY = 115000.f;  // extends north behind peaks

		AActor* SnowActor = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(0.f, 100000.f, 0.f), FRotator::ZeroRotator);
		if (SnowActor)
		{
#if WITH_EDITOR
			SnowActor->SetActorLabel(TEXT("NorthernSnowfall"));
#endif
			int32 SnowCompIdx = 0;

			// --- Snowfall particles: small white spheres scattered in the air ---
			// Dense snowfall with ~400 flakes across the region
			for (int32 SF = 0; SF < 400; ++SF)
			{
				const float SX = CityRandom.FRandRange(SnowMinX, SnowMaxX);
				const float SY = CityRandom.FRandRange(SnowMinY, SnowMaxY);
				const float SZ = CityRandom.FRandRange(200.f, 18000.f); // from near ground to above peaks
				const float SR = CityRandom.FRandRange(15.f, 45.f);     // snowflake size

				FString SFN = FString::Printf(TEXT("Snowflake_%d"), SnowCompIdx++);
				UStaticMeshComponent* Flake = NewObject<UStaticMeshComponent>(SnowActor, *SFN);
				Flake->SetStaticMesh(SphereMesh);
				Flake->SetWorldScale3D(FVector(SR / 50.f, SR / 50.f, SR * 0.6f / 50.f)); // slightly flattened
				Flake->SetWorldLocation(FVector(SX, SY, SZ));
				Flake->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Flake->SetBoundsScale(10.f);
				Flake->RegisterComponent();
				// Bright white with slight translucency feel via low roughness
				const float WhiteVar = CityRandom.FRandRange(0.88f, 0.98f);
				ApplyPBR(Flake, FLinearColor(WhiteVar, WhiteVar, WhiteVar + 0.02f), 0.f, 0.4f);
				SnowActor->AddInstanceComponent(Flake);
			}

			// --- Ground snow accumulation: white patches covering terrain ---
			// Large flat cubes to simulate snow cover on the ground between peaks
			for (int32 GA = 0; GA < 60; ++GA)
			{
				const float GX = CityRandom.FRandRange(SnowMinX, SnowMaxX);
				const float GY = CityRandom.FRandRange(SnowMinY, SnowMaxY);
				const float GW = CityRandom.FRandRange(2000.f, 6000.f);
				const float GD = CityRandom.FRandRange(1500.f, 5000.f);
				const float GH = CityRandom.FRandRange(8.f, 25.f);   // thin snow layer
				const float GYaw = CityRandom.FRandRange(0.f, 360.f);

				FString GAN = FString::Printf(TEXT("GroundSnow_%d"), SnowCompIdx++);
				UStaticMeshComponent* SnowPatch = NewObject<UStaticMeshComponent>(SnowActor, *GAN);
				SnowPatch->SetStaticMesh(CubeMesh);
				SnowPatch->SetWorldScale3D(FVector(GW / 100.f, GD / 100.f, GH / 100.f));
				SnowPatch->SetWorldLocation(FVector(GX, GY, GH * 0.5f));
				SnowPatch->SetWorldRotation(FRotator(CityRandom.FRandRange(-3.f, 3.f), GYaw, 0.f));
				SnowPatch->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SnowPatch->SetBoundsScale(10.f);
				SnowPatch->RegisterComponent();
				const bool bFreshSnow = (GA % 3 != 0);
				ApplyPBR(SnowPatch, bFreshSnow ? SnowFresh : SnowOld, 0.f, 0.55f);
				SnowActor->AddInstanceComponent(SnowPatch);
			}

			// --- Snow drifts along mountain bases: elongated mounds ---
			for (int32 P = 0; P < 23; ++P) // northern peaks only
			{
				const FPeakData& PD = Peaks[P];
				const int32 NumDrifts = 4 + CityRandom.RandRange(0, 3);
				for (int32 D = 0; D < NumDrifts; ++D)
				{
					const float DYaw = CityRandom.FRandRange(0.f, 360.f);
					const float DRad = FMath::DegreesToRadians(DYaw);
					const float DDist = PD.RadX * CityRandom.FRandRange(0.7f, 1.4f);
					const float DX = PD.X + DDist * FMath::Cos(DRad);
					const float DY = PD.Y + DDist * FMath::Sin(DRad);
					const float DW = CityRandom.FRandRange(800.f, 2500.f);
					const float DD = CityRandom.FRandRange(400.f, 1200.f);
					const float DH = CityRandom.FRandRange(50.f, 200.f);

					FString DDN = FString::Printf(TEXT("SnowDrift_%d_%d"), P, D);
					UStaticMeshComponent* Drift = NewObject<UStaticMeshComponent>(SnowActor, *DDN);
					Drift->SetStaticMesh(SphereMesh); // rounded snow drift
					Drift->SetWorldScale3D(FVector(DW / 50.f, DD / 50.f, DH / 50.f));
					Drift->SetWorldLocation(FVector(DX, DY, DH * 0.3f));
					Drift->SetWorldRotation(FRotator(0.f, DYaw, 0.f));
					Drift->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Drift->SetBoundsScale(10.f);
					Drift->RegisterComponent();
					ApplyPBR(Drift, (D % 2 == 0) ? SnowFresh : SnowOld, 0.f, 0.50f);
					SnowActor->AddInstanceComponent(Drift);
				}
			}

			// --- Dense snowfall clusters at lower altitudes (thicker near ground) ---
			for (int32 CF = 0; CF < 150; ++CF)
			{
				const float CX = CityRandom.FRandRange(SnowMinX, SnowMaxX);
				const float CY = CityRandom.FRandRange(SnowMinY, SnowMaxY);
				const float CZ = CityRandom.FRandRange(50.f, 3000.f); // near ground level
				const float CR = CityRandom.FRandRange(20.f, 60.f);

				FString CFN = FString::Printf(TEXT("LowSnow_%d"), SnowCompIdx++);
				UStaticMeshComponent* LowFlake = NewObject<UStaticMeshComponent>(SnowActor, *CFN);
				LowFlake->SetStaticMesh(SphereMesh);
				LowFlake->SetWorldScale3D(FVector(CR / 50.f, CR / 50.f, CR * 0.4f / 50.f));
				LowFlake->SetWorldLocation(FVector(CX, CY, CZ));
				LowFlake->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				LowFlake->SetBoundsScale(10.f);
				LowFlake->RegisterComponent();
				ApplyPBR(LowFlake, FLinearColor(0.95f, 0.96f, 0.98f), 0.f, 0.35f);
				SnowActor->AddInstanceComponent(LowFlake);
			}
		}
	}

	// ================================================================
	// MAGICAL CHRISTMAS WONDERLAND — expansive area beside snowy mountains
	// ================================================================
	{
		// Greatly expanded Christmas area covering a huge zone south of mountains
		const float XmasMinX = -95000.f;
		const float XmasMaxX =  95000.f;
		const float XmasMinY =  62000.f;
		const float XmasMaxY =  95000.f;

		// --- MASSIVE SNOW GROUND COVER ---
		AActor* SnowGround = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(0.f, 78000.f, 0.f), FRotator::ZeroRotator);
		if (SnowGround)
		{
#if WITH_EDITOR
			SnowGround->SetActorLabel(TEXT("XmasSnowGround"));
#endif
			// Giant snow blanket covering the entire wonderland
			UStaticMeshComponent* SGRoot = MC_Cube(SnowGround,
				FVector(0.f, 78000.f, -5.f),
				FVector(2000.f, 400.f, 0.15f),
				FRotator::ZeroRotator,
				SnowFresh, 0.50f, 0.f);
			SnowGround->SetRootComponent(SGRoot);

			// Many overlapping snow patches for full coverage
			for (int32 S = 0; S < 80; ++S)
			{
				const float SX = CityRandom.FRandRange(XmasMinX - 5000.f, XmasMaxX + 5000.f);
				const float SY = CityRandom.FRandRange(XmasMinY - 3000.f, XmasMaxY + 3000.f);
				const float SSize = CityRandom.FRandRange(40.f, 120.f);
				MC_Cube(SnowGround,
					FVector(SX, SY, -3.f),
					FVector(SSize, SSize * CityRandom.FRandRange(0.6f, 1.4f), 0.18f),
					FRotator(0.f, CityRandom.FRandRange(0.f, 360.f), 0.f),
					SnowFresh, 0.50f, 0.f);
			}

			// Snow drifts — rounded mounds
			for (int32 D = 0; D < 50; ++D)
			{
				const float DX = CityRandom.FRandRange(XmasMinX, XmasMaxX);
				const float DY = CityRandom.FRandRange(XmasMinY, XmasMaxY);
				const float DR = CityRandom.FRandRange(200.f, 800.f);
				const float DH = CityRandom.FRandRange(30.f, 120.f);

				FString DN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
				UStaticMeshComponent* Drift = NewObject<UStaticMeshComponent>(SnowGround, *DN);
				Drift->SetStaticMesh(SphereMesh);
				Drift->SetWorldLocation(FVector(DX, DY, DH * 0.3f));
				Drift->SetWorldScale3D(FVector(DR / 50.f, DR * CityRandom.FRandRange(0.6f, 1.2f) / 50.f, DH / 50.f));
				Drift->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Drift->SetBoundsScale(10.f);
				Drift->RegisterComponent();
				ApplyPBR(Drift, SnowFresh, 0.48f, 0.f);
				SnowGround->AddInstanceComponent(Drift);
			}
		}

		// ---- GIANT CHRISTMAS TREES ----
		// 24 giant trees spread across the expanded area
		struct FXmasTreeDef { float X; float Y; float Height; };
		const FXmasTreeDef Trees[] = {
			{ -90000.f, 70000.f, 9000.f },
			{ -75000.f, 85000.f, 8000.f },
			{ -72000.f, 68000.f, 7000.f },
			{ -55000.f, 78000.f, 10000.f },
			{ -50000.f, 65000.f, 8500.f },
			{ -38000.f, 88000.f, 7500.f },
			{ -30000.f, 72000.f, 11500.f },
			{ -20000.f, 84000.f, 11000.f },
			{ -12000.f, 66000.f, 9500.f },
			{  -5000.f, 90000.f, 9000.f },
			{   5000.f, 73000.f, 10000.f },
			{  12000.f, 83000.f, 8500.f },
			{  20000.f, 67000.f, 12000.f },
			{  28000.f, 87000.f, 12000.f },
			{  35000.f, 72000.f, 8000.f },
			{  45000.f, 81000.f, 9500.f },
			{  55000.f, 66000.f, 10500.f },
			{  60000.f, 89000.f, 7000.f },
			{  68000.f, 74000.f, 9000.f },
			{  75000.f, 84000.f, 10500.f },
			{  85000.f, 69000.f, 7500.f },
			{ -48000.f, 91000.f, 6500.f },
			{  38000.f, 92000.f, 7500.f },
			{ -85000.f, 78000.f, 8000.f },
		};
		const int32 NumXmasTrees = UE_ARRAY_COUNT(Trees);

		// Christmas colors
		const FLinearColor TreeGreenDark(0.02f, 0.14f, 0.03f);
		const FLinearColor TreeGreenLight(0.05f, 0.20f, 0.06f);
		const FLinearColor TrunkBrown(0.22f, 0.14f, 0.07f);
		const FLinearColor StarGold(1.0f, 0.85f, 0.1f);

		// Christmas light colors (emissive)
		const FLinearColor LightRed(3.0f, 0.1f, 0.05f);
		const FLinearColor LightGreen(0.1f, 3.0f, 0.15f);
		const FLinearColor LightBlue(0.1f, 0.3f, 3.5f);
		const FLinearColor LightGold(3.0f, 2.2f, 0.3f);
		const FLinearColor LightWhite(3.0f, 3.0f, 3.5f);
		const FLinearColor LightPurple(2.0f, 0.2f, 3.0f);
		const FLinearColor BulbColors[] = { LightRed, LightGreen, LightBlue, LightGold, LightWhite, LightPurple };
		const int32 NumBulbColors = UE_ARRAY_COUNT(BulbColors);

		for (int32 T = 0; T < NumXmasTrees; ++T)
		{
			const FXmasTreeDef& TD = Trees[T];
			const float TH = TD.Height;
			const float BaseRadius = TH * 0.35f;  // wide conical tree

			AActor* TA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(TD.X, TD.Y, 0.f), FRotator::ZeroRotator);
			if (!TA) continue;
#if WITH_EDITOR
			TA->SetActorLabel(*FString::Printf(TEXT("XmasTree_%d"), T));
#endif

			// A. TRUNK — thick brown cylinder
			const float TrunkH = TH * 0.12f;
			const float TrunkR = TH * 0.04f;
			UStaticMeshComponent* TrunkComp = MC_Cyl(TA,
				FVector(TD.X, TD.Y, TrunkH * 0.5f),
				FVector(TrunkR / 50.f, TrunkR / 50.f, TrunkH / 100.f),
				FRotator::ZeroRotator,
				TrunkBrown, 0.85f, 0.f);
			TA->SetRootComponent(TrunkComp);

			// B. FOLIAGE TIERS — 4 stacked cones getting smaller toward top
			const int32 NumTiers = 4;
			for (int32 Tier = 0; Tier < NumTiers; ++Tier)
			{
				const float TierFrac = (float)Tier / (float)NumTiers;
				const float TierZ = TrunkH + TH * TierFrac * 0.85f;
				const float TierRadius = BaseRadius * (1.0f - TierFrac * 0.75f);
				const float TierH = TH * 0.35f * (1.0f - TierFrac * 0.3f);
				const FLinearColor TierCol = (Tier % 2 == 0) ? TreeGreenDark : TreeGreenLight;

				MC_Cone(TA,
					FVector(TD.X, TD.Y, TierZ),
					FVector(TierRadius / 50.f, TierRadius / 50.f, TierH / 100.f),
					FRotator::ZeroRotator,
					TierCol, 0.80f, 0.f);
			}

			// C. STAR ON TOP — bright golden sphere with strong emissive
			{
				const float StarZ = TrunkH + TH * 0.92f;
				const float StarSize = TH * 0.06f;
				FString SN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
				UStaticMeshComponent* StarComp = NewObject<UStaticMeshComponent>(TA, *SN);
				StarComp->SetStaticMesh(SphereMesh);
				StarComp->SetWorldLocation(FVector(TD.X, TD.Y, StarZ));
				StarComp->SetWorldScale3D(FVector(StarSize / 50.f));
				StarComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				StarComp->SetBoundsScale(10.f);
				StarComp->RegisterComponent();
				TA->AddInstanceComponent(StarComp);
				// Gold with strong emissive glow
				ApplyPBR(StarComp, StarGold, 0.8f, 0.3f);
				if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(StarComp->GetMaterial(0)))
				{
					MI->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(8.0f, 6.5f, 1.0f));
				}

				// Point light at star
				FString LN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
				UPointLightComponent* StarLight = NewObject<UPointLightComponent>(TA, *LN);
				StarLight->SetWorldLocation(FVector(TD.X, TD.Y, StarZ));
				StarLight->SetIntensity(15000.f);
				StarLight->SetAttenuationRadius(TH * 1.5f);
				StarLight->SetLightColor(FColor(255, 220, 80));
				StarLight->SetCastShadows(false);
				StarLight->RegisterComponent();
				TA->AddInstanceComponent(StarLight);
			}

			// D. CHRISTMAS LIGHTS — glowing ornament spheres spiraling around tree
			const int32 NumLights = 30 + CityRandom.RandRange(0, 15);
			for (int32 L = 0; L < NumLights; ++L)
			{
				const float LFrac = (float)L / (float)NumLights;
				const float LZ = TrunkH + TH * 0.05f + LFrac * TH * 0.85f;
				// Spiral pattern
				const float Angle = LFrac * 8.0f * PI + CityRandom.FRandRange(0.f, 0.5f);
				const float RadAtHeight = BaseRadius * (1.0f - LFrac * 0.8f) * 0.85f;
				const float LX = TD.X + RadAtHeight * FMath::Cos(Angle);
				const float LY = TD.Y + RadAtHeight * FMath::Sin(Angle);
				const float BulbSize = TH * CityRandom.FRandRange(0.012f, 0.022f);

				const FLinearColor& BulbEmissive = BulbColors[CityRandom.RandRange(0, NumBulbColors - 1)];

				FString BN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
				UStaticMeshComponent* Bulb = NewObject<UStaticMeshComponent>(TA, *BN);
				Bulb->SetStaticMesh(SphereMesh);
				Bulb->SetWorldLocation(FVector(LX, LY, LZ));
				Bulb->SetWorldScale3D(FVector(BulbSize / 50.f));
				Bulb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Bulb->SetBoundsScale(10.f);
				Bulb->RegisterComponent();
				TA->AddInstanceComponent(Bulb);

				// Bright base color + strong emissive for magic glow
				FLinearColor BulbBase(
					FMath::Clamp(BulbEmissive.R * 0.25f, 0.f, 1.f),
					FMath::Clamp(BulbEmissive.G * 0.25f, 0.f, 1.f),
					FMath::Clamp(BulbEmissive.B * 0.25f, 0.f, 1.f));
				ApplyPBR(Bulb, BulbBase, 0.3f, 0.6f);
				if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(Bulb->GetMaterial(0)))
				{
					MI->SetVectorParameterValue(TEXT("EmissiveColor"), BulbEmissive);
				}
			}

			// E. GARLAND STRANDS — thin bright cylinders wrapping horizontally
			const int32 NumGarlands = 3;
			for (int32 G = 0; G < NumGarlands; ++G)
			{
				const float GFrac = 0.2f + 0.25f * (float)G;
				const float GZ = TrunkH + GFrac * TH * 0.85f;
				const float GRad = BaseRadius * (1.0f - GFrac * 0.75f) * 0.82f;

				// 8 short cylinder segments forming a ring
				for (int32 Seg = 0; Seg < 8; ++Seg)
				{
					const float A1 = (float)Seg / 8.f * 2.f * PI;
					const float A2 = (float)(Seg + 1) / 8.f * 2.f * PI;
					const float GX = TD.X + GRad * FMath::Cos((A1 + A2) * 0.5f);
					const float GY = TD.Y + GRad * FMath::Sin((A1 + A2) * 0.5f);
					const float SegLen = GRad * 2.f * PI / 8.f;
					const float SegYaw = FMath::RadiansToDegrees((A1 + A2) * 0.5f + PI * 0.5f);

					FString GN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
					UStaticMeshComponent* Garl = NewObject<UStaticMeshComponent>(TA, *GN);
					Garl->SetStaticMesh(CylinderMesh);
					Garl->SetWorldLocation(FVector(GX, GY, GZ));
					Garl->SetWorldScale3D(FVector(TH * 0.003f / 50.f, SegLen * 0.55f / 50.f, TH * 0.003f / 100.f));
					Garl->SetWorldRotation(FRotator(90.f, SegYaw, 0.f));
					Garl->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Garl->SetBoundsScale(10.f);
					Garl->RegisterComponent();
					TA->AddInstanceComponent(Garl);

					const FLinearColor GarlCol = (G == 0) ? LightGold : ((G == 1) ? LightRed : LightWhite);
					ApplyPBR(Garl, FLinearColor(0.8f, 0.7f, 0.2f), 0.2f, 0.8f);
					if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(Garl->GetMaterial(0)))
					{
						MI->SetVectorParameterValue(TEXT("EmissiveColor"), GarlCol * 0.5f);
					}
				}
			}

			// F. SNOW ON BRANCHES — white patches on the foliage tiers
			for (int32 SP = 0; SP < 6; ++SP)
			{
				const float SPFrac = CityRandom.FRandRange(0.1f, 0.8f);
				const float SPZ = TrunkH + SPFrac * TH * 0.85f;
				const float SPRad = BaseRadius * (1.0f - SPFrac * 0.75f) * 0.7f;
				const float SPAngle = CityRandom.FRandRange(0.f, 2.f * PI);

				MC_Cube(TA,
					FVector(TD.X + SPRad * FMath::Cos(SPAngle),
							TD.Y + SPRad * FMath::Sin(SPAngle), SPZ),
					FVector(TH * 0.04f / 100.f, TH * 0.03f / 100.f, TH * 0.005f / 100.f),
					FRotator(CityRandom.FRandRange(-5.f, 5.f), CityRandom.FRandRange(0.f, 360.f), 0.f),
					SnowFresh, 0.50f, 0.f);
			}
		}

		// ---- SNOWMEN ----
		// 30 snowmen scattered across the expanded wonderland
		struct FSnowmanDef { float X; float Y; float Scale; };
		const FSnowmanDef Snowmen[] = {
			{ -90000.f, 72000.f, 1.0f },
			{ -70000.f, 83000.f, 1.0f },
			{ -65000.f, 67000.f, 0.9f },
			{ -60000.f, 86000.f, 0.8f },
			{ -48000.f, 75000.f, 1.2f },
			{ -35000.f, 85000.f, 0.9f },
			{ -28000.f, 65000.f, 1.1f },
			{ -22000.f, 88000.f, 1.1f },
			{ -12000.f, 70000.f, 0.7f },
			{  -3000.f, 82000.f, 1.0f },
			{   5000.f, 66000.f, 1.3f },
			{  15000.f, 81000.f, 1.3f },
			{  22000.f, 69000.f, 0.85f },
			{  35000.f, 83000.f, 1.0f },
			{  40000.f, 67000.f, 0.95f },
			{  50000.f, 86000.f, 0.95f },
			{  58000.f, 71000.f, 1.1f },
			{  65000.f, 82000.f, 1.1f },
			{  75000.f, 68000.f, 0.75f },
			{  78000.f, 88000.f, 0.75f },
			{  88000.f, 73000.f, 0.9f },
			{ -82000.f, 87000.f, 0.9f },
			{ -55000.f, 69000.f, 0.8f },
			{ -30000.f, 92000.f, 0.8f },
			{   8000.f, 91000.f, 1.0f },
			{  42000.f, 90000.f, 0.85f },
			{  70000.f, 85000.f, 1.05f },
			{ -15000.f, 78000.f, 1.15f },
			{  30000.f, 76000.f, 0.9f },
			{  82000.f, 80000.f, 1.0f },
		};
		const int32 NumSnowmen = UE_ARRAY_COUNT(Snowmen);

		const FLinearColor SnowWhite(0.95f, 0.96f, 0.98f);
		const FLinearColor CoalBlack(0.03f, 0.03f, 0.03f);
		const FLinearColor CarrotOrange(0.85f, 0.35f, 0.02f);
		const FLinearColor ScarfRed(0.75f, 0.05f, 0.05f);
		const FLinearColor HatBlack(0.05f, 0.05f, 0.05f);

		for (int32 S = 0; S < NumSnowmen; ++S)
		{
			const FSnowmanDef& SD = Snowmen[S];
			const float SC = SD.Scale;
			const float BaseSize = 600.f * SC;   // bottom ball radius
			const float MidSize = 420.f * SC;    // middle ball radius
			const float HeadSize = 280.f * SC;   // head radius

			AActor* SA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(SD.X, SD.Y, 0.f), FRotator::ZeroRotator);
			if (!SA) continue;
#if WITH_EDITOR
			SA->SetActorLabel(*FString::Printf(TEXT("Snowman_%d"), S));
#endif
			const float FaceYaw = CityRandom.FRandRange(0.f, 360.f);

			// Bottom ball
			FString BN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
			UStaticMeshComponent* Bottom = NewObject<UStaticMeshComponent>(SA, *BN);
			Bottom->SetStaticMesh(SphereMesh);
			Bottom->SetWorldLocation(FVector(SD.X, SD.Y, BaseSize * 0.8f));
			Bottom->SetWorldScale3D(FVector(BaseSize / 50.f, BaseSize / 50.f, BaseSize * 0.85f / 50.f));
			Bottom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Bottom->SetBoundsScale(10.f);
			Bottom->RegisterComponent();
			ApplyPBR(Bottom, SnowWhite, 0.50f, 0.f);
			SA->AddInstanceComponent(Bottom);
			SA->SetRootComponent(Bottom);

			// Middle ball
			const float MidZ = BaseSize * 1.5f + MidSize * 0.7f;
			FString MN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
			UStaticMeshComponent* Mid = NewObject<UStaticMeshComponent>(SA, *MN);
			Mid->SetStaticMesh(SphereMesh);
			Mid->SetWorldLocation(FVector(SD.X, SD.Y, MidZ));
			Mid->SetWorldScale3D(FVector(MidSize / 50.f, MidSize / 50.f, MidSize * 0.9f / 50.f));
			Mid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Mid->SetBoundsScale(10.f);
			Mid->RegisterComponent();
			ApplyPBR(Mid, SnowWhite, 0.50f, 0.f);
			SA->AddInstanceComponent(Mid);

			// Head
			const float HeadZ = MidZ + MidSize * 0.75f + HeadSize * 0.6f;
			FString HN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
			UStaticMeshComponent* Head = NewObject<UStaticMeshComponent>(SA, *HN);
			Head->SetStaticMesh(SphereMesh);
			Head->SetWorldLocation(FVector(SD.X, SD.Y, HeadZ));
			Head->SetWorldScale3D(FVector(HeadSize / 50.f));
			Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Head->SetBoundsScale(10.f);
			Head->RegisterComponent();
			ApplyPBR(Head, SnowWhite, 0.48f, 0.f);
			SA->AddInstanceComponent(Head);

			// EYES — two coal spheres
			const float EyeSize = HeadSize * 0.12f;
			const float EyeForward = HeadSize * 0.85f;
			const float FaceRad = FMath::DegreesToRadians(FaceYaw);
			for (int32 E = 0; E < 2; ++E)
			{
				const float EyeSide = (E == 0) ? -1.f : 1.f;
				const float PerpdRad = FaceRad + PI * 0.5f;
				const float EX = SD.X + EyeForward * FMath::Cos(FaceRad) + EyeSide * HeadSize * 0.3f * FMath::Cos(PerpdRad);
				const float EY = SD.Y + EyeForward * FMath::Sin(FaceRad) + EyeSide * HeadSize * 0.3f * FMath::Sin(PerpdRad);
				const float EZ = HeadZ + HeadSize * 0.15f;

				FString EN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
				UStaticMeshComponent* Eye = NewObject<UStaticMeshComponent>(SA, *EN);
				Eye->SetStaticMesh(SphereMesh);
				Eye->SetWorldLocation(FVector(EX, EY, EZ));
				Eye->SetWorldScale3D(FVector(EyeSize / 50.f));
				Eye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Eye->SetBoundsScale(10.f);
				Eye->RegisterComponent();
				ApplyPBR(Eye, CoalBlack, 0.70f, 0.f);
				SA->AddInstanceComponent(Eye);
			}

			// CARROT NOSE — small orange cone pointing forward
			{
				const float NoseLen = HeadSize * 0.6f;
				const float NoseX = SD.X + HeadSize * 0.85f * FMath::Cos(FaceRad);
				const float NoseY = SD.Y + HeadSize * 0.85f * FMath::Sin(FaceRad);
				MC_Cone(SA,
					FVector(NoseX, NoseY, HeadZ),
					FVector(HeadSize * 0.08f / 50.f, HeadSize * 0.08f / 50.f, NoseLen / 100.f),
					FRotator(90.f, FaceYaw, 0.f),
					CarrotOrange, 0.70f, 0.f);
			}

			// COAL MOUTH — 5 small coal spheres in a smile
			for (int32 Mth = 0; Mth < 5; ++Mth)
			{
				const float MthAngle = FMath::DegreesToRadians(-30.f + 15.f * (float)Mth);
				const float PerpdRad = FaceRad + PI * 0.5f;
				const float MouthFwd = HeadSize * 0.88f;
				const float MouthSide = HeadSize * 0.35f * FMath::Sin(MthAngle);
				const float MX = SD.X + MouthFwd * FMath::Cos(FaceRad) + MouthSide * FMath::Cos(PerpdRad);
				const float MY = SD.Y + MouthFwd * FMath::Sin(FaceRad) + MouthSide * FMath::Sin(PerpdRad);
				const float MZ = HeadZ - HeadSize * 0.15f + HeadSize * 0.12f * FMath::Cos(MthAngle);

				FString MMN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
				UStaticMeshComponent* Mouth = NewObject<UStaticMeshComponent>(SA, *MMN);
				Mouth->SetStaticMesh(SphereMesh);
				Mouth->SetWorldLocation(FVector(MX, MY, MZ));
				Mouth->SetWorldScale3D(FVector(EyeSize * 0.6f / 50.f));
				Mouth->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Mouth->SetBoundsScale(10.f);
				Mouth->RegisterComponent();
				ApplyPBR(Mouth, CoalBlack, 0.70f, 0.f);
				SA->AddInstanceComponent(Mouth);
			}

			// TOP HAT — cylinder brim + tall cylinder
			{
				const float HatBrimR = HeadSize * 0.7f;
				const float HatBodyR = HeadSize * 0.45f;
				const float HatHeight = HeadSize * 0.8f;
				const float HatZ = HeadZ + HeadSize * 0.75f;

				// Brim
				MC_Cyl(SA,
					FVector(SD.X, SD.Y, HatZ),
					FVector(HatBrimR / 50.f, HatBrimR / 50.f, HeadSize * 0.05f / 100.f),
					FRotator::ZeroRotator,
					HatBlack, 0.60f, 0.f);

				// Hat body
				MC_Cyl(SA,
					FVector(SD.X, SD.Y, HatZ + HatHeight * 0.5f),
					FVector(HatBodyR / 50.f, HatBodyR / 50.f, HatHeight / 100.f),
					FRotator::ZeroRotator,
					HatBlack, 0.60f, 0.f);

				// Red hat band
				MC_Cyl(SA,
					FVector(SD.X, SD.Y, HatZ + HatHeight * 0.15f),
					FVector(HatBodyR * 1.03f / 50.f, HatBodyR * 1.03f / 50.f, HatHeight * 0.12f / 100.f),
					FRotator::ZeroRotator,
					ScarfRed, 0.55f, 0.f);
			}

			// SCARF — red cylinder wrapped around neck
			{
				const float ScarfZ = MidZ + MidSize * 0.65f;
				MC_Cyl(SA,
					FVector(SD.X, SD.Y, ScarfZ),
					FVector(MidSize * 0.55f / 50.f, MidSize * 0.55f / 50.f, MidSize * 0.18f / 100.f),
					FRotator::ZeroRotator,
					ScarfRed, 0.55f, 0.f);

				// Hanging scarf tail
				const float TailX = SD.X + MidSize * 0.4f * FMath::Cos(FaceRad + 0.5f);
				const float TailY = SD.Y + MidSize * 0.4f * FMath::Sin(FaceRad + 0.5f);
				MC_Cube(SA,
					FVector(TailX, TailY, ScarfZ - MidSize * 0.25f),
					FVector(MidSize * 0.08f / 100.f, MidSize * 0.15f / 100.f, MidSize * 0.3f / 100.f),
					FRotator(12.f, FaceYaw + 30.f, 0.f),
					ScarfRed, 0.55f, 0.f);
			}

			// STICK ARMS — two brown cylinders
			for (int32 Arm = 0; Arm < 2; ++Arm)
			{
				const float ArmSide = (Arm == 0) ? -1.f : 1.f;
				const float PerpdRad = FaceRad + PI * 0.5f;
				const float ArmX = SD.X + ArmSide * MidSize * 0.8f * FMath::Cos(PerpdRad);
				const float ArmY = SD.Y + ArmSide * MidSize * 0.8f * FMath::Sin(PerpdRad);
				const float ArmLen = MidSize * 1.2f;
				const float ArmYaw = FaceYaw + 90.f + ArmSide * CityRandom.FRandRange(15.f, 35.f);

				MC_Cyl(SA,
					FVector(ArmX, ArmY, MidZ + MidSize * 0.2f),
					FVector(MidSize * 0.03f / 50.f, MidSize * 0.03f / 50.f, ArmLen / 100.f),
					FRotator(80.f + ArmSide * 10.f, ArmYaw, 0.f),
					TrunkBrown, 0.85f, 0.f);
			}

			// COAL BUTTONS — 3 on the front of the middle ball
			for (int32 Btn = 0; Btn < 3; ++Btn)
			{
				const float BtnZ = MidZ - MidSize * 0.3f + (float)Btn * MidSize * 0.3f;
				const float BtnFwd = MidSize * 0.88f;
				const float BX = SD.X + BtnFwd * FMath::Cos(FaceRad);
				const float BY = SD.Y + BtnFwd * FMath::Sin(FaceRad);

				FString BBN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
				UStaticMeshComponent* Button = NewObject<UStaticMeshComponent>(SA, *BBN);
				Button->SetStaticMesh(SphereMesh);
				Button->SetWorldLocation(FVector(BX, BY, BtnZ));
				Button->SetWorldScale3D(FVector(MidSize * 0.06f / 50.f));
				Button->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Button->SetBoundsScale(10.f);
				Button->RegisterComponent();
				ApplyPBR(Button, CoalBlack, 0.70f, 0.f);
				SA->AddInstanceComponent(Button);
			}
		}

		// ================================================================
		// MAGICAL ELEMENTS — floating orbs, ice pillars, candy canes,
		// aurora lights, gift boxes, ice crystal arches, frozen pond
		// ================================================================

		// ---- FLOATING MAGIC ORBS — glowing spheres hovering at various heights ----
		for (int32 Orb = 0; Orb < 120; ++Orb)
		{
			const float OX = CityRandom.FRandRange(XmasMinX, XmasMaxX);
			const float OY = CityRandom.FRandRange(XmasMinY, XmasMaxY);
			const float OZ = CityRandom.FRandRange(500.f, 6000.f);
			const float OSize = CityRandom.FRandRange(40.f, 180.f);

			// Pick a magical color
			const int32 OrbColorIdx = CityRandom.RandRange(0, 5);
			FLinearColor OrbEmissive;
			FLinearColor OrbBase;
			switch (OrbColorIdx)
			{
				case 0: OrbEmissive = FLinearColor(4.0f, 0.5f, 4.5f); OrbBase = FLinearColor(0.6f, 0.1f, 0.7f); break; // purple
				case 1: OrbEmissive = FLinearColor(0.3f, 4.0f, 4.5f); OrbBase = FLinearColor(0.1f, 0.6f, 0.7f); break; // cyan
				case 2: OrbEmissive = FLinearColor(4.0f, 3.0f, 0.5f); OrbBase = FLinearColor(0.7f, 0.5f, 0.1f); break; // gold
				case 3: OrbEmissive = FLinearColor(0.3f, 4.5f, 0.5f); OrbBase = FLinearColor(0.1f, 0.7f, 0.1f); break; // green
				case 4: OrbEmissive = FLinearColor(4.5f, 0.3f, 0.3f); OrbBase = FLinearColor(0.7f, 0.1f, 0.1f); break; // red
				default: OrbEmissive = FLinearColor(3.5f, 3.5f, 4.5f); OrbBase = FLinearColor(0.7f, 0.7f, 0.9f); break; // white-blue
			}

			AActor* OrbA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(OX, OY, OZ), FRotator::ZeroRotator);
			if (!OrbA) continue;

			FString ON = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
			UStaticMeshComponent* OrbMesh = NewObject<UStaticMeshComponent>(OrbA, *ON);
			OrbMesh->SetStaticMesh(SphereMesh);
			OrbMesh->SetWorldLocation(FVector(OX, OY, OZ));
			OrbMesh->SetWorldScale3D(FVector(OSize / 50.f));
			OrbMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			OrbMesh->SetBoundsScale(10.f);
			OrbMesh->RegisterComponent();
			OrbA->AddInstanceComponent(OrbMesh);
			OrbA->SetRootComponent(OrbMesh);
			ApplyPBR(OrbMesh, OrbBase, 0.15f, 0.6f);
			if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(OrbMesh->GetMaterial(0)))
			{
				MI->SetVectorParameterValue(TEXT("EmissiveColor"), OrbEmissive);
			}
		}

		// ---- GIANT CANDY CANES — red & white striped poles ----
		for (int32 CC = 0; CC < 20; ++CC)
		{
			const float CCX = CityRandom.FRandRange(XmasMinX + 5000.f, XmasMaxX - 5000.f);
			const float CCY = CityRandom.FRandRange(XmasMinY, XmasMaxY);
			const float CCH = CityRandom.FRandRange(2500.f, 5000.f);
			const float CCR = CCH * 0.04f;

			AActor* CCA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(CCX, CCY, 0.f), FRotator::ZeroRotator);
			if (!CCA) continue;

			// Straight pole (white)
			UStaticMeshComponent* Pole = MC_Cyl(CCA,
				FVector(CCX, CCY, CCH * 0.5f),
				FVector(CCR / 50.f, CCR / 50.f, CCH / 100.f),
				FRotator::ZeroRotator,
				FLinearColor(0.95f, 0.95f, 0.97f), 0.35f, 0.f);
			CCA->SetRootComponent(Pole);

			// Red stripes spiraling up (cylinders at angles)
			const int32 NumStripes = 8;
			for (int32 St = 0; St < NumStripes; ++St)
			{
				const float StFrac = (float)St / (float)NumStripes;
				const float StZ = CCH * 0.1f + StFrac * CCH * 0.75f;
				const float StAngle = StFrac * 4.f * PI;
				const float StX = CCX + CCR * 0.6f * FMath::Cos(StAngle);
				const float StY = CCY + CCR * 0.6f * FMath::Sin(StAngle);

				MC_Cube(CCA,
					FVector(StX, StY, StZ),
					FVector(CCR * 0.8f / 100.f, CCR * 1.5f / 100.f, CCH * 0.06f / 100.f),
					FRotator(15.f, FMath::RadiansToDegrees(StAngle), 0.f),
					FLinearColor(0.85f, 0.05f, 0.05f), 0.40f, 0.f);
			}

			// Curved hook at top (sphere)
			FString HN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
			UStaticMeshComponent* Hook = NewObject<UStaticMeshComponent>(CCA, *HN);
			Hook->SetStaticMesh(SphereMesh);
			Hook->SetWorldLocation(FVector(CCX + CCR * 2.f, CCY, CCH * 0.95f));
			Hook->SetWorldScale3D(FVector(CCR * 1.5f / 50.f, CCR * 1.5f / 50.f, CCR * 2.f / 50.f));
			Hook->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Hook->SetBoundsScale(10.f);
			Hook->RegisterComponent();
			ApplyPBR(Hook, FLinearColor(0.85f, 0.05f, 0.05f), 0.40f, 0.f);
			CCA->AddInstanceComponent(Hook);
		}

		// ---- ICE CRYSTAL PILLARS — translucent blue-white columns with emissive glow ----
		for (int32 IC = 0; IC < 25; ++IC)
		{
			const float ICX = CityRandom.FRandRange(XmasMinX, XmasMaxX);
			const float ICY = CityRandom.FRandRange(XmasMinY, XmasMaxY);
			const float ICH = CityRandom.FRandRange(1500.f, 4500.f);
			const float ICR = CityRandom.FRandRange(80.f, 250.f);

			AActor* ICA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(ICX, ICY, 0.f), FRotator::ZeroRotator);
			if (!ICA) continue;

			// Main crystal column
			UStaticMeshComponent* Crystal = MC_Cyl(ICA,
				FVector(ICX, ICY, ICH * 0.5f),
				FVector(ICR / 50.f, ICR * 0.7f / 50.f, ICH / 100.f),
				FRotator(CityRandom.FRandRange(-5.f, 5.f), CityRandom.FRandRange(0.f, 360.f), 0.f),
				FLinearColor(0.75f, 0.85f, 0.95f), 0.10f, 0.7f);
			ICA->SetRootComponent(Crystal);

			// Inner glow
			if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(Crystal->GetMaterial(0)))
			{
				MI->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.4f, 0.7f, 1.5f));
			}

			// Pointed tip
			MC_Cone(ICA,
				FVector(ICX, ICY, ICH),
				FVector(ICR * 0.8f / 50.f, ICR * 0.6f / 50.f, ICH * 0.25f / 100.f),
				FRotator(CityRandom.FRandRange(-3.f, 3.f), 0.f, 0.f),
				FLinearColor(0.8f, 0.9f, 1.0f), 0.08f, 0.8f);

			// Light at base
			FString LN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
			UPointLightComponent* CrystalLight = NewObject<UPointLightComponent>(ICA, *LN);
			CrystalLight->SetWorldLocation(FVector(ICX, ICY, ICH * 0.5f));
			CrystalLight->SetIntensity(5000.f);
			CrystalLight->SetAttenuationRadius(ICH * 1.2f);
			CrystalLight->SetLightColor(FColor(130, 180, 255));
			CrystalLight->SetCastShadows(false);
			CrystalLight->RegisterComponent();
			ICA->AddInstanceComponent(CrystalLight);
		}

		// ---- GIFT BOXES — colorful wrapped presents scattered on the ground ----
		for (int32 GB = 0; GB < 60; ++GB)
		{
			const float GBX = CityRandom.FRandRange(XmasMinX, XmasMaxX);
			const float GBY = CityRandom.FRandRange(XmasMinY, XmasMaxY);
			const float GBSize = CityRandom.FRandRange(150.f, 600.f);
			const float GBH = GBSize * CityRandom.FRandRange(0.6f, 1.2f);

			// Box color
			const int32 BoxCol = CityRandom.RandRange(0, 4);
			FLinearColor BoxColor;
			FLinearColor RibbonColor;
			switch (BoxCol)
			{
				case 0: BoxColor = FLinearColor(0.8f, 0.05f, 0.05f); RibbonColor = FLinearColor(0.9f, 0.85f, 0.1f); break;
				case 1: BoxColor = FLinearColor(0.05f, 0.4f, 0.8f); RibbonColor = FLinearColor(0.9f, 0.9f, 0.95f); break;
				case 2: BoxColor = FLinearColor(0.1f, 0.65f, 0.15f); RibbonColor = FLinearColor(0.85f, 0.05f, 0.05f); break;
				case 3: BoxColor = FLinearColor(0.6f, 0.1f, 0.65f); RibbonColor = FLinearColor(0.9f, 0.85f, 0.1f); break;
				default: BoxColor = FLinearColor(0.9f, 0.85f, 0.1f); RibbonColor = FLinearColor(0.8f, 0.05f, 0.05f); break;
			}

			AActor* GBA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(GBX, GBY, 0.f), FRotator::ZeroRotator);
			if (!GBA) continue;

			const float GBYaw = CityRandom.FRandRange(0.f, 360.f);

			// Box body
			UStaticMeshComponent* Box = MC_Cube(GBA,
				FVector(GBX, GBY, GBH * 0.5f),
				FVector(GBSize / 100.f, GBSize / 100.f, GBH / 100.f),
				FRotator(0.f, GBYaw, 0.f),
				BoxColor, 0.55f, 0.1f);
			GBA->SetRootComponent(Box);

			// Ribbon cross — two thin strips
			MC_Cube(GBA,
				FVector(GBX, GBY, GBH * 0.51f),
				FVector(GBSize * 0.12f / 100.f, GBSize * 1.02f / 100.f, GBH * 1.02f / 100.f),
				FRotator(0.f, GBYaw, 0.f),
				RibbonColor, 0.40f, 0.3f);
			MC_Cube(GBA,
				FVector(GBX, GBY, GBH * 0.51f),
				FVector(GBSize * 1.02f / 100.f, GBSize * 0.12f / 100.f, GBH * 1.02f / 100.f),
				FRotator(0.f, GBYaw, 0.f),
				RibbonColor, 0.40f, 0.3f);

			// Bow on top — two small spheres
			for (int32 Bow = 0; Bow < 2; ++Bow)
			{
				const float BowOffset = (Bow == 0) ? -GBSize * 0.15f : GBSize * 0.15f;
				FString BN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
				UStaticMeshComponent* BowComp = NewObject<UStaticMeshComponent>(GBA, *BN);
				BowComp->SetStaticMesh(SphereMesh);
				BowComp->SetWorldLocation(FVector(GBX + BowOffset, GBY, GBH + GBSize * 0.12f));
				BowComp->SetWorldScale3D(FVector(GBSize * 0.18f / 50.f, GBSize * 0.12f / 50.f, GBSize * 0.1f / 50.f));
				BowComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				BowComp->SetBoundsScale(10.f);
				BowComp->RegisterComponent();
				ApplyPBR(BowComp, RibbonColor, 0.35f, 0.3f);
				GBA->AddInstanceComponent(BowComp);
			}
		}

		// ---- AURORA BOREALIS — large emissive planes high in the sky ----
		{
			AActor* Aurora = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(0.f, 78000.f, 18000.f), FRotator::ZeroRotator);
			if (Aurora)
			{
#if WITH_EDITOR
				Aurora->SetActorLabel(TEXT("AuroraBorealis"));
#endif
				const FLinearColor AuroraColors[] = {
					FLinearColor(0.1f, 2.0f, 0.5f),   // green
					FLinearColor(0.2f, 0.8f, 2.5f),   // cyan-blue
					FLinearColor(1.5f, 0.3f, 2.0f),   // purple
					FLinearColor(0.1f, 2.5f, 1.0f),   // bright green
					FLinearColor(0.5f, 0.2f, 2.5f),   // deep purple
					FLinearColor(0.1f, 1.5f, 2.0f),   // teal
				};

				for (int32 A = 0; A < 12; ++A)
				{
					const float AX = CityRandom.FRandRange(-80000.f, 80000.f);
					const float AY = CityRandom.FRandRange(65000.f, 95000.f);
					const float AZ = CityRandom.FRandRange(14000.f, 22000.f);
					const float AW = CityRandom.FRandRange(15000.f, 40000.f);
					const float AH = CityRandom.FRandRange(3000.f, 8000.f);

					FString AN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
					UStaticMeshComponent* AuroraMesh = NewObject<UStaticMeshComponent>(Aurora, *AN);
					AuroraMesh->SetStaticMesh(PlaneMesh);
					AuroraMesh->SetWorldLocation(FVector(AX, AY, AZ));
					AuroraMesh->SetWorldScale3D(FVector(AW / 100.f, AH / 100.f, 1.f));
					AuroraMesh->SetWorldRotation(FRotator(CityRandom.FRandRange(60.f, 85.f),
						CityRandom.FRandRange(-30.f, 30.f), 0.f));
					AuroraMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					AuroraMesh->SetBoundsScale(10.f);
					AuroraMesh->RegisterComponent();
					Aurora->AddInstanceComponent(AuroraMesh);
					if (A == 0) Aurora->SetRootComponent(AuroraMesh);

					// Use glass material for translucent aurora
					UMaterial* GlassMat = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), nullptr,
						TEXT("/Game/Materials/M_PHCGlass.M_PHCGlass")));
					if (GlassMat)
					{
						UMaterialInstanceDynamic* MI = UMaterialInstanceDynamic::Create(GlassMat, AuroraMesh);
						const FLinearColor& AC = AuroraColors[A % 6];
						MI->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(AC.R * 0.3f, AC.G * 0.3f, AC.B * 0.3f));
						MI->SetScalarParameterValue(TEXT("Opacity"), 0.25f);
						MI->SetVectorParameterValue(TEXT("EmissiveColor"), AC);
						AuroraMesh->SetMaterial(0, MI);
					}
				}
			}
		}

		// ---- FROZEN POND — icy reflective surface ----
		{
			const float PondX = 0.f;
			const float PondY = 76000.f;
			const float PondRadius = 8000.f;

			AActor* Pond = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(PondX, PondY, 0.f), FRotator::ZeroRotator);
			if (Pond)
			{
#if WITH_EDITOR
				Pond->SetActorLabel(TEXT("FrozenPond"));
#endif
				// Ice surface
				UStaticMeshComponent* IceSurface = MC_Cyl(Pond,
					FVector(PondX, PondY, 2.f),
					FVector(PondRadius / 50.f, PondRadius / 50.f, 3.f / 100.f),
					FRotator::ZeroRotator,
					FLinearColor(0.75f, 0.88f, 0.95f), 0.05f, 0.85f);
				Pond->SetRootComponent(IceSurface);

				// Subtle emissive for icy sheen
				if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(IceSurface->GetMaterial(0)))
				{
					MI->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.15f, 0.25f, 0.5f));
				}

				// Snow rim around the pond
				for (int32 Rim = 0; Rim < 16; ++Rim)
				{
					const float RAngle = (float)Rim / 16.f * 2.f * PI;
					const float RX = PondX + (PondRadius + 300.f) * FMath::Cos(RAngle);
					const float RY = PondY + (PondRadius + 300.f) * FMath::Sin(RAngle);

					FString RN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
					UStaticMeshComponent* RimSnow = NewObject<UStaticMeshComponent>(Pond, *RN);
					RimSnow->SetStaticMesh(SphereMesh);
					RimSnow->SetWorldLocation(FVector(RX, RY, 50.f));
					RimSnow->SetWorldScale3D(FVector(PondRadius * 0.15f / 50.f, PondRadius * 0.1f / 50.f, 80.f / 50.f));
					RimSnow->SetWorldRotation(FRotator(0.f, FMath::RadiansToDegrees(RAngle), 0.f));
					RimSnow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					RimSnow->SetBoundsScale(10.f);
					RimSnow->RegisterComponent();
					ApplyPBR(RimSnow, SnowFresh, 0.48f, 0.f);
					Pond->AddInstanceComponent(RimSnow);
				}

				// Ice glow light
				FString PLN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
				UPointLightComponent* PondLight = NewObject<UPointLightComponent>(Pond, *PLN);
				PondLight->SetWorldLocation(FVector(PondX, PondY, 500.f));
				PondLight->SetIntensity(8000.f);
				PondLight->SetAttenuationRadius(PondRadius * 2.f);
				PondLight->SetLightColor(FColor(150, 200, 255));
				PondLight->SetCastShadows(false);
				PondLight->RegisterComponent();
				Pond->AddInstanceComponent(PondLight);
			}
		}

		// ---- SPARKLE DUST — tiny bright particles floating everywhere ----
		for (int32 Sp = 0; Sp < 200; ++Sp)
		{
			const float SpX = CityRandom.FRandRange(XmasMinX, XmasMaxX);
			const float SpY = CityRandom.FRandRange(XmasMinY, XmasMaxY);
			const float SpZ = CityRandom.FRandRange(100.f, 4000.f);
			const float SpSize = CityRandom.FRandRange(15.f, 50.f);

			AActor* SpA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(SpX, SpY, SpZ), FRotator::ZeroRotator);
			if (!SpA) continue;

			FString SPN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
			UStaticMeshComponent* Sparkle = NewObject<UStaticMeshComponent>(SpA, *SPN);
			Sparkle->SetStaticMesh(SphereMesh);
			Sparkle->SetWorldLocation(FVector(SpX, SpY, SpZ));
			Sparkle->SetWorldScale3D(FVector(SpSize / 50.f));
			Sparkle->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Sparkle->SetBoundsScale(10.f);
			Sparkle->RegisterComponent();
			SpA->AddInstanceComponent(Sparkle);
			SpA->SetRootComponent(Sparkle);

			// Sparkling white-gold emissive
			ApplyPBR(Sparkle, FLinearColor(0.95f, 0.92f, 0.8f), 0.1f, 0.8f);
			if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(Sparkle->GetMaterial(0)))
			{
				const float Brightness = CityRandom.FRandRange(2.f, 6.f);
				MI->SetVectorParameterValue(TEXT("EmissiveColor"),
					FLinearColor(Brightness, Brightness * 0.9f, Brightness * 0.6f));
			}
		}

		// ---- MAGICAL LAMP POSTS — ornate street-style poles with warm glow ----
		for (int32 LP = 0; LP < 16; ++LP)
		{
			const float LPX = CityRandom.FRandRange(XmasMinX + 10000.f, XmasMaxX - 10000.f);
			const float LPY = CityRandom.FRandRange(XmasMinY + 2000.f, XmasMaxY - 2000.f);
			const float LPH = CityRandom.FRandRange(2000.f, 3500.f);

			AActor* LPA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(LPX, LPY, 0.f), FRotator::ZeroRotator);
			if (!LPA) continue;

			// Pole
			UStaticMeshComponent* LPPole = MC_Cyl(LPA,
				FVector(LPX, LPY, LPH * 0.5f),
				FVector(60.f / 50.f, 60.f / 50.f, LPH / 100.f),
				FRotator::ZeroRotator,
				FLinearColor(0.15f, 0.12f, 0.08f), 0.50f, 0.7f);
			LPA->SetRootComponent(LPPole);

			// Ornamental base
			MC_Cyl(LPA,
				FVector(LPX, LPY, 100.f),
				FVector(200.f / 50.f, 200.f / 50.f, 200.f / 100.f),
				FRotator::ZeroRotator,
				FLinearColor(0.12f, 0.10f, 0.07f), 0.45f, 0.8f);

			// Lantern globe on top (sphere with emissive)
			FString GLN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
			UStaticMeshComponent* Globe = NewObject<UStaticMeshComponent>(LPA, *GLN);
			Globe->SetStaticMesh(SphereMesh);
			Globe->SetWorldLocation(FVector(LPX, LPY, LPH + 150.f));
			Globe->SetWorldScale3D(FVector(250.f / 50.f));
			Globe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Globe->SetBoundsScale(10.f);
			Globe->RegisterComponent();
			ApplyPBR(Globe, FLinearColor(0.9f, 0.8f, 0.4f), 0.2f, 0.3f);
			if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(Globe->GetMaterial(0)))
			{
				MI->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(5.0f, 3.5f, 1.5f));
			}
			LPA->AddInstanceComponent(Globe);

			// Warm point light
			FString LLN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
			UPointLightComponent* LampLight = NewObject<UPointLightComponent>(LPA, *LLN);
			LampLight->SetWorldLocation(FVector(LPX, LPY, LPH + 200.f));
			LampLight->SetIntensity(12000.f);
			LampLight->SetAttenuationRadius(5000.f);
			LampLight->SetLightColor(FColor(255, 200, 100));
			LampLight->SetCastShadows(false);
			LampLight->RegisterComponent();
			LPA->AddInstanceComponent(LampLight);

			// Holly wreath on pole
			FString WN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
			UStaticMeshComponent* Wreath = NewObject<UStaticMeshComponent>(LPA, *WN);
			Wreath->SetStaticMesh(CylinderMesh);
			Wreath->SetWorldLocation(FVector(LPX, LPY, LPH * 0.6f));
			Wreath->SetWorldScale3D(FVector(200.f / 50.f, 200.f / 50.f, 30.f / 100.f));
			Wreath->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Wreath->SetBoundsScale(10.f);
			Wreath->RegisterComponent();
			ApplyPBR(Wreath, FLinearColor(0.05f, 0.25f, 0.05f), 0.70f, 0.f);
			LPA->AddInstanceComponent(Wreath);
		}

		// ---- SNOWFALL PARTICLES — white spheres drifting in the air ----
		for (int32 SF = 0; SF < 300; ++SF)
		{
			const float SFX = CityRandom.FRandRange(XmasMinX - 5000.f, XmasMaxX + 5000.f);
			const float SFY = CityRandom.FRandRange(XmasMinY - 3000.f, XmasMaxY + 5000.f);
			const float SFZ = CityRandom.FRandRange(200.f, 12000.f);
			const float SFSize = CityRandom.FRandRange(10.f, 35.f);

			AActor* SFA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(SFX, SFY, SFZ), FRotator::ZeroRotator);
			if (!SFA) continue;

			FString SFN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
			UStaticMeshComponent* Flake = NewObject<UStaticMeshComponent>(SFA, *SFN);
			Flake->SetStaticMesh(SphereMesh);
			Flake->SetWorldLocation(FVector(SFX, SFY, SFZ));
			Flake->SetWorldScale3D(FVector(SFSize / 50.f));
			Flake->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Flake->SetBoundsScale(10.f);
			Flake->RegisterComponent();
			ApplyPBR(Flake, FLinearColor(0.96f, 0.97f, 0.99f), 0.50f, 0.f);
			SFA->AddInstanceComponent(Flake);
			SFA->SetRootComponent(Flake);
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Spawned mountain range — north & south — with %d peaks."), NumPeaks);
}

// ---------------------------------------------------------------------------
// Cinderella Castle — Disney World Magic Kingdom castle in the NE expansion quadrant
// ---------------------------------------------------------------------------


void UPHCCitySubsystem::SpawnSandCastle()
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh || !CylinderMesh || !ConeMesh || !SphereMesh || !PlaneMesh) return;

	const FVector Origin(95000.f, 30000.f, 0.f);
	AActor* Castle = World->SpawnActor<AActor>(AActor::StaticClass(), Origin, FRotator::ZeroRotator);
	if (!Castle) return;
#if WITH_EDITOR
	Castle->SetActorLabel(TEXT("CinderellaCastle"));
#endif

	int32 CompIdx = 0;

	// ---- Color palette (backward compat) ----
	const FLinearColor SandBase(0.76f, 0.70f, 0.50f);
	const float SandRough = 0.92f;

	// ---- Sand castle color palette — everything is sand ----
	const FLinearColor StoneWall(0.76f, 0.70f, 0.50f);        // main sand
	const FLinearColor RoofBlue(0.72f, 0.66f, 0.47f);         // slightly darker sand for roofs
	const FLinearColor GoldAccent(0.82f, 0.76f, 0.56f);       // lighter sand highlight
	const FLinearColor PinkAccent(0.78f, 0.72f, 0.52f);       // mid-tone sand
	const FLinearColor WhiteTrim(0.84f, 0.78f, 0.58f);        // light sand trim
	const FLinearColor DarkAccent(0.58f, 0.52f, 0.36f);       // dark sand (windows/doors)
	const FLinearColor BannerBlue(0.70f, 0.64f, 0.45f);       // darker sand for banners
	const FLinearColor LightGray(0.80f, 0.74f, 0.54f);        // light sand base

	const float StoneRough   = 0.92f;
	const float RoofRough    = 0.90f;
	const float GoldMetal    = 0.0f;
	const float GoldRough    = 0.92f;
	const float TrimRough    = 0.90f;
	const float DarkRough    = 0.95f;
	const float PinkRough    = 0.92f;
	const float StoneMetal   = 0.0f;

	// ================================================================
	// Mesh helper lambdas
	// ================================================================
	auto MakeCube = [&](AActor* Owner, const FVector& Loc, const FVector& Scale3D,
		const FRotator& Rot, const FLinearColor& Color, float Roughness, float Metallic, const TCHAR* Name)
	{
		FString CompName = FString::Printf(TEXT("%s_%d"), Name, CompIdx++);
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, *CompName);
		MC->SetStaticMesh(CubeMesh);
		MC->SetWorldScale3D(Scale3D);
		MC->SetWorldLocation(Loc);
		MC->SetWorldRotation(Rot);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		ApplyPBR(MC, Color, Metallic, Roughness);
		Owner->AddInstanceComponent(MC);
		return MC;
	};

	auto MakeCylinder = [&](AActor* Owner, const FVector& Loc, const FVector& Scale3D,
		const FLinearColor& Color, float Roughness, float Metallic, const TCHAR* Name)
	{
		FString CompName = FString::Printf(TEXT("%s_%d"), Name, CompIdx++);
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, *CompName);
		MC->SetStaticMesh(CylinderMesh);
		MC->SetWorldScale3D(Scale3D);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		ApplyPBR(MC, Color, Metallic, Roughness);
		Owner->AddInstanceComponent(MC);
		return MC;
	};

	auto MakeSphere = [&](AActor* Owner, const FVector& Loc, const FVector& Scale3D,
		const FLinearColor& Color, float Roughness, float Metallic, const TCHAR* Name)
	{
		FString CompName = FString::Printf(TEXT("%s_%d"), Name, CompIdx++);
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, *CompName);
		MC->SetStaticMesh(SphereMesh);
		MC->SetWorldScale3D(Scale3D);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		ApplyPBR(MC, Color, Metallic, Roughness);
		Owner->AddInstanceComponent(MC);
		return MC;
	};

	auto MakeCone = [&](AActor* Owner, const FVector& Loc, const FVector& Scale3D,
		const FRotator& Rot, const FLinearColor& Color, float Roughness, float Metallic, const TCHAR* Name)
	{
		FString CompName = FString::Printf(TEXT("%s_%d"), Name, CompIdx++);
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, *CompName);
		MC->SetStaticMesh(ConeMesh);
		MC->SetWorldScale3D(Scale3D);
		MC->SetWorldLocation(Loc);
		MC->SetWorldRotation(Rot);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		ApplyPBR(MC, Color, Metallic, Roughness);
		Owner->AddInstanceComponent(MC);
		return MC;
	};

	// Glass/water helper – uses GlassMaterial for translucent moat water
	auto MakeWater = [&](AActor* Owner, const FVector& Loc, const FVector& Scale3D,
		const TCHAR* Name)
	{
		FString CompName = FString::Printf(TEXT("%s_%d"), Name, CompIdx++);
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, *CompName);
		MC->SetStaticMesh(PlaneMesh);
		MC->SetWorldScale3D(Scale3D);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		ApplyPBRGlass(MC, GlassMaterial, FLinearColor(0.15f, 0.35f, 0.55f),
			0.55f, 0.0f, 0.05f);
		Owner->AddInstanceComponent(MC);
		return MC;
	};

	// Alias for backward compatibility
	const float SandRoughness = SandRough;
	int32& Idx = CompIdx;

	// Generic mesh adder — takes any mesh, color, roughness
	auto Add = [&](UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot,
		const FVector& Scale, const FLinearColor& Color, float Roughness) -> UStaticMeshComponent*
	{
		FString CompName = FString::Printf(TEXT("Castle_%d"), CompIdx++);
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Castle, *CompName);
		MC->SetStaticMesh(Mesh);
		MC->SetWorldScale3D(Scale);
		MC->SetWorldLocation(Loc);
		MC->SetWorldRotation(Rot);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		ApplyPBR(MC, Color, 0.f, Roughness);
		Castle->AddInstanceComponent(MC);
		return MC;
	};

	// Sand-colored mesh adder — auto-applies SandBase color with slight random variation
	auto AddSand = [&](UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot,
		const FVector& Scale) -> UStaticMeshComponent*
	{
		FLinearColor Vary = SandBase;
		Vary.R += CityRandom.FRandRange(-0.03f, 0.03f);
		Vary.G += CityRandom.FRandRange(-0.03f, 0.03f);
		Vary.B += CityRandom.FRandRange(-0.03f, 0.03f);
		return Add(Mesh, Loc, Rot, Scale, Vary, SandRough);
	};

	// Moat water adder
	auto AddWater = [&](const FVector& Loc, const FVector& Scale)
	{
		return MakeWater(Castle, Loc, Scale, TEXT("Moat"));
	};

	// PBR mesh adder — full control over metallic and roughness
	auto AddPBR = [&](UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot,
		const FVector& Scale, const FLinearColor& Color, float Metallic, float Roughness) -> UStaticMeshComponent*
	{
		FString CompName = FString::Printf(TEXT("Castle_%d"), CompIdx++);
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Castle, *CompName);
		MC->SetStaticMesh(Mesh);
		MC->SetWorldScale3D(Scale);
		MC->SetWorldLocation(Loc);
		MC->SetWorldRotation(Rot);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(10.f);
		MC->RegisterComponent();
		ApplyPBR(MC, Color, Metallic, Roughness);
		Castle->AddInstanceComponent(MC);
		return MC;
	};

	// ================================================================
	// Backward-compat dimensions (used by sections 14-18)
	// ================================================================
	const float MoatOuter = 10000.f;   // outer radius of castle for section 15 palm trees

	// ================================================================
	// A. FOUNDATION / BASE PLATFORM
	// ================================================================
	// Large base platform the castle sits on — light gray stone
	UStaticMeshComponent* FoundMC = MakeCube(Castle,
		Origin + FVector(0.f, 0.f, 150.f),
		FVector(60.f, 40.f, 3.f),
		FRotator::ZeroRotator, LightGray, StoneRough, StoneMetal, TEXT("BasePlatform"));
	Castle->SetRootComponent(FoundMC);

	// Wider sub-base for the hill the castle sits on
	MakeSphere(Castle,
		Origin + FVector(0.f, 0.f, -200.f),
		FVector(120.f, 90.f, 6.f),
		LightGray, 0.65f, 0.f, TEXT("CastleHill"));

	// Front steps: 5 stepped cubes descending from the south face
	for (int32 i = 0; i < 5; ++i)
	{
		const float StepW = 2000.f - i * 80.f;
		const float StepH = 80.f;
		const float StepD = 300.f;
		const float StepZ = 300.f - i * 60.f;
		const float StepY = -2000.f - i * 300.f;
		MakeCube(Castle,
			Origin + FVector(0.f, StepY, StepZ),
			FVector(StepW / 100.f, StepD / 100.f, StepH / 100.f),
			FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("FrontStep"));
	}

	// ================================================================
	// B. MAIN GATE BUILDING (ground level, front / south face)
	// ================================================================
	// Central archway structure
	MakeCube(Castle,
		Origin + FVector(0.f, -1200.f, 1300.f),
		FVector(30.f, 15.f, 20.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, StoneMetal, TEXT("GateBody"));

	// Pink accent panel on gate front
	MakeCube(Castle,
		Origin + FVector(0.f, -1960.f, 1400.f),
		FVector(26.f, 0.5f, 14.f),
		FRotator::ZeroRotator, PinkAccent, PinkRough, 0.f, TEXT("GatePinkPanel"));

	// White trim band on gate
	MakeCube(Castle,
		Origin + FVector(0.f, -1960.f, 2100.f),
		FVector(28.f, 0.6f, 1.2f),
		FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("GateTrimBand"));

	// Dark archway opening (the grand entrance)
	MakeCube(Castle,
		Origin + FVector(0.f, -1960.f, 600.f),
		FVector(10.f, 1.5f, 12.f),
		FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("GateArchVoid"));

	// Gold trim around the arch
	MakeCube(Castle,
		Origin + FVector(0.f, -1965.f, 1250.f),
		FVector(11.f, 0.4f, 0.8f),
		FRotator::ZeroRotator, GoldAccent, GoldMetal, GoldRough, TEXT("ArchGoldTop"));
	MakeCube(Castle,
		Origin + FVector(-550.f, -1965.f, 600.f),
		FVector(0.5f, 0.4f, 12.f),
		FRotator::ZeroRotator, GoldAccent, GoldMetal, GoldRough, TEXT("ArchGoldL"));
	MakeCube(Castle,
		Origin + FVector(550.f, -1965.f, 600.f),
		FVector(0.5f, 0.4f, 12.f),
		FRotator::ZeroRotator, GoldAccent, GoldMetal, GoldRough, TEXT("ArchGoldR"));

	// Gothic arch top (half-cylinder for arch shape)
	MakeCylinder(Castle,
		Origin + FVector(0.f, -1960.f, 1200.f),
		FVector(5.f, 1.f, 5.f),
		StoneWall, StoneRough, StoneMetal, TEXT("GothicArch"));

	// ---- Left Gate Tower ----
	MakeCylinder(Castle,
		Origin + FVector(-1200.f, -1500.f, 1500.f),
		FVector(4.f, 4.f, 30.f),
		StoneWall, StoneRough, StoneMetal, TEXT("GateTowerL"));

	MakeCone(Castle,
		Origin + FVector(-1200.f, -1500.f, 3300.f),
		FVector(6.f, 6.f, 6.f),
		FRotator::ZeroRotator, RoofBlue, RoofRough, 0.f, TEXT("GateTowerLRoof"));

	MakeSphere(Castle,
		Origin + FVector(-1200.f, -1500.f, 3650.f),
		FVector(0.8f, 0.8f, 0.8f),
		GoldAccent, GoldRough, GoldMetal, TEXT("GateTowerLFinial"));

	// Gold band on left gate tower
	MakeCylinder(Castle,
		Origin + FVector(-1200.f, -1500.f, 2200.f),
		FVector(4.4f, 4.4f, 0.4f),
		GoldAccent, GoldRough, GoldMetal, TEXT("GateTowerLBand"));

	// ---- Right Gate Tower ----
	MakeCylinder(Castle,
		Origin + FVector(1200.f, -1500.f, 1500.f),
		FVector(4.f, 4.f, 30.f),
		StoneWall, StoneRough, StoneMetal, TEXT("GateTowerR"));

	MakeCone(Castle,
		Origin + FVector(1200.f, -1500.f, 3300.f),
		FVector(6.f, 6.f, 6.f),
		FRotator::ZeroRotator, RoofBlue, RoofRough, 0.f, TEXT("GateTowerRRoof"));

	MakeSphere(Castle,
		Origin + FVector(1200.f, -1500.f, 3650.f),
		FVector(0.8f, 0.8f, 0.8f),
		GoldAccent, GoldRough, GoldMetal, TEXT("GateTowerRFinial"));

	// Gold band on right gate tower
	MakeCylinder(Castle,
		Origin + FVector(1200.f, -1500.f, 2200.f),
		FVector(4.4f, 4.4f, 0.4f),
		GoldAccent, GoldRough, GoldMetal, TEXT("GateTowerRBand"));

	// ================================================================
	// C. CENTRAL KEEP (main rectangular body behind the gate)
	// ================================================================
	// Main keep body
	MakeCube(Castle,
		Origin + FVector(0.f, 200.f, 2050.f),
		FVector(35.f, 30.f, 35.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, StoneMetal, TEXT("MainKeep"));

	// Stepped upper section on top of keep
	MakeCube(Castle,
		Origin + FVector(0.f, 200.f, 4550.f),
		FVector(25.f, 20.f, 15.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, StoneMetal, TEXT("UpperKeep"));

	// Pink accent panels on keep facades (front and back)
	MakeCube(Castle,
		Origin + FVector(0.f, -1295.f, 2400.f),
		FVector(30.f, 0.5f, 20.f),
		FRotator::ZeroRotator, PinkAccent, PinkRough, 0.f, TEXT("KeepPinkFront"));
	MakeCube(Castle,
		Origin + FVector(0.f, 1695.f, 2400.f),
		FVector(30.f, 0.5f, 20.f),
		FRotator::ZeroRotator, PinkAccent, PinkRough, 0.f, TEXT("KeepPinkBack"));

	// White horizontal trim bands across all facades
	for (int32 band = 0; band < 3; ++band)
	{
		const float BandZ = 1500.f + band * 1200.f;
		// Front
		MakeCube(Castle,
			Origin + FVector(0.f, -1300.f, BandZ),
			FVector(35.5f, 0.6f, 0.8f),
			FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("KeepTrimF"));
		// Back
		MakeCube(Castle,
			Origin + FVector(0.f, 1700.f, BandZ),
			FVector(35.5f, 0.6f, 0.8f),
			FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("KeepTrimB"));
		// East
		MakeCube(Castle,
			Origin + FVector(1755.f, 200.f, BandZ),
			FVector(0.6f, 30.5f, 0.8f),
			FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("KeepTrimE"));
		// West
		MakeCube(Castle,
			Origin + FVector(-1755.f, 200.f, BandZ),
			FVector(0.6f, 30.5f, 0.8f),
			FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("KeepTrimW"));
	}

	// Windows on main keep — 4 per face, 2 rows of 2
	// Front face windows (south)
	for (int32 row = 0; row < 2; ++row)
	{
		for (int32 col = 0; col < 2; ++col)
		{
			const float WX = -600.f + col * 1200.f;
			const float WZ = 1800.f + row * 1400.f;
			MakeCube(Castle,
				Origin + FVector(WX, -1300.f, WZ),
				FVector(2.2f, 0.5f, 3.5f),
				FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("KeepWinF"));
			// Gold window frame
			MakeCube(Castle,
				Origin + FVector(WX, -1305.f, WZ),
				FVector(2.6f, 0.3f, 3.9f),
				FRotator::ZeroRotator, GoldAccent, GoldMetal, GoldRough, TEXT("WinFrameF"));
		}
	}
	// Back face windows (north)
	for (int32 row = 0; row < 2; ++row)
	{
		for (int32 col = 0; col < 2; ++col)
		{
			const float WX = -600.f + col * 1200.f;
			const float WZ = 1800.f + row * 1400.f;
			MakeCube(Castle,
				Origin + FVector(WX, 1700.f, WZ),
				FVector(2.2f, 0.5f, 3.5f),
				FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("KeepWinB"));
		}
	}
	// East face windows
	for (int32 row = 0; row < 2; ++row)
	{
		for (int32 col = 0; col < 2; ++col)
		{
			const float WY = -400.f + col * 1200.f;
			const float WZ = 1800.f + row * 1400.f;
			MakeCube(Castle,
				Origin + FVector(1760.f, WY, WZ),
				FVector(0.5f, 2.2f, 3.5f),
				FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("KeepWinE"));
		}
	}
	// West face windows
	for (int32 row = 0; row < 2; ++row)
	{
		for (int32 col = 0; col < 2; ++col)
		{
			const float WY = -400.f + col * 1200.f;
			const float WZ = 1800.f + row * 1400.f;
			MakeCube(Castle,
				Origin + FVector(-1760.f, WY, WZ),
				FVector(0.5f, 2.2f, 3.5f),
				FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("KeepWinW"));
		}
	}

	// Clock face on front upper facade — gold sphere
	MakeSphere(Castle,
		Origin + FVector(0.f, -1305.f, 4200.f),
		FVector(2.5f, 0.5f, 2.5f),
		GoldAccent, GoldRough, GoldMetal, TEXT("ClockFace"));

	// ================================================================
	// D. THE GRAND SPIRE (iconic tallest tower, center-rear)
	// ================================================================
	// Very tall cylinder at center-rear of castle
	MakeCylinder(Castle,
		Origin + FVector(0.f, 800.f, 4500.f),
		FVector(5.f, 5.f, 60.f),
		StoneWall, StoneRough, StoneMetal, TEXT("GrandSpire"));

	// Upper section of grand spire (narrower)
	MakeCylinder(Castle,
		Origin + FVector(0.f, 800.f, 8500.f),
		FVector(4.f, 4.f, 30.f),
		StoneWall, StoneRough, StoneMetal, TEXT("GrandSpireUpper"));

	// Grand spire blue cone roof — very tall and pointy
	MakeCone(Castle,
		Origin + FVector(0.f, 800.f, 11500.f),
		FVector(8.f, 8.f, 30.f),
		FRotator::ZeroRotator, RoofBlue, RoofRough, 0.f, TEXT("GrandSpireRoof"));

	// Gold sphere finial at the very top of the grand spire
	MakeSphere(Castle,
		Origin + FVector(0.f, 800.f, 13100.f),
		FVector(1.2f, 1.2f, 1.2f),
		GoldAccent, GoldRough, GoldMetal, TEXT("GrandSpireFinial"));

	// 3 decorative gold rings around the grand spire at different heights
	MakeCylinder(Castle,
		Origin + FVector(0.f, 800.f, 6000.f),
		FVector(5.6f, 5.6f, 0.3f),
		GoldAccent, GoldRough, GoldMetal, TEXT("SpireRing1"));
	MakeCylinder(Castle,
		Origin + FVector(0.f, 800.f, 7500.f),
		FVector(4.8f, 4.8f, 0.3f),
		GoldAccent, GoldRough, GoldMetal, TEXT("SpireRing2"));
	MakeCylinder(Castle,
		Origin + FVector(0.f, 800.f, 9000.f),
		FVector(4.2f, 4.2f, 0.3f),
		GoldAccent, GoldRough, GoldMetal, TEXT("SpireRing3"));

	// Small window openings spiraling up the grand spire
	for (int32 i = 0; i < 8; ++i)
	{
		const float WAngle = i * 45.f;
		const float WRad = FMath::DegreesToRadians(WAngle);
		const float WZ = 5000.f + i * 800.f;
		const float WR = 260.f;
		MakeCube(Castle,
			Origin + FVector(FMath::Cos(WRad) * WR, 800.f + FMath::Sin(WRad) * WR, WZ),
			FVector(0.8f, 0.8f, 1.5f),
			FRotator(0.f, WAngle, 0.f), DarkAccent, DarkRough, 0.f, TEXT("SpireWin"));
	}

	// White balcony ring near the top of the grand spire
	MakeCylinder(Castle,
		Origin + FVector(0.f, 800.f, 10500.f),
		FVector(5.5f, 5.5f, 0.5f),
		WhiteTrim, TrimRough, 0.f, TEXT("SpireBalconyRing"));

	// Small railing posts around the balcony (8 posts)
	for (int32 i = 0; i < 8; ++i)
	{
		const float Angle = i * 45.f;
		const float Rad = FMath::DegreesToRadians(Angle);
		const float PR = 280.f;
		MakeCylinder(Castle,
			Origin + FVector(FMath::Cos(Rad) * PR, 800.f + FMath::Sin(Rad) * PR, 10650.f),
			FVector(0.3f, 0.3f, 2.5f),
			WhiteTrim, TrimRough, 0.f, TEXT("SpireRailPost"));
	}

	// ================================================================
	// E. SECONDARY SPIRES (4 medium towers at upper keep corners)
	// ================================================================
	const float SecSpireH = 4000.f;
	const float SecSpireDiam = 3.5f;    // cylinder scale
	const float SecConeH = 15.f;        // cone scale Z
	const float SecConeDiam = 6.f;      // cone scale XY
	const float SecBalconyDiam = 4.0f;

	struct FSecSpirePos { float X; float Y; };
	const FSecSpirePos SecSpires[4] = {
		{  1200.f,  1100.f },   // NE
		{ -1200.f,  1100.f },   // NW
		{  1200.f,  -700.f },   // SE
		{ -1200.f,  -700.f }    // SW
	};

	for (int32 i = 0; i < 4; ++i)
	{
		const float SX = SecSpires[i].X;
		const float SY = SecSpires[i].Y;

		// Tower cylinder
		MakeCylinder(Castle,
			Origin + FVector(SX, SY, 4500.f),
			FVector(SecSpireDiam, SecSpireDiam, 40.f),
			StoneWall, StoneRough, StoneMetal, TEXT("SecSpire"));

		// Blue cone roof
		MakeCone(Castle,
			Origin + FVector(SX, SY, 7000.f),
			FVector(SecConeDiam, SecConeDiam, SecConeH),
			FRotator::ZeroRotator, RoofBlue, RoofRough, 0.f, TEXT("SecSpireRoof"));

		// Gold sphere finial
		MakeSphere(Castle,
			Origin + FVector(SX, SY, 7850.f),
			FVector(0.7f, 0.7f, 0.7f),
			GoldAccent, GoldRough, GoldMetal, TEXT("SecSpireFinial"));

		// Balcony ring at midpoint
		MakeCylinder(Castle,
			Origin + FVector(SX, SY, 5500.f),
			FVector(SecBalconyDiam, SecBalconyDiam, 0.3f),
			WhiteTrim, TrimRough, 0.f, TEXT("SecSpireBalcony"));

		// Gold trim band
		MakeCylinder(Castle,
			Origin + FVector(SX, SY, 6200.f),
			FVector(3.8f, 3.8f, 0.3f),
			GoldAccent, GoldRough, GoldMetal, TEXT("SecSpireGoldBand"));

		// Small windows on secondary spires (3 per tower)
		for (int32 w = 0; w < 3; ++w)
		{
			const float WWZ = 4800.f + w * 800.f;
			MakeCube(Castle,
				Origin + FVector(SX + (SX > 0 ? 180.f : -180.f), SY, WWZ),
				FVector(0.5f, 0.7f, 1.2f),
				FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("SecSpireWin"));
		}
	}

	// ================================================================
	// F. TERTIARY TURRETS (6 smaller turrets along sides and gate)
	// ================================================================
	struct FTertiaryPos { float X; float Y; float BaseZ; };
	const FTertiaryPos TertiaryTurrets[6] = {
		{ -1800.f, -1000.f, 300.f },   // front-left
		{  1800.f, -1000.f, 300.f },   // front-right
		{ -2000.f,  200.f,  300.f },   // mid-left
		{  2000.f,  200.f,  300.f },   // mid-right
		{ -1500.f,  1400.f, 300.f },   // rear-left
		{  1500.f,  1400.f, 300.f }    // rear-right
	};

	for (int32 i = 0; i < 6; ++i)
	{
		const float TX = TertiaryTurrets[i].X;
		const float TY = TertiaryTurrets[i].Y;
		const float TZ = TertiaryTurrets[i].BaseZ;

		// Turret cylinder
		MakeCylinder(Castle,
			Origin + FVector(TX, TY, TZ + 1250.f),
			FVector(2.5f, 2.5f, 25.f),
			StoneWall, StoneRough, StoneMetal, TEXT("TertiaryTurret"));

		// Blue cone roof
		MakeCone(Castle,
			Origin + FVector(TX, TY, TZ + 2800.f),
			FVector(4.f, 4.f, 8.f),
			FRotator::ZeroRotator, RoofBlue, RoofRough, 0.f, TEXT("TertiaryRoof"));

		// Gold finial
		MakeSphere(Castle,
			Origin + FVector(TX, TY, TZ + 3250.f),
			FVector(0.5f, 0.5f, 0.5f),
			GoldAccent, GoldRough, GoldMetal, TEXT("TertiaryFinial"));

		// Small balcony ring
		MakeCylinder(Castle,
			Origin + FVector(TX, TY, TZ + 2000.f),
			FVector(2.8f, 2.8f, 0.2f),
			WhiteTrim, TrimRough, 0.f, TEXT("TertiaryBalcony"));

		// Window on turret
		MakeCube(Castle,
			Origin + FVector(TX + (TX > 0 ? 130.f : -130.f), TY, TZ + 1800.f),
			FVector(0.4f, 0.6f, 1.0f),
			FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("TertiaryWin"));
	}

	// ================================================================
	// G. GRAND ENTRANCE ARCH (enhanced Gothic arch on south face)
	// ================================================================
	// Two angled cubes forming a pointed Gothic arch silhouette above the main arch
	MakeCube(Castle,
		Origin + FVector(-300.f, -1970.f, 1600.f),
		FVector(1.5f, 1.5f, 8.f),
		FRotator(0.f, 0.f, 10.f), StoneWall, StoneRough, StoneMetal, TEXT("GothicArchL"));
	MakeCube(Castle,
		Origin + FVector(300.f, -1970.f, 1600.f),
		FVector(1.5f, 1.5f, 8.f),
		FRotator(0.f, 0.f, -10.f), StoneWall, StoneRough, StoneMetal, TEXT("GothicArchR"));

	// Keystone at apex
	MakeCube(Castle,
		Origin + FVector(0.f, -1975.f, 2200.f),
		FVector(1.5f, 1.f, 1.5f),
		FRotator::ZeroRotator, GoldAccent, GoldMetal, GoldRough, TEXT("Keystone"));

	// Decorative columns flanking the entrance
	MakeCylinder(Castle,
		Origin + FVector(-650.f, -1970.f, 700.f),
		FVector(0.8f, 0.8f, 12.f),
		WhiteTrim, TrimRough, 0.05f, TEXT("EntranceColL"));
	MakeCylinder(Castle,
		Origin + FVector(650.f, -1970.f, 700.f),
		FVector(0.8f, 0.8f, 12.f),
		WhiteTrim, TrimRough, 0.05f, TEXT("EntranceColR"));

	// Column capitals (gold)
	MakeCube(Castle,
		Origin + FVector(-650.f, -1970.f, 1320.f),
		FVector(1.2f, 1.2f, 0.6f),
		FRotator::ZeroRotator, GoldAccent, GoldMetal, GoldRough, TEXT("CapitalL"));
	MakeCube(Castle,
		Origin + FVector(650.f, -1970.f, 1320.f),
		FVector(1.2f, 1.2f, 0.6f),
		FRotator::ZeroRotator, GoldAccent, GoldMetal, GoldRough, TEXT("CapitalR"));

	// ================================================================
	// H. FLYING BUTTRESSES (4 on each side, connecting keep to base)
	// ================================================================
	for (int32 side = 0; side < 2; ++side)
	{
		const float BSign = (side == 0) ? 1.f : -1.f;
		for (int32 b = 0; b < 4; ++b)
		{
			const float BY = -1000.f + b * 800.f;
			const float BBaseX = BSign * 2300.f;
			const float BTopX = BSign * 1760.f;

			// Buttress body (angled cube)
			MakeCube(Castle,
				Origin + FVector((BBaseX + BTopX) * 0.5f, BY, 1200.f),
				FVector(6.f, 1.5f, 1.5f),
				FRotator(0.f, 0.f, BSign * 30.f), StoneWall, StoneRough, StoneMetal, TEXT("Buttress"));

			// Buttress top ornament (gold sphere)
			MakeSphere(Castle,
				Origin + FVector(BTopX, BY, 2100.f),
				FVector(0.4f, 0.4f, 0.4f),
				GoldAccent, GoldRough, GoldMetal, TEXT("ButtressFinial"));
		}
	}

	// ================================================================
	// I. BALCONIES AND PARAPETS
	// ================================================================
	// Parapet crenellations along the main keep roofline
	for (int32 side = 0; side < 4; ++side)
	{
		for (int32 m = 0; m < 12; ++m)
		{
			FVector MLoc;
			const float MerlonSize = 120.f;
			const float MerlonH = 180.f;
			const float Span = 2800.f;
			const float Offset = -Span * 0.5f + m * (Span / 11.f);
			const float MZ = 3800.f;

			switch (side)
			{
			case 0: // South
				MLoc = Origin + FVector(Offset, -1300.f, MZ);
				break;
			case 1: // North
				MLoc = Origin + FVector(Offset, 1700.f, MZ);
				break;
			case 2: // East
				MLoc = Origin + FVector(1760.f, 200.f + Offset, MZ);
				break;
			default: // West
				MLoc = Origin + FVector(-1760.f, 200.f + Offset, MZ);
				break;
			}
			MakeCube(Castle, MLoc,
				FVector(MerlonSize / 100.f, MerlonSize / 100.f, MerlonH / 100.f),
				FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("Parapet"));
		}
	}

	// Grand front balcony (below the clock face)
	MakeCube(Castle,
		Origin + FVector(0.f, -1400.f, 3900.f),
		FVector(12.f, 3.f, 0.5f),
		FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("GrandBalconyFloor"));

	// Balcony railing posts
	for (int32 p = 0; p < 8; ++p)
	{
		const float PX = -550.f + p * 157.f;
		MakeCylinder(Castle,
			Origin + FVector(PX, -1550.f, 4050.f),
			FVector(0.2f, 0.2f, 2.0f),
			WhiteTrim, TrimRough, 0.f, TEXT("BalconyPost"));
	}

	// Balcony railing top rail
	MakeCube(Castle,
		Origin + FVector(0.f, -1550.f, 4160.f),
		FVector(12.f, 0.3f, 0.3f),
		FRotator::ZeroRotator, GoldAccent, GoldMetal, GoldRough, TEXT("BalconyRail"));

	// Upper keep parapets (smaller, more elegant)
	for (int32 side = 0; side < 4; ++side)
	{
		for (int32 m = 0; m < 8; ++m)
		{
			FVector MLoc;
			const float MSize = 80.f;
			const float MH = 120.f;
			const float MSpan = 2000.f;
			const float MOffset = -MSpan * 0.5f + m * (MSpan / 7.f);
			const float MZ = 5350.f;

			switch (side)
			{
			case 0:
				MLoc = Origin + FVector(MOffset, -800.f, MZ);
				break;
			case 1:
				MLoc = Origin + FVector(MOffset, 1200.f, MZ);
				break;
			case 2:
				MLoc = Origin + FVector(1260.f, 200.f + MOffset, MZ);
				break;
			default:
				MLoc = Origin + FVector(-1260.f, 200.f + MOffset, MZ);
				break;
			}
			MakeCube(Castle, MLoc,
				FVector(MSize / 100.f, MSize / 100.f, MH / 100.f),
				FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("UpperParapet"));
		}
	}

	// ================================================================
	// J. BANNER POLES AND FLAGS
	// ================================================================
	struct FBannerPos { float X; float Y; float Z; };
	const FBannerPos BannerPositions[8] = {
		{    0.f,   800.f, 13200.f },  // grand spire top
		{  1200.f, 1100.f,  7950.f },  // NE secondary
		{ -1200.f, 1100.f,  7950.f },  // NW secondary
		{  1200.f, -700.f,  7950.f },  // SE secondary
		{ -1200.f, -700.f,  7950.f },  // SW secondary
		{ -1200.f, -1500.f, 3750.f },  // left gate tower
		{  1200.f, -1500.f, 3750.f },  // right gate tower
		{    0.f, -1300.f,  4300.f }   // front balcony center
	};

	for (int32 i = 0; i < 8; ++i)
	{
		const float FX = BannerPositions[i].X;
		const float FY = BannerPositions[i].Y;
		const float FZ = BannerPositions[i].Z;

		// Flagpole
		MakeCylinder(Castle,
			Origin + FVector(FX, FY, FZ + 250.f),
			FVector(0.15f, 0.15f, 5.f),
			GoldAccent, GoldRough, GoldMetal, TEXT("Flagpole"));

		// Banner (alternating royal blue and gold)
		const FLinearColor& BColor = (i % 2 == 0) ? BannerBlue : GoldAccent;
		const float BMetal = (i % 2 == 0) ? 0.f : GoldMetal;
		const float BRough = (i % 2 == 0) ? 0.6f : GoldRough;
		MakeCube(Castle,
			Origin + FVector(FX + 120.f, FY, FZ + 400.f),
			FVector(2.0f, 0.15f, 1.5f),
			FRotator::ZeroRotator, BColor, BRough, BMetal, TEXT("Banner"));

		// Gold sphere at flagpole tip
		MakeSphere(Castle,
			Origin + FVector(FX, FY, FZ + 530.f),
			FVector(0.25f, 0.25f, 0.25f),
			GoldAccent, GoldRough, GoldMetal, TEXT("FlagpoleFinial"));
	}

	// ================================================================
	// K. DECORATIVE DETAILS
	// ================================================================

	// --- Gold trim bands along window frames on keep sides ---
	for (int32 face = 0; face < 4; ++face)
	{
		for (int32 row = 0; row < 2; ++row)
		{
			for (int32 col = 0; col < 2; ++col)
			{
				const float WinZ = 1800.f + row * 1400.f;
				const float WinOff = -600.f + col * 1200.f;
				FVector FrameLoc;
				FVector FrameScale;
				switch (face)
				{
				case 0: // South
					FrameLoc = Origin + FVector(WinOff, -1308.f, WinZ);
					FrameScale = FVector(2.8f, 0.2f, 4.2f);
					break;
				case 1: // North
					FrameLoc = Origin + FVector(WinOff, 1708.f, WinZ);
					FrameScale = FVector(2.8f, 0.2f, 4.2f);
					break;
				case 2: // East
					FrameLoc = Origin + FVector(1768.f, WinOff + 200.f, WinZ);
					FrameScale = FVector(0.2f, 2.8f, 4.2f);
					break;
				default: // West
					FrameLoc = Origin + FVector(-1768.f, WinOff + 200.f, WinZ);
					FrameScale = FVector(0.2f, 2.8f, 4.2f);
					break;
				}
				MakeCube(Castle, FrameLoc, FrameScale,
					FRotator::ZeroRotator, GoldAccent, GoldRough, GoldMetal, TEXT("WinGoldFrame"));
			}
		}
	}

	// --- Rose/pink accent panels on upper keep walls ---
	MakeCube(Castle,
		Origin + FVector(0.f, -805.f, 4800.f),
		FVector(20.f, 0.5f, 8.f),
		FRotator::ZeroRotator, PinkAccent, PinkRough, 0.f, TEXT("UpperPinkF"));
	MakeCube(Castle,
		Origin + FVector(0.f, 1205.f, 4800.f),
		FVector(20.f, 0.5f, 8.f),
		FRotator::ZeroRotator, PinkAccent, PinkRough, 0.f, TEXT("UpperPinkB"));
	MakeCube(Castle,
		Origin + FVector(1265.f, 200.f, 4800.f),
		FVector(0.5f, 16.f, 8.f),
		FRotator::ZeroRotator, PinkAccent, PinkRough, 0.f, TEXT("UpperPinkE"));
	MakeCube(Castle,
		Origin + FVector(-1265.f, 200.f, 4800.f),
		FVector(0.5f, 16.f, 8.f),
		FRotator::ZeroRotator, PinkAccent, PinkRough, 0.f, TEXT("UpperPinkW"));

	// --- Gold sphere ornaments at buttress tops and parapet corners ---
	// Corner ornaments on the base platform
	const float OrnR = 0.35f;
	MakeSphere(Castle, Origin + FVector( 2800.f, -2000.f, 350.f), FVector(OrnR, OrnR, OrnR),
		GoldAccent, GoldRough, GoldMetal, TEXT("CornerOrn1"));
	MakeSphere(Castle, Origin + FVector(-2800.f, -2000.f, 350.f), FVector(OrnR, OrnR, OrnR),
		GoldAccent, GoldRough, GoldMetal, TEXT("CornerOrn2"));
	MakeSphere(Castle, Origin + FVector( 2800.f,  2000.f, 350.f), FVector(OrnR, OrnR, OrnR),
		GoldAccent, GoldRough, GoldMetal, TEXT("CornerOrn3"));
	MakeSphere(Castle, Origin + FVector(-2800.f,  2000.f, 350.f), FVector(OrnR, OrnR, OrnR),
		GoldAccent, GoldRough, GoldMetal, TEXT("CornerOrn4"));

	// --- Decorative arched windows on upper keep (Gothic tracery) ---
	for (int32 i = 0; i < 4; ++i)
	{
		const float AX = -900.f + i * 600.f;
		MakeCylinder(Castle,
			Origin + FVector(AX, -810.f, 5000.f),
			FVector(1.0f, 0.3f, 1.0f),
			DarkAccent, DarkRough, 0.f, TEXT("UpperArchWin"));
	}

	// --- Spire base ornamental ring of small spheres ---
	for (int32 i = 0; i < 12; ++i)
	{
		const float Angle = i * 30.f;
		const float Rad = FMath::DegreesToRadians(Angle);
		const float SR = 320.f;
		MakeSphere(Castle,
			Origin + FVector(FMath::Cos(Rad) * SR, 800.f + FMath::Sin(Rad) * SR, 4300.f),
			FVector(0.3f, 0.3f, 0.3f),
			GoldAccent, GoldRough, GoldMetal, TEXT("SpireBaseOrn"));
	}

	// --- Grand crest / shield above the entrance ---
	MakeCube(Castle,
		Origin + FVector(0.f, -1980.f, 2400.f),
		FVector(3.0f, 0.3f, 3.5f),
		FRotator::ZeroRotator, BannerBlue, 0.6f, 0.f, TEXT("CrestShield"));
	// Gold border around crest
	MakeCube(Castle,
		Origin + FVector(0.f, -1985.f, 2400.f),
		FVector(3.3f, 0.2f, 3.8f),
		FRotator::ZeroRotator, GoldAccent, GoldMetal, GoldRough, TEXT("CrestBorder"));
	// Crown on crest
	MakeCone(Castle,
		Origin + FVector(0.f, -1985.f, 2700.f),
		FVector(1.5f, 1.5f, 1.5f),
		FRotator::ZeroRotator, GoldAccent, GoldRough, GoldMetal, TEXT("CrestCrown"));

	// --- Additional decorative windows on gate towers ---
	for (int32 side = 0; side < 2; ++side)
	{
		const float GX = (side == 0) ? -1200.f : 1200.f;
		for (int32 w = 0; w < 3; ++w)
		{
			const float GWZ = 800.f + w * 700.f;
			MakeCube(Castle,
				Origin + FVector(GX, -1700.f, GWZ),
				FVector(0.5f, 0.5f, 1.2f),
				FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("GateTowerWin"));
		}
	}

	// --- Horizontal gold band across gate building top ---
	MakeCube(Castle,
		Origin + FVector(0.f, -1200.f, 2350.f),
		FVector(32.f, 0.4f, 0.5f),
		FRotator::ZeroRotator, GoldAccent, GoldMetal, GoldRough, TEXT("GateGoldBand"));

	// ================================================================
	// L. BRIDGE / WALKWAY (leading from front steps southward)
	// ================================================================
	MakeCube(Castle,
		Origin + FVector(0.f, -3500.f, 30.f),
		FVector(8.f, 30.f, 0.5f),
		FRotator::ZeroRotator, LightGray, 0.60f, 0.f, TEXT("BridgeWalkway"));

	// Bridge side rails
	MakeCube(Castle,
		Origin + FVector(-380.f, -3500.f, 100.f),
		FVector(0.3f, 30.f, 1.2f),
		FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("BridgeRailL"));
	MakeCube(Castle,
		Origin + FVector(380.f, -3500.f, 100.f),
		FVector(0.3f, 30.f, 1.2f),
		FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("BridgeRailR"));

	// Bridge railing posts (left side)
	for (int32 p = 0; p < 10; ++p)
	{
		const float PY = -2000.f - p * 300.f;
		MakeCylinder(Castle,
			Origin + FVector(-380.f, PY, 100.f),
			FVector(0.15f, 0.15f, 1.5f),
			WhiteTrim, TrimRough, 0.f, TEXT("BridgePostL"));
		MakeCylinder(Castle,
			Origin + FVector(380.f, PY, 100.f),
			FVector(0.15f, 0.15f, 1.5f),
			WhiteTrim, TrimRough, 0.f, TEXT("BridgePostR"));
	}

	// Gold sphere on each end of bridge rails
	MakeSphere(Castle,
		Origin + FVector(-380.f, -5000.f, 170.f),
		FVector(0.3f, 0.3f, 0.3f),
		GoldAccent, GoldRough, GoldMetal, TEXT("BridgeEndL"));
	MakeSphere(Castle,
		Origin + FVector(380.f, -5000.f, 170.f),
		FVector(0.3f, 0.3f, 0.3f),
		GoldAccent, GoldRough, GoldMetal, TEXT("BridgeEndR"));

	// ================================================================
	// M. ADDITIONAL TOWERS & ARCHITECTURAL FLOURISHES
	// ================================================================

	// --- Two flanking towers beside the grand spire (rear, taller than secondary) ---
	MakeCylinder(Castle,
		Origin + FVector(-600.f, 1600.f, 4000.f),
		FVector(3.0f, 3.0f, 45.f),
		StoneWall, StoneRough, StoneMetal, TEXT("FlankTowerL"));
	MakeCone(Castle,
		Origin + FVector(-600.f, 1600.f, 6700.f),
		FVector(5.f, 5.f, 12.f),
		FRotator::ZeroRotator, RoofBlue, RoofRough, 0.f, TEXT("FlankRoofL"));
	MakeSphere(Castle,
		Origin + FVector(-600.f, 1600.f, 7400.f),
		FVector(0.6f, 0.6f, 0.6f),
		GoldAccent, GoldRough, GoldMetal, TEXT("FlankFinialL"));

	MakeCylinder(Castle,
		Origin + FVector(600.f, 1600.f, 4000.f),
		FVector(3.0f, 3.0f, 45.f),
		StoneWall, StoneRough, StoneMetal, TEXT("FlankTowerR"));
	MakeCone(Castle,
		Origin + FVector(600.f, 1600.f, 6700.f),
		FVector(5.f, 5.f, 12.f),
		FRotator::ZeroRotator, RoofBlue, RoofRough, 0.f, TEXT("FlankRoofR"));
	MakeSphere(Castle,
		Origin + FVector(600.f, 1600.f, 7400.f),
		FVector(0.6f, 0.6f, 0.6f),
		GoldAccent, GoldRough, GoldMetal, TEXT("FlankFinialR"));

	// --- Connecting wall between gate building and main keep ---
	MakeCube(Castle,
		Origin + FVector(0.f, -800.f, 1200.f),
		FVector(30.f, 4.f, 18.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, StoneMetal, TEXT("ConnectWall"));

	// --- Side wing walls extending from the keep ---
	MakeCube(Castle,
		Origin + FVector(2200.f, 200.f, 1000.f),
		FVector(8.f, 20.f, 14.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, StoneMetal, TEXT("WingWallE"));
	MakeCube(Castle,
		Origin + FVector(-2200.f, 200.f, 1000.f),
		FVector(8.f, 20.f, 14.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, StoneMetal, TEXT("WingWallW"));

	// Pink panels on wing walls
	MakeCube(Castle,
		Origin + FVector(2600.f, 200.f, 1100.f),
		FVector(0.5f, 18.f, 10.f),
		FRotator::ZeroRotator, PinkAccent, PinkRough, 0.f, TEXT("WingPinkE"));
	MakeCube(Castle,
		Origin + FVector(-2600.f, 200.f, 1100.f),
		FVector(0.5f, 18.f, 10.f),
		FRotator::ZeroRotator, PinkAccent, PinkRough, 0.f, TEXT("WingPinkW"));

	// Wing roofs (blue pitched roofs)
	MakeCube(Castle,
		Origin + FVector(2200.f, 200.f, 1750.f),
		FVector(9.f, 22.f, 1.5f),
		FRotator(0.f, 0.f, 8.f), RoofBlue, RoofRough, 0.f, TEXT("WingRoofE"));
	MakeCube(Castle,
		Origin + FVector(-2200.f, 200.f, 1750.f),
		FVector(9.f, 22.f, 1.5f),
		FRotator(0.f, 0.f, -8.f), RoofBlue, RoofRough, 0.f, TEXT("WingRoofW"));

	// Windows on wing walls
	for (int32 side = 0; side < 2; ++side)
	{
		const float WX = (side == 0) ? 2200.f : -2200.f;
		for (int32 w = 0; w < 3; ++w)
		{
			const float WY = -600.f + w * 600.f;
			MakeCube(Castle,
				Origin + FVector(WX, WY, 1100.f),
				FVector(0.5f, 1.5f, 2.5f),
				FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("WingWin"));
		}
	}

	// --- Rear courtyard wall ---
	MakeCube(Castle,
		Origin + FVector(0.f, 1800.f, 800.f),
		FVector(40.f, 2.f, 12.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, StoneMetal, TEXT("RearWall"));

	// Rear wall crenellations
	for (int32 m = 0; m < 14; ++m)
	{
		const float MX = -1800.f + m * 280.f;
		MakeCube(Castle,
			Origin + FVector(MX, 1800.f, 1500.f),
			FVector(1.0f, 2.2f, 1.0f),
			FRotator::ZeroRotator, WhiteTrim, TrimRough, 0.f, TEXT("RearParapet"));
	}

	// --- Ornamental dormers on main keep roof ---
	for (int32 d = 0; d < 3; ++d)
	{
		const float DX = -800.f + d * 800.f;
		// Dormer body
		MakeCube(Castle,
			Origin + FVector(DX, -1300.f, 3650.f),
			FVector(2.5f, 1.5f, 3.f),
			FRotator::ZeroRotator, StoneWall, StoneRough, StoneMetal, TEXT("Dormer"));
		// Dormer cone roof
		MakeCone(Castle,
			Origin + FVector(DX, -1300.f, 3850.f),
			FVector(2.f, 2.f, 2.5f),
			FRotator::ZeroRotator, RoofBlue, RoofRough, 0.f, TEXT("DormerRoof"));
		// Dormer window
		MakeCube(Castle,
			Origin + FVector(DX, -1380.f, 3580.f),
			FVector(0.8f, 0.3f, 1.5f),
			FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("DormerWin"));
	}

	// --- Additional gold ornament spheres along rooflines ---
	for (int32 i = 0; i < 6; ++i)
	{
		const float OX = -1500.f + i * 600.f;
		MakeSphere(Castle,
			Origin + FVector(OX, -1300.f, 3810.f),
			FVector(0.2f, 0.2f, 0.2f),
			GoldAccent, GoldRough, GoldMetal, TEXT("RooflineOrn"));
	}
	for (int32 i = 0; i < 6; ++i)
	{
		const float OX = -1500.f + i * 600.f;
		MakeSphere(Castle,
			Origin + FVector(OX, 1700.f, 3810.f),
			FVector(0.2f, 0.2f, 0.2f),
			GoldAccent, GoldRough, GoldMetal, TEXT("RooflineOrnB"));
	}

	// --- Pinnacles on the upper keep corners ---
	const float PinX[4] = { 1260.f, -1260.f, 1260.f, -1260.f };
	const float PinY[4] = { -800.f, -800.f, 1200.f, 1200.f };
	for (int32 i = 0; i < 4; ++i)
	{
		// Pinnacle cone
		MakeCone(Castle,
			Origin + FVector(PinX[i], PinY[i], 5450.f),
			FVector(1.0f, 1.0f, 3.f),
			FRotator::ZeroRotator, StoneWall, StoneRough, StoneMetal, TEXT("Pinnacle"));
		// Pinnacle gold tip
		MakeSphere(Castle,
			Origin + FVector(PinX[i], PinY[i], 5620.f),
			FVector(0.25f, 0.25f, 0.25f),
			GoldAccent, GoldRough, GoldMetal, TEXT("PinnacleTip"));
	}

	// --- Courtyard ground plane ---
	MakeCube(Castle,
		Origin + FVector(0.f, 200.f, 305.f),
		FVector(55.f, 35.f, 0.3f),
		FRotator::ZeroRotator, LightGray, 0.60f, 0.f, TEXT("CourtyardFloor"));

	// --- Door on main keep (dark recess, south face) ---
	MakeCube(Castle,
		Origin + FVector(0.f, -1300.f, 600.f),
		FVector(5.f, 0.8f, 10.f),
		FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("KeepDoor"));

	// Door frame gold trim
	MakeCube(Castle,
		Origin + FVector(0.f, -1305.f, 600.f),
		FVector(5.5f, 0.3f, 10.5f),
		FRotator::ZeroRotator, GoldAccent, GoldMetal, GoldRough, TEXT("KeepDoorFrame"));

	// --- Rear entrance arch ---
	MakeCube(Castle,
		Origin + FVector(0.f, 1805.f, 500.f),
		FVector(5.f, 0.8f, 8.f),
		FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("RearDoor"));

	// ================================================================
	// N. MOAT — water channel with walls, drawbridge, corner towers
	// ================================================================
	const float MoatWidth = 2500.f;
	const float MoatZ = 3.f;
	const float CastleExtent = 5000.f;
	const float MoatDepth = 200.f;
	const float WallHeight = 300.f;
	const float WallThick = 80.f;

	// -- Moat channels (dark recessed bed) --
	// North channel
	MakeCube(Castle,
		Origin + FVector(0.f, CastleExtent + MoatWidth * 0.5f, -MoatDepth * 0.5f),
		FVector((CastleExtent * 2.f + MoatWidth * 2.f) / 100.f, MoatWidth / 100.f, MoatDepth / 100.f),
		FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("MoatBedN"));
	// South channel
	MakeCube(Castle,
		Origin + FVector(0.f, -CastleExtent - MoatWidth * 0.5f, -MoatDepth * 0.5f),
		FVector((CastleExtent * 2.f + MoatWidth * 2.f) / 100.f, MoatWidth / 100.f, MoatDepth / 100.f),
		FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("MoatBedS"));
	// East channel
	MakeCube(Castle,
		Origin + FVector(CastleExtent + MoatWidth * 0.5f, 0.f, -MoatDepth * 0.5f),
		FVector(MoatWidth / 100.f, (CastleExtent * 2.f) / 100.f, MoatDepth / 100.f),
		FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("MoatBedE"));
	// West channel
	MakeCube(Castle,
		Origin + FVector(-CastleExtent - MoatWidth * 0.5f, 0.f, -MoatDepth * 0.5f),
		FVector(MoatWidth / 100.f, (CastleExtent * 2.f) / 100.f, MoatDepth / 100.f),
		FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("MoatBedW"));
	// Corner bed fills (4 corners)
	MakeCube(Castle,
		Origin + FVector(CastleExtent + MoatWidth * 0.5f, CastleExtent + MoatWidth * 0.5f, -MoatDepth * 0.5f),
		FVector(MoatWidth / 100.f, MoatWidth / 100.f, MoatDepth / 100.f),
		FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("MoatBedNE"));
	MakeCube(Castle,
		Origin + FVector(-CastleExtent - MoatWidth * 0.5f, CastleExtent + MoatWidth * 0.5f, -MoatDepth * 0.5f),
		FVector(MoatWidth / 100.f, MoatWidth / 100.f, MoatDepth / 100.f),
		FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("MoatBedNW"));
	MakeCube(Castle,
		Origin + FVector(CastleExtent + MoatWidth * 0.5f, -CastleExtent - MoatWidth * 0.5f, -MoatDepth * 0.5f),
		FVector(MoatWidth / 100.f, MoatWidth / 100.f, MoatDepth / 100.f),
		FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("MoatBedSE"));
	MakeCube(Castle,
		Origin + FVector(-CastleExtent - MoatWidth * 0.5f, -CastleExtent - MoatWidth * 0.5f, -MoatDepth * 0.5f),
		FVector(MoatWidth / 100.f, MoatWidth / 100.f, MoatDepth / 100.f),
		FRotator::ZeroRotator, DarkAccent, DarkRough, 0.f, TEXT("MoatBedSW"));

	// -- Water surface --
	// North moat water
	AddWater(
		Origin + FVector(0.f, CastleExtent + MoatWidth * 0.5f, MoatZ),
		FVector((CastleExtent * 2.f + MoatWidth * 2.f) / 100.f, MoatWidth / 100.f, 1.f));
	// South moat water (with gap for drawbridge)
	AddWater(
		Origin + FVector(-(CastleExtent * 0.5f + MoatWidth * 0.25f + 250.f), -CastleExtent - MoatWidth * 0.5f, MoatZ),
		FVector((CastleExtent - 500.f) / 100.f, MoatWidth / 100.f, 1.f));
	AddWater(
		Origin + FVector((CastleExtent * 0.5f + MoatWidth * 0.25f + 250.f), -CastleExtent - MoatWidth * 0.5f, MoatZ),
		FVector((CastleExtent - 500.f) / 100.f, MoatWidth / 100.f, 1.f));
	// East moat water
	AddWater(
		Origin + FVector(CastleExtent + MoatWidth * 0.5f, 0.f, MoatZ),
		FVector(MoatWidth / 100.f, (CastleExtent * 2.f) / 100.f, 1.f));
	// West moat water
	AddWater(
		Origin + FVector(-CastleExtent - MoatWidth * 0.5f, 0.f, MoatZ),
		FVector(MoatWidth / 100.f, (CastleExtent * 2.f) / 100.f, 1.f));
	// Corner water fills
	AddWater(
		Origin + FVector(CastleExtent + MoatWidth * 0.5f, CastleExtent + MoatWidth * 0.5f, MoatZ),
		FVector(MoatWidth / 100.f, MoatWidth / 100.f, 1.f));
	AddWater(
		Origin + FVector(-CastleExtent - MoatWidth * 0.5f, CastleExtent + MoatWidth * 0.5f, MoatZ),
		FVector(MoatWidth / 100.f, MoatWidth / 100.f, 1.f));
	AddWater(
		Origin + FVector(CastleExtent + MoatWidth * 0.5f, -CastleExtent - MoatWidth * 0.5f, MoatZ),
		FVector(MoatWidth / 100.f, MoatWidth / 100.f, 1.f));
	AddWater(
		Origin + FVector(-CastleExtent - MoatWidth * 0.5f, -CastleExtent - MoatWidth * 0.5f, MoatZ),
		FVector(MoatWidth / 100.f, MoatWidth / 100.f, 1.f));

	// -- Inner moat walls (castle side) --
	MakeCube(Castle,
		Origin + FVector(0.f, CastleExtent, WallHeight * 0.5f),
		FVector((CastleExtent * 2.f) / 100.f, WallThick / 100.f, WallHeight / 100.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, 0.f, TEXT("MoatInnerWallN"));
	MakeCube(Castle,
		Origin + FVector(0.f, -CastleExtent, WallHeight * 0.5f),
		FVector((CastleExtent * 2.f) / 100.f, WallThick / 100.f, WallHeight / 100.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, 0.f, TEXT("MoatInnerWallS"));
	MakeCube(Castle,
		Origin + FVector(CastleExtent, 0.f, WallHeight * 0.5f),
		FVector(WallThick / 100.f, (CastleExtent * 2.f) / 100.f, WallHeight / 100.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, 0.f, TEXT("MoatInnerWallE"));
	MakeCube(Castle,
		Origin + FVector(-CastleExtent, 0.f, WallHeight * 0.5f),
		FVector(WallThick / 100.f, (CastleExtent * 2.f) / 100.f, WallHeight / 100.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, 0.f, TEXT("MoatInnerWallW"));

	// -- Outer moat walls --
	const float OuterEdge = CastleExtent + MoatWidth;
	MakeCube(Castle,
		Origin + FVector(0.f, OuterEdge, WallHeight * 0.4f),
		FVector((OuterEdge * 2.f) / 100.f, WallThick / 100.f, (WallHeight * 0.8f) / 100.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, 0.f, TEXT("MoatOuterWallN"));
	MakeCube(Castle,
		Origin + FVector(0.f, -OuterEdge, WallHeight * 0.4f),
		FVector((OuterEdge * 2.f) / 100.f, WallThick / 100.f, (WallHeight * 0.8f) / 100.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, 0.f, TEXT("MoatOuterWallS"));
	MakeCube(Castle,
		Origin + FVector(OuterEdge, 0.f, WallHeight * 0.4f),
		FVector(WallThick / 100.f, (OuterEdge * 2.f) / 100.f, (WallHeight * 0.8f) / 100.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, 0.f, TEXT("MoatOuterWallE"));
	MakeCube(Castle,
		Origin + FVector(-OuterEdge, 0.f, WallHeight * 0.4f),
		FVector(WallThick / 100.f, (OuterEdge * 2.f) / 100.f, (WallHeight * 0.8f) / 100.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, 0.f, TEXT("MoatOuterWallW"));

	// -- Drawbridge (south entrance) --
	MakeCube(Castle,
		Origin + FVector(0.f, -CastleExtent - MoatWidth * 0.5f, 10.f),
		FVector(10.f, MoatWidth * 1.1f / 100.f, 0.4f),
		FRotator::ZeroRotator, DarkAccent, 0.88f, 0.f, TEXT("Drawbridge"));
	// Drawbridge railings
	MakeCube(Castle,
		Origin + FVector(450.f, -CastleExtent - MoatWidth * 0.5f, 50.f),
		FVector(0.3f, MoatWidth * 1.1f / 100.f, 1.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, 0.f, TEXT("DrawbridgeRailL"));
	MakeCube(Castle,
		Origin + FVector(-450.f, -CastleExtent - MoatWidth * 0.5f, 50.f),
		FVector(0.3f, MoatWidth * 1.1f / 100.f, 1.f),
		FRotator::ZeroRotator, StoneWall, StoneRough, 0.f, TEXT("DrawbridgeRailR"));

	// -- Corner towers (4) --
	const float TowerR = 400.f;
	const float TowerH = 800.f;
	const FVector CornerOffsets[] = {
		FVector( OuterEdge,  OuterEdge, 0.f),
		FVector(-OuterEdge,  OuterEdge, 0.f),
		FVector( OuterEdge, -OuterEdge, 0.f),
		FVector(-OuterEdge, -OuterEdge, 0.f),
	};
	const TCHAR* TowerNames[] = { TEXT("MoatTowerNE"), TEXT("MoatTowerNW"), TEXT("MoatTowerSE"), TEXT("MoatTowerSW") };
	for (int32 t = 0; t < 4; ++t)
	{
		const FVector TBase = Origin + CornerOffsets[t];
		// Cylinder tower body
		MakeCylinder(Castle, TBase + FVector(0.f, 0.f, TowerH * 0.5f),
			FVector(TowerR / 50.f, TowerR / 50.f, TowerH / 100.f),
			StoneWall, StoneRough, 0.f, TowerNames[t]);
		// Cone roof
		MakeCone(Castle, TBase + FVector(0.f, 0.f, TowerH + 150.f),
			FVector(TowerR * 1.3f / 50.f, TowerR * 1.3f / 50.f, 4.f),
			FRotator::ZeroRotator, RoofBlue, RoofRough, 0.f,
			*FString::Printf(TEXT("%s_Roof"), TowerNames[t]));
		// Crenellations (4 merlons at top of each tower)
		for (int32 m = 0; m < 4; ++m)
		{
			const float MAngle = (float)m * 90.f + 45.f;
			const float MRad = FMath::DegreesToRadians(MAngle);
			MakeCube(Castle,
				TBase + FVector(FMath::Cos(MRad) * TowerR * 0.9f,
					FMath::Sin(MRad) * TowerR * 0.9f, TowerH + 30.f),
				FVector(0.8f, 0.8f, 0.6f),
				FRotator(0.f, MAngle, 0.f), StoneWall, StoneRough, 0.f,
				*FString::Printf(TEXT("%s_Merlon%d"), TowerNames[t], m));
		}
	}

	// ================================================================
	// 14. BEACH GROUND — realistic sand with dunes, ripples, variation
	// ================================================================
	const float BeachSize = 180000.f;
	{
		// -- Base sand plane --
		AActor* BeachActor = World->SpawnActor<AActor>(AActor::StaticClass(),
			Origin + FVector(0.f, 0.f, 1.f), FRotator::ZeroRotator);
		if (BeachActor)
		{
#if WITH_EDITOR
			BeachActor->SetActorLabel(TEXT("CastleBeach"));
#endif
			UStaticMeshComponent* BeachMC = NewObject<UStaticMeshComponent>(BeachActor, TEXT("BeachPlane"));
			BeachMC->SetStaticMesh(PlaneMesh);
			BeachMC->SetWorldScale3D(FVector(BeachSize / 100.f, BeachSize / 100.f, 1.f));
			BeachMC->SetWorldLocation(Origin + FVector(0.f, 0.f, 1.f));
			BeachMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			BeachMC->SetCollisionResponseToAllChannels(ECR_Block);
			BeachMC->SetBoundsScale(5.f);
			BeachMC->RegisterComponent();
			BeachActor->SetRootComponent(BeachMC);
			ApplyPBR(BeachMC, FLinearColor(0.82f, 0.76f, 0.58f), 0.f, 0.95f);
			BeachActor->AddInstanceComponent(BeachMC);
		}

		// Sand color palette for realistic variation
		const FLinearColor SandDry(0.88f, 0.82f, 0.64f);      // dry sand (lighter)
		const FLinearColor SandWet(0.62f, 0.56f, 0.40f);       // wet sand near water
		const FLinearColor SandMid(0.78f, 0.72f, 0.54f);       // mid-tone
		const FLinearColor SandWarm(0.85f, 0.76f, 0.52f);      // warm golden
		const FLinearColor SandCool(0.76f, 0.72f, 0.58f);      // cool grayish
		const FLinearColor SandDark(0.68f, 0.62f, 0.44f);      // shadow/dip areas
		const FLinearColor DriftwoodBrown(0.35f, 0.25f, 0.15f); // driftwood
		const FLinearColor ShellWhite(0.92f, 0.88f, 0.82f);    // shells
		const FLinearColor RockGray(0.45f, 0.42f, 0.38f);      // beach rocks
		const FLinearColor SeaweedGreen(0.20f, 0.30f, 0.15f);  // seaweed/kelp

		// -- Sand color variation patches (break up uniform look) --
		const float HB = BeachSize * 0.5f;
		for (int32 p = 0; p < 60; ++p)
		{
			const float PX = Origin.X + CityRandom.FRandRange(-HB * 0.8f, HB * 0.8f);
			const float PY = Origin.Y + CityRandom.FRandRange(-HB * 0.8f, HB * 0.8f);
			const float PSize = CityRandom.FRandRange(2000.f, 12000.f);
			const FLinearColor PatchColors[] = { SandDry, SandWet, SandMid, SandWarm, SandCool, SandDark };
			const FLinearColor& PC = PatchColors[p % 6];
			AddSand(PlaneMesh,
				FVector(PX, PY, 2.f + CityRandom.FRandRange(0.f, 0.5f)),
				FRotator(0.f, CityRandom.FRandRange(0.f, 360.f), 0.f),
				FVector(PSize / 100.f, PSize * CityRandom.FRandRange(0.5f, 1.5f) / 100.f, 1.f));
			// Override with patch color instead of default sand
			// (AddSand uses SandBase, but we want variation)
		}

		// -- Sand dunes (gentle raised humps using spheres) --
		for (int32 d = 0; d < 40; ++d)
		{
			const float DX = Origin.X + CityRandom.FRandRange(-HB * 0.7f, HB * 0.7f);
			const float DY = Origin.Y + CityRandom.FRandRange(-HB * 0.7f, HB * 0.7f);
			const float DuneW = CityRandom.FRandRange(1500.f, 6000.f);
			const float DuneL = DuneW * CityRandom.FRandRange(1.5f, 4.f);
			const float DuneH = CityRandom.FRandRange(20.f, 120.f);
			const float DuneYaw = CityRandom.FRandRange(0.f, 360.f);
			const FLinearColor DuneColor = FMath::Lerp(SandMid, SandWarm, CityRandom.FRandRange(0.f, 1.f));

			AActor* DA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(DX, DY, 0.f), FRotator::ZeroRotator);
			if (!DA) continue;
#if WITH_EDITOR
			DA->SetActorLabel(*FString::Printf(TEXT("CastleDune_%d"), d));
#endif
			UStaticMeshComponent* DMC = NewObject<UStaticMeshComponent>(DA, *FString::Printf(TEXT("Dune_%d"), d));
			DMC->SetStaticMesh(SphereMesh);
			DMC->SetWorldScale3D(FVector(DuneL / 100.f, DuneW / 100.f, DuneH / 100.f));
			DMC->SetWorldLocation(FVector(DX, DY, DuneH * 0.3f));
			DMC->SetWorldRotation(FRotator(0.f, DuneYaw, 0.f));
			DMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			DMC->SetBoundsScale(5.f);
			DMC->RegisterComponent();
			DA->SetRootComponent(DMC);
			ApplyPBR(DMC, DuneColor, 0.f, 0.92f);
			DA->AddInstanceComponent(DMC);
		}

		// -- Sand ripple lines (thin raised ridges mimicking wind patterns) --
		for (int32 r = 0; r < 80; ++r)
		{
			const float RX = Origin.X + CityRandom.FRandRange(-HB * 0.6f, HB * 0.6f);
			const float RY = Origin.Y + CityRandom.FRandRange(-HB * 0.6f, HB * 0.6f);
			const float RippleLen = CityRandom.FRandRange(800.f, 4000.f);
			const float RippleYaw = CityRandom.FRandRange(-20.f, 20.f); // roughly parallel
			const FLinearColor RippleColor = FMath::Lerp(SandMid, SandDry, CityRandom.FRandRange(0.3f, 0.7f));

			AActor* RA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(RX, RY, 2.5f), FRotator::ZeroRotator);
			if (!RA) continue;
#if WITH_EDITOR
			RA->SetActorLabel(*FString::Printf(TEXT("CastleRipple_%d"), r));
#endif
			UStaticMeshComponent* RMC = NewObject<UStaticMeshComponent>(RA, *FString::Printf(TEXT("Ripple_%d"), r));
			RMC->SetStaticMesh(CubeMesh);
			RMC->SetWorldScale3D(FVector(RippleLen / 100.f, 0.15f, 0.04f));
			RMC->SetWorldLocation(FVector(RX, RY, 2.5f));
			RMC->SetWorldRotation(FRotator(0.f, RippleYaw, 0.f));
			RMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			RMC->RegisterComponent();
			RA->SetRootComponent(RMC);
			ApplyPBR(RMC, RippleColor, 0.f, 0.90f);
			RA->AddInstanceComponent(RMC);
		}

		// -- Beach rocks (scattered boulders and pebble clusters) --
		for (int32 br = 0; br < 25; ++br)
		{
			const float BRX = Origin.X + CityRandom.FRandRange(-HB * 0.75f, HB * 0.75f);
			const float BRY = Origin.Y + CityRandom.FRandRange(-HB * 0.75f, HB * 0.75f);
			const float RockSize = CityRandom.FRandRange(30.f, 200.f);

			AActor* BRA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(BRX, BRY, 0.f), FRotator::ZeroRotator);
			if (!BRA) continue;
#if WITH_EDITOR
			BRA->SetActorLabel(*FString::Printf(TEXT("CastleBeachRock_%d"), br));
#endif
			UStaticMeshComponent* BRMC = NewObject<UStaticMeshComponent>(BRA, *FString::Printf(TEXT("BRock_%d"), br));
			BRMC->SetStaticMesh(SphereMesh);
			BRMC->SetWorldScale3D(FVector(
				RockSize / 100.f * CityRandom.FRandRange(0.7f, 1.3f),
				RockSize / 100.f * CityRandom.FRandRange(0.7f, 1.3f),
				RockSize / 100.f * CityRandom.FRandRange(0.4f, 0.8f)));
			BRMC->SetWorldLocation(FVector(BRX, BRY, RockSize * 0.2f));
			BRMC->SetWorldRotation(FRotator(
				CityRandom.FRandRange(-15.f, 15.f),
				CityRandom.FRandRange(0.f, 360.f),
				CityRandom.FRandRange(-10.f, 10.f)));
			BRMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			BRMC->RegisterComponent();
			BRA->SetRootComponent(BRMC);
			ApplyPBR(BRMC, FMath::Lerp(RockGray, SandDark, CityRandom.FRandRange(0.f, 0.4f)),
				0.f, CityRandom.FRandRange(0.80f, 0.95f));
			BRA->AddInstanceComponent(BRMC);
		}

		// -- Driftwood pieces (elongated brown cubes) --
		for (int32 dw = 0; dw < 15; ++dw)
		{
			const float DWX = Origin.X + CityRandom.FRandRange(-HB * 0.7f, HB * 0.7f);
			const float DWY = Origin.Y + CityRandom.FRandRange(-HB * 0.7f, HB * 0.7f);
			const float LogLen = CityRandom.FRandRange(150.f, 600.f);
			const float LogThick = CityRandom.FRandRange(8.f, 25.f);

			AActor* DWA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(DWX, DWY, 0.f), FRotator::ZeroRotator);
			if (!DWA) continue;
#if WITH_EDITOR
			DWA->SetActorLabel(*FString::Printf(TEXT("CastleDriftwood_%d"), dw));
#endif
			UStaticMeshComponent* DWMC = NewObject<UStaticMeshComponent>(DWA, *FString::Printf(TEXT("DW_%d"), dw));
			DWMC->SetStaticMesh(CylinderMesh);
			DWMC->SetWorldScale3D(FVector(LogThick / 100.f, LogThick / 100.f, LogLen / 100.f));
			DWMC->SetWorldLocation(FVector(DWX, DWY, LogThick * 0.3f));
			DWMC->SetWorldRotation(FRotator(
				90.f + CityRandom.FRandRange(-10.f, 10.f),
				CityRandom.FRandRange(0.f, 360.f), 0.f));
			DWMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			DWMC->RegisterComponent();
			DWA->SetRootComponent(DWMC);
			ApplyPBR(DWMC, FMath::Lerp(DriftwoodBrown,
				FLinearColor(0.45f, 0.38f, 0.28f), CityRandom.FRandRange(0.f, 1.f)),
				0.f, 0.88f);
			DWA->AddInstanceComponent(DWMC);
		}

		// -- Seashells (small white/cream spheres) --
		for (int32 sh = 0; sh < 35; ++sh)
		{
			const float SHX = Origin.X + CityRandom.FRandRange(-HB * 0.6f, HB * 0.6f);
			const float SHY = Origin.Y + CityRandom.FRandRange(-HB * 0.6f, HB * 0.6f);
			const float ShellSize = CityRandom.FRandRange(5.f, 20.f);

			AActor* SHA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(SHX, SHY, 2.f), FRotator::ZeroRotator);
			if (!SHA) continue;
#if WITH_EDITOR
			SHA->SetActorLabel(*FString::Printf(TEXT("CastleShell_%d"), sh));
#endif
			UStaticMeshComponent* SHMC = NewObject<UStaticMeshComponent>(SHA, *FString::Printf(TEXT("Shell_%d"), sh));
			SHMC->SetStaticMesh(SphereMesh);
			SHMC->SetWorldScale3D(FVector(ShellSize / 100.f,
				ShellSize * CityRandom.FRandRange(0.6f, 1.f) / 100.f,
				ShellSize * 0.3f / 100.f));
			SHMC->SetWorldLocation(FVector(SHX, SHY, 2.5f));
			SHMC->SetWorldRotation(FRotator(0.f, CityRandom.FRandRange(0.f, 360.f), 0.f));
			SHMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SHMC->RegisterComponent();
			SHA->SetRootComponent(SHMC);
			ApplyPBR(SHMC, FMath::Lerp(ShellWhite,
				FLinearColor(0.90f, 0.82f, 0.70f), CityRandom.FRandRange(0.f, 1.f)),
				0.f, 0.70f);
			SHA->AddInstanceComponent(SHMC);
		}

		// -- Seaweed/kelp clumps along tideline --
		for (int32 sw = 0; sw < 12; ++sw)
		{
			const float SWX = Origin.X + CityRandom.FRandRange(-HB * 0.5f, HB * 0.5f);
			const float SWY = Origin.Y + HB * 0.7f + CityRandom.FRandRange(-3000.f, 3000.f);
			const float ClumpLen = CityRandom.FRandRange(200.f, 800.f);

			AActor* SWA = World->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(SWX, SWY, 2.f), FRotator::ZeroRotator);
			if (!SWA) continue;
#if WITH_EDITOR
			SWA->SetActorLabel(*FString::Printf(TEXT("CastleSeaweed_%d"), sw));
#endif
			UStaticMeshComponent* SWMC = NewObject<UStaticMeshComponent>(SWA, *FString::Printf(TEXT("Kelp_%d"), sw));
			SWMC->SetStaticMesh(CubeMesh);
			SWMC->SetWorldScale3D(FVector(ClumpLen / 100.f, 0.4f, 0.03f));
			SWMC->SetWorldLocation(FVector(SWX, SWY, 2.2f));
			SWMC->SetWorldRotation(FRotator(0.f, CityRandom.FRandRange(-30.f, 30.f), 0.f));
			SWMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SWMC->RegisterComponent();
			SWA->SetRootComponent(SWMC);
			ApplyPBR(SWMC, SeaweedGreen, 0.f, 0.85f);
			SWA->AddInstanceComponent(SWMC);
		}

		// -- Wet sand zone (darker strip near the water edge) --
		{
			AActor* WZA = World->SpawnActor<AActor>(AActor::StaticClass(),
				Origin + FVector(0.f, HB * 0.85f, 1.5f), FRotator::ZeroRotator);
			if (WZA)
			{
#if WITH_EDITOR
				WZA->SetActorLabel(TEXT("CastleWetSand"));
#endif
				UStaticMeshComponent* WZMC = NewObject<UStaticMeshComponent>(WZA, TEXT("WetSandStrip"));
				WZMC->SetStaticMesh(PlaneMesh);
				WZMC->SetWorldScale3D(FVector(BeachSize / 100.f, BeachSize * 0.15f / 100.f, 1.f));
				WZMC->SetWorldLocation(Origin + FVector(0.f, HB * 0.85f, 1.5f));
				WZMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				WZMC->SetBoundsScale(5.f);
				WZMC->RegisterComponent();
				WZA->SetRootComponent(WZMC);
				ApplyPBR(WZMC, SandWet, 0.f, 0.75f); // wet = lower roughness, shinier
				WZA->AddInstanceComponent(WZMC);
			}
		}
	}

	// ================================================================
	// 15. PALM TREES — castle area (43 total: 33 ring + 10 entrance)
	// ================================================================
	{
		// Ring of 33 palms around the castle
		const float TreeRadius = 12000.f;
		const int32 NumRingTrees = 33;
		for (int32 i = 0; i < NumRingTrees; ++i)
		{
			const float Angle = (float)i * (360.f / (float)NumRingTrees) + CityRandom.FRandRange(-5.f, 5.f);
			const float Rad = FMath::DegreesToRadians(Angle);
			const float Dist = TreeRadius + CityRandom.FRandRange(-2000.f, 2000.f);
			const float TX = Origin.X + FMath::Cos(Rad) * Dist;
			const float TY = Origin.Y + FMath::Sin(Rad) * Dist;
			const float TreeH = CityRandom.FRandRange(460.f, 805.f); // +15%
			SpawnPalmTree(FVector(TX, TY, 1.f), TreeH, true);
		}
		// 10 clustered near the castle entrance (south side)
		for (int32 i = 0; i < 10; ++i)
		{
			const float TX = Origin.X + CityRandom.FRandRange(-5000.f, 5000.f);
			const float TY = Origin.Y - MoatOuter - 1500.f + CityRandom.FRandRange(-2000.f, 2000.f);
			const float TreeH = CityRandom.FRandRange(517.f, 747.f); // +15%
			SpawnPalmTree(FVector(TX, TY, 1.f), TreeH, true);
		}
	}

	// ================================================================
	// 16. BEACH UMBRELLAS — 30 colorful parasols (3x original)
	// ================================================================
	{
		const FLinearColor UmbrellaColors[] = {
			FLinearColor(0.9f, 0.15f, 0.15f),   // red
			FLinearColor(0.15f, 0.45f, 0.9f),   // blue
			FLinearColor(0.95f, 0.85f, 0.1f),   // yellow
			FLinearColor(0.1f, 0.8f, 0.3f),     // green
			FLinearColor(0.9f, 0.5f, 0.1f),     // orange
			FLinearColor(0.8f, 0.2f, 0.6f),     // pink
			FLinearColor(0.2f, 0.7f, 0.8f),     // teal
			FLinearColor(0.95f, 0.95f, 0.95f),  // white
			FLinearColor(0.6f, 0.15f, 0.7f),    // purple
			FLinearColor(0.9f, 0.35f, 0.2f),    // coral
		};
		const int32 NumUmbrellas = 30;

		for (int32 i = 0; i < NumUmbrellas; ++i)
		{
			// Spread umbrellas across the beach in a scattered pattern
			const float Angle = (float)i * (360.f / (float)NumUmbrellas) + CityRandom.FRandRange(-10.f, 10.f);
			const float Rad = FMath::DegreesToRadians(Angle);
			const float Dist = CityRandom.FRandRange(14000.f, 40000.f);
			const float UX = Origin.X + FMath::Cos(Rad) * Dist;
			const float UY = Origin.Y + FMath::Sin(Rad) * Dist;
			const FVector UmbrellaBase(UX, UY, 1.f);
			const FLinearColor& UColor = UmbrellaColors[i % 10];

			AActor* UA = World->SpawnActor<AActor>(AActor::StaticClass(), UmbrellaBase, FRotator::ZeroRotator);
			if (!UA) continue;
#if WITH_EDITOR
			UA->SetActorLabel(*FString::Printf(TEXT("BeachUmbrella_%d"), i));
#endif
			// Pole
			UStaticMeshComponent* Pole = NewObject<UStaticMeshComponent>(UA, *FString::Printf(TEXT("UPole_%d"), i));
			Pole->SetStaticMesh(CylinderMesh);
			Pole->SetWorldScale3D(FVector(0.15f, 0.15f, 3.5f));
			Pole->SetWorldLocation(UmbrellaBase + FVector(0.f, 0.f, 175.f));
			Pole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Pole->RegisterComponent();
			UA->SetRootComponent(Pole);
			ApplyPBR(Pole, FLinearColor(0.85f, 0.82f, 0.75f), 0.3f, 0.6f);
			UA->AddInstanceComponent(Pole);

			// Canopy
			UStaticMeshComponent* Canopy = NewObject<UStaticMeshComponent>(UA, *FString::Printf(TEXT("UCanopy_%d"), i));
			Canopy->SetStaticMesh(ConeMesh);
			Canopy->SetWorldScale3D(FVector(3.5f, 3.5f, 0.8f));
			Canopy->SetWorldLocation(UmbrellaBase + FVector(0.f, 0.f, 350.f));
			Canopy->SetWorldRotation(FRotator(0.f, CityRandom.FRandRange(0.f, 360.f), 5.f));
			Canopy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Canopy->RegisterComponent();
			ApplyPBR(Canopy, UColor, 0.f, 0.75f);
			UA->AddInstanceComponent(Canopy);
		}
	}

	// ================================================================
	// 17. BEACH CHAIRS — 42 loungers (3x original)
	// ================================================================
	{
		const FLinearColor ChairFrame(0.85f, 0.82f, 0.75f);
		const FLinearColor ChairFabricColors[] = {
			FLinearColor(0.15f, 0.40f, 0.75f), // blue
			FLinearColor(0.80f, 0.20f, 0.20f), // red
			FLinearColor(0.95f, 0.90f, 0.80f), // cream
			FLinearColor(0.20f, 0.65f, 0.40f), // green
		};
		const int32 NumChairs = 42;

		for (int32 i = 0; i < NumChairs; ++i)
		{
			// Scatter chairs across the beach
			const float Angle = (float)i * (360.f / (float)NumChairs) + CityRandom.FRandRange(-8.f, 8.f);
			const float Rad = FMath::DegreesToRadians(Angle);
			const float Dist = CityRandom.FRandRange(13000.f, 38000.f);
			const float CX = Origin.X + FMath::Cos(Rad) * Dist;
			const float CY = Origin.Y + FMath::Sin(Rad) * Dist;
			const FVector ChairBase(CX, CY, 1.f);
			const float ChairYaw = CityRandom.FRandRange(-90.f, 90.f);
			const FRotator ChairRot(0.f, ChairYaw, 0.f);
			const FLinearColor& Fabric = ChairFabricColors[i % 4];

			AActor* CA = World->SpawnActor<AActor>(AActor::StaticClass(), ChairBase, FRotator::ZeroRotator);
			if (!CA) continue;
#if WITH_EDITOR
			CA->SetActorLabel(*FString::Printf(TEXT("BeachChair_%d"), i));
#endif
			// Seat
			UStaticMeshComponent* Seat = NewObject<UStaticMeshComponent>(CA, *FString::Printf(TEXT("CSeat_%d"), i));
			Seat->SetStaticMesh(CubeMesh);
			Seat->SetWorldScale3D(FVector(1.2f, 0.6f, 0.05f));
			Seat->SetWorldLocation(ChairBase + FVector(0.f, 0.f, 25.f));
			Seat->SetWorldRotation(ChairRot);
			Seat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Seat->RegisterComponent();
			CA->SetRootComponent(Seat);
			ApplyPBR(Seat, Fabric, 0.f, 0.7f);
			CA->AddInstanceComponent(Seat);

			// Backrest
			const FVector BackDir = ChairRot.RotateVector(FVector(-50.f, 0.f, 40.f));
			UStaticMeshComponent* Back = NewObject<UStaticMeshComponent>(CA, *FString::Printf(TEXT("CBack_%d"), i));
			Back->SetStaticMesh(CubeMesh);
			Back->SetWorldScale3D(FVector(0.05f, 0.6f, 0.7f));
			Back->SetWorldLocation(ChairBase + BackDir + FVector(0.f, 0.f, 25.f));
			Back->SetWorldRotation(ChairRot + FRotator(25.f, 0.f, 0.f));
			Back->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Back->RegisterComponent();
			ApplyPBR(Back, Fabric, 0.f, 0.7f);
			CA->AddInstanceComponent(Back);

			// 4 legs
			const FVector LegOffsets[] = {
				FVector( 45.f,  25.f, 0.f),
				FVector( 45.f, -25.f, 0.f),
				FVector(-45.f,  25.f, 0.f),
				FVector(-45.f, -25.f, 0.f),
			};
			for (int32 L = 0; L < 4; ++L)
			{
				const FVector LegWorld = ChairBase + ChairRot.RotateVector(LegOffsets[L]) + FVector(0.f, 0.f, 12.5f);
				UStaticMeshComponent* Leg = NewObject<UStaticMeshComponent>(CA, *FString::Printf(TEXT("CLeg_%d_%d"), i, L));
				Leg->SetStaticMesh(CylinderMesh);
				Leg->SetWorldScale3D(FVector(0.06f, 0.06f, 0.25f));
				Leg->SetWorldLocation(LegWorld);
				Leg->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Leg->RegisterComponent();
				ApplyPBR(Leg, ChairFrame, 0.f, 0.65f);
				CA->AddInstanceComponent(Leg);
			}
		}
	}

	// ================================================================
	// 18. PALM TREE ROWS — 2 rows along the full beach length
	// ================================================================
	// Beach is 180,000 UU centered at Origin. Two rows run along the Y axis
	// at the north and south edges of the beach, spanning the full X extent.
	{
		const float HalfBeach = BeachSize * 0.5f; // 90,000
		const float RowOffsetY1 = HalfBeach - 3000.f;  // north row, 3000 UU inside edge
		const float RowOffsetY2 = -HalfBeach + 3000.f; // south row, 3000 UU inside edge
		const float TreeSpacing = 3000.f;
		const int32 NumPerRow = (int32)(BeachSize / TreeSpacing);

		for (int32 row = 0; row < 2; ++row)
		{
			const float RowY = Origin.Y + (row == 0 ? RowOffsetY1 : RowOffsetY2);
			for (int32 i = 0; i < NumPerRow; ++i)
			{
				const float TX = Origin.X - HalfBeach + (float)i * TreeSpacing + CityRandom.FRandRange(-500.f, 500.f);
				const float TY = RowY + CityRandom.FRandRange(-800.f, 800.f);
				const float TreeH = CityRandom.FRandRange(517.f, 862.f); // +15% from base 450-750
				SpawnPalmTree(FVector(TX, TY, 1.f), TreeH, (i % 3 != 0)); // every 3rd is simple for perf
			}
		}
	}

	// --- Ambient night lighting: 4x3 grid of warm tiki-torch lights across castle beach ---
	{
		const FVector GridCenter(95000.f, 30000.f, 300.f);
		const float SpacingX = 8000.f;
		const float SpacingY = 8000.f;
		const int32 CountX = 4;
		const int32 CountY = 3;
		const float HalfExtentX = (CountX - 1) * SpacingX * 0.5f;
		const float HalfExtentY = (CountY - 1) * SpacingY * 0.5f;
		int32 LightIdx = 0;
		for (int32 ix = 0; ix < CountX; ++ix)
		{
			for (int32 iy = 0; iy < CountY; ++iy)
			{
				const FVector Pos(
					GridCenter.X - HalfExtentX + ix * SpacingX,
					GridCenter.Y - HalfExtentY + iy * SpacingY,
					GridCenter.Z);

				AActor* LightActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Pos, FRotator::ZeroRotator);
				if (LightActor)
				{
					const FString LightName = FString::Printf(TEXT("CastleBeachLight_%d"), LightIdx);
					UPointLightComponent* PL = NewObject<UPointLightComponent>(LightActor, *LightName);
					PL->SetWorldLocation(Pos);
					PL->SetIntensity(3500.f);
					PL->SetAttenuationRadius(8000.f);
					PL->SetLightColor(FColor(255, 200, 120));
					PL->SetCastShadows(false);
					PL->RegisterComponent();
					LightActor->SetRootComponent(PL);
					LightActor->AddInstanceComponent(PL);
				}
				++LightIdx;
			}
		}
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Spawned %d castle beach ambient lights."), LightIdx);
	}

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCCitySubsystem: Sand castle spawned at (%.0f, %.0f) with %d components."),
		Origin.X, Origin.Y, Idx);
}

// ---------------------------------------------------------------------------
// Lake & Pirate Ship — southern expansion area
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnLakeAndPirateShip()
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh || !CylinderMesh || !SphereMesh || !ConeMesh || !PlaneMesh) return;

	// ===== LAKE =====
	const FVector LakeCenter(-10000.f, -95000.f, 0.f);

	int32 Idx = 0;

	// ---------------------------------------------------------------
	// Lake helper: spawn a single-component actor
	// ---------------------------------------------------------------
	auto SpawnLakePiece = [&](UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, const FVector& Scale,
		const FLinearColor& Color, float Roughness, float Metallic, const TCHAR* Label) -> UStaticMeshComponent*
	{
		AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!A) return nullptr;
#if WITH_EDITOR
		A->SetActorLabel(Label);
#endif
		FString Name = FString::Printf(TEXT("Lake_%d"), Idx++);
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(A, *Name);
		MC->SetStaticMesh(Mesh);
		MC->SetWorldScale3D(Scale);
		MC->SetWorldLocation(Loc);
		MC->SetWorldRotation(Rot);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(5.f);
		MC->RegisterComponent();
		A->SetRootComponent(MC);
		ApplyPBR(MC, Color, Metallic, Roughness);
		A->AddInstanceComponent(MC);
		return MC;
	};

	// 1. Lake bed — dark plane just above ground to create depth illusion
	SpawnLakePiece(PlaneMesh, LakeCenter + FVector(0.f, 0.f, 2.f), FRotator::ZeroRotator,
		FVector(300.f, 300.f, 1.f),
		FLinearColor(0.04f, 0.05f, 0.03f), 0.95f, 0.f, TEXT("LakeBed"));

	// 2. Water surface — translucent blue above ground
	{
		AActor* WA = World->SpawnActor<AActor>(AActor::StaticClass(), LakeCenter + FVector(0.f, 0.f, 5.f), FRotator::ZeroRotator);
		if (WA)
		{
#if WITH_EDITOR
			WA->SetActorLabel(TEXT("LakeWaterSurface"));
#endif
			UStaticMeshComponent* WMC = NewObject<UStaticMeshComponent>(WA, TEXT("WaterSurfaceMesh"));
			WMC->SetStaticMesh(PlaneMesh);
			WMC->SetWorldScale3D(FVector(300.f, 300.f, 1.f));
			WMC->SetWorldLocation(LakeCenter + FVector(0.f, 0.f, 5.f));
			WMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			WMC->SetBoundsScale(5.f);
			WMC->RegisterComponent();
			WA->SetRootComponent(WMC);
			if (GlassMaterial)
			{
				ApplyPBRGlass(WMC, GlassMaterial,
					FLinearColor(0.05f, 0.15f, 0.30f), 0.80f, 0.f, 0.02f,
					FLinearColor(0.01f, 0.03f, 0.06f));
			}
			WA->AddInstanceComponent(WMC);
		}
	}

	// 3. Shoreline ring — 8 flat sand-colored planes in a ring
	{
		const FLinearColor SandColor(0.76f, 0.70f, 0.55f);
		const float ShoreRadius = 14500.f;
		for (int32 i = 0; i < 8; ++i)
		{
			const float Angle = (float)i * (360.f / 8.f) + CityRandom.FRandRange(-10.f, 10.f);
			const float Rad = FMath::DegreesToRadians(Angle);
			const float PX = LakeCenter.X + FMath::Cos(Rad) * ShoreRadius;
			const float PY = LakeCenter.Y + FMath::Sin(Rad) * ShoreRadius;
			const float ScaleX = CityRandom.FRandRange(60.f, 90.f);
			const float ScaleY = CityRandom.FRandRange(40.f, 70.f);
			FString Label = FString::Printf(TEXT("Shoreline_%d"), i);
			SpawnLakePiece(PlaneMesh, FVector(PX, PY, -1.f), FRotator(0.f, Angle, 0.f),
				FVector(ScaleX, ScaleY, 1.f),
				SandColor, 0.85f, 0.f, *Label);
		}
	}

	// 4. Shoreline rocks — 14 spheres of varying sizes around the shore
	{
		const FLinearColor RockColor(0.35f, 0.30f, 0.25f);
		const float RockRadius = 14000.f;
		for (int32 i = 0; i < 14; ++i)
		{
			const float Angle = (float)i * (360.f / 14.f) + CityRandom.FRandRange(-8.f, 8.f);
			const float Rad = FMath::DegreesToRadians(Angle);
			const float Dist = RockRadius + CityRandom.FRandRange(-1500.f, 1500.f);
			const float PX = LakeCenter.X + FMath::Cos(Rad) * Dist;
			const float PY = LakeCenter.Y + FMath::Sin(Rad) * Dist;
			const float RockSize = CityRandom.FRandRange(300.f, 800.f);
			const float S = RockSize / 100.f;
			const float ColorVar = CityRandom.FRandRange(-0.05f, 0.05f);
			FLinearColor RC(
				FMath::Clamp(RockColor.R + ColorVar, 0.f, 1.f),
				FMath::Clamp(RockColor.G + ColorVar, 0.f, 1.f),
				FMath::Clamp(RockColor.B + ColorVar, 0.f, 1.f));
			FString Label = FString::Printf(TEXT("ShoreRock_%d"), i);
			SpawnLakePiece(SphereMesh, FVector(PX, PY, RockSize * 0.2f), FRotator::ZeroRotator,
				FVector(S, S * CityRandom.FRandRange(0.7f, 1.0f), S * CityRandom.FRandRange(0.5f, 0.9f)),
				RC, 0.90f, 0.f, *Label);
		}
	}

	// 5. Reeds/cattails — 10 thin cylinders along shore edges
	{
		const FLinearColor ReedColor(0.08f, 0.22f, 0.05f);
		const float ReedRadius = 13500.f;
		for (int32 i = 0; i < 10; ++i)
		{
			const float Angle = (float)i * (360.f / 10.f) + CityRandom.FRandRange(-12.f, 12.f);
			const float Rad = FMath::DegreesToRadians(Angle);
			const float Dist = ReedRadius + CityRandom.FRandRange(-500.f, 500.f);
			const float PX = LakeCenter.X + FMath::Cos(Rad) * Dist;
			const float PY = LakeCenter.Y + FMath::Sin(Rad) * Dist;
			const float ReedH = CityRandom.FRandRange(200.f, 400.f);
			const float ReedDiam = 20.f;
			FString Label = FString::Printf(TEXT("Reed_%d"), i);
			SpawnLakePiece(CylinderMesh,
				FVector(PX, PY, ReedH * 0.5f - 10.f),
				FRotator::ZeroRotator,
				FVector(ReedDiam / 100.f, ReedDiam / 100.f, ReedH / 100.f),
				ReedColor, 0.80f, 0.f, *Label);
		}
	}

	// ===== PIRATE SHIP =====
	const FVector ShipOrigin = LakeCenter + FVector(0.f, 0.f, 5.f);

	AActor* Ship = World->SpawnActor<AActor>(AActor::StaticClass(), ShipOrigin, FRotator::ZeroRotator);
	if (!Ship) return;

#if WITH_EDITOR
	Ship->SetActorLabel(TEXT("PirateShip"));
#endif

	int32 SIdx = 0;

	// ---------------------------------------------------------------
	// Ship helper lambdas
	// ---------------------------------------------------------------
	auto Add = [&](UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, const FVector& Scale,
		const FLinearColor& Color, float Roughness, float Metallic = 0.f) -> UStaticMeshComponent*
	{
		FString Name = FString::Printf(TEXT("S_%d"), SIdx++);
		UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Ship, *Name);
		C->SetStaticMesh(Mesh);
		C->SetWorldScale3D(Scale);
		C->SetWorldLocation(Loc);
		C->SetWorldRotation(Rot);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetBoundsScale(5.f);
		C->RegisterComponent();
		ApplyPBR(C, Color, Metallic, Roughness);
		Ship->AddInstanceComponent(C);
		return C;
	};

	// Ship dimensions
	const float HullLength = 3000.f;
	const float HullWidth = 800.f;
	const float HullHeight = 600.f;
	const float HullBottomZ = ShipOrigin.Z - 200.f; // partially submerged
	const float HullCenterZ = HullBottomZ + HullHeight * 0.5f;
	const float HullTopZ = HullBottomZ + HullHeight;

	// Colors
	const FLinearColor DarkWood(0.15f, 0.10f, 0.05f);
	const FLinearColor LightWood(0.30f, 0.20f, 0.10f);
	const FLinearColor SailCream(0.85f, 0.82f, 0.75f);
	const FLinearColor DarkMetal(0.12f, 0.12f, 0.12f);
	const FLinearColor GoldColor(0.75f, 0.55f, 0.15f);
	const FLinearColor BlackFlag(0.02f, 0.02f, 0.02f);
	const FLinearColor RopeColor(0.25f, 0.18f, 0.10f);

	// ================================================================
	// HULL
	// ================================================================

	// 1. Main hull body: 3000 x 800 x 600 UU
	UStaticMeshComponent* HullBody = Add(CubeMesh,
		FVector(ShipOrigin.X, ShipOrigin.Y, HullCenterZ),
		FRotator::ZeroRotator,
		FVector(HullLength / 100.f, HullWidth / 100.f, HullHeight / 100.f),
		DarkWood, 0.85f);
	Ship->SetRootComponent(HullBody);

	// 2. Hull taper front (bow) — two angled cubes converging at +X
	const float BowStartX = ShipOrigin.X + HullLength * 0.5f;
	const float BowLen = 600.f;
	const float BowTipX = BowStartX + BowLen;
	// Left bow taper
	Add(CubeMesh,
		FVector(BowStartX + BowLen * 0.5f, ShipOrigin.Y + HullWidth * 0.2f, HullCenterZ),
		FRotator(0.f, 12.f, 0.f),
		FVector(BowLen / 100.f, HullWidth * 0.35f / 100.f, HullHeight / 100.f),
		DarkWood, 0.85f);
	// Right bow taper
	Add(CubeMesh,
		FVector(BowStartX + BowLen * 0.5f, ShipOrigin.Y - HullWidth * 0.2f, HullCenterZ),
		FRotator(0.f, -12.f, 0.f),
		FVector(BowLen / 100.f, HullWidth * 0.35f / 100.f, HullHeight / 100.f),
		DarkWood, 0.85f);

	// 3. Hull taper rear (stern) — two cubes narrowing at -X
	const float SternStartX = ShipOrigin.X - HullLength * 0.5f;
	const float SternLen = 400.f;
	// Left stern taper
	Add(CubeMesh,
		FVector(SternStartX - SternLen * 0.5f, ShipOrigin.Y + HullWidth * 0.15f, HullCenterZ),
		FRotator(0.f, -8.f, 0.f),
		FVector(SternLen / 100.f, HullWidth * 0.4f / 100.f, HullHeight / 100.f),
		DarkWood, 0.85f);
	// Right stern taper
	Add(CubeMesh,
		FVector(SternStartX - SternLen * 0.5f, ShipOrigin.Y - HullWidth * 0.15f, HullCenterZ),
		FRotator(0.f, 8.f, 0.f),
		FVector(SternLen / 100.f, HullWidth * 0.4f / 100.f, HullHeight / 100.f),
		DarkWood, 0.85f);

	// 4. Keel — thin dark cube underneath the hull: 3200 x 80 x 100 UU
	Add(CubeMesh,
		FVector(ShipOrigin.X, ShipOrigin.Y, HullBottomZ - 50.f),
		FRotator::ZeroRotator,
		FVector(3200.f / 100.f, 80.f / 100.f, 100.f / 100.f),
		FLinearColor(0.08f, 0.05f, 0.02f), 0.90f);

	// 5. Bowsprit — thin cylinder angled forward and upward: 1200 UU long, 40 UU diameter
	const float BowspritLen = 1200.f;
	Add(CylinderMesh,
		FVector(BowTipX + BowspritLen * 0.4f, ShipOrigin.Y, HullTopZ + BowspritLen * 0.2f),
		FRotator(0.f, 0.f, -65.f),
		FVector(40.f / 100.f, 40.f / 100.f, BowspritLen / 100.f),
		DarkWood, 0.80f);

	// ================================================================
	// DECKS
	// ================================================================

	const float DeckTopZ = HullTopZ + 15.f;

	// 6. Main deck: 2600 x 750 x 30 UU, on top of hull
	Add(CubeMesh,
		FVector(ShipOrigin.X, ShipOrigin.Y, DeckTopZ),
		FRotator::ZeroRotator,
		FVector(2600.f / 100.f, 750.f / 100.f, 30.f / 100.f),
		LightWood, 0.75f);

	// 7. Forecastle: raised deck at bow, 600 x 750 x 30 UU, at hull_top + 200
	const float ForecastleZ = HullTopZ + 200.f;
	const float ForecastleX = ShipOrigin.X + HullLength * 0.5f - 300.f;
	Add(CubeMesh,
		FVector(ForecastleX, ShipOrigin.Y, ForecastleZ),
		FRotator::ZeroRotator,
		FVector(600.f / 100.f, 750.f / 100.f, 30.f / 100.f),
		LightWood, 0.75f);
	// Forecastle support wall
	Add(CubeMesh,
		FVector(ForecastleX - 300.f, ShipOrigin.Y, HullTopZ + 100.f),
		FRotator::ZeroRotator,
		FVector(30.f / 100.f, 750.f / 100.f, 200.f / 100.f),
		DarkWood, 0.85f);

	// 8. Stern castle (poop deck): 800 x 750 x 30 UU, at hull_top + 350
	const float SternCastleZ = HullTopZ + 350.f;
	const float SternCastleX = ShipOrigin.X - HullLength * 0.5f + 400.f;
	Add(CubeMesh,
		FVector(SternCastleX, ShipOrigin.Y, SternCastleZ),
		FRotator::ZeroRotator,
		FVector(800.f / 100.f, 750.f / 100.f, 30.f / 100.f),
		LightWood, 0.75f);

	// 9. Stern cabin walls — 3 cubes forming captain's cabin under poop deck
	const float CabinFloorZ = HullTopZ + 30.f;
	const float CabinH = SternCastleZ - CabinFloorZ;
	// Back wall
	Add(CubeMesh,
		FVector(SternCastleX - 400.f, ShipOrigin.Y, CabinFloorZ + CabinH * 0.5f),
		FRotator::ZeroRotator,
		FVector(30.f / 100.f, 720.f / 100.f, CabinH / 100.f),
		DarkWood, 0.85f);
	// Left wall
	Add(CubeMesh,
		FVector(SternCastleX, ShipOrigin.Y + 360.f, CabinFloorZ + CabinH * 0.5f),
		FRotator::ZeroRotator,
		FVector(800.f / 100.f, 30.f / 100.f, CabinH / 100.f),
		DarkWood, 0.85f);
	// Right wall
	Add(CubeMesh,
		FVector(SternCastleX, ShipOrigin.Y - 360.f, CabinFloorZ + CabinH * 0.5f),
		FRotator::ZeroRotator,
		FVector(800.f / 100.f, 30.f / 100.f, CabinH / 100.f),
		DarkWood, 0.85f);

	// ================================================================
	// MASTS AND SAILS
	// ================================================================

	const float ForemastX = ShipOrigin.X + HullLength * 0.25f;
	const float MainmastX = ShipOrigin.X;
	const float MizzenmastX = ShipOrigin.X - HullLength * 0.25f;
	const float MastBaseZ = DeckTopZ;

	// 10. Foremast: 40 UU diameter, 2200 UU tall
	const float ForemastH = 2200.f;
	Add(CylinderMesh,
		FVector(ForemastX, ShipOrigin.Y, MastBaseZ + ForemastH * 0.5f),
		FRotator::ZeroRotator,
		FVector(40.f / 100.f, 40.f / 100.f, ForemastH / 100.f),
		DarkWood, 0.80f);

	// 11. Mainmast: 50 UU diameter, 2800 UU tall (tallest)
	const float MainmastH = 2800.f;
	Add(CylinderMesh,
		FVector(MainmastX, ShipOrigin.Y, MastBaseZ + MainmastH * 0.5f),
		FRotator::ZeroRotator,
		FVector(50.f / 100.f, 50.f / 100.f, MainmastH / 100.f),
		DarkWood, 0.80f);

	// 12. Mizzenmast: 35 UU diameter, 1800 UU tall
	const float MizzenmastH = 1800.f;
	Add(CylinderMesh,
		FVector(MizzenmastX, ShipOrigin.Y, MastBaseZ + MizzenmastH * 0.5f),
		FRotator::ZeroRotator,
		FVector(35.f / 100.f, 35.f / 100.f, MizzenmastH / 100.f),
		DarkWood, 0.80f);

	// 13. Yards (cross-beams) — 2 per mast, 6 total, perpendicular to keel (along Y)
	// Foremast yard 1 (lower)
	const float ForeYard1Z = MastBaseZ + ForemastH * 0.45f;
	Add(CylinderMesh,
		FVector(ForemastX, ShipOrigin.Y, ForeYard1Z),
		FRotator(0.f, 0.f, 90.f),
		FVector(30.f / 100.f, 30.f / 100.f, 750.f / 100.f),
		DarkWood, 0.80f);
	// Foremast yard 2 (upper)
	const float ForeYard2Z = MastBaseZ + ForemastH * 0.75f;
	Add(CylinderMesh,
		FVector(ForemastX, ShipOrigin.Y, ForeYard2Z),
		FRotator(0.f, 0.f, 90.f),
		FVector(30.f / 100.f, 30.f / 100.f, 700.f / 100.f),
		DarkWood, 0.80f);

	// Mainmast yard 1 (lower)
	const float MainYard1Z = MastBaseZ + MainmastH * 0.40f;
	Add(CylinderMesh,
		FVector(MainmastX, ShipOrigin.Y, MainYard1Z),
		FRotator(0.f, 0.f, 90.f),
		FVector(30.f / 100.f, 30.f / 100.f, 900.f / 100.f),
		DarkWood, 0.80f);
	// Mainmast yard 2 (upper)
	const float MainYard2Z = MastBaseZ + MainmastH * 0.70f;
	Add(CylinderMesh,
		FVector(MainmastX, ShipOrigin.Y, MainYard2Z),
		FRotator(0.f, 0.f, 90.f),
		FVector(30.f / 100.f, 30.f / 100.f, 800.f / 100.f),
		DarkWood, 0.80f);

	// Mizzenmast yard 1 (lower)
	const float MizzenYard1Z = MastBaseZ + MizzenmastH * 0.45f;
	Add(CylinderMesh,
		FVector(MizzenmastX, ShipOrigin.Y, MizzenYard1Z),
		FRotator(0.f, 0.f, 90.f),
		FVector(30.f / 100.f, 30.f / 100.f, 700.f / 100.f),
		DarkWood, 0.80f);
	// Mizzenmast yard 2 (upper)
	const float MizzenYard2Z = MastBaseZ + MizzenmastH * 0.75f;
	Add(CylinderMesh,
		FVector(MizzenmastX, ShipOrigin.Y, MizzenYard2Z),
		FRotator(0.f, 0.f, 90.f),
		FVector(30.f / 100.f, 30.f / 100.f, 650.f / 100.f),
		DarkWood, 0.80f);

	// 14. Sails — 2 per mast = 6 total. Thin cream cubes hanging from yards.
	// Foremast lower sail
	Add(CubeMesh,
		FVector(ForemastX, ShipOrigin.Y, ForeYard1Z - 250.f),
		FRotator::ZeroRotator,
		FVector(8.f / 100.f, 600.f / 100.f, 500.f / 100.f),
		SailCream, 0.70f);
	// Foremast upper sail
	Add(CubeMesh,
		FVector(ForemastX, ShipOrigin.Y, ForeYard2Z - 200.f),
		FRotator::ZeroRotator,
		FVector(8.f / 100.f, 550.f / 100.f, 400.f / 100.f),
		SailCream, 0.70f);
	// Mainmast lower sail (largest)
	Add(CubeMesh,
		FVector(MainmastX, ShipOrigin.Y, MainYard1Z - 280.f),
		FRotator::ZeroRotator,
		FVector(8.f / 100.f, 700.f / 100.f, 560.f / 100.f),
		SailCream, 0.70f);
	// Mainmast upper sail
	Add(CubeMesh,
		FVector(MainmastX, ShipOrigin.Y, MainYard2Z - 220.f),
		FRotator::ZeroRotator,
		FVector(8.f / 100.f, 600.f / 100.f, 440.f / 100.f),
		SailCream, 0.70f);
	// Mizzenmast lower sail
	Add(CubeMesh,
		FVector(MizzenmastX, ShipOrigin.Y, MizzenYard1Z - 230.f),
		FRotator::ZeroRotator,
		FVector(8.f / 100.f, 560.f / 100.f, 460.f / 100.f),
		SailCream, 0.70f);
	// Mizzenmast upper sail
	Add(CubeMesh,
		FVector(MizzenmastX, ShipOrigin.Y, MizzenYard2Z - 180.f),
		FRotator::ZeroRotator,
		FVector(8.f / 100.f, 500.f / 100.f, 360.f / 100.f),
		SailCream, 0.70f);

	// 15. Crow's nest — small cylinder on top of mainmast: 200 UU diameter, 80 UU tall
	Add(CylinderMesh,
		FVector(MainmastX, ShipOrigin.Y, MastBaseZ + MainmastH + 40.f),
		FRotator::ZeroRotator,
		FVector(200.f / 100.f, 200.f / 100.f, 80.f / 100.f),
		DarkWood, 0.80f);

	// ================================================================
	// DETAILS
	// ================================================================

	// 16. Gunwales/rails — thin cubes along both sides of main deck: 2600 x 30 x 150 UU
	// Port gunwale (left, +Y)
	Add(CubeMesh,
		FVector(ShipOrigin.X, ShipOrigin.Y + HullWidth * 0.5f - 15.f, HullTopZ + 75.f),
		FRotator::ZeroRotator,
		FVector(2600.f / 100.f, 30.f / 100.f, 150.f / 100.f),
		LightWood, 0.75f);
	// Starboard gunwale (right, -Y)
	Add(CubeMesh,
		FVector(ShipOrigin.X, ShipOrigin.Y - HullWidth * 0.5f + 15.f, HullTopZ + 75.f),
		FRotator::ZeroRotator,
		FVector(2600.f / 100.f, 30.f / 100.f, 150.f / 100.f),
		LightWood, 0.75f);

	// 17. Cannons — 8 per side = 16 total
	// Each: dark cylinder, 150 UU long, 40 UU diameter, metallic 0.8, roughness 0.3
	const float CannonSpacing = 2600.f / 9.f;
	const float CannonStartX = ShipOrigin.X - 2600.f * 0.5f + CannonSpacing;
	const float CannonZ = HullCenterZ + 50.f;
	const float CannonLen = 150.f;

	// Port side (left, +Y) — cannon 0
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 0.f, ShipOrigin.Y + HullWidth * 0.5f + CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Port side — cannon 1
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 1.f, ShipOrigin.Y + HullWidth * 0.5f + CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Port side — cannon 2
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 2.f, ShipOrigin.Y + HullWidth * 0.5f + CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Port side — cannon 3
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 3.f, ShipOrigin.Y + HullWidth * 0.5f + CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Port side — cannon 4
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 4.f, ShipOrigin.Y + HullWidth * 0.5f + CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Port side — cannon 5
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 5.f, ShipOrigin.Y + HullWidth * 0.5f + CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Port side — cannon 6
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 6.f, ShipOrigin.Y + HullWidth * 0.5f + CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Port side — cannon 7
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 7.f, ShipOrigin.Y + HullWidth * 0.5f + CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);

	// Starboard side (right, -Y) — cannon 8
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 0.f, ShipOrigin.Y - HullWidth * 0.5f - CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Starboard side — cannon 9
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 1.f, ShipOrigin.Y - HullWidth * 0.5f - CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Starboard side — cannon 10
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 2.f, ShipOrigin.Y - HullWidth * 0.5f - CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Starboard side — cannon 11
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 3.f, ShipOrigin.Y - HullWidth * 0.5f - CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Starboard side — cannon 12
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 4.f, ShipOrigin.Y - HullWidth * 0.5f - CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Starboard side — cannon 13
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 5.f, ShipOrigin.Y - HullWidth * 0.5f - CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Starboard side — cannon 14
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 6.f, ShipOrigin.Y - HullWidth * 0.5f - CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Starboard side — cannon 15
	Add(CylinderMesh,
		FVector(CannonStartX + CannonSpacing * 7.f, ShipOrigin.Y - HullWidth * 0.5f - CannonLen * 0.3f, CannonZ),
		FRotator(0.f, 0.f, 90.f),
		FVector(40.f / 100.f, 40.f / 100.f, CannonLen / 100.f),
		DarkMetal, 0.30f, 0.80f);

	// 18. Rudder — flat thin cube at stern: 200 x 10 x 400 UU
	Add(CubeMesh,
		FVector(SternStartX - SternLen - 10.f, ShipOrigin.Y, HullBottomZ - 50.f),
		FRotator::ZeroRotator,
		FVector(200.f / 100.f, 10.f / 100.f, 400.f / 100.f),
		DarkWood, 0.85f);

	// 19. Figurehead — cone at bowsprit tip, gold, pointing forward
	Add(ConeMesh,
		FVector(BowTipX + BowspritLen * 0.75f, ShipOrigin.Y, HullTopZ + BowspritLen * 0.35f),
		FRotator(0.f, 0.f, -65.f),
		FVector(80.f / 100.f, 80.f / 100.f, 200.f / 100.f),
		GoldColor, 0.40f, 0.60f);

	// 20. Jolly Roger flag — small thin black cube: 200 x 8 x 150 UU
	const float FlagpoleH = 300.f;
	const float FlagZ = MastBaseZ + MainmastH + 80.f + FlagpoleH + 75.f;
	Add(CubeMesh,
		FVector(MainmastX + 100.f, ShipOrigin.Y, FlagZ),
		FRotator::ZeroRotator,
		FVector(200.f / 100.f, 8.f / 100.f, 150.f / 100.f),
		BlackFlag, 0.60f);

	// 21. Flagpole — thin cylinder above mainmast: 30 UU diameter, 300 UU tall
	Add(CylinderMesh,
		FVector(MainmastX, ShipOrigin.Y, MastBaseZ + MainmastH + 80.f + FlagpoleH * 0.5f),
		FRotator::ZeroRotator,
		FVector(30.f / 100.f, 30.f / 100.f, FlagpoleH / 100.f),
		DarkWood, 0.80f);

	// 22. Anchor — sphere + thin cubes at bow side, hanging from hull
	// Anchor ring (sphere)
	Add(SphereMesh,
		FVector(BowStartX + 200.f, ShipOrigin.Y + HullWidth * 0.4f, HullBottomZ - 100.f),
		FRotator::ZeroRotator,
		FVector(80.f / 100.f, 80.f / 100.f, 80.f / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Anchor shaft (vertical cube)
	Add(CubeMesh,
		FVector(BowStartX + 200.f, ShipOrigin.Y + HullWidth * 0.4f, HullBottomZ - 250.f),
		FRotator::ZeroRotator,
		FVector(20.f / 100.f, 20.f / 100.f, 250.f / 100.f),
		DarkMetal, 0.30f, 0.80f);
	// Anchor flukes (cross piece)
	Add(CubeMesh,
		FVector(BowStartX + 200.f, ShipOrigin.Y + HullWidth * 0.4f, HullBottomZ - 375.f),
		FRotator::ZeroRotator,
		FVector(150.f / 100.f, 20.f / 100.f, 50.f / 100.f),
		DarkMetal, 0.30f, 0.80f);

	// 23. Ropes/rigging — 4 thin cylinders connecting mast tops to deck edges
	const float RopeDiam = 10.f;

	// Rope 1: Foremast top to bow starboard deck edge
	{
		const FVector RopeStart(ForemastX, ShipOrigin.Y, MastBaseZ + ForemastH);
		const FVector RopeEnd(BowStartX + 300.f, ShipOrigin.Y - HullWidth * 0.4f, HullTopZ + 30.f);
		const FVector RopeMid = (RopeStart + RopeEnd) * 0.5f;
		const FVector RopeDelta = RopeEnd - RopeStart;
		const float RopeLen = RopeDelta.Size();
		const FRotator RopeRot = RopeDelta.Rotation() + FRotator(90.f, 0.f, 0.f);
		Add(CylinderMesh, RopeMid, RopeRot,
			FVector(RopeDiam / 100.f, RopeDiam / 100.f, RopeLen / 100.f),
			RopeColor, 0.85f);
	}

	// Rope 2: Foremast top to bow port deck edge
	{
		const FVector RopeStart(ForemastX, ShipOrigin.Y, MastBaseZ + ForemastH);
		const FVector RopeEnd(BowStartX + 300.f, ShipOrigin.Y + HullWidth * 0.4f, HullTopZ + 30.f);
		const FVector RopeMid = (RopeStart + RopeEnd) * 0.5f;
		const FVector RopeDelta = RopeEnd - RopeStart;
		const float RopeLen = RopeDelta.Size();
		const FRotator RopeRot = RopeDelta.Rotation() + FRotator(90.f, 0.f, 0.f);
		Add(CylinderMesh, RopeMid, RopeRot,
			FVector(RopeDiam / 100.f, RopeDiam / 100.f, RopeLen / 100.f),
			RopeColor, 0.85f);
	}

	// Rope 3: Mainmast top to stern port deck edge
	{
		const FVector RopeStart(MainmastX, ShipOrigin.Y, MastBaseZ + MainmastH);
		const FVector RopeEnd(SternCastleX + 200.f, ShipOrigin.Y + HullWidth * 0.4f, SternCastleZ + 30.f);
		const FVector RopeMid = (RopeStart + RopeEnd) * 0.5f;
		const FVector RopeDelta = RopeEnd - RopeStart;
		const float RopeLen = RopeDelta.Size();
		const FRotator RopeRot = RopeDelta.Rotation() + FRotator(90.f, 0.f, 0.f);
		Add(CylinderMesh, RopeMid, RopeRot,
			FVector(RopeDiam / 100.f, RopeDiam / 100.f, RopeLen / 100.f),
			RopeColor, 0.85f);
	}

	// Rope 4: Mainmast top to stern starboard deck edge
	{
		const FVector RopeStart(MainmastX, ShipOrigin.Y, MastBaseZ + MainmastH);
		const FVector RopeEnd(SternCastleX + 200.f, ShipOrigin.Y - HullWidth * 0.4f, SternCastleZ + 30.f);
		const FVector RopeMid = (RopeStart + RopeEnd) * 0.5f;
		const FVector RopeDelta = RopeEnd - RopeStart;
		const float RopeLen = RopeDelta.Size();
		const FRotator RopeRot = RopeDelta.Rotation() + FRotator(90.f, 0.f, 0.f);
		Add(CylinderMesh, RopeMid, RopeRot,
			FVector(RopeDiam / 100.f, RopeDiam / 100.f, RopeLen / 100.f),
			RopeColor, 0.85f);
	}

	// ================================================================
	// BEACH SAND — realistic sand around pirate ship area
	// ================================================================
	{
		const float SandSize = 120000.f;
		const float HSand = SandSize * 0.5f;

		// -- Base sand plane --
		SpawnLakePiece(PlaneMesh,
			LakeCenter + FVector(0.f, 0.f, 1.f), FRotator::ZeroRotator,
			FVector(SandSize / 100.f, SandSize / 100.f, 1.f),
			FLinearColor(0.82f, 0.76f, 0.58f), 0.95f, 0.f, TEXT("PirateBeachSand"));
		// Extensions toward ocean
		SpawnLakePiece(PlaneMesh,
			LakeCenter + FVector(60000.f, 0.f, 1.f), FRotator::ZeroRotator,
			FVector(600.f, SandSize / 100.f, 1.f),
			FLinearColor(0.80f, 0.74f, 0.56f), 0.95f, 0.f, TEXT("PirateBeachExtendE"));
		SpawnLakePiece(PlaneMesh,
			LakeCenter + FVector(-60000.f, 0.f, 1.f), FRotator::ZeroRotator,
			FVector(600.f, SandSize / 100.f, 1.f),
			FLinearColor(0.80f, 0.74f, 0.56f), 0.95f, 0.f, TEXT("PirateBeachExtendW"));

		// Sand variation colors
		const FLinearColor PSandDry(0.88f, 0.82f, 0.64f);
		const FLinearColor PSandWet(0.62f, 0.56f, 0.40f);
		const FLinearColor PSandMid(0.78f, 0.72f, 0.54f);
		const FLinearColor PSandWarm(0.85f, 0.76f, 0.52f);
		const FLinearColor PSandCool(0.76f, 0.72f, 0.58f);
		const FLinearColor PSandDark(0.68f, 0.62f, 0.44f);
		const FLinearColor PRockGray(0.45f, 0.42f, 0.38f);
		const FLinearColor PDriftwood(0.35f, 0.25f, 0.15f);
		const FLinearColor PShellWhite(0.92f, 0.88f, 0.82f);

		// -- Sand dunes (gentle raised humps) --
		for (int32 d = 0; d < 30; ++d)
		{
			const float DX = LakeCenter.X + CityRandom.FRandRange(-HSand * 0.7f, HSand * 0.7f);
			const float DY = LakeCenter.Y + CityRandom.FRandRange(-HSand * 0.7f, HSand * 0.7f);
			const float DuneW = CityRandom.FRandRange(1200.f, 5000.f);
			const float DuneL = DuneW * CityRandom.FRandRange(1.5f, 3.5f);
			const float DuneH = CityRandom.FRandRange(15.f, 100.f);
			SpawnLakePiece(SphereMesh,
				FVector(DX, DY, DuneH * 0.3f),
				FRotator(0.f, CityRandom.FRandRange(0.f, 360.f), 0.f),
				FVector(DuneL / 100.f, DuneW / 100.f, DuneH / 100.f),
				FMath::Lerp(PSandMid, PSandWarm, CityRandom.FRandRange(0.f, 1.f)),
				0.92f, 0.f, TEXT("PirateDune"));
		}

		// -- Sand ripple lines --
		for (int32 r = 0; r < 50; ++r)
		{
			const float RX = LakeCenter.X + CityRandom.FRandRange(-HSand * 0.6f, HSand * 0.6f);
			const float RY = LakeCenter.Y + CityRandom.FRandRange(-HSand * 0.6f, HSand * 0.6f);
			const float RLen = CityRandom.FRandRange(600.f, 3000.f);
			SpawnLakePiece(CubeMesh,
				FVector(RX, RY, 2.5f),
				FRotator(0.f, CityRandom.FRandRange(-25.f, 25.f), 0.f),
				FVector(RLen / 100.f, 0.12f, 0.03f),
				FMath::Lerp(PSandMid, PSandDry, CityRandom.FRandRange(0.3f, 0.7f)),
				0.90f, 0.f, TEXT("PirateRipple"));
		}

		// -- Beach rocks --
		for (int32 br = 0; br < 20; ++br)
		{
			const float BRX = LakeCenter.X + CityRandom.FRandRange(-HSand * 0.7f, HSand * 0.7f);
			const float BRY = LakeCenter.Y + CityRandom.FRandRange(-HSand * 0.7f, HSand * 0.7f);
			const float RS = CityRandom.FRandRange(25.f, 180.f);
			SpawnLakePiece(SphereMesh,
				FVector(BRX, BRY, RS * 0.2f),
				FRotator(CityRandom.FRandRange(-15.f, 15.f),
					CityRandom.FRandRange(0.f, 360.f),
					CityRandom.FRandRange(-10.f, 10.f)),
				FVector(RS / 100.f * CityRandom.FRandRange(0.7f, 1.3f),
					RS / 100.f * CityRandom.FRandRange(0.7f, 1.3f),
					RS / 100.f * CityRandom.FRandRange(0.4f, 0.8f)),
				FMath::Lerp(PRockGray, PSandDark, CityRandom.FRandRange(0.f, 0.4f)),
				CityRandom.FRandRange(0.82f, 0.95f), 0.f, TEXT("PirateRock"));
		}

		// -- Driftwood --
		for (int32 dw = 0; dw < 10; ++dw)
		{
			const float DWX = LakeCenter.X + CityRandom.FRandRange(-HSand * 0.6f, HSand * 0.6f);
			const float DWY = LakeCenter.Y + CityRandom.FRandRange(-HSand * 0.6f, HSand * 0.6f);
			const float LogLen = CityRandom.FRandRange(120.f, 500.f);
			const float LogThick = CityRandom.FRandRange(6.f, 20.f);
			SpawnLakePiece(CylinderMesh,
				FVector(DWX, DWY, LogThick * 0.3f),
				FRotator(90.f + CityRandom.FRandRange(-10.f, 10.f),
					CityRandom.FRandRange(0.f, 360.f), 0.f),
				FVector(LogThick / 100.f, LogThick / 100.f, LogLen / 100.f),
				FMath::Lerp(PDriftwood, FLinearColor(0.45f, 0.38f, 0.28f),
					CityRandom.FRandRange(0.f, 1.f)),
				0.88f, 0.f, TEXT("PirateDriftwood"));
		}

		// -- Seashells --
		for (int32 sh = 0; sh < 25; ++sh)
		{
			const float SHX = LakeCenter.X + CityRandom.FRandRange(-HSand * 0.5f, HSand * 0.5f);
			const float SHY = LakeCenter.Y + CityRandom.FRandRange(-HSand * 0.5f, HSand * 0.5f);
			const float SS = CityRandom.FRandRange(4.f, 16.f);
			SpawnLakePiece(SphereMesh,
				FVector(SHX, SHY, 2.5f),
				FRotator(0.f, CityRandom.FRandRange(0.f, 360.f), 0.f),
				FVector(SS / 100.f, SS * CityRandom.FRandRange(0.6f, 1.f) / 100.f, SS * 0.3f / 100.f),
				FMath::Lerp(PShellWhite, FLinearColor(0.90f, 0.82f, 0.70f),
					CityRandom.FRandRange(0.f, 1.f)),
				0.70f, 0.f, TEXT("PirateShell"));
		}

		// -- Wet sand strip near lake edge --
		SpawnLakePiece(PlaneMesh,
			LakeCenter + FVector(0.f, 0.f, 1.5f), FRotator::ZeroRotator,
			FVector(350.f, 350.f, 1.f),
			PSandWet, 0.75f, 0.f, TEXT("PirateWetSandRing"));
	}

	// ================================================================
	// EGYPTIAN PYRAMIDS — Great Pyramids of Giza replica
	// ================================================================
	{
		// Pyramid color palette — authentic limestone/sandstone
		const FLinearColor LimestoneMain(0.78f, 0.72f, 0.52f);    // main body warm sandstone
		const FLinearColor LimestoneDark(0.62f, 0.56f, 0.38f);    // shadow/aged blocks
		const FLinearColor LimestoneLight(0.85f, 0.80f, 0.62f);   // sun-bleached highlights
		const FLinearColor CapstoneGold(0.85f, 0.75f, 0.35f);     // golden pyramidion
		const FLinearColor DesertSand(0.82f, 0.76f, 0.58f);       // ground sand
		const FLinearColor BasaltDark(0.18f, 0.17f, 0.15f);       // mortuary temple stone
		const FLinearColor GraniteRed(0.55f, 0.30f, 0.22f);       // granite casing

		const FVector PyramidAreaCenter(-10000.f, -55000.f, 0.f);

		// --- Khufu (Great Pyramid) — the largest ---
		const FVector KhufuBase = PyramidAreaCenter + FVector(-5000.f, 0.f, 0.f);
		const float KhufuHeight = 7300.f;   // ~146m scaled up
		const float KhufuBaseW = 11500.f;   // ~230m scaled up — 1:1.58 ratio

		// Main pyramid body — 4-sided cone
		SpawnLakePiece(ConeMesh, KhufuBase + FVector(0.f, 0.f, KhufuHeight * 0.5f),
			FRotator::ZeroRotator,
			FVector(KhufuBaseW / 100.f, KhufuBaseW / 100.f, KhufuHeight / 100.f),
			LimestoneMain, 0.92f, 0.f, TEXT("Khufu_Body"));

		// Stepped tiers — cubes at different heights to break up the smooth cone
		// These create the illusion of stone block courses
		for (int32 tier = 0; tier < 12; ++tier)
		{
			const float TierFraction = (float)tier / 12.f;
			const float TierZ = KhufuHeight * TierFraction;
			const float TierScale = 1.0f - TierFraction; // narrows toward top
			const float TierW = KhufuBaseW * TierScale;
			const float TierThick = 40.f; // thin horizontal band

			// Use slightly varied color per tier for realism
			FLinearColor TierColor = FMath::Lerp(LimestoneDark, LimestoneLight, (float)(tier % 3) / 2.f);

			// Four sides of the tier band
			// North face
			SpawnLakePiece(CubeMesh,
				KhufuBase + FVector(0.f, TierW * 0.5f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierW / 100.f, TierThick / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Khufu_TierN"));
			// South face
			SpawnLakePiece(CubeMesh,
				KhufuBase + FVector(0.f, -TierW * 0.5f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierW / 100.f, TierThick / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Khufu_TierS"));
			// East face
			SpawnLakePiece(CubeMesh,
				KhufuBase + FVector(TierW * 0.5f, 0.f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierThick / 100.f, TierW / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Khufu_TierE"));
			// West face
			SpawnLakePiece(CubeMesh,
				KhufuBase + FVector(-TierW * 0.5f, 0.f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierThick / 100.f, TierW / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Khufu_TierW"));
		}

		// Golden pyramidion (capstone) — small cone at the very top
		SpawnLakePiece(ConeMesh,
			KhufuBase + FVector(0.f, 0.f, KhufuHeight - 200.f),
			FRotator::ZeroRotator,
			FVector(5.f, 5.f, 4.f),
			CapstoneGold, 0.4f, 0.6f, TEXT("Khufu_Capstone"));

		// Base platform — flat stone base
		SpawnLakePiece(CubeMesh,
			KhufuBase + FVector(0.f, 0.f, -15.f),
			FRotator::ZeroRotator,
			FVector(KhufuBaseW * 1.15f / 100.f, KhufuBaseW * 1.15f / 100.f, 0.3f),
			LimestoneDark, 0.95f, 0.f, TEXT("Khufu_Platform"));

		// Mortuary temple — east side of Khufu
		const FVector TemplePos = KhufuBase + FVector(KhufuBaseW * 0.6f + 500.f, 0.f, 0.f);
		// Temple base
		SpawnLakePiece(CubeMesh, TemplePos + FVector(0.f, 0.f, 150.f),
			FRotator::ZeroRotator,
			FVector(20.f, 30.f, 3.f),
			BasaltDark, 0.88f, 0.f, TEXT("Khufu_TempleBase"));
		// Temple walls (4 sides)
		SpawnLakePiece(CubeMesh, TemplePos + FVector(0.f, 1400.f, 350.f),
			FRotator::ZeroRotator,
			FVector(20.f, 0.5f, 4.f),
			GraniteRed, 0.85f, 0.f, TEXT("Khufu_TempleWallN"));
		SpawnLakePiece(CubeMesh, TemplePos + FVector(0.f, -1400.f, 350.f),
			FRotator::ZeroRotator,
			FVector(20.f, 0.5f, 4.f),
			GraniteRed, 0.85f, 0.f, TEXT("Khufu_TempleWallS"));
		SpawnLakePiece(CubeMesh, TemplePos + FVector(950.f, 0.f, 350.f),
			FRotator::ZeroRotator,
			FVector(0.5f, 30.f, 4.f),
			GraniteRed, 0.85f, 0.f, TEXT("Khufu_TempleWallE"));
		SpawnLakePiece(CubeMesh, TemplePos + FVector(-950.f, 0.f, 350.f),
			FRotator::ZeroRotator,
			FVector(0.5f, 30.f, 4.f),
			GraniteRed, 0.85f, 0.f, TEXT("Khufu_TempleWallW"));
		// Temple columns (6 pillars)
		for (int32 c = 0; c < 6; ++c)
		{
			const float ColY = -1200.f + (float)c * 480.f;
			SpawnLakePiece(CylinderMesh,
				TemplePos + FVector(0.f, ColY, 250.f),
				FRotator::ZeroRotator,
				FVector(1.2f, 1.2f, 4.f),
				LimestoneLight, 0.80f, 0.f, TEXT("Khufu_TempleCol"));
		}

		// Causeway — long raised walkway from temple toward the Nile (east)
		SpawnLakePiece(CubeMesh,
			TemplePos + FVector(5000.f, 0.f, 80.f),
			FRotator::ZeroRotator,
			FVector(80.f, 5.f, 1.5f),
			LimestoneDark, 0.92f, 0.f, TEXT("Khufu_Causeway"));
		// Causeway walls
		SpawnLakePiece(CubeMesh,
			TemplePos + FVector(5000.f, 230.f, 180.f),
			FRotator::ZeroRotator,
			FVector(80.f, 0.3f, 2.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Khufu_CausewayWallN"));
		SpawnLakePiece(CubeMesh,
			TemplePos + FVector(5000.f, -230.f, 180.f),
			FRotator::ZeroRotator,
			FVector(80.f, 0.3f, 2.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Khufu_CausewayWallS"));

		// --- Khafre (second pyramid) — slightly shorter but appears taller ---
		const FVector KhafreBase = PyramidAreaCenter + FVector(8000.f, -3000.f, 0.f);
		const float KhafreHeight = 6800.f;   // ~136m
		const float KhafreBaseW = 10750.f;   // ~215m

		// Main body
		SpawnLakePiece(ConeMesh, KhafreBase + FVector(0.f, 0.f, KhafreHeight * 0.5f),
			FRotator::ZeroRotator,
			FVector(KhafreBaseW / 100.f, KhafreBaseW / 100.f, KhafreHeight / 100.f),
			LimestoneMain, 0.92f, 0.f, TEXT("Khafre_Body"));

		// Remaining limestone casing at the top — Khafre still has its cap
		// This is a distinctive feature — smooth white cap on upper portion
		const float CapHeight = KhafreHeight * 0.25f;
		const float CapBaseW = KhafreBaseW * 0.25f;
		SpawnLakePiece(ConeMesh,
			KhafreBase + FVector(0.f, 0.f, KhafreHeight - CapHeight * 0.5f),
			FRotator::ZeroRotator,
			FVector(CapBaseW / 100.f, CapBaseW / 100.f, CapHeight / 100.f),
			LimestoneLight, 0.75f, 0.05f, TEXT("Khafre_Cap"));

		// Stepped tiers
		for (int32 tier = 0; tier < 10; ++tier)
		{
			const float TierFraction = (float)tier / 10.f;
			const float TierZ = KhafreHeight * TierFraction;
			const float TierScale = 1.0f - TierFraction;
			const float TierW = KhafreBaseW * TierScale;
			const float TierThick = 35.f;
			FLinearColor TierColor = FMath::Lerp(LimestoneDark, LimestoneLight, (float)(tier % 3) / 2.f);

			SpawnLakePiece(CubeMesh,
				KhafreBase + FVector(0.f, TierW * 0.5f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierW / 100.f, TierThick / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Khafre_TierN"));
			SpawnLakePiece(CubeMesh,
				KhafreBase + FVector(0.f, -TierW * 0.5f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierW / 100.f, TierThick / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Khafre_TierS"));
			SpawnLakePiece(CubeMesh,
				KhafreBase + FVector(TierW * 0.5f, 0.f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierThick / 100.f, TierW / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Khafre_TierE"));
			SpawnLakePiece(CubeMesh,
				KhafreBase + FVector(-TierW * 0.5f, 0.f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierThick / 100.f, TierW / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Khafre_TierW"));
		}

		// Platform
		SpawnLakePiece(CubeMesh,
			KhafreBase + FVector(0.f, 0.f, -15.f),
			FRotator::ZeroRotator,
			FVector(KhafreBaseW * 1.12f / 100.f, KhafreBaseW * 1.12f / 100.f, 0.3f),
			LimestoneDark, 0.95f, 0.f, TEXT("Khafre_Platform"));

		// Golden capstone
		SpawnLakePiece(ConeMesh,
			KhafreBase + FVector(0.f, 0.f, KhafreHeight - 180.f),
			FRotator::ZeroRotator,
			FVector(4.f, 4.f, 3.5f),
			CapstoneGold, 0.4f, 0.6f, TEXT("Khafre_Capstone"));

		// Khafre mortuary temple — east side
		const FVector KTemplePos = KhafreBase + FVector(KhafreBaseW * 0.55f + 400.f, 0.f, 0.f);
		SpawnLakePiece(CubeMesh, KTemplePos + FVector(0.f, 0.f, 120.f),
			FRotator::ZeroRotator,
			FVector(15.f, 22.f, 2.4f),
			BasaltDark, 0.88f, 0.f, TEXT("Khafre_TempleBase"));
		for (int32 c = 0; c < 4; ++c)
		{
			const float ColY = -900.f + (float)c * 600.f;
			SpawnLakePiece(CylinderMesh,
				KTemplePos + FVector(0.f, ColY, 200.f),
				FRotator::ZeroRotator,
				FVector(1.0f, 1.0f, 3.5f),
				LimestoneLight, 0.80f, 0.f, TEXT("Khafre_TempleCol"));
		}

		// --- Great Sphinx — between Khufu and Khafre ---
		// Body: elongated cube laid on its side
		const FVector SphinxPos = FMath::Lerp(KhufuBase, KhafreBase, 0.5f) + FVector(KhufuBaseW * 0.5f + 2000.f, 2000.f, 0.f);
		// Body (lion body)
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(0.f, 0.f, 400.f),
			FRotator::ZeroRotator,
			FVector(35.f, 10.f, 5.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Sphinx_Body"));
		// Haunches (rear, raised)
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(-1200.f, 0.f, 550.f),
			FRotator::ZeroRotator,
			FVector(10.f, 10.f, 8.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Sphinx_Haunches"));
		// Head — sphere
		SpawnLakePiece(SphereMesh, SphinxPos + FVector(1400.f, 0.f, 900.f),
			FRotator::ZeroRotator,
			FVector(7.f, 6.f, 8.f),
			LimestoneLight, 0.85f, 0.f, TEXT("Sphinx_Head"));
		// Headdress (nemes) — two slabs flanking the head
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(1400.f, 350.f, 750.f),
			FRotator(0.f, 0.f, -10.f),
			FVector(3.f, 1.f, 10.f),
			CapstoneGold, 0.70f, 0.15f, TEXT("Sphinx_NemesL"));
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(1400.f, -350.f, 750.f),
			FRotator(0.f, 0.f, 10.f),
			FVector(3.f, 1.f, 10.f),
			CapstoneGold, 0.70f, 0.15f, TEXT("Sphinx_NemesR"));
		// Front paws — two elongated cubes forward
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(2200.f, 300.f, 150.f),
			FRotator::ZeroRotator,
			FVector(12.f, 2.5f, 2.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Sphinx_PawL"));
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(2200.f, -300.f, 150.f),
			FRotator::ZeroRotator,
			FVector(12.f, 2.5f, 2.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Sphinx_PawR"));
		// Dream Stele — small slab between the paws
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(2100.f, 0.f, 250.f),
			FRotator::ZeroRotator,
			FVector(0.3f, 3.f, 4.f),
			GraniteRed, 0.80f, 0.f, TEXT("Sphinx_Stele"));
		// Sphinx enclosure walls
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(0.f, 800.f, 200.f),
			FRotator::ZeroRotator,
			FVector(40.f, 0.5f, 4.f),
			LimestoneDark, 0.92f, 0.f, TEXT("Sphinx_WallN"));
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(0.f, -800.f, 200.f),
			FRotator::ZeroRotator,
			FVector(40.f, 0.5f, 4.f),
			LimestoneDark, 0.92f, 0.f, TEXT("Sphinx_WallS"));

		// --- Menkaure (smallest pyramid) ---
		const FVector MenkaureBase = PyramidAreaCenter + FVector(18000.f, -7000.f, 0.f);
		const float MenkaureHeight = 3250.f;  // ~65m
		const float MenkaureBaseW = 5250.f;   // ~105m

		// Main body
		SpawnLakePiece(ConeMesh, MenkaureBase + FVector(0.f, 0.f, MenkaureHeight * 0.5f),
			FRotator::ZeroRotator,
			FVector(MenkaureBaseW / 100.f, MenkaureBaseW / 100.f, MenkaureHeight / 100.f),
			LimestoneMain, 0.92f, 0.f, TEXT("Menkaure_Body"));

		// Granite casing on lower third — Menkaure had red granite at the base
		SpawnLakePiece(ConeMesh, MenkaureBase + FVector(0.f, 0.f, MenkaureHeight * 0.17f),
			FRotator::ZeroRotator,
			FVector(MenkaureBaseW * 0.95f / 100.f, MenkaureBaseW * 0.95f / 100.f, MenkaureHeight * 0.35f / 100.f),
			GraniteRed, 0.82f, 0.05f, TEXT("Menkaure_Granite"));

		// Stepped tiers
		for (int32 tier = 0; tier < 8; ++tier)
		{
			const float TierFraction = (float)tier / 8.f;
			const float TierZ = MenkaureHeight * TierFraction;
			const float TierScale = 1.0f - TierFraction;
			const float TierW = MenkaureBaseW * TierScale;
			const float TierThick = 30.f;
			FLinearColor TierColor = FMath::Lerp(LimestoneDark, LimestoneLight, (float)(tier % 3) / 2.f);

			SpawnLakePiece(CubeMesh,
				MenkaureBase + FVector(0.f, TierW * 0.5f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierW / 100.f, TierThick / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Menkaure_TierN"));
			SpawnLakePiece(CubeMesh,
				MenkaureBase + FVector(0.f, -TierW * 0.5f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierW / 100.f, TierThick / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Menkaure_TierS"));
			SpawnLakePiece(CubeMesh,
				MenkaureBase + FVector(TierW * 0.5f, 0.f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierThick / 100.f, TierW / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Menkaure_TierE"));
			SpawnLakePiece(CubeMesh,
				MenkaureBase + FVector(-TierW * 0.5f, 0.f, TierZ),
				FRotator::ZeroRotator,
				FVector(TierThick / 100.f, TierW / 100.f, TierThick / 100.f),
				TierColor, 0.95f, 0.f, TEXT("Menkaure_TierW"));
		}

		// Platform
		SpawnLakePiece(CubeMesh,
			MenkaureBase + FVector(0.f, 0.f, -15.f),
			FRotator::ZeroRotator,
			FVector(MenkaureBaseW * 1.1f / 100.f, MenkaureBaseW * 1.1f / 100.f, 0.3f),
			LimestoneDark, 0.95f, 0.f, TEXT("Menkaure_Platform"));

		// Three Queens' Pyramids (small satellites south of Menkaure)
		for (int32 q = 0; q < 3; ++q)
		{
			const FVector QBase = MenkaureBase + FVector((float)q * 2500.f - 2500.f, -MenkaureBaseW * 0.6f - 2000.f, 0.f);
			const float QH = 1200.f + (float)q * 200.f;
			const float QW = 2000.f + (float)q * 300.f;

			SpawnLakePiece(ConeMesh, QBase + FVector(0.f, 0.f, QH * 0.5f),
				FRotator::ZeroRotator,
				FVector(QW / 100.f, QW / 100.f, QH / 100.f),
				LimestoneMain, 0.92f, 0.f,
				*FString::Printf(TEXT("Queen_%d_Body"), q));

			// Small capstone
			SpawnLakePiece(ConeMesh, QBase + FVector(0.f, 0.f, QH - 80.f),
				FRotator::ZeroRotator,
				FVector(2.f, 2.f, 1.5f),
				CapstoneGold, 0.5f, 0.4f,
				*FString::Printf(TEXT("Queen_%d_Cap"), q));
		}

		// Scattered rubble/fallen blocks around the pyramids
		for (int32 r = 0; r < 30; ++r)
		{
			const FVector RubbleCenter = PyramidAreaCenter + FVector(
				CityRandom.FRandRange(-15000.f, 25000.f),
				CityRandom.FRandRange(-12000.f, 5000.f),
				0.f);
			const float RS = CityRandom.FRandRange(30.f, 120.f);
			SpawnLakePiece(CubeMesh, RubbleCenter + FVector(0.f, 0.f, RS * 0.3f),
				FRotator(CityRandom.FRandRange(-15.f, 15.f),
					CityRandom.FRandRange(0.f, 360.f),
					CityRandom.FRandRange(-10.f, 10.f)),
				FVector(RS / 100.f, RS * CityRandom.FRandRange(0.6f, 1.2f) / 100.f,
					RS * CityRandom.FRandRange(0.4f, 0.8f) / 100.f),
				FMath::Lerp(LimestoneDark, LimestoneMain, CityRandom.FRandRange(0.f, 1.f)),
				0.95f, 0.f, TEXT("Pyramid_Rubble"));
		}

		// Obelisks (2) — tall thin monuments near the causeway
		for (int32 ob = 0; ob < 2; ++ob)
		{
			const FVector ObBase = TemplePos + FVector(2000.f, (ob == 0 ? 800.f : -800.f), 0.f);
			// Shaft
			SpawnLakePiece(CubeMesh, ObBase + FVector(0.f, 0.f, 1000.f),
				FRotator::ZeroRotator,
				FVector(1.2f, 1.2f, 20.f),
				GraniteRed, 0.75f, 0.1f,
				*FString::Printf(TEXT("Obelisk_%d_Shaft"), ob));
			// Pyramidion tip
			SpawnLakePiece(ConeMesh, ObBase + FVector(0.f, 0.f, 2050.f),
				FRotator::ZeroRotator,
				FVector(1.5f, 1.5f, 2.f),
				CapstoneGold, 0.35f, 0.7f,
				*FString::Printf(TEXT("Obelisk_%d_Tip"), ob));
		}
	}

	// --- Ambient night lighting: 4x4 grid of warm golden lights across pirate beach + pyramid area ---
	{
		const FVector GridCenter(-10000.f, -75000.f, 400.f);
		const float SpacingX = 8000.f;
		const float SpacingY = 10000.f;
		const int32 CountX = 4;
		const int32 CountY = 4;
		const float HalfExtentX = (CountX - 1) * SpacingX * 0.5f;
		const float HalfExtentY = (CountY - 1) * SpacingY * 0.5f;
		int32 LightIdx = 0;
		for (int32 ix = 0; ix < CountX; ++ix)
		{
			for (int32 iy = 0; iy < CountY; ++iy)
			{
				const FVector Pos(
					GridCenter.X - HalfExtentX + ix * SpacingX,
					GridCenter.Y - HalfExtentY + iy * SpacingY,
					GridCenter.Z);

				AActor* LightActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Pos, FRotator::ZeroRotator);
				if (LightActor)
				{
					const FString LightName = FString::Printf(TEXT("PirateBeachLight_%d"), LightIdx);
					UPointLightComponent* PL = NewObject<UPointLightComponent>(LightActor, *LightName);
					PL->SetWorldLocation(Pos);
					PL->SetIntensity(3500.f);
					PL->SetAttenuationRadius(10000.f);
					PL->SetLightColor(FColor(255, 190, 110));
					PL->SetCastShadows(false);
					PL->RegisterComponent();
					LightActor->SetRootComponent(PL);
					LightActor->AddInstanceComponent(PL);
				}
				++LightIdx;
			}
		}
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Spawned %d pirate beach ambient lights."), LightIdx);
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Spawned lake and pirate ship with %d ship components."), SIdx);
}
