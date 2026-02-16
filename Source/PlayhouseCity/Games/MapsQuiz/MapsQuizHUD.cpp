// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/MapsQuiz/MapsQuizHUD.h"
#include "Games/MapsQuiz/MapsGameMode.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogMapsQuizHUD, Log, All);

// Globe / geography theme colours.
namespace {
const FLinearColor MapsBgColor(0.04f, 0.06f, 0.18f, 1.f);        // Dark navy blue
const FLinearColor DisplayAreaColor(0.06f, 0.09f, 0.24f, 1.f);    // Slightly lighter navy
const FLinearColor ChoiceBtnNormal(0.12f, 0.22f, 0.52f, 1.f);     // Globe blue
const FLinearColor ChoiceBtnHover(0.18f, 0.32f, 0.68f, 1.f);
const FLinearColor ChoiceBtnPress(0.08f, 0.14f, 0.38f, 1.f);
const FLinearColor NextBtnNormal(0.15f, 0.55f, 0.3f, 1.f);       // Green
const FLinearColor NextBtnHover(0.2f, 0.7f, 0.4f, 1.f);
const FLinearColor NextBtnPress(0.1f, 0.4f, 0.2f, 1.f);
const FLinearColor TimerBarBgColor(0.08f, 0.08f, 0.16f, 1.f);
const FLinearColor FeedbackOverlayBg(0.02f, 0.03f, 0.1f, 0.88f);
}

// ---------------------------------------------------------------------------
// Initialize - build the Maps Quiz UI programmatically
// ---------------------------------------------------------------------------

bool UMapsQuizHUD::Initialize()
{
	// Build base HUD widgets first (root canvas, top bar with score/timer/pause/quit,
	// pause overlay, game-over overlay).
	if (!Super::Initialize())
	{
		return false;
	}

	// Guard against double-init (e.g. if a Blueprint already provided the widgets).
	if (QuestionText)
	{
		return true;
	}

	// We need the root canvas created by the base class.
	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	// -- Fonts ---------------------------------------------------------------
	const FSlateFontInfo QuestionFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);
	const FSlateFontInfo ClueFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 36);
	const FSlateFontInfo ChoiceFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo StatusFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo FeedbackFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 28);
	const FSlateFontInfo NextBtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);

	// ========================================================================
	// GAME AREA - dark blue background below the top bar
	// ========================================================================
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("MapsGameArea"));
	GameArea->SetBrushColor(MapsBgColor);
	GameArea->SetPadding(FMargin(16.f, 12.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	// Main vertical layout inside the game area.
	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("MapsContentVBox"));
	GameArea->SetContent(ContentVBox);

	// ========================================================================
	// ROW 1: Status bar (Progress + Accuracy)
	// ========================================================================
	UHorizontalBox* StatusRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("StatusRow"));
	UVerticalBoxSlot* StatusRowSlot = ContentVBox->AddChildToVerticalBox(StatusRow);
	StatusRowSlot->SetHorizontalAlignment(HAlign_Fill);
	StatusRowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// Progress text (left side) - "3/20"
	ProgressText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ProgressText"));
	ProgressText->SetText(FText::FromString(TEXT("1/20")));
	ProgressText->SetFont(StatusFont);
	ProgressText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 1.f)));
	UHorizontalBoxSlot* ProgSlot = StatusRow->AddChildToHorizontalBox(ProgressText);
	ProgSlot->SetVerticalAlignment(VAlign_Center);
	ProgSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	// Spacer
	USpacer* StatusSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("StatusSpacer"));
	UHorizontalBoxSlot* StatusSpSlot = StatusRow->AddChildToHorizontalBox(StatusSpacer);
	StatusSpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Accuracy text (right side) - "Accuracy: 75%"
	AccuracyText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AccuracyText"));
	AccuracyText->SetText(FText::FromString(TEXT("Accuracy: 0%")));
	AccuracyText->SetFont(StatusFont);
	AccuracyText->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 1.f)));
	UHorizontalBoxSlot* AccSlot = StatusRow->AddChildToHorizontalBox(AccuracyText);
	AccSlot->SetVerticalAlignment(VAlign_Center);

	// ========================================================================
	// ROW 2: Question text - "Which country is this?"
	// ========================================================================
	QuestionText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("QuestionText"));
	QuestionText->SetText(FText::FromString(TEXT("Which country is this?")));
	QuestionText->SetFont(QuestionFont);
	QuestionText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	QuestionText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* QuestionSlot = ContentVBox->AddChildToVerticalBox(QuestionText);
	QuestionSlot->SetHorizontalAlignment(HAlign_Center);
	QuestionSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 12.f));

	// ========================================================================
	// ROW 3: Country display area (200x200 centered dark rectangle)
	// This holds the MapImage, FlagImage, or CountryClueText depending on
	// the question type.
	// ========================================================================
	USizeBox* DisplaySizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("DisplaySizeBox"));
	DisplaySizeBox->SetMinDesiredWidth(200.f);
	DisplaySizeBox->SetMinDesiredHeight(200.f);
	DisplaySizeBox->SetMaxDesiredWidth(200.f);
	DisplaySizeBox->SetMaxDesiredHeight(200.f);

	CountryDisplayBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("CountryDisplayBorder"));
	CountryDisplayBorder->SetBrushColor(DisplayAreaColor);
	CountryDisplayBorder->SetPadding(FMargin(8.f));

	// Use a canvas panel inside the display border so we can layer images and text.
	UCanvasPanel* DisplayCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("DisplayCanvas"));
	CountryDisplayBorder->SetContent(DisplayCanvas);

	// Map outline image (fills the display area).
	MapImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("MapImage"));
	MapImage->SetColorAndOpacity(FLinearColor::White);
	MapImage->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* MapImgSlot = DisplayCanvas->AddChildToCanvas(MapImage);
	MapImgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	MapImgSlot->SetOffsets(FMargin(0.f));

	// Flag image (fills the display area).
	FlagImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("FlagImage"));
	FlagImage->SetColorAndOpacity(FLinearColor::White);
	FlagImage->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* FlagImgSlot = DisplayCanvas->AddChildToCanvas(FlagImage);
	FlagImgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	FlagImgSlot->SetOffsets(FMargin(0.f));

	// Country clue text (centered in the display area, used for capital questions).
	CountryClueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CountryClueText"));
	CountryClueText->SetText(FText::FromString(TEXT("?")));
	CountryClueText->SetFont(ClueFont);
	CountryClueText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	CountryClueText->SetJustification(ETextJustify::Center);
	CountryClueText->SetVisibility(ESlateVisibility::Collapsed);
	UCanvasPanelSlot* ClueTextSlot = DisplayCanvas->AddChildToCanvas(CountryClueText);
	ClueTextSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
	ClueTextSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	ClueTextSlot->SetAutoSize(true);

	DisplaySizeBox->AddChild(CountryDisplayBorder);

	UVerticalBoxSlot* DisplaySlot = ContentVBox->AddChildToVerticalBox(DisplaySizeBox);
	DisplaySlot->SetHorizontalAlignment(HAlign_Center);
	DisplaySlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	// ========================================================================
	// ROW 4: Timer bar
	// ========================================================================
	TimerBarBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("TimerBarBorder"));
	TimerBarBorder->SetBrushColor(TimerBarBgColor);
	TimerBarBorder->SetPadding(FMargin(2.f));

	USizeBox* TimerSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("TimerSizeBox"));
	TimerSizeBox->SetMinDesiredHeight(12.f);
	TimerSizeBox->SetMaxDesiredHeight(12.f);

	TimerBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("TimerBar"));
	TimerBar->SetPercent(1.f);
	TimerBar->SetFillColorAndOpacity(FLinearColor::Green);

	TimerSizeBox->AddChild(TimerBar);
	TimerBarBorder->SetContent(TimerSizeBox);

	UVerticalBoxSlot* TimerSlot = ContentVBox->AddChildToVerticalBox(TimerBarBorder);
	TimerSlot->SetHorizontalAlignment(HAlign_Fill);
	TimerSlot->SetPadding(FMargin(20.f, 0.f, 20.f, 16.f));

	// ========================================================================
	// ROW 5: Answer choice buttons (2x2 grid)
	// Layout:  [Choice1] [Choice2]
	//          [Choice3] [Choice4]
	// ========================================================================

	// Helper lambda: create a styled choice button with a text label child.
	auto MakeChoiceButton = [&](const FName& BtnName, const FName& LblName,
								UButton*& OutButton, UTextBlock*& OutText)
	{
		OutButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), BtnName);
		FButtonStyle Style = OutButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(ChoiceBtnNormal);
		Style.Hovered.TintColor = FSlateColor(ChoiceBtnHover);
		Style.Pressed.TintColor = FSlateColor(ChoiceBtnPress);
		OutButton->SetStyle(Style);

		OutText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), LblName);
		OutText->SetText(FText::FromString(TEXT("---")));
		OutText->SetFont(ChoiceFont);
		OutText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		OutText->SetJustification(ETextJustify::Center);
		OutButton->AddChild(OutText);
	};

	// Top row of choices: [Choice1] [Choice2]
	UHorizontalBox* ChoiceRow1 = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ChoiceRow1"));
	UVerticalBoxSlot* CR1Slot = ContentVBox->AddChildToVerticalBox(ChoiceRow1);
	CR1Slot->SetHorizontalAlignment(HAlign_Center);
	CR1Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));

	// Choice 1
	{
		USizeBox* Sz = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("Choice1Size"));
		Sz->SetMinDesiredWidth(220.f);
		Sz->SetMinDesiredHeight(44.f);

		UButton* Btn = nullptr;
		UTextBlock* Lbl = nullptr;
		MakeChoiceButton(TEXT("Choice1Button"), TEXT("Choice1Text"), Btn, Lbl);
		Choice1Button = Btn;
		Choice1Text = Lbl;

		Sz->AddChild(Choice1Button);
		UHorizontalBoxSlot* BtnSlot = ChoiceRow1->AddChildToHorizontalBox(Sz);
		BtnSlot->SetPadding(FMargin(4.f, 0.f));
		BtnSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Choice 2
	{
		USizeBox* Sz = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("Choice2Size"));
		Sz->SetMinDesiredWidth(220.f);
		Sz->SetMinDesiredHeight(44.f);

		UButton* Btn = nullptr;
		UTextBlock* Lbl = nullptr;
		MakeChoiceButton(TEXT("Choice2Button"), TEXT("Choice2Text"), Btn, Lbl);
		Choice2Button = Btn;
		Choice2Text = Lbl;

		Sz->AddChild(Choice2Button);
		UHorizontalBoxSlot* BtnSlot = ChoiceRow1->AddChildToHorizontalBox(Sz);
		BtnSlot->SetPadding(FMargin(4.f, 0.f));
		BtnSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Bottom row of choices: [Choice3] [Choice4]
	UHorizontalBox* ChoiceRow2 = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ChoiceRow2"));
	UVerticalBoxSlot* CR2Slot = ContentVBox->AddChildToVerticalBox(ChoiceRow2);
	CR2Slot->SetHorizontalAlignment(HAlign_Center);
	CR2Slot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

	// Choice 3
	{
		USizeBox* Sz = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("Choice3Size"));
		Sz->SetMinDesiredWidth(220.f);
		Sz->SetMinDesiredHeight(44.f);

		UButton* Btn = nullptr;
		UTextBlock* Lbl = nullptr;
		MakeChoiceButton(TEXT("Choice3Button"), TEXT("Choice3Text"), Btn, Lbl);
		Choice3Button = Btn;
		Choice3Text = Lbl;

		Sz->AddChild(Choice3Button);
		UHorizontalBoxSlot* BtnSlot = ChoiceRow2->AddChildToHorizontalBox(Sz);
		BtnSlot->SetPadding(FMargin(4.f, 0.f));
		BtnSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// Choice 4
	{
		USizeBox* Sz = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("Choice4Size"));
		Sz->SetMinDesiredWidth(220.f);
		Sz->SetMinDesiredHeight(44.f);

		UButton* Btn = nullptr;
		UTextBlock* Lbl = nullptr;
		MakeChoiceButton(TEXT("Choice4Button"), TEXT("Choice4Text"), Btn, Lbl);
		Choice4Button = Btn;
		Choice4Text = Lbl;

		Sz->AddChild(Choice4Button);
		UHorizontalBoxSlot* BtnSlot = ChoiceRow2->AddChildToHorizontalBox(Sz);
		BtnSlot->SetPadding(FMargin(4.f, 0.f));
		BtnSlot->SetHorizontalAlignment(HAlign_Center);
	}

	// ========================================================================
	// ROW 6: Feedback text + Next button
	// ========================================================================

	// Feedback text (Correct! / Wrong! Answer: X / Time's Up!)
	FeedbackText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FeedbackText"));
	FeedbackText->SetText(FText::FromString(TEXT("")));
	FeedbackText->SetFont(FeedbackFont);
	FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FeedbackText->SetJustification(ETextJustify::Center);
	FeedbackText->SetVisibility(ESlateVisibility::Collapsed);

	UVerticalBoxSlot* FBSlot = ContentVBox->AddChildToVerticalBox(FeedbackText);
	FBSlot->SetHorizontalAlignment(HAlign_Center);
	FBSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	// Next button
	{
		USizeBox* NextSz = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("NextBtnSize"));
		NextSz->SetMinDesiredWidth(160.f);
		NextSz->SetMinDesiredHeight(40.f);

		NextButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("NextButton"));
		FButtonStyle Style = NextButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(NextBtnNormal);
		Style.Hovered.TintColor = FSlateColor(NextBtnHover);
		Style.Pressed.TintColor = FSlateColor(NextBtnPress);
		NextButton->SetStyle(Style);
		NextButton->SetVisibility(ESlateVisibility::Collapsed);

		NextButtonText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("NextButtonText"));
		NextButtonText->SetText(FText::FromString(TEXT("Next")));
		NextButtonText->SetFont(NextBtnFont);
		NextButtonText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		NextButtonText->SetJustification(ETextJustify::Center);
		NextButton->AddChild(NextButtonText);

		NextSz->AddChild(NextButton);

		UVerticalBoxSlot* NextSlot = ContentVBox->AddChildToVerticalBox(NextSz);
		NextSlot->SetHorizontalAlignment(HAlign_Center);
		NextSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	// ========================================================================
	// FEEDBACK OVERLAY (full-screen semi-transparent, hidden by default)
	// Shown when the player answers or time runs out to darken the background
	// behind the feedback text. This is placed on the root canvas so it covers
	// the entire game area.
	// ========================================================================
	FeedbackOverlay = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("FeedbackOverlay"));
	FeedbackOverlay->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* FOvSlot = RootCanvas->AddChildToCanvas(FeedbackOverlay);
	FOvSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	FOvSlot->SetOffsets(FMargin(0.f));

	// Remaining bottom spacer to push content up.
	USpacer* BottomSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("MapsBottomSpacer"));
	UVerticalBoxSlot* BSSlot = ContentVBox->AddChildToVerticalBox(BottomSpacer);
	BSSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UE_LOG(LogMapsQuizHUD, Log,
		TEXT("MapsQuizHUD widget tree built: question text, 200x200 display, "
			 "4 choice buttons (2x2), timer bar, feedback, next button."));

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct - wire up delegates and button callbacks
// ---------------------------------------------------------------------------

void UMapsQuizHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Bind choice button callbacks.
	if (Choice1Button) Choice1Button->OnClicked.AddDynamic(this, &UMapsQuizHUD::OnChoice1Clicked);
	if (Choice2Button) Choice2Button->OnClicked.AddDynamic(this, &UMapsQuizHUD::OnChoice2Clicked);
	if (Choice3Button) Choice3Button->OnClicked.AddDynamic(this, &UMapsQuizHUD::OnChoice3Clicked);
	if (Choice4Button) Choice4Button->OnClicked.AddDynamic(this, &UMapsQuizHUD::OnChoice4Clicked);
	if (NextButton)    NextButton->OnClicked.AddDynamic(this, &UMapsQuizHUD::OnNextClicked);

	// Hide feedback initially.
	if (FeedbackOverlay)
	{
		FeedbackOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (FeedbackText)
	{
		FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (NextButton)
	{
		NextButton->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Attempt to locate the Maps game mode from the base class reference
	// (set by the game mode when the HUD is created).
	if (!MapsGameModeRef && GameModeRef)
	{
		AMapsGameMode* Mode = Cast<AMapsGameMode>(GameModeRef);
		if (Mode)
		{
			SetMapsGameMode(Mode);
		}
	}
}

// ---------------------------------------------------------------------------

void UMapsQuizHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update the per-question timer bar.
	if (TimerBar && MapsGameModeRef)
	{
		float TimeRemaining = MapsGameModeRef->GetQuestionTimeRemaining();
		float TimePer = 15.f; // TimePerQuestion accessed via remaining/total.
		float Fraction = (TimePer > 0.f) ? (TimeRemaining / TimePer) : 0.f;
		TimerBar->SetPercent(FMath::Clamp(Fraction, 0.f, 1.f));

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

void UMapsQuizHUD::SetMapsGameMode(AMapsGameMode* InGameMode)
{
	MapsGameModeRef = InGameMode;
	SetGameMode(InGameMode);

	if (MapsGameModeRef)
	{
		MapsGameModeRef->OnQuestionPresented.AddDynamic(this, &UMapsQuizHUD::HandleQuestionPresented);
		MapsGameModeRef->OnAnswerSubmitted.AddDynamic(this, &UMapsQuizHUD::HandleAnswerSubmitted);
		MapsGameModeRef->OnMapsTimeUp.AddDynamic(this, &UMapsQuizHUD::HandleTimeUp);
	}

	RefreshStatusDisplay();
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void UMapsQuizHUD::HandleQuestionPresented(
	const FString& InQuestionText,
	const FString& ImagePath,
	EPHCMapsQuestionType QuestionType,
	const TArray<FString>& Choices)
{
	bShowingFeedback = false;

	// Set question text.
	if (QuestionText)
	{
		QuestionText->SetText(FText::FromString(InQuestionText));
	}

	// Configure image display based on question type.
	if (MapImage)
	{
		if (QuestionType == EPHCMapsQuestionType::IdentifyCountry)
		{
			MapImage->SetVisibility(ESlateVisibility::Visible);
			MapImage->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			MapImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	if (FlagImage)
	{
		if (QuestionType == EPHCMapsQuestionType::CountryFromFlag)
		{
			FlagImage->SetVisibility(ESlateVisibility::Visible);
			FlagImage->SetColorAndOpacity(FLinearColor::White);
		}
		else
		{
			FlagImage->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Show the country clue text for capital-from-country questions
	// (the country name itself is already in the question text, but we
	// can display a "?" or the country name in the display area).
	if (CountryClueText)
	{
		if (QuestionType == EPHCMapsQuestionType::CapitalFromCountry)
		{
			CountryClueText->SetVisibility(ESlateVisibility::Visible);
			CountryClueText->SetText(FText::FromString(TEXT("?")));
		}
		else
		{
			CountryClueText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Update choice labels.
	UTextBlock* ChoiceTexts[] = { Choice1Text, Choice2Text, Choice3Text, Choice4Text };
	for (int32 i = 0; i < 4; ++i)
	{
		if (ChoiceTexts[i] && Choices.IsValidIndex(i))
		{
			ChoiceTexts[i]->SetText(FText::FromString(Choices[i]));
		}
	}

	// Enable choice buttons.
	SetChoiceButtonsEnabled(true);

	// Hide feedback overlay and text.
	if (FeedbackOverlay)
	{
		FeedbackOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
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

void UMapsQuizHUD::HandleAnswerSubmitted(bool bCorrect, const FString& CorrectAnswer)
{
	bShowingFeedback = true;
	SetChoiceButtonsEnabled(false);

	if (FeedbackOverlay)
	{
		FeedbackOverlay->SetVisibility(ESlateVisibility::Visible);
	}

	if (FeedbackText)
	{
		FeedbackText->SetVisibility(ESlateVisibility::Visible);

		if (bCorrect)
		{
			FeedbackText->SetText(FText::FromString(TEXT("Correct!")));
			FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.9f, 0.3f)));
		}
		else
		{
			FString Msg = FString::Printf(TEXT("Wrong! Answer: %s"), *CorrectAnswer);
			FeedbackText->SetText(FText::FromString(Msg));
			FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.2f, 0.2f)));
		}
	}

	if (NextButton)
	{
		NextButton->SetVisibility(ESlateVisibility::Visible);
	}

	RefreshStatusDisplay();
}

void UMapsQuizHUD::HandleTimeUp()
{
	bShowingFeedback = true;
	SetChoiceButtonsEnabled(false);

	if (FeedbackOverlay)
	{
		FeedbackOverlay->SetVisibility(ESlateVisibility::Visible);
	}

	if (FeedbackText)
	{
		FeedbackText->SetVisibility(ESlateVisibility::Visible);
		FeedbackText->SetText(FText::FromString(TEXT("Time's Up!")));
		FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.5f, 0.f, 1.f)));
	}

	if (NextButton)
	{
		NextButton->SetVisibility(ESlateVisibility::Visible);
	}
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UMapsQuizHUD::OnChoice1Clicked() { SubmitChoice(0); }
void UMapsQuizHUD::OnChoice2Clicked() { SubmitChoice(1); }
void UMapsQuizHUD::OnChoice3Clicked() { SubmitChoice(2); }
void UMapsQuizHUD::OnChoice4Clicked() { SubmitChoice(3); }

void UMapsQuizHUD::OnNextClicked()
{
	if (MapsGameModeRef)
	{
		MapsGameModeRef->NextQuestion();
	}
}

void UMapsQuizHUD::SubmitChoice(int32 Index)
{
	if (bShowingFeedback || !MapsGameModeRef)
	{
		return;
	}

	MapsGameModeRef->SubmitAnswer(Index);
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void UMapsQuizHUD::SetChoiceButtonsEnabled(bool bEnabled)
{
	if (Choice1Button) Choice1Button->SetIsEnabled(bEnabled);
	if (Choice2Button) Choice2Button->SetIsEnabled(bEnabled);
	if (Choice3Button) Choice3Button->SetIsEnabled(bEnabled);
	if (Choice4Button) Choice4Button->SetIsEnabled(bEnabled);
}

void UMapsQuizHUD::RefreshStatusDisplay()
{
	if (!MapsGameModeRef)
	{
		return;
	}

	if (ProgressText)
	{
		ProgressText->SetText(FText::FromString(
			FString::Printf(TEXT("%d/%d"),
				MapsGameModeRef->GetCurrentQuestionNumber(),
				MapsGameModeRef->GetMaxQuestions())));
	}

	if (AccuracyText)
	{
		float Acc = MapsGameModeRef->GetAccuracy() * 100.f;
		AccuracyText->SetText(FText::FromString(
			FString::Printf(TEXT("Accuracy: %.0f%%"), Acc)));
	}
}
