// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/TowerDefense/TDHUD.h"
#include "Games/TowerDefense/TDGrid.h"
#include "Games/TowerDefense/TDGameMode.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "TimerManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogTDHUD, Log, All);

// -- TD Colors used outside Initialize() (wrapped to avoid unity build conflicts) --
namespace
{
const FLinearColor GrassColor(0.25f, 0.55f, 0.2f, 1.f);       // Green grass
const FLinearColor PathColor(0.55f, 0.4f, 0.25f, 1.f);        // Brown path
const FLinearColor TowerCellColor(0.35f, 0.35f, 0.5f, 1.f);   // Tower-placed cell
const FLinearColor ArrowColor(0.2f, 0.7f, 0.3f, 1.f);         // Green = arrow
const FLinearColor CannonColor(0.85f, 0.2f, 0.15f, 1.f);      // Red = cannon
const FLinearColor IceColor(0.3f, 0.6f, 0.95f, 1.f);          // Blue = ice
const FLinearColor LightningColor(0.9f, 0.85f, 0.2f, 1.f);    // Yellow = lightning
}

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

bool UTDHUD::Initialize()
{
	// Build base HUD widgets first (score, timer, pause, game over overlays).
	if (!Super::Initialize()) return false;

	// Guard against double-construction.
	if (TDCanvas)
	{
		return true;
	}

	// -- Colors ---------------------------------------------------------------
	const FLinearColor TDBgColor(0.08f, 0.12f, 0.08f, 0.95f);     // Dark green bg
	const FLinearColor TowerBtnHover(0.9f, 0.9f, 0.9f, 0.2f);
	const FLinearColor TowerBtnPress(0.5f, 0.5f, 0.5f, 0.3f);
	const FLinearColor StartWaveBtnColor(0.2f, 0.7f, 0.3f, 1.f);
	const FLinearColor StartWaveHover(0.3f, 0.8f, 0.4f, 1.f);
	const FLinearColor StartWavePress(0.15f, 0.55f, 0.2f, 1.f);
	const FLinearColor StatusBg(0.1f, 0.1f, 0.15f, 0.9f);

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
	const FSlateFontInfo AnnouncementFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 28);
	const FSlateFontInfo StatusFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 15);

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
		UBorder::StaticClass(), TEXT("TDGameArea"));
	GameArea->SetBrushColor(TDBgColor);
	GameArea->SetPadding(FMargin(8.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	// Main horizontal layout: [LEFT: grid area] [RIGHT: tower palette + status]
	UHorizontalBox* MainHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("TDMainHBox"));
	GameArea->SetContent(MainHBox);

	// Store canvas reference.
	TDCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("TDCanvas"));

	// ========================================================================
	// LEFT SIDE: GAME GRID (8x6)
	// ========================================================================
	UVerticalBox* GridVBox = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("GridVBox"));
	UHorizontalBoxSlot* GridHSlot = MainHBox->AddChildToHorizontalBox(GridVBox);
	GridHSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	GridHSlot->SetPadding(FMargin(4.f));

	{
		// Grid title
		UTextBlock* GridTitle = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("GridTitle"));
		GridTitle->SetText(FText::FromString(TEXT("BATTLEFIELD")));
		GridTitle->SetFont(HeaderFont);
		GridTitle->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.9f, 0.5f)));
		UVerticalBoxSlot* GTSlot = GridVBox->AddChildToVerticalBox(GridTitle);
		GTSlot->SetHorizontalAlignment(HAlign_Center);
		GTSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 4.f));

		// The grid is built as a UniformGridPanel with clickable cells.
		// We wrap it in a canvas panel so we can overlay enemy/tower images.
		GridOverlayCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("GridOverlayCanvas"));

		BoardGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(
			UUniformGridPanel::StaticClass(), TEXT("TDBoardGrid"));
		BoardGrid->SetSlotPadding(FMargin(1.f));

		// Add the grid to the overlay canvas.
		UCanvasPanelSlot* GridInCanvas = GridOverlayCanvas->AddChildToCanvas(BoardGrid);
		GridInCanvas->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		GridInCanvas->SetOffsets(FMargin(0.f));

		// Build 8x6 = 48 grid cells.
		const int32 TotalCells = GridCols * GridRows;
		GridCellWidgets.SetNum(TotalCells);
		GridCellButtons.SetNum(TotalCells);

		for (int32 row = 0; row < GridRows; ++row)
		{
			for (int32 col = 0; col < GridCols; ++col)
			{
				const int32 Idx = row * GridCols + col;

				USizeBox* CellSize = WidgetTree->ConstructWidget<USizeBox>(
					USizeBox::StaticClass(),
					*FString::Printf(TEXT("TDCellSize_%d_%d"), col, row));
				CellSize->SetMinDesiredWidth(VisualCellSize);
				CellSize->SetMinDesiredHeight(VisualCellSize);

				// Button for click handling.
				UButton* CellBtn = WidgetTree->ConstructWidget<UButton>(
					UButton::StaticClass(),
					*FString::Printf(TEXT("TDCellBtn_%d_%d"), col, row));
				// Make button background transparent; the border shows the color.
				FButtonStyle BtnStyle = CellBtn->GetStyle();
				BtnStyle.Normal.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.f));
				BtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f));
				BtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.3f));
				CellBtn->SetStyle(BtnStyle);

				UBorder* CellBorder = WidgetTree->ConstructWidget<UBorder>(
					UBorder::StaticClass(),
					*FString::Printf(TEXT("TDCell_%d_%d"), col, row));
				CellBorder->SetBrushColor(GrassColor); // Default to grass.
				CellBorder->SetPadding(FMargin(0.f));

				CellBorder->SetContent(CellBtn);
				CellSize->AddChild(CellBorder);
				BoardGrid->AddChildToUniformGrid(CellSize, row, col);

				GridCellWidgets[Idx] = CellBorder;
				GridCellButtons[Idx] = CellBtn;
			}
		}

		UVerticalBoxSlot* OverlaySlot = GridVBox->AddChildToVerticalBox(GridOverlayCanvas);
		OverlaySlot->SetHorizontalAlignment(HAlign_Center);
		OverlaySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		OverlaySlot->SetPadding(FMargin(0.f, 2.f));

		// Enemy count text below the grid.
		EnemyCountText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("EnemyCountText"));
		EnemyCountText->SetText(FText::GetEmpty());
		EnemyCountText->SetFont(BodyFont);
		EnemyCountText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.6f, 0.2f)));
		EnemyCountText->SetVisibility(ESlateVisibility::Collapsed);
		UVerticalBoxSlot* ECSlot = GridVBox->AddChildToVerticalBox(EnemyCountText);
		ECSlot->SetHorizontalAlignment(HAlign_Center);
		ECSlot->SetPadding(FMargin(0.f, 2.f));

		// Wave announcement text (centered overlay, initially hidden).
		WaveAnnouncementText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("WaveAnnouncementText"));
		WaveAnnouncementText->SetText(FText::GetEmpty());
		WaveAnnouncementText->SetFont(AnnouncementFont);
		WaveAnnouncementText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.8f, 0.2f)));
		WaveAnnouncementText->SetJustification(ETextJustify::Center);
		WaveAnnouncementText->SetVisibility(ESlateVisibility::Collapsed);
		UVerticalBoxSlot* WASlot = GridVBox->AddChildToVerticalBox(WaveAnnouncementText);
		WASlot->SetHorizontalAlignment(HAlign_Center);
		WASlot->SetPadding(FMargin(0.f, 4.f));
	}

	// ========================================================================
	// RIGHT SIDE: TOWER PALETTE + STATUS PANEL
	// ========================================================================
	UVerticalBox* SidePanel = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(), TEXT("TDSidePanel"));
	UHorizontalBoxSlot* SidePanelSlot = MainHBox->AddChildToHorizontalBox(SidePanel);
	SidePanelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
	SidePanelSlot->SetPadding(FMargin(8.f, 4.f, 4.f, 4.f));

	{
		// -- STATUS SECTION (Wave, Lives, Money) --
		UBorder* StatusBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("StatusBorder"));
		StatusBorder->SetBrushColor(StatusBg);
		StatusBorder->SetPadding(FMargin(10.f, 8.f));

		UVerticalBox* StatusVBox = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(), TEXT("StatusVBox"));
		StatusBorder->SetContent(StatusVBox);

		// Wave counter
		WaveText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("WaveText"));
		WaveText->SetText(FText::FromString(TEXT("Wave: 0")));
		WaveText->SetFont(StatusFont);
		WaveText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.8f, 0.2f)));
		UVerticalBoxSlot* WTSlot = StatusVBox->AddChildToVerticalBox(WaveText);
		WTSlot->SetPadding(FMargin(0.f, 2.f));

		// Lives counter
		LivesText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("LivesText"));
		LivesText->SetText(FText::FromString(TEXT("Lives: 20")));
		LivesText->SetFont(StatusFont);
		LivesText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.3f, 0.25f)));
		UVerticalBoxSlot* LTSlot = StatusVBox->AddChildToVerticalBox(LivesText);
		LTSlot->SetPadding(FMargin(0.f, 2.f));

		// Money counter
		MoneyText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("MoneyText"));
		MoneyText->SetText(FText::FromString(TEXT("Gold: 100")));
		MoneyText->SetFont(StatusFont);
		MoneyText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.f)));
		UVerticalBoxSlot* MTSlot = StatusVBox->AddChildToVerticalBox(MoneyText);
		MTSlot->SetPadding(FMargin(0.f, 2.f));

		UVerticalBoxSlot* SBSlot = SidePanel->AddChildToVerticalBox(StatusBorder);
		SBSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

		// -- TOWER SELECTION SECTION --
		UTextBlock* TowerHeader = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("TowerHeader"));
		TowerHeader->SetText(FText::FromString(TEXT("TOWERS")));
		TowerHeader->SetFont(HeaderFont);
		TowerHeader->SetColorAndOpacity(FSlateColor(FLinearColor(0.8f, 0.85f, 0.9f)));
		UVerticalBoxSlot* THSlot = SidePanel->AddChildToVerticalBox(TowerHeader);
		THSlot->SetHorizontalAlignment(HAlign_Center);
		THSlot->SetPadding(FMargin(0.f, 4.f, 0.f, 4.f));

		// Arrow Tower button
		ArrowTowerButton = MakeButton(TEXT("ArrowTowerButton"),
			TEXT("Arrow\n25g"), ArrowColor, StartWaveHover, StartWavePress, SmallFont);
		UVerticalBoxSlot* ATSlot = SidePanel->AddChildToVerticalBox(ArrowTowerButton);
		ATSlot->SetPadding(FMargin(0.f, 2.f));

		// Cannon Tower button
		CannonTowerButton = MakeButton(TEXT("CannonTowerButton"),
			TEXT("Cannon\n50g"), CannonColor,
			FLinearColor(0.95f, 0.3f, 0.25f), FLinearColor(0.7f, 0.15f, 0.1f), SmallFont);
		UVerticalBoxSlot* CTSlot = SidePanel->AddChildToVerticalBox(CannonTowerButton);
		CTSlot->SetPadding(FMargin(0.f, 2.f));

		// Ice Tower button
		IceTowerButton = MakeButton(TEXT("IceTowerButton"),
			TEXT("Ice\n35g"), IceColor,
			FLinearColor(0.4f, 0.7f, 1.f), FLinearColor(0.2f, 0.5f, 0.8f), SmallFont);
		UVerticalBoxSlot* ITSlot = SidePanel->AddChildToVerticalBox(IceTowerButton);
		ITSlot->SetPadding(FMargin(0.f, 2.f));

		// Lightning Tower button
		LightningTowerButton = MakeButton(TEXT("LightningTowerButton"),
			TEXT("Lightning\n60g"), LightningColor,
			FLinearColor(1.f, 0.95f, 0.3f), FLinearColor(0.75f, 0.7f, 0.15f), SmallFont);
		// Lightning label needs dark text on yellow background.
		if (UTextBlock* LightLbl = Cast<UTextBlock>(LightningTowerButton->GetChildAt(0)))
		{
			LightLbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.15f, 0.15f, 0.1f)));
		}
		UVerticalBoxSlot* LTBSlot = SidePanel->AddChildToVerticalBox(LightningTowerButton);
		LTBSlot->SetPadding(FMargin(0.f, 2.f));

		// Spacer
		USpacer* TowerSpacer = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("TowerSpacer"));
		UVerticalBoxSlot* TSSlot = SidePanel->AddChildToVerticalBox(TowerSpacer);
		TSSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// START WAVE button
		StartWaveButton = MakeButton(TEXT("StartWaveButton"),
			TEXT("START WAVE"), StartWaveBtnColor, StartWaveHover, StartWavePress, BtnFont);
		UVerticalBoxSlot* SWSlot = SidePanel->AddChildToVerticalBox(StartWaveButton);
		SWSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));
	}

	UE_LOG(LogTDHUD, Log,
		TEXT("TD HUD built: grid=%dx%d (%d cells), 4 tower btns"),
		GridCols, GridRows, GridCellWidgets.Num());

	return true;
}

void UTDHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Acquire the TD game mode from the base-class reference
	// (set by the PlayerController before NativeConstruct runs).
	TDMode = Cast<ATDGameMode>(GameModeRef);

	// Wire up the grid widget with the game mode (if present from Blueprint).
	if (GridWidget && TDMode)
	{
		GridWidget->SetGameMode(TDMode);
		GridWidget->InitializeGrid(TDMode->Grid, TDMode->GridWidth, TDMode->GridHeight);
	}

	// Wire tower selection buttons.
	if (ArrowTowerButton)     { ArrowTowerButton->OnClicked.AddDynamic(this, &UTDHUD::OnArrowSelected); }
	if (CannonTowerButton)    { CannonTowerButton->OnClicked.AddDynamic(this, &UTDHUD::OnCannonSelected); }
	if (IceTowerButton)       { IceTowerButton->OnClicked.AddDynamic(this, &UTDHUD::OnIceSelected); }
	if (LightningTowerButton) { LightningTowerButton->OnClicked.AddDynamic(this, &UTDHUD::OnLightningSelected); }
	if (StartWaveButton)      { StartWaveButton->OnClicked.AddDynamic(this, &UTDHUD::OnStartWaveClicked); }

	// Wire grid cell buttons (up to 48 cells for 8x6 grid).
	// We use a dispatch table since UFUNCTION callbacks cannot be bound by index.
	using CellFunc = void (UTDHUD::*)();
	static const CellFunc CellCallbacks[] = {
		&UTDHUD::OnGridCell0,  &UTDHUD::OnGridCell1,  &UTDHUD::OnGridCell2,  &UTDHUD::OnGridCell3,
		&UTDHUD::OnGridCell4,  &UTDHUD::OnGridCell5,  &UTDHUD::OnGridCell6,  &UTDHUD::OnGridCell7,
		&UTDHUD::OnGridCell8,  &UTDHUD::OnGridCell9,  &UTDHUD::OnGridCell10, &UTDHUD::OnGridCell11,
		&UTDHUD::OnGridCell12, &UTDHUD::OnGridCell13, &UTDHUD::OnGridCell14, &UTDHUD::OnGridCell15,
		&UTDHUD::OnGridCell16, &UTDHUD::OnGridCell17, &UTDHUD::OnGridCell18, &UTDHUD::OnGridCell19,
		&UTDHUD::OnGridCell20, &UTDHUD::OnGridCell21, &UTDHUD::OnGridCell22, &UTDHUD::OnGridCell23,
		&UTDHUD::OnGridCell24, &UTDHUD::OnGridCell25, &UTDHUD::OnGridCell26, &UTDHUD::OnGridCell27,
		&UTDHUD::OnGridCell28, &UTDHUD::OnGridCell29, &UTDHUD::OnGridCell30, &UTDHUD::OnGridCell31,
		&UTDHUD::OnGridCell32, &UTDHUD::OnGridCell33, &UTDHUD::OnGridCell34, &UTDHUD::OnGridCell35,
		&UTDHUD::OnGridCell36, &UTDHUD::OnGridCell37, &UTDHUD::OnGridCell38, &UTDHUD::OnGridCell39,
		&UTDHUD::OnGridCell40, &UTDHUD::OnGridCell41, &UTDHUD::OnGridCell42, &UTDHUD::OnGridCell43,
		&UTDHUD::OnGridCell44, &UTDHUD::OnGridCell45, &UTDHUD::OnGridCell46, &UTDHUD::OnGridCell47,
	};

	const int32 NumCallbacks = UE_ARRAY_COUNT(CellCallbacks);
	for (int32 i = 0; i < GridCellButtons.Num() && i < NumCallbacks; ++i)
	{
		if (GridCellButtons[i])
		{
			GridCellButtons[i]->OnClicked.AddDynamic(this, CellCallbacks[i]);
		}
	}

	// Hide the announcement text initially.
	if (WaveAnnouncementText)
	{
		WaveAnnouncementText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Initialize grid visuals from game mode data.
	if (TDMode)
	{
		RefreshGridCells();

		if (WaveText)
		{
			WaveText->SetText(FText::FromString(
				FString::Printf(TEXT("Wave: %d / %d"), TDMode->Wave, TDMode->MaxWaves)));
		}
		if (LivesText)
		{
			LivesText->SetText(FText::FromString(
				FString::Printf(TEXT("Lives: %d"), TDMode->Lives)));
		}
		if (MoneyText)
		{
			MoneyText->SetText(FText::FromString(
				FString::Printf(TEXT("Gold: %d"), TDMode->Gold)));
		}
	}

	LastWave = 0;
}

void UTDHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!TDMode || !TDMode->bGameActive)
	{
		return;
	}

	// Update status displays.
	if (WaveText)
	{
		WaveText->SetText(FText::FromString(
			FString::Printf(TEXT("Wave: %d / %d"), TDMode->Wave, TDMode->MaxWaves)));
	}
	if (LivesText)
	{
		LivesText->SetText(FText::FromString(
			FString::Printf(TEXT("Lives: %d"), TDMode->Lives)));

		// Color based on remaining lives.
		if (TDMode->Lives <= 3)
		{
			LivesText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.15f, 0.1f)));
		}
		else if (TDMode->Lives <= 8)
		{
			LivesText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.6f, 0.2f)));
		}
		else
		{
			LivesText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.3f, 0.25f)));
		}
	}
	if (MoneyText)
	{
		MoneyText->SetText(FText::FromString(
			FString::Printf(TEXT("Gold: %d"), TDMode->Gold)));
	}

	// Update enemy count display.
	if (EnemyCountText)
	{
		if (TDMode->bWaveActive)
		{
			EnemyCountText->SetText(FText::FromString(
				FString::Printf(TEXT("Enemies: %d"), TDMode->Enemies.Num())));
			EnemyCountText->SetVisibility(ESlateVisibility::Visible);
		}
		else
		{
			EnemyCountText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Enable/disable start wave button based on wave state.
	if (StartWaveButton)
	{
		StartWaveButton->SetIsEnabled(!TDMode->bWaveActive);
	}

	// Detect wave transitions for announcements.
	if (TDMode->Wave != LastWave && TDMode->bWaveActive)
	{
		LastWave = TDMode->Wave;
		ShowWaveAnnouncement(LastWave);
	}

	// Low lives warning.
	if (TDMode->Lives > 0 && TDMode->Lives <= 3)
	{
		ShowLivesWarning();
	}

	// Refresh grid cell colors (to show newly placed towers).
	RefreshGridCells();

	// Update enemy visuals (positions on the grid overlay).
	UpdateEnemyVisuals();

	// Update tower visuals.
	UpdateTowerVisuals();

	// Check for victory (handled by game mode EndGame, but we can show a special screen).
	if (!TDMode->bGameActive && TDMode->Wave >= TDMode->MaxWaves && TDMode->Lives > 0)
	{
		ShowVictory();
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UTDHUD::ShowWaveAnnouncement(int32 WaveNumber)
{
	if (!WaveAnnouncementText)
	{
		return;
	}

	WaveAnnouncementText->SetText(FText::FromString(
		FString::Printf(TEXT("Wave %d"), WaveNumber)));
	WaveAnnouncementText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.8f, 0.2f, 1.f)));
	WaveAnnouncementText->SetVisibility(ESlateVisibility::Visible);

	// Clear announcement after 2 seconds.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(AnnouncementTimerHandle);

		World->GetTimerManager().SetTimer(AnnouncementTimerHandle, [this]()
		{
			if (WaveAnnouncementText)
			{
				WaveAnnouncementText->SetVisibility(ESlateVisibility::Collapsed);
			}
		}, 2.f, false);
	}
}

void UTDHUD::ShowLivesWarning()
{
	if (WaveAnnouncementText && WaveAnnouncementText->GetVisibility() == ESlateVisibility::Collapsed)
	{
		// Flash a warning briefly. Reuse the announcement text.
		WaveAnnouncementText->SetText(FText::FromString(TEXT("LOW LIVES!")));
		WaveAnnouncementText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.1f, 0.1f, 1.f)));
		WaveAnnouncementText->SetVisibility(ESlateVisibility::Visible);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(AnnouncementTimerHandle);

			World->GetTimerManager().SetTimer(AnnouncementTimerHandle, [this]()
			{
				if (WaveAnnouncementText)
				{
					WaveAnnouncementText->SetVisibility(ESlateVisibility::Collapsed);
				}
			}, 1.5f, false);
		}
	}
}

void UTDHUD::ShowVictory()
{
	if (WaveAnnouncementText)
	{
		WaveAnnouncementText->SetText(FText::FromString(TEXT("VICTORY!")));
		WaveAnnouncementText->SetColorAndOpacity(FSlateColor(FLinearColor(0.2f, 1.f, 0.4f, 1.f)));
		WaveAnnouncementText->SetVisibility(ESlateVisibility::Visible);
	}
}

// ---------------------------------------------------------------------------
// Tower button callbacks
// ---------------------------------------------------------------------------

void UTDHUD::OnArrowSelected()     { SelectedTowerType = EPHCTowerType::Arrow; }
void UTDHUD::OnCannonSelected()    { SelectedTowerType = EPHCTowerType::Cannon; }
void UTDHUD::OnIceSelected()       { SelectedTowerType = EPHCTowerType::Ice; }
void UTDHUD::OnLightningSelected() { SelectedTowerType = EPHCTowerType::Lightning; }

void UTDHUD::OnStartWaveClicked()
{
	if (TDMode && !TDMode->bWaveActive)
	{
		TDMode->StartWave();
	}
}

// ---------------------------------------------------------------------------
// Grid cell click callbacks (48 cells for 8x6)
// ---------------------------------------------------------------------------

void UTDHUD::OnGridCell0()  { HandleGridCellClicked(0);  }
void UTDHUD::OnGridCell1()  { HandleGridCellClicked(1);  }
void UTDHUD::OnGridCell2()  { HandleGridCellClicked(2);  }
void UTDHUD::OnGridCell3()  { HandleGridCellClicked(3);  }
void UTDHUD::OnGridCell4()  { HandleGridCellClicked(4);  }
void UTDHUD::OnGridCell5()  { HandleGridCellClicked(5);  }
void UTDHUD::OnGridCell6()  { HandleGridCellClicked(6);  }
void UTDHUD::OnGridCell7()  { HandleGridCellClicked(7);  }
void UTDHUD::OnGridCell8()  { HandleGridCellClicked(8);  }
void UTDHUD::OnGridCell9()  { HandleGridCellClicked(9);  }
void UTDHUD::OnGridCell10() { HandleGridCellClicked(10); }
void UTDHUD::OnGridCell11() { HandleGridCellClicked(11); }
void UTDHUD::OnGridCell12() { HandleGridCellClicked(12); }
void UTDHUD::OnGridCell13() { HandleGridCellClicked(13); }
void UTDHUD::OnGridCell14() { HandleGridCellClicked(14); }
void UTDHUD::OnGridCell15() { HandleGridCellClicked(15); }
void UTDHUD::OnGridCell16() { HandleGridCellClicked(16); }
void UTDHUD::OnGridCell17() { HandleGridCellClicked(17); }
void UTDHUD::OnGridCell18() { HandleGridCellClicked(18); }
void UTDHUD::OnGridCell19() { HandleGridCellClicked(19); }
void UTDHUD::OnGridCell20() { HandleGridCellClicked(20); }
void UTDHUD::OnGridCell21() { HandleGridCellClicked(21); }
void UTDHUD::OnGridCell22() { HandleGridCellClicked(22); }
void UTDHUD::OnGridCell23() { HandleGridCellClicked(23); }
void UTDHUD::OnGridCell24() { HandleGridCellClicked(24); }
void UTDHUD::OnGridCell25() { HandleGridCellClicked(25); }
void UTDHUD::OnGridCell26() { HandleGridCellClicked(26); }
void UTDHUD::OnGridCell27() { HandleGridCellClicked(27); }
void UTDHUD::OnGridCell28() { HandleGridCellClicked(28); }
void UTDHUD::OnGridCell29() { HandleGridCellClicked(29); }
void UTDHUD::OnGridCell30() { HandleGridCellClicked(30); }
void UTDHUD::OnGridCell31() { HandleGridCellClicked(31); }
void UTDHUD::OnGridCell32() { HandleGridCellClicked(32); }
void UTDHUD::OnGridCell33() { HandleGridCellClicked(33); }
void UTDHUD::OnGridCell34() { HandleGridCellClicked(34); }
void UTDHUD::OnGridCell35() { HandleGridCellClicked(35); }
void UTDHUD::OnGridCell36() { HandleGridCellClicked(36); }
void UTDHUD::OnGridCell37() { HandleGridCellClicked(37); }
void UTDHUD::OnGridCell38() { HandleGridCellClicked(38); }
void UTDHUD::OnGridCell39() { HandleGridCellClicked(39); }
void UTDHUD::OnGridCell40() { HandleGridCellClicked(40); }
void UTDHUD::OnGridCell41() { HandleGridCellClicked(41); }
void UTDHUD::OnGridCell42() { HandleGridCellClicked(42); }
void UTDHUD::OnGridCell43() { HandleGridCellClicked(43); }
void UTDHUD::OnGridCell44() { HandleGridCellClicked(44); }
void UTDHUD::OnGridCell45() { HandleGridCellClicked(45); }
void UTDHUD::OnGridCell46() { HandleGridCellClicked(46); }
void UTDHUD::OnGridCell47() { HandleGridCellClicked(47); }

void UTDHUD::HandleGridCellClicked(int32 Index)
{
	if (!TDMode)
	{
		return;
	}

	// Convert flat index to grid coordinates.
	const int32 Col = Index % GridCols;
	const int32 Row = Index / GridCols;

	// The game mode uses Y * GridWidth + X format, and its grid may be
	// larger than our visual grid. Map to its coordinate space.
	const FIntPoint GridPos(Col, Row);

	// Attempt to place the selected tower.
	if (TDMode->PlaceTower(GridPos, SelectedTowerType))
	{
		UE_LOG(LogTDHUD, Log, TEXT("Tower placed at (%d,%d) type=%d"),
			Col, Row, static_cast<int32>(SelectedTowerType));
		RefreshGridCells();
	}
}

// ---------------------------------------------------------------------------
// Helpers - colors
// ---------------------------------------------------------------------------

FLinearColor UTDHUD::GetCellColor(int32 CellValue) const
{
	switch (CellValue)
	{
	case 0:  return GrassColor;      // Empty / buildable
	case 1:  return PathColor;       // Path
	case 2:  return TowerCellColor;  // Tower placed
	default: return GrassColor;
	}
}

FLinearColor UTDHUD::GetTowerColor(EPHCTowerType Type) const
{
	switch (Type)
	{
	case EPHCTowerType::Arrow:     return ArrowColor;
	case EPHCTowerType::Cannon:    return CannonColor;
	case EPHCTowerType::Ice:       return IceColor;
	case EPHCTowerType::Lightning: return LightningColor;
	default:                       return FLinearColor::White;
	}
}

FLinearColor UTDHUD::GetEnemyColor(EPHCEnemyType Type) const
{
	switch (Type)
	{
	case EPHCEnemyType::Basic: return FLinearColor(0.9f, 0.4f, 0.1f, 1.f);   // Orange
	case EPHCEnemyType::Fast:  return FLinearColor(0.9f, 0.9f, 0.2f, 1.f);   // Yellow
	case EPHCEnemyType::Tank:  return FLinearColor(0.5f, 0.5f, 0.55f, 1.f);  // Gray
	case EPHCEnemyType::Boss:  return FLinearColor(0.8f, 0.1f, 0.6f, 1.f);   // Purple
	default:                   return FLinearColor::White;
	}
}

// ---------------------------------------------------------------------------
// Helpers - visual updates
// ---------------------------------------------------------------------------

void UTDHUD::RefreshGridCells()
{
	if (!TDMode || GridCellWidgets.Num() == 0)
	{
		return;
	}

	for (int32 row = 0; row < GridRows; ++row)
	{
		for (int32 col = 0; col < GridCols; ++col)
		{
			const int32 VisIdx = row * GridCols + col;
			if (!GridCellWidgets.IsValidIndex(VisIdx) || !GridCellWidgets[VisIdx])
			{
				continue;
			}

			// Map to the game mode grid (which may have different dimensions).
			const int32 GMIdx = row * TDMode->GridWidth + col;
			int32 CellVal = 0;
			if (TDMode->Grid.IsValidIndex(GMIdx))
			{
				CellVal = TDMode->Grid[GMIdx];
			}

			GridCellWidgets[VisIdx]->SetBrushColor(GetCellColor(CellVal));
		}
	}
}

void UTDHUD::UpdateEnemyVisuals()
{
	if (!TDMode || !GridOverlayCanvas)
	{
		return;
	}

	// Enemy rendering is handled primarily by UTDGrid when it is present.
	// For the programmatic HUD, we update simple colored images.
	// (Enemy images would need to be positioned using canvas panel slots,
	// which requires careful coordinate mapping. For now we rely on the
	// UTDGrid sub-widget for full enemy rendering when available.)
}

void UTDHUD::UpdateTowerVisuals()
{
	// Tower cell colors are already reflected by RefreshGridCells().
	// Detailed tower rendering is handled by UTDGrid when available.
}

void UTDHUD::EnsureEnemyImagePool(int32 Count)
{
	// Reserve additional enemy images as needed.
	while (EnemyImages.Num() < Count)
	{
		UImage* Img = NewObject<UImage>(this);
		if (Img)
		{
			Img->SetVisibility(ESlateVisibility::Collapsed);
			EnemyImages.Add(Img);
		}
	}
}
