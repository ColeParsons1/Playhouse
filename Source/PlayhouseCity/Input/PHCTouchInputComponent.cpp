// Copyright PlayhouseCity. All Rights Reserved.

#include "Input/PHCTouchInputComponent.h"
#include "PlayhouseCity.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Components/InputComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCTouchInputComponent::UPHCTouchInputComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void UPHCTouchInputComponent::BeginPlay()
{
	Super::BeginPlay();

	// Detect mobile platform.
#if PLATFORM_IOS || PLATFORM_ANDROID
	bIsMobilePlatform = true;
#else
	bIsMobilePlatform = false;
#endif

	if (!bIsMobilePlatform)
	{
		// On desktop, deactivate ticking -- we don't need touch input.
		SetComponentTickEnabled(false);
		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHCTouchInputComponent: Non-mobile platform detected. Touch input disabled."));
		return;
	}

	// Bind touch events from the player controller.
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		return;
	}

	APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController());
	if (PC)
	{
		PC->InputComponent->BindTouch(IE_Pressed, this, &UPHCTouchInputComponent::HandleTouchStarted);
		PC->InputComponent->BindTouch(IE_Repeat, this, &UPHCTouchInputComponent::HandleTouchMoved);
		PC->InputComponent->BindTouch(IE_Released, this, &UPHCTouchInputComponent::HandleTouchEnded);

		UE_LOG(LogPlayhouseCity, Log,
			TEXT("PHCTouchInputComponent: Touch input bound on mobile platform."));
	}
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void UPHCTouchInputComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                             FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Compute look delta from previous to current position.
	if (LookFingerIndex >= 0)
	{
		LookDelta = (LookCurrent - LookPrevious) * LookSensitivity;
		LookPrevious = LookCurrent;
	}
	else
	{
		LookDelta = FVector2D::ZeroVector;
	}
}

// ---------------------------------------------------------------------------
// Public getters
// ---------------------------------------------------------------------------

FVector2D UPHCTouchInputComponent::GetMovementInput() const
{
	if (MoveFingerIndex < 0)
	{
		return FVector2D::ZeroVector;
	}

	// Get viewport size for normalizing.
	FVector2D ViewportSize(1920.f, 1080.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	const float ScreenWidth = ViewportSize.X;
	const FVector2D Offset = MoveCurrent - MoveOrigin;
	const float MaxRadius = JoystickRadius * ScreenWidth;
	const float DeadZoneRadius = JoystickDeadZone * ScreenWidth;
	const float Distance = Offset.Size();

	if (Distance < DeadZoneRadius)
	{
		return FVector2D::ZeroVector;
	}

	// Normalize to -1..1 range, clamped to max radius.
	FVector2D Normalized = Offset / MaxRadius;
	// Clamp magnitude to 1.
	if (Normalized.Size() > 1.f)
	{
		Normalized = Normalized.GetSafeNormal();
	}

	// Return as (X = right/left, Y = forward/back).
	// Touch Y is inverted (screen down = positive), so negate Y for "forward".
	return FVector2D(Normalized.X, -Normalized.Y);
}

FVector2D UPHCTouchInputComponent::GetLookInput() const
{
	return LookDelta;
}

// ---------------------------------------------------------------------------
// Touch handlers
// ---------------------------------------------------------------------------

void UPHCTouchInputComponent::HandleTouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	const FVector2D ScreenPos(Location.X, Location.Y);
	const int32 Half = GetScreenHalf(ScreenPos);
	const int32 Finger = static_cast<int32>(FingerIndex);

	if (Half == 0 && MoveFingerIndex < 0)
	{
		// Left side: start movement joystick.
		MoveFingerIndex = Finger;
		MoveOrigin = ScreenPos;
		MoveCurrent = ScreenPos;
	}
	else if (Half == 1 && LookFingerIndex < 0)
	{
		// Right side: start camera look.
		LookFingerIndex = Finger;
		LookPrevious = ScreenPos;
		LookCurrent = ScreenPos;
	}
}

void UPHCTouchInputComponent::HandleTouchMoved(ETouchIndex::Type FingerIndex, FVector Location)
{
	const FVector2D ScreenPos(Location.X, Location.Y);
	const int32 Finger = static_cast<int32>(FingerIndex);

	if (Finger == MoveFingerIndex)
	{
		MoveCurrent = ScreenPos;
	}
	else if (Finger == LookFingerIndex)
	{
		LookCurrent = ScreenPos;
	}
}

void UPHCTouchInputComponent::HandleTouchEnded(ETouchIndex::Type FingerIndex, FVector Location)
{
	const int32 Finger = static_cast<int32>(FingerIndex);

	if (Finger == MoveFingerIndex)
	{
		MoveFingerIndex = -1;
		MoveOrigin = FVector2D::ZeroVector;
		MoveCurrent = FVector2D::ZeroVector;
	}
	else if (Finger == LookFingerIndex)
	{
		LookFingerIndex = -1;
		LookPrevious = FVector2D::ZeroVector;
		LookCurrent = FVector2D::ZeroVector;
		LookDelta = FVector2D::ZeroVector;
	}
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

int32 UPHCTouchInputComponent::GetScreenHalf(const FVector2D& ScreenPos) const
{
	FVector2D ViewportSize(1920.f, 1080.f);
	if (GEngine && GEngine->GameViewport)
	{
		GEngine->GameViewport->GetViewportSize(ViewportSize);
	}

	return (ScreenPos.X < ViewportSize.X * 0.5f) ? 0 : 1;
}
