// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "Games/Carrom/CarromPiece.h"
#include "CarromGameMode.generated.h"

class ACarromPiece;
class ACarromBoard;

// ---------------------------------------------------------------------------
// ACarromGameMode
// ---------------------------------------------------------------------------

/**
 * Carrom game mode. Player (white coins) vs CPU (black coins) on a
 * square board. Flick the striker to pocket your coins. The red queen
 * must be "covered" (followed by pocketing one of your own coins
 * immediately after). First to pocket all own coins plus cover the
 * queen wins.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ACarromGameMode : public APHCMiniGameMode
{
	GENERATED_BODY()

public:
	ACarromGameMode();

	// -- Lifecycle --------------------------------------------------------

	virtual void StartGame() override;
	virtual void Tick(float DeltaTime) override;

	// -- Player actions ---------------------------------------------------

	/** Flick the striker in the given direction with the given power. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Carrom")
	void FlickStriker(FVector2D Direction, float Power);

	// -- State (read-only in Blueprint) -----------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Carrom")
	TArray<TObjectPtr<ACarromPiece>> Pieces;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Carrom")
	TObjectPtr<ACarromPiece> Striker;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Carrom")
	TObjectPtr<ACarromBoard> Board;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Carrom")
	bool bPlayerTurn = true;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Carrom")
	int32 PlayerScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Carrom")
	int32 CPUScore = 0;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Carrom")
	bool bQueenCovered = false;

	UPROPERTY(BlueprintReadOnly, Category = "PHC|Carrom")
	bool bPiecesMoving = false;

private:
	// -- Internal ---------------------------------------------------------

	void SpawnBoardAndPieces();

	/** Arrange 19 coins in the standard centre-circle pattern. */
	TArray<FVector> GetCenterArrangement(FVector Center, float CoinDiam) const;

	/** Evaluate the turn result after all pieces have stopped. */
	void EvaluateTurn();

	/** Handle a piece entering a pocket. */
	void OnPiecePocketed(ACarromPiece* Piece);

	/** Check foul conditions. */
	bool CheckFoul() const;

	/** Reset striker to the current player's baseline. */
	void ResetStriker();

	/** Check win / loss conditions. */
	void CheckGameOver();

	/** CPU AI: pick a direction and power to flick. */
	void CPUTurn();

	/** Find the best target piece for the CPU. */
	ACarromPiece* FindBestCPUTarget() const;

	// Board pocket callback
	UFUNCTION()
	void HandlePiecePocketed(ACarromPiece* Piece);

	/** Are any pieces still moving? */
	bool ArePiecesMoving() const;

	/** Pieces pocketed during the current shot. */
	TArray<TObjectPtr<ACarromPiece>> ShotPocketed;

	/** Was the striker pocketed this shot? */
	bool bStrikerPocketed = false;

	/** Did the player pocket at least one of their own pieces? */
	bool bPocketedOwnPiece = false;

	/** Was the queen pocketed this shot (needs covering)? */
	bool bQueenPocketedThisShot = false;

	/** Pending queen cover: the player who pocketed the queen must
	 *  pocket one of their own coins on the very next shot. */
	bool bQueenPendingCover = false;
	bool bQueenPendingCoverByPlayer = true;

	/** CPU think delay. */
	float CPUThinkTimer = 0.f;
	bool bCPUThinking = false;
	static constexpr float CPUThinkDelay = 1.0f;

	int32 PlayerPocketed = 0; // count of white coins pocketed
	int32 CPUPocketed = 0;    // count of black coins pocketed

	static constexpr float CoinDiameter = 3.2f;
};
