// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PHCBlackbirdPawn.generated.h"

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
 * SR-71 Blackbird reconnaissance jet for the PlayhouseCity open world.
 *
 * Built from basic-shape meshes: long slender fuselage with chines,
 * twin nacelle engines on delta wings, canted inward vertical stabilizers,
 * and iconic nose cone. Gold-plated finish. Double the F-22's speed
 * and acceleration.
 */
UCLASS()
class PLAYHOUSECITY_API APHCBlackbirdPawn : public APawn
{
	GENERATED_BODY()

public:
	APHCBlackbirdPawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Components --------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UBoxComponent> VehicleCollision;

	// Fuselage
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FuselageMain;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FuselageSpine;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FuselageBelly;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> NoseCone;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> NoseSection;

	// Chines (fuselage strakes)
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> ChineL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> ChineR;

	// Wings (delta blended with nacelles)
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> WingL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> WingR;

	// Engine nacelles (large cylindrical pods)
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> NacelleL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> NacelleR;

	// Nacelle spike cones (inlet shock cones)
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> InletSpikeL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> InletSpikeR;

	// Engine nozzles
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> NozzleL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> NozzleR;

	// Afterburner cones
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> AfterburnerL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> AfterburnerR;

	// Vertical stabilizers (canted inward — SR-71 signature)
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> VertStabL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> VertStabR;

	// Cockpit
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> CockpitCanopy;

	// Navigation lights
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> NavLightL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> NavLightR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> TailLight;

	// Camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UCameraComponent> VehicleCamera;

	// -- Vehicle tuning (double the F-22 Raptor) --------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float MaxSpeed = 28700.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float Acceleration = 13000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float BrakeDeceleration = 8000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float SteeringSpeed = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float NaturalDeceleration = 400.f;

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
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BodyColoredMeshes;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> AfterburnerMatL;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> AfterburnerMatR;

	/** ClearCoat material for gold body panels. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> ClearCoatMaterial;

	/** Glass material for cockpit canopy. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> GlassMaterial;

	float FireCooldownRemaining = 0.f;
	static constexpr float FireCooldown = 0.05f;

	float BombCooldownRemaining = 0.f;
	static constexpr float BombCooldown = 3.f;
};
