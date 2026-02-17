// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Bingo/BingoHUD.h"
#include "Games/Bingo/BingoGameMode.h"
#include "Games/Bingo/BingoCard.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogBingoHUD, Log, All);

// -- Color palette -----------------------------------------------------------
static const FLinearColor BingoBgColor(0.04f, 0.04f, 0.1f, 1.f);
static const FLinearColor CardBgColor(0.08f, 0.08f, 0.18f, 1.f);
static const FLinearColor HeaderBgColor(0.15f, 0.2f, 0.5f, 1.f);
static const FLinearColor CellDefaultColor(0.12f, 0.12f, 0.22f, 1.f);
static const FLinearColor BallDisplayColor(0.9f, 0.75f, 0.0f, 1.f);
static const FLinearColor CalledBoardBgColor(0.06f, 0.06f, 0.14f, 1.f);
static const FLinearColor BingoBtnNormal(0.1f, 0.6f, 0.2f, 1.f);
static const FLinearColor BingoBtnHover(0.15f, 0.75f, 0.3f, 1.f);
static const FLinearColor BingoBtnPress(0.08f, 0.45f, 0.15f, 1.f);

// -- Column header colours (one per B-I-N-G-O) ------------------------------
static const FLinearColor ColHeaderColors[5] =
{
	FLinearColor(0.2f, 0.4f, 0.9f, 1.f),   // B  - blue
	FLinearColor(0.9f, 0.3f, 0.2f, 1.f),   // I  - red
	FLinearColor(0.95f, 0.85f, 0.1f, 1.f),  // N  - yellow
	FLinearColor(0.1f, 0.75f, 0.3f, 1.f),   // G  - green
	FLinearColor(0.8f, 0.4f, 0.9f, 1.f),    // O  - purple
};

static const TCHAR* BingoLetters[5] = { TEXT("B"), TEXT("I"), TEXT("N"), TEXT("G"), TEXT("O") };

// ---------------------------------------------------------------------------
// Initialize - build all Bingo-specific widgets programmatically
// ---------------------------------------------------------------------------

bool UBingoHUD::Initialize()
{
	// Let the base class build the top bar, pause overlay, and game-over overlay.
	if (!Super::Initialize())
	{
		return false;
	}

	// Guard against double-build: CalledBoardWidget is a reliable sentinel
	// because it is always constructed here, whereas BingoCardWidget is a
	// UUserWidget sub-class that cannot be created via WidgetTree.
	if (CalledBoardWidget)
	{
		return true; // Already built.
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	// -- Fonts ---------------------------------------------------------------
	const FSlateFontInfo HeaderFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);
	const FSlateFontInfo CellFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
	const FSlateFontInfo BallFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 36);
	const FSlateFontInfo LabelFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo CalledNumFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 13);
	const FSlateFontInfo BtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);

	// ========================================================================
	// GAME AREA - full-screen below the 50px top bar
	// ========================================================================
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("BingoGameArea"));
	GameArea->SetBrushColor(BingoBgColor);
	GameArea->SetPadding(FMargin(12.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f));

	// Main vertical layout: [CurrentBall] [Card] [CalledBoard] [BingoBtn + CPU]
	UVerticalBox* MainVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("BingoMainVBox"));
	GameArea->SetContent(MainVBox);

	// ========================================================================
	// 1. CURRENT BALL DISPLAY
	// ========================================================================
	{
		UBorder* BallBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("BallBorder"));
		BallBorder->SetBrushColor(FLinearColor(0.1f, 0.1f, 0.25f, 1.f));
		BallBorder->SetPadding(FMargin(16.f, 8.f));

		UTextBlock* BallLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("BallLabel"));
		BallLabel->SetText(FText::FromString(TEXT("--")));
		BallLabel->SetFont(BallFont);
		BallLabel->SetColorAndOpacity(FSlateColor(BallDisplayColor));
		BallLabel->SetJustification(ETextJustify::Center);
		BallBorder->SetContent(BallLabel);

		UVerticalBoxSlot* BallSlot = MainVBox->AddChildToVerticalBox(BallBorder);
		BallSlot->SetHorizontalAlignment(HAlign_Center);
		BallSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 8.f));
	}

	// ========================================================================
	// 2. BINGO CARD (5x5 grid with B-I-N-G-O column headers)
	// ========================================================================
	{
		UBorder* CardBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("CardBorder"));
		CardBorder->SetBrushColor(CardBgColor);
		CardBorder->SetPadding(FMargin(8.f));

		UVerticalBox* CardVBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("CardVBox"));
		CardBorder->SetContent(CardVBox);

		// -- Column header row: B I N G O --
		UHorizontalBox* HeaderRow = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("BingoHeaderRow"));

		for (int32 c = 0; c < 5; ++c)
		{
			USizeBox* LetterSize = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(),
				*FString::Printf(TEXT("HeaderSize_%d"), c));
			LetterSize->SetMinDesiredWidth(56.f);
			LetterSize->SetMinDesiredHeight(40.f);

			UBorder* LetterBg = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(),
				*FString::Printf(TEXT("HeaderBg_%d"), c));
			LetterBg->SetBrushColor(ColHeaderColors[c]);
			LetterBg->SetPadding(FMargin(0.f));

			UTextBlock* LetterText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(),
				*FString::Printf(TEXT("HeaderLetter_%d"), c));
			LetterText->SetText(FText::FromString(BingoLetters[c]));
			LetterText->SetFont(HeaderFont);
			LetterText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			LetterText->SetJustification(ETextJustify::Center);

			LetterBg->SetContent(LetterText);
			LetterSize->AddChild(LetterBg);

			UHorizontalBoxSlot* HSlot = HeaderRow->AddChildToHorizontalBox(LetterSize);
			HSlot->SetHorizontalAlignment(HAlign_Center);
			HSlot->SetPadding(FMargin(1.f, 0.f));
		}

		UVerticalBoxSlot* HeaderRowSlot = CardVBox->AddChildToVerticalBox(HeaderRow);
		HeaderRowSlot->SetHorizontalAlignment(HAlign_Center);
		HeaderRowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));

		// -- 5x5 number grid --
		UUniformGridPanel* CardGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(
			UUniformGridPanel::StaticClass(), TEXT("BingoCardGrid"));
		CardGrid->SetSlotPadding(FMargin(1.f));

		for (int32 Row = 0; Row < 5; ++Row)
		{
			for (int32 Col = 0; Col < 5; ++Col)
			{
				USizeBox* CellSize = WidgetTree->ConstructWidget<USizeBox>(
					USizeBox::StaticClass(),
					*FString::Printf(TEXT("CellSize_%d_%d"), Row, Col));
				CellSize->SetMinDesiredWidth(56.f);
				CellSize->SetMinDesiredHeight(48.f);

				UBorder* CellBg = WidgetTree->ConstructWidget<UBorder>(
					UBorder::StaticClass(),
					*FString::Printf(TEXT("CellBg_%d_%d"), Row, Col));
				CellBg->SetBrushColor(CellDefaultColor);
				CellBg->SetPadding(FMargin(4.f));

				UTextBlock* CellText = WidgetTree->ConstructWidget<UTextBlock>(
					UTextBlock::StaticClass(),
					*FString::Printf(TEXT("CellText_%d_%d"), Row, Col));

				// Centre cell (2,2) is the free space.
				if (Row == 2 && Col == 2)
				{
					CellText->SetText(FText::FromString(TEXT("FREE")));
				}
				else
				{
					CellText->SetText(FText::FromString(TEXT("--")));
				}
				CellText->SetFont(CellFont);
				CellText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				CellText->SetJustification(ETextJustify::Center);

				CellBg->SetContent(CellText);
				CellSize->AddChild(CellBg);

				CardGrid->AddChildToUniformGrid(CellSize, Row, Col);
			}
		}

		UVerticalBoxSlot* GridSlot = CardVBox->AddChildToVerticalBox(CardGrid);
		GridSlot->SetHorizontalAlignment(HAlign_Center);
		GridSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

		UVerticalBoxSlot* CardSlot = MainVBox->AddChildToVerticalBox(CardBorder);
		CardSlot->SetHorizontalAlignment(HAlign_Center);
		CardSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	// ========================================================================
	// 3. CALLED NUMBERS BOARD (WrapBox)
	// ========================================================================
	{
		UBorder* CalledBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("CalledBorder"));
		CalledBorder->SetBrushColor(CalledBoardBgColor);
		CalledBorder->SetPadding(FMargin(8.f, 6.f));

		UVerticalBox* CalledVBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("CalledVBox"));
		CalledBorder->SetContent(CalledVBox);

		UTextBlock* CalledLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("CalledLabel"));
		CalledLabel->SetText(FText::FromString(TEXT("Called Numbers")));
		CalledLabel->SetFont(LabelFont);
		CalledLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.8f)));
		CalledLabel->SetJustification(ETextJustify::Center);

		UVerticalBoxSlot* CLabelSlot = CalledVBox->AddChildToVerticalBox(CalledLabel);
		CLabelSlot->SetHorizontalAlignment(HAlign_Center);
		CLabelSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

		CalledBoardWidget = WidgetTree->ConstructWidget<UWrapBox>(
			UWrapBox::StaticClass(), TEXT("CalledBoardWidget"));
		CalledBoardWidget->SetInnerSlotPadding(FVector2D(4.f, 4.f));

		UVerticalBoxSlot* WrapSlot = CalledVBox->AddChildToVerticalBox(CalledBoardWidget);
		WrapSlot->SetHorizontalAlignment(HAlign_Center);
		WrapSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UVerticalBoxSlot* CalledSlot = MainVBox->AddChildToVerticalBox(CalledBorder);
		CalledSlot->SetHorizontalAlignment(HAlign_Fill);
		CalledSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		CalledSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));
	}

	// ========================================================================
	// 4. BOTTOM ROW: Bingo! button + CPU count text
	// ========================================================================
	{
		UHorizontalBox* BottomHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("BingoBottomHBox"));

		// -- "Bingo!" call button --
		BingoCallButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("BingoCallButton"));
		{
			FButtonStyle Style = BingoCallButton->GetStyle();
			Style.Normal.TintColor = FSlateColor(BingoBtnNormal);
			Style.Hovered.TintColor = FSlateColor(BingoBtnHover);
			Style.Pressed.TintColor = FSlateColor(BingoBtnPress);
			BingoCallButton->SetStyle(Style);

			UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("BingoCallBtnLbl"));
			BtnLabel->SetText(FText::FromString(TEXT("BINGO!")));
			BtnLabel->SetFont(BtnFont);
			BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			BtnLabel->SetJustification(ETextJustify::Center);
			BingoCallButton->AddChild(BtnLabel);
		}

		USizeBox* BtnSizeBox = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("BingoBtnSizeBox"));
		BtnSizeBox->SetMinDesiredWidth(160.f);
		BtnSizeBox->SetMinDesiredHeight(48.f);
		BtnSizeBox->AddChild(BingoCallButton);

		UHorizontalBoxSlot* BtnSlot = BottomHBox->AddChildToHorizontalBox(BtnSizeBox);
		BtnSlot->SetVerticalAlignment(VAlign_Center);
		BtnSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

		// -- Spacer between button and CPU text --
		USpacer* BottomSpacer = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("BottomSpacer"));
		UHorizontalBoxSlot* SpacerSlot = BottomHBox->AddChildToHorizontalBox(BottomSpacer);
		SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// -- CPU opponent count text --
		CPUCountText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("CPUCountText"));
		CPUCountText->SetText(FText::FromString(TEXT("Opponents: 19")));
		CPUCountText->SetFont(LabelFont);
		CPUCountText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.8f, 0.9f)));

		UHorizontalBoxSlot* CPUSlot = BottomHBox->AddChildToHorizontalBox(CPUCountText);
		CPUSlot->SetVerticalAlignment(VAlign_Center);
		CPUSlot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));

		UVerticalBoxSlot* BottomSlot = MainVBox->AddChildToVerticalBox(BottomHBox);
		BottomSlot->SetHorizontalAlignment(HAlign_Fill);
		BottomSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	}

	// Note: BingoCardWidget (UBingoCard) is a UUserWidget sub-class and
	// cannot be created via WidgetTree->ConstructWidget.  It remains null
	// here; the visual 5x5 grid above serves the same display purpose,
	// and NativeConstruct() already null-checks BingoCardWidget before use.

	UE_LOG(LogBingoHUD, Log,
		TEXT("BingoHUD widget tree built programmatically "
			 "(CalledBoard, BingoCallButton, CPUCountText)."));

	return true;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void UBingoHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Resolve the bingo game-mode from the base-class reference
	// (set by the PlayerController before NativeConstruct runs).
	BingoModeRef = Cast<ABingoGameMode>(GameModeRef);

	if (BingoModeRef)
	{
		// Subscribe to bingo-specific delegates.
		BingoModeRef->OnNumberCalled.AddDynamic(this, &UBingoHUD::HandleNumberCalled);
		BingoModeRef->OnBingoAchieved.AddDynamic(this, &UBingoHUD::HandleBingoAchieved);

		// Feed the card widget with the player's card data.
		if (BingoCardWidget)
		{
			BingoCardWidget->SetGameMode(BingoModeRef);
			BingoCardWidget->SetCard(BingoModeRef->GetPlayerCardFlat());
		}
	}

	if (BingoCallButton)
	{
		BingoCallButton->OnClicked.AddDynamic(this, &UBingoHUD::OnBingoClicked);
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UBingoHUD::OnNumberCalled(int32 Number)
{
	// Forward to the card widget.
	if (BingoCardWidget)
	{
		BingoCardWidget->HighlightCalledNumber(Number);
	}

	// Add a text block to the called-board widget.
	if (CalledBoardWidget)
	{
		UTextBlock* NumText = NewObject<UTextBlock>(this);
		if (NumText)
		{
			NumText->SetText(FText::AsNumber(Number));

			FSlateFontInfo Font = NumText->GetFont();
			Font.Size = 14;
			NumText->SetFont(Font);

			UWrapBoxSlot* Slot = CalledBoardWidget->AddChildToWrapBox(NumText);
			if (Slot)
			{
				Slot->SetPadding(FMargin(4.f));
			}
		}
	}
}

void UBingoHUD::UpdateCPUStatus(int32 CPUsRemaining)
{
	if (CPUCountText)
	{
		CPUCountText->SetText(FText::FromString(
			FString::Printf(TEXT("Opponents: %d"), CPUsRemaining)));
	}
}

// ---------------------------------------------------------------------------
// Internal callbacks
// ---------------------------------------------------------------------------

void UBingoHUD::OnBingoClicked()
{
	if (BingoModeRef)
	{
		BingoModeRef->OnBingoCalled();
	}
}

void UBingoHUD::HandleNumberCalled(int32 Number)
{
	OnNumberCalled(Number);

	// Update the CPU count each time a number is called.
	if (BingoModeRef)
	{
		UpdateCPUStatus(BingoModeRef->GetCPUOpponentsRemaining());
	}
}

void UBingoHUD::HandleBingoAchieved(bool bIsPlayer, int32 WinnerIndex)
{
	if (bIsPlayer)
	{
		// Player won -- the base HUD will show game-over via OnGameEnded.
		// Optionally play a celebration animation here.
	}
	else
	{
		// A CPU opponent achieved bingo first.
		if (CPUCountText)
		{
			CPUCountText->SetText(FText::FromString(
				FString::Printf(TEXT("CPU #%d got Bingo!"), WinnerIndex)));
		}
	}
}
