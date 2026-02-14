// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/ColorCards/ColorCardsHUD.h"
#include "Games/ColorCards/ColorCardsGameMode.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogColorCardsHUD, Log, All);

// -- Card colours used outside Initialize() (wrapped to avoid unity build conflicts) --
namespace
{
const FLinearColor CardRed(0.9f, 0.15f, 0.15f, 1.f);
const FLinearColor CardBlue(0.15f, 0.3f, 0.9f, 1.f);
const FLinearColor CardGreen(0.15f, 0.8f, 0.15f, 1.f);
const FLinearColor CardYellow(0.95f, 0.85f, 0.1f, 1.f);
const FLinearColor CardWild(0.35f, 0.35f, 0.35f, 1.f);
}

// ---------------------------------------------------------------------------
// Initialize - build the Color Cards UI programmatically
// ---------------------------------------------------------------------------

bool UColorCardsHUD::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (HandScrollBox)
	{
		return true; // Already built.
	}

	// -- Colors ---------------------------------------------------------------
	const FLinearColor TableBgColor(0.05f, 0.2f, 0.05f, 1.f);
	const FLinearColor BtnNormal(0.2f, 0.3f, 0.55f, 1.f);
	const FLinearColor BtnHover(0.3f, 0.4f, 0.7f, 1.f);
	const FLinearColor BtnPress(0.15f, 0.2f, 0.4f, 1.f);

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo LargeFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20);
	const FSlateFontInfo MediumFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo SmallFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14);
	const FSlateFontInfo CardFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);

	// ==== Game area (dark green felt background) ====

	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CC_GameArea"));
	GameArea->SetBrushColor(TableBgColor);
	GameArea->SetPadding(FMargin(10.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("CC_ContentVBox"));
	GameArea->SetContent(ContentVBox);

	// ==== 1. CPU hand indicator (top) ====

	CPUHandText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CC_CPUHandText"));
	CPUHandText->SetText(FText::FromString(TEXT("CPU 2: 7  |  CPU 3: 7  |  CPU 4: 7")));
	CPUHandText->SetFont(SmallFont);
	CPUHandText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.8f)));
	CPUHandText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* CPUSlot = ContentVBox->AddChildToVerticalBox(CPUHandText);
	CPUSlot->SetHorizontalAlignment(HAlign_Center);
	CPUSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));

	// ==== 2. Turn status text ====

	TurnStatusText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CC_TurnStatus"));
	TurnStatusText->SetText(FText::FromString(TEXT("Your Turn")));
	TurnStatusText->SetFont(LargeFont);
	TurnStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	TurnStatusText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* TurnSlot = ContentVBox->AddChildToVerticalBox(TurnStatusText);
	TurnSlot->SetHorizontalAlignment(HAlign_Center);
	TurnSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 8.f));

	// ==== 3. Discard pile (centre) ====

	UHorizontalBox* CenterRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CC_CenterRow"));
	UVerticalBoxSlot* CenterSlot = ContentVBox->AddChildToVerticalBox(CenterRow);
	CenterSlot->SetHorizontalAlignment(HAlign_Center);
	CenterSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	CenterSlot->SetPadding(FMargin(0.f, 8.f));

	// -- Discard card visual --
	USizeBox* DiscardSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("CC_DiscardSize"));
	DiscardSize->SetMinDesiredWidth(100.f);
	DiscardSize->SetMinDesiredHeight(140.f);

	DiscardBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CC_DiscardBorder"));
	DiscardBorder->SetBrushColor(CardRed);
	DiscardBorder->SetPadding(FMargin(8.f));

	DiscardValueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CC_DiscardValue"));
	DiscardValueText->SetText(FText::FromString(TEXT("R 0")));
	DiscardValueText->SetFont(LargeFont);
	DiscardValueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	DiscardValueText->SetJustification(ETextJustify::Center);

	DiscardBorder->SetContent(DiscardValueText);
	DiscardSize->AddChild(DiscardBorder);

	UHorizontalBoxSlot* DiscardSlot = CenterRow->AddChildToHorizontalBox(DiscardSize);
	DiscardSlot->SetHorizontalAlignment(HAlign_Center);
	DiscardSlot->SetVerticalAlignment(VAlign_Center);
	DiscardSlot->SetPadding(FMargin(8.f, 0.f));

	// -- Current active colour indicator --
	UVerticalBox* ColorInfoVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("CC_ColorInfoVBox"));

	UTextBlock* ActiveLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CC_ActiveLabel"));
	ActiveLabel->SetText(FText::FromString(TEXT("Active Color")));
	ActiveLabel->SetFont(SmallFont);
	ActiveLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.7f)));
	ActiveLabel->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* ALSlot = ColorInfoVBox->AddChildToVerticalBox(ActiveLabel);
	ALSlot->SetHorizontalAlignment(HAlign_Center);
	ALSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	USizeBox* ColorSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("CC_ColorSizeBox"));
	ColorSizeBox->SetMinDesiredWidth(60.f);
	ColorSizeBox->SetMinDesiredHeight(60.f);

	CurrentColorBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CC_CurrentColorBorder"));
	CurrentColorBorder->SetBrushColor(CardRed);
	CurrentColorBorder->SetPadding(FMargin(4.f));

	CurrentColorLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CC_CurrentColorLabel"));
	CurrentColorLabel->SetText(FText::FromString(TEXT("Red")));
	CurrentColorLabel->SetFont(SmallFont);
	CurrentColorLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CurrentColorLabel->SetJustification(ETextJustify::Center);

	CurrentColorBorder->SetContent(CurrentColorLabel);
	ColorSizeBox->AddChild(CurrentColorBorder);

	UVerticalBoxSlot* ColorBoxSlot = ColorInfoVBox->AddChildToVerticalBox(ColorSizeBox);
	ColorBoxSlot->SetHorizontalAlignment(HAlign_Center);

	UHorizontalBoxSlot* ColorInfoSlot = CenterRow->AddChildToHorizontalBox(ColorInfoVBox);
	ColorInfoSlot->SetHorizontalAlignment(HAlign_Center);
	ColorInfoSlot->SetVerticalAlignment(VAlign_Center);
	ColorInfoSlot->SetPadding(FMargin(16.f, 0.f));

	// ==== 4. Draw card button ====

	UHorizontalBox* DrawRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CC_DrawRow"));
	UVerticalBoxSlot* DrawRowSlot = ContentVBox->AddChildToVerticalBox(DrawRow);
	DrawRowSlot->SetHorizontalAlignment(HAlign_Center);
	DrawRowSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));

	USizeBox* DrawBtnSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("CC_DrawBtnSize"));
	DrawBtnSize->SetMinDesiredWidth(160.f);
	DrawBtnSize->SetMinDesiredHeight(40.f);

	DrawCardButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("CC_DrawCardBtn"));
	{
		FButtonStyle Style = DrawCardButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(BtnNormal);
		Style.Hovered.TintColor = FSlateColor(BtnHover);
		Style.Pressed.TintColor = FSlateColor(BtnPress);
		DrawCardButton->SetStyle(Style);
	}

	DrawCardLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CC_DrawCardLabel"));
	DrawCardLabel->SetText(FText::FromString(TEXT("Draw Card")));
	DrawCardLabel->SetFont(MediumFont);
	DrawCardLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	DrawCardLabel->SetJustification(ETextJustify::Center);

	DrawCardButton->AddChild(DrawCardLabel);
	DrawBtnSize->AddChild(DrawCardButton);

	UHorizontalBoxSlot* DrawBtnSlot = DrawRow->AddChildToHorizontalBox(DrawBtnSize);
	DrawBtnSlot->SetHorizontalAlignment(HAlign_Center);

	// ==== 5. Player hand (bottom, scrollable) ====

	HandScrollBox = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(), TEXT("CC_HandScrollBox"));
	HandScrollBox->SetOrientation(Orient_Horizontal);
	HandScrollBox->SetScrollBarVisibility(ESlateVisibility::Collapsed);

	UVerticalBoxSlot* HandSlot = ContentVBox->AddChildToVerticalBox(HandScrollBox);
	HandSlot->SetHorizontalAlignment(HAlign_Center);
	HandSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 8.f));

	// ==== 6. Wild colour chooser overlay ====

	ColorChooserOverlay = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("CC_ColorChooserOverlay"));
	ColorChooserOverlay->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(ColorChooserOverlay);
	OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	OverlaySlot->SetOffsets(FMargin(0.f));

	// Semi-transparent backdrop
	UBorder* OverlayBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CC_OverlayBg"));
	OverlayBg->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
	OverlayBg->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* BgSlot = ColorChooserOverlay->AddChildToCanvas(OverlayBg);
	BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BgSlot->SetOffsets(FMargin(0.f));

	// Centre the chooser buttons
	UVerticalBox* ChooserVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("CC_ChooserVBox"));

	UTextBlock* ChooserTitle = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CC_ChooserTitle"));
	ChooserTitle->SetText(FText::FromString(TEXT("Choose a Color")));
	ChooserTitle->SetFont(LargeFont);
	ChooserTitle->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	ChooserTitle->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* TitleSlot = ChooserVBox->AddChildToVerticalBox(ChooserTitle);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));

	// Row of 4 colour buttons
	UHorizontalBox* ColorBtnRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CC_ColorBtnRow"));
	UVerticalBoxSlot* ColorBtnRowSlot = ChooserVBox->AddChildToVerticalBox(ColorBtnRow);
	ColorBtnRowSlot->SetHorizontalAlignment(HAlign_Center);

	auto MakeColorButton = [&](const TCHAR* Name, const FLinearColor& Color,
		const FString& Label) -> UButton*
	{
		USizeBox* BtnSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("CC_%sSize"), Name));
		BtnSize->SetMinDesiredWidth(90.f);
		BtnSize->SetMinDesiredHeight(60.f);

		UButton* Btn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), *FString::Printf(TEXT("CC_%sBtn"), Name));
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor = FSlateColor(Color);
		Style.Hovered.TintColor = FSlateColor(Color * 1.2f);
		Style.Pressed.TintColor = FSlateColor(Color * 0.7f);
		Btn->SetStyle(Style);

		UTextBlock* BtnLbl = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("CC_%sLbl"), Name));
		BtnLbl->SetText(FText::FromString(Label));
		BtnLbl->SetFont(MediumFont);
		BtnLbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BtnLbl->SetJustification(ETextJustify::Center);

		Btn->AddChild(BtnLbl);
		BtnSize->AddChild(Btn);

		UHorizontalBoxSlot* Slot = ColorBtnRow->AddChildToHorizontalBox(BtnSize);
		Slot->SetPadding(FMargin(6.f, 0.f));
		Slot->SetHorizontalAlignment(HAlign_Center);

		return Btn;
	};

	WildRedButton    = MakeColorButton(TEXT("WildRed"),    CardRed,    TEXT("Red"));
	WildBlueButton   = MakeColorButton(TEXT("WildBlue"),   CardBlue,   TEXT("Blue"));
	WildGreenButton  = MakeColorButton(TEXT("WildGreen"),  CardGreen,  TEXT("Green"));
	WildYellowButton = MakeColorButton(TEXT("WildYellow"), CardYellow, TEXT("Yellow"));

	// Place the chooser VBox centred in the overlay
	UCanvasPanelSlot* ChooserSlot = ColorChooserOverlay->AddChildToCanvas(ChooserVBox);
	ChooserSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	ChooserSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ChooserSlot->SetAutoSize(true);

	UE_LOG(LogColorCardsHUD, Log, TEXT("ColorCards HUD built programmatically."));

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct - wire up delegates from ColorCardsGameMode
// ---------------------------------------------------------------------------

void UColorCardsHUD::NativeConstruct()
{
	Super::NativeConstruct();

	ColorCardsMode = Cast<AColorCardsGameMode>(GameModeRef);

	if (ColorCardsMode)
	{
		ColorCardsMode->OnCardPlayed.AddDynamic(this, &UColorCardsHUD::HandleCardPlayed);
		ColorCardsMode->OnCardDrawn.AddDynamic(this, &UColorCardsHUD::HandleCardDrawn);
		ColorCardsMode->OnTurnChanged.AddDynamic(this, &UColorCardsHUD::HandleTurnChanged);
		ColorCardsMode->OnColorChosen.AddDynamic(this, &UColorCardsHUD::HandleColorChosen);
		ColorCardsMode->OnWinner.AddDynamic(this, &UColorCardsHUD::HandleWinner);

		// Initial display.
		RefreshHand();
		RefreshDiscardPile();
		RefreshCPUHandCounts();
		RefreshTurnStatus();
	}

	// Wire button clicks.
	if (DrawCardButton)
	{
		DrawCardButton->OnClicked.AddDynamic(this, &UColorCardsHUD::OnDrawCardClicked);
	}
	if (WildRedButton)
	{
		WildRedButton->OnClicked.AddDynamic(this, &UColorCardsHUD::OnWildRedClicked);
	}
	if (WildBlueButton)
	{
		WildBlueButton->OnClicked.AddDynamic(this, &UColorCardsHUD::OnWildBlueClicked);
	}
	if (WildGreenButton)
	{
		WildGreenButton->OnClicked.AddDynamic(this, &UColorCardsHUD::OnWildGreenClicked);
	}
	if (WildYellowButton)
	{
		WildYellowButton->OnClicked.AddDynamic(this, &UColorCardsHUD::OnWildYellowClicked);
	}
}

// ---------------------------------------------------------------------------
// Refresh helpers
// ---------------------------------------------------------------------------

void UColorCardsHUD::RefreshHand()
{
	if (!ColorCardsMode || !HandScrollBox)
	{
		return;
	}

	const TArray<FPHCUnoCard> Hand = ColorCardsMode->GetPlayerHand();
	CachedHand = Hand;
	CardButtons.Reset();
	HandScrollBox->ClearChildren();

	const FSlateFontInfo CardFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);

	const bool bIsPlayerTurn = (ColorCardsMode->GetCurrentPlayerIndex() == 0);

	for (int32 i = 0; i < Hand.Num(); ++i)
	{
		const FPHCUnoCard& Card = Hand[i];

		USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("CC_Card%d_Size"), i));
		CardSize->SetMinDesiredWidth(70.f);
		CardSize->SetMinDesiredHeight(100.f);

		UButton* CardBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), *FString::Printf(TEXT("CC_Card%d_Btn"), i));

		const FLinearColor BtnColor = CardColorToLinear(Card.Color);
		FButtonStyle Style = CardBtn->GetStyle();
		Style.Normal.TintColor = FSlateColor(BtnColor);
		Style.Hovered.TintColor = FSlateColor(BtnColor * 1.2f);
		Style.Pressed.TintColor = FSlateColor(BtnColor * 0.8f);
		CardBtn->SetStyle(Style);

		const bool bPlayable = bIsPlayerTurn && ColorCardsMode->CanPlayCard(Card);
		CardBtn->SetIsEnabled(bPlayable);

		UTextBlock* CardLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("CC_Card%d_Lbl"), i));
		CardLabel->SetText(FText::FromString(CardValueToString(Card)));
		CardLabel->SetFont(CardFont);
		CardLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		CardLabel->SetJustification(ETextJustify::Center);

		CardBtn->AddChild(CardLabel);
		CardBtn->OnClicked.AddDynamic(this, &UColorCardsHUD::HandleAnyCardClicked);

		CardSize->AddChild(CardBtn);
		HandScrollBox->AddChild(CardSize);

		CardButtons.Add(CardBtn);
	}
}

void UColorCardsHUD::RefreshDiscardPile()
{
	if (!ColorCardsMode)
	{
		return;
	}

	const FPHCUnoCard TopCard = ColorCardsMode->GetTopDiscard();
	const EPHCCardColor ActiveColor = ColorCardsMode->GetCurrentColor();

	if (DiscardBorder)
	{
		DiscardBorder->SetBrushColor(CardColorToLinear(TopCard.Color));
	}
	if (DiscardValueText)
	{
		DiscardValueText->SetText(FText::FromString(CardValueToString(TopCard)));
	}

	// Active colour indicator (may differ from card colour after a Wild).
	if (CurrentColorBorder)
	{
		CurrentColorBorder->SetBrushColor(CardColorToLinear(ActiveColor));
	}
	if (CurrentColorLabel)
	{
		FString ColorName;
		switch (ActiveColor)
		{
		case EPHCCardColor::Red:    ColorName = TEXT("Red");    break;
		case EPHCCardColor::Blue:   ColorName = TEXT("Blue");   break;
		case EPHCCardColor::Green:  ColorName = TEXT("Green");  break;
		case EPHCCardColor::Yellow: ColorName = TEXT("Yellow"); break;
		default:                    ColorName = TEXT("Wild");    break;
		}
		CurrentColorLabel->SetText(FText::FromString(ColorName));
	}
}

void UColorCardsHUD::RefreshCPUHandCounts()
{
	if (!ColorCardsMode || !CPUHandText)
	{
		return;
	}

	FString Counts;
	for (int32 i = 1; i < NUM_PLAYERS; ++i)
	{
		if (i > 1)
		{
			Counts += TEXT("  |  ");
		}
		Counts += FString::Printf(TEXT("CPU %d: %d cards"),
			i, ColorCardsMode->GetHandCount(i));
	}
	CPUHandText->SetText(FText::FromString(Counts));
}

void UColorCardsHUD::RefreshTurnStatus()
{
	if (!ColorCardsMode || !TurnStatusText)
	{
		return;
	}

	const int32 Current = ColorCardsMode->GetCurrentPlayerIndex();
	if (Current == 0)
	{
		TurnStatusText->SetText(FText::FromString(TEXT("Your Turn")));
		TurnStatusText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}
	else
	{
		TurnStatusText->SetText(FText::FromString(
			FString::Printf(TEXT("CPU %d's Turn"), Current)));
		TurnStatusText->SetColorAndOpacity(
			FSlateColor(FLinearColor(0.6f, 0.6f, 0.7f)));
	}

	const bool bIsPlayerTurn = (Current == 0);
	SetHandInteractable(bIsPlayerTurn);
	if (DrawCardButton)
	{
		DrawCardButton->SetIsEnabled(bIsPlayerTurn);
	}
}

void UColorCardsHUD::SetHandInteractable(bool bEnabled)
{
	if (!ColorCardsMode)
	{
		return;
	}

	for (int32 i = 0; i < CardButtons.Num(); ++i)
	{
		if (CardButtons[i])
		{
			if (bEnabled && CachedHand.IsValidIndex(i))
			{
				CardButtons[i]->SetIsEnabled(ColorCardsMode->CanPlayCard(CachedHand[i]));
			}
			else
			{
				CardButtons[i]->SetIsEnabled(false);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

FLinearColor UColorCardsHUD::CardColorToLinear(EPHCCardColor InColor)
{
	switch (InColor)
	{
	case EPHCCardColor::Red:    return CardRed;
	case EPHCCardColor::Blue:   return CardBlue;
	case EPHCCardColor::Green:  return CardGreen;
	case EPHCCardColor::Yellow: return CardYellow;
	case EPHCCardColor::Wild:   return CardWild;
	default:                    return FLinearColor::White;
	}
}

FString UColorCardsHUD::CardValueToString(const FPHCUnoCard& Card)
{
	FString ColorPrefix;
	switch (Card.Color)
	{
	case EPHCCardColor::Red:    ColorPrefix = TEXT("R"); break;
	case EPHCCardColor::Blue:   ColorPrefix = TEXT("B"); break;
	case EPHCCardColor::Green:  ColorPrefix = TEXT("G"); break;
	case EPHCCardColor::Yellow: ColorPrefix = TEXT("Y"); break;
	case EPHCCardColor::Wild:   ColorPrefix = TEXT("W"); break;
	}

	FString ValueStr;
	switch (Card.Value)
	{
	case EPHCCardValue::Zero:           ValueStr = TEXT("0"); break;
	case EPHCCardValue::One:            ValueStr = TEXT("1"); break;
	case EPHCCardValue::Two:            ValueStr = TEXT("2"); break;
	case EPHCCardValue::Three:          ValueStr = TEXT("3"); break;
	case EPHCCardValue::Four:           ValueStr = TEXT("4"); break;
	case EPHCCardValue::Five:           ValueStr = TEXT("5"); break;
	case EPHCCardValue::Six:            ValueStr = TEXT("6"); break;
	case EPHCCardValue::Seven:          ValueStr = TEXT("7"); break;
	case EPHCCardValue::Eight:          ValueStr = TEXT("8"); break;
	case EPHCCardValue::Nine:           ValueStr = TEXT("9"); break;
	case EPHCCardValue::Skip:           ValueStr = TEXT("S"); break;
	case EPHCCardValue::Reverse:        ValueStr = TEXT("R"); break;
	case EPHCCardValue::DrawTwo:        ValueStr = TEXT("+2"); break;
	case EPHCCardValue::WildCard:       ValueStr = TEXT("W"); break;
	case EPHCCardValue::WildDrawFour:   ValueStr = TEXT("+4"); break;
	}

	return FString::Printf(TEXT("%s %s"), *ColorPrefix, *ValueStr);
}

// ---------------------------------------------------------------------------
// Game mode delegate handlers
// ---------------------------------------------------------------------------

void UColorCardsHUD::HandleCardPlayed(int32 PlayerIndex, FPHCUnoCard Card)
{
	RefreshDiscardPile();
	RefreshCPUHandCounts();
	RefreshHand();

	// If the human player just played a Wild, show the colour chooser.
	if (PlayerIndex == 0 && Card.IsWild())
	{
		if (ColorChooserOverlay)
		{
			ColorChooserOverlay->SetVisibility(ESlateVisibility::Visible);
		}
	}
}

void UColorCardsHUD::HandleCardDrawn(int32 PlayerIndex)
{
	RefreshCPUHandCounts();
	RefreshHand();
}

void UColorCardsHUD::HandleTurnChanged(int32 NewPlayerIndex)
{
	RefreshTurnStatus();
	RefreshHand();
}

void UColorCardsHUD::HandleColorChosen(EPHCCardColor ChosenColor)
{
	// Hide the overlay and refresh the discard display.
	if (ColorChooserOverlay)
	{
		ColorChooserOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	RefreshDiscardPile();
}

void UColorCardsHUD::HandleWinner(int32 WinnerIndex)
{
	// Disable all interaction.
	SetHandInteractable(false);
	if (DrawCardButton)
	{
		DrawCardButton->SetIsEnabled(false);
	}

	if (TurnStatusText)
	{
		if (WinnerIndex == 0)
		{
			TurnStatusText->SetText(FText::FromString(TEXT("You Win!")));
			TurnStatusText->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.2f, 0.9f, 0.3f)));
		}
		else
		{
			TurnStatusText->SetText(FText::FromString(
				FString::Printf(TEXT("CPU %d Wins!"), WinnerIndex)));
			TurnStatusText->SetColorAndOpacity(
				FSlateColor(FLinearColor(0.9f, 0.3f, 0.2f)));
		}
	}
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UColorCardsHUD::OnDrawCardClicked()
{
	if (ColorCardsMode)
	{
		ColorCardsMode->DrawCard();
	}
}

void UColorCardsHUD::HandleAnyCardClicked()
{
	if (!ColorCardsMode)
	{
		return;
	}

	// Identify which card button was clicked via IsHovered().
	for (int32 i = 0; i < CardButtons.Num(); ++i)
	{
		if (CardButtons[i] && CardButtons[i]->IsHovered())
		{
			ColorCardsMode->PlayCard(i);
			break;
		}
	}
}

void UColorCardsHUD::OnWildRedClicked()
{
	if (ColorChooserOverlay)
	{
		ColorChooserOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ColorCardsMode)
	{
		ColorCardsMode->ChooseWildColor(EPHCCardColor::Red);
	}
}

void UColorCardsHUD::OnWildBlueClicked()
{
	if (ColorChooserOverlay)
	{
		ColorChooserOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ColorCardsMode)
	{
		ColorCardsMode->ChooseWildColor(EPHCCardColor::Blue);
	}
}

void UColorCardsHUD::OnWildGreenClicked()
{
	if (ColorChooserOverlay)
	{
		ColorChooserOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ColorCardsMode)
	{
		ColorCardsMode->ChooseWildColor(EPHCCardColor::Green);
	}
}

void UColorCardsHUD::OnWildYellowClicked()
{
	if (ColorChooserOverlay)
	{
		ColorChooserOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (ColorCardsMode)
	{
		ColorCardsMode->ChooseWildColor(EPHCCardColor::Yellow);
	}
}
