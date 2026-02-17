// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/LuckySlots/SlotsHUD.h"
#include "Games/LuckySlots/SlotsGameMode.h"
#include "Games/LuckySlots/SlotsWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogSlotsHUD, Log, All);

// ---------------------------------------------------------------------------
// Initialize - build the Lucky Slots UI
// ---------------------------------------------------------------------------

bool USlotsHUD::Initialize()
{
	// Build base HUD widgets first (score, timer, pause, game over overlays).
	if (!Super::Initialize())
	{
		return false;
	}

	// Casino colour palette
	const FLinearColor DarkPurpleBg(0.15f, 0.05f, 0.25f, 1.f);
	const FLinearColor SlotsGoldAccent(1.f, 0.843f, 0.f, 1.f);     // #FFD700
	const FLinearColor ReelBgColor(0.08f, 0.02f, 0.14f, 1.f);
	const FLinearColor SpinBtnNormal(0.7f, 0.55f, 0.f, 1.f);      // Dark gold
	const FLinearColor SpinBtnHover(0.85f, 0.7f, 0.1f, 1.f);
	const FLinearColor SpinBtnPress(0.5f, 0.4f, 0.f, 1.f);
	const FLinearColor SmallBtnNormal(0.3f, 0.15f, 0.45f, 1.f);
	const FLinearColor SmallBtnHover(0.45f, 0.25f, 0.6f, 1.f);
	const FLinearColor SmallBtnPress(0.2f, 0.1f, 0.3f, 1.f);
	const FLinearColor WinTextColor(0.f, 1.f, 0.4f, 1.f);

	if (SpinButton)
	{
		return true; // Already built.
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo TitleFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 28);
	const FSlateFontInfo ReelFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 36);
	const FSlateFontInfo BtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);
	const FSlateFontInfo InfoFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo SmallBtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);

	// ---- Game content area (dark purple background) ----
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("SlotsGameArea"));
	GameArea->SetBrushColor(DarkPurpleBg);
	GameArea->SetPadding(FMargin(20.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("SlotsContentVBox"));
	GameArea->SetContent(ContentVBox);

	// ---- Title text: "LUCKY SLOTS" ----
	TitleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SlotsTitleText"));
	TitleText->SetText(FText::FromString(TEXT("LUCKY SLOTS")));
	TitleText->SetFont(TitleFont);
	TitleText->SetColorAndOpacity(FSlateColor(SlotsGoldAccent));
	TitleText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* TitleSlot = ContentVBox->AddChildToVerticalBox(TitleText);
	TitleSlot->SetHorizontalAlignment(HAlign_Center);
	TitleSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 16.f));

	// ---- Reel display (3 reels side by side) ----
	UHorizontalBox* ReelRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ReelRow"));
	UVerticalBoxSlot* ReelRowSlot = ContentVBox->AddChildToVerticalBox(ReelRow);
	ReelRowSlot->SetHorizontalAlignment(HAlign_Center);
	ReelRowSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 16.f));

	ReelBorders.SetNum(NUM_REELS);
	ReelLabels.SetNum(NUM_REELS);

	for (int32 i = 0; i < NUM_REELS; ++i)
	{
		USizeBox* ReelSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), *FString::Printf(TEXT("ReelSize%d"), i));
		ReelSize->SetMinDesiredWidth(100.f);
		ReelSize->SetMinDesiredHeight(100.f);

		UBorder* ReelBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("ReelBorder%d"), i));
		ReelBorder->SetBrushColor(ReelBgColor);
		ReelBorder->SetPadding(FMargin(8.f));

		UTextBlock* ReelLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("ReelLabel%d"), i));
		ReelLabel->SetText(FText::FromString(TEXT("?")));
		ReelLabel->SetFont(ReelFont);
		ReelLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		ReelLabel->SetJustification(ETextJustify::Center);

		ReelBorder->SetContent(ReelLabel);
		ReelSize->AddChild(ReelBorder);

		UHorizontalBoxSlot* ReelSlot = ReelRow->AddChildToHorizontalBox(ReelSize);
		ReelSlot->SetPadding(FMargin(6.f, 0.f));
		ReelSlot->SetHorizontalAlignment(HAlign_Center);
		ReelSlot->SetVerticalAlignment(VAlign_Center);

		ReelBorders[i] = ReelBorder;
		ReelLabels[i] = ReelLabel;
	}

	// ---- Win text (hidden by default) ----
	WinText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SlotsWinText"));
	WinText->SetText(FText::FromString(TEXT("")));
	WinText->SetFont(BtnFont);
	WinText->SetColorAndOpacity(FSlateColor(WinTextColor));
	WinText->SetJustification(ETextJustify::Center);
	WinText->SetVisibility(ESlateVisibility::Hidden);

	UVerticalBoxSlot* WinSlot = ContentVBox->AddChildToVerticalBox(WinText);
	WinSlot->SetHorizontalAlignment(HAlign_Center);
	WinSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));

	// ---- SPIN button (large, prominent, centered) ----
	USizeBox* SpinSize = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("SpinBtnSize"));
	SpinSize->SetMinDesiredWidth(200.f);
	SpinSize->SetMinDesiredHeight(60.f);

	SpinButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("SpinButton"));
	{
		FButtonStyle Style = SpinButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(SpinBtnNormal);
		Style.Hovered.TintColor = FSlateColor(SpinBtnHover);
		Style.Pressed.TintColor = FSlateColor(SpinBtnPress);
		SpinButton->SetStyle(Style);
	}

	SpinButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SpinBtnLabel"));
	SpinButtonLabel->SetText(FText::FromString(TEXT("SPIN!")));
	SpinButtonLabel->SetFont(BtnFont);
	SpinButtonLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SpinButtonLabel->SetJustification(ETextJustify::Center);
	SpinButton->AddChild(SpinButtonLabel);

	SpinSize->AddChild(SpinButton);

	UVerticalBoxSlot* SpinSlot = ContentVBox->AddChildToVerticalBox(SpinSize);
	SpinSlot->SetHorizontalAlignment(HAlign_Center);
	SpinSlot->SetPadding(FMargin(0.f, 12.f, 0.f, 12.f));

	// ---- Bet controls row: [-] Bet: 10 [+] ----
	UHorizontalBox* BetRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("BetRow"));
	UVerticalBoxSlot* BetRowSlot = ContentVBox->AddChildToVerticalBox(BetRow);
	BetRowSlot->SetHorizontalAlignment(HAlign_Center);
	BetRowSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));

	// [-] button
	{
		USizeBox* BtnSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("BetDownSize"));
		BtnSize->SetMinDesiredWidth(50.f);
		BtnSize->SetMinDesiredHeight(40.f);

		BetDownButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("BetDownButton"));
		FButtonStyle Style = BetDownButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(SmallBtnNormal);
		Style.Hovered.TintColor = FSlateColor(SmallBtnHover);
		Style.Pressed.TintColor = FSlateColor(SmallBtnPress);
		BetDownButton->SetStyle(Style);

		UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("BetDownLbl"));
		Lbl->SetText(FText::FromString(TEXT("-")));
		Lbl->SetFont(SmallBtnFont);
		Lbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Lbl->SetJustification(ETextJustify::Center);
		BetDownButton->AddChild(Lbl);

		BtnSize->AddChild(BetDownButton);

		UHorizontalBoxSlot* S = BetRow->AddChildToHorizontalBox(BtnSize);
		S->SetPadding(FMargin(4.f, 0.f));
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	// Bet text
	BetText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SlotsBetText"));
	BetText->SetText(FText::FromString(TEXT("Bet: 10")));
	BetText->SetFont(InfoFont);
	BetText->SetColorAndOpacity(FSlateColor(SlotsGoldAccent));
	BetText->SetJustification(ETextJustify::Center);
	{
		UHorizontalBoxSlot* S = BetRow->AddChildToHorizontalBox(BetText);
		S->SetPadding(FMargin(8.f, 0.f));
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	// [+] button
	{
		USizeBox* BtnSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("BetUpSize"));
		BtnSize->SetMinDesiredWidth(50.f);
		BtnSize->SetMinDesiredHeight(40.f);

		BetUpButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("BetUpButton"));
		FButtonStyle Style = BetUpButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(SmallBtnNormal);
		Style.Hovered.TintColor = FSlateColor(SmallBtnHover);
		Style.Pressed.TintColor = FSlateColor(SmallBtnPress);
		BetUpButton->SetStyle(Style);

		UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("BetUpLbl"));
		Lbl->SetText(FText::FromString(TEXT("+")));
		Lbl->SetFont(SmallBtnFont);
		Lbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Lbl->SetJustification(ETextJustify::Center);
		BetUpButton->AddChild(Lbl);

		BtnSize->AddChild(BetUpButton);

		UHorizontalBoxSlot* S = BetRow->AddChildToHorizontalBox(BtnSize);
		S->SetPadding(FMargin(4.f, 0.f));
		S->SetHorizontalAlignment(HAlign_Center);
		S->SetVerticalAlignment(VAlign_Center);
	}

	// ---- Balance text ----
	BalanceText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SlotsBalanceText"));
	BalanceText->SetText(FText::FromString(TEXT("Balance: 0")));
	BalanceText->SetFont(InfoFont);
	BalanceText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	BalanceText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* BalSlot = ContentVBox->AddChildToVerticalBox(BalanceText);
	BalSlot->SetHorizontalAlignment(HAlign_Center);
	BalSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));

	UE_LOG(LogSlotsHUD, Log, TEXT("Lucky Slots HUD built: %d reels, spin button, bet controls"),
		ReelBorders.Num());

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct - wire up delegates
// ---------------------------------------------------------------------------

void USlotsHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Get the Slots game mode from the base class GameModeRef.
	SlotsMode = Cast<ASlotsGameMode>(GameModeRef);

	if (SlotsMode)
	{
		SlotsMode->OnSpinComplete.AddDynamic(this, &USlotsHUD::HandleSpinComplete);
		SlotsMode->OnWin.AddDynamic(this, &USlotsHUD::HandleWin);

		// Initialize display from current state.
		RefreshUI();

		// Show last results if any are available.
		const TArray<EPHCSlotSymbol> LastResults = SlotsMode->GetLastResults();
		for (int32 i = 0; i < FMath::Min(NUM_REELS, LastResults.Num()); ++i)
		{
			SetReelDisplay(i, LastResults[i]);
		}
	}

	// Wire button clicks.
	if (SpinButton)
	{
		SpinButton->OnClicked.AddDynamic(this, &USlotsHUD::OnSpinClicked);
	}
	if (BetUpButton)
	{
		BetUpButton->OnClicked.AddDynamic(this, &USlotsHUD::OnBetUpClicked);
	}
	if (BetDownButton)
	{
		BetDownButton->OnClicked.AddDynamic(this, &USlotsHUD::OnBetDownClicked);
	}
}

// ---------------------------------------------------------------------------
// NativeTick - animate reels during a spin
// ---------------------------------------------------------------------------

void USlotsHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bAnimating)
	{
		return;
	}

	AnimClock += InDeltaTime;

	for (int32 i = 0; i < NUM_REELS; ++i)
	{
		if (AnimClock < ReelStopTime[i])
		{
			// Still spinning -- show a random symbol for visual effect.
			const int32 RandSym = FMath::RandRange(0, static_cast<int32>(EPHCSlotSymbol::Wild));
			SetReelDisplay(i, static_cast<EPHCSlotSymbol>(RandSym));
		}
		else if (!PendingResults.IsValidIndex(i))
		{
			// No result for this reel -- skip.
			continue;
		}
		else
		{
			// Check if this reel just stopped (transition from spinning to stopped).
			// We detect this by checking if the label doesn't yet show the final symbol.
			const FString Expected = GetSymbolEmoji(PendingResults[i]);
			if (ReelLabels.IsValidIndex(i) && ReelLabels[i] &&
				ReelLabels[i]->GetText().ToString() != Expected)
			{
				SetReelDisplay(i, PendingResults[i]);
				++ReelsStopped;

				if (ReelsStopped >= NUM_REELS)
				{
					OnAllReelsStopped();
				}
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void USlotsHUD::HandleSpinComplete(const TArray<EPHCSlotSymbol>& Results)
{
	StartReelAnimation(Results);
}

void USlotsHUD::HandleWin(int32 PayoutAmount)
{
	if (WinText)
	{
		WinText->SetText(FText::FromString(
			FString::Printf(TEXT("WIN: %d"), PayoutAmount)));
		WinText->SetVisibility(ESlateVisibility::Visible);
	}
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void USlotsHUD::OnSpinClicked()
{
	if (bAnimating || !SlotsMode)
	{
		return;
	}

	// Hide previous win text.
	if (WinText)
	{
		WinText->SetVisibility(ESlateVisibility::Hidden);
	}

	// Disable spin button during animation.
	if (SpinButton)
	{
		SpinButton->SetIsEnabled(false);
	}

	SlotsMode->Spin();
}

void USlotsHUD::OnBetUpClicked()
{
	if (SlotsMode)
	{
		SlotsMode->SetBet(SlotsMode->GetBetAmount() + 5);
		RefreshUI();
	}
}

void USlotsHUD::OnBetDownClicked()
{
	if (SlotsMode)
	{
		SlotsMode->SetBet(SlotsMode->GetBetAmount() - 5);
		RefreshUI();
	}
}

// ---------------------------------------------------------------------------
// Reel animation
// ---------------------------------------------------------------------------

void USlotsHUD::StartReelAnimation(const TArray<EPHCSlotSymbol>& Results)
{
	bAnimating = true;
	AnimClock = 0.f;
	ReelsStopped = 0;
	PendingResults = Results;
}

void USlotsHUD::OnAllReelsStopped()
{
	bAnimating = false;

	// Re-enable spin button.
	if (SpinButton)
	{
		SpinButton->SetIsEnabled(true);
	}

	RefreshUI();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void USlotsHUD::RefreshUI()
{
	if (!SlotsMode)
	{
		return;
	}

	if (BetText)
	{
		BetText->SetText(FText::FromString(
			FString::Printf(TEXT("Bet: %d"), SlotsMode->GetBetAmount())));
	}
	if (BalanceText)
	{
		BalanceText->SetText(FText::FromString(
			FString::Printf(TEXT("Balance: %d"), SlotsMode->GetBalance())));
	}
}

void USlotsHUD::SetReelDisplay(int32 ReelIndex, EPHCSlotSymbol Symbol)
{
	if (!ReelBorders.IsValidIndex(ReelIndex) || !ReelLabels.IsValidIndex(ReelIndex))
	{
		return;
	}

	if (ReelBorders[ReelIndex])
	{
		ReelBorders[ReelIndex]->SetBrushColor(USlotsWidget::GetSymbolColor(Symbol));
	}

	if (ReelLabels[ReelIndex])
	{
		ReelLabels[ReelIndex]->SetText(FText::FromString(GetSymbolEmoji(Symbol)));
	}
}

FString USlotsHUD::GetSymbolEmoji(EPHCSlotSymbol Symbol)
{
	switch (Symbol)
	{
	case EPHCSlotSymbol::Cherry:  return TEXT("Cherry");
	case EPHCSlotSymbol::Lemon:   return TEXT("Lemon");
	case EPHCSlotSymbol::Orange:  return TEXT("Orange");
	case EPHCSlotSymbol::Plum:    return TEXT("Plum");
	case EPHCSlotSymbol::Bell:    return TEXT("Bell");
	case EPHCSlotSymbol::Bar:     return TEXT("BAR");
	case EPHCSlotSymbol::Seven:   return TEXT("777");
	case EPHCSlotSymbol::Diamond: return TEXT("Diam.");
	case EPHCSlotSymbol::Wild:    return TEXT("WILD");
	default:                      return TEXT("?");
	}
}
