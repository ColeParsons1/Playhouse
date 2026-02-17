// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "CelDamageHUD.generated.h"

class UCanvasPanel;
class UTextBlock;
class UProgressBar;
class UBorder;
class UButton;
class ACelDamageGameMode;

/**
 * HUD overlay for the Cel Damage vehicular combat mini-game.
 * Displays HP bars, smack scores, weapon indicator, virtual joystick, and fire button.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UCelDamageHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	virtual FReply NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

protected:
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> PlayerHPBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> EnemyHPBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerSmacksText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AISmacksText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WeaponNameText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> WeaponIndicator;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> FireButton;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACelDamageGameMode> CelDamageMode;

	UFUNCTION()
	void OnFireClicked();

	// Virtual joystick state
	bool bJoystickActive = false;
	FVector2D JoystickOrigin = FVector2D::ZeroVector;
	FVector2D JoystickCurrent = FVector2D::ZeroVector;
	int32 JoystickFingerIndex = -1;
	static constexpr float JoystickDeadzone = 15.f;
	static constexpr float JoystickMaxRadius = 100.f;
};
