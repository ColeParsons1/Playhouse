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
class UVerticalBox;
class UPHCMainHUD;
class APHCVehiclePawn;
class APHCDuneBuggyPawn;
class APHCRaptorPawn;
class APHCTankPawn;
class APHCDirtBikePawn;
class APHCBlackbirdPawn;
class APHCBlackHawkPawn;
class APHCRollerBladesPawn;

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

	/** Set by PlayerController when a vehicle entry is requested but vehicle hasn't replicated yet. */
	bool bPendingVehicleEntry = false;

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

	UPROPERTY()
	TObjectPtr<UButton> EnterVehicleButton;

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
	TObjectPtr<UButton> SelectScytheBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectDaggerBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectAK47Btn;

	UPROPERTY()
	TObjectPtr<UButton> SelectSniperBtn;

	UPROPERTY()
	TObjectPtr<UButton> WeaponFireButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> WeaponFireLabel;

	// -- Night mode toggle -----------------------------------------------------

	UPROPERTY()
	TObjectPtr<UButton> NightButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> NightButtonLabel;

	// -- Disaster selection popup ----------------------------------------------

	UPROPERTY()
	TObjectPtr<UButton> DisastersButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> DisastersButtonLabel;

	UPROPERTY()
	TObjectPtr<UBorder> DisasterSelectOverlay;

	UPROPERTY()
	TObjectPtr<UButton> SelectTornadoBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectEarthquakeBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectThunderstormBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectMeteorBtn;

	UPROPERTY()
	TObjectPtr<UButton> StopDisasterBtn;

	// -- Lobby player list overlay ---------------------------------------------

	UPROPERTY()
	TObjectPtr<UBorder> LobbyOverlay;

	UPROPERTY()
	TObjectPtr<UVerticalBox> LobbyPlayerList;

	bool bLobbyOpen = false;

	// -- Vehicle selection popup -----------------------------------------------

	UPROPERTY()
	TObjectPtr<UBorder> VehicleSelectOverlay;

	UPROPERTY()
	TObjectPtr<UButton> SelectAventadorBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectBuggyBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectRaptorBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectJetpackBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectTankBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectDirtBikeBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectBlackHawkBtn;

	UPROPERTY()
	TObjectPtr<UButton> SelectRollerBladesBtn;

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

	// -- Health bar (right of location monitor) --------------------------------
	UPROPERTY()
	TObjectPtr<UBorder> HealthBarBG;

	UPROPERTY()
	TObjectPtr<UBorder> HealthBarFill;

	UPROPERTY()
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY()
	TObjectPtr<UTextBlock> IPInputText;

	// -- Kill/Death ratio display (right of health bar) ----------------------
	UPROPERTY()
	TObjectPtr<UBorder> KDDisplayBG;

	UPROPERTY()
	TObjectPtr<UTextBlock> KDText;

	// -- Damage flash overlay (full-screen translucent red) -------------------
	UPROPERTY()
	TObjectPtr<UBorder> DamageFlashOverlay;

	// -- COD-style crosshair (FP mode only) --------------------------------
	UPROPERTY() TObjectPtr<UBorder> CrosshairDot;
	UPROPERTY() TObjectPtr<UBorder> CrosshairTop;
	UPROPERTY() TObjectPtr<UBorder> CrosshairBottom;
	UPROPERTY() TObjectPtr<UBorder> CrosshairLeft;
	UPROPERTY() TObjectPtr<UBorder> CrosshairRight;

	// -- Vehicle crosshair (flight vehicles: Raptor, Blackbird, Black Hawk) --
	UPROPERTY() TObjectPtr<UBorder> VehicleCrosshairDot;
	UPROPERTY() TObjectPtr<UBorder> VehicleCrosshairTop;
	UPROPERTY() TObjectPtr<UBorder> VehicleCrosshairBottom;
	UPROPERTY() TObjectPtr<UBorder> VehicleCrosshairLeft;
	UPROPERTY() TObjectPtr<UBorder> VehicleCrosshairRight;

	// -- Hit marker (white X flash at crosshair) ---------------------------
	UPROPERTY() TObjectPtr<UBorder> HitMarkerTL;
	UPROPERTY() TObjectPtr<UBorder> HitMarkerTR;
	UPROPERTY() TObjectPtr<UBorder> HitMarkerBL;
	UPROPERTY() TObjectPtr<UBorder> HitMarkerBR;

	// -- Mobile ADS button -------------------------------------------------
	UPROPERTY() TObjectPtr<UButton> ADSButton;

	// -- Sniper scope overlay (COD-style) -----------------------------------
	UPROPERTY() TObjectPtr<UBorder> ScopeOverlay;       // full-screen black backdrop
	UPROPERTY() TObjectPtr<UBorder> ScopeBlackTop;      // thick black bar above scope circle
	UPROPERTY() TObjectPtr<UBorder> ScopeBlackBottom;   // thick black bar below scope circle
	UPROPERTY() TObjectPtr<UBorder> ScopeBlackLeft;     // thick black bar left of scope circle
	UPROPERTY() TObjectPtr<UBorder> ScopeBlackRight;    // thick black bar right of scope circle
	UPROPERTY() TObjectPtr<UBorder> ScopeRingBorder;    // thin ring around scope viewport
	UPROPERTY() TObjectPtr<UBorder> ScopeCrosshairH;    // horizontal crosshair line
	UPROPERTY() TObjectPtr<UBorder> ScopeCrosshairV;    // vertical crosshair line
	UPROPERTY() TObjectPtr<UBorder> ScopeCrosshairDot;  // center dot

	// Scope zoom buttons
	UPROPERTY() TObjectPtr<UButton> ScopeZoomInBtn;
	UPROPERTY() TObjectPtr<UButton> ScopeZoomOutBtn;
	UPROPERTY() TObjectPtr<UTextBlock> ScopeZoomLabel;

	UFUNCTION() void OnScopeZoomInPressed();
	UFUNCTION() void OnScopeZoomOutPressed();
	UFUNCTION() void OnThermalTogglePressed();

	UPROPERTY() TObjectPtr<UButton> ThermalToggleBtn;
	UPROPERTY() TObjectPtr<UTextBlock> ThermalToggleLabel;

	/** The IP address string being entered by the user. */
	FString EnteredIP;

	bool bDrivingMode = false;
	bool bFireHeld = false;
	bool bEnterTouchActive = false;

	// Speed tracking for speedometer
	FVector PrevPawnLocation = FVector::ZeroVector;
	float SmoothedSpeed = 0.f;
	bool bSpeedInitialized = false;

	// Walking touch state (virtual joystick + camera look)
	FVector2D WalkMoveOrigin = FVector2D::ZeroVector;
	int32 WalkMoveFingerIdx = -1;
	int32 WalkLookFingerIdx = -1;
	FVector2D WalkLookPrev = FVector2D::ZeroVector;

	// Tap-to-jump tracking per finger
	bool WalkFingerWasDown[5] = {};
	FVector2D WalkTapStartPos[5];
	double WalkTapStartTime[5] = {};

private:
	UFUNCTION()
	void OnMenuButtonClicked();

	UFUNCTION()
	void OnCloseMenuButtonClicked();

	UFUNCTION()
	void OnCarButtonClicked();

	UFUNCTION()
	void OnEnterVehicleClicked();

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
	void OnSelectJetpack();

	UFUNCTION()
	void OnSelectTank();

	UFUNCTION()
	void OnSelectDirtBike();

	UFUNCTION()
	void OnSelectBlackHawk();

	UFUNCTION()
	void OnSelectRollerBlades();

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
	void OnSelectScythe();

	UFUNCTION()
	void OnSelectDagger();

	UFUNCTION()
	void OnSelectAK47();

	UFUNCTION()
	void OnSelectSniper();

	UFUNCTION()
	void OnADSPressed();

	UFUNCTION()
	void OnADSReleased();

	UFUNCTION()
	void OnWeaponFirePressed();

	UFUNCTION()
	void OnWeaponFireReleased();

	UFUNCTION()
	void OnNightButtonClicked();

	UFUNCTION()
	void OnDisastersButtonClicked();

	UFUNCTION()
	void OnCameraModeButtonClicked();

	UFUNCTION()
	void OnLobbyButtonClicked();

	UFUNCTION()
	void OnLobbyCloseClicked();

	void ShowLobby();
	void HideLobby();
	void RefreshLobbyPlayerList();

	UFUNCTION()
	void OnSelectTornado();

	UFUNCTION()
	void OnSelectEarthquake();

	UFUNCTION()
	void OnSelectThunderstorm();

	UFUNCTION()
	void OnSelectMeteor();

	UFUNCTION()
	void OnStopDisaster();

	void ShowWeaponSelection();
	void HideWeaponSelection();

	void ShowDisasterSelection();
	void HideDisasterSelection();

	// -- Camera mode toggle ---------------------------------------------------

	UPROPERTY()
	TObjectPtr<UButton> CameraModeButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> CameraModeLabel;

	UPROPERTY()
	TObjectPtr<UButton> TorchButton;

	UPROPERTY()
	TObjectPtr<UTextBlock> TorchButtonLabel;

	// -- XP bar + Level display -----------------------------------------------

	UPROPERTY() TObjectPtr<UBorder> XPBarBG;
	UPROPERTY() TObjectPtr<UBorder> XPBarFill;
	UPROPERTY() TObjectPtr<UTextBlock> LevelText;
	UPROPERTY() TObjectPtr<UTextBlock> XPText;

	// -- Kill streak display --------------------------------------------------

	UPROPERTY() TObjectPtr<UTextBlock> KillStreakText;

	// -- Level-up flash overlay -----------------------------------------------

	UPROPERTY() TObjectPtr<UBorder> LevelUpOverlay;
	UPROPERTY() TObjectPtr<UTextBlock> LevelUpText;

	// -- Loot crate spinner overlay -------------------------------------------

	UPROPERTY() TObjectPtr<UBorder> LootCrateOverlay;
	UPROPERTY() TObjectPtr<UTextBlock> LootCrateSpinText;
	UPROPERTY() TObjectPtr<UTextBlock> LootCrateRewardText;

	// -- Achievement popup banner ---------------------------------------------

	UPROPERTY() TObjectPtr<UBorder> AchievementPopupBG;
	UPROPERTY() TObjectPtr<UTextBlock> AchievementPopupText;

	// -- Limited-time event banner --------------------------------------------

	UPROPERTY() TObjectPtr<UBorder> EventBannerBG;
	UPROPERTY() TObjectPtr<UTextBlock> EventBannerText;
	UPROPERTY() TObjectPtr<UTextBlock> EventTimerText;

	// -- Daily login reward popup ---------------------------------------------

	UPROPERTY() TObjectPtr<UBorder> DailyLoginOverlay;
	UPROPERTY() TObjectPtr<UTextBlock> DailyLoginTitle;
	UPROPERTY() TObjectPtr<UTextBlock> DailyLoginStreakText;
	UPROPERTY() TObjectPtr<UTextBlock> DailyLoginRewardText;
	UPROPERTY() TObjectPtr<UButton> DailyLoginClaimBtn;
	UPROPERTY() TObjectPtr<UTextBlock> DailyLoginClaimLabel;
	UPROPERTY() TObjectPtr<UBorder> DailyLoginDay1;
	UPROPERTY() TObjectPtr<UBorder> DailyLoginDay2;
	UPROPERTY() TObjectPtr<UBorder> DailyLoginDay3;
	UPROPERTY() TObjectPtr<UBorder> DailyLoginDay4;
	UPROPERTY() TObjectPtr<UBorder> DailyLoginDay5;
	UPROPERTY() TObjectPtr<UBorder> DailyLoginDay6;
	UPROPERTY() TObjectPtr<UBorder> DailyLoginDay7;
	bool bDailyLoginShown = false;

	UFUNCTION() void OnDailyLoginClaimClicked();

	// -- Psychedelic trip overlay (DeepDream-style patterns) ------------------

	UPROPERTY() TObjectPtr<UImage> TripOverlay0;
	UPROPERTY() TObjectPtr<UImage> TripOverlay1;
	UPROPERTY() TObjectPtr<UImage> TripOverlay2;

	UPROPERTY() TObjectPtr<UTexture2D> TripTex0;
	UPROPERTY() TObjectPtr<UTexture2D> TripTex1;
	UPROPERTY() TObjectPtr<UTexture2D> TripTex2;

	bool bTripOverlaysCreated = false;
	bool bTripOverlaysVisible = false;

	void CreateTripOverlays();
	UTexture2D* GenerateTripTexture(int32 Pattern, int32 Size = 256);

	bool bMenuOpen = false;
	bool bVehicleSelectOpen = false;
	bool bWeaponSelectOpen = false;
	bool bDisasterSelectOpen = false;
	bool bWeaponFireHeld = false;
	int32 EquippedWeaponType = 3;

	// -- Military Radar -------------------------------------------------------

	UPROPERTY() TObjectPtr<UBorder> MinimapBorder;
	UPROPERTY() TObjectPtr<UCanvasPanel> MinimapCanvas;
	UPROPERTY() TObjectPtr<UBorder> MinimapLocalDot;
	UPROPERTY() TObjectPtr<UBorder> MinimapDirectionIndicator;
	TArray<TObjectPtr<UBorder>> MinimapPlayerDots;

	// Radar decorations
	UPROPERTY() TObjectPtr<UBorder> RadarSweepLine;
	UPROPERTY() TObjectPtr<UTextBlock> RadarLabelN;
	UPROPERTY() TObjectPtr<UTextBlock> RadarLabelS;
	UPROPERTY() TObjectPtr<UTextBlock> RadarLabelE;
	UPROPERTY() TObjectPtr<UTextBlock> RadarLabelW;
	UPROPERTY() TObjectPtr<UBorder> RadarRingInner;
	UPROPERTY() TObjectPtr<UBorder> RadarRingOuter;
	UPROPERTY() TObjectPtr<UBorder> RadarCrossH;
	UPROPERTY() TObjectPtr<UBorder> RadarCrossV;

	float RadarSweepAngle = 0.f;

	static constexpr float MinimapSize = 180.f;
	static constexpr float MinimapRadius = 50000.f;

	void UpdateMinimap(float DeltaTime);
};
