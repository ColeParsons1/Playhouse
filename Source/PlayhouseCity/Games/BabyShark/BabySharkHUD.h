// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "BabySharkHUD.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class ABabySharkGameMode;

// ---------------------------------------------------------------------------
// FPHCPipeVisual
// ---------------------------------------------------------------------------

/** Pair of images representing the top and bottom halves of a pipe obstacle. */
USTRUCT()
struct FPHCPipeVisual
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UImage> TopImage = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UImage> BottomImage = nullptr;
};

// ---------------------------------------------------------------------------
// UBabySharkHUD
// ---------------------------------------------------------------------------

/**
 * UMG widget for the Baby Shark (Flappy Bird) mini-game.
 *
 * Renders the shark, pipe obstacles, score, and tap-to-start overlay.
 * Tapping anywhere on the widget triggers a flap and starts the game
 * if it has not begun.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UBabySharkHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	// -- Widget lifecycle -----------------------------------------------------

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// -- Input ----------------------------------------------------------------

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;

	// -- Visual updates -------------------------------------------------------

	/** Update shark image position and rotation based on velocity. */
	UFUNCTION(BlueprintCallable, Category = "PHC|BabySharkHUD")
	void UpdateSharkVisual(float Y, float Velocity);

	/** Create top/bottom pipe images for a new obstacle. */
	UFUNCTION(BlueprintCallable, Category = "PHC|BabySharkHUD")
	void SpawnPipeVisual(float X, float GapCenter, float GapSize);

	/** Remove a pipe visual by index. */
	UFUNCTION(BlueprintCallable, Category = "PHC|BabySharkHUD")
	void RemovePipeVisual(int32 Index);

	/** Show the "Tap to Start" overlay. */
	UFUNCTION(BlueprintCallable, Category = "PHC|BabySharkHUD")
	void ShowGameStart();

	/** Update the large score text. */
	UFUNCTION(BlueprintCallable, Category = "PHC|BabySharkHUD")
	void ShowScore(int32 InScore);

	/** Play death animation: shark falls, screen flashes. */
	UFUNCTION(BlueprintCallable, Category = "PHC|BabySharkHUD")
	void AnimateDeath();

protected:
	// -- Bind widgets ---------------------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> GameCanvas;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> SharkImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreDisplay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TapToStartText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BestScoreText;

private:
	// -- Helpers --------------------------------------------------------------

	/** Handle the tap/click action: start game if needed, then flap. */
	void OnTap();

	/** Convert game-field coordinates to canvas pixel positions. */
	FVector2D FieldToCanvas(FVector2D FieldPos) const;

	/** Cached game mode. */
	UPROPERTY(Transient)
	TObjectPtr<ABabySharkGameMode> SharkMode;

	/** Active pipe visuals matching the game mode's pipe array. */
	UPROPERTY(Transient)
	TArray<FPHCPipeVisual> PipeVisuals;

	/** Timer for the death flash effect. */
	FTimerHandle DeathFlashHandle;

	/** Tracks how many pipes we have spawned visuals for (sync with mode). */
	int32 LastKnownPipeCount = 0;
};
