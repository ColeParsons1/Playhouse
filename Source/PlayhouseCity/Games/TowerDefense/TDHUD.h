// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "Games/TowerDefense/TDGameMode.h"
#include "TDHUD.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class USizeBox;
class UTextBlock;
class UUniformGridPanel;
class UTDGrid;
class ATDGameMode;

// ---------------------------------------------------------------------------
// UTDHUD
// ---------------------------------------------------------------------------

/**
 * Top-level HUD widget for the Tower Defense mini-game.
 *
 * Builds the game grid, tower selection palette, wave/lives/money
 * counters, and start-wave button programmatically in Initialize().
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UTDHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	// -- Widget lifecycle -----------------------------------------------------

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	// -- Public API -----------------------------------------------------------

	/** Show a wave-start announcement. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDHUD")
	void ShowWaveAnnouncement(int32 WaveNumber);

	/** Show a warning when lives are critically low. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDHUD")
	void ShowLivesWarning();

	/** Show the victory screen after surviving all waves. */
	UFUNCTION(BlueprintCallable, Category = "PHC|TDHUD")
	void ShowVictory();

protected:
	// -- Bind widgets ---------------------------------------------------------

	/** Canvas wrapping all TD-specific content below the top bar. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> TDCanvas;

	/** The embedded grid sub-widget (if using UTDGrid). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTDGrid> GridWidget;

	/** Uniform grid panel for the game board cells. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> BoardGrid;

	/** Flat array of cell border widgets [row * GridCols + col]. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> GridCellWidgets;

	/** Wave announcement overlay text. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WaveAnnouncementText;

	/** Enemy count during active waves. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> EnemyCountText;

	// -- Tower selection buttons ----------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ArrowTowerButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> CannonTowerButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> IceTowerButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> LightningTowerButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> StartWaveButton;

	// -- Status displays ------------------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> WaveText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> LivesText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MoneyText;

	// -- Dynamic visuals (enemy/tower images on the grid) ---------------------

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> EnemyImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> TowerImages;

	/** Currently selected tower type for placement. */
	UPROPERTY(BlueprintReadOnly, Category = "PHC|TDHUD")
	EPHCTowerType SelectedTowerType = EPHCTowerType::Arrow;

private:
	// -- Tower button callbacks -----------------------------------------------

	UFUNCTION() void OnArrowSelected();
	UFUNCTION() void OnCannonSelected();
	UFUNCTION() void OnIceSelected();
	UFUNCTION() void OnLightningSelected();
	UFUNCTION() void OnStartWaveClicked();

	// -- Grid cell click handling ---------------------------------------------

	UFUNCTION() void OnGridCell0();  UFUNCTION() void OnGridCell1();
	UFUNCTION() void OnGridCell2();  UFUNCTION() void OnGridCell3();
	UFUNCTION() void OnGridCell4();  UFUNCTION() void OnGridCell5();
	UFUNCTION() void OnGridCell6();  UFUNCTION() void OnGridCell7();
	UFUNCTION() void OnGridCell8();  UFUNCTION() void OnGridCell9();
	UFUNCTION() void OnGridCell10(); UFUNCTION() void OnGridCell11();
	UFUNCTION() void OnGridCell12(); UFUNCTION() void OnGridCell13();
	UFUNCTION() void OnGridCell14(); UFUNCTION() void OnGridCell15();
	UFUNCTION() void OnGridCell16(); UFUNCTION() void OnGridCell17();
	UFUNCTION() void OnGridCell18(); UFUNCTION() void OnGridCell19();
	UFUNCTION() void OnGridCell20(); UFUNCTION() void OnGridCell21();
	UFUNCTION() void OnGridCell22(); UFUNCTION() void OnGridCell23();
	UFUNCTION() void OnGridCell24(); UFUNCTION() void OnGridCell25();
	UFUNCTION() void OnGridCell26(); UFUNCTION() void OnGridCell27();
	UFUNCTION() void OnGridCell28(); UFUNCTION() void OnGridCell29();
	UFUNCTION() void OnGridCell30(); UFUNCTION() void OnGridCell31();
	UFUNCTION() void OnGridCell32(); UFUNCTION() void OnGridCell33();
	UFUNCTION() void OnGridCell34(); UFUNCTION() void OnGridCell35();
	UFUNCTION() void OnGridCell36(); UFUNCTION() void OnGridCell37();
	UFUNCTION() void OnGridCell38(); UFUNCTION() void OnGridCell39();
	UFUNCTION() void OnGridCell40(); UFUNCTION() void OnGridCell41();
	UFUNCTION() void OnGridCell42(); UFUNCTION() void OnGridCell43();
	UFUNCTION() void OnGridCell44(); UFUNCTION() void OnGridCell45();
	UFUNCTION() void OnGridCell46(); UFUNCTION() void OnGridCell47();

	void HandleGridCellClicked(int32 Index);

	// -- Helpers --------------------------------------------------------------

	/** Get the color for a grid cell value (0=grass, 1=path, 2=tower). */
	FLinearColor GetCellColor(int32 CellValue) const;

	/** Get the color for a tower type. */
	FLinearColor GetTowerColor(EPHCTowerType Type) const;

	/** Get the color for an enemy type. */
	FLinearColor GetEnemyColor(EPHCEnemyType Type) const;

	/** Update all grid cell visuals from the game mode grid data. */
	void RefreshGridCells();

	/** Update enemy image pool and positions. */
	void UpdateEnemyVisuals();

	/** Update tower images on the grid. */
	void UpdateTowerVisuals();

	/** Ensure the enemy image pool has enough images. */
	void EnsureEnemyImagePool(int32 Count);

	/** Cached reference to the TD game mode. */
	UPROPERTY(Transient)
	TObjectPtr<ATDGameMode> TDMode;

	/** Timer for clearing the wave announcement text. */
	FTimerHandle AnnouncementTimerHandle;

	/** Track the last known wave for detecting transitions. */
	int32 LastWave = 0;

	/** Grid dimensions for cell lookups. */
	static constexpr int32 GridCols = 8;
	static constexpr int32 GridRows = 6;

	/** Visual cell size in pixels (for enemy positioning). */
	float VisualCellSize = 40.f;

	/** Canvas panel that holds the grid and enemy/tower images. */
	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> GridOverlayCanvas;

	/** Array of cell buttons for click handling on the grid. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> GridCellButtons;
};
