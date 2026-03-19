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
#include "Components/DecalComponent.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/DirectionalLight.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/Package.h"
#include "Engine/Engine.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Engine/PostProcessVolume.h"
#include "Components/TextRenderComponent.h"
#include "Vehicle/PHCMissileProjectile.h"
#include "Player/PHCCityCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "EngineUtils.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "ProceduralMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Vehicle/PHCBlackbirdPawn.h"
#include "Vehicle/PHCBlackHawkPawn.h"
#include "Vehicle/PHCNukeExplosion.h"
#include "Core/PHCPlayerController.h"
#include "Core/PHCGameInstance.h"
#include "Economy/PHCEconomyManager.h"
#include "TimerManager.h"
#include "UI/PHCCityHUD.h"

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
	const FLinearColor& Emissive = FLinearColor::Black,
	UTexture2D* NormalMap = nullptr, UTexture2D* RoughnessMap = nullptr)
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
	if (NormalMap)
		Mat->SetTextureParameterValue(TEXT("NormalMap"), NormalMap);
	if (RoughnessMap)
		Mat->SetTextureParameterValue(TEXT("RoughnessMap"), RoughnessMap);
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

	// Dedicated servers don't need city visuals — skip generation entirely
	if (InWorld.GetNetMode() == NM_DedicatedServer)
	{
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Skipping city generation on dedicated server"));
		bCityGenerated = true;
		return;
	}

	// Skip city generation on the initial map load when we know a ClientTravel
	// to the dedicated server is coming — avoids generating the city twice
	// (once now, then tear down + regenerate after the server travel).
	if (UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(GetWorld()))
	{
		if (GI->bPendingServerConnect)
		{
			UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Deferring city generation until server connection completes"));
			return;
		}
	}

	if (bCityGenerated) return;
	bCityGenerated = true;

	CityRandom.Initialize(42);
	DetailRandom.Initialize(137);

	// Enhanced shadow console variables
	static IConsoleVariable* ShadowMaxRes = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.MaxResolution"));
	if (ShadowMaxRes) ShadowMaxRes->Set(1024);
	static IConsoleVariable* ShadowCSMCascades = IConsoleManager::Get().FindConsoleVariable(TEXT("r.Shadow.CSM.MaxCascades"));
	if (ShadowCSMCascades) ShadowCSMCascades->Set(3);
	static IConsoleVariable* ContactShadows = IConsoleManager::Get().FindConsoleVariable(TEXT("r.ContactShadows"));
	if (ContactShadows) ContactShadows->Set(1);

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
	ChromaAberrationMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCChromaAberration.M_PHCChromaAberration"));
	DecalMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCDecal.M_PHCDecal"));
	TripSkyMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Game/Materials/M_PHCTripSky.M_PHCTripSky"));

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

	// Generate procedural normal & roughness textures before city spawn
	GenerateProceduralTextures();

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

		// PUBG 2026: neutral white sun at noon, subtle warm at horizon
		if (Elevation > 0.01f)
		{
			const float OrangeFactor = 1.f - FMath::Pow(Elevation, 0.4f);
			const FLinearColor NoonColor(1.f, 0.99f, 0.97f);  // nearly pure white
			const FLinearColor HorizonColor(1.f, 0.55f, 0.25f); // warm but less saturated
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
		SkyLightComp->SetIntensity(FMath::Lerp(0.05f, 1.8f, DayFactor)); // dark night, bright day

		// Tint: blue-white during day, very deep blue at night.
		const float NightFactor = FMath::Max(0.f, -SunSine);
		// PUBG 2026: neutral pale blue sky light
		const FLinearColor DayTint(0.7f, 0.75f, 0.85f);
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
		FogComp->SetFogDensity(FMath::Lerp(0.0004f, 0.0015f, NightFactor));
		FogComp->SetFogMaxOpacity(FMath::Lerp(0.225f, 0.025f, NightFactor));
		FogComp->SetVolumetricFogDistance(FMath::Lerp(4000.f, 2000.f, NightFactor));

		// GTA V-style directional inscattering -- warm haze at dawn/dusk, near-black at night
		const float DawnDusk = 1.f - FMath::Abs(SunSine);
		FogComp->SetDirectionalInscatteringExponent(8.f);
		FogComp->SetDirectionalInscatteringStartDistance(5000.f);
		// PUBG 2026: neutral blue-gray daytime fog, subtle warm haze at horizon
		const FLinearColor NeutralFog(0.55f, 0.58f, 0.65f); // cool blue-gray
		const FLinearColor WarmHaze(0.9f, 0.7f, 0.4f);      // reduced warmth
		const FLinearColor NightFog(0.015f, 0.005f, 0.04f);
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

		PP.BloomThreshold = FMath::Lerp(1.5f, 0.3f, NF);
		PP.BloomIntensity = FMath::Lerp(0.7f, 1.0f, NF);

		// GTA V 4-phase color grading (ColorGain + ColorOffset)
		// Determine time-of-day phase: dawn (T~0), day (T~0.25), sunset (T~0.5), night (T~0.75)
		FVector4 TargetGain;
		FVector4 TargetOffset;
		if (T < 0.125f || T >= 0.875f)
		{
			// Dawn: very subtle warm, mostly neutral
			TargetGain = FVector4(1.03f, 1.00f, 0.96f, 1.0f);
			TargetOffset = FVector4(0.005f, 0.002f, -0.002f, 0.0f);
		}
		else if (T < 0.375f)
		{
			// Day: perfectly neutral
			TargetGain = FVector4(1.00f, 1.00f, 1.00f, 1.0f);
			TargetOffset = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		}
		else if (T < 0.625f)
		{
			// Sunset: barely warm, no sepia
			TargetGain = FVector4(1.04f, 0.98f, 0.92f, 1.0f);
			TargetOffset = FVector4(0.008f, 0.002f, -0.004f, 0.0f);
		}
		else
		{
			// Night: darker cool blue
			TargetGain = FVector4(0.18f, 0.22f, 0.35f, 1.0f);
			TargetOffset = FVector4(-0.015f, -0.006f, 0.006f, 0.0f);
		}
		// Smooth interpolation to target
		PP.ColorGain = FMath::Lerp(PP.ColorGain, TargetGain, FMath::Min(1.f, DeltaTime * 2.f));
		PP.ColorOffset = FMath::Lerp(PP.ColorOffset, TargetOffset, FMath::Min(1.f, DeltaTime * 2.f));

		// PUBG 2026 ColorGamma — neutral during day, cool at night
		const float WarmR = FMath::Lerp(1.0f, 0.97f, NF);
		const float WarmG = FMath::Lerp(1.0f, 0.98f, NF);
		const float WarmB = FMath::Lerp(1.0f, 1.03f, NF);
		PP.ColorGamma = FVector4(WarmR, WarmG, WarmB, 1.0f);

		PP.VignetteIntensity = FMath::Lerp(0.35f, 0.42f, DawnDuskFactor);

		// Dynamic saturation — boost during golden hour, desaturate at night
		{
			float TargetSat;
			if (T < 0.125f || T >= 0.875f || (T >= 0.375f && T < 0.625f))
				TargetSat = 1.08f;  // golden hour boost
			else if (T >= 0.625f && T < 0.875f)
				TargetSat = 0.85f;  // night desaturate
			else
				TargetSat = 1.0f;   // day neutral
			PP.bOverride_ColorSaturation = true;
			PP.ColorSaturation = FMath::Lerp(PP.ColorSaturation,
				FVector4(TargetSat, TargetSat, TargetSat, 1.0f),
				FMath::Min(1.f, DeltaTime * 2.f));
		}

		PP.AutoExposureMinBrightness = FMath::Lerp(0.08f, 0.005f, NF);
		PP.AutoExposureMaxBrightness = FMath::Lerp(4.0f, 0.6f, NF);

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
		const float TargetFstop = bDriving ? 8.f : 11.f;
		const float TargetFocal = bDriving ? 6000.f : 4000.f;
		PP.DepthOfFieldFstop = FMath::FInterpTo(PP.DepthOfFieldFstop, TargetFstop, DeltaTime, 3.f);
		PP.DepthOfFieldFocalDistance = FMath::FInterpTo(PP.DepthOfFieldFocalDistance, TargetFocal, DeltaTime, 3.f);
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

	// ---- Cloud drift (all clouds, varied speeds for natural feel) ----
	{
		const float BaseSpeed = 400.f; // UU per minute
		const float BaseDrift = DeltaTime * (BaseSpeed / 60.f);

		for (int32 Ci = 0; Ci < CloudActors.Num(); ++Ci)
		{
			AActor* CA = CloudActors[Ci];
			if (!CA) continue;
			// Each cloud drifts at a slightly different speed/direction
			// based on its index for natural variation
			float SpeedMul = 0.5f + (float(Ci % 7) / 7.f) * 1.0f; // 0.5x to 1.5x
			float AngleOff = float(Ci % 5) * 0.2f - 0.4f; // slight Y variation
			CA->AddActorWorldOffset(FVector(BaseDrift * SpeedMul, BaseDrift * AngleOff, 0.f));
		}
	// Cloud day/night color
		if (CloudMIDs.Num() > 0)
		{
			const float NightFactor = FMath::Max(0.f, -SunSine);
			const float DawnDuskFactor = 1.f - FMath::Abs(SunSine);
			// Day: white-gray, Sunset: warm orange-pink, Night: dark blue-gray
			FLinearColor DayCol(0.85f, 0.85f, 0.88f);
			FLinearColor SunsetCol(0.95f, 0.70f, 0.50f);
			FLinearColor NightCol(0.04f, 0.04f, 0.06f); // dark gray at night
			FLinearColor CloudCol = FMath::Lerp(DayCol, SunsetCol, DawnDuskFactor * 0.6f);
			CloudCol = FMath::Lerp(CloudCol, NightCol, NightFactor);

			FLinearColor DayEmis(0.3f, 0.3f, 0.32f);
			FLinearColor NightEmis(0.0f, 0.0f, 0.0f);
			FLinearColor CloudEmis = FMath::Lerp(DayEmis, NightEmis, NightFactor);

			for (int32 Ci = 0; Ci < CloudMIDs.Num(); ++Ci)
			{
				UMaterialInstanceDynamic* CM = CloudMIDs[Ci];
				if (!CM) continue;
				CM->SetVectorParameterValue(TEXT("BaseColor"), CloudCol);
				CM->SetVectorParameterValue(TEXT("EmissiveColor"), CloudEmis);
			}
		}
	}

	// ---- Atmospheric dust motes — follow camera with gentle drift ----
	if (DustMoteActor && DustMotes.Num() > 0)
	{
		FVector CamPos = FVector::ZeroVector;
		if (UWorld* W = GetWorld())
		{
			if (APlayerController* PC = W->GetFirstPlayerController())
			{
				if (APawn* P = PC->GetPawn())
					CamPos = P->GetActorLocation();
			}
		}
		DustMoteActor->SetActorLocation(CamPos);

		const float GT = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		for (int32 i = 0; i < DustMotes.Num(); ++i)
		{
			if (DustMotes[i])
			{
				FVector Drift(
					FMath::Sin(GT * 0.3f + i * 1.7f) * 15.f,
					FMath::Cos(GT * 0.25f + i * 2.3f) * 15.f,
					FMath::Sin(GT * 0.4f + i * 0.9f) * 8.f);
				DustMotes[i]->AddRelativeLocation(Drift * DeltaTime);
			}
		}
	}

	// ---- Detect airport Blackbird destruction (persist across sessions) ----
	if (AirportBlackbird && !IsValid(AirportBlackbird))
	{
		bAirportBlackbirdDestroyed = true;
		AirportBlackbird = nullptr;
		SaveDestroyedVehicleState();
	}
	else if (AirportBlackbird && AirportBlackbird->bIsCrashing && !bAirportBlackbirdDestroyed)
	{
		bAirportBlackbirdDestroyed = true;
		SaveDestroyedVehicleState();
	}

	// ---- NPC patrol helicopters (2x, daytime, damageable, crash + pair respawn) ----
	// Pair respawn timer — both must be down, then 60s wait
	if (NPCHeliPairRespawnTimer >= 0.f)
	{
		NPCHeliPairRespawnTimer -= DeltaTime;
		if (NPCHeliPairRespawnTimer <= 0.f)
		{
			RespawnNPCHelicopter();
		}
	}

	// Lambda to tick a single NPC helicopter
	auto TickNPCHeli = [&](APHCBlackHawkPawn* Heli, bool& bVisible, bool& bCrashing, float& CrashTimer,
		float& PatrolAngle, float& DmgFlash, bool bDown,
		TArray<TObjectPtr<UMaterialInstanceDynamic>>& MIDs)
	{
		if (!Heli) return;

		// ---- Crash spiral ----
		if (bCrashing)
		{
			CrashTimer += DeltaTime;
			FVector Pos = Heli->GetActorLocation();
			FRotator Rot = Heli->GetActorRotation();

			const float FallSpeed = 800.f + CrashTimer * 200.f;
			Pos.Z -= FallSpeed * DeltaTime;
			Rot.Yaw += 180.f * DeltaTime;
			Rot.Roll = FMath::FInterpTo(Rot.Roll, 45.f, DeltaTime, 1.5f);
			Rot.Pitch = FMath::FInterpTo(Rot.Pitch, -20.f, DeltaTime, 1.0f);

			Heli->SetActorLocation(Pos);
			Heli->SetActorRotation(Rot);
			Heli->bIsOccupied = false;

			if (Pos.Z <= 10.f)
			{
				const FVector CrashLoc(Pos.X, Pos.Y, FMath::Max(Pos.Z, 0.f));
				FActorSpawnParameters ExpSP;
				ExpSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
				World->SpawnActor<APHCNukeExplosion>(APHCNukeExplosion::StaticClass(), CrashLoc, FRotator::ZeroRotator, ExpSP);

				Heli->SetActorHiddenInGame(true);
				bVisible = false;
				bCrashing = false;
			}
		}
		// ---- Normal patrol (skip if down/crashed) ----
		else if (!bDown)
		{
			const bool bShouldBeVisible = (T > 0.02f && T < 0.48f);
			if (bShouldBeVisible != bVisible)
			{
				bVisible = bShouldBeVisible;
				Heli->SetActorHiddenInGame(!bVisible);
			}

			if (bVisible)
			{
				Heli->bIsOccupied = true;

				const float PatrolRadius = 8000.f;
				const float PatrolAltitude = 2500.f;
				const float PatrolCenterX = -10000.f;
				const float PatrolCenterY = -10000.f;
				const float PatrolSpeed = 0.015f;
				PatrolAngle += PatrolSpeed * DeltaTime;
				if (PatrolAngle > 2.f * UE_PI) PatrolAngle -= 2.f * UE_PI;

				const FVector HeliPos(
					PatrolCenterX + PatrolRadius * FMath::Cos(PatrolAngle),
					PatrolCenterY + PatrolRadius * FMath::Sin(PatrolAngle),
					PatrolAltitude);
				Heli->SetActorLocation(HeliPos);

				const float HeadingDeg = FMath::RadiansToDegrees(PatrolAngle) + 90.f;
				Heli->SetActorRotation(FRotator(0.f, HeadingDeg, 0.f));
			}
			else
			{
				Heli->bIsOccupied = false;
			}
		}

		// ---- Damage flash ----
		if (DmgFlash > 0.f)
		{
			DmgFlash -= DeltaTime;
			const FLinearColor FlashColor = (DmgFlash > 0.f) ? FLinearColor(4.f, 0.f, 0.f) : FLinearColor::Black;
			for (UMaterialInstanceDynamic* MID : MIDs)
			{
				if (MID) MID->SetVectorParameterValue(TEXT("EmissiveColor"), FlashColor);
			}
		}
	};

	TickNPCHeli(NPCHelicopter, bNPCHeliVisible, bNPCHeliCrashing, NPCHeliCrashTimer,
		NPCHeliPatrolAngle, NPCHeliDamageFlashTimer, bNPCHeli1Down, NPCHeliBodyMIDs);

	TickNPCHeli(NPCHelicopter2, bNPCHeliVisible2, bNPCHeliCrashing2, NPCHeliCrashTimer2,
		NPCHeliPatrolAngle2, NPCHeliDamageFlashTimer2, bNPCHeli2Down, NPCHeliBodyMIDs2);

	// ---- NPC player characters ----
	{
			// Animation constants (same as PHCCityCharacter)
			constexpr float ShPivZ = 54.f, HipPivZ = 2.f;
			constexpr float UArmHalf = 10.f, FArmHalf = 9.f;
			constexpr float ThighHalf = 14.f, ShinHalf = 12.f;
			constexpr float WalkSpeed = 600.f;
			constexpr float CycleSpeed = 13.f;
			constexpr float ThighAmp = 35.f, UpperArmAmp = 30.f;
			constexpr float KneeBendMax = 40.f, ElbowBendMax = 35.f;
			constexpr float LeanAmp = 6.f, BobAmp = 2.f;

			auto OrgSin = [](float P) -> float
			{
				return FMath::Sin(P) + 0.12f * FMath::Sin(2.f * P) + 0.05f * FMath::Sin(3.f * P - 0.4f);
			};

			for (int32 NI = 0; NI < NumNPCPlayers; ++NI)
			{
				FNPCPlayerData& NP = NPCPlayers[NI];
				if (!NP.Actor || !NP.bVisible) continue;

				// ---- Wander AI ----
				FVector Pos = NP.Actor->GetActorLocation();
				bool bMoving = false;

				// Keep Z on terrain even when idle
				{
					const float IdleTerrainZ = GetTerrainHeight(Pos.X, Pos.Y) + 90.f;
					if (FMath::Abs(Pos.Z - IdleTerrainZ) > 1.f)
					{
						Pos.Z = FMath::FInterpTo(Pos.Z, IdleTerrainZ, DeltaTime, 12.f);
						NP.Actor->SetActorLocation(Pos);
					}
				}

				if (NP.WanderWaitTimer > 0.f)
				{
					NP.WanderWaitTimer -= DeltaTime;
				}
				else
				{
					const float DistToTarget = FVector::Dist2D(Pos, NP.WanderTarget);
					if (DistToTarget < 200.f)
					{
						// Pick new wander target within city
						NP.WanderTarget = FVector(
							FMath::RandRange(-25000.f, 25000.f),
							FMath::RandRange(-25000.f, 25000.f), 0.f);
						NP.WanderWaitTimer = FMath::RandRange(2.f, 5.f);
					}
					else
					{
						bMoving = true;
						FVector MoveDir = (NP.WanderTarget - Pos).GetSafeNormal2D();
						FVector NewPos = Pos + MoveDir * WalkSpeed * DeltaTime;
						const float TerrainZ = GetTerrainHeight(NewPos.X, NewPos.Y) + 90.f;
						NewPos.Z = FMath::FInterpTo(Pos.Z, TerrainZ, DeltaTime, 12.f);
						NP.Actor->SetActorLocation(NewPos);

						// Face movement direction
						FRotator DesiredRot(0.f, MoveDir.Rotation().Yaw, 0.f);
						FRotator CurRot = NP.Actor->GetActorRotation();
						NP.Actor->SetActorRotation(FMath::RInterpTo(CurRot, DesiredRot, DeltaTime, 6.f));
					}
				}

				// ---- Walk animation ----
				const float TargetBlend = bMoving ? 1.f : 0.f;
				NP.WalkAnimBlend = FMath::FInterpTo(NP.WalkAnimBlend, TargetBlend, DeltaTime, 8.f);
				const float Blend = NP.WalkAnimBlend;

				if (bMoving) NP.AnimTime += CycleSpeed * DeltaTime;

				const float Phase = NP.AnimTime;
				const float O = OrgSin(Phase);
				const float LThighSwing = O * ThighAmp * Blend;
				const float RThighSwing = -LThighSwing;
				const float RArmSwing = O * UpperArmAmp * Blend;
				const float LArmSwing = -RArmSwing;

				const float LKneeBend = FMath::Max(0.f, -LThighSwing / (ThighAmp + 0.01f)) * KneeBendMax * Blend;
				const float RKneeBend = FMath::Max(0.f, -RThighSwing / (ThighAmp + 0.01f)) * KneeBendMax * Blend;
				const float LElbowBend = (0.3f + 0.7f * FMath::Abs(LArmSwing / (UpperArmAmp + 0.01f))) * ElbowBendMax * Blend;
				const float RElbowBend = (0.3f + 0.7f * FMath::Abs(RArmSwing / (UpperArmAmp + 0.01f))) * ElbowBendMax * Blend;

				const float TorsoLean = LeanAmp * Blend;
				const float VertBounce = FMath::Cos(Phase * 2.f) * BobAmp * Blend;

				// AnimateArm lambda
				auto AnimArm = [&](UStaticMeshComponent* UA, UStaticMeshComponent* FA,
					UStaticMeshComponent* EJ, UStaticMeshComponent* HN,
					UStaticMeshComponent* SH, float Y, float Swing, float Bend_)
				{
					const FRotator URot(Swing, 0.f, 0.f);
					const FVector UOff = URot.RotateVector(FVector(0.f, 0.f, -UArmHalf));
					if (UA) { UA->SetRelativeLocation(FVector(UOff.X, Y, ShPivZ + UOff.Z + VertBounce));
					          UA->SetRelativeRotation(FRotator(Swing, 0.f, 0.f)); }
					const FVector ELocal = URot.RotateVector(FVector(0.f, 0.f, -UArmHalf * 2.f));
					const FVector EPos(ELocal.X, Y, ShPivZ + ELocal.Z + VertBounce);
					if (EJ) { EJ->SetRelativeLocation(EPos); }
					const float FAngle = Swing - Bend_;
					const FRotator FRot(FAngle, 0.f, 0.f);
					const FVector FOff = FRot.RotateVector(FVector(0.f, 0.f, -FArmHalf));
					if (FA) { FA->SetRelativeLocation(FVector(EPos.X + FOff.X, Y, EPos.Z + FOff.Z));
					          FA->SetRelativeRotation(FRotator(FAngle, 0.f, 0.f)); }
					if (HN) {
						const FVector HLocal = FRot.RotateVector(FVector(0.f, 0.f, -(FArmHalf * 2.f + 3.f)));
						HN->SetRelativeLocation(FVector(EPos.X + HLocal.X, Y, EPos.Z + HLocal.Z));
					}
					if (SH) { SH->SetRelativeLocation(FVector(UOff.X * 0.2f, Y + (Y > 0 ? -1.f : 1.f), ShPivZ + VertBounce)); }
				};

				// AnimateLeg lambda
				auto AnimLeg = [&](UStaticMeshComponent* TH, UStaticMeshComponent* SN,
					UStaticMeshComponent* KN, UStaticMeshComponent* FT,
					UStaticMeshComponent* SL, float Y, float Swing, float Bend_)
				{
					const FRotator TRot(Swing, 0.f, 0.f);
					const FVector TOff = TRot.RotateVector(FVector(0.f, 0.f, -ThighHalf));
					if (TH) { TH->SetRelativeLocation(FVector(TOff.X, Y, HipPivZ + TOff.Z));
					          TH->SetRelativeRotation(FRotator(Swing, 0.f, 0.f)); }
					const FVector KLocal = TRot.RotateVector(FVector(0.f, 0.f, -ThighHalf * 2.f));
					const FVector KPos(KLocal.X, Y, HipPivZ + KLocal.Z);
					if (KN) { KN->SetRelativeLocation(KPos); }
					const float SAngle = Swing + Bend_;
					const FRotator SRot(SAngle, 0.f, 0.f);
					const FVector SOff = SRot.RotateVector(FVector(0.f, 0.f, -ShinHalf));
					if (SN) { SN->SetRelativeLocation(FVector(KPos.X + SOff.X, Y, KPos.Z + SOff.Z));
					          SN->SetRelativeRotation(FRotator(SAngle, 0.f, 0.f)); }
					const FVector FLocal = SRot.RotateVector(FVector(0.f, 0.f, -(ShinHalf * 2.f + 4.f)));
					float FZ = FMath::Max(KPos.Z + FLocal.Z, -73.f);
					float FX = KPos.X + FLocal.X + 4.f;
					if (FT) { FT->SetRelativeLocation(FVector(FX, Y, FZ)); FT->SetRelativeRotation(FRotator::ZeroRotator); }
					if (SL) { SL->SetRelativeLocation(FVector(FX, Y, FZ - 5.f)); SL->SetRelativeRotation(FRotator::ZeroRotator); }
				};

				// Apply animation
				AnimArm(NP.LUpperArmMesh, NP.LForearmMesh, NP.LElbowMesh, NP.LHandMesh, NP.LShoulderMesh, -26.f, LArmSwing, LElbowBend);
				AnimArm(NP.RUpperArmMesh, NP.RForearmMesh, NP.RElbowMesh, NP.RHandMesh, NP.RShoulderMesh,  26.f, RArmSwing, RElbowBend);
				AnimLeg(NP.LThighMesh, NP.LShinMesh, NP.LKneeMesh, NP.LFootMesh, NP.LSoleMesh, -11.f, LThighSwing, LKneeBend);
				AnimLeg(NP.RThighMesh, NP.RShinMesh, NP.RKneeMesh, NP.RFootMesh, NP.RSoleMesh,  11.f, RThighSwing, RKneeBend);

				// Torso lean + bounce
				if (NP.ChestMesh) {
					NP.ChestMesh->SetRelativeLocation(FVector(1.f, 0.f, 44.f + VertBounce));
					NP.ChestMesh->SetRelativeRotation(FRotator(TorsoLean, 0.f, 0.f));
				}
				if (NP.AbsMesh) {
					NP.AbsMesh->SetRelativeLocation(FVector(0.f, 0.f, 28.f + VertBounce * 0.8f));
					NP.AbsMesh->SetRelativeRotation(FRotator(TorsoLean * 0.6f, 0.f, 0.f));
				}
				if (NP.PelvisMesh) NP.PelvisMesh->SetRelativeLocation(FVector(0.f, 0.f, 12.f + VertBounce * 0.4f));
				if (NP.HeadMesh) NP.HeadMesh->SetRelativeLocation(FVector(0.f, 0.f, 76.f + VertBounce * 0.3f));
				if (NP.JawMesh) NP.JawMesh->SetRelativeLocation(FVector(2.f, 0.f, 67.f + VertBounce * 0.3f));
				if (NP.NeckMesh) NP.NeckMesh->SetRelativeLocation(FVector(0.f, 0.f, 60.f + VertBounce * 0.5f));
				if (NP.CollarMesh) NP.CollarMesh->SetRelativeLocation(FVector(2.f, 0.f, 56.f + VertBounce));
				if (NP.BeltMesh) NP.BeltMesh->SetRelativeLocation(FVector(0.f, 0.f, 20.f + VertBounce * 0.4f));
				if (NP.BeltBuckleMesh) NP.BeltBuckleMesh->SetRelativeLocation(FVector(8.f, 0.f, 20.f + VertBounce * 0.4f));
				if (NP.HairTopMesh) NP.HairTopMesh->SetRelativeLocation(FVector(-1.f, 0.f, 88.f + VertBounce * 0.3f));
				if (NP.HairBackMesh) NP.HairBackMesh->SetRelativeLocation(FVector(-8.f, 0.f, 80.f + VertBounce * 0.3f));

				// ---- Damage flash ----
				if (NP.DamageFlashTimer > 0.f)
				{
					NP.DamageFlashTimer -= DeltaTime;
					const FLinearColor FlashColor = (NP.DamageFlashTimer > 0.f)
						? FLinearColor(4.f, 0.f, 0.f) : FLinearColor::Black;
					for (UMaterialInstanceDynamic* MID : NP.BodyMIDs)
					{
						if (MID) MID->SetVectorParameterValue(TEXT("EmissiveColor"), FlashColor);
					}
				}
			}
		}

	// ---- Grass streaming (update instances around player) ----
	{
		UWorld* GrassWorld = GetWorld();
		if (GrassWorld && GrassHISMs.Num() > 0)
		{
			APawn* LocalPawn = GrassWorld->GetFirstPlayerController() ?
				GrassWorld->GetFirstPlayerController()->GetPawn() : nullptr;
			if (LocalPawn)
			{
				UpdateGrassInstances(LocalPawn->GetActorLocation());
			}
		}
	}

	// ---- River wave animation (Gerstner-style ripples) ----
	if (RiverWaterPMC && RiverWaterBaseVerts.Num() > 0)
	{
		const float GT = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		TArray<FVector> AnimVerts;
		AnimVerts.Reserve(RiverWaterBaseVerts.Num());
		for (const FVector& V : RiverWaterBaseVerts)
		{
			float Z = V.Z;
			// Wave 1: along river (X direction), long wavelength
			Z += 12.f * FMath::Sin(V.X * 0.0008f + GT * 1.2f);
			// Wave 2: cross-river chop
			Z += 6.f * FMath::Sin(V.Y * 0.002f + GT * 2.0f);
			// Wave 3: fine ripple detail
			Z += 3.f * FMath::Sin((V.X + V.Y) * 0.003f + GT * 3.5f);
			AnimVerts.Add(FVector(V.X, V.Y, Z));
		}
		TArray<FVector> AnimNormals;
		// Recompute normals not needed every frame for mobile perf
		RiverWaterPMC->UpdateMeshSection(0, AnimVerts, AnimNormals, RiverWaterUVs,
			TArray<FColor>(), TArray<FProcMeshTangent>());
	}

	// ---- Torch flame animation (flickering, swaying) ----
	if (TorchFlames.Num() > 0)
	{
		const float GT = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		for (int32 Fi = 0; Fi < TorchFlames.Num(); ++Fi)
		{
			FTorchFlameData& F = TorchFlames[Fi];
			const float P = F.Phase;
			const float BX = F.BasePos.X;
			const float BY = F.BasePos.Y;
			const float BZ = F.BasePos.Z;

			// Flicker: rapid scale oscillation
			const float Flicker = 1.0f + 0.15f * FMath::Sin(GT * 12.f + P)
				+ 0.08f * FMath::Sin(GT * 19.f + P * 2.3f)
				+ 0.05f * FMath::Sin(GT * 31.f + P * 5.1f);

			// Sway: slow lateral movement (wind)
			const float SwayX = 8.f * FMath::Sin(GT * 2.5f + P) + 4.f * FMath::Sin(GT * 4.1f + P * 1.7f);
			const float SwayY = 6.f * FMath::Sin(GT * 3.2f + P * 1.3f);

			// Height bob
			const float ZBob = 5.f * FMath::Sin(GT * 5.f + P * 0.9f);

			if (F.Core)
			{
				F.Core->SetWorldLocation(FVector(BX + SwayX * 0.5f, BY + SwayY * 0.5f, BZ + 30.f + ZBob));
				F.Core->SetWorldScale3D(FVector(0.30f * Flicker, 0.30f * Flicker, 1.8f * Flicker));
			}
			if (F.Mid)
			{
				F.Mid->SetWorldLocation(FVector(BX + SwayX * 0.7f, BY + SwayY * 0.7f, BZ + 20.f + ZBob * 0.8f));
				F.Mid->SetWorldScale3D(FVector(0.50f * Flicker, 0.50f * Flicker, 2.2f * Flicker));
			}
			if (F.Outer)
			{
				F.Outer->SetWorldLocation(FVector(BX + SwayX, BY + SwayY, BZ + 10.f + ZBob * 0.5f));
				F.Outer->SetWorldScale3D(FVector(0.70f * Flicker * 0.95f, 0.70f * Flicker * 0.95f, 2.8f * Flicker));
			}
			if (F.Tip)
			{
				F.Tip->SetWorldLocation(FVector(BX + SwayX * 1.3f, BY + SwayY * 1.3f, BZ + 100.f + ZBob * 1.5f));
				F.Tip->SetWorldScale3D(FVector(0.25f * Flicker, 0.25f * Flicker, 1.5f * Flicker * 1.1f));
			}
		}
	}

	// ---- Wet roads at night (dew/humidity effect) ----
	if (RoadMaterials.Num() > 0)
	{
		const float NightFactor = FMath::Max(0.f, -SunSine);
		const float TargetWetness = NightFactor * 0.5f; // up to 50% wetness at midnight
		if (FMath::Abs(TargetWetness - GlobalWetness) > 0.02f)
		{
			GlobalWetness = FMath::FInterpTo(GlobalWetness, TargetWetness, DeltaTime, 0.5f);
			const float WetRoughness = FMath::Lerp(0.85f, 0.08f, GlobalWetness);
			const float DarkenFactor = FMath::Lerp(1.0f, 0.5f, GlobalWetness);
			const float WetMetallic = GlobalWetness * 0.15f; // specular reflections
			for (auto& RoadMat : RoadMaterials)
			{
				if (RoadMat)
				{
					RoadMat->SetScalarParameterValue(TEXT("Roughness"), WetRoughness);
					RoadMat->SetScalarParameterValue(TEXT("Metallic"), WetMetallic);
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

	// ---- NPC enemies (night only, chase player, avoid safe zones) ----
	{
		const float CrabNF = FMath::Max(0.f, -SunSine);
		const bool bShouldShowCrabs = (CrabNF > 0.85f) || bNightModeLocked;

		if (bShouldShowCrabs != bCrabsNightVisible)
		{
			bCrabsNightVisible = bShouldShowCrabs;
			if (bShouldShowCrabs)
			{
				// Night starting — fresh wave
				CrabsAlive = NumGiantCrabs;
				WaveRestTimer = -1.f;
				for (int32 CI = 0; CI < NumGiantCrabs; ++CI)
				{
					if (Crabs[CI].Actor) RespawnCrab(CI);
				}
			}
			else
			{
				// Day starting — kill all, hide them
				for (int32 CI = 0; CI < NumGiantCrabs; ++CI)
				{
					if (Crabs[CI].Actor)
					{
						Crabs[CI].Actor->SetActorHiddenInGame(true);
						Crabs[CI].bVisible = false;
						Crabs[CI].Health = 0.f;
					}
				}
				CrabsAlive = 0;
				WaveRestTimer = -1.f;
			}
		}

		// Wave rest timer — after all NPCs killed, rest then respawn a new wave
		if (WaveRestTimer >= 0.f && bCrabsNightVisible)
		{
			WaveRestTimer -= DeltaTime;
			if (WaveRestTimer <= 0.f)
			{
				// Rest period over — respawn all NPCs for the next wave
				WaveRestTimer = -1.f;
				CrabsAlive = NumGiantCrabs;
				for (int32 CI = 0; CI < NumGiantCrabs; ++CI)
				{
					RespawnCrab(CI);
				}
			}
		}

		// Get player location once (invisible to NPCs while tripping)
		FVector PlayerLoc = FVector::ZeroVector;
		bool bPlayerTripping = false;
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APHCCityCharacter* PChar = Cast<APHCCityCharacter>(PC->GetPawn()))
			{
				PlayerLoc = PChar->GetActorLocation();
				bPlayerTripping = PChar->bIsTripping;
			}
		}

		if (bCrabsNightVisible)
		{
		const float CrabSpeed = 2094.f;
		const float GaitFreq = 4.0f;
		const float MinPlayerDist = 1200.f; // get very close before orbiting
		const float PaceRadius = 800.f;     // tight orbit around the player

		for (int32 CI = 0; CI < NumGiantCrabs; ++CI)
		{
			FCrabData& CD = Crabs[CI];
			if (!CD.Actor || !CD.bVisible) continue;

			// --- Movement: approach player but keep distance ---
			FVector CPos = CD.Actor->GetActorLocation();

			FVector MoveDir;
			if (bPlayerTripping)
			{
				// Player is tripping — crabs wander randomly instead of chasing
				float WanderAngle = (CI * 137.f + GetWorld()->GetTimeSeconds() * 0.3f);
				WanderAngle = FMath::Fmod(WanderAngle, 360.f);
				float WR = FMath::DegreesToRadians(WanderAngle);
				MoveDir = FVector(FMath::Cos(WR), FMath::Sin(WR), 0.f);
			}
			else
			{
			FVector ToPlayer = PlayerLoc - CPos;
			ToPlayer.Z = 0;
			const float DistToPlayer = ToPlayer.Size2D();

			if (DistToPlayer > MinPlayerDist)
			{
				// Far away — walk toward player
				MoveDir = ToPlayer.GetSafeNormal2D();
			}
			else
			{
				// Close — pace laterally around the player (orbit)
				FVector Tangent(-ToPlayer.Y, ToPlayer.X, 0.f);
				Tangent = Tangent.GetSafeNormal2D();
				// Alternate orbit direction per crab index
				if (CI % 2 == 0) Tangent = -Tangent;
				// Slight drift outward if too close
				FVector Outward = -ToPlayer.GetSafeNormal2D();
				float InwardBias = (DistToPlayer < PaceRadius) ? 0.4f : -0.1f;
				MoveDir = (Tangent + Outward * InwardBias).GetSafeNormal2D();
			}
			}

			FVector NewPos = CPos + MoveDir * CrabSpeed * DeltaTime;
			NewPos.Z = GetTerrainHeight(NewPos.X, NewPos.Y);

			// Only move if destination is not in a safe zone
			if (!IsInCrabForbiddenZone(NewPos))
			{
				CD.Actor->SetActorLocation(NewPos);
			}

			// Face movement direction (head is at local +Y)
			if (!MoveDir.IsNearlyZero())
			{
				FRotator DesiredRot(0, MoveDir.Rotation().Yaw - 90.f, 0);
				FRotator CurRot = CD.Actor->GetActorRotation();
				CD.Actor->SetActorRotation(
					FMath::RInterpTo(CurRot, DesiredRot, DeltaTime, 4.0f));
			}

			// --- Damage flash (briefly flash NPC body meshes red) ---
			if (CD.DamageFlashTimer > 0.f)
			{
				CD.DamageFlashTimer -= DeltaTime;
				if (CD.DamageFlashTimer > 0.f)
				{
					const FLinearColor RedFlash(4.0f, 0.0f, 0.0f);
					for (int32 MI = 0; MI < CD.BodyMIDs.Num(); MI++)
					{
						if (CD.BodyMIDs[MI]) CD.BodyMIDs[MI]->SetVectorParameterValue(TEXT("EmissiveColor"), RedFlash);
					}
				}
				else
				{
					for (int32 MI = 0; MI < CD.BodyMIDs.Num(); MI++)
					{
						if (CD.BodyMIDs[MI]) CD.BodyMIDs[MI]->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
					}
				}
			}

			// --- Biped walk animation (legs + arms swing) ---
			CD.AnimTime += DeltaTime;

			// Legs: HipJoints[0]=left, [1]=right — swing in Roll (around X)
			for (int i = 0; i < CD.HipJoints.Num() && i < CD.HipBaseRot.Num(); i++)
			{
				float Phase = (i == 0) ? 0.f : UE_PI;
				float A = CD.AnimTime * GaitFreq * UE_TWO_PI + Phase;
				float Swing = FMath::Sin(A) * 25.f;
				FRotator HR = CD.HipBaseRot[i];
				HR.Roll += Swing;
				CD.HipJoints[i]->SetRelativeRotation(HR);
			}

			// Arms: KneeJoints[0]=left, [1]=right — swing opposite to same-side leg
			for (int i = 0; i < CD.KneeJoints.Num() && i < CD.KneeBaseRot.Num(); i++)
			{
				float Phase = (i == 0) ? UE_PI : 0.f;
				float A = CD.AnimTime * GaitFreq * UE_TWO_PI + Phase;
				float Swing = FMath::Sin(A) * 20.f;
				FRotator KR = CD.KneeBaseRot[i];
				KR.Roll += Swing;
				CD.KneeJoints[i]->SetRelativeRotation(KR);
			}
		}
		} // if bCrabsNightVisible

		// --- Projectile vs crab collision (all bullets + RPGs) ---
		// Get player character for kill tracking
		APHCCityCharacter* PlayerChar = nullptr;
		if (APlayerController* PCC = World->GetFirstPlayerController())
		{
			PlayerChar = Cast<APHCCityCharacter>(PCC->GetPawn());
		}

		for (TActorIterator<APHCMissileProjectile> It(World); It; ++It)
		{
			APHCMissileProjectile* Proj = *It;
			if (!Proj) continue;

			const FVector PLoc = Proj->GetActorLocation();
			const bool bIsRPG = Proj->bIsRPGRocket;
			const float HitRadius = bIsRPG ? 800.f : 400.f;
			const float BulletDamage = bIsRPG ? 100.f : 11.12f; // RPG = instant, Gun = ~11.12 per hit (9 shots to kill)

			for (int32 CI = 0; CI < NumGiantCrabs; ++CI)
			{
				if (!Crabs[CI].Actor || !Crabs[CI].bVisible) continue;
				const FVector CLoc = Crabs[CI].Actor->GetActorLocation();
				if (FVector::Dist2D(PLoc, CLoc) < HitRadius)
				{
					Crabs[CI].Health -= BulletDamage;
					Crabs[CI].DamageFlashTimer = 0.2f;

					if (Crabs[CI].Health <= 0.f)
					{
						// NPC killed — explode and stay dead until next wave
						Proj->SpawnExplosion(CLoc);
						if (PlayerChar)
						{
							PlayerChar->CrabKills++;
							PlayerChar->KillStreakCount++;
							PlayerChar->KillStreakTimer = 5.f;
							PlayerChar->AddXP(50);
						}

						// Loot crate drop instead of fixed coins
						if (PlayerChar)
						{
							PlayerChar->StartLootCrate();
						}

						Crabs[CI].Actor->SetActorHiddenInGame(true);
						Crabs[CI].bVisible = false;
						CrabsAlive--;

						// All NPCs eliminated — start rest period
						if (CrabsAlive <= 0)
						{
							WaveRestTimer = WaveRestDuration;
							CrabsAlive = 0;
						}
					}

					Proj->Destroy();
					break;
				}
			}

			// NPC helicopter proximity damage from projectiles (RPG, vehicle rockets)
			if (NPCHelicopter && bNPCHeliVisible && !bNPCHeliCrashing && Proj && IsValid(Proj))
			{
				const FVector HLoc = NPCHelicopter->GetActorLocation();
				if (FVector::Dist(PLoc, HLoc) < HitRadius)
				{
					NPCHeliHealth -= BulletDamage;
					NPCHeliDamageFlashTimer = 0.15f;
					if (NPCHeliHealth <= 0.f)
					{
						OnNPCHeliDestroyed();
					}
					Proj->Destroy();
				}
			}

			// NPC helicopter 2 proximity damage
			if (NPCHelicopter2 && bNPCHeliVisible2 && !bNPCHeliCrashing2 && Proj && IsValid(Proj))
			{
				const FVector HLoc2 = NPCHelicopter2->GetActorLocation();
				if (FVector::Dist(PLoc, HLoc2) < HitRadius)
				{
					NPCHeliHealth2 -= BulletDamage;
					NPCHeliDamageFlashTimer2 = 0.15f;
					if (NPCHeliHealth2 <= 0.f)
					{
						OnNPCHeli2Destroyed();
					}
					Proj->Destroy();
				}
			}

			// NPC player proximity damage — removed (no more NPC players)
		}

		// --- Crab contact damage to player (immune while tripping) ---
		if (PlayerChar && PlayerChar->Health > 0.f && !bPlayerTripping)
		{
			const float ContactRadius = 450.f;
			const float ContactDamage = 25.f;
			const float ContactCooldownTime = 0.6f;

			for (int32 CI = 0; CI < NumGiantCrabs; ++CI)
			{
				FCrabData& CD = Crabs[CI];
				if (!CD.Actor || !CD.bVisible) continue;

				// Tick down cooldown
				if (CD.ContactCooldown > 0.f)
				{
					CD.ContactCooldown -= DeltaTime;
					continue;
				}

				const FVector CrabLoc = CD.Actor->GetActorLocation();
				const float Dist = FVector::Dist2D(PlayerLoc, CrabLoc);
				const float ZDiff = FMath::Abs(PlayerLoc.Z - CrabLoc.Z);
				if (Dist < ContactRadius && ZDiff < 500.f)
				{
					PlayerChar->Health -= ContactDamage;
					PlayerChar->DamageFlashTimer = 0.3f;
					CD.ContactCooldown = ContactCooldownTime;

					if (PlayerChar->Health <= 0.f)
					{
						// Player died — increment deaths, respawn
						PlayerChar->Deaths++;
						PlayerChar->Health = PlayerChar->MaxHealth;
						PlayerChar->SetActorLocation(FVector(0.f, 0.f, 100.f));
					}
				}
			}
		}
	}

	// ---- Natural Disasters ----
	{
		using DT = EDisasterType;

		// Random disaster trigger
		if (ActiveDisaster == DT::None)
		{
			DisasterCooldown -= DeltaTime;
			if (DisasterCooldown <= 0.f)
			{
				const int32 Pick = FMath::RandRange(0, 3);
				switch (Pick)
				{
				case 0: ActiveDisaster = DT::Tornado;       DisasterTimer = 30.f; break;
				case 1: ActiveDisaster = DT::Earthquake;     DisasterTimer = 20.f; break;
				case 2: ActiveDisaster = DT::Thunderstorm;   DisasterTimer = 45.f; break;
				case 3: ActiveDisaster = DT::MeteorShower;   DisasterTimer = 40.f; break;
				}
				DisasterCooldown = FMath::FRandRange(120.f, 300.f);
			}
		}

		if (ActiveDisaster != DT::None)
		{
			DisasterTimer -= DeltaTime;

			// ---- TORNADO ----
			if (ActiveDisaster == DT::Tornado)
			{
				// Spawn tornado actor if needed
				if (!TornadoActor || TornadoActor->IsHidden())
				{
					if (!TornadoActor)
					{
						SpawnTornado();
					}
					if (TornadoActor)
					{
						// Place near world origin at random angle
						float RandAngle = FMath::FRandRange(0.f, 360.f);
						TornadoPos = FVector(
							FMath::Cos(FMath::DegreesToRadians(RandAngle)) * 8000.f,
							FMath::Sin(FMath::DegreesToRadians(RandAngle)) * 8000.f,
							0.f);
						TornadoActor->SetActorLocation(TornadoPos);
						TornadoActor->SetActorHiddenInGame(false);
						TornadoAngle = FMath::FRandRange(0.f, 360.f);
					}
				}

				if (TornadoActor)
				{
					// Spin the tornado
					TornadoSpinTime += DeltaTime;
					TornadoActor->SetActorRotation(FRotator(0, TornadoSpinTime * 720.f, 0));

					// Wander
					TornadoAngle += FMath::FRandRange(-30.f, 30.f) * DeltaTime;
					FVector Dir(FMath::Cos(FMath::DegreesToRadians(TornadoAngle)),
						FMath::Sin(FMath::DegreesToRadians(TornadoAngle)), 0.f);
					TornadoPos += Dir * 2000.f * DeltaTime;
					TornadoActor->SetActorLocation(TornadoPos);
				}

				// Atmospheric effects: thicken fog, speed clouds
				if (FogComp)
				{
					float BaseDensity = FogComp->FogDensity;
					FogComp->SetFogDensity(FMath::Max(BaseDensity, 0.002f));
				}
				if (CloudLayerActor)
				{
					CloudLayerActor->AddActorWorldOffset(FVector(DeltaTime * 80.f, DeltaTime * 30.f, 0.f));
				}
				if (CloudLayerActor2)
				{
					CloudLayerActor2->AddActorWorldOffset(FVector(DeltaTime * 50.f, DeltaTime * 15.f, 0.f));
				}
			}

			// ---- EARTHQUAKE ----
			if (ActiveDisaster == DT::Earthquake)
			{
				EarthquakePhase += DeltaTime * 25.f;

				// Intensity ramp: up over 3s, sustain, down over last 3s
				float Duration = 20.f;
				float Elapsed = Duration - DisasterTimer;
				if (Elapsed < 3.f)
					EarthquakeIntensity = Elapsed / 3.f;
				else if (DisasterTimer < 3.f)
					EarthquakeIntensity = DisasterTimer / 3.f;
				else
					EarthquakeIntensity = 1.f;

				// Shake the player pawn
				UWorld* W = GetWorld();
				if (W)
				{
					APawn* P = W->GetFirstPlayerController() ? W->GetFirstPlayerController()->GetPawn() : nullptr;
					if (P)
					{
						FVector ShakeOffset(
							FMath::Sin(EarthquakePhase * 1.1f) * 15.f * EarthquakeIntensity,
							FMath::Sin(EarthquakePhase * 0.9f) * 10.f * EarthquakeIntensity,
							FMath::Sin(EarthquakePhase * 1.7f) * 5.f * EarthquakeIntensity);
						P->AddActorWorldOffset(ShakeOffset);
					}
				}

				// Dusty fog
				if (FogComp)
				{
					FogComp->SetFogDensity(FMath::Max(FogComp->FogDensity, 0.001f * EarthquakeIntensity));
					FogComp->SetFogInscatteringColor(FLinearColor(0.4f, 0.3f, 0.15f));
				}
				if (PostProcessVolume)
				{
					PostProcessVolume->Settings.SceneFringeIntensity = 0.4f + 1.6f * EarthquakeIntensity;
				}
			}

			// ---- THUNDERSTORM ----
			if (ActiveDisaster == DT::Thunderstorm)
			{
				// Darken sky
				if (SkyLightComp)
				{
					SkyLightComp->SetIntensity(FMath::Min(SkyLightComp->Intensity, 0.4f));
				}
				if (FogComp)
				{
					FogComp->SetFogDensity(FMath::Max(FogComp->FogDensity, 0.0025f));
					FogComp->SetFogInscatteringColor(FLinearColor(0.15f, 0.15f, 0.25f));
				}

				// Lightning flash
				ThunderTimer -= DeltaTime;
				if (ThunderTimer <= 0.f)
				{
					LightningFlashAlpha = 1.f;
					ThunderTimer = FMath::FRandRange(3.f, 8.f);
				}

				if (LightningFlashAlpha > 0.f)
				{
					LightningFlashAlpha -= DeltaTime * 8.f; // rapid decay
					if (LightningFlashAlpha < 0.f) LightningFlashAlpha = 0.f;

					if (SunLightComp)
					{
						SunLightComp->SetIntensity(SunLightComp->Intensity + 20.f * LightningFlashAlpha);
					}
					if (PostProcessVolume)
					{
						float F = LightningFlashAlpha;
						PostProcessVolume->Settings.ColorGain = FVector4(
							1.f + 2.f * F, 1.f + 2.f * F, 1.f + 2.f * F, 1.f);
					}
				}

				// Speed up clouds
				if (CloudLayerActor)
				{
					CloudLayerActor->AddActorWorldOffset(FVector(DeltaTime * 40.f, DeltaTime * 15.f, 0.f));
				}
				if (CloudLayerActor2)
				{
					CloudLayerActor2->AddActorWorldOffset(FVector(DeltaTime * 25.f, DeltaTime * 10.f, 0.f));
				}
			}

			// ---- METEOR SHOWER ----
			if (ActiveDisaster == DT::MeteorShower)
			{
				MeteorSpawnTimer -= DeltaTime;
				UWorld* W = GetWorld();

				if (MeteorSpawnTimer <= 0.f && ActiveMeteors.Num() < 8 && W && SphereMesh && ColorMaterial)
				{
					MeteorSpawnTimer = FMath::FRandRange(0.5f, 1.2f);

					FVector StartPos(
						FMath::FRandRange(-25000.f, 25000.f),
						FMath::FRandRange(-25000.f, 25000.f),
						FMath::FRandRange(12000.f, 15000.f));

					AActor* Meteor = W->SpawnActor<AActor>(AActor::StaticClass(), StartPos, FRotator::ZeroRotator);
					if (Meteor)
					{
						USceneComponent* MRoot = NewObject<USceneComponent>(Meteor, TEXT("MRoot"));
						MRoot->RegisterComponent();
						Meteor->SetRootComponent(MRoot);
						Meteor->AddInstanceComponent(MRoot);

						auto* MC = NewObject<UStaticMeshComponent>(Meteor, TEXT("MeteorMesh"));
						MC->SetStaticMesh(SphereMesh);
						MC->SetRelativeScale3D(FVector(FMath::FRandRange(2.f, 5.f)));
						MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						MC->SetCastShadow(false);
						auto* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, MC);
						Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.3f, 0.08f, 0.02f));
						Mat->SetScalarParameterValue(TEXT("Metallic"), 0.6f);
						Mat->SetScalarParameterValue(TEXT("Roughness"), 0.4f);
						Mat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(5.f, 2.f, 0.3f));
						MC->SetMaterial(0, Mat);
						MC->AttachToComponent(MRoot, FAttachmentTransformRules::KeepRelativeTransform);
						MC->RegisterComponent();
						Meteor->AddInstanceComponent(MC);

						auto* PL = NewObject<UPointLightComponent>(Meteor, TEXT("MeteorLight"));
						PL->SetIntensity(6000.f);
						PL->SetAttenuationRadius(3000.f);
						PL->SetLightColor(FColor(255, 150, 30));
						PL->SetCastShadows(false);
						PL->AttachToComponent(MRoot, FAttachmentTransformRules::KeepRelativeTransform);
						PL->RegisterComponent();
						Meteor->AddInstanceComponent(PL);

						ActiveMeteors.Add(Meteor);

						float Angle = FMath::FRandRange(-70.f, -45.f);
						float HDir = FMath::FRandRange(0.f, 360.f);
						float Speed = FMath::FRandRange(8000.f, 15000.f);
						FVector Vel(
							FMath::Cos(FMath::DegreesToRadians(HDir)) * FMath::Cos(FMath::DegreesToRadians(Angle)) * Speed,
							FMath::Sin(FMath::DegreesToRadians(HDir)) * FMath::Cos(FMath::DegreesToRadians(Angle)) * Speed,
							FMath::Sin(FMath::DegreesToRadians(Angle)) * Speed);
						MeteorVelocities.Add(Vel);
					}
				}

				// Move and cull meteors
				for (int32 i = ActiveMeteors.Num() - 1; i >= 0; --i)
				{
					if (ActiveMeteors[i])
					{
						ActiveMeteors[i]->AddActorWorldOffset(MeteorVelocities[i] * DeltaTime);
						if (ActiveMeteors[i]->GetActorLocation().Z < 50.f)
						{
							ActiveMeteors[i]->Destroy();
							ActiveMeteors.RemoveAt(i);
							MeteorVelocities.RemoveAt(i);
						}
					}
					else
					{
						ActiveMeteors.RemoveAt(i);
						MeteorVelocities.RemoveAt(i);
					}
				}
			}

			// ---- Disaster expiration ----
			if (DisasterTimer <= 0.f)
			{
				// Cleanup
				if (ActiveDisaster == DT::Tornado && TornadoActor)
				{
					TornadoActor->SetActorHiddenInGame(true);
				}
				if (ActiveDisaster == DT::Earthquake && PostProcessVolume)
				{
					PostProcessVolume->Settings.SceneFringeIntensity = 0.4f;
					EarthquakeIntensity = 0.f;
				}
				if (ActiveDisaster == DT::Thunderstorm)
				{
					LightningFlashAlpha = 0.f;
					ThunderTimer = 0.f;
				}
				if (ActiveDisaster == DT::MeteorShower)
				{
					for (auto& M : ActiveMeteors) { if (M) M->Destroy(); }
					ActiveMeteors.Empty();
					MeteorVelocities.Empty();
					MeteorSpawnTimer = 0.f;
				}
				ActiveDisaster = DT::None;
				DisasterTimer = 0.f;
			}
		}
	}

	// ---- Pyramid interior psychedelic color animation ----
	if (PyramidInteriorMIDs.Num() > 0)
	{
		PyramidColorTime += DeltaTime;
		const int32 NumMIDs = PyramidInteriorMIDs.Num();
		for (int32 i = 0; i < NumMIDs; ++i)
		{
			if (!PyramidInteriorMIDs[i]) continue;

			const float PhaseOffset = (float)i / (float)NumMIDs;

			// Hue cycles full rainbow at 0.3 Hz (~3.3s per cycle), offset per element
			float HueDeg = FMath::Fmod((PyramidColorTime * 0.3f + PhaseOffset) * 360.f, 360.f);
			if (HueDeg < 0.f) HueDeg += 360.f;

			// Brightness pulse at 1.5 Hz (540 deg/s), per-element phase offset
			const float PulseAngle = FMath::DegreesToRadians(PyramidColorTime * 540.f + (float)i * 40.f);
			const float Pulse = 0.6f + 0.4f * FMath::Sin(PulseAngle);

			// HSV→RGB (H=degrees, S=1, V=Pulse)
			FLinearColor RGB = FLinearColor(HueDeg, 1.f, Pulse, 1.f).HSVToLinearRGB();

			// Scale to HDR for bright emissive glow
			const float Intensity = 2.f + 3.f * Pulse;
			PyramidInteriorMIDs[i]->SetVectorParameterValue(TEXT("EmissiveColor"),
				FLinearColor(RGB.R * Intensity, RGB.G * Intensity, RGB.B * Intensity));
		}
	}

	// ---- Mushroom Psychedelic Trip Effect ----
	if (MushroomTripTimer >= 0.f && PostProcessVolume)
	{
		MushroomTripTimer += DeltaTime;

		if (MushroomTripTimer >= MushroomTripDuration)
		{
			// Trip ended — reset everything
			MushroomTripTimer = -1.f;
			MushroomTripDuration = 60.f;
			TripDoseCount = 0;
			TripIntensityMultiplier = 1.f;
			TripIntensity = 0.f;
			TripFixationAlpha = 0.f;
			if (UWorld* W = GetWorld())
			{
				if (APlayerController* PC = W->GetFirstPlayerController())
				{
					if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
					{
						Char->FollowCamera->SetFieldOfView(TripBaseFOV);
						Char->CameraBoom->TargetArmLength = 450.f;
						Char->CameraBoom->SocketOffset = FVector(0.f, 75.f, 60.f);
						FRotator R = Char->GetActorRotation();
						Char->SetActorRotation(FRotator(R.Pitch, R.Yaw, 0.f));
						Char->bIsTripping = false;
					}
				}
			}
			static IConsoleVariable* TAAWeightCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAACurrentFrameWeight"));
			if (TAAWeightCVar) TAAWeightCVar->Set(0.1f);

			// Destroy world-space hallucination actor
			if (TripHallucinationActor)
			{
				TripHallucinationActor->Destroy();
				TripHallucinationActor = nullptr;
			}
			TripTileHISM = nullptr;
			TripBarHISM = nullptr;
			TripDotHISM = nullptr;
			TripTileMID = nullptr;
			TripDecalMID = nullptr;
			TripDecals.Empty();
			TripGlowLights.Empty();
			TripSkyDome = nullptr;
			TripSkyMID = nullptr;
			bTripHallucinationsSpawned = false;

			// Restore river + ocean water to normal
			if (bWaterTripping)
			{
				bWaterTripping = false;
				// Restore river
				if (RiverWaterPMC && GlassMaterial)
				{
					UMaterialInstanceDynamic* RR = UMaterialInstanceDynamic::Create(GlassMaterial, RiverWaterPMC);
					if (RR)
					{
						RR->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.05f, 0.16f, 0.28f));
						RR->SetScalarParameterValue(TEXT("Opacity"), 0.60f);
						RR->SetScalarParameterValue(TEXT("Metallic"), 0.2f);
						RR->SetScalarParameterValue(TEXT("Roughness"), 0.06f);
						RR->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.04f, 0.08f, 0.15f));
						RiverWaterPMC->SetMaterial(0, RR);
					}
				}
				// Restore ocean/bay surfaces
				if (GetWorld() && GlassMaterial && TripSkyMaterial)
				{
					for (TActorIterator<AActor> It(GetWorld()); It; ++It)
					{
						TArray<UStaticMeshComponent*> Meshes;
						(*It)->GetComponents<UStaticMeshComponent>(Meshes);
						for (auto* MC : Meshes)
						{
							if (!MC || !MC->GetMaterial(0)) continue;
							UMaterialInterface* MatIF = MC->GetMaterial(0);
							if (MatIF && MatIF->GetBaseMaterial() == TripSkyMaterial)
							{
								UMaterialInstanceDynamic* WR = UMaterialInstanceDynamic::Create(GlassMaterial, MC);
								if (WR)
								{
									WR->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.03f, 0.10f, 0.22f));
									WR->SetScalarParameterValue(TEXT("Opacity"), 0.70f);
									WR->SetScalarParameterValue(TEXT("Metallic"), 0.15f);
									WR->SetScalarParameterValue(TEXT("Roughness"), 0.08f);
									WR->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.04f, 0.08f, 0.14f));
									MC->SetMaterial(0, WR);
								}
							}
						}
					}
				}
				RiverTripMID = nullptr;
				WaterMIDs.Empty();
			}

			// Restore sky visibility
			if (StarfieldActor) StarfieldActor->SetActorHiddenInGame(false);
			if (CloudLayerActor) CloudLayerActor->SetActorHiddenInGame(false);
			if (CloudLayerActor2) CloudLayerActor2->SetActorHiddenInGame(false);
			if (CloudLayerActor3) CloudLayerActor3->SetActorHiddenInGame(false);

			// Remove custom chromatic aberration blendable
			if (TripChromaMID && PostProcessVolume)
			{
				auto& Blendables = PostProcessVolume->Settings.WeightedBlendables.Array;
				Blendables.RemoveAll([this](const FWeightedBlendable& WB)
				{
					return WB.Object == TripChromaMID;
				});
				TripChromaMID = nullptr;
			}

			// Restore sun and fog colors
			if (SunLightComp)
				SunLightComp->SetLightColor(FLinearColor::White);
			if (FogComp)
			{
				FogComp->SetFogInscatteringColor(FLinearColor(0.45f, 0.55f, 0.65f));
				FogComp->SetFogHeightFalloff(0.08f);
				// Density will be restored by day/night cycle on next tick
			}

			// Restore world materials from trip morphing
			if (GroundSlabMID)
			{
				GroundSlabMID->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.12f, 0.18f, 0.08f));
				GroundSlabMID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
			}
			for (auto& RoadMat : RoadMaterials)
			{
				if (!RoadMat) continue;
				RoadMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.04f, 0.04f, 0.05f));
				RoadMat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
			}
			for (auto& WinMat : NightWindowMaterials)
			{
				if (!WinMat) continue;
				// Window emissive will be restored by day/night cycle on next tick
				WinMat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
			}
			if (SkyLightComp)
			{
				SkyLightComp->SetLightColor(FLinearColor(0.6f, 0.7f, 1.0f));
				SkyLightComp->SetIntensity(0.3f);
			}
		}
		else
		{
			FPostProcessSettings& PP = PostProcessVolume->Settings;
			const float TT = MushroomTripTimer;
			const float GT = GetWorld() ? GetWorld()->GetTimeSeconds() : TT;

			// === TRIP ENVELOPE (scales with dynamic duration) ===
			const float FadeInEnd = FMath::Min(10.f, MushroomTripDuration * 0.15f);
			const float FadeOutStart = MushroomTripDuration - FMath::Min(15.f, MushroomTripDuration * 0.25f);
			float Envelope;
			if (TT < FadeInEnd)
			{
				float A = TT / FadeInEnd;
				Envelope = A * A * A;
			}
			else if (TT < FadeOutStart)
			{
				Envelope = 1.f;
			}
			else
			{
				float A = (TT - FadeOutStart) / 40.f;
				Envelope = FMath::Max(1.f - A * A, 0.f);
			}

			// === MACRO WAVE INTENSITY ===
			const float MacroWave =
				0.55f * FMath::Sin(GT * 0.0503f) +
				0.30f * FMath::Sin(GT * 0.0311f + 1.7f) +
				0.15f * FMath::Sin(GT * 0.0171f + 3.9f);
			const float WaveIntensity = 0.65f + 0.35f * MacroWave;
			const float BaseIntensity = Envelope * WaveIntensity;
			const float Intensity = FMath::Min(BaseIntensity * TripIntensityMultiplier, 1.f);
			TripIntensity = Intensity; // store for HUD access
			// Overdrive allows select effects to exceed normal peak (capped at 4x)
			const float Overdrive = FMath::Min(BaseIntensity * TripIntensityMultiplier, 4.f);

			// === FIXATION DETECTION ===
			float FixationTarget = 0.f;
			if (UWorld* W = GetWorld())
			{
				if (APlayerController* PC = W->GetFirstPlayerController())
				{
					FRotator CamRot = PC->GetControlRotation();
					FRotator RotDiff = (CamRot - TripLastCameraRot).GetNormalized();
					float RotDelta = FMath::Abs(RotDiff.Pitch) + FMath::Abs(RotDiff.Yaw) + FMath::Abs(RotDiff.Roll);
					TripLastCameraRot = CamRot;
					FixationTarget = (RotDelta < 0.5f) ? 1.f : 0.f;
				}
			}
			const float FixationSpeed = (FixationTarget > TripFixationAlpha) ? 0.5f : 3.3f;
			TripFixationAlpha = FMath::FInterpTo(TripFixationAlpha, FixationTarget, DeltaTime, FixationSpeed);
			const float FixMul = 0.5f + 0.5f * TripFixationAlpha;

			// === WORLD-SPACE HALLUCINATIONS ===
			{
				FVector PlayerPos = FVector::ZeroVector;
				if (UWorld* W = GetWorld())
				{
					if (APlayerController* PC = W->GetFirstPlayerController())
					{
						if (APawn* P = PC->GetPawn())
							PlayerPos = P->GetActorLocation();
					}
				}

				// 10-color palette (galaxy nighttime — cool, deep, ethereal)
				static const FLinearColor TripPalette[] = {
					FLinearColor(0.05f, 0.08f, 0.30f),  // deep space blue
					FLinearColor(0.30f, 0.10f, 0.55f),  // nebula violet
					FLinearColor(0.00f, 0.20f, 0.50f),  // midnight ocean
					FLinearColor(0.15f, 0.40f, 0.55f),  // aurora teal
					FLinearColor(0.10f, 0.05f, 0.35f),  // void indigo
					FLinearColor(0.50f, 0.40f, 0.70f),  // soft lavender
					FLinearColor(0.02f, 0.15f, 0.40f),  // dark cosmic blue
					FLinearColor(0.40f, 0.15f, 0.60f),  // deep magenta
					FLinearColor(0.20f, 0.02f, 0.45f),  // dark purple
					FLinearColor(0.05f, 0.35f, 0.45f),  // deep teal
				};
				constexpr int32 PalSize = 10;

				auto SamplePal = [&](float PalT) -> FLinearColor
				{
					PalT = FMath::Frac(PalT) * PalSize;
					const int32 I0 = FMath::FloorToInt32(PalT) % PalSize;
					const int32 I1 = (I0 + 1) % PalSize;
					return FMath::Lerp(TripPalette[I0], TripPalette[I1],
						PalT - FMath::FloorToFloat(PalT));
				};

				// Spawn ground shadow tiles + custom CA on first frame
				if (!bTripHallucinationsSpawned && GetWorld())
				{
					bTripHallucinationsSpawned = true;
					FActorSpawnParameters SpawnParams;
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
					TripHallucinationActor = GetWorld()->SpawnActor<AActor>(
						AActor::StaticClass(), PlayerPos, FRotator::ZeroRotator, SpawnParams);
					if (TripHallucinationActor)
					{
						TripHallucinationActor->SetRootComponent(
							NewObject<USceneComponent>(TripHallucinationActor, TEXT("TripRoot")));
						TripHallucinationActor->GetRootComponent()->RegisterComponent();
					}

					// Spawn fractal sky dome (replaces sky with three.js-style shader)
					if (TripSkyMaterial && SphereMesh && TripHallucinationActor)
					{
						TripSkyDome = NewObject<UStaticMeshComponent>(
							TripHallucinationActor, TEXT("TripSkyDome"));
						TripSkyDome->SetStaticMesh(SphereMesh);
						TripSkyDome->SetWorldLocation(PlayerPos);
						TripSkyDome->SetWorldScale3D(FVector(4500.f, 4500.f, 4500.f));
						TripSkyDome->SetWorldRotation(FRotator(90.f, 0.f, 0.f)); // rotate pole to horizon
						TripSkyDome->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						TripSkyDome->CastShadow = false;
						TripSkyDome->AttachToComponent(
							TripHallucinationActor->GetRootComponent(),
							FAttachmentTransformRules::KeepWorldTransform);
						TripSkyDome->RegisterComponent();

						TripSkyMID = UMaterialInstanceDynamic::Create(TripSkyMaterial, this);
						TripSkyMID->SetScalarParameterValue(TEXT("Bass"), 0.f);
						TripSkyMID->SetScalarParameterValue(TEXT("High"), 0.f);
						TripSkyMID->SetScalarParameterValue(TEXT("Level"), 0.5f);
						TripSkyMID->SetScalarParameterValue(TEXT("Mid"), 0.f);
						TripSkyMID->SetScalarParameterValue(TEXT("SceneIndex"), 0.f);
						TripSkyDome->SetMaterial(0, TripSkyMID);

						// Hide existing sky elements
						if (StarfieldActor) StarfieldActor->SetActorHiddenInGame(true);
						if (CloudLayerActor) CloudLayerActor->SetActorHiddenInGame(true);
						if (CloudLayerActor2) CloudLayerActor2->SetActorHiddenInGame(true);
						if (CloudLayerActor3) CloudLayerActor3->SetActorHiddenInGame(true);
						if (SkyLightComp) SkyLightComp->SetIntensity(0.1f);
					}

					// Apply trip sky pattern to river + ocean (not lakes/small pools)
					if (TripSkyMaterial && !bWaterTripping)
					{
						bWaterTripping = true;

						// River PMC
						if (RiverWaterPMC)
						{
							RiverTripMID = UMaterialInstanceDynamic::Create(TripSkyMaterial, RiverWaterPMC);
							if (RiverTripMID)
							{
								RiverTripMID->SetScalarParameterValue(TEXT("Bass"), 0.4f);
								RiverTripMID->SetScalarParameterValue(TEXT("High"), 0.3f);
								RiverTripMID->SetScalarParameterValue(TEXT("Level"), 0.5f);
								RiverTripMID->SetScalarParameterValue(TEXT("Mid"), 0.35f);
								RiverTripMID->SetScalarParameterValue(TEXT("SceneIndex"), 0.f);
								RiverWaterPMC->SetMaterial(0, RiverTripMID);
							}
						}

						// Ocean + bay: find large water surfaces using GlassMaterial
						// Skip small surfaces (lakes/pools) by checking scale
						if (GetWorld() && GlassMaterial)
						{
							for (TActorIterator<AActor> It(GetWorld()); It; ++It)
							{
								TArray<UStaticMeshComponent*> Meshes;
								(*It)->GetComponents<UStaticMeshComponent>(Meshes);
								for (auto* MC : Meshes)
								{
									if (!MC || !MC->GetMaterial(0)) continue;
									UMaterialInterface* MatIF = MC->GetMaterial(0);
									if (!MatIF || MatIF->GetBaseMaterial() != GlassMaterial) continue;
									// Only swap large water surfaces (ocean/bay scale > 100)
									FVector S = MC->GetComponentScale();
									if (S.X < 100.f && S.Y < 100.f) continue;
									UMaterialInstanceDynamic* TripWM = UMaterialInstanceDynamic::Create(TripSkyMaterial, MC);
									if (TripWM)
									{
										TripWM->SetScalarParameterValue(TEXT("Bass"), 0.4f);
										TripWM->SetScalarParameterValue(TEXT("High"), 0.3f);
										TripWM->SetScalarParameterValue(TEXT("Level"), 0.5f);
										TripWM->SetScalarParameterValue(TEXT("Mid"), 0.35f);
										TripWM->SetScalarParameterValue(TEXT("SceneIndex"), 0.f);
										MC->SetMaterial(0, TripWM);
										WaterMIDs.Add(TripWM);
									}
								}
							}
						}
					}

					// Spawn kaleidoscopic mirror triangle floor pattern
					if (ColorMaterial && CubeMesh && TripHallucinationActor)
					{
						const FVector FloorOrigin(PlayerPos.X, PlayerPos.Y, 0.f);
						auto MakeHISM = [&](UStaticMesh* Mesh, const TCHAR* Name)
							-> UHierarchicalInstancedStaticMeshComponent*
						{
							auto* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(
								TripHallucinationActor, Name);
							H->SetStaticMesh(Mesh);
							H->SetCollisionEnabled(ECollisionEnabled::NoCollision);
							H->CastShadow = false;
							H->SetWorldLocation(FloorOrigin);
							return H;
						};

						TripTileHISM = MakeHISM(CubeMesh, TEXT("TripMirrorHISM"));
						TripBarHISM  = MakeHISM(CubeMesh, TEXT("TripMirrorHISM2"));

						const float Z = 5.f;

						// Mirror triangles are thin flat cubes scaled to look
						// like elongated triangular shards, arranged in concentric
						// rings with alternating mirror symmetry.

						// Ring 1 — 6 inner shards (tight hexagonal star)
						for (int32 i = 0; i < 6; ++i)
						{
							const float A = i * 2.f * UE_PI / 6.f;
							const float Deg = FMath::RadiansToDegrees(A);
							TripTileHISM->AddInstance(FTransform(
								FRotator(0.f, Deg, 0.f),
								FVector(150.f * FMath::Cos(A), 150.f * FMath::Sin(A), Z),
								FVector(3.0f, 0.15f, 0.02f)), false);
						}

						// Ring 2 — 12 shards, alternating mirror flip
						for (int32 i = 0; i < 12; ++i)
						{
							const float A = i * 2.f * UE_PI / 12.f;
							const float Deg = FMath::RadiansToDegrees(A);
							const float Flip = (i % 2 == 0) ? 0.f : 30.f;
							TripTileHISM->AddInstance(FTransform(
								FRotator(0.f, Deg + Flip, 0.f),
								FVector(450.f * FMath::Cos(A), 450.f * FMath::Sin(A), Z),
								FVector(4.0f, 0.18f, 0.02f)), false);
						}

						// Ring 3 — 18 shards
						for (int32 i = 0; i < 18; ++i)
						{
							const float A = i * 2.f * UE_PI / 18.f;
							const float Deg = FMath::RadiansToDegrees(A);
							const float Flip = (i % 2 == 0) ? 0.f : 20.f;
							TripTileHISM->AddInstance(FTransform(
								FRotator(0.f, Deg + Flip, 0.f),
								FVector(900.f * FMath::Cos(A), 900.f * FMath::Sin(A), Z),
								FVector(5.0f, 0.20f, 0.02f)), false);
						}

						// Ring 4 — 24 shards (second layer, counter-rotating)
						for (int32 i = 0; i < 24; ++i)
						{
							const float A = i * 2.f * UE_PI / 24.f;
							const float Deg = FMath::RadiansToDegrees(A);
							const float Flip = (i % 2 == 0) ? 0.f : 15.f;
							TripBarHISM->AddInstance(FTransform(
								FRotator(0.f, Deg + Flip, 0.f),
								FVector(1500.f * FMath::Cos(A), 1500.f * FMath::Sin(A), Z),
								FVector(6.0f, 0.22f, 0.02f)), false);
						}

						// Ring 5 — 30 shards
						for (int32 i = 0; i < 30; ++i)
						{
							const float A = i * 2.f * UE_PI / 30.f;
							const float Deg = FMath::RadiansToDegrees(A);
							const float Flip = (i % 2 == 0) ? 0.f : 12.f;
							TripBarHISM->AddInstance(FTransform(
								FRotator(0.f, Deg + Flip, 0.f),
								FVector(2200.f * FMath::Cos(A), 2200.f * FMath::Sin(A), Z),
								FVector(7.0f, 0.25f, 0.02f)), false);
						}

						// Ring 6 — 36 outer shards
						for (int32 i = 0; i < 36; ++i)
						{
							const float A = i * 2.f * UE_PI / 36.f;
							const float Deg = FMath::RadiansToDegrees(A);
							const float Flip = (i % 2 == 0) ? 0.f : 10.f;
							TripBarHISM->AddInstance(FTransform(
								FRotator(0.f, Deg + Flip, 0.f),
								FVector(3200.f * FMath::Cos(A), 3200.f * FMath::Sin(A), Z),
								FVector(8.0f, 0.28f, 0.02f)), false);
						}

						// Ring 7 — 42 outermost shards
						for (int32 i = 0; i < 42; ++i)
						{
							const float A = i * 2.f * UE_PI / 42.f;
							const float Deg = FMath::RadiansToDegrees(A);
							const float Flip = (i % 2 == 0) ? 0.f : 8.6f;
							TripBarHISM->AddInstance(FTransform(
								FRotator(0.f, Deg + Flip, 0.f),
								FVector(4200.f * FMath::Cos(A), 4200.f * FMath::Sin(A), Z),
								FVector(9.0f, 0.30f, 0.02f)), false);
						}

						TripTileHISM->RegisterComponent();
						TripHallucinationActor->AddInstanceComponent(TripTileHISM);
						TripBarHISM->RegisterComponent();
						TripHallucinationActor->AddInstanceComponent(TripBarHISM);

						TripTileMID = UMaterialInstanceDynamic::Create(ColorMaterial, TripTileHISM);
						TripTileMID->SetVectorParameterValue(TEXT("BaseColor"),
							FLinearColor(0.05f, 0.05f, 0.08f));
						TripTileMID->SetScalarParameterValue(TEXT("Roughness"), 0.08f);
						TripTileMID->SetScalarParameterValue(TEXT("Metallic"), 0.95f);
						TripTileMID->SetVectorParameterValue(TEXT("EmissiveColor"),
							FLinearColor(0.f, 0.f, 0.f));
						TripTileHISM->SetMaterial(0, TripTileMID);
						TripBarHISM->SetMaterial(0, TripTileMID);
					}

					// Spawn deferred decals to project pattern onto walls/terrain
					if (DecalMaterial)
					{
						TripDecalMID = UMaterialInstanceDynamic::Create(DecalMaterial, TripHallucinationActor);
						TripDecalMID->SetVectorParameterValue(TEXT("BaseColor"),
							FLinearColor(0.02f, 0.02f, 0.03f));
						TripDecalMID->SetScalarParameterValue(TEXT("Opacity"), 0.85f);
						TripDecalMID->SetVectorParameterValue(TEXT("EmissiveColor"),
							FLinearColor(0.f, 0.f, 0.f));

						// Decal ring layout — project dark shapes onto all surfaces
						struct FDecalRing { int32 Count; float Radius; float Size; };
						static const FDecalRing DRings[] = {
							{  1,    0.f, 200.f },  // center
							{  6,  300.f, 150.f },
							{ 12,  800.f, 180.f },
							{ 12, 1500.f, 220.f },
							{ 16, 2200.f, 250.f },
							{ 18, 3000.f, 280.f },
							{ 24, 4000.f, 300.f },
						};

						for (const auto& DR : DRings)
						{
							const float AngleStep = (DR.Count > 1) ? (2.f * UE_PI / DR.Count) : 0.f;
							for (int32 di = 0; di < DR.Count; ++di)
							{
								const float DA = di * AngleStep;
								UDecalComponent* Decal = NewObject<UDecalComponent>(TripHallucinationActor);
								Decal->SetDecalMaterial(TripDecalMID);
								Decal->DecalSize = FVector(500.f, DR.Size, DR.Size);
								Decal->SetRelativeLocation(FVector(
									DR.Radius * FMath::Cos(DA),
									DR.Radius * FMath::Sin(DA), 250.f));
								Decal->SetRelativeRotation(FRotator(-90.f, FMath::RadiansToDegrees(DA), 0.f));
								Decal->RegisterComponent();
								TripHallucinationActor->AddInstanceComponent(Decal);
								TripDecals.Add(Decal);
							}
						}
					}

					// Spawn subtle underglow point lights
					{
						struct FGlowRing { int32 Count; float Radius; float Intensity; float Atten; };
						static const FGlowRing GRings[] = {
							{  1,    0.f, 1500.f, 500.f },
							{  6,  600.f, 1000.f, 400.f },
							{ 12, 1400.f,  800.f, 350.f },
							{ 12, 2400.f,  600.f, 350.f },
							{  8, 3500.f,  500.f, 300.f },
						};

						for (const auto& GR : GRings)
						{
							const float AngleStep = (GR.Count > 1) ? (2.f * UE_PI / GR.Count) : 0.f;
							for (int32 gi = 0; gi < GR.Count; ++gi)
							{
								const float GA = gi * AngleStep;
								UPointLightComponent* Glow = NewObject<UPointLightComponent>(
									TripHallucinationActor);
								Glow->SetRelativeLocation(FVector(
									GR.Radius * FMath::Cos(GA),
									GR.Radius * FMath::Sin(GA), 10.f));
								Glow->SetIntensity(GR.Intensity);
								Glow->SetAttenuationRadius(GR.Atten);
								Glow->SetLightColor(FColor(207, 192, 255));
								Glow->CastShadows = false;
								Glow->RegisterComponent();
								TripHallucinationActor->AddInstanceComponent(Glow);
								TripGlowLights.Add(Glow);
							}
						}
					}
				}

				// Animate fog + sun + ground tiles
				if (bTripHallucinationsSpawned && TripHallucinationActor)
				{
					if (SunLightComp)
					{
						FLinearColor SunTint = SamplePal(GT * 0.04f);
						SunLightComp->SetLightColor(FMath::Lerp(
							FLinearColor::White, SunTint, Intensity));
					}

					if (FogComp)
					{
						FLinearColor FogTint = SamplePal(GT * 0.03f + 0.5f);
						FogComp->SetFogInscatteringColor(FMath::Lerp(
							FLinearColor(0.45f, 0.55f, 0.65f), FogTint, Intensity));
					}

					// Animate mirror triangle floor — counter-rotating layers
					if (TripTileHISM)
					{
						const FVector FloorPos(PlayerPos.X, PlayerPos.Y, 0.f);
						TripTileHISM->SetWorldLocation(FloorPos);
						TripTileHISM->SetWorldRotation(FRotator(0.f, GT * 8.f, 0.f));
						if (TripBarHISM)
						{
							TripBarHISM->SetWorldLocation(FloorPos);
							TripBarHISM->SetWorldRotation(FRotator(0.f, GT * -5.f, 0.f));
						}

						if (TripTileMID)
						{
							FLinearColor TriCol = SamplePal(GT * 0.1f + 0.3f);
							const float TriAlpha = FMath::Max(0.5f, Intensity);
							TripTileMID->SetVectorParameterValue(TEXT("BaseColor"),
								FLinearColor(0.05f, 0.05f, 0.08f));
							TripTileMID->SetVectorParameterValue(TEXT("EmissiveColor"),
								TriCol * 1.5f * TriAlpha);
						}
					}

					// Move decals with player + animate emissive
					if (TripDecalMID)
					{
						TripHallucinationActor->SetActorLocation(
							FVector(PlayerPos.X, PlayerPos.Y, 0.f));
						FLinearColor DecalCol = SamplePal(GT * 0.08f + 0.7f);
						const float DecAlpha = FMath::Max(0.5f, Intensity);
						TripDecalMID->SetVectorParameterValue(TEXT("EmissiveColor"),
							DecalCol * 0.4f * DecAlpha);
					}

					// Animate underglow light colors — each light offset in palette
					for (int32 li = 0; li < TripGlowLights.Num(); ++li)
					{
						if (TripGlowLights[li])
						{
							const float Phase = GT * 0.06f + li * 0.15f;
							FLinearColor GlowCol = SamplePal(Phase);
							TripGlowLights[li]->SetLightColor(
								(GlowCol * 0.7f + FLinearColor(0.15f, 0.15f, 0.15f)).ToFColor(true));
						}
					}

					// Animate Neon Circuitry sky dome — static params, no audio reactivity
					if (TripSkyMID)
					{
						TripSkyMID->SetScalarParameterValue(TEXT("Bass"), 0.5f * Intensity);
						TripSkyMID->SetScalarParameterValue(TEXT("High"), 0.3f * Intensity);
						TripSkyMID->SetScalarParameterValue(TEXT("Level"), 0.5f);
						TripSkyMID->SetScalarParameterValue(TEXT("Mid"), 0.35f * Intensity);
						TripSkyMID->SetScalarParameterValue(TEXT("SceneIndex"), 0.f);

						if (TripSkyDome)
							TripSkyDome->SetWorldLocation(PlayerPos);
					}

					// Animate all trip water — pattern moves via Time node in shader,
					// params pulse gently for living feel
					{
						const float Pulse = 0.5f + 0.15f * FMath::Sin(GT * 0.7f);
						const float HVal = (0.3f + 0.1f * FMath::Sin(GT * 1.1f)) * Intensity;
						const float LVal = 0.5f + 0.1f * FMath::Sin(GT * 0.4f);
						const float MVal = (0.35f + 0.1f * FMath::Cos(GT * 0.9f)) * Intensity;

						if (RiverTripMID)
						{
							RiverTripMID->SetScalarParameterValue(TEXT("Bass"), Pulse * Intensity);
							RiverTripMID->SetScalarParameterValue(TEXT("High"), HVal);
							RiverTripMID->SetScalarParameterValue(TEXT("Level"), LVal);
							RiverTripMID->SetScalarParameterValue(TEXT("Mid"), MVal);
						}
						for (int32 Wi = 0; Wi < WaterMIDs.Num(); ++Wi)
						{
							UMaterialInstanceDynamic* WM = WaterMIDs[Wi];
							if (!WM) continue;
							WM->SetScalarParameterValue(TEXT("Bass"), Pulse * Intensity);
							WM->SetScalarParameterValue(TEXT("High"), HVal);
							WM->SetScalarParameterValue(TEXT("Level"), LVal);
							WM->SetScalarParameterValue(TEXT("Mid"), MVal);
						}
					}

					// === WORLD MORPHING — Full mandala palette colors + intensity ===

					// Ground — pure palette color with strong emissive glow
					if (GroundSlabMID)
					{
						FLinearColor GroundCol = SamplePal(GT * 0.05f + 0.3f);
						GroundSlabMID->SetVectorParameterValue(TEXT("BaseColor"),
							FMath::Lerp(FLinearColor(0.12f, 0.18f, 0.08f), GroundCol, Intensity));
						GroundSlabMID->SetVectorParameterValue(TEXT("EmissiveColor"),
							GroundCol * 2.f * Intensity);
					}

					// Roads — pure palette color with strong emissive glow
					for (int32 i = 0; i < RoadMaterials.Num(); ++i)
					{
						if (!RoadMaterials[i]) continue;
						FLinearColor RoadCol = SamplePal(GT * 0.04f + i * 0.07f);
						RoadMaterials[i]->SetVectorParameterValue(TEXT("BaseColor"),
							FMath::Lerp(FLinearColor(0.04f, 0.04f, 0.05f), RoadCol, Intensity));
						RoadMaterials[i]->SetVectorParameterValue(TEXT("EmissiveColor"),
							RoadCol * 3.f * Intensity);
					}

					// Windows — vivid palette emissive matching mandala brightness
					for (int32 i = 0; i < NightWindowMaterials.Num(); ++i)
					{
						if (!NightWindowMaterials[i]) continue;
						FLinearColor WinCol = SamplePal(GT * 0.06f + i * 0.05f);
						NightWindowMaterials[i]->SetVectorParameterValue(TEXT("EmissiveColor"),
							WinCol * 8.f * Intensity);
					}

					// Fog — keep normal (no trip color change)
					// Sky light — keep normal (no trip color change)

				}
			}

			// === 1. COLOR — subtle, gentle shifts ===
			const float SatBoost = 1.f + 0.20f * Overdrive;
			PP.bOverride_ColorSaturation = true;
			PP.ColorSaturation = FVector4(SatBoost, SatBoost, SatBoost, 1.0f);

			// Gentle hue drift
			const float WarmCool = FMath::Sin(GT * 0.15f);
			const float ColorAmp = 0.06f * Intensity;
			PP.bOverride_ColorGain = true;
			PP.ColorGain.X = 1.f + ColorAmp * (0.3f * WarmCool);
			PP.ColorGain.Y = 1.f + ColorAmp * (0.2f * FMath::Sin(GT * 0.23f + 1.5f));
			PP.ColorGain.Z = 1.f + ColorAmp * (-0.2f * WarmCool);

			// Color contrast boost — hyper-vivid, synthetic look (DMT "plastic" effect)
			PP.bOverride_ColorContrast = true;
			PP.ColorContrast = FVector4(1.f + 0.25f * Intensity, 1.f + 0.25f * Intensity,
				1.f + 0.25f * Intensity, 1.f);

			// === 2. BLOOM — self-luminous glow, everything radiates light (DMT) ===
			const float BloomBreath = 0.20f * FMath::Sin(GT * 1.885f);
			PP.BloomIntensity += (2.5f + BloomBreath) * Intensity;
			PP.BloomThreshold = FMath::Max(PP.BloomThreshold - 0.8f * Intensity, 0.f);
			PP.bOverride_BloomSizeScale = true;
			PP.BloomSizeScale = 1.f + 3.f * Intensity;

			// === 5. DEPTH OF FIELD — heavy focus swimming ===
			const float FocalDrift = 3000.f * Intensity * FixMul * (
				0.50f * FMath::Sin(GT * 0.131f) +
				0.30f * FMath::Sin(GT * 0.217f + 1.4f) +
				0.20f * FMath::Sin(GT * 0.073f + 3.8f));
			PP.DepthOfFieldFstop = FMath::Lerp(PP.DepthOfFieldFstop, 1.8f, Intensity * 0.5f);
			PP.DepthOfFieldFocalDistance = 3000.f + FocalDrift;

			// === 6. FILM GRAIN — disabled ===

			// === 7. TRACERS — moderate (won't desaturate) ===
			PP.bOverride_MotionBlurAmount = true;
			PP.MotionBlurAmount = 2.5f * Intensity;
			PP.bOverride_MotionBlurMax = true;
			PP.MotionBlurMax = 10.f * Intensity;
			PP.bOverride_MotionBlurTargetFPS = true;
			PP.MotionBlurTargetFPS = FMath::Lerp(60, 24, Intensity);
			{
				static IConsoleVariable* TAAWeightCVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.TemporalAACurrentFrameWeight"));
				if (TAAWeightCVar)
					TAAWeightCVar->Set(FMath::Lerp(0.1f, 0.05f, Intensity));
			}

			// === 8. AUTO EXPOSURE — subtle pupil dilation ===
			const float ExpShift = 0.3f * Intensity;
			PP.AutoExposureMinBrightness *= (1.f + ExpShift * FMath::Sin(GT * 0.23f));
			PP.AutoExposureMaxBrightness *= (1.f + ExpShift * FMath::Sin(GT * 0.14f + 1.f));

			// === 9. CAMERA DISTORTION ===
			if (UWorld* W = GetWorld())
			{
				if (APlayerController* PC = W->GetFirstPlayerController())
				{
					if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
					{
						if (TT < DeltaTime * 2.f)
							TripBaseFOV = Char->FollowCamera->FieldOfView;

						const float I = Overdrive * FixMul;

						// Breathing: FOV + boom arm — heavy morphing
						const float BreathPhase = GT * 1.257f;
						const float FOVBreath =
							FMath::Sin(BreathPhase) * 10.f +
							FMath::Sin(GT * 0.628f + 2.1f) * 6.f +
							FMath::Sin(GT * 1.885f + 0.7f) * 4.f;
						const float BoomBreathVal =
							FMath::Sin(BreathPhase + 1.571f) * 70.f +
							FMath::Sin(GT * 0.628f + 2.1f + 1.571f) * 45.f +
							FMath::Sin(GT * 1.885f + 0.7f + 1.571f) * 25.f;

						Char->FollowCamera->SetFieldOfView(TripBaseFOV + FOVBreath * I);
						Char->CameraBoom->TargetArmLength = 450.f + BoomBreathVal * I;

						// Camera roll — heavy swirl
						const float SwirlSpeed = 0.21f;
						const float SwirlAngle = GT * SwirlSpeed * 57.296f;
						const float SwirlDeg = FMath::Fmod(SwirlAngle, 360.f);
						const float SwirlAmp = 5.f + 3.f * FMath::Sin(GT * 0.311f);
						const float SwirlVal = SwirlAmp * FMath::Sin(FMath::DegreesToRadians(SwirlDeg));
						const float TiltVal =
							FMath::Sin(GT * 0.197f + 2.8f) * 2.f +
							FMath::Sin(GT * 0.523f + 5.1f) * 1.2f;
						const float Roll = SwirlVal + TiltVal;
						FRotator CurrentRot = Char->GetActorRotation();
						Char->SetActorRotation(FRotator(CurrentRot.Pitch, CurrentRot.Yaw, Roll * I));

						// Pitch/yaw wobble — stronger drift
						const float YawWobble =
							FMath::Sin(GT * 3.17f) * 0.18f +
							FMath::Sin(GT * 5.23f + 1.3f) * 0.12f +
							FMath::Sin(GT * 7.91f + 4.1f) * 0.07f +
							FMath::Sin(GT * 1.41f + 2.7f) * 0.25f;
						const float PitchWobble =
							FMath::Sin(GT * 2.83f + 0.5f) * 0.18f +
							FMath::Sin(GT * 4.71f + 3.6f) * 0.12f +
							FMath::Sin(GT * 6.67f + 1.9f) * 0.07f +
							FMath::Sin(GT * 1.13f + 5.2f) * 0.25f;
						PC->AddYawInput(YawWobble * I);
						PC->AddPitchInput(PitchWobble * I);

						// Socket offset drift — wider orbit
						const float OrbitPhase = GT * SwirlSpeed;
						const float OrbitRadius = 50.f;
						const float CircY = FMath::Cos(OrbitPhase) * OrbitRadius;
						const float CircZ = FMath::Sin(OrbitPhase) * OrbitRadius;
						const float DriftY =
							FMath::Sin(GT * 0.341f + 0.8f) * 35.f +
							FMath::Sin(GT * 0.197f + 3.2f) * 22.f;
						const float DriftZ =
							FMath::Sin(GT * 0.271f + 2.5f) * 28.f +
							FMath::Sin(GT * 0.157f + 5.7f) * 18.f;
						Char->CameraBoom->SocketOffset = FVector(0.f,
							75.f + (CircY + DriftY) * I,
							60.f + (CircZ + DriftZ) * I);
					}
				}
			}
		}
	}

	// ---- Gold Coin Spin / Pickup / Respawn ----
	{
		// Get pawn location (works for both character and vehicle)
		FVector PawnLoc = FVector::ZeroVector;
		bool bHasPawn = false;
		if (World)
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					PawnLoc = Pawn->GetActorLocation();
					bHasPawn = true;
				}
			}
		}

		for (int32 CI = 0; CI < NumGoldCoins; ++CI)
		{
			FGoldCoinData& GC = GoldCoins[CI];
			if (!GC.Actor) continue;

			// --- Spin animation (always, even during pickup anim) ---
			GC.SpinAngle += DeltaTime * 90.f;
			if (GC.SpinAngle >= 360.f) GC.SpinAngle -= 360.f;

			// --- Pickup animation in progress ---
			if (GC.PickupAnimTimer >= 0.f)
			{
				GC.PickupAnimTimer += DeltaTime;
				const float AnimDuration = 0.4f;
				const float Alpha = FMath::Clamp(GC.PickupAnimTimer / AnimDuration, 0.f, 1.f);
				const float Scale = FMath::Lerp(1.f, 0.f, Alpha);

				GC.Actor->SetActorScale3D(FVector(7.f * Scale, 7.f * Scale, 0.7f * Scale));
				GC.Actor->SetActorLocation(FVector(GC.CurrentPos.X, GC.CurrentPos.Y,
					GC.CurrentPos.Z + Alpha * 100.f));
				GC.Actor->SetActorRotation(FRotator(90.f, GC.SpinAngle, 0.f));

				if (GC.PickupAnimTimer >= AnimDuration)
				{
					GC.Actor->SetActorHiddenInGame(true);
					GC.PickupAnimTimer = -1.f;
					GC.RespawnTimer = 10.f;
				}
				continue;
			}

			// --- Respawn timer ---
			if (GC.RespawnTimer >= 0.f)
			{
				GC.RespawnTimer -= DeltaTime;
				if (GC.RespawnTimer <= 0.f)
				{
					// Respawn at mirrored position
					const float MX = -GC.OriginalSpawnPos.X;
					const float MY = -GC.OriginalSpawnPos.Y;
					const bool bSkyCoin = GC.OriginalSpawnPos.Z > 5000.f;
					const float MZ = bSkyCoin ? GC.OriginalSpawnPos.Z : GetTerrainHeight(MX, MY) + 600.f;
					GC.CurrentPos = FVector(MX, MY, MZ);
					GC.OriginalSpawnPos = GC.CurrentPos; // update for next mirror
					GC.Actor->SetActorLocation(GC.CurrentPos);
					GC.Actor->SetActorScale3D(FVector(7.f, 7.f, 0.7f));
					GC.Actor->SetActorHiddenInGame(false);
					GC.RespawnTimer = -1.f;
				}
				continue;
			}

			// --- Active coin: rotate and check pickup ---
			GC.Actor->SetActorRotation(FRotator(90.f, GC.SpinAngle, 0.f));

			if (bHasPawn && FVector::Dist2D(PawnLoc, GC.CurrentPos) < 800.f)
			{
				// Start pickup animation
				GC.PickupAnimTimer = 0.f;

				// Award coins
				if (World)
				{
					if (UPHCGameInstance* GI = Cast<UPHCGameInstance>(World->GetGameInstance()))
					{
						GI->LocalPlayerProfile.Coins += 20;
						GI->SaveCoinsToBackend();

						// Award XP for coin pickup
						if (APlayerController* PC0 = World->GetFirstPlayerController())
						{
							if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC0->GetPawn()))
							{
								Char->AddXP(10);
							}
						}

						// Update HUD
						if (APlayerController* PC = World->GetFirstPlayerController())
						{
							APHCPlayerController* PHC_PC = Cast<APHCPlayerController>(PC);
							if (PHC_PC && PHC_PC->GetActiveWidget())
							{
								if (UPHCCityHUD* CityHUD = Cast<UPHCCityHUD>(PHC_PC->GetActiveWidget()))
								{
									CityHUD->UpdateCurrencyDisplay(GI->LocalPlayerProfile.Coins, GI->LocalPlayerProfile.Gems);
								}
							}
						}
					}
				}
			}
		}
	}

	// ---- Collectible Mushroom Bob / Pickup / Respawn / Trip ----
	{
		FVector PawnLoc2 = FVector::ZeroVector;
		bool bHasPawn2 = false;
		if (World)
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					PawnLoc2 = Pawn->GetActorLocation();
					bHasPawn2 = true;
				}
			}
		}

		const float GT = World ? World->GetTimeSeconds() : 0.f;

		for (int32 MI = 0; MI < CollectibleMushrooms.Num(); ++MI)
		{
			FMushroomData& MD = CollectibleMushrooms[MI];
			if (!MD.Actor) continue;

			// --- Pickup animation in progress ---
			if (MD.PickupAnimTimer >= 0.f)
			{
				MD.PickupAnimTimer += DeltaTime;
				const float AnimDuration = 0.5f;
				const float Alpha = FMath::Clamp(MD.PickupAnimTimer / AnimDuration, 0.f, 1.f);
				const float Scale = FMath::Lerp(1.f, 0.f, Alpha);

				MD.Actor->SetActorScale3D(FVector(Scale));
				MD.Actor->SetActorLocation(FVector(MD.CurrentPos.X, MD.CurrentPos.Y,
					MD.CurrentPos.Z + Alpha * 80.f));

				if (MD.PickupAnimTimer >= AnimDuration)
				{
					MD.Actor->SetActorHiddenInGame(true);
					if (MD.GlowLight) MD.GlowLight->SetVisibility(false);
					MD.PickupAnimTimer = -1.f;
					MD.RespawnTimer = 60.f; // respawn after 60s
				}
				continue;
			}

			// --- Respawn timer ---
			if (MD.RespawnTimer >= 0.f)
			{
				MD.RespawnTimer -= DeltaTime;
				if (MD.RespawnTimer <= 0.f)
				{
					MD.Actor->SetActorLocation(MD.OriginalSpawnPos);
					MD.CurrentPos = MD.OriginalSpawnPos;
					MD.Actor->SetActorScale3D(FVector(1.f));
					MD.Actor->SetActorHiddenInGame(false);
					if (MD.GlowLight) MD.GlowLight->SetVisibility(true);
					MD.RespawnTimer = -1.f;
				}
				continue;
			}

			// --- Active: bob animation + light pulse ---
			const float Bob = FMath::Sin(GT * 2.f + MD.BobPhase) * 8.f;
			MD.Actor->SetActorLocation(FVector(MD.CurrentPos.X, MD.CurrentPos.Y,
				MD.CurrentPos.Z + Bob));

			// Pulse the glow light
			if (MD.GlowLight)
			{
				const float Pulse = 0.7f + 0.3f * FMath::Sin(GT * 3.f + MD.BobPhase);
				MD.GlowLight->SetIntensity(8000.f * Pulse);
			}

			// --- Proximity pickup ---
			if (bHasPawn2 && FVector::Dist(PawnLoc2, MD.CurrentPos) < 350.f)
			{
				MD.PickupAnimTimer = 0.f;

				if (MushroomTripTimer >= 0.f)
				{
					// Re-dose — extend trip by 20s and increase intensity (doubled)
					TripDoseCount++;
					MushroomTripDuration += 20.f;
					TripIntensityMultiplier = FMath::Min(TripIntensityMultiplier + 1.4f, 6.f);
				}
				else
				{
					// First dose — start the trip (60 seconds base, doubled intensity)
					MushroomTripTimer = 0.f;
					MushroomTripDuration = 60.f;
					TripDoseCount = 1;
					TripIntensityMultiplier = 2.f;

					// Mark character as tripping (replicated — hides from other players/NPCs)
					if (UWorld* TW = GetWorld())
					{
						if (APlayerController* TPC = TW->GetFirstPlayerController())
						{
							if (APHCCityCharacter* TChar = Cast<APHCCityCharacter>(TPC->GetPawn()))
								TChar->bIsTripping = true;
						}
					}
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Procedural Normal & Roughness Textures
// ---------------------------------------------------------------------------

UTexture2D* UPHCCitySubsystem::CreateProceduralTexture(int32 W, int32 H,
	const TFunction<FColor(int32,int32)>& PixelFunc, bool bNormalMap)
{
	UTexture2D* Tex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
	if (!Tex) return nullptr;

	Tex->SRGB = !bNormalMap;
	if (bNormalMap)
	{
		Tex->CompressionSettings = TC_Normalmap;
	}
	Tex->Filter = TF_Bilinear;
	Tex->AddressX = TA_Wrap;
	Tex->AddressY = TA_Wrap;

	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
	FColor* Pixels = static_cast<FColor*>(Data);

	for (int32 Y = 0; Y < H; ++Y)
	{
		for (int32 X = 0; X < W; ++X)
		{
			Pixels[Y * W + X] = PixelFunc(X, Y);
		}
	}

	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	return Tex;
}

void UPHCCitySubsystem::GenerateProceduralTextures()
{
	// Simple hash for deterministic noise (fast, no external deps)
	auto Hash = [](int32 X, int32 Y, int32 Seed) -> float
	{
		int32 N = X * 374761393 + Y * 668265263 + Seed * 1274126177;
		N = (N ^ (N >> 13)) * 1274126177;
		N = N ^ (N >> 16);
		return (float)(N & 0x7FFFFFFF) / (float)0x7FFFFFFF; // 0..1
	};

	// Pack a tangent-space normal (Nx,Ny,Nz) into a color
	auto PackNormal = [](float Nx, float Ny, float Nz) -> FColor
	{
		return FColor(
			FMath::Clamp((int32)((Nx * 0.5f + 0.5f) * 255.f), 0, 255),
			FMath::Clamp((int32)((Ny * 0.5f + 0.5f) * 255.f), 0, 255),
			FMath::Clamp((int32)((Nz * 0.5f + 0.5f) * 255.f), 0, 255),
			255
		);
	};

	constexpr int32 TexSize = 256;

	// ---- Grass Normal Map ----
	GrassNormalMap = CreateProceduralTexture(TexSize, TexSize, [&](int32 X, int32 Y) -> FColor
	{
		// Per-4x4 cluster blade tilt
		int32 CX = X / 4, CY = Y / 4;
		float Tilt = Hash(CX, CY, 1) * 0.4f - 0.2f;
		float TiltY = Hash(CX, CY, 2) * 0.4f - 0.2f;
		// Fine noise per pixel
		float Fn = Hash(X, Y, 3) * 0.1f - 0.05f;
		float Nx = Tilt + Fn;
		float Ny = TiltY + Fn;
		float Nz = FMath::Sqrt(FMath::Max(0.f, 1.f - Nx*Nx - Ny*Ny));
		return PackNormal(Nx, Ny, Nz);
	}, true);

	// ---- Brick Normal Map ----
	BrickNormalMap = CreateProceduralTexture(TexSize, TexSize, [&](int32 X, int32 Y) -> FColor
	{
		// 64x32 brick pattern with 3px mortar grooves
		int32 Row = Y / 32;
		int32 OffsetX = (Row & 1) ? 32 : 0; // stagger
		int32 BX = (X + OffsetX) % 64;
		int32 BY = Y % 32;
		bool bMortar = (BX < 3 || BY < 3);
		if (bMortar)
		{
			// Recessed mortar
			return PackNormal(0.f, 0.f, 1.f);
		}
		// Brick face — subtle raised bumps
		float Bump = Hash(X, Y, 10) * 0.08f - 0.04f;
		float Nx = Bump + ((BX == 3) ? 0.3f : (BX == 63) ? -0.3f : 0.f);
		float Ny = Bump + ((BY == 3) ? 0.3f : (BY == 31) ? -0.3f : 0.f);
		float Nz = FMath::Sqrt(FMath::Max(0.f, 1.f - Nx*Nx - Ny*Ny));
		return PackNormal(Nx, Ny, Nz);
	}, true);

	// ---- Concrete Normal Map ----
	ConcreteNormalMap = CreateProceduralTexture(TexSize, TexSize, [&](int32 X, int32 Y) -> FColor
	{
		// Subtle surface variation + occasional crack lines
		float Base = Hash(X, Y, 20) * 0.06f - 0.03f;
		// Large-scale undulation
		float LX = FMath::Sin((float)X * 0.05f) * 0.02f;
		float LY = FMath::Cos((float)Y * 0.05f) * 0.02f;
		float Nx = Base + LX;
		float Ny = Base + LY;
		float Nz = FMath::Sqrt(FMath::Max(0.f, 1.f - Nx*Nx - Ny*Ny));
		return PackNormal(Nx, Ny, Nz);
	}, true);

	// ---- Asphalt Normal Map ----
	AsphaltNormalMap = CreateProceduralTexture(TexSize, TexSize, [&](int32 X, int32 Y) -> FColor
	{
		// Multi-frequency hash noise (coarse + fine aggregate)
		float Coarse = Hash(X / 3, Y / 3, 30) * 0.15f - 0.075f;
		float Fine = Hash(X, Y, 31) * 0.08f - 0.04f;
		float Nx = Coarse + Fine;
		float Ny = Hash(X / 3, Y / 3, 32) * 0.15f - 0.075f + Hash(X, Y, 33) * 0.08f - 0.04f;
		float Nz = FMath::Sqrt(FMath::Max(0.f, 1.f - Nx*Nx - Ny*Ny));
		return PackNormal(Nx, Ny, Nz);
	}, true);

	// ---- Sand Normal Map ----
	SandNormalMap = CreateProceduralTexture(TexSize, TexSize, [&](int32 X, int32 Y) -> FColor
	{
		// Per-pixel grain bumps + wind ripple sine pattern (~40px wavelength)
		float Grain = Hash(X, Y, 40) * 0.12f - 0.06f;
		float Ripple = FMath::Sin((float)X * 0.157f) * 0.08f; // ~40px wavelength
		float Nx = Grain + FMath::Cos((float)X * 0.157f) * 0.08f;
		float Ny = Grain;
		float Nz = FMath::Sqrt(FMath::Max(0.f, 1.f - Nx*Nx - Ny*Ny));
		(void)Ripple; // used via derivative in Nx
		return PackNormal(Nx, Ny, Nz);
	}, true);

	// ---- Grass Roughness Map ----
	GrassRoughnessMap = CreateProceduralTexture(TexSize, TexSize, [&](int32 X, int32 Y) -> FColor
	{
		// 0.80 - 0.95
		float R = 0.80f + Hash(X, Y, 50) * 0.15f;
		uint8 V = FMath::Clamp((int32)(R * 255.f), 0, 255);
		return FColor(V, V, V, 255);
	}, false);

	// ---- Brick Roughness Map ----
	BrickRoughnessMap = CreateProceduralTexture(TexSize, TexSize, [&](int32 X, int32 Y) -> FColor
	{
		int32 Row = Y / 32;
		int32 OffsetX = (Row & 1) ? 32 : 0;
		int32 BX = (X + OffsetX) % 64;
		int32 BY = Y % 32;
		bool bMortar = (BX < 3 || BY < 3);
		float R = bMortar ? (0.70f + Hash(X, Y, 60) * 0.10f) : (0.85f + Hash(X, Y, 61) * 0.10f);
		uint8 V = FMath::Clamp((int32)(R * 255.f), 0, 255);
		return FColor(V, V, V, 255);
	}, false);

	// ---- Concrete Roughness Map ----
	ConcreteRoughnessMap = CreateProceduralTexture(TexSize, TexSize, [&](int32 X, int32 Y) -> FColor
	{
		float R = 0.70f + Hash(X, Y, 70) * 0.20f;
		uint8 V = FMath::Clamp((int32)(R * 255.f), 0, 255);
		return FColor(V, V, V, 255);
	}, false);

	// ---- Asphalt Roughness Map ----
	AsphaltRoughnessMap = CreateProceduralTexture(TexSize, TexSize, [&](int32 X, int32 Y) -> FColor
	{
		float R = 0.75f + Hash(X / 2, Y / 2, 80) * 0.15f + Hash(X, Y, 81) * 0.08f;
		R = FMath::Clamp(R, 0.f, 1.f);
		uint8 V = FMath::Clamp((int32)(R * 255.f), 0, 255);
		return FColor(V, V, V, 255);
	}, false);

	// ---- Sand Roughness Map ----
	SandRoughnessMap = CreateProceduralTexture(TexSize, TexSize, [&](int32 X, int32 Y) -> FColor
	{
		float R = 0.82f + Hash(X, Y, 90) * 0.12f;
		uint8 V = FMath::Clamp((int32)(R * 255.f), 0, 255);
		return FColor(V, V, V, 255);
	}, false);

	// ---- Flat Normal Map (default, points straight up) ----
	FlatNormalMap = CreateProceduralTexture(4, 4, [&](int32 X, int32 Y) -> FColor
	{
		return FColor(128, 128, 255, 255); // (0,0,1) in tangent space
	}, true);

	// ---- White Roughness Map (default, full roughness scalar passthrough) ----
	WhiteRoughnessMap = CreateProceduralTexture(4, 4, [&](int32 X, int32 Y) -> FColor
	{
		return FColor(255, 255, 255, 255);
	}, false);

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHC: Generated %d procedural textures"), 12);
}

// ---------------------------------------------------------------------------
// Perlin Noise 2D — classic gradient noise for terrain generation
// ---------------------------------------------------------------------------

// Static gradient table for Perlin noise
static const FVector2D PerlinGradients[] = {
	{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1},
	{0.7071f,0.7071f},{-0.7071f,0.7071f},{0.7071f,-0.7071f},{-0.7071f,-0.7071f},
	{1,0},{0,1},{-1,0},{0,-1}
};
static const int32 PerlinPerm[] = {
	151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,
	142,8,99,37,240,21,10,23,190,6,148,247,120,234,75,0,26,197,62,94,252,219,
	203,117,35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168,68,175,
	74,165,71,134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,
	105,92,41,55,46,245,40,244,102,143,54,65,25,63,161,1,216,80,73,209,76,132,
	187,208,89,18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186,3,
	64,52,217,226,250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,
	47,16,58,17,182,189,28,42,223,183,170,213,119,248,152,2,44,154,163,70,221,
	153,101,155,167,43,172,9,129,22,39,253,19,98,108,110,79,113,224,232,178,185,
	112,104,218,246,97,228,251,34,242,193,238,210,144,12,191,179,162,241,81,51,
	145,235,249,14,239,107,49,192,214,31,181,199,106,157,184,84,204,176,115,121,
	50,45,127,4,150,254,138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,
	66,215,61,156,180
};

float UPHCCitySubsystem::PerlinNoise2D(float X, float Y)
{
	auto Fade = [](float T) { return T * T * T * (T * (T * 6.f - 15.f) + 10.f); };
	auto Perm = [](int32 I) { return PerlinPerm[I & 255]; };

	int32 Xi = FMath::FloorToInt(X) & 255;
	int32 Yi = FMath::FloorToInt(Y) & 255;
	float Xf = X - FMath::FloorToFloat(X);
	float Yf = Y - FMath::FloorToFloat(Y);

	float U = Fade(Xf);
	float V = Fade(Yf);

	int32 AA = Perm(Perm(Xi) + Yi) & 15;
	int32 AB = Perm(Perm(Xi) + Yi + 1) & 15;
	int32 BA = Perm(Perm(Xi + 1) + Yi) & 15;
	int32 BB = Perm(Perm(Xi + 1) + Yi + 1) & 15;

	float D00 = FVector2D::DotProduct(PerlinGradients[AA], FVector2D(Xf, Yf));
	float D10 = FVector2D::DotProduct(PerlinGradients[BA], FVector2D(Xf - 1.f, Yf));
	float D01 = FVector2D::DotProduct(PerlinGradients[AB], FVector2D(Xf, Yf - 1.f));
	float D11 = FVector2D::DotProduct(PerlinGradients[BB], FVector2D(Xf - 1.f, Yf - 1.f));

	float X1 = FMath::Lerp(D00, D10, U);
	float X2 = FMath::Lerp(D01, D11, U);
	return FMath::Lerp(X1, X2, V);
}

float UPHCCitySubsystem::GetTerrainHeight(float X, float Y) const
{
	// City bounds: flat at Z=0
	const float CityEdge = 36000.f;
	const float TransitionWidth = 8000.f;

	float AbsX = FMath::Abs(X);
	float AbsY = FMath::Abs(Y);
	float MaxDist = FMath::Max(AbsX, AbsY);

	// Allow hills to extend into the city edge near (32371, 12947)
	// where a visible gap exists between surrounding hills.
	const bool bInHillPatch = (X > 25000.f && X < 40000.f && Y > 5000.f && Y < 22000.f);

	if (MaxDist < CityEdge && !bInHillPatch)
		return 0.f;

	// Smooth transition factor: 0 at CityEdge, 1 at CityEdge+TransitionWidth
	float Alpha;
	if (bInHillPatch && MaxDist < CityEdge)
	{
		// Blend hills in gradually within the patch zone
		float PatchEdge = 28000.f;
		Alpha = FMath::Clamp((MaxDist - PatchEdge) / (CityEdge - PatchEdge), 0.f, 1.f);
		Alpha = Alpha * Alpha * (3.f - 2.f * Alpha);
	}
	else
	{
		Alpha = FMath::Clamp((MaxDist - CityEdge) / TransitionWidth, 0.f, 1.f);
		Alpha = Alpha * Alpha * (3.f - 2.f * Alpha); // smoothstep
	}

	// Multi-octave Perlin noise
	float N = 0.f;
	N += PerlinNoise2D(X * 0.00004f, Y * 0.00004f) * 6000.f;  // large hills
	N += PerlinNoise2D(X * 0.00015f, Y * 0.00015f) * 1600.f;    // medium bumps
	N += PerlinNoise2D(X * 0.0006f, Y * 0.0006f) * 320.f;       // small detail

	float Result = N * Alpha;

	// Double height for hill patch zone near (32371, 12947)
	if (bInHillPatch)
	{
		Result *= 2.f;
	}

	// Mountain range spanning the entire north coast
	// PERF-OPT: cheap AABB pre-check avoids sqrt when clearly outside zone
	{
		const float MtnCX = 0.f;
		const float MtnCY = 110000.f;
		const float MtnHalfX = 120000.f;
		const float MtnHalfY = 15000.f;
		const float MtnTransition = 10000.f;
		float MtnDistX = FMath::Max(0.f, FMath::Abs(X - MtnCX) - MtnHalfX);
		float MtnDistY = FMath::Max(0.f, FMath::Abs(Y - MtnCY) - MtnHalfY);
		if (MtnDistX < MtnTransition && MtnDistY < MtnTransition) // PERF-OPT: AABB pre-check
		{
			float MtnDist = FMath::Sqrt(MtnDistX * MtnDistX + MtnDistY * MtnDistY);
			if (MtnDist < MtnTransition)
			{
				// Lower-frequency noise for broader, wider mountains
				float MtnN = 0.f;
				MtnN += PerlinNoise2D(X * 0.00002f, Y * 0.00002f) * 6000.f;
				MtnN += PerlinNoise2D(X * 0.00008f, Y * 0.00008f) * 1600.f;
				MtnN += PerlinNoise2D(X * 0.0003f, Y * 0.0003f) * 320.f;
				float MtnResult = FMath::Max(MtnN * Alpha * 6.9f, 0.f);

				float MtnAlpha = MtnDist / MtnTransition;
				MtnAlpha = MtnAlpha * MtnAlpha * (3.f - 2.f * MtnAlpha);
				Result = FMath::Lerp(MtnResult, Result, MtnAlpha);
			}
		}
	}

	// Christmas area clearing: flatten terrain within the Christmas zone
	// so decorations, trees, and snowmen are not buried by mountain terrain.
	{
		const float XmasCX = 39000.f;
		const float XmasCY = 117000.f;
		const float XmasHalfX = 26000.f;  // spans 13000..65000
		const float XmasHalfY = 10000.f;  // spans 107000..127000
		const float XmasTransition = 5000.f;
		float XmasDistX = FMath::Max(0.f, FMath::Abs(X - XmasCX) - XmasHalfX);
		float XmasDistY = FMath::Max(0.f, FMath::Abs(Y - XmasCY) - XmasHalfY);
		float XmasDist = FMath::Sqrt(XmasDistX * XmasDistX + XmasDistY * XmasDistY);
		if (XmasDist < XmasTransition)
		{
			float XmasAlpha = XmasDist / XmasTransition;
			XmasAlpha = XmasAlpha * XmasAlpha * (3.f - 2.f * XmasAlpha);
			// Blend toward flat (0) inside the Christmas zone
			Result = FMath::Lerp(0.f, Result, XmasAlpha);
		}
	}

	// Snowy rolling hills behind Christmas area — gentle terrain matching
	// the hill character at (38345, 52577), using base Perlin noise (no mountain multiplier).
	{
		const float HillCX = 39000.f;
		const float HillCY = 133000.f;
		const float HillHalfX = 65000.f;   // X: -26000..104000 (full length behind Christmas)
		const float HillHalfY = 8000.f;    // Y: 125000..141000 (starts at Christmas edge)
		const float HillTransition = 5000.f;
		float HillDistX = FMath::Max(0.f, FMath::Abs(X - HillCX) - HillHalfX);
		float HillDistY = FMath::Max(0.f, FMath::Abs(Y - HillCY) - HillHalfY);
		if (HillDistX < HillTransition && HillDistY < HillTransition) // PERF-OPT: AABB pre-check
		{
		float HillDist = FMath::Sqrt(HillDistX * HillDistX + HillDistY * HillDistY);
		if (HillDist < HillTransition)
		{
			// Offset noise so hills have unique shapes (same frequencies as base terrain)
			const float OX = X + 300000.f;
			const float OY = Y + 300000.f;
			float HillN = 0.f;
			HillN += PerlinNoise2D(OX * 0.00004f, OY * 0.00004f) * 6000.f;
			HillN += PerlinNoise2D(OX * 0.00015f, OY * 0.00015f) * 1600.f;
			HillN += PerlinNoise2D(OX * 0.0006f,  OY * 0.0006f)  * 320.f;
			float HillResult = FMath::Max(HillN * 3.f, 0.f);

			float HillAlpha = HillDist / HillTransition;
			HillAlpha = HillAlpha * HillAlpha * (3.f - 2.f * HillAlpha);
			Result = FMath::Lerp(HillResult, Result, HillAlpha);
		}
		}
	}

	// Snowy rolling hills in FRONT of Christmas area — hides the area from
	// a distance so players discover it by walking over the hills.
	{
		const float FHillCX = 39000.f;
		const float FHillCY = 97000.f;
		const float FHillHalfX = 65000.f;  // X: -26000..104000 (full width)
		const float FHillHalfY = 8000.f;   // Y: 89000..105000
		const float FHillTransition = 5000.f;
		float FHDistX = FMath::Max(0.f, FMath::Abs(X - FHillCX) - FHillHalfX);
		float FHDistY = FMath::Max(0.f, FMath::Abs(Y - FHillCY) - FHillHalfY);
		if (FHDistX < FHillTransition && FHDistY < FHillTransition) // PERF-OPT: AABB pre-check
		{
		float FHDist = FMath::Sqrt(FHDistX * FHDistX + FHDistY * FHDistY);
		if (FHDist < FHillTransition)
		{
			const float FOX = X + 400000.f;
			const float FOY = Y + 400000.f;
			float FHillN = 0.f;
			FHillN += PerlinNoise2D(FOX * 0.00004f, FOY * 0.00004f) * 6000.f;
			FHillN += PerlinNoise2D(FOX * 0.00015f, FOY * 0.00015f) * 1600.f;
			FHillN += PerlinNoise2D(FOX * 0.0006f,  FOY * 0.0006f)  * 320.f;
			float FHillResult = FMath::Max(FHillN * 6.f, 0.f);

			float FHAlpha = FHDist / FHillTransition;
			FHAlpha = FHAlpha * FHAlpha * (3.f - 2.f * FHAlpha);
			Result = FMath::Lerp(FHillResult, Result, FHAlpha);
		}
		}
	}

	// Motocross park area: replace surrounding terrain with natural
	// rolling hills tuned for dirt bike riding (modest, realistic jumps).
	const float ParkCX = -120000.f;
	const float ParkCY = 20000.f;
	const float ParkHalfX = 55000.f;
	const float ParkHalfY = 55000.f;
	const float ParkTransition = 8000.f;
	float ParkDistX = FMath::Max(0.f, FMath::Abs(X - ParkCX) - ParkHalfX);
	float ParkDistY = FMath::Max(0.f, FMath::Abs(Y - ParkCY) - ParkHalfY);
	if (ParkDistX < ParkTransition && ParkDistY < ParkTransition) // PERF-OPT: AABB pre-check
	{
	float ParkDist = FMath::Sqrt(ParkDistX * ParkDistX + ParkDistY * ParkDistY);
	if (ParkDist < ParkTransition)
	{
		// Park-specific terrain: offset coords so noise is uncorrelated with surroundings
		const float PX = X + 500000.f;
		const float PY = Y + 500000.f;
		float ParkH = 0.f;
		ParkH += PerlinNoise2D(PX * 0.00008f, PY * 0.00008f) * 400.f;  // gentle rolling hills
		ParkH += PerlinNoise2D(PX * 0.0003f,  PY * 0.0003f)  * 150.f;  // medium undulation
		ParkH += PerlinNoise2D(PX * 0.001f,   PY * 0.001f)   * 40.f;   // surface texture
		ParkH = FMath::Max(ParkH, 0.f); // floor at Z=0, no valleys below ground

		float ParkAlpha = ParkDist / ParkTransition;
		ParkAlpha = ParkAlpha * ParkAlpha * (3.f - 2.f * ParkAlpha); // smoothstep
		// Blend: inside park = ParkH, outside = Result (surrounding terrain)
		Result = FMath::Lerp(ParkH, Result, ParkAlpha);
	}
	}

	// River channel: carve terrain to 0 along the river path
	// River center follows Y = -97000 + sin(X * 0.00004) * 2500
	{
		const float RiverCenterY = -97000.f + FMath::Sin(X * 0.00004f) * 2500.f;
		const float RiverHalfW = 2000.f;   // half of 4000 UU river width
		const float RiverBankW = 1500.f;   // smooth transition on banks
		float RiverDist = FMath::Abs(Y - RiverCenterY);
		if (RiverDist < RiverHalfW + RiverBankW)
		{
			float RiverAlpha;
			if (RiverDist < RiverHalfW)
				RiverAlpha = 0.f; // fully flat inside river
			else
			{
				RiverAlpha = (RiverDist - RiverHalfW) / RiverBankW;
				RiverAlpha = RiverAlpha * RiverAlpha * (3.f - 2.f * RiverAlpha); // smoothstep
			}
			Result = FMath::Lerp(0.f, Result, RiverAlpha);
		}
	}

	// Never return below ground — keeps character above surface everywhere.
	// Underground areas (airport tunnel) are handled separately in character code.
	return FMath::Max(Result, 0.f);
}

// ---------------------------------------------------------------------------
// Procedural Terrain Grid (replaces flat ground plane)
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnTerrainGrid()
{
	UWorld* World = GetWorld();
	if (!World) return;

	TerrainActor = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TerrainActor) return;
#if WITH_EDITOR
	TerrainActor->SetActorLabel(TEXT("ProceduralTerrain"));
#endif

	const float ChunkSize = 10000.f;
	const float HalfWorld = 150000.f; // how far out to generate terrain
	int32 ChunkIdx = 0;

	for (float CX = -HalfWorld; CX < HalfWorld; CX += ChunkSize)
	{
		for (float CY = -HalfWorld; CY < HalfWorld; CY += ChunkSize)
		{
			const float CenterDist = FMath::Sqrt(CX * CX + CY * CY);

			// LOD based on distance from center
			int32 GridRes;
			if (CenterDist < 25000.f)
				GridRes = 24;   // high detail near player
			else if (CenterDist < 50000.f)
				GridRes = 16;
			else if (CenterDist < 100000.f)
				GridRes = 8;
			else
				GridRes = 4;

			// Boost resolution for motocross park chunks so terrain features
			// are smooth enough for natural dirt bike riding
			const bool bInPark = (CX > -180000.f && CX + ChunkSize < -60000.f &&
				CY > -40000.f && CY + ChunkSize < 80000.f);
			if (bInPark) GridRes = FMath::Max(GridRes, 20);

			const float Step = ChunkSize / (float)GridRes;

			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector2D> UVs;
			TArray<FVector> Normals;
			TArray<FColor> Colors;

			Vertices.Reserve((GridRes + 1) * (GridRes + 1));
			UVs.Reserve((GridRes + 1) * (GridRes + 1));
			Normals.Reserve((GridRes + 1) * (GridRes + 1));

			for (int32 Y = 0; Y <= GridRes; ++Y)
			{
				for (int32 X = 0; X <= GridRes; ++X)
				{
					float WX = CX + X * Step;
					float WY = CY + Y * Step;
					float WZ = GetTerrainHeight(WX, WY);
					Vertices.Add(FVector(WX, WY, WZ));
					UVs.Add(FVector2D(WX / 1000.f, WY / 1000.f)); // tile every 1000 UU
					Normals.Add(FVector(0.f, 0.f, 1.f)); // approximate; could compute from neighbors
				}
			}

			// Compute better normals from height differences
			for (int32 Y = 0; Y <= GridRes; ++Y)
			{
				for (int32 X = 0; X <= GridRes; ++X)
				{
					float WX = CX + X * Step;
					float WY = CY + Y * Step;
					float Hc = GetTerrainHeight(WX, WY);
					float Hx = GetTerrainHeight(WX + Step, WY);
					float Hy = GetTerrainHeight(WX, WY + Step);
					FVector Dx(Step, 0.f, Hx - Hc);
					FVector Dy(0.f, Step, Hy - Hc);
					FVector N = FVector::CrossProduct(Dx, Dy).GetSafeNormal();
					if (N.Z < 0.f) N = -N;
					Normals[Y * (GridRes + 1) + X] = N;
				}
			}

			Triangles.Reserve(GridRes * GridRes * 6);
			for (int32 Y = 0; Y < GridRes; ++Y)
			{
				for (int32 X = 0; X < GridRes; ++X)
				{
					int32 I0 = Y * (GridRes + 1) + X;
					int32 I1 = I0 + 1;
					int32 I2 = I0 + (GridRes + 1);
					int32 I3 = I2 + 1;
					Triangles.Add(I0); Triangles.Add(I2); Triangles.Add(I1);
					Triangles.Add(I1); Triangles.Add(I2); Triangles.Add(I3);
				}
			}

			FString CompName = FString::Printf(TEXT("TerrainChunk_%d"), ChunkIdx++);
			UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(TerrainActor, *CompName);
			// Enable collision on all chunks so character can walk on terrain everywhere
			PMC->CreateMeshSection(0, Vertices, Triangles, Normals, UVs, Colors, TArray<FProcMeshTangent>(), true);
			PMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			PMC->SetCollisionObjectType(ECC_WorldStatic);
			PMC->SetCollisionResponseToAllChannels(ECR_Block);
			PMC->RegisterComponent();
			TerrainActor->AddInstanceComponent(PMC);

			// Apply terrain material with natural biome blending
			// Sample multiple points within the chunk and average for smooth color
			UMaterialInstanceDynamic* Mat = nullptr;
			if (ColorMaterial)
			{
				Mat = UMaterialInstanceDynamic::Create(ColorMaterial, PMC);
				if (Mat)
				{
					// Deterministic noise from position for organic variation
					auto TerrainNoise = [](float X, float Y, float Scale) -> float
					{
						float fx = X * Scale;
						float fy = Y * Scale;
						int32 ix = FMath::FloorToInt(fx);
						int32 iy = FMath::FloorToInt(fy);
						float fracX = fx - ix;
						float fracY = fy - iy;
						fracX = fracX * fracX * (3.f - 2.f * fracX);
						fracY = fracY * fracY * (3.f - 2.f * fracY);
						auto Hash = [](int32 x, int32 y) -> float
						{
							int32 h = x * 374761393 + y * 668265263;
							h = (h ^ (h >> 13)) * 1274126177;
							return (float)(h & 0x7FFFFFFF) / (float)0x7FFFFFFF;
						};
						float c00 = Hash(ix, iy);
						float c10 = Hash(ix + 1, iy);
						float c01 = Hash(ix, iy + 1);
						float c11 = Hash(ix + 1, iy + 1);
						return FMath::Lerp(
							FMath::Lerp(c00, c10, fracX),
							FMath::Lerp(c01, c11, fracX), fracY);
					};

					// Color computation for a single world position
					auto ComputeTerrainColor = [&](float WX, float WY) -> FLinearColor
					{
						const float WZ = GetTerrainHeight(WX, WY);

						// Multi-octave noise for organic grass variation
						float n1 = TerrainNoise(WX, WY, 0.00006f);
						float n2 = TerrainNoise(WX, WY, 0.00025f);
						float n3 = TerrainNoise(WX, WY, 0.0008f);
						float NoiseVal = n1 * 0.5f + n2 * 0.3f + n3 * 0.2f;

						// PUBG-style terrain tones: yellowy-green dried grass
						const FLinearColor GrassLush(0.16f, 0.24f, 0.08f);
						const FLinearColor GrassDry(0.22f, 0.22f, 0.10f);
						const FLinearColor GrassLight(0.26f, 0.24f, 0.12f);

						FLinearColor TC;
						if (NoiseVal < 0.4f)
							TC = FMath::Lerp(GrassLush, GrassDry, NoiseVal / 0.4f);
						else
							TC = FMath::Lerp(GrassDry, GrassLight, (NoiseVal - 0.4f) / 0.6f);

						// Height-based: valleys richer green, hills browner
						float HFactor = FMath::Clamp(FMath::Abs(WZ) / 1500.f, 0.f, 1.f);
						const FLinearColor SlopeBrown(0.18f, 0.14f, 0.09f);
						TC = FMath::Lerp(TC, SlopeBrown, HFactor * 0.55f);

						// Slope-based: steeper terrain gets exposed earth
						float Hx = GetTerrainHeight(WX + 200.f, WY);
						float Hy = GetTerrainHeight(WX, WY + 200.f);
						float SlopeX = (Hx - WZ) / 200.f;
						float SlopeY = (Hy - WZ) / 200.f;
						float Steepness = FMath::Sqrt(SlopeX * SlopeX + SlopeY * SlopeY);
						if (Steepness > 0.15f)
						{
							float RockAlpha = FMath::Clamp((Steepness - 0.15f) / 0.4f, 0.f, 1.f);
							RockAlpha = RockAlpha * RockAlpha;
							const FLinearColor RockGray(0.22f, 0.20f, 0.17f);
							TC = FMath::Lerp(TC, FMath::Lerp(SlopeBrown, RockGray, 0.5f), RockAlpha);
						}

						// --- Biome overrides with Hermite smoothstep + noise-warped edges ---

						// Pyramid desert
						{
							const float PCX = 22248.f, PCY = -62419.f;
							const float SR = 50000.f, ST = 18000.f;
							float PDist = FMath::Sqrt(FMath::Square(WX - PCX) + FMath::Square(WY - PCY));
							if (PDist < SR + ST)
							{
								float A = (PDist <= SR) ? 1.f : 1.f - FMath::Clamp((PDist - SR) / ST, 0.f, 1.f);
								A = A * A * (3.f - 2.f * A);
								// Noise-warp the edge for natural jaggedness
								float EdgeN = TerrainNoise(WX * 1.3f, WY * 0.7f, 0.00015f) * 0.25f;
								A = FMath::Clamp(A + EdgeN * (1.f - A * 0.5f), 0.f, 1.f);
								const FLinearColor DesertSand(0.55f, 0.48f, 0.32f);
								TC = FMath::Lerp(TC, DesertSand, A);
							}
						}

						// Pirate beach
						{
							const float BCX = -248405.f, BCY = 7389.f;
							const float BR = 60000.f, BT = 25000.f;
							float BDist = FMath::Sqrt(FMath::Square(WX - BCX) + FMath::Square(WY - BCY));
							if (BDist < BR + BT)
							{
								float A = (BDist <= BR) ? 1.f : 1.f - FMath::Clamp((BDist - BR) / BT, 0.f, 1.f);
								A = A * A * (3.f - 2.f * A);
								float EdgeN = TerrainNoise(WX + 7000.f, WY + 3000.f, 0.00018f) * 0.2f;
								A = FMath::Clamp(A + EdgeN * (1.f - A * 0.5f), 0.f, 1.f);
								const FLinearColor BeachSand(0.82f, 0.76f, 0.58f);
								TC = FMath::Lerp(TC, BeachSand, A);
							}
						}

						// Mountain snow
						{
							const float MtnZoneCY = 110000.f;
							float MDX = FMath::Max(0.f, FMath::Abs(WX) - 120000.f);
							float MDY = FMath::Max(0.f, FMath::Abs(WY - MtnZoneCY) - 15000.f);
							float MDist = FMath::Sqrt(MDX * MDX + MDY * MDY);
							if (MDist < 18000.f)
							{
								float A = 1.f - FMath::Clamp(MDist / 18000.f, 0.f, 1.f);
								A = A * A * (3.f - 2.f * A);
								const FLinearColor SnowWhite(0.85f, 0.88f, 0.92f);
								TC = FMath::Lerp(TC, SnowWhite, A);
							}
						}

						// Christmas snow
						{
							const float XCX = 39000.f, XCY = 117000.f;
							const float XHX = 30000.f, XHY = 14000.f, XT = 10000.f;
							float XDX = FMath::Max(0.f, FMath::Abs(WX - XCX) - XHX);
							float XDY = FMath::Max(0.f, FMath::Abs(WY - XCY) - XHY);
							float XDist = FMath::Sqrt(XDX * XDX + XDY * XDY);
							if (XDist < XT)
							{
								float A = 1.f - FMath::Clamp(XDist / XT, 0.f, 1.f);
								A = A * A * (3.f - 2.f * A);
								const FLinearColor XmasSnow(0.88f, 0.92f, 0.97f);
								TC = FMath::Lerp(TC, XmasSnow, A);
							}
						}

						return TC;
					};

					// Dense sampling across the chunk for accurate biome color
					// Use 7×7 grid (49 samples) instead of 3×3 (9 samples)
					// This dramatically reduces visible chunk boundary artifacts
					FLinearColor AvgColor(0.f, 0.f, 0.f, 0.f);
					constexpr int32 SampleRes = 7;
					const float SampleStep = ChunkSize / float(SampleRes);
					for (int32 SY = 0; SY < SampleRes; ++SY)
					{
						for (int32 SX = 0; SX < SampleRes; ++SX)
						{
							float SampleX = CX + SampleStep * (0.5f + SX);
							float SampleY = CY + SampleStep * (0.5f + SY);
							AvgColor += ComputeTerrainColor(SampleX, SampleY);
						}
					}
					constexpr float InvSamples = 1.f / float(SampleRes * SampleRes);
					AvgColor.R *= InvSamples;
					AvgColor.G *= InvSamples;
					AvgColor.B *= InvSamples;

					// Per-chunk micro-variation for visual texture (reduces flat look)
					int32 ChunkHash = FMath::FloorToInt(CX * 0.01f) * 374761 + FMath::FloorToInt(CY * 0.01f) * 668265;
					ChunkHash = (ChunkHash ^ (ChunkHash >> 13)) * 1274126177;
					float ChunkNoise = float(ChunkHash & 0xFFFF) / 65535.f;
					float ColorVar = (ChunkNoise - 0.5f) * 0.06f; // ±3% color jitter
					AvgColor.R = FMath::Clamp(AvgColor.R + ColorVar, 0.f, 1.f);
					AvgColor.G = FMath::Clamp(AvgColor.G + ColorVar * 1.2f, 0.f, 1.f);
					AvgColor.B = FMath::Clamp(AvgColor.B + ColorVar * 0.5f, 0.f, 1.f);

					// Vary roughness by biome (sand=smoother, rock=rougher)
					float ChunkRough = 0.88f + (ChunkNoise - 0.5f) * 0.08f;
					Mat->SetVectorParameterValue(TEXT("BaseColor"), AvgColor);
					Mat->SetScalarParameterValue(TEXT("Roughness"), ChunkRough);
					PMC->SetMaterial(0, Mat);
				}
			}
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHC: Generated procedural terrain with %d chunks"), ChunkIdx);
}

// ---------------------------------------------------------------------------
// Grass & Foliage Instancing (HISM)
// ---------------------------------------------------------------------------

bool UPHCCitySubsystem::IsOnRoadOrSidewalk(float X, float Y) const
{
	// Check against the 25x25 road grid
	for (int32 Ri = 0; Ri < NumRoads; ++Ri)
	{
		const float RoadCenter = FirstRoad + Ri * RoadSpacing;
		const float HalfRoad = RoadWidth * 0.5f + SidewalkWidth;
		// X-aligned road
		if (FMath::Abs(Y - RoadCenter) < HalfRoad &&
			X > FirstRoad - 500.f && X < FirstRoad + (NumRoads - 1) * RoadSpacing + 500.f)
			return true;
		// Y-aligned road
		if (FMath::Abs(X - RoadCenter) < HalfRoad &&
			Y > FirstRoad - 500.f && Y < FirstRoad + (NumRoads - 1) * RoadSpacing + 500.f)
			return true;
	}
	return false;
}

// ---------------------------------------------------------------------------
// Gold Coins
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnGoldCoins()
{
	UWorld* W = GetWorld();
	if (!W || !CylinderMesh || !ColorMaterial) return;

	// First 2 coins near spawn so player sees them immediately
	const FVector NearSpawnPositions[2] = {
		{ 1500.f,     0.f, 0.f},
		{-1500.f,     0.f, 0.f},
	};

	// Last 4 coins high in the sky — reachable only by jet
	const FVector SkyCoinPositions[4] = {
		{     0.f,      0.f, 8000.f},
		{ 15000.f,  15000.f, 10000.f},
		{-15000.f,  20000.f, 9000.f},
		{ 20000.f, -10000.f, 11000.f},
	};

	for (int32 CI = 0; CI < NumGoldCoins; ++CI)
	{
		FGoldCoinData& GC = GoldCoins[CI];

		float X, Y, Z;
		if (CI < 2)
		{
			// Near-spawn coins at known positions
			X = NearSpawnPositions[CI].X;
			Y = NearSpawnPositions[CI].Y;
			Z = GetTerrainHeight(X, Y) + 600.f;
		}
		else if (CI >= 10)
		{
			// Sky coins — high altitude, jet-only
			const FVector& SP = SkyCoinPositions[CI - 10];
			X = SP.X;
			Y = SP.Y;
			Z = SP.Z;
		}
		else
		{
			// Scattered across the entire playable map (city, outskirts, hills, beach)
			X = CityRandom.FRandRange(-80000.f, 80000.f);
			Y = CityRandom.FRandRange(-80000.f, 80000.f);
			Z = GetTerrainHeight(X, Y) + 600.f;
		}
		const FVector SpawnPos(X, Y, Z);

		GC.OriginalSpawnPos = SpawnPos;
		GC.CurrentPos = SpawnPos;

		AActor* CoinActor = W->SpawnActor<AActor>(AActor::StaticClass(),
			SpawnPos, FRotator(90.f, 0.f, 0.f));
		if (!CoinActor) continue;
		GC.Actor = CoinActor;

#if WITH_EDITOR
		CoinActor->SetActorLabel(*FString::Printf(TEXT("GoldCoin_%d"), CI));
#endif

		// Mesh as root component (same pattern as ramps/launch pads)
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(CoinActor,
			*FString::Printf(TEXT("CoinMesh_%d"), CI));
		MC->SetStaticMesh(CylinderMesh);
		MC->SetMaterial(0, ColorMaterial);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->bNeverDistanceCull = true;
		MC->SetWorldLocation(SpawnPos);
		MC->SetWorldScale3D(FVector(7.f, 7.f, 0.7f));
		MC->RegisterComponent();
		CoinActor->SetRootComponent(MC);

		// Mirror-polished gold — highly reflective, catches sunlight
		ApplyPBR(MC,
			FLinearColor(1.0f, 0.84f, 0.0f),      // BaseColor: bright pure gold
			1.0f,                                   // Metallic: full metal
			0.05f,                                  // Roughness: mirror polish
			FLinearColor::Black);                   // No emissive
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("SpawnGoldCoins -- Spawned %d gold coins."), NumGoldCoins);
}

void UPHCCitySubsystem::SpawnCollectibleMushrooms()
{
	UWorld* W = GetWorld();
	if (!W || !SphereMesh || !CylinderMesh || !ColorMaterial) return;

	// Jungle bounds
	const float JMinX = -45000.f, JMaxX = 75000.f;
	const float JMinY = -162000.f, JMaxY = -106000.f;

	FRandomStream MushRand(7777);
	const int32 NumOriginalMushrooms = 12;

	for (int32 MI = 0; MI < NumOriginalMushrooms; ++MI)
	{
		CollectibleMushrooms.AddDefaulted();
		FMushroomData& MD = CollectibleMushrooms.Last();

		const float X = MushRand.FRandRange(JMinX + 3000.f, JMaxX - 3000.f);
		const float Y = MushRand.FRandRange(JMinY + 3000.f, JMaxY - 3000.f);
		const float Z = GetTerrainHeight(X, Y) + 40.f;
		const FVector SpawnPos(X, Y, Z);

		MD.OriginalSpawnPos = SpawnPos;
		MD.CurrentPos = SpawnPos;
		MD.BobPhase = MushRand.FRand() * PI * 2.f;

		AActor* MA = W->SpawnActor<AActor>(AActor::StaticClass(),
			SpawnPos, FRotator::ZeroRotator);
		if (!MA) continue;
		MD.Actor = MA;

#if WITH_EDITOR
		MA->SetActorLabel(*FString::Printf(TEXT("CollectMushroom_%d"), MI));
#endif

		// Stem
		UStaticMeshComponent* Stem = NewObject<UStaticMeshComponent>(MA,
			*FString::Printf(TEXT("MStem_%d"), MI));
		Stem->SetStaticMesh(CylinderMesh);
		Stem->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Stem->SetWorldLocation(SpawnPos + FVector(0.f, 0.f, 20.f));
		Stem->SetWorldScale3D(FVector(0.12f, 0.12f, 0.40f));
		Stem->RegisterComponent();
		MA->SetRootComponent(Stem);
		ApplyPBR(Stem, FLinearColor(0.75f, 0.70f, 0.80f), 0.f, 0.75f,
			FLinearColor(0.20f, 0.05f, 0.35f)); // faint purple emissive

		// Cap
		UStaticMeshComponent* Cap = NewObject<UStaticMeshComponent>(MA,
			*FString::Printf(TEXT("MCap_%d"), MI));
		Cap->SetStaticMesh(SphereMesh);
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cap->SetWorldLocation(SpawnPos + FVector(0.f, 0.f, 55.f));
		Cap->SetWorldScale3D(FVector(0.45f, 0.45f, 0.20f));
		Cap->AttachToComponent(Stem, FAttachmentTransformRules::KeepWorldTransform);
		Cap->RegisterComponent();
		const FLinearColor CapColor(0.55f, 0.10f, 0.75f);
		ApplyPBR(Cap, CapColor, 0.f, 0.70f,
			CapColor * 4.f); // strong purple emissive glow

		// Point light — bioluminescent purple beacon
		UPointLightComponent* PL = NewObject<UPointLightComponent>(MA,
			*FString::Printf(TEXT("MLight_%d"), MI));
		PL->SetWorldLocation(SpawnPos + FVector(0.f, 0.f, 60.f));
		PL->SetIntensity(8000.f);
		PL->SetAttenuationRadius(900.f);
		PL->SetLightColor(FLinearColor(0.50f, 0.08f, 0.80f));
		PL->CastShadows = false;
		PL->AttachToComponent(Stem, FAttachmentTransformRules::KeepWorldTransform);
		PL->RegisterComponent();
		MD.GlowLight = PL;
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("SpawnCollectibleMushrooms -- Spawned %d mushrooms."),
		CollectibleMushrooms.Num());
}

void UPHCCitySubsystem::SpawnGrassSystem()
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh || !SphereMesh || !ConeMesh) return;

	GrassActor = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator);
	if (!GrassActor) return;
#if WITH_EDITOR
	GrassActor->SetActorLabel(TEXT("GrassInstances"));
#endif

	// PUBG-style grass: 6 color variants (green + yellowy dried)
	const FLinearColor GrassColors[] = {
		FLinearColor(0.14f, 0.22f, 0.06f),  // medium green
		FLinearColor(0.18f, 0.26f, 0.08f),  // light green
		FLinearColor(0.22f, 0.24f, 0.10f),  // yellow-green (PUBG dominant)
		FLinearColor(0.25f, 0.22f, 0.08f),  // dried yellow
		FLinearColor(0.20f, 0.28f, 0.07f),  // bright green
		FLinearColor(0.28f, 0.25f, 0.12f),  // straw/hay tone
	};

	GrassHISMs.Empty();
	for (int32 Gi = 0; Gi < 6; ++Gi)
	{
		FString HName = FString::Printf(TEXT("GrassHISM_%d"), Gi);
		UHierarchicalInstancedStaticMeshComponent* HISM =
			NewObject<UHierarchicalInstancedStaticMeshComponent>(GrassActor, *HName);
		HISM->SetStaticMesh(ConeMesh);  // tapered blade shape
		HISM->SetCastShadow(false);
		HISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		HISM->SetCullDistances(0.f, 10000.f);
		HISM->RegisterComponent();
		GrassActor->AddInstanceComponent(HISM);

		if (ColorMaterial)
		{
			UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, HISM);
			if (Mat)
			{
				Mat->SetVectorParameterValue(TEXT("BaseColor"), GrassColors[Gi]);
				Mat->SetScalarParameterValue(TEXT("Roughness"), 0.82f);
				HISM->SetMaterial(0, Mat);
			}
		}

		GrassHISMs.Add(HISM);
	}

	// --- Ground clutter HISMs: rocks + dirt patches (spawned once, static) ---
	if (SphereMesh && CubeMesh && ColorMaterial)
	{
		// Rock HISM
		auto* RockHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(GrassActor, TEXT("RockClutterHISM"));
		RockHISM->SetStaticMesh(SphereMesh);
		RockHISM->SetCastShadow(false);
		RockHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		RockHISM->SetCullDistances(0.f, 8000.f);
		RockHISM->RegisterComponent();
		GrassActor->AddInstanceComponent(RockHISM);
		if (ColorMaterial)
		{
			UMaterialInstanceDynamic* RM = UMaterialInstanceDynamic::Create(ColorMaterial, RockHISM);
			if (RM)
			{
				RM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.25f, 0.23f, 0.20f));
				RM->SetScalarParameterValue(TEXT("Roughness"), 0.85f);
				RockHISM->SetMaterial(0, RM);
			}
		}

		// Dirt patch HISM
		auto* DirtHISM = NewObject<UHierarchicalInstancedStaticMeshComponent>(GrassActor, TEXT("DirtClutterHISM"));
		DirtHISM->SetStaticMesh(CubeMesh);
		DirtHISM->SetCastShadow(false);
		DirtHISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DirtHISM->SetCullDistances(0.f, 6000.f);
		DirtHISM->RegisterComponent();
		GrassActor->AddInstanceComponent(DirtHISM);
		if (ColorMaterial)
		{
			UMaterialInstanceDynamic* DM = UMaterialInstanceDynamic::Create(ColorMaterial, DirtHISM);
			if (DM)
			{
				DM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.16f, 0.11f, 0.05f));
				DM->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
				DirtHISM->SetMaterial(0, DM);
			}
		}

		// Scatter 600 rocks and 400 dirt patches across the world (deterministic)
		FRandomStream ClutterRand(42424);
		for (int32 Ci = 0; Ci < 1000; ++Ci)
		{
			float CX = ClutterRand.FRandRange(-80000.f, 80000.f);
			float CY = ClutterRand.FRandRange(-80000.f, 80000.f);
			if (IsOnRoadOrSidewalk(CX, CY)) continue;
			float CZ = GetTerrainHeight(CX, CY);
			if (CZ > 200.f) continue;
			// Skip desert/beach/snow
			if (FMath::Square(CX - 22248.f) + FMath::Square(CY + 62419.f) < 50000.f * 50000.f) continue;
			if (CX > 4000.f && CX < 74000.f && CY > 98000.f && CY < 136000.f) continue;

			if (Ci < 600)
			{
				// Rock
				float RS = ClutterRand.FRandRange(0.05f, 0.20f);
				FTransform RT;
				RT.SetLocation(FVector(CX, CY, CZ + RS * 25.f));
				RT.SetRotation(FRotator(
					ClutterRand.FRandRange(-15.f, 15.f),
					ClutterRand.FRandRange(0.f, 360.f),
					ClutterRand.FRandRange(-10.f, 10.f)).Quaternion());
				RT.SetScale3D(FVector(RS, RS * ClutterRand.FRandRange(0.7f, 1.3f), RS * ClutterRand.FRandRange(0.4f, 0.8f)));
				RockHISM->AddInstance(RT, false);
			}
			else
			{
				// Dirt patch
				float PW = ClutterRand.FRandRange(0.4f, 1.0f);
				FTransform DT;
				DT.SetLocation(FVector(CX, CY, CZ + 1.f));
				DT.SetRotation(FRotator(0.f, ClutterRand.FRandRange(0.f, 360.f), 0.f).Quaternion());
				DT.SetScale3D(FVector(PW, PW * ClutterRand.FRandRange(0.6f, 1.4f), 0.005f));
				DirtHISM->AddInstance(DT, false);
			}
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHC: Grass instancing system created with 4 HISM + clutter"));
}

void UPHCCitySubsystem::UpdateGrassInstances(const FVector& PlayerLoc)
{
	if (GrassHISMs.Num() < 6) return;

	// Only update when player moves >2000 UU from last update
	if (FVector::Dist2D(PlayerLoc, LastGrassUpdateLocation) < 2000.f)
		return;
	LastGrassUpdateLocation = PlayerLoc;

	// Clear all instances
	for (auto& HISM : GrassHISMs)
	{
		if (HISM) HISM->ClearInstances();
	}

	// PUBG-density grass: thick carpet of tall blades
	const float Radius = 12000.f;
	const int32 TotalBlades = 15000;
	FRandomStream GrassRand(FMath::FloorToInt(PlayerLoc.X * 0.01f) * 1000 + FMath::FloorToInt(PlayerLoc.Y * 0.01f));

	auto ShouldSkipZone = [](float GX, float GY) -> bool
	{
		if (FMath::Square(GX - 22248.f) + FMath::Square(GY + 62419.f) < 50000.f * 50000.f) return true;
		if (FMath::Square(GX + 248405.f) + FMath::Square(GY - 7389.f) < 65000.f * 65000.f) return true;
		if (GX > 4000.f && GX < 74000.f && GY > 98000.f && GY < 136000.f) return true;
		return false;
	};

	for (int32 Bi = 0; Bi < TotalBlades; ++Bi)
	{
		float Angle = GrassRand.FRandRange(0.f, 2.f * PI);
		float RawDist = GrassRand.FRand();
		float Dist = 150.f + FMath::Pow(RawDist, 0.5f) * (Radius - 150.f);
		float GX = PlayerLoc.X + FMath::Cos(Angle) * Dist;
		float GY = PlayerLoc.Y + FMath::Sin(Angle) * Dist;

		if (IsOnRoadOrSidewalk(GX, GY)) continue;
		float GZ = GetTerrainHeight(GX, GY);
		if (GZ > 200.f) continue;
		if (ShouldSkipZone(GX, GY)) continue;

		float Yaw = GrassRand.FRandRange(0.f, 360.f);
		float Pitch = GrassRand.FRandRange(-12.f, 12.f);
		// PUBG grass: predominantly tall (20-50 UU), some short ground cover
		float BladeH = GrassRand.FRandRange(18.f, 50.f);
		if (GrassRand.FRand() < 0.2f) BladeH = GrassRand.FRandRange(5.f, 18.f);

		FTransform BladeT;
		BladeT.SetLocation(FVector(GX, GY, GZ + BladeH * 0.45f));
		BladeT.SetRotation(FRotator(Pitch, Yaw, 0.f).Quaternion());
		BladeT.SetScale3D(FVector(0.018f, 0.006f, BladeH / 100.f));

		int32 Variant = Bi % 6;
		if (GrassHISMs[Variant])
			GrassHISMs[Variant]->AddInstance(BladeT);
	}

	// Ground clutter is handled by static HISM in SpawnGrassSystem
}

// ---------------------------------------------------------------------------
// Visual Polish — atmospheric dust motes near camera
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnVisualPolish()
{
	UWorld* World = GetWorld();
	if (!World || !SphereMesh || !GlassMaterial) return;

	// Atmospheric dust motes — 40 tiny translucent spheres that follow the camera
	DustMoteActor = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator);
	if (!DustMoteActor) return;
#if WITH_EDITOR
	DustMoteActor->SetActorLabel(TEXT("DustMotes"));
#endif

	USceneComponent* Root = NewObject<USceneComponent>(DustMoteActor, TEXT("DustRoot"));
	Root->RegisterComponent();
	DustMoteActor->SetRootComponent(Root);

	FRandomStream DustRand(9999);
	const int32 NumMotes = 40;
	DustMotes.Reserve(NumMotes);

	for (int32 i = 0; i < NumMotes; ++i)
	{
		UStaticMeshComponent* Mote = NewObject<UStaticMeshComponent>(
			DustMoteActor, *FString::Printf(TEXT("Dust_%d"), i));
		Mote->SetStaticMesh(SphereMesh);
		Mote->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Mote->SetCastShadow(false);
		const float S = DustRand.FRandRange(0.005f, 0.015f);
		Mote->SetWorldScale3D(FVector(S));
		Mote->SetRelativeLocation(FVector(
			DustRand.FRandRange(-800.f, 800.f),
			DustRand.FRandRange(-800.f, 800.f),
			DustRand.FRandRange(50.f, 400.f)));
		Mote->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);

		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(GlassMaterial, Mote);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.9f, 0.9f, 0.85f));
			Mat->SetScalarParameterValue(TEXT("Opacity"), DustRand.FRandRange(0.08f, 0.20f));
			Mat->SetScalarParameterValue(TEXT("Roughness"), 1.f);
			Mote->SetMaterial(0, Mat);
		}
		Mote->RegisterComponent();
		DustMotes.Add(Mote);
	}

	// ================================================================
	// PUBG 2026 deciduous tree — single test tree near spawn for tuning
	// ================================================================
	if (CylinderMesh && SphereMesh && ColorMaterial)
	{
		// Right next to player spawn (-28000, -28000, 58)
		const FVector TreePos(-27700.f, -28000.f, 58.f);

		AActor* TreeActor = World->SpawnActor<AActor>(AActor::StaticClass(),
			TreePos, FRotator::ZeroRotator);
		if (TreeActor)
		{
			USceneComponent* TRoot = NewObject<USceneComponent>(TreeActor, TEXT("TRoot"));
			TRoot->RegisterComponent();
			TreeActor->SetRootComponent(TRoot);

			int32 CompIdx = 0;
			auto MakeTC = [&](UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot,
				const FVector& Scale, const FLinearColor& Col, float Rough)
			{
				UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(TreeActor,
					*FString::Printf(TEXT("TC_%d"), CompIdx++));
				MC->SetStaticMesh(Mesh);
				MC->AttachToComponent(TRoot, FAttachmentTransformRules::KeepRelativeTransform);
				MC->SetRelativeLocation(Loc);
				MC->SetRelativeRotation(Rot);
				MC->SetRelativeScale3D(Scale);
				MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				MC->SetCastShadow(true);
				MC->RegisterComponent();
				TreeActor->AddInstanceComponent(MC);
				if (ColorMaterial)
				{
					UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, MC);
					if (Mat)
					{
						Mat->SetVectorParameterValue(TEXT("BaseColor"), Col);
						Mat->SetScalarParameterValue(TEXT("Roughness"), Rough);
						MC->SetMaterial(0, Mat);
					}
				}
			};

			// === PUBG TREE PROPORTIONS ===
			// Total height: ~2800 UU
			// Trunk: 60% of height (1680 UU), thin
			// Canopy: top 50% (1400 UU), taller than wide (vertical oval)
			const float TotalH = 2800.f;
			const float TrunkH = TotalH * 0.60f;   // 1680 UU
			const float TrunkBaseR = 35.f;           // base radius
			const float TrunkTopR = 22.f;            // top radius (tapered)
			const float CanopyBottom = TotalH * 0.50f; // canopy starts at 50%
			const float CanopyTop = TotalH * 1.0f;
			const float CanopyW = 700.f;             // canopy width (radius)
			const float CanopyTallH = CanopyTop - CanopyBottom; // 1400 UU tall

			// Trunk color: gray-brown (PUBG desaturated bark)
			const FLinearColor BarkCol(0.22f, 0.18f, 0.13f);
			const FLinearColor BarkLight(0.28f, 0.24f, 0.18f);

			// --- TRUNK: 4 tapered segments with slight lean ---
			const int32 TrunkSegs = 4;
			const float SegH = TrunkH / TrunkSegs;
			const float LeanX = 40.f; // slight lean
			for (int32 S = 0; S < TrunkSegs; ++S)
			{
				float T = float(S) / float(TrunkSegs);
				float R = FMath::Lerp(TrunkBaseR, TrunkTopR, T);
				if (S == 0) R *= 1.4f; // flared base
				float SZ = S * SegH + SegH * 0.5f;
				float SX = LeanX * FMath::Sin(T * PI * 0.5f);
				FLinearColor SegCol = FMath::Lerp(BarkCol, BarkLight, T);

				MakeTC(CylinderMesh, FVector(SX, 0.f, SZ), FRotator::ZeroRotator,
					FVector(R / 50.f, R / 50.f, SegH / 100.f), SegCol, 0.90f);
			}

			// --- MAJOR BRANCHES: 3-4 thick branches spreading from trunk top ---
			const float BranchStartZ = TrunkH * 0.85f;
			const FLinearColor BranchCol(0.24f, 0.20f, 0.14f);
			const float BranchAngles[] = { 0.f, 95.f, 200.f, 310.f };
			const float BranchPitches[] = { 35.f, 42.f, 30.f, 38.f };
			const float BranchLens[] = { 400.f, 350.f, 450.f, 320.f };

			for (int32 B = 0; B < 4; ++B)
			{
				float BY = FMath::DegreesToRadians(BranchAngles[B]);
				float BP = FMath::DegreesToRadians(BranchPitches[B]);
				float BLen = BranchLens[B];
				float BR = 14.f - B * 1.5f; // taper per branch

				FVector BDir(FMath::Cos(BP) * FMath::Cos(BY),
							 FMath::Cos(BP) * FMath::Sin(BY),
							 FMath::Sin(BP));
				FVector BMid = FVector(LeanX * 0.8f, 0.f, BranchStartZ) + BDir * BLen * 0.5f;

				MakeTC(CylinderMesh, BMid,
					FRotator(0.f, BranchAngles[B], 90.f - BranchPitches[B]),
					FVector(BR / 50.f, BR / 50.f, BLen / 100.f),
					BranchCol, 0.88f);
			}

			// --- CANOPY: vertical oval shape, 12-16 foliage clusters ---
			// PUBG canopy: taller than wide, irregular bumpy surface
			const FLinearColor LeafDark(0.08f, 0.22f, 0.05f);
			const FLinearColor LeafMid(0.12f, 0.30f, 0.06f);
			const FLinearColor LeafBright(0.18f, 0.35f, 0.08f);
			const FLinearColor LeafHighlight(0.22f, 0.38f, 0.10f);

			FRandomStream CanopyRand(12345);
			const int32 NumLeafClusters = 14;

			for (int32 Li = 0; Li < NumLeafClusters; ++Li)
			{
				float FI = float(Li);
				// Distribute clusters in a vertical oval pattern
				float Angle = CanopyRand.FRandRange(0.f, 2.f * PI);
				float VPos = CanopyRand.FRandRange(0.f, 1.f); // 0=bottom, 1=top of canopy
				// Oval profile: wider at middle, narrow at top and bottom
				float OvalFactor = FMath::Sin(VPos * PI); // 0 at edges, 1 at equator
				float HorizDist = CanopyW * OvalFactor * CanopyRand.FRandRange(0.3f, 0.95f);
				float CX = FMath::Cos(Angle) * HorizDist + LeanX * 0.5f;
				float CY = FMath::Sin(Angle) * HorizDist;
				float CZ = CanopyBottom + VPos * CanopyTallH;

				// Cluster size: larger in middle, smaller at edges
				float ClusterSz = CanopyRand.FRandRange(180.f, 320.f);
				ClusterSz *= (0.7f + 0.3f * OvalFactor);

				// Color: darker interior, brighter edges (sun-facing)
				float SunFacing = FMath::Max(0.f, FMath::Cos(Angle)); // east-facing = bright
				FLinearColor LeafCol = FMath::Lerp(LeafDark, LeafMid, VPos);
				LeafCol = FMath::Lerp(LeafCol, LeafHighlight, SunFacing * 0.5f);
				// Per-cluster random variation
				LeafCol.R += CanopyRand.FRandRange(-0.02f, 0.02f);
				LeafCol.G += CanopyRand.FRandRange(-0.03f, 0.03f);

				// Flatten spheres vertically for leafy look (wider than tall)
				float FlattenZ = CanopyRand.FRandRange(0.5f, 0.75f);

				MakeTC(SphereMesh, FVector(CX, CY, CZ), FRotator::ZeroRotator,
					FVector(ClusterSz / 50.f, ClusterSz / 50.f, ClusterSz * FlattenZ / 50.f),
					LeafCol, 0.72f);
			}

			// --- CANOPY FILLER: 6 smaller clusters to fill gaps ---
			for (int32 Fi = 0; Fi < 6; ++Fi)
			{
				float Angle = CanopyRand.FRandRange(0.f, 2.f * PI);
				float VPos = CanopyRand.FRandRange(0.2f, 0.8f);
				float HorizDist = CanopyW * FMath::Sin(VPos * PI) * CanopyRand.FRandRange(0.1f, 0.6f);
				float CX = FMath::Cos(Angle) * HorizDist + LeanX * 0.3f;
				float CY = FMath::Sin(Angle) * HorizDist;
				float CZ = CanopyBottom + VPos * CanopyTallH;
				float Sz = CanopyRand.FRandRange(120.f, 220.f);

				MakeTC(SphereMesh, FVector(CX, CY, CZ), FRotator::ZeroRotator,
					FVector(Sz / 50.f, Sz / 50.f, Sz * 0.6f / 50.f),
					FMath::Lerp(LeafMid, LeafBright, CanopyRand.FRand()), 0.72f);
			}
		}
	}

	// ================================================================
	// Volumetric cloud clusters (replace flat planes with sphere HISMs)
	// ================================================================
	// Volumetric clouds removed — revisit later
}

// ---------------------------------------------------------------------------
void UPHCCitySubsystem::SpawnCityEnvironment()
{
	LoadDestroyedVehicleState();
	SpawnTerrainGrid();
	SpawnGround(); // flat ground slabs for city center collision
	SpawnOceanAndBeach();
	SpawnSkyAndLighting();
	SpawnCityGrid();
	SpawnBlockStreets();
	SpawnSkyscrapers();
	// SpawnStreetLabels();  // removed per user request
	// SpawnLangtree();  // removed per user request
	// SpawnStreetLights();  // removed for mobile battery/thermal perf
	SpawnRamps();
	SpawnOutskirts();
	// SpawnHills();        // removed — spherical terrain
	SpawnDirtBikePark();
	SpawnStarfield();
	SpawnCloudLayer();
	SpawnMountainRange();
	SpawnSandCastle();
	SpawnLakeAndPirateShip();
	SpawnJungle();
	SpawnUndergroundCavern();
	SpawnGiantCrab();
	SpawnGoldCoins();
	SpawnCollectibleMushrooms();
	SpawnGrassSystem();

	// --- PUBG 2026 DECIDUOUS FOREST ---
	{
		UWorld* TW = GetWorld();
		if (TW && CylinderMesh && SphereMesh && ColorMaterial)
		{
			FRandomStream ForestRand(88888);
			const int32 NumForestTrees = 1600;
			int32 SpawnedCount = 0;

			// Define cluster centers for natural grouping (like PUBG forest patches)
			const FVector2D ClusterCenters[] = {
				{-45000.f, -40000.f}, {-55000.f, 30000.f}, {40000.f, 45000.f},
				{60000.f, -35000.f}, {-30000.f, 55000.f}, {-70000.f, -20000.f},
				{25000.f, -50000.f}, {50000.f, 20000.f}, {-20000.f, -60000.f},
				{70000.f, 50000.f}, {-60000.f, 60000.f}, {35000.f, -65000.f},
			};
			constexpr int32 NumClusters = 12;

			for (int32 FTi = 0; FTi < NumForestTrees; ++FTi)
			{
				float TX, TY;
				if (ForestRand.FRand() < 0.65f)
				{
					// 65% of trees spawn near a cluster center (tight grouping)
					const FVector2D& CC = ClusterCenters[ForestRand.RandRange(0, NumClusters - 1)];
					TX = CC.X + ForestRand.FRandRange(-8000.f, 8000.f);
					TY = CC.Y + ForestRand.FRandRange(-8000.f, 8000.f);
				}
				else
				{
					// 35% scattered randomly for natural fill
					TX = ForestRand.FRandRange(-90000.f, 90000.f);
					TY = ForestRand.FRandRange(-90000.f, 90000.f);
				}
				// Skip inner city grid
				if (FMath::Abs(TX) < 18000.f && FMath::Abs(TY) < 18000.f) continue;
				// Skip desert
				if (FMath::Square(TX - 22248.f) + FMath::Square(TY + 62419.f) < 45000.f * 45000.f) continue;
				// Skip snow
				if (TX > 4000.f && TX < 74000.f && TY > 98000.f && TY < 136000.f) continue;
				// Skip jungle zone
				if (TY < -100000.f && TY > -170000.f && TX > -50000.f && TX < 80000.f) continue;

				float TZ = GetTerrainHeight(TX, TY);
				if (TZ > 300.f) continue;

				const FVector TreeBase(TX, TY, TZ);

			AActor* TreeA = TW->SpawnActor<AActor>(AActor::StaticClass(), TreeBase, FRotator::ZeroRotator);
			if (TreeA)
			{
				int32 TIdx = 0;
				auto SP = [&](UStaticMesh* Mesh, const FVector& WLoc, const FVector& Scale,
					const FLinearColor& Col, float Rough)
				{
					UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(TreeA,
						*FString::Printf(TEXT("DT_%d"), TIdx++));
					MC->SetStaticMesh(Mesh);
					MC->SetWorldLocation(WLoc);
					MC->SetWorldScale3D(Scale);
					MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					MC->CastShadow = true;
					MC->RegisterComponent();
					TreeA->AddInstanceComponent(MC);
					UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, MC);
					if (Mat)
					{
						Mat->SetVectorParameterValue(TEXT("BaseColor"), Col);
						Mat->SetScalarParameterValue(TEXT("Roughness"), Rough);
						MC->SetMaterial(0, Mat);
					}
					if (TIdx == 1) TreeA->SetRootComponent(MC);
				};

				const float BX = TreeBase.X, BY = TreeBase.Y, BZ = TreeBase.Z;

				// Randomized per-tree
				const float SizeMul = ForestRand.FRandRange(0.6f, 1.2f);
				const bool bAutumn = ForestRand.FRand() < 0.2f; // 20% autumn trees

				const float TrunkH = 1200.f * SizeMul;
				const float TrunkR = 25.f * SizeMul;
				const FLinearColor BarkCol(
					0.28f + ForestRand.FRandRange(-0.03f, 0.03f),
					0.24f + ForestRand.FRandRange(-0.03f, 0.03f),
					0.18f + ForestRand.FRandRange(-0.02f, 0.02f));

				// Lower trunk (slightly wider base)
				SP(CylinderMesh,
					FVector(BX, BY, BZ + TrunkH * 0.25f),
					FVector(TrunkR * 1.3f / 50.f, TrunkR * 1.3f / 50.f, TrunkH * 0.5f / 100.f),
					FLinearColor(0.24f, 0.20f, 0.14f), 0.92f);

				// Upper trunk (tapered into canopy)
				SP(CylinderMesh,
					FVector(BX, BY, BZ + TrunkH * 0.75f),
					FVector(TrunkR / 50.f, TrunkR / 50.f, TrunkH * 0.5f / 100.f),
					BarkCol, 0.90f);

				// === CANOPY ===
				// PUBG: broccoli-head shape — wide lumpy dome, NOT smooth sphere
				// Many small clusters (20+) creating bumpy organic silhouette
				// Width ≈ Height, roughly 1200 UU diameter × 1000 UU tall
				const float CanopyCenterZ = BZ + TrunkH + 200.f * SizeMul;
				const float CanopyRadius = 600.f * SizeMul;
				const float CanopyHeight = 500.f * SizeMul;

				FRandomStream CR(54321 + FTi * 137);

				// Color palette — green or autumn
				FLinearColor LeafDark, LeafMid, LeafBright, LeafSunlit;
				if (bAutumn)
				{
					LeafDark = FLinearColor(0.30f, 0.18f, 0.04f);
					LeafMid = FLinearColor(0.45f, 0.30f, 0.05f);
					LeafBright = FLinearColor(0.60f, 0.40f, 0.06f);
					LeafSunlit = FLinearColor(0.70f, 0.50f, 0.08f);
				}
				else
				{
					LeafDark = FLinearColor(0.06f, 0.18f, 0.04f);
					LeafMid = FLinearColor(0.10f, 0.26f, 0.05f);
					LeafBright = FLinearColor(0.16f, 0.32f, 0.06f);
					LeafSunlit = FLinearColor(0.24f, 0.38f, 0.08f);
				}

				// Core mass: 6 large overlapping spheres filling the center
				for (int32 Ci = 0; Ci < 6; ++Ci)
				{
					float Ang = Ci * (6.28f / 6.f) + CR.FRandRange(-0.4f, 0.4f);
					float Dist = CR.FRandRange(50.f, CanopyRadius * 0.4f);
					float ZOff = CR.FRandRange(-CanopyHeight * 0.2f, CanopyHeight * 0.2f);
					float Sz = CR.FRandRange(4.0f, 5.5f);

					SP(SphereMesh,
						FVector(BX + FMath::Cos(Ang) * Dist, BY + FMath::Sin(Ang) * Dist, CanopyCenterZ + ZOff),
						FVector(Sz, Sz, Sz * 0.7f),
						FMath::Lerp(LeafDark, LeafMid, CR.FRand()), 0.70f);
				}

				// Outer bumps: 14 smaller clusters creating the broccoli silhouette
				for (int32 Ci = 0; Ci < 14; ++Ci)
				{
					float Ang = CR.FRandRange(0.f, 6.28f);
					float Dist = CanopyRadius * CR.FRandRange(0.5f, 1.0f);
					float ZOff = CR.FRandRange(-CanopyHeight * 0.4f, CanopyHeight * 0.45f);
					float Sz = CR.FRandRange(2.2f, 3.8f);

					// Sun-facing clusters are brighter (positive X direction = sun)
					float SunFactor = FMath::Max(0.f, FMath::Cos(Ang)) * 0.6f;
					FLinearColor LC = FMath::Lerp(LeafMid, LeafSunlit, SunFactor);
					LC.R += CR.FRandRange(-0.02f, 0.02f);
					LC.G += CR.FRandRange(-0.03f, 0.03f);

					SP(SphereMesh,
						FVector(BX + FMath::Cos(Ang) * Dist, BY + FMath::Sin(Ang) * Dist, CanopyCenterZ + ZOff),
						FVector(Sz, Sz, Sz * 0.65f),
						LC, 0.72f);
				}

				// Top crown: 4 small clusters poking up above
				for (int32 Ci = 0; Ci < 4; ++Ci)
				{
					float Ang = CR.FRandRange(0.f, 6.28f);
					float Dist = CR.FRandRange(30.f, 200.f);
					float Sz = CR.FRandRange(1.8f, 2.8f);

					SP(SphereMesh,
						FVector(BX + FMath::Cos(Ang) * Dist, BY + FMath::Sin(Ang) * Dist,
								CanopyCenterZ + CanopyHeight * 0.4f + CR.FRandRange(50.f, 200.f)),
						FVector(Sz, Sz, Sz * 0.6f),
						FMath::Lerp(LeafBright, LeafSunlit, CR.FRand()), 0.70f);
				}

				// Bottom fringe: 4 clusters hanging slightly below canopy center
				for (int32 Ci = 0; Ci < 4; ++Ci)
				{
					float Ang = CR.FRandRange(0.f, 6.28f);
					float Dist = CanopyRadius * CR.FRandRange(0.4f, 0.8f);
					float Sz = CR.FRandRange(2.0f, 3.0f);

					SP(SphereMesh,
						FVector(BX + FMath::Cos(Ang) * Dist, BY + FMath::Sin(Ang) * Dist,
								CanopyCenterZ - CanopyHeight * 0.35f - CR.FRandRange(0.f, 100.f)),
						FVector(Sz, Sz, Sz * 0.55f),
						LeafDark, 0.74f);
				}

				SpawnedCount++;
			}
			} // end for loop

			UE_LOG(LogPlayhouseCity, Log, TEXT("PUBG Forest: spawned %d deciduous trees"), SpawnedCount);
		}
	}

	SpawnVisualPolish();
	SpawnNPCHelicopter();
	// SpawnNPCPlayers(); // Disabled for now — re-enable for testing

	// City is fully generated — dismiss the loading screen after a delay
	// to let the renderer settle (LOD streaming, materials compiling, etc.).
	if (UWorld* W = GetWorld())
	{
		APlayerController* PC = W->GetFirstPlayerController();
		if (APHCPlayerController* PHC_PC = Cast<APHCPlayerController>(PC))
		{
			FTimerHandle LoadDismissHandle;
			W->GetTimerManager().SetTimer(LoadDismissHandle, [PHC_PC]()
			{
				PHC_PC->HideLoadingScreen();
			}, 3.5f, false);
		}
	}
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

	const FLinearColor GrassColor(0.20f, 0.22f, 0.10f); // PUBG yellowy-green

	// TopZ: desired Z of the top surface.
	// Cube default is 100×100×100 centered at origin.
	// ScaleZ=0.02 → 2 UU thick, half-height=1.  Center at TopZ-3 so terrain
	// chunks (at Z=0) always render on top without z-fighting.
	auto SpawnGroundSlab = [&](float TopZ, const FVector& XYLoc,
		const FVector& XYScale, const TCHAR* Label) -> UMaterialInstanceDynamic*
	{
		const FVector Loc(XYLoc.X, XYLoc.Y, TopZ - 10.f); // well below terrain
		AActor* GA = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!GA) return nullptr;
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
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"), GrassColor);
			Mat->SetScalarParameterValue(TEXT("Roughness"), 0.9f);
			if (GrassNormalMap) Mat->SetTextureParameterValue(TEXT("NormalMap"), GrassNormalMap);
			if (GrassRoughnessMap) Mat->SetTextureParameterValue(TEXT("RoughnessMap"), GrassRoughnessMap);
			MC->SetMaterial(0, Mat);
		}
		GA->AddInstanceComponent(MC);
		return Mat;
	};

	// Main city ground: doubled map — covers ±126,000 UU.
	// Coastline zone starts at 126k where ocean ramps begin.
	GroundSlabMID = SpawnGroundSlab(0.f, FVector(0.f, 0.f, 0.f), FVector(2520.f, 2520.f, 1.f), TEXT("CityGround"));

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
				Mat->SetScalarParameterValue(TEXT("Opacity"), 0.70f);
				Mat->SetScalarParameterValue(TEXT("Metallic"), 0.15f);
				Mat->SetScalarParameterValue(TEXT("Roughness"), 0.08f);
				Mat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.04f, 0.08f, 0.14f));
				MC->SetMaterial(0, Mat);
				WaterMIDs.Add(Mat);  // Store for trip sky swap
			}
		}
		else
		{
			UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
			if (Mat)
			{
				Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
				Mat->SetScalarParameterValue(TEXT("Roughness"), bIsWater ? 0.02f : 0.85f);
				if (!bIsWater && SandNormalMap) Mat->SetTextureParameterValue(TEXT("NormalMap"), SandNormalMap);
				if (!bIsWater && SandRoughnessMap) Mat->SetTextureParameterValue(TEXT("RoughnessMap"), SandRoughnessMap);
				MC->SetMaterial(0, Mat);
			}
		}
		WA->AddInstanceComponent(MC);
	};

	// PUBG-style water colors
	const FLinearColor DeepOcean(0.03f, 0.10f, 0.22f);
	const FLinearColor ShallowWater(0.06f, 0.18f, 0.30f);
	const FLinearColor Sand(0.76f, 0.70f, 0.55f);

	// Global ocean floor — massive, covers everything below land
	SpawnWater(FVector(0.f, 0.f, -8.f), FVector(4000.f, 4000.f, 1.f), DeepOcean, TEXT("OceanFloor"), true);

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

			// ---- PUBG-quality shadows ----
			SunLightComp->DynamicShadowCascades = 4;
			SunLightComp->CascadeDistributionExponent = 3.0f;
			SunLightComp->ContactShadowLength = 0.03f;
			SunLightComp->DynamicShadowDistanceMovableLight = 30000.f;
			SunLightComp->ShadowBias = 0.5f;
			SunLightComp->ShadowSlopeBias = 0.5f;
			SunLightComp->ShadowSharpen = 0.0f; // fully soft penumbra shadows

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
			// PUBG-style atmospheric distance haze
			FogComp->SetFogDensity(0.001f);         // denser for visible distance haze
			FogComp->SetFogHeightFalloff(0.04f);     // slower falloff = haze at all heights
			FogComp->SetFogMaxOpacity(0.6f);          // visible distance fog
			FogComp->SetStartDistance(3000.f);         // starts closer
			FogComp->SetVolumetricFog(true);
			FogComp->SetVolumetricFogScatteringDistribution(0.2f);
			FogComp->SetVolumetricFogExtinctionScale(0.5f);
			FogComp->SetVolumetricFogDistance(6000.f);
			// Warm golden inscattering (visible sun shafts through fog)
			FogComp->SetDirectionalInscatteringColor(FLinearColor(0.9f, 0.7f, 0.4f));
			FogComp->SetDirectionalInscatteringExponent(6.f);  // wider scatter cone
			FogComp->SetDirectionalInscatteringStartDistance(5000.f);
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

			// === PUBG-quality post-process pipeline ===

			// Ambient Occlusion — visible but not crushing
			PP.bOverride_AmbientOcclusionIntensity = true;
			PP.AmbientOcclusionIntensity = 0.5f;  // reduced from 0.8 for better shadow visibility
			PP.bOverride_AmbientOcclusionRadius = true;
			PP.AmbientOcclusionRadius = 200.f;
			PP.bOverride_AmbientOcclusionQuality = true;
			PP.AmbientOcclusionQuality = 60.f;
			PP.bOverride_AmbientOcclusionMipBlend = true;
			PP.AmbientOcclusionMipBlend = 0.6f;

			// Vignette — lighter
			PP.bOverride_VignetteIntensity = true;
			PP.VignetteIntensity = 0.15f;  // reduced from 0.3

			// Auto Exposure — brighter minimum, faster adaptation for visibility
			PP.bOverride_AutoExposureMinBrightness = true;
			PP.AutoExposureMinBrightness = 0.6f;  // up from 0.3 — shadows never get too dark
			PP.bOverride_AutoExposureMaxBrightness = true;
			PP.AutoExposureMaxBrightness = 3.0f;   // tighter max prevents washed out
			PP.bOverride_AutoExposureSpeedUp = true;
			PP.AutoExposureSpeedUp = 2.5f;   // faster adaptation
			PP.bOverride_AutoExposureSpeedDown = true;
			PP.AutoExposureSpeedDown = 1.5f;

			// Color Grading — neutral, no desaturation
			PP.bOverride_ColorGamma = true;
			PP.ColorGamma = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
			PP.bOverride_ColorSaturation = true;
			PP.ColorSaturation = FVector4(1.0f, 1.0f, 1.0f, 1.0f); // full saturation, no desaturation

			// Filmic Tone Mapping — lifted shadows for visibility
			PP.bOverride_FilmSlope = true;
			PP.FilmSlope = 0.88f;   // up from 0.82 — brighter midtones
			PP.bOverride_FilmToe = true;
			PP.FilmToe = 0.45f;     // down from 0.6 — less crushed shadows
			PP.bOverride_FilmShoulder = true;
			PP.FilmShoulder = 0.26f; // up from 0.22 — smoother highlight rolloff
			PP.bOverride_FilmBlackClip = true;
			PP.FilmBlackClip = 0.0f; // down from 0.01 — no black crush at all
			PP.bOverride_FilmWhiteClip = true;
			PP.FilmWhiteClip = 0.04f;

			// Screen-space reflections (mobile-friendly, wet roads/windows)
			PP.bOverride_ScreenSpaceReflectionIntensity = true;
			PP.ScreenSpaceReflectionIntensity = 50.f;
			PP.bOverride_ScreenSpaceReflectionQuality = true;
			PP.ScreenSpaceReflectionQuality = 25.f;
			PP.bOverride_ScreenSpaceReflectionMaxRoughness = true;
			PP.ScreenSpaceReflectionMaxRoughness = 0.35f;

			// Subtle motion blur (PUBG uses light motion blur)
			PP.bOverride_MotionBlurAmount = true;
			PP.MotionBlurAmount = 0.12f;
			PP.bOverride_MotionBlurMax = true;
			PP.MotionBlurMax = 2.f;

			// Chromatic Aberration — very subtle for realism
			PP.bOverride_SceneFringeIntensity = true;
			PP.SceneFringeIntensity = 0.15f;

			// Film Grain — subtle texture
			PP.bOverride_FilmGrainIntensity = true;
			PP.FilmGrainIntensity = 0.03f;

			// Depth of Field — shallow, cinematic
			PP.bOverride_DepthOfFieldFocalDistance = true;
			PP.DepthOfFieldFocalDistance = 5000.f;
			PP.bOverride_DepthOfFieldFstop = true;
			PP.DepthOfFieldFstop = 16.f;
			PP.bOverride_DepthOfFieldSensorWidth = true;
			PP.DepthOfFieldSensorWidth = 24.576f;

			// PUBG 2026: perfectly neutral gain, subtle cool shadow shift
			PP.bOverride_ColorGain = true;
			PP.ColorGain = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
			PP.bOverride_ColorOffset = true;
			PP.ColorOffset = FVector4(0.0f, 0.0f, 0.004f, 0.0f); // tiny blue in shadows
		}
	}
}

// ---------------------------------------------------------------------------
// City grid -- roads, sidewalks, and buildings (Brookhaven suburban style)
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnCityGrid()
{
	// ---------------------------------------------------------------
	// 1. Charlotte Uptown — no landmark table needed (SpawnSkyscrapers
	//    handles all named buildings individually).
	// ---------------------------------------------------------------
	const int32 NumBlocks = NumRoads - 1; // 24

	// ---------------------------------------------------------------
	// 2. District classification — Charlotte's Four Wards
	// ---------------------------------------------------------------
	// Charlotte Uptown blocks: X roads 6-17, Y roads 7-20
	// Trade/Tryon intersection at road (12,12)
	auto GetDistrict = [](int32 bx, int32 by) -> ECityDistrict
	{
		// Charlotte Uptown core — all buildings placed by SpawnSkyscrapers
		if (bx >= 6 && bx <= 17 && by >= 7 && by <= 20)
		{
			return ECityDistrict::DowntownCore;
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

			ECityDistrict District = GetDistrict(bx, by);

			switch (District)
			{
			// =======================================================
			// CHARLOTTE UPTOWN — skip, all buildings placed by
			// SpawnSkyscrapers() with exact real-world positions
			// =======================================================
			case ECityDistrict::DowntownCore:
				break;

			// =======================================================
			// RESIDENTIAL (surrounding Charlotte neighborhoods)
			// =======================================================
			case ECityDistrict::Residential:
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
	Body->SetBoundsScale(50.f);
	Body->RegisterComponent();
	House->SetRootComponent(Body);
	UMaterialInstanceDynamic* BodyMat = UMaterialInstanceDynamic::Create(Body->GetMaterial(0), Body);
	if (BodyMat)
	{
		BodyMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor);
		BodyMat->SetScalarParameterValue(TEXT("Roughness"), 0.92f);
		if (BrickNormalMap) BodyMat->SetTextureParameterValue(TEXT("NormalMap"), BrickNormalMap);
		if (BrickRoughnessMap) BodyMat->SetTextureParameterValue(TEXT("RoughnessMap"), BrickRoughnessMap);
		Body->SetMaterial(0, BodyMat);
	}
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

	// ---- Foundation baseboard (dark strip at building base) ----
	{
		const float BaseH = 20.f;
		const FLinearColor BaseBoardColor = WallColor * 0.6f;
		const FVector BaseboardLocs[] = {
			FVector(Loc.X, Loc.Y + D * 0.5f + 1.f, BaseH * 0.5f),
			FVector(Loc.X, Loc.Y - D * 0.5f - 1.f, BaseH * 0.5f),
			FVector(Loc.X + W * 0.5f + 1.f, Loc.Y, BaseH * 0.5f),
			FVector(Loc.X - W * 0.5f - 1.f, Loc.Y, BaseH * 0.5f),
		};
		const FVector BaseboardScales[] = {
			FVector(W / 100.f, 4.f / 100.f, BaseH / 100.f),
			FVector(W / 100.f, 4.f / 100.f, BaseH / 100.f),
			FVector(4.f / 100.f, D / 100.f, BaseH / 100.f),
			FVector(4.f / 100.f, D / 100.f, BaseH / 100.f),
		};
		for (int32 Bi = 0; Bi < 4; ++Bi)
		{
			FString BName = FString::Printf(TEXT("HouseBase_%d"), Bi);
			UStaticMeshComponent* Base = NewObject<UStaticMeshComponent>(House, *BName);
			Base->SetStaticMesh(CubeMesh);
			Base->SetWorldScale3D(BaseboardScales[Bi]);
			Base->SetWorldLocation(BaseboardLocs[Bi]);
			Base->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Base->RegisterComponent();
			ApplyPBR(Base, BaseBoardColor, 0.f, 0.8f);
			House->AddInstanceComponent(Base);
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

				// Window frame (white trim around ~30% of windows)
				if (DetailRandom.FRand() < 0.30f)
				{
					const float FrameT = 4.f; // frame thickness
					const FLinearColor FrameCol(0.85f, 0.85f, 0.82f);
					// Top strip
					FString FName = FString::Printf(TEXT("WF_%d_T"), WIdx - 1);
					UStaticMeshComponent* FTop = NewObject<UStaticMeshComponent>(House, *FName);
					FTop->SetStaticMesh(CubeMesh);
					FTop->SetWorldScale3D(FVector((WinW + FrameT * 2.f) / 100.f, 4.f / 100.f, FrameT / 100.f));
					FTop->SetWorldLocation(FVector(WX, FaceY, FloorZ + WinH * 0.5f + FrameT * 0.5f));
					FTop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					FTop->RegisterComponent();
					ApplyPBR(FTop, FrameCol, 0.f, 0.7f);
					House->AddInstanceComponent(FTop);
					// Bottom strip
					FName = FString::Printf(TEXT("WF_%d_B"), WIdx - 1);
					UStaticMeshComponent* FBot = NewObject<UStaticMeshComponent>(House, *FName);
					FBot->SetStaticMesh(CubeMesh);
					FBot->SetWorldScale3D(FVector((WinW + FrameT * 2.f) / 100.f, 5.f / 100.f, FrameT / 100.f));
					FBot->SetWorldLocation(FVector(WX, FaceY, FloorZ - WinH * 0.5f - FrameT * 0.5f));
					FBot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					FBot->RegisterComponent();
					ApplyPBR(FBot, FrameCol, 0.f, 0.7f);
					House->AddInstanceComponent(FBot);
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

		// Door frame (dark wood trim around door)
		{
			const float DoorW = 55.f;
			const float DoorH = 120.f;
			const float DoorZ = 60.f;
			const float FrameT = 5.f;
			const float DoorFaceY = Loc.Y + D * 0.5f + 3.5f;
			const FLinearColor DoorFrameCol(0.14f, 0.09f, 0.04f);

			// Left jamb
			UStaticMeshComponent* LJamb = NewObject<UStaticMeshComponent>(House, TEXT("DoorFrame_L"));
			LJamb->SetStaticMesh(CubeMesh);
			LJamb->SetWorldScale3D(FVector(FrameT / 100.f, 4.f / 100.f, DoorH / 100.f));
			LJamb->SetWorldLocation(FVector(Loc.X - DoorW * 0.5f - FrameT * 0.5f, DoorFaceY, DoorZ));
			LJamb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			LJamb->RegisterComponent();
			ApplyPBR(LJamb, DoorFrameCol, 0.f, 0.65f);
			House->AddInstanceComponent(LJamb);

			// Right jamb
			UStaticMeshComponent* RJamb = NewObject<UStaticMeshComponent>(House, TEXT("DoorFrame_R"));
			RJamb->SetStaticMesh(CubeMesh);
			RJamb->SetWorldScale3D(FVector(FrameT / 100.f, 4.f / 100.f, DoorH / 100.f));
			RJamb->SetWorldLocation(FVector(Loc.X + DoorW * 0.5f + FrameT * 0.5f, DoorFaceY, DoorZ));
			RJamb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			RJamb->RegisterComponent();
			ApplyPBR(RJamb, DoorFrameCol, 0.f, 0.65f);
			House->AddInstanceComponent(RJamb);

			// Header
			UStaticMeshComponent* Header = NewObject<UStaticMeshComponent>(House, TEXT("DoorFrame_H"));
			Header->SetStaticMesh(CubeMesh);
			Header->SetWorldScale3D(FVector((DoorW + FrameT * 2.f) / 100.f, 4.f / 100.f, FrameT / 100.f));
			Header->SetWorldLocation(FVector(Loc.X, DoorFaceY, DoorZ + DoorH * 0.5f + FrameT * 0.5f));
			Header->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Header->RegisterComponent();
			ApplyPBR(Header, DoorFrameCol, 0.f, 0.65f);
			House->AddInstanceComponent(Header);
		}

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
		if (LMat)
		{
			LMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.15f, 0.35f, 0.08f));
			LMat->SetScalarParameterValue(TEXT("Roughness"), 0.9f);
			if (GrassNormalMap) LMat->SetTextureParameterValue(TEXT("NormalMap"), GrassNormalMap);
			if (GrassRoughnessMap) LMat->SetTextureParameterValue(TEXT("RoughnessMap"), GrassRoughnessMap);
			Lawn->SetMaterial(0, LMat);
		}
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
	Body->SetBoundsScale(50.f);
	Body->RegisterComponent();
	Shop->SetRootComponent(Body);
	UMaterialInstanceDynamic* BodyMat = UMaterialInstanceDynamic::Create(Body->GetMaterial(0), Body);
	if (BodyMat)
	{
		BodyMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor);
		BodyMat->SetScalarParameterValue(TEXT("Roughness"), 0.55f);
		if (ConcreteNormalMap) BodyMat->SetTextureParameterValue(TEXT("NormalMap"), ConcreteNormalMap);
		if (ConcreteRoughnessMap) BodyMat->SetTextureParameterValue(TEXT("RoughnessMap"), ConcreteRoughnessMap);
		Body->SetMaterial(0, BodyMat);
	}
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
	Body->SetBoundsScale(50.f);
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
	Body->SetBoundsScale(50.f);
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
	if (GrassMat)
	{
		GrassMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.15f, 0.30f, 0.10f));
		if (GrassNormalMap) GrassMat->SetTextureParameterValue(TEXT("NormalMap"), GrassNormalMap);
		if (GrassRoughnessMap) GrassMat->SetTextureParameterValue(TEXT("RoughnessMap"), GrassRoughnessMap);
		Grass->SetMaterial(0, GrassMat);
	}
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
	MC->SetBoundsScale(50.f);
	MC->RegisterComponent();
	Building->SetRootComponent(MC);

	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
	if (Mat)
	{
		Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Mat->SetScalarParameterValue(TEXT("Roughness"), 0.55f);
		if (ConcreteNormalMap) Mat->SetTextureParameterValue(TEXT("NormalMap"), ConcreteNormalMap);
		if (ConcreteRoughnessMap) Mat->SetTextureParameterValue(TEXT("RoughnessMap"), ConcreteRoughnessMap);
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

	// ---- Floor division trim bands (PUBG-style building detail) ----
	if (Size.Z > 300.f && CubeMesh)
	{
		const FLinearColor TrimColor = Color * 1.15f; // slightly lighter than facade
		const float TrimH = 12.f;
		const float TrimDepth = 6.f;
		const float StoryH = FMath::Clamp(Size.Z / FMath::Max(1.f, FMath::Floor(Size.Z / 350.f)), 250.f, 400.f);

		for (float BZ = StoryH; BZ < Size.Z - 50.f; BZ += StoryH)
		{
			// Front and back face bands
			for (int32 Face = 0; Face < 2; ++Face)
			{
				float FY = Location.Y + (Face == 0 ? Size.Y * 0.5f + TrimDepth * 0.5f : -Size.Y * 0.5f - TrimDepth * 0.5f);
				UStaticMeshComponent* Band = NewObject<UStaticMeshComponent>(Building,
					*FString::Printf(TEXT("FloorBand_%d_%d"), (int32)BZ, Face));
				Band->SetStaticMesh(CubeMesh);
				Band->SetWorldScale3D(FVector((Size.X + TrimDepth * 2.f) / 100.f, TrimDepth / 100.f, TrimH / 100.f));
				Band->SetWorldLocation(FVector(Location.X, FY, BZ));
				Band->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Band->SetCastShadow(false);
				Band->RegisterComponent();
				Building->AddInstanceComponent(Band);
				if (ColorMaterial)
				{
					UMaterialInstanceDynamic* BM = UMaterialInstanceDynamic::Create(ColorMaterial, Band);
					if (BM)
					{
						BM->SetVectorParameterValue(TEXT("BaseColor"), TrimColor);
						BM->SetScalarParameterValue(TEXT("Roughness"), 0.4f);
						BM->SetScalarParameterValue(TEXT("Metallic"), 0.15f);
						Band->SetMaterial(0, BM);
					}
				}
			}
		}
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
		if (AsphaltNormalMap) Mat->SetTextureParameterValue(TEXT("NormalMap"), AsphaltNormalMap);
		if (AsphaltRoughnessMap) Mat->SetTextureParameterValue(TEXT("RoughnessMap"), AsphaltRoughnessMap);
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

	// ---- Road curbs (raised edges at road boundary) ----
	const float CurbH = 8.f;
	const float CurbW = 12.f;
	const float CurbZ = 2.f + CurbH * 0.5f; // top of road + half curb
	const FLinearColor CurbColor(0.50f, 0.48f, 0.44f); // light concrete

	auto SpawnCurb = [&](const FVector& CurbLoc, const FVector& CurbScale)
	{
		UStaticMeshComponent* Curb = NewObject<UStaticMeshComponent>(Road);
		Curb->SetStaticMesh(CubeMesh);
		Curb->SetWorldScale3D(CurbScale);
		Curb->SetWorldLocation(CurbLoc);
		Curb->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Curb->RegisterComponent();
		ApplyPBR(Curb, CurbColor, 0.f, 0.75f, FLinearColor::Black, ConcreteNormalMap, ConcreteRoughnessMap);
		Road->AddInstanceComponent(Curb);
	};

	if (bAlongX)
	{
		SpawnCurb(FVector(Center.X, Center.Y - RoadWid * 0.5f, CurbZ),
			FVector(RoadLen / 100.f, CurbW / 100.f, CurbH / 100.f));
		SpawnCurb(FVector(Center.X, Center.Y + RoadWid * 0.5f, CurbZ),
			FVector(RoadLen / 100.f, CurbW / 100.f, CurbH / 100.f));
	}
	else
	{
		SpawnCurb(FVector(Center.X - RoadWid * 0.5f, Center.Y, CurbZ),
			FVector(CurbW / 100.f, RoadLen / 100.f, CurbH / 100.f));
		SpawnCurb(FVector(Center.X + RoadWid * 0.5f, Center.Y, CurbZ),
			FVector(CurbW / 100.f, RoadLen / 100.f, CurbH / 100.f));
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
		if (ConcreteNormalMap) Mat->SetTextureParameterValue(TEXT("NormalMap"), ConcreteNormalMap);
		if (ConcreteRoughnessMap) Mat->SetTextureParameterValue(TEXT("RoughnessMap"), ConcreteRoughnessMap);
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
	SL->SetCastShadows(false);
	SL->MaxDrawDistance = 8000.f;  // only render when player is nearby
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

	// Consume random values to keep deterministic seed in sync
	// (19 mountains × 5 values + 4 far west × 5 + 80 trees × 3 = 335 random calls)
	for (int32 i = 0; i < 335; ++i) CityRandom.FRandRange(0.f, 1.f);

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Outskirts spawned (highway only)."));
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
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
			if (GrassNormalMap) Mat->SetTextureParameterValue(TEXT("NormalMap"), GrassNormalMap);
			if (GrassRoughnessMap) Mat->SetTextureParameterValue(TEXT("RoughnessMap"), GrassRoughnessMap);
			MC->SetMaterial(0, Mat);
		}
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

	// Southeast cluster
	SpawnHill(FVector(55000.f, -30000.f, 0.f), 3200.f, 2600.f, 450.f, GrassGreen, TEXT("Hill_SE1"));
	SpawnHill(FVector(60000.f, -25000.f, 0.f), 2500.f, 2200.f, 380.f, DryGrass, TEXT("Hill_SE2"));

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
// Motocross Dirt Bike Park — rolling hills designed for jumping
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnDirtBikePark()
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh) return;

	// Park center: X = -120000, Y = 20000
	// Terrain is generated by GetTerrainHeight() with park-specific Perlin noise.
	// No ramp geometry needed — the ProceduralMeshComponent terrain provides
	// natural rolling hills for the dirt bike to ride over.
	const float CX = -120000.f;
	const float CY = 20000.f;

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCCitySubsystem: Motocross dirt bike park spawned — natural terrain hills via GetTerrainHeight()."));
}

// ---------------------------------------------------------------------------
// Tropical Jungle — dense multi-layered jungle between city and pyramids
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnJungle()
{
	UWorld* W = GetWorld();
	if (!W || !CubeMesh || !CylinderMesh || !SphereMesh || !ConeMesh || !PlaneMesh || !ColorMaterial) return;

	// === JUNGLE BOUNDS — Zone B, far south coast (2x size) ===
	const float JMinX = -45000.f, JMaxX = 75000.f;    // 120,000 wide
	const float JMinY = -162000.f, JMaxY = -106000.f;  // 56,000 deep
	const float JCX = (JMinX + JMaxX) * 0.5f;         // 15000
	const float JCY = (JMinY + JMaxY) * 0.5f;         // -134000

	AActor* JA = W->SpawnActor<AActor>(AActor::StaticClass(), FVector(JCX, JCY, 0.f), FRotator::ZeroRotator);
	if (!JA) return;
#if WITH_EDITOR
	JA->SetActorLabel(TEXT("TropicalJungle"));
#endif

	int32 CI = 0;
	auto JC = [&](UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot,
		const FVector& S, const FLinearColor& Col, float Rough,
		float Metal = 0.f, bool bGlass = false, float Opacity = 0.3f) -> UStaticMeshComponent*
	{
		auto* C = NewObject<UStaticMeshComponent>(JA, *FString::Printf(TEXT("J_%d"), CI++));
		C->SetStaticMesh(Mesh);
		C->SetWorldLocation(Loc);
		C->SetWorldRotation(Rot);
		C->SetWorldScale3D(S);
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCastShadow(false);
		C->SetBoundsScale(10.f);
		if (bGlass && GlassMaterial)
			ApplyPBRGlass(C, GlassMaterial, Col, Opacity, Metal, Rough);
		else
		{
			UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, C);
			if (Mat)
			{
				Mat->SetVectorParameterValue(TEXT("BaseColor"), Col);
				Mat->SetScalarParameterValue(TEXT("Roughness"), Rough);
				if (Metal != 0.f) Mat->SetScalarParameterValue(TEXT("Metallic"), Metal);
				C->SetMaterial(0, Mat);
			}
		}
		if (CI == 1) { C->RegisterComponent(); JA->SetRootComponent(C); }
		else { C->AttachToComponent(JA->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform); C->RegisterComponent(); }
		JA->AddInstanceComponent(C);
		return C;
	};

	// === COLOR PALETTE (brightened for iOS filmic tone mapping) ===
	const FLinearColor FernBright(0.15f, 0.50f, 0.10f);
	const FLinearColor FernDark(0.08f, 0.30f, 0.06f);
	const FLinearColor BushGreen(0.10f, 0.38f, 0.08f);
	const FLinearColor LeafGreen(0.10f, 0.40f, 0.08f);
	const FLinearColor FlowerCol[] = {
		FLinearColor(0.55f, 0.12f, 0.65f), FLinearColor(0.85f, 0.10f, 0.15f),
		FLinearColor(0.95f, 0.55f, 0.08f), FLinearColor(0.90f, 0.25f, 0.50f),
	};
	const FLinearColor FruitCol[] = {
		FLinearColor(0.90f, 0.55f, 0.12f), FLinearColor(0.80f, 0.12f, 0.10f),
		FLinearColor(0.95f, 0.85f, 0.18f),
	};
	const FLinearColor BambooGreen(0.55f, 0.65f, 0.20f);
	const FLinearColor VineDark(0.06f, 0.25f, 0.04f);
	const FLinearColor PondTeal(0.05f, 0.16f, 0.25f); // PUBG blue-teal pond
	const FLinearColor PondBed(0.04f, 0.08f, 0.05f);
	const FLinearColor JungleFloor(0.12f, 0.10f, 0.06f);
	const FLinearColor MistGreen(0.08f, 0.15f, 0.08f);
	const FLinearColor PathDirt(0.25f, 0.18f, 0.10f);
	const FLinearColor RuinStone(0.45f, 0.42f, 0.35f);
	const FLinearColor RuinMoss(0.12f, 0.30f, 0.08f);
	const FLinearColor RuinDark(0.28f, 0.25f, 0.20f);
	const FLinearColor LogBrown(0.32f, 0.22f, 0.12f);
	const FLinearColor LogMoss(0.15f, 0.32f, 0.10f);

	// Leaf greens for tree crowns (brightened)
	const FLinearColor LeafColors[] = {
		FLinearColor(0.06f, 0.22f, 0.05f), FLinearColor(0.10f, 0.35f, 0.08f),
		FLinearColor(0.16f, 0.45f, 0.10f), FLinearColor(0.20f, 0.40f, 0.08f),
	};

	// === SPATIAL HELPERS ===
	const FVector PondCenter(15000.f, -134000.f, 0.f);
	const float ClearingRadius = 3000.f;

	const FVector PathPts[] = {
		{ -42000.f, -134000.f, 0.f }, { -32000.f, -128000.f, 0.f },
		{ -22000.f, -138000.f, 0.f }, { -12000.f, -130000.f, 0.f },
		{ -2000.f, -136000.f, 0.f },  { 8000.f, -126000.f, 0.f },
		{ 15000.f, -134000.f, 0.f },  { 25000.f, -142000.f, 0.f },
		{ 35000.f, -128000.f, 0.f },  { 45000.f, -138000.f, 0.f },
		{ 55000.f, -130000.f, 0.f },  { 65000.f, -136000.f, 0.f },
		{ 72000.f, -134000.f, 0.f },
	};
	const int32 NumPathSegs = 12;
	const float PathHW = 400.f;

	auto IsNearPath = [&](float X, float Y, float Thresh) -> bool
	{
		for (int32 i = 0; i < NumPathSegs; ++i)
		{
			FVector A(PathPts[i].X, PathPts[i].Y, 0.f);
			FVector B(PathPts[i+1].X, PathPts[i+1].Y, 0.f);
			FVector P(X, Y, 0.f);
			FVector AB = B - A;
			float t = FMath::Clamp(FVector::DotProduct(P - A, AB) / FVector::DotProduct(AB, AB), 0.f, 1.f);
			if (FVector::Dist2D(P, A + AB * t) < Thresh) return true;
		}
		return false;
	};
	auto IsInClearing = [&](float X, float Y) -> bool
	{
		return FVector2D::Distance(FVector2D(X, Y), FVector2D(PondCenter.X, PondCenter.Y)) < ClearingRadius;
	};

	// ================================================================
	// Realistic tree builder — palm-tree-style segmented trunks + branch fronds
	// ================================================================
	auto SpawnTree = [&](float TX, float TY, float Height, float BaseR, int32 TrunkSegs,
		int32 NumBranches, bool bButtress, bool bFruit)
	{
		float TZ = GetTerrainHeight(TX, TY);

		// --- Trunk: multi-segment with S-curve and taper (like SpawnPalmTree) ---
		float TopR = BaseR * 0.35f;
		float SegH = Height / TrunkSegs;
		float CurveAmp = Height * 0.010f;
		float CurveYaw = CityRandom.FRandRange(0.f, 360.f);
		float CurveYawR = FMath::DegreesToRadians(CurveYaw);

		FLinearColor TrunkBase(CityRandom.FRandRange(0.28f, 0.38f), CityRandom.FRandRange(0.18f, 0.24f), CityRandom.FRandRange(0.08f, 0.14f));
		FLinearColor TrunkTop(CityRandom.FRandRange(0.38f, 0.48f), CityRandom.FRandRange(0.28f, 0.36f), CityRandom.FRandRange(0.16f, 0.24f));
		FLinearColor RingScar(0.48f, 0.40f, 0.25f);

		FVector CrownPos(TX, TY, TZ + Height);

		for (int32 S = 0; S < TrunkSegs; ++S)
		{
			float T = (float)S / TrunkSegs;
			float R = FMath::Lerp(BaseR, TopR, T);
			if (S == 0) R *= 1.6f;  // wider flared base

			float COff = CurveAmp * FMath::Sin(T * PI);
			float SX = TX + COff * FMath::Cos(CurveYawR);
			float SY = TY + COff * FMath::Sin(CurveYawR);
			float SZ = TZ + S * SegH + SegH * 0.5f;

			float dC = CurveAmp * PI * FMath::Cos(T * PI) / Height;
			float TiltDeg = FMath::RadiansToDegrees(FMath::Atan(dC));

			JC(CylinderMesh, FVector(SX, SY, SZ),
				FRotator(TiltDeg * FMath::Cos(CurveYawR), CurveYaw, TiltDeg * FMath::Sin(CurveYawR)),
				FVector(R / 50.f, R / 50.f, SegH / 100.f),
				FMath::Lerp(TrunkBase, TrunkTop, T), 0.88f);

			// Ring scar every 2 segments
			if (S > 0 && S % 2 == 0)
			{
				JC(CylinderMesh, FVector(SX, SY, TZ + S * SegH),
					FRotator::ZeroRotator,
					FVector(R * 1.15f / 50.f, R * 1.15f / 50.f, 0.015f),
					RingScar, 0.75f);
			}

			if (S == TrunkSegs - 1)
			{
				float TopOff = CurveAmp * FMath::Sin(1.f * PI);
				CrownPos.X = TX + TopOff * FMath::Cos(CurveYawR);
				CrownPos.Y = TY + TopOff * FMath::Sin(CurveYawR);
				CrownPos.Z = TZ + Height;
			}
		}

		// --- Buttress roots (large, visible, PUBG-style) ---
		if (bButtress)
		{
			const FLinearColor RootColor = FMath::Lerp(TrunkBase, FLinearColor(0.10f, 0.14f, 0.05f), 0.35f);
			const FLinearColor MossColor(0.08f, 0.22f, 0.04f);

			// 4-5 large buttress roots radiating outward
			const int32 NumRoots = CityRandom.RandRange(4, 5);
			for (int32 B = 0; B < NumRoots; ++B)
			{
				float BY = B * (360.f / NumRoots) + CityRandom.FRandRange(-20.f, 20.f);
				float BR = FMath::DegreesToRadians(BY);
				float RootLen = BaseR * CityRandom.FRandRange(2.5f, 4.0f);
				float RootH = BaseR * CityRandom.FRandRange(1.2f, 2.0f);

				// Main buttress — tall flared wall
				JC(CubeMesh,
					FVector(TX + FMath::Cos(BR) * RootLen * 0.5f,
							TY + FMath::Sin(BR) * RootLen * 0.5f,
							TZ + RootH * 0.5f),
					FRotator(0.f, BY, -15.f),
					FVector(RootLen / 100.f, 0.08f, RootH / 100.f),
					RootColor, 0.92f);

				// Surface root extending further out (thin cylinder on ground)
				float SurfLen = BaseR * CityRandom.FRandRange(3.0f, 6.0f);
				float SurfR = BaseR * 0.08f;
				JC(CylinderMesh,
					FVector(TX + FMath::Cos(BR) * SurfLen * 0.5f,
							TY + FMath::Sin(BR) * SurfLen * 0.5f,
							TZ + SurfR * 0.3f),
					FRotator(0.f, BY, 85.f),
					FVector(SurfR / 50.f, SurfR / 50.f, SurfLen / 200.f),
					FMath::Lerp(TrunkBase, RootColor, 0.5f), 0.90f);
			}

			// Soil mound at base (flattened sphere)
			JC(SphereMesh,
				FVector(TX, TY, TZ - BaseR * 0.1f),
				FRotator::ZeroRotator,
				FVector(BaseR * 0.06f, BaseR * 0.06f, BaseR * 0.015f),
				FLinearColor(0.10f, 0.08f, 0.04f), 0.95f);

			// Moss/leaf litter clusters at base (4-6 small spheres)
			const int32 NumMoss = CityRandom.RandRange(4, 6);
			for (int32 M = 0; M < NumMoss; ++M)
			{
				float MA = CityRandom.FRandRange(0.f, 2.f * PI);
				float MDist = BaseR * CityRandom.FRandRange(1.0f, 3.0f);
				float MSize = CityRandom.FRandRange(0.15f, 0.4f);
				JC(SphereMesh,
					FVector(TX + FMath::Cos(MA) * MDist,
							TY + FMath::Sin(MA) * MDist,
							TZ + MSize * 10.f),
					FRotator::ZeroRotator,
					FVector(MSize, MSize, MSize * 0.3f),
					FMath::Lerp(MossColor, FLinearColor(0.14f, 0.12f, 0.06f), CityRandom.FRand()),
					0.92f);
			}
		}

		// --- Branches with leaf fronds (palm-tree style V-fold leaves) ---
		float FrondLen = Height * 0.22f;
		float FrondW = Height * 0.035f;
		float SpineThk = Height * 0.003f;
		float BladeThk = Height * 0.0012f;
		float GlobalYaw = CityRandom.FRandRange(0.f, 360.f);

		for (int32 BR = 0; BR < NumBranches; ++BR)
		{
			float BrYaw = GlobalYaw + (360.f / NumBranches) * BR + CityRandom.FRandRange(-12.f, 12.f);
			float BrPitch = CityRandom.FRandRange(15.f, 45.f);
			float BrLen = Height * CityRandom.FRandRange(0.15f, 0.25f);
			float BrR = BaseR * 0.15f;
			float BrYawR = FMath::DegreesToRadians(BrYaw);
			float BrPitchR = FMath::DegreesToRadians(BrPitch);

			FVector BrDir(FMath::Cos(BrPitchR) * FMath::Cos(BrYawR),
						  FMath::Cos(BrPitchR) * FMath::Sin(BrYawR),
						  FMath::Sin(BrPitchR));
			FVector BrCenter = CrownPos + BrDir * (BrLen * 0.5f);

			// Branch cylinder
			JC(CylinderMesh, BrCenter,
				FRotator(-(90.f - BrPitch), BrYaw, 0.f),
				FVector(BrR / 50.f, BrR / 50.f, BrLen / 100.f),
				TrunkTop, 0.85f);

			// Leaf frond at branch tip — rachis spine + 2 V-fold blades
			FVector FrondBase = CrownPos + BrDir * BrLen;
			float Droop = CityRandom.FRandRange(10.f, 35.f);
			FLinearColor LC = LeafColors[CityRandom.RandRange(0, 3)];
			FLinearColor SpineC(LC.R * 0.6f, LC.G * 0.7f, LC.B * 0.5f);

			float DroopR = FMath::DegreesToRadians(Droop);
			FVector FDir(FMath::Cos(DroopR) * FMath::Cos(BrYawR),
						 FMath::Cos(DroopR) * FMath::Sin(BrYawR),
						 -FMath::Sin(DroopR));
			FVector FCenter = FrondBase + FDir * (FrondLen * 0.5f);

			// Spine
			JC(CubeMesh, FCenter, FRotator(-Droop, BrYaw, 0.f),
				FVector(FrondLen / 100.f, SpineThk / 100.f, SpineThk / 100.f), SpineC, 0.55f);

			// Left blade
			FRotator LRot(-Droop, BrYaw, -18.f);
			FVector LRight = FRotationMatrix(FRotator(-Droop, BrYaw, 0.f)).GetUnitAxis(EAxis::Y);
			JC(CubeMesh, FCenter - LRight * (FrondW * 0.22f), LRot,
				FVector(FrondLen * 0.88f / 100.f, FrondW * 0.44f / 100.f, BladeThk / 100.f), LC, 0.55f);

			// Right blade
			JC(CubeMesh, FCenter + LRight * (FrondW * 0.22f), FRotator(-Droop, BrYaw, 18.f),
				FVector(FrondLen * 0.88f / 100.f, FrondW * 0.44f / 100.f, BladeThk / 100.f), LC, 0.55f);
		}

		// --- Fruit clusters ---
		if (bFruit)
		{
			int32 NF = CityRandom.RandRange(2, 5);
			for (int32 f = 0; f < NF; ++f)
			{
				float FYaw = CityRandom.FRandRange(0.f, 360.f);
				float FD = CityRandom.FRandRange(150.f, 400.f);
				JC(SphereMesh,
					FVector(CrownPos.X + FD * FMath::Cos(FMath::DegreesToRadians(FYaw)),
							CrownPos.Y + FD * FMath::Sin(FMath::DegreesToRadians(FYaw)),
							CrownPos.Z - CityRandom.FRandRange(100.f, 500.f)),
					FRotator::ZeroRotator, FVector(0.35f, 0.35f, 0.35f),
					FruitCol[CityRandom.RandRange(0, 2)], 0.65f);
			}
		}
	};

	// ================================================================
	// LAYER 0: Jungle floor
	// ================================================================
	{
		float FZ = GetTerrainHeight(JCX, JCY);
		JC(PlaneMesh, FVector(JCX, JCY, FZ + 2.f), FRotator::ZeroRotator,
			FVector(1200.f, 560.f, 1.f), JungleFloor, 0.95f);
	}

	// ================================================================
	// LAYER 1: Winding path
	// ================================================================
	for (int32 i = 0; i < NumPathSegs; ++i)
	{
		FVector A = PathPts[i], B = PathPts[i+1];
		FVector Mid = (A + B) * 0.5f;
		float Len = FVector::Dist2D(A, B);
		float Yaw = FMath::RadiansToDegrees(FMath::Atan2(B.Y - A.Y, B.X - A.X));
		float TZ = GetTerrainHeight(Mid.X, Mid.Y);
		JC(PlaneMesh, FVector(Mid.X, Mid.Y, TZ + 3.f), FRotator(0.f, Yaw, 0.f),
			FVector(Len / 100.f, PathHW * 2.f / 100.f, 1.f), PathDirt, 0.93f);
	}

	// ================================================================
	// LAYER 2: Canopy trees — emergent giants with full branch fronds
	// ================================================================
	TArray<FVector> CanopyPos;
	TArray<float> CanopyH;

	const float CanopyCell = 6500.f;
	const int32 CGX = FMath::CeilToInt((JMaxX - JMinX) / CanopyCell);
	const int32 CGY = FMath::CeilToInt((JMaxY - JMinY) / CanopyCell);

	for (int32 GX = 0; GX < CGX; ++GX)
	{
		for (int32 GY = 0; GY < CGY; ++GY)
		{
			float CX = JMinX + GX * CanopyCell + CityRandom.FRandRange(CanopyCell * 0.1f, CanopyCell * 0.9f);
			float CY = JMinY + GY * CanopyCell + CityRandom.FRandRange(CanopyCell * 0.1f, CanopyCell * 0.9f);
			if (CX < JMinX || CX > JMaxX || CY < JMinY || CY > JMaxY) continue;
			if (IsInClearing(CX, CY)) continue;
			if (IsNearPath(CX, CY, PathHW + 800.f)) continue;
			if (CityRandom.FRand() > 0.85f) continue;

			float TH = CityRandom.FRandRange(4500.f, 7500.f);
			CanopyPos.Add(FVector(CX, CY, GetTerrainHeight(CX, CY)));
			CanopyH.Add(TH);

			SpawnTree(CX, CY, TH, CityRandom.FRandRange(90.f, 140.f),
				6, CityRandom.RandRange(4, 6), true, CityRandom.FRand() < 0.20f);
		}
	}

	// ================================================================
	// LAYER 3: Mid-story trees — smaller, thinner, more branches
	// ================================================================
	const float MidCell = 4500.f;
	const int32 MGX = FMath::CeilToInt((JMaxX - JMinX) / MidCell);
	const int32 MGY = FMath::CeilToInt((JMaxY - JMinY) / MidCell);

	for (int32 GX = 0; GX < MGX; ++GX)
	{
		for (int32 GY = 0; GY < MGY; ++GY)
		{
			float MX = JMinX + GX * MidCell + CityRandom.FRandRange(MidCell * 0.1f, MidCell * 0.9f);
			float MY = JMinY + GY * MidCell + CityRandom.FRandRange(MidCell * 0.1f, MidCell * 0.9f);
			if (MX < JMinX || MX > JMaxX || MY < JMinY || MY > JMaxY) continue;
			if (IsInClearing(MX, MY)) continue;
			if (IsNearPath(MX, MY, PathHW + 300.f)) continue;
			if (CityRandom.FRand() > 0.75f) continue;

			float MH = CityRandom.FRandRange(2000.f, 4000.f);
			SpawnTree(MX, MY, MH, CityRandom.FRandRange(45.f, 75.f),
				4, CityRandom.RandRange(3, 5), false, CityRandom.FRand() < 0.30f);
		}
	}

	// ================================================================
	// LAYER 4: Understory vegetation (ferns, bushes, leaves, flowers)
	// ================================================================
	const float UCell = 3200.f;
	const int32 UGX = FMath::CeilToInt((JMaxX - JMinX) / UCell);
	const int32 UGY = FMath::CeilToInt((JMaxY - JMinY) / UCell);

	for (int32 GX = 0; GX < UGX; ++GX)
	{
		for (int32 GY = 0; GY < UGY; ++GY)
		{
			float UX = JMinX + GX * UCell + CityRandom.FRandRange(UCell * 0.15f, UCell * 0.85f);
			float UY = JMinY + GY * UCell + CityRandom.FRandRange(UCell * 0.15f, UCell * 0.85f);
			if (UX < JMinX || UX > JMaxX || UY < JMinY || UY > JMaxY) continue;
			if (IsNearPath(UX, UY, PathHW)) continue;
			if (CityRandom.FRand() > 0.70f) continue;

			float TZ = GetTerrainHeight(UX, UY);
			float Roll = CityRandom.FRand();

			if (Roll < 0.30f)
			{
				JC(ConeMesh, FVector(UX, UY, TZ + 80.f),
					FRotator(CityRandom.FRandRange(-8.f, 8.f), CityRandom.FRandRange(0.f, 360.f), 0.f),
					FVector(1.0f, 1.0f, 2.0f),
					CityRandom.FRand() > 0.5f ? FernBright : FernDark, 0.80f);
			}
			else if (Roll < 0.55f)
			{
				for (int32 bs = 0; bs < 2; ++bs)
				{
					float BSz = CityRandom.FRandRange(0.6f, 1.2f);
					JC(SphereMesh,
						FVector(UX + CityRandom.FRandRange(-40.f, 40.f),
								UY + CityRandom.FRandRange(-40.f, 40.f), TZ + BSz * 25.f),
						FRotator::ZeroRotator, FVector(BSz, BSz, BSz * 0.7f), BushGreen, 0.85f);
				}
			}
			else if (Roll < 0.80f)
			{
				JC(CubeMesh, FVector(UX, UY, TZ + CityRandom.FRandRange(50.f, 200.f)),
					FRotator(CityRandom.FRandRange(20.f, 60.f), CityRandom.FRandRange(0.f, 360.f), 0.f),
					FVector(2.0f, 1.2f, 0.02f), LeafGreen, 0.75f);
			}
			else
			{
				JC(CylinderMesh, FVector(UX, UY, TZ + 40.f), FRotator::ZeroRotator,
					FVector(0.08f, 0.08f, 0.8f), FernDark, 0.80f);
				JC(SphereMesh, FVector(UX, UY, TZ + 90.f), FRotator::ZeroRotator,
					FVector(0.35f, 0.35f, 0.25f), FlowerCol[CityRandom.RandRange(0, 3)], 0.65f);
			}
		}
	}

	// ================================================================
	// LAYER 5: Bamboo groves (28 clusters along edges)
	// ================================================================
	for (int32 b = 0; b < 40; ++b)
	{
		float BX = (b < 20) ? JMinX + CityRandom.FRandRange(0.f, 5000.f) : JMaxX - CityRandom.FRandRange(0.f, 5000.f);
		float BY = JMinY + CityRandom.FRandRange(1000.f, (JMaxY - JMinY) - 1000.f);
		float BTZ = GetTerrainHeight(BX, BY);
		int32 NS = CityRandom.RandRange(4, 6);
		for (int32 s = 0; s < NS; ++s)
		{
			float SX = BX + CityRandom.FRandRange(-150.f, 150.f);
			float SY = BY + CityRandom.FRandRange(-150.f, 150.f);
			float SH = CityRandom.FRandRange(1500.f, 3000.f);
			float SR = CityRandom.FRandRange(12.f, 22.f);
			JC(CylinderMesh, FVector(SX, SY, BTZ + SH * 0.5f),
				FRotator(CityRandom.FRandRange(-5.f, 5.f), CityRandom.FRandRange(0.f, 360.f), CityRandom.FRandRange(-3.f, 3.f)),
				FVector(SR / 50.f, SR / 50.f, SH / 100.f), BambooGreen, 0.60f, 0.05f);
		}
	}

	// ================================================================
	// LAYER 6: Hanging vines
	// ================================================================
	if (CanopyPos.Num() > 0)
	{
		int32 NV = FMath::Min(80, CanopyPos.Num() * 2);
		for (int32 v = 0; v < NV; ++v)
		{
			int32 TI = CityRandom.RandRange(0, CanopyPos.Num() - 1);
			FVector TP = CanopyPos[TI]; float TH = CanopyH[TI];
			float VYaw = CityRandom.FRandRange(0.f, 360.f);
			float VD = CityRandom.FRandRange(200.f, 800.f);
			float VLen = CityRandom.FRandRange(800.f, 2500.f);
			float VR = CityRandom.FRandRange(4.f, 8.f);
			JC(CylinderMesh,
				FVector(TP.X + VD * FMath::Cos(FMath::DegreesToRadians(VYaw)),
						TP.Y + VD * FMath::Sin(FMath::DegreesToRadians(VYaw)),
						TP.Z + TH - CityRandom.FRandRange(0.f, 500.f) - VLen * 0.5f),
				FRotator::ZeroRotator,
				FVector(VR / 50.f, VR / 50.f, VLen / 100.f), VineDark, 0.80f);
		}
	}

	// ================================================================
	// LAYER 7: Water — pond + stream
	// ================================================================
	{
		float PZ = GetTerrainHeight(PondCenter.X, PondCenter.Y);
		JC(PlaneMesh, FVector(PondCenter.X, PondCenter.Y, PZ + 1.f), FRotator::ZeroRotator,
			FVector(15.f, 12.f, 1.f), PondBed, 0.95f);
		JC(PlaneMesh, FVector(PondCenter.X, PondCenter.Y, PZ + 3.f), FRotator::ZeroRotator,
			FVector(14.f, 11.f, 1.f), PondTeal, 0.02f, 0.f, true, 0.75f);

		for (int32 ss = 1; ss <= 4; ++ss)
		{
			float SX = PondCenter.X + ss * 2500.f;
			float SY = PondCenter.Y + CityRandom.FRandRange(-1500.f, 1500.f);
			float SZ = GetTerrainHeight(SX, SY);
			JC(PlaneMesh, FVector(SX, SY, SZ + 1.f), FRotator(0.f, CityRandom.FRandRange(-15.f, 15.f), 0.f),
				FVector(2.5f, 14.f, 1.f), PondBed, 0.95f);
			JC(PlaneMesh, FVector(SX, SY, SZ + 2.f), FRotator(0.f, CityRandom.FRandRange(-15.f, 15.f), 0.f),
				FVector(2.f, 13.f, 1.f), PondTeal, 0.02f, 0.f, true, 0.70f);
		}

		for (int32 pr = 0; pr < 16; ++pr)
		{
			float PAng = pr * (360.f / 16.f) + CityRandom.FRandRange(-8.f, 8.f);
			float PD = CityRandom.FRandRange(1500.f, 2800.f);
			float PX = PondCenter.X + PD * FMath::Cos(FMath::DegreesToRadians(PAng));
			float PY = PondCenter.Y + PD * FMath::Sin(FMath::DegreesToRadians(PAng));
			float PTZ = GetTerrainHeight(PX, PY);
			if (CityRandom.FRand() < 0.5f)
				JC(ConeMesh, FVector(PX, PY, PTZ + 80.f),
					FRotator(CityRandom.FRandRange(-5.f, 5.f), CityRandom.FRandRange(0.f, 360.f), 0.f),
					FVector(1.2f, 1.2f, 2.5f), FernBright, 0.80f);
			else
			{
				float BSz = CityRandom.FRandRange(0.8f, 1.5f);
				JC(SphereMesh, FVector(PX, PY, PTZ + BSz * 25.f),
					FRotator::ZeroRotator, FVector(BSz, BSz, BSz * 0.7f), BushGreen, 0.85f);
			}
		}
	}

	// ================================================================
	// LAYER 8: Fallen logs
	// ================================================================
	for (int32 L = 0; L < 35; ++L)
	{
		float LX = CityRandom.FRandRange(JMinX + 3000.f, JMaxX - 3000.f);
		float LY = CityRandom.FRandRange(JMinY + 3000.f, JMaxY - 3000.f);
		if (IsInClearing(LX, LY)) continue;
		float LTZ = GetTerrainHeight(LX, LY);
		float LogLen = CityRandom.FRandRange(600.f, 1400.f);
		float LogR = CityRandom.FRandRange(30.f, 70.f);
		JC(CylinderMesh, FVector(LX, LY, LTZ + LogR),
			FRotator(90.f, CityRandom.FRandRange(0.f, 360.f), 0.f),
			FVector(LogR / 50.f, LogR / 50.f, LogLen / 100.f), LogBrown, 0.90f);
		JC(SphereMesh,
			FVector(LX + CityRandom.FRandRange(-80.f, 80.f), LY + CityRandom.FRandRange(-80.f, 80.f), LTZ + LogR * 1.8f),
			FRotator::ZeroRotator, FVector(LogR * 0.025f, LogR * 0.025f, LogR * 0.012f), LogMoss, 0.88f);
	}

	// ================================================================
	// LAYER 9: Stone ruins (4 structures)
	// ================================================================
	{
		const FVector R1(-25000.f, -120000.f, 0.f);
		float RZ = GetTerrainHeight(R1.X, R1.Y);
		for (int32 p = 0; p < 2; ++p)
		{
			float PX = R1.X + (p == 0 ? -250.f : 250.f);
			float PH = CityRandom.FRandRange(600.f, 900.f);
			JC(CylinderMesh, FVector(PX, R1.Y, RZ + PH * 0.5f),
				FRotator(CityRandom.FRandRange(-3.f, 3.f), 0.f, CityRandom.FRandRange(-3.f, 3.f)),
				FVector(0.8f, 0.8f, PH / 100.f), RuinStone, 0.92f);
			JC(SphereMesh, FVector(PX, R1.Y, RZ + PH * 0.3f), FRotator::ZeroRotator,
				FVector(0.9f, 0.9f, 0.4f), RuinMoss, 0.88f);
		}
		JC(CubeMesh, FVector(R1.X, R1.Y, RZ + 50.f), FRotator(8.f, 5.f, 15.f),
			FVector(5.5f, 0.8f, 0.6f), RuinDark, 0.92f);
		for (int32 sb = 0; sb < 4; ++sb)
		{
			float BSz = CityRandom.FRandRange(40.f, 120.f);
			JC(CubeMesh,
				FVector(R1.X + CityRandom.FRandRange(-400.f, 400.f), R1.Y + CityRandom.FRandRange(-400.f, 400.f), RZ + BSz * 0.3f),
				FRotator(CityRandom.FRandRange(-15.f, 15.f), CityRandom.FRandRange(0.f, 45.f), CityRandom.FRandRange(-10.f, 10.f)),
				FVector(BSz / 100.f, BSz * 0.8f / 100.f, BSz * 0.6f / 100.f),
				FMath::Lerp(RuinStone, RuinMoss, CityRandom.FRandRange(0.f, 0.4f)), 0.92f);
		}
	}
	{
		const FVector R2(55000.f, -150000.f, 0.f);
		float RZ = GetTerrainHeight(R2.X, R2.Y);
		float PW[] = { 600.f, 450.f, 300.f };
		for (int32 step = 0; step < 3; ++step)
			JC(CubeMesh, FVector(R2.X, R2.Y, RZ + 80.f * step + 40.f), FRotator::ZeroRotator,
				FVector(PW[step] / 100.f, PW[step] / 100.f, 0.8f),
				FMath::Lerp(RuinStone, RuinDark, step * 0.15f), 0.92f);
		JC(CubeMesh, FVector(R2.X, R2.Y, RZ + 280.f), FRotator(0.f, 30.f, 0.f),
			FVector(1.2f, 0.6f, 0.5f), RuinDark, 0.90f);
		for (int32 ms = 0; ms < 3; ++ms)
			JC(SphereMesh,
				FVector(R2.X + CityRandom.FRandRange(-300.f, 300.f), R2.Y + CityRandom.FRandRange(-300.f, 300.f), RZ + CityRandom.FRandRange(20.f, 200.f)),
				FRotator::ZeroRotator,
				FVector(CityRandom.FRandRange(0.8f, 1.5f), CityRandom.FRandRange(0.8f, 1.5f), CityRandom.FRandRange(0.3f, 0.6f)), RuinMoss, 0.88f);
	}
	{
		const FVector R3(-10000.f, -112000.f, 0.f);
		float RZ = GetTerrainHeight(R3.X, R3.Y);
		JC(CubeMesh, FVector(R3.X, R3.Y, RZ + 250.f), FRotator(0.f, 25.f, 3.f),
			FVector(8.f, 0.4f, 5.f), RuinStone, 0.92f);
		JC(CubeMesh, FVector(R3.X + 300.f, R3.Y, RZ + 450.f), FRotator(10.f, 25.f, -8.f),
			FVector(3.f, 0.4f, 2.f), RuinDark, 0.92f);
		for (int32 wv = 0; wv < 2; ++wv)
			JC(CylinderMesh,
				FVector(R3.X + CityRandom.FRandRange(-200.f, 200.f), R3.Y + 20.f, RZ + CityRandom.FRandRange(100.f, 400.f)),
				FRotator(CityRandom.FRandRange(-20.f, 20.f), 25.f, 0.f),
				FVector(0.06f, 0.06f, CityRandom.FRandRange(2.f, 4.f)), VineDark, 0.80f);
		JC(SphereMesh, FVector(R3.X - 100.f, R3.Y, RZ + 150.f), FRotator::ZeroRotator,
			FVector(1.5f, 0.5f, 1.0f), RuinMoss, 0.88f);
	}
	{
		const FVector R4(60000.f, -145000.f, 0.f);
		float RZ = GetTerrainHeight(R4.X, R4.Y);
		for (int32 p = 0; p < 3; ++p)
		{
			float PH = CityRandom.FRandRange(400.f, 700.f);
			float PX = R4.X + (p - 1) * 350.f;
			JC(CylinderMesh, FVector(PX, R4.Y, RZ + PH * 0.5f),
				FRotator(CityRandom.FRandRange(-5.f, 5.f), 0.f, CityRandom.FRandRange(-5.f, 5.f)),
				FVector(0.7f, 0.7f, PH / 100.f), RuinStone, 0.92f);
		}
		JC(CubeMesh, FVector(R4.X, R4.Y, RZ + 30.f), FRotator::ZeroRotator,
			FVector(5.f, 5.f, 0.3f), RuinDark, 0.92f);
		for (int32 ms = 0; ms < 4; ++ms)
			JC(SphereMesh,
				FVector(R4.X + CityRandom.FRandRange(-500.f, 500.f), R4.Y + CityRandom.FRandRange(-500.f, 500.f), RZ + CityRandom.FRandRange(10.f, 100.f)),
				FRotator::ZeroRotator, FVector(CityRandom.FRandRange(0.6f, 1.2f), CityRandom.FRandRange(0.6f, 1.2f), 0.4f), RuinMoss, 0.88f);
	}

	// ================================================================
	// LAYER 10: Atmospheric mist (16 planes)
	// ================================================================
	for (int32 m = 0; m < 16; ++m)
	{
		float MX = CityRandom.FRandRange(JMinX + 5000.f, JMaxX - 5000.f);
		float MY = CityRandom.FRandRange(JMinY + 3000.f, JMaxY - 3000.f);
		float MZ = CityRandom.FRandRange(500.f, 1500.f);
		float MTZ = GetTerrainHeight(MX, MY);
		JC(PlaneMesh, FVector(MX, MY, MTZ + MZ), FRotator::ZeroRotator,
			FVector(CityRandom.FRandRange(30.f, 60.f), CityRandom.FRandRange(30.f, 60.f), 1.f),
			MistGreen, 0.0f, 0.f, true, CityRandom.FRandRange(0.03f, 0.06f));
	}

	// ================================================================
	// LAYER 11: Palm trees scattered through the jungle
	// ================================================================
	{
		const float PalmCell = 7000.f;
		const int32 PGX = FMath::CeilToInt((JMaxX - JMinX) / PalmCell);
		const int32 PGY = FMath::CeilToInt((JMaxY - JMinY) / PalmCell);

		for (int32 GX = 0; GX < PGX; ++GX)
		{
			for (int32 GY = 0; GY < PGY; ++GY)
			{
				float PX = JMinX + GX * PalmCell + CityRandom.FRandRange(PalmCell * 0.1f, PalmCell * 0.9f);
				float PY = JMinY + GY * PalmCell + CityRandom.FRandRange(PalmCell * 0.1f, PalmCell * 0.9f);
				if (PX < JMinX || PX > JMaxX || PY < JMinY || PY > JMaxY) continue;
				if (IsInClearing(PX, PY)) continue;
				if (IsNearPath(PX, PY, PathHW + 600.f)) continue;
				if (CityRandom.FRand() > 0.55f) continue;

				float PH = CityRandom.FRandRange(55.f, 85.f);
				SpawnPalmTree(FVector(PX, PY, 0.f), PH, CityRandom.FRand() < 0.6f);
			}
		}
	}

	// ================================================================
	// LAYER 12: Giant tree ferns (thin trunk + drooping frond crown)
	// ================================================================
	{
		const float FernCell = 5000.f;
		const int32 FGX = FMath::CeilToInt((JMaxX - JMinX) / FernCell);
		const int32 FGY = FMath::CeilToInt((JMaxY - JMinY) / FernCell);

		for (int32 GX = 0; GX < FGX; ++GX)
		{
			for (int32 GY = 0; GY < FGY; ++GY)
			{
				float FX = JMinX + GX * FernCell + CityRandom.FRandRange(FernCell * 0.15f, FernCell * 0.85f);
				float FY = JMinY + GY * FernCell + CityRandom.FRandRange(FernCell * 0.15f, FernCell * 0.85f);
				if (FX < JMinX || FX > JMaxX || FY < JMinY || FY > JMaxY) continue;
				if (IsNearPath(FX, FY, PathHW + 200.f)) continue;
				if (CityRandom.FRand() > 0.35f) continue;

				float FTZ = GetTerrainHeight(FX, FY);
				float FH = CityRandom.FRandRange(1200.f, 2200.f);
				float FR = CityRandom.FRandRange(15.f, 25.f);

				// Thin fibrous trunk
				JC(CylinderMesh, FVector(FX, FY, FTZ + FH * 0.5f),
					FRotator(CityRandom.FRandRange(-3.f, 3.f), 0.f, CityRandom.FRandRange(-3.f, 3.f)),
					FVector(FR / 50.f, FR / 50.f, FH / 100.f),
					FLinearColor(0.28f, 0.20f, 0.10f), 0.90f);

				// Crown of 6-8 drooping fronds
				int32 NF = CityRandom.RandRange(6, 8);
				float FYawBase = CityRandom.FRandRange(0.f, 360.f);
				for (int32 f = 0; f < NF; ++f)
				{
					float FYaw = FYawBase + (360.f / NF) * f + CityRandom.FRandRange(-10.f, 10.f);
					float Droop = CityRandom.FRandRange(25.f, 55.f);
					float FLen = FH * CityRandom.FRandRange(0.30f, 0.50f);
					float FYawR = FMath::DegreesToRadians(FYaw);
					float DroopR = FMath::DegreesToRadians(Droop);

					FVector FDir(FMath::Cos(DroopR) * FMath::Cos(FYawR),
								 FMath::Cos(DroopR) * FMath::Sin(FYawR),
								 -FMath::Sin(DroopR));
					FVector FCenter = FVector(FX, FY, FTZ + FH) + FDir * (FLen * 0.5f);

					FLinearColor FLC = CityRandom.FRand() > 0.5f ? FernBright : FernDark;

					// Rachis spine
					JC(CubeMesh, FCenter, FRotator(-Droop, FYaw, 0.f),
						FVector(FLen / 100.f, 0.03f, 0.03f), FLinearColor(FLC.R * 0.6f, FLC.G * 0.7f, FLC.B * 0.5f), 0.55f);

					// Two V-fold leaf blades
					FVector LRight = FRotationMatrix(FRotator(-Droop, FYaw, 0.f)).GetUnitAxis(EAxis::Y);
					float BladeW = FLen * 0.15f;
					JC(CubeMesh, FCenter - LRight * (BladeW * 0.25f),
						FRotator(-Droop, FYaw, -20.f),
						FVector(FLen * 0.9f / 100.f, BladeW * 0.5f / 100.f, 0.012f), FLC, 0.55f);
					JC(CubeMesh, FCenter + LRight * (BladeW * 0.25f),
						FRotator(-Droop, FYaw, 20.f),
						FVector(FLen * 0.9f / 100.f, BladeW * 0.5f / 100.f, 0.012f), FLC, 0.55f);
				}
			}
		}
	}

	// ================================================================
	// LAYER 13: Banana plants (thick trunk + large upright leaf blades)
	// ================================================================
	for (int32 bp = 0; bp < 55; ++bp)
	{
		float BPX = CityRandom.FRandRange(JMinX + 2000.f, JMaxX - 2000.f);
		float BPY = CityRandom.FRandRange(JMinY + 2000.f, JMaxY - 2000.f);
		if (IsNearPath(BPX, BPY, PathHW + 400.f)) continue;
		if (IsInClearing(BPX, BPY)) continue;

		float BTZ = GetTerrainHeight(BPX, BPY);
		float BH = CityRandom.FRandRange(800.f, 1400.f);

		// Thick green-brown trunk
		JC(CylinderMesh, FVector(BPX, BPY, BTZ + BH * 0.5f),
			FRotator(CityRandom.FRandRange(-2.f, 2.f), 0.f, CityRandom.FRandRange(-2.f, 2.f)),
			FVector(0.5f, 0.5f, BH / 100.f),
			FLinearColor(0.28f, 0.35f, 0.14f), 0.82f);

		// 4-6 large banana leaves radiating from top
		int32 NL = CityRandom.RandRange(4, 6);
		float BYawBase = CityRandom.FRandRange(0.f, 360.f);
		for (int32 l = 0; l < NL; ++l)
		{
			float LYaw = BYawBase + (360.f / NL) * l + CityRandom.FRandRange(-15.f, 15.f);
			float LPitch = CityRandom.FRandRange(15.f, 50.f);
			float LLen = BH * CityRandom.FRandRange(0.5f, 0.8f);

			JC(CubeMesh, FVector(BPX, BPY, BTZ + BH) +
				FRotationMatrix(FRotator(-LPitch, LYaw, 0.f)).GetUnitAxis(EAxis::X) * (LLen * 0.5f),
				FRotator(-LPitch, LYaw, 0.f),
				FVector(LLen / 100.f, 1.5f, 0.015f),
				FLinearColor(0.12f, 0.45f, 0.08f), 0.60f);
		}

		// Banana bunch hanging below crown
		if (CityRandom.FRand() < 0.40f)
		{
			float BunchYaw = CityRandom.FRandRange(0.f, 360.f);
			float BunchDist = 60.f;
			JC(SphereMesh,
				FVector(BPX + BunchDist * FMath::Cos(FMath::DegreesToRadians(BunchYaw)),
						BPY + BunchDist * FMath::Sin(FMath::DegreesToRadians(BunchYaw)),
						BTZ + BH - 100.f),
				FRotator::ZeroRotator, FVector(0.4f, 0.4f, 0.6f),
				FLinearColor(0.80f, 0.70f, 0.10f), 0.65f);
		}
	}

	// ================================================================
	// LAYER 14: Moss-covered boulders
	// ================================================================
	for (int32 mb = 0; mb < 45; ++mb)
	{
		float MBX = CityRandom.FRandRange(JMinX + 1000.f, JMaxX - 1000.f);
		float MBY = CityRandom.FRandRange(JMinY + 1000.f, JMaxY - 1000.f);
		if (IsNearPath(MBX, MBY, PathHW)) continue;

		float MBTZ = GetTerrainHeight(MBX, MBY);
		float BSz = CityRandom.FRandRange(60.f, 200.f);

		// Rock
		JC(SphereMesh, FVector(MBX, MBY, MBTZ + BSz * 0.35f),
			FRotator(CityRandom.FRandRange(-10.f, 10.f), CityRandom.FRandRange(0.f, 360.f), CityRandom.FRandRange(-10.f, 10.f)),
			FVector(BSz / 50.f, BSz * 0.8f / 50.f, BSz * 0.6f / 50.f),
			FLinearColor(0.25f, 0.23f, 0.20f), 0.92f);

		// Moss on top
		JC(SphereMesh, FVector(MBX, MBY, MBTZ + BSz * 0.55f),
			FRotator::ZeroRotator,
			FVector(BSz * 0.8f / 50.f, BSz * 0.6f / 50.f, BSz * 0.15f / 50.f),
			RuinMoss, 0.88f);
	}

	// ================================================================
	// LAYER 15: Mushroom clusters — collectible (added to CollectibleMushrooms)
	// ================================================================
	for (int32 mc = 0; mc < 40; ++mc)
	{
		float MCX = CityRandom.FRandRange(JMinX + 2000.f, JMaxX - 2000.f);
		float MCY = CityRandom.FRandRange(JMinY + 2000.f, JMaxY - 2000.f);
		if (IsNearPath(MCX, MCY, PathHW)) continue;

		float MCTZ = GetTerrainHeight(MCX, MCY);
		int32 NM = CityRandom.RandRange(3, 6);

		// Bioluminescent purple palette for mushroom caps
		FLinearColor MushroomCol[] = {
			FLinearColor(0.45f, 0.05f, 0.65f), FLinearColor(0.55f, 0.10f, 0.80f),
			FLinearColor(0.35f, 0.08f, 0.55f), FLinearColor(0.60f, 0.15f, 0.70f),
		};
		FLinearColor MCol = MushroomCol[CityRandom.RandRange(0, 3)];

		const FVector ClusterPos(MCX, MCY, MCTZ + 40.f);

		// Spawn a collectible actor for this cluster
		AActor* MA = W->SpawnActor<AActor>(AActor::StaticClass(),
			ClusterPos, FRotator::ZeroRotator);
		if (!MA) continue;
#if WITH_EDITOR
		MA->SetActorLabel(*FString::Printf(TEXT("JungleMushroom_%d"), mc));
#endif

		// First stem is root component
		UStaticMeshComponent* RootStem = nullptr;

		for (int32 mi = 0; mi < NM; ++mi)
		{
			float MX = CityRandom.FRandRange(-80.f, 80.f);
			float MY = CityRandom.FRandRange(-80.f, 80.f);
			float MH = CityRandom.FRandRange(25.f, 60.f);
			float MR = CityRandom.FRandRange(8.f, 20.f);

			// Stem
			auto* StemC = NewObject<UStaticMeshComponent>(MA,
				*FString::Printf(TEXT("JS_%d_%d"), mc, mi));
			StemC->SetStaticMesh(CylinderMesh);
			StemC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			StemC->SetRelativeLocation(FVector(MX, MY, MH * 0.5f));
			StemC->SetWorldScale3D(FVector(MR * 0.4f / 50.f, MR * 0.4f / 50.f, MH / 100.f));
			StemC->SetCastShadow(false);
			if (mi == 0)
			{
				MA->SetRootComponent(StemC);
				RootStem = StemC;
			}
			else if (RootStem)
			{
				StemC->AttachToComponent(RootStem, FAttachmentTransformRules::KeepRelativeTransform);
			}
			StemC->RegisterComponent();
			ApplyPBR(StemC, FLinearColor(0.70f, 0.65f, 0.75f), 0.f, 0.75f,
				FLinearColor(0.15f, 0.02f, 0.25f));

			// Cap
			auto* CapC = NewObject<UStaticMeshComponent>(MA,
				*FString::Printf(TEXT("JC_%d_%d"), mc, mi));
			CapC->SetStaticMesh(SphereMesh);
			CapC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			CapC->SetRelativeLocation(FVector(MX, MY, MH));
			CapC->SetWorldScale3D(FVector(MR / 50.f, MR / 50.f, MR * 0.4f / 50.f));
			CapC->SetCastShadow(false);
			if (RootStem)
				CapC->AttachToComponent(RootStem, FAttachmentTransformRules::KeepRelativeTransform);
			CapC->RegisterComponent();
			ApplyPBR(CapC, MCol, 0.f, 0.70f, MCol * 3.f);
		}

		// Point light — purple bioluminescent glow
		UPointLightComponent* PL = NewObject<UPointLightComponent>(MA,
			*FString::Printf(TEXT("JML_%d"), mc));
		PL->SetWorldLocation(ClusterPos + FVector(0.f, 0.f, 20.f));
		PL->SetIntensity(5000.f);
		PL->SetAttenuationRadius(600.f);
		PL->SetLightColor(FLinearColor(0.50f, 0.08f, 0.75f));
		PL->CastShadows = false;
		if (RootStem)
			PL->AttachToComponent(RootStem, FAttachmentTransformRules::KeepWorldTransform);
		PL->RegisterComponent();

		// Register as collectible
		FMushroomData MD;
		MD.Actor = MA;
		MD.GlowLight = PL;
		MD.OriginalSpawnPos = ClusterPos;
		MD.CurrentPos = ClusterPos;
		MD.BobPhase = CityRandom.FRand() * PI * 2.f;
		CollectibleMushrooms.Add(MD);
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("SpawnJungle: spawned %d components + palm trees"), CI);
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
		MC->SetBoundsScale(50.f);
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
		MC->SetBoundsScale(50.f);
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
		// In hangars (hangar 2 reserved for real SR-71 Blackbird pawn)
		{ FVector(HangarX, 5000.f,  AirportZ + 100.f),  90.f, FLinearColor(0.90f, 0.88f, 0.80f), DarkGold, 1.0f },
		{ FVector(HangarX, 48000.f, AirportZ + 100.f),  90.f, FLinearColor(0.85f, 0.85f, 0.88f), Gold, 1.0f },
	};

	for (int32 j = 0; j < 6; ++j)
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

	// ================================================================
	// SPAWN SR-71 BLACKBIRD in hangar 2 (discoverable by proximity + E key)
	// ================================================================
	if (!bAirportBlackbirdDestroyed)
	{
		{
			FActorSpawnParameters BSP;
			BSP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			const FVector BBLoc(HangarX, 25000.f, AirportZ + 100.f);
			const FRotator BBRot(0.f, 90.f, 0.f);
			AirportBlackbird = World->SpawnActor<APHCBlackbirdPawn>(
				APHCBlackbirdPawn::StaticClass(), BBLoc, BBRot, BSP);
			if (AirportBlackbird)
			{
				AirportBlackbird->SetBodyColor(FLinearColor(0.83f, 0.68f, 0.22f));
				AirportBlackbird->bIsOccupied = false;
				AirportBlackbird->CurrentSpeed = 0.f;
			}
		}
	}

	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCCitySubsystem: Underground luxury airport spawned (%d components)."), Idx);
}


// ---------------------------------------------------------------------------
// Respawn a killed NPC at its original spawn point
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::RespawnCrab(int32 Index)
{
	if (Index < 0 || Index >= NumGiantCrabs) return;
	FCrabData& CD = Crabs[Index];
	if (!CD.Actor) return;

	CD.Health = 100.f;
	CD.DamageFlashTimer = 0.f;
	CD.ContactCooldown = 0.f;
	CD.Actor->SetActorLocation(CD.SpawnPosition);
	CD.Actor->SetActorHiddenInGame(!bCrabsNightVisible);
	CD.bVisible = bCrabsNightVisible;

	// Reset glow light to normal
	if (CD.GlowLight)
	{
		CD.GlowLight->SetIntensity(CD.GlowBaseIntensity);
		CD.GlowLight->SetLightColor(FColor(255, 160, 30));
	}
}

// ---------------------------------------------------------------------------
// Giant Crab NPC — spawns hidden, revealed at night, roams the city
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnGiantCrab()
{
	UWorld* W = GetWorld();
	if (!W || !SphereMesh || !CylinderMesh || !CubeMesh || !ConeMesh || !ColorMaterial) return;

	// Spawn positions — scattered across the city
	const FVector CrabSpawns[NumGiantCrabs] = {
		{  5000,   5000, 0}, { -8000,  12000, 0}, { 20000, -15000, 0},
		{-18000, -20000, 0},
	};

	// --- Colors (Roblox "Noob" style) ---
	const FLinearColor SkinYellow(1.0f, 0.82f, 0.0f);
	const FLinearColor ShirtBlue(0.05f, 0.20f, 0.70f);
	const FLinearColor PantsGreen(0.16f, 0.40f, 0.08f);
	const FLinearColor EyeWhite(0.95f, 0.95f, 0.95f);
	const FLinearColor PupilBlack(0.02f, 0.02f, 0.02f);
	const FLinearColor MouthBlack(0.03f, 0.02f, 0.02f);
	const FLinearColor NoEmissive(0.f, 0.f, 0.f);

	int32 GCIdx = 0; // global sequential counter for unique component names

	for (int32 CI = 0; CI < NumGiantCrabs; ++CI)
	{
		FCrabData& CD = Crabs[CI];
		CD.HipJoints.Empty();
		CD.HipBaseRot.Empty();
		CD.KneeJoints.Empty();
		CD.KneeBaseRot.Empty();

		CD.SpawnPosition = CrabSpawns[CI];

		AActor* CrabActor = W->SpawnActor<AActor>(AActor::StaticClass(),
			CrabSpawns[CI], FRotator::ZeroRotator);
		if (!CrabActor) continue;
		CD.Actor = CrabActor;
		CrabActor->Tags.Add(FName("GiantCrab"));

#if WITH_EDITOR
		CrabActor->SetActorLabel(*FString::Printf(TEXT("GiantCrab_%d"), CI));
#endif

		// --- Root ---
		USceneComponent* Root = NewObject<USceneComponent>(CrabActor,
			*FString::Printf(TEXT("CR_%d"), GCIdx++));
		Root->SetRelativeScale3D(FVector(0.25f));
		Root->RegisterComponent();
		CrabActor->SetRootComponent(Root);
		CrabActor->AddInstanceComponent(Root);
		CrabActor->SetActorLocation(CrabSpawns[CI]);

		// --- Mesh helper — uses global counter for guaranteed unique names ---
		auto CM = [&](UStaticMesh* Mesh, USceneComponent* Parent, const FVector& Loc,
			const FRotator& Rot, const FVector& S, const FLinearColor& Col,
			float Metal, float Rough,
			const FLinearColor& Emis = FLinearColor(0.0f, 3.0f, 0.2f)) -> UStaticMeshComponent*
		{
			auto* C = NewObject<UStaticMeshComponent>(CrabActor,
				*FString::Printf(TEXT("CR_%d"), GCIdx++));
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
			CrabActor->AddInstanceComponent(C);
			CD.BodyMIDs.Add(Mat);
			return C;
		};

		auto MakeJoint = [&](USceneComponent* Parent, const FVector& Loc,
			const FRotator& Rot) -> USceneComponent*
		{
			auto* SC = NewObject<USceneComponent>(CrabActor,
				*FString::Printf(TEXT("CR_%d"), GCIdx++));
			SC->SetRelativeLocation(Loc);
			SC->SetRelativeRotation(Rot);
			SC->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform);
			SC->RegisterComponent();
			CrabActor->AddInstanceComponent(SC);
			return SC;
		};

		// ========== ROBLOX-STYLE CHARACTER (2.5x player height) ==========
		// Player is ~153 UU tall. 2.5x = ~383 UU. Root scale 0.25, so
		// local coords are 4x actual: ~1530 local units tall.
		// Character faces local +Y (matches rotation code).

		// --- HEAD (blocky cube) ---
		CM(CubeMesh, Root, FVector(0, 0, 1300), FRotator::ZeroRotator,
			FVector(4.2f, 4.2f, 4.2f), SkinYellow, 0.0f, 0.85f, NoEmissive);

		// Face — on +Y side of head
		// Left eye white
		CM(CubeMesh, Root, FVector(-70, 215, 1330), FRotator::ZeroRotator,
			FVector(0.9f, 0.12f, 0.9f), EyeWhite, 0.0f, 0.3f, NoEmissive);
		// Right eye white
		CM(CubeMesh, Root, FVector(70, 215, 1330), FRotator::ZeroRotator,
			FVector(0.9f, 0.12f, 0.9f), EyeWhite, 0.0f, 0.3f, NoEmissive);
		// Left pupil
		CM(CubeMesh, Root, FVector(-70, 220, 1330), FRotator::ZeroRotator,
			FVector(0.45f, 0.1f, 0.45f), PupilBlack, 0.0f, 0.2f, NoEmissive);
		// Right pupil
		CM(CubeMesh, Root, FVector(70, 220, 1330), FRotator::ZeroRotator,
			FVector(0.45f, 0.1f, 0.45f), PupilBlack, 0.0f, 0.2f, NoEmissive);
		// Smile
		CM(CubeMesh, Root, FVector(0, 220, 1230), FRotator::ZeroRotator,
			FVector(1.6f, 0.1f, 0.35f), MouthBlack, 0.0f, 0.3f, NoEmissive);

		// --- TORSO ---
		CM(CubeMesh, Root, FVector(0, 0, 870), FRotator::ZeroRotator,
			FVector(4.2f, 2.2f, 5.0f), ShirtBlue, 0.0f, 0.85f, NoEmissive);

		// --- LEGS (with hip joints for walk animation) ---
		for (int LG = 0; LG < 2; LG++)
		{
			float SX = (LG == 0) ? -105.f : 105.f;
			FVector HipPos(SX, 0, 620);
			FRotator HipBase(0.f, 0.f, 0.f);

			USceneComponent* Hip = MakeJoint(Root, HipPos, HipBase);
			CD.HipJoints.Add(Hip);
			CD.HipBaseRot.Add(HipBase);

			// Leg mesh hangs below hip joint
			CM(CubeMesh, Hip, FVector(0, 0, -275), FRotator::ZeroRotator,
				FVector(1.9f, 1.9f, 5.5f), PantsGreen, 0.0f, 0.85f, NoEmissive);
			// Foot
			CM(CubeMesh, Hip, FVector(0, 20, -560), FRotator::ZeroRotator,
				FVector(1.9f, 2.4f, 0.6f), SkinYellow, 0.0f, 0.85f, NoEmissive);
		}

		// --- ARMS (with shoulder joints for walk animation) ---
		for (int AR = 0; AR < 2; AR++)
		{
			float SX = (AR == 0) ? -310.f : 310.f;
			FVector ShoulderPos(SX, 0, 1100);
			FRotator ShoulderBase(0.f, 0.f, 0.f);

			USceneComponent* Shoulder = MakeJoint(Root, ShoulderPos, ShoulderBase);
			CD.KneeJoints.Add(Shoulder);
			CD.KneeBaseRot.Add(ShoulderBase);

			// Arm mesh hangs below shoulder joint
			CM(CubeMesh, Shoulder, FVector(0, 0, -250), FRotator::ZeroRotator,
				FVector(1.6f, 1.6f, 5.0f), SkinYellow, 0.0f, 0.85f, NoEmissive);
			// Hand
			CM(CubeMesh, Shoulder, FVector(0, 0, -510), FRotator::ZeroRotator,
				FVector(1.6f, 1.6f, 0.8f), SkinYellow, 0.0f, 0.85f, NoEmissive);
		}

		// --- Amber glow ---
		auto AddGlow = [&](const FVector& Loc, float Inten, float Rad, const FColor& Col)
		{
			auto* PL = NewObject<UPointLightComponent>(CrabActor,
				*FString::Printf(TEXT("CR_%d"), GCIdx++));
			PL->SetRelativeLocation(Loc);
			PL->SetIntensity(Inten);
			PL->SetAttenuationRadius(Rad);
			PL->SetLightColor(Col);
			PL->SetCastShadows(false);
			PL->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			PL->RegisterComponent();
			CrabActor->AddInstanceComponent(PL);
		};

		// Main glow light — store reference for damage flash
		{
			auto* PL = NewObject<UPointLightComponent>(CrabActor,
				*FString::Printf(TEXT("CR_%d"), GCIdx++));
			PL->SetRelativeLocation(FVector(0, 0, 500));
			PL->SetIntensity(4000.f);
			PL->SetAttenuationRadius(3500.f);
			PL->SetLightColor(FColor(255, 160, 30));
			PL->SetCastShadows(false);
			PL->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			PL->RegisterComponent();
			CrabActor->AddInstanceComponent(PL);
			CD.GlowLight = PL;
			CD.GlowBaseIntensity = 4000.f;
		}

		// Start hidden — revealed at night
		CrabActor->SetActorHiddenInGame(true);
		CD.bVisible = false;
	}
}

// ---------------------------------------------------------------------------
// Tornado funnel — 8 stacked spinning translucent cylinders
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnTornado()
{
	UWorld* W = GetWorld();
	if (!W || !CylinderMesh || !SphereMesh || !GlassMaterial || !ColorMaterial) return;

	TornadoActor = W->SpawnActor<AActor>(AActor::StaticClass(),
		FVector::ZeroVector, FRotator::ZeroRotator);
	if (!TornadoActor) return;

	USceneComponent* Root = NewObject<USceneComponent>(TornadoActor, TEXT("TornadoRoot"));
	Root->RegisterComponent();
	TornadoActor->SetRootComponent(Root);
	TornadoActor->AddInstanceComponent(Root);

	// ========== DARK STORM CLOUD MASS at top ==========
	// Multiple overlapping dark spheres to form a menacing cloud base
	const float CloudZ = 10000.f;
	const FLinearColor StormDark(0.06f, 0.06f, 0.08f);

	struct FCloudBlob { float X, Y, Z, SX, SY, SZ; float Opacity; };
	const FCloudBlob Blobs[] = {
		{     0,     0, CloudZ,         60, 50, 12, 0.70f },  // main mass
		{  1500,   800, CloudZ + 200,   45, 35, 10, 0.60f },  // right lobe
		{ -1200,  -600, CloudZ - 100,   40, 40,  8, 0.55f },  // left lobe
		{   500, -1000, CloudZ + 400,   35, 30,  9, 0.50f },  // rear
		{  -800,  1200, CloudZ + 100,   30, 25,  7, 0.45f },  // front
		{     0,     0, CloudZ - 400,   50, 45, 6,  0.65f },  // underbelly (darker)
	};

	for (int32 i = 0; i < UE_ARRAY_COUNT(Blobs); i++)
	{
		const auto& B = Blobs[i];
		auto* SC = NewObject<UStaticMeshComponent>(TornadoActor,
			FName(*FString::Printf(TEXT("StormCloud%d"), i)));
		SC->SetStaticMesh(SphereMesh);
		SC->SetRelativeLocation(FVector(B.X, B.Y, B.Z));
		SC->SetRelativeScale3D(FVector(B.SX, B.SY, B.SZ));
		SC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SC->SetCastShadow(false);

		auto* Mat = UMaterialInstanceDynamic::Create(GlassMaterial, SC);
		Mat->SetVectorParameterValue(TEXT("BaseColor"), StormDark);
		Mat->SetScalarParameterValue(TEXT("Opacity"), B.Opacity);
		Mat->SetScalarParameterValue(TEXT("Roughness"), 1.0f);
		SC->SetMaterial(0, Mat);

		SC->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		SC->RegisterComponent();
		TornadoActor->AddInstanceComponent(SC);
	}

	// ========== FUNNEL — descends from cloud to near ground ==========
	// 12 stacked cylinders: wide at cloud base, narrow at ground
	const int32 NumSegments = 12;
	const float FunnelTop = CloudZ - 600.f;    // just below cloud
	const float FunnelBottom = 200.f;           // near ground
	const float TotalHeight = FunnelTop - FunnelBottom;
	const float SegHeight = TotalHeight / NumSegments;
	const float TopRadius = 18.f;    // wide where it meets the cloud
	const float BottomRadius = 3.f;  // narrow tip near ground

	for (int32 i = 0; i < NumSegments; i++)
	{
		float Alpha = (float)i / (float)(NumSegments - 1);   // 0=top, 1=bottom
		float Radius = FMath::Lerp(TopRadius, BottomRadius, Alpha);
		float Z = FunnelTop - i * SegHeight;

		// Slight opacity variation — thicker in middle
		float MidFactor = 1.f - FMath::Abs(Alpha - 0.5f) * 2.f;
		float Opacity = 0.35f + MidFactor * 0.25f;

		auto* Cyl = NewObject<UStaticMeshComponent>(TornadoActor,
			FName(*FString::Printf(TEXT("FunnelSeg%d"), i)));
		Cyl->SetStaticMesh(CylinderMesh);
		Cyl->SetRelativeLocation(FVector(0, 0, Z));
		Cyl->SetRelativeScale3D(FVector(Radius, Radius, SegHeight / 100.f));
		Cyl->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cyl->SetCastShadow(false);

		auto* Mat = UMaterialInstanceDynamic::Create(GlassMaterial, Cyl);
		// Darker at top, slightly lighter gray-brown at bottom
		FLinearColor FunnelColor = FMath::Lerp(
			FLinearColor(0.08f, 0.08f, 0.10f),   // top (near-black)
			FLinearColor(0.20f, 0.18f, 0.14f),    // bottom (dusty)
			Alpha);
		Mat->SetVectorParameterValue(TEXT("BaseColor"), FunnelColor);
		Mat->SetScalarParameterValue(TEXT("Opacity"), Opacity);
		Mat->SetScalarParameterValue(TEXT("Roughness"), 0.95f);
		Cyl->SetMaterial(0, Mat);

		Cyl->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		Cyl->RegisterComponent();
		TornadoActor->AddInstanceComponent(Cyl);
	}

	// ========== DEBRIS CLOUD at base ==========
	// A few dusty spheres swirling near ground level
	for (int32 i = 0; i < 5; i++)
	{
		float Angle = i * 72.f;
		float Dist = 400.f + i * 100.f;
		auto* DS = NewObject<UStaticMeshComponent>(TornadoActor,
			FName(*FString::Printf(TEXT("Debris%d"), i)));
		DS->SetStaticMesh(SphereMesh);
		DS->SetRelativeLocation(FVector(
			FMath::Cos(FMath::DegreesToRadians(Angle)) * Dist,
			FMath::Sin(FMath::DegreesToRadians(Angle)) * Dist,
			150.f + i * 80.f));
		DS->SetRelativeScale3D(FVector(5.f + i, 4.f + i, 2.f + i * 0.5f));
		DS->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		DS->SetCastShadow(false);

		auto* Mat = UMaterialInstanceDynamic::Create(GlassMaterial, DS);
		Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.25f, 0.20f, 0.12f));
		Mat->SetScalarParameterValue(TEXT("Opacity"), 0.3f);
		Mat->SetScalarParameterValue(TEXT("Roughness"), 1.0f);
		DS->SetMaterial(0, Mat);

		DS->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		DS->RegisterComponent();
		TornadoActor->AddInstanceComponent(DS);
	}

	// ========== LIGHTING ==========
	// Eerie greenish glow under the cloud
	auto AddLight = [&](const FVector& Loc, float Inten, float Rad, const FColor& Col, const FName& Name)
	{
		auto* PL = NewObject<UPointLightComponent>(TornadoActor, Name);
		PL->SetRelativeLocation(Loc);
		PL->SetIntensity(Inten);
		PL->SetAttenuationRadius(Rad);
		PL->SetLightColor(Col);
		PL->SetCastShadows(false);
		PL->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		PL->RegisterComponent();
		TornadoActor->AddInstanceComponent(PL);
	};

	AddLight(FVector(0, 0, CloudZ - 800), 12000.f, 15000.f, FColor(120, 150, 100), TEXT("CloudGlow"));
	AddLight(FVector(0, 0, 500),           8000.f, 8000.f,  FColor(200, 180, 140), TEXT("BaseGlow"));

	TornadoActor->SetActorHiddenInGame(true);
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
		// Milky Way arc: azimuth centered at 120 degrees, spread +/-75
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
	if (!World || !SphereMesh || !ColorMaterial) return;

	FRandomStream CloudRand(333333);

	// Spawn 25 cumulus cloud clusters at varying positions/heights
	// Each cloud = 5-10 overlapping flattened spheres forming a puffy mass
	const int32 NumClouds = 50;
	int32 CloudCompIdx = 0;

	for (int32 Ci = 0; Ci < NumClouds; ++Ci)
	{
		float CX = CloudRand.FRandRange(-150000.f, 150000.f);
		float CY = CloudRand.FRandRange(-150000.f, 150000.f);
		float CZ = CloudRand.FRandRange(50000.f, 80000.f);

		AActor* CA = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(CX, CY, CZ), FRotator::ZeroRotator);
		if (!CA) continue;

		// Store first 3 for legacy refs + all for drift
		if (Ci == 0) CloudLayerActor = CA;
		else if (Ci == 1) CloudLayerActor2 = CA;
		else if (Ci == 2) CloudLayerActor3 = CA;
		CloudActors.Add(CA);

		int32 NumPuffs = CloudRand.RandRange(5, 10);
		float CloudWidth = CloudRand.FRandRange(4000.f, 12000.f);
		float CloudDepth = CloudWidth * CloudRand.FRandRange(0.5f, 0.9f);
		float CloudHeight = CloudWidth * CloudRand.FRandRange(0.15f, 0.3f);

		bool bFirstComp = true;
		for (int32 Pi = 0; Pi < NumPuffs; ++Pi)
		{
			float PX = CloudRand.FRandRange(-CloudWidth * 0.4f, CloudWidth * 0.4f);
			float PY = CloudRand.FRandRange(-CloudDepth * 0.4f, CloudDepth * 0.4f);
			float PZ = CloudRand.FRandRange(-CloudHeight * 0.3f, CloudHeight * 0.4f);
			float PSz = CloudRand.FRandRange(CloudWidth * 0.2f, CloudWidth * 0.45f);

			UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(CA,
				*FString::Printf(TEXT("Cloud_%d_%d"), Ci, CloudCompIdx++));
			MC->SetStaticMesh(SphereMesh);
			MC->SetWorldLocation(FVector(CX + PX, CY + PY, CZ + PZ));
			// Flat ellipsoid: wide, shallow
			MC->SetWorldScale3D(FVector(PSz / 50.f, PSz / 50.f, PSz * 0.35f / 50.f));
			MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MC->SetCastShadow(false);
			MC->SetBoundsScale(50.f);
			MC->bNeverDistanceCull = true;
			MC->RegisterComponent();

			if (bFirstComp) { CA->SetRootComponent(MC); bFirstComp = false; }
			CA->AddInstanceComponent(MC);

			// White cloud with emissive brightness
			// Top puffs brighter (sun-lit), bottom slightly darker (self-shadow)
			float VertBrightness = FMath::GetMappedRangeValueClamped(
				FVector2D(-CloudHeight * 0.3f, CloudHeight * 0.4f),
				FVector2D(0.5f, 1.0f), PZ);
			FLinearColor CloudCol(
				0.85f * VertBrightness,
				0.85f * VertBrightness,
				0.88f * VertBrightness);
			FLinearColor CloudEmis(
				0.3f * VertBrightness,
				0.3f * VertBrightness,
				0.32f * VertBrightness);

			UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, MC);
			if (Mat)
			{
				Mat->SetVectorParameterValue(TEXT("BaseColor"), CloudCol);
				Mat->SetScalarParameterValue(TEXT("Roughness"), 1.0f);
				Mat->SetVectorParameterValue(TEXT("EmissiveColor"), CloudEmis);
				MC->SetMaterial(0, Mat);
				CloudMIDs.Add(Mat);
			}
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("Spawned %d PUBG cumulus clouds with %d sphere puffs"), NumClouds, CloudCompIdx);
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

	// Skyscraper amplification — make towers visible from across the map
	constexpr float HSF = 3.0f;   // height scale factor
	constexpr float WSF = 2.45f;  // width/depth scale factor (increased for 50% more surface area)

	// ---------------------------------------------------------------
	// Helper lambdas
	// ---------------------------------------------------------------

	// Note: Size parameter is in UU; BasicShapes are 100 UU, so divide by 100 for scale.
	// HSF/WSF applied here so ALL skyscraper geometry scales uniformly.
	auto MakeCubeMC = [&](AActor* Owner, const FVector& Loc, const FVector& Size,
		const FLinearColor& Color, const TCHAR* Name,
		float Metallic = 0.f, float Roughness = 0.5f,
		const FLinearColor& Emissive = FLinearColor::Black) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, Name);
		MC->SetStaticMesh(CubeMesh);
		MC->SetWorldScale3D(FVector(Size.X * WSF, Size.Y * WSF, Size.Z * HSF) / 100.f);
		MC->SetWorldLocation(FVector(Loc.X, Loc.Y, Loc.Z * HSF));
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(50.f);
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
		MC->SetWorldScale3D(FVector(Size.X * WSF, Size.Y * WSF, Size.Z * HSF) / 100.f);
		MC->SetWorldLocation(FVector(Loc.X, Loc.Y, Loc.Z * HSF));
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(50.f);
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
		MC->SetWorldScale3D(FVector(Size.X * WSF, Size.Y * WSF, Size.Z * HSF) / 100.f);
		MC->SetWorldLocation(FVector(Loc.X, Loc.Y, Loc.Z * HSF));
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->SetBoundsScale(50.f);
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

		const int32 NumRows = FMath::Clamp(FMath::FloorToInt((TowerH - StartZ) / FloorH), 1, 30);

		// Two wider faces — use scaled offsets so windows sit on scaled tower surface.
		const float ScW = TowerW * WSF;
		const float ScD = TowerD * WSF;
		struct FWF { float OffX, OffY; bool bAlongX; float FW; };
		FWF Faces[2];
		if (TowerW >= TowerD)
		{
			Faces[0] = { 0.f,  ScD * 0.5f + 5.f, true, ScW };
			Faces[1] = { 0.f, -ScD * 0.5f - 5.f, true, ScW };
		}
		else
		{
			Faces[0] = {  ScW * 0.5f + 5.f, 0.f, false, ScD };
			Faces[1] = { -ScW * 0.5f - 5.f, 0.f, false, ScD };
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
		// Scaled face offsets
		const float ScW = W * WSF;
		const float ScD = D * WSF;

		for (int32 b = 0; b < NumBands; ++b)
		{
			const float BZ = BaseLoc.Z + H * (0.15f + 0.7f * b / FMath::Max(1, NumBands - 1));
			FString NY = FString::Printf(TEXT("Band_%d"), BIdx++);
			FString NnY = FString::Printf(TEXT("Band_%d"), BIdx++);
			FString NX = FString::Printf(TEXT("Band_%d"), BIdx++);
			FString NnX = FString::Printf(TEXT("Band_%d"), BIdx++);
			// +Y face
			MakeCubeMC(Owner, FVector(BaseLoc.X, BaseLoc.Y + ScD * 0.5f + 3.f, BZ),
				FVector(W, BandD, BandH), GlassColor, *NY, 0.1f, 0.05f);
			// -Y face
			MakeCubeMC(Owner, FVector(BaseLoc.X, BaseLoc.Y - ScD * 0.5f - 3.f, BZ),
				FVector(W, BandD, BandH), GlassColor, *NnY, 0.1f, 0.05f);
			// +X face
			MakeCubeMC(Owner, FVector(BaseLoc.X + ScW * 0.5f + 3.f, BaseLoc.Y, BZ),
				FVector(BandD, D, BandH), GlassColor, *NX, 0.1f, 0.05f);
			// -X face
			MakeCubeMC(Owner, FVector(BaseLoc.X - ScW * 0.5f - 3.f, BaseLoc.Y, BZ),
				FVector(BandD, D, BandH), GlassColor, *NnX, 0.1f, 0.05f);
		}
	};

	// Helper: add vertical corner columns.
	auto AddCornerColumns = [&](AActor* Owner, const FVector& BaseLoc,
		float W, float D, float H, float ColW, const FLinearColor& Color)
	{
		const float HalfW = W * WSF * 0.5f;
		const float HalfD = D * WSF * 0.5f;
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
	// Charlotte Uptown Color Palette
	// ===================================================================
	const FLinearColor SteelGray(0.42f, 0.44f, 0.46f);
	const FLinearColor DarkSteel(0.20f, 0.22f, 0.24f);
	const FLinearColor LightConcrete(0.72f, 0.70f, 0.68f);
	const FLinearColor WarmConcrete(0.65f, 0.58f, 0.50f);
	const FLinearColor CoolWhite(0.82f, 0.84f, 0.88f);
	const FLinearColor TintedGlass(0.10f, 0.14f, 0.22f);
	const FLinearColor DarkGlass(0.05f, 0.08f, 0.14f);
	const FLinearColor SkyGlass(0.12f, 0.18f, 0.28f);
	const FLinearColor GoldAccent(0.75f, 0.55f, 0.15f);
	const FLinearColor CopperAccent(0.62f, 0.35f, 0.12f);
	const FLinearColor AntennaRed(0.85f, 0.10f, 0.10f);
	const FLinearColor LightGray(0.58f, 0.56f, 0.54f);
	const FLinearColor SandStone(0.72f, 0.65f, 0.52f);
	const FLinearColor CharcoalGray(0.15f, 0.15f, 0.17f);
	const FLinearColor WhiteMarble(0.90f, 0.88f, 0.85f);
	// Charlotte-specific
	const FLinearColor RosyBeige(0.72f, 0.65f, 0.58f);    // BofA Corporate Center granite
	const FLinearColor BofASilver(0.80f, 0.80f, 0.82f);    // BofA crown spires
	const FLinearColor CobaltBlue(0.08f, 0.12f, 0.28f);    // 300 South Tryon
	const FLinearColor OliveDrab(0.22f, 0.24f, 0.14f);     // military/utilitarian
	const FLinearColor PantherBlack(0.04f, 0.04f, 0.04f);  // Stadium panther statues
	const FLinearColor StadiumGray(0.55f, 0.55f, 0.52f);   // Stadium concrete
	const FLinearColor FieldGreen(0.08f, 0.35f, 0.08f);    // Playing field grass
	const FLinearColor BaseballDirt(0.55f, 0.38f, 0.22f);   // Infield dirt
	const FLinearColor ParkGreen(0.12f, 0.42f, 0.12f);     // Park lawn
	const FLinearColor ChurchStone(0.68f, 0.62f, 0.52f);   // Gothic church stone
	const FLinearColor NASCARSteel(0.70f, 0.72f, 0.74f);   // NASCAR Hall stainless

	// ===================================================================
	// CHARLOTTE, NC UPTOWN — Real buildings at real positions
	// Scale: 8 UU = 1 foot. BlockCenter(12,12) = Trade/Tryon intersection.
	// ===================================================================

	// ===================================================================
	// 1. BANK OF AMERICA CORPORATE CENTER — 871 ft (6,968 UU)
	//    100 N. Tryon St — tallest in Charlotte. Rosy beige granite,
	//    6 stepped setbacks, iconic 100-ft metal crown spires.
	//    Block (12,14) — N. Tryon between Trade and 5th St
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 14);
		AActor* T = MakeTower(Loc, TEXT("CLT_BofA_CorporateCenter"));
		if (T)
		{
			// 30-ft granite base
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 120.f),
				FVector(700.f, 600.f, 240.f), RosyBeige, TEXT("BofA_Base"), 0.1f, 0.7f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Floors 1-13: Full footprint
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 240.f + 760.f),
				FVector(700.f, 600.f, 1520.f), RosyBeige, TEXT("BofA_Tier1"), 0.05f, 0.6f);
			// Floors 13-44: Setback 1
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1760.f + 1800.f),
				FVector(600.f, 500.f, 3600.f), RosyBeige * 1.05f, TEXT("BofA_Tier2"), 0.05f, 0.55f);
			// Floors 44-47
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 5360.f + 180.f),
				FVector(500.f, 400.f, 360.f), RosyBeige * 1.08f, TEXT("BofA_Tier3"), 0.05f, 0.5f);
			// Floors 47-53
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 5720.f + 360.f),
				FVector(400.f, 300.f, 720.f), RosyBeige * 1.10f, TEXT("BofA_Tier4"), 0.05f, 0.5f);
			// Floors 53-56
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 6440.f + 180.f),
				FVector(300.f, 250.f, 360.f), BofASilver, TEXT("BofA_Tier5"), 0.3f, 0.4f);
			// Floors 56-60 (top section)
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 6800.f + 84.f),
				FVector(200.f, 180.f, 168.f), BofASilver, TEXT("BofA_Tier6"), 0.3f, 0.4f);
			// Crown spires — 8 vertical rods in a ring (100 ft = 800 UU)
			const float CrownBase = 6968.f;
			for (int32 i = 0; i < 8; ++i)
			{
				const float Angle = i * 45.f;
				const float Rad = FMath::DegreesToRadians(Angle);
				const float CR = 80.f;
				FString SN = FString::Printf(TEXT("BofA_Spire_%d"), i);
				MakeCylMC(T, FVector(Loc.X + FMath::Cos(Rad) * CR, Loc.Y + FMath::Sin(Rad) * CR, CrownBase + 400.f),
					FVector(8.f, 8.f, 800.f), BofASilver, *SN, 0.9f, 0.1f);
			}
			AddGlassBands(T, Loc, 700.f, 600.f, 5360.f, SkyGlass, BandIdx);
			AddWindows(T, Loc, 700.f, 600.f, 6968.f, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 2. 550 SOUTH TRYON (fmr. Duke Energy Center) — 786 ft (6,288 UU)
	//    Glass curtain wall, "handlebar" crown with LED arches.
	//    Block (12,8)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 8);
		AActor* T = MakeTower(Loc, TEXT("CLT_550_SouthTryon"));
		if (T)
		{
			const float H = 6288.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(550.f, 500.f, H), TintedGlass, TEXT("550S_Main"), 0.15f, 0.08f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// "Handlebar" crown — two arched elements (approximated with cylinders)
			MakeCylMC(T, FVector(Loc.X - 200.f, Loc.Y, H + 200.f),
				FVector(30.f, 30.f, 400.f), CoolWhite, TEXT("550S_CrownL"), 0.8f, 0.15f,
				FLinearColor(0.8f, 0.4f, 1.0f));
			MakeCylMC(T, FVector(Loc.X + 200.f, Loc.Y, H + 200.f),
				FVector(30.f, 30.f, 400.f), CoolWhite, TEXT("550S_CrownR"), 0.8f, 0.15f,
				FLinearColor(0.8f, 0.4f, 1.0f));
			// Crown connecting bar
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 400.f),
				FVector(450.f, 30.f, 30.f), CoolWhite, TEXT("550S_CrownBar"), 0.8f, 0.15f,
				FLinearColor(0.8f, 0.4f, 1.0f));
			AddGlassBands(T, Loc, 550.f, 500.f, H, SkyGlass, BandIdx);
			AddWindows(T, Loc, 550.f, 500.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 3. TRUIST CENTER (fmr. Hearst Tower) — 659 ft (5,272 UU)
	//    Reverse floorplate (widens upward). 15-floor podium base.
	//    214 N. Tryon St. Block (12,16)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 16);
		AActor* T = MakeTower(Loc, TEXT("CLT_TruistCenter"));
		if (T)
		{
			// 15-floor podium base (1,200 UU)
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 600.f),
				FVector(800.f, 700.f, 1200.f), LightConcrete, TEXT("Truist_Podium"), 0.05f, 0.6f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Tower body — widens upward (reverse floorplate)
			// Lower section (narrower)
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1200.f + 1000.f),
				FVector(500.f, 450.f, 2000.f), SteelGray, TEXT("Truist_Lower"), 0.1f, 0.4f);
			// Upper section (wider)
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 3200.f + 1036.f),
				FVector(600.f, 550.f, 2072.f), SteelGray * 1.05f, TEXT("Truist_Upper"), 0.1f, 0.4f);
			// Art Deco crown
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 5272.f + 80.f),
				FVector(620.f, 570.f, 160.f), CoolWhite, TEXT("Truist_Crown"), 0.4f, 0.2f);
			AddGlassBands(T, Loc, 600.f, 550.f, 5272.f, TintedGlass, BandIdx);
			AddWindows(T, Loc, 600.f, 550.f, 5272.f, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 4. BANK OF AMERICA TOWER AT LEGACY UNION — 632 ft (5,056 UU)
	//    Translucent pyramid crown, LED illuminated.
	//    Block (12,9)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 9);
		AActor* T = MakeTower(Loc, TEXT("CLT_BofA_LegacyUnion"));
		if (T)
		{
			const float H = 5056.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(500.f, 450.f, H), TintedGlass, TEXT("LU_Main"), 0.12f, 0.1f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// 100-ft translucent pyramid crown (4 angled faces)
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 200.f),
				FVector(350.f, 350.f, 400.f), CoolWhite, TEXT("LU_PyramidBase"), 0.2f, 0.15f);
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 500.f),
				FVector(200.f, 200.f, 200.f), CoolWhite, TEXT("LU_PyramidMid"), 0.2f, 0.15f,
				FLinearColor(2.f, 1.f, 3.f));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 700.f),
				FVector(80.f, 80.f, 100.f), CoolWhite, TEXT("LU_PyramidTip"), 0.2f, 0.15f,
				FLinearColor(3.f, 1.5f, 4.f));
			AddGlassBands(T, Loc, 500.f, 450.f, H, SkyGlass, BandIdx);
			AddWindows(T, Loc, 500.f, 450.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 5. DUKE ENERGY PLAZA — 629 ft (5,032 UU)
	//    525 S. Tryon St. Block (12,9) offset
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 9) + FVector(600.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_DukeEnergyPlaza"));
		if (T)
		{
			const float H = 5032.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(480.f, 420.f, H), SteelGray, TEXT("DEP_Main"), 0.1f, 0.35f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 60.f),
				FVector(500.f, 440.f, 30.f), LightGray, TEXT("DEP_Roof"), 0.3f, 0.3f);
			AddGlassBands(T, Loc, 480.f, 420.f, H, SkyGlass, BandIdx);
			AddWindows(T, Loc, 480.f, 420.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 6. ONE WELLS FARGO CENTER — 588 ft (4,704 UU)
	//    301 S. College St. Postmodern. Block (13,10)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(13, 10);
		AActor* T = MakeTower(Loc, TEXT("CLT_OneWellsFargo"));
		if (T)
		{
			const float H = 4704.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(500.f, 480.f, H), LightConcrete, TEXT("OWF_Main"), 0.05f, 0.55f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Postmodern crown detail
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 40.f),
				FVector(520.f, 500.f, 20.f), SandStone, TEXT("OWF_Crown"), 0.15f, 0.4f);
			AddCornerColumns(T, Loc, 500.f, 480.f, H, 25.f, DarkSteel);
			AddGlassBands(T, Loc, 500.f, 480.f, H, TintedGlass, BandIdx);
			AddWindows(T, Loc, 500.f, 480.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 7. THE VUE — 576 ft (4,608 UU)
	//    Residential, blue glass, 51 floors, stepbacks. Block (11,9)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(11, 9);
		AActor* T = MakeTower(Loc, TEXT("CLT_TheVue"));
		if (T)
		{
			const float H = 4608.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(350.f, 320.f, H), CobaltBlue, TEXT("Vue_Main"), 0.15f, 0.08f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Stepback at floor 30 (~3000 UU)
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1500.f),
				FVector(400.f, 370.f, 3000.f), CobaltBlue * 0.95f, TEXT("Vue_Base"), 0.15f, 0.08f);
			// White column accents
			AddCornerColumns(T, Loc, 350.f, 320.f, H, 15.f, CoolWhite);
			AddWindows(T, Loc, 350.f, 320.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 8. 300 SOUTH TRYON (Barings) — 463 ft (3,704 UU)
	//    Cobalt blue glass, sail form, illuminated crown. Block (12,10)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 10);
		AActor* T = MakeTower(Loc, TEXT("CLT_300SouthTryon"));
		if (T)
		{
			const float H = 3704.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(450.f, 400.f, H), CobaltBlue, TEXT("300S_Main"), 0.18f, 0.06f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Illuminated crown
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 80.f),
				FVector(470.f, 420.f, 160.f), CobaltBlue * 1.2f, TEXT("300S_Crown"), 0.3f, 0.1f,
				FLinearColor(1.5f, 2.0f, 4.0f));
			AddGlassBands(T, Loc, 450.f, 400.f, H, SkyGlass, BandIdx);
			AddWindows(T, Loc, 450.f, 400.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 9. FIFTH THIRD CENTER — 447 ft (3,576 UU)
	//    201 N. Tryon. Postmodern glass + steel. Block (12,15)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 15);
		AActor* T = MakeTower(Loc, TEXT("CLT_FifthThirdCenter"));
		if (T)
		{
			const float H = 3576.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(420.f, 380.f, H), SteelGray, TEXT("53rd_Main"), 0.12f, 0.3f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddGlassBands(T, Loc, 420.f, 380.f, H, TintedGlass, BandIdx);
			AddWindows(T, Loc, 420.f, 380.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 10. TWO WELLS FARGO CENTER — 433 ft (3,464 UU)
	//     S. College St. International modernist. Block (13,11)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(13, 11);
		AActor* T = MakeTower(Loc, TEXT("CLT_TwoWellsFargo"));
		if (T)
		{
			const float H = 3464.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(450.f, 400.f, H), WarmConcrete, TEXT("TWF_Main"), 0.05f, 0.55f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddGlassBands(T, Loc, 450.f, 400.f, H, TintedGlass, BandIdx);
			AddWindows(T, Loc, 450.f, 400.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 11. 400 SOUTH TRYON — 420 ft (3,360 UU). Block (12,10) offset
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 10) + FVector(500.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_400SouthTryon"));
		if (T)
		{
			const float H = 3360.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(400.f, 380.f, H), LightConcrete, TEXT("400S_Main"), 0.05f, 0.5f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddGlassBands(T, Loc, 400.f, 380.f, H, TintedGlass, BandIdx);
			AddWindows(T, Loc, 400.f, 380.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 12. CARILLON TOWER — 394 ft (3,152 UU)
	//     Neo-Gothic, copper bell tower spire. 227 W. Trade. Block (11,12)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(11, 12);
		AActor* T = MakeTower(Loc, TEXT("CLT_CarillonTower"));
		if (T)
		{
			const float H = 3152.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(380.f, 350.f, H), SandStone, TEXT("Carillon_Main"), 0.05f, 0.6f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Copper spire crown
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H + 250.f),
				FVector(80.f, 80.f, 500.f), CopperAccent, TEXT("Carillon_Spire"), 0.85f, 0.25f);
			// Cone cap on spire
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 500.f + 100.f),
				FVector(100.f, 100.f, 200.f), CopperAccent * 1.1f, TEXT("Carillon_SpireTip"), 0.85f, 0.25f);
			AddWindows(T, Loc, 380.f, 350.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 13. CHARLOTTE PLAZA — 388 ft (3,104 UU). Block (11,13)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(11, 13);
		AActor* T = MakeTower(Loc, TEXT("CLT_CharlottePlaza"));
		if (T)
		{
			const float H = 3104.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(420.f, 400.f, H), CoolWhite, TEXT("ChPl_Main"), 0.08f, 0.4f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddGlassBands(T, Loc, 420.f, 400.f, H, SkyGlass, BandIdx);
			AddWindows(T, Loc, 420.f, 400.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 14. ALLY CHARLOTTE CENTER — 378 ft (3,024 UU)
	//     Precast + glass, illuminated crown. Block (12,7)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 7);
		AActor* T = MakeTower(Loc, TEXT("CLT_AllyCharlotteCenter"));
		if (T)
		{
			const float H = 3024.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(430.f, 400.f, H), CoolWhite, TEXT("Ally_Main"), 0.05f, 0.45f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Illuminated polycarbonate crown
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 60.f),
				FVector(450.f, 420.f, 120.f), CoolWhite, TEXT("Ally_Crown"), 0.1f, 0.2f,
				FLinearColor(2.0f, 1.5f, 1.0f));
			AddGlassBands(T, Loc, 430.f, 400.f, H, TintedGlass, BandIdx);
			AddWindows(T, Loc, 430.f, 400.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 15. THE WESTIN CHARLOTTE — 350 ft (2,800 UU)
	//     601 S. College. 700-room hotel. Block (13,8)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(13, 8);
		AActor* T = MakeTower(Loc, TEXT("CLT_WestinCharlotte"));
		if (T)
		{
			const float H = 2800.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(450.f, 400.f, H), WarmConcrete, TEXT("Westin_Main"), 0.05f, 0.5f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 450.f, 400.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 16. ASCENT UPTOWN — 338 ft (2,704 UU). Residential. Block (14,14)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(14, 14);
		AActor* T = MakeTower(Loc, TEXT("CLT_AscentUptown"));
		if (T)
		{
			const float H = 2704.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(350.f, 320.f, H), CoolWhite, TEXT("Ascent_Main"), 0.05f, 0.45f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 350.f, 320.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 17. TRADEMARK — 325 ft (2,600 UU). Condos. Block (11,12) offset
	// ===================================================================
	{
		const FVector Loc = BlockCenter(11, 12) + FVector(-500.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_Trademark"));
		if (T)
		{
			const float H = 2600.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(300.f, 280.f, H), SteelGray, TEXT("TM_Main"), 0.08f, 0.35f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 300.f, 280.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 18. AC HOTEL/RESIDENCE INN — 320 ft (2,560 UU). Block (14,12)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(14, 12);
		AActor* T = MakeTower(Loc, TEXT("CLT_ACHotel"));
		if (T)
		{
			const float H = 2560.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(300.f, 280.f, H), WarmConcrete, TEXT("AC_Main"), 0.05f, 0.5f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 300.f, 280.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 19. 101 INDEPENDENCE CENTER — 301 ft (2,408 UU)
	//     101 N. Tryon. At Trade/Tryon intersection. Block (12,12) NE
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 12) + FVector(300.f, 300.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_101Independence"));
		if (T)
		{
			const float H = 2408.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(380.f, 350.f, H), LightConcrete, TEXT("101I_Main"), 0.05f, 0.5f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 380.f, 350.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 20. THE AVENUE — ~370 ft (2,960 UU). 36 condos. Block (13,15)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(13, 15);
		AActor* T = MakeTower(Loc, TEXT("CLT_TheAvenue"));
		if (T)
		{
			const float H = 2960.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(320.f, 300.f, H), TintedGlass, TEXT("Ave_Main"), 0.12f, 0.1f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 320.f, 300.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 21. DELOITTE TOWER — 290 ft (2,320 UU). Block (12,8) offset
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 8) + FVector(-600.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_DeloitteTower"));
		if (T)
		{
			const float H = 2320.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(380.f, 350.f, H), SteelGray, TEXT("Deloitte_Main"), 0.08f, 0.35f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 380.f, 350.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 22. HONEYWELL TOWER — 290 ft (2,320 UU)
	//     700 S. Mint. Overlooks stadium. Block (9,8)
	// ===================================================================
	{
		const FVector Loc = BlockCenter(9, 8);
		AActor* T = MakeTower(Loc, TEXT("CLT_HoneywellTower"));
		if (T)
		{
			const float H = 2320.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(380.f, 360.f, H), CoolWhite, TEXT("Honey_Main"), 0.05f, 0.45f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 380.f, 360.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// 23. 112 TRYON PLAZA — 280 ft (2,240 UU)
	//     Historic 1927. Block (12,12) SE
	// ===================================================================
	{
		const FVector Loc = BlockCenter(12, 12) + FVector(300.f, -300.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_112TryonPlaza"));
		if (T)
		{
			const float H = 2240.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(350.f, 320.f, H), SandStone, TEXT("112T_Main"), 0.05f, 0.65f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 350.f, 320.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// LANDMARK: FIRST PRESBYTERIAN CHURCH — 212 ft steeple (1,696 UU)
	//     200 W. Trade. Gothic Revival. Block (11,12) south edge
	// ===================================================================
	{
		const FVector Loc = BlockCenter(11, 12) + FVector(0.f, -400.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_FirstPresbyterianChurch"));
		if (T)
		{
			// Church body
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 300.f),
				FVector(400.f, 640.f, 600.f), ChurchStone, TEXT("Church_Body"), 0.f, 0.8f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Steeple base
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 250.f, 600.f + 250.f),
				FVector(150.f, 150.f, 500.f), ChurchStone, TEXT("Church_SteepleBase"), 0.f, 0.75f);
			// Steeple spire (cone approximated by cylinder tapering)
			MakeCylMC(T, FVector(Loc.X, Loc.Y + 250.f, 1100.f + 300.f),
				FVector(60.f, 60.f, 600.f), ChurchStone * 0.9f, TEXT("Church_Spire"), 0.f, 0.7f);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// LANDMARK: BANK OF AMERICA STADIUM — blocks (8-10, 5-7)
	//     800 S. Mint. NFL/MLS. 75,037 seats. ~33 acres.
	// ===================================================================
	{
		const FVector Loc = BlockCenter(9, 6);
		AActor* T = MakeTower(Loc, TEXT("CLT_BofA_Stadium"));
		if (T)
		{
			// Stadium bowl — large oval footprint
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 400.f),
				FVector(2400.f, 1800.f, 800.f), StadiumGray, TEXT("Stadium_Bowl"), 0.05f, 0.7f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Inner field (green)
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 80.f),
				FVector(1600.f, 900.f, 10.f), FieldGreen, TEXT("Stadium_Field"), 0.f, 0.85f);
			// Upper deck walls (taller outer rim)
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 950.f, 500.f),
				FVector(2200.f, 100.f, 1000.f), StadiumGray, TEXT("Stadium_WallN"), 0.05f, 0.6f);
			MakeCubeMC(T, FVector(Loc.X, Loc.Y - 950.f, 500.f),
				FVector(2200.f, 100.f, 1000.f), StadiumGray, TEXT("Stadium_WallS"), 0.05f, 0.6f);
			MakeCubeMC(T, FVector(Loc.X + 1250.f, Loc.Y, 500.f),
				FVector(100.f, 1800.f, 1000.f), StadiumGray, TEXT("Stadium_WallE"), 0.05f, 0.6f);
			MakeCubeMC(T, FVector(Loc.X - 1250.f, Loc.Y, 500.f),
				FVector(100.f, 1800.f, 1000.f), StadiumGray, TEXT("Stadium_WallW"), 0.05f, 0.6f);
			// Entry arches (4 massive arches at cardinal points)
			for (int32 i = 0; i < 4; ++i)
			{
				const float Angle = i * 90.f;
				const float R = FMath::DegreesToRadians(Angle);
				const float AX = Loc.X + FMath::Cos(R) * 1300.f;
				const float AY = Loc.Y + FMath::Sin(R) * 1000.f;
				FString AN = FString::Printf(TEXT("Stadium_Arch_%d"), i);
				MakeCylMC(T, FVector(AX, AY, 600.f),
					FVector(80.f, 80.f, 1200.f), StadiumGray * 0.8f, *AN, 0.1f, 0.5f);
			}
			// Panther statues (6 dark cubes at entrances)
			for (int32 i = 0; i < 6; ++i)
			{
				const float Angle = i * 60.f;
				const float R = FMath::DegreesToRadians(Angle);
				const float PX = Loc.X + FMath::Cos(R) * 1350.f;
				const float PY = Loc.Y + FMath::Sin(R) * 1050.f;
				FString PN = FString::Printf(TEXT("Stadium_Panther_%d"), i);
				MakeCubeMC(T, FVector(PX, PY, 80.f),
					FVector(60.f, 30.f, 160.f), PantherBlack, *PN, 0.7f, 0.2f);
			}
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// LANDMARK: TRUIST FIELD (baseball) — block (9,10)
	//     324 S. Mint. Charlotte Knights. 10,000 seats.
	// ===================================================================
	{
		const FVector Loc = BlockCenter(9, 10);
		AActor* T = MakeTower(Loc, TEXT("CLT_TruistField"));
		if (T)
		{
			// Grandstand (C-shaped seating behind home plate)
			auto* Body = MakeCubeMC(T, FVector(Loc.X - 200.f, Loc.Y, 200.f),
				FVector(1200.f, 800.f, 400.f), StadiumGray, TEXT("TField_Grandstand"), 0.05f, 0.65f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Playing field — baseball diamond
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 5.f),
				FVector(1400.f, 1200.f, 10.f), FieldGreen, TEXT("TField_Outfield"), 0.f, 0.85f);
			// Infield dirt
			MakeCubeMC(T, FVector(Loc.X - 200.f, Loc.Y, 6.f),
				FVector(600.f, 600.f, 8.f), BaseballDirt, TEXT("TField_Infield"), 0.f, 0.9f);
			// Press box
			MakeCubeMC(T, FVector(Loc.X - 700.f, Loc.Y, 450.f),
				FVector(400.f, 300.f, 200.f), DarkSteel, TEXT("TField_PressBox"), 0.15f, 0.3f);
			// Outfield wall
			MakeCubeMC(T, FVector(Loc.X + 500.f, Loc.Y, 50.f),
				FVector(30.f, 1000.f, 100.f), DarkSteel, TEXT("TField_Wall"), 0.1f, 0.5f);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// LANDMARK: SPECTRUM CENTER — blocks (14-15, 12)
	//     330 E. Trade. Charlotte Hornets. 19,444 seats.
	// ===================================================================
	{
		const FVector Loc = BlockCenter(14, 12) + FVector(400.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_SpectrumCenter"));
		if (T)
		{
			// Main arena body
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 350.f),
				FVector(1400.f, 1200.f, 700.f), SteelGray, TEXT("Spectrum_Body"), 0.1f, 0.4f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Rounded roof
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, 700.f),
				FVector(1300.f, 1100.f, 400.f), DarkSteel, TEXT("Spectrum_Roof"), 0.15f, 0.3f);
			// Glass entrance facade
			MakeCubeMC(T, FVector(Loc.X - 750.f, Loc.Y, 350.f),
				FVector(50.f, 800.f, 700.f), SkyGlass, TEXT("Spectrum_Glass"), 0.1f, 0.05f);
			// Outdoor plaza
			MakeCubeMC(T, FVector(Loc.X - 1000.f, Loc.Y, 3.f),
				FVector(500.f, 1000.f, 6.f), LightGray, TEXT("Spectrum_Plaza"), 0.f, 0.7f);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// LANDMARK: NASCAR HALL OF FAME — block (14,8)
	//     Racetrack-inspired shape, stainless steel facade.
	// ===================================================================
	{
		const FVector Loc = BlockCenter(14, 8);
		AActor* T = MakeTower(Loc, TEXT("CLT_NASCARHallOfFame"));
		if (T)
		{
			// Museum body — sinuous offset cubes
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 250.f),
				FVector(700.f, 500.f, 500.f), NASCARSteel, TEXT("NASCAR_Main"), 0.65f, 0.15f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Offset curved section
			MakeCubeMC(T, FVector(Loc.X + 200.f, Loc.Y + 100.f, 200.f),
				FVector(500.f, 400.f, 400.f), NASCARSteel * 0.95f, TEXT("NASCAR_Wing"), 0.6f, 0.2f);
			// 110-ft canopy overhang
			MakeCubeMC(T, FVector(Loc.X - 500.f, Loc.Y, 400.f),
				FVector(600.f, 500.f, 20.f), NASCARSteel, TEXT("NASCAR_Canopy"), 0.7f, 0.1f);
			// Associated office tower (19 stories)
			MakeCubeMC(T, FVector(Loc.X + 500.f, Loc.Y, 760.f),
				FVector(250.f, 250.f, 1520.f), SteelGray, TEXT("NASCAR_Tower"), 0.1f, 0.35f);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// LANDMARK: ROMARE BEARDEN PARK — block (10,10)
	//     5.4 acres. Fountain, paths, trees.
	// ===================================================================
	{
		const FVector Loc = BlockCenter(10, 10);
		AActor* T = MakeTower(Loc, TEXT("CLT_RomareBearden_Park"));
		if (T)
		{
			// Green lawn
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2.f),
				FVector(1600.f, 1600.f, 4.f), ParkGreen, TEXT("RB_Lawn"), 0.f, 0.85f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Fountain basin
			MakeCylMC(T, FVector(Loc.X, Loc.Y, 30.f),
				FVector(200.f, 200.f, 60.f), LightGray, TEXT("RB_Basin"), 0.1f, 0.5f);
			// Fountain spout
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, 80.f),
				FVector(40.f, 40.f, 40.f), CoolWhite, TEXT("RB_Spout"), 0.1f, 0.3f,
				FLinearColor(0.3f, 0.5f, 1.0f));
			// Walking paths (cross shape)
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 3.5f),
				FVector(1400.f, 80.f, 3.f), LightGray, TEXT("RB_PathEW"), 0.f, 0.75f);
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 3.5f),
				FVector(80.f, 1400.f, 3.f), LightGray, TEXT("RB_PathNS"), 0.f, 0.75f);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// LANDMARK: FIRST WARD PARK — blocks (14-15, 17-18)
	//     4.6 acres. Circular fountain, green lawn.
	// ===================================================================
	{
		const FVector Loc = BlockCenter(14, 17) + FVector(400.f, 400.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_FirstWardPark"));
		if (T)
		{
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2.f),
				FVector(2200.f, 2200.f, 4.f), ParkGreen, TEXT("FWP_Lawn"), 0.f, 0.85f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Large circular fountain
			MakeCylMC(T, FVector(Loc.X, Loc.Y, 20.f),
				FVector(300.f, 300.f, 40.f), CoolWhite, TEXT("FWP_Fountain"), 0.1f, 0.4f);
			// White arbor
			MakeCubeMC(T, FVector(Loc.X + 400.f, Loc.Y, 200.f),
				FVector(300.f, 20.f, 400.f), CoolWhite, TEXT("FWP_Arbor"), 0.f, 0.5f);
			MakeCubeMC(T, FVector(Loc.X + 400.f, Loc.Y, 400.f),
				FVector(300.f, 150.f, 20.f), CoolWhite, TEXT("FWP_ArborTop"), 0.f, 0.5f);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// CHARLOTTE CONVENTION CENTER — blocks (13-14, 8)
	//     280,000 sf exhibit space. South Uptown.
	// ===================================================================
	{
		const FVector Loc = BlockCenter(13, 8) + FVector(500.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_ConventionCenter"));
		if (T)
		{
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 200.f),
				FVector(1800.f, 800.f, 400.f), LightConcrete, TEXT("Conv_Main"), 0.05f, 0.55f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Glass atrium entrance
			MakeCubeMC(T, FVector(Loc.X - 800.f, Loc.Y, 250.f),
				FVector(200.f, 600.f, 500.f), SkyGlass, TEXT("Conv_Atrium"), 0.1f, 0.05f);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// ===================================================================
	// Additional mid-rise context buildings to fill Charlotte Uptown
	// ===================================================================

	// Charlotte-Mecklenburg Government Center (City Hall) — 14 floors, triangular
	{
		const FVector Loc = BlockCenter(15, 12);
		AActor* T = MakeTower(Loc, TEXT("CLT_GovernmentCenter"));
		if (T)
		{
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 560.f),
				FVector(500.f, 450.f, 1120.f), LightConcrete, TEXT("GovCtr_Main"), 0.05f, 0.5f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 500.f, 450.f, 1120.f, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// Discovery Place Science — 301 N. Tryon
	{
		const FVector Loc = BlockCenter(12, 17);
		AActor* T = MakeTower(Loc, TEXT("CLT_DiscoveryPlace"));
		if (T)
		{
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 200.f),
				FVector(600.f, 500.f, 400.f), CoolWhite, TEXT("DiscP_Main"), 0.05f, 0.45f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// IMAX dome
			MakeSphereMC(T, FVector(Loc.X + 200.f, Loc.Y, 400.f),
				FVector(200.f, 200.f, 200.f), DarkSteel, TEXT("DiscP_Dome"), 0.2f, 0.3f);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// Bechtler Museum of Modern Art — 430 S. Tryon, Levine Center
	{
		const FVector Loc = BlockCenter(12, 7) + FVector(-500.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_BechtlerMuseum"));
		if (T)
		{
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 200.f),
				FVector(350.f, 300.f, 400.f), SandStone, TEXT("Becht_Main"), 0.05f, 0.7f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			// Cantilevered 4th floor
			MakeCubeMC(T, FVector(Loc.X - 100.f, Loc.Y, 350.f),
				FVector(500.f, 320.f, 100.f), SandStone * 0.95f, TEXT("Becht_Cant"), 0.05f, 0.65f);
			// Support column
			MakeCylMC(T, FVector(Loc.X - 250.f, Loc.Y, 175.f),
				FVector(32.f, 32.f, 350.f), LightConcrete, TEXT("Becht_Col"), 0.2f, 0.4f);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// SkyHouse Uptown (two 24-story towers) — 640 N. Church
	{
		const FVector Loc = BlockCenter(10, 18);
		AActor* T = MakeTower(Loc, TEXT("CLT_SkyHouse"));
		if (T)
		{
			const float H = 2080.f; // ~260 ft
			auto* Body = MakeCubeMC(T, FVector(Loc.X - 200.f, Loc.Y, H * 0.5f),
				FVector(280.f, 260.f, H), CoolWhite, TEXT("SkyH_TowerA"), 0.05f, 0.4f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			MakeCubeMC(T, FVector(Loc.X + 200.f, Loc.Y, H * 0.5f),
				FVector(280.f, 260.f, H), CoolWhite * 0.98f, TEXT("SkyH_TowerB"), 0.05f, 0.4f);
			AddWindows(T, FVector(Loc.X - 200.f, Loc.Y, 0.f), 280.f, 260.f, H, WinIdx);
			AddWindows(T, FVector(Loc.X + 200.f, Loc.Y, 0.f), 280.f, 260.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// JW Marriott (associated with Ally Charlotte Center) — 22 stories
	{
		const FVector Loc = BlockCenter(12, 7) + FVector(500.f, 0.f, 0.f);
		AActor* T = MakeTower(Loc, TEXT("CLT_JWMarriott"));
		if (T)
		{
			const float H = 1760.f; // ~220 ft
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(350.f, 300.f, H), WarmConcrete, TEXT("JWM_Main"), 0.05f, 0.5f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 350.f, 300.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// Charlotte Marriott City Center — ~260 ft (2,080 UU), 19 floors
	{
		const FVector Loc = BlockCenter(13, 12);
		AActor* T = MakeTower(Loc, TEXT("CLT_MarriottCityCenter"));
		if (T)
		{
			const float H = 2080.f;
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(380.f, 350.f, H), WarmConcrete, TEXT("MCC_Main"), 0.05f, 0.55f);
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);
			AddWindows(T, Loc, 380.f, 350.f, H, WinIdx);
			T->SetRootComponent(Body);
			SkyCount++;
		}
	}

	// Mid-rise office filler buildings to populate empty Uptown blocks
	{
		struct FFillerBuilding { int32 bx, by; float H; float W; float D; const TCHAR* Name; };
		const FFillerBuilding Fillers[] = {
			// Core Uptown fillers
			{ 10, 12, 1200.f, 350.f, 320.f, TEXT("CLT_Filler_ChurchTrade") },
			{ 10, 13, 1000.f, 300.f, 280.f, TEXT("CLT_Filler_Church5th") },
			{ 10, 14, 1400.f, 380.f, 350.f, TEXT("CLT_Filler_Church6th") },
			{ 11, 14, 1100.f, 320.f, 300.f, TEXT("CLT_Filler_W5th") },
			{ 11, 15, 900.f, 300.f, 280.f, TEXT("CLT_Filler_W6th") },
			{ 13, 13, 1300.f, 350.f, 320.f, TEXT("CLT_Filler_College3rd") },
			{ 13, 14, 1000.f, 320.f, 300.f, TEXT("CLT_Filler_College5th") },
			{ 14, 13, 900.f, 300.f, 280.f, TEXT("CLT_Filler_Brevard3rd") },
			{ 15, 13, 800.f, 280.f, 260.f, TEXT("CLT_Filler_Davidson3rd") },
			{ 15, 14, 700.f, 260.f, 240.f, TEXT("CLT_Filler_Davidson5th") },
			{ 11, 16, 800.f, 280.f, 260.f, TEXT("CLT_Filler_W7th") },
			{ 13, 16, 1000.f, 320.f, 300.f, TEXT("CLT_Filler_College7th") },
			{ 14, 15, 700.f, 260.f, 250.f, TEXT("CLT_Filler_Brevard5th") },
			{ 14, 16, 800.f, 280.f, 260.f, TEXT("CLT_Filler_Brevard7th") },
			{ 10, 11, 900.f, 300.f, 280.f, TEXT("CLT_Filler_ChurchMLK") },
			{ 11, 11, 1000.f, 320.f, 300.f, TEXT("CLT_Filler_WMLK") },
			{ 13, 9, 900.f, 300.f, 280.f, TEXT("CLT_Filler_CollegeHill") },
			{ 11, 10, 800.f, 280.f, 260.f, TEXT("CLT_Filler_W1st") },
			{ 10, 15, 1200.f, 340.f, 320.f, TEXT("CLT_Filler_Church6thA") },
			{ 15, 12, 1400.f, 360.f, 340.f, TEXT("CLT_Filler_DavidsonTrade") },
			// Extended skyline — outer ring towers
			{  8, 10, 1800.f, 380.f, 350.f, TEXT("CLT_Filler_Ext_SW1") },
			{  8, 12, 2200.f, 400.f, 380.f, TEXT("CLT_Filler_Ext_W1") },
			{  8, 14, 1600.f, 360.f, 340.f, TEXT("CLT_Filler_Ext_NW1") },
			{  9, 10, 2000.f, 380.f, 360.f, TEXT("CLT_Filler_Ext_SW2") },
			{  9, 14, 1900.f, 370.f, 350.f, TEXT("CLT_Filler_Ext_NW2") },
			{  9, 16, 1400.f, 340.f, 320.f, TEXT("CLT_Filler_Ext_NW3") },
			{ 16, 10, 1700.f, 360.f, 340.f, TEXT("CLT_Filler_Ext_SE1") },
			{ 16, 12, 2100.f, 390.f, 370.f, TEXT("CLT_Filler_Ext_E1") },
			{ 16, 14, 1500.f, 350.f, 330.f, TEXT("CLT_Filler_Ext_NE1") },
			{ 17, 11, 1300.f, 320.f, 300.f, TEXT("CLT_Filler_Ext_E2") },
			{ 17, 13, 1600.f, 350.f, 330.f, TEXT("CLT_Filler_Ext_E3") },
			{  7, 11, 1200.f, 320.f, 300.f, TEXT("CLT_Filler_Ext_W2") },
			{  7, 13, 1500.f, 340.f, 320.f, TEXT("CLT_Filler_Ext_W3") },
			{ 10,  8, 1400.f, 340.f, 320.f, TEXT("CLT_Filler_Ext_S1") },
			{ 12,  6, 1800.f, 370.f, 350.f, TEXT("CLT_Filler_Ext_S2") },
			{ 14,  8, 1300.f, 320.f, 300.f, TEXT("CLT_Filler_Ext_S3") },
			{ 10, 17, 1100.f, 300.f, 280.f, TEXT("CLT_Filler_Ext_N1") },
			{ 12, 18, 1600.f, 350.f, 330.f, TEXT("CLT_Filler_Ext_N2") },
			{ 14, 17, 1200.f, 320.f, 300.f, TEXT("CLT_Filler_Ext_N3") },
			{ 16, 16, 1000.f, 300.f, 280.f, TEXT("CLT_Filler_Ext_NE2") },
			{  9, 12, 2400.f, 420.f, 400.f, TEXT("CLT_Filler_Ext_CW") },
			{ 15, 10, 1800.f, 370.f, 350.f, TEXT("CLT_Filler_Ext_CE") },
		};

		const FLinearColor FillerColors[] = {
			LightConcrete, SteelGray, CoolWhite, WarmConcrete, SandStone,
		};
		const int32 NumFillerColors = UE_ARRAY_COUNT(FillerColors);

		for (int32 fi = 0; fi < (int32)UE_ARRAY_COUNT(Fillers); ++fi)
		{
			const FFillerBuilding& F = Fillers[fi];
			const FVector Loc = BlockCenter(F.bx, F.by);
			AActor* T = MakeTower(Loc, F.Name);
			if (T)
			{
				const FLinearColor& Col = FillerColors[fi % NumFillerColors];
				auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, F.H * 0.5f),
					FVector(F.W, F.D, F.H), Col, TEXT("Fill_Main"), 0.05f, 0.5f);
				Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Body->SetCollisionResponseToAllChannels(ECR_Block);
				AddWindows(T, Loc, F.W, F.D, F.H, WinIdx);
				T->SetRootComponent(Body);
				SkyCount++;
			}
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("Charlotte Uptown: Spawned %d buildings (%d windows)."), SkyCount, WinIdx);
}

// ---------------------------------------------------------------------------
// Mountain Range — dramatic peaks in the northern expansion area
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnMountainRange()
{
	UWorld* World = GetWorld();
	if (!World || !SphereMesh || !CubeMesh || !CylinderMesh || !ConeMesh) return;

	int32 CompIdx = 0;
	bool bIcePeak = false; // set per-peak; lambdas capture by ref

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
		MC->SetBoundsScale(50.f);
		MC->RegisterComponent();
		if (bIcePeak)
		{
			// Rocky ice mountain: dark blue-gray rock with subtle icy emissive
			float B = (Col.R + Col.G + Col.B) / 3.f;
			FLinearColor IceRockCol(0.22f + B * 0.12f, 0.25f + B * 0.12f, 0.32f + B * 0.10f);
			ApplyPBR(MC, IceRockCol, 0.08f, 0.88f,
				FLinearColor(0.01f, 0.03f, 0.06f));
		}
		else
		{
			ApplyPBR(MC, Col, Metal, Rough);
		}
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
		MC->SetBoundsScale(50.f);
		MC->RegisterComponent();
		if (bIcePeak)
		{
			float B = (Col.R + Col.G + Col.B) / 3.f;
			FLinearColor IceRockCol(0.22f + B * 0.12f, 0.25f + B * 0.12f, 0.32f + B * 0.10f);
			ApplyPBR(MC, IceRockCol, 0.08f, 0.88f,
				FLinearColor(0.01f, 0.03f, 0.06f));
		}
		else
		{
			ApplyPBR(MC, Col, Metal, Rough);
		}
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
		MC->SetBoundsScale(50.f);
		MC->RegisterComponent();
		if (bIcePeak)
		{
			float B = (Col.R + Col.G + Col.B) / 3.f;
			FLinearColor IceRockCol(0.22f + B * 0.12f, 0.25f + B * 0.12f, 0.32f + B * 0.10f);
			ApplyPBR(MC, IceRockCol, 0.08f, 0.88f,
				FLinearColor(0.01f, 0.03f, 0.06f));
		}
		else
		{
			ApplyPBR(MC, Col, Metal, Rough);
		}
		O->AddInstanceComponent(MC);
		return MC;
	};

	// ================================================================
	// Peak definitions — 23 peaks in two rows
	// ================================================================
	struct FPeakData { float X; float Y; float Height; float RadX; float RadY; };

	const FPeakData Peaks[] = {
		// ---- Northern front row — centered at X:-436, Y:101303 (pushed toward coast) ----
		{ -22436.f, 101303.f, 14500.f, 4800.f, 4200.f },
		{ -19686.f,  98303.f, 10200.f, 3900.f, 3500.f },
		{ -16936.f, 103303.f, 16800.f, 5400.f, 4800.f },
		{ -14186.f,  97303.f, 11500.f, 4200.f, 3700.f },
		{ -11436.f, 102303.f, 17500.f, 5700.f, 5100.f },
		{  -8686.f,  99303.f,  8800.f, 3600.f, 3300.f },
		{  -5936.f, 104303.f, 15200.f, 5100.f, 4500.f },
		{  -3186.f,  98803.f, 12000.f, 4300.f, 3900.f },
		{   -436.f, 102803.f, 17000.f, 5500.f, 4900.f },
		{   2314.f,  97803.f,  9500.f, 3700.f, 3500.f },
		{   5064.f, 101803.f, 15800.f, 5300.f, 4700.f },
		{   7814.f, 100303.f,  7200.f, 3300.f, 3000.f },
		{  10564.f, 103803.f, 13500.f, 4500.f, 4100.f },
		{  13314.f,  98303.f, 16200.f, 5400.f, 4800.f },
		{  16064.f, 101303.f, 10800.f, 4100.f, 3700.f },
		{  18814.f,  99803.f, 14800.f, 4900.f, 4300.f },
		{  21564.f, 102803.f,  5800.f, 3000.f, 2700.f },
		// ---- Northern back row ----
		{ -18436.f, 109303.f,  6500.f, 3100.f, 2900.f },
		{ -11236.f, 111303.f,  9000.f, 3900.f, 3300.f },
		{  -4036.f, 110303.f,  7800.f, 3500.f, 3100.f },
		{   3164.f, 112303.f, 10500.f, 4200.f, 3600.f },
		{  10364.f, 109803.f,  8200.f, 3600.f, 3300.f },
		{  17564.f, 108303.f,  7500.f, 3300.f, 3000.f },
		// ---- South edge of Christmas area (X:21564 → 63500, Y:~107000) ----
		{  24500.f, 107500.f, 12000.f, 4500.f, 4000.f },
		{  29000.f, 108800.f, 15500.f, 5200.f, 4600.f },
		{  33500.f, 107000.f, 10800.f, 4000.f, 3600.f },
		{  38000.f, 109000.f, 16000.f, 5400.f, 4800.f },
		{  42500.f, 107800.f,  9200.f, 3800.f, 3400.f },
		{  47000.f, 108500.f, 14200.f, 4800.f, 4200.f },
		{  51500.f, 107200.f, 11500.f, 4200.f, 3800.f },
		{  56000.f, 109200.f, 13800.f, 4600.f, 4100.f },
		{  60500.f, 107500.f, 10200.f, 4000.f, 3500.f },
		{  64000.f, 108800.f, 15000.f, 5100.f, 4500.f },
		// ---- East edge (X:~65000, Y:109000 → 125000) ----
		{  66000.f, 111000.f, 14500.f, 4800.f, 4200.f },
		{  65500.f, 115000.f, 11800.f, 4300.f, 3800.f },
		{  66500.f, 119000.f, 16200.f, 5300.f, 4700.f },
		{  65000.f, 123000.f, 12500.f, 4500.f, 4000.f },
		{  66000.f, 126500.f, 13000.f, 4600.f, 4100.f },
		// ---- North edge (Y:~127000, X:14500 → 63500) ----
		{  16000.f, 127500.f, 13500.f, 4700.f, 4100.f },
		{  20500.f, 128800.f, 10800.f, 4100.f, 3700.f },
		{  25000.f, 127000.f, 16500.f, 5500.f, 4900.f },
		{  29500.f, 128500.f,  9500.f, 3800.f, 3400.f },
		{  34000.f, 127200.f, 15200.f, 5200.f, 4600.f },
		{  38500.f, 128000.f, 11000.f, 4200.f, 3700.f },
		{  43000.f, 127500.f, 14800.f, 4900.f, 4300.f },
		{  47500.f, 128800.f, 10200.f, 4000.f, 3600.f },
		{  52000.f, 127000.f, 16000.f, 5400.f, 4800.f },
		{  56500.f, 128500.f,  8800.f, 3600.f, 3200.f },
		{  61000.f, 127200.f, 13200.f, 4600.f, 4000.f },
		// ---- West edge (X:~12500, Y:112000 → 127000) ----
		{  12500.f, 114000.f, 12800.f, 4400.f, 3900.f },
		{  13000.f, 118500.f, 15500.f, 5200.f, 4600.f },
		{  12000.f, 123000.f, 10500.f, 4000.f, 3500.f },
		{  13500.f, 127000.f, 14000.f, 4700.f, 4200.f },
	};
	const int32 NumPeaks = UE_ARRAY_COUNT(Peaks);

	// ================================================================
	// Color palette — layered geological look
	// ================================================================
	const FLinearColor SnowFresh(0.88f, 0.92f, 0.97f);   // bright ice-white snow
	const FLinearColor SnowOld(0.72f, 0.78f, 0.88f);     // packed blue-tinted ice


	// ================================================================
	// SNOWFALL & GROUND ACCUMULATION — northern mountain range
	// Visible snowfall (suspended white spheres) and snow ground cover
	// ================================================================
	{
		const float SnowMinX =  14500.f;
		const float SnowMaxX =  63500.f;
		const float SnowMinY = 109000.f;
		const float SnowMaxY = 125000.f;

		AActor* SnowActor = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(39000.f, 126000.f, 0.f), FRotator::ZeroRotator);
		if (SnowActor)
		{
#if WITH_EDITOR
			SnowActor->SetActorLabel(TEXT("NorthernSnowfall"));
#endif
			int32 SnowCompIdx = 0;

			// --- Snowfall particles: consume random stream to preserve determinism ---
			for (int32 SF = 0; SF < 400; ++SF)
			{
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
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
				SnowPatch->SetBoundsScale(50.f);
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
					Drift->SetBoundsScale(50.f);
					Drift->RegisterComponent();
					ApplyPBR(Drift, (D % 2 == 0) ? SnowFresh : SnowOld, 0.f, 0.50f);
					SnowActor->AddInstanceComponent(Drift);
				}
			}

			// --- Dense snowfall clusters: consume random stream to preserve determinism ---
			for (int32 CF = 0; CF < 150; ++CF)
			{
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
			}
		}
	}

	// ================================================================
	// MAGICAL CHRISTMAS WONDERLAND — expansive area beside snowy mountains
	// ================================================================
	{
		// Christmas area centered at X:39000, Y:117000
		const float XmasMinX =  14500.f;
		const float XmasMaxX =  63500.f;
		const float XmasMinY = 109000.f;
		const float XmasMaxY = 125000.f;

		// --- SNOW GROUND COVER ---
		AActor* SnowGround = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(39000.f, 117000.f, 0.f), FRotator::ZeroRotator);
		if (SnowGround)
		{
#if WITH_EDITOR
			SnowGround->SetActorLabel(TEXT("XmasSnowGround"));
#endif
			// Ice sheet covering the entire christmas wonderland
			UStaticMeshComponent* SGRoot = MC_Cube(SnowGround,
				FVector(39000.f, 117000.f, -5.f),
				FVector(300.f, 200.f, 0.15f),
				FRotator::ZeroRotator,
				FLinearColor(0.75f, 0.88f, 0.95f), 0.05f, 0.85f);
			SnowGround->SetRootComponent(SGRoot);
			// Add subtle icy sheen emissive
			if (UMaterialInstanceDynamic* MI = Cast<UMaterialInstanceDynamic>(SGRoot->GetMaterial(0)))
			{
				MI->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.08f, 0.12f, 0.25f));
			}

			// Consume random stream for removed snow patches (80 patches × 5 calls each)
			for (int32 S = 0; S < 80; ++S)
			{
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
			}

			// Consume random stream for removed snow drifts (50 drifts × 5 calls each)
			for (int32 D = 0; D < 50; ++D)
			{
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
				CityRandom.FRandRange(0.f, 1.f);
			}
		}

		// ---- GIANT CHRISTMAS TREES ----
		// 5 giant trees spread across the expanded area (reduced from 10)
		struct FXmasTreeDef { float X; float Y; float Height; };
		const FXmasTreeDef Trees[] = {
			{  61500.f, 111000.f, 12000.f },
			{  16500.f, 123000.f, 12000.f },
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

			// C. REFLECTIVE GOLD COIN ON TOP — mirror-polished gold (matches collectible coins)
			{
				const float StarZ = TrunkH + TH * 0.92f;
				FString SN = FString::Printf(TEXT("Mtn_%d"), CompIdx++);
				UStaticMeshComponent* CoinComp = NewObject<UStaticMeshComponent>(TA, *SN);
				CoinComp->SetStaticMesh(CylinderMesh);
				CoinComp->SetMaterial(0, ColorMaterial);
				CoinComp->SetWorldLocation(FVector(TD.X, TD.Y, StarZ));
				CoinComp->SetWorldScale3D(FVector(7.f, 7.f, 0.7f));
				CoinComp->SetWorldRotation(FRotator(90.f, 0.f, 0.f));
				CoinComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				CoinComp->bNeverDistanceCull = true;
				CoinComp->SetBoundsScale(50.f);
				CoinComp->RegisterComponent();
				TA->AddInstanceComponent(CoinComp);
				ApplyPBR(CoinComp,
					FLinearColor(1.0f, 0.84f, 0.0f), 1.0f, 0.05f,
					FLinearColor::Black);
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
				Bulb->SetBoundsScale(50.f);
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
					Garl->SetBoundsScale(50.f);
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
		// 12 snowmen scattered within the christmas tree area
		struct FSnowmanDef { float X; float Y; float Scale; };
		const FSnowmanDef Snowmen[] = {
			{  15500.f, 117000.f, 1.0f },
			{  23500.f, 115000.f, 0.8f },
			{  30500.f, 123000.f, 1.1f },
			{  36500.f, 120000.f, 1.0f },
			{  40000.f, 118000.f, 1.3f },
			{  45500.f, 110000.f, 0.95f },
			{  49500.f, 113000.f, 1.1f },
			{  55500.f, 119000.f, 0.75f },
			{  61500.f, 116000.f, 0.8f },
			{  22500.f, 124000.f, 1.0f },
			{  44500.f, 123000.f, 1.15f },
			{  52500.f, 110000.f, 0.9f },
		};
		const int32 NumSnowmen = UE_ARRAY_COUNT(Snowmen);
		// Consume 18 random calls for removed snowmen to preserve determinism
		for (int32 Skip = 0; Skip < 18; ++Skip) { CityRandom.FRandRange(0.f, 1.f); }

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
			Bottom->SetBoundsScale(50.f);
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
			Mid->SetBoundsScale(50.f);
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
			Head->SetBoundsScale(50.f);
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
				Eye->SetBoundsScale(50.f);
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
				Mouth->SetBoundsScale(50.f);
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
				Button->SetBoundsScale(50.f);
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
			OrbMesh->SetBoundsScale(50.f);
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
			Hook->SetBoundsScale(50.f);
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
				BowComp->SetBoundsScale(50.f);
				BowComp->RegisterComponent();
				ApplyPBR(BowComp, RibbonColor, 0.35f, 0.3f);
				GBA->AddInstanceComponent(BowComp);
			}
		}

		// (Frozen pond removed — entire area is now a sheet of ice)

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
			Sparkle->SetBoundsScale(50.f);
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
			Globe->SetBoundsScale(50.f);
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
			Wreath->SetBoundsScale(50.f);
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
			Flake->SetBoundsScale(50.f);
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
		MC->SetBoundsScale(50.f);
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
		MC->SetBoundsScale(50.f);
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
		MC->SetBoundsScale(50.f);
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
		MC->SetBoundsScale(50.f);
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
		MC->SetBoundsScale(50.f);
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
		MC->SetBoundsScale(50.f);
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
		MC->SetBoundsScale(50.f);
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
			BeachMC->SetBoundsScale(30.f);
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

		// (Sand dune spheres removed — terrain provides natural elevation)
		// Consume random values to keep deterministic seed in sync (40 × 7 calls)
		for (int32 d = 0; d < 40; ++d)
		{
			CityRandom.FRandRange(0.f, 1.f); CityRandom.FRandRange(0.f, 1.f);
			CityRandom.FRandRange(0.f, 1.f); CityRandom.FRandRange(0.f, 1.f);
			CityRandom.FRandRange(0.f, 1.f); CityRandom.FRandRange(0.f, 1.f);
			CityRandom.FRandRange(0.f, 1.f);
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
				WZMC->SetBoundsScale(30.f);
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
	const FVector LakeCenter(-248405.f, 7389.f, 0.f);

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
		MC->SetBoundsScale(30.f);
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
			WMC->SetBoundsScale(30.f);
			WMC->RegisterComponent();
			WA->SetRootComponent(WMC);
			if (GlassMaterial)
			{
				ApplyPBRGlass(WMC, GlassMaterial,
					FLinearColor(0.06f, 0.18f, 0.32f), 0.65f, 0.15f, 0.08f,
					FLinearColor(0.03f, 0.06f, 0.12f));
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
		C->SetBoundsScale(30.f);
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

		// (Sand dune spheres removed — terrain provides natural elevation)
		// Consume random values to keep deterministic seed in sync (30 × 7 calls)
		for (int32 d = 0; d < 30; ++d)
		{
			CityRandom.FRandRange(0.f, 1.f); CityRandom.FRandRange(0.f, 1.f);
			CityRandom.FRandRange(0.f, 1.f); CityRandom.FRandRange(0.f, 1.f);
			CityRandom.FRandRange(0.f, 1.f); CityRandom.FRandRange(0.f, 1.f);
			CityRandom.FRandRange(0.f, 1.f);
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
		// Pyramid color palette — dark sand
		const FLinearColor LimestoneMain(0.55f, 0.48f, 0.32f);    // dark sand — main body
		const FLinearColor LimestoneDark(0.45f, 0.38f, 0.25f);    // darker sand — shadows
		const FLinearColor LimestoneLight(0.60f, 0.53f, 0.36f);   // lighter sand variation
		const FLinearColor CapstoneGold(0.58f, 0.50f, 0.30f);     // capstone — warm dark sand
		const FLinearColor DesertSand(0.55f, 0.48f, 0.32f);       // ground — dark sand
		const FLinearColor BasaltDark(0.35f, 0.30f, 0.20f);       // mortuary temple — deep sand
		const FLinearColor GraniteRed(0.45f, 0.38f, 0.25f);       // granite — dark sand tone

		const FVector PyramidAreaCenter(22248.f, -62419.f, 0.f);

		// (Desert sand ground plane removed — terrain chunks handle sand coloring)

		// --- Khufu (Great Pyramid) — the largest (2x scale) ---
		const FVector KhufuBase = PyramidAreaCenter + FVector(-10000.f, 0.f, 0.f);
		const float KhufuHeight = 14600.f;   // ~292m (2x)
		const float KhufuBaseW = 23000.f;    // ~460m (2x)

		// Tura Limestone material for all pyramid block surfaces
		const FLinearColor TuraLimestone(0.55f, 0.48f, 0.32f);  // dark sand tone
		const float TuraRough = 0.82f;   // rough hewn stone
		const float TuraMetal = 0.0f;    // non-metallic stone

		// Lambda: build a stepped Tura limestone block pyramid
		// Stacks flat cube layers from base to top, each slightly smaller
		auto BuildBlockPyramid = [&](const FVector& Base, float Height, float BaseWidth,
			const TCHAR* Prefix, int32 NumLayers)
		{
			const float LayerH = Height / (float)NumLayers;
			for (int32 L = 0; L < NumLayers; ++L)
			{
				const float Frac = (float)L / (float)NumLayers;
				const float W = BaseWidth * (1.0f - Frac);     // each layer narrows
				const float Z = L * LayerH + LayerH * 0.5f;    // center of this layer

				// Alternate between two limestone tones for visible block courses
				const FLinearColor& BlockColor = (L % 2 == 0) ? TuraLimestone
					: FLinearColor(0.50f, 0.43f, 0.28f);   // slightly darker alternate
				const float BlockRough = (L % 2 == 0) ? 0.82f : 0.88f;

				SpawnLakePiece(CubeMesh,
					Base + FVector(0.f, 0.f, Z),
					FRotator::ZeroRotator,
					FVector(W / 100.f, W / 100.f, LayerH / 100.f),
					BlockColor, BlockRough, TuraMetal, Prefix);
			}
		};

		// Build Khufu from 30 stacked limestone block layers
		BuildBlockPyramid(KhufuBase, KhufuHeight, KhufuBaseW, TEXT("Khufu_Block"), 30);

		// Gold pyramidion (capstone) at the very top
		SpawnLakePiece(CubeMesh,
			KhufuBase + FVector(0.f, 0.f, KhufuHeight - 200.f),
			FRotator::ZeroRotator,
			FVector(6.f, 6.f, 4.f),
			CapstoneGold, 0.4f, 0.5f, TEXT("Khufu_Capstone"));

		// Base platform — limestone foundation
		SpawnLakePiece(CubeMesh,
			KhufuBase + FVector(0.f, 0.f, -15.f),
			FRotator::ZeroRotator,
			FVector(KhufuBaseW * 1.15f / 100.f, KhufuBaseW * 1.15f / 100.f, 0.3f),
			LimestoneDark, 0.92f, 0.f, TEXT("Khufu_Platform"));

		// Mortuary temple — east side of Khufu
		const FVector TemplePos = KhufuBase + FVector(KhufuBaseW * 0.6f + 1000.f, 0.f, 0.f);
		// Temple base
		SpawnLakePiece(CubeMesh, TemplePos + FVector(0.f, 0.f, 300.f),
			FRotator::ZeroRotator,
			FVector(40.f, 60.f, 6.f),
			BasaltDark, 0.88f, 0.f, TEXT("Khufu_TempleBase"));
		// Temple walls (4 sides)
		SpawnLakePiece(CubeMesh, TemplePos + FVector(0.f, 2800.f, 700.f),
			FRotator::ZeroRotator,
			FVector(40.f, 1.f, 8.f),
			GraniteRed, 0.85f, 0.f, TEXT("Khufu_TempleWallN"));
		SpawnLakePiece(CubeMesh, TemplePos + FVector(0.f, -2800.f, 700.f),
			FRotator::ZeroRotator,
			FVector(40.f, 1.f, 8.f),
			GraniteRed, 0.85f, 0.f, TEXT("Khufu_TempleWallS"));
		SpawnLakePiece(CubeMesh, TemplePos + FVector(1900.f, 0.f, 700.f),
			FRotator::ZeroRotator,
			FVector(1.f, 60.f, 8.f),
			GraniteRed, 0.85f, 0.f, TEXT("Khufu_TempleWallE"));
		SpawnLakePiece(CubeMesh, TemplePos + FVector(-1900.f, 0.f, 700.f),
			FRotator::ZeroRotator,
			FVector(1.f, 60.f, 8.f),
			GraniteRed, 0.85f, 0.f, TEXT("Khufu_TempleWallW"));
		// Temple columns (6 pillars)
		for (int32 c = 0; c < 6; ++c)
		{
			const float ColY = -2400.f + (float)c * 960.f;
			SpawnLakePiece(CylinderMesh,
				TemplePos + FVector(0.f, ColY, 500.f),
				FRotator::ZeroRotator,
				FVector(2.4f, 2.4f, 8.f),
				LimestoneLight, 0.80f, 0.f, TEXT("Khufu_TempleCol"));
		}

		// Causeway — long raised walkway from temple toward the Nile (east)
		SpawnLakePiece(CubeMesh,
			TemplePos + FVector(10000.f, 0.f, 160.f),
			FRotator::ZeroRotator,
			FVector(160.f, 10.f, 3.f),
			LimestoneDark, 0.92f, 0.f, TEXT("Khufu_Causeway"));
		// Causeway walls
		SpawnLakePiece(CubeMesh,
			TemplePos + FVector(10000.f, 460.f, 360.f),
			FRotator::ZeroRotator,
			FVector(160.f, 0.6f, 4.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Khufu_CausewayWallN"));
		SpawnLakePiece(CubeMesh,
			TemplePos + FVector(10000.f, -460.f, 360.f),
			FRotator::ZeroRotator,
			FVector(160.f, 0.6f, 4.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Khufu_CausewayWallS"));

		// --- Khufu Interior — Mirror cone + floor matching outer pyramid shape ---
		{
			const FLinearColor ChromeMirror(0.92f, 0.92f, 0.95f);

			// Mirror floor matching pyramid base (sunk below terrain so it's always underfoot)
			SpawnLakePiece(CubeMesh, KhufuBase + FVector(0.f, 0.f, -200.f),
				FRotator::ZeroRotator,
				FVector(KhufuBaseW * 0.98f / 100.f, KhufuBaseW * 0.98f / 100.f, 2.f),
				ChromeMirror, 0.02f, 1.0f, TEXT("Khufu_MirrorFloor"));
			// Mirror inner cone walls matching outer pyramid shape
			SpawnLakePiece(ConeMesh, KhufuBase + FVector(0.f, 0.f, KhufuHeight * 0.5f),
				FRotator::ZeroRotator,
				FVector(KhufuBaseW * 0.98f / 100.f, KhufuBaseW * 0.98f / 100.f, KhufuHeight * 0.98f / 100.f),
				ChromeMirror, 0.02f, 1.0f, TEXT("Khufu_MirrorCone"));

			// Floating emissive orbs — 16 spheres in two concentric rings
			for (int32 o = 0; o < 16; ++o)
			{
				const bool bOuter = (o < 8);
				const float Radius = bOuter ? 3200.f : 1600.f;
				const int32 Ring = bOuter ? o : (o - 8);
				const int32 RingCount = 8;
				const float Angle = FMath::DegreesToRadians((float)Ring / (float)RingCount * 360.f);
				const float Heights[] = {800.f, 1600.f, 2400.f, 3200.f, 4000.f, 4800.f, 1200.f, 3600.f};
				const float OrbZ = Heights[Ring];
				const float OrbScale = bOuter ? (1.5f + (float)(Ring % 3) * 0.5f) : (1.0f + (float)(Ring % 4) * 0.4f);

				const FVector OrbPos = KhufuBase + FVector(
					FMath::Cos(Angle) * Radius,
					FMath::Sin(Angle) * Radius,
					OrbZ);

				UStaticMeshComponent* MC = SpawnLakePiece(SphereMesh, OrbPos,
					FRotator::ZeroRotator,
					FVector(OrbScale, OrbScale, OrbScale),
					FLinearColor(1.f, 1.f, 1.f), 0.05f, 0.f, TEXT("Khufu_Orb"));
				if (MC)
				{
					UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MC->GetMaterial(0));
					if (MID)
					{
						MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(2.f, 0.f, 0.f));
						PyramidInteriorMIDs.Add(MID);
					}
				}
			}

			// Emissive light panels — 8 tall thin slabs, 2 per wall
			for (int32 p = 0; p < 8; ++p)
			{
				const int32 Wall = p / 2;
				const float Offset = (p % 2 == 0) ? -1500.f : 1500.f;
				FVector PanelPos;
				FVector PanelScale;
				switch (Wall)
				{
				case 0: // North
					PanelPos = KhufuBase + FVector(Offset, 3800.f, 2800.f);
					PanelScale = FVector(1.5f, 0.2f, 45.f);
					break;
				case 1: // South
					PanelPos = KhufuBase + FVector(Offset, -3800.f, 2800.f);
					PanelScale = FVector(1.5f, 0.2f, 45.f);
					break;
				case 2: // East
					PanelPos = KhufuBase + FVector(3800.f, Offset, 2800.f);
					PanelScale = FVector(0.2f, 1.5f, 45.f);
					break;
				default: // West
					PanelPos = KhufuBase + FVector(-3800.f, Offset, 2800.f);
					PanelScale = FVector(0.2f, 1.5f, 45.f);
					break;
				}

				UStaticMeshComponent* MC = SpawnLakePiece(CubeMesh, PanelPos,
					FRotator::ZeroRotator,
					PanelScale,
					FLinearColor(1.f, 1.f, 1.f), 0.05f, 0.f, TEXT("Khufu_Panel"));
				if (MC)
				{
					UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MC->GetMaterial(0));
					if (MID)
					{
						MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.f, 2.f, 0.f));
						PyramidInteriorMIDs.Add(MID);
					}
				}
			}

			// Central crystal — large glowing sphere at room center
			{
				UStaticMeshComponent* MC = SpawnLakePiece(SphereMesh,
					KhufuBase + FVector(0.f, 0.f, 2500.f),
					FRotator::ZeroRotator,
					FVector(5.f, 5.f, 5.f),
					FLinearColor(1.f, 1.f, 1.f), 0.05f, 0.3f, TEXT("Khufu_Crystal"));
				if (MC)
				{
					UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(MC->GetMaterial(0));
					if (MID)
					{
						MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(3.f, 1.f, 3.f));
						PyramidInteriorMIDs.Add(MID);
					}
				}
			}
		}

		// --- Khafre (second pyramid) — slightly shorter but appears taller (2x scale) ---
		const FVector KhafreBase = PyramidAreaCenter + FVector(16000.f, -6000.f, 0.f);
		const float KhafreHeight = 13600.f;   // ~272m (2x)
		const float KhafreBaseW = 21500.f;    // ~430m (2x)

		// Build Khafre from 28 stacked limestone block layers
		BuildBlockPyramid(KhafreBase, KhafreHeight, KhafreBaseW, TEXT("Khafre_Block"), 28);

		// Gold pyramidion (capstone)
		SpawnLakePiece(CubeMesh,
			KhafreBase + FVector(0.f, 0.f, KhafreHeight - 180.f),
			FRotator::ZeroRotator,
			FVector(5.f, 5.f, 3.5f),
			CapstoneGold, 0.4f, 0.5f, TEXT("Khafre_Capstone"));

		// Platform
		SpawnLakePiece(CubeMesh,
			KhafreBase + FVector(0.f, 0.f, -15.f),
			FRotator::ZeroRotator,
			FVector(KhafreBaseW * 1.12f / 100.f, KhafreBaseW * 1.12f / 100.f, 0.3f),
			LimestoneDark, 0.92f, 0.f, TEXT("Khafre_Platform"));

		// --- Khafre Interior — Mirror cone + floor matching outer pyramid shape ---
		{
			const FLinearColor ChromeMirror(0.92f, 0.92f, 0.95f);
			// Mirror floor matching pyramid base (sunk below terrain so it's always underfoot)
			SpawnLakePiece(CubeMesh, KhafreBase + FVector(0.f, 0.f, -200.f),
				FRotator::ZeroRotator,
				FVector(KhafreBaseW * 0.98f / 100.f, KhafreBaseW * 0.98f / 100.f, 2.f),
				ChromeMirror, 0.02f, 1.0f, TEXT("Khafre_MirrorFloor"));
			// Mirror inner cone walls matching outer pyramid shape
			SpawnLakePiece(ConeMesh, KhafreBase + FVector(0.f, 0.f, KhafreHeight * 0.5f),
				FRotator::ZeroRotator,
				FVector(KhafreBaseW * 0.98f / 100.f, KhafreBaseW * 0.98f / 100.f, KhafreHeight * 0.98f / 100.f),
				ChromeMirror, 0.02f, 1.0f, TEXT("Khafre_MirrorCone"));
		}

		// Khafre mortuary temple — east side (2x)
		const FVector KTemplePos = KhafreBase + FVector(KhafreBaseW * 0.55f + 800.f, 0.f, 0.f);
		SpawnLakePiece(CubeMesh, KTemplePos + FVector(0.f, 0.f, 240.f),
			FRotator::ZeroRotator,
			FVector(30.f, 44.f, 4.8f),
			BasaltDark, 0.88f, 0.f, TEXT("Khafre_TempleBase"));
		for (int32 c = 0; c < 4; ++c)
		{
			const float ColY = -1800.f + (float)c * 1200.f;
			SpawnLakePiece(CylinderMesh,
				KTemplePos + FVector(0.f, ColY, 400.f),
				FRotator::ZeroRotator,
				FVector(2.0f, 2.0f, 7.0f),
				LimestoneLight, 0.80f, 0.f, TEXT("Khafre_TempleCol"));
		}

		// --- Great Sphinx — between Khufu and Khafre (2x scale) ---
		// Body: elongated cube laid on its side
		const FVector SphinxPos = FMath::Lerp(KhufuBase, KhafreBase, 0.5f) + FVector(KhufuBaseW * 0.5f + 4000.f, 4000.f, 0.f);
		// Body (lion body)
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(0.f, 0.f, 800.f),
			FRotator::ZeroRotator,
			FVector(70.f, 20.f, 10.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Sphinx_Body"));
		// Haunches (rear, raised)
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(-2400.f, 0.f, 1100.f),
			FRotator::ZeroRotator,
			FVector(20.f, 20.f, 16.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Sphinx_Haunches"));
		// Head — sphere
		SpawnLakePiece(SphereMesh, SphinxPos + FVector(2800.f, 0.f, 1800.f),
			FRotator::ZeroRotator,
			FVector(14.f, 12.f, 16.f),
			LimestoneLight, 0.85f, 0.f, TEXT("Sphinx_Head"));
		// Headdress (nemes) — two slabs flanking the head
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(2800.f, 700.f, 1500.f),
			FRotator(0.f, 0.f, -10.f),
			FVector(6.f, 2.f, 20.f),
			CapstoneGold, 0.70f, 0.15f, TEXT("Sphinx_NemesL"));
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(2800.f, -700.f, 1500.f),
			FRotator(0.f, 0.f, 10.f),
			FVector(6.f, 2.f, 20.f),
			CapstoneGold, 0.70f, 0.15f, TEXT("Sphinx_NemesR"));
		// Front paws — two elongated cubes forward
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(4400.f, 600.f, 300.f),
			FRotator::ZeroRotator,
			FVector(24.f, 5.f, 4.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Sphinx_PawL"));
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(4400.f, -600.f, 300.f),
			FRotator::ZeroRotator,
			FVector(24.f, 5.f, 4.f),
			LimestoneMain, 0.90f, 0.f, TEXT("Sphinx_PawR"));
		// Dream Stele — small slab between the paws
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(4200.f, 0.f, 500.f),
			FRotator::ZeroRotator,
			FVector(0.6f, 6.f, 8.f),
			GraniteRed, 0.80f, 0.f, TEXT("Sphinx_Stele"));
		// Sphinx enclosure walls
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(0.f, 1600.f, 400.f),
			FRotator::ZeroRotator,
			FVector(80.f, 1.f, 8.f),
			LimestoneDark, 0.92f, 0.f, TEXT("Sphinx_WallN"));
		SpawnLakePiece(CubeMesh, SphinxPos + FVector(0.f, -1600.f, 400.f),
			FRotator::ZeroRotator,
			FVector(80.f, 1.f, 8.f),
			LimestoneDark, 0.92f, 0.f, TEXT("Sphinx_WallS"));

		// --- Menkaure (smallest pyramid, 2x scale) ---
		const FVector MenkaureBase = PyramidAreaCenter + FVector(36000.f, -14000.f, 0.f);
		const float MenkaureHeight = 6500.f;  // ~130m (2x)
		const float MenkaureBaseW = 10500.f;  // ~210m (2x)

		// Build Menkaure from 20 stacked limestone block layers
		BuildBlockPyramid(MenkaureBase, MenkaureHeight, MenkaureBaseW, TEXT("Menkaure_Block"), 20);

		// Gold capstone
		SpawnLakePiece(CubeMesh,
			MenkaureBase + FVector(0.f, 0.f, MenkaureHeight - 100.f),
			FRotator::ZeroRotator,
			FVector(3.f, 3.f, 2.f),
			FLinearColor(0.58f, 0.50f, 0.30f), 0.3f, 0.5f, TEXT("Menkaure_Capstone"));

		// Platform
		SpawnLakePiece(CubeMesh,
			MenkaureBase + FVector(0.f, 0.f, -15.f),
			FRotator::ZeroRotator,
			FVector(MenkaureBaseW * 1.1f / 100.f, MenkaureBaseW * 1.1f / 100.f, 0.3f),
			LimestoneDark, 0.92f, 0.f, TEXT("Menkaure_Platform"));

		// --- Menkaure Interior — Mirror cone + floor matching outer pyramid shape ---
		{
			const FLinearColor ChromeMirror(0.92f, 0.92f, 0.95f);
			// Mirror floor matching pyramid base (sunk below terrain so it's always underfoot)
			SpawnLakePiece(CubeMesh, MenkaureBase + FVector(0.f, 0.f, -200.f),
				FRotator::ZeroRotator,
				FVector(MenkaureBaseW * 0.98f / 100.f, MenkaureBaseW * 0.98f / 100.f, 2.f),
				ChromeMirror, 0.02f, 1.0f, TEXT("Menkaure_MirrorFloor"));
			// Mirror inner cone walls matching outer pyramid shape
			SpawnLakePiece(ConeMesh, MenkaureBase + FVector(0.f, 0.f, MenkaureHeight * 0.5f),
				FRotator::ZeroRotator,
				FVector(MenkaureBaseW * 0.98f / 100.f, MenkaureBaseW * 0.98f / 100.f, MenkaureHeight * 0.98f / 100.f),
				ChromeMirror, 0.02f, 1.0f, TEXT("Menkaure_MirrorCone"));
		}

		// Three Queens' Pyramids (small satellites south of Menkaure, 2x)
		for (int32 q = 0; q < 3; ++q)
		{
			const FVector QBase = MenkaureBase + FVector((float)q * 5000.f - 5000.f, -MenkaureBaseW * 0.6f - 4000.f, 0.f);
			const float QH = 2400.f + (float)q * 400.f;
			const float QW = 4000.f + (float)q * 600.f;

			// Build each queen's pyramid from stacked limestone blocks
			BuildBlockPyramid(QBase, QH, QW,
				*FString::Printf(TEXT("Queen_%d_Block"), q), 12);

			// Gold capstone
			SpawnLakePiece(CubeMesh, QBase + FVector(0.f, 0.f, QH - 60.f),
				FRotator::ZeroRotator,
				FVector(2.f, 2.f, 1.2f),
				FLinearColor(0.58f, 0.50f, 0.30f), 0.3f, 0.5f,
				*FString::Printf(TEXT("Queen_%d_Capstone"), q));
		}

		// --- Torch flames on pyramid faces — base, mid, upper, apex tiers ---
		// Outlines each pyramid's silhouette with fire at night.
		// Torches sit directly on the pyramid surface (no pole — just flame + light).
		{
			struct FPyramidInfo { FVector Base; float HalfW; float Height; };
			const FPyramidInfo Pyramids[] = {
				{ KhufuBase,    KhufuBaseW * 0.5f,    KhufuHeight },
				{ KhafreBase,   KhafreBaseW * 0.5f,    KhafreHeight },
				{ MenkaureBase, MenkaureBaseW * 0.5f,  MenkaureHeight },
			};

			const FLinearColor PCharredWood(0.05f, 0.03f, 0.02f);
			const FLinearColor PFlameCore(1.0f, 0.95f, 0.7f);
			const FLinearColor PFlameMid(1.0f, 0.55f, 0.05f);
			const FLinearColor PFlameOuter(0.9f, 0.25f, 0.02f);
			const FLinearColor PCoreEmissive(20.f, 16.f, 6.f);
			const FLinearColor PMidEmissive(14.f, 5.f, 0.5f);
			const FLinearColor POuterEmissive(8.f, 2.f, 0.2f);

			int32 PTorchIdx = 0;

			// Height tiers as fraction of pyramid height, with lateral spread factor
			// Format: { heightFrac, spreadFrac (how far from center as fraction of width at that height) }
			struct FTorchTier { float HeightFrac; float SpreadFrac; };
			const FTorchTier Tiers[] = {
				{ 0.15f, 0.80f },   // base tier — 4 torches, wide spread along edges
				{ 0.15f, 0.40f },   // base tier inner — 4 more torches, mid-edge
				{ 0.35f, 0.65f },   // lower-mid — 4 torches along edges
				{ 0.55f, 0.45f },   // mid tier — 4 torches, edges narrowing
				{ 0.75f, 0.30f },   // upper tier — 4 torches near edges
				{ 0.90f, 0.00f },   // near apex — 1 torch at center of each face
			};

			// 4 face directions (center of each face)
			const FVector2D FaceDirs[] = {
				FVector2D(1.f, 0.f), FVector2D(-1.f, 0.f),
				FVector2D(0.f, 1.f), FVector2D(0.f, -1.f)
			};

			auto SpawnPyramidTorch = [&](const FVector& Pos)
			{
				AActor* TA = World->SpawnActor<AActor>(
					AActor::StaticClass(), Pos, FRotator::ZeroRotator);
				if (!TA) return;

				// Charred bowl/mount on the pyramid surface
				UStaticMeshComponent* Bowl = NewObject<UStaticMeshComponent>(
					TA, *FString::Printf(TEXT("PyTBowl_%d"), PTorchIdx));
				Bowl->SetStaticMesh(CylinderMesh);
				Bowl->SetWorldLocation(Pos);
				Bowl->SetWorldScale3D(FVector(0.45f, 0.45f, 0.20f));
				Bowl->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Bowl->SetBoundsScale(30.f);
				Bowl->RegisterComponent();
				ApplyPBR(Bowl, PCharredWood, 0.f, 0.95f, FLinearColor(0.8f, 0.2f, 0.02f));
				TA->SetRootComponent(Bowl);
				TA->AddInstanceComponent(Bowl);

				// Flame core — white-hot
				UStaticMeshComponent* FCore = NewObject<UStaticMeshComponent>(
					TA, *FString::Printf(TEXT("PyTFCore_%d"), PTorchIdx));
				FCore->SetStaticMesh(ConeMesh);
				FCore->SetWorldLocation(Pos + FVector(0.f, 0.f, 30.f));
				FCore->SetWorldScale3D(FVector(0.30f, 0.30f, 1.8f));
				FCore->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				FCore->SetBoundsScale(30.f);
				FCore->RegisterComponent();
				ApplyPBR(FCore, PFlameCore, 0.f, 0.15f, PCoreEmissive);
				TA->AddInstanceComponent(FCore);

				// Mid flame — orange
				UStaticMeshComponent* FMid = NewObject<UStaticMeshComponent>(
					TA, *FString::Printf(TEXT("PyTFMid_%d"), PTorchIdx));
				FMid->SetStaticMesh(ConeMesh);
				FMid->SetWorldLocation(Pos + FVector(0.f, 0.f, 20.f));
				FMid->SetWorldScale3D(FVector(0.50f, 0.50f, 2.2f));
				FMid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				FMid->SetBoundsScale(30.f);
				FMid->RegisterComponent();
				if (GlassMaterial)
					ApplyPBRGlass(FMid, GlassMaterial, PFlameMid, 0.6f, 0.f, 0.1f, PMidEmissive);
				TA->AddInstanceComponent(FMid);

				// Outer flame — red-orange
				UStaticMeshComponent* FOuter = NewObject<UStaticMeshComponent>(
					TA, *FString::Printf(TEXT("PyTFOut_%d"), PTorchIdx));
				FOuter->SetStaticMesh(ConeMesh);
				FOuter->SetWorldLocation(Pos + FVector(0.f, 0.f, 10.f));
				FOuter->SetWorldScale3D(FVector(0.70f, 0.70f, 2.8f));
				FOuter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				FOuter->SetBoundsScale(30.f);
				FOuter->RegisterComponent();
				if (GlassMaterial)
					ApplyPBRGlass(FOuter, GlassMaterial, PFlameOuter, 0.4f, 0.f, 0.1f, POuterEmissive);
				TA->AddInstanceComponent(FOuter);

				// Ember glow sphere
				UStaticMeshComponent* Ember = NewObject<UStaticMeshComponent>(
					TA, *FString::Printf(TEXT("PyTEmber_%d"), PTorchIdx));
				Ember->SetStaticMesh(SphereMesh);
				Ember->SetWorldLocation(Pos + FVector(0.f, 0.f, 15.f));
				Ember->SetWorldScale3D(FVector(0.55f, 0.55f, 0.35f));
				Ember->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Ember->SetBoundsScale(30.f);
				Ember->RegisterComponent();
				ApplyPBR(Ember, FLinearColor(1.f, 0.4f, 0.02f), 0.f, 0.2f,
					FLinearColor(12.f, 4.f, 0.3f));
				TA->AddInstanceComponent(Ember);

				// Point light — warm fire glow
				UPointLightComponent* TLight = NewObject<UPointLightComponent>(
					TA, *FString::Printf(TEXT("PyTLight_%d"), PTorchIdx));
				TLight->SetWorldLocation(Pos + FVector(0.f, 0.f, 120.f));
				TLight->SetIntensity(8000.f);
				TLight->SetAttenuationRadius(5000.f);
				TLight->SetLightColor(FColor(255, 140, 40));
				TLight->SetCastShadows(false);
				TLight->RegisterComponent();
				TA->AddInstanceComponent(TLight);

				++PTorchIdx;
			};

			for (const FPyramidInfo& P : Pyramids)
			{
				for (const FTorchTier& Tier : Tiers)
				{
					const float Z = P.Base.Z + Tier.HeightFrac * P.Height;
					// Width of pyramid at this height (narrows linearly)
					const float WidthAtH = P.HalfW * (1.f - Tier.HeightFrac);

					if (Tier.SpreadFrac < 0.01f)
					{
						// Apex tier — one torch per face, right at center
						for (int32 F = 0; F < 4; ++F)
						{
							const FVector2D& Dir = FaceDirs[F];
							const FVector TPos = FVector(
								P.Base.X + Dir.X * WidthAtH * 0.5f,
								P.Base.Y + Dir.Y * WidthAtH * 0.5f,
								Z);
							SpawnPyramidTorch(TPos);
						}
					}
					else
					{
						// Edge tier — one torch per face, offset along the edge
						for (int32 F = 0; F < 4; ++F)
						{
							const FVector2D& Dir = FaceDirs[F];
							// Face outward position (on the surface)
							const float FaceOut = WidthAtH + 50.f;
							// Lateral offset along the edge
							const float Lateral = WidthAtH * Tier.SpreadFrac;
							const FVector2D Perp(Dir.Y, -Dir.X);

							// Two torches per face at this tier, symmetrically placed
							for (int32 S = 0; S < 2; ++S)
							{
								const float Side = (S == 0) ? -Lateral : Lateral;
								const FVector TPos = FVector(
									P.Base.X + Dir.X * FaceOut + Perp.X * Side,
									P.Base.Y + Dir.Y * FaceOut + Perp.Y * Side,
									Z);
								SpawnPyramidTorch(TPos);
							}
						}
					}
				}
			}
			UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Spawned %d pyramid surface torches."), PTorchIdx);
		}

		// Scattered rubble/fallen blocks around the pyramids (2x range & size)
		for (int32 r = 0; r < 30; ++r)
		{
			const FVector RubbleCenter = PyramidAreaCenter + FVector(
				CityRandom.FRandRange(-30000.f, 50000.f),
				CityRandom.FRandRange(-24000.f, 10000.f),
				0.f);
			const float RS = CityRandom.FRandRange(60.f, 240.f);
			SpawnLakePiece(CubeMesh, RubbleCenter + FVector(0.f, 0.f, RS * 0.3f),
				FRotator(CityRandom.FRandRange(-15.f, 15.f),
					CityRandom.FRandRange(0.f, 360.f),
					CityRandom.FRandRange(-10.f, 10.f)),
				FVector(RS / 100.f, RS * CityRandom.FRandRange(0.6f, 1.2f) / 100.f,
					RS * CityRandom.FRandRange(0.4f, 0.8f) / 100.f),
				FMath::Lerp(LimestoneDark, LimestoneMain, CityRandom.FRandRange(0.f, 1.f)),
				0.95f, 0.f, TEXT("Pyramid_Rubble"));
		}

		// Obelisks (2) — tall thin monuments near the causeway (2x)
		for (int32 ob = 0; ob < 2; ++ob)
		{
			const FVector ObBase = TemplePos + FVector(4000.f, (ob == 0 ? 1600.f : -1600.f), 0.f);
			// Shaft
			SpawnLakePiece(CubeMesh, ObBase + FVector(0.f, 0.f, 2000.f),
				FRotator::ZeroRotator,
				FVector(2.4f, 2.4f, 40.f),
				GraniteRed, 0.75f, 0.1f,
				*FString::Printf(TEXT("Obelisk_%d_Shaft"), ob));
			// Pyramidion tip
			SpawnLakePiece(ConeMesh, ObBase + FVector(0.f, 0.f, 4100.f),
				FRotator::ZeroRotator,
				FVector(3.f, 3.f, 4.f),
				CapstoneGold, 0.35f, 0.7f,
				*FString::Printf(TEXT("Obelisk_%d_Tip"), ob));
		}
	}

	// ================================================================
	// GIANT RIVER — runs east-west between pyramids and jungle,
	// connecting both oceans
	// ================================================================
	{
		const float RiverCenterY = -97000.f;
		const float RiverWidth = 4000.f;
		const float RiverStartX = -145000.f;
		const float RiverEndX = 145000.f;
		const float SegLen = 20000.f;
		const int32 NumSegs = FMath::CeilToInt((RiverEndX - RiverStartX) / SegLen);

		const FLinearColor RiverBed(0.06f, 0.05f, 0.03f);
		const FLinearColor RiverWater(0.05f, 0.16f, 0.28f); // PUBG blue-teal
		const FLinearColor RiverBank(0.38f, 0.30f, 0.18f);
		const FLinearColor RiverMud(0.18f, 0.14f, 0.08f);
		const FLinearColor RiverReed(0.12f, 0.35f, 0.08f);

		for (int32 s = 0; s < NumSegs; ++s)
		{
			float SX = RiverStartX + s * SegLen + SegLen * 0.5f;
			// Gentle sinusoidal curve for natural look
			float CurveY = RiverCenterY + FMath::Sin(SX * 0.00004f) * 2500.f;
			float TZ = GetTerrainHeight(SX, CurveY);

			// River bed (dark muddy bottom)
			SpawnLakePiece(PlaneMesh, FVector(SX, CurveY, TZ + 5.f),
				FRotator::ZeroRotator,
				FVector(SegLen / 100.f, RiverWidth * 1.15f / 100.f, 1.f),
				RiverBed, 0.95f, 0.f,
				*FString::Printf(TEXT("River_Bed_%d"), s));

			// Water surface — ProceduralMeshComponent grid for wave animation
			// Only create ONE PMC for the first segment; store for Tick animation
			if (s == 0 && !RiverWaterPMC)
			{
				const float TotalLen = RiverEndX - RiverStartX;
				const int32 ResX = 60; // along river length
				const int32 ResY = 8;  // across river width
				const float StepX = TotalLen / float(ResX);
				const float StepY = RiverWidth / float(ResY);

				RiverWaterBaseVerts.Empty();
				RiverWaterTris.Empty();
				RiverWaterUVs.Empty();
				TArray<FVector> Normals;
				TArray<FColor> Colors;

				for (int32 VY = 0; VY <= ResY; ++VY)
				{
					for (int32 VX = 0; VX <= ResX; ++VX)
					{
						float WX = RiverStartX + VX * StepX;
						float WCurveY = RiverCenterY + FMath::Sin(WX * 0.00004f) * 2500.f;
						float WY = WCurveY - RiverWidth * 0.5f + VY * StepY;
						float WZ = GetTerrainHeight(WX, WY) + 8.f;
						RiverWaterBaseVerts.Add(FVector(WX, WY, WZ));
						RiverWaterUVs.Add(FVector2D(float(VX) / ResX, float(VY) / ResY));
						Normals.Add(FVector(0.f, 0.f, 1.f));
					}
				}
				for (int32 VY = 0; VY < ResY; ++VY)
				{
					for (int32 VX = 0; VX < ResX; ++VX)
					{
						int32 I0 = VY * (ResX + 1) + VX;
						RiverWaterTris.Add(I0);
						RiverWaterTris.Add(I0 + ResX + 1);
						RiverWaterTris.Add(I0 + 1);
						RiverWaterTris.Add(I0 + 1);
						RiverWaterTris.Add(I0 + ResX + 1);
						RiverWaterTris.Add(I0 + ResX + 2);
					}
				}

				AActor* RWA = World->SpawnActor<AActor>(AActor::StaticClass(),
					FVector::ZeroVector, FRotator::ZeroRotator);
				if (RWA)
				{
					RiverWaterPMC = NewObject<UProceduralMeshComponent>(RWA, TEXT("RiverWaterPMC"));
					RiverWaterPMC->CreateMeshSection(0, RiverWaterBaseVerts, RiverWaterTris,
						Normals, RiverWaterUVs, Colors, TArray<FProcMeshTangent>(), false);
					RiverWaterPMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					RiverWaterPMC->SetCastShadow(false);
					RiverWaterPMC->SetBoundsScale(100.f);
					RiverWaterPMC->bNeverDistanceCull = true;
					RiverWaterPMC->RegisterComponent();
					RWA->SetRootComponent(RiverWaterPMC);

					if (GlassMaterial)
					{
						UMaterialInstanceDynamic* WMat = UMaterialInstanceDynamic::Create(GlassMaterial, RiverWaterPMC);
						if (WMat)
						{
							WMat->SetVectorParameterValue(TEXT("BaseColor"), RiverWater);
							WMat->SetScalarParameterValue(TEXT("Opacity"), 0.60f);
							WMat->SetScalarParameterValue(TEXT("Metallic"), 0.2f);
							WMat->SetScalarParameterValue(TEXT("Roughness"), 0.06f);
							WMat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(0.04f, 0.08f, 0.15f));
							RiverWaterPMC->SetMaterial(0, WMat);
						}
					}
					RWA->AddInstanceComponent(RiverWaterPMC);
				}
			}

			// PUBG-style shore: sand strip + scattered rocks + dense reeds
			const FLinearColor ShoreSand(0.65f, 0.58f, 0.42f);
			const FLinearColor ShoreRock(0.30f, 0.28f, 0.24f);

			// North shore — wide sandy strip
			SpawnLakePiece(PlaneMesh,
				FVector(SX, CurveY + RiverWidth * 0.5f + 400.f, TZ + 4.f),
				FRotator::ZeroRotator,
				FVector(SegLen / 100.f, 16.f, 1.f),
				ShoreSand, 0.90f, 0.f,
				*FString::Printf(TEXT("River_ShoreN_%d"), s));

			// South shore — wide sandy strip
			SpawnLakePiece(PlaneMesh,
				FVector(SX, CurveY - RiverWidth * 0.5f - 400.f, TZ + 4.f),
				FRotator::ZeroRotator,
				FVector(SegLen / 100.f, 16.f, 1.f),
				ShoreSand, 0.90f, 0.f,
				*FString::Printf(TEXT("River_ShoreS_%d"), s));

			// Muddy bank transition behind sand
			SpawnLakePiece(PlaneMesh,
				FVector(SX, CurveY + RiverWidth * 0.5f + 1200.f, TZ + 5.f),
				FRotator::ZeroRotator,
				FVector(SegLen / 100.f, 10.f, 1.f),
				RiverMud, 0.94f, 0.f,
				*FString::Printf(TEXT("River_MudN_%d"), s));
			SpawnLakePiece(PlaneMesh,
				FVector(SX, CurveY - RiverWidth * 0.5f - 1200.f, TZ + 5.f),
				FRotator::ZeroRotator,
				FVector(SegLen / 100.f, 10.f, 1.f),
				RiverMud, 0.94f, 0.f,
				*FString::Printf(TEXT("River_MudS_%d"), s));

			// Shore rocks (3-4 per segment per side)
			for (int32 r = 0; r < 7; ++r)
			{
				float RX = SX + CityRandom.FRandRange(-SegLen * 0.45f, SegLen * 0.45f);
				float Side = (r < 4) ? 1.f : -1.f;
				float RY = CurveY + Side * (RiverWidth * 0.5f + CityRandom.FRandRange(50.f, 600.f));
				float RZ = GetTerrainHeight(RX, RY);
				float RS = CityRandom.FRandRange(0.3f, 1.2f);

				SpawnLakePiece(SphereMesh, FVector(RX, RY, RZ + RS * 20.f),
					FRotator(CityRandom.FRandRange(-10.f, 10.f), CityRandom.FRandRange(0.f, 360.f), 0.f),
					FVector(RS, RS * CityRandom.FRandRange(0.7f, 1.3f), RS * CityRandom.FRandRange(0.3f, 0.6f)),
					ShoreRock, CityRandom.FRandRange(0.80f, 0.95f), 0.f,
					*FString::Printf(TEXT("River_Rock_%d_%d"), s, r));
			}

			// Dense reeds / vegetation (10 per segment)
			for (int32 r = 0; r < 10; ++r)
			{
				float RX = SX + CityRandom.FRandRange(-SegLen * 0.45f, SegLen * 0.45f);
				float Side = (r < 5) ? 1.f : -1.f;
				float RY = CurveY + Side * (RiverWidth * 0.5f + CityRandom.FRandRange(100.f, 900.f));
				float RZ = GetTerrainHeight(RX, RY);
				float RH = CityRandom.FRandRange(150.f, 450.f);

				SpawnLakePiece(CylinderMesh, FVector(RX, RY, RZ + RH * 0.5f),
					FRotator(CityRandom.FRandRange(-8.f, 8.f), CityRandom.FRandRange(0.f, 360.f), 0.f),
					FVector(0.05f, 0.05f, RH / 100.f),
					RiverReed, 0.75f, 0.f,
					*FString::Printf(TEXT("River_Reed_%d_%d"), s, r));
			}
		}
	}

	// --- Torch lights along the river near the pyramids ---
	{
		const float TorchStartX = -20000.f;
		const float TorchEndX = 60000.f;
		const float TorchSpacing = 5000.f;
		const int32 NumTorches = FMath::CeilToInt((TorchEndX - TorchStartX) / TorchSpacing);
		const FLinearColor TorchWood(0.12f, 0.07f, 0.03f);
		const FLinearColor CharredWood(0.05f, 0.03f, 0.02f);
		const FLinearColor IronBand(0.10f, 0.09f, 0.08f);
		// Flame colors — layered from white-hot core to smoky tip
		const FLinearColor FlameCore(1.0f, 0.95f, 0.7f);
		const FLinearColor FlameMid(1.0f, 0.55f, 0.05f);
		const FLinearColor FlameOuter(0.9f, 0.25f, 0.02f);
		const FLinearColor FlameTip(0.6f, 0.12f, 0.01f);
		const FLinearColor CoreEmissive(20.f, 16.f, 6.f);
		const FLinearColor MidEmissive(14.f, 5.f, 0.5f);
		const FLinearColor OuterEmissive(8.f, 2.f, 0.2f);
		const FLinearColor TipEmissive(3.f, 0.6f, 0.06f);
		int32 TorchIdx = 0;

		for (int32 Ti = 0; Ti < NumTorches; ++Ti)
		{
			const float TX = TorchStartX + Ti * TorchSpacing;
			const float RiverY = -97000.f + FMath::Sin(TX * 0.00004f) * 2500.f;

			for (int32 Side = 0; Side < 2; ++Side)
			{
				const float BankOffset = (Side == 0) ? 3000.f : -3000.f;
				const float TY = RiverY + BankOffset;
				const float TZ = GetTerrainHeight(TX, TY);

				AActor* TorchActor = World->SpawnActor<AActor>(
					AActor::StaticClass(), FVector(TX, TY, TZ), FRotator::ZeroRotator);
				if (!TorchActor) continue;

				const float PoleH = 600.f;
				const float TopZ = TZ + PoleH;

				// Torch pole — dark wood stake
				UStaticMeshComponent* Pole = NewObject<UStaticMeshComponent>(
					TorchActor, *FString::Printf(TEXT("TorchPole_%d"), TorchIdx));
				Pole->SetStaticMesh(CylinderMesh);
				Pole->SetWorldLocation(FVector(TX, TY, TZ + PoleH * 0.5f));
				Pole->SetWorldScale3D(FVector(0.25f, 0.25f, PoleH / 100.f));
				Pole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Pole->SetBoundsScale(30.f);
				Pole->RegisterComponent();
				ApplyPBR(Pole, TorchWood, 0.f, 0.90f);
				TorchActor->SetRootComponent(Pole);
				TorchActor->AddInstanceComponent(Pole);

				// Iron band near top
				UStaticMeshComponent* Band = NewObject<UStaticMeshComponent>(
					TorchActor, *FString::Printf(TEXT("TorchBand_%d"), TorchIdx));
				Band->SetStaticMesh(CylinderMesh);
				Band->SetWorldLocation(FVector(TX, TY, TopZ - 40.f));
				Band->SetWorldScale3D(FVector(0.35f, 0.35f, 0.15f));
				Band->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Band->SetBoundsScale(30.f);
				Band->RegisterComponent();
				ApplyPBR(Band, IronBand, 0.7f, 0.45f);
				TorchActor->AddInstanceComponent(Band);

				// Charred wrap at top of pole
				UStaticMeshComponent* Wrap = NewObject<UStaticMeshComponent>(
					TorchActor, *FString::Printf(TEXT("TorchWrap_%d"), TorchIdx));
				Wrap->SetStaticMesh(CylinderMesh);
				Wrap->SetWorldLocation(FVector(TX, TY, TopZ));
				Wrap->SetWorldScale3D(FVector(0.40f, 0.40f, 0.35f));
				Wrap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Wrap->SetBoundsScale(30.f);
				Wrap->RegisterComponent();
				ApplyPBR(Wrap, CharredWood, 0.f, 0.95f, FLinearColor(0.8f, 0.2f, 0.02f));
				TorchActor->AddInstanceComponent(Wrap);

				// Flame core — white-hot inner cone (brightest)
				UStaticMeshComponent* FCore = NewObject<UStaticMeshComponent>(
					TorchActor, *FString::Printf(TEXT("TorchFCore_%d"), TorchIdx));
				FCore->SetStaticMesh(ConeMesh);
				FCore->SetWorldLocation(FVector(TX, TY, TopZ + 30.f));
				FCore->SetWorldScale3D(FVector(0.30f, 0.30f, 1.8f));
				FCore->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				FCore->SetBoundsScale(30.f);
				FCore->RegisterComponent();
				ApplyPBR(FCore, FlameCore, 0.f, 0.15f, CoreEmissive);
				TorchActor->AddInstanceComponent(FCore);

				// Mid flame — orange cone, slightly wider
				UStaticMeshComponent* FMid = NewObject<UStaticMeshComponent>(
					TorchActor, *FString::Printf(TEXT("TorchFMid_%d"), TorchIdx));
				FMid->SetStaticMesh(ConeMesh);
				FMid->SetWorldLocation(FVector(TX, TY, TopZ + 20.f));
				FMid->SetWorldScale3D(FVector(0.50f, 0.50f, 2.2f));
				FMid->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				FMid->SetBoundsScale(30.f);
				FMid->RegisterComponent();
				if (GlassMaterial)
					ApplyPBRGlass(FMid, GlassMaterial, FlameMid, 0.6f, 0.f, 0.1f, MidEmissive);
				TorchActor->AddInstanceComponent(FMid);

				// Outer flame — red-orange translucent cone, widest
				UStaticMeshComponent* FOuter = NewObject<UStaticMeshComponent>(
					TorchActor, *FString::Printf(TEXT("TorchFOut_%d"), TorchIdx));
				FOuter->SetStaticMesh(ConeMesh);
				FOuter->SetWorldLocation(FVector(TX, TY, TopZ + 10.f));
				FOuter->SetWorldScale3D(FVector(0.70f, 0.70f, 2.8f));
				FOuter->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				FOuter->SetBoundsScale(30.f);
				FOuter->RegisterComponent();
				if (GlassMaterial)
					ApplyPBRGlass(FOuter, GlassMaterial, FlameOuter, 0.4f, 0.f, 0.1f, OuterEmissive);
				TorchActor->AddInstanceComponent(FOuter);

				// Flame tip — tall wispy cone at the very top
				UStaticMeshComponent* FTip = NewObject<UStaticMeshComponent>(
					TorchActor, *FString::Printf(TEXT("TorchFTip_%d"), TorchIdx));
				FTip->SetStaticMesh(ConeMesh);
				FTip->SetWorldLocation(FVector(TX, TY, TopZ + 100.f));
				FTip->SetWorldScale3D(FVector(0.25f, 0.25f, 1.5f));
				FTip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				FTip->SetBoundsScale(30.f);
				FTip->RegisterComponent();
				if (GlassMaterial)
					ApplyPBRGlass(FTip, GlassMaterial, FlameTip, 0.3f, 0.f, 0.1f, TipEmissive);
				TorchActor->AddInstanceComponent(FTip);

				// Store flame data for Tick animation
				{
					FTorchFlameData FD;
					FD.Core = FCore;
					FD.Mid = FMid;
					FD.Outer = FOuter;
					FD.Tip = FTip;
					FD.BasePos = FVector(TX, TY, TopZ);
					FD.Phase = CityRandom.FRandRange(0.f, 6.28f);
					TorchFlames.Add(FD);
				}

				// Ember glow sphere at the base of the flame
				UStaticMeshComponent* Ember = NewObject<UStaticMeshComponent>(
					TorchActor, *FString::Printf(TEXT("TorchEmber_%d"), TorchIdx));
				Ember->SetStaticMesh(SphereMesh);
				Ember->SetWorldLocation(FVector(TX, TY, TopZ + 15.f));
				Ember->SetWorldScale3D(FVector(0.55f, 0.55f, 0.35f));
				Ember->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Ember->SetBoundsScale(30.f);
				Ember->RegisterComponent();
				ApplyPBR(Ember, FLinearColor(1.f, 0.4f, 0.02f), 0.f, 0.2f,
					FLinearColor(12.f, 4.f, 0.3f));
				TorchActor->AddInstanceComponent(Ember);

				// Main torch light — strong warm glow
				UPointLightComponent* TorchLight = NewObject<UPointLightComponent>(
					TorchActor, *FString::Printf(TEXT("TorchLight_%d"), TorchIdx));
				TorchLight->SetWorldLocation(FVector(TX, TY, TopZ + 120.f));
				TorchLight->SetIntensity(8000.f);
				TorchLight->SetAttenuationRadius(5000.f);
				TorchLight->SetLightColor(FColor(255, 140, 40));
				TorchLight->SetCastShadows(false);
				TorchLight->RegisterComponent();
				TorchActor->AddInstanceComponent(TorchLight);

				++TorchIdx;
			}
		}
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Spawned %d river torches near pyramids."), TorchIdx);
	}

	// --- Ambient night lighting: 4x4 grid of warm golden lights across pirate beach + pyramid area ---
	{
		const FVector GridCenter(LakeCenter.X, LakeCenter.Y, 400.f);
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

	// ===== ANCIENT RUINS — weathered stone scattered around the lake =====
	{
		AActor* RuinsActor = World->SpawnActor<AActor>(AActor::StaticClass(), LakeCenter, FRotator::ZeroRotator);
		if (!RuinsActor) goto SkipRuins;
#if WITH_EDITOR
		RuinsActor->SetActorLabel(TEXT("AncientRuins"));
#endif
		int32 RIdx = 0;

		// Ancient stone palette
		const FLinearColor SandStone(0.52f, 0.46f, 0.36f);     // warm sandstone
		const FLinearColor Limestone(0.58f, 0.55f, 0.48f);      // pale limestone
		const FLinearColor WeatheredGray(0.40f, 0.38f, 0.34f);  // dark weathered stone
		const FLinearColor MossStone(0.32f, 0.36f, 0.28f);      // moss-covered stone
		const FLinearColor DarkStone(0.22f, 0.20f, 0.18f);      // ancient dark rock
		const FLinearColor Ivory(0.62f, 0.58f, 0.50f);          // bleached column caps

		const FLinearColor RuinColors[] = { SandStone, Limestone, WeatheredGray, MossStone, DarkStone, Ivory };

		FRandomStream RuinRand(777);

		auto RuinMat = [&]() -> FLinearColor {
			return RuinColors[RuinRand.RandRange(0, 5)];
		};
		auto RuinRough = [&]() -> float {
			return RuinRand.FRandRange(0.82f, 0.96f);
		};

		// Helper: spawn a ruin piece on this actor
		auto SpawnRuin = [&](UStaticMesh* Mesh, const FVector& Pos, const FVector& Scale,
			const FRotator& Rot, const FLinearColor& Color, float Roughness) -> UStaticMeshComponent*
		{
			FString Name = FString::Printf(TEXT("Ruin_%d"), RIdx++);
			UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(RuinsActor, *Name);
			MC->SetStaticMesh(Mesh);
			MC->SetWorldLocation(Pos);
			MC->SetWorldScale3D(Scale);
			MC->SetWorldRotation(Rot);
			MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MC->SetBoundsScale(30.f);
			MC->RegisterComponent();
			ApplyPBR(MC, Color, 0.f, Roughness);
			RuinsActor->AddInstanceComponent(MC);
			return MC;
		};

		if (RIdx == 0) // set root
		{
			UStaticMeshComponent* Root = SpawnRuin(CubeMesh,
				LakeCenter + FVector(0.f, 0.f, -50.f), FVector(0.01f, 0.01f, 0.01f),
				FRotator::ZeroRotator, DarkStone, 0.9f);
			RuinsActor->SetRootComponent(Root);
		}

		const float LCX = LakeCenter.X;
		const float LCY = LakeCenter.Y;
		const float RingR = 28000.f;  // scatter radius around lake

		// --- Broken columns (cylinder bases + fallen cylinder shafts) ---
		for (int32 C = 0; C < 30; ++C)
		{
			const float Angle = RuinRand.FRandRange(0.f, 360.f);
			const float Dist = RuinRand.FRandRange(22000.f, 40000.f);
			const float Rad = FMath::DegreesToRadians(Angle);
			const float RX = LCX + Dist * FMath::Cos(Rad);
			const float RY = LCY + Dist * FMath::Sin(Rad);
			const FLinearColor Col = RuinMat();
			const float Rough = RuinRough();

			const float ColRadius = RuinRand.FRandRange(1.5f, 4.0f);
			const float ColHeight = RuinRand.FRandRange(4.f, 18.f);

			// Standing base stub
			const float StubH = RuinRand.FRandRange(2.f, FMath::Min(8.f, ColHeight));
			SpawnRuin(CylinderMesh,
				FVector(RX, RY, StubH * 50.f * 0.5f),
				FVector(ColRadius, ColRadius, StubH),
				FRotator::ZeroRotator, Col, Rough);

			// Fallen shaft nearby (tilted/toppled)
			if (RuinRand.FRand() > 0.3f)
			{
				const float FallAngle = RuinRand.FRandRange(0.f, 360.f);
				const float FallDist = RuinRand.FRandRange(200.f, 800.f);
				SpawnRuin(CylinderMesh,
					FVector(RX + FallDist * FMath::Cos(FMath::DegreesToRadians(FallAngle)),
						RY + FallDist * FMath::Sin(FMath::DegreesToRadians(FallAngle)),
						ColRadius * 50.f),
					FVector(ColRadius * 0.9f, ColRadius * 0.9f, ColHeight * 0.7f),
					FRotator(RuinRand.FRandRange(70.f, 90.f), FallAngle, 0.f),
					Col, Rough);
			}

			// Square pedestal base
			if (RuinRand.FRand() > 0.5f)
			{
				SpawnRuin(CubeMesh,
					FVector(RX, RY, 40.f),
					FVector(ColRadius * 1.8f, ColRadius * 1.8f, 0.8f),
					FRotator(0.f, Angle, 0.f), Limestone, 0.88f);
			}
		}

		// --- Crumbled walls (low cube segments, some tilted) ---
		for (int32 W = 0; W < 18; ++W)
		{
			const float Angle = RuinRand.FRandRange(0.f, 360.f);
			const float Dist = RuinRand.FRandRange(24000.f, 38000.f);
			const float Rad = FMath::DegreesToRadians(Angle);
			const float WX = LCX + Dist * FMath::Cos(Rad);
			const float WY = LCY + Dist * FMath::Sin(Rad);
			const FLinearColor Col = RuinMat();
			const float WallYaw = RuinRand.FRandRange(0.f, 360.f);

			const float WallLen = RuinRand.FRandRange(15.f, 40.f);
			const float WallH = RuinRand.FRandRange(3.f, 12.f);
			const float WallThick = RuinRand.FRandRange(1.0f, 2.5f);

			// Main wall segment
			SpawnRuin(CubeMesh,
				FVector(WX, WY, WallH * 50.f * 0.5f),
				FVector(WallLen, WallThick, WallH),
				FRotator(RuinRand.FRandRange(-3.f, 3.f), WallYaw, RuinRand.FRandRange(-5.f, 5.f)),
				Col, RuinRough());

			// Rubble chunks at wall base
			for (int32 R = 0; R < 3 + RuinRand.RandRange(0, 3); ++R)
			{
				const float RD = RuinRand.FRandRange(200.f, 1200.f);
				const float RA = RuinRand.FRandRange(0.f, 360.f);
				const float RS = RuinRand.FRandRange(0.8f, 3.0f);
				SpawnRuin(CubeMesh,
					FVector(WX + RD * FMath::Cos(FMath::DegreesToRadians(RA)),
						WY + RD * FMath::Sin(FMath::DegreesToRadians(RA)),
						RS * 25.f),
					FVector(RS, RS * RuinRand.FRandRange(0.5f, 1.5f), RS * RuinRand.FRandRange(0.3f, 1.0f)),
					FRotator(RuinRand.FRandRange(-15.f, 15.f), RuinRand.FRandRange(0.f, 360.f), RuinRand.FRandRange(-10.f, 10.f)),
					RuinMat(), RuinRough());
			}
		}

		// --- Stone archways (two columns + lintel) ---
		for (int32 A = 0; A < 6; ++A)
		{
			const float Angle = RuinRand.FRandRange(0.f, 360.f);
			const float Dist = RuinRand.FRandRange(25000.f, 36000.f);
			const float Rad = FMath::DegreesToRadians(Angle);
			const float AX = LCX + Dist * FMath::Cos(Rad);
			const float AY = LCY + Dist * FMath::Sin(Rad);
			const float ArchYaw = RuinRand.FRandRange(0.f, 360.f);
			const FLinearColor Col = RuinMat();
			const float Rough = RuinRough();

			const float PillarH = RuinRand.FRandRange(10.f, 20.f);
			const float PillarR = RuinRand.FRandRange(1.5f, 3.0f);
			const float Gap = RuinRand.FRandRange(600.f, 1200.f);
			const float ArchRad = FMath::DegreesToRadians(ArchYaw);
			const float OffX = Gap * 0.5f * FMath::Cos(ArchRad);
			const float OffY = Gap * 0.5f * FMath::Sin(ArchRad);

			// Left pillar
			SpawnRuin(CylinderMesh,
				FVector(AX - OffX, AY - OffY, PillarH * 50.f * 0.5f),
				FVector(PillarR, PillarR, PillarH),
				FRotator(RuinRand.FRandRange(-2.f, 2.f), 0.f, RuinRand.FRandRange(-2.f, 2.f)),
				Col, Rough);

			// Right pillar
			SpawnRuin(CylinderMesh,
				FVector(AX + OffX, AY + OffY, PillarH * 50.f * 0.5f),
				FVector(PillarR, PillarR, PillarH),
				FRotator(RuinRand.FRandRange(-3.f, 3.f), 0.f, RuinRand.FRandRange(-3.f, 3.f)),
				Col, Rough);

			// Lintel across the top (some intact, some fallen)
			if (RuinRand.FRand() > 0.3f)
			{
				SpawnRuin(CubeMesh,
					FVector(AX, AY, PillarH * 50.f + 30.f),
					FVector(Gap / 100.f + 3.f, PillarR * 2.f, 1.5f),
					FRotator(RuinRand.FRandRange(-4.f, 4.f), ArchYaw, RuinRand.FRandRange(-3.f, 3.f)),
					Col, Rough);
			}
		}

		// --- Scattered stone slabs (flat fallen pieces) ---
		for (int32 S = 0; S < 40; ++S)
		{
			const float Angle = RuinRand.FRandRange(0.f, 360.f);
			const float Dist = RuinRand.FRandRange(20000.f, 42000.f);
			const float Rad = FMath::DegreesToRadians(Angle);
			const float SX = LCX + Dist * FMath::Cos(Rad);
			const float SY = LCY + Dist * FMath::Sin(Rad);
			const float SlabW = RuinRand.FRandRange(3.f, 10.f);
			const float SlabD = RuinRand.FRandRange(2.f, 8.f);
			const float SlabH = RuinRand.FRandRange(0.3f, 1.2f);

			SpawnRuin(CubeMesh,
				FVector(SX, SY, SlabH * 25.f),
				FVector(SlabW, SlabD, SlabH),
				FRotator(RuinRand.FRandRange(-8.f, 8.f), RuinRand.FRandRange(0.f, 360.f), RuinRand.FRandRange(-6.f, 6.f)),
				RuinMat(), RuinRough());
		}

		// --- Sphere boulders (eroded ancient carved stones) ---
		for (int32 B = 0; B < 20; ++B)
		{
			const float Angle = RuinRand.FRandRange(0.f, 360.f);
			const float Dist = RuinRand.FRandRange(22000.f, 40000.f);
			const float Rad = FMath::DegreesToRadians(Angle);
			const float BX = LCX + Dist * FMath::Cos(Rad);
			const float BY = LCY + Dist * FMath::Sin(Rad);
			const float BR = RuinRand.FRandRange(2.f, 6.f);

			SpawnRuin(SphereMesh,
				FVector(BX, BY, BR * 30.f),
				FVector(BR, BR * RuinRand.FRandRange(0.7f, 1.3f), BR * RuinRand.FRandRange(0.5f, 1.0f)),
				FRotator(RuinRand.FRandRange(-20.f, 20.f), RuinRand.FRandRange(0.f, 360.f), 0.f),
				RuinMat(), RuinRough());
		}

		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Spawned %d ancient ruin pieces around pirate lake."), RIdx);
	}
	SkipRuins:

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Spawned lake and pirate ship with %d ship components."), SIdx);
}

// ---------------------------------------------------------------------------
// SpawnStreetLabels — Charlotte Uptown street name signs at intersections
// ---------------------------------------------------------------------------
void UPHCCitySubsystem::SpawnStreetLabels()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Charlotte N-S streets (X-axis road index -> name)
	struct FStreetName { int32 Road; const TCHAR* Name; };
	const FStreetName NSStreets[] = {
		{ 6,  TEXT("Graham St") },
		{ 7,  TEXT("Cedar St") },
		{ 8,  TEXT("Pine St") },
		{ 9,  TEXT("Mint St") },
		{ 10, TEXT("Church St") },
		{ 12, TEXT("Tryon St") },
		{ 13, TEXT("College St") },
		{ 14, TEXT("Brevard St") },
		{ 15, TEXT("Davidson St") },
		{ 16, TEXT("Caldwell St") },
		{ 17, TEXT("McDowell St") },
	};
	const int32 NumNS = UE_ARRAY_COUNT(NSStreets);

	// Charlotte E-W streets (Y-axis road index -> name)
	const FStreetName EWStreets[] = {
		{ 6,  TEXT("Morehead St") },
		{ 7,  TEXT("Brooklyn Village Ave") },
		{ 8,  TEXT("MLK Jr Blvd") },
		{ 9,  TEXT("Hill St") },
		{ 12, TEXT("Trade St") },
		{ 13, TEXT("3rd St") },
		{ 14, TEXT("4th St") },
		{ 15, TEXT("5th St") },
		{ 16, TEXT("6th St") },
		{ 17, TEXT("7th St") },
		{ 18, TEXT("8th St") },
		{ 19, TEXT("10th St") },
		{ 20, TEXT("11th St") },
	};
	const int32 NumEW = UE_ARRAY_COUNT(EWStreets);

	// Sign visual constants
	const float PoleHeight = 350.f;
	const float SignBoardH = 60.f;
	const float SignBoardW = 280.f;
	const float SignBoardD = 10.f;
	const float TextHeight = 32.f;
	const FLinearColor SignGreen(0.04f, 0.28f, 0.10f);
	const FLinearColor PoleGray(0.45f, 0.45f, 0.45f);

	int32 SignCount = 0;

	// At each intersection of a named N-S street with a named E-W street, spawn a sign post
	for (int32 ns = 0; ns < NumNS; ++ns)
	{
		for (int32 ew = 0; ew < NumEW; ++ew)
		{
			const int32 RoadX = NSStreets[ns].Road;
			const int32 RoadY = EWStreets[ew].Road;

			// World position of this intersection (road centerline crossing)
			const float WorldX = FirstRoad + RoadX * RoadSpacing;
			const float WorldY = FirstRoad + RoadY * RoadSpacing;
			const FVector IntersectionPos(WorldX, WorldY, 0.f);

			// Offset sign to the NE corner of the intersection (so it doesn't block the road)
			const FVector SignBase = IntersectionPos + FVector(RoadWidth * 0.5f + 40.f, RoadWidth * 0.5f + 40.f, 0.f);

			AActor* SignActor = World->SpawnActor<AActor>(AActor::StaticClass(), SignBase, FRotator::ZeroRotator);
			if (!SignActor) continue;

			const FString ActorName = FString::Printf(TEXT("StreetSign_%d_%d"), RoadX, RoadY);

			// --- Pole (thin cylinder) ---
			UStaticMeshComponent* Pole = NewObject<UStaticMeshComponent>(SignActor, *FString::Printf(TEXT("Pole_%d"), SignCount));
			Pole->SetStaticMesh(CylinderMesh);
			Pole->SetWorldLocation(SignBase + FVector(0.f, 0.f, PoleHeight * 0.5f));
			Pole->SetWorldScale3D(FVector(0.06f, 0.06f, PoleHeight / 100.f));
			Pole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Pole->RegisterComponent();
			SignActor->SetRootComponent(Pole);
			SignActor->AddInstanceComponent(Pole);
			ApplyPBR(Pole, PoleGray, 0.7f, 0.35f);

			// --- N-S street sign board (faces Y direction — readable when looking north/south) ---
			{
				UStaticMeshComponent* Board = NewObject<UStaticMeshComponent>(SignActor, *FString::Printf(TEXT("BoardNS_%d"), SignCount));
				Board->SetStaticMesh(CubeMesh);
				Board->SetWorldLocation(SignBase + FVector(0.f, 0.f, PoleHeight + SignBoardH * 0.5f));
				Board->SetWorldScale3D(FVector(SignBoardW / 100.f, SignBoardD / 100.f, SignBoardH / 100.f));
				Board->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Board->RegisterComponent();
				Board->AttachToComponent(Pole, FAttachmentTransformRules::KeepWorldTransform);
				SignActor->AddInstanceComponent(Board);
				ApplyPBR(Board, SignGreen, 0.f, 0.7f);

				// Text for N-S street name
				UTextRenderComponent* NSText = NewObject<UTextRenderComponent>(SignActor, *FString::Printf(TEXT("TextNS_%d"), SignCount));
				NSText->SetText(FText::FromString(NSStreets[ns].Name));
				NSText->SetTextRenderColor(FColor::White);
				NSText->SetWorldSize(TextHeight);
				NSText->SetHorizontalAlignment(EHTA_Center);
				NSText->SetVerticalAlignment(EVRTA_TextCenter);
				NSText->SetWorldLocation(SignBase + FVector(0.f, -(SignBoardD * 0.5f + 1.f), PoleHeight + SignBoardH * 0.5f));
				NSText->SetWorldRotation(FRotator(0.f, 180.f, 0.f));
				NSText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				NSText->RegisterComponent();
				NSText->AttachToComponent(Pole, FAttachmentTransformRules::KeepWorldTransform);
				SignActor->AddInstanceComponent(NSText);

				// Back side text (readable from other direction)
				UTextRenderComponent* NSTextBack = NewObject<UTextRenderComponent>(SignActor, *FString::Printf(TEXT("TextNSB_%d"), SignCount));
				NSTextBack->SetText(FText::FromString(NSStreets[ns].Name));
				NSTextBack->SetTextRenderColor(FColor::White);
				NSTextBack->SetWorldSize(TextHeight);
				NSTextBack->SetHorizontalAlignment(EHTA_Center);
				NSTextBack->SetVerticalAlignment(EVRTA_TextCenter);
				NSTextBack->SetWorldLocation(SignBase + FVector(0.f, SignBoardD * 0.5f + 1.f, PoleHeight + SignBoardH * 0.5f));
				NSTextBack->SetWorldRotation(FRotator(0.f, 0.f, 0.f));
				NSTextBack->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				NSTextBack->RegisterComponent();
				NSTextBack->AttachToComponent(Pole, FAttachmentTransformRules::KeepWorldTransform);
				SignActor->AddInstanceComponent(NSTextBack);
			}

			// --- E-W street sign board (faces X direction — readable when looking east/west) ---
			{
				UStaticMeshComponent* Board2 = NewObject<UStaticMeshComponent>(SignActor, *FString::Printf(TEXT("BoardEW_%d"), SignCount));
				Board2->SetStaticMesh(CubeMesh);
				Board2->SetWorldLocation(SignBase + FVector(0.f, 0.f, PoleHeight + SignBoardH * 1.5f + 10.f));
				Board2->SetWorldScale3D(FVector(SignBoardD / 100.f, SignBoardW / 100.f, SignBoardH / 100.f));
				Board2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Board2->RegisterComponent();
				Board2->AttachToComponent(Pole, FAttachmentTransformRules::KeepWorldTransform);
				SignActor->AddInstanceComponent(Board2);
				ApplyPBR(Board2, SignGreen, 0.f, 0.7f);

				// Text for E-W street name
				UTextRenderComponent* EWText = NewObject<UTextRenderComponent>(SignActor, *FString::Printf(TEXT("TextEW_%d"), SignCount));
				EWText->SetText(FText::FromString(EWStreets[ew].Name));
				EWText->SetTextRenderColor(FColor::White);
				EWText->SetWorldSize(TextHeight);
				EWText->SetHorizontalAlignment(EHTA_Center);
				EWText->SetVerticalAlignment(EVRTA_TextCenter);
				EWText->SetWorldLocation(SignBase + FVector(-(SignBoardD * 0.5f + 1.f), 0.f, PoleHeight + SignBoardH * 1.5f + 10.f));
				EWText->SetWorldRotation(FRotator(0.f, 270.f, 0.f));
				EWText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				EWText->RegisterComponent();
				EWText->AttachToComponent(Pole, FAttachmentTransformRules::KeepWorldTransform);
				SignActor->AddInstanceComponent(EWText);

				// Back side text
				UTextRenderComponent* EWTextBack = NewObject<UTextRenderComponent>(SignActor, *FString::Printf(TEXT("TextEWB_%d"), SignCount));
				EWTextBack->SetText(FText::FromString(EWStreets[ew].Name));
				EWTextBack->SetTextRenderColor(FColor::White);
				EWTextBack->SetWorldSize(TextHeight);
				EWTextBack->SetHorizontalAlignment(EHTA_Center);
				EWTextBack->SetVerticalAlignment(EVRTA_TextCenter);
				EWTextBack->SetWorldLocation(SignBase + FVector(SignBoardD * 0.5f + 1.f, 0.f, PoleHeight + SignBoardH * 1.5f + 10.f));
				EWTextBack->SetWorldRotation(FRotator(0.f, 90.f, 0.f));
				EWTextBack->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				EWTextBack->RegisterComponent();
				EWTextBack->AttachToComponent(Pole, FAttachmentTransformRules::KeepWorldTransform);
				SignActor->AddInstanceComponent(EWTextBack);
			}

			++SignCount;
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("Charlotte Uptown: Spawned %d street name signs."), SignCount);
}

// ---------------------------------------------------------------------------
// SpawnLangtree — Langtree Lake Norman apartment complex (peninsula area)
// ---------------------------------------------------------------------------
void UPHCCitySubsystem::SpawnLangtree()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// -----------------------------------------------------------------------
	// Langtree Lake Norman — 380-acre mixed-use "Downtown Lake Norman"
	// Mooresville NC, off I-77 Exit 31 on eastern shore of Lake Norman.
	// Lake is WEST, I-77 is EAST. Langtree Road connects from the highway.
	// -----------------------------------------------------------------------
	const FVector ComplexCenter(-175000.f, 7000.f, 0.f);
	constexpr float FT = 80.f; // UU per foot — scaled up for immersion

	// 6 building color palettes — sand fort tones (warm tan / golden / dry earth)
	struct FBldgPalette
	{
		FLinearColor Facade;       // Upper wall sand
		FLinearColor BrickAccent;  // Base/foundation sand (slightly darker)
		FLinearColor Trim;         // Decorative band sand
		FLinearColor Roof;         // Battlement/merlon sand
		FLinearColor Awning1;      // Overhang sand
		FLinearColor Awning2;      // Overhang sand (variation)
	};
	const FBldgPalette Palettes[6] = {
		// Warm golden sand
		{ {0.88f,0.78f,0.55f}, {0.80f,0.70f,0.48f}, {0.84f,0.74f,0.52f}, {0.86f,0.76f,0.54f}, {0.82f,0.72f,0.50f}, {0.80f,0.70f,0.48f} },
		// Light cream sand
		{ {0.92f,0.82f,0.58f}, {0.84f,0.74f,0.52f}, {0.88f,0.78f,0.55f}, {0.90f,0.80f,0.56f}, {0.86f,0.76f,0.54f}, {0.84f,0.74f,0.52f} },
		// Desert tan sand
		{ {0.85f,0.75f,0.52f}, {0.78f,0.68f,0.46f}, {0.82f,0.72f,0.50f}, {0.84f,0.74f,0.52f}, {0.80f,0.70f,0.48f}, {0.78f,0.68f,0.46f} },
		// Amber sand
		{ {0.90f,0.78f,0.52f}, {0.82f,0.70f,0.46f}, {0.86f,0.74f,0.50f}, {0.88f,0.76f,0.52f}, {0.84f,0.72f,0.48f}, {0.82f,0.70f,0.46f} },
		// Pale dune sand
		{ {0.93f,0.84f,0.62f}, {0.86f,0.76f,0.54f}, {0.90f,0.80f,0.58f}, {0.92f,0.82f,0.60f}, {0.88f,0.78f,0.56f}, {0.86f,0.76f,0.54f} },
		// Warm earth sand
		{ {0.86f,0.76f,0.52f}, {0.80f,0.70f,0.48f}, {0.84f,0.74f,0.50f}, {0.86f,0.76f,0.52f}, {0.82f,0.72f,0.48f}, {0.80f,0.70f,0.46f} },
	};

	// Shared colors — sand fort tones
	const FLinearColor ConcreteGray(0.82f, 0.72f, 0.52f);  // sand walkway
	const FLinearColor StoneGray(0.75f, 0.65f, 0.45f);     // darker sand
	const FLinearColor DarkTrim(0.72f, 0.62f, 0.42f);      // shadow sand
	const FLinearColor RoofCharcoal(0.84f, 0.74f, 0.52f);  // battlement sand
	const FLinearColor PoolBlue(0.15f, 0.45f, 0.70f);      // keep pool blue
	const FLinearColor CabanaWhite(0.90f, 0.82f, 0.62f);   // sand cabana
	const FLinearColor LawnGreen(0.84f, 0.76f, 0.56f);     // sandy ground
	const FLinearColor WoodBrown(0.76f, 0.66f, 0.44f);     // driftwood sand
	const FLinearColor AsphaltDark(0.74f, 0.64f, 0.44f);   // sandy path
	const FLinearColor SidewalkTan(0.86f, 0.78f, 0.58f);   // light sand

	// Real Langtree business names (ground floor tenants)
	const TCHAR* LangtreeShops[] = {
		TEXT("TABLE 31"), TEXT("THE HIDDEN BIN"), TEXT("STARBUCKS"),
		TEXT("TWO SCOOPS"), TEXT("NOVANTA 90"), TEXT("SUN UP CAFE"),
		TEXT("KILTED BUFFALO"), TEXT("PHO ME"), TEXT("METRO DELI"),
		TEXT("ORCHID SPA"), TEXT("MOYERS"), TEXT("GOLD LEAF CIGARS"),
		TEXT("COOL RENEWAL"), TEXT("VINYASA YOGA"), TEXT("HAMPTONS"),
		TEXT("BACCHUS"), TEXT("R GREGORY"), TEXT("SPECTRUM EYE"),
		TEXT("EDWARD JONES"), TEXT("ROOST REALTY"), TEXT("SWEETHEART BAKE"),
		TEXT("BETH & CO"), TEXT("MI AMICHE"), TEXT("SCRATCH KITCHEN"),
	};
	int32 ShopIdx = 0;

	int32 CompCount = 0;

	// Helper: spawn a simple PBR cube component on an actor
	auto MakeCube = [&](AActor* Owner, USceneComponent* Root, const FVector& Loc,
		const FVector& Size, const FLinearColor& Color, const TCHAR* Name,
		float Metallic = 0.f, float Roughness = 0.92f, bool bCollision = false) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, Name);
		MC->SetStaticMesh(CubeMesh);
		MC->SetWorldLocation(Loc);
		MC->SetWorldScale3D(Size / 100.f);
		MC->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
		if (bCollision) MC->SetCollisionResponseToAllChannels(ECR_Block);
		MC->RegisterComponent();
		if (Root) MC->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
		else Owner->SetRootComponent(MC);
		Owner->AddInstanceComponent(MC);
		ApplyPBRGlass(MC, GlassMaterial, FLinearColor(0.7f, 0.85f, 0.95f), 0.25f, 0.1f, 0.02f);
		++CompCount;
		return MC;
	};

	auto MakeCyl = [&](AActor* Owner, USceneComponent* Root, const FVector& Loc,
		const FVector& Size, const FLinearColor& Color, const TCHAR* Name,
		float Metallic = 0.f, float Roughness = 0.92f) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, Name);
		MC->SetStaticMesh(CylinderMesh);
		MC->SetWorldLocation(Loc);
		MC->SetWorldScale3D(Size / 100.f);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->RegisterComponent();
		if (Root) MC->AttachToComponent(Root, FAttachmentTransformRules::KeepWorldTransform);
		else Owner->SetRootComponent(MC);
		Owner->AddInstanceComponent(MC);
		ApplyPBRGlass(MC, GlassMaterial, FLinearColor(0.7f, 0.85f, 0.95f), 0.25f, 0.1f, 0.02f);
		++CompCount;
		return MC;
	};

	// ========================================================================
	// 6 APARTMENT BUILDINGS (8-story mid-rise, ~180x60x92 ft each)
	// Along Landings Drive, 3 per side, ground floor retail with real tenants
	// Lake-facing balconies on the west side
	// ========================================================================
	const float AptW = 180.f * FT; // length (now along X, facing city)
	const float AptD = 60.f * FT;  // depth (now along Y)
	const float GndH = 14.f * FT;  // tall ground floor retail
	const float UpperFloorH = 11.f * FT; // each residential floor
	const float UpperH = 7.f * UpperFloorH; // 7 residential floors
	const float AptH = GndH + UpperH;  // 8 stories total
	const float AptSpacing = AptW; // buildings spaced along X (no gap, touch end-to-end)
	const float RowSpacing = 90.f * FT; // rows separated along Y (Landings Drive gap)
	const int32 ShopsPerBldg = 4; // 4 shops per building
	const float ShopBayW = AptW / (float)ShopsPerBldg;

	// Building positions: South row faces north (+Y), North row faces south (-Y)
	// Complex rotated so long axis (3 buildings) runs along X toward city
	struct FAptPos { float OffX; float OffY; int32 Front; };
	const FAptPos AptPos[] = {
		{ -AptSpacing, -RowSpacing,  1 }, // 119 Landings Dr (south)
		{  0.f,        -RowSpacing,  1 }, // 130 Landings Dr (south)
		{  AptSpacing, -RowSpacing,  1 }, // 143 Landings Dr (south)
		{ -AptSpacing,  RowSpacing, -1 }, // 120 Landings Dr (north)
		{  0.f,         RowSpacing, -1 }, // 138 Landings Dr (north)
		{  AptSpacing,  RowSpacing, -1 }, // 150 Landings Dr (north)
	};

	for (int32 b = 0; b < 6; ++b)
	{
		const FBldgPalette& P = Palettes[b];
		const FVector BC = ComplexCenter + FVector(AptPos[b].OffX, AptPos[b].OffY, 0.f);
		const int32 Fr = AptPos[b].Front;
		AActor* BA = World->SpawnActor<AActor>(AActor::StaticClass(), BC, FRotator::ZeroRotator);
		if (!BA) continue;

		// Ground floor — brick/stone base (with collision)
		UStaticMeshComponent* GF = MakeCube(BA, nullptr,
			BC + FVector(0.f, 0.f, GndH * 0.5f),
			FVector(AptW, AptD, GndH), P.BrickAccent,
			*FString::Printf(TEXT("GF_%d"), b), 0.f, 0.8f, true);

		// ---- Walkable shop fronts (now spaced along X, facing ±Y) ----
		for (int32 sh = 0; sh < ShopsPerBldg; ++sh)
		{
			const float ShX = BC.X - AptW * 0.5f + ShopBayW * 0.5f + sh * ShopBayW;
			const float FrY = BC.Y + Fr * (AptD * 0.5f);

			// Divider pillar between shops
			if (sh > 0)
			{
				MakeCyl(BA, GF,
					FVector(BC.X - AptW * 0.5f + sh * ShopBayW, FrY + Fr * 0.5f, GndH * 0.5f),
					FVector(1.2f * FT, 1.2f * FT, GndH), P.Trim,
					*FString::Printf(TEXT("Dv_%d_%d"), b, sh), 0.5f, 0.4f);
			}

			// Glass storefront — NO COLLISION (walkable)
			{
				UStaticMeshComponent* SG = NewObject<UStaticMeshComponent>(BA,
					*FString::Printf(TEXT("SF_%d_%d"), b, sh));
				SG->SetStaticMesh(CubeMesh);
				SG->SetWorldLocation(FVector(ShX, FrY + Fr * 1.f, GndH * 0.52f));
				SG->SetWorldScale3D(FVector((ShopBayW - 4.f * FT) / 100.f, 0.02f, (GndH * 0.72f) / 100.f));
				SG->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SG->RegisterComponent();
				SG->AttachToComponent(GF, FAttachmentTransformRules::KeepWorldTransform);
				BA->AddInstanceComponent(SG);
				ApplyPBRGlass(SG, GlassMaterial, FLinearColor(0.7f, 0.85f, 0.95f), 0.25f, 0.1f, 0.02f);
				++CompCount;
			}

			// Interior floor (walkable)
			MakeCube(BA, GF,
				FVector(ShX, BC.Y + Fr * (AptD * 0.2f), 1.f),
				FVector(ShopBayW - 2.f * FT, AptD * 0.4f, 0.5f * FT),
				FLinearColor(0.50f, 0.44f, 0.38f),
				*FString::Printf(TEXT("SFl_%d_%d"), b, sh), 0.f, 0.65f, true);

			// Back wall
			MakeCube(BA, GF,
				FVector(ShX, BC.Y - Fr * (AptD * 0.5f - 1.5f * FT), GndH * 0.5f),
				FVector(ShopBayW - 2.f * FT, 0.5f * FT, GndH * 0.85f),
				FLinearColor(0.88f, 0.85f, 0.80f),
				*FString::Printf(TEXT("SBk_%d_%d"), b, sh), 0.f, 0.7f);

			// Side walls
			for (int32 sw = 0; sw < 2; ++sw)
			{
				const float SWX = ShX + (sw == 0 ? -(ShopBayW * 0.5f - 0.5f * FT) : (ShopBayW * 0.5f - 0.5f * FT));
				MakeCube(BA, GF,
					FVector(SWX, BC.Y + Fr * (AptD * 0.2f), GndH * 0.5f),
					FVector(0.5f * FT, AptD * 0.4f, GndH * 0.85f),
					FLinearColor(0.85f, 0.83f, 0.78f),
					*FString::Printf(TEXT("SW_%d_%d_%d"), b, sh, sw), 0.f, 0.7f);
			}

			// Ceiling
			MakeCube(BA, GF,
				FVector(ShX, BC.Y + Fr * (AptD * 0.2f), GndH - 0.5f * FT),
				FVector(ShopBayW - 2.f * FT, AptD * 0.4f, 0.3f * FT),
				FLinearColor(0.92f, 0.90f, 0.87f),
				*FString::Printf(TEXT("SC_%d_%d"), b, sh), 0.f, 0.7f);

			// Awning (alternating colors from palette)
			const FLinearColor& Aw = (sh % 2 == 0) ? P.Awning1 : P.Awning2;
			MakeCube(BA, GF,
				FVector(ShX, FrY + Fr * 4.f * FT, GndH * 0.9f),
				FVector(ShopBayW - 1.f * FT, 8.f * FT, 0.5f * FT), Aw,
				*FString::Printf(TEXT("Aw_%d_%d"), b, sh), 0.f, 0.6f);

			// Shop name sign (real Langtree tenants)
			const TCHAR* SN = LangtreeShops[ShopIdx % 24];
			++ShopIdx;
			UTextRenderComponent* SS = NewObject<UTextRenderComponent>(BA,
				*FString::Printf(TEXT("SSg_%d_%d"), b, sh));
			SS->SetText(FText::FromString(SN));
			SS->SetTextRenderColor(FColor::White);
			SS->SetWorldSize(2.5f * FT);
			SS->SetHorizontalAlignment(EHTA_Center);
			SS->SetVerticalAlignment(EVRTA_TextCenter);
			SS->SetWorldLocation(FVector(ShX, FrY + Fr * 4.2f * FT, GndH * 0.78f));
			SS->SetWorldRotation(FRotator(0.f, (Fr > 0) ? 270.f : 90.f, 0.f));
			SS->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SS->RegisterComponent();
			SS->AttachToComponent(GF, FAttachmentTransformRules::KeepWorldTransform);
			BA->AddInstanceComponent(SS);
			++CompCount;

			// Counter inside shop
			MakeCube(BA, GF,
				FVector(ShX, BC.Y, 2.f * FT),
				FVector(ShopBayW * 0.5f, 8.f * FT, 3.5f * FT), WoodBrown,
				*FString::Printf(TEXT("Ct_%d_%d"), b, sh), 0.f, 0.7f);
		}

		// ---- Upper 7 residential floors — HardiPlank siding ----
		MakeCube(BA, GF,
			BC + FVector(0.f, 0.f, GndH + UpperH * 0.5f),
			FVector(AptW, AptD - 4.f, UpperH), P.Facade,
			*FString::Printf(TEXT("Up_%d"), b), 0.f, 0.7f);

		const float FlH = UpperFloorH;

		// ---- Balconies on outer side (-Y from building center) ----
		for (int32 fl = 0; fl < 7; ++fl)
		{
			const float FlZ = GndH + fl * FlH + 4.f;
			for (int32 bl = 0; bl < 5; ++bl)
			{
				const float BX = BC.X - AptW * 0.4f + bl * (AptW * 0.8f / 4.f);
				const float BY = BC.Y - (AptD * 0.5f + 4.f * FT);
				MakeCube(BA, GF, FVector(BX, BY, FlZ),
					FVector(12.f * FT, 7.f * FT, 0.5f * FT), ConcreteGray,
					*FString::Printf(TEXT("Bl_%d_%d_%d"), b, fl, bl), 0.f, 0.7f);
				MakeCube(BA, GF, FVector(BX, BY - 3.f * FT, FlZ + 2.5f * FT),
					FVector(12.f * FT, 1.f * FT, 4.f * FT), P.Trim,
					*FString::Printf(TEXT("BR_%d_%d_%d"), b, fl, bl), 0.f, 0.92f);
			}
		}

		// ---- Crenellated battlement (sand fort) ----
		// Base battlement wall (continuous)
		MakeCube(BA, GF, BC + FVector(0.f, 0.f, AptH + 1.5f * FT),
			FVector(AptW + 2.f * FT, AptD + 2.f * FT, 3.f * FT), P.Facade,
			*FString::Printf(TEXT("BtW_%d"), b), 0.f, 0.92f);
		// Merlons along the two long sides (Y-facing sides, spaced along X)
		{
			const float MerlonW = 6.f * FT;
			const float MerlonGap = 4.f * FT;
			const float MerlonPitch = MerlonW + MerlonGap;
			const float MerlonH = 3.f * FT;
			const float MerlonTopZ = AptH + 3.f * FT + MerlonH * 0.5f;
			const int32 NM = FMath::Max(1, (int32)(AptW / MerlonPitch));
			for (int32 si = 0; si < 2; ++si)
			{
				const float MY = BC.Y + ((si == 0) ? -(AptD * 0.5f) : (AptD * 0.5f));
				for (int32 m = 0; m < NM; ++m)
				{
					const float MX = BC.X - AptW * 0.5f + MerlonPitch * 0.5f + m * MerlonPitch;
					MakeCube(BA, GF, FVector(MX, MY, MerlonTopZ),
						FVector(MerlonW, 3.f * FT, MerlonH), P.Roof,
						*FString::Printf(TEXT("M_%d_%d_%d"), b, si, m), 0.f, 0.92f);
				}
			}
		}

		// ---- Trim stripe ----
		MakeCube(BA, GF, BC + FVector(0.f, 0.f, GndH + 0.25f * FT),
			FVector(AptW + 1.f * FT, AptD + 1.f * FT, 0.5f * FT), P.Trim,
			*FString::Printf(TEXT("Tr_%d"), b), 0.f, 0.92f);

		// ---- Corner turrets (sand fort towers with cone caps) ----
		for (int32 c = 0; c < 4; ++c)
		{
			const float cx = (c < 2) ? -AptW * 0.5f : AptW * 0.5f;
			const float cy = (c % 2 == 0) ? -AptD * 0.5f : AptD * 0.5f;
			const float TowerH = AptH + 8.f * FT;
			// Tower cylinder
			MakeCyl(BA, GF, BC + FVector(cx, cy, TowerH * 0.5f),
				FVector(5.f * FT, 5.f * FT, TowerH), P.Facade,
				*FString::Printf(TEXT("Tw_%d_%d"), b, c), 0.f, 0.92f);
			// Cone cap
			{
				UStaticMeshComponent* Cap = NewObject<UStaticMeshComponent>(BA,
					*FString::Printf(TEXT("TwC_%d_%d"), b, c));
				Cap->SetStaticMesh(ConeMesh);
				Cap->SetWorldLocation(BC + FVector(cx, cy, TowerH + 4.f * FT));
				Cap->SetWorldScale3D(FVector(8.f * FT / 100.f, 8.f * FT / 100.f, 8.f * FT / 100.f));
				Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Cap->RegisterComponent();
				Cap->AttachToComponent(GF, FAttachmentTransformRules::KeepWorldTransform);
				BA->AddInstanceComponent(Cap);
				ApplyPBRGlass(Cap, GlassMaterial, FLinearColor(0.7f, 0.85f, 0.95f), 0.25f, 0.1f, 0.02f);
				++CompCount;
			}
			// Small battlement ring at tower top
			MakeCyl(BA, GF, BC + FVector(cx, cy, TowerH + 0.5f * FT),
				FVector(6.f * FT, 6.f * FT, 1.5f * FT), P.Trim,
				*FString::Printf(TEXT("TwR_%d_%d"), b, c), 0.f, 0.92f);
		}

		// ---- Amber edge lights — glow at sunset/night ----
		{
			const FLinearColor AmberGlow(1.0f, 0.6f, 0.1f);
			const float StripW = 0.4f * FT;

			// 4 vertical corner strips (full building height)
			for (int32 e = 0; e < 4; ++e)
			{
				const float ex = (e < 2) ? -(AptW * 0.5f + StripW * 0.5f) : (AptW * 0.5f + StripW * 0.5f);
				const float ey = (e % 2 == 0) ? -(AptD * 0.5f + StripW * 0.5f) : (AptD * 0.5f + StripW * 0.5f);

				UStaticMeshComponent* Strip = NewObject<UStaticMeshComponent>(BA,
					*FString::Printf(TEXT("VEL_%d_%d"), b, e));
				Strip->SetStaticMesh(CubeMesh);
				Strip->SetWorldLocation(BC + FVector(ex, ey, AptH * 0.5f));
				Strip->SetWorldScale3D(FVector(StripW / 100.f, StripW / 100.f, AptH / 100.f));
				Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Strip->RegisterComponent();
				Strip->AttachToComponent(GF, FAttachmentTransformRules::KeepWorldTransform);
				BA->AddInstanceComponent(Strip);
				ApplyPBR(Strip, FLinearColor(0.05f, 0.05f, 0.05f), 0.3f, 0.3f);
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Strip->GetMaterial(0));
				if (MID)
				{
					NightWindowMaterials.Add(MID);
					NightWindowLitColors.Add(AmberGlow);
					NightWindowLitStates.Add(true);
				}
				++CompCount;
			}

			// 2 horizontal roofline strips (front and back edges, along Y)
			for (int32 e = 0; e < 2; ++e)
			{
				const float ey = (e == 0) ? -(AptD * 0.5f + StripW * 0.5f) : (AptD * 0.5f + StripW * 0.5f);

				UStaticMeshComponent* Strip = NewObject<UStaticMeshComponent>(BA,
					*FString::Printf(TEXT("HEL_%d_%d"), b, e));
				Strip->SetStaticMesh(CubeMesh);
				Strip->SetWorldLocation(BC + FVector(0.f, ey, AptH + StripW * 0.5f));
				Strip->SetWorldScale3D(FVector(AptW / 100.f, StripW / 100.f, StripW / 100.f));
				Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Strip->RegisterComponent();
				Strip->AttachToComponent(GF, FAttachmentTransformRules::KeepWorldTransform);
				BA->AddInstanceComponent(Strip);
				ApplyPBR(Strip, FLinearColor(0.05f, 0.05f, 0.05f), 0.3f, 0.3f);
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Strip->GetMaterial(0));
				if (MID)
				{
					NightWindowMaterials.Add(MID);
					NightWindowLitColors.Add(AmberGlow);
					NightWindowLitStates.Add(true);
				}
				++CompCount;
			}

			// 2 horizontal roofline strips (side edges along X)
			for (int32 e = 0; e < 2; ++e)
			{
				const float ex = (e == 0) ? -(AptW * 0.5f + StripW * 0.5f) : (AptW * 0.5f + StripW * 0.5f);

				UStaticMeshComponent* Strip = NewObject<UStaticMeshComponent>(BA,
					*FString::Printf(TEXT("SEL_%d_%d"), b, e));
				Strip->SetStaticMesh(CubeMesh);
				Strip->SetWorldLocation(BC + FVector(ex, 0.f, AptH + StripW * 0.5f));
				Strip->SetWorldScale3D(FVector(StripW / 100.f, AptD / 100.f, StripW / 100.f));
				Strip->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Strip->RegisterComponent();
				Strip->AttachToComponent(GF, FAttachmentTransformRules::KeepWorldTransform);
				BA->AddInstanceComponent(Strip);
				ApplyPBR(Strip, FLinearColor(0.05f, 0.05f, 0.05f), 0.3f, 0.3f);
				UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Strip->GetMaterial(0));
				if (MID)
				{
					NightWindowMaterials.Add(MID);
					NightWindowLitColors.Add(AmberGlow);
					NightWindowLitStates.Add(true);
				}
				++CompCount;
			}
		}
	}

	// ========================================================================
	// SKY BRIDGES — enclosed walkways connecting south and north rows
	// 3 bridges at each building center, spanning Landings Drive at 4th floor
	// ========================================================================
	{
		const float BridgeFloorZ = GndH + 3.f * UpperFloorH;
		const float BridgeH = UpperFloorH;
		const float BridgeW = 20.f * FT;
		const float SouthFace = -RowSpacing + AptD * 0.5f;
		const float NorthFace = RowSpacing - AptD * 0.5f;
		const float BridgeLen = NorthFace - SouthFace;
		const float BridgeCY = (SouthFace + NorthFace) * 0.5f;

		for (int32 br = 0; br < 3; ++br)
		{
			const float BX = (br - 1) * AptSpacing;
			const FVector BrC = ComplexCenter + FVector(BX, BridgeCY, BridgeFloorZ);

			AActor* BrA = World->SpawnActor<AActor>(AActor::StaticClass(), BrC, FRotator::ZeroRotator);
			if (!BrA) continue;

			// Floor slab (walkable)
			UStaticMeshComponent* BrFloor = MakeCube(BrA, nullptr,
				BrC + FVector(0.f, 0.f, 0.5f * FT),
				FVector(BridgeW, BridgeLen, 1.f * FT), ConcreteGray,
				*FString::Printf(TEXT("SBFlr_%d"), br), 0.f, 0.7f, true);

			// Ceiling
			MakeCube(BrA, BrFloor,
				BrC + FVector(0.f, 0.f, BridgeH),
				FVector(BridgeW, BridgeLen, 0.5f * FT), RoofCharcoal,
				*FString::Printf(TEXT("SBCeil_%d"), br), 0.2f, 0.5f);

			// Sand fort corridor walls (west + east)
			for (int32 gs = 0; gs < 2; ++gs)
			{
				const float SideX = (gs == 0) ? -BridgeW * 0.5f : BridgeW * 0.5f;
				MakeCube(BrA, BrFloor,
					BrC + FVector(SideX, 0.f, BridgeH * 0.5f),
					FVector(1.f * FT, BridgeLen, BridgeH - 1.f * FT), Palettes[0].Facade,
					*FString::Printf(TEXT("SBWl_%d_%d"), br, gs), 0.f, 0.92f);
			}
		}
	}

	// ========================================================================
	// CONNECTORS — link east apartment ends to office buildings
	// Enclosed corridors from each row's east building to the nearby office
	// ========================================================================
	{
		// East apartment ends at X = AptSpacing + AptW/2
		const float EastAptEndX = AptSpacing + AptW * 0.5f;

		// Office positions (rotated: X and Y swapped from original)
		const float Off0X = 370.f * FT;
		const float Off1X = 370.f * FT;
		const float Off0Y = -80.f * FT;
		const float Off1Y = 120.f * FT;
		const float OffD = 70.f * FT;

		// Office 0 (south) — connect to south apartment row
		{
			const float ConnStartX = EastAptEndX;
			const float ConnEndX = Off0X - OffD * 0.5f;
			const float ConnLen = ConnEndX - ConnStartX;
			if (ConnLen > 0.f)
			{
				const float ConnCX = (ConnStartX + ConnEndX) * 0.5f;
				const float ConnY = -RowSpacing;
				const float ConnZ = GndH + 2.f * UpperFloorH;
				const FVector CC1 = ComplexCenter + FVector(ConnCX, ConnY, ConnZ);

				AActor* CnA = World->SpawnActor<AActor>(AActor::StaticClass(), CC1, FRotator::ZeroRotator);
				if (CnA)
				{
					UStaticMeshComponent* CnRoot = MakeCube(CnA, nullptr,
						CC1 + FVector(0.f, 0.f, 0.5f * FT),
						FVector(ConnLen, AptD, 1.f * FT), ConcreteGray,
						TEXT("Cn0Flr"), 0.f, 0.7f, true);
					MakeCube(CnA, CnRoot,
						CC1 + FVector(0.f, 0.f, UpperFloorH),
						FVector(ConnLen, AptD, 0.5f * FT), RoofCharcoal,
						TEXT("Cn0Ceil"), 0.2f, 0.5f);
					// Facade walls
					for (int32 ws = 0; ws < 2; ++ws)
					{
						const float WY = (ws == 0) ? -AptD * 0.5f : AptD * 0.5f;
						MakeCube(CnA, CnRoot,
							CC1 + FVector(0.f, WY, UpperFloorH * 0.5f),
							FVector(ConnLen, 0.5f * FT, UpperFloorH), Palettes[2].Facade,
							*FString::Printf(TEXT("Cn0W_%d"), ws), 0.f, 0.7f);
					}
				}
			}
		}

		// Office 1 (north) — connect to north apartment row
		{
			const float ConnStartX = EastAptEndX;
			const float ConnEndX = Off1X - OffD * 0.5f;
			const float ConnLen = ConnEndX - ConnStartX;
			if (ConnLen > 0.f)
			{
				const float ConnCX = (ConnStartX + ConnEndX) * 0.5f;
				const float ConnY = RowSpacing;
				const float ConnZ = GndH + 2.f * UpperFloorH;
				const FVector CC2 = ComplexCenter + FVector(ConnCX, ConnY, ConnZ);

				AActor* CnA = World->SpawnActor<AActor>(AActor::StaticClass(), CC2, FRotator::ZeroRotator);
				if (CnA)
				{
					UStaticMeshComponent* CnRoot = MakeCube(CnA, nullptr,
						CC2 + FVector(0.f, 0.f, 0.5f * FT),
						FVector(ConnLen, AptD, 1.f * FT), ConcreteGray,
						TEXT("Cn1Flr"), 0.f, 0.7f, true);
					MakeCube(CnA, CnRoot,
						CC2 + FVector(0.f, 0.f, UpperFloorH),
						FVector(ConnLen, AptD, 0.5f * FT), RoofCharcoal,
						TEXT("Cn1Ceil"), 0.2f, 0.5f);
					for (int32 ws = 0; ws < 2; ++ws)
					{
						const float WY = (ws == 0) ? -AptD * 0.5f : AptD * 0.5f;
						MakeCube(CnA, CnRoot,
							CC2 + FVector(0.f, WY, UpperFloorH * 0.5f),
							FVector(ConnLen, 0.5f * FT, UpperFloorH), Palettes[5].Facade,
							*FString::Printf(TEXT("Cn1W_%d"), ws), 0.f, 0.7f);
					}
				}
			}
		}

		// Bridge between the two offices
		{
			const float OffBridgeLen = Off1Y - Off0Y;
			const float OffBridgeCY = (Off0Y + Off1Y) * 0.5f;
			const float OffBridgeZ = 56.f * FT; // mid-height of 7-story offices
			const FVector OBC = ComplexCenter + FVector(Off0X, OffBridgeCY, OffBridgeZ);

			AActor* OBA = World->SpawnActor<AActor>(AActor::StaticClass(), OBC, FRotator::ZeroRotator);
			if (OBA)
			{
				UStaticMeshComponent* OBFlr = MakeCube(OBA, nullptr,
					OBC + FVector(0.f, 0.f, 0.5f * FT),
					FVector(20.f * FT, OffBridgeLen, 1.f * FT), ConcreteGray,
					TEXT("OBFlr"), 0.f, 0.7f, true);
				MakeCube(OBA, OBFlr,
					OBC + FVector(0.f, 0.f, 14.f * FT),
					FVector(20.f * FT, OffBridgeLen, 0.5f * FT), RoofCharcoal,
					TEXT("OBCeil"), 0.2f, 0.5f);
				for (int32 gs = 0; gs < 2; ++gs)
				{
					const float SideX = (gs == 0) ? -10.f * FT : 10.f * FT;
					UStaticMeshComponent* GW = NewObject<UStaticMeshComponent>(OBA,
						*FString::Printf(TEXT("OBGl_%d"), gs));
					GW->SetStaticMesh(CubeMesh);
					GW->SetWorldLocation(OBC + FVector(SideX, 0.f, 7.f * FT));
					GW->SetWorldScale3D(FVector(0.02f, OffBridgeLen / 100.f, (12.f * FT) / 100.f));
					GW->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					GW->RegisterComponent();
					GW->AttachToComponent(OBFlr, FAttachmentTransformRules::KeepWorldTransform);
					OBA->AddInstanceComponent(GW);
					ApplyPBR(GW, FLinearColor(0.86f, 0.76f, 0.54f), 0.f, 0.92f); // sand corridor wall
					++CompCount;
				}
			}
		}
	}

	// ========================================================================
	// LANDINGS DRIVE — main E-W street through town center (rotated)
	// ========================================================================
	{
		AActor* RdA = World->SpawnActor<AActor>(AActor::StaticClass(), ComplexCenter, FRotator::ZeroRotator);
		if (RdA)
		{
			// Landings Drive roadway (between the two rows, now E-W)
			UStaticMeshComponent* LandingsDr = MakeCube(RdA, nullptr,
				ComplexCenter + FVector(0.f, 0.f, 1.f),
				FVector(700.f * FT, 50.f * FT, 0.3f * FT), AsphaltDark,
				TEXT("LandingsDr"), 0.f, 0.85f, true);

			// Sidewalks on both sides (north + south)
			for (int32 s = 0; s < 2; ++s)
			{
				const float SwY = (s == 0) ? -(30.f * FT) : (30.f * FT);
				MakeCube(RdA, LandingsDr,
					ComplexCenter + FVector(0.f, SwY, 2.f),
					FVector(700.f * FT, 14.f * FT, 0.3f * FT), SidewalkTan,
					*FString::Printf(TEXT("LandingSW_%d"), s), 0.f, 0.7f, true);
			}

			// Street sign
			UTextRenderComponent* LDSign = NewObject<UTextRenderComponent>(RdA, TEXT("LandingsDrSign"));
			LDSign->SetText(FText::FromString(TEXT("LANDINGS DR")));
			LDSign->SetTextRenderColor(FColor::White);
			LDSign->SetWorldSize(2.f * FT);
			LDSign->SetHorizontalAlignment(EHTA_Center);
			LDSign->SetWorldLocation(ComplexCenter + FVector(-340.f * FT, 0.f, 12.f * FT));
			LDSign->SetWorldRotation(FRotator(0.f, 270.f, 0.f));
			LDSign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			LDSign->RegisterComponent();
			LDSign->AttachToComponent(LandingsDr, FAttachmentTransformRules::KeepWorldTransform);
			RdA->AddInstanceComponent(LDSign);
			++CompCount;

			// Street sign pole
			MakeCyl(RdA, LandingsDr,
				ComplexCenter + FVector(-340.f * FT, 0.f, 6.f * FT),
				FVector(0.3f * FT, 0.3f * FT, 12.f * FT), DarkTrim,
				TEXT("LDSignPole"), 0.5f, 0.4f);

			// Langtree Road N-S connector to I-77
			MakeCube(RdA, LandingsDr,
				ComplexCenter + FVector(-300.f * FT, 250.f * FT, 1.f),
				FVector(40.f * FT, 500.f * FT, 0.3f * FT), AsphaltDark,
				TEXT("LangtreeRd"), 0.f, 0.85f, true);

			// Langtree Village Drive (east of center, N-S)
			MakeCube(RdA, LandingsDr,
				ComplexCenter + FVector(320.f * FT, 0.f, 1.f),
				FVector(35.f * FT, 400.f * FT, 0.3f * FT), AsphaltDark,
				TEXT("VillageDr"), 0.f, 0.85f, true);

			// Village View Drive (further east, N-S)
			MakeCube(RdA, LandingsDr,
				ComplexCenter + FVector(450.f * FT, -80.f * FT, 1.f),
				FVector(30.f * FT, 300.f * FT, 0.3f * FT), AsphaltDark,
				TEXT("VillageViewDr"), 0.f, 0.85f, true);

			// Walkway to pool/clubhouse (south)
			MakeCube(RdA, LandingsDr,
				ComplexCenter + FVector(20.f * FT, -130.f * FT, 2.f),
				FVector(14.f * FT, 220.f * FT, 0.3f * FT), SidewalkTan,
				TEXT("PoolWalk"), 0.f, 0.7f, true);

			// Langtree Campus Drive (easternmost, to office campus)
			MakeCube(RdA, LandingsDr,
				ComplexCenter + FVector(580.f * FT, -150.f * FT, 1.f),
				FVector(30.f * FT, 350.f * FT, 0.3f * FT), AsphaltDark,
				TEXT("CampusDr"), 0.f, 0.85f, true);
		}
	}

	// ========================================================================
	// OFFICE BUILDINGS — Regus/Novant Health (106 Langtree Village Rd)
	// 7-story, along Langtree Village Drive, east of apartments
	// ========================================================================
	for (int32 o = 0; o < 2; ++o)
	{
		const FVector OP = ComplexCenter + FVector(370.f * FT, (o == 0) ? -80.f * FT : 120.f * FT, 0.f);
		AActor* OA = World->SpawnActor<AActor>(AActor::StaticClass(), OP, FRotator::ZeroRotator);
		if (!OA) continue;

		const float OW = 70.f * FT, OD = 90.f * FT, OH = 98.f * FT;
		UStaticMeshComponent* OB = MakeCube(OA, nullptr,
			OP + FVector(0.f, 0.f, OH * 0.5f),
			FVector(OW, OD, OH), FLinearColor(0.86f, 0.76f, 0.54f),
			*FString::Printf(TEXT("Off_%d"), o), 0.f, 0.92f, true);

		// Glass curtain walls (all 4 sides, per floor)
		const float OFH = 14.f * FT;
		for (int32 fl = 0; fl < 7; ++fl)
		{
			const float FlZ = fl * OFH + OFH * 0.6f;
			for (int32 f = 0; f < 4; ++f)
			{
				float fx = 0.f, fy = 0.f; FVector sc;
				if (f < 2) { fx = (f == 0) ? -(OW * 0.5f + 1.f) : (OW * 0.5f + 1.f); sc = FVector(0.02f, (OD - 4.f * FT) / 100.f, (8.f * FT) / 100.f); }
				else { fy = (f == 2) ? -(OD * 0.5f + 1.f) : (OD * 0.5f + 1.f); sc = FVector((OW - 4.f * FT) / 100.f, 0.02f, (8.f * FT) / 100.f); }
				UStaticMeshComponent* OG = NewObject<UStaticMeshComponent>(OA, *FString::Printf(TEXT("OG_%d_%d_%d"), o, fl, f));
				OG->SetStaticMesh(CubeMesh);
				OG->SetWorldLocation(OP + FVector(fx, fy, FlZ));
				OG->SetWorldScale3D(sc);
				OG->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				OG->RegisterComponent();
				OG->AttachToComponent(OB, FAttachmentTransformRules::KeepWorldTransform);
				OA->AddInstanceComponent(OG);
				ApplyPBR(OG, FLinearColor(0.06f, 0.05f, 0.04f), 0.f, 0.95f); // dark recessed window
				++CompCount;
			}
		}

		// Roof
		MakeCube(OA, OB, OP + FVector(0.f, 0.f, OH + 1.f * FT),
			FVector(OW + 2.f * FT, OD + 2.f * FT, 1.f * FT), RoofCharcoal,
			*FString::Printf(TEXT("ORf_%d"), o), 0.2f, 0.5f);

		// Building label
		const TCHAR* OffNames[] = { TEXT("REGUS"), TEXT("NOVANT HEALTH") };
		UTextRenderComponent* OTxt = NewObject<UTextRenderComponent>(OA, *FString::Printf(TEXT("OTx_%d"), o));
		OTxt->SetText(FText::FromString(OffNames[o]));
		OTxt->SetTextRenderColor(FColor::White);
		OTxt->SetWorldSize(2.f * FT);
		OTxt->SetHorizontalAlignment(EHTA_Center);
		OTxt->SetWorldLocation(OP + FVector(0.f, -(OD * 0.5f + 1.f), OH * 0.85f));
		OTxt->SetWorldRotation(FRotator(0.f, 270.f, 0.f));
		OTxt->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OTxt->RegisterComponent();
		OTxt->AttachToComponent(OB, FAttachmentTransformRules::KeepWorldTransform);
		OA->AddInstanceComponent(OTxt);
		++CompCount;
	}

	// ========================================================================
	// CORVID TECHNOLOGIES — 8-story glass office (153 Campus Dr)
	// SunGuard SNR 43 reflective blue glass, modern corporate
	// ========================================================================
	{
		const FVector CorvidPos = ComplexCenter + FVector(560.f * FT, -200.f * FT, 0.f);
		AActor* CA = World->SpawnActor<AActor>(AActor::StaticClass(), CorvidPos, FRotator::ZeroRotator);
		if (CA)
		{
			const float CW = 120.f * FT, CD = 80.f * FT, CH = 112.f * FT;
			UStaticMeshComponent* CB = MakeCube(CA, nullptr,
				CorvidPos + FVector(0.f, 0.f, CH * 0.5f),
				FVector(CW, CD, CH), FLinearColor(0.88f, 0.78f, 0.56f),
				TEXT("CorvidBody"), 0.f, 0.92f, true);

			// Full glass curtain wall (reflective blue) — all 4 sides
			for (int32 f = 0; f < 4; ++f)
			{
				float fx = 0.f, fy = 0.f; FVector sc;
				if (f < 2) { fx = (f == 0) ? -(CW * 0.5f + 1.f) : (CW * 0.5f + 1.f); sc = FVector(0.02f, (CD - 2.f * FT) / 100.f, (CH * 0.85f) / 100.f); }
				else { fy = (f == 2) ? -(CD * 0.5f + 1.f) : (CD * 0.5f + 1.f); sc = FVector((CW - 2.f * FT) / 100.f, 0.02f, (CH * 0.85f) / 100.f); }
				UStaticMeshComponent* CG = NewObject<UStaticMeshComponent>(CA, *FString::Printf(TEXT("CvG_%d"), f));
				CG->SetStaticMesh(CubeMesh);
				CG->SetWorldLocation(CorvidPos + FVector(fx, fy, CH * 0.5f));
				CG->SetWorldScale3D(sc);
				CG->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				CG->RegisterComponent();
				CG->AttachToComponent(CB, FAttachmentTransformRules::KeepWorldTransform);
				CA->AddInstanceComponent(CG);
				// Dark recessed window opening
				ApplyPBR(CG, FLinearColor(0.06f, 0.05f, 0.04f), 0.f, 0.95f);
				++CompCount;
			}

			// Flat modern roof
			MakeCube(CA, CB, CorvidPos + FVector(0.f, 0.f, CH + 0.5f * FT),
				FVector(CW + 1.f * FT, CD + 1.f * FT, 0.5f * FT), FLinearColor(0.84f, 0.74f, 0.52f),
				TEXT("CorvidRoof"), 0.f, 0.92f);

			// CORVID TECHNOLOGIES sign
			UTextRenderComponent* CVT = NewObject<UTextRenderComponent>(CA, TEXT("CorvidSign"));
			CVT->SetText(FText::FromString(TEXT("CORVID\nTECHNOLOGIES")));
			CVT->SetTextRenderColor(FColor::White);
			CVT->SetWorldSize(3.f * FT);
			CVT->SetHorizontalAlignment(EHTA_Center);
			CVT->SetWorldLocation(CorvidPos + FVector(0.f, -(CD * 0.5f + 2.f), CH * 0.7f));
			CVT->SetWorldRotation(FRotator(0.f, 270.f, 0.f));
			CVT->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			CVT->RegisterComponent();
			CVT->AttachToComponent(CB, FAttachmentTransformRules::KeepWorldTransform);
			CA->AddInstanceComponent(CVT);
			++CompCount;

			// Outdoor patio with fire pit (south side)
			MakeCube(CA, CB,
				CorvidPos + FVector(0.f, -(CD * 0.5f + 30.f * FT), 0.5f),
				FVector(50.f * FT, 40.f * FT, 0.5f * FT), ConcreteGray,
				TEXT("CorvidPatio"), 0.f, 0.7f, true);
			// Fire pit (cylinder)
			MakeCyl(CA, CB,
				CorvidPos + FVector(0.f, -(CD * 0.5f + 30.f * FT), 2.f * FT),
				FVector(3.f * FT, 3.f * FT, 2.5f * FT), StoneGray,
				TEXT("FirePit"), 0.f, 0.7f);
		}
	}

	// ========================================================================
	// GREAT OUTDOORS STAGE — outdoor concert venue
	// Central gathering area between apartment rows
	// ========================================================================
	{
		const FVector StagePos = ComplexCenter + FVector(-80.f * FT, 0.f, 0.f);
		AActor* StgA = World->SpawnActor<AActor>(AActor::StaticClass(), StagePos, FRotator::ZeroRotator);
		if (StgA)
		{
			// Stage platform (raised 3 ft)
			UStaticMeshComponent* StgPlat = MakeCube(StgA, nullptr,
				StagePos + FVector(0.f, 0.f, 1.5f * FT),
				FVector(40.f * FT, 60.f * FT, 3.f * FT), FLinearColor(0.30f, 0.28f, 0.25f),
				TEXT("StagePlat"), 0.f, 0.6f, true);

			// Stage roof/canopy
			MakeCube(StgA, StgPlat,
				StagePos + FVector(0.f, 0.f, 18.f * FT),
				FVector(45.f * FT, 65.f * FT, 0.5f * FT), DarkTrim,
				TEXT("StageRoof"), 0.3f, 0.4f);

			// 4 support columns
			for (int32 col = 0; col < 4; ++col)
			{
				const float cx = (col % 2 == 0) ? -18.f * FT : 18.f * FT;
				const float cy = (col < 2) ? -28.f * FT : 28.f * FT;
				MakeCyl(StgA, StgPlat,
					StagePos + FVector(cx, cy, 9.f * FT),
					FVector(0.8f * FT, 0.8f * FT, 18.f * FT), DarkTrim,
					*FString::Printf(TEXT("StgCol_%d"), col), 0.5f, 0.3f);
			}

			// Lawn area in front of stage
			MakeCube(StgA, StgPlat,
				StagePos + FVector(0.f, 50.f * FT, 0.5f),
				FVector(80.f * FT, 60.f * FT, 0.5f), LawnGreen,
				TEXT("StageLawn"), 0.f, 0.9f);

			// "GREAT OUTDOORS STAGE" sign
			UTextRenderComponent* StgSign = NewObject<UTextRenderComponent>(StgA, TEXT("StageSign"));
			StgSign->SetText(FText::FromString(TEXT("GREAT OUTDOORS STAGE")));
			StgSign->SetTextRenderColor(FColor::White);
			StgSign->SetWorldSize(1.5f * FT);
			StgSign->SetHorizontalAlignment(EHTA_Center);
			StgSign->SetWorldLocation(StagePos + FVector(32.f * FT, 0.f, 15.f * FT));
			StgSign->SetWorldRotation(FRotator(0.f, 270.f, 0.f));
			StgSign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			StgSign->RegisterComponent();
			StgSign->AttachToComponent(StgPlat, FAttachmentTransformRules::KeepWorldTransform);
			StgA->AddInstanceComponent(StgSign);
			++CompCount;
		}
	}

	// ========================================================================
	// CLUBHOUSE + FITNESS CENTER — lakefront, west of apartments
	// ========================================================================
	{
		const FVector ClubPos = ComplexCenter + FVector(-20.f * FT, -200.f * FT, 0.f);
		AActor* ClA = World->SpawnActor<AActor>(AActor::StaticClass(), ClubPos, FRotator::ZeroRotator);
		if (ClA)
		{
			const float CW = 100.f * FT, CD = 80.f * FT, CH = 60.f * FT;
			UStaticMeshComponent* ClB = MakeCube(ClA, nullptr,
				ClubPos + FVector(0.f, 0.f, CH * 0.5f),
				FVector(CW, CD, CH), FLinearColor(0.86f, 0.76f, 0.54f),
				TEXT("ClubBody"), 0.f, 0.92f, true);

			// Large south-facing windows
			{
				UStaticMeshComponent* CG = NewObject<UStaticMeshComponent>(ClA, TEXT("ClubGlass"));
				CG->SetStaticMesh(CubeMesh);
				CG->SetWorldLocation(ClubPos + FVector(0.f, -(CD * 0.5f + 1.f), CH * 0.55f));
				CG->SetWorldScale3D(FVector((CW - 8.f * FT) / 100.f, 0.02f, (CH * 0.7f) / 100.f));
				CG->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				CG->RegisterComponent();
				CG->AttachToComponent(ClB, FAttachmentTransformRules::KeepWorldTransform);
				ClA->AddInstanceComponent(CG);
				ApplyPBR(CG, FLinearColor(0.06f, 0.05f, 0.04f), 0.f, 0.95f); // dark recessed opening
				++CompCount;
			}

			// Entrance overhang
			MakeCube(ClA, ClB, ClubPos + FVector(0.f, CD * 0.5f + 5.f * FT, CH * 0.8f),
				FVector(40.f * FT, 12.f * FT, 0.8f * FT), RoofCharcoal, TEXT("ClubOverhang"), 0.2f, 0.5f);

			// Roof
			MakeCube(ClA, ClB, ClubPos + FVector(0.f, 0.f, CH + 0.5f * FT),
				FVector(CW + 2.f * FT, CD + 2.f * FT, 0.8f * FT), RoofCharcoal, TEXT("ClubRoof"), 0.2f, 0.5f);

			// "CLUBHOUSE" sign
			UTextRenderComponent* ClSign = NewObject<UTextRenderComponent>(ClA, TEXT("ClubSign"));
			ClSign->SetText(FText::FromString(TEXT("LANGTREE CLUBHOUSE")));
			ClSign->SetTextRenderColor(FColor::White);
			ClSign->SetWorldSize(2.f * FT);
			ClSign->SetHorizontalAlignment(EHTA_Center);
			ClSign->SetWorldLocation(ClubPos + FVector(0.f, CD * 0.5f + 1.f, CH * 0.75f));
			ClSign->SetWorldRotation(FRotator(0.f, 270.f, 0.f));
			ClSign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ClSign->RegisterComponent();
			ClSign->AttachToComponent(ClB, FAttachmentTransformRules::KeepWorldTransform);
			ClA->AddInstanceComponent(ClSign);
			++CompCount;
		}
	}

	// ========================================================================
	// SALTWATER POOL — resort-style, lakefront, with cabanas & grill
	// ========================================================================
	{
		const FVector PoolC = ComplexCenter + FVector(60.f * FT, -260.f * FT, 0.f);
		AActor* PA = World->SpawnActor<AActor>(AActor::StaticClass(), PoolC, FRotator::ZeroRotator);
		if (PA)
		{
			UStaticMeshComponent* PDk = MakeCube(PA, nullptr,
				PoolC + FVector(0.f, 0.f, 0.5f * FT),
				FVector(100.f * FT, 80.f * FT, 1.f * FT), ConcreteGray,
				TEXT("PoolDeck"), 0.f, 0.7f, true);

			// Pool basin
			MakeCube(PA, PDk, PoolC + FVector(0.f, 0.f, -2.f * FT),
				FVector(60.f * FT, 30.f * FT, 5.f * FT), FLinearColor(0.08f, 0.25f, 0.45f),
				TEXT("PoolBasin"), 0.f, 0.4f);

			// Water surface
			{
				UStaticMeshComponent* PW = NewObject<UStaticMeshComponent>(PA, TEXT("PoolWater"));
				PW->SetStaticMesh(PlaneMesh);
				PW->SetWorldLocation(PoolC + FVector(0.f, 0.f, 0.3f * FT));
				PW->SetWorldScale3D(FVector(60.f * FT / 100.f, 30.f * FT / 100.f, 1.f));
				PW->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PW->RegisterComponent();
				PW->AttachToComponent(PDk, FAttachmentTransformRules::KeepWorldTransform);
				PA->AddInstanceComponent(PW);
				ApplyPBRGlass(PW, GlassMaterial, PoolBlue, 0.75f, 0.f, 0.02f);
				++CompCount;
			}

			// 4 cabanas
			for (int32 c = 0; c < 4; ++c)
			{
				const float CY = -25.f * FT + c * 18.f * FT;
				const FVector CP = PoolC + FVector(38.f * FT, CY, 0.f);
				for (int32 p = 0; p < 4; ++p)
				{
					const float px = (p % 2 == 0) ? -3.5f * FT : 3.5f * FT;
					const float py = (p < 2) ? -3.5f * FT : 3.5f * FT;
					MakeCyl(PA, PDk, CP + FVector(px, py, 4.f * FT),
						FVector(0.3f * FT, 0.3f * FT, 8.f * FT), WoodBrown,
						*FString::Printf(TEXT("CbP_%d_%d"), c, p), 0.f, 0.7f);
				}
				MakeCube(PA, PDk, CP + FVector(0.f, 0.f, 8.f * FT),
					FVector(9.f * FT, 9.f * FT, 0.5f * FT), CabanaWhite,
					*FString::Printf(TEXT("CbT_%d"), c), 0.f, 0.8f);
			}

			// Grill station
			MakeCube(PA, PDk, PoolC + FVector(-35.f * FT, -30.f * FT, 3.f * FT),
				FVector(6.f * FT, 4.f * FT, 6.f * FT), FLinearColor(0.30f, 0.30f, 0.32f),
				TEXT("GrillStn"), 0.6f, 0.4f);

			// Outdoor fireplace
			MakeCube(PA, PDk, PoolC + FVector(-35.f * FT, 30.f * FT, 3.5f * FT),
				FVector(5.f * FT, 5.f * FT, 7.f * FT), StoneGray,
				TEXT("PoolFire"), 0.f, 0.7f);
		}
	}

	// ========================================================================
	// BOARDWALK — extends from pool area west and south down to Lake Norman
	// Wooden walkway with railings, benches, and lampposts
	// ========================================================================
	{
		const FVector BWStart = ComplexCenter + FVector(60.f * FT, -260.f * FT, 0.f); // at pool
		AActor* BWA = World->SpawnActor<AActor>(AActor::StaticClass(), BWStart, FRotator::ZeroRotator);
		if (BWA)
		{
			// Main boardwalk heading south from pool
			UStaticMeshComponent* BWRoot = MakeCube(BWA, nullptr,
				BWStart + FVector(0.f, -80.f * FT, 1.5f * FT),
				FVector(12.f * FT, 120.f * FT, 1.f * FT), WoodBrown,
				TEXT("BWMain"), 0.f, 0.75f, true);

			// Boardwalk turns east along the lakefront
			MakeCube(BWA, BWRoot,
				BWStart + FVector(100.f * FT, -140.f * FT, 1.5f * FT),
				FVector(200.f * FT, 12.f * FT, 1.f * FT), WoodBrown,
				TEXT("BWSouth"), 0.f, 0.75f, true);

			// Corner connector piece
			MakeCube(BWA, BWRoot,
				BWStart + FVector(0.f, -140.f * FT, 1.5f * FT),
				FVector(14.f * FT, 14.f * FT, 1.f * FT), WoodBrown,
				TEXT("BWCorner"), 0.f, 0.75f, true);

			// Boardwalk continues further east to a lookout
			MakeCube(BWA, BWRoot,
				BWStart + FVector(260.f * FT, -140.f * FT, 1.5f * FT),
				FVector(120.f * FT, 12.f * FT, 1.f * FT), WoodBrown,
				TEXT("BWSouth2"), 0.f, 0.75f, true);

			// Lakeside lookout platform (wider section at the end)
			MakeCube(BWA, BWRoot,
				BWStart + FVector(340.f * FT, -140.f * FT, 1.5f * FT),
				FVector(30.f * FT, 30.f * FT, 1.f * FT), WoodBrown,
				TEXT("BWLookout"), 0.f, 0.75f, true);

			// Railings on both sides of main N-S section
			for (int32 s = 0; s < 2; ++s)
			{
				const float RX = (s == 0) ? -6.f * FT : 6.f * FT;
				MakeCube(BWA, BWRoot,
					BWStart + FVector(RX, -80.f * FT, 4.f * FT),
					FVector(0.3f * FT, 120.f * FT, 3.f * FT), DarkTrim,
					*FString::Printf(TEXT("BWR_EW_%d"), s), 0.5f, 0.4f);
			}

			// Railings on E-W sections
			for (int32 s = 0; s < 2; ++s)
			{
				const float RY = (s == 0) ? -146.f * FT : -134.f * FT;
				MakeCube(BWA, BWRoot,
					BWStart + FVector(180.f * FT, RY, 4.f * FT),
					FVector(360.f * FT, 0.3f * FT, 3.f * FT), DarkTrim,
					*FString::Printf(TEXT("BWR_NS_%d"), s), 0.5f, 0.4f);
			}

			// Lookout railing (3 sides)
			for (int32 r = 0; r < 3; ++r)
			{
				float rx = 0.f, ry = 0.f; FVector rsc;
				if (r == 0) { rx = 355.f * FT; rsc = FVector(0.3f * FT, 30.f * FT, 3.f * FT); }
				else if (r == 1) { ry = -155.f * FT; rx = 340.f * FT; rsc = FVector(30.f * FT, 0.3f * FT, 3.f * FT); }
				else { ry = -125.f * FT; rx = 340.f * FT; rsc = FVector(30.f * FT, 0.3f * FT, 3.f * FT); }
				MakeCube(BWA, BWRoot,
					BWStart + FVector(rx, ry, 4.f * FT),
					rsc, DarkTrim,
					*FString::Printf(TEXT("BWR_LO_%d"), r), 0.5f, 0.4f);
			}

			// Benches along the boardwalk (every ~60 ft)
			for (int32 bn = 0; bn < 6; ++bn)
			{
				const float BX = 20.f * FT + bn * 55.f * FT;
				// Seat
				MakeCube(BWA, BWRoot,
					BWStart + FVector(BX, -136.f * FT, 2.5f * FT),
					FVector(4.f * FT, 5.f * FT, 1.f * FT), WoodBrown,
					*FString::Printf(TEXT("BWBnch_%d"), bn), 0.f, 0.7f);
				// Backrest
				MakeCube(BWA, BWRoot,
					BWStart + FVector(BX, -138.f * FT, 4.f * FT),
					FVector(4.f * FT, 0.5f * FT, 2.5f * FT), WoodBrown,
					*FString::Printf(TEXT("BWBkR_%d"), bn), 0.f, 0.7f);
			}

			// Lampposts along boardwalk (every ~80 ft)
			for (int32 lp = 0; lp < 5; ++lp)
			{
				const float LX = 10.f * FT + lp * 70.f * FT;
				MakeCyl(BWA, BWRoot,
					BWStart + FVector(LX, -144.f * FT, 6.f * FT),
					FVector(0.3f * FT, 0.3f * FT, 12.f * FT), DarkTrim,
					*FString::Printf(TEXT("BWLmp_%d"), lp), 0.5f, 0.4f);
				// Lamp head
				UStaticMeshComponent* LH = NewObject<UStaticMeshComponent>(BWA,
					*FString::Printf(TEXT("BWLmH_%d"), lp));
				LH->SetStaticMesh(SphereMesh);
				LH->SetWorldLocation(BWStart + FVector(LX, -144.f * FT, 12.5f * FT));
				LH->SetWorldScale3D(FVector(1.2f * FT, 1.2f * FT, 1.2f * FT) / 100.f);
				LH->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				LH->RegisterComponent();
				LH->AttachToComponent(BWRoot, FAttachmentTransformRules::KeepWorldTransform);
				BWA->AddInstanceComponent(LH);
				ApplyPBR(LH, FLinearColor(0.95f, 0.85f, 0.55f), 0.f, 0.3f);
				++CompCount;
			}
		}
	}

	// ========================================================================
	// LAKE NORMAN — large body of water to the south of the complex
	// Northern shore runs along the boardwalk and pool area
	// ========================================================================
	{
		const FVector LakeCenter = ComplexCenter + FVector(120.f * FT, -450.f * FT, 0.f);
		AActor* LakeA = World->SpawnActor<AActor>(AActor::StaticClass(), LakeCenter, FRotator::ZeroRotator);
		if (LakeA)
		{
			// Lake water surface (large translucent plane)
			UStaticMeshComponent* LakeWater = NewObject<UStaticMeshComponent>(LakeA, TEXT("LakeNorman"));
			LakeWater->SetStaticMesh(PlaneMesh);
			LakeWater->SetWorldLocation(LakeCenter + FVector(0.f, 0.f, -4.f));
			LakeWater->SetWorldScale3D(FVector(800.f * FT / 100.f, 600.f * FT / 100.f, 1.f));
			LakeWater->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			LakeWater->RegisterComponent();
			LakeA->SetRootComponent(LakeWater);
			LakeA->AddInstanceComponent(LakeWater);
			ApplyPBRGlass(LakeWater, GlassMaterial, FLinearColor(0.04f, 0.12f, 0.22f), 0.8f, 0.f, 0.05f);
			++CompCount;

			// Lake bed (dark bottom visible through water)
			UStaticMeshComponent* LakeBed = NewObject<UStaticMeshComponent>(LakeA, TEXT("LakeBed"));
			LakeBed->SetStaticMesh(CubeMesh);
			LakeBed->SetWorldLocation(LakeCenter + FVector(0.f, 0.f, -10.f * FT));
			LakeBed->SetWorldScale3D(FVector(800.f * FT / 100.f, 600.f * FT / 100.f, 0.1f));
			LakeBed->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			LakeBed->RegisterComponent();
			LakeBed->AttachToComponent(LakeWater, FAttachmentTransformRules::KeepWorldTransform);
			LakeA->AddInstanceComponent(LakeBed);
			ApplyPBR(LakeBed, FLinearColor(0.06f, 0.10f, 0.14f), 0.f, 0.9f);
			++CompCount;

			// Shoreline bank — sloped earth between grass and water
			// North edge of the shore (closest to buildings)
			MakeCube(LakeA, LakeWater,
				ComplexCenter + FVector(120.f * FT, -280.f * FT, -2.f),
				FVector(700.f * FT, 30.f * FT, 2.f * FT), FLinearColor(0.35f, 0.30f, 0.20f),
				TEXT("ShoreBankE"), 0.f, 0.85f);

			// Sandy beach strip along shore
			MakeCube(LakeA, LakeWater,
				ComplexCenter + FVector(120.f * FT, -310.f * FT, -1.f),
				FVector(700.f * FT, 25.f * FT, 0.5f * FT), FLinearColor(0.72f, 0.66f, 0.50f),
				TEXT("ShoreBeach"), 0.f, 0.8f, true);
		}
	}

	// ========================================================================
	// PARKING DECK — covered, east of apartments, controlled access
	// ========================================================================
	{
		const FVector PkP = ComplexCenter + FVector(0.f, 160.f * FT, 0.f);
		AActor* PkA = World->SpawnActor<AActor>(AActor::StaticClass(), PkP, FRotator::ZeroRotator);
		if (PkA)
		{
			const float PW = 150.f * FT, PD = 80.f * FT, PH = 24.f * FT;
			UStaticMeshComponent* PkB = MakeCube(PkA, nullptr,
				PkP + FVector(0.f, 0.f, PH * 0.5f),
				FVector(PW, PD, PH), ConcreteGray,
				TEXT("ParkDeck"), 0.f, 0.75f, true);

			// Dark recesses on long sides
			for (int32 s = 0; s < 2; ++s)
			{
				const float SO = (s == 0) ? -(PD * 0.5f + 1.f) : (PD * 0.5f + 1.f);
				for (int32 lv = 0; lv < 2; ++lv)
				{
					MakeCube(PkA, PkB,
						PkP + FVector(0.f, SO, 4.f * FT + lv * 12.f * FT),
						FVector(PW - 10.f * FT, 0.3f * FT, 9.f * FT), FLinearColor(0.03f, 0.03f, 0.04f),
						*FString::Printf(TEXT("PkO_%d_%d"), s, lv), 0.f, 0.9f);
				}
			}

			MakeCube(PkA, PkB, PkP + FVector(0.f, 0.f, PH + 0.5f * FT),
				FVector(PW + 1.f * FT, PD + 1.f * FT, 0.5f * FT), FLinearColor(0.35f, 0.35f, 0.37f),
				TEXT("ParkRoof"), 0.2f, 0.5f);
		}
	}

	// ========================================================================
	// DOG PARK — fenced, north of apartments
	// ========================================================================
	{
		const FVector DPP = ComplexCenter + FVector(-280.f * FT, 60.f * FT, 0.f);
		AActor* DA = World->SpawnActor<AActor>(AActor::StaticClass(), DPP, FRotator::ZeroRotator);
		if (DA)
		{
			const float DW = 80.f * FT, DD = 60.f * FT;
			UStaticMeshComponent* DL = MakeCube(DA, nullptr,
				DPP + FVector(0.f, 0.f, 0.5f),
				FVector(DW, DD, 1.f), LawnGreen, TEXT("DogLawn"), 0.f, 0.9f);

			const float FH = 4.f * FT;
			int32 FI = 0;
			for (int32 side = 0; side < 4; ++side)
			{
				const int32 Cnt = (side < 2) ? 8 : 6;
				for (int32 i = 0; i <= Cnt; ++i)
				{
					float fx = 0.f, fy = 0.f;
					if (side == 0) { fx = -DW * 0.5f; fy = -DD * 0.5f + i * (DD / Cnt); }
					else if (side == 1) { fx = DW * 0.5f; fy = -DD * 0.5f + i * (DD / Cnt); }
					else if (side == 2) { fy = -DD * 0.5f; fx = -DW * 0.5f + i * (DW / Cnt); }
					else { fy = DD * 0.5f; fx = -DW * 0.5f + i * (DW / Cnt); }
					MakeCyl(DA, DL, DPP + FVector(fx, fy, FH * 0.5f),
						FVector(0.15f * FT, 0.15f * FT, FH), DarkTrim,
						*FString::Printf(TEXT("Fn_%d"), FI++), 0.5f, 0.4f);
				}
			}
			for (int32 side = 0; side < 4; ++side)
			{
				for (int32 lv = 0; lv < 2; ++lv)
				{
					float fx = 0.f, fy = 0.f; FVector sc;
					const float RZ = 1.5f * FT + lv * 2.f * FT;
					if (side == 0) { fx = -DW * 0.5f; sc = FVector(0.02f, DD / 100.f, 0.02f); }
					else if (side == 1) { fx = DW * 0.5f; sc = FVector(0.02f, DD / 100.f, 0.02f); }
					else if (side == 2) { fy = -DD * 0.5f; sc = FVector(DW / 100.f, 0.02f, 0.02f); }
					else { fy = DD * 0.5f; sc = FVector(DW / 100.f, 0.02f, 0.02f); }
					MakeCube(DA, DL, DPP + FVector(fx, fy, RZ),
						sc * 100.f, DarkTrim,
						*FString::Printf(TEXT("FR_%d_%d"), side, lv), 0.5f, 0.4f);
				}
			}
		}
	}

	// ========================================================================
	// LANDSCAPING — trees along streets and around amenities
	// ========================================================================
	{
		// Trees along Landings Drive (now E-W)
		for (int32 t = 0; t < 14; ++t)
		{
			const float TX = ComplexCenter.X - 320.f * FT + t * 48.f * FT;
			SpawnPalmTree(FVector(TX, ComplexCenter.Y - 38.f * FT, 0.f), 55.f, false);
			SpawnPalmTree(FVector(TX, ComplexCenter.Y + 38.f * FT, 0.f), 50.f, false);
		}
		// Trees around pool
		for (int32 t = 0; t < 6; ++t)
		{
			const float A = t * (360.f / 6.f);
			const float R = FMath::DegreesToRadians(A);
			SpawnPalmTree(FVector(
				ComplexCenter.X + 60.f * FT + FMath::Cos(R) * 50.f * FT,
				ComplexCenter.Y - 260.f * FT + FMath::Sin(R) * 60.f * FT,
				0.f), 65.f, false);
		}
		// Trees along lakefront (now south)
		for (int32 t = 0; t < 10; ++t)
		{
			const float TX = ComplexCenter.X - 250.f * FT + t * 65.f * FT;
			SpawnPalmTree(FVector(TX, ComplexCenter.Y - 340.f * FT, 0.f), 70.f, true);
		}
		// Trees along Langtree Village Drive (now N-S, east)
		for (int32 t = 0; t < 6; ++t)
		{
			const float TY = ComplexCenter.Y - 100.f * FT + t * 60.f * FT;
			SpawnPalmTree(FVector(ComplexCenter.X + 340.f * FT, TY, 0.f), 50.f, false);
		}
	}


	// ========================================================================
	// WARM AMBIENT LIGHTING — torch-like point lights for night glow
	// Positioned at tower tops and along the base of the fort walls
	// ========================================================================
	{
		AActor* LightActor = World->SpawnActor<AActor>(AActor::StaticClass(), ComplexCenter, FRotator::ZeroRotator);
		if (LightActor)
		{
			USceneComponent* LRoot = NewObject<USceneComponent>(LightActor, TEXT("LTLightRoot"));
			LRoot->RegisterComponent();
			LightActor->SetRootComponent(LRoot);

			const FColor WarmTorch(255, 170, 60);  // warm amber/orange
			const float TorchIntensity = 4000.f;
			const float TorchRadius = 12000.f;

			// Tower-top lights — 4 outer corners of each row (rotated layout)
			struct FTorchPos { float X; float Y; float Z; };
			const float TowerTopZ = AptH + 10.f * FT;
			const FTorchPos TorchPositions[] = {
				// South row — west end corners
				{ -AptSpacing - AptW * 0.5f, -RowSpacing - AptD * 0.5f, TowerTopZ },
				{  AptSpacing + AptW * 0.5f, -RowSpacing - AptD * 0.5f, TowerTopZ },
				// South row — north face, mid-building for cross-illumination
				{ 0.f, -RowSpacing + AptD * 0.5f, TowerTopZ },
				// North row — east end corners
				{ -AptSpacing - AptW * 0.5f,  RowSpacing + AptD * 0.5f, TowerTopZ },
				{  AptSpacing + AptW * 0.5f,  RowSpacing + AptD * 0.5f, TowerTopZ },
				// North row — south face, mid-building for cross-illumination
				{ 0.f,  RowSpacing - AptD * 0.5f, TowerTopZ },
				// Ground-level warm uplights at the fort entrance (west)
				{ -AptSpacing - AptW * 0.5f - 10.f * FT, 0.f, 8.f * FT },
				// Ground-level warm uplight at east end
				{  AptSpacing + AptW * 0.5f + 10.f * FT, 0.f, 8.f * FT },
			};

			for (int32 t = 0; t < 8; ++t)
			{
				const FVector TPos = ComplexCenter + FVector(TorchPositions[t].X, TorchPositions[t].Y, TorchPositions[t].Z);
				UPointLightComponent* PL = NewObject<UPointLightComponent>(LightActor,
					*FString::Printf(TEXT("FortTorch_%d"), t));
				PL->SetWorldLocation(TPos);
				PL->SetLightColor(WarmTorch);
				PL->SetIntensity(TorchIntensity);
				PL->SetAttenuationRadius(TorchRadius);
				PL->SetCastShadows(false);
				PL->RegisterComponent();
				PL->AttachToComponent(LRoot, FAttachmentTransformRules::KeepWorldTransform);
				LightActor->AddInstanceComponent(PL);

				// Visible torch flame sphere (small glowing orange ball)
				UStaticMeshComponent* Flame = NewObject<UStaticMeshComponent>(LightActor,
					*FString::Printf(TEXT("Flame_%d"), t));
				Flame->SetStaticMesh(SphereMesh);
				Flame->SetWorldLocation(TPos);
				Flame->SetWorldScale3D(FVector(0.015f, 0.015f, 0.02f));
				Flame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Flame->RegisterComponent();
				Flame->AttachToComponent(LRoot, FAttachmentTransformRules::KeepWorldTransform);
				LightActor->AddInstanceComponent(Flame);
				UMaterialInstanceDynamic* FlameMat = UMaterialInstanceDynamic::Create(ColorMaterial, Flame);
				FlameMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(1.0f, 0.6f, 0.15f));
				FlameMat->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor(3.0f, 1.5f, 0.3f));
				FlameMat->SetScalarParameterValue(TEXT("Roughness"), 0.1f);
				Flame->SetMaterial(0, FlameMat);
				CompCount += 2;
			}
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("Langtree Lake Norman: Spawned sand fort complex with %d components."), CompCount);
}

// ---------------------------------------------------------------------------
// SpawnNPCHelicopter — one AI-controlled Black Hawk patrol helicopter
// ---------------------------------------------------------------------------
void UPHCCitySubsystem::SpawnNPCHelicopter()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Always spawn — pair respawns when both are shot down
	bNPCHeliPermanentlyDestroyed = false;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	UStaticMesh* ArrowConeMesh = LoadObject<UStaticMesh>(nullptr,
		TEXT("/Engine/BasicShapes/Cone.Cone"));
	UMaterial* ArrowGlassMat = LoadObject<UMaterial>(nullptr,
		TEXT("/Game/Materials/M_PHCGlass.M_PHCGlass"));

	// Lambda to configure a spawned NPC helicopter
	auto ConfigureNPCHeli = [&](APHCBlackHawkPawn* Heli, TArray<TObjectPtr<UMaterialInstanceDynamic>>& MIDs,
		UStaticMeshComponent*& OutArrow)
	{
		if (!Heli) return;

		// NPC helicopters are local-only — disable replication so the net driver
		// doesn't destroy them when connected to a dedicated server.
		Heli->SetReplicates(false);
		Heli->SetReplicateMovement(false);

		Heli->bIsOccupied = true;
		Heli->MinAltitude = -100000.f;
		Heli->Tags.Add(FName("NPCHelicopter"));

		if (Heli->VehicleCollision)
		{
			Heli->VehicleCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Heli->VehicleCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
			Heli->VehicleCollision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		}

		MIDs.Empty();
		for (UStaticMeshComponent* Comp : Heli->BodyColoredMeshes)
		{
			if (!Comp) continue;
			UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Comp->GetMaterial(0));
			if (MID) MIDs.Add(MID);
		}

		// Liquid glass arrow indicator above helicopter
		if (ArrowConeMesh && ArrowGlassMat && Heli->VehicleCollision)
		{
			UStaticMeshComponent* Arrow = NewObject<UStaticMeshComponent>(Heli, TEXT("ArrowIndicator"));
			Arrow->SetupAttachment(Heli->VehicleCollision);
			Arrow->SetStaticMesh(ArrowConeMesh);
			Arrow->SetRelativeLocation(FVector(0.f, 0.f, 250.f));
			Arrow->SetRelativeRotation(FRotator(180.f, 0.f, 0.f));
			Arrow->SetRelativeScale3D(FVector(1.5f, 1.5f, 2.0f));
			Arrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Arrow->SetCastShadow(false);

			UMaterialInstanceDynamic* ArrowMID = UMaterialInstanceDynamic::Create(ArrowGlassMat, Heli);
			ArrowMID->SetVectorParameterValue(TEXT("BaseColor"),
				FLinearColor(1.0f, 0.9f, 0.3f));
			ArrowMID->SetScalarParameterValue(TEXT("Opacity"), 0.8f);
			ArrowMID->SetScalarParameterValue(TEXT("Metallic"), 0.3f);
			ArrowMID->SetScalarParameterValue(TEXT("Roughness"), 0.05f);
			ArrowMID->SetVectorParameterValue(TEXT("EmissiveColor"),
				FLinearColor(10.0f, 8.0f, 2.0f));
			Arrow->SetMaterial(0, ArrowMID);
			Arrow->RegisterComponent();
			OutArrow = Arrow;
		}

		Heli->SetActorHiddenInGame(true);
	};

	// Helicopter 1
	const FVector SpawnPos1(-2000.f, -10000.f, 2500.f);
	NPCHelicopter = World->SpawnActor<APHCBlackHawkPawn>(APHCBlackHawkPawn::StaticClass(), SpawnPos1, FRotator::ZeroRotator, Params);
	UStaticMeshComponent* Arrow1Raw = nullptr;
	ConfigureNPCHeli(NPCHelicopter, NPCHeliBodyMIDs, Arrow1Raw);
	NPCHeliArrow = Arrow1Raw;
	bNPCHeliVisible = true;
	NPCHeliPatrolAngle = 0.f;
	NPCHeliHealth = 240.f;
	bNPCHeliCrashing = false;
	NPCHeliCrashTimer = 0.f;
	NPCHeliDamageFlashTimer = 0.f;
	bNPCHeli1Down = false;
	if (NPCHelicopter)
	{
		NPCHelicopter->SetActorHiddenInGame(false);
		UE_LOG(LogPlayhouseCity, Warning, TEXT("NPC Helicopter 1 spawned at %s"), *NPCHelicopter->GetActorLocation().ToString());
	}

	// Helicopter 2 — starts on opposite side
	const FVector SpawnPos2(-18000.f, -10000.f, 2500.f);
	NPCHelicopter2 = World->SpawnActor<APHCBlackHawkPawn>(APHCBlackHawkPawn::StaticClass(), SpawnPos2, FRotator::ZeroRotator, Params);
	UStaticMeshComponent* Arrow2Raw = nullptr;
	ConfigureNPCHeli(NPCHelicopter2, NPCHeliBodyMIDs2, Arrow2Raw);
	NPCHeliArrow2 = Arrow2Raw;
	bNPCHeliVisible2 = true;
	NPCHeliPatrolAngle2 = UE_PI; // opposite side
	NPCHeliHealth2 = 240.f;
	bNPCHeliCrashing2 = false;
	NPCHeliCrashTimer2 = 0.f;
	NPCHeliDamageFlashTimer2 = 0.f;
	bNPCHeli2Down = false;
	if (NPCHelicopter2)
	{
		NPCHelicopter2->SetActorHiddenInGame(false);
		UE_LOG(LogPlayhouseCity, Warning, TEXT("NPC Helicopter 2 spawned at %s"), *NPCHelicopter2->GetActorLocation().ToString());
	}
	else
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("NPC Helicopter 2 FAILED to spawn!"));
	}

	NPCHeliPairRespawnTimer = -1.f;

	UE_LOG(LogPlayhouseCity, Log, TEXT("2 NPC patrol helicopters spawned."));
}

// ---------------------------------------------------------------------------
void UPHCCitySubsystem::OnNPCHeliDestroyed()
{
	bNPCHeliCrashing = true;
	NPCHeliCrashTimer = 0.f;
	bNPCHeli1Down = true;

	// Start pair respawn timer only when both are down
	if (bNPCHeli2Down && NPCHeliPairRespawnTimer < 0.f)
	{
		NPCHeliPairRespawnTimer = 60.f;
	}
}

// ---------------------------------------------------------------------------
void UPHCCitySubsystem::OnNPCHeli2Destroyed()
{
	bNPCHeliCrashing2 = true;
	NPCHeliCrashTimer2 = 0.f;
	bNPCHeli2Down = true;

	// Start pair respawn timer only when both are down
	if (bNPCHeli1Down && NPCHeliPairRespawnTimer < 0.f)
	{
		NPCHeliPairRespawnTimer = 60.f;
	}
}

// ---------------------------------------------------------------------------
void UPHCCitySubsystem::RespawnNPCHelicopter()
{
	auto ResetHeli = [this](APHCBlackHawkPawn* Heli, TArray<TObjectPtr<UMaterialInstanceDynamic>>& MIDs,
		float& Health, bool& bCrashing, float& CrashTimer, float& DmgFlash, float& PatrolAngle,
		bool& bVisible, bool& bDown, const FVector& SpawnPos, float StartAngle)
	{
		if (!Heli) return;

		Health = 240.f;
		bCrashing = false;
		CrashTimer = 0.f;
		DmgFlash = 0.f;
		PatrolAngle = StartAngle;
		bDown = false;

		for (UMaterialInstanceDynamic* MID : MIDs)
		{
			if (MID) MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
		}

		Heli->SetActorLocation(SpawnPos);
		Heli->SetActorRotation(FRotator::ZeroRotator);
		Heli->bIsOccupied = true;

		const bool bDay = (DayNightTime > 0.02f && DayNightTime < 0.48f);
		Heli->SetActorHiddenInGame(!bDay);
		bVisible = bDay;
	};

	ResetHeli(NPCHelicopter, NPCHeliBodyMIDs, NPCHeliHealth, bNPCHeliCrashing,
		NPCHeliCrashTimer, NPCHeliDamageFlashTimer, NPCHeliPatrolAngle,
		bNPCHeliVisible, bNPCHeli1Down, FVector(-2000.f, -10000.f, 2500.f), 0.f);

	ResetHeli(NPCHelicopter2, NPCHeliBodyMIDs2, NPCHeliHealth2, bNPCHeliCrashing2,
		NPCHeliCrashTimer2, NPCHeliDamageFlashTimer2, NPCHeliPatrolAngle2,
		bNPCHeliVisible2, bNPCHeli2Down, FVector(-18000.f, -10000.f, 2500.f), UE_PI);

	NPCHeliPairRespawnTimer = -1.f;

	UE_LOG(LogPlayhouseCity, Log, TEXT("Both NPC helicopters respawned."));
}

// ---------------------------------------------------------------------------
void UPHCCitySubsystem::SaveDestroyedVehicleState()
{
	const FString FilePath = FPaths::ProjectSavedDir() / TEXT("Vehicles") / TEXT("destroyed.dat");
	const FString Directory = FPaths::GetPath(FilePath);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*Directory))
	{
		PlatformFile.CreateDirectoryTree(*Directory);
	}
	FString Data;
	if (bNPCHeliPermanentlyDestroyed) Data += TEXT("NPCHeli\n");
	if (bAirportBlackbirdDestroyed) Data += TEXT("AirportBlackbird\n");
	FFileHelper::SaveStringToFile(Data, *FilePath);
}

void UPHCCitySubsystem::LoadDestroyedVehicleState()
{
	const FString FilePath = FPaths::ProjectSavedDir() / TEXT("Vehicles") / TEXT("destroyed.dat");
	FString Content;
	if (FFileHelper::LoadFileToString(Content, *FilePath))
	{
		bNPCHeliPermanentlyDestroyed = Content.Contains(TEXT("NPCHeli"));
		bAirportBlackbirdDestroyed = Content.Contains(TEXT("AirportBlackbird"));
	}
}

// ---------------------------------------------------------------------------
// SpawnNPCPlayers — 15 wandering NPC player characters (always visible, damageable)
// ---------------------------------------------------------------------------
void UPHCCitySubsystem::SpawnNPCPlayers()
{
	UWorld* World = GetWorld();
	if (!World || !SphereMesh || !CylinderMesh || !CubeMesh || !ColorMaterial) return;

	const FVector Spawns[NumNPCPlayers] = {
		{   500.f,    300.f, 90.f },
		{  -400.f,    600.f, 90.f },
		{   800.f,   -500.f, 90.f },
		{  -600.f,   -400.f, 90.f },
		{   200.f,    900.f, 90.f },
		{  3000.f,   2000.f, 90.f },
		{ -2500.f,   3500.f, 90.f },
		{  5000.f,  -3000.f, 90.f },
		{ -4000.f,  -2000.f, 90.f },
		{  1500.f,   5000.f, 90.f },
		{ -6000.f,   1000.f, 90.f },
		{  7000.f,   4000.f, 90.f },
		{ -3000.f,  -6000.f, 90.f },
		{  4500.f,   6000.f, 90.f },
		{ -5000.f,   5000.f, 90.f },
	};

	// Colors (matching PHCCityCharacter::BeginPlay)
	const FLinearColor Skin(0.55f, 0.38f, 0.26f);
	const FLinearColor SkinDark(0.42f, 0.28f, 0.18f);
	const FLinearColor Hair(0.02f, 0.02f, 0.03f);
	const FLinearColor Shirt(0.12f, 0.38f, 0.18f);
	const FLinearColor Denim(0.08f, 0.10f, 0.20f);
	const FLinearColor BeltColor(0.06f, 0.04f, 0.03f);
	const FLinearColor Buckle(0.65f, 0.60f, 0.50f);
	const FLinearColor Shoe(0.92f, 0.92f, 0.90f);
	const FLinearColor SoleColor(0.05f, 0.05f, 0.05f);

	for (int32 NI = 0; NI < NumNPCPlayers; ++NI)
	{
		FNPCPlayerData& NP = NPCPlayers[NI];
		NP.SpawnPosition = Spawns[NI];

		AActor* A = World->SpawnActor<AActor>(AActor::StaticClass(), Spawns[NI], FRotator::ZeroRotator);
		if (!A) continue;
		NP.Actor = A;
		A->Tags.Add(FName("NPCPlayer"));

		// Root: box component for hitscan collision (QueryOnly)
		UBoxComponent* Root = NewObject<UBoxComponent>(A, TEXT("NPCRoot"));
		Root->SetBoxExtent(FVector(22.f, 26.f, 90.f));
		Root->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Root->SetCollisionResponseToAllChannels(ECR_Ignore);
		Root->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		Root->SetGenerateOverlapEvents(false);
		Root->RegisterComponent();
		A->SetRootComponent(Root);
		A->AddInstanceComponent(Root);

		// Mesh helpers (runtime, not constructor)
		auto MC = [&](const FName& Name) -> UStaticMeshComponent*
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(A, Name);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetGenerateOverlapEvents(false);
			C->CastShadow = false;
			C->RegisterComponent();
			C->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			A->AddInstanceComponent(C);
			return C;
		};

		auto MakeOval = [&](const FName& Name, const FVector& Loc, const FVector& Scale) -> UStaticMeshComponent*
		{
			UStaticMeshComponent* C = MC(Name);
			C->SetStaticMesh(SphereMesh);
			C->SetRelativeScale3D(Scale);
			C->SetRelativeLocation(Loc);
			return C;
		};

		auto MakeSph = [&](const FName& Name, const FVector& Loc, float R) -> UStaticMeshComponent*
		{
			UStaticMeshComponent* C = MC(Name);
			C->SetStaticMesh(SphereMesh);
			float S = R / 50.f;
			C->SetRelativeScale3D(FVector(S, S, S));
			C->SetRelativeLocation(Loc);
			return C;
		};

		auto MakeCyl = [&](const FName& Name, const FVector& Loc, float R, float H) -> UStaticMeshComponent*
		{
			UStaticMeshComponent* C = MC(Name);
			C->SetStaticMesh(CylinderMesh);
			C->SetRelativeScale3D(FVector(R / 50.f, R / 50.f, H / 100.f));
			C->SetRelativeLocation(Loc);
			return C;
		};

		auto MakeCube = [&](const FName& Name, const FVector& Loc, const FVector& SizeUU) -> UStaticMeshComponent*
		{
			UStaticMeshComponent* C = MC(Name);
			C->SetStaticMesh(CubeMesh);
			C->SetRelativeScale3D(SizeUU / 100.f);
			C->SetRelativeLocation(Loc);
			return C;
		};

		auto SetColor = [&](UStaticMeshComponent* Comp, const FLinearColor& Color, float Roughness, float Metallic = 0.f)
		{
			if (!Comp) return;
			UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(ColorMaterial, Comp);
			if (!MID) return;
			MID->SetVectorParameterValue(TEXT("BaseColor"), Color);
			MID->SetScalarParameterValue(TEXT("Roughness"), Roughness);
			MID->SetScalarParameterValue(TEXT("Metallic"), Metallic);
			Comp->SetMaterial(0, MID);
			NP.BodyMIDs.Add(MID);
		};

		// Unique per-NPC names
		auto N = [NI](const TCHAR* Base) -> FName
		{
			return FName(*FString::Printf(TEXT("NPC%d_%s"), NI, Base));
		};

		// ---- HEAD ----
		NP.HeadMesh = MakeOval(N(TEXT("Head")), FVector(0, 0, 76), FVector(0.26f, 0.28f, 0.28f));
		SetColor(NP.HeadMesh, Skin, 0.45f);
		NP.JawMesh = MakeOval(N(TEXT("Jaw")), FVector(2, 0, 67), FVector(0.22f, 0.24f, 0.16f));
		SetColor(NP.JawMesh, SkinDark, 0.50f);

		// ---- HAIR ----
		NP.HairTopMesh = MakeOval(N(TEXT("HairTop")), FVector(-1, 0, 88), FVector(0.28f, 0.30f, 0.10f));
		SetColor(NP.HairTopMesh, Hair, 0.85f);
		NP.HairBackMesh = MakeOval(N(TEXT("HairBack")), FVector(-8, 0, 80), FVector(0.12f, 0.26f, 0.18f));
		SetColor(NP.HairBackMesh, Hair, 0.85f);

		// ---- NECK ----
		NP.NeckMesh = MakeCyl(N(TEXT("Neck")), FVector(0, 0, 60), 4.f, 10.f);
		SetColor(NP.NeckMesh, Skin, 0.45f);

		// ---- TORSO ----
		NP.ChestMesh = MakeOval(N(TEXT("Chest")), FVector(1, 0, 44), FVector(0.22f, 0.44f, 0.26f));
		SetColor(NP.ChestMesh, Shirt, 0.82f);
		NP.AbsMesh = MakeOval(N(TEXT("Abs")), FVector(0, 0, 28), FVector(0.16f, 0.32f, 0.18f));
		SetColor(NP.AbsMesh, Shirt, 0.82f);
		NP.PelvisMesh = MakeOval(N(TEXT("Pelvis")), FVector(0, 0, 12), FVector(0.17f, 0.36f, 0.20f));
		SetColor(NP.PelvisMesh, Denim, 0.78f);

		// ---- CLOTHING DETAIL ----
		NP.CollarMesh = MakeCube(N(TEXT("Collar")), FVector(2, 0, 56), FVector(10, 26, 2));
		SetColor(NP.CollarMesh, Shirt, 0.82f);
		NP.BeltMesh = MakeCube(N(TEXT("Belt")), FVector(0, 0, 20), FVector(12, 36, 3));
		SetColor(NP.BeltMesh, BeltColor, 0.35f);
		NP.BeltBuckleMesh = MakeCube(N(TEXT("Buckle")), FVector(8, 0, 20), FVector(3, 4, 4));
		SetColor(NP.BeltBuckleMesh, Buckle, 0.30f, 0.6f);

		// ---- LEFT ARM ----
		NP.LShoulderMesh = MakeSph(N(TEXT("LShld")), FVector(0, -25, 54), 7.f);
		SetColor(NP.LShoulderMesh, Shirt, 0.82f);
		NP.LUpperArmMesh = MakeCyl(N(TEXT("LUArm")), FVector(0, -26, 44), 4.5f, 20.f);
		SetColor(NP.LUpperArmMesh, Shirt, 0.82f);
		NP.LElbowMesh = MakeSph(N(TEXT("LElbow")), FVector(0, -26, 32), 4.f);
		SetColor(NP.LElbowMesh, Skin, 0.45f);
		NP.LForearmMesh = MakeCyl(N(TEXT("LFArm")), FVector(0, -26, 20), 4.f, 18.f);
		SetColor(NP.LForearmMesh, Skin, 0.45f);
		NP.LHandMesh = MakeSph(N(TEXT("LHand")), FVector(0, -26, 6), 3.5f);
		SetColor(NP.LHandMesh, Skin, 0.45f);

		// ---- RIGHT ARM ----
		NP.RShoulderMesh = MakeSph(N(TEXT("RShld")), FVector(0, 25, 54), 7.f);
		SetColor(NP.RShoulderMesh, Shirt, 0.82f);
		NP.RUpperArmMesh = MakeCyl(N(TEXT("RUArm")), FVector(0, 26, 44), 4.5f, 20.f);
		SetColor(NP.RUpperArmMesh, Shirt, 0.82f);
		NP.RElbowMesh = MakeSph(N(TEXT("RElbow")), FVector(0, 26, 32), 4.f);
		SetColor(NP.RElbowMesh, Skin, 0.45f);
		NP.RForearmMesh = MakeCyl(N(TEXT("RFArm")), FVector(0, 26, 20), 4.f, 18.f);
		SetColor(NP.RForearmMesh, Skin, 0.45f);
		NP.RHandMesh = MakeSph(N(TEXT("RHand")), FVector(0, 26, 6), 3.5f);
		SetColor(NP.RHandMesh, Skin, 0.45f);

		// ---- LEFT LEG ----
		NP.LThighMesh = MakeCyl(N(TEXT("LThigh")), FVector(0, -11, -12), 5.5f, 28.f);
		SetColor(NP.LThighMesh, Denim, 0.78f);
		NP.LKneeMesh = MakeSph(N(TEXT("LKnee")), FVector(0, -11, -30), 5.f);
		SetColor(NP.LKneeMesh, Denim, 0.78f);
		NP.LShinMesh = MakeCyl(N(TEXT("LShin")), FVector(0, -11, -48), 4.5f, 24.f);
		SetColor(NP.LShinMesh, Denim, 0.78f);
		NP.LFootMesh = MakeCube(N(TEXT("LFoot")), FVector(4, -11, -68), FVector(22, 12, 8));
		SetColor(NP.LFootMesh, Shoe, 0.45f);
		NP.LSoleMesh = MakeCube(N(TEXT("LSole")), FVector(4, -11, -73), FVector(22, 12, 2.5f));
		SetColor(NP.LSoleMesh, SoleColor, 0.90f);

		// ---- RIGHT LEG ----
		NP.RThighMesh = MakeCyl(N(TEXT("RThigh")), FVector(0, 11, -12), 5.5f, 28.f);
		SetColor(NP.RThighMesh, Denim, 0.78f);
		NP.RKneeMesh = MakeSph(N(TEXT("RKnee")), FVector(0, 11, -30), 5.f);
		SetColor(NP.RKneeMesh, Denim, 0.78f);
		NP.RShinMesh = MakeCyl(N(TEXT("RShin")), FVector(0, 11, -48), 4.5f, 24.f);
		SetColor(NP.RShinMesh, Denim, 0.78f);
		NP.RFootMesh = MakeCube(N(TEXT("RFoot")), FVector(4, 11, -68), FVector(22, 12, 8));
		SetColor(NP.RFootMesh, Shoe, 0.45f);
		NP.RSoleMesh = MakeCube(N(TEXT("RSole")), FVector(4, 11, -73), FVector(22, 12, 2.5f));
		SetColor(NP.RSoleMesh, SoleColor, 0.90f);

		// Cyan underglow point light
		UPointLightComponent* Glow = NewObject<UPointLightComponent>(A, N(TEXT("Glow")));
		Glow->SetRelativeLocation(FVector(0.f, 0.f, -70.f));
		Glow->SetLightColor(FLinearColor(0.f, 1.f, 1.f));
		Glow->SetIntensity(8000.f);
		Glow->SetAttenuationRadius(600.f);
		Glow->SetCastShadows(false);
		Glow->MaxDrawDistance = 15000.f;
		Glow->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
		Glow->RegisterComponent();
		A->AddInstanceComponent(Glow);
		NP.GlowLight = Glow;

		// Floating arrow indicator above head
		if (ConeMesh && GlassMaterial)
		{
			UStaticMeshComponent* Arrow = NewObject<UStaticMeshComponent>(A, N(TEXT("Arrow")));
			Arrow->SetStaticMesh(ConeMesh);
			Arrow->SetRelativeLocation(FVector(0.f, 0.f, 130.f));
			Arrow->SetRelativeRotation(FRotator(180.f, 0.f, 0.f));
			Arrow->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.8f));
			Arrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Arrow->SetCastShadow(false);
			Arrow->AttachToComponent(Root, FAttachmentTransformRules::KeepRelativeTransform);
			Arrow->RegisterComponent();
			A->AddInstanceComponent(Arrow);

			UMaterialInstanceDynamic* ArrowMID = UMaterialInstanceDynamic::Create(GlassMaterial, A);
			ArrowMID->SetVectorParameterValue(TEXT("BaseColor"),
				FLinearColor(0.2f, 0.9f, 1.0f));
			ArrowMID->SetScalarParameterValue(TEXT("Opacity"), 0.75f);
			ArrowMID->SetScalarParameterValue(TEXT("Metallic"), 0.2f);
			ArrowMID->SetScalarParameterValue(TEXT("Roughness"), 0.05f);
			ArrowMID->SetVectorParameterValue(TEXT("EmissiveColor"),
				FLinearColor(0.f, 4.f, 5.f));
			Arrow->SetMaterial(0, ArrowMID);
		}

		// Start visible so NPCs appear immediately
		A->SetActorHiddenInGame(false);
		NP.bVisible = true;
		NP.Health = 100.f;
		NP.AnimTime = 0.f;
		NP.WalkAnimBlend = 0.f;
		NP.WanderTarget = Spawns[NI] + FVector(FMath::RandRange(-5000.f, 5000.f), FMath::RandRange(-5000.f, 5000.f), 0.f);
		NP.WanderWaitTimer = FMath::RandRange(1.f, 4.f);
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("Spawned %d NPC player characters."), NumNPCPlayers);
}

// ---------------------------------------------------------------------------
void UPHCCitySubsystem::RespawnNPCPlayer(int32 Index)
{
	if (Index < 0 || Index >= NumNPCPlayers) return;
	FNPCPlayerData& NP = NPCPlayers[Index];
	if (!NP.Actor) return;

	NP.Health = 100.f;
	NP.DamageFlashTimer = 0.f;
	NP.AnimTime = 0.f;
	NP.WalkAnimBlend = 0.f;

	for (UMaterialInstanceDynamic* MID : NP.BodyMIDs)
	{
		if (MID) MID->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
	}

	NP.Actor->SetActorLocation(NP.SpawnPosition);
	NP.Actor->SetActorRotation(FRotator::ZeroRotator);

	NP.Actor->SetActorHiddenInGame(false);
	NP.bVisible = true;

	NP.WanderTarget = NP.SpawnPosition + FVector(FMath::RandRange(-5000.f, 5000.f), FMath::RandRange(-5000.f, 5000.f), 0.f);
	NP.WanderWaitTimer = FMath::RandRange(2.f, 5.f);
}
