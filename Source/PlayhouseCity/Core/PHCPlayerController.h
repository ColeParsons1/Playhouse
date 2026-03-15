// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Core/PHCTypes.h"
#include "PHCPlayerController.generated.h"

class UPHCGameInstance;
class UUserWidget;
class UPHCMiniGameHUD;
class APHCMiniGameMode;
class UPHCCityHUD;
class APHCVehiclePawn;
class APHCDuneBuggyPawn;
class APHCRaptorPawn;
class APHCCityCharacter;
class APHCTankPawn;
class APHCDirtBikePawn;
class APHCBlackbirdPawn;
class APHCBlackHawkPawn;
class APHCRollerBladesPawn;
class STextBlock;

/**
 * Top-level player controller for the PlayhouseCity lobby.
 *
 * Responsible for:
 *  - Checking login state on BeginPlay and routing to auth or main HUD.
 *  - Creating / destroying the auth-screen and main-HUD widgets.
 *  - Launching mini-games and returning to the lobby.
 *  - Supporting both UI-only mode (legacy lobby) and 3D world mode.
 *  - Listening to GameInstance login/logout delegates to swap screens.
 */
UCLASS()
class PLAYHOUSECITY_API APHCPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APHCPlayerController();

	// -- Widget class references (assign in BP or defaults) -------------------

	/** Widget Blueprint class shown when the player is NOT logged in. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|UI")
	TSubclassOf<UUserWidget> AuthWidgetClass;

	/** Widget Blueprint class shown as the main HUD after login. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|UI")
	TSubclassOf<UUserWidget> MainHUDWidgetClass;

	/** Widget class for the 3D open-world city HUD overlay. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|UI")
	TSubclassOf<UPHCCityHUD> CityHUDWidgetClass;

	/** True when the controller is in 3D world mode (character visible,
	 *  game + UI input) rather than pure UI mode (lobby, auth, mini-game). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|UI")
	bool bIsInWorldMode = false;

	// -- Screen management ----------------------------------------------------

	/** Remove any active widget and show the authentication screen. */
	UFUNCTION(BlueprintCallable, Category = "PHC|UI")
	void ShowAuthScreen();

	/** Remove any active widget and show the main lobby HUD. */
	UFUNCTION(BlueprintCallable, Category = "PHC|UI")
	void ShowMainHUD();

	/** Switch to 3D world mode: show the city HUD overlay and enable
	 *  game + UI input so the player can control the character. */
	UFUNCTION(BlueprintCallable, Category = "PHC|UI")
	void EnterWorldMode();

	/** Switch from world mode back to full UI mode (for menus/auth). */
	UFUNCTION(BlueprintCallable, Category = "PHC|UI")
	void EnterMenuMode();

	// -- Game launch ----------------------------------------------------------

	/** Launches a mini-game: removes lobby HUD, spawns game mode actor,
	 *  creates the game HUD, and starts the game. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Game")
	void LaunchGame(EPHCGameType GameType);

	/** Returns from a mini-game to the lobby: destroys the game mode actor,
	 *  removes the game HUD, and re-shows the main lobby HUD. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Game")
	void ReturnFromGame();

	// -- Delegate callbacks ---------------------------------------------------

	/** Bound to UPHCGameInstance::OnLoginSuccess. */
	UFUNCTION()
	void OnLoginSuccess(const FPHCPlayerProfile& PlayerProfile);

	/** Bound to UPHCGameInstance::OnLogoutComplete. */
	UFUNCTION()
	void OnLogoutComplete();

	// -- Vehicle interaction -----------------------------------------------

	/** Attempt to enter the nearest vehicle (called by character's interact key). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Vehicle")
	void EnterNearestVehicle();

	/** Spawn a vehicle at the player's location and immediately enter it.
	 *  VehicleType: 0 = Aventador, 1 = Dune Buggy, 2 = F-22 Raptor. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Vehicle")
	void SpawnAndEnterVehicle(int32 VehicleType = 0);

	/** Called by the vehicle pawn when the player presses the exit key. */
	UFUNCTION()
	void OnVehicleExitRequested();

	/** Equip or remove the jetpack on the character. */
	void EquipJetpack();
	void UnequipJetpack();

	/** Equip or remove roller blades on the character. */
	void EquipRollerBlades();
	void UnequipRollerBlades();

	/** Equip or remove climbing gear on the character. */
	void EquipClimbingGear();
	void UnequipClimbingGear();

	/** Server RPC: spawn a vehicle on the server and enter it. */
	UFUNCTION(Server, Reliable)
	void ServerSpawnAndEnterVehicle(int32 VehicleType);

	/** Client RPC: update local HUD after server possession change (entering vehicle). */
	UFUNCTION(Client, Reliable)
	void ClientOnVehicleEntered(APawn* NewVehicle);

	/** Server RPC: exit the current vehicle on the server. */
	UFUNCTION(Server, Reliable)
	void ServerExitVehicle();

	/** Client RPC: update local HUD after server possession change (exiting vehicle). */
	UFUNCTION(Client, Reliable)
	void ClientOnVehicleExited(FVector ExitLocation);

	/** Server RPC: send the local player's profile to the server so it can
	 *  be set on the PlayerState and replicated to all clients. */
	UFUNCTION(Server, Reliable)
	void ServerSendPlayerProfile(const FPHCPlayerProfile& Profile);

	/** Server RPC: relay vehicle input through the controller (vehicle RPCs
	 *  don't arrive on dynamically spawned pawns). */
	UFUNCTION(Server, Unreliable)
	void ServerSetVehicleInput(float Throttle, float Steering, float Pitch);

	/** Returns the currently active top-level widget (e.g. CityHUD). */
	UUserWidget* GetActiveWidget() const { return ActiveWidget; }

	/** Show a full-screen loading overlay (masks HUD construction + server connect). */
	void ShowLoadingScreen();

	/** Remove the loading overlay and reveal the game. */
	void HideLoadingScreen();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaTime) override;

private:
	/** Currently displayed top-level widget (auth or HUD). */
	UPROPERTY()
	TObjectPtr<UUserWidget> ActiveWidget;

	/** Active mini-game mode actor (spawned, not the framework game mode). */
	UPROPERTY()
	TObjectPtr<APHCMiniGameMode> ActiveMiniGame;

	/** Active mini-game HUD widget. */
	UPROPERTY()
	TObjectPtr<UPHCMiniGameHUD> ActiveGameHUD;

	/** Returns the game mode class for a given game type. */
	TSubclassOf<APHCMiniGameMode> GetGameModeClassForType(EPHCGameType GameType) const;

	/** Helper: remove the current ActiveWidget from viewport and null it. */
	void ClearActiveWidget();

	/** Helper: configure input mode to UI-only with mouse cursor visible. */
	void SetUIOnlyInputMode();

	/** Helper: configure input mode for 3D gameplay + UI overlay. */
	void SetGameAndUIInputMode();

	/** The character pawn we unpossessed when entering a vehicle. */
	UPROPERTY()
	TObjectPtr<APHCCityCharacter> SavedCharacterPawn;

	/** The vehicle we're currently driving (Aventador or Dune Buggy). */
	UPROPERTY()
	TObjectPtr<APawn> CurrentVehicle;

	/** Client-side: last known vehicle position, updated every tick while
	 *  driving. Used for exit location since the server's vehicle position
	 *  may diverge due to Unreliable input RPCs. */
	FVector LastVehiclePosition = FVector::ZeroVector;

	/** Client-side: target exit position to enforce for several frames after
	 *  exiting a vehicle. This overrides the server's stale replicated position
	 *  until the client's ServerUpdateMovement corrects the server. */
	FVector PendingExitLocation = FVector::ZeroVector;
	int32 ExitTeleportFrames = 0;

	/** True while we're driving a vehicle (set on enter, cleared on Tick-based
	 *  exit detection). Used to detect vehicle-to-character pawn transition. */
	bool bWasDrivingVehicle = false;

	/** Performs all client-side vehicle exit cleanup: position, input contexts,
	 *  collision, HUD mode. Called from Tick when pawn change is detected. */
	void HandleLocalVehicleExit();

	/** Full-screen Slate loading overlay shown during world transitions. */
	TSharedPtr<SWidget> LoadingScreenSlate;

	/** Animated "Loading..." text inside the loading screen. */
	TSharedPtr<STextBlock> LoadingDotsSlate;

	/** Timer driving the dot animation on the loading screen. */
	FTimerHandle LoadingDotsTimerHandle;

	/** Current dot animation phase (0-2). */
	int32 LoadingDotPhase = 0;

	/** Whether the loading screen is currently active. */
	bool bLoadingScreenVisible = false;
};
