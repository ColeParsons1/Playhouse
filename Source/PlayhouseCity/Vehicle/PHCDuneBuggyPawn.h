// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PHCDuneBuggyPawn.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class UInputMappingContext;
class UInputAction;
class UMaterialInstanceDynamic;
struct FInputActionValue;

/**
 * Off-road dune buggy for the PlayhouseCity open world.
 *
 * Built from basic-shape meshes: high ride, open roll cage, wide off-road
 * wheels, brush guard, and spare tire. Slower than the Aventador but more
 * agile steering.
 */
UCLASS()
class PLAYHOUSECITY_API APHCDuneBuggyPawn : public APawn
{
	GENERATED_BODY()

public:
	APHCDuneBuggyPawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Components --------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UBoxComponent> VehicleCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> ChassisMesh;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> FloorPanMesh;

	// Roll cage bars
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> RollCageTopL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> RollCageTopR;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> RollCageTopCross;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> RollCageRearL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> RollCageRearR;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> RollCageRearCross;

	// Front brush guard
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> BrushGuardTop;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> BrushGuardL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> BrushGuardR;

	// Rear spare tire
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> SpareTireMesh;

	// Rear engine block
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> EngineBlockMesh;

	// Seat
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> SeatMesh;

	// Wheels
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WheelFL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WheelFR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WheelRL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WheelRR;

	// Lights
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> HeadlightMeshL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> HeadlightMeshR;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> TaillightMeshL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> TaillightMeshR;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<USpotLightComponent> HeadlightSpotL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<USpotLightComponent> HeadlightSpotR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UCameraComponent> VehicleCamera;

	// -- Vehicle tuning (15% slower, more agile) ----------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float MaxSpeed = 5100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float Acceleration = 3200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float BrakeDeceleration = 4000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float SteeringSpeed = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float NaturalDeceleration = 350.f;

	// -- State -------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float CurrentSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float ThrottleInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float SteeringInput = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PHC|Vehicle")
	bool bIsOccupied = false;

	float LaunchVelocityZ = 0.f;

	void SetBodyColor(const FLinearColor& Color);
	void SetHeadlightsEnabled(bool bEnabled);
	FVector GetExitLocation() const;

	/** Fire a missile projectile forward from the buggy. */
	void FireMissile();

	/** Server RPC: spawn the missile on the server so it replicates to all clients. */
	UFUNCTION(Server, Reliable)
	void ServerFireMissile();

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
	TObjectPtr<UInputAction> ExitVehicleAction;

private:
	void CreateInputAssets();

	void HandleThrottle(const FInputActionValue& Value);
	void HandleThrottleRelease(const FInputActionValue& Value);
	void HandleBrake(const FInputActionValue& Value);
	void HandleBrakeRelease(const FInputActionValue& Value);
	void HandleSteer(const FInputActionValue& Value);
	void HandleSteerRelease(const FInputActionValue& Value);
	void HandleExitVehicle(const FInputActionValue& Value);

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BodyColoredMeshes;

	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> MetalFrameMeshes;

	float WheelSpinAngle = 0.f;

	float FireCooldownRemaining = 0.f;
	static constexpr float FireCooldown = 0.5f;
};
