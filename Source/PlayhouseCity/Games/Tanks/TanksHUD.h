// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "TanksHUD.generated.h"

class UCanvasPanel;
class UTextBlock;
class UImage;
class UProgressBar;
class UBorder;
class UButton;
class ATanksGameMode;

UCLASS(Blueprintable)
class PLAYHOUSECITY_API UTanksHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;

protected:
	// -- Battlefield -------------------------------------------------------

	/** Full game area canvas (dark green battlefield). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> GameCanvas;

	/** Terrain silhouette along the bottom of the battlefield. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> TerrainBorder;

	// -- Tank widgets ------------------------------------------------------

	/** Player (blue) tank body widget. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PlayerTankWidget;

	/** Player tank turret (lighter blue, overlaid on body). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PlayerTurretWidget;

	/** Enemy (red) tank body widget. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> EnemyTankWidget;

	/** Enemy tank turret. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> EnemyTurretWidget;

	// -- Health bars -------------------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PlayerHPBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> EnemyHPBar;

	// -- Angle / Power controls -------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AngleUpButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AngleDownButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AngleValueText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PowerBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PowerUpButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PowerDownButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PowerValueText;

	// -- Fire button -------------------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> FireButton;

	// -- Status / indicators -----------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WindText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TurnIndicatorText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CPURemainingText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> PlayerTankImage;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusText;

private:
	UPROPERTY(Transient)
	TObjectPtr<ATanksGameMode> TanksMode;

	FVector2D ScreenToArena(const FGeometry& Geometry, FVector2D ScreenPos) const;

	// -- Button callbacks --------------------------------------------------

	UFUNCTION() void OnAngleUp();
	UFUNCTION() void OnAngleDown();
	UFUNCTION() void OnPowerUp();
	UFUNCTION() void OnPowerDown();
	UFUNCTION() void OnFireClicked();

	/** Current player aim angle in degrees (0-90). */
	float AimAngleDeg = 45.f;

	/** Current player power (0..1). */
	float AimPower = 0.5f;
};
