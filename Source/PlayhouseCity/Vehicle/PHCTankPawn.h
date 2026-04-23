// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PHCTankPawn.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class USceneComponent;
class UInputMappingContext;
class UInputAction;
class UMaterial;
class UMaterialInstanceDynamic;
class UAudioComponent;
struct FInputActionValue;

/**
 * M60A1 Main Battle Tank for the PlayhouseCity open world.
 *
 * Built entirely from basic-shape meshes: hull with sloped glacis,
 * rounded turret, 105mm M68 cannon with bore evacuator, six road wheels
 * per side, track skirts, commander's cupola. Turret rotates independently
 * of the hull. Fires projectile shells from the main gun.
 */
UCLASS()
class PLAYHOUSECITY_API APHCTankPawn : public APawn
{
	GENERATED_BODY()

public:
	APHCTankPawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Components --------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UBoxComponent> VehicleCollision;

	// Hull
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> HullBodyMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> GlacisMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> RearPlateMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> EngineDeckMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> EngineGrilleL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> EngineGrilleR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> HullSideL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> HullSideR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FenderL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FenderR;

	// Turret pivot (all turret parts attach here for rotation)
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<USceneComponent> TurretPivot;

	// Turret
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> TurretBaseMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> TurretBustleMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> CupolaMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> CupolaHatchMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> MantletMesh;

	// Main gun
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> GunBarrelMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> BoreEvacuatorMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> MuzzleBrakeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> GunCradleMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> MuzzleFlashMesh;

	// Track assembly — left
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> TrackBodyL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> DriveSprocketL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> IdlerWheelL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TArray<TObjectPtr<UStaticMeshComponent>> RoadWheelsL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TArray<TObjectPtr<UStaticMeshComponent>> ReturnRollersL;

	// Track assembly — right
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> TrackBodyR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> DriveSprocketR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> IdlerWheelR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TArray<TObjectPtr<UStaticMeshComponent>> RoadWheelsR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TArray<TObjectPtr<UStaticMeshComponent>> ReturnRollersR;

	// Details
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> HeadlightMeshL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> HeadlightMeshR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> AntennaMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> ToolStowageMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> SmokeLauncherL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> SmokeLauncherR;

	// Lights
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<USpotLightComponent> HeadlightSpotL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<USpotLightComponent> HeadlightSpotR;

	// Camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UCameraComponent> VehicleCamera;

	// -- Vehicle tuning (slow, heavy tank) ---------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float MaxSpeed = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float Acceleration = 1800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float BrakeDeceleration = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float SteeringSpeed = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float NaturalDeceleration = 600.f;

	float TurretRotSpeed = 60.f;

	// -- State -------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float CurrentSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float ThrottleInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float SteeringInput = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PHC|Vehicle")
	bool bIsOccupied = false;

	float VehicleHealth = 240.f;
	bool bIsDestroyed = false;
	float DamageFlashTimer = 0.f;

	float VerticalVelocity = 0.f;
	bool bWasGrounded = false;
	bool bHasBeenGrounded = false;

	// Turret
	float TurretYaw = 0.f;
	float TurretYawInput = 0.f;

	// Cannon
	float FireCooldownRemaining = 0.f;
	float MuzzleFlashTimer = 0.f;
	float RecoilOffset = 0.f;

	static constexpr float FireCooldown = 2.0f;

	void SetBodyColor(const FLinearColor& Color);
	void SetHeadlightsEnabled(bool bEnabled);
	FVector GetExitLocation() const;

	void FireCannon();

	UFUNCTION(Server, Reliable)
	void ServerFireCannon();

	UFUNCTION(Server, Unreliable)
	void ServerSetThrottle(float Value);

	UFUNCTION(Server, Unreliable)
	void ServerSetSteering(float Value);

	UFUNCTION(Server, Unreliable)
	void ServerSetTurretYaw(float Value);

	// -- Enhanced Input (created at runtime) --------------------------------

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> VehicleMappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BrakeAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SteerAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ExitVehicleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> FireCannonAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> TurretLookAction;

private:
	void CreateInputAssets();

	void HandleThrottle(const FInputActionValue& Value);
	void HandleThrottleRelease(const FInputActionValue& Value);
	void HandleBrake(const FInputActionValue& Value);
	void HandleBrakeRelease(const FInputActionValue& Value);
	void HandleSteer(const FInputActionValue& Value);
	void HandleSteerRelease(const FInputActionValue& Value);
	void HandleExitVehicle(const FInputActionValue& Value);
	void HandleFireCannon(const FInputActionValue& Value);
	void HandleTurretLook(const FInputActionValue& Value);

	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
	UPROPERTY() TObjectPtr<UMaterial> ClearCoatMaterial;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> OliveDrabMeshes;

	float WheelSpinAngle = 0.f;

	/** 3D-attached looping engine sound (volume/pitch modulated by speed). */
	UPROPERTY() TObjectPtr<UAudioComponent> EngineAC;
};
