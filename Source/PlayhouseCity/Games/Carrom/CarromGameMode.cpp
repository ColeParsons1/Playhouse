// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Carrom/CarromGameMode.h"
#include "Games/Carrom/CarromHUD.h"
#include "Games/Carrom/CarromPiece.h"
#include "Games/Carrom/CarromBoard.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------

ACarromGameMode::ACarromGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	GameType = EPHCGameType::Carrom;
	GameHUDClass = UCarromHUD::StaticClass();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void ACarromGameMode::StartGame()
{
	Super::StartGame();

	Pieces.Empty();
	ShotPocketed.Empty();
	bPlayerTurn = true;
	PlayerScore = 0;
	CPUScore = 0;
	PlayerPocketed = 0;
	CPUPocketed = 0;
	bQueenCovered = false;
	bPiecesMoving = false;
	bQueenPendingCover = false;
	bCPUThinking = false;

	SpawnBoardAndPieces();
}

// ---------------------------------------------------------------------------

void ACarromGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive)
	{
		return;
	}

	// CPU thinking delay
	if (bCPUThinking)
	{
		CPUThinkTimer += DeltaTime;
		if (CPUThinkTimer >= CPUThinkDelay)
		{
			bCPUThinking = false;
			CPUThinkTimer = 0.f;
			CPUTurn();
		}
		return;
	}

	// Check if all pieces have settled
	const bool bMoving = ArePiecesMoving();
	if (bPiecesMoving && !bMoving)
	{
		bPiecesMoving = false;
		EvaluateTurn();
	}
	bPiecesMoving = bMoving;
}

// ---------------------------------------------------------------------------
// Player action
// ---------------------------------------------------------------------------

void ACarromGameMode::FlickStriker(FVector2D Direction, float Power)
{
	if (!Striker || bPiecesMoving)
	{
		return;
	}

	// Reset per-shot tracking
	ShotPocketed.Empty();
	bStrikerPocketed = false;
	bPocketedOwnPiece = false;
	bQueenPocketedThisShot = false;

	const FVector Impulse = FVector(Direction.X, Direction.Y, 0.f).GetSafeNormal()
		* FMath::Clamp(Power, 0.f, 1.f) * 400.f;

	if (Striker->CollisionDisc)
	{
		Striker->CollisionDisc->AddImpulse(Impulse, NAME_None, true);
	}

	bPiecesMoving = true;
}

// ---------------------------------------------------------------------------
// Spawning
// ---------------------------------------------------------------------------

void ACarromGameMode::SpawnBoardAndPieces()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FVector Origin = FVector::ZeroVector;

	Board = World->SpawnActor<ACarromBoard>(ACarromBoard::StaticClass(), Origin, FRotator::ZeroRotator, SP);
	if (Board)
	{
		Board->OnPiecePocketed.AddDynamic(this, &ACarromGameMode::HandlePiecePocketed);
	}

	const float Z = CoinDiameter * 0.5f + 1.f;
	const FVector Center = Origin + FVector(0.f, 0.f, Z);

	TArray<FVector> Positions = GetCenterArrangement(Center, CoinDiameter);

	// Spawn 9 white, 9 black in alternating ring pattern, plus queen in the very centre
	// Arrangement: index 0 = centre (queen), 1-6 inner ring, 7-18 outer ring
	int32 WhiteCount = 0;
	int32 BlackCount = 0;

	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		ACarromPiece* Piece = World->SpawnActor<ACarromPiece>(
			ACarromPiece::StaticClass(), Positions[i], FRotator::ZeroRotator, SP);
		if (!Piece)
		{
			continue;
		}

		if (i == 0)
		{
			// Centre = Queen
			Piece->InitPiece(EPHCCarromPieceType::Queen);
		}
		else if (i % 2 == 1 && WhiteCount < 9)
		{
			Piece->InitPiece(EPHCCarromPieceType::White);
			++WhiteCount;
		}
		else if (BlackCount < 9)
		{
			Piece->InitPiece(EPHCCarromPieceType::Black);
			++BlackCount;
		}
		else
		{
			Piece->InitPiece(EPHCCarromPieceType::White);
			++WhiteCount;
		}

		Pieces.Add(Piece);
	}

	// Spawn striker on player's baseline (bottom)
	const float BaselineY = Board ? -(Board->BoardSize * 0.5f - 8.f) : -28.f;
	const FVector StrikerPos = Origin + FVector(0.f, BaselineY, Z);
	Striker = World->SpawnActor<ACarromPiece>(
		ACarromPiece::StaticClass(), StrikerPos, FRotator::ZeroRotator, SP);
	if (Striker)
	{
		Striker->InitPiece(EPHCCarromPieceType::Striker);
		Pieces.Add(Striker);
	}
}

// ---------------------------------------------------------------------------

TArray<FVector> ACarromGameMode::GetCenterArrangement(FVector Center, float CoinDiam) const
{
	TArray<FVector> Positions;

	// Centre (queen)
	Positions.Add(Center);

	// Inner ring: 6 coins
	const float InnerRadius = CoinDiam * 1.1f;
	for (int32 i = 0; i < 6; ++i)
	{
		const float Angle = FMath::DegreesToRadians(i * 60.f);
		Positions.Add(Center + FVector(FMath::Cos(Angle) * InnerRadius,
										FMath::Sin(Angle) * InnerRadius, 0.f));
	}

	// Outer ring: 12 coins
	const float OuterRadius = CoinDiam * 2.2f;
	for (int32 i = 0; i < 12; ++i)
	{
		const float Angle = FMath::DegreesToRadians(i * 30.f + 15.f);
		Positions.Add(Center + FVector(FMath::Cos(Angle) * OuterRadius,
										FMath::Sin(Angle) * OuterRadius, 0.f));
	}

	return Positions; // 1 + 6 + 12 = 19 positions
}

// ---------------------------------------------------------------------------
// Pocket handling
// ---------------------------------------------------------------------------

void ACarromGameMode::HandlePiecePocketed(ACarromPiece* Piece)
{
	if (!Piece || Piece->bPocketed)
	{
		return;
	}

	ShotPocketed.Add(Piece);
	OnPiecePocketed(Piece);
}

// ---------------------------------------------------------------------------

void ACarromGameMode::OnPiecePocketed(ACarromPiece* Piece)
{
	if (!Piece)
	{
		return;
	}

	if (Piece->PieceType == EPHCCarromPieceType::Striker)
	{
		bStrikerPocketed = true;
		Piece->OnPocketed();
		return;
	}

	if (Piece->PieceType == EPHCCarromPieceType::Queen)
	{
		bQueenPocketedThisShot = true;
		Piece->OnPocketed();
		return;
	}

	Piece->OnPocketed();

	// Track scoring
	const EPHCCarromPieceType OwnType = bPlayerTurn
		? EPHCCarromPieceType::White
		: EPHCCarromPieceType::Black;

	if (Piece->PieceType == OwnType)
	{
		bPocketedOwnPiece = true;
		if (bPlayerTurn)
		{
			++PlayerPocketed;
			PlayerScore += 1;
		}
		else
		{
			++CPUPocketed;
			CPUScore += 1;
		}
		AddScore(bPlayerTurn ? 10 : 0);
	}
}

// ---------------------------------------------------------------------------
// Turn evaluation
// ---------------------------------------------------------------------------

void ACarromGameMode::EvaluateTurn()
{
	// Handle queen covering rules
	if (bQueenPocketedThisShot)
	{
		if (bPocketedOwnPiece)
		{
			// Queen covered immediately
			bQueenCovered = true;
			const int32 QueenPoints = 3;
			if (bPlayerTurn)
			{
				PlayerScore += QueenPoints;
			}
			else
			{
				CPUScore += QueenPoints;
			}
		}
		else
		{
			// Queen needs covering on the next shot
			bQueenPendingCover = true;
			bQueenPendingCoverByPlayer = bPlayerTurn;
		}
	}

	// Handle pending queen cover from a previous shot
	if (bQueenPendingCover && !bQueenPocketedThisShot)
	{
		const bool bSamePlayer = (bQueenPendingCoverByPlayer == bPlayerTurn);
		if (bSamePlayer && bPocketedOwnPiece)
		{
			// Successfully covered the queen
			bQueenCovered = true;
			bQueenPendingCover = false;
			const int32 QueenPoints = 3;
			if (bPlayerTurn)
			{
				PlayerScore += QueenPoints;
			}
			else
			{
				CPUScore += QueenPoints;
			}
		}
		else if (bSamePlayer && !bPocketedOwnPiece)
		{
			// Failed to cover: queen goes back to centre
			bQueenPendingCover = false;
			// Respawn queen in centre
			for (const TObjectPtr<ACarromPiece>& P : Pieces)
			{
				if (P && P->PieceType == EPHCCarromPieceType::Queen && P->bPocketed)
				{
					const float Z = CoinDiameter * 0.5f + 1.f;
					P->ResetToPosition(FVector(0.f, 0.f, Z));
					break;
				}
			}
		}
	}

	// Handle striker foul
	if (bStrikerPocketed)
	{
		// Penalty: return one previously pocketed coin of the fouling player
		// For simplicity we just reset the striker
		ResetStriker();
		bPlayerTurn = !bPlayerTurn;
	}
	else if (!bPocketedOwnPiece)
	{
		// Didn't pocket anything: switch turns
		bPlayerTurn = !bPlayerTurn;
	}
	// else: pocketed own piece -> same player continues

	// Always reset striker position for next shot
	ResetStriker();

	// Check game over
	CheckGameOver();

	// If CPU's turn, start thinking
	if (!bPlayerTurn && bGameActive)
	{
		bCPUThinking = true;
		CPUThinkTimer = 0.f;
	}
}

// ---------------------------------------------------------------------------

bool ACarromGameMode::CheckFoul() const
{
	return bStrikerPocketed;
}

// ---------------------------------------------------------------------------

void ACarromGameMode::ResetStriker()
{
	if (!Striker)
	{
		return;
	}

	const float BaselineOffset = Board ? (Board->BoardSize * 0.5f - 8.f) : 28.f;
	const float BaselineY = bPlayerTurn ? -BaselineOffset : BaselineOffset;
	const float Z = CoinDiameter * 0.5f + 1.f;

	Striker->ResetToPosition(FVector(0.f, BaselineY, Z));
}

// ---------------------------------------------------------------------------

void ACarromGameMode::CheckGameOver()
{
	// Need queen covered and all own coins pocketed
	const int32 RequiredCoins = 9;

	if (PlayerPocketed >= RequiredCoins && bQueenCovered)
	{
		EndGame(true);
		return;
	}
	if (CPUPocketed >= RequiredCoins && bQueenCovered)
	{
		EndGame(false);
		return;
	}
}

// ---------------------------------------------------------------------------
// CPU AI
// ---------------------------------------------------------------------------

void ACarromGameMode::CPUTurn()
{
	if (!Striker || Striker->bPocketed)
	{
		ResetStriker();
	}

	ACarromPiece* Target = FindBestCPUTarget();
	if (!Target)
	{
		// Random flick
		const float Angle = FMath::FRandRange(0.f, 360.f);
		const FVector2D Dir(FMath::Cos(FMath::DegreesToRadians(Angle)),
							FMath::Sin(FMath::DegreesToRadians(Angle)));
		FlickStriker(Dir, 0.4f);
		return;
	}

	// Direction from striker to target
	FVector ToTarget = Target->GetActorLocation() - Striker->GetActorLocation();
	ToTarget.Z = 0.f;
	FVector2D Dir2D(ToTarget.X, ToTarget.Y);
	Dir2D.Normalize();

	// Add inaccuracy
	const float Inaccuracy = FMath::FRandRange(-0.1f, 0.1f);
	Dir2D = Dir2D.GetRotated(FMath::RadiansToDegrees(Inaccuracy));

	const float Dist = ToTarget.Size();
	const float Power = FMath::Clamp(Dist / 60.f, 0.3f, 0.9f);

	FlickStriker(Dir2D, Power);
}

// ---------------------------------------------------------------------------

ACarromPiece* ACarromGameMode::FindBestCPUTarget() const
{
	if (!Striker)
	{
		return nullptr;
	}

	ACarromPiece* Best = nullptr;
	float BestDist = TNumericLimits<float>::Max();

	const FVector StrikerPos = Striker->GetActorLocation();

	for (const TObjectPtr<ACarromPiece>& P : Pieces)
	{
		if (!P || P->bPocketed || P == Striker)
		{
			continue;
		}

		// CPU plays black
		if (P->PieceType != EPHCCarromPieceType::Black &&
			P->PieceType != EPHCCarromPieceType::Queen)
		{
			continue;
		}

		// If queen, only target if most black coins are pocketed
		if (P->PieceType == EPHCCarromPieceType::Queen && CPUPocketed < 7)
		{
			continue;
		}

		const float Dist = FVector::Dist(StrikerPos, P->GetActorLocation());
		if (Dist < BestDist)
		{
			BestDist = Dist;
			Best = P.Get();
		}
	}

	return Best;
}

// ---------------------------------------------------------------------------

bool ACarromGameMode::ArePiecesMoving() const
{
	for (const TObjectPtr<ACarromPiece>& P : Pieces)
	{
		if (P && P->IsMoving())
		{
			return true;
		}
	}
	return false;
}
