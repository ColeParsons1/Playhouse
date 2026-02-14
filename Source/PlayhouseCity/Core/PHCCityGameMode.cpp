// Copyright PlayhouseCity. All Rights Reserved.

#include "Core/PHCCityGameMode.h"
#include "Core/PHCPlayerController.h"
#include "Player/PHCCityCharacter.h"
#include "Player/PHCPlayerState.h"
#include "Vehicle/PHCVehiclePawn.h"
#include "PlayhouseCity.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Engine/DirectionalLight.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "UObject/ConstructorHelpers.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/Engine.h"

// ---------------------------------------------------------------------------

APHCCityGameMode::APHCCityGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = APHCCityCharacter::StaticClass();
	PlayerControllerClass = APHCPlayerController::StaticClass();
	PlayerStateClass = APHCPlayerState::StaticClass();
	CityRandom.Initialize(42);
}

void APHCCityGameMode::BeginPlay()
{
	Super::BeginPlay();

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
		ColorMaterial = NewObject<UMaterial>(this, TEXT("PHC_ColorMaterial"));

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

// ---------------------------------------------------------------------------
// Day/Night Cycle -- 20 minute full cycle (10 min day + 10 min night)
// ---------------------------------------------------------------------------

void APHCCityGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

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

	// ---- Sky light (ambient fill) ----
	if (SkyLightComp)
	{
		// Bright during day, dim at night.
		const float DayFactor = FMath::Max(0.f, SunSine);
		SkyLightComp->SetIntensity(FMath::Lerp(0.08f, 1.5f, DayFactor));

		// Tint: blue-white during day, deep blue at night.
		const float NightFactor = FMath::Max(0.f, -SunSine);
		const FLinearColor DayTint(0.6f, 0.7f, 1.0f);
		const FLinearColor NightTint(0.04f, 0.04f, 0.12f);
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
		FogComp->SetFogDensity(FMath::Lerp(0.0008f, 0.003f, NightFactor));
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
void APHCCityGameMode::SpawnCityEnvironment()
{
	SpawnGround();
	SpawnOceanAndBeach();
	SpawnSkyAndLighting();
	SpawnCityGrid();
	SpawnStreetLights();
	SpawnVehicles();
	SpawnRamps();
	SpawnOutskirts();
}

// ---------------------------------------------------------------------------
// Ground
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnGround()
{
	UWorld* World = GetWorld();
	if (!World || !PlaneMesh) return;

	const FLinearColor GroundColor(0.08f, 0.18f, 0.04f);

	// Helper to spawn a ground section.
	auto SpawnGroundPlane = [&](const FVector& Loc, const FVector& Scale, const TCHAR* Label)
	{
		AActor* Ground = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!Ground) return;
#if WITH_EDITOR
		Ground->SetActorLabel(Label);
#endif
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Ground, TEXT("GroundMesh"));
		MC->SetStaticMesh(PlaneMesh);
		MC->SetWorldScale3D(Scale);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MC->SetCollisionResponseToAllChannels(ECR_Block);
		MC->RegisterComponent();
		Ground->SetRootComponent(MC);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
		if (Mat)
		{
			Mat->SetVectorParameterValue(TEXT("BaseColor"), GroundColor);
			MC->SetMaterial(0, Mat);
		}
		Ground->AddInstanceComponent(MC);
	};

	// Peninsula body: X from -30000 to 28000, Y from -30000 to 22000.
	// North-east corner is open (bay area).
	SpawnGroundPlane(FVector(-1000.f, -4000.f, 0.f), FVector(580.f, 520.f, 1.f), TEXT("PeninsulaBody"));

	// Northwest fill: X from -30000 to 5000, Y from 22000 to 28000.
	// Keeps the NW city on land while leaving NE open for the bay.
	SpawnGroundPlane(FVector(-12500.f, 25000.f, 0.f), FVector(350.f, 60.f, 1.f), TEXT("NorthwestFill"));

	// Outskirts: large drivable area extending west from city.
	SpawnGroundPlane(FVector(-55000.f, 0.f, 0.f), FVector(1400.f, 900.f, 1.f), TEXT("OutskirtsMain"));
	// Far outskirts: extends even further west.
	SpawnGroundPlane(FVector(-150000.f, 0.f, 0.f), FVector(600.f, 700.f, 1.f), TEXT("OutskirtsFar"));

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Peninsula ground spawned."));
}

// ---------------------------------------------------------------------------
// Ocean and Beach
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnOceanAndBeach()
{
	UWorld* World = GetWorld();
	if (!World || !PlaneMesh) return;

	const FLinearColor DeepOcean(0.02f, 0.08f, 0.22f);
	const FLinearColor ShallowBay(0.04f, 0.15f, 0.30f);
	const FLinearColor SandColor(0.65f, 0.55f, 0.35f);

	auto SpawnWater = [&](const FVector& Loc, const FVector& Scale,
		const FLinearColor& Color, const TCHAR* Label)
	{
		AActor* Water = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!Water) return;
#if WITH_EDITOR
		Water->SetActorLabel(Label);
#endif
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Water, TEXT("WaterMesh"));
		MC->SetStaticMesh(PlaneMesh);
		MC->SetWorldScale3D(Scale);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MC->SetCollisionResponseToAllChannels(ECR_Block);
		MC->RegisterComponent();
		Water->SetRootComponent(MC);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
		if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), Color); MC->SetMaterial(0, Mat); }
		Water->AddInstanceComponent(MC);
	};

	auto SpawnBeach = [&](const FVector& Loc, const FVector& Scale)
	{
		AActor* Beach = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!Beach) return;
#if WITH_EDITOR
		Beach->SetActorLabel(TEXT("Beach"));
#endif
		UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Beach, TEXT("BeachMesh"));
		MC->SetStaticMesh(PlaneMesh);
		MC->SetWorldScale3D(Scale);
		MC->SetWorldLocation(Loc);
		MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		MC->SetCollisionResponseToAllChannels(ECR_Block);
		MC->RegisterComponent();
		Beach->SetRootComponent(MC);
		UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
		if (Mat) { Mat->SetVectorParameterValue(TEXT("BaseColor"), SandColor); MC->SetMaterial(0, Mat); }
		Beach->AddInstanceComponent(MC);
	};

	// ---- Global ocean (visible everywhere land doesn't cover it) ----
	SpawnWater(FVector(0.f, 0.f, -8.f), FVector(2000.f, 2000.f, 1.f),
		DeepOcean, TEXT("OceanFloor"));

	// ---- Bay inlet (shallower water cutting into NE quadrant) ----
	SpawnWater(FVector(18000.f, 25000.f, -3.f), FVector(150.f, 80.f, 1.f),
		ShallowBay, TEXT("BayInlet"));

	// ---- Beach strips along peninsula coastline ----
	// East coast (runs N-S along eastern edge)
	SpawnBeach(FVector(29500.f, -4000.f, -1.f), FVector(18.f, 520.f, 1.f));
	// South coast (runs E-W along southern edge)
	SpawnBeach(FVector(-1000.f, -31500.f, -1.f), FVector(600.f, 16.f, 1.f));
	// North coast west of bay (runs E-W)
	SpawnBeach(FVector(-12500.f, 23500.f, -1.f), FVector(200.f, 16.f, 1.f));
	// Bay south shoreline (inner edge of bay)
	SpawnBeach(FVector(18000.f, 21000.f, -1.f), FVector(140.f, 12.f, 1.f));
	// Bay west shoreline (inner edge of bay)
	SpawnBeach(FVector(7000.f, 25000.f, -1.f), FVector(12.f, 50.f, 1.f));
	// Southeast corner (connects east + south beaches)
	SpawnBeach(FVector(29000.f, -30000.f, -1.f), FVector(20.f, 20.f, 1.f));

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Peninsula with bay area spawned."));
}

// ---------------------------------------------------------------------------
// Sky and Lighting -- NIGHT TIME
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnSkyAndLighting()
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
			FogComp->SetFogMaxOpacity(0.35f);
			FogComp->SetStartDistance(8000.f);
			FogComp->SetVolumetricFog(false);
			FogComp->RegisterComponent();
			FogActor->SetRootComponent(FogComp);
			FogActor->AddInstanceComponent(FogComp);
		}
	}
}

// ---------------------------------------------------------------------------
// City grid -- roads, sidewalks, and buildings (Brookhaven suburban style)
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnCityGrid()
{
	constexpr int32 NumRoads = 11;
	constexpr float RoadSpacing = 5000.f;
	constexpr float RoadWidth = 900.f;
	constexpr float SidewalkWidth = 250.f;
	constexpr float CityHalfExtent = (NumRoads - 1) * RoadSpacing * 0.5f + 2000.f;
	constexpr float RoadLength = CityHalfExtent * 2.f;
	const float FirstRoad = -(NumRoads - 1) * RoadSpacing * 0.5f;

	// ---- Roads (avenues along X) ----
	for (int32 i = 0; i < NumRoads; ++i)
	{
		const float Y = FirstRoad + i * RoadSpacing;
		SpawnRoadStrip(FVector(0.f, Y, 0.f), RoadLength, RoadWidth);
		SpawnSidewalk(FVector(0.f, Y - RoadWidth * 0.5f - SidewalkWidth * 0.5f, 0.f), RoadLength, SidewalkWidth);
		SpawnSidewalk(FVector(0.f, Y + RoadWidth * 0.5f + SidewalkWidth * 0.5f, 0.f), RoadLength, SidewalkWidth);
	}
	// ---- Roads (streets along Y) ----
	for (int32 i = 0; i < NumRoads; ++i)
	{
		const float X = FirstRoad + i * RoadSpacing;
		SpawnRoadStrip(FVector(X, 0.f, 0.f), RoadWidth, RoadLength);
		SpawnSidewalk(FVector(X - RoadWidth * 0.5f - SidewalkWidth * 0.5f, 0.f, 0.f), SidewalkWidth, RoadLength);
		SpawnSidewalk(FVector(X + RoadWidth * 0.5f + SidewalkWidth * 0.5f, 0.f, 0.f), SidewalkWidth, RoadLength);
	}

	// ---- Brookhaven-style color palettes ----
	const FLinearColor HouseWalls[] = {
		FLinearColor(0.85f, 0.83f, 0.80f),  // white
		FLinearColor(0.82f, 0.78f, 0.65f),  // cream
		FLinearColor(0.72f, 0.78f, 0.85f),  // pale blue
		FLinearColor(0.85f, 0.82f, 0.62f),  // light yellow
		FLinearColor(0.68f, 0.75f, 0.65f),  // sage green
		FLinearColor(0.80f, 0.72f, 0.58f),  // light tan
	};
	const FLinearColor HouseRoofs[] = {
		FLinearColor(0.18f, 0.18f, 0.20f),  // charcoal
		FLinearColor(0.22f, 0.15f, 0.08f),  // dark brown
		FLinearColor(0.20f, 0.22f, 0.28f),  // slate blue
	};
	const FLinearColor ShopWalls[] = {
		FLinearColor(0.88f, 0.86f, 0.84f),  // white
		FLinearColor(0.78f, 0.76f, 0.74f),  // light gray
		FLinearColor(0.85f, 0.80f, 0.70f),  // cream
	};
	const FLinearColor ShopAwnings[] = {
		FLinearColor(0.70f, 0.12f, 0.10f),  // red
		FLinearColor(0.10f, 0.45f, 0.48f),  // teal
		FLinearColor(0.12f, 0.42f, 0.18f),  // green
		FLinearColor(0.75f, 0.38f, 0.08f),  // orange
	};

	int32 BuildingCount = 0;

	for (int32 bx = 0; bx < NumRoads - 1; ++bx)
	{
		for (int32 by = 0; by < NumRoads - 1; ++by)
		{
			const float BlockMinX = FirstRoad + bx * RoadSpacing + RoadWidth * 0.5f + SidewalkWidth;
			const float BlockMaxX = FirstRoad + (bx + 1) * RoadSpacing - RoadWidth * 0.5f - SidewalkWidth;
			const float BlockMinY = FirstRoad + by * RoadSpacing + RoadWidth * 0.5f + SidewalkWidth;
			const float BlockMaxY = FirstRoad + (by + 1) * RoadSpacing - RoadWidth * 0.5f - SidewalkWidth;

			const float BlockW = BlockMaxX - BlockMinX;
			const float BlockH = BlockMaxY - BlockMinY;
			const float BlockCenterX = (BlockMinX + BlockMaxX) * 0.5f;
			const float BlockCenterY = (BlockMinY + BlockMaxY) * 0.5f;

			if (BlockW < 500.f || BlockH < 500.f) continue;

			// Central park (2x2 blocks around origin) -- unchanged.
			if ((bx == 4 || bx == 5) && (by == 4 || by == 5)) continue;

			// ---- Zone classification ----
			const bool bIsDowntown = (bx >= 3 && bx <= 6 && by >= 3 && by <= 6);
			const bool bIsSuburban = (!bIsDowntown &&
				((bx >= 1 && bx <= 2) || (bx >= 7 && bx <= 8) ||
				 (by >= 1 && by <= 2) || (by >= 7 && by <= 8)));

			// ---- Landmark institutions at specific blocks ----
			bool bIsLandmark = false;

			if (bx == 1 && by == 1) // Hospital
			{
				bIsLandmark = true;
				SpawnInstitution(FVector(BlockCenterX, BlockCenterY, 0.f),
					BlockW * 0.75f, BlockH * 0.65f, 840.f,
					FLinearColor(0.88f, 0.86f, 0.84f), FLinearColor(0.70f, 0.10f, 0.10f));
				BuildingCount++;
			}
			else if (bx == 1 && by == 8) // Police Station
			{
				bIsLandmark = true;
				SpawnInstitution(FVector(BlockCenterX, BlockCenterY, 0.f),
					BlockW * 0.65f, BlockH * 0.55f, 560.f,
					FLinearColor(0.55f, 0.35f, 0.25f), FLinearColor(0.15f, 0.25f, 0.55f));
				BuildingCount++;
			}
			else if (bx == 8 && by == 1) // Fire Station
			{
				bIsLandmark = true;
				SpawnInstitution(FVector(BlockCenterX, BlockCenterY, 0.f),
					BlockW * 0.65f, BlockH * 0.55f, 560.f,
					FLinearColor(0.65f, 0.18f, 0.12f), FLinearColor(0.88f, 0.86f, 0.84f));
				BuildingCount++;
			}
			else if (bx == 8 && by == 5) // School
			{
				bIsLandmark = true;
				SpawnInstitution(FVector(BlockCenterX, BlockCenterY, 0.f),
					BlockW * 0.70f, BlockH * 0.60f, 700.f,
					FLinearColor(0.80f, 0.72f, 0.58f), FLinearColor(0.15f, 0.42f, 0.20f));
				BuildingCount++;
			}
			else if (bx == 3 && by == 5) // Grocery Store (large shop)
			{
				bIsLandmark = true;
				SpawnShop(FVector(BlockCenterX, BlockCenterY, 0.f),
					BlockW * 0.80f, BlockH * 0.60f, 1,
					FLinearColor(0.88f, 0.86f, 0.84f), FLinearColor(0.12f, 0.42f, 0.18f));
				BuildingCount++;
			}
			else if (bx == 5 && by == 3) // Coffee Shop
			{
				bIsLandmark = true;
				SpawnShop(FVector(BlockCenterX, BlockCenterY, 0.f),
					BlockW * 0.55f, BlockH * 0.50f, 1,
					FLinearColor(0.85f, 0.80f, 0.70f), FLinearColor(0.35f, 0.22f, 0.10f));
				BuildingCount++;
			}

			if (bIsLandmark) continue;

			// ---- Zone-based building spawning ----
			if (bIsDowntown)
			{
				// Shops: 2-4 per block
				const int32 NumShops = CityRandom.RandRange(2, 4);
				const float ShopW = BlockW / NumShops * 0.85f;
				const float ShopD = BlockH * CityRandom.FRandRange(0.45f, 0.60f);
				const float StartX = BlockMinX + (BlockW - ShopW * NumShops) * 0.5f + ShopW * 0.5f;

				for (int32 i = 0; i < NumShops; ++i)
				{
					const float SX = StartX + i * ShopW;
					const int32 ShopStories = CityRandom.RandRange(1, 2);
					SpawnShop(FVector(SX, BlockCenterY, 0.f),
						ShopW * CityRandom.FRandRange(0.75f, 0.95f),
						ShopD,
						ShopStories,
						ShopWalls[CityRandom.RandRange(0, 2)],
						ShopAwnings[CityRandom.RandRange(0, 3)]);
					BuildingCount++;
				}
			}
			else if (bIsSuburban)
			{
				// Houses: 2-4 per block
				const int32 NumHouses = CityRandom.RandRange(2, 4);
				const float LotW = BlockW / FMath::Min(NumHouses, 2);

				for (int32 i = 0; i < NumHouses; ++i)
				{
					const float HX = BlockMinX + (i % 2 + 0.5f) * LotW;
					const float HY = (NumHouses > 2 && i >= 2)
						? BlockCenterY + BlockH * 0.25f
						: BlockCenterY - BlockH * 0.25f;

					const float HW = CityRandom.FRandRange(500.f, 700.f);
					const float HD = CityRandom.FRandRange(400.f, 550.f);
					const int32 HStories = CityRandom.RandRange(1, 2);

					SpawnHouse(FVector(HX, HY, 0.f), HW, HD, HStories,
						HouseWalls[CityRandom.RandRange(0, 5)],
						HouseRoofs[CityRandom.RandRange(0, 2)]);
					BuildingCount++;
				}
			}
			else // Edge -- sparse large-lot houses
			{
				const int32 NumHouses = CityRandom.RandRange(1, 2);
				for (int32 i = 0; i < NumHouses; ++i)
				{
					const float HX = BlockCenterX + (i == 0 ? -BlockW * 0.2f : BlockW * 0.2f);
					const float HW = CityRandom.FRandRange(550.f, 700.f);
					const float HD = CityRandom.FRandRange(450.f, 550.f);

					SpawnHouse(FVector(HX, BlockCenterY, 0.f), HW, HD,
						CityRandom.RandRange(1, 2),
						HouseWalls[CityRandom.RandRange(0, 5)],
						HouseRoofs[CityRandom.RandRange(0, 2)]);
					BuildingCount++;
				}
			}
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Spawned %d Brookhaven-style buildings."), BuildingCount);

	// ---- Central Park features (blocks 4,5 x 4,5) ----
	{
		const float ParkCenterX = FirstRoad + 4.5f * RoadSpacing;
		const float ParkCenterY = FirstRoad + 4.5f * RoadSpacing;

		// Park ground (green grass overlay slightly above road).
		const float ParkSize = RoadSpacing * 1.8f;
		AActor* ParkGround = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(ParkCenterX, ParkCenterY, 4.f), FRotator::ZeroRotator);
		if (ParkGround && PlaneMesh)
		{
#if WITH_EDITOR
			ParkGround->SetActorLabel(TEXT("CentralParkGround"));
#endif
			UStaticMeshComponent* PGM = NewObject<UStaticMeshComponent>(ParkGround, TEXT("ParkMesh"));
			PGM->SetStaticMesh(PlaneMesh);
			PGM->SetWorldScale3D(FVector(ParkSize / 100.f, ParkSize / 100.f, 1.f));
			PGM->SetWorldLocation(FVector(ParkCenterX, ParkCenterY, 4.f));
			PGM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			PGM->SetCollisionResponseToAllChannels(ECR_Block);
			PGM->RegisterComponent();
			ParkGround->SetRootComponent(PGM);
			UMaterialInstanceDynamic* ParkMat = UMaterialInstanceDynamic::Create(PGM->GetMaterial(0), PGM);
			if (ParkMat)
			{
				ParkMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.06f, 0.22f, 0.04f));
				PGM->SetMaterial(0, ParkMat);
			}
			ParkGround->AddInstanceComponent(PGM);
		}

		// Fountain (central cylinder + water disc).
		if (CylinderMesh && SphereMesh)
		{
			AActor* Fountain = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
				FVector(ParkCenterX, ParkCenterY, 0.f), FRotator::ZeroRotator);
			if (Fountain)
			{
#if WITH_EDITOR
				Fountain->SetActorLabel(TEXT("ParkFountain"));
#endif

				// Basin (short wide cylinder).
				UStaticMeshComponent* Basin = NewObject<UStaticMeshComponent>(Fountain, TEXT("Basin"));
				Basin->SetStaticMesh(CylinderMesh);
				Basin->SetWorldScale3D(FVector(5.f, 5.f, 0.6f));
				Basin->SetWorldLocation(FVector(ParkCenterX, ParkCenterY, 30.f));
				Basin->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				Basin->SetCollisionResponseToAllChannels(ECR_Block);
				Basin->RegisterComponent();
				Fountain->SetRootComponent(Basin);
				UMaterialInstanceDynamic* BasinMat = UMaterialInstanceDynamic::Create(Basin->GetMaterial(0), Basin);
				if (BasinMat)
				{
					BasinMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.55f, 0.52f, 0.48f));
					Basin->SetMaterial(0, BasinMat);
				}
				Fountain->AddInstanceComponent(Basin);

				// Water surface (flat blue disc).
				UStaticMeshComponent* Water = NewObject<UStaticMeshComponent>(Fountain, TEXT("Water"));
				Water->SetStaticMesh(CylinderMesh);
				Water->SetWorldScale3D(FVector(4.5f, 4.5f, 0.05f));
				Water->SetWorldLocation(FVector(ParkCenterX, ParkCenterY, 55.f));
				Water->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Water->RegisterComponent();
				UMaterialInstanceDynamic* WaterMat = UMaterialInstanceDynamic::Create(Water->GetMaterial(0), Water);
				if (WaterMat)
				{
					WaterMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.05f, 0.18f, 0.35f));
					Water->SetMaterial(0, WaterMat);
				}
				Fountain->AddInstanceComponent(Water);

				// Central spout (tall thin cylinder).
				UStaticMeshComponent* Spout = NewObject<UStaticMeshComponent>(Fountain, TEXT("Spout"));
				Spout->SetStaticMesh(CylinderMesh);
				Spout->SetWorldScale3D(FVector(0.3f, 0.3f, 2.5f));
				Spout->SetWorldLocation(FVector(ParkCenterX, ParkCenterY, 150.f));
				Spout->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Spout->RegisterComponent();
				UMaterialInstanceDynamic* SpoutMat = UMaterialInstanceDynamic::Create(Spout->GetMaterial(0), Spout);
				if (SpoutMat)
				{
					SpoutMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.60f, 0.58f, 0.55f));
					Spout->SetMaterial(0, SpoutMat);
				}
				Fountain->AddInstanceComponent(Spout);
			}
		}

		// Park benches (small cubes arranged around the fountain).
		if (CubeMesh)
		{
			const FVector2D BenchOffsets[] = {
				{ 400.f,  0.f},  {-400.f,  0.f},
				{ 0.f,  400.f},  { 0.f, -400.f},
				{ 700.f,  700.f}, {-700.f,  700.f},
				{ 700.f, -700.f}, {-700.f, -700.f},
			};
			for (int32 i = 0; i < 8; ++i)
			{
				const FVector BenchLoc(ParkCenterX + BenchOffsets[i].X,
					ParkCenterY + BenchOffsets[i].Y, 25.f);
				AActor* Bench = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
					BenchLoc, FRotator::ZeroRotator);
				if (!Bench) continue;
#if WITH_EDITOR
				Bench->SetActorLabel(TEXT("ParkBench"));
#endif
				UStaticMeshComponent* BMC = NewObject<UStaticMeshComponent>(Bench, TEXT("BenchMesh"));
				BMC->SetStaticMesh(CubeMesh);
				BMC->SetWorldScale3D(FVector(1.2f, 0.4f, 0.35f));
				BMC->SetWorldLocation(BenchLoc);
				BMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				BMC->SetCollisionResponseToAllChannels(ECR_Block);
				BMC->RegisterComponent();
				Bench->SetRootComponent(BMC);
				UMaterialInstanceDynamic* BMat = UMaterialInstanceDynamic::Create(BMC->GetMaterial(0), BMC);
				if (BMat)
				{
					BMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.30f, 0.18f, 0.08f));
					BMC->SetMaterial(0, BMat);
				}
				Bench->AddInstanceComponent(BMC);
			}
		}

		// Walking path through park (X-shaped paths).
		if (PlaneMesh)
		{
			auto SpawnPath = [&](const FVector& Loc, float LenX, float LenY)
			{
				AActor* Path = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
					FVector(Loc.X, Loc.Y, 5.f), FRotator::ZeroRotator);
				if (!Path) return;
#if WITH_EDITOR
				Path->SetActorLabel(TEXT("ParkPath"));
#endif
				UStaticMeshComponent* PMC = NewObject<UStaticMeshComponent>(Path, TEXT("PathMesh"));
				PMC->SetStaticMesh(PlaneMesh);
				PMC->SetWorldScale3D(FVector(LenX / 100.f, LenY / 100.f, 1.f));
				PMC->SetWorldLocation(FVector(Loc.X, Loc.Y, 5.f));
				PMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				PMC->SetCollisionResponseToAllChannels(ECR_Block);
				PMC->RegisterComponent();
				Path->SetRootComponent(PMC);
				UMaterialInstanceDynamic* PMat = UMaterialInstanceDynamic::Create(PMC->GetMaterial(0), PMC);
				if (PMat)
				{
					PMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.35f, 0.33f, 0.28f));
					PMC->SetMaterial(0, PMat);
				}
				Path->AddInstanceComponent(PMC);
			};

			// Cross-paths through the park.
			SpawnPath(FVector(ParkCenterX, ParkCenterY, 0.f), ParkSize * 0.9f, 200.f);
			SpawnPath(FVector(ParkCenterX, ParkCenterY, 0.f), 200.f, ParkSize * 0.9f);
		}

		// Palm trees scattered around the park.
		const FVector2D TreeOffsets[] = {
			{ 1200.f, 600.f}, {-1200.f, 600.f}, { 600.f, 1200.f}, {-600.f, -1200.f},
			{ 1500.f, -800.f}, {-1500.f, -800.f}, { 800.f, -1500.f}, {-800.f, 1500.f},
			{ 2000.f, 2000.f}, {-2000.f, 2000.f}, { 2000.f, -2000.f}, {-2000.f, -2000.f},
		};
		for (const FVector2D& Off : TreeOffsets)
		{
			SpawnPalmTree(FVector(ParkCenterX + Off.X, ParkCenterY + Off.Y, 0.f),
				CityRandom.FRandRange(300.f, 500.f));
		}
	}

	// ---- Bus stops along main avenues (every few blocks) ----
	if (CubeMesh)
	{
		const float BusStopInterval = RoadSpacing * 3.f;
		for (float Y = FirstRoad + RoadSpacing; Y < -FirstRoad; Y += BusStopInterval)
		{
			for (int32 Side = -1; Side <= 1; Side += 2)
			{
				const float BusX = Side * (RoadWidth * 0.5f + SidewalkWidth + 50.f);
				const FVector BusLoc(BusX, Y + 200.f, 0.f);

				AActor* BusStop = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
					BusLoc, FRotator::ZeroRotator);
				if (!BusStop) continue;
#if WITH_EDITOR
				BusStop->SetActorLabel(TEXT("BusStop"));
#endif

				// Shelter roof.
				UStaticMeshComponent* Roof = NewObject<UStaticMeshComponent>(BusStop, TEXT("ShelterRoof"));
				Roof->SetStaticMesh(CubeMesh);
				Roof->SetWorldScale3D(FVector(2.0f, 1.2f, 0.06f));
				Roof->SetWorldLocation(FVector(BusLoc.X, BusLoc.Y, 280.f));
				Roof->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				Roof->RegisterComponent();
				BusStop->SetRootComponent(Roof);
				UMaterialInstanceDynamic* RoofMat = UMaterialInstanceDynamic::Create(Roof->GetMaterial(0), Roof);
				if (RoofMat)
				{
					RoofMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.35f, 0.35f, 0.38f));
					Roof->SetMaterial(0, RoofMat);
				}
				BusStop->AddInstanceComponent(Roof);

				// Support pole.
				if (CylinderMesh)
				{
					UStaticMeshComponent* Pole = NewObject<UStaticMeshComponent>(BusStop, TEXT("ShelterPole"));
					Pole->SetStaticMesh(CylinderMesh);
					Pole->SetWorldScale3D(FVector(0.06f, 0.06f, 2.8f));
					Pole->SetWorldLocation(FVector(BusLoc.X - 80.f, BusLoc.Y, 140.f));
					Pole->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					Pole->RegisterComponent();
					UMaterialInstanceDynamic* PoleMat = UMaterialInstanceDynamic::Create(Pole->GetMaterial(0), Pole);
					if (PoleMat)
					{
						PoleMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.30f, 0.30f, 0.32f));
						Pole->SetMaterial(0, PoleMat);
					}
					BusStop->AddInstanceComponent(Pole);
				}

				// Bench under shelter.
				UStaticMeshComponent* BBench = NewObject<UStaticMeshComponent>(BusStop, TEXT("BusBench"));
				BBench->SetStaticMesh(CubeMesh);
				BBench->SetWorldScale3D(FVector(1.5f, 0.4f, 0.3f));
				BBench->SetWorldLocation(FVector(BusLoc.X + 30.f, BusLoc.Y, 30.f));
				BBench->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
				BBench->SetCollisionResponseToAllChannels(ECR_Block);
				BBench->RegisterComponent();
				UMaterialInstanceDynamic* BBMat = UMaterialInstanceDynamic::Create(BBench->GetMaterial(0), BBench);
				if (BBMat)
				{
					BBMat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.28f, 0.16f, 0.06f));
					BBench->SetMaterial(0, BBMat);
				}
				BusStop->AddInstanceComponent(BBench);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Street Lights
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnStreetLights()
{
	constexpr int32 NumRoads = 11;
	constexpr float RoadSpacing = 5000.f;
	constexpr float RoadWidth = 900.f;
	const float FirstRoad = -(NumRoads - 1) * RoadSpacing * 0.5f;

	int32 LightCount = 0;

	// Place 2 lights at every intersection (diagonal corners).
	for (int32 i = 0; i < NumRoads; ++i)
	{
		const float Y = FirstRoad + i * RoadSpacing;
		for (int32 j = 0; j < NumRoads; ++j)
		{
			const float X = FirstRoad + j * RoadSpacing;
			const float Off = RoadWidth * 0.5f + 300.f;
			SpawnStreetLight(FVector(X + Off, Y + Off, 0.f)); LightCount++;
			SpawnStreetLight(FVector(X - Off, Y - Off, 0.f)); LightCount++;
		}
	}

	// Midway lights only on inner avenues (every other road) to manage perf.
	for (int32 i = 2; i < NumRoads - 2; i += 2)
	{
		const float Y = FirstRoad + i * RoadSpacing;
		for (int32 j = 0; j < NumRoads - 1; ++j)
		{
			const float MidX = FirstRoad + j * RoadSpacing + RoadSpacing * 0.5f;
			const float Off = RoadWidth * 0.5f + 300.f;
			SpawnStreetLight(FVector(MidX, Y + Off, 0.f)); LightCount++;
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Spawned %d street lights."), LightCount);
}

// ---------------------------------------------------------------------------
// Palm Trees
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnPalmTrees()
{
	constexpr int32 NumRoads = 11;
	constexpr float RoadSpacing = 5000.f;
	constexpr float RoadWidth = 900.f;
	constexpr float SidewalkWidth = 250.f;
	const float FirstRoad = -(NumRoads - 1) * RoadSpacing * 0.5f;

	int32 TreeCount = 0;

	// Along sidewalks of avenues (every other road to manage density).
	for (int32 i = 0; i < NumRoads; i += 2)
	{
		const float Y = FirstRoad + i * RoadSpacing;
		const float SidewalkY = Y + RoadWidth * 0.5f + SidewalkWidth * 0.5f;

		for (float X = FirstRoad; X <= -FirstRoad; X += CityRandom.FRandRange(1200.f, 2500.f))
		{
			if (CityRandom.FRand() > 0.6f) continue;
			const float H = CityRandom.FRandRange(600.f, 1200.f);
			SpawnPalmTree(FVector(X + CityRandom.FRandRange(-50.f, 50.f), SidewalkY, 0.f), H);
			TreeCount++;
		}
	}

	// Along sidewalks of streets (every other road).
	for (int32 i = 1; i < NumRoads; i += 2)
	{
		const float X = FirstRoad + i * RoadSpacing;
		const float SidewalkX = X + RoadWidth * 0.5f + SidewalkWidth * 0.5f;

		for (float Y = FirstRoad; Y <= -FirstRoad; Y += CityRandom.FRandRange(1400.f, 2800.f))
		{
			if (CityRandom.FRand() > 0.55f) continue;
			const float H = CityRandom.FRandRange(600.f, 1200.f);
			SpawnPalmTree(FVector(SidewalkX, Y + CityRandom.FRandRange(-50.f, 50.f), 0.f), H);
			TreeCount++;
		}
	}

	// Beach palm trees -- east coast (dense double row along shoreline).
	for (float Y = -40000.f; Y <= 40000.f; Y += CityRandom.FRandRange(250.f, 600.f))
	{
		const float X = 41000.f + CityRandom.FRandRange(-300.f, 300.f);
		const float H = CityRandom.FRandRange(700.f, 1400.f);
		SpawnPalmTree(FVector(X, Y, 0.f), H);
		TreeCount++;
		// Second row set back from the shore.
		if (CityRandom.FRand() < 0.5f)
		{
			SpawnPalmTree(FVector(X - CityRandom.FRandRange(400.f, 800.f), Y + CityRandom.FRandRange(-100.f, 100.f), 0.f),
				CityRandom.FRandRange(600.f, 1200.f));
			TreeCount++;
		}
	}

	// South coast palm trees.
	for (float X = -25000.f; X <= 25000.f; X += CityRandom.FRandRange(300.f, 700.f))
	{
		const float Y = -31000.f + CityRandom.FRandRange(-300.f, 300.f);
		SpawnPalmTree(FVector(X, Y, 0.f), CityRandom.FRandRange(600.f, 1300.f));
		TreeCount++;
	}

	// North coast palm trees (west of bay).
	for (float X = -25000.f; X <= 0.f; X += CityRandom.FRandRange(350.f, 750.f))
	{
		const float Y = 23000.f + CityRandom.FRandRange(-300.f, 300.f);
		SpawnPalmTree(FVector(X, Y, 0.f), CityRandom.FRandRange(600.f, 1200.f));
		TreeCount++;
	}

	// Bay shoreline palm trees (inner edges).
	for (float X = 8000.f; X <= 25000.f; X += CityRandom.FRandRange(400.f, 800.f))
	{
		const float Y = 21500.f + CityRandom.FRandRange(-200.f, 200.f);
		SpawnPalmTree(FVector(X, Y, 0.f), CityRandom.FRandRange(500.f, 1100.f));
		TreeCount++;
	}
	for (float Y = 22000.f; Y <= 28000.f; Y += CityRandom.FRandRange(400.f, 800.f))
	{
		const float X = 7500.f + CityRandom.FRandRange(-200.f, 200.f);
		SpawnPalmTree(FVector(X, Y, 0.f), CityRandom.FRandRange(500.f, 1100.f));
		TreeCount++;
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Spawned %d palm trees."), TreeCount);
}

// ---------------------------------------------------------------------------
// Vehicles
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnVehicles()
{
	// Vehicles are now spawned on-demand via the "Car" HUD button.
	// This function is kept for future use (parked NPC cars, etc.).
}

// ---------------------------------------------------------------------------
// Launch ramps at city edges
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnRamps()
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh) return;

	// Flat launch pads at the 4 cardinal edges of the city.
	// Driving over one launches the vehicle into the sky.
	constexpr float PadSize = 900.f;

	const float EdgeDist = 26000.f;

	const TArray<FVector> PadLocations = {
		FVector( EdgeDist, 0.f, 3.f),  // east
		FVector(-EdgeDist, 0.f, 3.f),  // west
		FVector(0.f,  EdgeDist, 3.f),  // north
		FVector(0.f, -EdgeDist, 3.f),  // south
	};

	const FLinearColor PadColor(0.80f, 0.10f, 0.08f);   // bright red
	const FLinearColor StripeColor(1.0f, 0.95f, 0.15f);  // yellow warning

	for (const FVector& Loc : PadLocations)
	{
		AActor* Pad = World->SpawnActor<AActor>(AActor::StaticClass(), Loc, FRotator::ZeroRotator);
		if (!Pad) continue;
#if WITH_EDITOR
		Pad->SetActorLabel(TEXT("LaunchPad"));
#endif
		Pad->Tags.Add(TEXT("LaunchPad"));

		// Main pad surface.
		UStaticMeshComponent* PadMC = NewObject<UStaticMeshComponent>(Pad, TEXT("PadMesh"));
		PadMC->SetStaticMesh(CubeMesh);
		PadMC->SetWorldScale3D(FVector(PadSize / 100.f, PadSize / 100.f, 0.06f));
		PadMC->SetWorldLocation(Loc);
		PadMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PadMC->SetCollisionResponseToAllChannels(ECR_Block);
		PadMC->RegisterComponent();
		Pad->SetRootComponent(PadMC);

		UMaterialInstanceDynamic* PadMat = UMaterialInstanceDynamic::Create(PadMC->GetMaterial(0), PadMC);
		if (PadMat)
		{
			PadMat->SetVectorParameterValue(TEXT("BaseColor"), PadColor);
			PadMC->SetMaterial(0, PadMat);
		}
		Pad->AddInstanceComponent(PadMC);

		// Warning stripes (X-shaped cross on the pad).
		for (int32 S = 0; S < 2; ++S)
		{
			FString StripeName = FString::Printf(TEXT("Stripe_%d"), S);
			UStaticMeshComponent* Stripe = NewObject<UStaticMeshComponent>(Pad, *StripeName);
			Stripe->SetStaticMesh(CubeMesh);
			Stripe->SetWorldScale3D(FVector(PadSize * 0.9f / 100.f, 0.4f, 0.02f));
			Stripe->SetWorldLocation(FVector(Loc.X, Loc.Y, Loc.Z + 4.f));
			Stripe->SetWorldRotation(FRotator(0.f, S == 0 ? 45.f : -45.f, 0.f));
			Stripe->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Stripe->RegisterComponent();

			UMaterialInstanceDynamic* SMat = UMaterialInstanceDynamic::Create(Stripe->GetMaterial(0), Stripe);
			if (SMat)
			{
				SMat->SetVectorParameterValue(TEXT("BaseColor"), StripeColor);
				Stripe->SetMaterial(0, SMat);
			}
			Pad->AddInstanceComponent(Stripe);
		}

		// Upward arrow indicator (thin tall cube pointing up).
		UStaticMeshComponent* Arrow = NewObject<UStaticMeshComponent>(Pad, TEXT("Arrow"));
		Arrow->SetStaticMesh(CubeMesh);
		Arrow->SetWorldScale3D(FVector(0.15f, 0.15f, 2.5f));
		Arrow->SetWorldLocation(FVector(Loc.X, Loc.Y, 130.f));
		Arrow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Arrow->RegisterComponent();

		UMaterialInstanceDynamic* ArrowMat = UMaterialInstanceDynamic::Create(Arrow->GetMaterial(0), Arrow);
		if (ArrowMat)
		{
			ArrowMat->SetVectorParameterValue(TEXT("BaseColor"), StripeColor);
			Arrow->SetMaterial(0, ArrowMat);
		}
		Pad->AddInstanceComponent(Arrow);
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Spawned 4 launch pads at city edges."));
}

// ---------------------------------------------------------------------------
// SpawnHouse -- Brookhaven-style suburban house with pitched roof and lawn
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnHouse(const FVector& Loc, float W, float D, int32 Stories,
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
}

// ---------------------------------------------------------------------------
// SpawnShop -- Brookhaven-style commercial storefront with awning
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnShop(const FVector& Loc, float W, float D, int32 Stories,
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
}

// ---------------------------------------------------------------------------
// SpawnInstitution -- Brookhaven-style institutional building
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnInstitution(const FVector& Loc, float W, float D, float H,
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
}


// ---------------------------------------------------------------------------
// Building spawner (with window lights for night)
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnBuilding(const FVector& Location, const FVector& Size,
	const FLinearColor& Color, bool bAddWindows)
{
	UWorld* World = GetWorld();
	if (!World || !CubeMesh) return;

	const FVector SpawnLoc(Location.X, Location.Y, Size.Z);

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

		// Pick a random face.
		const int32 Face = CityRandom.RandRange(0, 3);
		FVector AwningLoc = Location;
		FVector AwningScale;
		switch (Face)
		{
		case 0: AwningLoc.Y += Size.Y + AwningDepth * 0.5f; AwningScale = FVector(AwningWidth / 50.f, AwningDepth / 50.f, 0.08f); break;
		case 1: AwningLoc.Y -= Size.Y + AwningDepth * 0.5f; AwningScale = FVector(AwningWidth / 50.f, AwningDepth / 50.f, 0.08f); break;
		case 2: AwningLoc.X += Size.X + AwningDepth * 0.5f; AwningScale = FVector(AwningDepth / 50.f, AwningWidth / 50.f, 0.08f); break;
		default: AwningLoc.X -= Size.X + AwningDepth * 0.5f; AwningScale = FVector(AwningDepth / 50.f, AwningWidth / 50.f, 0.08f); break;
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
		const float FullHeight = Size.Z * 2.f;
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

		struct FBandFace { float OffX, OffY; bool bAlongX; float HW; };
		const FBandFace Faces[] = {
			{  0.f,  Size.Y + 3.f, true,  Size.X },
			{  0.f, -Size.Y - 3.f, true,  Size.X },
			{  Size.X + 3.f, 0.f,  false, Size.Y },
			{ -Size.X - 3.f, 0.f,  false, Size.Y },
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
		const float FullHeight = Size.Z * 2.f;
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
		// Pick the two wider faces for more visible windows.
		if (Size.X >= Size.Y)
		{
			WFaces[NumFaces++] = {  0.f,  Size.Y + 6.f, true,  Size.X * 2.f };
			WFaces[NumFaces++] = {  0.f, -Size.Y - 6.f, true,  Size.X * 2.f };
		}
		else
		{
			WFaces[NumFaces++] = {  Size.X + 6.f, 0.f, false, Size.Y * 2.f };
			WFaces[NumFaces++] = { -Size.X - 6.f, 0.f, false, Size.Y * 2.f };
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
}

// ---------------------------------------------------------------------------
// Road strip
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnRoadStrip(const FVector& Center, float LengthX, float LengthY)
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

void APHCCityGameMode::SpawnSidewalk(const FVector& Center, float LengthX, float LengthY)
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

void APHCCityGameMode::SpawnStreetLight(const FVector& Location)
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

void APHCCityGameMode::SpawnPalmTree(const FVector& Location, float Height)
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
	constexpr int32 TrunkSegments = 3;
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
	const int32 NumFronds = CityRandom.RandRange(6, 9);
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

	// ---- Coconut cluster (small spheres at the crown base) ----
	if (SphereMesh && CityRandom.FRand() < 0.6f)
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
// Outskirts (highway + roadside content west of city)
// ---------------------------------------------------------------------------

void APHCCityGameMode::SpawnOutskirts()
{
	UWorld* World = GetWorld();
	if (!World || !PlaneMesh || !CubeMesh) return;

	// Main highway running west from city edge out to the far outskirts.
	const float HighwayStartX = -28000.f;
	const float HighwayEndX = -140000.f;
	const float HighwayY = 0.f;
	const float HighwayWidth = 1400.f;
	const float HighwayLen = FMath::Abs(HighwayEndX - HighwayStartX);
	const float HighwayCenterX = (HighwayStartX + HighwayEndX) * 0.5f;

	// Main highway road surface.
	SpawnRoadStrip(FVector(HighwayCenterX, HighwayY, 0.f), HighwayLen, HighwayWidth);

	// Center divider line (thin yellow strip).
	{
		AActor* Divider = World->SpawnActor<AActor>(AActor::StaticClass(),
			FVector(HighwayCenterX, HighwayY, 2.5f), FRotator::ZeroRotator);
		if (Divider)
		{
#if WITH_EDITOR
			Divider->SetActorLabel(TEXT("HighwayDivider"));
#endif
			UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Divider, TEXT("DividerMesh"));
			MC->SetStaticMesh(CubeMesh);
			MC->SetWorldScale3D(FVector(HighwayLen / 100.f, 0.3f, 0.05f));
			MC->SetWorldLocation(FVector(HighwayCenterX, HighwayY, 2.5f));
			MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			MC->RegisterComponent();
			Divider->SetRootComponent(MC);
			UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
			if (Mat)
			{
				Mat->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.7f, 0.6f, 0.1f));
				MC->SetMaterial(0, Mat);
			}
			Divider->AddInstanceComponent(MC);
		}
	}

	// Sidewalks along highway edges.
	SpawnSidewalk(FVector(HighwayCenterX, HighwayY + HighwayWidth * 0.5f + 150.f, 0.f), HighwayLen, 250.f);
	SpawnSidewalk(FVector(HighwayCenterX, HighwayY - HighwayWidth * 0.5f - 150.f, 0.f), HighwayLen, 250.f);

	// --- Side roads branching off the highway ---
	struct FSideRoadDef
	{
		float BranchX;
		float Direction;
		float Length;
	};

	const TArray<FSideRoadDef> SideRoads = {
		{ -45000.f,  1.f, 8000.f },
		{ -70000.f, -1.f, 10000.f },
		{ -95000.f,  1.f, 6000.f },
	};

	for (const FSideRoadDef& SR : SideRoads)
	{
		const float SideY = HighwayY + SR.Direction * (HighwayWidth * 0.5f + SR.Length * 0.5f);
		SpawnRoadStrip(FVector(SR.BranchX, SideY, 0.f), 900.f, SR.Length);
	}

	// --- Palm trees along the highway ---
	for (float X = HighwayStartX; X > HighwayEndX; X -= 3500.f)
	{
		const float OffsetY = CityRandom.FRandRange(200.f, 600.f);
		const float Height = CityRandom.FRandRange(300.f, 550.f);

		if (CityRandom.FRand() > 0.3f)
		{
			SpawnPalmTree(FVector(X + CityRandom.FRandRange(-200.f, 200.f),
				HighwayY + HighwayWidth * 0.5f + OffsetY, 0.f), Height);
		}
		if (CityRandom.FRand() > 0.3f)
		{
			SpawnPalmTree(FVector(X + CityRandom.FRandRange(-200.f, 200.f),
				HighwayY - HighwayWidth * 0.5f - OffsetY, 0.f), Height);
		}
	}

	// --- Scattered trees across the outskirts fields ---
	for (int32 i = 0; i < 60; ++i)
	{
		const float TX = CityRandom.FRandRange(HighwayEndX + 5000.f, HighwayStartX - 2000.f);
		const float TY = CityRandom.FRandRange(-35000.f, 35000.f);
		if (FMath::Abs(TY - HighwayY) < HighwayWidth + 800.f) continue;
		const float TH = CityRandom.FRandRange(250.f, 500.f);
		SpawnPalmTree(FVector(TX, TY, 0.f), TH);
	}

	// --- Small roadside structures (gas stations, warehouses, etc.) ---
	struct FOutskirtBuilding
	{
		FVector Location;
		FVector Size;
		FLinearColor Color;
	};

	const TArray<FOutskirtBuilding> Buildings = {
		{ FVector(-35000.f, 3500.f, 0.f), FVector(1200.f, 800.f, 400.f),
			FLinearColor(0.75f, 0.72f, 0.65f) },
		{ FVector(-50000.f, -4000.f, 0.f), FVector(2000.f, 1500.f, 600.f),
			FLinearColor(0.45f, 0.42f, 0.40f) },
		{ FVector(-65000.f, 3000.f, 0.f), FVector(2500.f, 600.f, 500.f),
			FLinearColor(0.60f, 0.50f, 0.35f) },
		{ FVector(-80000.f, -3500.f, 0.f), FVector(900.f, 700.f, 350.f),
			FLinearColor(0.55f, 0.20f, 0.15f) },
		{ FVector(-100000.f, 2500.f, 0.f), FVector(400.f, 400.f, 1200.f),
			FLinearColor(0.50f, 0.50f, 0.52f) },
		{ FVector(-115000.f, -5000.f, 0.f), FVector(1800.f, 1200.f, 700.f),
			FLinearColor(0.50f, 0.18f, 0.12f) },
		{ FVector(-125000.f, 3000.f, 0.f), FVector(800.f, 600.f, 350.f),
			FLinearColor(0.65f, 0.62f, 0.55f) },
	};

	for (const FOutskirtBuilding& B : Buildings)
	{
		SpawnBuilding(B.Location, B.Size, B.Color, false);
	}

	// --- Street lights along the highway (less frequent than city) ---
	for (float X = HighwayStartX; X > HighwayEndX; X -= 7000.f)
	{
		SpawnStreetLight(FVector(X, HighwayY + HighwayWidth * 0.5f + 100.f, 0.f));
		SpawnStreetLight(FVector(X, HighwayY - HighwayWidth * 0.5f - 100.f, 0.f));
	}

	// --- Mountain ranges (California coastal style) ---
	if (SphereMesh && ConeMesh)
	{
		int32 MountainPieces = 0;

		auto SpawnMountainPiece = [&](UStaticMesh* Mesh, const FVector& Center,
			const FVector& Scale, const FRotator& Rot, const FLinearColor& Color,
			const TCHAR* Label)
		{
			AActor* Piece = World->SpawnActor<AActor>(AActor::StaticClass(),
				Center, FRotator::ZeroRotator);
			if (!Piece) return;
#if WITH_EDITOR
			Piece->SetActorLabel(Label);
#endif

			UStaticMeshComponent* MC = NewObject<UStaticMeshComponent>(Piece, TEXT("Mesh"));
			MC->SetStaticMesh(Mesh);
			MC->SetWorldScale3D(Scale);
			MC->SetWorldLocation(Center);
			MC->SetWorldRotation(Rot);
			MC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			MC->SetCollisionResponseToAllChannels(ECR_Block);
			MC->RegisterComponent();
			Piece->SetRootComponent(MC);

			UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(MC->GetMaterial(0), MC);
			if (Mat)
			{
				Mat->SetVectorParameterValue(TEXT("BaseColor"), Color);
				MC->SetMaterial(0, Mat);
			}
			Piece->AddInstanceComponent(MC);
			MountainPieces++;
		};

		// NORTHERN MOUNTAIN RANGE
		SpawnMountainPiece(SphereMesh, FVector(-75000.f, 22000.f, -8000.f), FVector(1600.f, 250.f, 240.f), FRotator(0.f, 10.f, 0.f), FLinearColor(0.38f, 0.34f, 0.24f), TEXT("NorthRidge_Main"));
		SpawnMountainPiece(SphereMesh, FVector(-65000.f, 26000.f, -9000.f), FVector(1000.f, 200.f, 260.f), FRotator(0.f, -8.f, 0.f), FLinearColor(0.35f, 0.30f, 0.22f), TEXT("NorthRidge_Secondary"));
		SpawnMountainPiece(SphereMesh, FVector(-85000.f, 30000.f, -10000.f), FVector(1200.f, 180.f, 280.f), FRotator(0.f, 5.f, 0.f), FLinearColor(0.30f, 0.28f, 0.25f), TEXT("NorthRidge_Back"));
		SpawnMountainPiece(ConeMesh, FVector(-55000.f, 22000.f, 3000.f), FVector(100.f, 100.f, 80.f), FRotator::ZeroRotator, FLinearColor(0.42f, 0.38f, 0.30f), TEXT("NorthPeak_1"));
		SpawnMountainPiece(ConeMesh, FVector(-72000.f, 24000.f, 3500.f), FVector(120.f, 120.f, 100.f), FRotator::ZeroRotator, FLinearColor(0.45f, 0.40f, 0.32f), TEXT("NorthPeak_2"));
		SpawnMountainPiece(ConeMesh, FVector(-95000.f, 23000.f, 2500.f), FVector(90.f, 90.f, 70.f), FRotator::ZeroRotator, FLinearColor(0.40f, 0.36f, 0.28f), TEXT("NorthPeak_3"));
		SpawnMountainPiece(SphereMesh, FVector(-45000.f, 12000.f, -7000.f), FVector(200.f, 180.f, 160.f), FRotator::ZeroRotator, FLinearColor(0.32f, 0.38f, 0.22f), TEXT("NorthFoothill_1"));
		SpawnMountainPiece(SphereMesh, FVector(-60000.f, 10000.f, -6500.f), FVector(180.f, 160.f, 140.f), FRotator::ZeroRotator, FLinearColor(0.36f, 0.40f, 0.24f), TEXT("NorthFoothill_2"));
		SpawnMountainPiece(SphereMesh, FVector(-80000.f, 13000.f, -7500.f), FVector(220.f, 200.f, 170.f), FRotator::ZeroRotator, FLinearColor(0.34f, 0.36f, 0.20f), TEXT("NorthFoothill_3"));
		SpawnMountainPiece(SphereMesh, FVector(-105000.f, 14000.f, -6000.f), FVector(160.f, 150.f, 130.f), FRotator::ZeroRotator, FLinearColor(0.30f, 0.35f, 0.22f), TEXT("NorthFoothill_4"));

		// SOUTHERN MOUNTAIN RANGE
		SpawnMountainPiece(SphereMesh, FVector(-70000.f, -22000.f, -8500.f), FVector(1400.f, 220.f, 250.f), FRotator(0.f, -12.f, 0.f), FLinearColor(0.40f, 0.36f, 0.26f), TEXT("SouthRidge_Main"));
		SpawnMountainPiece(SphereMesh, FVector(-60000.f, -27000.f, -9500.f), FVector(900.f, 200.f, 270.f), FRotator(0.f, 6.f, 0.f), FLinearColor(0.36f, 0.32f, 0.24f), TEXT("SouthRidge_Secondary"));
		SpawnMountainPiece(SphereMesh, FVector(-90000.f, -30000.f, -10000.f), FVector(1100.f, 170.f, 260.f), FRotator(0.f, -5.f, 0.f), FLinearColor(0.32f, 0.30f, 0.27f), TEXT("SouthRidge_Back"));
		SpawnMountainPiece(ConeMesh, FVector(-58000.f, -22000.f, 3200.f), FVector(110.f, 110.f, 90.f), FRotator::ZeroRotator, FLinearColor(0.44f, 0.40f, 0.30f), TEXT("SouthPeak_1"));
		SpawnMountainPiece(ConeMesh, FVector(-82000.f, -24000.f, 2800.f), FVector(100.f, 100.f, 85.f), FRotator::ZeroRotator, FLinearColor(0.42f, 0.37f, 0.28f), TEXT("SouthPeak_2"));
		SpawnMountainPiece(ConeMesh, FVector(-105000.f, -21000.f, 2200.f), FVector(80.f, 80.f, 65.f), FRotator::ZeroRotator, FLinearColor(0.38f, 0.35f, 0.28f), TEXT("SouthPeak_3"));
		SpawnMountainPiece(SphereMesh, FVector(-48000.f, -12000.f, -7000.f), FVector(200.f, 170.f, 150.f), FRotator::ZeroRotator, FLinearColor(0.34f, 0.40f, 0.22f), TEXT("SouthFoothill_1"));
		SpawnMountainPiece(SphereMesh, FVector(-68000.f, -11000.f, -6500.f), FVector(170.f, 160.f, 140.f), FRotator::ZeroRotator, FLinearColor(0.38f, 0.42f, 0.25f), TEXT("SouthFoothill_2"));
		SpawnMountainPiece(SphereMesh, FVector(-95000.f, -13000.f, -7000.f), FVector(190.f, 180.f, 155.f), FRotator::ZeroRotator, FLinearColor(0.32f, 0.36f, 0.20f), TEXT("SouthFoothill_3"));

		// FAR WESTERN MOUNTAINS
		SpawnMountainPiece(SphereMesh, FVector(-130000.f, 5000.f, -12000.f), FVector(800.f, 600.f, 360.f), FRotator(0.f, 15.f, 0.f), FLinearColor(0.30f, 0.28f, 0.26f), TEXT("FarWest_Ridge"));
		SpawnMountainPiece(ConeMesh, FVector(-130000.f, 5000.f, 5000.f), FVector(160.f, 160.f, 140.f), FRotator::ZeroRotator, FLinearColor(0.35f, 0.32f, 0.30f), TEXT("FarWest_Peak"));
		SpawnMountainPiece(SphereMesh, FVector(-120000.f, -10000.f, -11000.f), FVector(700.f, 500.f, 320.f), FRotator(0.f, -10.f, 0.f), FLinearColor(0.33f, 0.30f, 0.25f), TEXT("FarWest_Ridge2"));

		// ROADSIDE HILLS
		SpawnMountainPiece(SphereMesh, FVector(-75000.f, 6000.f, -6000.f), FVector(160.f, 140.f, 140.f), FRotator::ZeroRotator, FLinearColor(0.36f, 0.40f, 0.22f), TEXT("Roadside_North"));
		SpawnMountainPiece(SphereMesh, FVector(-76000.f, -6000.f, -5500.f), FVector(150.f, 130.f, 130.f), FRotator::ZeroRotator, FLinearColor(0.38f, 0.36f, 0.24f), TEXT("Roadside_South"));

		// ---- Palm trees on mountain slopes ----
		const FVector2D SlopeTrees[] = {
			{-50000.f, 14000.f}, {-58000.f, 16000.f}, {-66000.f, 15000.f},
			{-75000.f, 17000.f}, {-85000.f, 16000.f}, {-95000.f, 18000.f},
			{-48000.f, 20000.f}, {-62000.f, 19000.f}, {-80000.f, 21000.f},
			{-52000.f, -14000.f}, {-60000.f, -15000.f}, {-72000.f, -14000.f},
			{-85000.f, -16000.f}, {-98000.f, -15000.f}, {-55000.f, -20000.f},
			{-68000.f, -19000.f}, {-90000.f, -20000.f},
			{-73000.f, 5000.f}, {-77000.f, -5000.f},
		};
		for (const FVector2D& TP : SlopeTrees)
		{
			FHitResult SlopeHit;
			FCollisionQueryParams SlopeTrace;
			if (World->LineTraceSingleByChannel(SlopeHit,
				FVector(TP.X, TP.Y, 15000.f), FVector(TP.X, TP.Y, -500.f),
				ECC_Visibility, SlopeTrace))
			{
				SpawnPalmTree(FVector(TP.X, TP.Y, SlopeHit.ImpactPoint.Z),
					CityRandom.FRandRange(200.f, 400.f));
			}
		}

		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Spawned %d mountain pieces."), MountainPieces);
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCCityGameMode: Outskirts spawned (highway + mountains + roadside content)."));
}
