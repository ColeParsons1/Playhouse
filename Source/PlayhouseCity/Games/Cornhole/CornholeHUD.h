// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "CornholeHUD.generated.h"

class UCanvasPanel;
class UTextBlock;
class UProgressBar;
class UBorder;
class UButton;
class ACornholeGameMode;

// ---------------------------------------------------------------------------
// UCornholeHUD
// ---------------------------------------------------------------------------

/**
 * In-game HUD widget for the Cornhole mini-game.
 *
 * Displays player / CPU scores, round number, turn indicator, bags
 * remaining, and a power bar.  Supports slingshot-style aiming via
 * mouse drag or touch input.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UCornholeHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// -- Input: mouse -----------------------------------------------------

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// -- Input: touch -----------------------------------------------------

	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;

protected:
	// -- Bind-widgets -----------------------------------------------------

	/** Full game field canvas (green grass background). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> GameCanvas;

	/** The cornhole board display (angled rectangle). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BoardWidget;

	/** The hole on the board. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> HoleWidget;

	/** The bean bag widget. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BagWidget;

	/** Aim left button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AimLeftButton;

	/** Aim right button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AimRightButton;

	/** Power up button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PowerUpButton;

	/** Power down button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PowerDownButton;

	/** Throw button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ThrowButton;

	/** Player score text. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerScoreText;

	/** CPU score text. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CPUScoreText;

	/** Round counter text. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> RoundText;

	/** Turn indicator text. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TurnText;

	/** Bags remaining text. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BagsRemainingText;

	/** Power bar (also updated during slingshot aiming). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PowerBar;

	/** Power level label. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PowerLabelText;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACornholeGameMode> CornholeMode;

	// -- Aiming state -----------------------------------------------------

	bool bAiming = false;
	FVector2D AimStartPos = FVector2D::ZeroVector;
	FVector2D AimCurrentPos = FVector2D::ZeroVector;

	/** Button-driven aim offset (-1.0 to 1.0, left to right). */
	float AimOffset = 0.f;

	/** Button-driven power level (0.0 to 1.0). */
	float PowerLevel = 0.5f;

	/** True when the bag is in flight (for animation). */
	bool bBagInFlight = false;

	/** Flight animation elapsed time. */
	float FlightTime = 0.f;

	/** Bag start position for flight animation. */
	FVector2D BagStartPos = FVector2D::ZeroVector;

	/** Bag target position for flight animation. */
	FVector2D BagTargetPos = FVector2D::ZeroVector;

	/** Maximum drag distance (pixels) used to normalise power. */
	static constexpr float MaxDragDistance = 300.f;

	/** Flight animation duration (seconds). */
	static constexpr float FlightDuration = 0.8f;

	// -- Button callbacks -------------------------------------------------

	UFUNCTION()
	void OnAimLeftClicked();

	UFUNCTION()
	void OnAimRightClicked();

	UFUNCTION()
	void OnPowerUpClicked();

	UFUNCTION()
	void OnPowerDownClicked();

	UFUNCTION()
	void OnThrowClicked();

	// -- Helpers ----------------------------------------------------------

	void ProcessAimRelease();
	FVector2D GetThrowDirection() const;
	float GetThrowPower() const;
};
