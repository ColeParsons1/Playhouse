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
class UPointLightComponent;
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

	// -- Fortnite-Quality Character (45 components) --------------------------
	// Sphere-based organic body, facial features, overlapping shapes for
	// smooth silhouette. All /Engine/BasicShapes/ primitives.

	// Head & Face
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> HeadMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> JawMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> NeckMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftEyeWhiteMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightEyeWhiteMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftPupilMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightPupilMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> NoseMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> MouthMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftEarMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightEarMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftBrowMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightBrowMesh;

	// Hair
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> HairTopMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> HairBackMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> HairSideLMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> HairSideRMesh;

	// Torso (sphere-based organic)
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> ChestMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> AbsMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> PelvisMesh;

	// Clothing details
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> CollarMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BeltMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BeltBuckleMesh;

	// Left Arm
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftShoulderMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftUpperArmMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftElbowMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftForearmMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftHandMesh;

	// Right Arm
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightShoulderMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightUpperArmMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightElbowMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightForearmMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightHandMesh;

	// Left Leg
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftThighMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftKneeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftShinMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftFootMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftToeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftSoleMesh;

	// Right Leg
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightThighMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightKneeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightShinMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightFootMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightToeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightSoleMesh;

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
	float WalkSpeed = 1788.f;

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

	// -- Jetpack --------------------------------------------------------------

	bool bJetpackEquipped = false;
	float JetpackVerticalInput = 0.f;  // +1 = ascend, -1 = descend
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackBodyMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackNozzleL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackNozzleR;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackFlameL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackFlameR;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackStrapL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackStrapR;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> JetpackGlowL;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> JetpackGlowR;

	void EquipJetpack();
	void UnequipJetpack();

	// -- Dagger weapon mesh ---------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> DaggerHandleMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> DaggerBladeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> DaggerGuardMesh;

	float DaggerStabTimer = 0.f;  // >0 = currently stabbing

	// -- Scythe weapon mesh ---------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> ScytheHandleMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> ScytheBladeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> ScytheBladeEdgeMesh;

	// -- Minigun weapon mesh --------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> MinigunBodyMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> MinigunBarrelMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> MinigunMuzzleMesh;

	// -- Bazooka (RPG) weapon mesh --------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaTubeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaRearEndMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaFrontSightMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaRearSightMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaGripMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaScopeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaMuzzleMesh;

	// -- Health ---------------------------------------------------------------
	float Health = 100.f;
	float MaxHealth = 100.f;

	// -- Kill / Death tracking ------------------------------------------------
	int32 CrabKills = 0;
	int32 Deaths = 0;

	/** >0 = screen should flash red (set by subsystem on crab contact). */
	float DamageFlashTimer = 0.f;

	// -- Weapon state ---------------------------------------------------------

	int32 CurrentWeaponType = 3; // -1=none, 0=Gun, 1=RPG, 2=Sword, 3=Scythe

	float ScytheSwingTimer = 0.f;  // >0 = currently swinging

	float MinigunSpinAngle = 0.f;
	bool bMinigunFiring = false;
	float MinigunFireTimer = 0.f;

	float BazookaFireTimer = 0.f;
	float BazookaMuzzleFlashTimer = 0.f;
	float CameraShakeTimer = 0.f;
	float CameraShakeIntensity = 0.f;

	void FireWeapon();

	// -- Spring damper for secondary body motion ----------------------------

	struct FSimpleSpring
	{
		float Pos = 0.f;
		float Vel = 0.f;
		float Update(float Target, float Stiff, float Damp, float DT)
		{
			Vel += ((Target - Pos) * Stiff - Vel * Damp) * DT;
			Pos += Vel * DT;
			return Pos;
		}
	};

	FVector PrevFrameWorldVel = FVector::ZeroVector;
	FSimpleSpring LeanPitchSpring;
	FSimpleSpring LeanRollSpring;
	FSimpleSpring LHandLagX, LHandLagZ;
	FSimpleSpring RHandLagX, RHandLagZ;
	FSimpleSpring HairLagZ;
	float IdleTimer = 0.f;

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
