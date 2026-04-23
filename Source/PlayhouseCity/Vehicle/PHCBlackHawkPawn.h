// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PHCBlackHawkPawn.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UMaterial;
class UMaterialInstanceDynamic;
class USpotLightComponent;
class UAudioComponent;
struct FInputActionValue;

/**
 * UH-60 Black Hawk military helicopter for the PlayhouseCity open world.
 *
 * Built from basic-shape meshes: fuselage, tail boom, 4-blade main rotor,
 * 2-blade tail rotor, landing skids, engine housings, cockpit glass.
 * Stealth gray finish matching the F-22 Raptor.
 * Flight model identical to Raptor at 50% speed.
 */
UCLASS()
class PLAYHOUSECITY_API APHCBlackHawkPawn : public APawn
{
	GENERATED_BODY()

public:
	APHCBlackHawkPawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Components --------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UBoxComponent> VehicleCollision;

	// Fuselage
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FuselageMain;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FuselageBelly;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FuselageRear;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> TailBoom;

	// Engine housings
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> EngineHousingL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> EngineHousingR;

	// Main rotor (4 blades + hub)
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> MainRotorHub;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> MainBlade0;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> MainBlade1;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> MainBlade2;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> MainBlade3;

	// Tail rotor (2 blades + hub)
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> TailRotorHub;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> TailBlade0;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> TailBlade1;

	// Tail stabilizers
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> VerticalStab;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> HorizStabL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> HorizStabR;

	// Landing skids
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> SkidL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> SkidR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> StrutFL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> StrutFR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> StrutRL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> StrutRR;

	// Cockpit
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> CockpitGlass;

	// Doors
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> DoorL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> DoorR;

	// Navigation lights
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> NavLightL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> NavLightR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> TailLight;

	// Headlight spotlights
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<USpotLightComponent> HeadlightSpotL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<USpotLightComponent> HeadlightSpotR;

	// Camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UCameraComponent> VehicleCamera;

	// -- Vehicle tuning (50% of F-22 Raptor) ------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float MaxSpeed = 7175.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float Acceleration = 3250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float BrakeDeceleration = 2750.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float SteeringSpeed = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float NaturalDeceleration = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float PitchSpeed = 80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float MaxPitchAngle = 60.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float AutoLevelSpeed = 2.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float MinAltitude = 50.f;

	// -- State -------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float CurrentSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float ThrottleInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float SteeringInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float PitchInput = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PHC|Vehicle")
	bool bIsOccupied = false;

	// -- Damage system --
	float VehicleHealth = 240.f;
	bool bIsCrashing = false;
	float CrashTimer = 0.f;
	float DamageFlashTimer = 0.f;

	void SetBodyColor(const FLinearColor& Color);
	void SetHeadlightsEnabled(bool bEnabled);
	FVector GetExitLocation() const;

	/** Fire a rocket forward. */
	void FireRocket();

	UFUNCTION(Server, Reliable)
	void ServerFireRocket();

	/** Drop a gravity bomb. */
	void DropBomb();

	UFUNCTION(Server, Reliable)
	void ServerDropBomb();

	UFUNCTION(Server, Unreliable)
	void ServerSetThrottle(float Value);

	UFUNCTION(Server, Unreliable)
	void ServerSetSteering(float Value);

	UFUNCTION(Server, Unreliable)
	void ServerSetPitch(float Value);

	// -- Enhanced Input (created at runtime) --------------------------------

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> VehicleMappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BrakeAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SteerAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PitchUpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> PitchDownAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ExitVehicleAction;

private:
	void CreateInputAssets();

	void HandleThrottle(const FInputActionValue& Value);
	void HandleThrottleRelease(const FInputActionValue& Value);
	void HandleBrake(const FInputActionValue& Value);
	void HandleBrakeRelease(const FInputActionValue& Value);
	void HandleSteer(const FInputActionValue& Value);
	void HandleSteerRelease(const FInputActionValue& Value);
	void HandlePitchUp(const FInputActionValue& Value);
	void HandlePitchUpRelease(const FInputActionValue& Value);
	void HandlePitchDown(const FInputActionValue& Value);
	void HandlePitchDownRelease(const FInputActionValue& Value);
	void HandleExitVehicle(const FInputActionValue& Value);

	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
public:
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BodyColoredMeshes;
private:

	/** ClearCoat material for stealth gray body panels. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> ClearCoatMaterial;

	/** Glass material for cockpit. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> GlassMaterial;

	// Rotor animation state
	float MainRotorAngle = 0.f;
	float TailRotorAngle = 0.f;
	float MainRotorSpeed = 0.f; // current deg/s, interps to target

	float FireCooldownRemaining = 0.f;
	static constexpr float FireCooldown = 0.05f;

	float BombCooldownRemaining = 0.f;
	static constexpr float BombCooldown = 3.f;

	/** 3D-attached looping rotor sound (volume/pitch modulated by speed). */
	UPROPERTY() TObjectPtr<UAudioComponent> EngineAC;
};
