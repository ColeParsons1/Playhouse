// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "GooBridgeHUD.generated.h"

class UCanvasPanel;
class UTextBlock;
class UButton;
class UBorder;
class USizeBox;
class UImage;
class AGooBridgeGameMode;

// ---------------------------------------------------------------------------
// UGooBridgeHUD
// ---------------------------------------------------------------------------

/**
 * HUD widget for the Goo Bridge mini-game.
 *
 * Displays the remaining goo count, current level, simulate / reset
 * controls, and handles touch input for placing goo nodes.  While
 * dragging, visual feedback shows potential connections to nearby nodes.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UGooBridgeHUD : public UPHCMiniGameHUD
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
	// -- Bind-widgets (matched by name in UMG designer) -------------------

	/** Main game canvas where goo nodes are placed. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> GameCanvas;

	/** Build area canvas that sits inside the game field. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> BuildAreaCanvas;

	/** Sky background for the landscape scene. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SkyBackground;

	/** Left ground platform. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> LeftGround;

	/** Right ground platform. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> RightGround;

	/** The gap / chasm between the two ground platforms. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> GapDisplay;

	/** Brown goo node widgets placed by the player. */
	UPROPERTY(meta = (BindWidgetOptional))
	TArray<TObjectPtr<UBorder>> NodeWidgets;

	/** Gray strut widgets connecting nodes. */
	UPROPERTY(meta = (BindWidgetOptional))
	TArray<TObjectPtr<UBorder>> StrutWidgets;

	/** Red vehicle widget on the left side waiting to cross. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> VehicleWidget;

	/** Text showing remaining goo nodes to place. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GooCountText;

	/** Displays the current build mode / status. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BuildModeText;

	/** Place Node tool button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PlaceNodeButton;

	/** Connect tool button (connect two nodes with a strut). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ConnectButton;

	/** Delete tool button (remove a node). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DeleteButton;

	/** GO button - starts the vehicle crossing simulation. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> GoButton;

	/** Toggles simulation on/off. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SimulateButton;

	/** Resets the current level (removes all player-placed nodes). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ResetButton;

	/** Displays the current level number. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	/** Nodes remaining counter text. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> NodesRemainingText;

private:
	// -- Button callbacks -------------------------------------------------

	UFUNCTION()
	void OnSimulateClicked();

	UFUNCTION()
	void OnResetClicked();

	UFUNCTION()
	void OnPlaceNodeClicked();

	UFUNCTION()
	void OnConnectClicked();

	UFUNCTION()
	void OnDeleteClicked();

	UFUNCTION()
	void OnGoClicked();

	// -- Helpers ----------------------------------------------------------

	/** Refresh all HUD text elements to match current game state. */
	void RefreshDisplay();

	/** Convert a screen-space position to a game-world 2D position. */
	FVector2D ScreenToGamePosition(const FVector2D& ScreenPos) const;

	/** Cached reference to the Goo Bridge game mode. */
	UPROPERTY(Transient)
	TObjectPtr<AGooBridgeGameMode> GooBridgeMode;

	/** Is the player currently dragging to place a node? */
	bool bIsDragging = false;

	/** Screen position where the current drag started. */
	FVector2D DragStartPos = FVector2D::ZeroVector;

	/** Current drag position (screen space). */
	FVector2D CurrentDragPos = FVector2D::ZeroVector;

	// -- Build mode state -------------------------------------------------

	/** Current build tool mode. */
	enum class EBuildTool : uint8
	{
		PlaceNode,
		Connect,
		Delete
	};

	EBuildTool CurrentTool = EBuildTool::PlaceNode;

	// -- Vehicle crossing animation state ---------------------------------

	/** Current vehicle X position in the build area for animation. */
	float VehiclePosX = 0.f;

	/** True while the vehicle is crossing the bridge. */
	bool bVehicleCrossing = false;

	/** Vehicle crossing speed (pixels/sec). */
	static constexpr float VehicleSpeed = 120.f;

	/** Starting X position for vehicle. */
	static constexpr float VehicleStartX = 30.f;

	/** Target X position for vehicle (right side). */
	static constexpr float VehicleTargetX = 700.f;
};
