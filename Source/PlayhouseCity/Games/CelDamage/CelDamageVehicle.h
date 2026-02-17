// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CelDamageVehicle.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UCameraComponent;
class ACelDamageProjectile;

/** Weapon types available in Cel Damage. */
UENUM(BlueprintType)
enum class ECelDamageWeaponType : uint8
{
	Fists       UMETA(DisplayName = "Fists"),
	Bat         UMETA(DisplayName = "Baseball Bat"),
	FreezeRay   UMETA(DisplayName = "Freeze Ray"),
	Cannon      UMETA(DisplayName = "Cannon"),
	TNT         UMETA(DisplayName = "TNT"),
	Homing      UMETA(DisplayName = "Homing Missile")
};

/**
 * Simplified combat vehicle for the Cel Damage mini-game.
 * Supports both player and CPU AI via the bIsCPU flag.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ACelDamageVehicle : public APawn
{
	GENERATED_BODY()

public:
	ACelDamageVehicle();

	virtual void Tick(float DeltaTime) override;

	// -- Movement -----------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PHC|CelDamage")
	void SetThrottle(float Value);

	UFUNCTION(BlueprintCallable, Category = "PHC|CelDamage")
	void SetSteering(float Value);

	// -- Combat -------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PHC|CelDamage")
	void FireWeapon();

	UFUNCTION(BlueprintCallable, Category = "PHC|CelDamage")
	void EquipWeapon(ECelDamageWeaponType NewWeapon);

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
							 AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintCallable, Category = "PHC|CelDamage")
	void Respawn(const FVector& SpawnLocation);

	// -- Delegates ----------------------------------------------------------

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVehicleHit, ACelDamageVehicle*, Attacker, ACelDamageVehicle*, Victim);

	UPROPERTY(BlueprintAssignable, Category = "PHC|CelDamage")
	FOnVehicleHit OnVehicleHit;

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVehicleDestroyed, ACelDamageVehicle*, Vehicle);

	UPROPERTY(BlueprintAssignable, Category = "PHC|CelDamage")
	FOnVehicleDestroyed OnVehicleDestroyed;

	// -- CPU AI -------------------------------------------------------------

	void CPUThink(float DT);

	UFUNCTION(BlueprintCallable, Category = "PHC|CelDamage")
	void SetCPUTarget(AActor* InTarget);

	void SetNearestPickup(AActor* InPickup);

	// -- Components ---------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UBoxComponent> VehicleCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UStaticMeshComponent> WheelFL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UStaticMeshComponent> WheelFR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UStaticMeshComponent> WheelRL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UStaticMeshComponent> WheelRR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|CelDamage")
	TObjectPtr<UCameraComponent> VehicleCamera;

	// -- Vehicle tuning -----------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	float MaxSpeed = 2500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	float AccelerationRate = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	float BrakeRate = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	float NaturalDecel = 800.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	float SteeringSpeed = 180.f;

	// -- Health state -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	float Health = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	float MaxHealth = 100.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	bool bIsInvincible = false;

	// -- Weapon state -------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	ECelDamageWeaponType CurrentWeapon = ECelDamageWeaponType::Fists;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	float FireCooldown = 0.8f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	float FireTimer = 0.f;

	// -- Movement state -----------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	float CurrentSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	float ThrottleInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|CelDamage")
	float SteeringInput = 0.f;

	// -- Configuration ------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	bool bIsCPU = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|CelDamage")
	FLinearColor VehicleColor = FLinearColor(0.2f, 0.7f, 0.2f, 1.f);

private:
	void UpdateMovement(float DT);
	void UpdateWheelSpin(float DT);
	void UpdateWeaponMeshColor();
	FVector GetMuzzleLocation() const;

	/** Arena bounds for clamping (set by game mode). */
	FVector ArenaCenter = FVector(100000.f, 100000.f, 0.f);
	float ArenaHalfSize = 3800.f;

	float InvincibilityTimer = 0.f;
	static constexpr float InvincibilityDuration = 2.5f;

	float WheelSpinAngle = 0.f;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CPUTarget;

	UPROPERTY(Transient)
	TObjectPtr<AActor> CPUPickupTarget;

	enum class EAIState : uint8 { Roaming, SeekingPickup, Attacking, Fleeing };
	EAIState AIState = EAIState::Roaming;
	float AIStateTimer = 0.f;
	float AIRoamYaw = 0.f;
	float CPUFireAccumulator = 0.f;
};
