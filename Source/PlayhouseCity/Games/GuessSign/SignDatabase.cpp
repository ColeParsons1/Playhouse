// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/GuessSign/SignDatabase.h"

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void USignDatabase::Initialize()
{
	SignEntries.Empty();

	// -----------------------------------------------------------------------
	// Alphabet (A-Z)
	// -----------------------------------------------------------------------
	AddSign(TEXT("A"), TEXT("Alphabet"));
	AddSign(TEXT("B"), TEXT("Alphabet"));
	AddSign(TEXT("C"), TEXT("Alphabet"));
	AddSign(TEXT("D"), TEXT("Alphabet"));
	AddSign(TEXT("E"), TEXT("Alphabet"));
	AddSign(TEXT("F"), TEXT("Alphabet"));
	AddSign(TEXT("G"), TEXT("Alphabet"));
	AddSign(TEXT("H"), TEXT("Alphabet"));
	AddSign(TEXT("I"), TEXT("Alphabet"));
	AddSign(TEXT("J"), TEXT("Alphabet"));
	AddSign(TEXT("K"), TEXT("Alphabet"));
	AddSign(TEXT("L"), TEXT("Alphabet"));
	AddSign(TEXT("M"), TEXT("Alphabet"));
	AddSign(TEXT("N"), TEXT("Alphabet"));
	AddSign(TEXT("O"), TEXT("Alphabet"));
	AddSign(TEXT("P"), TEXT("Alphabet"));
	AddSign(TEXT("Q"), TEXT("Alphabet"));
	AddSign(TEXT("R"), TEXT("Alphabet"));
	AddSign(TEXT("S"), TEXT("Alphabet"));
	AddSign(TEXT("T"), TEXT("Alphabet"));
	AddSign(TEXT("U"), TEXT("Alphabet"));
	AddSign(TEXT("V"), TEXT("Alphabet"));
	AddSign(TEXT("W"), TEXT("Alphabet"));
	AddSign(TEXT("X"), TEXT("Alphabet"));
	AddSign(TEXT("Y"), TEXT("Alphabet"));
	AddSign(TEXT("Z"), TEXT("Alphabet"));

	// -----------------------------------------------------------------------
	// Numbers (0-9)
	// -----------------------------------------------------------------------
	AddSign(TEXT("0"), TEXT("Numbers"));
	AddSign(TEXT("1"), TEXT("Numbers"));
	AddSign(TEXT("2"), TEXT("Numbers"));
	AddSign(TEXT("3"), TEXT("Numbers"));
	AddSign(TEXT("4"), TEXT("Numbers"));
	AddSign(TEXT("5"), TEXT("Numbers"));
	AddSign(TEXT("6"), TEXT("Numbers"));
	AddSign(TEXT("7"), TEXT("Numbers"));
	AddSign(TEXT("8"), TEXT("Numbers"));
	AddSign(TEXT("9"), TEXT("Numbers"));

	// -----------------------------------------------------------------------
	// Common Words (30+)
	// -----------------------------------------------------------------------
	AddSign(TEXT("Hello"),      TEXT("CommonWords"));
	AddSign(TEXT("Goodbye"),    TEXT("CommonWords"));
	AddSign(TEXT("Thank You"),  TEXT("CommonWords"));
	AddSign(TEXT("Please"),     TEXT("CommonWords"));
	AddSign(TEXT("Sorry"),      TEXT("CommonWords"));
	AddSign(TEXT("Yes"),        TEXT("CommonWords"));
	AddSign(TEXT("No"),         TEXT("CommonWords"));
	AddSign(TEXT("Help"),       TEXT("CommonWords"));
	AddSign(TEXT("Love"),       TEXT("CommonWords"));
	AddSign(TEXT("Friend"),     TEXT("CommonWords"));
	AddSign(TEXT("Family"),     TEXT("CommonWords"));
	AddSign(TEXT("Mother"),     TEXT("CommonWords"));
	AddSign(TEXT("Father"),     TEXT("CommonWords"));
	AddSign(TEXT("Baby"),       TEXT("CommonWords"));
	AddSign(TEXT("Water"),      TEXT("CommonWords"));
	AddSign(TEXT("Food"),       TEXT("CommonWords"));
	AddSign(TEXT("Eat"),        TEXT("CommonWords"));
	AddSign(TEXT("Drink"),      TEXT("CommonWords"));
	AddSign(TEXT("Sleep"),      TEXT("CommonWords"));
	AddSign(TEXT("Play"),       TEXT("CommonWords"));
	AddSign(TEXT("Happy"),      TEXT("CommonWords"));
	AddSign(TEXT("Sad"),        TEXT("CommonWords"));
	AddSign(TEXT("Angry"),      TEXT("CommonWords"));
	AddSign(TEXT("Scared"),     TEXT("CommonWords"));
	AddSign(TEXT("Good"),       TEXT("CommonWords"));
	AddSign(TEXT("Bad"),        TEXT("CommonWords"));
	AddSign(TEXT("Big"),        TEXT("CommonWords"));
	AddSign(TEXT("Small"),      TEXT("CommonWords"));
	AddSign(TEXT("More"),       TEXT("CommonWords"));
	AddSign(TEXT("Stop"),       TEXT("CommonWords"));
	AddSign(TEXT("Go"),         TEXT("CommonWords"));
	AddSign(TEXT("School"),     TEXT("CommonWords"));
	AddSign(TEXT("Home"),       TEXT("CommonWords"));
	AddSign(TEXT("Book"),       TEXT("CommonWords"));
	AddSign(TEXT("Dog"),        TEXT("CommonWords"));
	AddSign(TEXT("Cat"),        TEXT("CommonWords"));
	AddSign(TEXT("Color"),      TEXT("CommonWords"));
	AddSign(TEXT("Music"),      TEXT("CommonWords"));
	AddSign(TEXT("Dance"),      TEXT("CommonWords"));
	AddSign(TEXT("Run"),        TEXT("CommonWords"));
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

FPHCSignEntry USignDatabase::GetRandomSign() const
{
	if (SignEntries.Num() == 0)
	{
		return FPHCSignEntry();
	}

	int32 Idx = FMath::RandRange(0, SignEntries.Num() - 1);
	return SignEntries[Idx];
}

TArray<FString> USignDatabase::GetRandomChoices(const FString& CorrectAnswer, int32 Count) const
{
	TArray<FString> Result;

	// Collect all names except the correct answer.
	TArray<FString> Pool;
	for (const FPHCSignEntry& Entry : SignEntries)
	{
		if (!Entry.SignName.Equals(CorrectAnswer, ESearchCase::IgnoreCase))
		{
			Pool.Add(Entry.SignName);
		}
	}

	// Shuffle the pool.
	for (int32 i = Pool.Num() - 1; i > 0; --i)
	{
		int32 j = FMath::RandRange(0, i);
		Pool.Swap(i, j);
	}

	// Take up to Count items.
	int32 NumToTake = FMath::Min(Count, Pool.Num());
	for (int32 i = 0; i < NumToTake; ++i)
	{
		Result.Add(Pool[i]);
	}

	return Result;
}

TArray<FPHCSignEntry> USignDatabase::GetSignsByCategory(const FString& Category) const
{
	TArray<FPHCSignEntry> Result;

	for (const FPHCSignEntry& Entry : SignEntries)
	{
		if (Entry.Category.Equals(Category, ESearchCase::IgnoreCase))
		{
			Result.Add(Entry);
		}
	}

	return Result;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void USignDatabase::AddSign(const FString& Name, const FString& Category)
{
	FPHCSignEntry Entry;
	Entry.SignName = Name;
	Entry.Category = Category;

	// Construct a standard image path: /Game/UI/Signs/<Category>/<Name>
	Entry.ImagePath = FString::Printf(
		TEXT("/Game/UI/Signs/%s/%s"), *Category, *Name);

	SignEntries.Add(Entry);
}
