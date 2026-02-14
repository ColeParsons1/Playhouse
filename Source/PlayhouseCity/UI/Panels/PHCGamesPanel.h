// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "PHCGamesPanel.generated.h"

class UUniformGridPanel;
class UEditableTextBox;
class UButton;
class UPHCMainHUD;

/**
 * Display metadata for a single game card in the grid.
 */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCGameGridItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayhouseCity|Games")
	EPHCGameType GameType = EPHCGameType::Bingo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayhouseCity|Games")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayhouseCity|Games")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PlayhouseCity|Games")
	FString IconPath;
};

/**
 * UPHCGameCardWidget
 *
 * Small widget representing one game card in the grid.
 * Contains a clickable button, game name, and icon placeholder.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class PLAYHOUSECITY_API UPHCGameCardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual bool Initialize() override;

	/** Configures this card for a specific game type. */
	void SetupCard(const FPHCGameGridItem& Item);

	/** The game type this card represents. */
	UPROPERTY(BlueprintReadOnly, Category = "PlayhouseCity|Games")
	EPHCGameType GameType = EPHCGameType::Bingo;

	/** Delegate fired when this card is clicked. */
	DECLARE_DELEGATE_OneParam(FOnGameCardClicked, EPHCGameType);
	FOnGameCardClicked OnGameCardClicked;

protected:
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UButton> CardButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> GameNameText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<class UTextBlock> GameDescriptionText;

private:
	UFUNCTION()
	void OnCardClicked();
};

/**
 * UPHCGamesPanel
 *
 * Displays a grid of all 21 available mini-games.
 * Games are laid out in a 3-column uniform grid.
 * Clicking a card navigates to the game via the MainHUD.
 */
UCLASS(BlueprintType, Blueprintable, meta = (DisableNativeTick))
class PLAYHOUSECITY_API UPHCGamesPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCGamesPanel(const FObjectInitializer& ObjectInitializer);

	virtual bool Initialize() override;
	virtual void NativeConstruct() override;

	// ---------------------------------------------------------------
	//  Public API
	// ---------------------------------------------------------------

	/** Returns the user-friendly display name for a game type. */
	UFUNCTION(BlueprintPure, Category = "PlayhouseCity|Games")
	static FString GetGameDisplayName(EPHCGameType GameType);

	/** Returns a short description for a game type. */
	UFUNCTION(BlueprintPure, Category = "PlayhouseCity|Games")
	static FString GetGameDescription(EPHCGameType GameType);

protected:
	// ---------------------------------------------------------------
	//  Bound Widgets
	// ---------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UUniformGridPanel> GameGrid;

	/** Optional search input for filtering the game grid. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional))
	TObjectPtr<UEditableTextBox> SearchInput;

	// ---------------------------------------------------------------
	//  Configuration
	// ---------------------------------------------------------------

	/** Widget class to use for individual game cards. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|Games")
	TSubclassOf<UPHCGameCardWidget> GameCardWidgetClass;

	/** Number of columns in the game grid. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "PlayhouseCity|Games")
	int32 GridColumns = 3;

private:
	// ---------------------------------------------------------------
	//  Internal
	// ---------------------------------------------------------------

	/** Populates the grid with cards for all game types. */
	void PopulateGames();

	/** Builds the master list of all game grid items. */
	TArray<FPHCGameGridItem> BuildGameList() const;

	/** Called when a game card is clicked. */
	void OnGameSelected(EPHCGameType GameType);

	/** Callback for optional search filtering. */
	UFUNCTION()
	void OnSearchChanged(const FText& Text);

	/** Applies search filter and rebuilds the grid. */
	void ApplySearchFilter(const FString& SearchText);

	/** Attempts to find the owning MainHUD. */
	UPHCMainHUD* GetMainHUD() const;

	/** Cached full game list. */
	TArray<FPHCGameGridItem> AllGames;

	/** Currently displayed card widgets. */
	UPROPERTY()
	TArray<TObjectPtr<UPHCGameCardWidget>> CardWidgets;
};
