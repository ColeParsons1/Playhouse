// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/PHCTypes.h"
#include "PHCPlayerSearchWidget.generated.h"

class UEditableTextBox;
class UScrollBox;
class UTextBlock;
class UButton;
class UWidget;
class UPHCHttpClient;
class UPHCFollowManager;

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

/** Broadcast when a player result is clicked (e.g. to open their profile). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPHCPlayerClicked, int32, PlayerId);

// ---------------------------------------------------------------------------
// UPHCPlayerSearchWidget
// ---------------------------------------------------------------------------

/**
 * UPHCPlayerSearchWidget
 *
 * A user widget for searching players by username. Features debounced
 * input to avoid excessive API calls, a scrollable results list with
 * follow/unfollow buttons, and loading/empty state indicators.
 *
 * Bind the following widgets in the UMG designer:
 *   - SearchInput       (UEditableTextBox)  -- Text input for search query
 *   - ResultsList        (UScrollBox)        -- Scrollable list of results
 *   - LoadingIndicator   (UWidget)           -- Shown while search is in progress
 *   - EmptyText          (UTextBlock)        -- Shown when no results found
 *   - ClearButton        (UButton)           -- Clears input and results
 */
UCLASS(BlueprintType, Blueprintable)
class PLAYHOUSECITY_API UPHCPlayerSearchWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPHCPlayerSearchWidget(const FObjectInitializer& ObjectInitializer);

	// ---------------------------------------------------------------
	//  UUserWidget Interface
	// ---------------------------------------------------------------

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// ---------------------------------------------------------------
	//  Configuration
	// ---------------------------------------------------------------

	/**
	 * Set external dependencies. Should be called after the widget is
	 * constructed, typically by the owning screen or game instance.
	 *
	 * @param InHttpClient      HTTP client for player search API calls.
	 * @param InFollowManager   Follow manager for follow/unfollow actions.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Social|Search")
	void SetDependencies(UPHCHttpClient* InHttpClient, UPHCFollowManager* InFollowManager);

	// ---------------------------------------------------------------
	//  Delegates
	// ---------------------------------------------------------------

	/** Fired when a player result row is clicked. */
	UPROPERTY(BlueprintAssignable, Category = "PHC|Social|Search")
	FOnPHCPlayerClicked OnPlayerClicked;

protected:
	// ---------------------------------------------------------------
	//  Bound Widgets
	// ---------------------------------------------------------------

	/** Text input field for the search query. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UEditableTextBox> SearchInput;

	/** Scrollable list displaying search results. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UScrollBox> ResultsList;

	/** Generic widget shown while a search is in progress. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UWidget> LoadingIndicator;

	/** Text shown when search returns no results. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UTextBlock> EmptyText;

	/** Button to clear the search input and results. */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UButton> ClearButton;

	// ---------------------------------------------------------------
	//  Configuration Properties
	// ---------------------------------------------------------------

	/** Minimum number of characters before a search is triggered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Social|Search")
	int32 MinSearchLength = 2;

	/** Delay in seconds before executing search after typing stops. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Social|Search")
	float DebounceDelay = 0.3f;

	// ---------------------------------------------------------------
	//  Event Handlers
	// ---------------------------------------------------------------

	/**
	 * Called when the search input text changes. Resets the debounce
	 * timer and schedules a new search.
	 */
	UFUNCTION()
	void OnSearchTextChanged(const FText& Text);

	/**
	 * Called when the search input is committed (e.g. Enter pressed).
	 * Immediately executes the search without debounce.
	 */
	UFUNCTION()
	void OnSearchTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	/**
	 * Execute the search query using the current input text.
	 * Called after the debounce timer fires.
	 */
	UFUNCTION()
	void ExecuteSearch();

	/**
	 * Handle the API response from a player search.
	 *
	 * @param bSuccess  Whether the HTTP request succeeded.
	 * @param Response  The raw JSON response body.
	 */
	void OnSearchResults(bool bSuccess, const FString& Response);

	/**
	 * Create a widget row for a single player search result.
	 *
	 * @param Player  The player info to display.
	 * @return The created widget, or nullptr on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "PHC|Social|Search")
	UWidget* CreatePlayerResultRow(const FPHCFollowInfo& Player);

	/**
	 * Handle a follow/unfollow toggle for a player in the results.
	 *
	 * @param PlayerId  The player whose follow state to toggle.
	 */
	UFUNCTION()
	void OnFollowToggle(int32 PlayerId);

	/**
	 * Handle a click on a player result row to view their profile.
	 *
	 * @param PlayerId  The clicked player's ID.
	 */
	UFUNCTION()
	void OnPlayerResultClicked(int32 PlayerId);

	/**
	 * Clear the search input and results list.
	 */
	UFUNCTION()
	void OnClearClicked();

private:
	// ---------------------------------------------------------------
	//  Internal Helpers
	// ---------------------------------------------------------------

	/** Show or hide the loading indicator. */
	void SetLoadingState(bool bLoading);

	/** Show or hide the empty results text. */
	void SetEmptyState(bool bEmpty);

	/** Clear all result rows from the scroll box. */
	void ClearResults();

	/**
	 * Parse the search response JSON into an array of follow info structs.
	 *
	 * @param JsonResponse  The raw JSON response.
	 * @return Parsed array of player info.
	 */
	TArray<FPHCFollowInfo> ParseSearchResults(const FString& JsonResponse) const;

	// ---------------------------------------------------------------
	//  State
	// ---------------------------------------------------------------

	/** Timer handle for the debounce delay. */
	FTimerHandle DebounceTimer;

	/** The last query string that was searched. */
	FString LastQuery;

	/** Whether a search request is currently in flight. */
	bool bIsSearching = false;

	/** Cached search results for the current query. */
	TArray<FPHCFollowInfo> CurrentResults;

	/** HTTP client for search API calls. */
	UPROPERTY()
	TObjectPtr<UPHCHttpClient> HttpClient;

	/** Follow manager for follow/unfollow actions. */
	UPROPERTY()
	TObjectPtr<UPHCFollowManager> FollowManager;
};
