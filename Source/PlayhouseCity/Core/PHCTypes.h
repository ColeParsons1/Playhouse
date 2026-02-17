// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PHCTypes.generated.h"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

/** Currency types available in PlayhouseCity economy. */
UENUM(BlueprintType)
enum class EPHCCurrencyType : uint8
{
	Coins	UMETA(DisplayName = "Coins"),
	Gems	UMETA(DisplayName = "Gems")
};

/** Broad categories for shop / inventory items. */
UENUM(BlueprintType)
enum class EPHCItemCategory : uint8
{
	Avatar		UMETA(DisplayName = "Avatar"),
	Room		UMETA(DisplayName = "Room"),
	Game		UMETA(DisplayName = "Game"),
	Special		UMETA(DisplayName = "Special")
};

/** Runtime status of a multiplayer room. */
UENUM(BlueprintType)
enum class EPHCRoomStatus : uint8
{
	Open	UMETA(DisplayName = "Open"),
	InGame	UMETA(DisplayName = "In Game"),
	Full	UMETA(DisplayName = "Full"),
	Closed	UMETA(DisplayName = "Closed")
};

/** All mini-game types supported by the platform. */
UENUM(BlueprintType)
enum class EPHCGameType : uint8
{
	Bingo			UMETA(DisplayName = "Bingo"),
	ColorCards		UMETA(DisplayName = "Color Cards"),
	LuckySlots		UMETA(DisplayName = "Lucky Slots"),
	Connect4		UMETA(DisplayName = "Connect 4"),
	Ludo			UMETA(DisplayName = "Ludo"),
	PegGame			UMETA(DisplayName = "Peg Game"),
	GuessSign		UMETA(DisplayName = "Guess Sign"),
	MapsQuiz		UMETA(DisplayName = "Maps Quiz"),
	Pong			UMETA(DisplayName = "Pong"),
	PackCatch		UMETA(DisplayName = "Pack Catch"),
	BabyShark		UMETA(DisplayName = "Baby Shark"),
	TempleRun		UMETA(DisplayName = "Temple Run"),
	TowerDefense	UMETA(DisplayName = "Tower Defense"),
	KitchenChaos	UMETA(DisplayName = "Kitchen Chaos"),
	Pool			UMETA(DisplayName = "Pool"),
	Carrom			UMETA(DisplayName = "Carrom"),
	Tanks			UMETA(DisplayName = "Tanks"),
	AngryDucks		UMETA(DisplayName = "Angry Ducks"),
	GooBridge		UMETA(DisplayName = "Goo Bridge"),
	Jenga			UMETA(DisplayName = "Jenga"),
	Cornhole		UMETA(DisplayName = "Cornhole"),
	CelDamage		UMETA(DisplayName = "Cel Damage"),
	Simulator		UMETA(DisplayName = "Playhouse City")
};

/** The current gameplay mode the player is in. */
UENUM(BlueprintType)
enum class EPHCGameplayMode : uint8
{
	Lobby		UMETA(DisplayName = "Lobby (UI Only)"),
	OpenWorld	UMETA(DisplayName = "Open World"),
	MiniGame	UMETA(DisplayName = "Mini Game"),
	Vehicle		UMETA(DisplayName = "Vehicle"),
	Interior	UMETA(DisplayName = "Interior")
};

/** Growth state of a garden plot plant. */
UENUM(BlueprintType)
enum class EPHCPlantState : uint8
{
	Empty		UMETA(DisplayName = "Empty"),
	Planted		UMETA(DisplayName = "Planted"),
	Growing		UMETA(DisplayName = "Growing"),
	Ready		UMETA(DisplayName = "Ready"),
	Wilted		UMETA(DisplayName = "Wilted")
};

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

/** Core player profile mirroring the Django User / Profile models. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCPlayerProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Player")
	int32 PlayerId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Player")
	FString Username;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Player")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Player")
	FString AvatarUrl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Player")
	int32 Coins = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Player")
	int32 Gems = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Player")
	int32 Level = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Player")
	int32 XP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Player")
	int32 FollowerCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Player")
	int32 FollowingCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Player")
	bool bIsGuest = false;
};

/** A single layer of the composited avatar (hair, eyes, shirt, etc.). */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCAvatarLayer
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Avatar")
	FString LayerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Avatar")
	FString AssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Avatar")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Avatar")
	int32 ZOrder = 0;
};

/** Full avatar definition: ordered layers plus skin tone preset. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCAvatarData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Avatar")
	TArray<FPHCAvatarLayer> Layers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Avatar")
	FString SkinTone;
};

/** Single item available (or owned) in the shop. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCShopItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Shop")
	int32 ItemId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Shop")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Shop")
	FString Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Shop")
	FString ImageUrl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Shop")
	EPHCItemCategory Category = EPHCItemCategory::Avatar;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Shop")
	EPHCCurrencyType CurrencyType = EPHCCurrencyType::Coins;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Shop")
	int32 Price = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Shop")
	bool bOwned = false;
};

/** Lightweight descriptor of a multiplayer room for lobby lists. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCRoomInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Room")
	int32 RoomId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Room")
	FString RoomName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Room")
	FString HostName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Room")
	int32 HostId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Room")
	FString ThemeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Room")
	EPHCRoomStatus Status = EPHCRoomStatus::Open;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Room")
	int32 PlayerCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Room")
	int32 MaxPlayers = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Room")
	EPHCGameType CurrentGame = EPHCGameType::Bingo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Room")
	bool bHasPassword = false;
};

/** A single chat message in a room or DM channel. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCChatMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Chat")
	FString SenderName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Chat")
	int32 SenderId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Chat")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Chat")
	FString Timestamp;
};

/** One row in any leaderboard. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCLeaderboardEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Leaderboard")
	int32 Rank = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Leaderboard")
	FString PlayerName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Leaderboard")
	int32 PlayerId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Leaderboard")
	int32 Score = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Leaderboard")
	FString AvatarUrl;
};

/** A single plot in the player's garden. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCGardenPlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Garden")
	int32 PlotIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Garden")
	EPHCPlantState State = EPHCPlantState::Empty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Garden")
	FString PlantType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Garden")
	float GrowthProgress = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Garden")
	FDateTime PlantedTime;
};

/** Daily login bonus state sent from the backend. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCDailyBonus
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|DailyBonus")
	int32 CurrentStreak = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|DailyBonus")
	int32 CoinsReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|DailyBonus")
	int32 GemsReward = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|DailyBonus")
	bool bClaimedToday = false;
};

/** Follow / friend-list entry. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCFollowInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Social")
	int32 PlayerId = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Social")
	FString Username;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Social")
	FString AvatarUrl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Social")
	bool bIsFollowing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Social")
	bool bIsOnline = false;
};

/** Color-theme palette used across all UI widgets. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCThemeData
{
	GENERATED_BODY()

	// -- Identity --------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FString ThemeId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FString ThemeName;

	// -- Core palette ----------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor PrimaryColor = FLinearColor(0.2f, 0.4f, 0.8f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor SecondaryColor = FLinearColor(0.1f, 0.2f, 0.5f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor AccentColor = FLinearColor(1.f, 0.6f, 0.f, 1.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor BackgroundColor = FLinearColor(0.05f, 0.05f, 0.1f, 1.f);

	/** Surface / card colour -- panels, modals, cards. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor SurfaceColor = FLinearColor(0.1f, 0.1f, 0.15f, 1.f);

	// -- Semantic colours ------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor TextColor = FLinearColor::White;

	/** Muted / secondary text colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor TextSecondaryColor = FLinearColor(0.7f, 0.7f, 0.7f, 1.f);

	/** Legacy alias kept for backward compatibility (maps to SurfaceColor). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor CardColor = FLinearColor(0.15f, 0.15f, 0.25f, 1.f);

	/** Success feedback colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor SuccessColor = FLinearColor(0.2f, 0.8f, 0.3f, 1.f);

	/** Warning feedback colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor WarningColor = FLinearColor(1.f, 0.8f, 0.f, 1.f);

	/** Error / danger colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor ErrorColor = FLinearColor(0.9f, 0.2f, 0.2f, 1.f);

	/** Border / divider colour. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Theme")
	FLinearColor BorderColor = FLinearColor(0.25f, 0.25f, 0.3f, 1.f);

	// -- Helpers ---------------------------------------------------------

	bool IsValid() const { return !ThemeId.IsEmpty(); }
};

/** Result payload returned when a mini-game round ends. */
USTRUCT(BlueprintType)
struct PLAYHOUSECITY_API FPHCGameResult
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Game")
	EPHCGameType GameType = EPHCGameType::Bingo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Game")
	int32 Score = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Game")
	int32 CoinsEarned = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Game")
	int32 XPEarned = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Game")
	bool bWon = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Game")
	float TimePlayed = 0.f;
};
