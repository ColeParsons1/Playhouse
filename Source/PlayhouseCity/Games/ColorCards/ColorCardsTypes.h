// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ColorCardsTypes.generated.h"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

/** Card colour in the Color Cards (Uno-style) game. */
UENUM(BlueprintType)
enum class EPHCCardColor : uint8
{
	Red		UMETA(DisplayName = "Red"),
	Blue	UMETA(DisplayName = "Blue"),
	Green	UMETA(DisplayName = "Green"),
	Yellow	UMETA(DisplayName = "Yellow"),
	Wild	UMETA(DisplayName = "Wild")
};

/** Card face value / action. */
UENUM(BlueprintType)
enum class EPHCCardValue : uint8
{
	Zero			UMETA(DisplayName = "0"),
	One				UMETA(DisplayName = "1"),
	Two				UMETA(DisplayName = "2"),
	Three			UMETA(DisplayName = "3"),
	Four			UMETA(DisplayName = "4"),
	Five			UMETA(DisplayName = "5"),
	Six				UMETA(DisplayName = "6"),
	Seven			UMETA(DisplayName = "7"),
	Eight			UMETA(DisplayName = "8"),
	Nine			UMETA(DisplayName = "9"),
	Skip			UMETA(DisplayName = "Skip"),
	Reverse			UMETA(DisplayName = "Reverse"),
	DrawTwo			UMETA(DisplayName = "Draw Two"),
	WildCard		UMETA(DisplayName = "Wild"),
	WildDrawFour	UMETA(DisplayName = "Wild Draw Four")
};

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

/** Represents a single card in the Color Cards deck. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCUnoCard
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|ColorCards")
	EPHCCardColor Color = EPHCCardColor::Red;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|ColorCards")
	EPHCCardValue Value = EPHCCardValue::Zero;

	bool operator==(const FPHCUnoCard& Other) const
	{
		return Color == Other.Color && Value == Other.Value;
	}

	bool operator!=(const FPHCUnoCard& Other) const
	{
		return !(*this == Other);
	}

	/** True if this card is a Wild or Wild Draw Four. */
	bool IsWild() const
	{
		return Color == EPHCCardColor::Wild;
	}
};
