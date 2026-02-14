// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/GuessSign/GuessSignHUD.h"
#include "Games/GuessSign/GuessSignGameMode.h"
#include "Components/Image.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogGuessSignHUD, Log, All);

// -- Color palette -----------------------------------------------------------

static const FLinearColor DarkPurpleBg(0.12f, 0.06f, 0.18f, 1.f);
static const FLinearColor SignAreaBg(0.18f, 0.08f, 0.28f, 1.f);
static const FLinearColor ChoiceBtnNormal(0.25f, 0.12f, 0.4f, 1.f);
static const FLinearColor ChoiceBtnHover(0.35f, 0.18f, 0.55f, 1.f);
static const FLinearColor ChoiceBtnPress(0.18f, 0.08f, 0.3f, 1.f);
static const FLinearColor CorrectGreen(0.15f, 0.75f, 0.3f, 1.f);
static const FLinearColor IncorrectRed(0.85f, 0.15f, 0.15f, 1.f);
static const FLinearColor NextBtnNormal(0.2f, 0.3f, 0.7f, 1.f);
static const FLinearColor NextBtnHover(0.3f, 0.4f, 0.85f, 1.f);
static const FLinearColor NextBtnPress(0.15f, 0.2f, 0.5f, 1.f);
static const FLinearColor TimerBarBg(0.1f, 0.05f, 0.15f, 1.f);

// ---------------------------------------------------------------------------
// Initialize - build the Guess the Sign game UI
// ---------------------------------------------------------------------------

bool UGuessSignHUD::Initialize()
{
	// Build base HUD widgets first (root canvas, top bar with score/timer/pause/quit).
	if (!Super::Initialize())
	{
		return false;
	}

	// Guard: if already built, don't rebuild.
	if (SignDisplayBorder)
	{
		return true;
	}

	// Grab the root canvas created by Super::Initialize().
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	// -- Fonts ---------------------------------------------------------------

	const FSlateFontInfo SignLetterFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 72);
	const FSlateFontInfo SignDescFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo ChoiceFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20);
	const FSlateFontInfo StatusFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo FeedbackFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 28);
	const FSlateFontInfo NextBtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);

	// ========================================================================
	// GAME AREA - dark purple background, below top bar
	// ========================================================================

	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("GS_GameArea"));
	GameArea->SetBrushColor(DarkPurpleBg);
	GameArea->SetPadding(FMargin(10.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	// Main vertical layout inside the game area.
	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("GS_ContentVBox"));
	GameArea->SetContent(ContentVBox);

	// ========================================================================
	// ROW 1: Round counter + streak (horizontal bar)
	// ========================================================================

	UHorizontalBox* StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("GS_StatusRow"));
	UVerticalBoxSlot* StatusRowSlot = ContentVBox->AddChildToVerticalBox(StatusRow);
	StatusRowSlot->SetHorizontalAlignment(HAlign_Fill);
	StatusRowSlot->SetPadding(FMargin(20.f, 8.f, 20.f, 4.f));

	// Round counter: "Question 1/20"
	ProgressText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ProgressText"));
	ProgressText->SetText(FText::FromString(TEXT("Question 1/20")));
	ProgressText->SetFont(StatusFont);
	ProgressText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	UHorizontalBoxSlot* ProgSlot = StatusRow->AddChildToHorizontalBox(ProgressText);
	ProgSlot->SetVerticalAlignment(VAlign_Center);
	ProgSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

	// Spacer to push streak to the right.
	USpacer* StatusSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("GS_StatusSpacer"));
	UHorizontalBoxSlot* StatusSpacerSlot = StatusRow->AddChildToHorizontalBox(StatusSpacer);
	StatusSpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Streak display
	StreakText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("StreakText"));
	StreakText->SetText(FText::FromString(TEXT("")));
	StreakText->SetFont(StatusFont);
	StreakText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.f)));
	StreakText->SetVisibility(ESlateVisibility::Collapsed);

	UHorizontalBoxSlot* StreakSlot = StatusRow->AddChildToHorizontalBox(StreakText);
	StreakSlot->SetVerticalAlignment(VAlign_Center);

	// ========================================================================
	// ROW 2: Timer bar (full-width progress bar)
	// ========================================================================

	TimerBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("TimerBar"));
	TimerBar->SetPercent(1.f);
	TimerBar->SetFillColorAndOpacity(FLinearColor::Green);

	// Wrap in a SizeBox to control height.
	USizeBox* TimerSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("GS_TimerSizeBox"));
	TimerSizeBox->SetMinDesiredHeight(12.f);
	TimerSizeBox->SetMaxDesiredHeight(12.f);
	TimerSizeBox->AddChild(TimerBar);

	UVerticalBoxSlot* TimerSlot = ContentVBox->AddChildToVerticalBox(TimerSizeBox);
	TimerSlot->SetHorizontalAlignment(HAlign_Fill);
	TimerSlot->SetPadding(FMargin(20.f, 0.f, 20.f, 8.f));

	// ========================================================================
	// ROW 3: Sign display area (large centered box, 250x200)
	// ========================================================================

	USizeBox* SignSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("GS_SignSizeBox"));
	SignSizeBox->SetMinDesiredWidth(250.f);
	SignSizeBox->SetMinDesiredHeight(200.f);

	SignDisplayBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SignDisplayBorder"));
	SignDisplayBorder->SetBrushColor(SignAreaBg);
	SignDisplayBorder->SetPadding(FMargin(20.f));

	SignSizeBox->AddChild(SignDisplayBorder);

	// Vertical layout inside the sign display area.
	UVerticalBox* SignVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("GS_SignVBox"));
	SignDisplayBorder->SetContent(SignVBox);

	// Spacer at top to vertically center content.
	USpacer* SignTopSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("GS_SignTopSpacer"));
	UVerticalBoxSlot* SignTopSpSlot = SignVBox->AddChildToVerticalBox(SignTopSpacer);
	SignTopSpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Large letter text (e.g. "?", updated when question is presented).
	SignLetterText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SignLetterText"));
	SignLetterText->SetText(FText::FromString(TEXT("?")));
	SignLetterText->SetFont(SignLetterFont);
	SignLetterText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	SignLetterText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* LetterSlot = SignVBox->AddChildToVerticalBox(SignLetterText);
	LetterSlot->SetHorizontalAlignment(HAlign_Center);
	LetterSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

	// Description text below the letter.
	SignDescriptionText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("SignDescriptionText"));
	SignDescriptionText->SetText(FText::FromString(TEXT("What letter does this ASL sign represent?")));
	SignDescriptionText->SetFont(SignDescFont);
	SignDescriptionText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.6f, 0.9f)));
	SignDescriptionText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* DescSlot = SignVBox->AddChildToVerticalBox(SignDescriptionText);
	DescSlot->SetHorizontalAlignment(HAlign_Center);
	DescSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 0.f));

	// Spacer at bottom to vertically center content.
	USpacer* SignBotSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("GS_SignBotSpacer"));
	UVerticalBoxSlot* SignBotSpSlot = SignVBox->AddChildToVerticalBox(SignBotSpacer);
	SignBotSpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Add the sign size box to the main content, centered.
	UVerticalBoxSlot* SignBoxSlot = ContentVBox->AddChildToVerticalBox(SignSizeBox);
	SignBoxSlot->SetHorizontalAlignment(HAlign_Center);
	SignBoxSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 16.f));

	// ========================================================================
	// ROW 4: Four answer choice buttons (2x2 grid using nested HBoxes)
	// ========================================================================

	// Helper lambda: create one choice button with border + text.
	auto MakeChoiceButton = [&](const FName& BtnName, const FName& TxtName,
		const FName& BorderName, const FString& DefaultLabel,
		UButton*& OutButton, UTextBlock*& OutText, UBorder*& OutBorder)
	{
		// Outer border for color feedback.
		OutBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), BorderName);
		OutBorder->SetBrushColor(ChoiceBtnNormal);
		OutBorder->SetPadding(FMargin(2.f));

		// Size box to give uniform button size.
		USizeBox* BtnSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), FName(*(BtnName.ToString() + TEXT("Size"))));
		BtnSizeBox->SetMinDesiredWidth(180.f);
		BtnSizeBox->SetMinDesiredHeight(50.f);

		// The button itself.
		OutButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), BtnName);
		FButtonStyle Style = OutButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(ChoiceBtnNormal);
		Style.Hovered.TintColor = FSlateColor(ChoiceBtnHover);
		Style.Pressed.TintColor = FSlateColor(ChoiceBtnPress);
		OutButton->SetStyle(Style);

		// Text label inside the button.
		OutText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TxtName);
		OutText->SetText(FText::FromString(DefaultLabel));
		OutText->SetFont(ChoiceFont);
		OutText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		OutText->SetJustification(ETextJustify::Center);
		OutButton->AddChild(OutText);

		BtnSizeBox->AddChild(OutButton);
		OutBorder->SetContent(BtnSizeBox);
	};

	// Create the 4 choice buttons.
	UButton* TmpBtn1 = nullptr; UTextBlock* TmpTxt1 = nullptr; UBorder* TmpBdr1 = nullptr;
	UButton* TmpBtn2 = nullptr; UTextBlock* TmpTxt2 = nullptr; UBorder* TmpBdr2 = nullptr;
	UButton* TmpBtn3 = nullptr; UTextBlock* TmpTxt3 = nullptr; UBorder* TmpBdr3 = nullptr;
	UButton* TmpBtn4 = nullptr; UTextBlock* TmpTxt4 = nullptr; UBorder* TmpBdr4 = nullptr;

	MakeChoiceButton(TEXT("Choice1Button"), TEXT("Choice1Text"), TEXT("Choice1Border"),
		TEXT("A"), TmpBtn1, TmpTxt1, TmpBdr1);
	MakeChoiceButton(TEXT("Choice2Button"), TEXT("Choice2Text"), TEXT("Choice2Border"),
		TEXT("B"), TmpBtn2, TmpTxt2, TmpBdr2);
	MakeChoiceButton(TEXT("Choice3Button"), TEXT("Choice3Text"), TEXT("Choice3Border"),
		TEXT("C"), TmpBtn3, TmpTxt3, TmpBdr3);
	MakeChoiceButton(TEXT("Choice4Button"), TEXT("Choice4Text"), TEXT("Choice4Border"),
		TEXT("D"), TmpBtn4, TmpTxt4, TmpBdr4);

	Choice1Button = TmpBtn1; Choice1Text = TmpTxt1; Choice1Border = TmpBdr1;
	Choice2Button = TmpBtn2; Choice2Text = TmpTxt2; Choice2Border = TmpBdr2;
	Choice3Button = TmpBtn3; Choice3Text = TmpTxt3; Choice3Border = TmpBdr3;
	Choice4Button = TmpBtn4; Choice4Text = TmpTxt4; Choice4Border = TmpBdr4;

	// Top row of choices (1 and 2).
	UHorizontalBox* ChoiceRow1 = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("GS_ChoiceRow1"));

	UHorizontalBoxSlot* C1Slot = ChoiceRow1->AddChildToHorizontalBox(Choice1Border);
	C1Slot->SetHorizontalAlignment(HAlign_Center);
	C1Slot->SetPadding(FMargin(8.f, 4.f));
	C1Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UHorizontalBoxSlot* C2Slot = ChoiceRow1->AddChildToHorizontalBox(Choice2Border);
	C2Slot->SetHorizontalAlignment(HAlign_Center);
	C2Slot->SetPadding(FMargin(8.f, 4.f));
	C2Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBoxSlot* Row1Slot = ContentVBox->AddChildToVerticalBox(ChoiceRow1);
	Row1Slot->SetHorizontalAlignment(HAlign_Center);
	Row1Slot->SetPadding(FMargin(20.f, 0.f, 20.f, 0.f));

	// Bottom row of choices (3 and 4).
	UHorizontalBox* ChoiceRow2 = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("GS_ChoiceRow2"));

	UHorizontalBoxSlot* C3Slot = ChoiceRow2->AddChildToHorizontalBox(Choice3Border);
	C3Slot->SetHorizontalAlignment(HAlign_Center);
	C3Slot->SetPadding(FMargin(8.f, 4.f));
	C3Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UHorizontalBoxSlot* C4Slot = ChoiceRow2->AddChildToHorizontalBox(Choice4Border);
	C4Slot->SetHorizontalAlignment(HAlign_Center);
	C4Slot->SetPadding(FMargin(8.f, 4.f));
	C4Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UVerticalBoxSlot* Row2Slot = ContentVBox->AddChildToVerticalBox(ChoiceRow2);
	Row2Slot->SetHorizontalAlignment(HAlign_Center);
	Row2Slot->SetPadding(FMargin(20.f, 0.f, 20.f, 8.f));

	// ========================================================================
	// ROW 5: Feedback text ("Correct!" / "Wrong!") - centered, hidden by default
	// ========================================================================

	FeedbackText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FeedbackText"));
	FeedbackText->SetText(FText::FromString(TEXT("")));
	FeedbackText->SetFont(FeedbackFont);
	FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FeedbackText->SetJustification(ETextJustify::Center);
	FeedbackText->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBoxSlot* FeedbackSlot = ContentVBox->AddChildToVerticalBox(FeedbackText);
	FeedbackSlot->SetHorizontalAlignment(HAlign_Center);
	FeedbackSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));

	// ========================================================================
	// ROW 6: Next button - centered, hidden by default
	// ========================================================================

	NextButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("NextButton"));
	{
		FButtonStyle Style = NextButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(NextBtnNormal);
		Style.Hovered.TintColor = FSlateColor(NextBtnHover);
		Style.Pressed.TintColor = FSlateColor(NextBtnPress);
		NextButton->SetStyle(Style);
	}

	NextButtonText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("NextButtonText"));
	NextButtonText->SetText(FText::FromString(TEXT("Next Question")));
	NextButtonText->SetFont(NextBtnFont);
	NextButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	NextButtonText->SetJustification(ETextJustify::Center);
	NextButton->AddChild(NextButtonText);

	USizeBox* NextSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("GS_NextSizeBox"));
	NextSizeBox->SetMinDesiredWidth(200.f);
	NextSizeBox->SetMinDesiredHeight(44.f);
	NextSizeBox->AddChild(NextButton);

	NextButton->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBoxSlot* NextSlot = ContentVBox->AddChildToVerticalBox(NextSizeBox);
	NextSlot->SetHorizontalAlignment(HAlign_Center);
	NextSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 8.f));

	// ========================================================================
	// Bottom spacer to push content upward
	// ========================================================================

	USpacer* BottomSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("GS_BottomSpacer"));
	UVerticalBoxSlot* BotSpSlot = ContentVBox->AddChildToVerticalBox(BottomSpacer);
	BotSpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UE_LOG(LogGuessSignHUD, Log,
		TEXT("GuessSign HUD built: sign display, 4 choice buttons, timer bar, feedback text."));

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct - wire up delegates and bind to game mode
// ---------------------------------------------------------------------------

void UGuessSignHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind choice button callbacks.
	if (Choice1Button) Choice1Button->OnClicked.AddDynamic(this, &UGuessSignHUD::OnChoice1Clicked);
	if (Choice2Button) Choice2Button->OnClicked.AddDynamic(this, &UGuessSignHUD::OnChoice2Clicked);
	if (Choice3Button) Choice3Button->OnClicked.AddDynamic(this, &UGuessSignHUD::OnChoice3Clicked);
	if (Choice4Button) Choice4Button->OnClicked.AddDynamic(this, &UGuessSignHUD::OnChoice4Clicked);
	if (NextButton)    NextButton->OnClicked.AddDynamic(this, &UGuessSignHUD::OnNextClicked);

	// Hide feedback initially.
	if (FeedbackText)
	{
		FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (NextButton)
	{
		NextButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Attempt to bind to game mode if already set via GameModeRef (from base class).
	if (!GuessSignModeRef)
	{
		AGuessSignGameMode* GM = Cast<AGuessSignGameMode>(GameModeRef);
		if (GM)
		{
			SetGuessSignGameMode(GM);
		}
	}
}

void UGuessSignHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update the per-question timer bar.
	if (TimerBar && GuessSignModeRef)
	{
		float TimeRemaining = GuessSignModeRef->GetQuestionTimeRemaining();
		float TimePer = GuessSignModeRef->GetTimePerQuestion();
		float Fraction = (TimePer > 0.f) ? (TimeRemaining / TimePer) : 0.f;
		TimerBar->SetPercent(FMath::Clamp(Fraction, 0.f, 1.f));

		// Color the bar red when time is low.
		if (Fraction < 0.25f)
		{
			TimerBar->SetFillColorAndOpacity(FLinearColor::Red);
		}
		else if (Fraction < 0.5f)
		{
			TimerBar->SetFillColorAndOpacity(FLinearColor::Yellow);
		}
		else
		{
			TimerBar->SetFillColorAndOpacity(FLinearColor::Green);
		}
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UGuessSignHUD::SetGuessSignGameMode(AGuessSignGameMode* InGameMode)
{
	GuessSignModeRef = InGameMode;
	SetGameMode(InGameMode);

	if (GuessSignModeRef)
	{
		GuessSignModeRef->OnQuestionPresented.AddDynamic(this, &UGuessSignHUD::HandleQuestionPresented);
		GuessSignModeRef->OnAnswerSubmitted.AddDynamic(this, &UGuessSignHUD::HandleAnswerSubmitted);
		GuessSignModeRef->OnSignTimeUp.AddDynamic(this, &UGuessSignHUD::HandleTimeUp);
	}

	RefreshStatusDisplay();
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void UGuessSignHUD::HandleQuestionPresented(const FString& SignImagePath, const TArray<FString>& Choices)
{
	bShowingFeedback = false;

	// Update the sign display area with a placeholder letter.
	// In production, SignImage would be loaded from SignImagePath.
	if (SignImage)
	{
		SignImage->SetColorAndOpacity(FLinearColor::White);
	}

	// Show a "?" in the sign display until the player answers.
	if (SignLetterText)
	{
		SignLetterText->SetText(FText::FromString(TEXT("?")));
		SignLetterText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}

	if (SignDescriptionText)
	{
		SignDescriptionText->SetText(
			FText::FromString(TEXT("What letter does this ASL sign represent?")));
	}

	// Update choice button labels.
	UTextBlock* ChoiceTexts[] = { Choice1Text, Choice2Text, Choice3Text, Choice4Text };
	for (int32 i = 0; i < 4; ++i)
	{
		if (ChoiceTexts[i] && Choices.IsValidIndex(i))
		{
			ChoiceTexts[i]->SetText(FText::FromString(Choices[i]));
		}
	}

	// Reset button border colors and enable buttons.
	ResetChoiceBorderColors();
	SetChoiceButtonsEnabled(true);

	if (FeedbackText)
	{
		FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (NextButton)
	{
		NextButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	RefreshStatusDisplay();
}

void UGuessSignHUD::HandleAnswerSubmitted(bool bCorrect, const FString& CorrectAnswer)
{
	bShowingFeedback = true;
	SetChoiceButtonsEnabled(false);

	if (FeedbackText)
	{
		FeedbackText->SetVisibility(ESlateVisibility::Visible);

		if (bCorrect)
		{
			FeedbackText->SetText(FText::FromString(TEXT("Correct!")));
			FeedbackText->SetColorAndOpacity(FSlateColor(CorrectGreen));
		}
		else
		{
			FString Msg = FString::Printf(TEXT("Wrong! It was: %s"), *CorrectAnswer);
			FeedbackText->SetText(FText::FromString(Msg));
			FeedbackText->SetColorAndOpacity(FSlateColor(IncorrectRed));
		}
	}

	// Update sign display to reveal the correct answer.
	if (SignLetterText)
	{
		SignLetterText->SetText(FText::FromString(CorrectAnswer));
		SignLetterText->SetColorAndOpacity(
			FSlateColor(bCorrect ? CorrectGreen : IncorrectRed));
	}

	if (SignDescriptionText)
	{
		SignDescriptionText->SetText(FText::FromString(
			FString::Printf(TEXT("The sign was: %s"), *CorrectAnswer)));
	}

	// Highlight the correct choice border in green.
	if (GuessSignModeRef)
	{
		UBorder* ChoiceBorders[] = { Choice1Border, Choice2Border, Choice3Border, Choice4Border };
		int32 CorrectIdx = GuessSignModeRef->GetCurrentChoices().IndexOfByKey(CorrectAnswer);
		for (int32 i = 0; i < 4; ++i)
		{
			if (ChoiceBorders[i])
			{
				ChoiceBorders[i]->SetBrushColor(
					(i == CorrectIdx) ? CorrectGreen : IncorrectRed);
			}
		}
	}

	if (NextButton)
	{
		NextButton->SetVisibility(ESlateVisibility::Visible);
	}

	RefreshStatusDisplay();
}

void UGuessSignHUD::HandleTimeUp()
{
	bShowingFeedback = true;
	SetChoiceButtonsEnabled(false);

	if (FeedbackText)
	{
		FeedbackText->SetVisibility(ESlateVisibility::Visible);
		FeedbackText->SetText(FText::FromString(TEXT("Time's Up!")));
		FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.5f, 0.f, 1.f)));
	}

	if (SignLetterText)
	{
		SignLetterText->SetColorAndOpacity(
			FSlateColor(FLinearColor(1.f, 0.5f, 0.f, 1.f)));
	}

	if (NextButton)
	{
		NextButton->SetVisibility(ESlateVisibility::Visible);
	}
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UGuessSignHUD::OnChoice1Clicked() { SubmitChoice(0); }
void UGuessSignHUD::OnChoice2Clicked() { SubmitChoice(1); }
void UGuessSignHUD::OnChoice3Clicked() { SubmitChoice(2); }
void UGuessSignHUD::OnChoice4Clicked() { SubmitChoice(3); }

void UGuessSignHUD::OnNextClicked()
{
	if (GuessSignModeRef)
	{
		GuessSignModeRef->NextQuestion();
	}
}

void UGuessSignHUD::SubmitChoice(int32 Index)
{
	if (bShowingFeedback || !GuessSignModeRef)
	{
		return;
	}

	// Highlight the selected choice border before submitting.
	UBorder* ChoiceBorders[] = { Choice1Border, Choice2Border, Choice3Border, Choice4Border };
	if (ChoiceBorders[Index])
	{
		ChoiceBorders[Index]->SetBrushColor(FLinearColor(0.4f, 0.3f, 0.7f, 1.f));
	}

	GuessSignModeRef->SubmitAnswer(Index);
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void UGuessSignHUD::SetChoiceButtonsEnabled(bool bEnabled)
{
	if (Choice1Button) Choice1Button->SetIsEnabled(bEnabled);
	if (Choice2Button) Choice2Button->SetIsEnabled(bEnabled);
	if (Choice3Button) Choice3Button->SetIsEnabled(bEnabled);
	if (Choice4Button) Choice4Button->SetIsEnabled(bEnabled);
}

void UGuessSignHUD::ResetChoiceBorderColors()
{
	if (Choice1Border) Choice1Border->SetBrushColor(ChoiceBtnNormal);
	if (Choice2Border) Choice2Border->SetBrushColor(ChoiceBtnNormal);
	if (Choice3Border) Choice3Border->SetBrushColor(ChoiceBtnNormal);
	if (Choice4Border) Choice4Border->SetBrushColor(ChoiceBtnNormal);
}

void UGuessSignHUD::RefreshStatusDisplay()
{
	if (!GuessSignModeRef)
	{
		return;
	}

	if (ProgressText)
	{
		ProgressText->SetText(FText::FromString(
			FString::Printf(TEXT("Question %d/%d"),
				GuessSignModeRef->GetTotalQuestions() + 1,
				GuessSignModeRef->GetMaxQuestions())));
	}

	if (StreakText)
	{
		int32 Streak = GuessSignModeRef->GetCurrentStreak();
		if (Streak > 0)
		{
			StreakText->SetVisibility(ESlateVisibility::Visible);
			StreakText->SetText(FText::FromString(
				FString::Printf(TEXT("Streak: %d"), Streak)));
		}
		else
		{
			StreakText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}
}
