// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "LudoHUD.generated.h"

class ULudoBoard;
class UImage;
class UTextBlock;
class ALudoGameMode;

// ---------------------------------------------------------------------------
// ULudoHUD
// ---------------------------------------------------------------------------

/**
 * HUD widget for the Ludo mini-game. Embeds the LudoBoard widget and
 * provides player turn indicators, dice interaction, and piece selection.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API ULudoHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Bind to the Ludo-specific game mode and wire up delegates. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Ludo")
	void SetLudoGameMode(ALudoGameMode* InGameMode);

protected:
	// -- Bind widgets -----------------------------------------------------

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<ULudoBoard> LudoBoardWidget;

	/** Colored indicators showing whose turn it is (4 images). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> RedIndicator;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> BlueIndicator;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> GreenIndicator;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> YellowIndicator;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerTurnLabel;

private:
	UPROPERTY(Transient)
	TObjectPtr<ALudoGameMode> LudoGameModeRef;

	// -- Delegate handlers ------------------------------------------------

	UFUNCTION()
	void HandleDiceClicked();

	UFUNCTION()
	void HandlePieceClicked(int32 PieceIndex);

	UFUNCTION()
	void HandleDiceRolled(int32 Value);

	UFUNCTION()
	void HandlePieceMoved(int32 PlayerIndex, int32 PieceIndex);

	UFUNCTION()
	void HandleTurnChanged(int32 NewPlayerIndex);

	UFUNCTION()
	void HandlePieceCaptured(int32 CapturedPlayerIndex, int32 CapturedPieceIndex);

	/** Update the turn indicator images to highlight the active player. */
	void UpdateTurnIndicators(int32 ActivePlayerIndex);

	/** Refresh the board display from current game state. */
	void RefreshBoard();
};
