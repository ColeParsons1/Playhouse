// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/GooBridge/GooBridgeHUD.h"
#include "Games/GooBridge/GooBridgeGameMode.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogGooBridgeHUD, Log, All);

// -- Colors ---------------------------------------------------------------

static const FLinearColor SkyBlue(0.53f, 0.81f, 0.92f, 1.f);
static const FLinearColor GroundGreen(0.25f, 0.6f, 0.18f, 1.f);
static const FLinearColor GapBrown(0.35f, 0.22f, 0.1f, 1.f);
static const FLinearColor NodeBrown(0.55f, 0.35f, 0.15f, 1.f);
static const FLinearColor StrutGray(0.5f, 0.5f, 0.5f, 1.f);
static const FLinearColor VehicleRed(0.85f, 0.15f, 0.1f, 1.f);
static const FLinearColor BtnNormal(0.2f, 0.3f, 0.7f, 1.f);
static const FLinearColor BtnHover(0.3f, 0.4f, 0.85f, 1.f);
static const FLinearColor BtnPress(0.15f, 0.2f, 0.5f, 1.f);
static const FLinearColor BtnActive(0.15f, 0.65f, 0.3f, 1.f);
static const FLinearColor BtnActiveHover(0.2f, 0.75f, 0.4f, 1.f);
static const FLinearColor GoGreen(0.1f, 0.7f, 0.2f, 1.f);
static const FLinearColor GoGreenHover(0.15f, 0.8f, 0.3f, 1.f);
static const FLinearColor DeleteRed(0.8f, 0.15f, 0.15f, 1.f);
static const FLinearColor DeleteRedHover(0.9f, 0.25f, 0.25f, 1.f);
static const FLinearColor PanelBg(0.05f, 0.05f, 0.1f, 0.85f);

// ---------------------------------------------------------------------------
// Initialize - build the Goo Bridge construction scene
// ---------------------------------------------------------------------------

bool UGooBridgeHUD::Initialize()
{
	// Build base HUD widgets first (score, timer, pause, game over overlays).
	if (!Super::Initialize())
	{
		return false;
	}

	// Guard against double initialization.
	if (GameCanvas)
	{
		return true;
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo LabelFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo StatusFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
	const FSlateFontInfo BtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);
	const FSlateFontInfo SmallFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 12);

	// Helper: create a styled button
	auto MakeButton = [&](const FName& Name, const FString& Label,
		FLinearColor NormalCol, FLinearColor HoverCol, FLinearColor PressCol) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor = FSlateColor(NormalCol);
		Style.Hovered.TintColor = FSlateColor(HoverCol);
		Style.Pressed.TintColor = FSlateColor(PressCol);
		Btn->SetStyle(Style);

		UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("Lbl"))));
		BtnLabel->SetText(FText::FromString(Label));
		BtnLabel->SetFont(BtnFont);
		BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BtnLabel->SetJustification(ETextJustify::Center);
		Btn->AddChild(BtnLabel);

		return Btn;
	};

	// ====================================================================
	// GAME CANVAS (below top bar, fills remaining space)
	// ====================================================================
	GameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("GameCanvas"));

	UCanvasPanelSlot* GameCanvasSlot = RootCanvas->AddChildToCanvas(GameCanvas);
	GameCanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameCanvasSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	// ---- Sky background ----
	SkyBackground = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SkyBg"));
	SkyBackground->SetBrushColor(SkyBlue);

	UCanvasPanelSlot* SkySlot = GameCanvas->AddChildToCanvas(SkyBackground);
	SkySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	SkySlot->SetOffsets(FMargin(0.f));

	// ====================================================================
	// BUILD AREA CANVAS (central construction zone)
	// ====================================================================
	BuildAreaCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("BuildAreaCanvas"));

	UCanvasPanelSlot* BuildSlot = GameCanvas->AddChildToCanvas(BuildAreaCanvas);
	BuildSlot->SetAnchors(FAnchors(0.05f, 0.05f, 0.95f, 0.78f));
	BuildSlot->SetOffsets(FMargin(0.f));

	// ====================================================================
	// LEFT GROUND PLATFORM
	// ====================================================================
	LeftGround = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("LeftGround"));
	LeftGround->SetBrushColor(GroundGreen);

	UCanvasPanelSlot* LeftGroundSlot = BuildAreaCanvas->AddChildToCanvas(LeftGround);
	LeftGroundSlot->SetAnchors(FAnchors(0.f, 0.65f, 0.35f, 1.f));
	LeftGroundSlot->SetOffsets(FMargin(0.f));

	// Left ground label
	UTextBlock* LeftLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("LeftGroundLbl"));
	LeftLabel->SetText(FText::FromString(TEXT("START")));
	LeftLabel->SetFont(SmallFont);
	LeftLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	LeftLabel->SetJustification(ETextJustify::Center);
	LeftGround->SetContent(LeftLabel);

	// ====================================================================
	// GAP / CHASM (center)
	// ====================================================================
	GapDisplay = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("GapDisplay"));
	GapDisplay->SetBrushColor(GapBrown);

	UCanvasPanelSlot* GapSlot = BuildAreaCanvas->AddChildToCanvas(GapDisplay);
	GapSlot->SetAnchors(FAnchors(0.35f, 0.75f, 0.65f, 1.f));
	GapSlot->SetOffsets(FMargin(0.f));

	// Gap label
	UTextBlock* GapLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("GapLbl"));
	GapLabel->SetText(FText::FromString(TEXT("-- GAP --")));
	GapLabel->SetFont(SmallFont);
	GapLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.5f, 0.3f)));
	GapLabel->SetJustification(ETextJustify::Center);
	GapDisplay->SetContent(GapLabel);

	// ====================================================================
	// RIGHT GROUND PLATFORM
	// ====================================================================
	RightGround = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("RightGround"));
	RightGround->SetBrushColor(GroundGreen);

	UCanvasPanelSlot* RightGroundSlot = BuildAreaCanvas->AddChildToCanvas(RightGround);
	RightGroundSlot->SetAnchors(FAnchors(0.65f, 0.65f, 1.f, 1.f));
	RightGroundSlot->SetOffsets(FMargin(0.f));

	// Right ground label
	UTextBlock* RightLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("RightGroundLbl"));
	RightLabel->SetText(FText::FromString(TEXT("GOAL")));
	RightLabel->SetFont(SmallFont);
	RightLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	RightLabel->SetJustification(ETextJustify::Center);
	RightGround->SetContent(RightLabel);

	// ====================================================================
	// INITIAL ANCHOR NODES (brown circles on platform edges)
	// ====================================================================
	struct FNodeDef { float AnchorX; float AnchorY; };
	const FNodeDef AnchorDefs[] = {
		{ 0.30f, 0.64f },  // Left platform edge - top
		{ 0.34f, 0.64f },  // Left platform edge - top right
		{ 0.66f, 0.64f },  // Right platform edge - top left
		{ 0.70f, 0.64f },  // Right platform edge - top right
	};

	NodeWidgets.SetNum(UE_ARRAY_COUNT(AnchorDefs));
	for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(AnchorDefs)); ++i)
	{
		const FNodeDef& Def = AnchorDefs[i];
		UBorder* Node = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("Node%d"), i));
		Node->SetBrushColor(NodeBrown);

		UCanvasPanelSlot* NodeSlot = BuildAreaCanvas->AddChildToCanvas(Node);
		NodeSlot->SetAnchors(FAnchors(Def.AnchorX, Def.AnchorY, Def.AnchorX, Def.AnchorY));
		NodeSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		NodeSlot->SetOffsets(FMargin(0.f, 0.f, 18.f, 18.f));

		NodeWidgets[i] = Node;
	}

	// ====================================================================
	// INITIAL STRUTS (gray connections between anchor nodes)
	// ====================================================================
	// Draw two struts: left pair and right pair
	struct FStrutDef { float X1; float Y1; float X2; float Y2; };
	const FStrutDef StrutDefs[] = {
		{ 0.30f, 0.64f, 0.34f, 0.64f },  // Left anchors horizontal
		{ 0.66f, 0.64f, 0.70f, 0.64f },  // Right anchors horizontal
	};

	StrutWidgets.SetNum(UE_ARRAY_COUNT(StrutDefs));
	for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(StrutDefs)); ++i)
	{
		const FStrutDef& Def = StrutDefs[i];
		UBorder* Strut = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("Strut%d"), i));
		Strut->SetBrushColor(StrutGray);

		// Position strut as a horizontal bar between the two nodes
		const float MidX = (Def.X1 + Def.X2) * 0.5f;
		const float MidY = (Def.Y1 + Def.Y2) * 0.5f;

		UCanvasPanelSlot* StrutSlot = BuildAreaCanvas->AddChildToCanvas(Strut);
		StrutSlot->SetAnchors(FAnchors(Def.X1, MidY, Def.X2, MidY));
		StrutSlot->SetAlignment(FVector2D(0.f, 0.5f));
		StrutSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 6.f)); // 6px thick strut

		StrutWidgets[i] = Strut;
	}

	// ====================================================================
	// VEHICLE (red, on left side)
	// ====================================================================
	VehicleWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Vehicle"));
	VehicleWidget->SetBrushColor(VehicleRed);

	UCanvasPanelSlot* VehicleSlot = BuildAreaCanvas->AddChildToCanvas(VehicleWidget);
	VehicleSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	VehicleSlot->SetOffsets(FMargin(VehicleStartX, 0.f, 50.f, 30.f)); // 50x30 vehicle

	// Position vehicle on top of left ground
	VehicleSlot->SetAnchors(FAnchors(0.08f, 0.58f, 0.08f, 0.58f));
	VehicleSlot->SetAlignment(FVector2D(0.f, 1.f));
	VehicleSlot->SetOffsets(FMargin(0.f, 0.f, 50.f, 28.f));

	// Vehicle label
	UTextBlock* VehicleLbl = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("VehicleLbl"));
	VehicleLbl->SetText(FText::FromString(TEXT("CAR")));
	VehicleLbl->SetFont(SmallFont);
	VehicleLbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	VehicleLbl->SetJustification(ETextJustify::Center);
	VehicleWidget->SetContent(VehicleLbl);

	// ====================================================================
	// BOTTOM TOOL PANEL
	// ====================================================================
	UBorder* ToolPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ToolPanel"));
	ToolPanel->SetBrushColor(PanelBg);
	ToolPanel->SetPadding(FMargin(12.f, 8.f));

	UCanvasPanelSlot* ToolPanelSlot = GameCanvas->AddChildToCanvas(ToolPanel);
	ToolPanelSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
	ToolPanelSlot->SetOffsets(FMargin(0.f, -90.f, 0.f, 0.f)); // 90px at bottom

	UHorizontalBox* ToolHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ToolHBox"));
	ToolPanel->SetContent(ToolHBox);

	// ---- Build Mode text ----
	BuildModeText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("BuildModeText"));
	BuildModeText->SetText(FText::FromString(TEXT("Mode: Place Node")));
	BuildModeText->SetFont(StatusFont);
	BuildModeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	UHorizontalBoxSlot* ModeSlot = ToolHBox->AddChildToHorizontalBox(BuildModeText);
	ModeSlot->SetVerticalAlignment(VAlign_Center);
	ModeSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

	// ---- Goo count ----
	GooCountText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("GooCountText"));
	GooCountText->SetText(FText::FromString(TEXT("Goo: 20")));
	GooCountText->SetFont(StatusFont);
	GooCountText->SetColorAndOpacity(FSlateColor(NodeBrown));

	UHorizontalBoxSlot* GooSlot = ToolHBox->AddChildToHorizontalBox(GooCountText);
	GooSlot->SetVerticalAlignment(VAlign_Center);
	GooSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

	// ---- Nodes remaining ----
	NodesRemainingText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("NodesRemainingText"));
	NodesRemainingText->SetText(FText::FromString(TEXT("Nodes: 4")));
	NodesRemainingText->SetFont(LabelFont);
	NodesRemainingText->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.8f, 0.9f)));

	UHorizontalBoxSlot* NodesSlot = ToolHBox->AddChildToHorizontalBox(NodesRemainingText);
	NodesSlot->SetVerticalAlignment(VAlign_Center);
	NodesSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	// ---- Level text ----
	LevelText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("LevelText"));
	LevelText->SetText(FText::FromString(TEXT("Level 1")));
	LevelText->SetFont(LabelFont);
	LevelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	UHorizontalBoxSlot* LvlSlot = ToolHBox->AddChildToHorizontalBox(LevelText);
	LvlSlot->SetVerticalAlignment(VAlign_Center);
	LvlSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	// ---- Spacer ----
	USpacer* ToolSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("ToolSpacer"));
	UHorizontalBoxSlot* SpacerSlot = ToolHBox->AddChildToHorizontalBox(ToolSpacer);
	SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// ---- Place Node button (active by default) ----
	PlaceNodeButton = MakeButton(TEXT("PlaceNodeBtn"), TEXT("Place Node"),
		BtnActive, BtnActiveHover, BtnPress);

	UHorizontalBoxSlot* PlaceSlot = ToolHBox->AddChildToHorizontalBox(PlaceNodeButton);
	PlaceSlot->SetVerticalAlignment(VAlign_Center);
	PlaceSlot->SetPadding(FMargin(4.f, 0.f));
	PlaceSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	// ---- Connect button ----
	ConnectButton = MakeButton(TEXT("ConnectBtn"), TEXT("Connect"),
		BtnNormal, BtnHover, BtnPress);

	UHorizontalBoxSlot* ConnSlot = ToolHBox->AddChildToHorizontalBox(ConnectButton);
	ConnSlot->SetVerticalAlignment(VAlign_Center);
	ConnSlot->SetPadding(FMargin(4.f, 0.f));
	ConnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	// ---- Delete button ----
	DeleteButton = MakeButton(TEXT("DeleteBtn"), TEXT("Delete"),
		DeleteRed, DeleteRedHover, BtnPress);

	UHorizontalBoxSlot* DelSlot = ToolHBox->AddChildToHorizontalBox(DeleteButton);
	DelSlot->SetVerticalAlignment(VAlign_Center);
	DelSlot->SetPadding(FMargin(4.f, 0.f));
	DelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	// ---- Simulate button ----
	SimulateButton = MakeButton(TEXT("SimulateBtn"), TEXT("Simulate"),
		BtnNormal, BtnHover, BtnPress);

	UHorizontalBoxSlot* SimSlot = ToolHBox->AddChildToHorizontalBox(SimulateButton);
	SimSlot->SetVerticalAlignment(VAlign_Center);
	SimSlot->SetPadding(FMargin(4.f, 0.f));
	SimSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	// ---- Reset button ----
	ResetButton = MakeButton(TEXT("ResetBtn"), TEXT("Reset"),
		BtnNormal, BtnHover, BtnPress);

	UHorizontalBoxSlot* RstSlot = ToolHBox->AddChildToHorizontalBox(ResetButton);
	RstSlot->SetVerticalAlignment(VAlign_Center);
	RstSlot->SetPadding(FMargin(4.f, 0.f));
	RstSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	// ---- GO button (starts vehicle crossing) ----
	GoButton = MakeButton(TEXT("GoBtn"), TEXT("GO!"),
		GoGreen, GoGreenHover, BtnPress);

	UHorizontalBoxSlot* GoSlot = ToolHBox->AddChildToHorizontalBox(GoButton);
	GoSlot->SetVerticalAlignment(VAlign_Center);
	GoSlot->SetPadding(FMargin(8.f, 0.f));
	GoSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	UE_LOG(LogGooBridgeHUD, Log,
		TEXT("GooBridge HUD built: %d anchor nodes, %d struts, vehicle, tool panel with 6 buttons"),
		NodeWidgets.Num(), StrutWidgets.Num());

	return true;
}

// ---------------------------------------------------------------------------

void UGooBridgeHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Cache the game mode from the base-class reference
	// (set by the PlayerController before NativeConstruct runs).
	GooBridgeMode = Cast<AGooBridgeGameMode>(GameModeRef);

	// Bind button callbacks
	if (SimulateButton)
	{
		SimulateButton->OnClicked.AddDynamic(this, &UGooBridgeHUD::OnSimulateClicked);
	}
	if (ResetButton)
	{
		ResetButton->OnClicked.AddDynamic(this, &UGooBridgeHUD::OnResetClicked);
	}
	if (PlaceNodeButton)
	{
		PlaceNodeButton->OnClicked.AddDynamic(this, &UGooBridgeHUD::OnPlaceNodeClicked);
	}
	if (ConnectButton)
	{
		ConnectButton->OnClicked.AddDynamic(this, &UGooBridgeHUD::OnConnectClicked);
	}
	if (DeleteButton)
	{
		DeleteButton->OnClicked.AddDynamic(this, &UGooBridgeHUD::OnDeleteClicked);
	}
	if (GoButton)
	{
		GoButton->OnClicked.AddDynamic(this, &UGooBridgeHUD::OnGoClicked);
	}

	RefreshDisplay();
}

// ---------------------------------------------------------------------------

void UGooBridgeHUD::NativeTick(
	const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	RefreshDisplay();

	// -- Vehicle crossing animation --
	if (bVehicleCrossing && VehicleWidget)
	{
		VehiclePosX += VehicleSpeed * InDeltaTime;

		UCanvasPanelSlot* VSlot = Cast<UCanvasPanelSlot>(VehicleWidget->Slot);
		if (VSlot)
		{
			// Convert pixel position to a relative anchor X
			// Build area is roughly 0.05 to 0.95 of screen width;
			// vehicle travels across build area
			const float NormalizedX = FMath::GetMappedRangeValueClamped(
				FVector2D(VehicleStartX, VehicleTargetX),
				FVector2D(0.08f, 0.85f),
				VehiclePosX);
			VSlot->SetAnchors(FAnchors(NormalizedX, 0.58f, NormalizedX, 0.58f));
		}

		// Check if vehicle reached the right side
		if (VehiclePosX >= VehicleTargetX)
		{
			bVehicleCrossing = false;

			// Notify game mode of successful crossing
			if (GooBridgeMode)
			{
				GooBridgeMode->StopSimulation();
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Mouse input
// ---------------------------------------------------------------------------

FReply UGooBridgeHUD::NativeOnMouseButtonDown(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDragging = true;
		DragStartPos = InMouseEvent.GetScreenSpacePosition();
		CurrentDragPos = DragStartPos;
		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UGooBridgeHUD::NativeOnMouseButtonUp(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDragging && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bIsDragging = false;

		// Place a node at the release position
		if (GooBridgeMode && !GooBridgeMode->bSimulating)
		{
			const FVector2D GamePos = ScreenToGamePosition(
				InMouseEvent.GetScreenSpacePosition());

			switch (CurrentTool)
			{
			case EBuildTool::PlaceNode:
				GooBridgeMode->PlaceNode(GamePos);
				break;
			case EBuildTool::Connect:
				// In connect mode, placing creates a node and auto-connects
				GooBridgeMode->PlaceNode(GamePos);
				break;
			case EBuildTool::Delete:
				{
					// Find nearest node to delete
					TArray<AGooNode*> Nearby = GooBridgeMode->GetNearbyNodes(GamePos, 50.f);
					if (Nearby.Num() > 0)
					{
						GooBridgeMode->RemoveNode(Nearby[0]);
					}
				}
				break;
			}
		}

		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UGooBridgeHUD::NativeOnMouseMove(
	const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsDragging)
	{
		CurrentDragPos = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

// ---------------------------------------------------------------------------
// Touch input
// ---------------------------------------------------------------------------

FReply UGooBridgeHUD::NativeOnTouchStarted(
	const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	bIsDragging = true;
	DragStartPos = InTouchEvent.GetScreenSpacePosition();
	CurrentDragPos = DragStartPos;
	return FReply::Handled();
}

FReply UGooBridgeHUD::NativeOnTouchMoved(
	const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	if (bIsDragging)
	{
		CurrentDragPos = InTouchEvent.GetScreenSpacePosition();
	}
	return FReply::Handled();
}

FReply UGooBridgeHUD::NativeOnTouchEnded(
	const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	if (bIsDragging)
	{
		bIsDragging = false;

		if (GooBridgeMode && !GooBridgeMode->bSimulating)
		{
			const FVector2D GamePos = ScreenToGamePosition(
				InTouchEvent.GetScreenSpacePosition());

			switch (CurrentTool)
			{
			case EBuildTool::PlaceNode:
			case EBuildTool::Connect:
				GooBridgeMode->PlaceNode(GamePos);
				break;
			case EBuildTool::Delete:
				{
					TArray<AGooNode*> Nearby = GooBridgeMode->GetNearbyNodes(GamePos, 50.f);
					if (Nearby.Num() > 0)
					{
						GooBridgeMode->RemoveNode(Nearby[0]);
					}
				}
				break;
			}
		}
	}
	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UGooBridgeHUD::OnSimulateClicked()
{
	if (!GooBridgeMode)
	{
		return;
	}

	if (GooBridgeMode->bSimulating)
	{
		GooBridgeMode->StopSimulation();
	}
	else
	{
		GooBridgeMode->StartSimulation();
	}
}

// ---------------------------------------------------------------------------

void UGooBridgeHUD::OnResetClicked()
{
	if (GooBridgeMode)
	{
		GooBridgeMode->LoadLevel(GooBridgeMode->CurrentLevel);
	}

	// Reset vehicle position
	bVehicleCrossing = false;
	VehiclePosX = VehicleStartX;
	if (VehicleWidget)
	{
		UCanvasPanelSlot* VSlot = Cast<UCanvasPanelSlot>(VehicleWidget->Slot);
		if (VSlot)
		{
			VSlot->SetAnchors(FAnchors(0.08f, 0.58f, 0.08f, 0.58f));
		}
	}
}

// ---------------------------------------------------------------------------

void UGooBridgeHUD::OnPlaceNodeClicked()
{
	CurrentTool = EBuildTool::PlaceNode;
	if (BuildModeText)
	{
		BuildModeText->SetText(FText::FromString(TEXT("Mode: Place Node")));
	}

	// Update button visual states
	if (PlaceNodeButton)
	{
		FButtonStyle Style = PlaceNodeButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(BtnActive);
		Style.Hovered.TintColor = FSlateColor(BtnActiveHover);
		PlaceNodeButton->SetStyle(Style);
	}
	if (ConnectButton)
	{
		FButtonStyle Style = ConnectButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(BtnNormal);
		Style.Hovered.TintColor = FSlateColor(BtnHover);
		ConnectButton->SetStyle(Style);
	}
	if (DeleteButton)
	{
		FButtonStyle Style = DeleteButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(DeleteRed);
		Style.Hovered.TintColor = FSlateColor(DeleteRedHover);
		DeleteButton->SetStyle(Style);
	}
}

void UGooBridgeHUD::OnConnectClicked()
{
	CurrentTool = EBuildTool::Connect;
	if (BuildModeText)
	{
		BuildModeText->SetText(FText::FromString(TEXT("Mode: Connect")));
	}

	// Update button visual states
	if (PlaceNodeButton)
	{
		FButtonStyle Style = PlaceNodeButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(BtnNormal);
		Style.Hovered.TintColor = FSlateColor(BtnHover);
		PlaceNodeButton->SetStyle(Style);
	}
	if (ConnectButton)
	{
		FButtonStyle Style = ConnectButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(BtnActive);
		Style.Hovered.TintColor = FSlateColor(BtnActiveHover);
		ConnectButton->SetStyle(Style);
	}
	if (DeleteButton)
	{
		FButtonStyle Style = DeleteButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(DeleteRed);
		Style.Hovered.TintColor = FSlateColor(DeleteRedHover);
		DeleteButton->SetStyle(Style);
	}
}

void UGooBridgeHUD::OnDeleteClicked()
{
	CurrentTool = EBuildTool::Delete;
	if (BuildModeText)
	{
		BuildModeText->SetText(FText::FromString(TEXT("Mode: Delete")));
	}

	// Update button visual states
	if (PlaceNodeButton)
	{
		FButtonStyle Style = PlaceNodeButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(BtnNormal);
		Style.Hovered.TintColor = FSlateColor(BtnHover);
		PlaceNodeButton->SetStyle(Style);
	}
	if (ConnectButton)
	{
		FButtonStyle Style = ConnectButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(BtnNormal);
		Style.Hovered.TintColor = FSlateColor(BtnHover);
		ConnectButton->SetStyle(Style);
	}
	if (DeleteButton)
	{
		FButtonStyle Style = DeleteButton->GetStyle();
		Style.Normal.TintColor = FSlateColor(BtnActive);
		Style.Hovered.TintColor = FSlateColor(BtnActiveHover);
		DeleteButton->SetStyle(Style);
	}
}

void UGooBridgeHUD::OnGoClicked()
{
	if (!GooBridgeMode)
	{
		return;
	}

	// Start simulation first if not already running
	if (!GooBridgeMode->bSimulating)
	{
		GooBridgeMode->StartSimulation();
	}

	// Begin vehicle crossing animation
	bVehicleCrossing = true;
	VehiclePosX = VehicleStartX;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void UGooBridgeHUD::RefreshDisplay()
{
	if (!GooBridgeMode)
	{
		return;
	}

	if (GooCountText)
	{
		GooCountText->SetText(FText::FromString(
			FString::Printf(TEXT("Goo: %d"), GooBridgeMode->GooAvailable)));
	}

	if (LevelText)
	{
		LevelText->SetText(FText::FromString(
			FString::Printf(TEXT("Level %d"), GooBridgeMode->CurrentLevel)));
	}

	if (NodesRemainingText)
	{
		NodesRemainingText->SetText(FText::FromString(
			FString::Printf(TEXT("Nodes: %d"), GooBridgeMode->Nodes.Num())));
	}

	// Update simulate button label
	if (SimulateButton)
	{
		if (UTextBlock* BtnText = Cast<UTextBlock>(SimulateButton->GetChildAt(0)))
		{
			BtnText->SetText(FText::FromString(
				GooBridgeMode->bSimulating ? TEXT("Stop") : TEXT("Simulate")));
		}
	}
}

// ---------------------------------------------------------------------------

FVector2D UGooBridgeHUD::ScreenToGamePosition(const FVector2D& ScreenPos) const
{
	// Simple mapping from screen coordinates to game-world XY plane.
	// In a full implementation this would use a camera projection / deproject.
	// For now we use a basic centered offset + scale.
	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			FVector WorldOrigin, WorldDirection;
			if (PC->DeprojectScreenPositionToWorld(
					ScreenPos.X, ScreenPos.Y, WorldOrigin, WorldDirection))
			{
				// Intersect ray with the Z=0 play plane
				if (!FMath::IsNearlyZero(WorldDirection.Z))
				{
					const float T = -WorldOrigin.Z / WorldDirection.Z;
					const FVector HitPoint = WorldOrigin + WorldDirection * T;
					return FVector2D(HitPoint.X, HitPoint.Y);
				}
			}
		}
	}

	// Fallback: rough screen-to-world approximation
	return FVector2D(ScreenPos.X - 500.f, ScreenPos.Y - 300.f);
}
