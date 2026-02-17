// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PHCRaptorPawn.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UMaterial;
class UMaterialInstanceDynamic;
struct FInputActionValue;

/**
 * F-22 Raptor fighter jet for the PlayhouseCity open world.
 *
 * Built from basic-shape meshes: stealth fuselage, swept wings, canted
 * vertical stabilizers, engine nozzles with afterburner cones. Flies in
 * full 3D with pitch control via left touch vertical or Space/Shift keys.
 */
UCLASS()
class PLAYHOUSECITY_API APHCRaptorPawn : public APawn
{
	GENERATED_BODY()

public:
	APHCRaptorPawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Components --------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UBoxComponent> VehicleCollision;

	// Fuselage
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> FuselageMain;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> FuselageNose;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> FuselageRear;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> NoseTip;

	// Cockpit
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> CockpitCanopy;

	// Wings
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WingL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WingR;

	// Horizontal stabilizers
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> HorizStabL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> HorizStabR;

	// Vertical stabilizers (canted)
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> VertStabL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> VertStabR;

	// Engine intakes
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> IntakeL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> IntakeR;

	// Engine nozzles
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> NozzleL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> NozzleR;

	// Afterburner cones
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> AfterburnerL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> AfterburnerR;

	// Navigation lights
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> NavLightL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> NavLightR;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> TailLight;

	// Camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UCameraComponent> VehicleCamera;

	// -- Vehicle tuning (fast, flight-capable) -----------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float MaxSpeed = 12000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float Acceleration = 4500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float BrakeDeceleration = 5500.f;

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

	void SetBodyColor(const FLinearColor& Color);
	void SetHeadlightsEnabled(bool bEnabled);
	FVector GetExitLocation() const;

	/** Fire a cyan-glowing rocket forward from the jet. */
	void FireRocket();

	/** Server RPC: spawn the rocket on the server so it replicates to all clients. */
	UFUNCTION(Server, Reliable)
	void ServerFireRocket();

	/** Drop a gravity bomb that detonates with a nuclear explosion on impact. */
	void DropBomb();

	/** Server RPC: spawn the bomb on the server. */
	UFUNCTION(Server, Reliable)
	void ServerDropBomb();

	/** Server RPC: replicate throttle input from owning client to server. */
	UFUNCTION(Server, Unreliable)
	void ServerSetThrottle(float Value);

	/** Server RPC: replicate steering input from owning client to server. */
	UFUNCTION(Server, Unreliable)
	void ServerSetSteering(float Value);

	/** Server RPC: replicate pitch input from owning client to server. */
	UFUNCTION(Server, Unreliable)
	void ServerSetPitch(float Value);

	// -- Enhanced Input (created at runtime) --------------------------------

	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> VehicleMappingContext;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ThrottleAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> BrakeAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> SteerAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PitchUpAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> PitchDownAction;

	UPROPERTY(Transient)
	TObjectPtr<UInputAction> ExitVehicleAction;

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

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BodyColoredMeshes;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> AfterburnerMatL;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> AfterburnerMatR;

	/** ClearCoat material for body panels. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> ClearCoatMaterial;

	/** Glass material for cockpit canopy. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> GlassMaterial;

	float FireCooldownRemaining = 0.f;
	static constexpr float FireCooldown = 0.05f;

	float BombCooldownRemaining = 0.f;
	static constexpr float BombCooldown = 3.f;
};
