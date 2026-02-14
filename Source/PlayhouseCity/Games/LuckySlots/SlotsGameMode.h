// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "SlotsGameMode.generated.h"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

/** Symbols that can appear on the slot reels. */
UENUM(BlueprintType)
enum class EPHCSlotSymbol : uint8
{
	Cherry		UMETA(DisplayName = "Cherry"),
	Lemon		UMETA(DisplayName = "Lemon"),
	Orange		UMETA(DisplayName = "Orange"),
	Plum		UMETA(DisplayName = "Plum"),
	Bell		UMETA(DisplayName = "Bell"),
	Bar			UMETA(DisplayName = "Bar"),
	Seven		UMETA(DisplayName = "Seven"),
	Diamond		UMETA(DisplayName = "Diamond"),
	Wild		UMETA(DisplayName = "Wild")
};

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotsSpinComplete, const TArray<EPHCSlotSymbol>&, Results);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSlotsWin, int32, PayoutAmount);

// ---------------------------------------------------------------------------
// ASlotsGameMode
// ---------------------------------------------------------------------------

/**
 * Lucky Slots mini-game mode.
 *
 * Three-reel slot machine with weighted symbol probabilities and a
 * configurable pay table. The player spins using their coin balance;
 * wins are multiplied by the bet amount and credited to their balance.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ASlotsGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	ASlotsGameMode();

	// -- Game flow --------------------------------------------------------

	virtual void StartGame() override;

	// -- Gameplay ---------------------------------------------------------

	/** Execute a spin. Deducts the bet, generates results, checks the pay line. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Slots")
	void Spin();

	/** Set the bet amount per spin. Clamped to [MinBet, MaxBet]. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Slots")
	void SetBet(int32 Amount);

	// -- Queries ----------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "PHC|Slots")
	int32 GetBetAmount() const { return BetAmount; }

	UFUNCTION(BlueprintPure, Category = "PHC|Slots")
	int32 GetBalance() const { return CoinBalance; }

	UFUNCTION(BlueprintPure, Category = "PHC|Slots")
	bool IsSpinning() const { return bSpinning; }

	UFUNCTION(BlueprintPure, Category = "PHC|Slots")
	TArray<EPHCSlotSymbol> GetLastResults() const { return ReelResults; }

	/** Retrieve the full pay table (symbol -> payout multiplier for 3-of-a-kind). */
	UFUNCTION(BlueprintPure, Category = "PHC|Slots")
	TMap<EPHCSlotSymbol, int32> GetPayTable() const { return PayTable; }

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|Slots")
	FOnSlotsSpinComplete OnSpinComplete;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Slots")
	FOnSlotsWin OnWin;

	// -- Tunables ---------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|Slots")
	int32 MinBet = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "PHC|Slots")
	int32 MaxBet = 100;

protected:
	// -- State ------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Slots")
	int32 BetAmount = 10;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Slots")
	int32 CoinBalance = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Slots")
	bool bSpinning = false;

	TArray<EPHCSlotSymbol> ReelResults;

	/** 3-of-a-kind payout multiplier for each symbol. */
	UPROPERTY(EditDefaultsOnly, Category = "PHC|Slots")
	TMap<EPHCSlotSymbol, int32> PayTable;

	// -- Internal ---------------------------------------------------------

	/** Generate a single reel symbol using weighted probabilities. */
	EPHCSlotSymbol GenerateSymbol() const;

	/** Check the payline and return the payout multiplier (0 = no win). */
	int32 CheckWin(const TArray<EPHCSlotSymbol>& Results) const;

	/** Initialise the default pay table. */
	void InitPayTable();

	/** Weighted probability table (symbol -> relative weight). */
	TMap<EPHCSlotSymbol, int32> SymbolWeights;

	/** Sum of all symbol weights (cached). */
	int32 TotalWeight = 0;

	/** Initialise symbol weights. */
	void InitWeights();
};
