// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PHCRollerBladesPawn.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UMaterialInstanceDynamic;
struct FInputActionValue;

/**
 * Inline Roller Blades for the PlayhouseCity open world.
 *
 * Built from basic-shape meshes: two boots with inline wheel frames
 * and 4 wheels per foot. The player character remains visible and
 * attached (like the dirt bike), with foot/toe/sole meshes hidden
 * and replaced by the blade boot meshes.
 *
 * Moves at 3x walk speed with smooth terrain following.
 */
UCLASS()
class PLAYHOUSECITY_API APHCRollerBladesPawn : public APawn
{
	GENERATED_BODY()

public:
	APHCRollerBladesPawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Components --------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UBoxComponent> VehicleCollision;

	// Left roller blade
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> LeftBootMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> LeftFrameMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> LeftWheel1;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> LeftWheel2;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> LeftWheel3;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> LeftWheel4;

	// Right roller blade
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> RightBootMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> RightFrameMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> RightWheel1;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> RightWheel2;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> RightWheel3;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> RightWheel4;

	// Camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UCameraComponent> VehicleCamera;

	// -- Vehicle tuning (3x walk speed, graceful rolling) ------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float MaxSpeed = 5900.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float Acceleration = 3500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float BrakeDeceleration = 3000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float SteeringSpeed = 180.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float NaturalDeceleration = 600.f;

	// -- State -------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float CurrentSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float ThrottleInput = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Vehicle")
	float SteeringInput = 0.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PHC|Vehicle")
	bool bIsOccupied = false;

	bool bIsDestroyed = false;
	float VerticalVelocity = 0.f;

	FVector GetExitLocation() const;

	// -- Enhanced Input (created at runtime) --------------------------------

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> VehicleMappingContext;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ThrottleAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BrakeAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SteerAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> ExitVehicleAction;

private:
	void CreateInputAssets();

	void HandleThrottle(const FInputActionValue& Value);
	void HandleThrottleRelease(const FInputActionValue& Value);
	void HandleBrake(const FInputActionValue& Value);
	void HandleBrakeRelease(const FInputActionValue& Value);
	void HandleSteer(const FInputActionValue& Value);
	void HandleSteerRelease(const FInputActionValue& Value);
	void HandleExitVehicle(const FInputActionValue& Value);

	float WheelSpinAngle = 0.f;

	// Terrain cache (same pattern as dirt bike)
	float CachedTerrainZ = 0.f;
	FVector2D CachedTerrainPos = FVector2D::ZeroVector;
	int32 TerrainCacheFrameCounter = 0;
	int32 WallTraceFrameCounter = 0;
};
