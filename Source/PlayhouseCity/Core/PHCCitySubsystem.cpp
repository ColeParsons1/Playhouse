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
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/DirectionalLight.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Engine.h"

// ---------------------------------------------------------------------------
// Centralized city-grid constants (used across multiple Spawn* functions).
// ---------------------------------------------------------------------------
static constexpr int32 NumRoads = 25;
static constexpr float RoadSpacing = 4000.f;
static constexpr float RoadWidth = 900.f;
static constexpr float SidewalkWidth = 250.f;
static constexpr float CityHalfExtent = (NumRoads - 1) * RoadSpacing * 0.5f + 2000.f; // 50000
static constexpr float FirstRoad = -(NumRoads - 1) * RoadSpacing * 0.5f; // -48000

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

#if WITH_EDITORONLY_DATA
	if (!ColorMaterial)
	{
		// First editor run before the .uasset exists -- create inline.
		ColorMaterial = NewObject<UMaterial>(GetTransientPackageAsObject(), TEXT("PHC_ColorMaterial"));

		UMaterialExpressionVectorParameter* ColorParam =
			NewObject<UMaterialExpressionVectorParameter>(ColorMaterial);
		ColorParam->ParameterName = TEXT("BaseColor");
		ColorParam->DefaultValue = FLinearColor(1.f, 1.f, 1.f, 1.f);

		UMaterialEditorOnlyData* EditorData = ColorMaterial->GetEditorOnlyData();
		EditorData->ExpressionCollection.Expressions.Add(ColorParam);
		EditorData->BaseColor.Expression = ColorParam;

		ColorMaterial->PreEditChange(nullptr);
		ColorMaterial->PostEditChange();
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

	DayNightTime += DeltaTime / FullCycleDuration;
	if (DayNightTime >= 1.f) DayNightTime -= 1.f;

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
		MoonLightComp->SetIntensity(MoonElevation * 0.35f);
		const float MoonPitch = -60.f * MoonElevation;
		const float MoonYaw = 90.f - T * 360.f + 180.f;
		MoonLightComp->SetWorldRotation(FRotator(MoonPitch, MoonYaw, 0.f));
	}

	// ---- Sky light (ambient fill) ----
	if (SkyLightComp)
	{
		// Bright during day, dim at night.
		const float DayFactor = FMath::Max(0.f, SunSine);
		SkyLightComp->SetIntensity(FMath::Lerp(0.20f, 1.5f, DayFactor));

		// Tint: blue-white during day, deep blue at night.
		const float NightFactor = FMath::Max(0.f, -SunSine);
		const FLinearColor DayTint(0.6f, 0.7f, 1.0f);
		const FLinearColor NightTint(0.06f, 0.06f, 0.15f);
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

	// ---- Fog (thicker at night) ----
	if (FogComp)
	{
		const float NightFactor = FMath::Max(0.f, -SunSine);
		FogComp->SetFogDensity(FMath::Lerp(0.0008f, 0.0015f, NightFactor));
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
					// Lerp from dark glass to warm glow based on brightness.
					const FLinearColor WinColor = FMath::Lerp(
						DarkGlass, NightWindowLitColors[i], NightWindowBrightness);
					NightWindowMaterials[i]->SetVectorParameterValue(TEXT("BaseColor"), WinColor);
				}
				else
				{
					NightWindowMaterials[i]->SetVectorParameterValue(TEXT("BaseColor"), DarkGlass);
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
}

// ---------------------------------------------------------------------------
// Ground
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnGround()
{
	UWorld* World = GetWorld();
	if (!World || !PlaneMesh) return;

	const FLinearColor GrassColor(0.12f, 0.18f, 0.08f);

	auto SpawnGroundPlane = [&](const FVector& Loc, const FVector& Scale, const TCHAR* Label)
	{
		AActor* GA = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!GA) return;
#if WITH_EDITOR
		GA->SetActorLabel(Label);
#endif
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(GA, TEXT("GroundMesh"));
		MC->SetStaticMesh(PlaneMesh);
		MC->SetWorldScale3D(Scale);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MC->SetCollisionResponseToAllChannels(ECR_Block);
		MC->RegisterComponent();
		GA->SetRootComponent(MC);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
		if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), GrassColor); MC->SetMaterial(0, Mat); }
		GA->AddInstanceComponent(MC);
	};

	// Main city ground: covers the expanded 24x24 grid + margin
	SpawnGroundPlane(FVector(0.f, 0.f, 0.f), FVector(1400.f, 1400.f, 1.f), TEXT("CityGround"));

	// Northwest fill: extends ground beyond north edge
	SpawnGroundPlane(FVector(-30000.f, 75000.f, 0.f), FVector(700.f, 200.f, 1.f), TEXT("NorthwestFill"));

	// Outskirts ground: west of city extending along highway
	SpawnGroundPlane(FVector(-130000.f, 0.f, 0.f), FVector(2000.f, 1400.f, 1.f), TEXT("OutskirtsMain"));

	// Far outskirts: very far west
	SpawnGroundPlane(FVector(-280000.f, 0.f, 0.f), FVector(1000.f, 1000.f, 1.f), TEXT("OutskirtsFar"));
}

// ---------------------------------------------------------------------------
// Ocean and Beach
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnOceanAndBeach()
{
	UWorld* World = GetWorld();
	if (!World || !PlaneMesh) return;

	auto SpawnWater = [&](const FVector& Loc, const FVector& Scale, const FLinearColor& Color, const TCHAR* Label)
	{
		AActor* WA = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!WA) return;
#if WITH_EDITOR
		WA->SetActorLabel(Label);
#endif
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(WA, TEXT("WaterMesh"));
		MC->SetStaticMesh(PlaneMesh);
		MC->SetWorldScale3D(Scale);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MC->SetCollisionResponseToAllChannels(ECR_Block);
		MC->RegisterComponent();
		WA->SetRootComponent(MC);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
		if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), Color); MC->SetMaterial(0, Mat); }
		WA->AddInstanceComponent(MC);
	};

	const FLinearColor DeepOcean(0.02f, 0.06f, 0.14f);
	const FLinearColor ShallowWater(0.04f, 0.12f, 0.22f);
	const FLinearColor Sand(0.76f, 0.70f, 0.55f);

	// Global ocean floor — massive, covers everything below land
	SpawnWater(FVector(0.f, 0.f, -8.f), FVector(4000.f, 4000.f, 1.f), DeepOcean, TEXT("OceanFloor"));

	// Bay inlet (NE corner) — shallow water sitting above ocean
	SpawnWater(FVector(45000.f, 60000.f, -3.f), FVector(250.f, 150.f, 1.f), ShallowWater, TEXT("BayInlet"));

	// ---- Beach strips (sandy shores around the peninsula) ----

	// East coast beach — long strip running N-S
	SpawnWater(FVector(67000.f, 0.f, -1.f), FVector(20.f, 1400.f, 1.f), Sand, TEXT("EastBeach"));

	// South coast beach — long strip running E-W
	SpawnWater(FVector(0.f, -67000.f, -1.f), FVector(1400.f, 20.f, 1.f), Sand, TEXT("SouthBeach"));

	// North coast beach (west of bay)
	SpawnWater(FVector(-20000.f, 64000.f, -1.f), FVector(500.f, 20.f, 1.f), Sand, TEXT("NorthBeachWest"));

	// Bay south shoreline
	SpawnWater(FVector(45000.f, 52000.f, -1.f), FVector(250.f, 14.f, 1.f), Sand, TEXT("BaySouthShore"));

	// Bay west shoreline
	SpawnWater(FVector(32000.f, 60000.f, -1.f), FVector(14.f, 100.f, 1.f), Sand, TEXT("BayWestShore"));

	// Southeast corner beach
	SpawnWater(FVector(65000.f, -65000.f, -1.f), FVector(30.f, 30.f, 1.f), Sand, TEXT("SoutheastBeach"));

	// Waterfront boardwalk beach — along NE coast within waterfront district
	SpawnWater(FVector(55000.f, 55000.f, -1.f), FVector(100.f, 80.f, 1.f), Sand, TEXT("WaterfrontBeach"));
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
			FogComp->SetVolumetricFog(false);
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
		{ 3,  14, 1200, 900, 800, FLinearColor(0.95f, 0.95f, 0.95f), FLinearColor(0.8f, 0.1f, 0.1f)   },  // Hospital
		{ 3,  20, 800,  700, 600, FLinearColor(0.3f, 0.35f, 0.55f),  FLinearColor(0.85f, 0.75f, 0.2f)  },  // Police
		{ 20,  3, 700,  600, 500, FLinearColor(0.8f, 0.2f, 0.15f),   FLinearColor(0.85f, 0.85f, 0.2f)  },  // Fire
		{ 15, 12, 1100, 800, 600, FLinearColor(0.75f, 0.6f, 0.4f),   FLinearColor(0.4f, 0.5f, 0.3f)    },  // School
		{ 6,  13, 600,  500, 400, FLinearColor(0.6f, 0.55f, 0.45f),  FLinearColor(0.3f, 0.6f, 0.25f)   },  // Grocery
		{ 8,   9, 400,  350, 350, FLinearColor(0.45f, 0.3f, 0.2f),   FLinearColor(0.9f, 0.85f, 0.7f)   },  // Coffee
		{ 20, 20, 900,  700, 500, FLinearColor(0.9f, 0.9f, 0.88f),   FLinearColor(0.2f, 0.4f, 0.7f)    },  // Marina
		{ 2,   3, 1000, 800, 700, FLinearColor(0.5f, 0.5f, 0.48f),   FLinearColor(0.6f, 0.4f, 0.15f)   },  // Factory
		{ 16,  6, 1400, 1200,500, FLinearColor(0.6f, 0.6f, 0.58f),   FLinearColor(0.15f, 0.35f, 0.6f)  },  // Stadium
		{ 1,  12, 500,  400, 350, FLinearColor(0.85f, 0.82f, 0.78f), FLinearColor(0.7f, 0.15f, 0.1f)   },  // Gas Station
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
	for (int32 i = 0; i < NumRoads; ++i)
	{
		const float RoadPos = FirstRoad + i * RoadSpacing;

		// X-direction road (runs east-west at Y = RoadPos)
		SpawnRoadStrip(FVector(0.f, RoadPos, 2.f), CityHalfExtent, RoadWidth * 0.5f);
		SpawnSidewalk(FVector(0.f, RoadPos + RoadWidth * 0.5f + SidewalkWidth * 0.5f, 1.5f), CityHalfExtent, SidewalkWidth * 0.5f);
		SpawnSidewalk(FVector(0.f, RoadPos - RoadWidth * 0.5f - SidewalkWidth * 0.5f, 1.5f), CityHalfExtent, SidewalkWidth * 0.5f);

		// Y-direction road (runs north-south at X = RoadPos)
		SpawnRoadStrip(FVector(RoadPos, 0.f, 2.f), RoadWidth * 0.5f, CityHalfExtent);
		SpawnSidewalk(FVector(RoadPos + RoadWidth * 0.5f + SidewalkWidth * 0.5f, 0.f, 1.5f), SidewalkWidth * 0.5f, CityHalfExtent);
		SpawnSidewalk(FVector(RoadPos - RoadWidth * 0.5f - SidewalkWidth * 0.5f, 0.f, 1.5f), SidewalkWidth * 0.5f, CityHalfExtent);
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
		FLinearColor(0.70f, 0.82f, 0.92f),  // pastel blue
		FLinearColor(0.65f, 0.85f, 0.68f),  // pastel green
		FLinearColor(0.95f, 0.92f, 0.60f),  // pastel yellow
		FLinearColor(0.95f, 0.65f, 0.55f),  // coral
		FLinearColor(0.75f, 0.65f, 0.85f),  // lavender
		FLinearColor(0.96f, 0.94f, 0.88f),  // cream
		FLinearColor(0.55f, 0.68f, 0.52f),  // sage green
		FLinearColor(0.85f, 0.62f, 0.65f),  // dusty rose
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
	const float BlockW = RoadSpacing - RoadWidth - 2.f * SidewalkWidth; // 2600

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

			// ------- Landmark check -------
			const int32 BlockKey = by * NumBlocks + bx;
			if (LandmarkKeys.Contains(BlockKey))
			{
				for (const FLandmark& L : Landmarks)
				{
					if (L.bx == bx && L.by == by)
					{
						SpawnInstitution(
							FVector(BlockCenterX, BlockCenterY, 0.f),
							L.W, L.D, L.H,
							L.Wall, L.Accent);
						break;
					}
				}
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
					const float BldgW = CityRandom.FRandRange(300.f, 600.f);
					const float BldgD = CityRandom.FRandRange(300.f, 600.f);
					// Neat 2x2 grid placement
					const int32 Col = n % 2;
					const int32 Row = n / 2;
					const float OffX = BlockW * (0.25f + 0.5f * Col);
					const float OffY = BlockH * (0.25f + 0.5f * Row);
					const FVector Loc(BlockMinX + OffX, BlockMinY + OffY, 0.f);
					const FLinearColor& Color = DowntownColors[CityRandom.RandRange(0, NumDowntownColors - 1)];

					const float Roll = CityRandom.FRandRange(0.f, 1.f);
					if (Roll < 0.70f)
					{
						const float Height = CityRandom.FRandRange(2000.f, 5000.f);
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
			// MIDTOWN — 1-3 buildings per block
			// =======================================================
			case ECityDistrict::Midtown:
			{
				const int32 Count = CityRandom.RandRange(1, 3);
				for (int32 n = 0; n < Count; ++n)
				{
					if (CityRandom.FRand() > 0.40f) continue;
					const float BldgW = CityRandom.FRandRange(300.f, 550.f);
					const float BldgD = CityRandom.FRandRange(300.f, 550.f);
					// Neat grid placement
					const int32 Col = n % 2;
					const int32 Row = n / 2;
					const float OffX = BlockW * (0.25f + 0.5f * Col);
					const float OffY = BlockH * (0.25f + 0.5f * Row);
					const FVector Loc(BlockMinX + OffX, BlockMinY + OffY, 0.f);
					const FLinearColor& WallColor = MidtownColors[CityRandom.RandRange(0, NumMidtownColors - 1)];

					const float Roll = CityRandom.FRandRange(0.f, 1.f);
					if (Roll < 0.60f)
					{
						const int32 Stories = CityRandom.RandRange(2, 3);
						SpawnShop(Loc, BldgW, BldgD, Stories, WallColor,
							MidtownColors[CityRandom.RandRange(0, NumMidtownColors - 1)]);
					}
					else if (Roll < 0.90f)
					{
						const float Height = CityRandom.FRandRange(800.f, 2000.f);
						SpawnBuilding(Loc, FVector(BldgW, BldgD, Height), WallColor, true);
					}
					else
					{
						const int32 Stories = CityRandom.RandRange(1, 2);
						SpawnHouse(Loc, BldgW, BldgD, Stories, WallColor,
							MidtownColors[CityRandom.RandRange(0, NumMidtownColors - 1)]);
					}
				}
				// Warm midtown ground light
				{
					AActor* MtActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
						FVector(BlockCenterX, BlockCenterY, 200.f), FRotator::ZeroRotator);
					if (MtActor)
					{
						UPointLightComponent* MtLight = NewObject<UPointLightComponent>(MtActor, TEXT("MtLight"));
						MtLight->SetWorldLocation(FVector(BlockCenterX, BlockCenterY, 200.f));
						MtLight->SetIntensity(3000.f);
						MtLight->SetAttenuationRadius(1800.f);
						MtLight->SetLightColor(FColor(255, 220, 160));
						MtLight->SetCastShadows(false);
						MtLight->RegisterComponent();
						MtActor->SetRootComponent(MtLight);
						MtActor->AddInstanceComponent(MtLight);
					}
				}
				break;
			}

			// =======================================================
			// RESIDENTIAL — 1-3 buildings (2 for outer ring)
			// =======================================================
			case ECityDistrict::Residential:
			{
				// Structured 2x2 neighborhood grid — houses face roads
				const bool bOuterRing = (bx <= 2 || bx >= 21 || by <= 2 || by >= 21);
				const int32 HouseCount = bOuterRing ? 3 : 4;

				// Grid positions: 2 columns x 2 rows within the block
				const float FrontSetback = 200.f;  // distance from block edge to house front
				const float SideMargin = 150.f;     // distance from block edge to house side
				const float HalfBlock = BlockW * 0.5f;

				// Row Y positions (north row and south row)
				const float NorthRowY = BlockMinY + FrontSetback;
				const float SouthRowY = BlockMinY + BlockH - FrontSetback;
				// Column X positions (left and right)
				const float LeftColX = BlockMinX + SideMargin + 250.f;
				const float RightColX = BlockMinX + BlockW - SideMargin - 250.f;

				struct FHouseSlot { float X; float Y; bool bFacesSouth; };
				const FHouseSlot Slots[] = {
					{ LeftColX,  NorthRowY + 250.f, false },  // NW — faces north road
					{ RightColX, NorthRowY + 250.f, false },  // NE — faces north road
					{ LeftColX,  SouthRowY - 250.f, true  },  // SW — faces south road
					{ RightColX, SouthRowY - 250.f, true  },  // SE — faces south road
				};

				for (int32 n = 0; n < HouseCount; ++n)
				{
					// 80% chance to skip each slot — reduces total houses by ~80%
					if (CityRandom.FRand() > 0.20f) continue;

					const FHouseSlot& Slot = Slots[n];
					// Small random jitter for natural feel
					const float JitterX = CityRandom.FRandRange(-50.f, 50.f);
					const float JitterY = CityRandom.FRandRange(-30.f, 30.f);

					// Varied house sizes
					const float BldgW = CityRandom.FRandRange(350.f, 700.f);
					const float BldgD = CityRandom.FRandRange(350.f, 650.f);
					const FVector Loc(Slot.X + JitterX, Slot.Y + JitterY, 0.f);
					const FLinearColor& WallColor = ResidentialColors[CityRandom.RandRange(0, NumResidentialColors - 1)];
					const FLinearColor& RoofColor = RoofColors[CityRandom.RandRange(0, NumRoofColors - 1)];

					// 85% houses, 15% corner shops
					const float Roll = CityRandom.FRandRange(0.f, 1.f);
					if (Roll < 0.85f)
					{
						const int32 Stories = CityRandom.RandRange(1, 2); // genuine 1 or 2 story
						SpawnHouse(Loc, BldgW, BldgD, Stories, WallColor, RoofColor);
					}
					else
					{
						SpawnShop(Loc, BldgW, BldgD, 1, WallColor, RoofColor);
					}

					// Driveway: small dark-gray slab from house front to block edge
					{
						UWorld* DriveWorld = GetWorld();
						if (DriveWorld && CubeMesh)
						{
							const float DriveW = 80.f;
							const float DriveThick = 6.f;
							float DriveLen, DriveY;
							if (Slot.bFacesSouth)
							{
								DriveLen = (BlockMinY + BlockH) - (Slot.Y + JitterY + BldgD * 0.5f);
								DriveY = Slot.Y + JitterY + BldgD * 0.5f + DriveLen * 0.5f;
							}
							else
							{
								DriveLen = (Slot.Y + JitterY - BldgD * 0.5f) - BlockMinY;
								DriveY = BlockMinY + DriveLen * 0.5f;
							}

							if (DriveLen > 10.f)
							{
								AActor* DriveAct = DriveWorld->SpawnActor<AActor>(
									AActor::StaticClass(),
									FVector(Slot.X + JitterX, DriveY, DriveThick * 0.5f),
									FRotator::ZeroRotator);
								if (DriveAct)
								{
									UStaticMeshComponent* DriveMC = NewObject<UStaticMeshComponent>(
										DriveAct, TEXT("Driveway"));
									DriveMC->SetStaticMesh(CubeMesh);
									DriveMC->SetWorldScale3D(FVector(
										DriveW / 100.f, DriveLen / 100.f, DriveThick / 100.f));
									DriveMC->SetWorldLocation(FVector(
										Slot.X + JitterX, DriveY, DriveThick * 0.5f));
									DriveMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
									DriveMC->RegisterComponent();
									UMaterialInstanceDynamic* DMat = UMaterialInstanceDynamic::Create(
										DriveMC->GetMaterial(0), DriveMC);
									if (DMat)
									{
										DMat->SetVectorParameterValue(TEXT("BaseColor"),
											FLinearColor(0.25f, 0.25f, 0.25f));
										DriveMC->SetMaterial(0, DMat);
									}
									DriveAct->SetRootComponent(DriveMC);
									DriveAct->AddInstanceComponent(DriveMC);
								}
							}
						}
					}
				}
				break;
			}

			// =======================================================
			// ARTS DISTRICT — Wynwood-style colorful buildings + murals
			// =======================================================
			case ECityDistrict::ArtsDistrict:
			{
				// 1-2 colorful low-rise buildings per block
				const int32 Count = CityRandom.RandRange(1, 2);
				for (int32 n = 0; n < Count; ++n)
				{
					const float BldgW = CityRandom.FRandRange(400.f, 700.f);
					const float BldgD = CityRandom.FRandRange(350.f, 600.f);
					// Neat grid placement
					const int32 Col = n % 2;
					const int32 Row = n / 2;
					const float OffX = BlockW * (0.25f + 0.5f * Col);
					const float OffY = BlockH * (0.25f + 0.5f * Row);
					const FVector Loc(BlockMinX + OffX, BlockMinY + OffY, 0.f);
					const FLinearColor& WallColor = ArtsColors[CityRandom.RandRange(0, NumArtsColors - 1)];
					const FLinearColor& AwningColor = ArtsColors[CityRandom.RandRange(0, NumArtsColors - 1)];

					const int32 Stories = CityRandom.RandRange(1, 2);
					SpawnShop(Loc, BldgW, BldgD, Stories, WallColor, AwningColor);

					// Mural wall panel on one side of the building
					if (CubeMesh)
					{
						const FLinearColor& MuralColor1 = ArtsColors[CityRandom.RandRange(0, NumArtsColors - 1)];
						const FLinearColor& MuralColor2 = ArtsColors[CityRandom.RandRange(0, NumArtsColors - 1)];
						const float MuralH = Stories * 280.f * 0.8f;
						const float MuralW = BldgW * 0.7f;

						// Main mural panel (on +Y face)
						AActor* MuralAct = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
							FVector(Loc.X, Loc.Y + BldgD * 0.5f + 5.f, MuralH * 0.5f), FRotator::ZeroRotator);
						if (MuralAct)
						{
							UStaticMeshComponent* MP = NewObject<UStaticMeshComponent>(MuralAct, TEXT("Mural"));
							MP->SetStaticMesh(CubeMesh);
							MP->SetWorldScale3D(FVector(MuralW / 100.f, 3.f / 100.f, MuralH / 100.f));
							MP->SetWorldLocation(FVector(Loc.X, Loc.Y + BldgD * 0.5f + 5.f, MuralH * 0.5f));
							MP->SetCollisionEnabled(ECollisionEnabled::NoCollision);
							MP->RegisterComponent();
							UMaterialInstanceDynamic* MMat = UMaterialInstanceDynamic::Create(MP->GetMaterial(0), MP);
							if (MMat) { MMat->SetVectorParameterValue(TEXT("BaseColor"), MuralColor1); MP->SetMaterial(0, MMat); }
							MuralAct->SetRootComponent(MP);
							MuralAct->AddInstanceComponent(MP);

							// Second color stripe on the mural
							UStaticMeshComponent* Stripe = NewObject<UStaticMeshComponent>(MuralAct, TEXT("MuralStripe"));
							Stripe->SetStaticMesh(CubeMesh);
							Stripe->SetWorldScale3D(FVector(MuralW * 0.4f / 100.f, 4.f / 100.f, MuralH * 0.5f / 100.f));
							Stripe->SetWorldLocation(FVector(Loc.X + MuralW * 0.2f, Loc.Y + BldgD * 0.5f + 7.f, MuralH * 0.65f));
							Stripe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
							Stripe->RegisterComponent();
							UMaterialInstanceDynamic* SMat = UMaterialInstanceDynamic::Create(Stripe->GetMaterial(0), Stripe);
							if (SMat) { SMat->SetVectorParameterValue(TEXT("BaseColor"), MuralColor2); Stripe->SetMaterial(0, SMat); }
							MuralAct->AddInstanceComponent(Stripe);
						}
					}
				}

				// Freestanding street art wall (50% chance per block)
				if (CityRandom.FRand() < 0.50f && CubeMesh)
				{
					const float WallW = CityRandom.FRandRange(300.f, 600.f);
					const float WallH = CityRandom.FRandRange(200.f, 400.f);
					const float WallX = CityRandom.FRandRange(BlockMinX + 200.f, BlockMinX + BlockW - 200.f);
					const float WallY = CityRandom.FRandRange(BlockMinY + 200.f, BlockMinY + BlockH - 200.f);

					AActor* WallAct = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
						FVector(WallX, WallY, WallH * 0.5f), FRotator::ZeroRotator);
					if (WallAct)
					{
						UStaticMeshComponent* WC = NewObject<UStaticMeshComponent>(WallAct, TEXT("ArtWall"));
						WC->SetStaticMesh(CubeMesh);
						WC->SetWorldScale3D(FVector(WallW / 100.f, 8.f / 100.f, WallH / 100.f));
						WC->SetWorldLocation(FVector(WallX, WallY, WallH * 0.5f));
						WC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
						WC->SetCollisionResponseToAllChannels(ECR_Block);
						WC->RegisterComponent();
						const FLinearColor& ArtColor = ArtsColors[CityRandom.RandRange(0, NumArtsColors - 1)];
						UMaterialInstanceDynamic* WMat = UMaterialInstanceDynamic::Create(WC->GetMaterial(0), WC);
						if (WMat) { WMat->SetVectorParameterValue(TEXT("BaseColor"), ArtColor); WC->SetMaterial(0, WMat); }
						WallAct->SetRootComponent(WC);
						WallAct->AddInstanceComponent(WC);

						// Colorful accent stripe on the art wall
						const FLinearColor& Stripe2Color = ArtsColors[CityRandom.RandRange(0, NumArtsColors - 1)];
						UStaticMeshComponent* WS = NewObject<UStaticMeshComponent>(WallAct, TEXT("ArtWallStripe"));
						WS->SetStaticMesh(CubeMesh);
						WS->SetWorldScale3D(FVector(WallW * 0.6f / 100.f, 9.f / 100.f, WallH * 0.3f / 100.f));
						WS->SetWorldLocation(FVector(WallX - WallW * 0.1f, WallY, WallH * 0.7f));
						WS->SetCollisionEnabled(ECollisionEnabled::NoCollision);
						WS->RegisterComponent();
						UMaterialInstanceDynamic* WSMat = UMaterialInstanceDynamic::Create(WS->GetMaterial(0), WS);
						if (WSMat) { WSMat->SetVectorParameterValue(TEXT("BaseColor"), Stripe2Color); WS->SetMaterial(0, WSMat); }
						WallAct->AddInstanceComponent(WS);
					}
				}

				// Warm colored ground light for the arts district
				{
					AActor* AdLight = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
						FVector(BlockCenterX, BlockCenterY, 200.f), FRotator::ZeroRotator);
					if (AdLight)
					{
						UPointLightComponent* PL = NewObject<UPointLightComponent>(AdLight, TEXT("ArtsLight"));
						PL->SetWorldLocation(FVector(BlockCenterX, BlockCenterY, 200.f));
						PL->SetIntensity(4000.f);
						PL->SetAttenuationRadius(2000.f);
						PL->SetLightColor(FColor(255, 180, 200)); // warm pink
						PL->SetCastShadows(false);
						PL->RegisterComponent();
						AdLight->SetRootComponent(PL);
						AdLight->AddInstanceComponent(PL);
					}
				}
				break;
			}

			// =======================================================
			// WATERFRONT — 1-2 low buildings per block
			// =======================================================
			case ECityDistrict::Waterfront:
			{
				const int32 Count = CityRandom.RandRange(1, 2);
				for (int32 n = 0; n < Count; ++n)
				{
					if (CityRandom.FRand() > 0.40f) continue;
					const float BldgW = CityRandom.FRandRange(400.f, 600.f);
					const float BldgD = CityRandom.FRandRange(400.f, 600.f);
					// Neat grid placement
					const int32 Col = n % 2;
					const int32 Row = n / 2;
					const float OffX = BlockW * (0.25f + 0.5f * Col);
					const float OffY = BlockH * (0.25f + 0.5f * Row);
					const FVector Loc(BlockMinX + OffX, BlockMinY + OffY, 0.f);
					const FLinearColor& WallColor = WaterfrontWallColors[CityRandom.RandRange(0, NumWaterfrontWallColors - 1)];
					const FLinearColor& AwningColor = WaterfrontAwningColors[CityRandom.RandRange(0, NumWaterfrontAwningColors - 1)];

					const int32 Stories = CityRandom.RandRange(1, 2);
					SpawnShop(Loc, BldgW, BldgD, Stories, WallColor, AwningColor);
				}
				break;
			}

			// =======================================================
			// INDUSTRIAL — 1-2 warehouses per block
			// =======================================================
			case ECityDistrict::Industrial:
			{
				const int32 Count = CityRandom.RandRange(1, 2);
				for (int32 n = 0; n < Count; ++n)
				{
					if (CityRandom.FRand() > 0.40f) continue;
					const float W = CityRandom.FRandRange(800.f, 1400.f);
					const float D = CityRandom.FRandRange(600.f, 1000.f);
					const float H = CityRandom.FRandRange(300.f, 600.f);
					// Neat grid placement
					const int32 Col = n % 2;
					const int32 Row = n / 2;
					const float OffX = BlockW * (0.25f + 0.5f * Col);
					const float OffY = BlockH * (0.25f + 0.5f * Row);
					const FVector Loc(BlockMinX + OffX, BlockMinY + OffY, 0.f);
					const FLinearColor& Color = IndustrialColors[CityRandom.RandRange(0, NumIndustrialColors - 1)];
					SpawnWarehouse(Loc, W, D, H, Color);
				}
				break;
			}

			// =======================================================
			// EDGE — 0-1 buildings per block (50% chance)
			// =======================================================
			case ECityDistrict::Edge:
			{
				if (CityRandom.FRandRange(0.f, 1.f) < 0.10f)
				{
					const float BldgW = CityRandom.FRandRange(400.f, 600.f);
					const float BldgD = CityRandom.FRandRange(400.f, 600.f);
					// Center the single building in the block
					const float OffX = BlockW * 0.5f;
					const float OffY = BlockH * 0.5f;
					const FVector Loc(BlockMinX + OffX, BlockMinY + OffY, 0.f);
					const FLinearColor Color = ResidentialColors[CityRandom.RandRange(0, NumResidentialColors - 1)];

					if (CityRandom.FRandRange(0.f, 1.f) < 0.50f)
					{
						const float Height = CityRandom.FRandRange(800.f, 1200.f);
						SpawnBuilding(Loc, FVector(BldgW, BldgD, Height), Color, false);
					}
					else
					{
						SpawnHouse(Loc, BldgW, BldgD, 2, Color,
							ResidentialColors[CityRandom.RandRange(0, NumResidentialColors - 1)]);
					}
				}
				break;
			}

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
	const float BlockW = RoadSpacing - RoadWidth - 2.f * SidewalkWidth; // 2600
	const float HalfSpan = BlockW * 0.5f; // 1800
	const float InteriorHalfW = 150.f; // 300 UU wide streets

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
	// Every 3rd avenue, both sides
	for (int32 i = 0; i < NumRoads; i += 3)
	{
		const float RoadX = FirstRoad + i * RoadSpacing;
		const float SidewalkOff = RoadWidth * 0.5f + SidewalkWidth * 0.5f;

		// Trees along the length of this avenue
		float Y = FirstRoad + 500.f;
		while (Y < -FirstRoad - 500.f)
		{
			// Determine if this is inner city (downtown/midtown) or outer
			const bool bInner = (i >= 5 && i <= 11);
			const float SkipChance = bInner ? 0.55f : 0.70f;

			if (CityRandom.FRand() > SkipChance)
			{
				const float H = CityRandom.FRandRange(600.f, 1200.f);
				SpawnPalmTree(FVector(RoadX, Y, 0.f), H, bInner);
			}
			// Trees on the other side of the road too
			if (CityRandom.FRand() > SkipChance)
			{
				const float H = CityRandom.FRandRange(600.f, 1200.f);
				SpawnPalmTree(FVector(RoadX + SidewalkOff * 2.f, Y, 0.f), H, bInner);
			}

			Y += CityRandom.FRandRange(1500.f, 3000.f);
		}
	}

	// ---- East coast beach palms ----
	{
		float Y = -70000.f;
		while (Y < 70000.f)
		{
			const float TX = 68000.f + CityRandom.FRandRange(-200.f, 400.f);
			const float H = CityRandom.FRandRange(700.f, 1400.f);
			SpawnPalmTree(FVector(TX, Y, 0.f), H, false);
			Y += CityRandom.FRandRange(500.f, 1200.f);
		}
	}

	// ---- South coast beach palms ----
	{
		float X = -65000.f;
		while (X < 65000.f)
		{
			const float TY = -68000.f + CityRandom.FRandRange(-200.f, 400.f);
			const float H = CityRandom.FRandRange(600.f, 1300.f);
			SpawnPalmTree(FVector(X, TY, 0.f), H, false);
			X += CityRandom.FRandRange(600.f, 1400.f);
		}
	}

	// ---- North coast beach palms (west of bay) ----
	{
		float X = -65000.f;
		while (X < 0.f)
		{
			const float TY = 66000.f + CityRandom.FRandRange(-200.f, 400.f);
			const float H = CityRandom.FRandRange(600.f, 1200.f);
			SpawnPalmTree(FVector(X, TY, 0.f), H, false);
			X += CityRandom.FRandRange(700.f, 1500.f);
		}
	}

	// ---- Bay shoreline palms ----
	{
		// South edge of bay
		float X = 32000.f;
		while (X < 55000.f)
		{
			const float TY = 53000.f + CityRandom.FRandRange(-300.f, 300.f);
			const float H = CityRandom.FRandRange(500.f, 1100.f);
			SpawnPalmTree(FVector(X, TY, 0.f), H, false);
			X += CityRandom.FRandRange(600.f, 1200.f);
		}
		// West edge of bay
		float Y2 = 53000.f;
		while (Y2 < 68000.f)
		{
			const float TX = 33000.f + CityRandom.FRandRange(-300.f, 300.f);
			const float H = CityRandom.FRandRange(500.f, 1000.f);
			SpawnPalmTree(FVector(TX, Y2, 0.f), H, false);
			Y2 += CityRandom.FRandRange(600.f, 1200.f);
		}
	}

	// ---- Waterfront district bonus palms ----
	// Scatter extra palms in the waterfront area (bx >= 18, by >= 17 in block coords)
	{
		const float WFMinX = FirstRoad + 18 * RoadSpacing;
		const float WFMinY = FirstRoad + 17 * RoadSpacing;
		const float WFMaxX = FirstRoad + 22 * RoadSpacing;
		const float WFMaxY = FirstRoad + 21 * RoadSpacing;

		for (int32 t = 0; t < 40; ++t)
		{
			const float TX = CityRandom.FRandRange(WFMinX, WFMaxX);
			const float TY = CityRandom.FRandRange(WFMinY, WFMaxY);
			const float H = CityRandom.FRandRange(600.f, 1100.f);
			SpawnPalmTree(FVector(TX, TY, 0.f), H, false);
		}
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

		// Arrow indicator pointing away from origin
		FVector ArrowDir = FVector(Loc.X, Loc.Y, 0.f);
		ArrowDir.Normalize();
		float ArrowYaw = FMath::Atan2(ArrowDir.Y, ArrowDir.X) * (180.f / PI);

		FVector ArrowLoc = Loc + ArrowDir * 300.f;
		ArrowLoc.Z = Loc.Z + 130.f;

		UStaticMeshComponent* ArrowMC = NewObject<UStaticMeshComponent>(RampActor, *FString::Printf(TEXT("Arrow_%d"), i));
		ArrowMC->SetStaticMesh(CubeMesh);
		ArrowMC->SetWorldLocation(ArrowLoc);
		ArrowMC->SetWorldScale3D(FVector(3.0f, 0.4f, 0.15f));
		ArrowMC->SetWorldRotation(FRotator(0.f, ArrowYaw, 0.f));
		ArrowMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ArrowMC->RegisterComponent();
		ApplyColor(ArrowMC, ArrowColor);
		RampActor->AddInstanceComponent(ArrowMC);
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
	if (BodyMat) { BodyMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor); Body->SetMaterial(0, BodyMat); }
	House->AddInstanceComponent(Body);

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
		if (RMat) { RMat->SetVectorParameterValue(TEXT("BaseColor"), RoofColor); Roof->SetMaterial(0, RMat); }
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
	if (RidgeMat) { RidgeMat->SetVectorParameterValue(TEXT("BaseColor"), RoofColor * 0.8f); Ridge->SetMaterial(0, RidgeMat); }
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
		if (DMat) { DMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.18f, 0.12f, 0.06f)); Door->SetMaterial(0, DMat); }
		House->AddInstanceComponent(Door);
	}

	// ---- Lawn (green plane slightly larger than footprint) ----
	{
		UStaticMeshComponent* Lawn = NewObject<UStaticMeshComponent>(House, TEXT("Lawn"));
		Lawn->SetStaticMesh(PlaneMesh);
		Lawn->SetWorldScale3D(FVector((W + 200.f) / 100.f, (D + 200.f) / 100.f, 1.f));
		Lawn->SetWorldLocation(FVector(Loc.X, Loc.Y, 4.f));
		Lawn->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Lawn->SetCollisionResponseToAllChannels(ECR_Block);
		Lawn->RegisterComponent();
		UMaterialInstanceDynamic* LMat = UMaterialInstanceDynamic::Create(Lawn->GetMaterial(0), Lawn);
		if (LMat) { LMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.15f, 0.35f, 0.08f)); Lawn->SetMaterial(0, LMat); }
		House->AddInstanceComponent(Lawn);
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
		if (GMat) { GMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor); Garage->SetMaterial(0, GMat); }
		House->AddInstanceComponent(Garage);

		// Garage door face.
		UStaticMeshComponent* GDoor = NewObject<UStaticMeshComponent>(House, TEXT("GarageDoor"));
		GDoor->SetStaticMesh(CubeMesh);
		GDoor->SetWorldScale3D(FVector(GW * 0.8f / 100.f, 3.f / 100.f, GH * 0.7f / 100.f));
		GDoor->SetWorldLocation(FVector(GX, Loc.Y + GD * 0.5f + 3.f, GH * 0.35f));
		GDoor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		GDoor->RegisterComponent();
		UMaterialInstanceDynamic* GDMat = UMaterialInstanceDynamic::Create(GDoor->GetMaterial(0), GDoor);
		if (GDMat) { GDMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.22f, 0.20f, 0.18f)); GDoor->SetMaterial(0, GDMat); }
		House->AddInstanceComponent(GDoor);
	}

	// ---- Chimney (35% chance) ----
	if (DetailRandom.FRand() < 0.35f)
	{
		const float ChW = 35.f;
		const float ChD = 30.f;
		const float ChH = 120.f;
		const float ChX = Loc.X + W * 0.25f;
		const float ChZ = BodyH + D * 0.5f * FMath::Tan(FMath::DegreesToRadians(30.f)) * 0.5f;
		UStaticMeshComponent* Chimney = NewObject<UStaticMeshComponent>(House, TEXT("Chimney"));
		Chimney->SetStaticMesh(CubeMesh);
		Chimney->SetWorldScale3D(FVector(ChW / 100.f, ChD / 100.f, ChH / 100.f));
		Chimney->SetWorldLocation(FVector(ChX, Loc.Y - D * 0.2f, ChZ + ChH * 0.5f));
		Chimney->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Chimney->RegisterComponent();
		UMaterialInstanceDynamic* ChMat = UMaterialInstanceDynamic::Create(Chimney->GetMaterial(0), Chimney);
		if (ChMat) { ChMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.45f, 0.25f, 0.18f)); Chimney->SetMaterial(0, ChMat); }
		House->AddInstanceComponent(Chimney);

		// Chimney cap
		UStaticMeshComponent* Cap = NewObject<UStaticMeshComponent>(House, TEXT("ChimneyCap"));
		Cap->SetStaticMesh(CubeMesh);
		Cap->SetWorldScale3D(FVector((ChW + 10.f) / 100.f, (ChD + 10.f) / 100.f, 0.08f));
		Cap->SetWorldLocation(FVector(ChX, Loc.Y - D * 0.2f, ChZ + ChH + 4.f));
		Cap->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Cap->RegisterComponent();
		UMaterialInstanceDynamic* CapMat = UMaterialInstanceDynamic::Create(Cap->GetMaterial(0), Cap);
		if (CapMat) { CapMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.3f, 0.3f, 0.28f)); Cap->SetMaterial(0, CapMat); }
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
		if (PchMat) { PchMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.40f, 0.30f, 0.20f)); Porch->SetMaterial(0, PchMat); }
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
				if (PostMat) { PostMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.85f, 0.85f, 0.82f)); Post->SetMaterial(0, PostMat); }
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
		if (PRMat) { PRMat->SetVectorParameterValue(TEXT("BaseColor"), RoofColor); PRoof->SetMaterial(0, PRMat); }
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
	if (BodyMat) { BodyMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor); Body->SetMaterial(0, BodyMat); }
	Shop->AddInstanceComponent(Body);

	// ---- Roof parapet (thin strip along top) ----
	{
		UStaticMeshComponent* Parapet = NewObject<UStaticMeshComponent>(Shop, TEXT("Parapet"));
		Parapet->SetStaticMesh(CubeMesh);
		Parapet->SetWorldScale3D(FVector((W + 10.f) / 100.f, (D + 10.f) / 100.f, 0.25f));
		Parapet->SetWorldLocation(FVector(Loc.X, Loc.Y, BodyH + 12.f));
		Parapet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Parapet->RegisterComponent();
		UMaterialInstanceDynamic* PMat = UMaterialInstanceDynamic::Create(Parapet->GetMaterial(0), Parapet);
		if (PMat) { PMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor * 0.9f); Parapet->SetMaterial(0, PMat); }
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
		if (AMat) { AMat->SetVectorParameterValue(TEXT("BaseColor"), AwningColor); Awning->SetMaterial(0, AMat); }
		Shop->AddInstanceComponent(Awning);
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
		if (SMat) { SMat->SetVectorParameterValue(TEXT("BaseColor"), AwningColor * 1.15f); Sign->SetMaterial(0, SMat); }
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
				Win->SetMaterial(0, WMat);
				NightWindowMaterials.Add(WMat);
				NightWindowLitColors.Add(WarmColors[CityRandom.RandRange(0, 3)]);
				NightWindowLitStates.Add(CityRandom.FRand() < 0.65f);
			}
		}
	}

	// ---- Sidewalk pad (light gray in front of shop) ----
	{
		UStaticMeshComponent* Pad = NewObject<UStaticMeshComponent>(Shop, TEXT("SidewalkPad"));
		Pad->SetStaticMesh(PlaneMesh);
		Pad->SetWorldScale3D(FVector(W / 100.f, 1.2f, 1.f));
		Pad->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + 60.f, 4.f));
		Pad->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Pad->SetCollisionResponseToAllChannels(ECR_Block);
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
		Neon->SetStaticMesh(CubeMesh);
		Neon->SetWorldScale3D(FVector(NeonW / 100.f, 3.f / 100.f, NeonH / 100.f));
		Neon->SetWorldLocation(FVector(Loc.X, Loc.Y + D * 0.5f + 6.f, NeonZ));
		Neon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Neon->RegisterComponent();
		Shop->AddInstanceComponent(Neon);

		UMaterialInstanceDynamic* NMat = UMaterialInstanceDynamic::Create(Neon->GetMaterial(0), Neon);
		if (NMat)
		{
			NMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.08f, 0.08f, 0.10f));
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
	if (BodyMat) { BodyMat->SetVectorParameterValue(TEXT("BaseColor"), WallColor); Body->SetMaterial(0, BodyMat); }
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
		if (SMat) { SMat->SetVectorParameterValue(TEXT("BaseColor"), AccentColor); Stripe->SetMaterial(0, SMat); }
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
		if (OMat) { OMat->SetVectorParameterValue(TEXT("BaseColor"), AccentColor); Overhang->SetMaterial(0, OMat); }
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
			if (CMat) { CMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.80f, 0.78f, 0.75f)); Col->SetMaterial(0, CMat); }
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
		if (DMat) { DMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.15f, 0.10f, 0.05f)); Door->SetMaterial(0, DMat); }
		Inst->AddInstanceComponent(Door);
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
	if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), Color); Body->SetMaterial(0, Mat); }
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
		if (DM) { DM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.4f, 0.4f, 0.38f)); Dock->SetMaterial(0, DM); }
		Bldg->AddInstanceComponent(Dock);
	}

	// Smokestack (20% chance) — thin cylinder on roof
	if (CityRandom.FRand() < 0.35f && CylinderMesh)
	{
		UStaticMeshComponent* Stack = NewObject<UStaticMeshComponent>(Bldg, TEXT("Stack"));
		Stack->SetStaticMesh(CylinderMesh);
		Stack->SetWorldScale3D(FVector(0.4f, 0.4f, 2.0f));
		Stack->SetWorldLocation(FVector(Loc.X + W * 0.3f, Loc.Y, H + 100.f));
		Stack->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Stack->RegisterComponent();
		UMaterialInstanceDynamic* SM = UMaterialInstanceDynamic::Create(Stack->GetMaterial(0), Stack);
		if (SM) { SM->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.35f, 0.35f, 0.33f)); Stack->SetMaterial(0, SM); }
		Bldg->AddInstanceComponent(Stack);
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
			if (BDMat) { BDMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.25f, 0.25f, 0.23f)); Bay->SetMaterial(0, BDMat); }
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
			if (AMat) { AMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.60f, 0.35f, 0.10f)); Accent->SetMaterial(0, AMat); }
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
		MC->SetMaterial(0, Mat);
	}
	Building->AddInstanceComponent(MC);

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
				if (WM) { WM->SetVectorParameterValue(TEXT("BaseColor"), BandColor); WB->SetMaterial(0, WM); }
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
			WFaces[NumFaces++] = {  0.f,  WExtY + 6.f, true,  static_cast<float>(Size.X) };
			WFaces[NumFaces++] = {  0.f, -WExtY - 6.f, true,  static_cast<float>(Size.X) };
		}
		else
		{
			WFaces[NumFaces++] = {  WExtX + 6.f, 0.f, false, static_cast<float>(Size.Y) };
			WFaces[NumFaces++] = { -WExtX - 6.f, 0.f, false, static_cast<float>(Size.Y) };
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

	// ---- Rooftop detail (25% -- AC unit or water tank) ----
	if (DetailRandom.FRand() < 0.25f && Size.Z > 600.f)
	{
		const float RoofZ = Size.Z + 2.f;
		if (DetailRandom.FRand() < 0.5f)
		{
			// AC unit (cube)
			UStaticMeshComponent* AC = NewObject<UStaticMeshComponent>(Building, TEXT("RooftopAC"));
			AC->SetStaticMesh(CubeMesh);
			AC->SetWorldScale3D(FVector(0.6f, 0.4f, 0.3f));
			AC->SetWorldLocation(FVector(Location.X + Size.X * 0.2f, Location.Y - Size.Y * 0.2f, RoofZ + 15.f));
			AC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			AC->RegisterComponent();
			UMaterialInstanceDynamic* ACMat = UMaterialInstanceDynamic::Create(AC->GetMaterial(0), AC);
			if (ACMat) { ACMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.50f, 0.50f, 0.48f)); AC->SetMaterial(0, ACMat); }
			Building->AddInstanceComponent(AC);
		}
		else if (CylinderMesh)
		{
			// Water tank (cylinder)
			UStaticMeshComponent* Tank = NewObject<UStaticMeshComponent>(Building, TEXT("RooftopTank"));
			Tank->SetStaticMesh(CylinderMesh);
			Tank->SetWorldScale3D(FVector(0.5f, 0.5f, 0.8f));
			Tank->SetWorldLocation(FVector(Location.X - Size.X * 0.15f, Location.Y + Size.Y * 0.15f, RoofZ + 40.f));
			Tank->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Tank->RegisterComponent();
			UMaterialInstanceDynamic* TkMat = UMaterialInstanceDynamic::Create(Tank->GetMaterial(0), Tank);
			if (TkMat) { TkMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.40f, 0.40f, 0.38f)); Tank->SetMaterial(0, TkMat); }
			Building->AddInstanceComponent(Tank);
		}
	}
}

// ---------------------------------------------------------------------------
// Road strip
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnRoadStrip(const FVector& Center, float LengthX, float LengthY)
{
	UWorld* World = GetWorld();
	if (!World || !PlaneMesh) return;

	AActor* Road = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector(Center.X, Center.Y, 2.f), FRotator::ZeroRotator);
	if (!Road) return;
#if WITH_EDITOR
	Road->SetActorLabel(TEXT("CityRoad"));
#endif

	UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Road, TEXT("RoadMesh"));
	MC->SetStaticMesh(PlaneMesh);
	MC->SetWorldScale3D(FVector(LengthX / 100.f, LengthY / 100.f, 1.f));
	MC->SetWorldLocation(FVector(Center.X, Center.Y, 2.f));
	MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MC->SetCollisionResponseToAllChannels(ECR_Block);
	MC->RegisterComponent();
	Road->SetRootComponent(MC);

	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
	if (Mat)
	{
		// Black asphalt road surface.
		Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.015f, 0.015f, 0.018f));
		MC->SetMaterial(0, Mat);
	}
	Road->AddInstanceComponent(MC);

	// ---- Road markings (yellow center line + white edge lines) ----
	if (!CubeMesh) return;

	const bool bAlongX = (LengthX > LengthY);
	const float RoadLen = bAlongX ? LengthX : LengthY;
	const float RoadWid = bAlongX ? LengthY : LengthX;

	auto SpawnLine = [&](const FVector& LineLoc, const FLinearColor& LineColor)
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
			LineMC->SetMaterial(0, LMat);
		}
		Road->AddInstanceComponent(LineMC);
	};

	const FLinearColor YellowLine(1.2f, 1.0f, 0.0f);
	const FLinearColor WhiteLine(0.7f, 0.7f, 0.7f);
	const float LineZ = 3.5f;

	// Center line.
	SpawnLine(FVector(Center.X, Center.Y, LineZ), YellowLine);

	// Edge lines.
	if (bAlongX)
	{
		SpawnLine(FVector(Center.X, Center.Y - RoadWid * 0.5f + 50.f, LineZ), WhiteLine);
		SpawnLine(FVector(Center.X, Center.Y + RoadWid * 0.5f - 50.f, LineZ), WhiteLine);
	}
	else
	{
		SpawnLine(FVector(Center.X - RoadWid * 0.5f + 50.f, Center.Y, LineZ), WhiteLine);
		SpawnLine(FVector(Center.X + RoadWid * 0.5f - 50.f, Center.Y, LineZ), WhiteLine);
	}
}

// ---------------------------------------------------------------------------
// Sidewalk strip
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnSidewalk(const FVector& Center, float LengthX, float LengthY)
{
	UWorld* World = GetWorld();
	if (!World || !PlaneMesh) return;

	AActor* Walk = World->SpawnActor<AActor>(AActor::StaticClass(),
		FVector(Center.X, Center.Y, 3.f), FRotator::ZeroRotator);
	if (!Walk) return;
#if WITH_EDITOR
	Walk->SetActorLabel(TEXT("CitySidewalk"));
#endif

	UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Walk, TEXT("SidewalkMesh"));
	MC->SetStaticMesh(PlaneMesh);
	MC->SetWorldScale3D(FVector(LengthX / 100.f, LengthY / 100.f, 1.f));
	MC->SetWorldLocation(FVector(Center.X, Center.Y, 3.f));
	MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MC->SetCollisionResponseToAllChannels(ECR_Block);
	MC->RegisterComponent();
	Walk->SetRootComponent(MC);

	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
	if (Mat)
	{
		// Concrete sidewalk.
		Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.40f, 0.38f, 0.35f));
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
		PoleMC->SetMaterial(0, PoleMat);
	}
	LampActor->AddInstanceComponent(PoleMC);

	// ---- Point light (warm sodium street light) ----
	UPointLightComponent* PL = NewObject<UPointLightComponent>(LampActor, TEXT("LampLight"));
	PL->SetWorldLocation(FVector(Location.X, Location.Y, PoleHeight));
	PL->SetIntensity(12000.f);
	PL->SetAttenuationRadius(2500.f);
	PL->SetLightColor(FColor(255, 200, 120));
	PL->SetCastShadows(false); // Performance: no shadows from street lights
	PL->RegisterComponent();
	LampActor->AddInstanceComponent(PL);
}

// ---------------------------------------------------------------------------
// Palm tree (trunk cylinder + canopy sphere)
// ---------------------------------------------------------------------------

void UPHCCitySubsystem::SpawnPalmTree(const FVector& Location, float Height, bool bDetailed)
{
	UWorld* World = GetWorld();
	if (!World || !CylinderMesh || !CubeMesh) return;

	// Scale all trees ~4.8x taller (20% boost over original 4x).
	Height *= 4.8f;

	AActor* Tree = World->SpawnActor<AActor>(AActor::StaticClass(),
		Location, FRotator::ZeroRotator);
	if (!Tree) return;
#if WITH_EDITOR
	Tree->SetActorLabel(TEXT("PalmTree"));
#endif

	// ---- Trunk (tapered: wider at base, thinner at top) ----
	const float BaseRadius = Height * 0.012f;
	const float TopRadius = Height * 0.006f;
	const int32 TrunkSegments = bDetailed ? 3 : 2;
	const float SegHeight = Height / TrunkSegments;
	const float LeanAngle = CityRandom.FRandRange(-5.f, 5.f);
	const float LeanYaw = CityRandom.FRandRange(0.f, 360.f);

	// Brown bark color (varies slightly per tree).
	const FLinearColor TrunkColor(
		CityRandom.FRandRange(0.18f, 0.28f),
		CityRandom.FRandRange(0.12f, 0.18f),
		CityRandom.FRandRange(0.04f, 0.08f));

	for (int32 S = 0; S < TrunkSegments; ++S)
	{
		const float T = static_cast<float>(S) / TrunkSegments;
		const float Radius = FMath::Lerp(BaseRadius, TopRadius, T);
		const float SegZ = S * SegHeight + SegHeight * 0.5f;

		FString Name = FString::Printf(TEXT("Trunk_%d"), S);
		UStaticMeshComponent* Seg = NewObject<UStaticMeshComponent>(Tree, *Name);
		Seg->SetStaticMesh(CylinderMesh);
		Seg->SetWorldScale3D(FVector(Radius / 50.f, Radius / 50.f, SegHeight / 100.f));
		Seg->SetWorldLocation(FVector(Location.X, Location.Y, SegZ));
		Seg->SetWorldRotation(FRotator(LeanAngle * T, LeanYaw, 0.f));
		Seg->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Seg->RegisterComponent();
		if (S == 0) Tree->SetRootComponent(Seg);

		UMaterialInstanceDynamic* SegMat = UMaterialInstanceDynamic::Create(Seg->GetMaterial(0), Seg);
		if (SegMat)
		{
			SegMat->SetVectorParameterValue(TEXT("BaseColor"), TrunkColor);
			Seg->SetMaterial(0, SegMat);
		}
		Tree->AddInstanceComponent(Seg);
	}

	// ---- Fronds (elongated flat cubes radiating from the crown) ----
	const float CrownZ = Height;
	const int32 NumFronds = bDetailed ? CityRandom.RandRange(6, 9) : CityRandom.RandRange(4, 6);
	const float FrondLength = Height * 0.28f;
	const float FrondWidth = Height * 0.04f;
	const float FrondThickness = Height * 0.008f;

	// Frond green (varies slightly per tree).
	const FLinearColor FrondColor(
		CityRandom.FRandRange(0.02f, 0.08f),
		CityRandom.FRandRange(0.15f, 0.30f),
		CityRandom.FRandRange(0.01f, 0.06f));

	for (int32 F = 0; F < NumFronds; ++F)
	{
		const float Yaw = (360.f / NumFronds) * F + CityRandom.FRandRange(-10.f, 10.f);
		// Droop angle: fronds hang downward from the crown.
		const float Droop = CityRandom.FRandRange(20.f, 45.f);

		FString Name = FString::Printf(TEXT("Frond_%d"), F);
		UStaticMeshComponent* Frond = NewObject<UStaticMeshComponent>(Tree, *Name);
		Frond->SetStaticMesh(CubeMesh);
		// Long, flat, thin leaf shape.
		Frond->SetWorldScale3D(FVector(FrondLength / 50.f, FrondWidth / 50.f, FrondThickness / 50.f));

		// Position at the crown, offset outward along the frond direction.
		const float HalfLen = FrondLength * 0.5f;
		const float YawRad = FMath::DegreesToRadians(Yaw);
		const float DroopRad = FMath::DegreesToRadians(Droop);
		const FVector FrondCenter(
			Location.X + HalfLen * FMath::Cos(DroopRad) * FMath::Cos(YawRad),
			Location.Y + HalfLen * FMath::Cos(DroopRad) * FMath::Sin(YawRad),
			CrownZ - HalfLen * FMath::Sin(DroopRad));

		Frond->SetWorldLocation(FrondCenter);
		Frond->SetWorldRotation(FRotator(-Droop, Yaw, 0.f));
		Frond->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Frond->RegisterComponent();

		UMaterialInstanceDynamic* FMat = UMaterialInstanceDynamic::Create(Frond->GetMaterial(0), Frond);
		if (FMat)
		{
			FMat->SetVectorParameterValue(TEXT("BaseColor"), FrondColor);
			Frond->SetMaterial(0, FMat);
		}
		Tree->AddInstanceComponent(Frond);
	}

	// ---- Coconut cluster (small spheres at the crown base, detailed only) ----
	if (bDetailed && SphereMesh && CityRandom.FRand() < 0.6f)
	{
		const int32 NumCoconuts = CityRandom.RandRange(2, 4);
		const float CoconutRadius = Height * 0.008f;
		for (int32 C = 0; C < NumCoconuts; ++C)
		{
			const float CYaw = CityRandom.FRandRange(0.f, 360.f);
			const float CDist = TopRadius + CoconutRadius * 2.f;
			FString Name = FString::Printf(TEXT("Coconut_%d"), C);
			UStaticMeshComponent* Coco = NewObject<UStaticMeshComponent>(Tree, *Name);
			Coco->SetStaticMesh(SphereMesh);
			Coco->SetWorldScale3D(FVector(CoconutRadius / 50.f));
			Coco->SetWorldLocation(FVector(
				Location.X + CDist * FMath::Cos(FMath::DegreesToRadians(CYaw)),
				Location.Y + CDist * FMath::Sin(FMath::DegreesToRadians(CYaw)),
				CrownZ - CoconutRadius * 3.f));
			Coco->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Coco->RegisterComponent();

			UMaterialInstanceDynamic* CMat = UMaterialInstanceDynamic::Create(Coco->GetMaterial(0), Coco);
			if (CMat)
			{
				CMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.25f, 0.18f, 0.06f));
				Coco->SetMaterial(0, CMat);
			}
			Tree->AddInstanceComponent(Coco);
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
		const FLinearColor& Color, const TCHAR* Label)
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
	SpawnHill(FVector(-255000.f, -22000.f, 0.f), 11000.f, 9000.f, 2800.f, EarthBrown, TEXT("Mtn_Backdrop3"));

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCitySubsystem: Spawned rolling hills and highway corridor mountains."));
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

	const float SkyRadius = 80000.f;

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
		if (SM) { SM->SetVectorParameterValue(TEXT("BaseColor"), Col); SC->SetMaterial(0, SM); }
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
		if (SM) { SM->SetVectorParameterValue(TEXT("BaseColor"), Col); SC->SetMaterial(0, SM); }
		StarfieldActor->AddInstanceComponent(SC);
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Spawned %d stars (180 bg + 250 Milky Way)."), StarIdx);
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
		const FLinearColor& Color, const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, Name);
		MC->SetStaticMesh(CubeMesh);
		MC->SetWorldScale3D(Size / 100.f);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->RegisterComponent();
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, MC);
		if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), Color); MC->SetMaterial(0, Mat); }
		Owner->AddInstanceComponent(MC);
		return MC;
	};

	auto MakeCylMC = [&](AActor* Owner, const FVector& Loc, const FVector& Size,
		const FLinearColor& Color, const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, Name);
		MC->SetStaticMesh(CylinderMesh);
		MC->SetWorldScale3D(Size / 100.f);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->RegisterComponent();
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, MC);
		if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), Color); MC->SetMaterial(0, Mat); }
		Owner->AddInstanceComponent(MC);
		return MC;
	};

	auto MakeSphereMC = [&](AActor* Owner, const FVector& Loc, const FVector& Size,
		const FLinearColor& Color, const TCHAR* Name) -> UStaticMeshComponent*
	{
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Owner, Name);
		MC->SetStaticMesh(SphereMesh);
		MC->SetWorldScale3D(Size / 100.f);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MC->RegisterComponent();
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(ColorMaterial, MC);
		if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), Color); MC->SetMaterial(0, Mat); }
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
				FVector(W, BandD, BandH), GlassColor, *NY);
			// -Y face
			MakeCubeMC(Owner, FVector(BaseLoc.X, BaseLoc.Y - D * 0.5f - 3.f, BZ),
				FVector(W, BandD, BandH), GlassColor, *NnY);
			// +X face
			MakeCubeMC(Owner, FVector(BaseLoc.X + W * 0.5f + 3.f, BaseLoc.Y, BZ),
				FVector(BandD, D, BandH), GlassColor, *NX);
			// -X face
			MakeCubeMC(Owner, FVector(BaseLoc.X - W * 0.5f - 3.f, BaseLoc.Y, BZ),
				FVector(BandD, D, BandH), GlassColor, *NnX);
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
			MakeCubeMC(Owner, Corners[ci], FVector(ColW, ColW, H), Color, *CN);
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
			// Wide podium base (3 stories)
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 500.f),
				FVector(900.f, 800.f, 1000.f), SandStone, TEXT("Pinnacle_Pod"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Main tower
			const float MainH = 5000.f;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1000.f + MainH * 0.5f),
				FVector(550.f, 500.f, MainH), WarmConcrete, TEXT("Pinnacle_Main"));

			// First setback
			const float Set1H = 1800.f;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1000.f + MainH + Set1H * 0.5f),
				FVector(400.f, 380.f, Set1H), WarmConcrete * 0.95f, TEXT("Pinnacle_Set1"));

			// Second setback (crown)
			const float Set2H = 900.f;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1000.f + MainH + Set1H + Set2H * 0.5f),
				FVector(250.f, 240.f, Set2H), SandStone, TEXT("Pinnacle_Set2"));

			// Decorative crown ledges
			const float CrownZ = 1000.f + MainH + Set1H + Set2H;
			for (int32 i = 0; i < 4; ++i)
			{
				const float LZ = CrownZ - i * 100.f;
				const float LW = 260.f - i * 15.f;
				FString LN = FString::Printf(TEXT("Pinnacle_Ledge_%d"), i);
				MakeCubeMC(T, FVector(Loc.X, Loc.Y, LZ),
					FVector(LW + 30.f, LW + 20.f, 25.f), GoldAccent, *LN);
			}

			// Antenna spire
			MakeCylMC(T, FVector(Loc.X, Loc.Y, CrownZ + 600.f),
				FVector(15.f, 15.f, 1200.f), LightGray, TEXT("Pinnacle_Spire"));
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, CrownZ + 1200.f),
				FVector(25.f, 25.f, 25.f), AntennaRed, TEXT("Pinnacle_Light"));

			AddCornerColumns(T, FVector(Loc.X, Loc.Y, 1000.f), 560.f, 510.f, MainH, 35.f, SandStone * 0.8f);
			AddGlassBands(T, FVector(Loc.X, Loc.Y, 1000.f), 550.f, 500.f, MainH, TintedGlass, BandIdx);
			AddWindows(T, FVector(Loc.X, Loc.Y, 1000.f), 550.f, 500.f, MainH + Set1H, WinIdx);

			// Ground floor canopy
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 410.f, 280.f),
				FVector(400.f, 120.f, 15.f), GoldAccent, TEXT("Pinnacle_Canopy"));

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
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(500.f, 450.f, H), MidnightBlue, TEXT("Glass_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			AddGlassBands(T, Loc, 500.f, 450.f, H, SkyGlass, BandIdx);

			// Mechanical penthouse
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, H + 200.f),
				FVector(300.f, 250.f, 400.f), DarkSteel, TEXT("Glass_Pent"));

			// Rooftop antenna array
			for (int32 i = -1; i <= 1; ++i)
			{
				FString AN = FString::Printf(TEXT("Glass_Ant_%d"), i + 1);
				MakeCylMC(T, FVector(Loc.X + i * 80.f, Loc.Y, H + 400.f + 300.f),
					FVector(8.f, 8.f, 600.f), LightGray, *AN);
			}

			// Ground floor glass lobby
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 230.f, 200.f),
				FVector(480.f, 20.f, 380.f), SkyGlass * 1.5f, TEXT("Glass_Lobby"));

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
			// Wide base (6 stories)
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1000.f),
				FVector(1000.f, 900.f, 2000.f), LightConcrete, TEXT("Crown_Base"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Main tower rising from base
			const float TH = 4500.f;
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2000.f + TH * 0.5f),
				FVector(600.f, 550.f, TH), CoolWhite, TEXT("Crown_Tower"));

			// Gold crown cap
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2000.f + TH + 100.f),
				FVector(650.f, 600.f, 200.f), GoldAccent, TEXT("Crown_Cap1"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2000.f + TH + 250.f),
				FVector(550.f, 500.f, 100.f), GoldAccent, TEXT("Crown_Cap2"));

			AddCornerColumns(T, FVector(Loc.X, Loc.Y, 2000.f), 610.f, 560.f, TH, 40.f, LightConcrete * 0.85f);

			// Center vertical spines
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + 280.f, 2000.f + TH * 0.5f),
				FVector(40.f, 8.f, TH), LightConcrete * 0.8f, TEXT("Crown_SpineA"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y - 280.f, 2000.f + TH * 0.5f),
				FVector(40.f, 8.f, TH), LightConcrete * 0.8f, TEXT("Crown_SpineB"));

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

			auto* Body = MakeCylMC(T, FVector(Loc.X, Loc.Y, H * 0.5f),
				FVector(R, R, H), SteelGray, TEXT("Cyl_Main"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Observation deck ring
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H * 0.82f),
				FVector(R + 80.f, R + 80.f, 180.f), DarkGlass, TEXT("Cyl_ObsDeck"));
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H * 0.82f - 80.f),
				FVector(R + 90.f, R + 90.f, 20.f), LightGray, TEXT("Cyl_ObsFloor"));

			// Roof disc
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H + 10.f),
				FVector(R + 20.f, R + 20.f, 20.f), DarkSteel, TEXT("Cyl_Roof"));

			// Antenna
			MakeCylMC(T, FVector(Loc.X, Loc.Y, H + 600.f),
				FVector(12.f, 12.f, 1200.f), LightGray, TEXT("Cyl_Ant"));
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, H + 1200.f),
				FVector(20.f, 20.f, 20.f), AntennaRed, TEXT("Cyl_AntLight"));

			// Ground floor entrance
			MakeCubeMC(T, FVector(Loc.X, Loc.Y + R + 50.f, 200.f),
				FVector(300.f, 100.f, 380.f), SkyGlass * 1.5f, TEXT("Cyl_Entrance"));

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
			// Tier 1: wide base
			auto* Body = MakeCubeMC(T, FVector(Loc.X, Loc.Y, 1200.f),
				FVector(800.f, 700.f, 2400.f), SteelGray, TEXT("Mono_T1"));
			Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			Body->SetCollisionResponseToAllChannels(ECR_Block);

			// Tier 2
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2400.f + 1500.f),
				FVector(600.f, 520.f, 3000.f), SteelGray * 0.95f, TEXT("Mono_T2"));
			// Tier 3
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 5400.f + 1000.f),
				FVector(420.f, 380.f, 2000.f), SteelGray * 0.9f, TEXT("Mono_T3"));
			// Tier 4: crown
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 7400.f + 400.f),
				FVector(250.f, 230.f, 800.f), DarkSteel, TEXT("Mono_T4"));

			// Setback ledges
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 2400.f),
				FVector(820.f, 720.f, 40.f), CopperAccent, TEXT("Mono_L1"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 5400.f),
				FVector(620.f, 540.f, 40.f), CopperAccent, TEXT("Mono_L2"));
			MakeCubeMC(T, FVector(Loc.X, Loc.Y, 7400.f),
				FVector(440.f, 400.f, 40.f), CopperAccent, TEXT("Mono_L3"));

			// Spire
			MakeCylMC(T, FVector(Loc.X, Loc.Y, 8200.f + 700.f),
				FVector(18.f, 18.f, 1400.f), LightGray, TEXT("Mono_Spire"));
			MakeSphereMC(T, FVector(Loc.X, Loc.Y, 9600.f),
				FVector(28.f, 28.f, 28.f), AntennaRed, TEXT("Mono_Light"));

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
