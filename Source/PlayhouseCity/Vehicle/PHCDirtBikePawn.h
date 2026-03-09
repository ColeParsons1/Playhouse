// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PHCDirtBikePawn.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class USpotLightComponent;
class UInputMappingContext;
class UInputAction;
class UMaterial;
class UMaterialInstanceDynamic;
struct FInputActionValue;

/**
 * Dirt Bike for the PlayhouseCity open world.
 *
 * Built from basic-shape meshes: frame, engine block, front fork,
 * two wheels, seat with visible rider figure, exhaust pipe, handlebars.
 * Controls identical to ground vehicles (throttle, brake, steer).
 */
UCLASS()
class PLAYHOUSECITY_API APHCDirtBikePawn : public APawn
{
	GENERATED_BODY()

public:
	APHCDirtBikePawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Components --------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UBoxComponent> VehicleCollision;

	// Frame
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FrameMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FrameDownTube;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> SwingArm;

	// Engine
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> EngineBlock;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> ExhaustPipe;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> ExhaustTip;

	// Front fork
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> ForkLegL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> ForkLegR;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> ForkBridge;

	// Handlebars
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> HandlebarMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> GripL;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> GripR;

	// Wheels
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WheelFront;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WheelRear;

	// Fenders
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FrontFender;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> RearFender;

	// Seat
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> SeatMesh;

	// Fuel tank
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> FuelTankMesh;

	// Headlight
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> HeadlightMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<USpotLightComponent> HeadlightSpot;

	// Taillight
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle") TObjectPtr<UStaticMeshComponent> TaillightMesh;

	// Camera
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UCameraComponent> VehicleCamera;

	// -- Vehicle tuning (agile, medium speed) ------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float MaxSpeed = 9344.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float Acceleration = 6469.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float BrakeDeceleration = 4500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float SteeringSpeed = 200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float NaturalDeceleration = 400.f;

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
	float PrevSurfaceZ = 0.f;
	bool bWasGrounded = false;
	bool bHasBeenGrounded = false;

	void SetBodyColor(const FLinearColor& Color);
	void SetHeadlightsEnabled(bool bEnabled);
	FVector GetExitLocation() const;

	/** Server RPC: replicate throttle input from owning client to server. */
	UFUNCTION(Server, Unreliable)
	void ServerSetThrottle(float Value);

	/** Server RPC: replicate steering input from owning client to server. */
	UFUNCTION(Server, Unreliable)
	void ServerSetSteering(float Value);

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

	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> BodyMaterial;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BodyColoredMeshes;

	float WheelSpinAngle = 0.f;

	// PERF-OPT: Cached terrain height to avoid 15 Perlin calls every frame.
	// Remove this block and restore direct GetTerrainHeight call in Tick to rollback.
	float CachedTerrainZ = 0.f;
	FVector2D CachedTerrainPos = FVector2D::ZeroVector;
	int32 TerrainCacheFrameCounter = 0;
	int32 WallTraceFrameCounter = 0;

	/** ClearCoat material for paint. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> ClearCoatMaterial;
};
