// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "PongHUD.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UTextBlock;
class APongGameMode;

// ---------------------------------------------------------------------------
// UPongHUD
// ---------------------------------------------------------------------------

/**
 * UMG widget that visualises the Pong game state.
 *
 * All game elements (ball, paddles) are UImage widgets positioned within
 * a UCanvasPanel. Positions are driven each tick from APongGameMode.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UPongHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	// -- Widget lifecycle -----------------------------------------------------

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// -- Input ----------------------------------------------------------------

	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;

	// -- Visual updates -------------------------------------------------------

	/** Position ball and paddles from game-mode coordinates. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PongHUD")
	void UpdatePositions(FVector2D BallPos, float PlayerY, float CPUY);

	/** Brief visual flash when a goal is scored. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PongHUD")
	void ShowGoalScored(bool bPlayerScored);

	/** Display the final match result. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PongHUD")
	void ShowMatchResult(bool bPlayerWon);

protected:
	// -- Bind widgets ---------------------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> GameCanvas;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerScoreText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CPUScoreText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BallImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PlayerPaddleImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CPUPaddleImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ServingText;

private:
	// -- Helpers --------------------------------------------------------------

	/** Convert game-field coordinates into canvas-local pixel positions. */
	FVector2D FieldToCanvas(FVector2D FieldPos) const;

	/** Convert a mouse/touch Y in screen space to game-field Y. */
	float ScreenYToFieldY(const FGeometry& Geometry, float ScreenY) const;

	/** Cached reference to the Pong game mode. */
	UPROPERTY(Transient)
	TObjectPtr<APongGameMode> PongMode;

	/** Timer handle for clearing the goal-scored flash. */
	FTimerHandle GoalFlashHandle;
};
