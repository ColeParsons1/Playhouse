// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "CarromHUD.generated.h"

class UProgressBar;
class UTextBlock;
class UCanvasPanel;
class UImage;
class UBorder;
class UButton;
class ACarromGameMode;

// ---------------------------------------------------------------------------
// UCarromHUD
// ---------------------------------------------------------------------------

/**
 * Carrom HUD overlay.
 * Shows flick power/direction indicator, score display, turn info,
 * and queen status.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API UCarromHUD : public UPHCMiniGameHUD
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	/** Update the flick power indicator (0..1). */
	UFUNCTION(BlueprintCallable, Category = "PHC|Carrom")
	void SetFlickPower(float Power);

	/** Update the flick direction indicator. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Carrom")
	void SetFlickDirection(FVector2D Direction);

protected:
	// -- Board ------------------------------------------------------------

	/** Canvas that holds the carrom board and all pieces. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UCanvasPanel> BoardCanvas;

	/** The carrom board surface (tan/brown square). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> BoardSurface;

	/** Corner pocket indicators (4 dark circles). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PocketTL;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PocketTR;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PocketBL;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> PocketBR;

	// -- Pieces -----------------------------------------------------------

	/** White piece widgets (9 pieces). */
	UPROPERTY(meta = (BindWidgetOptional))
	TArray<TObjectPtr<UBorder>> WhitePieceWidgets;

	/** Black piece widgets (9 pieces). */
	UPROPERTY(meta = (BindWidgetOptional))
	TArray<TObjectPtr<UBorder>> BlackPieceWidgets;

	/** The queen piece (red). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> QueenWidget;

	/** The striker widget (red, at bottom baseline). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UBorder> StrikerWidget;

	// -- Aim controls -----------------------------------------------------

	/** Direction arrow image (rotated to match flick aim). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> DirectionArrow;

	/** AIM LEFT button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AimLeftButton;

	/** AIM RIGHT button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> AimRightButton;

	/** Power indicator bar. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> FlickPowerBar;

	/** Power UP button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PowerUpButton;

	/** Power DOWN button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> PowerDownButton;

	/** Power value text (percentage). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PowerValueText;

	/** SHOOT button. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ShootButton;

	// -- Score / status ---------------------------------------------------

	/** Turn indicator text. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> TurnText;

	/** Player score label. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerScoreText;

	/** CPU score label. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CPUScoreText;

	/** Queen status label ("Queen: Uncovered" / "Queen: Covered"). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> QueenStatusText;

	/** Pieces remaining labels. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> PlayerPiecesText;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CPUPiecesText;

	/** Aim angle display. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> AimAngleText;

private:
	UPROPERTY(Transient)
	TObjectPtr<ACarromGameMode> CarromMode;

	void RefreshDisplay();

	// -- Button callbacks -------------------------------------------------

	UFUNCTION() void OnAimLeft();
	UFUNCTION() void OnAimRight();
	UFUNCTION() void OnPowerUp();
	UFUNCTION() void OnPowerDown();
	UFUNCTION() void OnShootClicked();

	/** Current aim angle in degrees (0 = straight up, negative = left, positive = right). */
	float AimAngleDeg = 0.f;

	/** Current flick power (0..1). */
	float FlickPower = 0.5f;
};
