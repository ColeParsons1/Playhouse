// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "PHCLeaderboardWidget.generated.h"

class UComboBoxString;
class UScrollBox;
class UButton;
class UTextBlock;
class UWidget;
class UPHCHttpClient;

/**
 * UPHCLeaderboardWidget
 *
 * Displays ranked player lists for each of the 21 supported mini-games.
 * Users select a game type from a combo-box, and the widget fetches the
 * top scores and highlights the local player's rank.
 */
UCLASS()
class PLAYHOUSECITY_API UPHCLeaderboardWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCLeaderboardWidget(const FObjectInitializer& ObjectInitializer);

	// -- UUserWidget interface -------------------------------------------------
	virtual void NativeConstruct() override;

	// -- Public API ------------------------------------------------------------

	/** Load the leaderboard for a specific game type. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Leaderboard")
	void LoadLeaderboard(EPHCGameType GameType);

protected:
	// -- Bind Widgets ----------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UComboBoxString> GameTypeSelector;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UScrollBox> LeaderboardList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> RefreshButton;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> MyRankText;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> LoadingIndicator;

	// -- Methods ---------------------------------------------------------------

	/** Create a single row widget for a leaderboard entry. */
	UFUNCTION(BlueprintCallable, Category = "PHC|Leaderboard")
	UWidget* CreateLeaderboardEntry(const FPHCLeaderboardEntry& Entry);

private:
	// -- Internal helpers ------------------------------------------------------

	/** Populate the GameTypeSelector combo box with all 21 game types. */
	void PopulateGameTypeSelector();

	/** Handle combo-box selection change. */
	UFUNCTION()
	void OnGameTypeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	/** Handle Refresh button click. */
	UFUNCTION()
	void OnRefreshClicked();

	/** Callback after leaderboard API returns. */
	UFUNCTION()
	void OnDataReceived(bool bSuccess, int32 StatusCode, const FString& Response);

	/** Show or hide loading indicator. */
	void SetLoading(bool bLoading);

	/** Convert an EPHCGameType to its display/API string. */
	static FString GameTypeToString(EPHCGameType GameType);

	/** Convert a display string back to EPHCGameType. */
	static EPHCGameType StringToGameType(const FString& Str);

	/** Convenience accessor for the HttpClient via GameInstance. */
	UPHCHttpClient* GetHttpClient() const;

	/** Currently selected game type. */
	EPHCGameType CurrentGameType = EPHCGameType::Bingo;

	/** All game type entries for the combo box, in enum order. */
	static const TArray<TPair<EPHCGameType, FString>> AllGameTypes;
};
