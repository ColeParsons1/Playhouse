// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Games/MapsQuiz/MapsGameMode.h"
#include "CountryDatabase.generated.h"

// ---------------------------------------------------------------------------
// UCountryDatabase
// ---------------------------------------------------------------------------

/**
 * In-memory database of countries used by the Maps Quiz mini-game.
 *
 * Contains 50+ countries with their names, capitals, continents,
 * flag references, and outline image paths.
 */
UCLASS(BlueprintType)
class PLAYHOUSECITY_API UCountryDatabase : public UObject
{
	GENERATED_BODY()

public:
	/** Populate the database with all country entries. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MapsQuiz")
	void Initialize();

	/** Get a random country entry. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MapsQuiz")
	FPHCCountryEntry GetRandomCountry() const;

	/** Get all countries on a specific continent. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MapsQuiz")
	TArray<FPHCCountryEntry> GetCountriesByContinent(const FString& Continent) const;

	/** Generate N random distractors from a specified field, excluding the correct answer. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MapsQuiz")
	TArray<FString> GetRandomChoices(const FString& Correct, const FString& Field, int32 Count) const;

	/** Get all countries. */
	UFUNCTION(BlueprintCallable, Category = "PHC|MapsQuiz")
	TArray<FPHCCountryEntry> GetAllCountries() const { return Countries; }

	/** Get the total number of entries. */
	UFUNCTION(BlueprintPure, Category = "PHC|MapsQuiz")
	int32 GetEntryCount() const { return Countries.Num(); }

protected:
	UPROPERTY()
	TArray<FPHCCountryEntry> Countries;

private:
	/** Helper to add a country entry. */
	void AddCountry(const FString& Name, const FString& Capital,
					const FString& Continent, const FString& FlagEmoji);
};
