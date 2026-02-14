// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "AngryDucksHUD.generated.h"

class UCanvasPanel;
class UTextBlock;
class UImage;
class UBorder;
class UButton;
class USizeBox;
class AAngryDucksGameMode;

// ---------------------------------------------------------------------------
// UAngryDucksHUD
// ---------------------------------------------------------------------------

/**
 * In-game HUD widget for the Angry Ducks mini-game.
 *
 * Displays duck and pig counts, current level, and handles
 * mouse/touch drag input for aiming the slingshot.  The aim line
 * image widget is repositioned to show the trajectory preview.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UAngryDucksHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;

	// -- Public helpers ---------------------------------------------------

	/** Update the displayed duck count. */
	UFUNCTION(BlueprintCallable, Category = "PHC|AngryDucks")
	void UpdateDuckCount(int32 Count);

	/** Show the level-cleared banner briefly. */
	UFUNCTION(BlueprintCallable, Category = "PHC|AngryDucks")
	void ShowLevelClear(int32 Level);

	/** Update the aim line visual from the slingshot to the current aim point. */
	UFUNCTION(BlueprintCallable, Category = "PHC|AngryDucks")
	void UpdateAim(FVector2D AimStart, FVector2D AimEnd);

protected:
	// -- Bind-widgets (matched by name in the UMG designer) ---------------

	/** Main game field canvas containing all game-specific visuals. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> GameCanvas;

	/** Sky blue background for the game field. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> GameFieldBackground;

	/** Brown slingshot on the left side of the field. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlingshotWidget;

	/** The slingshot fork (top part). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SlingshotFork;

	/** Yellow duck sitting on the slingshot, ready to launch. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> DuckWidget;

	/** Active projectile duck (in flight). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> ProjectileDuck;

	/** Gray destructible block widgets (structures on the right side). */
	UPROPERTY(meta = (BindWidgetOptional))
	TArray<TObjectPtr<UBorder>> StructureBlocks;

	/** Green pig target widgets nested among structures. */
	UPROPERTY(meta = (BindWidgetOptional))
	TArray<TObjectPtr<UBorder>> PigTargets;

	/** Ground surface at the bottom of the field. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> GroundWidget;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DucksRemainingText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PigsRemainingText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AngleText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PowerText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> AimLine;

	/** Aim up button (increase launch angle). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AimUpButton;

	/** Aim down button (decrease launch angle). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AimDownButton;

	/** Fire / launch button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LaunchButton;

private:
	/** Cached game mode reference. */
	UPROPERTY(Transient)
	TObjectPtr<AAngryDucksGameMode> DucksMode;

	/** Button callbacks for aim/launch controls. */
	UFUNCTION()
	void OnAimUpClicked();

	UFUNCTION()
	void OnAimDownClicked();

	UFUNCTION()
	void OnLaunchClicked();

	/** Convert screen-space position to a normalised canvas coordinate. */
	FVector2D ScreenToCanvas(const FGeometry& Geometry, FVector2D ScreenPos) const;

	/** Calculate aim direction from drag start to current position. */
	FVector2D ComputeAimDirection(FVector2D DragStart, FVector2D DragCurrent) const;

	/** Calculate launch power from drag distance. */
	float ComputeLaunchPower(FVector2D DragStart, FVector2D DragCurrent) const;

	/** Screen position where the current drag began. */
	FVector2D DragStartPos = FVector2D::ZeroVector;

	/** Current drag position in screen space. */
	FVector2D DragCurrentPos = FVector2D::ZeroVector;

	/** True while the player is dragging (aiming). */
	bool bDragging = false;

	/** Maximum drag distance in canvas-normalised space for full power. */
	static constexpr float MaxDragDistance = 300.f;

	// -- Projectile animation state --

	/** Current launch angle in degrees (0 = horizontal, 90 = straight up). */
	float LaunchAngle = 45.f;

	/** Current projectile position for NativeTick animation. */
	FVector2D ProjectilePos = FVector2D::ZeroVector;

	/** Current projectile velocity for NativeTick animation. */
	FVector2D ProjectileVel = FVector2D::ZeroVector;

	/** True while a projectile is animating across the field. */
	bool bProjectileActive = false;

	/** Gravity constant for projectile arc (pixels/sec^2). */
	static constexpr float ProjectileGravity = 400.f;

	/** Launch speed scalar for button-based launch. */
	static constexpr float ButtonLaunchSpeed = 500.f;
};
