// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "PoolHUD.generated.h"

class UProgressBar;
class UTextBlock;
class UImage;
class UHorizontalBox;
class UCanvasPanel;
class APoolGameMode;

// ---------------------------------------------------------------------------
// UPoolHUD
// ---------------------------------------------------------------------------

/**
 * Pool / billiards HUD overlay.
 * Displays power slider, turn indicator, potted-ball status for both
 * players, and foul notifications.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UPoolHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Show a temporary foul notification. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void ShowFoulIndicator(const FText& Message);

	/** Hide the foul notification. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void HideFoulIndicator();

	/** Update the power bar fill (0..1). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Pool")
	void SetPowerLevel(float Power);

protected:
	// -- Bind widgets (matched by name in UMG designer) -------------------

	/** Power slider showing shot strength. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PowerBar;

	/** "Your Turn" / "CPU Turn" label. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TurnIndicatorText;

	/** Container for player's potted-ball icons. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> PlayerPottedBox;

	/** Container for CPU's potted-ball icons. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UHorizontalBox> CPUPottedBox;

	/** Ball type label for the player (Solids / Stripes). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerBallTypeText;

	/** Ball type label for the CPU. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CPUBallTypeText;

	/** Overlay shown briefly on foul. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> FoulOverlay;

	/** Text inside the foul overlay. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> FoulText;

private:
	/** Cached game mode reference. */
	UPROPERTY(Transient)
	TObjectPtr<APoolGameMode> PoolMode;

	/** Foul auto-hide timer. */
	float FoulDisplayTimer = 0.f;
	float FoulDisplayDuration = 2.5f;
	bool bShowingFoul = false;

	/** Refresh the turn indicator and potted ball displays. */
	void RefreshDisplay();
};
