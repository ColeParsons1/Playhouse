// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PHCCitySubsystem.generated.h"

class UMaterial;
class UMaterialInstanceDynamic;
class UHierarchicalInstancedStaticMeshComponent;
class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class USpotLightComponent;
class UPointLightComponent;
class APostProcessVolume;
class APHCBlackbirdPawn;
class APHCBlackHawkPawn;
class UProceduralMeshComponent;
class UDecalComponent;

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

	/** SR-71 Blackbird spawned in the underground airport hangar. */
	UPROPERTY(Transient) TObjectPtr<APHCBlackbirdPawn> AirportBlackbird;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

	/** Whether the city has already been generated on this machine. */
	bool bCityGenerated = false;

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
	void SpawnGiantCrab();
	void SpawnMountainRange();
	void SpawnSandCastle();
	void SpawnLakeAndPirateShip();
	void SpawnStreetLabels();
	void SpawnLangtree();
	void SpawnTerrainGrid();
	void SpawnGrassSystem();
	void SpawnDirtBikePark();
	void SpawnGoldCoins();
	void SpawnJungle();
	void SpawnOakGrove();
	void SpawnVisualPolish();
	void UpdateGrassInstances(const FVector& PlayerLoc);

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

	/** Post-process material for custom-colored chromatic aberration during trip. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> ChromaAberrationMaterial;

	// -- Procedural normal/roughness map textures (generated at startup) ------
	UPROPERTY(Transient) TObjectPtr<UTexture2D> GrassNormalMap;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> BrickNormalMap;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> ConcreteNormalMap;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> AsphaltNormalMap;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> SandNormalMap;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> GrassRoughnessMap;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> BrickRoughnessMap;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> ConcreteRoughnessMap;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> AsphaltRoughnessMap;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> SandRoughnessMap;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> FlatNormalMap;
	UPROPERTY(Transient) TObjectPtr<UTexture2D> WhiteRoughnessMap;

	void GenerateProceduralTextures();
	UTexture2D* CreateProceduralTexture(int32 W, int32 H, const TFunction<FColor(int32,int32)>& PixelFunc, bool bNormalMap = false);

	/** Cached lighting components for day/night cycle. */
	UPROPERTY(Transient) TObjectPtr<UDirectionalLightComponent> SunLightComp;
	UPROPERTY(Transient) TObjectPtr<USkyLightComponent> SkyLightComp;
	UPROPERTY(Transient) TObjectPtr<UExponentialHeightFogComponent> FogComp;
	UPROPERTY(Transient) TObjectPtr<UDirectionalLightComponent> MoonLightComp;
	UPROPERTY(Transient) TObjectPtr<AActor> StarfieldActor;

	/** Global unbound post-process volume for cinematic look. */
	UPROPERTY(Transient) TObjectPtr<APostProcessVolume> PostProcessVolume;

public:
	/** Normalized time in the day/night cycle (0 = sunrise, 0.25 = noon, 0.5 = sunset, 0.75 = midnight). */
	float DayNightTime = 0.25f;

	/** When true, DayNightTime is frozen (Night mode from HUD button). */
	bool bNightModeLocked = false;

	/** Mushroom psychedelic trip effect. */
	float MushroomTripTimer = -1.f;          // <0 = inactive, >=0 = elapsed seconds
	float MushroomTripDuration = 240.f;      // base 4 minutes, +20s per re-dose
	int32 TripDoseCount = 0;                 // how many mushrooms collected this trip
	float TripIntensityMultiplier = 1.f;     // grows with re-doses
	float TripBaseFOV = 90.f;                // cached camera FOV before trip
	FRotator TripLastCameraRot = FRotator::ZeroRotator;  // for fixation detection
	float TripFixationAlpha = 0.f;           // 0=moving, 1=staring (ramps up/down)
	float TripIntensity = 0.f;               // current computed intensity (0-1), readable by HUD

	/** World-space hallucination — ground shadow tiles + mandala. */
	UPROPERTY(Transient) TObjectPtr<AActor> TripHallucinationActor;

	/** Ground overlay — DMT chrysanthemum pattern using multiple HISMs. */
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TripTileHISM;   // Cone petals
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TripBarHISM;    // Cube lattice bars
	UPROPERTY(Transient) TObjectPtr<UHierarchicalInstancedStaticMeshComponent> TripDotHISM;    // Cylinder nodes
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TripTileMID;

	/** Deferred decal projection — projects DMT pattern onto walls/terrain. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> DecalMaterial;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TripDecalMID;
	UPROPERTY(Transient) TArray<TObjectPtr<UDecalComponent>> TripDecals;

	/** Subtle underglow point lights for floral pattern. */
	UPROPERTY(Transient) TArray<TObjectPtr<UPointLightComponent>> TripGlowLights;

	/** Custom chromatic aberration post-process material (mandala-colored fringing). */
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TripChromaMID;

	/** Fractal sky dome — replaces sky during trip. */
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> TripSkyDome;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> TripSkyMID;
	UPROPERTY(Transient) TObjectPtr<UMaterial> TripSkyMaterial;

	/** Water surfaces — stored for trip sky pattern swap. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> WaterMIDs;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> WaterOriginalMIDs;
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> RiverTripMID;
	bool bWaterTripping = false;

	bool bTripHallucinationsSpawned = false;

	/** Natural disaster system. */
	enum class EDisasterType : uint8 { None, Tornado, Earthquake, Thunderstorm, MeteorShower };
	EDisasterType ActiveDisaster = EDisasterType::None;
	float DisasterTimer = 0.f;
	float DisasterCooldown = 180.f;

private:
	/** Night-lit window materials and their warm glow colors. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> NightWindowMaterials;
	TArray<FLinearColor> NightWindowLitColors;
	TArray<bool> NightWindowLitStates;
	float NightWindowBrightness = 0.f;
	float WindowToggleTimer = 0.f;
	float SkyRecaptureTimer = 0.f;

public:
	/** NPC enemies — always visible, chase the player. */
	static constexpr int32 NumGiantCrabs = 4;

	struct FCrabData
	{
		TObjectPtr<AActor> Actor;
		TArray<TObjectPtr<USceneComponent>> HipJoints;
		TArray<FRotator> HipBaseRot;
		TArray<TObjectPtr<USceneComponent>> KneeJoints;
		TArray<FRotator> KneeBaseRot;
		float AnimTime = 0.f;
		bool bVisible = false;
		float Health = 100.f;
		float ContactCooldown = 0.f;
		float DamageFlashTimer = 0.f;
		FVector SpawnPosition = FVector::ZeroVector;
		TObjectPtr<UPointLightComponent> GlowLight;
		float GlowBaseIntensity = 4000.f;
		TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMIDs;
	};

	FCrabData Crabs[NumGiantCrabs];
	bool bCrabsNightVisible = false;
	int32 CrabsAlive = NumGiantCrabs;
	float WaveRestTimer = -1.f;  // >=0 means resting between waves
	static constexpr float WaveRestDuration = 60.f; // 60 second rest after clearing all NPCs

	/** Respawn a killed NPC at its original spawn point (if night is active). */
	void RespawnCrab(int32 Index);

	/** Collectible gold coins scattered across the map. */
	static constexpr int32 NumGoldCoins = 14;

	struct FGoldCoinData
	{
		TObjectPtr<AActor> Actor;
		FVector OriginalSpawnPos = FVector::ZeroVector;
		FVector CurrentPos = FVector::ZeroVector;
		float SpinAngle = 0.f;
		float RespawnTimer = -1.f;       // <0 = active, >=0 = waiting to respawn
		float PickupAnimTimer = -1.f;    // >=0 = shrinking animation active
	};

	FGoldCoinData GoldCoins[NumGoldCoins];

	/** Collectible mushrooms in the jungle (original 12 + decorative clusters). */
	struct FMushroomData
	{
		TObjectPtr<AActor> Actor;
		TObjectPtr<UPointLightComponent> GlowLight;
		FVector OriginalSpawnPos = FVector::ZeroVector;
		FVector CurrentPos = FVector::ZeroVector;
		float PickupAnimTimer = -1.f;
		float RespawnTimer = -1.f;
		float BobPhase = 0.f;
	};
	TArray<FMushroomData> CollectibleMushrooms;
	void SpawnCollectibleMushrooms();

	/** NPC player characters — wander city during daytime, damageable. */
	static constexpr int32 NumNPCPlayers = 15;

	struct FNPCPlayerData
	{
		TObjectPtr<AActor> Actor;
		// Body meshes needing per-frame animation
		TObjectPtr<UStaticMeshComponent> HeadMesh, JawMesh, NeckMesh;
		TObjectPtr<UStaticMeshComponent> ChestMesh, AbsMesh, PelvisMesh;
		TObjectPtr<UStaticMeshComponent> CollarMesh, BeltMesh, BeltBuckleMesh;
		TObjectPtr<UStaticMeshComponent> LShoulderMesh, LUpperArmMesh, LElbowMesh, LForearmMesh, LHandMesh;
		TObjectPtr<UStaticMeshComponent> RShoulderMesh, RUpperArmMesh, RElbowMesh, RForearmMesh, RHandMesh;
		TObjectPtr<UStaticMeshComponent> LThighMesh, LKneeMesh, LShinMesh, LFootMesh, LSoleMesh;
		TObjectPtr<UStaticMeshComponent> RThighMesh, RKneeMesh, RShinMesh, RFootMesh, RSoleMesh;
		TObjectPtr<UStaticMeshComponent> HairTopMesh, HairBackMesh;
		TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMIDs;
		TObjectPtr<UPointLightComponent> GlowLight;
		float Health = 100.f;
		float DamageFlashTimer = 0.f;
		bool bVisible = false;
		FVector SpawnPosition = FVector::ZeroVector;
		float AnimTime = 0.f;
		float WalkAnimBlend = 0.f;
		FVector WanderTarget = FVector::ZeroVector;
		float WanderWaitTimer = 0.f;
	};

	FNPCPlayerData NPCPlayers[NumNPCPlayers];
	bool bNPCPlayersVisible = false;

	void SpawnNPCPlayers();
	void RespawnNPCPlayer(int32 Index);

private:

	static bool IsInCrabForbiddenZone(const FVector& P)
	{
		// Safe zone 1: Sand castle area (around 95000, 30000)
		if (FVector::Dist2D(P, FVector(95000.f, 30000.f, 0.f)) < 20000.f) return true;
		// Safe zone 2: Underground airport (X 90000-210000, Y -20000 to 80000)
		if (P.X > 90000.f && P.X < 210000.f && P.Y > -20000.f && P.Y < 80000.f) return true;
		return false;
	}

public:
	/** NPC patrol helicopter (flies during daytime, damageable). */
	void SpawnNPCHelicopter();
	void OnNPCHeliDestroyed();
	void RespawnNPCHelicopter();

	UPROPERTY(Transient) TObjectPtr<APHCBlackHawkPawn> NPCHelicopter;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> NPCHeliBodyMIDs;
	float NPCHeliPatrolAngle = 0.f;
	bool bNPCHeliVisible = false;
	float NPCHeliHealth = 240.f;
	float NPCHeliDamageFlashTimer = 0.f;
	bool bNPCHeliCrashing = false;
	float NPCHeliCrashTimer = 0.f;
	bool bNPCHeli1Down = false;

	/** Second NPC patrol helicopter (opposite side of city). */
	void OnNPCHeli2Destroyed();

	UPROPERTY(Transient) TObjectPtr<APHCBlackHawkPawn> NPCHelicopter2;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> NPCHeliBodyMIDs2;

	// Liquid glass arrow indicators above NPC helicopters
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> NPCHeliArrow;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> NPCHeliArrow2;

	float NPCHeliPatrolAngle2 = UE_PI; // start on opposite side
	bool bNPCHeliVisible2 = false;
	float NPCHeliHealth2 = 240.f;
	float NPCHeliDamageFlashTimer2 = 0.f;
	bool bNPCHeliCrashing2 = false;
	float NPCHeliCrashTimer2 = 0.f;
	bool bNPCHeli2Down = false;

	/** Both helis must be down before the pair respawns together. */
	float NPCHeliPairRespawnTimer = -1.f;
	bool bNPCHeliPermanentlyDestroyed = false;
	bool bAirportBlackbirdDestroyed = false;

	/** Save/load destroyed vehicle state to persist across sessions. */
	void SaveDestroyedVehicleState();
	void LoadDestroyedVehicleState();

private:

	/** Cloud layer actors for parallax drift. */
	UPROPERTY(Transient) TObjectPtr<AActor> CloudLayerActor;
	UPROPERTY(Transient) TObjectPtr<AActor> CloudLayerActor2;
	UPROPERTY(Transient) TObjectPtr<AActor> CloudLayerActor3;

	/** Atmospheric dust motes near camera. */
	UPROPERTY(Transient) TObjectPtr<AActor> DustMoteActor;
	TArray<TObjectPtr<UStaticMeshComponent>> DustMotes;

	/** Ocean actor reference for wave animation. */

	/** River water animated mesh. */
	UPROPERTY(Transient) TObjectPtr<UProceduralMeshComponent> RiverWaterPMC;
	TArray<FVector> RiverWaterBaseVerts;
	TArray<int32> RiverWaterTris;
	TArray<FVector2D> RiverWaterUVs;

	/** Cloud MIDs for day/night color change. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> CloudMIDs;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> CloudActors;

	/** Torch flame components for animation. */
	struct FTorchFlameData
	{
		TObjectPtr<UStaticMeshComponent> Core;
		TObjectPtr<UStaticMeshComponent> Mid;
		TObjectPtr<UStaticMeshComponent> Outer;
		TObjectPtr<UStaticMeshComponent> Tip;
		FVector BasePos;  // base world position of flame
		float Phase;      // random phase offset
	};
	TArray<FTorchFlameData> TorchFlames;

	/** Ground slab MID for trip color morphing. */
	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> GroundSlabMID;

	/** Road materials for wet road effect at night. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> RoadMaterials;
	float GlobalWetness = 0.f;

	/** Khufu pyramid interior — animated psychedelic emissive MIDs. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> PyramidInteriorMIDs;
	float PyramidColorTime = 0.f;

	/** Weather state. */
	float WeatherTimer = 0.f;
	float WeatherTransitionAlpha = 1.f;
	bool bIsRaining = false;

	/** Tornado. */
	void SpawnTornado();
	UPROPERTY(Transient) TObjectPtr<AActor> TornadoActor;
	FVector TornadoPos = FVector::ZeroVector;
	float TornadoAngle = 0.f;
	float TornadoSpinTime = 0.f;

	/** Earthquake. */
	float EarthquakeIntensity = 0.f;
	float EarthquakePhase = 0.f;

	/** Thunderstorm. */
	float ThunderTimer = 0.f;
	float LightningFlashAlpha = 0.f;

	/** Meteor shower. */
	float MeteorSpawnTimer = 0.f;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> ActiveMeteors;
	TArray<FVector> MeteorVelocities;

	/** Procedural terrain actor. */
	UPROPERTY(Transient) TObjectPtr<AActor> TerrainActor;

	/** Grass instancing system (4 HISM color variants). */
	UPROPERTY(Transient) TObjectPtr<AActor> GrassActor;
	UPROPERTY(Transient) TArray<TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> GrassHISMs;
	FVector LastGrassUpdateLocation = FVector(1e9f, 1e9f, 0.f);
	bool IsOnRoadOrSidewalk(float X, float Y) const;

public:
	/** Get terrain height at world XY. Flat inside city, undulating outside. */
	float GetTerrainHeight(float X, float Y) const;

private:

	/** Simple 2D Perlin noise. */
	static float PerlinNoise2D(float X, float Y);

	/** Deterministic random stream for city generation. */
	FRandomStream CityRandom;

	/** Separate random stream for building detail elements (avoids disturbing CityRandom). */
	FRandomStream DetailRandom;

};
