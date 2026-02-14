// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/KitchenChaos/KitchenHUD.h"
#include "Games/KitchenChaos/KitchenGameMode.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogKitchenHUD, Log, All);

// -- Kitchen-specific palette ------------------------------------------------
static const FLinearColor KitchenBg(0.15f, 0.12f, 0.1f, 0.95f);      // Warm dark brown
static const FLinearColor CounterColor(0.92f, 0.9f, 0.88f, 1.f);      // White counter
static const FLinearColor OrderCardBg(0.2f, 0.18f, 0.15f, 1.f);       // Dark card bg
static const FLinearColor IngBtnNormal(0.85f, 0.75f, 0.55f, 1.f);     // Warm tan
static const FLinearColor IngBtnHover(0.95f, 0.85f, 0.65f, 1.f);      // Light tan
static const FLinearColor IngBtnPress(0.7f, 0.6f, 0.4f, 1.f);         // Pressed tan
static const FLinearColor StationBtnNormal(0.5f, 0.45f, 0.4f, 1.f);   // Warm gray
static const FLinearColor StationBtnHover(0.6f, 0.55f, 0.5f, 1.f);
static const FLinearColor StationBtnPress(0.4f, 0.35f, 0.3f, 1.f);
static const FLinearColor ServeBtnColor(0.2f, 0.75f, 0.3f, 1.f);      // Green SERVE
static const FLinearColor ServeBtnHover(0.3f, 0.85f, 0.4f, 1.f);
static const FLinearColor ServeBtnPress(0.15f, 0.6f, 0.2f, 1.f);
static const FLinearColor ClearBtnColor(0.8f, 0.25f, 0.2f, 1.f);     // Red CLEAR
static const FLinearColor ClearBtnHover(0.9f, 0.35f, 0.3f, 1.f);
static const FLinearColor ClearBtnPress(0.65f, 0.15f, 0.12f, 1.f);
static const FLinearColor CookBtnColor(0.9f, 0.5f, 0.1f, 1.f);       // Orange COOK stations
static const FLinearColor CookBtnHover(1.f, 0.6f, 0.2f, 1.f);
static const FLinearColor CookBtnPress(0.75f, 0.4f, 0.05f, 1.f);

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

bool UKitchenHUD::Initialize()
{
	// Build base HUD widgets first (score, timer, pause, game over overlays).
	if (!Super::Initialize()) return false;

	// Guard against double-construction.
	if (KitchenCanvas)
	{
		return true;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	// -- Fonts ---------------------------------------------------------------
	const FSlateFontInfo HeaderFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo BodyFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 13);
	const FSlateFontInfo BtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);
	const FSlateFontInfo SmallFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 11);
	const FSlateFontInfo ComboFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);

	// Helper: create a styled button with a text label.
	auto MakeButton = [&](const FName& Name, const FString& Label,
		FLinearColor NormalCol, FLinearColor HoverCol, FLinearColor PressCol,
		const FSlateFontInfo& Font) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor = FSlateColor(NormalCol);
		Style.Hovered.TintColor = FSlateColor(HoverCol);
		Style.Pressed.TintColor = FSlateColor(PressCol);
		Btn->SetStyle(Style);

		UTextBlock* Lbl = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("Lbl"))));
		Lbl->SetText(FText::FromString(Label));
		Lbl->SetFont(Font);
		Lbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		Lbl->SetJustification(ETextJustify::Center);
		Btn->AddChild(Lbl);

		return Btn;
	};

	// ========================================================================
	// GAME CONTENT AREA (below top bar)
	// ========================================================================
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("KitchenGameArea"));
	GameArea->SetBrushColor(KitchenBg);
	GameArea->SetPadding(FMargin(10.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	UVerticalBox* ContentVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("KitchenContentVBox"));
	GameArea->SetContent(ContentVBox);

	// -- Store a reference for the canvas-level widget. --
	KitchenCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("KitchenCanvas"));

	// ========================================================================
	// SECTION 1: ORDER QUEUE (top, 3 slots)
	// ========================================================================
	{
		UTextBlock* OrderHeader = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("OrderHeader"));
		OrderHeader->SetText(FText::FromString(TEXT("ORDERS")));
		OrderHeader->SetFont(HeaderFont);
		OrderHeader->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.4f)));
		UVerticalBoxSlot* OHSlot = ContentVBox->AddChildToVerticalBox(OrderHeader);
		OHSlot->SetHorizontalAlignment(HAlign_Center);
		OHSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 2.f));

		OrdersBar = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("OrdersBar"));
		UVerticalBoxSlot* OBSlot = ContentVBox->AddChildToVerticalBox(OrdersBar);
		OBSlot->SetHorizontalAlignment(HAlign_Center);
		OBSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));

		// Pre-create 3 order slot borders (visual containers for dynamic cards).
		OrderSlotWidgets.SetNum(3);
		OrderTimerBars.SetNum(3);
		for (int32 i = 0; i < 3; ++i)
		{
			UBorder* SlotBorder = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), *FString::Printf(TEXT("OrderSlot%d"), i));
			SlotBorder->SetBrushColor(OrderCardBg);
			SlotBorder->SetPadding(FMargin(8.f, 6.f));

			USizeBox* SlotSize = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(), *FString::Printf(TEXT("OrderSlotSize%d"), i));
			SlotSize->SetMinDesiredWidth(130.f);
			SlotSize->SetMinDesiredHeight(80.f);

			// Vertical box inside the border for the card content.
			UVerticalBox* CardVBox = WidgetTree->ConstructWidget<UVerticalBox>(
				UVerticalBox::StaticClass(), *FString::Printf(TEXT("OrderCardVBox%d"), i));

			UTextBlock* EmptyLabel = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), *FString::Printf(TEXT("OrderEmpty%d"), i));
			EmptyLabel->SetText(FText::FromString(TEXT("---")));
			EmptyLabel->SetFont(BodyFont);
			EmptyLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.4f, 0.4f)));
			EmptyLabel->SetJustification(ETextJustify::Center);
			CardVBox->AddChildToVerticalBox(EmptyLabel);

			// Timer progress bar at the bottom of each card.
			UProgressBar* TimerBar = WidgetTree->ConstructWidget<UProgressBar>(
				UProgressBar::StaticClass(), *FString::Printf(TEXT("OrderTimer%d"), i));
			TimerBar->SetPercent(1.f);
			TimerBar->SetFillColorAndOpacity(FLinearColor(0.2f, 0.8f, 0.3f, 1.f));
			UVerticalBoxSlot* TBSlot = CardVBox->AddChildToVerticalBox(TimerBar);
			TBSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

			OrderTimerBars[i] = TimerBar;

			SlotBorder->SetContent(CardVBox);
			SlotSize->AddChild(SlotBorder);

			UHorizontalBoxSlot* CardSlot = OrdersBar->AddChildToHorizontalBox(SlotSize);
			CardSlot->SetPadding(FMargin(4.f, 0.f));
			CardSlot->SetHorizontalAlignment(HAlign_Center);

			OrderSlotWidgets[i] = SlotBorder;
		}
	}

	// ========================================================================
	// SECTION 2: COMBO / FEEDBACK TEXT
	// ========================================================================
	{
		ComboText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ComboText"));
		ComboText->SetText(FText::GetEmpty());
		ComboText->SetFont(ComboFont);
		ComboText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.9f, 0.2f)));
		ComboText->SetJustification(ETextJustify::Center);
		ComboText->SetVisibility(ESlateVisibility::Collapsed);
		UVerticalBoxSlot* CTSlot = ContentVBox->AddChildToVerticalBox(ComboText);
		CTSlot->SetHorizontalAlignment(HAlign_Center);
		CTSlot->SetPadding(FMargin(0.f, 2.f));

		FeedbackText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("FeedbackText"));
		FeedbackText->SetText(FText::GetEmpty());
		FeedbackText->SetFont(HeaderFont);
		FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 1.f, 0.4f)));
		FeedbackText->SetJustification(ETextJustify::Center);
		FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
		UVerticalBoxSlot* FBSlot = ContentVBox->AddChildToVerticalBox(FeedbackText);
		FBSlot->SetHorizontalAlignment(HAlign_Center);
		FBSlot->SetPadding(FMargin(0.f, 2.f));
	}

	// ========================================================================
	// SECTION 3: KITCHEN COUNTER (middle - held item + plate + current dish)
	// ========================================================================
	{
		UBorder* CounterBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("CounterBorder"));
		CounterBorder->SetBrushColor(CounterColor);
		CounterBorder->SetPadding(FMargin(12.f, 8.f));

		UVerticalBox* CounterVBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("CounterVBox"));
		CounterBorder->SetContent(CounterVBox);

		// -- Held item row --
		UHorizontalBox* HeldRow = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("HeldRow"));

		UTextBlock* HeldLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("HeldLabel"));
		HeldLabel->SetText(FText::FromString(TEXT("Holding: ")));
		HeldLabel->SetFont(BodyFont);
		HeldLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.25f, 0.2f)));
		UHorizontalBoxSlot* HLSlot = HeldRow->AddChildToHorizontalBox(HeldLabel);
		HLSlot->SetVerticalAlignment(VAlign_Center);

		HeldItemImage = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(), TEXT("HeldItemImage"));
		HeldItemImage->SetVisibility(ESlateVisibility::Collapsed);

		USizeBox* HeldImgSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(), TEXT("HeldImgSize"));
		HeldImgSize->SetMinDesiredWidth(28.f);
		HeldImgSize->SetMinDesiredHeight(28.f);
		HeldImgSize->AddChild(HeldItemImage);
		UHorizontalBoxSlot* HISSlot = HeldRow->AddChildToHorizontalBox(HeldImgSize);
		HISSlot->SetVerticalAlignment(VAlign_Center);
		HISSlot->SetPadding(FMargin(4.f, 0.f));

		HeldItemText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("HeldItemText"));
		HeldItemText->SetText(FText::FromString(TEXT("Empty")));
		HeldItemText->SetFont(BodyFont);
		HeldItemText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
		UHorizontalBoxSlot* HITSlot = HeldRow->AddChildToHorizontalBox(HeldItemText);
		HITSlot->SetVerticalAlignment(VAlign_Center);

		UVerticalBoxSlot* HRSlot = CounterVBox->AddChildToVerticalBox(HeldRow);
		HRSlot->SetPadding(FMargin(0.f, 2.f));

		// -- Plate display row --
		UHorizontalBox* PlateRow = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("PlateRow"));

		UTextBlock* PlateLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("PlateLabel"));
		PlateLabel->SetText(FText::FromString(TEXT("Plate: ")));
		PlateLabel->SetFont(BodyFont);
		PlateLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.25f, 0.2f)));
		UHorizontalBoxSlot* PLSlot = PlateRow->AddChildToHorizontalBox(PlateLabel);
		PLSlot->SetVerticalAlignment(VAlign_Center);

		PlateDisplay = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("PlateDisplay"));
		UHorizontalBoxSlot* PDSlot = PlateRow->AddChildToHorizontalBox(PlateDisplay);
		PDSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		PDSlot->SetVerticalAlignment(VAlign_Center);

		UVerticalBoxSlot* PRSlot = CounterVBox->AddChildToVerticalBox(PlateRow);
		PRSlot->SetPadding(FMargin(0.f, 2.f));

		// -- Current dish text --
		CurrentDishText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("CurrentDishText"));
		CurrentDishText->SetText(FText::FromString(TEXT("Assemble ingredients on the plate")));
		CurrentDishText->SetFont(SmallFont);
		CurrentDishText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.45f, 0.4f)));
		CurrentDishText->SetJustification(ETextJustify::Center);
		UVerticalBoxSlot* CDSlot = CounterVBox->AddChildToVerticalBox(CurrentDishText);
		CDSlot->SetHorizontalAlignment(HAlign_Center);
		CDSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 2.f));

		// -- SUBMIT / CLEAR buttons row --
		UHorizontalBox* ActionRow = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("ActionRow"));

		SubmitButton = MakeButton(TEXT("SubmitButton"), TEXT("SERVE"),
			ServeBtnColor, ServeBtnHover, ServeBtnPress, BtnFont);
		UHorizontalBoxSlot* SBSlot = ActionRow->AddChildToHorizontalBox(SubmitButton);
		SBSlot->SetPadding(FMargin(4.f, 0.f));
		SBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		ClearPlateButton = MakeButton(TEXT("ClearPlateButton"), TEXT("CLEAR"),
			ClearBtnColor, ClearBtnHover, ClearBtnPress, BtnFont);
		UHorizontalBoxSlot* CBSlot = ActionRow->AddChildToHorizontalBox(ClearPlateButton);
		CBSlot->SetPadding(FMargin(4.f, 0.f));
		CBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UVerticalBoxSlot* ARSlot = CounterVBox->AddChildToVerticalBox(ActionRow);
		ARSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

		UVerticalBoxSlot* CounterSlot = ContentVBox->AddChildToVerticalBox(CounterBorder);
		CounterSlot->SetPadding(FMargin(20.f, 6.f));
	}

	// ========================================================================
	// SECTION 4: COOKING STATIONS
	// ========================================================================
	{
		UTextBlock* StationHeader = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("StationHeader"));
		StationHeader->SetText(FText::FromString(TEXT("COOKING STATIONS")));
		StationHeader->SetFont(HeaderFont);
		StationHeader->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.65f, 0.2f)));
		UVerticalBoxSlot* SHSlot = ContentVBox->AddChildToVerticalBox(StationHeader);
		SHSlot->SetHorizontalAlignment(HAlign_Center);
		SHSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 2.f));

		UHorizontalBox* StationRow = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("StationRow"));

		GrillButton = MakeButton(TEXT("GrillButton"), TEXT("GRILL"),
			CookBtnColor, CookBtnHover, CookBtnPress, BtnFont);
		UHorizontalBoxSlot* GBSlot = StationRow->AddChildToHorizontalBox(GrillButton);
		GBSlot->SetPadding(FMargin(4.f, 0.f));
		GBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		ChopButton = MakeButton(TEXT("ChopButton"), TEXT("CHOP"),
			FLinearColor(0.7f, 0.55f, 0.3f, 1.f),
			FLinearColor(0.8f, 0.65f, 0.4f, 1.f),
			FLinearColor(0.6f, 0.45f, 0.2f, 1.f), BtnFont);
		UHorizontalBoxSlot* CHBSlot = StationRow->AddChildToHorizontalBox(ChopButton);
		CHBSlot->SetPadding(FMargin(4.f, 0.f));
		CHBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		FryButton = MakeButton(TEXT("FryButton"), TEXT("FRY"),
			FLinearColor(0.4f, 0.4f, 0.45f, 1.f),
			FLinearColor(0.5f, 0.5f, 0.55f, 1.f),
			FLinearColor(0.3f, 0.3f, 0.35f, 1.f), BtnFont);
		UHorizontalBoxSlot* FBSlot = StationRow->AddChildToHorizontalBox(FryButton);
		FBSlot->SetPadding(FMargin(4.f, 0.f));
		FBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		ServeButton = MakeButton(TEXT("ServeButton"), TEXT("COUNTER"),
			FLinearColor(0.8f, 0.8f, 0.85f, 1.f),
			FLinearColor(0.9f, 0.9f, 0.92f, 1.f),
			FLinearColor(0.7f, 0.7f, 0.75f, 1.f), BtnFont);
		// Set the serve button label to dark text for readability on light bg.
		if (UTextBlock* ServeLbl = Cast<UTextBlock>(ServeButton->GetChildAt(0)))
		{
			ServeLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 0.2f, 0.25f)));
		}
		UHorizontalBoxSlot* SVBSlot = StationRow->AddChildToHorizontalBox(ServeButton);
		SVBSlot->SetPadding(FMargin(4.f, 0.f));
		SVBSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UVerticalBoxSlot* SRSlot = ContentVBox->AddChildToVerticalBox(StationRow);
		SRSlot->SetHorizontalAlignment(HAlign_Center);
		SRSlot->SetPadding(FMargin(20.f, 0.f, 20.f, 4.f));
	}

	// ========================================================================
	// SECTION 5: INGREDIENT SELECTION (bottom)
	// ========================================================================
	{
		UTextBlock* IngHeader = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("IngHeader"));
		IngHeader->SetText(FText::FromString(TEXT("INGREDIENTS")));
		IngHeader->SetFont(HeaderFont);
		IngHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.85f, 0.5f)));
		UVerticalBoxSlot* IHSlot = ContentVBox->AddChildToVerticalBox(IngHeader);
		IHSlot->SetHorizontalAlignment(HAlign_Center);
		IHSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 2.f));

		// Top row of ingredients: Bun, Patty, Lettuce, Tomato, Cheese
		UHorizontalBox* IngRow1 = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("IngRow1"));

		// Helper lambda to create an ingredient button with its ingredient color.
		auto MakeIngButton = [&](const FName& Name, const FString& Label,
			FLinearColor IngColor) -> UButton*
		{
			// Derive button colors from ingredient color (slightly muted).
			FLinearColor Normal = FLinearColor::LerpUsingHSV(IngColor, IngBtnNormal, 0.3f);
			Normal.A = 1.f;
			FLinearColor Hover = Normal * 1.15f;
			Hover.A = 1.f;
			FLinearColor Press = Normal * 0.8f;
			Press.A = 1.f;

			UButton* Btn = MakeButton(Name, Label, Normal, Hover, Press, SmallFont);
			return Btn;
		};

		BunButton = MakeIngButton(TEXT("BunButton"), TEXT("Bread"),
			FLinearColor(0.85f, 0.65f, 0.3f));
		UHorizontalBoxSlot* BB = IngRow1->AddChildToHorizontalBox(BunButton);
		BB->SetPadding(FMargin(3.f, 0.f));
		BB->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		PattyButton = MakeIngButton(TEXT("PattyButton"), TEXT("Meat"),
			FLinearColor(0.55f, 0.25f, 0.15f));
		UHorizontalBoxSlot* PB = IngRow1->AddChildToHorizontalBox(PattyButton);
		PB->SetPadding(FMargin(3.f, 0.f));
		PB->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		LettuceButton = MakeIngButton(TEXT("LettuceButton"), TEXT("Lettuce"),
			FLinearColor(0.3f, 0.8f, 0.2f));
		UHorizontalBoxSlot* LB = IngRow1->AddChildToHorizontalBox(LettuceButton);
		LB->SetPadding(FMargin(3.f, 0.f));
		LB->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		TomatoButton = MakeIngButton(TEXT("TomatoButton"), TEXT("Tomato"),
			FLinearColor(0.9f, 0.2f, 0.15f));
		UHorizontalBoxSlot* TB = IngRow1->AddChildToHorizontalBox(TomatoButton);
		TB->SetPadding(FMargin(3.f, 0.f));
		TB->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		CheeseButton = MakeIngButton(TEXT("CheeseButton"), TEXT("Cheese"),
			FLinearColor(1.f, 0.85f, 0.1f));
		UHorizontalBoxSlot* ChB = IngRow1->AddChildToHorizontalBox(CheeseButton);
		ChB->SetPadding(FMargin(3.f, 0.f));
		ChB->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UVerticalBoxSlot* IR1Slot = ContentVBox->AddChildToVerticalBox(IngRow1);
		IR1Slot->SetHorizontalAlignment(HAlign_Center);
		IR1Slot->SetPadding(FMargin(20.f, 0.f, 20.f, 2.f));

		// Bottom row of ingredients: Onion, Fish, Rice, Nori
		UHorizontalBox* IngRow2 = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("IngRow2"));

		OnionButton = MakeIngButton(TEXT("OnionButton"), TEXT("Onion"),
			FLinearColor(0.9f, 0.85f, 0.7f));
		UHorizontalBoxSlot* OB = IngRow2->AddChildToHorizontalBox(OnionButton);
		OB->SetPadding(FMargin(3.f, 0.f));
		OB->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		FishButton = MakeIngButton(TEXT("FishButton"), TEXT("Fish"),
			FLinearColor(0.6f, 0.75f, 0.85f));
		UHorizontalBoxSlot* FB = IngRow2->AddChildToHorizontalBox(FishButton);
		FB->SetPadding(FMargin(3.f, 0.f));
		FB->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		RiceButton = MakeIngButton(TEXT("RiceButton"), TEXT("Rice"),
			FLinearColor(0.95f, 0.95f, 0.9f));
		// Rice button label needs dark text (light background).
		if (UTextBlock* RiceLbl = Cast<UTextBlock>(RiceButton->GetChildAt(0)))
		{
			RiceLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 0.3f, 0.3f)));
		}
		UHorizontalBoxSlot* RB = IngRow2->AddChildToHorizontalBox(RiceButton);
		RB->SetPadding(FMargin(3.f, 0.f));
		RB->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		NoriButton = MakeIngButton(TEXT("NoriButton"), TEXT("Nori"),
			FLinearColor(0.1f, 0.25f, 0.1f));
		UHorizontalBoxSlot* NB = IngRow2->AddChildToHorizontalBox(NoriButton);
		NB->SetPadding(FMargin(3.f, 0.f));
		NB->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		UVerticalBoxSlot* IR2Slot = ContentVBox->AddChildToVerticalBox(IngRow2);
		IR2Slot->SetHorizontalAlignment(HAlign_Center);
		IR2Slot->SetPadding(FMargin(20.f, 0.f, 20.f, 4.f));
	}

	// ========================================================================
	// BOTTOM SPACER (fills remaining space)
	// ========================================================================
	{
		USpacer* BottomSpacer = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("BottomSpacer"));
		UVerticalBoxSlot* BSSlot = ContentVBox->AddChildToVerticalBox(BottomSpacer);
		BSSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	UE_LOG(LogKitchenHUD, Log,
		TEXT("Kitchen HUD built: orders=%d, ingredient btns=9, station btns=4"),
		OrderSlotWidgets.Num());

	return true;
}

void UKitchenHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Acquire the kitchen game mode from the base-class reference
	// (set by the PlayerController before NativeConstruct runs).
	KitchenMode = Cast<AKitchenGameMode>(GameModeRef);

	// -- Wire ingredient buttons ----------------------------------------------

	if (BunButton)		{ BunButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnBunClicked); }
	if (PattyButton)	{ PattyButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnPattyClicked); }
	if (LettuceButton)	{ LettuceButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnLettuceClicked); }
	if (TomatoButton)	{ TomatoButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnTomatoClicked); }
	if (CheeseButton)	{ CheeseButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnCheeseClicked); }
	if (OnionButton)	{ OnionButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnOnionClicked); }
	if (FishButton)		{ FishButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnFishClicked); }
	if (RiceButton)		{ RiceButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnRiceClicked); }
	if (NoriButton)		{ NoriButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnNoriClicked); }

	// -- Wire station buttons -------------------------------------------------

	if (GrillButton)	{ GrillButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnGrillClicked); }
	if (ChopButton)		{ ChopButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnChopClicked); }
	if (FryButton)		{ FryButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnFryClicked); }
	if (ServeButton)	{ ServeButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnServeClicked); }

	// -- Wire action buttons --------------------------------------------------

	if (SubmitButton)	{ SubmitButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnSubmitClicked); }
	if (ClearPlateButton) { ClearPlateButton->OnClicked.AddDynamic(this, &UKitchenHUD::OnClearClicked); }

	// -- Bind game mode delegates ---------------------------------------------

	if (KitchenMode)
	{
		KitchenMode->OnOrderCompleted.AddDynamic(this, &UKitchenHUD::HandleOrderCompleted);
		KitchenMode->OnOrderFailed.AddDynamic(this, &UKitchenHUD::HandleOrderFailed);
	}

	// -- Initial state --------------------------------------------------------

	if (FeedbackText)
	{
		FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (HeldItemImage)
	{
		HeldItemImage->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (HeldItemText)
	{
		HeldItemText->SetText(FText::FromString(TEXT("Empty")));
	}
}

void UKitchenHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!KitchenMode || !KitchenMode->bGameActive)
	{
		return;
	}

	// Sync display with game mode state each frame.
	UpdateOrders(KitchenMode->ActiveOrders);
	UpdateHeldItem(KitchenMode->HeldItem);
	UpdatePlate(KitchenMode->CurrentPlate);
}

// ---------------------------------------------------------------------------
// Visual updates
// ---------------------------------------------------------------------------

void UKitchenHUD::UpdateOrders(const TArray<FPHCOrder>& Orders)
{
	if (!OrdersBar)
	{
		return;
	}

	// Clear existing order card widgets.
	OrdersBar->ClearChildren();
	OrderCardWidgets.Empty();

	for (int32 i = 0; i < Orders.Num(); ++i)
	{
		const FPHCOrder& Order = Orders[i];

		// Create a vertical box for each order card.
		UVerticalBox* Card = NewObject<UVerticalBox>(this);
		if (!Card)
		{
			continue;
		}

		// -- Recipe name --
		UTextBlock* RecipeName = NewObject<UTextBlock>(this);
		if (RecipeName)
		{
			RecipeName->SetText(FText::FromString(
				AKitchenGameMode::GetRecipeName(Order.Recipe)));
			RecipeName->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			Card->AddChildToVerticalBox(RecipeName);
		}

		// -- Ingredient list --
		for (const EPHCIngredient& Ing : Order.RequiredIngredients)
		{
			UTextBlock* IngText = NewObject<UTextBlock>(this);
			if (IngText)
			{
				IngText->SetText(FText::FromString(
					FString::Printf(TEXT("  - %s"),
						*AKitchenGameMode::GetIngredientName(Ing))));

				const FLinearColor IngColor = GetIngredientColor(Ing);
				IngText->SetColorAndOpacity(FSlateColor(IngColor));

				Card->AddChildToVerticalBox(IngText);
			}
		}

		// -- Timer --
		UTextBlock* TimerLabel = NewObject<UTextBlock>(this);
		if (TimerLabel)
		{
			const int32 SecsRemaining = FMath::CeilToInt32(Order.TimeRemaining);
			TimerLabel->SetText(FText::FromString(
				FString::Printf(TEXT("Time: %ds"), SecsRemaining)));

			// Color based on urgency.
			FLinearColor TimerColor = FLinearColor::White;
			const float TimeRatio = Order.TimeRemaining / Order.TimeLimit;
			if (TimeRatio < 0.25f)
			{
				TimerColor = FLinearColor(1.f, 0.1f, 0.1f, 1.f);
			}
			else if (TimeRatio < 0.5f)
			{
				TimerColor = FLinearColor(1.f, 0.6f, 0.1f, 1.f);
			}
			TimerLabel->SetColorAndOpacity(FSlateColor(TimerColor));

			Card->AddChildToVerticalBox(TimerLabel);
		}

		// Add card to the horizontal bar with padding.
		UHorizontalBoxSlot* Slot = OrdersBar->AddChildToHorizontalBox(Card);
		if (Slot)
		{
			Slot->SetPadding(FMargin(8.f, 4.f, 8.f, 4.f));
		}

		OrderCardWidgets.Add(Card);
	}
}

void UKitchenHUD::UpdateHeldItem(const FPHCHeldItem& Item)
{
	if (!Item.bValid)
	{
		if (HeldItemImage)
		{
			HeldItemImage->SetVisibility(ESlateVisibility::Collapsed);
		}
		if (HeldItemText)
		{
			HeldItemText->SetText(FText::FromString(TEXT("Empty")));
			HeldItemText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.f)));
		}
		return;
	}

	if (HeldItemImage)
	{
		HeldItemImage->SetVisibility(ESlateVisibility::Visible);
		FLinearColor ItemColor = GetIngredientColor(Item.Ingredient);

		if (Item.bBurned)
		{
			ItemColor = FLinearColor(0.2f, 0.15f, 0.1f, 1.f); // Burned = dark brown
		}
		else if (Item.bCooked)
		{
			// Slightly darken to indicate cooked.
			ItemColor = ItemColor * 0.7f;
			ItemColor.A = 1.f;
		}

		HeldItemImage->SetColorAndOpacity(ItemColor);
	}

	if (HeldItemText)
	{
		FString Label = AKitchenGameMode::GetIngredientName(Item.Ingredient);
		if (Item.bBurned)
		{
			Label += TEXT(" (BURNED)");
		}
		else
		{
			if (Item.bChopped) { Label += TEXT(" [Chopped]"); }
			if (Item.bCooked) { Label += TEXT(" [Cooked]"); }
		}
		HeldItemText->SetText(FText::FromString(Label));
		HeldItemText->SetColorAndOpacity(FSlateColor(
			Item.bBurned ? FLinearColor(1.f, 0.2f, 0.2f, 1.f) : FLinearColor::White));
	}
}

void UKitchenHUD::UpdatePlate(const TArray<FPHCHeldItem>& Plate)
{
	if (!PlateDisplay)
	{
		return;
	}

	PlateDisplay->ClearChildren();

	if (Plate.Num() == 0)
	{
		// Show "empty plate" text.
		UTextBlock* EmptyLabel = NewObject<UTextBlock>(this);
		if (EmptyLabel)
		{
			EmptyLabel->SetText(FText::FromString(TEXT("Plate: empty")));
			EmptyLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.f)));
			PlateDisplay->AddChildToHorizontalBox(EmptyLabel);
		}
		return;
	}

	for (const FPHCHeldItem& PlateItem : Plate)
	{
		UImage* ItemImg = NewObject<UImage>(this);
		if (!ItemImg)
		{
			continue;
		}

		FLinearColor Color = GetIngredientColor(PlateItem.Ingredient);
		if (PlateItem.bCooked)
		{
			Color = Color * 0.7f;
			Color.A = 1.f;
		}
		ItemImg->SetColorAndOpacity(Color);

		UHorizontalBoxSlot* Slot = PlateDisplay->AddChildToHorizontalBox(ItemImg);
		if (Slot)
		{
			Slot->SetPadding(FMargin(2.f));
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		}
	}
}

void UKitchenHUD::ShowOrderComplete(int32 Score)
{
	if (!FeedbackText)
	{
		return;
	}

	FeedbackText->SetText(FText::FromString(
		FString::Printf(TEXT("Order Complete! +%d"), Score)));
	FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 1.f, 0.4f, 1.f)));
	FeedbackText->SetVisibility(ESlateVisibility::Visible);

	// Clear after 1.5 seconds.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FeedbackTimerHandle);

		World->GetTimerManager().SetTimer(FeedbackTimerHandle,
			[this]()
			{
				if (FeedbackText)
				{
					FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
				}
			}, 1.5f, false);
	}
}

void UKitchenHUD::ShowOrderFailed()
{
	if (!FeedbackText)
	{
		return;
	}

	FeedbackText->SetText(FText::FromString(TEXT("Order Expired!")));
	FeedbackText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.2f, 0.2f, 1.f)));
	FeedbackText->SetVisibility(ESlateVisibility::Visible);

	// Clear after 1.5 seconds.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(FeedbackTimerHandle);

		World->GetTimerManager().SetTimer(FeedbackTimerHandle,
			[this]()
			{
				if (FeedbackText)
				{
					FeedbackText->SetVisibility(ESlateVisibility::Collapsed);
				}
			}, 1.5f, false);
	}
}

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

FLinearColor UKitchenHUD::GetIngredientColor(EPHCIngredient Ingredient) const
{
	switch (Ingredient)
	{
	case EPHCIngredient::Bun:		return FLinearColor(0.85f, 0.65f, 0.3f, 1.f);  // Golden brown
	case EPHCIngredient::Patty:		return FLinearColor(0.55f, 0.25f, 0.15f, 1.f);  // Dark brown
	case EPHCIngredient::Lettuce:	return FLinearColor(0.3f, 0.8f, 0.2f, 1.f);     // Green
	case EPHCIngredient::Tomato:	return FLinearColor(0.9f, 0.2f, 0.15f, 1.f);    // Red
	case EPHCIngredient::Cheese:	return FLinearColor(1.f, 0.85f, 0.1f, 1.f);     // Yellow
	case EPHCIngredient::Onion:		return FLinearColor(0.9f, 0.85f, 0.7f, 1.f);    // Cream
	case EPHCIngredient::Fish:		return FLinearColor(0.6f, 0.75f, 0.85f, 1.f);   // Light blue
	case EPHCIngredient::Rice:		return FLinearColor(0.95f, 0.95f, 0.9f, 1.f);   // White
	case EPHCIngredient::Nori:		return FLinearColor(0.1f, 0.25f, 0.1f, 1.f);    // Dark green
	default:						return FLinearColor::White;
	}
}

FLinearColor UKitchenHUD::GetStationColor(EPHCCookingStation Station) const
{
	switch (Station)
	{
	case EPHCCookingStation::Grill:			return FLinearColor(0.9f, 0.3f, 0.1f, 1.f);
	case EPHCCookingStation::ChoppingBoard:	return FLinearColor(0.7f, 0.55f, 0.3f, 1.f);
	case EPHCCookingStation::FryingPan:		return FLinearColor(0.4f, 0.4f, 0.45f, 1.f);
	case EPHCCookingStation::ServingCounter:return FLinearColor(0.8f, 0.8f, 0.85f, 1.f);
	default:								return FLinearColor::White;
	}
}

// ---------------------------------------------------------------------------
// Ingredient button callbacks
// ---------------------------------------------------------------------------

void UKitchenHUD::OnBunClicked()		{ OnIngredientClicked(EPHCIngredient::Bun); }
void UKitchenHUD::OnPattyClicked()		{ OnIngredientClicked(EPHCIngredient::Patty); }
void UKitchenHUD::OnLettuceClicked()	{ OnIngredientClicked(EPHCIngredient::Lettuce); }
void UKitchenHUD::OnTomatoClicked()		{ OnIngredientClicked(EPHCIngredient::Tomato); }
void UKitchenHUD::OnCheeseClicked()		{ OnIngredientClicked(EPHCIngredient::Cheese); }
void UKitchenHUD::OnOnionClicked()		{ OnIngredientClicked(EPHCIngredient::Onion); }
void UKitchenHUD::OnFishClicked()		{ OnIngredientClicked(EPHCIngredient::Fish); }
void UKitchenHUD::OnRiceClicked()		{ OnIngredientClicked(EPHCIngredient::Rice); }
void UKitchenHUD::OnNoriClicked()		{ OnIngredientClicked(EPHCIngredient::Nori); }

// ---------------------------------------------------------------------------
// Station button callbacks
// ---------------------------------------------------------------------------

void UKitchenHUD::OnGrillClicked()	{ OnStationClicked(EPHCCookingStation::Grill); }
void UKitchenHUD::OnChopClicked()	{ OnStationClicked(EPHCCookingStation::ChoppingBoard); }
void UKitchenHUD::OnFryClicked()	{ OnStationClicked(EPHCCookingStation::FryingPan); }
void UKitchenHUD::OnServeClicked()	{ OnStationClicked(EPHCCookingStation::ServingCounter); }

// ---------------------------------------------------------------------------
// Action button callbacks
// ---------------------------------------------------------------------------

void UKitchenHUD::OnSubmitClicked()
{
	if (KitchenMode)
	{
		KitchenMode->SubmitOrder();
	}
}

void UKitchenHUD::OnClearClicked()
{
	if (KitchenMode)
	{
		KitchenMode->ClearPlate();
	}
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void UKitchenHUD::HandleOrderCompleted(int32 ScoreEarned)
{
	ShowOrderComplete(ScoreEarned);
}

void UKitchenHUD::HandleOrderFailed()
{
	ShowOrderFailed();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void UKitchenHUD::OnIngredientClicked(EPHCIngredient Ingredient)
{
	if (KitchenMode)
	{
		KitchenMode->PickupIngredient(Ingredient);
	}
}

void UKitchenHUD::OnStationClicked(EPHCCookingStation Station)
{
	if (KitchenMode)
	{
		KitchenMode->UseStation(Station);
	}
}
