// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/TowerDefense/TDGrid.h"
#include "Games/TowerDefense/TDGameMode.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

void UTDGrid::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind tower palette buttons.
	if (StartWaveButton)
	{
		StartWaveButton->OnClicked.AddDynamic(this, &UTDGrid::OnStartWaveClicked);
	}
	if (ArrowTowerButton)
	{
		ArrowTowerButton->OnClicked.AddDynamic(this, &UTDGrid::OnArrowSelected);
	}
	if (CannonTowerButton)
	{
		CannonTowerButton->OnClicked.AddDynamic(this, &UTDGrid::OnCannonSelected);
	}
	if (IceTowerButton)
	{
		IceTowerButton->OnClicked.AddDynamic(this, &UTDGrid::OnIceSelected);
	}
	if (LightningTowerButton)
	{
		LightningTowerButton->OnClicked.AddDynamic(this, &UTDGrid::OnLightningSelected);
	}

	// Default selection.
	SelectedTowerType = EPHCTowerType::Arrow;
}

void UTDGrid::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!TDMode || !TDMode->bGameActive)
	{
		return;
	}

	// Sync HUD displays from game mode state.
	UpdateGold(TDMode->Gold);
	UpdateLives(TDMode->Lives);
	UpdateWave(TDMode->Wave, TDMode->MaxWaves);
	UpdateEnemies(TDMode->Enemies);
	UpdateTowers(TDMode->Towers);

	// Disable start wave button during active wave.
	if (StartWaveButton)
	{
		StartWaveButton->SetIsEnabled(!TDMode->bWaveActive);
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UTDGrid::SetGameMode(ATDGameMode* InGameMode)
{
	TDMode = InGameMode;
}

void UTDGrid::InitializeGrid(const TArray<int32>& GridData, int32 Width, int32 Height)
{
	if (!GridCanvas)
	{
		return;
	}

	CachedGridWidth = Width;
	CachedGridHeight = Height;

	// Clear existing cell images.
	for (UImage* Img : GridCellImages)
	{
		if (Img)
		{
			Img->RemoveFromParent();
		}
	}
	GridCellImages.Empty();

	// Create cell images.
	for (int32 Y = 0; Y < Height; ++Y)
	{
		for (int32 X = 0; X < Width; ++X)
		{
			const int32 Index = Y * Width + X;
			const int32 CellValue = GridData.IsValidIndex(Index) ? GridData[Index] : 0;

			UImage* CellImage = NewObject<UImage>(this);
			if (!CellImage)
			{
				continue;
			}

			CellImage->SetColorAndOpacity(GetCellColor(CellValue));

			UCanvasPanelSlot* Slot = GridCanvas->AddChildToCanvas(CellImage);
			if (Slot)
			{
				Slot->SetPosition(FVector2D(X * VisualCellSize, Y * VisualCellSize));
				Slot->SetSize(FVector2D(VisualCellSize - 1.f, VisualCellSize - 1.f));
			}

			GridCellImages.Add(CellImage);
		}
	}
}

void UTDGrid::OnGridCellClicked(FIntPoint Pos)
{
	if (!TDMode)
	{
		return;
	}

	const bool bPlaced = TDMode->PlaceTower(Pos, SelectedTowerType);
	if (bPlaced)
	{
		// Update the grid cell visual.
		const int32 CellIdx = Pos.Y * CachedGridWidth + Pos.X;
		if (GridCellImages.IsValidIndex(CellIdx) && GridCellImages[CellIdx])
		{
			GridCellImages[CellIdx]->SetColorAndOpacity(GetCellColor(2));
		}
	}
}

void UTDGrid::OnTowerTypeSelected(EPHCTowerType Type)
{
	SelectedTowerType = Type;
}

// ---------------------------------------------------------------------------
// Visual updates
// ---------------------------------------------------------------------------

void UTDGrid::UpdateEnemies(const TArray<FPHCEnemy>& InEnemies)
{
	if (!GridCanvas)
	{
		return;
	}

	EnsureEnemyImagePool(InEnemies.Num());

	for (int32 i = 0; i < EnemyImages.Num(); ++i)
	{
		if (!EnemyImages[i])
		{
			continue;
		}

		if (i < InEnemies.Num())
		{
			EnemyImages[i]->SetVisibility(ESlateVisibility::Visible);
			EnemyImages[i]->SetColorAndOpacity(GetEnemyColor(InEnemies[i].Type));

			// Scale size based on enemy type.
			float EnemySize = 12.f;
			switch (InEnemies[i].Type)
			{
			case EPHCEnemyType::Basic:	EnemySize = 12.f; break;
			case EPHCEnemyType::Fast:	EnemySize = 10.f; break;
			case EPHCEnemyType::Tank:	EnemySize = 16.f; break;
			case EPHCEnemyType::Boss:	EnemySize = 20.f; break;
			}

			if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(EnemyImages[i]->Slot))
			{
				// Convert world position to canvas position.
				// The game mode uses CellSize for world units; we use VisualCellSize for display.
				const float Scale = (TDMode && TDMode->GetCellSize() > 0.f)
					? (VisualCellSize / TDMode->GetCellSize())
					: 1.f;

				const FVector2D CanvasPos(
					InEnemies[i].WorldPosition.X * Scale - EnemySize * 0.5f,
					InEnemies[i].WorldPosition.Y * Scale - EnemySize * 0.5f);

				Slot->SetPosition(CanvasPos);
				Slot->SetSize(FVector2D(EnemySize, EnemySize));
			}
		}
		else
		{
			EnemyImages[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UTDGrid::UpdateTowers(const TArray<FPHCTower>& InTowers)
{
	if (!GridCanvas)
	{
		return;
	}

	// Ensure we have enough tower images.
	while (TowerImages.Num() < InTowers.Num())
	{
		UImage* TowerImg = NewObject<UImage>(this);
		if (TowerImg)
		{
			GridCanvas->AddChildToCanvas(TowerImg);
			TowerImages.Add(TowerImg);
		}
	}

	for (int32 i = 0; i < TowerImages.Num(); ++i)
	{
		if (!TowerImages[i])
		{
			continue;
		}

		if (i < InTowers.Num())
		{
			TowerImages[i]->SetVisibility(ESlateVisibility::Visible);
			TowerImages[i]->SetColorAndOpacity(GetTowerColor(InTowers[i].Type));

			const float TowerSize = VisualCellSize * 0.8f;
			const FVector2D CellPos = GetGridCellPosition(InTowers[i].GridPos);

			if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(TowerImages[i]->Slot))
			{
				Slot->SetPosition(CellPos + FVector2D(
					(VisualCellSize - TowerSize) * 0.5f,
					(VisualCellSize - TowerSize) * 0.5f));
				Slot->SetSize(FVector2D(TowerSize, TowerSize));
			}
		}
		else
		{
			TowerImages[i]->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}

void UTDGrid::ShowProjectile(FVector2D From, FVector2D To)
{
	if (!GridCanvas)
	{
		return;
	}

	// Create a small projectile image that animates from source to target.
	UImage* Projectile = NewObject<UImage>(this);
	if (!Projectile)
	{
		return;
	}

	Projectile->SetColorAndOpacity(FLinearColor(1.f, 1.f, 0.2f, 1.f));

	UCanvasPanelSlot* Slot = GridCanvas->AddChildToCanvas(Projectile);
	if (Slot)
	{
		Slot->SetPosition(From);
		Slot->SetSize(FVector2D(6.f, 6.f));
	}

	// Schedule removal after brief display.
	if (UWorld* World = GetWorld())
	{
		FTimerHandle RemoveHandle;
		World->GetTimerManager().SetTimer(RemoveHandle, [Projectile]()
		{
			if (Projectile)
			{
				Projectile->RemoveFromParent();
			}
		}, 0.15f, false);
	}
}

void UTDGrid::ShowTowerRange(FIntPoint Pos, float Range)
{
	if (!GridCanvas)
	{
		return;
	}

	if (!RangeIndicator)
	{
		RangeIndicator = NewObject<UImage>(this);
		if (RangeIndicator)
		{
			GridCanvas->AddChildToCanvas(RangeIndicator);
		}
	}

	if (RangeIndicator)
	{
		RangeIndicator->SetVisibility(ESlateVisibility::Visible);
		RangeIndicator->SetColorAndOpacity(FLinearColor(0.2f, 0.6f, 1.f, 0.2f));

		const float Scale = (TDMode && TDMode->GetCellSize() > 0.f)
			? (VisualCellSize / TDMode->GetCellSize())
			: 1.f;
		const float VisualRange = Range * Scale;

		const FVector2D CellCenter = GetGridCellPosition(Pos) +
			FVector2D(VisualCellSize * 0.5f, VisualCellSize * 0.5f);

		if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(RangeIndicator->Slot))
		{
			Slot->SetPosition(CellCenter - FVector2D(VisualRange, VisualRange));
			Slot->SetSize(FVector2D(VisualRange * 2.f, VisualRange * 2.f));
		}
	}
}

void UTDGrid::HideTowerRange()
{
	if (RangeIndicator)
	{
		RangeIndicator->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UTDGrid::UpdateGold(int32 InGold)
{
	if (GoldText)
	{
		GoldText->SetText(FText::FromString(FString::Printf(TEXT("Gold: %d"), InGold)));
	}
}

void UTDGrid::UpdateLives(int32 InLives)
{
	if (LivesText)
	{
		LivesText->SetText(FText::FromString(FString::Printf(TEXT("Lives: %d"), InLives)));

		// Red tint when low on lives.
		const FLinearColor TextColor = (InLives <= 5)
			? FLinearColor(1.f, 0.3f, 0.3f, 1.f)
			: FLinearColor::White;
		LivesText->SetColorAndOpacity(FSlateColor(TextColor));
	}
}

void UTDGrid::UpdateWave(int32 InWave, int32 InMaxWaves)
{
	if (WaveText)
	{
		WaveText->SetText(FText::FromString(
			FString::Printf(TEXT("Wave: %d / %d"), InWave, InMaxWaves)));
	}
}

FVector2D UTDGrid::GetGridCellPosition(FIntPoint Cell) const
{
	return FVector2D(
		Cell.X * VisualCellSize,
		Cell.Y * VisualCellSize);
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UTDGrid::OnStartWaveClicked()
{
	if (TDMode)
	{
		TDMode->StartWave();
	}
}

void UTDGrid::OnArrowSelected()
{
	OnTowerTypeSelected(EPHCTowerType::Arrow);
}

void UTDGrid::OnCannonSelected()
{
	OnTowerTypeSelected(EPHCTowerType::Cannon);
}

void UTDGrid::OnIceSelected()
{
	OnTowerTypeSelected(EPHCTowerType::Ice);
}

void UTDGrid::OnLightningSelected()
{
	OnTowerTypeSelected(EPHCTowerType::Lightning);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

FLinearColor UTDGrid::GetCellColor(int32 CellValue) const
{
	switch (CellValue)
	{
	case 0: return FLinearColor(0.15f, 0.25f, 0.1f, 1.f);  // Grass / buildable
	case 1: return FLinearColor(0.6f, 0.5f, 0.3f, 1.f);    // Path
	case 2: return FLinearColor(0.3f, 0.3f, 0.4f, 1.f);    // Tower
	default: return FLinearColor(0.1f, 0.1f, 0.1f, 1.f);
	}
}

FLinearColor UTDGrid::GetEnemyColor(EPHCEnemyType Type) const
{
	switch (Type)
	{
	case EPHCEnemyType::Basic:	return FLinearColor(1.f, 0.2f, 0.2f, 1.f);
	case EPHCEnemyType::Fast:	return FLinearColor(1.f, 0.8f, 0.1f, 1.f);
	case EPHCEnemyType::Tank:	return FLinearColor(0.4f, 0.2f, 0.6f, 1.f);
	case EPHCEnemyType::Boss:	return FLinearColor(0.8f, 0.1f, 0.5f, 1.f);
	default:					return FLinearColor::Red;
	}
}

FLinearColor UTDGrid::GetTowerColor(EPHCTowerType Type) const
{
	switch (Type)
	{
	case EPHCTowerType::Arrow:		return FLinearColor(0.2f, 0.7f, 0.2f, 1.f);
	case EPHCTowerType::Cannon:		return FLinearColor(0.6f, 0.4f, 0.1f, 1.f);
	case EPHCTowerType::Ice:		return FLinearColor(0.3f, 0.7f, 1.f, 1.f);
	case EPHCTowerType::Lightning:	return FLinearColor(0.9f, 0.9f, 0.2f, 1.f);
	default:						return FLinearColor::White;
	}
}

void UTDGrid::EnsureEnemyImagePool(int32 Count)
{
	if (!GridCanvas)
	{
		return;
	}

	while (EnemyImages.Num() < Count)
	{
		UImage* EnemyImg = NewObject<UImage>(this);
		if (EnemyImg)
		{
			GridCanvas->AddChildToCanvas(EnemyImg);
			EnemyImages.Add(EnemyImg);
		}
	}
}
