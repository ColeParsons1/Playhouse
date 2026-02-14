// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "PackCatchGameMode.h"
#include "PackCatchHUD.generated.h"

class UCanvasPanel;
class UImage;
class UHorizontalBox;
class UTextBlock;
class APackCatchGameMode;

// ---------------------------------------------------------------------------
// UPackCatchHUD
// ---------------------------------------------------------------------------

/**
 * UMG widget for the Pack Catch mini-game.
 *
 * Renders the catcher, falling items, lives display, and combo text
 * within a UCanvasPanel. Positions are updated each tick from the
 * game mode's data arrays.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UPackCatchHUD : public UPHCMiniGameHUD
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

	/** Update the catcher position on screen. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PackCatchHUD")
	void UpdateCatcherPosition(float X);

	/** Show catch effect (sparkle for good, explosion for bomb). */
	UFUNCTION(BlueprintCallable, Category = "PHC|PackCatchHUD")
	void ShowCatchEffect(FVector2D Position, bool bGoodCatch);

	/** Update the hearts display. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PackCatchHUD")
	void UpdateLives(int32 CurrentLives);

	/** Show combo text popup. */
	UFUNCTION(BlueprintCallable, Category = "PHC|PackCatchHUD")
	void ShowCombo(int32 ComboCount);

protected:
	// -- Bind widgets ---------------------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> GameCanvas;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> CatcherImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> LivesDisplay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ComboText;

	// -- Dynamic visuals ------------------------------------------------------

	/** Images for currently falling items, keyed by item ID. */
	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UImage>> FallingItemImages;

private:
	// -- Helpers --------------------------------------------------------------

	/** Spawn a visual UImage for a new falling item. */
	UImage* SpawnItemVisual(const FPHCFallingItem& Item);

	/** Remove the visual for an item by its ID. */
	void RemoveItemVisual(int32 ItemId);

	/** Convert game-field coordinates to canvas pixel positions. */
	FVector2D FieldToCanvas(FVector2D FieldPos) const;

	/** Convert screen X to game-field X. */
	float ScreenXToFieldX(const FGeometry& Geometry, float ScreenX) const;

	/** Get a color for a package type. */
	FLinearColor GetPackageColor(EPHCPackageType Type) const;

	// -- Delegate handlers (bound to game mode events) ------------------------

	/** Called when an item is caught by the player. */
	UFUNCTION()
	void HandleItemCaught(int32 ItemId, bool bGoodCatch);

	/** Called when an item is missed (falls off screen). */
	UFUNCTION()
	void HandleItemMissed(int32 ItemId);

	/** Called when a new falling item is spawned. */
	UFUNCTION()
	void HandleItemSpawned(const FPHCFallingItem& Item);

	/** Cached game mode reference. */
	UPROPERTY(Transient)
	TObjectPtr<APackCatchGameMode> CatchMode;

	/** Heart icon images for the lives display. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> HeartImages;

	/** Timer for clearing combo text. */
	FTimerHandle ComboFadeHandle;

	/** Maximum lives for initializing heart display. */
	int32 MaxLivesDisplay = 3;
};
