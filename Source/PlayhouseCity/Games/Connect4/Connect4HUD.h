// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "Connect4HUD.generated.h"

class AConnect4GameMode;
class UButton;
class UBorder;
class UTextBlock;
class UUniformGridPanel;

/**
 * HUD for the Connect 4 mini-game.
 *
 * Displays a 7x6 board grid with colored cells, column drop buttons,
 * and a status label showing whose turn it is.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UConnect4HUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;

protected:
	UPROPERTY(Transient)
	TObjectPtr<AConnect4GameMode> Connect4Mode;

	UPROPERTY()
	TObjectPtr<UTextBlock> TurnStatusText;

	UPROPERTY()
	TObjectPtr<UUniformGridPanel> BoardGrid;

	/** 7x6 grid of border widgets (indexed [col * 6 + row]). Row 0 = bottom. */
	UPROPERTY()
	TArray<TObjectPtr<UBorder>> CellWidgets;

	/** 7 column drop buttons. */
	UPROPERTY()
	TArray<TObjectPtr<UButton>> ColumnButtons;

private:
	void UpdateCellColor(int32 Col, int32 Row, int32 Player);
	void UpdateAllCells();
	void SetColumnButtonsEnabled(bool bEnabled);

	UFUNCTION()
	void HandlePiecePlaced(int32 Column, int32 Row, int32 Player);

	UFUNCTION()
	void HandleTurnChanged(bool bIsPlayerTurn);

	UFUNCTION()
	void HandleGameOver(int32 Winner);

	// Column button callbacks (one per column).
	UFUNCTION() void OnCol0() { DropInColumn(0); }
	UFUNCTION() void OnCol1() { DropInColumn(1); }
	UFUNCTION() void OnCol2() { DropInColumn(2); }
	UFUNCTION() void OnCol3() { DropInColumn(3); }
	UFUNCTION() void OnCol4() { DropInColumn(4); }
	UFUNCTION() void OnCol5() { DropInColumn(5); }
	UFUNCTION() void OnCol6() { DropInColumn(6); }

	void DropInColumn(int32 Col);

	static constexpr int32 NUM_COLS = 7;
	static constexpr int32 NUM_ROWS = 6;
};
