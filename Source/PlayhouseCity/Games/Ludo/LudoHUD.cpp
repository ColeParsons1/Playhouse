// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Ludo/LudoHUD.h"
#include "Games/Ludo/LudoBoard.h"
#include "Games/Ludo/LudoGameMode.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogLudoHUD, Log, All);

static const FLinearColor LudoBoardBg(0.06f, 0.06f, 0.15f, 1.f);
static const FLinearColor LudoRedColor(0.9f, 0.15f, 0.15f, 1.f);
static const FLinearColor LudoBlueColor(0.15f, 0.4f, 0.9f, 1.f);
static const FLinearColor LudoGreenColor(0.15f, 0.8f, 0.3f, 1.f);
static const FLinearColor LudoYellowColor(0.95f, 0.85f, 0.1f, 1.f);

// ---------------------------------------------------------------------------
// Initialize - build the Ludo HUD UI programmatically
// ---------------------------------------------------------------------------

bool ULudoHUD::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (PlayerTurnLabel)
	{
		return true; // Already built.
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo TurnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20);
	const FSlateFontInfo SmallFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);

	// ---- Game content area (below top bar) ----
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GameArea"));
	GameArea->SetBrushColor(LudoBoardBg);
	GameArea->SetPadding(FMargin(10.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f));

	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("ContentVBox"));
	GameArea->SetContent(ContentVBox);

	// ---- Turn label ----
	PlayerTurnLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PlayerTurnLabel"));
	PlayerTurnLabel->SetText(FText::FromString(TEXT("Your Turn")));
	PlayerTurnLabel->SetFont(TurnFont);
	PlayerTurnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	PlayerTurnLabel->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* TurnSlot = ContentVBox->AddChildToVerticalBox(PlayerTurnLabel);
	TurnSlot->SetHorizontalAlignment(HAlign_Center);
	TurnSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 8.f));

	// ---- Player color indicators (4 colored squares in a row) ----
	UHorizontalBox* IndicatorRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("IndicatorRow"));
	UVerticalBoxSlot* IndSlot = ContentVBox->AddChildToVerticalBox(IndicatorRow);
	IndSlot->SetHorizontalAlignment(HAlign_Center);
	IndSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 8.f));

	// Red indicator
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RedIndSize"));
		Size->SetMinDesiredWidth(32.f);
		Size->SetMinDesiredHeight(32.f);
		RedIndicator = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("RedIndicator"));
		RedIndicator->SetColorAndOpacity(LudoRedColor);
		Size->AddChild(RedIndicator);
		UHorizontalBoxSlot* S = IndicatorRow->AddChildToHorizontalBox(Size);
		S->SetPadding(FMargin(4.f, 0.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}

	// Blue indicator
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("BlueIndSize"));
		Size->SetMinDesiredWidth(32.f);
		Size->SetMinDesiredHeight(32.f);
		BlueIndicator = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("BlueIndicator"));
		BlueIndicator->SetColorAndOpacity(LudoBlueColor);
		Size->AddChild(BlueIndicator);
		UHorizontalBoxSlot* S = IndicatorRow->AddChildToHorizontalBox(Size);
		S->SetPadding(FMargin(4.f, 0.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}

	// Green indicator
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GreenIndSize"));
		Size->SetMinDesiredWidth(32.f);
		Size->SetMinDesiredHeight(32.f);
		GreenIndicator = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("GreenIndicator"));
		GreenIndicator->SetColorAndOpacity(LudoGreenColor);
		Size->AddChild(GreenIndicator);
		UHorizontalBoxSlot* S = IndicatorRow->AddChildToHorizontalBox(Size);
		S->SetPadding(FMargin(4.f, 0.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}

	// Yellow indicator
	{
		USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("YellowIndSize"));
		Size->SetMinDesiredWidth(32.f);
		Size->SetMinDesiredHeight(32.f);
		YellowIndicator = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("YellowIndicator"));
		YellowIndicator->SetColorAndOpacity(LudoYellowColor);
		Size->AddChild(YellowIndicator);
		UHorizontalBoxSlot* S = IndicatorRow->AddChildToHorizontalBox(Size);
		S->SetPadding(FMargin(4.f, 0.f));
		S->SetHorizontalAlignment(HAlign_Center);
	}

	// ---- Ludo board widget ----
	LudoBoardWidget = WidgetTree->ConstructWidget<ULudoBoard>(
		ULudoBoard::StaticClass(), TEXT("LudoBoardWidget"));

	UVerticalBoxSlot* BoardSlot = ContentVBox->AddChildToVerticalBox(LudoBoardWidget);
	BoardSlot->SetHorizontalAlignment(HAlign_Center);
	BoardSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	BoardSlot->SetPadding(FMargin(0.f, 4.f));

	UE_LOG(LogLudoHUD, Log, TEXT("Ludo HUD built programmatically."));

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct - wire up delegates
// ---------------------------------------------------------------------------

void ULudoHUD::NativeConstruct()
{
	Super::NativeConstruct();

	if (LudoBoardWidget)
	{
		LudoBoardWidget->OnDiceClicked.AddDynamic(this, &ULudoHUD::HandleDiceClicked);
		LudoBoardWidget->OnPieceClicked.AddDynamic(this, &ULudoHUD::HandlePieceClicked);
	}
}

void ULudoHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ULudoHUD::SetLudoGameMode(ALudoGameMode* InGameMode)
{
	LudoGameModeRef = InGameMode;

	// Also set the base class reference.
	SetGameMode(InGameMode);

	if (LudoGameModeRef)
	{
		LudoGameModeRef->OnDiceRolled.AddDynamic(this, &ULudoHUD::HandleDiceRolled);
		LudoGameModeRef->OnPieceMoved.AddDynamic(this, &ULudoHUD::HandlePieceMoved);
		LudoGameModeRef->OnTurnChanged.AddDynamic(this, &ULudoHUD::HandleTurnChanged);
		LudoGameModeRef->OnPieceCaptured.AddDynamic(this, &ULudoHUD::HandlePieceCaptured);
	}

	if (LudoBoardWidget)
	{
		LudoBoardWidget->InitializeBoard();
	}

	RefreshBoard();
	UpdateTurnIndicators(0);
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void ULudoHUD::HandleDiceClicked()
{
	if (!LudoGameModeRef)
	{
		return;
	}

	// Only allow human player to roll.
	int32 CurrentIdx = LudoGameModeRef->GetCurrentPlayerIndex();
	if (CurrentIdx != 0)
	{
		return; // Not the human's turn.
	}

	LudoGameModeRef->RollDice();
}

void ULudoHUD::HandlePieceClicked(int32 PieceIndex)
{
	if (!LudoGameModeRef)
	{
		return;
	}

	// Only allow human player to select pieces.
	if (LudoGameModeRef->GetCurrentPlayerIndex() != 0)
	{
		return;
	}

	// PieceIndex from the board is an absolute index (0-15).
	// The human player is index 0, so pieces 0-3 belong to them.
	if (PieceIndex < 0 || PieceIndex > 3)
	{
		return;
	}

	if (LudoGameModeRef->CanMovePiece(PieceIndex))
	{
		LudoGameModeRef->MovePiece(PieceIndex);
	}
}

void ULudoHUD::HandleDiceRolled(int32 Value)
{
	if (LudoBoardWidget)
	{
		LudoBoardWidget->ShowDiceResult(Value);

		// Show valid moves for the human player.
		if (LudoGameModeRef && LudoGameModeRef->GetCurrentPlayerIndex() == 0)
		{
			TArray<int32> ValidMoves = LudoGameModeRef->GetValidMoves();
			LudoBoardWidget->HighlightValidMoves(ValidMoves);
		}
	}
}

void ULudoHUD::HandlePieceMoved(int32 PlayerIndex, int32 PieceIndex)
{
	RefreshBoard();
}

void ULudoHUD::HandleTurnChanged(int32 NewPlayerIndex)
{
	UpdateTurnIndicators(NewPlayerIndex);
	RefreshBoard();
}

void ULudoHUD::HandlePieceCaptured(int32 CapturedPlayerIndex, int32 CapturedPieceIndex)
{
	if (LudoBoardWidget)
	{
		int32 AbsoluteIndex = CapturedPlayerIndex * 4 + CapturedPieceIndex;
		LudoBoardWidget->ShowCapture(AbsoluteIndex);
	}

	RefreshBoard();
}

// ---------------------------------------------------------------------------
// Internal
// ---------------------------------------------------------------------------

void ULudoHUD::UpdateTurnIndicators(int32 ActivePlayerIndex)
{
	const float ActiveOpacity = 1.0f;
	const float InactiveOpacity = 0.3f;

	if (RedIndicator)    RedIndicator->SetRenderOpacity(ActivePlayerIndex == 0 ? ActiveOpacity : InactiveOpacity);
	if (BlueIndicator)   BlueIndicator->SetRenderOpacity(ActivePlayerIndex == 1 ? ActiveOpacity : InactiveOpacity);
	if (GreenIndicator)  GreenIndicator->SetRenderOpacity(ActivePlayerIndex == 2 ? ActiveOpacity : InactiveOpacity);
	if (YellowIndicator) YellowIndicator->SetRenderOpacity(ActivePlayerIndex == 3 ? ActiveOpacity : InactiveOpacity);

	if (PlayerTurnLabel)
	{
		static const FString ColorNames[] = { TEXT("Red"), TEXT("Blue"), TEXT("Green"), TEXT("Yellow") };
		FString LabelStr = (ActivePlayerIndex == 0)
			? TEXT("Your Turn")
			: FString::Printf(TEXT("%s's Turn"), *ColorNames[FMath::Clamp(ActivePlayerIndex, 0, 3)]);
		PlayerTurnLabel->SetText(FText::FromString(LabelStr));
	}
}

void ULudoHUD::RefreshBoard()
{
	if (LudoBoardWidget && LudoGameModeRef)
	{
		TArray<FPHCLudoPiece> AllPieces = LudoGameModeRef->GetAllPieces();
		LudoBoardWidget->UpdatePieces(AllPieces);
	}
}
