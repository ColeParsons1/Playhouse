// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/Panels/PHCGamesPanel.h"
#include "UI/PHCMainHUD.h"

#include "Components/UniformGridPanel.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"

DEFINE_LOG_CATEGORY_STATIC(LogPHCGamesPanel, Log, All);

// ===================================================================
//  Game Card Widget
// ===================================================================

bool UPHCGameCardWidget::Initialize()
{
	bool bResult = Super::Initialize();
	if (!bResult) return false;

	if (!CardButton)
	{
		// SizeBox as root -- gives intrinsic size so the UniformGridPanel
		// can measure each cell. CanvasPanel would collapse to zero.
		USizeBox* Root = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("CardSizeBox"));
		WidgetTree->RootWidget = Root;
		Root->SetMinDesiredWidth(140.f);
		Root->SetMinDesiredHeight(110.f);

		// Button fills the size box.
		CardButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("CardButton"));
		FButtonStyle BtnStyle = CardButton->GetStyle();
		BtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.20f, 0.20f, 0.35f, 1.f));
		BtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.35f, 0.35f, 0.55f, 1.f));
		BtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.45f, 0.45f, 0.65f, 1.f));
		CardButton->SetStyle(BtnStyle);
		Root->AddChild(CardButton);

		// Vertical layout inside button: name + description.
		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CardVBox"));
		CardButton->AddChild(VBox);

		GameNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GameNameText"));
		GameNameText->SetText(FText::FromString(TEXT("Game")));
		FSlateFontInfo NameFont = GameNameText->GetFont();
		NameFont.Size = 14;
		GameNameText->SetFont(NameFont);
		GameNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		GameNameText->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* NameSlot = VBox->AddChildToVerticalBox(GameNameText);
		NameSlot->SetPadding(FMargin(6.f, 12.f, 6.f, 4.f));
		NameSlot->SetHorizontalAlignment(HAlign_Center);

		GameDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GameDescText"));
		GameDescriptionText->SetText(FText::FromString(TEXT("")));
		FSlateFontInfo DescFont = GameDescriptionText->GetFont();
		DescFont.Size = 9;
		GameDescriptionText->SetFont(DescFont);
		GameDescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f, 1.f)));
		GameDescriptionText->SetJustification(ETextJustify::Center);
		GameDescriptionText->SetAutoWrapText(true);
		UVerticalBoxSlot* DescSlot = VBox->AddChildToVerticalBox(GameDescriptionText);
		DescSlot->SetPadding(FMargin(6.f, 0.f, 6.f, 8.f));
		DescSlot->SetHorizontalAlignment(HAlign_Center);
	}

	return true;
}

void UPHCGameCardWidget::SetupCard(const FPHCGameGridItem& Item)
{
	GameType = Item.GameType;

	if (GameNameText)
	{
		GameNameText->SetText(FText::FromString(Item.Name));
	}
	if (GameDescriptionText)
	{
		GameDescriptionText->SetText(FText::FromString(Item.Description));
	}
	if (CardButton && !CardButton->OnClicked.IsBound())
	{
		CardButton->OnClicked.AddDynamic(this, &UPHCGameCardWidget::OnCardClicked);
	}
}

void UPHCGameCardWidget::OnCardClicked()
{
	OnGameCardClicked.ExecuteIfBound(GameType);
}

// ===================================================================
//  Games Panel
// ===================================================================

UPHCGamesPanel::UPHCGamesPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameCardWidgetClass = UPHCGameCardWidget::StaticClass();
}

bool UPHCGamesPanel::Initialize()
{
	bool bResult = Super::Initialize();
	if (!bResult) return false;

	// Only build if not already provided by Blueprint
	if (!GameGrid)
	{
		UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
		WidgetTree->RootWidget = Root;

		UBorder* Bg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Bg"));
		Bg->SetBrushColor(FLinearColor(0.051f, 0.051f, 0.102f, 1.0f));
		Bg->SetPadding(FMargin(12.f));

		UCanvasPanelSlot* BgSlot = Root->AddChildToCanvas(Bg);
		BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BgSlot->SetOffsets(FMargin(0.f));

		// Vertical layout: Title + scrollable grid
		UVerticalBox* VBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GamesVBox"));
		Bg->SetContent(VBox);

		UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TitleLabel"));
		Title->SetText(FText::FromString(TEXT("Games")));
		FSlateFontInfo Font = Title->GetFont();
		Font.Size = 24;
		Title->SetFont(Font);
		Title->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Title->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* TitleSlot = VBox->AddChildToVerticalBox(Title);
		TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));
		TitleSlot->SetHorizontalAlignment(HAlign_Center);

		// ScrollBox so the grid is scrollable
		UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("GamesScroll"));
		UVerticalBoxSlot* ScrollSlot = VBox->AddChildToVerticalBox(Scroll);
		ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// Uniform grid inside the scroll box
		GameGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("GameGrid"));
		GameGrid->SetSlotPadding(FMargin(6.f));
		Scroll->AddChild(GameGrid);
	}

	return true;
}

void UPHCGamesPanel::NativeConstruct()
{
	Super::NativeConstruct();

	// Build the master game list.
	AllGames = BuildGameList();

	// Optional search binding.
	if (SearchInput)
	{
		SearchInput->OnTextChanged.AddDynamic(this, &UPHCGamesPanel::OnSearchChanged);
	}

	// Populate the grid.
	PopulateGames();

	UE_LOG(LogPHCGamesPanel, Verbose, TEXT("GamesPanel constructed with %d games"), AllGames.Num());
}

// ===================================================================
//  Game Data
// ===================================================================

FString UPHCGamesPanel::GetGameDisplayName(EPHCGameType GameType)
{
	switch (GameType)
	{
	case EPHCGameType::Bingo:         return TEXT("Bingo");
	case EPHCGameType::ColorCards:    return TEXT("Color Cards");
	case EPHCGameType::LuckySlots:    return TEXT("Lucky Slots");
	case EPHCGameType::Connect4:      return TEXT("Connect 4");
	case EPHCGameType::Ludo:          return TEXT("Ludo");
	case EPHCGameType::PegGame:       return TEXT("Peg Game");
	case EPHCGameType::GuessSign:     return TEXT("Guess the Sign");
	case EPHCGameType::MapsQuiz:      return TEXT("Maps Quiz");
	case EPHCGameType::Pong:          return TEXT("Pong");
	case EPHCGameType::PackCatch:     return TEXT("Pack Catch");
	case EPHCGameType::BabyShark:     return TEXT("Baby Shark");
	case EPHCGameType::TempleRun:     return TEXT("Temple Run");
	case EPHCGameType::TowerDefense:  return TEXT("Tower Defense");
	case EPHCGameType::KitchenChaos:  return TEXT("Kitchen Chaos");
	case EPHCGameType::Pool:          return TEXT("Pool");
	case EPHCGameType::Carrom:        return TEXT("Carrom");
	case EPHCGameType::Tanks:         return TEXT("Tanks");
	case EPHCGameType::AngryDucks:    return TEXT("Angry Ducks");
	case EPHCGameType::GooBridge:     return TEXT("Goo Bridge");
	case EPHCGameType::Jenga:         return TEXT("Jenga");
	case EPHCGameType::Cornhole:      return TEXT("Cornhole");
	case EPHCGameType::CelDamage:    return TEXT("Cel Damage");
	case EPHCGameType::Simulator:    return TEXT("Playhouse City");
	default:                          return TEXT("Unknown");
	}
}

FString UPHCGamesPanel::GetGameDescription(EPHCGameType GameType)
{
	switch (GameType)
	{
	case EPHCGameType::Bingo:         return TEXT("Classic number-calling bingo with power-ups.");
	case EPHCGameType::ColorCards:    return TEXT("Match colors and patterns in fast-paced rounds.");
	case EPHCGameType::LuckySlots:    return TEXT("Spin the reels and try your luck!");
	case EPHCGameType::Connect4:      return TEXT("Drop discs to connect four in a row.");
	case EPHCGameType::Ludo:          return TEXT("Roll the dice and race around the board.");
	case EPHCGameType::PegGame:       return TEXT("Jump pegs to clear the board.");
	case EPHCGameType::GuessSign:     return TEXT("Identify road signs from around the world.");
	case EPHCGameType::MapsQuiz:      return TEXT("Guess locations on the world map.");
	case EPHCGameType::Pong:          return TEXT("Classic paddle vs paddle action.");
	case EPHCGameType::PackCatch:     return TEXT("Catch falling packages before they hit the ground.");
	case EPHCGameType::BabyShark:     return TEXT("Swim, dodge, and collect in the deep sea.");
	case EPHCGameType::TempleRun:     return TEXT("Endless runner through ancient temples.");
	case EPHCGameType::TowerDefense:  return TEXT("Build towers to defend your base from waves.");
	case EPHCGameType::KitchenChaos:  return TEXT("Cook and serve dishes under time pressure.");
	case EPHCGameType::Pool:          return TEXT("8-ball billiards with realistic physics.");
	case EPHCGameType::Carrom:        return TEXT("Flick and pocket carrom pieces.");
	case EPHCGameType::Tanks:         return TEXT("Command your tank in head-to-head battles.");
	case EPHCGameType::AngryDucks:    return TEXT("Launch ducks to topple structures.");
	case EPHCGameType::GooBridge:     return TEXT("Build bridges from goo to cross gaps.");
	case EPHCGameType::Jenga:         return TEXT("Carefully remove blocks without toppling the tower.");
	case EPHCGameType::Cornhole:      return TEXT("Toss bean bags for points.");
	case EPHCGameType::CelDamage:    return TEXT("Vehicular combat! Collect weapons and smash your opponent.");
	case EPHCGameType::Simulator:    return TEXT("Build your dream city and watch it grow!");
	default:                          return TEXT("");
	}
}

TArray<FPHCGameGridItem> UPHCGamesPanel::BuildGameList() const
{
	TArray<FPHCGameGridItem> Games;

	// Iterate all EPHCGameType values using the UEnum reflection.
	const UEnum* GameEnum = StaticEnum<EPHCGameType>();
	if (!GameEnum)
	{
		UE_LOG(LogPHCGamesPanel, Error, TEXT("Failed to get EPHCGameType enum reflection."));
		return Games;
	}

	for (int32 i = 0; i < GameEnum->NumEnums(); ++i)
	{
		// Skip the auto-generated _MAX entry and any hidden entries.
#if WITH_METADATA
		if (GameEnum->HasMetaData(TEXT("Hidden"), i))
		{
			continue;
		}
#endif

		// Skip _MAX entry by checking if its name contains "MAX".
		const FString EntryName = GameEnum->GetNameStringByIndex(i);
		if (EntryName.Contains(TEXT("MAX")))
		{
			continue;
		}

		const EPHCGameType Type = static_cast<EPHCGameType>(GameEnum->GetValueByIndex(i));

		FPHCGameGridItem Item;
		Item.GameType    = Type;
		Item.Name        = GetGameDisplayName(Type);
		Item.Description = GetGameDescription(Type);
		Item.IconPath    = FString::Printf(TEXT("/Game/UI/Icons/Games/%s"), *Item.Name.Replace(TEXT(" "), TEXT("")));

		Games.Add(Item);
	}

	return Games;
}

// ===================================================================
//  Grid Population
// ===================================================================

void UPHCGamesPanel::PopulateGames()
{
	ApplySearchFilter(TEXT(""));
}

void UPHCGamesPanel::ApplySearchFilter(const FString& SearchText)
{
	if (!GameGrid)
	{
		UE_LOG(LogPHCGamesPanel, Error, TEXT("GameGrid is null - cannot populate."));
		return;
	}

	// Clear existing cards.
	GameGrid->ClearChildren();
	CardWidgets.Empty();

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogPHCGamesPanel, Error, TEXT("No owning PlayerController."));
		return;
	}

	if (!GameCardWidgetClass)
	{
		UE_LOG(LogPHCGamesPanel, Warning, TEXT("GameCardWidgetClass not set. Cannot create game cards."));
		return;
	}

	int32 CardIndex = 0;
	for (const FPHCGameGridItem& Game : AllGames)
	{
		// Filter by search text if provided.
		if (!SearchText.IsEmpty())
		{
			if (!Game.Name.Contains(SearchText, ESearchCase::IgnoreCase) &&
				!Game.Description.Contains(SearchText, ESearchCase::IgnoreCase))
			{
				continue;
			}
		}

		UPHCGameCardWidget* Card = CreateWidget<UPHCGameCardWidget>(PC, GameCardWidgetClass);
		if (!Card)
		{
			UE_LOG(LogPHCGamesPanel, Error, TEXT("Failed to create game card for: %s"), *Game.Name);
			continue;
		}

		Card->SetupCard(Game);
		Card->OnGameCardClicked.BindUObject(this, &UPHCGamesPanel::OnGameSelected);

		// Calculate grid row and column.
		const int32 Row = CardIndex / GridColumns;
		const int32 Col = CardIndex % GridColumns;
		GameGrid->AddChildToUniformGrid(Card, Row, Col);

		CardWidgets.Add(Card);
		++CardIndex;
	}

	UE_LOG(LogPHCGamesPanel, Verbose, TEXT("Populated %d game cards (%d columns)"),
		CardIndex, GridColumns);
}

// ===================================================================
//  Callbacks
// ===================================================================

void UPHCGamesPanel::OnGameSelected(EPHCGameType GameType)
{
	UE_LOG(LogPHCGamesPanel, Log, TEXT("Game selected: %s"),
		*GetGameDisplayName(GameType));

	UPHCMainHUD* MainHUD = GetMainHUD();
	if (MainHUD)
	{
		MainHUD->NavigateToGame(GameType);
	}
}

void UPHCGamesPanel::OnSearchChanged(const FText& Text)
{
	ApplySearchFilter(Text.ToString());
}

// ===================================================================
//  Helpers
// ===================================================================

UPHCMainHUD* UPHCGamesPanel::GetMainHUD() const
{
	// Walk the UObject Outer chain from our parent widget.
	// Our parent (ContentSwitcher) was created by the MainHUD's WidgetTree,
	// so its Outer chain leads: ContentSwitcher → WidgetTree → MainHUD.
	UObject* Current = GetParent();
	while (Current)
	{
		if (UPHCMainHUD* HUD = Cast<UPHCMainHUD>(Current))
		{
			return HUD;
		}
		Current = Current->GetOuter();
	}
	return nullptr;
}
