// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "JengaHUD.generated.h"

class UCanvasPanel;
class UTextBlock;
class UProgressBar;
class UBorder;
class UButton;
class AJengaGameMode;
class AJengaBlock;

// ---------------------------------------------------------------------------
// UJengaHUD
// ---------------------------------------------------------------------------

/**
 * HUD widget for the Jenga mini-game.
 *
 * Provides a 3D game viewport with touch controls:
 *  - Tap a block to select it, drag to pull it out
 *  - Release to complete removal (if pulled far enough) or snap back
 *  - Camera orbit: drag on empty space to rotate view
 *  - Pinch to zoom the camera in/out
 *
 * Displays blocks removed count, tower stability meter (progress bar),
 * and contextual touch instructions.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UJengaHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// -- Input handlers ---------------------------------------------------

	virtual FReply NativeOnMouseButtonDown(
		const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(
		const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(
		const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// -- Touch handlers ---------------------------------------------------

	virtual FReply NativeOnTouchStarted(
		const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnTouchMoved(
		const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnTouchEnded(
		const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;

protected:
	// -- Bind-widgets -----------------------------------------------------

	/** 3D game view canvas. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> GameView;

	/** Canvas holding the tower block display. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> TowerCanvas;

	/** Visual block widgets (6 rows x 3 blocks = 18). */
	UPROPERTY()
	TArray<TObjectPtr<UBorder>> BlockWidgets;

	/** Highlight overlay on the selected block. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SelectedBlockHighlight;

	/** Shows number of blocks successfully removed. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BlocksRemovedText;

	/** Block count text (remaining blocks in tower). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BlockCountText;

	/** Progress bar showing tower lean (green to red). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> StabilityMeter;

	/** Label for the stability meter. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StabilityLabel;

	/** Pull left button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PullLeftButton;

	/** Pull right button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PullRightButton;

	/** Place on top button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlaceOnTopButton;

	/** Indicates whose turn it is. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TurnText;

	/** Contextual instructions (e.g. "Tap a block to select"). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TouchInstructions;

private:
	/** Refresh all HUD elements from current game state. */
	void RefreshDisplay();

	/** Attempt to find a Jenga block under the given screen position. */
	AJengaBlock* TraceBlockAtScreenPos(const FVector2D& ScreenPos) const;

	/** Compute the pull direction based on drag delta. */
	FVector ComputePullDirection(const FVector2D& DragDelta) const;

	// -- Button callbacks -------------------------------------------------

	UFUNCTION()
	void OnPullLeftClicked();

	UFUNCTION()
	void OnPullRightClicked();

	UFUNCTION()
	void OnPlaceOnTopClicked();

	/** Cached reference to the Jenga game mode. */
	UPROPERTY(Transient)
	TObjectPtr<AJengaGameMode> JengaMode;

	/** Is the player currently dragging a block? */
	bool bDraggingBlock = false;

	/** Is the player orbiting the camera? */
	bool bOrbitingCamera = false;

	/** Screen position where the current interaction started. */
	FVector2D InteractionStartPos = FVector2D::ZeroVector;

	/** Previous frame's pointer position (for delta calculation). */
	FVector2D PreviousPointerPos = FVector2D::ZeroVector;

	/** Accumulated time for wobble animation. */
	float WobbleTime = 0.f;

	/** Index of the currently selected block widget (-1 = none). */
	int32 SelectedBlockIndex = -1;

	/** Tower block dimensions. */
	static constexpr float BlockWidth = 50.f;
	static constexpr float BlockHeight = 16.f;
	static constexpr float BlockGap = 2.f;
	static constexpr int32 NumRows = 6;
	static constexpr int32 BlocksPerRow = 3;

	/** Sensitivity for camera orbit rotation (degrees per pixel). */
	static constexpr float OrbitSensitivity = 0.3f;

	/** Sensitivity for block pulling (cm per pixel). */
	static constexpr float PullSensitivity = 1.5f;
};
