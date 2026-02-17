// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TankPawn.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class ATankProjectile;

// ---------------------------------------------------------------------------
// ATankPawn
// ---------------------------------------------------------------------------

/**
 * Top-down tank pawn with separate body and turret rotation.
 * Supports both player input and CPU AI behaviour.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ATankPawn : public APawn
{
	GENERATED_BODY()

public:
	ATankPawn();

	virtual void Tick(float DeltaTime) override;

	// -- Movement ---------------------------------------------------------

	/** Move the tank body forward/backward (-1..1). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Tanks")
	void MoveForward(float Value);

	/** Rotate the tank body (-1..1). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Tanks")
	void Turn(float Value);

	/** Aim the turret toward a world-space 2D target. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Tanks")
	void AimTurret(FVector2D WorldTarget);

	/** Fire a projectile from the turret. Returns null if on cooldown. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Tanks")
	ATankProjectile* Fire();

	// -- Damage -----------------------------------------------------------

	/** Apply damage and check death. Overrides AActor::TakeDamage. */
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
							 AController* EventInstigator, AActor* DamageCauser) override;

	/** Delegate broadcast when this tank is destroyed. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTankDestroyed, ATankPawn*, Tank);

	UPROPERTY(BlueprintAssignable, Category = "PHC|Tanks")
	FOnTankDestroyed OnTankDestroyed;

	// -- CPU AI -----------------------------------------------------------

	/** Run simple AI logic each frame (called from Tick when bIsCPU). */
	void CPUThink(float DT);

	/** Set the target for the CPU to pursue. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Tanks")
	void SetCPUTarget(AActor* InTarget);

	// -- Components -------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Tanks")
	TObjectPtr<UBoxComponent> Body;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Tanks")
	TObjectPtr<UStaticMeshComponent> TankMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Tanks")
	TObjectPtr<UStaticMeshComponent> TurretMesh;

	// -- Properties -------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Tanks")
	float MoveSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Tanks")
	float RotationSpeed = 120.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Tanks")
	float TurretRotationSpeed = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Tanks")
	int32 HP = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Tanks")
	int32 MaxHP = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Tanks")
	float FireCooldown = 1.2f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Tanks")
	float FireTimer = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Tanks")
	bool bIsCPU = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Tanks")
	TSubclassOf<ATankProjectile> ProjectileClass;

private:
	/** Muzzle offset from turret pivot. */
	FVector GetMuzzleLocation() const;

	/** CPU AI state. */
	UPROPERTY(Transient)
	TObjectPtr<AActor> CPUTarget;

	float CPUFireAccumulator = 0.f;
	float CPUDirectionChangeTimer = 0.f;
	float CPUCurrentMoveDir = 1.f;
};
