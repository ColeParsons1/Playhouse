// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PHCCitySubsystem.generated.h"

class UMaterial;
class UMaterialInstanceDynamic;
class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class USpotLightComponent;
class APostProcessVolume;

/** City district types for zone-based procedural generation. */
enum class ECityDistrict : uint8
{
	DowntownCore,
	Midtown,
	Residential,
	Waterfront,
	Industrial,
	Park,
	ArtsDistrict,
	Edge
};

/**
 * World subsystem that handles all procedural city generation and day/night cycle.
 *
 * Runs on ALL machines (server and clients) in multiplayer, so each machine
 * independently generates an identical city from the deterministic seed.
 * No city data is replicated over the network.
 */
UCLASS()
class PLAYHOUSECITY_API UPHCCitySubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// -- USubsystem interface -------------------------------------------------
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	void SpawnCityEnvironment();
	void SpawnGround();
	void SpawnOceanAndBeach();
	void SpawnSkyAndLighting();
	void SpawnCityGrid();
	void SpawnStreetLights();
	void SpawnPalmTrees();
	void SpawnVehicles();
	void SpawnRamps();
	void SpawnOutskirts();
	void SpawnSkyscrapers();
	void SpawnBlockStreets();
	void SpawnStarfield();
	void SpawnHills();
	void SpawnCloudLayer();
	void SpawnUndergroundCavern();
	void SpawnMountainRange();
	void SpawnSandCastle();
	void SpawnLakeAndPirateShip();

	/** Spawn a single building with optional window lights. */
	void SpawnBuilding(const FVector& Location, const FVector& Size,
		const FLinearColor& Color, bool bAddWindows = true);

	/** Spawn a suburban house with pitched roof and lawn. */
	void SpawnHouse(const FVector& Loc, float W, float D, int32 Stories,
		const FLinearColor& WallColor, const FLinearColor& RoofColor);

	/** Spawn a commercial storefront with awning. */
	void SpawnShop(const FVector& Loc, float W, float D, int32 Stories,
		const FLinearColor& WallColor, const FLinearColor& AwningColor);

	/** Spawn an institutional building (hospital, police, school, etc.). */
	void SpawnInstitution(const FVector& Loc, float W, float D, float H,
		const FLinearColor& WallColor, const FLinearColor& AccentColor);

	/** Spawn an industrial warehouse (large footprint, no windows). */
	void SpawnWarehouse(const FVector& Loc, float W, float D, float H,
		const FLinearColor& Color);

	/** Spawn a park block with trees, paths, and optional fountain. */
	void SpawnParkBlock(const FVector& ParkCenter, float ParkSize, bool bAddFountain);

	/** Spawn a road strip (flat plane slightly above ground). */
	void SpawnRoadStrip(const FVector& Center, float LengthX, float LengthY);

	/** Spawn a sidewalk strip. */
	void SpawnSidewalk(const FVector& Center, float LengthX, float LengthY);

	/** Spawn a street light pole with point light. */
	void SpawnStreetLight(const FVector& Location);

	/** Spawn a palm tree (trunk + canopy). bDetailed=false uses fewer components. */
	void SpawnPalmTree(const FVector& Location, float Height, bool bDetailed = true);

	/** Cached meshes loaded once. */
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> PlaneMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> ConeMesh;

	/** Base material with an exposed "BaseColor" vector parameter. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> ColorMaterial;

	/** Translucent glass/water material with BaseColor, Opacity, Metallic, Roughness, EmissiveColor. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> GlassMaterial;

	/** Cached lighting components for day/night cycle. */
	UPROPERTY(Transient) TObjectPtr<UDirectionalLightComponent> SunLightComp;
	UPROPERTY(Transient) TObjectPtr<USkyLightComponent> SkyLightComp;
	UPROPERTY(Transient) TObjectPtr<UExponentialHeightFogComponent> FogComp;
	UPROPERTY(Transient) TObjectPtr<UDirectionalLightComponent> MoonLightComp;
	UPROPERTY(Transient) TObjectPtr<AActor> StarfieldActor;

	/** Global unbound post-process volume for cinematic look. */
	UPROPERTY(Transient) TObjectPtr<APostProcessVolume> PostProcessVolume;

	/** Normalized time in the day/night cycle (0 = sunrise, 0.25 = noon, 0.5 = sunset, 0.75 = midnight). */
	float DayNightTime = 0.f;

	/** Night-lit window materials and their warm glow colors. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> NightWindowMaterials;
	TArray<FLinearColor> NightWindowLitColors;
	TArray<bool> NightWindowLitStates;
	float NightWindowBrightness = 0.f;
	float WindowToggleTimer = 0.f;
	float SkyRecaptureTimer = 0.f;

	/** Cloud layer actors for parallax drift. */
	UPROPERTY(Transient) TObjectPtr<AActor> CloudLayerActor;
	UPROPERTY(Transient) TObjectPtr<AActor> CloudLayerActor2;

	/** Ocean actor reference for wave animation. */
	UPROPERTY(Transient) TObjectPtr<AActor> OceanActor;
	float WaterTime = 0.f;

	/** Road materials for wet road effect at night. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> RoadMaterials;
	float GlobalWetness = 0.f;

	/** Weather state. */
	float WeatherTimer = 0.f;
	float WeatherTransitionAlpha = 1.f;
	bool bIsRaining = false;

	/** Deterministic random stream for city generation. */
	FRandomStream CityRandom;

	/** Separate random stream for building detail elements (avoids disturbing CityRandom). */
	FRandomStream DetailRandom;

	/** Whether the city has already been generated on this machine. */
	bool bCityGenerated = false;
};
