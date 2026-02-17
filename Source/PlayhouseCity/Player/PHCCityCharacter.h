// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "PHCCityCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UPHCTouchInputComponent;
struct FInputActionValue;

/**
 * First-person player character for the PlayhouseCity open world.
 *
 * Provides a first-person camera at eye height, WASD + mouse look on desktop,
 * and virtual joystick input on mobile via UPHCTouchInputComponent.
 * Uses the built-in UCharacterMovementComponent for walk, sprint, and jump.
 * No visible character model -- purely first-person view.
 *
 * Enhanced Input assets (UInputAction, UInputMappingContext) are created
 * programmatically in BeginPlay -- no .uasset files required.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API APHCCityCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	APHCCityCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Components -----------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UPHCTouchInputComponent> TouchInput;

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
	float WalkSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Movement")
	float SprintSpeed = 600.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PHC|Movement")
	bool bIsSprinting = false;

	/** Set by HandleInteract to defer vehicle entry to next Tick (avoids
	 *  crash from changing possession during Enhanced Input evaluation). */
	bool bPendingVehicleInteract = false;

protected:
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleJumpCompleted(const FInputActionValue& Value);
	void HandleSprintStarted(const FInputActionValue& Value);
	void HandleSprintCompleted(const FInputActionValue& Value);
	void HandleInteract(const FInputActionValue& Value);

private:
	/** Creates all UInputAction and UInputMappingContext objects in code. */
	void CreateInputAssets();
};
