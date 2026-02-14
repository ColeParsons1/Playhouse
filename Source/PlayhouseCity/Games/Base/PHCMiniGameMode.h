// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Core/PHCTypes.h"
#include "PHCMiniGameMode.generated.h"

class UPHCGameInstance;
class UPHCMiniGameHUD;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

/** Broadcast when a mini-game round ends (win, lose, or timeout). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPHCGameEnded, const FPHCGameResult&, Result);

// ---------------------------------------------------------------------------
// APHCMiniGameMode
// ---------------------------------------------------------------------------

/**
 * Base game mode for all 21 PlayhouseCity mini-games.
 *
 * Provides shared infrastructure: score tracking, a countdown timer,
 * pause/resume flow, result calculation, server score submission,
 * and a clean transition back to the lobby level.
 *
 * Subclasses override StartGame() and EndGame() to layer in
 * game-specific logic while calling Super to retain base behaviour.
 */
UCLASS(Abstract, Blueprintable)
class PLAYHOUSECITY_API APHCMiniGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	APHCMiniGameMode();

	// -- Lifecycle --------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -- Game flow ---------------------------------------------------------

	/** Start (or restart) the mini-game round. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MiniGame")
	virtual void StartGame();

	/** End the current round. Pass true if the local player won. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MiniGame")
	virtual void EndGame(bool bWon);

	/** Pause the active game (freezes timer, sets bGamePaused). */
	UFUNCTION(BlueprintCallable, Category = "PHC|MiniGame")
	virtual void PauseGame();

	/** Resume from a paused state. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MiniGame")
	virtual void ResumeGame();

	/** Add points to the current score (clamped >= 0). */
	UFUNCTION(BlueprintCallable, Category = "PHC|MiniGame")
	void AddScore(int32 Points);

	/** Return the current score. */
	UFUNCTION(BlueprintPure, Category = "PHC|MiniGame")
	int32 GetScore() const { return CurrentScore; }

	/** Seconds remaining before the timer expires. */
	UFUNCTION(BlueprintPure, Category = "PHC|MiniGame")
	float GetTimeRemaining() const;

	/** Transition the session back to the lobby map / game mode. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MiniGame")
	void ReturnToLobby();

	// -- Delegates --------------------------------------------------------

	/** Fired once when the round ends (after results are calculated). */
	UPROPERTY(BlueprintAssignable, Category = "PHC|MiniGame")
	FOnPHCGameEnded OnGameEnded;

	// -- State (read-only in Blueprint) -----------------------------------

	/** Identifies which mini-game this mode represents. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|MiniGame")
	EPHCGameType GameType = EPHCGameType::Bingo;

	/** HUD widget class to use for this game. If null, the base MiniGameHUD is used. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|MiniGame")
	TSubclassOf<UPHCMiniGameHUD> GameHUDClass;

	/** Running score for the current round. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|MiniGame")
	int32 CurrentScore = 0;

	/** Highest score achieved in this session (not persisted). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|MiniGame")
	int32 HighScore = 0;

	/** Elapsed / countdown timer for the round. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|MiniGame")
	float GameTimer = 0.f;

	/** Maximum allowed time per round in seconds. 0 means unlimited. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|MiniGame")
	float MaxGameTime = 0.f;

	/** True while a round is actively being played. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|MiniGame")
	bool bGameActive = false;

	/** True when the game is paused via PauseGame(). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|MiniGame")
	bool bGamePaused = false;

	/** Result of the most recently completed round. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|MiniGame")
	FPHCGameResult LastResult;

	/** The lobby level to travel to when the player exits. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|MiniGame")
	FName LobbyMapName = TEXT("City_Main");

protected:
	// -- Internal helpers --------------------------------------------------

	/** POST the round score to the PlayhouseCity backend. */
	void SubmitScoreToServer();

	/** Compute coin / XP rewards from the score and win state. */
	FPHCGameResult CalculateRewards(int32 Score, bool bWon) const;

	/** Convenience: cached reference to the owning game instance. */
	UPROPERTY(Transient)
	TObjectPtr<UPHCGameInstance> CachedGameInstance;
};
