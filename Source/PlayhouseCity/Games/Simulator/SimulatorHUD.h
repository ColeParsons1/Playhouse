// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "SimulatorHUD.generated.h"

class UCanvasPanel;
class UTextBlock;
class UButton;
class UBorder;
class ASimulatorGameMode;

/**
 * HUD for the Playhouse City driving game.
 *
 * Displays a top-down 3-lane road with the player car at the bottom,
 * obstacles scrolling down, lane-change buttons, speed/distance/coins display.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API USimulatorHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

protected:
	/** Canvas for the road/car/obstacles rendering. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> RoadCanvas;

	/** The player car visual. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PlayerCarWidget;

	/** Speed display. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SpeedText;

	/** Distance display. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DistanceText;

	/** Coins display. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CoinsText;

	/** Left lane change button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LeftButton;

	/** Right lane change button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> RightButton;

private:
	UPROPERTY(Transient)
	TObjectPtr<ASimulatorGameMode> DrivingMode;

	/** Road lane divider lines. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> LaneDividers;

	/** Road dashed center lines for scrolling effect. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> RoadDashes;

	/** Pool of obstacle widgets (reused). */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> ObstacleWidgets;

	UFUNCTION() void OnLeftClicked();
	UFUNCTION() void OnRightClicked();

	UFUNCTION() void HandleCrash();
	UFUNCTION() void HandleCoinCollected(int32 TotalCoins);

	void UpdatePlayerPosition();
	void UpdateObstacleWidgets();
	UBorder* GetOrCreateObstacleWidget(int32 Index);

	float GetLaneX(int32 Lane) const;

	static constexpr int32 NUM_LANES = 3;
	static constexpr float ROAD_WIDTH = 300.f;
	static constexpr float CAR_WIDTH = 50.f;
	static constexpr float CAR_HEIGHT = 80.f;
	static constexpr float OBS_WIDTH = 45.f;
	static constexpr float OBS_HEIGHT = 45.f;
};
