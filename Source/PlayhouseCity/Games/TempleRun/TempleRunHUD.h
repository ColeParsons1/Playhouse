// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "Games/TempleRun/TempleRunGameMode.h"
#include "TempleRunHUD.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UBorder;
class UButton;
class USizeBox;
class ATempleRunGameMode;

// ---------------------------------------------------------------------------
// UTempleRunHUD
// ---------------------------------------------------------------------------

/**
 * HUD widget for the endless runner mini-game.
 *
 * Renders the 3-lane runner view with scrolling obstacles and coins.
 * Detects swipe gestures for lane changes, jumps, and slides.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UTempleRunHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	// -- Widget lifecycle -----------------------------------------------------

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// -- Touch / swipe input --------------------------------------------------

	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;

	// -- Visual updates -------------------------------------------------------

	/** Animate the runner image to the target lane position. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TempleRunHUD")
	void UpdateRunnerLane(int32 Lane);

	/** Play the jump animation on the runner. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TempleRunHUD")
	void ShowJumpAnimation();

	/** Play the slide animation on the runner. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TempleRunHUD")
	void ShowSlideAnimation();

	/** Spawn a visual representation of an obstacle on screen. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TempleRunHUD")
	void SpawnObstacleVisual(int32 Lane, EPHCObstacleType Type, float ZPosition);

	/** Spawn a visual coin pickup on screen. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TempleRunHUD")
	void SpawnCoinVisual(int32 Lane, float ZPosition);

	/** Remove a visual element from the screen by index. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TempleRunHUD")
	void RemoveVisual(int32 VisualIndex);

	/** Show a distance milestone marker. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TempleRunHUD")
	void ShowDistanceMarker(float Distance);

protected:
	// -- Bind widgets (game area) ---------------------------------------------

	/** Canvas for the road/runner/obstacles rendering. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> GameCanvas;

	/** The runner character widget (green block at bottom of road). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> RunnerWidget;

	/** Legacy runner image - kept for backward compatibility. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> RunnerImage;

	// -- Bind widgets (lane indicators) ---------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Lane0Indicator;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Lane1Indicator;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> Lane2Indicator;

	// -- Bind widgets (HUD text) ----------------------------------------------

	/** Distance traveled display. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DistanceText;

	/** Coins collected display. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CoinCountText;

	// -- Bind widgets (control buttons) ---------------------------------------

	/** Move runner left one lane. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LeftButton;

	/** Move runner right one lane. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RightButton;

	/** Trigger a jump action. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> JumpButton;

	/** Trigger a slide action. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> SlideButton;

	// -- Bind widgets (swipe area) --------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SwipeDetector;

	// -- Dynamic visuals ------------------------------------------------------

	/** Obstacle images currently displayed on the canvas. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> ObstacleVisuals;

	/** Coin images currently displayed on the canvas. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CoinVisuals;

	/** Lane divider lines on the temple path. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> LaneDividers;

	/** Scrolling road dash marks for visual movement effect. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> RoadDashes;

	/** Pool of obstacle widgets (reused each tick). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ObstacleWidgets;

private:
	// -- Button callbacks -----------------------------------------------------

	UFUNCTION() void OnLeftClicked();
	UFUNCTION() void OnRightClicked();
	UFUNCTION() void OnJumpClicked();
	UFUNCTION() void OnSlideClicked();

	// -- Obstacle widget pool -------------------------------------------------

	/** Get or create an obstacle widget at the given pool index. */
	UBorder* GetOrCreateObstacleWidget(int32 Index);

	/** Update all obstacle and coin visuals from game mode state. */
	void UpdateObstacleWidgets();

	/** Update the scrolling road dash animation. */
	void UpdateRoadDashes(float InDeltaTime);

	// -- Swipe detection ------------------------------------------------------

	/** Parse the swipe direction from start/end screen positions. */
	FVector2D ParseSwipeDirection(FVector2D Start, FVector2D End) const;

	/** Process a detected swipe and forward to the game mode. */
	void OnSwipeDetected(FVector2D Direction);

	/** Starting position of the current touch gesture. */
	FVector2D TouchStartPosition = FVector2D::ZeroVector;

	/** Whether a touch is currently active. */
	bool bTouchActive = false;

	/** Minimum swipe distance (in pixels) to register as a directional swipe. */
	float MinSwipeDistance = 40.f;

	/** Convert a game-distance Z value to a canvas Y position. */
	float ZToCanvasY(float ZPosition) const;

	/** Get the canvas X position for a given lane. */
	float LaneToCanvasX(int32 Lane) const;

	/** Current lane position for smooth animation. */
	float CurrentLaneX = 0.f;

	/** Target lane X for animation interpolation. */
	float TargetLaneX = 0.f;

	/** Lane interpolation speed. */
	float LaneLerpSpeed = 12.f;

	/** Runner's base Y position on canvas. */
	float RunnerBaseY = 0.f;

	/** Current vertical offset for jump/slide animations. */
	float RunnerVerticalOffset = 0.f;

	/** Accumulated road dash scroll offset. */
	float RoadDashOffset = 0.f;

	/** Cached reference to the runner game mode. */
	UPROPERTY(Transient)
	TObjectPtr<ATempleRunGameMode> RunnerMode;

	/** Track the last distance milestone shown. */
	float LastDistanceMilestone = 0.f;

	// -- Layout constants -----------------------------------------------------

	static constexpr int32 NUM_LANES = 3;
	static constexpr float ROAD_WIDTH = 300.f;
	static constexpr float RUNNER_WIDTH = 40.f;
	static constexpr float RUNNER_HEIGHT = 50.f;
	static constexpr float OBS_WIDTH = 45.f;
	static constexpr float OBS_HEIGHT = 30.f;
	static constexpr int32 NUM_ROAD_DASHES = 8;
	static constexpr float ROAD_DASH_HEIGHT = 20.f;
	static constexpr float ROAD_DASH_SPACING = 80.f;
};
