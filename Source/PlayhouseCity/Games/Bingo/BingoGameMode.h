// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "BingoGameMode.generated.h"

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

/** Broadcast every time a new number is called. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBingoNumberCalled, int32, Number);

/** Broadcast when any participant (player or CPU) achieves bingo. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnBingoAchieved, bool, bIsPlayer, int32, WinnerIndex);

// ---------------------------------------------------------------------------
// ABingoGameMode
// ---------------------------------------------------------------------------

/**
 * Bingo mini-game mode.
 *
 * Rules:
 *  - 75-ball bingo (B 1-15, I 16-30, N 31-45, G 46-60, O 61-75).
 *  - Player receives one 5x5 card; centre cell is a free space.
 *  - 19 CPU opponents each hold one card.
 *  - Numbers are called on a timer (CallInterval seconds).
 *  - First to complete a row, column, or diagonal wins.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ABingoGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	ABingoGameMode();

	// -- Game flow --------------------------------------------------------

	virtual void StartGame() override;
	virtual void EndGame(bool bWon) override;

	// -- Gameplay ---------------------------------------------------------

	/** Mark a cell on the player's card. Returns true if the number was valid. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Bingo")
	bool MarkCell(int32 Row, int32 Col);

	/** Player declares "Bingo!" -- verify and end if valid. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Bingo")
	void OnBingoCalled();

	// -- Queries ----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "PHC|Bingo")
	const TArray<int32>& GetCalledNumbers() const { return CalledNumbers; }

	UFUNCTION(BlueprintPure, Category = "PHC|Bingo")
	int32 GetLastCalledNumber() const;

	UFUNCTION(BlueprintPure, Category = "PHC|Bingo")
	int32 GetCPUOpponentsRemaining() const;

	/** Retrieve the player's card data (5x5 = 25 elements, row-major). */
	UFUNCTION(BlueprintPure, Category = "PHC|Bingo")
	TArray<int32> GetPlayerCardFlat() const;

	/** Retrieve the player's marked state (5x5, row-major). */
	UFUNCTION(BlueprintPure, Category = "PHC|Bingo")
	TArray<bool> GetPlayerMarkedFlat() const;

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|Bingo")
	FOnBingoNumberCalled OnNumberCalled;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Bingo")
	FOnBingoAchieved OnBingoAchieved;

	// -- Tunables ---------------------------------------------------------

	/** Seconds between automatic number calls. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|Bingo")
	float CallInterval = 3.0f;

protected:
	// -- Data -------------------------------------------------------------

	/** Numbers already called this round (in call order). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Bingo")
	TArray<int32> CalledNumbers;

	/** Pool of numbers still available to call. */
	TArray<int32> RemainingNumbers;

	/** Player's 5x5 card [Row][Col]. */
	TArray<TArray<int32>> PlayerCard;

	/** Player's marked state [Row][Col]. */
	TArray<TArray<bool>> PlayerMarked;

	/** 19 CPU opponent cards, each 5x5. */
	TArray<TArray<TArray<int32>>> CPUCards;

	/** CPU marked states. */
	TArray<TArray<TArray<bool>>> CPUMarked;

	/** Timer handle driving automatic number calls. */
	FTimerHandle CallTimerHandle;

	/** Number of CPU opponents that have not yet bingo'd out. */
	int32 ActiveCPUCount = 0;

	// -- Internal helpers --------------------------------------------------

	/** Pull the next number from the pool, broadcast, and check CPU cards. */
	void CallNextNumber();

	/** Check all CPU cards for bingo and auto-mark the latest number. */
	void CheckAllCPUCards(int32 CalledNumber);

	/** Generate a valid 5x5 bingo card. */
	static TArray<TArray<int32>> GenerateCard();

	/** Return true if the given marked grid contains a completed bingo line. */
	static bool CheckBingo(const TArray<TArray<bool>>& Marked);

	/** Locate the (Row, Col) of a number on a card. Returns false if not found. */
	static bool FindNumberOnCard(const TArray<TArray<int32>>& Card, int32 Number, int32& OutRow, int32& OutCol);
};
