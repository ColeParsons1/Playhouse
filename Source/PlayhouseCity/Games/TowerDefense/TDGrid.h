// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Games/TowerDefense/TDGameMode.h"
#include "TDGrid.generated.h"

class UCanvasPanel;
class UImage;
class UTextBlock;
class UButton;
class UCanvasPanelSlot;
class ATDGameMode;

// ---------------------------------------------------------------------------
// UTDGrid
// ---------------------------------------------------------------------------

/**
 * Widget that renders the tower defense grid, towers, enemies, and
 * provides interactive controls for tower placement and management.
 *
 * The grid is drawn as a set of UImage cells within a UCanvasPanel.
 * Enemy positions are updated each tick from the game mode state.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UTDGrid : public UUserWidget
{
	GENERATED_BODY()

public:
	// -- Widget lifecycle -----------------------------------------------------

	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// -- Grid initialization --------------------------------------------------

	/** Build the visual grid from game mode grid data. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void InitializeGrid(const TArray<int32>& GridData, int32 Width, int32 Height);

	// -- Interactive ----------------------------------------------------------

	/** Handle a click/tap on a grid cell. Places the selected tower type. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void OnGridCellClicked(FIntPoint Pos);

	/** Select a tower type from the palette. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void OnTowerTypeSelected(EPHCTowerType Type);

	// -- Visual updates -------------------------------------------------------

	/** Update all enemy visuals from game mode data. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void UpdateEnemies(const TArray<FPHCEnemy>& InEnemies);

	/** Update tower visuals from game mode data. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void UpdateTowers(const TArray<FPHCTower>& InTowers);

	/** Show a brief projectile animation from source to target. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void ShowProjectile(FVector2D From, FVector2D To);

	/** Highlight the range circle for a tower at the given grid position. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void ShowTowerRange(FIntPoint Pos, float Range);

	/** Hide any active range indicator. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void HideTowerRange();

	/** Update the gold display. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void UpdateGold(int32 InGold);

	/** Update the lives display. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void UpdateLives(int32 InLives);

	/** Update the wave display. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void UpdateWave(int32 InWave, int32 InMaxWaves);

	/** Convert a grid cell coordinate to a canvas pixel position. */
	UFUNCTION(BlueprintPure, Category = "PHC|TDGrid")
	FVector2D GetGridCellPosition(FIntPoint Cell) const;

	/** Set the game mode reference. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDGrid")
	void SetGameMode(ATDGameMode* InGameMode);

protected:
	// -- Bind widgets ---------------------------------------------------------

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UCanvasPanel> GridCanvas;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> LivesText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UTextBlock> WaveText;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> StartWaveButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> ArrowTowerButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> CannonTowerButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> IceTowerButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> LightningTowerButton;

	// -- Dynamic visuals ------------------------------------------------------

	/** Cell images forming the grid background. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> GridCellImages;

	/** Images representing enemies on the field. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> EnemyImages;

	/** Images representing towers on the grid. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> TowerImages;

	/** The currently selected tower type for placement. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TDGrid")
	EPHCTowerType SelectedTowerType = EPHCTowerType::Arrow;

	/** Range indicator image (shown when hovering a tower). */
	UPROPERTY(Transient)
	TObjectPtr<UImage> RangeIndicator;

private:
	// -- Button callbacks -----------------------------------------------------

	UFUNCTION()
	void OnStartWaveClicked();

	UFUNCTION()
	void OnArrowSelected();

	UFUNCTION()
	void OnCannonSelected();

	UFUNCTION()
	void OnIceSelected();

	UFUNCTION()
	void OnLightningSelected();

	// -- Helpers --------------------------------------------------------------

	/** Get the color for a grid cell based on its type. */
	FLinearColor GetCellColor(int32 CellValue) const;

	/** Get the color for an enemy based on its type. */
	FLinearColor GetEnemyColor(EPHCEnemyType Type) const;

	/** Get the color for a tower based on its type. */
	FLinearColor GetTowerColor(EPHCTowerType Type) const;

	/** Ensure the enemy image pool has enough images. */
	void EnsureEnemyImagePool(int32 Count);

	/** Cached game mode reference. */
	UPROPERTY(Transient)
	TObjectPtr<ATDGameMode> TDMode;

	/** Visual cell size on the canvas. */
	float VisualCellSize = 40.f;

	/** Grid dimensions cached from initialization. */
	int32 CachedGridWidth = 0;
	int32 CachedGridHeight = 0;
};
