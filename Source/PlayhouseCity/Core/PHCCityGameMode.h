// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PHCCityGameMode.generated.h"

class APHCVehiclePawn;
class UMaterial;
class UDirectionalLightComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;

/**
 * Game mode for the PlayhouseCity open-world city level.
 *
 * Procedurally generates a Florida-coastline night city with:
 * - Moonlit sky, street lights, building window glow
 * - Ocean and beach on the east side
 * - Palm trees along streets and shoreline
 * - Parked vehicles throughout the city
 * - Grid road network with sidewalks
 */
UCLASS()
class PLAYHOUSECITY_API APHCCityGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APHCCityGameMode();

	/** All spawned vehicles in the city (for interaction queries). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|City")
	TArray<TObjectPtr<APHCVehiclePawn>> SpawnedVehicles;

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

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

	/** Spawn a road strip (flat plane slightly above ground). */
	void SpawnRoadStrip(const FVector& Center, float LengthX, float LengthY);

	/** Spawn a sidewalk strip. */
	void SpawnSidewalk(const FVector& Center, float LengthX, float LengthY);

	/** Spawn a street light pole with point light. */
	void SpawnStreetLight(const FVector& Location);

	/** Spawn a palm tree (trunk + canopy). */
	void SpawnPalmTree(const FVector& Location, float Height);

	/** Cached meshes loaded once. */
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CubeMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> PlaneMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMesh> ConeMesh;

	/** Base material with an exposed "BaseColor" vector parameter. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> ColorMaterial;

	/** Cached lighting components for day/night cycle. */
	UPROPERTY(Transient) TObjectPtr<UDirectionalLightComponent> SunLightComp;
	UPROPERTY(Transient) TObjectPtr<USkyLightComponent> SkyLightComp;
	UPROPERTY(Transient) TObjectPtr<UExponentialHeightFogComponent> FogComp;

	/** Normalized time in the day/night cycle (0 = sunrise, 0.25 = noon, 0.5 = sunset, 0.75 = midnight). */
	float DayNightTime = 0.f;

	/** Night-lit window materials and their warm glow colors. */
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> NightWindowMaterials;
	TArray<FLinearColor> NightWindowLitColors;
	TArray<bool> NightWindowLitStates;
	float NightWindowBrightness = 0.f;
	float WindowToggleTimer = 0.f;
	float SkyRecaptureTimer = 0.f;

	/** Deterministic random stream for city generation. */
	FRandomStream CityRandom;
};
