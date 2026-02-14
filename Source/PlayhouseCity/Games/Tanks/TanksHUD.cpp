// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Tanks/TanksHUD.h"
#include "Games/Tanks/TanksGameMode.h"
#include "Games/Tanks/TankPawn.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogTanksHUD, Log, All);

// -- Colour palette ---------------------------------------------------------

static const FLinearColor FieldGreen(0.06f, 0.22f, 0.06f, 1.f);      // dark green field
static const FLinearColor TerrainGreen(0.08f, 0.35f, 0.08f, 1.f);     // terrain silhouette
static const FLinearColor SkyGradient(0.15f, 0.25f, 0.45f, 1.f);      // sky background tint
static const FLinearColor PlayerBlue(0.15f, 0.35f, 0.85f, 1.f);       // blue player tank
static const FLinearColor PlayerTurretBlue(0.25f, 0.50f, 0.95f, 1.f); // lighter turret
static const FLinearColor EnemyRed(0.85f, 0.15f, 0.15f, 1.f);         // red enemy tank
static const FLinearColor EnemyTurretRed(0.95f, 0.30f, 0.20f, 1.f);   // lighter turret
static const FLinearColor HPGreen(0.2f, 0.85f, 0.2f, 1.f);            // health bar fill
static const FLinearColor HPBg(0.15f, 0.15f, 0.15f, 1.f);             // health bar background
static const FLinearColor BtnNormal(0.2f, 0.3f, 0.7f, 1.f);
static const FLinearColor BtnHover(0.3f, 0.4f, 0.85f, 1.f);
static const FLinearColor BtnPress(0.15f, 0.2f, 0.5f, 1.f);
static const FLinearColor FireBtnNormal(0.85f, 0.30f, 0.10f, 1.f);
static const FLinearColor FireBtnHover(0.95f, 0.40f, 0.15f, 1.f);
static const FLinearColor FireBtnPress(0.65f, 0.20f, 0.08f, 1.f);
static const FLinearColor ControlBg(0.05f, 0.05f, 0.1f, 0.85f);

// ---------------------------------------------------------------------------
// Initialize - build the Tanks game UI
// ---------------------------------------------------------------------------

bool UTanksHUD::Initialize()
{
	// Build base HUD widgets first (score, timer, pause, game over overlays).
	if (!Super::Initialize())
	{
		return false;
	}

	// Guard against double-build.
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
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);
	const FSlateFontInfo ValueFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo BtnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);
	const FSlateFontInfo TurnFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
	const FSlateFontInfo WindFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 13);

	// -- Helper: styled button with label -----------------------------------
	auto MakeButton = [&](const FName& Name, const FString& Label,
						   FLinearColor NormalCol, FLinearColor HoverCol,
						   FLinearColor PressCol, const FSlateFontInfo& Font) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor  = FSlateColor(NormalCol);
		Style.Hovered.TintColor = FSlateColor(HoverCol);
		Style.Pressed.TintColor = FSlateColor(PressCol);
		Btn->SetStyle(Style);

		UTextBlock* BtnLbl = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("Lbl"))));
		BtnLbl->SetText(FText::FromString(Label));
		BtnLbl->SetFont(Font);
		BtnLbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BtnLbl->SetJustification(ETextJustify::Center);
		Btn->AddChild(BtnLbl);
		return Btn;
	};

	// ===================================================================
	// GAME AREA  (below top bar, fills remaining screen)
	// ===================================================================
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("TanksGameArea"));
	GameArea->SetBrushColor(SkyGradient);
	GameArea->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* GameAreaSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameAreaSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameAreaSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f));

	// Inner canvas for absolute positioning of tanks, terrain, etc.
	GameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("GameCanvas"));
	GameArea->SetContent(GameCanvas);

	// ===================================================================
	// TERRAIN SILHOUETTE (bottom portion of battlefield)
	// ===================================================================
	TerrainBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("TerrainBorder"));
	TerrainBorder->SetBrushColor(TerrainGreen);

	UCanvasPanelSlot* TerrainSlot = GameCanvas->AddChildToCanvas(TerrainBorder);
	TerrainSlot->SetAnchors(FAnchors(0.f, 0.7f, 1.f, 1.f));
	TerrainSlot->SetOffsets(FMargin(0.f));

	// Dark-green ground fill below the terrain line.
	UBorder* GroundFill = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("GroundFill"));
	GroundFill->SetBrushColor(FieldGreen);

	UCanvasPanelSlot* GroundSlot = GameCanvas->AddChildToCanvas(GroundFill);
	GroundSlot->SetAnchors(FAnchors(0.f, 0.82f, 1.f, 1.f));
	GroundSlot->SetOffsets(FMargin(0.f));

	// ===================================================================
	// PLAYER TANK (left side)
	// ===================================================================
	PlayerTankWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("PlayerTankWidget"));
	PlayerTankWidget->SetBrushColor(PlayerBlue);

	UCanvasPanelSlot* PTankSlot = GameCanvas->AddChildToCanvas(PlayerTankWidget);
	PTankSlot->SetAnchors(FAnchors(0.12f, 0.62f, 0.12f, 0.62f));
	PTankSlot->SetOffsets(FMargin(-25.f, -15.f, 50.f, 30.f)); // size: 50x30

	// Player turret (smaller, overlaid on top of body).
	PlayerTurretWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("PlayerTurretWidget"));
	PlayerTurretWidget->SetBrushColor(PlayerTurretBlue);

	UCanvasPanelSlot* PTurretSlot = GameCanvas->AddChildToCanvas(PlayerTurretWidget);
	PTurretSlot->SetAnchors(FAnchors(0.12f, 0.60f, 0.12f, 0.60f));
	PTurretSlot->SetOffsets(FMargin(-5.f, -5.f, 35.f, 10.f)); // size: 35x10

	// ===================================================================
	// ENEMY TANK (right side)
	// ===================================================================
	EnemyTankWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("EnemyTankWidget"));
	EnemyTankWidget->SetBrushColor(EnemyRed);

	UCanvasPanelSlot* ETankSlot = GameCanvas->AddChildToCanvas(EnemyTankWidget);
	ETankSlot->SetAnchors(FAnchors(0.85f, 0.62f, 0.85f, 0.62f));
	ETankSlot->SetOffsets(FMargin(-25.f, -15.f, 50.f, 30.f));

	EnemyTurretWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("EnemyTurretWidget"));
	EnemyTurretWidget->SetBrushColor(EnemyTurretRed);

	UCanvasPanelSlot* ETurretSlot = GameCanvas->AddChildToCanvas(EnemyTurretWidget);
	ETurretSlot->SetAnchors(FAnchors(0.85f, 0.60f, 0.85f, 0.60f));
	ETurretSlot->SetOffsets(FMargin(-30.f, -5.f, 35.f, 10.f));

	// ===================================================================
	// PLAYER HEALTH BAR (above player tank)
	// ===================================================================
	PlayerHPBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("PlayerHPBar"));
	PlayerHPBar->SetPercent(1.f);
	PlayerHPBar->SetFillColorAndOpacity(HPGreen);

	UCanvasPanelSlot* PHPSlot = GameCanvas->AddChildToCanvas(PlayerHPBar);
	PHPSlot->SetAnchors(FAnchors(0.06f, 0.52f, 0.06f, 0.52f));
	PHPSlot->SetOffsets(FMargin(0.f, 0.f, 70.f, 12.f));

	// Small label above health bar.
	UTextBlock* PlayerHPLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PlayerHPLabel"));
	PlayerHPLabel->SetText(FText::FromString(TEXT("PLAYER")));
	PlayerHPLabel->SetFont(WindFont);
	PlayerHPLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	UCanvasPanelSlot* PHPLblSlot = GameCanvas->AddChildToCanvas(PlayerHPLabel);
	PHPLblSlot->SetAnchors(FAnchors(0.06f, 0.48f, 0.06f, 0.48f));
	PHPLblSlot->SetOffsets(FMargin(0.f, 0.f, 70.f, 16.f));

	// ===================================================================
	// ENEMY HEALTH BAR (above enemy tank)
	// ===================================================================
	EnemyHPBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("EnemyHPBar"));
	EnemyHPBar->SetPercent(1.f);
	EnemyHPBar->SetFillColorAndOpacity(FLinearColor(0.85f, 0.2f, 0.2f, 1.f));

	UCanvasPanelSlot* EHPSlot = GameCanvas->AddChildToCanvas(EnemyHPBar);
	EHPSlot->SetAnchors(FAnchors(0.80f, 0.52f, 0.80f, 0.52f));
	EHPSlot->SetOffsets(FMargin(0.f, 0.f, 70.f, 12.f));

	UTextBlock* EnemyHPLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("EnemyHPLabel"));
	EnemyHPLabel->SetText(FText::FromString(TEXT("ENEMY")));
	EnemyHPLabel->SetFont(WindFont);
	EnemyHPLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.6f, 0.6f)));

	UCanvasPanelSlot* EHPLblSlot = GameCanvas->AddChildToCanvas(EnemyHPLabel);
	EHPLblSlot->SetAnchors(FAnchors(0.80f, 0.48f, 0.80f, 0.48f));
	EHPLblSlot->SetOffsets(FMargin(0.f, 0.f, 70.f, 16.f));

	// ===================================================================
	// WIND INDICATOR (top-centre of game area)
	// ===================================================================
	WindText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WindText"));
	WindText->SetText(FText::FromString(TEXT("Wind: -- 0 mph")));
	WindText->SetFont(WindFont);
	WindText->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.9f, 1.f)));
	WindText->SetJustification(ETextJustify::Center);

	UCanvasPanelSlot* WindSlot = GameCanvas->AddChildToCanvas(WindText);
	WindSlot->SetAnchors(FAnchors(0.5f, 0.02f, 0.5f, 0.02f));
	WindSlot->SetOffsets(FMargin(-80.f, 0.f, 160.f, 20.f));
	WindSlot->SetAlignment(FVector2D(0.f, 0.f));

	// ===================================================================
	// TURN INDICATOR (just below wind)
	// ===================================================================
	TurnIndicatorText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TurnIndicatorText"));
	TurnIndicatorText->SetText(FText::FromString(TEXT("YOUR TURN")));
	TurnIndicatorText->SetFont(TurnFont);
	TurnIndicatorText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	TurnIndicatorText->SetJustification(ETextJustify::Center);

	UCanvasPanelSlot* TurnSlot = GameCanvas->AddChildToCanvas(TurnIndicatorText);
	TurnSlot->SetAnchors(FAnchors(0.5f, 0.07f, 0.5f, 0.07f));
	TurnSlot->SetOffsets(FMargin(-90.f, 0.f, 180.f, 26.f));
	TurnSlot->SetAlignment(FVector2D(0.f, 0.f));

	// ===================================================================
	// ENEMIES REMAINING (top right)
	// ===================================================================
	CPURemainingText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("CPURemainingText"));
	CPURemainingText->SetText(FText::FromString(TEXT("Enemies: 1")));
	CPURemainingText->SetFont(LabelFont);
	CPURemainingText->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.6f, 0.6f)));

	UCanvasPanelSlot* CPURSlot = GameCanvas->AddChildToCanvas(CPURemainingText);
	CPURSlot->SetAnchors(FAnchors(0.88f, 0.02f, 0.88f, 0.02f));
	CPURSlot->SetOffsets(FMargin(0.f, 0.f, 120.f, 20.f));

	// ===================================================================
	// CONTROL PANEL  (bottom bar: angle, power, fire)
	// ===================================================================
	UBorder* ControlBar = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ControlBar"));
	ControlBar->SetBrushColor(ControlBg);
	ControlBar->SetPadding(FMargin(12.f, 6.f));

	UCanvasPanelSlot* CtrlSlot = RootCanvas->AddChildToCanvas(ControlBar);
	CtrlSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
	CtrlSlot->SetOffsets(FMargin(0.f, -70.f, 0.f, 70.f));

	UHorizontalBox* CtrlHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CtrlHBox"));
	ControlBar->SetContent(CtrlHBox);

	// -- Angle controls --------------------------------------------------
	UTextBlock* AngleLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AngleLabel"));
	AngleLabel->SetText(FText::FromString(TEXT("Angle:")));
	AngleLabel->SetFont(LabelFont);
	AngleLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UHorizontalBoxSlot* ALblSlot = CtrlHBox->AddChildToHorizontalBox(AngleLabel);
	ALblSlot->SetVerticalAlignment(VAlign_Center);
	ALblSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));

	AngleDownButton = MakeButton(TEXT("AngleDownButton"), TEXT("-"),
		BtnNormal, BtnHover, BtnPress, BtnFont);
	UHorizontalBoxSlot* ADBSlot = CtrlHBox->AddChildToHorizontalBox(AngleDownButton);
	ADBSlot->SetVerticalAlignment(VAlign_Center);
	ADBSlot->SetPadding(FMargin(2.f, 0.f));

	AngleValueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AngleValueText"));
	AngleValueText->SetText(FText::FromString(TEXT("45")));
	AngleValueText->SetFont(ValueFont);
	AngleValueText->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.9f, 0.3f)));
	AngleValueText->SetJustification(ETextJustify::Center);
	UHorizontalBoxSlot* AVSlot = CtrlHBox->AddChildToHorizontalBox(AngleValueText);
	AVSlot->SetVerticalAlignment(VAlign_Center);
	AVSlot->SetPadding(FMargin(4.f, 0.f));

	AngleUpButton = MakeButton(TEXT("AngleUpButton"), TEXT("+"),
		BtnNormal, BtnHover, BtnPress, BtnFont);
	UHorizontalBoxSlot* AUBSlot = CtrlHBox->AddChildToHorizontalBox(AngleUpButton);
	AUBSlot->SetVerticalAlignment(VAlign_Center);
	AUBSlot->SetPadding(FMargin(2.f, 0.f));

	// Spacer between angle and power.
	USpacer* Sp1 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("CtrlSp1"));
	UHorizontalBoxSlot* Sp1Slot = CtrlHBox->AddChildToHorizontalBox(Sp1);
	Sp1Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// -- Power controls ---------------------------------------------------
	UTextBlock* PowerLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PowerLabel"));
	PowerLabel->SetText(FText::FromString(TEXT("Power:")));
	PowerLabel->SetFont(LabelFont);
	PowerLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UHorizontalBoxSlot* PLblSlot = CtrlHBox->AddChildToHorizontalBox(PowerLabel);
	PLblSlot->SetVerticalAlignment(VAlign_Center);
	PLblSlot->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));

	PowerDownButton = MakeButton(TEXT("PowerDownButton"), TEXT("-"),
		BtnNormal, BtnHover, BtnPress, BtnFont);
	UHorizontalBoxSlot* PDBSlot = CtrlHBox->AddChildToHorizontalBox(PowerDownButton);
	PDBSlot->SetVerticalAlignment(VAlign_Center);
	PDBSlot->SetPadding(FMargin(2.f, 0.f));

	// Power bar (visual).
	USizeBox* PowerSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("PowerSizeBox"));
	PowerSizeBox->SetMinDesiredWidth(100.f);
	PowerSizeBox->SetMinDesiredHeight(20.f);

	PowerBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("PowerBar"));
	PowerBar->SetPercent(0.5f);
	PowerBar->SetFillColorAndOpacity(FLinearColor(1.f, 0.6f, 0.1f, 1.f));
	PowerSizeBox->AddChild(PowerBar);

	UHorizontalBoxSlot* PBarSlot = CtrlHBox->AddChildToHorizontalBox(PowerSizeBox);
	PBarSlot->SetVerticalAlignment(VAlign_Center);
	PBarSlot->SetPadding(FMargin(4.f, 0.f));

	PowerValueText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PowerValueText"));
	PowerValueText->SetText(FText::FromString(TEXT("50%")));
	PowerValueText->SetFont(ValueFont);
	PowerValueText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.7f, 0.2f)));
	UHorizontalBoxSlot* PVSlot = CtrlHBox->AddChildToHorizontalBox(PowerValueText);
	PVSlot->SetVerticalAlignment(VAlign_Center);
	PVSlot->SetPadding(FMargin(4.f, 0.f));

	PowerUpButton = MakeButton(TEXT("PowerUpButton"), TEXT("+"),
		BtnNormal, BtnHover, BtnPress, BtnFont);
	UHorizontalBoxSlot* PUBSlot = CtrlHBox->AddChildToHorizontalBox(PowerUpButton);
	PUBSlot->SetVerticalAlignment(VAlign_Center);
	PUBSlot->SetPadding(FMargin(2.f, 0.f));

	// Spacer before fire.
	USpacer* Sp2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("CtrlSp2"));
	UHorizontalBoxSlot* Sp2Slot = CtrlHBox->AddChildToHorizontalBox(Sp2);
	Sp2Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// -- FIRE button ------------------------------------------------------
	FireButton = MakeButton(TEXT("FireButton"), TEXT("FIRE"),
		FireBtnNormal, FireBtnHover, FireBtnPress, TurnFont);

	USizeBox* FireSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("FireSizeBox"));
	FireSizeBox->SetMinDesiredWidth(90.f);
	FireSizeBox->SetMinDesiredHeight(44.f);
	FireSizeBox->AddChild(FireButton);

	UHorizontalBoxSlot* FireSlot = CtrlHBox->AddChildToHorizontalBox(FireSizeBox);
	FireSlot->SetVerticalAlignment(VAlign_Center);
	FireSlot->SetPadding(FMargin(8.f, 0.f));

	UE_LOG(LogTanksHUD, Log,
		TEXT("Tanks HUD built: battlefield, 2 tanks, HP bars, angle/power controls, fire button."));

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct - wire up delegates
// ---------------------------------------------------------------------------

void UTanksHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Cast from the base-class reference (set by the PlayerController
	// before NativeConstruct runs).
	TanksMode = Cast<ATanksGameMode>(GameModeRef);

	if (StatusText)
	{
		StatusText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Wire button callbacks.
	if (AngleUpButton)
	{
		AngleUpButton->OnClicked.AddDynamic(this, &UTanksHUD::OnAngleUp);
	}
	if (AngleDownButton)
	{
		AngleDownButton->OnClicked.AddDynamic(this, &UTanksHUD::OnAngleDown);
	}
	if (PowerUpButton)
	{
		PowerUpButton->OnClicked.AddDynamic(this, &UTanksHUD::OnPowerUp);
	}
	if (PowerDownButton)
	{
		PowerDownButton->OnClicked.AddDynamic(this, &UTanksHUD::OnPowerDown);
	}
	if (FireButton)
	{
		FireButton->OnClicked.AddDynamic(this, &UTanksHUD::OnFireClicked);
	}

	SetIsFocusable(true);
}

// ---------------------------------------------------------------------------
// NativeTick - update dynamic HUD elements
// ---------------------------------------------------------------------------

void UTanksHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!TanksMode)
	{
		return;
	}

	// Update player HP bar.
	if (PlayerHPBar && TanksMode->PlayerTank)
	{
		const float HPFrac = static_cast<float>(TanksMode->PlayerTank->HP)
			/ static_cast<float>(TanksMode->PlayerTank->MaxHP);
		PlayerHPBar->SetPercent(FMath::Clamp(HPFrac, 0.f, 1.f));
	}

	// Update enemy HP bar (show first CPU tank's health).
	if (EnemyHPBar && TanksMode->CPUTanks.Num() > 0)
	{
		float EnemyHP = 0.f;
		float EnemyMaxHP = 1.f;
		for (const TObjectPtr<ATankPawn>& CPU : TanksMode->CPUTanks)
		{
			if (CPU && CPU->HP > 0)
			{
				EnemyHP = static_cast<float>(CPU->HP);
				EnemyMaxHP = static_cast<float>(CPU->MaxHP);
				break;
			}
		}
		EnemyHPBar->SetPercent(FMath::Clamp(EnemyHP / EnemyMaxHP, 0.f, 1.f));
	}

	// Update CPU remaining text.
	if (CPURemainingText)
	{
		CPURemainingText->SetText(FText::FromString(
			FString::Printf(TEXT("Enemies: %d"), TanksMode->CPUTanksRemaining)));
	}

	// Update angle/power display.
	if (AngleValueText)
	{
		AngleValueText->SetText(FText::FromString(
			FString::Printf(TEXT("%d"), FMath::RoundToInt32(AimAngleDeg))));
	}
	if (PowerValueText)
	{
		PowerValueText->SetText(FText::FromString(
			FString::Printf(TEXT("%d%%"), FMath::RoundToInt32(AimPower * 100.f))));
	}
	if (PowerBar)
	{
		PowerBar->SetPercent(AimPower);
	}

	// Turn indicator.
	if (TurnIndicatorText)
	{
		const bool bActive = TanksMode->bGameActive;
		TurnIndicatorText->SetText(FText::FromString(
			bActive ? TEXT("YOUR TURN - AIM & FIRE!") : TEXT("GAME OVER")));
	}
}

// ---------------------------------------------------------------------------
// Input handlers
// ---------------------------------------------------------------------------

FReply UTanksHUD::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (TanksMode && TanksMode->bGameActive)
	{
		const FVector2D ArenaPos = ScreenToArena(InGeometry, InMouseEvent.GetScreenSpacePosition());
		TanksMode->AimPlayerTurret(ArenaPos);
	}
	return FReply::Handled();
}

FReply UTanksHUD::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (TanksMode && TanksMode->bGameActive)
	{
		TanksMode->PlayerFire();
	}
	return FReply::Handled();
}

FReply UTanksHUD::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	if (TanksMode && TanksMode->bGameActive)
	{
		const FVector2D ArenaPos = ScreenToArena(InGeometry, InTouchEvent.GetScreenSpacePosition());
		TanksMode->AimPlayerTurret(ArenaPos);
	}
	return FReply::Handled();
}

FReply UTanksHUD::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	if (TanksMode && TanksMode->bGameActive)
	{
		const FVector2D ArenaPos = ScreenToArena(InGeometry, InTouchEvent.GetScreenSpacePosition());
		TanksMode->AimPlayerTurret(ArenaPos);
		TanksMode->PlayerFire();
	}
	return FReply::Handled();
}

FVector2D UTanksHUD::ScreenToArena(const FGeometry& Geometry, FVector2D ScreenPos) const
{
	if (!GameCanvas || !TanksMode)
	{
		return FVector2D::ZeroVector;
	}

	const FGeometry& CanvasGeometry = GameCanvas->GetCachedGeometry();
	const FVector2D LocalPos = CanvasGeometry.AbsoluteToLocal(ScreenPos);
	const FVector2D CanvasSize = CanvasGeometry.GetLocalSize();

	if (CanvasSize.X <= 0.f || CanvasSize.Y <= 0.f)
	{
		return FVector2D::ZeroVector;
	}

	const float NormX = FMath::Clamp(LocalPos.X / CanvasSize.X, 0.f, 1.f);
	const float NormY = FMath::Clamp(LocalPos.Y / CanvasSize.Y, 0.f, 1.f);

	return FVector2D(NormX * TanksMode->ArenaWidth, NormY * TanksMode->ArenaHeight);
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UTanksHUD::OnAngleUp()
{
	AimAngleDeg = FMath::Clamp(AimAngleDeg + 5.f, 0.f, 90.f);
}

void UTanksHUD::OnAngleDown()
{
	AimAngleDeg = FMath::Clamp(AimAngleDeg - 5.f, 0.f, 90.f);
}

void UTanksHUD::OnPowerUp()
{
	AimPower = FMath::Clamp(AimPower + 0.1f, 0.f, 1.f);
}

void UTanksHUD::OnPowerDown()
{
	AimPower = FMath::Clamp(AimPower - 0.1f, 0.f, 1.f);
}

void UTanksHUD::OnFireClicked()
{
	if (TanksMode && TanksMode->bGameActive)
	{
		TanksMode->PlayerFire();
	}
}
