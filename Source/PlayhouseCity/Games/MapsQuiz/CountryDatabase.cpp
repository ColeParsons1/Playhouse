// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/MapsQuiz/CountryDatabase.h"

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void UCountryDatabase::Initialize()
{
	Countries.Empty();

	// -----------------------------------------------------------------------
	// North America (7)
	// -----------------------------------------------------------------------
	AddCountry(TEXT("United States"),  TEXT("Washington D.C."), TEXT("North America"), TEXT("US"));
	AddCountry(TEXT("Canada"),         TEXT("Ottawa"),          TEXT("North America"), TEXT("CA"));
	AddCountry(TEXT("Mexico"),         TEXT("Mexico City"),     TEXT("North America"), TEXT("MX"));
	AddCountry(TEXT("Cuba"),           TEXT("Havana"),          TEXT("North America"), TEXT("CU"));
	AddCountry(TEXT("Jamaica"),        TEXT("Kingston"),        TEXT("North America"), TEXT("JM"));
	AddCountry(TEXT("Panama"),         TEXT("Panama City"),     TEXT("North America"), TEXT("PA"));
	AddCountry(TEXT("Costa Rica"),     TEXT("San Jose"),        TEXT("North America"), TEXT("CR"));

	// -----------------------------------------------------------------------
	// South America (8)
	// -----------------------------------------------------------------------
	AddCountry(TEXT("Brazil"),       TEXT("Brasilia"),       TEXT("South America"), TEXT("BR"));
	AddCountry(TEXT("Argentina"),    TEXT("Buenos Aires"),   TEXT("South America"), TEXT("AR"));
	AddCountry(TEXT("Colombia"),     TEXT("Bogota"),         TEXT("South America"), TEXT("CO"));
	AddCountry(TEXT("Chile"),        TEXT("Santiago"),        TEXT("South America"), TEXT("CL"));
	AddCountry(TEXT("Peru"),         TEXT("Lima"),            TEXT("South America"), TEXT("PE"));
	AddCountry(TEXT("Venezuela"),    TEXT("Caracas"),         TEXT("South America"), TEXT("VE"));
	AddCountry(TEXT("Ecuador"),      TEXT("Quito"),           TEXT("South America"), TEXT("EC"));
	AddCountry(TEXT("Uruguay"),      TEXT("Montevideo"),      TEXT("South America"), TEXT("UY"));

	// -----------------------------------------------------------------------
	// Europe (14)
	// -----------------------------------------------------------------------
	AddCountry(TEXT("United Kingdom"), TEXT("London"),       TEXT("Europe"), TEXT("GB"));
	AddCountry(TEXT("France"),         TEXT("Paris"),        TEXT("Europe"), TEXT("FR"));
	AddCountry(TEXT("Germany"),        TEXT("Berlin"),       TEXT("Europe"), TEXT("DE"));
	AddCountry(TEXT("Italy"),          TEXT("Rome"),         TEXT("Europe"), TEXT("IT"));
	AddCountry(TEXT("Spain"),          TEXT("Madrid"),       TEXT("Europe"), TEXT("ES"));
	AddCountry(TEXT("Portugal"),       TEXT("Lisbon"),       TEXT("Europe"), TEXT("PT"));
	AddCountry(TEXT("Netherlands"),    TEXT("Amsterdam"),    TEXT("Europe"), TEXT("NL"));
	AddCountry(TEXT("Belgium"),        TEXT("Brussels"),     TEXT("Europe"), TEXT("BE"));
	AddCountry(TEXT("Switzerland"),    TEXT("Bern"),         TEXT("Europe"), TEXT("CH"));
	AddCountry(TEXT("Sweden"),         TEXT("Stockholm"),    TEXT("Europe"), TEXT("SE"));
	AddCountry(TEXT("Norway"),         TEXT("Oslo"),         TEXT("Europe"), TEXT("NO"));
	AddCountry(TEXT("Poland"),         TEXT("Warsaw"),       TEXT("Europe"), TEXT("PL"));
	AddCountry(TEXT("Greece"),         TEXT("Athens"),       TEXT("Europe"), TEXT("GR"));
	AddCountry(TEXT("Ireland"),        TEXT("Dublin"),       TEXT("Europe"), TEXT("IE"));

	// -----------------------------------------------------------------------
	// Asia (12)
	// -----------------------------------------------------------------------
	AddCountry(TEXT("China"),          TEXT("Beijing"),       TEXT("Asia"), TEXT("CN"));
	AddCountry(TEXT("Japan"),          TEXT("Tokyo"),         TEXT("Asia"), TEXT("JP"));
	AddCountry(TEXT("India"),          TEXT("New Delhi"),     TEXT("Asia"), TEXT("IN"));
	AddCountry(TEXT("South Korea"),    TEXT("Seoul"),         TEXT("Asia"), TEXT("KR"));
	AddCountry(TEXT("Thailand"),       TEXT("Bangkok"),       TEXT("Asia"), TEXT("TH"));
	AddCountry(TEXT("Vietnam"),        TEXT("Hanoi"),         TEXT("Asia"), TEXT("VN"));
	AddCountry(TEXT("Indonesia"),      TEXT("Jakarta"),       TEXT("Asia"), TEXT("ID"));
	AddCountry(TEXT("Philippines"),    TEXT("Manila"),        TEXT("Asia"), TEXT("PH"));
	AddCountry(TEXT("Turkey"),         TEXT("Ankara"),        TEXT("Asia"), TEXT("TR"));
	AddCountry(TEXT("Saudi Arabia"),   TEXT("Riyadh"),        TEXT("Asia"), TEXT("SA"));
	AddCountry(TEXT("Israel"),         TEXT("Jerusalem"),     TEXT("Asia"), TEXT("IL"));
	AddCountry(TEXT("Malaysia"),       TEXT("Kuala Lumpur"),  TEXT("Asia"), TEXT("MY"));

	// -----------------------------------------------------------------------
	// Africa (8)
	// -----------------------------------------------------------------------
	AddCountry(TEXT("Egypt"),          TEXT("Cairo"),         TEXT("Africa"), TEXT("EG"));
	AddCountry(TEXT("South Africa"),   TEXT("Pretoria"),      TEXT("Africa"), TEXT("ZA"));
	AddCountry(TEXT("Nigeria"),        TEXT("Abuja"),         TEXT("Africa"), TEXT("NG"));
	AddCountry(TEXT("Kenya"),          TEXT("Nairobi"),       TEXT("Africa"), TEXT("KE"));
	AddCountry(TEXT("Ethiopia"),       TEXT("Addis Ababa"),   TEXT("Africa"), TEXT("ET"));
	AddCountry(TEXT("Morocco"),        TEXT("Rabat"),         TEXT("Africa"), TEXT("MA"));
	AddCountry(TEXT("Ghana"),          TEXT("Accra"),         TEXT("Africa"), TEXT("GH"));
	AddCountry(TEXT("Tanzania"),       TEXT("Dodoma"),        TEXT("Africa"), TEXT("TZ"));

	// -----------------------------------------------------------------------
	// Oceania (3)
	// -----------------------------------------------------------------------
	AddCountry(TEXT("Australia"),      TEXT("Canberra"),      TEXT("Oceania"), TEXT("AU"));
	AddCountry(TEXT("New Zealand"),    TEXT("Wellington"),    TEXT("Oceania"), TEXT("NZ"));
	AddCountry(TEXT("Fiji"),           TEXT("Suva"),          TEXT("Oceania"), TEXT("FJ"));
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

FPHCCountryEntry UCountryDatabase::GetRandomCountry() const
{
	if (Countries.Num() == 0)
	{
		return FPHCCountryEntry();
	}

	int32 Idx = FMath::RandRange(0, Countries.Num() - 1);
	return Countries[Idx];
}

TArray<FPHCCountryEntry> UCountryDatabase::GetCountriesByContinent(const FString& Continent) const
{
	TArray<FPHCCountryEntry> Result;

	for (const FPHCCountryEntry& Entry : Countries)
	{
		if (Entry.Continent.Equals(Continent, ESearchCase::IgnoreCase))
		{
			Result.Add(Entry);
		}
	}

	return Result;
}

TArray<FString> UCountryDatabase::GetRandomChoices(
	const FString& Correct, const FString& Field, int32 Count) const
{
	TArray<FString> Pool;

	for (const FPHCCountryEntry& Entry : Countries)
	{
		FString Value;

		if (Field.Equals(TEXT("CountryName"), ESearchCase::IgnoreCase))
		{
			Value = Entry.CountryName;
		}
		else if (Field.Equals(TEXT("Capital"), ESearchCase::IgnoreCase))
		{
			Value = Entry.Capital;
		}
		else if (Field.Equals(TEXT("Continent"), ESearchCase::IgnoreCase))
		{
			Value = Entry.Continent;
		}
		else
		{
			Value = Entry.CountryName;
		}

		if (!Value.Equals(Correct, ESearchCase::IgnoreCase))
		{
			Pool.AddUnique(Value);
		}
	}

	// Shuffle the pool.
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		int32 j = FMath::RandRange(0, i);
		Pool.Swap(i, j);
	}

	// Take up to Count items.
	TArray<FString> Result;
	int32 NumToTake = FMath::Min(Count, Pool.Num());
	for (int32 i = 0; i < NumToTake; ++i)
	{
		Result.Add(Pool[i]);
	}

	return Result;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UCountryDatabase::AddCountry(
	const FString& Name, const FString& Capital,
	const FString& Continent, const FString& FlagEmoji)
{
	FPHCCountryEntry Entry;
	Entry.CountryName = Name;
	Entry.Capital = Capital;
	Entry.Continent = Continent;
	Entry.FlagEmoji = FlagEmoji;
	Entry.OutlinePath = FString::Printf(TEXT("/Game/UI/Maps/Outlines/%s"), *Name);

	Countries.Add(Entry);
}
