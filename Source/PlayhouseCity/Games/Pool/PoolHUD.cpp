// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Pool/PoolHUD.h"
#include "Games/Pool/PoolGameMode.h"
#include "Games/Pool/PoolBall.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPoolHUD, Log, All);

static const FLinearColor PoolBgColor(0.02f, 0.08f, 0.02f, 1.f);
static const FLinearColor PoolPanelColor(0.06f, 0.06f, 0.15f, 1.f);
static const FLinearColor PoolAccent(0.2f, 0.85f, 0.4f, 1.f);
static const FLinearColor PoolFoulColor(0.95f, 0.2f, 0.15f, 1.f);

// ---------------------------------------------------------------------------
// Initialize - build the Pool HUD UI
// ---------------------------------------------------------------------------

bool UPoolHUD::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (PowerBar)
	{
		return true; // Already built.
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo LabelFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo StatusFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20);
	const FSlateFontInfo FoulFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 24);

	// ---- Game content area (below top bar) ----
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GameArea"));
	GameArea->SetBrushColor(PoolBgColor);
	GameArea->SetPadding(FMargin(10.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f));

	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("ContentVBox"));
	GameArea->SetContent(ContentVBox);

	// ---- Turn indicator ----
	TurnIndicatorText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TurnIndicatorText"));
	TurnIndicatorText->SetText(FText::FromString(TEXT("Your Turn")));
	TurnIndicatorText->SetFont(StatusFont);
	TurnIndicatorText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	TurnIndicatorText->SetJustification(ETextJustify::Center);

	UVerticalBoxSlot* TurnSlot = ContentVBox->AddChildToVerticalBox(TurnIndicatorText);
	TurnSlot->SetHorizontalAlignment(HAlign_Center);
	TurnSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));

	// ---- Power bar row ----
	UHorizontalBox* PowerRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("PowerRow"));
	UVerticalBoxSlot* PowerRowSlot = ContentVBox->AddChildToVerticalBox(PowerRow);
	PowerRowSlot->SetHorizontalAlignment(HAlign_Center);
	PowerRowSlot->SetPadding(FMargin(0.f, 4.f));

	UTextBlock* PowerLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PowerLabel"));
	PowerLabel->SetText(FText::FromString(TEXT("Power: ")));
	PowerLabel->SetFont(LabelFont);
	PowerLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	UHorizontalBoxSlot* PowerLabelSlot = PowerRow->AddChildToHorizontalBox(PowerLabel);
	PowerLabelSlot->SetVerticalAlignment(VAlign_Center);
	PowerLabelSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	USizeBox* PowerSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("PowerSizeBox"));
	PowerSizeBox->SetMinDesiredWidth(200.f);
	PowerSizeBox->SetMinDesiredHeight(20.f);

	PowerBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("PowerBar"));
	PowerBar->SetPercent(0.f);
	PowerBar->SetFillColorAndOpacity(PoolAccent);

	PowerSizeBox->AddChild(PowerBar);

	UHorizontalBoxSlot* PowerBarSlot = PowerRow->AddChildToHorizontalBox(PowerSizeBox);
	PowerBarSlot->SetVerticalAlignment(VAlign_Center);

	// ---- Ball type labels row ----
	UHorizontalBox* BallTypeRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("BallTypeRow"));
	UVerticalBoxSlot* BallTypeRowSlot = ContentVBox->AddChildToVerticalBox(BallTypeRow);
	BallTypeRowSlot->SetHorizontalAlignment(HAlign_Center);
	BallTypeRowSlot->SetPadding(FMargin(0.f, 6.f));

	PlayerBallTypeText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PlayerBallTypeText"));
	PlayerBallTypeText->SetText(FText::FromString(TEXT("You: ---")));
	PlayerBallTypeText->SetFont(LabelFont);
	PlayerBallTypeText->SetColorAndOpacity(FSlateColor(PoolAccent));

	UHorizontalBoxSlot* PlayerTypeSlot = BallTypeRow->AddChildToHorizontalBox(PlayerBallTypeText);
	PlayerTypeSlot->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));

	CPUBallTypeText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CPUBallTypeText"));
	CPUBallTypeText->SetText(FText::FromString(TEXT("CPU: ---")));
	CPUBallTypeText->SetFont(LabelFont);
	CPUBallTypeText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.4f, 0.4f)));

	BallTypeRow->AddChildToHorizontalBox(CPUBallTypeText);

	// ---- Potted ball containers ----
	UHorizontalBox* PottedRow = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("PottedRow"));
	UVerticalBoxSlot* PottedRowSlot = ContentVBox->AddChildToVerticalBox(PottedRow);
	PottedRowSlot->SetHorizontalAlignment(HAlign_Center);
	PottedRowSlot->SetPadding(FMargin(0.f, 4.f));

	UTextBlock* PPLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PPLabel"));
	PPLabel->SetText(FText::FromString(TEXT("You: ")));
	PPLabel->SetFont(LabelFont);
	PPLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	UHorizontalBoxSlot* PPLabelSlot = PottedRow->AddChildToHorizontalBox(PPLabel);
	PPLabelSlot->SetVerticalAlignment(VAlign_Center);

	PlayerPottedBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("PlayerPottedBox"));
	UHorizontalBoxSlot* PPSlot = PottedRow->AddChildToHorizontalBox(PlayerPottedBox);
	PPSlot->SetPadding(FMargin(0.f, 0.f, 20.f, 0.f));

	UTextBlock* CPLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CPLabel"));
	CPLabel->SetText(FText::FromString(TEXT("CPU: ")));
	CPLabel->SetFont(LabelFont);
	CPLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	UHorizontalBoxSlot* CPLabelSlot = PottedRow->AddChildToHorizontalBox(CPLabel);
	CPLabelSlot->SetVerticalAlignment(VAlign_Center);

	CPUPottedBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CPUPottedBox"));
	PottedRow->AddChildToHorizontalBox(CPUPottedBox);

	// ---- Foul overlay ----
	FoulOverlay = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("FoulOverlay"));
	FoulOverlay->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* FoulSlot = RootCanvas->AddChildToCanvas(FoulOverlay);
	FoulSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	FoulSlot->SetOffsets(FMargin(0.f));

	UBorder* FoulBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("FoulBg"));
	FoulBg->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
	FoulBg->SetPadding(FMargin(20.f));

	UCanvasPanelSlot* FoulBgSlot = FoulOverlay->AddChildToCanvas(FoulBg);
	FoulBgSlot->SetAnchors(FAnchors(0.2f, 0.35f, 0.8f, 0.65f));
	FoulBgSlot->SetOffsets(FMargin(0.f));

	FoulText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FoulText"));
	FoulText->SetText(FText::FromString(TEXT("FOUL")));
	FoulText->SetFont(FoulFont);
	FoulText->SetColorAndOpacity(FSlateColor(PoolFoulColor));
	FoulText->SetJustification(ETextJustify::Center);
	FoulBg->SetContent(FoulText);

	UE_LOG(LogPoolHUD, Log, TEXT("Pool HUD built programmatically."));

	return true;
}

// ---------------------------------------------------------------------------

void UPoolHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Cast from the base-class reference (set by the PlayerController
	// before NativeConstruct runs).
	PoolMode = Cast<APoolGameMode>(GameModeRef);

	if (FoulOverlay)
	{
		FoulOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (PowerBar)
	{
		PowerBar->SetPercent(0.f);
	}
}

// ---------------------------------------------------------------------------

void UPoolHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Auto-hide foul indicator
	if (bShowingFoul)
	{
		FoulDisplayTimer += InDeltaTime;
		if (FoulDisplayTimer >= FoulDisplayDuration)
		{
			HideFoulIndicator();
		}
	}

	RefreshDisplay();
}

// ---------------------------------------------------------------------------

void UPoolHUD::ShowFoulIndicator(const FText& Message)
{
	bShowingFoul = true;
	FoulDisplayTimer = 0.f;

	if (FoulOverlay)
	{
		FoulOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (FoulText)
	{
		FoulText->SetText(Message);
	}
}

// ---------------------------------------------------------------------------

void UPoolHUD::HideFoulIndicator()
{
	bShowingFoul = false;

	if (FoulOverlay)
	{
		FoulOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ---------------------------------------------------------------------------

void UPoolHUD::SetPowerLevel(float Power)
{
	if (PowerBar)
	{
		PowerBar->SetPercent(FMath::Clamp(Power, 0.f, 1.f));
	}
}

// ---------------------------------------------------------------------------

void UPoolHUD::RefreshDisplay()
{
	if (!PoolMode)
	{
		return;
	}

	// Turn indicator
	if (TurnIndicatorText)
	{
		const FText TurnMsg = PoolMode->bPlayerTurn
			? FText::FromString(TEXT("Your Turn"))
			: FText::FromString(TEXT("CPU Turn"));
		TurnIndicatorText->SetText(TurnMsg);
	}

	// Ball type assignment labels
	if (PoolMode->bBallTypeAssigned)
	{
		if (PlayerBallTypeText)
		{
			const FString TypeStr = (PoolMode->PlayerBallType == EPHCPoolBallType::Solid)
				? TEXT("Solids") : TEXT("Stripes");
			PlayerBallTypeText->SetText(FText::FromString(FString::Printf(TEXT("You: %s"), *TypeStr)));
		}
		if (CPUBallTypeText)
		{
			const FString TypeStr = (PoolMode->CPUBallType == EPHCPoolBallType::Solid)
				? TEXT("Solids") : TEXT("Stripes");
			CPUBallTypeText->SetText(FText::FromString(FString::Printf(TEXT("CPU: %s"), *TypeStr)));
		}
	}
	else
	{
		if (PlayerBallTypeText)
		{
			PlayerBallTypeText->SetText(FText::FromString(TEXT("You: ---")));
		}
		if (CPUBallTypeText)
		{
			CPUBallTypeText->SetText(FText::FromString(TEXT("CPU: ---")));
		}
	}

	// Score display
	UpdateScore(PoolMode->GetScore());
}
