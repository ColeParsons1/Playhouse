// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/PHCLeaderboardWidget.h"

#include "Components/ComboBoxString.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Border.h"

#include "Core/PHCGameInstance.h"
#include "Network/PHCHttpClient.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

// ---------------------------------------------------------------------------
// Static data
// ---------------------------------------------------------------------------

const TArray<TPair<EPHCGameType, FString>> UPHCLeaderboardWidget::AllGameTypes = {
	{ EPHCGameType::Bingo,			TEXT("Bingo") },
	{ EPHCGameType::ColorCards,		TEXT("Color Cards") },
	{ EPHCGameType::LuckySlots,		TEXT("Lucky Slots") },
	{ EPHCGameType::Connect4,		TEXT("Connect 4") },
	{ EPHCGameType::Ludo,			TEXT("Ludo") },
	{ EPHCGameType::PegGame,		TEXT("Peg Game") },
	{ EPHCGameType::GuessSign,		TEXT("Guess Sign") },
	{ EPHCGameType::MapsQuiz,		TEXT("Maps Quiz") },
	{ EPHCGameType::Pong,			TEXT("Pong") },
	{ EPHCGameType::PackCatch,		TEXT("Pack Catch") },
	{ EPHCGameType::BabyShark,		TEXT("Baby Shark") },
	{ EPHCGameType::TempleRun,		TEXT("Temple Run") },
	{ EPHCGameType::TowerDefense,	TEXT("Tower Defense") },
	{ EPHCGameType::KitchenChaos,	TEXT("Kitchen Chaos") },
	{ EPHCGameType::Pool,			TEXT("Pool") },
	{ EPHCGameType::Carrom,			TEXT("Carrom") },
	{ EPHCGameType::Tanks,			TEXT("Tanks") },
	{ EPHCGameType::AngryDucks,		TEXT("Angry Ducks") },
	{ EPHCGameType::GooBridge,		TEXT("Goo Bridge") },
	{ EPHCGameType::Jenga,			TEXT("Jenga") },
	{ EPHCGameType::Cornhole,		TEXT("Cornhole") }
};

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCLeaderboardWidget::UPHCLeaderboardWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

// ---------------------------------------------------------------------------
// UUserWidget overrides
// ---------------------------------------------------------------------------

void UPHCLeaderboardWidget::NativeConstruct()
{
	Super::NativeConstruct();

	PopulateGameTypeSelector();

	if (GameTypeSelector)
	{
		GameTypeSelector->OnSelectionChanged.AddDynamic(this, &UPHCLeaderboardWidget::OnGameTypeChanged);
	}
	if (RefreshButton)
	{
		RefreshButton->OnClicked.AddDynamic(this, &UPHCLeaderboardWidget::OnRefreshClicked);
	}

	// Load the default game type leaderboard.
	LoadLeaderboard(CurrentGameType);
}

// ---------------------------------------------------------------------------
// Public methods
// ---------------------------------------------------------------------------

void UPHCLeaderboardWidget::LoadLeaderboard(EPHCGameType GameType)
{
	CurrentGameType = GameType;

	UPHCHttpClient* Http = GetHttpClient();
	if (!Http)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCLeaderboard: HttpClient unavailable."));
		return;
	}

	SetLoading(true);

	if (LeaderboardList)
	{
		LeaderboardList->ClearChildren();
	}

	const FString GameTypeStr = GameTypeToString(GameType);

	FPHCJsonResponseDynamic Callback;
	Callback.BindDynamic(this, &UPHCLeaderboardWidget::OnDataReceived);
	Http->GetLeaderboard(GameTypeStr, /*Page=*/ 1, Callback);
}

// ---------------------------------------------------------------------------
// Protected methods
// ---------------------------------------------------------------------------

UWidget* UPHCLeaderboardWidget::CreateLeaderboardEntry(const FPHCLeaderboardEntry& Entry)
{
	// Build a horizontal row: [Rank] [PlayerName] [Score]
	UHorizontalBox* Row = NewObject<UHorizontalBox>(this);
	if (!Row)
	{
		return nullptr;
	}

	// -- Rank column -----------------------------------------------------------
	UTextBlock* RankText = NewObject<UTextBlock>(Row);
	if (RankText)
	{
		RankText->SetText(FText::FromString(FString::Printf(TEXT("#%d"), Entry.Rank)));
		FSlateFontInfo FontInfo = RankText->GetFont();
		FontInfo.Size = 14;
		RankText->SetFont(FontInfo);

		// Gold, silver, bronze coloring for top 3.
		if (Entry.Rank == 1)
		{
			RankText->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.84f, 0.0f, 1.0f)));
		}
		else if (Entry.Rank == 2)
		{
			RankText->SetColorAndOpacity(FSlateColor(FLinearColor(0.75f, 0.75f, 0.75f, 1.0f)));
		}
		else if (Entry.Rank == 3)
		{
			RankText->SetColorAndOpacity(FSlateColor(FLinearColor(0.80f, 0.50f, 0.20f, 1.0f)));
		}

		UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(RankText);
		if (Slot)
		{
			Slot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}
	}

	// -- Player name column ----------------------------------------------------
	UTextBlock* NameText = NewObject<UTextBlock>(Row);
	if (NameText)
	{
		NameText->SetText(FText::FromString(Entry.PlayerName));
		FSlateFontInfo FontInfo = NameText->GetFont();
		FontInfo.Size = 14;
		NameText->SetFont(FontInfo);

		UHorizontalBoxSlot* Slot = Row->AddChildToHorizontalBox(NameText);
		if (Slot)
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); // Fill available space.
		}
	}

	// -- Score column ----------------------------------------------------------
	UTextBlock* ScoreText = NewObject<UTextBlock>(Row);
	if (ScoreText)
	{
		ScoreText->SetText(FText::AsNumber(Entry.Score));
		FSlateFontInfo FontInfo = ScoreText->GetFont();
		FontInfo.Size = 14;
		ScoreText->SetFont(FontInfo);
		ScoreText->SetJustification(ETextJustify::Right);

		Row->AddChildToHorizontalBox(ScoreText);
	}

	return Row;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void UPHCLeaderboardWidget::PopulateGameTypeSelector()
{
	if (!GameTypeSelector)
	{
		return;
	}

	GameTypeSelector->ClearOptions();

	for (const auto& Pair : AllGameTypes)
	{
		GameTypeSelector->AddOption(Pair.Value);
	}

	// Select the first option by default.
	if (AllGameTypes.Num() > 0)
	{
		GameTypeSelector->SetSelectedOption(AllGameTypes[0].Value);
		CurrentGameType = AllGameTypes[0].Key;
	}
}

void UPHCLeaderboardWidget::OnGameTypeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	const EPHCGameType GameType = StringToGameType(SelectedItem);
	LoadLeaderboard(GameType);
}

void UPHCLeaderboardWidget::OnRefreshClicked()
{
	LoadLeaderboard(CurrentGameType);
}

void UPHCLeaderboardWidget::OnDataReceived(bool bSuccess, int32 StatusCode, const FString& Response)
{
	SetLoading(false);

	if (!bSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCLeaderboard: Failed to load data. Status=%d"), StatusCode);
		return;
	}

	// Parse JSON response.
	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCLeaderboard: Failed to parse JSON."));
		return;
	}

	// Parse the leaderboard entries array.
	const TArray<TSharedPtr<FJsonValue>>* EntriesArray = nullptr;
	if (!RootObj->TryGetArrayField(TEXT("entries"), EntriesArray) || !EntriesArray)
	{
		UE_LOG(LogTemp, Warning, TEXT("PHCLeaderboard: No 'entries' array in response."));
		return;
	}

	if (LeaderboardList)
	{
		LeaderboardList->ClearChildren();
	}

	for (const TSharedPtr<FJsonValue>& Val : *EntriesArray)
	{
		const TSharedPtr<FJsonObject>* EntryObj = nullptr;
		if (!Val->TryGetObject(EntryObj) || !EntryObj || !(*EntryObj).IsValid())
		{
			continue;
		}

		FPHCLeaderboardEntry Entry;
		Entry.Rank = static_cast<int32>((*EntryObj)->GetNumberField(TEXT("rank")));
		Entry.PlayerName = (*EntryObj)->GetStringField(TEXT("player_name"));
		Entry.PlayerId = static_cast<int32>((*EntryObj)->GetNumberField(TEXT("player_id")));
		Entry.Score = static_cast<int32>((*EntryObj)->GetNumberField(TEXT("score")));
		Entry.AvatarUrl = (*EntryObj)->GetStringField(TEXT("avatar_url"));

		UWidget* RowWidget = CreateLeaderboardEntry(Entry);
		if (RowWidget && LeaderboardList)
		{
			LeaderboardList->AddChild(RowWidget);
		}
	}

	// Show the local player's rank if included.
	if (MyRankText && RootObj->HasField(TEXT("my_rank")))
	{
		const int32 MyRank = static_cast<int32>(RootObj->GetNumberField(TEXT("my_rank")));
		if (MyRank > 0)
		{
			MyRankText->SetText(FText::FromString(FString::Printf(TEXT("Your Rank: #%d"), MyRank)));
		}
		else
		{
			MyRankText->SetText(FText::FromString(TEXT("Unranked")));
		}
	}
}

void UPHCLeaderboardWidget::SetLoading(bool bLoading)
{
	if (LoadingIndicator)
	{
		LoadingIndicator->SetVisibility(bLoading ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
}

FString UPHCLeaderboardWidget::GameTypeToString(EPHCGameType GameType)
{
	for (const auto& Pair : AllGameTypes)
	{
		if (Pair.Key == GameType)
		{
			return Pair.Value;
		}
	}
	return TEXT("Bingo");
}

EPHCGameType UPHCLeaderboardWidget::StringToGameType(const FString& Str)
{
	for (const auto& Pair : AllGameTypes)
	{
		if (Pair.Value == Str)
		{
			return Pair.Key;
		}
	}
	return EPHCGameType::Bingo;
}

UPHCHttpClient* UPHCLeaderboardWidget::GetHttpClient() const
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	return GI ? GI->GetHttpClient() : nullptr;
}
