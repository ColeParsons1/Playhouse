// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PHCCityHUD.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UBorder;
class UCanvasPanel;
class UPHCMainHUD;
class APHCVehiclePawn;
class APHCDuneBuggyPawn;
class APHCRaptorPawn;

/**
 * Lightweight HUD overlay for the PlayhouseCity 3D open world.
 *
 * Displays currency, a menu button (opens the existing MainHUD as a
 * GTA-style "phone" overlay), and mobile touch controls for both
 * on-foot and driving modes.
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYHOUSECITY_API UPHCCityHUD : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCCityHUD(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Updates the currency readouts. */
	UFUNCTION(BlueprintCallable, Category = "PHC|UI")
	void UpdateCurrencyDisplay(int32 Coins, int32 Gems);

	/** Returns true if the lobby menu overlay is currently visible. */
	UFUNCTION(BlueprintPure, Category = "PHC|UI")
	bool IsMenuOpen() const { return bMenuOpen; }

	/** Opens the lobby menu overlay. */
	UFUNCTION(BlueprintCallable, Category = "PHC|UI")
	void OpenMenu();

	/** Closes the lobby menu overlay. */
	UFUNCTION(BlueprintCallable, Category = "PHC|UI")
	void CloseMenu();

	/** Switch to driving mode: show vehicle touch controls, hide walk controls. */
	UFUNCTION(BlueprintCallable, Category = "PHC|UI")
	void SetDrivingMode(bool bDriving, APawn* Vehicle);

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CoinText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GemText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> MenuButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CloseMenuButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CarButton;

	// Jump/Sprint/Interact buttons removed — mobile uses touch panels only.

	// -- Missile fire button (shown when driving dune buggy) ----------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> FireButton;

	// -- Bomb drop button (shown when driving raptor) ---------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> DropBombButton;

	// -- Driving mode widgets (mobile only) --------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> SteeringPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SteeringLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> GasPedalPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> GasLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BrakePedalPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> BrakeLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExitVehicleButton;

	// -- Walking mode touch panels (mobile only) ----------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> WalkMovePanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WalkMoveLabel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UBorder> WalkLookPanel;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WalkLookLabel;

	/** The lobby main HUD shown as an overlay when the menu is opened. */
	UPROPERTY()
	TObjectPtr<UPHCMainHUD> MenuOverlay;

	/** Widget class for the lobby menu overlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|UI")
	TSubclassOf<UPHCMainHUD> MainHUDClass;

	/** The vehicle we're currently controlling via touch. */
	UPROPERTY()
	TObjectPtr<APawn> DrivenVehicle;

	// -- Weapon selection popup ------------------------------------------------

	UPROPERTY()
	TObjectPtr<UButton> WeaponsButton;

	UPROPERTY()
	TObjectPtr<UBorder> WeaponSelectOverlay;

	UPROPERTY()
	TObjectPtr<UButton> SelectGunBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectRPGBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectSwordBtn;

	UPROPERTY()
	TObjectPtr<UButton> WeaponFireButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> WeaponFireLabel;

	// -- Vehicle selection popup -----------------------------------------------

	UPROPERTY()
	TObjectPtr<UBorder> VehicleSelectOverlay;

	UPROPERTY()
	TObjectPtr<UButton> SelectAventadorBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectBuggyBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectRaptorBtn;

	// -- Multiplayer UI -------------------------------------------------------

	UPROPERTY()
	TObjectPtr<UButton> HostButton;

	UPROPERTY()
	TObjectPtr<UButton> JoinButton;

	UPROPERTY()
	TObjectPtr<UBorder> JoinOverlay;

	UPROPERTY()
	TObjectPtr<UButton> JoinConnectButton;

	UPROPERTY()
	TObjectPtr<UButton> JoinCancelButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> PlayerCountText;

	/** Live coordinate display at top of screen. */
	UPROPERTY()
	TObjectPtr<UTextBlock> LocationText;

	UPROPERTY()
	TObjectPtr<UTextBlock> IPInputText;

	/** The IP address string being entered by the user. */
	FString EnteredIP;

	bool bDrivingMode = false;
	bool bFireHeld = false;

	// Speed tracking for speedometer
	FVector PrevPawnLocation = FVector::ZeroVector;
	float SmoothedSpeed = 0.f;
	bool bSpeedInitialized = false;

	// Walking touch state (virtual joystick + camera look)
	FVector2D WalkMoveOrigin = FVector2D::ZeroVector;
	int32 WalkMoveFingerIdx = -1;
	int32 WalkLookFingerIdx = -1;
	FVector2D WalkLookPrev = FVector2D::ZeroVector;

private:
	UFUNCTION()
	void OnMenuButtonClicked();

	UFUNCTION()
	void OnCloseMenuButtonClicked();

	UFUNCTION()
	void OnCarButtonClicked();

	// Jump/Sprint/Interact button callbacks removed.

	UFUNCTION()
	void OnExitVehicleClicked();

	UFUNCTION()
	void OnFireButtonPressed();

	UFUNCTION()
	void OnFireButtonReleased();

	UFUNCTION()
	void OnSelectAventador();

	UFUNCTION()
	void OnSelectBuggy();

	UFUNCTION()
	void OnSelectRaptor();

	UFUNCTION()
	void OnDropBombClicked();

	UFUNCTION()
	void OnHostButtonClicked();

	UFUNCTION()
	void OnJoinButtonClicked();

	UFUNCTION()
	void OnJoinConnectClicked();

	UFUNCTION()
	void OnJoinCancelClicked();

	void ShowVehicleSelection();
	void HideVehicleSelection();

	UFUNCTION()
	void OnWeaponsButtonClicked();

	UFUNCTION()
	void OnSelectGun();

	UFUNCTION()
	void OnSelectRPG();

	UFUNCTION()
	void OnSelectSword();

	UFUNCTION()
	void OnWeaponFirePressed();

	void ShowWeaponSelection();
	void HideWeaponSelection();

	bool bMenuOpen = false;
	bool bVehicleSelectOpen = false;
	bool bWeaponSelectOpen = false;
	int32 EquippedWeaponType = -1;
};
