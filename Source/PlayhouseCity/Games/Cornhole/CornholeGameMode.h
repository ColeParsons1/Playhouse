// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "CornholeGameMode.generated.h"

class ABeanBag;
class ACornholeBoard;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCornholeThrowComplete, int32, ScoreValue, bool, bIsPlayer);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCornholeRoundEnd, int32, PlayerScore, int32, CPUScore);

// ---------------------------------------------------------------------------
// ACornholeGameMode
// ---------------------------------------------------------------------------

/**
 * Game mode for the Cornhole mini-game.
 *
 * Two sides (player and CPU) alternate throwing 4 bean bags per round.
 * Cancellation scoring: only the side that out-scored the other earns
 * the difference.  First to win RoundsToWin rounds takes the match.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ACornholeGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	ACornholeGameMode();

	virtual void BeginPlay() override;
	virtual void StartGame() override;

	// -- API --------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "PHC|Cornhole")
	void ThrowBag(FVector Direction, float Power);

	UFUNCTION(BlueprintPure, Category = "PHC|Cornhole")
	bool IsPlayerTurn() const { return bPlayerTurn; }

	// -- Config -----------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Cornhole")
	int32 BagsPerRound = 4;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Cornhole")
	int32 RoundsToWin = 3;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PHC|Cornhole")
	float ThrowPowerScale = 500.f;

	// -- State ------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	int32 PlayerRoundScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	int32 CPURoundScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	int32 PlayerTotalScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	int32 CPUTotalScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	int32 CurrentRound = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	int32 PlayerBagsThrown = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	int32 CPUBagsThrown = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Cornhole")
	bool bPlayerTurn = true;

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|Cornhole")
	FOnCornholeThrowComplete OnThrowComplete;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Cornhole")
	FOnCornholeRoundEnd OnRoundEnd;

protected:
	UPROPERTY(Transient)
	TObjectPtr<ACornholeBoard> Board;

	UPROPERTY(Transient)
	TObjectPtr<ABeanBag> ActiveBag;

	UPROPERTY(Transient)
	TArray<TObjectPtr<ABeanBag>> AllBags;

	void SpawnBoard();
	void SpawnBag(bool bIsPlayer);

	UFUNCTION()
	void OnBagSettled(ABeanBag* Bag);

	void CPUThrow();
	void ScheduleCPUThrow();
	void CheckRoundEnd();
	void StartNewRound();

	FTimerHandle CPUThrowHandle;
	FVector PlayerThrowOrigin = FVector(-200.f, 0.f, 30.f);
	FVector CPUThrowOrigin = FVector(-200.f, 40.f, 30.f);
};
