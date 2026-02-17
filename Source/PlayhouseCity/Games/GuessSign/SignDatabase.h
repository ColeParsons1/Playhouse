// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Games/GuessSign/GuessSignGameMode.h"
#include "SignDatabase.generated.h"

// ---------------------------------------------------------------------------
// USignDatabase
// ---------------------------------------------------------------------------

/**
 * In-memory database of ASL (American Sign Language) signs.
 *
 * Contains entries for the full alphabet (A-Z), numbers (0-9),
 * and 30+ common words/phrases used in the Guess the Sign mini-game.
 */
UCLASS(BlueprintType)
class PLAYHOUSECITY_API USignDatabase : public UObject
{
	GENERATED_BODY()

public:
	/** Populate the database with all ASL sign entries. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GuessSign")
	void Initialize();

	/** Get a random sign entry. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GuessSign")
	FPHCSignEntry GetRandomSign() const;

	/** Generate N random wrong choices (not matching the correct answer). */
	UFUNCTION(BlueprintCallable, Category = "PHC|GuessSign")
	TArray<FString> GetRandomChoices(const FString& CorrectAnswer, int32 Count) const;

	/** Get all signs in a specific category. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GuessSign")
	TArray<FPHCSignEntry> GetSignsByCategory(const FString& Category) const;

	/** Get all signs. */
	UFUNCTION(BlueprintCallable, Category = "PHC|GuessSign")
	TArray<FPHCSignEntry> GetAllSigns() const { return SignEntries; }

	/** Get the total number of entries. */
	UFUNCTION(BlueprintPure, Category = "PHC|GuessSign")
	int32 GetEntryCount() const { return SignEntries.Num(); }

protected:
	UPROPERTY()
	TArray<FPHCSignEntry> SignEntries;

private:
	/** Helper to add a sign entry. */
	void AddSign(const FString& Name, const FString& Category);
};
