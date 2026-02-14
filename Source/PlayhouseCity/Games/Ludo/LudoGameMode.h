// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "LudoGameMode.generated.h"

// ---------------------------------------------------------------------------
// Enums & Structs
// ---------------------------------------------------------------------------

/** The four player colors in Ludo. */
UENUM(BlueprintType)
enum class EPHCLudoColor : uint8
{
	Red		UMETA(DisplayName = "Red"),
	Blue	UMETA(DisplayName = "Blue"),
	Green	UMETA(DisplayName = "Green"),
	Yellow	UMETA(DisplayName = "Yellow")
};

/** Represents a single Ludo piece on the board. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCLudoPiece
{
	GENERATED_BODY()

	/** Position on the track. -1 = home yard, 0-51 = shared track,
	 *  52-57 = home column (color-specific final stretch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Ludo")
	int32 Position = -1;

	/** True when the piece is still in the starting home yard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Ludo")
	bool bAtHome = true;

	/** True when the piece has reached the center and is done. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Ludo")
	bool bFinished = false;

	/** Owner color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Ludo")
	EPHCLudoColor Color = EPHCLudoColor::Red;
};

/** Per-player state (4 pieces + metadata). */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCLudoPlayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Ludo")
	EPHCLudoColor Color = EPHCLudoColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Ludo")
	TArray<FPHCLudoPiece> Pieces;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Ludo")
	bool bIsHuman = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Ludo")
	bool bHasFinished = false;
};

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLudoDiceRolled, int32, Value);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLudoPieceMoved, int32, PlayerIndex, int32, PieceIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLudoTurnChanged, int32, NewPlayerIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnLudoPieceCaptured, int32, CapturedPlayerIndex, int32, CapturedPieceIndex);

// ---------------------------------------------------------------------------
// ALudoGameMode
// ---------------------------------------------------------------------------

/**
 * Full Ludo (Parcheesi-family) board game for 4 players (1 human + 3 CPU).
 *
 * Classic rules: roll 6 to leave home, capture opponents by landing on them,
 * safe zones at star positions, 6-square home columns, extra turn on 6.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ALudoGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	ALudoGameMode();

	// -- Lifecycle --------------------------------------------------------

	virtual void BeginPlay() override;
	virtual void StartGame() override;

	// -- Core gameplay ----------------------------------------------------

	/** Roll the dice (1-6). Must roll 6 to leave home. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	int32 RollDice();

	/** Move the specified piece for the current player by the dice value. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	void MovePiece(int32 PieceIndex);

	/** Check if a particular piece can legally move with the current dice. */
	UFUNCTION(BlueprintPure, Category = "PHC|Ludo")
	bool CanMovePiece(int32 PieceIndex) const;

	/** Get indices of all pieces that can legally move right now. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	TArray<int32> GetValidMoves() const;

	/** Check if an opponent piece is at the given absolute position and capture it. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	void CheckCapture(int32 AbsolutePosition, EPHCLudoColor MovingColor);

	/** True if all 4 pieces of the given player have finished. */
	UFUNCTION(BlueprintPure, Category = "PHC|Ludo")
	bool CheckFinished(int32 PlayerIndex) const;

	/** Execute the CPU player's turn automatically. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	void CPUTurn();

	/** Advance to the next player's turn (extra turn on 6). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	void NextTurn();

	/** True if the given absolute board position is a safe zone. */
	UFUNCTION(BlueprintPure, Category = "PHC|Ludo")
	bool IsSafeZone(int32 AbsolutePosition) const;

	/** Convert a relative track position (0-56) to an absolute board position
	 *  accounting for each color's start offset. */
	UFUNCTION(BlueprintPure, Category = "PHC|Ludo")
	int32 GetAbsolutePosition(int32 RelativePos, EPHCLudoColor Color) const;

	// -- Accessors --------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "PHC|Ludo")
	int32 GetDiceValue() const { return DiceValue; }

	UFUNCTION(BlueprintPure, Category = "PHC|Ludo")
	int32 GetCurrentPlayerIndex() const { return CurrentPlayerIndex; }

	UFUNCTION(BlueprintPure, Category = "PHC|Ludo")
	const TArray<FPHCLudoPlayer>& GetPlayers() const { return Players; }

	UFUNCTION(BlueprintPure, Category = "PHC|Ludo")
	TArray<FPHCLudoPiece> GetAllPieces() const;

	// -- Delegates --------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "PHC|Ludo")
	FOnLudoDiceRolled OnDiceRolled;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Ludo")
	FOnLudoPieceMoved OnPieceMoved;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Ludo")
	FOnLudoTurnChanged OnTurnChanged;

	UPROPERTY(BlueprintAssignable, Category = "PHC|Ludo")
	FOnLudoPieceCaptured OnPieceCaptured;

protected:
	// -- State ------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Ludo")
	int32 DiceValue = 0;

	/** 0 = human, 1-3 = CPU. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Ludo")
	int32 CurrentPlayerIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Ludo")
	TArray<FPHCLudoPlayer> Players;

	/** True after dice has been rolled, waiting for piece selection. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Ludo")
	bool bDiceRolled = false;

	/** Count of consecutive 6-rolls (3 in a row = lose turn). */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Ludo")
	int32 ConsecutiveSixes = 0;

	/** Finishing order tracking. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|Ludo")
	TArray<int32> FinishOrder;

public:
	// -- Board constants --------------------------------------------------

	/** Number of shared squares on the outer track. */
	static constexpr int32 TRACK_LENGTH = 52;

	/** Length of each color's home column (final stretch). */
	static constexpr int32 HOME_COLUMN_LENGTH = 6;

	/** Start square offset on the shared track for each color.
	 *  Red=0, Blue=13, Green=26, Yellow=39. */
	static int32 GetStartOffset(EPHCLudoColor Color);

	/** Entry square for the home column (relative to color start). */
	static constexpr int32 HOME_ENTRY_RELATIVE = 51;

	/** Safe zone positions on the absolute track (star squares). */
	static const TArray<int32>& GetSafeZones();

protected:

private:
	/** Internal: compute where a piece would land given current dice. */
	int32 ComputeNewPosition(const FPHCLudoPiece& Piece, int32 DiceRoll) const;

	/** Timer handle for CPU turn delay. */
	FTimerHandle CPUTimerHandle;
};
