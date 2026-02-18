// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PHCCityCharacter.generated.h"

class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

/**
 * Third-person player character for the PlayhouseCity open world.
 *
 * Derives from APawn (NOT ACharacter) to avoid the capsule/CMC physics
 * integration that launches the actor to Z≈90000 via depenetration.
 * All movement is manual — same pattern as the vehicle pawns.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API APHCCityCharacter : public APawn
{
	GENERATED_BODY()

public:
	APHCCityCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Components -----------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Collision")
	TObjectPtr<UBoxComponent> CharacterCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// -- Blocky character mesh (Roblox Brookhaven style) ---------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Mesh")
	TObjectPtr<UStaticMeshComponent> HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Mesh")
	TObjectPtr<UStaticMeshComponent> TorsoMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Mesh")
	TObjectPtr<UStaticMeshComponent> LeftArmMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Mesh")
	TObjectPtr<UStaticMeshComponent> RightArmMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Mesh")
	TObjectPtr<UStaticMeshComponent> LeftLegMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Mesh")
	TObjectPtr<UStaticMeshComponent> RightLegMesh;

	// -- Enhanced Input assets (created at runtime) ---------------------------

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputAction> InteractAction;

	// -- Movement parameters --------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Movement")
	float WalkSpeed = 2771.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Movement")
	float SprintSpeed = 2000.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PHC|Movement")
	bool bIsSprinting = false;

	bool bPendingVehicleInteract = false;

	/** Accumulated movement direction from HandleMove (camera-relative). */
	FVector PendingMoveInput = FVector::ZeroVector;

	/** Manual vertical velocity for gravity + jump. */
	float ManualVelocityZ = 0.f;

	/** True after the character has touched ground at least once. */
	bool bHasBeenGrounded = false;

	/** True when grounded on previous frame. */
	bool bWasGrounded = false;

	// -- R6 walk animation state ----------------------------------------------

	float WalkCyclePhase = 0.f;
	float WalkAnimBlend = 0.f;

	// -- Weapon state ---------------------------------------------------------

	int32 CurrentWeaponType = -1; // -1=none, 0=Gun, 1=RPG, 2=Sword

	void FireWeapon();

	// -- Mobile touch input (written by HUD each frame, read by Tick) ------

	float TouchMoveX = 0.f;
	float TouchMoveY = 0.f;
	float TouchLookX = 0.f;
	float TouchLookY = 0.f;
	bool bHUDTouchActive = false;

protected:
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleJumpCompleted(const FInputActionValue& Value);
	void HandleSprintStarted(const FInputActionValue& Value);
	void HandleSprintCompleted(const FInputActionValue& Value);
	void HandleInteract(const FInputActionValue& Value);

private:
	void CreateInputAssets();
};
