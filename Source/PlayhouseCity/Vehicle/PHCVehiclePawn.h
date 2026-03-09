// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PHCVehiclePawn.generated.h"

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
 * Lamborghini Aventador S Roadster for the PlayhouseCity open world.
 *
 * Built from multiple basic-shape meshes to approximate the aggressive
 * wedge silhouette of the Aventador. No roof (roadster), low and wide,
 * with mid-engine cover, side intakes, front splitter, and rear diffuser.
 */
UCLASS()
class PLAYHOUSECITY_API APHCVehiclePawn : public APawn
{
	GENERATED_BODY()

public:
	APHCVehiclePawn();

	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Components --------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UBoxComponent> VehicleCollision;

	/** Lower car body. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	/** Low windshield frame (roadster -- no roof). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> CabinMesh;

	/** Front nose wedge (slopes down from body). */
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> FrontNoseMesh;

	/** Rear mid-engine cover (raised hump behind cockpit). */
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> RearEngineMesh;

	/** Side air intakes. */
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> SideIntakeL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> SideIntakeR;

	/** Front splitter (thin wide piece at front bottom). */
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> FrontSplitterMesh;

	/** Rear diffuser. */
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> RearDiffuserMesh;

	/** Rear spoiler lip. */
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> RearSpoilerMesh;

	/** Four wheels. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WheelFL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WheelFR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WheelRL;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> WheelRR;

	/** Headlight meshes (small bright cubes). */
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> HeadlightMeshL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> HeadlightMeshR;

	/** Taillight meshes (small red cubes). */
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> TaillightMeshL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<UStaticMeshComponent> TaillightMeshR;

	/** Headlight spotlights (only on when occupied). */
	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<USpotLightComponent> HeadlightSpotL;

	UPROPERTY(VisibleAnywhere, Category = "PHC|Vehicle")
	TObjectPtr<USpotLightComponent> HeadlightSpotR;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Vehicle")
	TObjectPtr<UCameraComponent> VehicleCamera;

	// -- Vehicle tuning ----------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float MaxSpeed = 8375.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float Acceleration = 5500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float BrakeDeceleration = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Vehicle")
	float SteeringSpeed = 150.f;

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

	/** Vertical velocity for airborne physics (launch pads / ramps). */
	float VerticalVelocity = 0.f;

	/** True when grounded on previous frame (for launch detection). */
	bool bWasGrounded = false;

	/** True after the vehicle has touched ground at least once (prevents falling on spawn). */
	bool bHasBeenGrounded = false;

	/** Set the body/cabin color (dynamic material). */
	void SetBodyColor(const FLinearColor& Color);

	/** Turn headlights on/off (spotlights + bright mesh). */
	void SetHeadlightsEnabled(bool bEnabled);

	/** Get the location where a player exits the vehicle. */
	FVector GetExitLocation() const;

	/** Server RPC: replicate throttle input from owning client to server. */
	UFUNCTION(Server, Unreliable)
	void ServerSetThrottle(float Value);

	/** Server RPC: replicate steering input from owning client to server. */
	UFUNCTION(Server, Unreliable)
	void ServerSetSteering(float Value);

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
	TObjectPtr<UMaterialInstanceDynamic> CabinMaterial;

	/** All meshes that should follow body paint color. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> BodyColoredMeshes;

	/** All meshes that are dark trim / carbon fiber. */
	UPROPERTY()
	TArray<TObjectPtr<UStaticMeshComponent>> DarkTrimMeshes;

	/** Accumulated wheel spin angle for visual rotation. */
	float WheelSpinAngle = 0.f;

	/** ClearCoat material for automotive paint. */
	UPROPERTY(Transient) TObjectPtr<UMaterial> ClearCoatMaterial;

	/** Exhaust puff pool (recycled small spheres). */
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ExhaustPuffs;
	TArray<float> ExhaustPuffAge;
	int32 NextExhaustPuff = 0;
	float ExhaustSpawnTimer = 0.f;
};
