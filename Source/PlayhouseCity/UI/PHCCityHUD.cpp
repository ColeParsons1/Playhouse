// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/PHCCityHUD.h"
#include "UI/PHCMainHUD.h"
#include "Core/PHCGameInstance.h"
#include "Voice/AgoraVoiceManager.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"
#include "Core/PHCPlayerController.h"
#include "Player/PHCCityCharacter.h"
#include "Vehicle/PHCVehiclePawn.h"
#include "Vehicle/PHCDuneBuggyPawn.h"
#include "Vehicle/PHCRaptorPawn.h"
#include "Vehicle/PHCTankPawn.h"
#include "Vehicle/PHCDirtBikePawn.h"
#include "Vehicle/PHCBlackbirdPawn.h"
#include "Vehicle/PHCBlackHawkPawn.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "PlayhouseCity.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Components/ScrollBox.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "Player/PHCPlayerState.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"
#include "Core/PHCCitySubsystem.h"
#include "Core/PHCCityGameMode.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

UPHCCityHUD::UPHCCityHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MainHUDClass = UPHCMainHUD::StaticClass();
}

// ---------------------------------------------------------------------------
// Widget tree construction
// ---------------------------------------------------------------------------

bool UPHCCityHUD::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	// Only build programmatically if not already wired from a UMG Blueprint.
	if (!CoinText)
	{
		const FLinearColor BarBg(0.f, 0.f, 0.f, 0.5f);
		const FLinearColor White(FLinearColor::White);
		const FSlateFontInfo CurrencyFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
		const FSlateFontInfo BtnFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);

		// ---- Root canvas (full screen) ----
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("CityHUDRoot"));
		WidgetTree->RootWidget = RootCanvas;

		// ============================================================
		//  TOP-RIGHT: Currency display  (semi-transparent bar)
		// ============================================================
		UBorder* CurrencyBar = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("CurrencyBar"));
		CurrencyBar->SetBrushColor(BarBg);
		CurrencyBar->SetPadding(FMargin(12.f, 6.f, 12.f, 6.f));

		UCanvasPanelSlot* CurrencySlot = RootCanvas->AddChildToCanvas(CurrencyBar);
		CurrencySlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f)); // top-right
		CurrencySlot->SetAlignment(FVector2D(1.f, 0.f));
		CurrencySlot->SetOffsets(FMargin(-80.f, 16.f, 0.f, 0.f));
		CurrencySlot->SetAutoSize(true);

		UHorizontalBox* CurrencyHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("CurrencyHBox"));
		CurrencyBar->SetContent(CurrencyHBox);

		// Coins
		CoinText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("CoinText"));
		CoinText->SetText(FText::FromString(TEXT("Coins: 0")));
		CoinText->SetFont(CurrencyFont);
		CoinText->SetColorAndOpacity(FSlateColor(White));

		UHorizontalBoxSlot* CoinSlot = CurrencyHBox->AddChildToHorizontalBox(CoinText);
		CoinSlot->SetVerticalAlignment(VAlign_Center);
		CoinSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

		// Gems
		GemText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("GemText"));
		GemText->SetText(FText::FromString(TEXT("Gems: 0")));
		GemText->SetFont(CurrencyFont);
		GemText->SetColorAndOpacity(FSlateColor(White));

		UHorizontalBoxSlot* GemSlot = CurrencyHBox->AddChildToHorizontalBox(GemText);
		GemSlot->SetVerticalAlignment(VAlign_Center);

		// ============================================================
		//  TOP-RIGHT: Military PPI Radar
		// ============================================================
		{
			const float Half = MinimapSize * 0.5f;
			const FLinearColor RadarGreen(0.0f, 1.0f, 0.3f, 1.f);
			const FLinearColor RadarGreenDim(0.0f, 0.6f, 0.15f, 0.35f);
			const FLinearColor RadarBG(0.02f, 0.06f, 0.02f, 0.85f);
			const FSlateFontInfo RadarFont(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 10);

			// Dark green-black background
			MinimapBorder = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("MinimapBorder"));
			MinimapBorder->SetBrushColor(RadarBG);
			MinimapBorder->SetPadding(FMargin(0.f));

			UCanvasPanelSlot* MmSlot = RootCanvas->AddChildToCanvas(MinimapBorder);
			MmSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
			MmSlot->SetAlignment(FVector2D(1.f, 0.f));
			MmSlot->SetAutoSize(false);
			MmSlot->SetPosition(FVector2D(-10.f, 82.f));
			MmSlot->SetSize(FVector2D(MinimapSize, MinimapSize));

			MinimapCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
				UCanvasPanel::StaticClass(), TEXT("MinimapCanvas"));
			MinimapBorder->SetContent(MinimapCanvas);

			// Outer ring (bright green border)
			RadarRingOuter = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("RadarRingOuter"));
			RadarRingOuter->SetBrushColor(FLinearColor(0.0f, 0.8f, 0.2f, 0.5f));
			RadarRingOuter->SetPadding(FMargin(1.f));
			{
				UBorder* RingClear = WidgetTree->ConstructWidget<UBorder>(
					UBorder::StaticClass(), TEXT("RadarRingOuterClear"));
				RingClear->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f)); // transparent center
				RingClear->SetPadding(FMargin(0.f));
				RadarRingOuter->SetContent(RingClear);
			}
			UCanvasPanelSlot* OutSlot = MinimapCanvas->AddChildToCanvas(RadarRingOuter);
			OutSlot->SetSize(FVector2D(MinimapSize, MinimapSize));
			OutSlot->SetPosition(FVector2D(0.f, 0.f));
			OutSlot->SetAutoSize(false);

			// Inner range ring (50% radius)
			RadarRingInner = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("RadarRingInner"));
			RadarRingInner->SetBrushColor(RadarGreenDim);
			RadarRingInner->SetPadding(FMargin(1.f));
			{
				UBorder* InClear = WidgetTree->ConstructWidget<UBorder>(
					UBorder::StaticClass(), TEXT("RadarRingInnerClear"));
				InClear->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
				InClear->SetPadding(FMargin(0.f));
				RadarRingInner->SetContent(InClear);
			}
			const float InnerDiam = MinimapSize * 0.5f;
			UCanvasPanelSlot* InSlot = MinimapCanvas->AddChildToCanvas(RadarRingInner);
			InSlot->SetSize(FVector2D(InnerDiam, InnerDiam));
			InSlot->SetPosition(FVector2D(Half - InnerDiam * 0.5f, Half - InnerDiam * 0.5f));
			InSlot->SetAutoSize(false);

			// Crosshair lines (horizontal)
			RadarCrossH = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("RadarCrossH"));
			RadarCrossH->SetBrushColor(RadarGreenDim);
			RadarCrossH->SetPadding(FMargin(0.f));
			UCanvasPanelSlot* CHSlot = MinimapCanvas->AddChildToCanvas(RadarCrossH);
			CHSlot->SetSize(FVector2D(MinimapSize, 1.f));
			CHSlot->SetPosition(FVector2D(0.f, Half));
			CHSlot->SetAutoSize(false);

			// Crosshair lines (vertical)
			RadarCrossV = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("RadarCrossV"));
			RadarCrossV->SetBrushColor(RadarGreenDim);
			RadarCrossV->SetPadding(FMargin(0.f));
			UCanvasPanelSlot* CVSlot = MinimapCanvas->AddChildToCanvas(RadarCrossV);
			CVSlot->SetSize(FVector2D(1.f, MinimapSize));
			CVSlot->SetPosition(FVector2D(Half, 0.f));
			CVSlot->SetAutoSize(false);

			// Sweep line (animated rotating line from center)
			RadarSweepLine = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("RadarSweepLine"));
			RadarSweepLine->SetBrushColor(FLinearColor(0.0f, 1.0f, 0.3f, 0.6f));
			RadarSweepLine->SetPadding(FMargin(0.f));
			UCanvasPanelSlot* SwSlot = MinimapCanvas->AddChildToCanvas(RadarSweepLine);
			SwSlot->SetSize(FVector2D(2.f, Half));
			SwSlot->SetPosition(FVector2D(Half - 1.f, 0.f)); // starts pointing up
			SwSlot->SetAutoSize(false);

			// Cardinal direction labels — N
			RadarLabelN = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("RadarN"));
			RadarLabelN->SetText(FText::FromString(TEXT("N")));
			RadarLabelN->SetFont(RadarFont);
			RadarLabelN->SetColorAndOpacity(FSlateColor(RadarGreen));
			UCanvasPanelSlot* NSlot = MinimapCanvas->AddChildToCanvas(RadarLabelN);
			NSlot->SetPosition(FVector2D(Half - 5.f, 2.f));
			NSlot->SetAutoSize(true);

			// S
			RadarLabelS = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("RadarS"));
			RadarLabelS->SetText(FText::FromString(TEXT("S")));
			RadarLabelS->SetFont(RadarFont);
			RadarLabelS->SetColorAndOpacity(FSlateColor(RadarGreen));
			UCanvasPanelSlot* SSlot = MinimapCanvas->AddChildToCanvas(RadarLabelS);
			SSlot->SetPosition(FVector2D(Half - 4.f, MinimapSize - 16.f));
			SSlot->SetAutoSize(true);

			// E
			RadarLabelE = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("RadarE"));
			RadarLabelE->SetText(FText::FromString(TEXT("E")));
			RadarLabelE->SetFont(RadarFont);
			RadarLabelE->SetColorAndOpacity(FSlateColor(RadarGreen));
			UCanvasPanelSlot* ESlot = MinimapCanvas->AddChildToCanvas(RadarLabelE);
			ESlot->SetPosition(FVector2D(MinimapSize - 14.f, Half - 7.f));
			ESlot->SetAutoSize(true);

			// W
			RadarLabelW = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("RadarW"));
			RadarLabelW->SetText(FText::FromString(TEXT("W")));
			RadarLabelW->SetFont(RadarFont);
			RadarLabelW->SetColorAndOpacity(FSlateColor(RadarGreen));
			UCanvasPanelSlot* WSlot = MinimapCanvas->AddChildToCanvas(RadarLabelW);
			WSlot->SetPosition(FVector2D(3.f, Half - 7.f));
			WSlot->SetAutoSize(true);

			// Local player blip (bright green triangle-like dot at center)
			MinimapLocalDot = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("MinimapLocalDot"));
			MinimapLocalDot->SetBrushColor(RadarGreen);
			MinimapLocalDot->SetPadding(FMargin(0.f));
			UCanvasPanelSlot* LocalDotSlot = MinimapCanvas->AddChildToCanvas(MinimapLocalDot);
			LocalDotSlot->SetSize(FVector2D(6.f, 6.f));
			LocalDotSlot->SetPosition(FVector2D(Half - 3.f, Half - 3.f));
			LocalDotSlot->SetAutoSize(false);

			// Direction indicator (heading line from center)
			MinimapDirectionIndicator = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("MinimapDirIndicator"));
			MinimapDirectionIndicator->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.9f));
			MinimapDirectionIndicator->SetPadding(FMargin(0.f));
			UCanvasPanelSlot* DirSlot = MinimapCanvas->AddChildToCanvas(MinimapDirectionIndicator);
			DirSlot->SetSize(FVector2D(2.f, 16.f));
			DirSlot->SetPosition(FVector2D(Half - 1.f, Half - 22.f));
			DirSlot->SetAutoSize(false);

			// Pre-create 8 blip widgets (reused from pool)
			for (int32 i = 0; i < 8; ++i)
			{
				UBorder* Dot = WidgetTree->ConstructWidget<UBorder>(
					UBorder::StaticClass(), *FString::Printf(TEXT("MinimapDot%d"), i));
				Dot->SetBrushColor(RadarGreen);
				Dot->SetPadding(FMargin(0.f));
				Dot->SetVisibility(ESlateVisibility::Collapsed);

				UCanvasPanelSlot* DotSlot = MinimapCanvas->AddChildToCanvas(Dot);
				DotSlot->SetSize(FVector2D(6.f, 6.f));
				DotSlot->SetPosition(FVector2D(0.f, 0.f));
				DotSlot->SetAutoSize(false);

				MinimapPlayerDots.Add(Dot);
			}
		}

		// ============================================================
		//  TOP-CENTER: Live location monitor (X, Y, Z coordinates)
		// ============================================================
		{
			const FSlateFontInfo LocFont = FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18);

			UBorder* LocBar = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("LocationBar"));
			LocBar->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.75f));
			LocBar->SetPadding(FMargin(14.f, 6.f, 14.f, 6.f));

			UCanvasPanelSlot* LocSlot = RootCanvas->AddChildToCanvas(LocBar);
			LocSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f)); // top-center
			LocSlot->SetAlignment(FVector2D(0.5f, 0.f));
			LocSlot->SetOffsets(FMargin(0.f, 10.f, 0.f, 0.f));
			LocSlot->SetAutoSize(true);

			LocationText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("LocationText"));
			LocationText->SetText(FText::FromString(TEXT("X: 0  Y: 0  Z: 0")));
			LocationText->SetFont(LocFont);
			LocationText->SetColorAndOpacity(FSlateColor(FLinearColor(0.f, 1.f, 0.f)));
			LocBar->SetContent(LocationText);
		}

		// ============================================================
		//  HEALTH BAR — right of the location monitor, same style/size
		// ============================================================
		{
			const FSlateFontInfo HPFont = FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 12);

			// Outer background (dark, same style as location bar)
			HealthBarBG = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("HealthBarBG"));
			HealthBarBG->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
			HealthBarBG->SetPadding(FMargin(4.f, 4.f, 4.f, 4.f));

			UCanvasPanelSlot* HPBGS = RootCanvas->AddChildToCanvas(HealthBarBG);
			HPBGS->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f)); // top-center
			HPBGS->SetAlignment(FVector2D(0.f, 0.f)); // left-aligned from anchor
			HPBGS->SetOffsets(FMargin(260.f, 10.f, 160.f, 26.f)); // right of LocationBar
			HPBGS->SetAutoSize(false);

			// Inner fill bar (green = healthy)
			HealthBarFill = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("HealthBarFill"));
			HealthBarFill->SetBrushColor(FLinearColor(0.1f, 0.75f, 0.15f, 0.9f));
			HealthBarFill->SetPadding(FMargin(0.f));

			// Size box to control the fill width
			USizeBox* HPSizeBox = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(), TEXT("HPSizeBox"));
			HPSizeBox->SetWidthOverride(152.f); // full width at 100% hp
			HPSizeBox->SetHeightOverride(18.f);
			HPSizeBox->AddChild(HealthBarFill);

			// HP text overlay
			HealthText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("HealthText"));
			HealthText->SetText(FText::FromString(TEXT("100")));
			HealthText->SetFont(HPFont);
			HealthText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			HealthText->SetJustification(ETextJustify::Center);

			// Overlay to stack fill bar + text
			UOverlay* HPOverlay = WidgetTree->ConstructWidget<UOverlay>(
				UOverlay::StaticClass(), TEXT("HPOverlay"));
			UOverlaySlot* FillSlot = HPOverlay->AddChildToOverlay(HPSizeBox);
			FillSlot->SetHorizontalAlignment(HAlign_Left);
			FillSlot->SetVerticalAlignment(VAlign_Fill);
			UOverlaySlot* TextSlot = HPOverlay->AddChildToOverlay(HealthText);
			TextSlot->SetHorizontalAlignment(HAlign_Center);
			TextSlot->SetVerticalAlignment(VAlign_Center);

			HealthBarBG->SetContent(HPOverlay);
		}

		// ============================================================
		//  K/D DISPLAY — right of the health bar, same style
		// ============================================================
		{
			const FSlateFontInfo KDFont = FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 11);

			KDDisplayBG = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("KDDisplayBG"));
			KDDisplayBG->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
			KDDisplayBG->SetPadding(FMargin(8.f, 4.f, 8.f, 4.f));

			UCanvasPanelSlot* KDS = RootCanvas->AddChildToCanvas(KDDisplayBG);
			KDS->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f)); // top-center
			KDS->SetAlignment(FVector2D(0.f, 0.f));
			KDS->SetOffsets(FMargin(430.f, 10.f, 220.f, 26.f)); // right of health bar
			KDS->SetAutoSize(false);

			KDText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("KDText"));
			KDText->SetText(FText::FromString(TEXT("K:0  D:0")));
			KDText->SetFont(KDFont);
			KDText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			KDText->SetJustification(ETextJustify::Center);
			KDDisplayBG->SetContent(KDText);
		}

		// ============================================================
		//  XP Progress Bar + Level Display (below health bar)
		// ============================================================
		{
			const FSlateFontInfo XPFont = FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 11);

			// Level badge (left of XP bar)
			LevelText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("LevelText"));
			LevelText->SetText(FText::FromString(TEXT("LV 1")));
			LevelText->SetFont(FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14));
			LevelText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.f)));
			LevelText->SetJustification(ETextJustify::Center);

			UCanvasPanelSlot* LvlSlot = RootCanvas->AddChildToCanvas(LevelText);
			LvlSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
			LvlSlot->SetAlignment(FVector2D(0.f, 0.f));
			LvlSlot->SetOffsets(FMargin(210.f, 40.f, 50.f, 16.f));
			LvlSlot->SetAutoSize(false);

			// XP bar background
			XPBarBG = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("XPBarBG"));
			XPBarBG->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
			XPBarBG->SetPadding(FMargin(0.f));

			UCanvasPanelSlot* XPBGS = RootCanvas->AddChildToCanvas(XPBarBG);
			XPBGS->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
			XPBGS->SetAlignment(FVector2D(0.f, 0.f));
			XPBGS->SetOffsets(FMargin(260.f, 40.f, 390.f, 16.f));
			XPBGS->SetAutoSize(false);

			// XP bar fill (resized dynamically in Tick)
			UOverlay* XPOverlay = WidgetTree->ConstructWidget<UOverlay>(
				UOverlay::StaticClass(), TEXT("XPOverlay"));
			XPBarBG->SetContent(XPOverlay);

			USizeBox* XPSizeBox = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(), TEXT("XPSizeBox"));
			XPSizeBox->SetWidthOverride(390.f);
			XPSizeBox->SetHeightOverride(16.f);
			UOverlaySlot* XPFillSlot = XPOverlay->AddChildToOverlay(XPSizeBox);
			XPFillSlot->SetHorizontalAlignment(HAlign_Left);

			XPBarFill = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("XPBarFill"));
			XPBarFill->SetBrushColor(FLinearColor(0.2f, 0.6f, 1.f, 0.9f));
			XPBarFill->SetPadding(FMargin(0.f));
			XPSizeBox->AddChild(XPBarFill);

			// XP text overlay on the bar
			XPText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("XPText"));
			XPText->SetText(FText::FromString(TEXT("0 / 100 XP")));
			XPText->SetFont(XPFont);
			XPText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			XPText->SetJustification(ETextJustify::Center);
			UOverlaySlot* XPTSlot = XPOverlay->AddChildToOverlay(XPText);
			XPTSlot->SetHorizontalAlignment(HAlign_Center);
			XPTSlot->SetVerticalAlignment(VAlign_Center);
		}

		// ============================================================
		//  Kill Streak text (bottom-center, above action buttons)
		// ============================================================
		KillStreakText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("KillStreakText"));
		KillStreakText->SetText(FText::FromString(TEXT("")));
		KillStreakText->SetFont(FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22));
		KillStreakText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.4f, 0.1f)));
		KillStreakText->SetJustification(ETextJustify::Center);
		{
			UCanvasPanelSlot* KSSlot = RootCanvas->AddChildToCanvas(KillStreakText);
			KSSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
			KSSlot->SetAlignment(FVector2D(0.5f, 1.f));
			KSSlot->SetOffsets(FMargin(-100.f, -120.f, 200.f, 40.f));
			KSSlot->SetAutoSize(false);
		}

		// ============================================================
		//  Level-up flash overlay (full-screen golden glow)
		// ============================================================
		LevelUpOverlay = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("LevelUpOverlay"));
		LevelUpOverlay->SetBrushColor(FLinearColor(1.f, 0.85f, 0.f, 0.f));
		LevelUpOverlay->SetPadding(FMargin(0.f));
		{
			UCanvasPanelSlot* LUSlot = RootCanvas->AddChildToCanvas(LevelUpOverlay);
			LUSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			LUSlot->SetOffsets(FMargin(0.f));

			LevelUpText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("LevelUpText"));
			LevelUpText->SetText(FText::FromString(TEXT("")));
			LevelUpText->SetFont(FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 36));
			LevelUpText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.f)));
			LevelUpText->SetJustification(ETextJustify::Center);

			UCanvasPanelSlot* LUTSlot = RootCanvas->AddChildToCanvas(LevelUpText);
			LUTSlot->SetAnchors(FAnchors(0.5f, 0.4f, 0.5f, 0.4f));
			LUTSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			LUTSlot->SetOffsets(FMargin(-200.f, -30.f, 400.f, 60.f));
			LUTSlot->SetAutoSize(false);
		}

		// ============================================================
		//  Daily Login Reward Popup (centered overlay)
		// ============================================================
		{
			const FSlateFontInfo PopupFont = FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
			const FSlateFontInfo PopupTitleFont = FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);

			DailyLoginOverlay = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("DailyLoginOverlay"));
			DailyLoginOverlay->SetBrushColor(FLinearColor(0.02f, 0.02f, 0.05f, 0.92f));
			DailyLoginOverlay->SetPadding(FMargin(20.f));
			DailyLoginOverlay->SetVisibility(ESlateVisibility::Collapsed);

			UCanvasPanelSlot* DLSlot = RootCanvas->AddChildToCanvas(DailyLoginOverlay);
			DLSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
			DLSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			DLSlot->SetOffsets(FMargin(-200.f, -160.f, 400.f, 320.f));
			DLSlot->SetAutoSize(false);

			// Vertical layout inside popup
			UVerticalBox* DLVBox = WidgetTree->ConstructWidget<UVerticalBox>(
				UVerticalBox::StaticClass(), TEXT("DLVBox"));
			DailyLoginOverlay->SetContent(DLVBox);

			// Title
			DailyLoginTitle = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("DailyLoginTitle"));
			DailyLoginTitle->SetText(FText::FromString(TEXT("DAILY REWARD")));
			DailyLoginTitle->SetFont(PopupTitleFont);
			DailyLoginTitle->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.f)));
			DailyLoginTitle->SetJustification(ETextJustify::Center);
			UVerticalBoxSlot* TSlot = DLVBox->AddChildToVerticalBox(DailyLoginTitle);
			TSlot->SetHorizontalAlignment(HAlign_Center);
			TSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

			// Streak text
			DailyLoginStreakText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("DailyLoginStreakText"));
			DailyLoginStreakText->SetText(FText::FromString(TEXT("Day 1 of 7")));
			DailyLoginStreakText->SetFont(PopupFont);
			DailyLoginStreakText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			DailyLoginStreakText->SetJustification(ETextJustify::Center);
			UVerticalBoxSlot* SSlot = DLVBox->AddChildToVerticalBox(DailyLoginStreakText);
			SSlot->SetHorizontalAlignment(HAlign_Center);
			SSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

			// 7-day indicator row
			UHorizontalBox* DayRow = WidgetTree->ConstructWidget<UHorizontalBox>(
				UHorizontalBox::StaticClass(), TEXT("DayRow"));
			UVerticalBoxSlot* DRSlot = DLVBox->AddChildToVerticalBox(DayRow);
			DRSlot->SetHorizontalAlignment(HAlign_Center);
			DRSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));

			static const int32 DayRewards[] = { 5, 10, 20, 40, 80, 150, 300 };
			UBorder* DayBorders[7] = {};
			for (int32 D = 0; D < 7; ++D)
			{
				UBorder* DayBox = WidgetTree->ConstructWidget<UBorder>(
					UBorder::StaticClass(), *FString::Printf(TEXT("DayBox%d"), D));
				DayBox->SetBrushColor(FLinearColor(0.15f, 0.15f, 0.15f, 0.9f));
				DayBox->SetPadding(FMargin(6.f, 4.f, 6.f, 4.f));

				UTextBlock* DayLabel = WidgetTree->ConstructWidget<UTextBlock>(
					UTextBlock::StaticClass(), *FString::Printf(TEXT("DayLabel%d"), D));
				DayLabel->SetText(FText::FromString(
					FString::Printf(TEXT("D%d\n%d"), D + 1, DayRewards[D])));
				DayLabel->SetFont(FSlateFontInfo(
					FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 10));
				DayLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
				DayLabel->SetJustification(ETextJustify::Center);
				DayBox->SetContent(DayLabel);

				UHorizontalBoxSlot* HSlot = DayRow->AddChildToHorizontalBox(DayBox);
				HSlot->SetPadding(FMargin(3.f, 0.f));

				DayBorders[D] = DayBox;
			}
			DailyLoginDay1 = DayBorders[0];
			DailyLoginDay2 = DayBorders[1];
			DailyLoginDay3 = DayBorders[2];
			DailyLoginDay4 = DayBorders[3];
			DailyLoginDay5 = DayBorders[4];
			DailyLoginDay6 = DayBorders[5];
			DailyLoginDay7 = DayBorders[6];

			// Reward text
			DailyLoginRewardText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("DailyLoginRewardText"));
			DailyLoginRewardText->SetText(FText::FromString(TEXT("Claim 5 Coins + 25 XP!")));
			DailyLoginRewardText->SetFont(PopupFont);
			DailyLoginRewardText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 1.f, 0.4f)));
			DailyLoginRewardText->SetJustification(ETextJustify::Center);
			UVerticalBoxSlot* RSlot = DLVBox->AddChildToVerticalBox(DailyLoginRewardText);
			RSlot->SetHorizontalAlignment(HAlign_Center);
			RSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 15.f));

			// Claim button
			DailyLoginClaimBtn = WidgetTree->ConstructWidget<UButton>(
				UButton::StaticClass(), TEXT("DailyLoginClaimBtn"));
			FButtonStyle ClaimStyle = DailyLoginClaimBtn->GetStyle();
			ClaimStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.1f, 0.6f, 0.15f, 0.95f));
			ClaimStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.75f, 0.2f, 1.0f));
			ClaimStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.45f, 0.1f, 1.0f));
			DailyLoginClaimBtn->SetStyle(ClaimStyle);

			DailyLoginClaimLabel = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("DailyLoginClaimLabel"));
			DailyLoginClaimLabel->SetText(FText::FromString(TEXT("CLAIM")));
			DailyLoginClaimLabel->SetFont(PopupTitleFont);
			DailyLoginClaimLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			DailyLoginClaimLabel->SetJustification(ETextJustify::Center);
			DailyLoginClaimBtn->AddChild(DailyLoginClaimLabel);

			UVerticalBoxSlot* BSlot = DLVBox->AddChildToVerticalBox(DailyLoginClaimBtn);
			BSlot->SetHorizontalAlignment(HAlign_Center);
		}

		// ============================================================
		//  LOOT CRATE spinner overlay (centered, hidden by default)
		// ============================================================
		{
			LootCrateOverlay = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("LootCrateOverlay"));
			LootCrateOverlay->SetBrushColor(FLinearColor(0.05f, 0.02f, 0.0f, 0.88f));
			LootCrateOverlay->SetPadding(FMargin(20.f));
			LootCrateOverlay->SetVisibility(ESlateVisibility::Collapsed);

			UCanvasPanelSlot* LCSlot = RootCanvas->AddChildToCanvas(LootCrateOverlay);
			LCSlot->SetAnchors(FAnchors(0.5f, 0.35f, 0.5f, 0.35f));
			LCSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			LCSlot->SetOffsets(FMargin(-120.f, -50.f, 240.f, 100.f));
			LCSlot->SetAutoSize(false);

			UVerticalBox* LCVBox = WidgetTree->ConstructWidget<UVerticalBox>(
				UVerticalBox::StaticClass(), TEXT("LCVBox"));
			LootCrateOverlay->SetContent(LCVBox);

			LootCrateSpinText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("LootCrateSpinText"));
			LootCrateSpinText->SetText(FText::FromString(TEXT("LOOT CRATE!")));
			LootCrateSpinText->SetFont(FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20));
			LootCrateSpinText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.f)));
			LootCrateSpinText->SetJustification(ETextJustify::Center);
			UVerticalBoxSlot* LSSlot = LCVBox->AddChildToVerticalBox(LootCrateSpinText);
			LSSlot->SetHorizontalAlignment(HAlign_Center);

			LootCrateRewardText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("LootCrateRewardText"));
			LootCrateRewardText->SetText(FText::FromString(TEXT("")));
			LootCrateRewardText->SetFont(FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16));
			LootCrateRewardText->SetColorAndOpacity(FSlateColor(FLinearColor(0.3f, 1.f, 0.4f)));
			LootCrateRewardText->SetJustification(ETextJustify::Center);
			UVerticalBoxSlot* LRSlot = LCVBox->AddChildToVerticalBox(LootCrateRewardText);
			LRSlot->SetHorizontalAlignment(HAlign_Center);
			LRSlot->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
		}

		// ============================================================
		//  ACHIEVEMENT popup banner (top-center, hidden by default)
		// ============================================================
		{
			AchievementPopupBG = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("AchievementPopupBG"));
			AchievementPopupBG->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.15f, 0.9f));
			AchievementPopupBG->SetPadding(FMargin(16.f, 8.f));
			AchievementPopupBG->SetVisibility(ESlateVisibility::Collapsed);

			UCanvasPanelSlot* APSlot = RootCanvas->AddChildToCanvas(AchievementPopupBG);
			APSlot->SetAnchors(FAnchors(0.5f, 0.12f, 0.5f, 0.12f));
			APSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			APSlot->SetOffsets(FMargin(-160.f, -20.f, 320.f, 40.f));
			APSlot->SetAutoSize(false);

			AchievementPopupText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("AchievementPopupText"));
			AchievementPopupText->SetText(FText::FromString(TEXT("")));
			AchievementPopupText->SetFont(FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14));
			AchievementPopupText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.f)));
			AchievementPopupText->SetJustification(ETextJustify::Center);
			AchievementPopupBG->SetContent(AchievementPopupText);
		}

		// ============================================================
		//  EVENT BANNER (top area, shows active limited-time event)
		// ============================================================
		{
			EventBannerBG = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("EventBannerBG"));
			EventBannerBG->SetBrushColor(FLinearColor(0.6f, 0.1f, 0.0f, 0.85f));
			EventBannerBG->SetPadding(FMargin(12.f, 6.f));
			EventBannerBG->SetVisibility(ESlateVisibility::Collapsed);

			UCanvasPanelSlot* EBSlot = RootCanvas->AddChildToCanvas(EventBannerBG);
			EBSlot->SetAnchors(FAnchors(0.3f, 0.02f, 0.3f, 0.02f));
			EBSlot->SetAlignment(FVector2D(0.5f, 0.f));
			EBSlot->SetOffsets(FMargin(-170.f, 0.f, 340.f, 50.f));
			EBSlot->SetAutoSize(false);

			UVerticalBox* EVBox = WidgetTree->ConstructWidget<UVerticalBox>(
				UVerticalBox::StaticClass(), TEXT("EVBox"));
			EventBannerBG->SetContent(EVBox);

			EventBannerText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("EventBannerText"));
			EventBannerText->SetText(FText::FromString(TEXT("")));
			EventBannerText->SetFont(FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14));
			EventBannerText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.9f, 0.3f)));
			EventBannerText->SetJustification(ETextJustify::Center);
			UVerticalBoxSlot* EBTSlot = EVBox->AddChildToVerticalBox(EventBannerText);
			EBTSlot->SetHorizontalAlignment(HAlign_Center);

			EventTimerText = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("EventTimerText"));
			EventTimerText->SetText(FText::FromString(TEXT("")));
			EventTimerText->SetFont(FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 12));
			EventTimerText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			EventTimerText->SetJustification(ETextJustify::Center);
			UVerticalBoxSlot* ETTSlot = EVBox->AddChildToVerticalBox(EventTimerText);
			ETTSlot->SetHorizontalAlignment(HAlign_Center);
		}

		// ============================================================
		//  TOP-RIGHT (below currency): "Fire" button -- missile launcher
		// ============================================================
		// ============================================================
		//  TOP-LEFT: Menu button (opens lobby HUD as overlay)
		// ============================================================
		MenuButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("MenuButton"));

		FButtonStyle MenuBtnStyle = MenuButton->GetStyle();
		MenuBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.82f, 0.86f, 0.92f, 0.22f));
		MenuBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.88f, 0.92f, 0.97f, 0.35f));
		MenuBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.95f, 0.97f, 1.00f, 0.50f));
		MenuButton->SetStyle(MenuBtnStyle);

		UTextBlock* MenuLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("MenuLabel"));
		MenuLabel->SetText(FText::FromString(TEXT("Menu")));
		MenuLabel->SetFont(BtnFont);
		MenuLabel->SetColorAndOpacity(FSlateColor(White));
		MenuLabel->SetJustification(ETextJustify::Center);
		MenuButton->AddChild(MenuLabel);

		UCanvasPanelSlot* MenuBtnSlot = RootCanvas->AddChildToCanvas(MenuButton);
		MenuBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f)); // top-left
		MenuBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		MenuBtnSlot->SetOffsets(FMargin(50.f, 16.f, 130.f, 50.f));
		MenuButton->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  Voice Mute Toggle Button (top-left, below menu)
		// ============================================================
		VoiceMuteButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("VoiceMuteButton"));
		{
			FButtonStyle MuteBtnStyle = VoiceMuteButton->GetStyle();
			MuteBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.1f, 0.6f, 0.1f, 0.5f));
			MuteBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.7f, 0.15f, 0.6f));
			MuteBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.2f, 0.8f, 0.2f, 0.7f));
			VoiceMuteButton->SetStyle(MuteBtnStyle);
		}
		VoiceMuteLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("VoiceMuteLabel"));
		VoiceMuteLabel->SetText(FText::FromString(TEXT("MIC ON")));
		VoiceMuteLabel->SetFont(FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 12));
		VoiceMuteLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		VoiceMuteLabel->SetJustification(ETextJustify::Center);
		VoiceMuteButton->AddChild(VoiceMuteLabel);

		UCanvasPanelSlot* MuteBtnSlot = RootCanvas->AddChildToCanvas(VoiceMuteButton);
		MuteBtnSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f)); // top-center like location bar
		MuteBtnSlot->SetAlignment(FVector2D(1.f, 0.f)); // right edge aligned to anchor
		MuteBtnSlot->SetOffsets(FMargin(-160.f, 10.f, 90.f, 34.f)); // left of location bar

		VoiceMuteButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnVoiceMuteClicked);

		// ============================================================
		//  TOP-LEFT (below Menu): "Car" button -- spawns Aventador
		// ============================================================
		CarButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("CarButton"));

		FButtonStyle CarBtnStyle = CarButton->GetStyle();
		CarBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.82f, 0.86f, 0.92f, 0.22f));
		CarBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.88f, 0.92f, 0.97f, 0.35f));
		CarBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.95f, 0.97f, 1.00f, 0.50f));
		CarButton->SetStyle(CarBtnStyle);

		UTextBlock* CarLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("CarLabel"));
		CarLabel->SetText(FText::FromString(TEXT("Vehicle")));
		CarLabel->SetFont(BtnFont);
		CarLabel->SetColorAndOpacity(FSlateColor(White));
		CarLabel->SetJustification(ETextJustify::Center);
		CarButton->AddChild(CarLabel);

		UCanvasPanelSlot* CarBtnSlot = RootCanvas->AddChildToCanvas(CarButton);
		CarBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f)); // top-left
		CarBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		CarBtnSlot->SetOffsets(FMargin(50.f, 80.f, 130.f, 55.f));

		// ============================================================
		//  CENTER-BOTTOM: "ENTER VEHICLE" button — large, bright green
		// ============================================================
		EnterVehicleButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("EnterVehicleButton"));

		FButtonStyle EnterBtnStyle = EnterVehicleButton->GetStyle();
		EnterBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.10f, 0.65f, 0.20f, 0.92f));
		EnterBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.75f, 0.25f, 0.95f));
		EnterBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.07f, 0.50f, 0.15f, 1.0f));
		EnterVehicleButton->SetStyle(EnterBtnStyle);

		const FSlateFontInfo EnterFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20);
		UTextBlock* EnterLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("EnterLabel"));
		EnterLabel->SetText(FText::FromString(TEXT("ENTER VEHICLE")));
		EnterLabel->SetFont(EnterFont);
		EnterLabel->SetColorAndOpacity(FSlateColor(White));
		EnterLabel->SetJustification(ETextJustify::Center);
		EnterVehicleButton->AddChild(EnterLabel);

		UCanvasPanelSlot* EnterBtnSlot = RootCanvas->AddChildToCanvas(EnterVehicleButton);
		EnterBtnSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f)); // bottom-center
		EnterBtnSlot->SetAlignment(FVector2D(0.5f, 1.f));
		EnterBtnSlot->SetOffsets(FMargin(0.f, -120.f, 200.f, 55.f));

		// Start hidden — proximity detection in NativeTick will show it
		EnterVehicleButton->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  TOP-LEFT (below Car): "Weapons" button
		// ============================================================
		WeaponsButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("WeaponsButton"));

		FButtonStyle WepBtnStyle = WeaponsButton->GetStyle();
		WepBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.82f, 0.86f, 0.92f, 0.22f));
		WepBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.88f, 0.92f, 0.97f, 0.35f));
		WepBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.95f, 0.97f, 1.00f, 0.50f));
		WeaponsButton->SetStyle(WepBtnStyle);

		UTextBlock* WepLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("WeaponsLabel"));
		WepLabel->SetText(FText::FromString(TEXT("Weapons")));
		WepLabel->SetFont(BtnFont);
		WepLabel->SetColorAndOpacity(FSlateColor(White));
		WepLabel->SetJustification(ETextJustify::Center);
		WeaponsButton->AddChild(WepLabel);

		UCanvasPanelSlot* WepBtnSlot = RootCanvas->AddChildToCanvas(WeaponsButton);
		WepBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		WepBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		WepBtnSlot->SetOffsets(FMargin(50.f, 150.f, 130.f, 55.f));

		// ============================================================
		//  TOP-LEFT (below Weapons): "Night" toggle button
		// ============================================================
		NightButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("NightButton"));

		FButtonStyle NightBtnStyle = NightButton->GetStyle();
		NightBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.82f, 0.86f, 0.92f, 0.22f));
		NightBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.88f, 0.92f, 0.97f, 0.35f));
		NightBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.95f, 0.97f, 1.00f, 0.50f));
		NightButton->SetStyle(NightBtnStyle);

		NightButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("NightButtonLabel"));
		NightButtonLabel->SetText(FText::FromString(TEXT("Night")));
		NightButtonLabel->SetFont(BtnFont);
		NightButtonLabel->SetColorAndOpacity(FSlateColor(White));
		NightButtonLabel->SetJustification(ETextJustify::Center);
		NightButton->AddChild(NightButtonLabel);

		UCanvasPanelSlot* NightBtnSlot = RootCanvas->AddChildToCanvas(NightButton);
		NightBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		NightBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		NightBtnSlot->SetOffsets(FMargin(50.f, 220.f, 130.f, 55.f));
		NightButton->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  DEV MODE button (below Night) — time of day controls
		// ============================================================
		DevModeButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("DevModeButton"));
		FButtonStyle DevBtnStyle = DevModeButton->GetStyle();
		DevBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.2f, 0.8f, 0.3f, 0.3f));
		DevBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.3f, 0.9f, 0.4f, 0.45f));
		DevBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.4f, 1.0f, 0.5f, 0.6f));
		DevModeButton->SetStyle(DevBtnStyle);

		DevModeButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("DevModeLabel"));
		DevModeButtonLabel->SetText(FText::FromString(TEXT("Dev")));
		DevModeButtonLabel->SetFont(BtnFont);
		DevModeButtonLabel->SetColorAndOpacity(FSlateColor(White));
		DevModeButtonLabel->SetJustification(ETextJustify::Center);
		DevModeButton->AddChild(DevModeButtonLabel);

		UCanvasPanelSlot* DevBtnSlot = RootCanvas->AddChildToCanvas(DevModeButton);
		DevBtnSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f)); // top-center like mic button
		DevBtnSlot->SetAlignment(FVector2D(1.f, 0.f));
		DevBtnSlot->SetOffsets(FMargin(-260.f, 10.f, 90.f, 34.f)); // left of mic button

		// Dev Time Panel (hidden by default)
		DevTimePanel = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), TEXT("DevTimePanel"));
		UCanvasPanelSlot* DevPanelSlot = RootCanvas->AddChildToCanvas(DevTimePanel);
		DevPanelSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
		DevPanelSlot->SetAlignment(FVector2D(1.f, 0.f));
		DevPanelSlot->SetOffsets(FMargin(-360.f, 50.f, 400.f, 260.f));
		DevTimePanel->SetVisibility(ESlateVisibility::Collapsed);

		const TCHAR* TimeLabels[] = {
			TEXT("Sunrise"), TEXT("Morning"), TEXT("Noon"), TEXT("Afternoon"),
			TEXT("Sunset"), TEXT("Dusk"), TEXT("Midnight"), TEXT("Cycle")
		};
		for (int32 Ti = 0; Ti < 8; ++Ti)
		{
			UButton* TBtn = WidgetTree->ConstructWidget<UButton>(
				UButton::StaticClass(), *FString::Printf(TEXT("DevTimeBtn_%d"), Ti));
			FButtonStyle TStyle = TBtn->GetStyle();
			TStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.15f, 0.15f, 0.25f, 0.7f));
			TStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.25f, 0.25f, 0.4f, 0.85f));
			TStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.4f, 0.4f, 0.6f, 1.0f));
			TBtn->SetStyle(TStyle);

			UTextBlock* TLabel = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), *FString::Printf(TEXT("DevTimeLbl_%d"), Ti));
			TLabel->SetText(FText::FromString(TimeLabels[Ti]));
			TLabel->SetFont(BtnFont);
			TLabel->SetColorAndOpacity(FSlateColor(White));
			TLabel->SetJustification(ETextJustify::Center);
			TBtn->AddChild(TLabel);

			int32 Row = Ti / 2;
			int32 Col = Ti % 2;
			UCanvasPanelSlot* TSlot = DevTimePanel->AddChildToCanvas(TBtn);
			TSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
			TSlot->SetAlignment(FVector2D(0.f, 0.f));
			TSlot->SetOffsets(FMargin(Col * 195.f, Row * 60.f, 190.f, 55.f));
			DevTimeBtns.Add(TBtn);
		}

		// ============================================================
		//  Disasters button (below Dev Mode)
		// ============================================================
		DisastersButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("DisastersButton"));

		FButtonStyle DisBtnStyle = DisastersButton->GetStyle();
		DisBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.82f, 0.86f, 0.92f, 0.22f));
		DisBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.88f, 0.92f, 0.97f, 0.35f));
		DisBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.95f, 0.97f, 1.00f, 0.50f));
		DisastersButton->SetStyle(DisBtnStyle);

		// Adjust Disasters button position (moved down for Dev Mode)
		DisastersButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("DisastersButtonLabel"));
		DisastersButtonLabel->SetText(FText::FromString(TEXT("Disasters")));
		DisastersButtonLabel->SetFont(BtnFont);
		DisastersButtonLabel->SetColorAndOpacity(FSlateColor(White));
		DisastersButtonLabel->SetJustification(ETextJustify::Center);
		DisastersButton->AddChild(DisastersButtonLabel);

		UCanvasPanelSlot* DisBtnSlot = RootCanvas->AddChildToCanvas(DisastersButton);
		DisBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		DisBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		DisBtnSlot->SetOffsets(FMargin(50.f, 430.f, 130.f, 55.f));
		DisastersButton->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  Camera mode toggle (below Disasters)
		// ============================================================
		CameraModeButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("CameraModeButton"));

		FButtonStyle CamBtnStyle = CameraModeButton->GetStyle();
		CamBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.82f, 0.86f, 0.92f, 0.22f));
		CamBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.88f, 0.92f, 0.97f, 0.35f));
		CamBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.95f, 0.97f, 1.00f, 0.50f));
		CameraModeButton->SetStyle(CamBtnStyle);

		CameraModeLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("CameraModeLabel"));
		CameraModeLabel->SetText(FText::FromString(TEXT("View")));
		CameraModeLabel->SetFont(BtnFont);
		CameraModeLabel->SetColorAndOpacity(FSlateColor(White));
		CameraModeLabel->SetJustification(ETextJustify::Center);
		CameraModeButton->AddChild(CameraModeLabel);

		UCanvasPanelSlot* CamBtnSlot = RootCanvas->AddChildToCanvas(CameraModeButton);
		CamBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		CamBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		CamBtnSlot->SetOffsets(FMargin(50.f, 220.f, 130.f, 55.f));

		// ============================================================
		//  Torch toggle (below View)
		// ============================================================
		TorchButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("TorchButton"));

		FButtonStyle TorchBtnStyle = TorchButton->GetStyle();
		TorchBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.82f, 0.86f, 0.92f, 0.22f));
		TorchBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.88f, 0.92f, 0.97f, 0.35f));
		TorchBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.95f, 0.97f, 1.00f, 0.50f));
		TorchButton->SetStyle(TorchBtnStyle);

		TorchButtonLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("TorchButtonLabel"));
		TorchButtonLabel->SetText(FText::FromString(TEXT("Lobby")));
		TorchButtonLabel->SetFont(BtnFont);
		TorchButtonLabel->SetColorAndOpacity(FSlateColor(White));
		TorchButtonLabel->SetJustification(ETextJustify::Center);
		TorchButton->AddChild(TorchButtonLabel);

		UCanvasPanelSlot* TorchBtnSlot = RootCanvas->AddChildToCanvas(TorchButton);
		TorchBtnSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		TorchBtnSlot->SetAlignment(FVector2D(0.f, 0.f));
		TorchBtnSlot->SetOffsets(FMargin(50.f, 290.f, 130.f, 55.f));

		// ============================================================
		//  LOBBY PLAYER LIST OVERLAY
		// ============================================================
		{
			const FSlateFontInfo LobbyTitleFont = FSlateFontInfo(
				FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);

			LobbyOverlay = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("LobbyOverlay"));
			LobbyOverlay->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
			LobbyOverlay->SetPadding(FMargin(0.f));
			LobbyOverlay->SetVisibility(ESlateVisibility::Collapsed);

			UCanvasPanelSlot* LobbyOvSlot = RootCanvas->AddChildToCanvas(LobbyOverlay);
			LobbyOvSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			LobbyOvSlot->SetOffsets(FMargin(0.f));

			// Inner panel — centered card
			UBorder* LobbyPanel = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("LobbyPanel"));
			LobbyPanel->SetBrushColor(FLinearColor(0.08f, 0.08f, 0.14f, 0.85f));
			LobbyPanel->SetPadding(FMargin(32.f, 28.f, 32.f, 28.f));
			LobbyPanel->SetHorizontalAlignment(HAlign_Center);
			LobbyPanel->SetVerticalAlignment(VAlign_Center);
			LobbyOverlay->SetContent(LobbyPanel);

			// Inner vertical layout
			UVerticalBox* LobbyVBox = WidgetTree->ConstructWidget<UVerticalBox>(
				UVerticalBox::StaticClass(), TEXT("LobbyVBox"));
			LobbyPanel->SetContent(LobbyVBox);

			// Header: "Players Online"
			UTextBlock* LobbyTitle = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("LobbyTitle"));
			LobbyTitle->SetText(FText::FromString(TEXT("Players Online")));
			LobbyTitle->SetFont(LobbyTitleFont);
			LobbyTitle->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			LobbyTitle->SetJustification(ETextJustify::Center);
			UVerticalBoxSlot* TitleSlot = LobbyVBox->AddChildToVerticalBox(LobbyTitle);
			TitleSlot->SetHorizontalAlignment(HAlign_Fill);
			TitleSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

			// Separator line
			USizeBox* SepSB = WidgetTree->ConstructWidget<USizeBox>(
				USizeBox::StaticClass(), TEXT("LobbySepSB"));
			SepSB->SetHeightOverride(1.f);
			UBorder* SepLine = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("LobbySepLine"));
			SepLine->SetBrushColor(FLinearColor(0.3f, 0.3f, 0.4f, 0.4f));
			SepLine->SetPadding(FMargin(0.f));
			SepSB->AddChild(SepLine);
			UVerticalBoxSlot* SepSlot = LobbyVBox->AddChildToVerticalBox(SepSB);
			SepSlot->SetHorizontalAlignment(HAlign_Fill);
			SepSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

			// Scrollable player list container
			UScrollBox* LobbyScroll = WidgetTree->ConstructWidget<UScrollBox>(
				UScrollBox::StaticClass(), TEXT("LobbyScroll"));

			LobbyPlayerList = WidgetTree->ConstructWidget<UVerticalBox>(
				UVerticalBox::StaticClass(), TEXT("LobbyPlayerList"));
			LobbyScroll->AddChild(LobbyPlayerList);

			UVerticalBoxSlot* ScrollSlot = LobbyVBox->AddChildToVerticalBox(LobbyScroll);
			ScrollSlot->SetHorizontalAlignment(HAlign_Fill);
			ScrollSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); // fill available space

		}

		// ============================================================
		//  MOBILE-ONLY: On-foot action buttons (bottom-right)
		// ============================================================
#if PLATFORM_IOS || PLATFORM_ANDROID
		const FLinearColor ActionBtnBg(0.15f, 0.15f, 0.15f, 0.7f);
		const FLinearColor ActionBtnHover(0.25f, 0.25f, 0.25f, 0.85f);
		const FLinearColor ActionBtnPress(0.35f, 0.35f, 0.35f, 0.95f);

		auto MakeActionButton = [&](const FName& Name, const FString& Label, float RightOffset, float BottomOffset, float Size) -> UButton*
		{
			UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
			FButtonStyle Style = Btn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(ActionBtnBg);
			Style.Hovered.TintColor = FSlateColor(ActionBtnHover);
			Style.Pressed.TintColor = FSlateColor(ActionBtnPress);
			Btn->SetStyle(Style);

			UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("_Label"))));
			BtnLabel->SetText(FText::FromString(Label));
			BtnLabel->SetFont(BtnFont);
			BtnLabel->SetColorAndOpacity(FSlateColor(White));
			BtnLabel->SetJustification(ETextJustify::Center);
			Btn->AddChild(BtnLabel);

			UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(Btn);
			Slot->SetAnchors(FAnchors(1.f, 1.f, 1.f, 1.f)); // bottom-right
			Slot->SetAlignment(FVector2D(1.f, 1.f));
			Slot->SetOffsets(FMargin(-RightOffset, -BottomOffset, Size, Size));
			return Btn;
		};

		// ============================================================
		//  MOBILE-ONLY: Walking touch panels (left = MOVE, right = LOOK)
		// ============================================================
		const FSlateFontInfo WalkZoneFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);

		// Move panel (left 45%, bottom 50%)
		// SelfHitTestInvisible so touches pass through to buttons underneath
		// (touch input is handled via raw GetInputTouchState in NativeTick)
		WalkMovePanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("WalkMovePanel"));
		WalkMovePanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		WalkMovePanel->SetPadding(FMargin(0.f));
		WalkMovePanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		WalkMoveLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("WalkMoveLabel"));
		WalkMoveLabel->SetText(FText::FromString(TEXT("MOVE")));
		WalkMoveLabel->SetFont(WalkZoneFont);
		WalkMoveLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.12f)));
		WalkMoveLabel->SetJustification(ETextJustify::Center);
		WalkMovePanel->SetContent(WalkMoveLabel);

		{
			UCanvasPanelSlot* WalkMoveSlot = RootCanvas->AddChildToCanvas(WalkMovePanel);
			WalkMoveSlot->SetAnchors(FAnchors(0.f, 0.5f, 0.45f, 1.f));
			WalkMoveSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));
		}

		// Look panel (right 55%, bottom 50%)
		// SelfHitTestInvisible so touches pass through to buttons underneath
		WalkLookPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("WalkLookPanel"));
		WalkLookPanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		WalkLookPanel->SetPadding(FMargin(0.f));
		WalkLookPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

		WalkLookLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("WalkLookLabel"));
		WalkLookLabel->SetText(FText::FromString(TEXT("LOOK")));
		WalkLookLabel->SetFont(WalkZoneFont);
		WalkLookLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.12f)));
		WalkLookLabel->SetJustification(ETextJustify::Center);
		WalkLookPanel->SetContent(WalkLookLabel);

		{
			UCanvasPanelSlot* WalkLookSlot = RootCanvas->AddChildToCanvas(WalkLookPanel);
			WalkLookSlot->SetAnchors(FAnchors(0.45f, 0.5f, 1.f, 1.f));
			WalkLookSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));
		}

		// ============================================================
		//  MOBILE-ONLY: Driving controls (hidden by default)
		// ============================================================

		const FSlateFontInfo DriveFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
		const FSlateFontInfo DriveFontSmall = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);

		// -- Steering panel (left side, bottom half) --
		// Touch position within this panel controls steering:
		//   Left side of panel = steer left, right side = steer right.
		SteeringPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("SteeringPanel"));
		SteeringPanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		SteeringPanel->SetPadding(FMargin(0.f));

		SteeringLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("SteeringLabel"));
		SteeringLabel->SetText(FText::FromString(FString::Printf(TEXT("%s   STEER   %s"),
			UTF8_TO_TCHAR("\xe2\x97\x84"), UTF8_TO_TCHAR("\xe2\x96\xba"))));
		SteeringLabel->SetFont(DriveFont);
		SteeringLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f)));
		SteeringLabel->SetJustification(ETextJustify::Center);
		SteeringPanel->SetContent(SteeringLabel);

		UCanvasPanelSlot* SteerSlot = RootCanvas->AddChildToCanvas(SteeringPanel);
		// Left 45% of screen, bottom 50% height
		SteerSlot->SetAnchors(FAnchors(0.f, 0.5f, 0.45f, 1.f));
		SteerSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));

		SteeringPanel->SetVisibility(ESlateVisibility::Collapsed);

		// -- Gas pedal (right side, lower portion) --
		GasPedalPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("GasPedalPanel"));
		GasPedalPanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		GasPedalPanel->SetPadding(FMargin(0.f));

		GasLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("GasLabel"));
		GasLabel->SetText(FText::FromString(TEXT("GAS")));
		GasLabel->SetFont(DriveFont);
		GasLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f)));
		GasLabel->SetJustification(ETextJustify::Center);
		GasPedalPanel->SetContent(GasLabel);

		UCanvasPanelSlot* GasSlot = RootCanvas->AddChildToCanvas(GasPedalPanel);
		// Right 45% of screen, bottom 35% height (above brake)
		GasSlot->SetAnchors(FAnchors(0.55f, 0.5f, 1.f, 0.75f));
		GasSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));

		GasPedalPanel->SetVisibility(ESlateVisibility::Collapsed);

		// -- Brake pedal (right side, very bottom) --
		BrakePedalPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("BrakePedalPanel"));
		BrakePedalPanel->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
		BrakePedalPanel->SetPadding(FMargin(0.f));

		BrakeLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("BrakeLabel"));
		BrakeLabel->SetText(FText::FromString(TEXT("BRAKE")));
		BrakeLabel->SetFont(DriveFontSmall);
		BrakeLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f)));
		BrakeLabel->SetJustification(ETextJustify::Center);
		BrakePedalPanel->SetContent(BrakeLabel);

		UCanvasPanelSlot* BrakeSlot = RootCanvas->AddChildToCanvas(BrakePedalPanel);
		// Right 45%, bottom 25% height
		BrakeSlot->SetAnchors(FAnchors(0.55f, 0.75f, 1.f, 1.f));
		BrakeSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));

		BrakePedalPanel->SetVisibility(ESlateVisibility::Collapsed);

		// -- Exit Vehicle button (top-left, where "Car" button is in walk mode) --
		ExitVehicleButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("ExitVehicleButton"));

		FButtonStyle ExitBtnStyle = ExitVehicleButton->GetStyle();
		ExitBtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.6f, 0.1f, 0.1f, 0.85f));
		ExitBtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.8f, 0.15f, 0.15f, 0.95f));
		ExitBtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.4f, 0.08f, 0.08f, 1.0f));
		ExitVehicleButton->SetStyle(ExitBtnStyle);

		UTextBlock* ExitLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ExitVehicleLabel"));
		ExitLabel->SetText(FText::FromString(TEXT("Exit")));
		ExitLabel->SetFont(BtnFont);
		ExitLabel->SetColorAndOpacity(FSlateColor(White));
		ExitLabel->SetJustification(ETextJustify::Center);
		ExitVehicleButton->AddChild(ExitLabel);

		UCanvasPanelSlot* ExitSlot = RootCanvas->AddChildToCanvas(ExitVehicleButton);
		ExitSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		ExitSlot->SetAlignment(FVector2D(0.f, 0.f));
		ExitSlot->SetOffsets(FMargin(50.f, 64.f, 130.f, 50.f));

		ExitVehicleButton->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  FIRE button -- large circular red, on top of drive panels
		// ============================================================
		FireButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("FireButton"));

		const float FireBtnSize = 168.f;
		const float CornerR = FireBtnSize * 0.5f;

		auto MakeRoundBrush = [&](const FLinearColor& Color) -> FSlateBrush
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(Color);
			Brush.OutlineSettings.CornerRadii = FVector4(CornerR, CornerR, CornerR, CornerR);
			Brush.OutlineSettings.Color = FSlateColor(FLinearColor::Transparent);
			Brush.OutlineSettings.Width = 0.f;
			return Brush;
		};

		FButtonStyle FireBtnStyle = FireButton->GetStyle();
		FireBtnStyle.Normal  = MakeRoundBrush(FLinearColor(0.7f, 0.1f, 0.1f, 0.85f));
		FireBtnStyle.Hovered = MakeRoundBrush(FLinearColor(0.85f, 0.15f, 0.15f, 0.95f));
		FireBtnStyle.Pressed = MakeRoundBrush(FLinearColor(0.5f, 0.08f, 0.08f, 1.0f));
		FireButton->SetStyle(FireBtnStyle);
		FireButton->SetTouchMethod(EButtonTouchMethod::DownAndUp);
		FireButton->SetClickMethod(EButtonClickMethod::MouseDown);
		FireButton->SetPressMethod(EButtonPressMethod::ButtonPress);

		UTextBlock* FireLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("FireLabel"));
		const FSlateFontInfo FireFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);
		FireLabel->SetText(FText::FromString(TEXT("FIRE")));
		FireLabel->SetFont(FireFont);
		FireLabel->SetColorAndOpacity(FSlateColor(White));
		FireLabel->SetJustification(ETextJustify::Center);
		FireButton->AddChild(FireLabel);

		UCanvasPanelSlot* FireBtnSlot = RootCanvas->AddChildToCanvas(FireButton);
		FireBtnSlot->SetAnchors(FAnchors(0.85f, 0.30f, 0.85f, 0.30f));
		FireBtnSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		FireBtnSlot->SetOffsets(FMargin(0.f, 0.f, FireBtnSize, FireBtnSize));
		FireButton->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  DROP button -- purple circle, bomb drop
		// ============================================================
		DropBombButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("DropBombButton"));

		const float DropBtnSize = 140.f;
		const float DropCornerR = DropBtnSize * 0.5f;

		auto MakeRoundBrushDrop = [&](const FLinearColor& Color) -> FSlateBrush
		{
			FSlateBrush Brush;
			Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
			Brush.TintColor = FSlateColor(Color);
			Brush.OutlineSettings.CornerRadii = FVector4(DropCornerR, DropCornerR, DropCornerR, DropCornerR);
			Brush.OutlineSettings.Color = FSlateColor(FLinearColor::Transparent);
			Brush.OutlineSettings.Width = 0.f;
			return Brush;
		};

		FButtonStyle DropBtnStyle = DropBombButton->GetStyle();
		DropBtnStyle.Normal  = MakeRoundBrushDrop(FLinearColor(0.15f, 0.08f, 0.45f, 0.85f));
		DropBtnStyle.Hovered = MakeRoundBrushDrop(FLinearColor(0.22f, 0.12f, 0.55f, 0.95f));
		DropBtnStyle.Pressed = MakeRoundBrushDrop(FLinearColor(0.10f, 0.05f, 0.35f, 1.0f));
		DropBombButton->SetStyle(DropBtnStyle);
		DropBombButton->SetTouchMethod(EButtonTouchMethod::DownAndUp);
		DropBombButton->SetClickMethod(EButtonClickMethod::MouseDown);
		DropBombButton->SetPressMethod(EButtonPressMethod::ButtonPress);

		UTextBlock* DropLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("DropBombLabel"));
		const FSlateFontInfo DropFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
		DropLabel->SetText(FText::FromString(TEXT("DROP")));
		DropLabel->SetFont(DropFont);
		DropLabel->SetColorAndOpacity(FSlateColor(White));
		DropLabel->SetJustification(ETextJustify::Center);
		DropBombButton->AddChild(DropLabel);

		UCanvasPanelSlot* DropBtnSlot = RootCanvas->AddChildToCanvas(DropBombButton);
		DropBtnSlot->SetAnchors(FAnchors(0.15f, 0.48f, 0.15f, 0.48f));
		DropBtnSlot->SetAlignment(FVector2D(0.5f, 1.f));
		DropBtnSlot->SetOffsets(FMargin(0.f, 0.f, DropBtnSize, DropBtnSize));
		DropBombButton->SetVisibility(ESlateVisibility::Collapsed);
#endif

		// -- Vehicle crosshair (for flight vehicles) --
		{
			const FLinearColor VehCrossColor(0.0f, 1.0f, 0.4f, 0.85f);
			const float Gap = 10.f, ArmW = 2.f, ArmL = 18.f, DotSz = 4.f;

			auto MakeVC = [&](const TCHAR* Name, float W, float H, float OffX, float OffY) -> UBorder*
			{
				UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
				B->SetBrushColor(VehCrossColor);
				B->SetPadding(FMargin(0.f));
				UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(B);
				S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
				S->SetAlignment(FVector2D(0.f, 0.f));
				S->SetOffsets(FMargin(OffX, OffY, W, H));
				S->SetAutoSize(false);
				B->SetVisibility(ESlateVisibility::Collapsed);
				return B;
			};

			// Center dot
			VehicleCrosshairDot = MakeVC(TEXT("VehCrossDot"), DotSz, DotSz,
				-DotSz * 0.5f, -DotSz * 0.5f);
			// Top arm
			VehicleCrosshairTop = MakeVC(TEXT("VehCrossTop"), ArmW, ArmL,
				-ArmW * 0.5f, -(Gap + ArmL));
			// Bottom arm
			VehicleCrosshairBottom = MakeVC(TEXT("VehCrossBot"), ArmW, ArmL,
				-ArmW * 0.5f, Gap);
			// Left arm
			VehicleCrosshairLeft = MakeVC(TEXT("VehCrossLeft"), ArmL, ArmW,
				-(Gap + ArmL), -ArmW * 0.5f);
			// Right arm
			VehicleCrosshairRight = MakeVC(TEXT("VehCrossRight"), ArmL, ArmW,
				Gap, -ArmW * 0.5f);
		}

		// Larger font for selection popup buttons
		const FSlateFontInfo PopupBtnFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);

		// ============================================================
		//  Vehicle selection popup (shown when "Car" button is tapped)
		// ============================================================
		VehicleSelectOverlay = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("VehicleSelectOverlay"));
		VehicleSelectOverlay->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		VehicleSelectOverlay->SetPadding(FMargin(0.f));

		UCanvasPanelSlot* OverlaySlot = RootCanvas->AddChildToCanvas(VehicleSelectOverlay);
		OverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		OverlaySlot->SetOffsets(FMargin(0.f));

		// Inner panel (centered container for the two buttons).
		UBorder* VehicleSelectPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("VehicleSelectPanel"));
		VehicleSelectPanel->SetBrushColor(FLinearColor(0.12f, 0.14f, 0.20f, 0.55f));
		VehicleSelectPanel->SetPadding(FMargin(40.f, 28.f, 40.f, 28.f));
		VehicleSelectPanel->SetHorizontalAlignment(HAlign_Center);
		VehicleSelectPanel->SetVerticalAlignment(VAlign_Center);
		VehicleSelectOverlay->SetContent(VehicleSelectPanel);

		UHorizontalBox* SelectHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("VehicleSelectHBox"));
		VehicleSelectPanel->SetContent(SelectHBox);

		// -- Aventador button (burnt orange) --
		SelectAventadorBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectAventadorBtn"));
		{
			FButtonStyle Style = SelectAventadorBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectAventadorBtn->SetStyle(Style);
		}
		UTextBlock* AventadorLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("AventadorSelectLabel"));
		AventadorLabel->SetText(FText::FromString(TEXT("Lamborghini")));
		AventadorLabel->SetFont(PopupBtnFont);
		AventadorLabel->SetColorAndOpacity(FSlateColor(White));
		AventadorLabel->SetJustification(ETextJustify::Center);
		SelectAventadorBtn->AddChild(AventadorLabel);

		UHorizontalBoxSlot* AvSlot = SelectHBox->AddChildToHorizontalBox(SelectAventadorBtn);
		AvSlot->SetPadding(FMargin(12.f));
		AvSlot->SetVerticalAlignment(VAlign_Center);
		AvSlot->SetHorizontalAlignment(HAlign_Center);

		// Spacer between buttons.
		USpacer* BtnSpacer = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("VehicleBtnSpacer"));
		BtnSpacer->SetSize(FVector2D(8.f, 0.f));
		SelectHBox->AddChildToHorizontalBox(BtnSpacer);

		// -- Dune Buggy button (olive green) --
		SelectBuggyBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectBuggyBtn"));
		{
			FButtonStyle Style = SelectBuggyBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectBuggyBtn->SetStyle(Style);
		}
		UTextBlock* BuggyLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("BuggySelectLabel"));
		BuggyLabel->SetText(FText::FromString(TEXT("Dune Buggy")));
		BuggyLabel->SetFont(PopupBtnFont);
		BuggyLabel->SetColorAndOpacity(FSlateColor(White));
		BuggyLabel->SetJustification(ETextJustify::Center);
		SelectBuggyBtn->AddChild(BuggyLabel);

		UHorizontalBoxSlot* BgSlot = SelectHBox->AddChildToHorizontalBox(SelectBuggyBtn);
		BgSlot->SetPadding(FMargin(12.f));
		BgSlot->SetVerticalAlignment(VAlign_Center);
		BgSlot->SetHorizontalAlignment(HAlign_Center);

		// Spacer between Buggy and Raptor buttons.
		USpacer* BtnSpacer2 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("VehicleBtnSpacer2"));
		BtnSpacer2->SetSize(FVector2D(8.f, 0.f));
		SelectHBox->AddChildToHorizontalBox(BtnSpacer2);

		// -- F-22 Raptor button (stealth gray) --
		SelectRaptorBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectRaptorBtn"));
		{
			FButtonStyle Style = SelectRaptorBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectRaptorBtn->SetStyle(Style);
		}
		UTextBlock* RaptorLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("RaptorSelectLabel"));
		RaptorLabel->SetText(FText::FromString(TEXT("F-22 Raptor")));
		RaptorLabel->SetFont(PopupBtnFont);
		RaptorLabel->SetColorAndOpacity(FSlateColor(White));
		RaptorLabel->SetJustification(ETextJustify::Center);
		SelectRaptorBtn->AddChild(RaptorLabel);

		UHorizontalBoxSlot* RpSlot = SelectHBox->AddChildToHorizontalBox(SelectRaptorBtn);
		RpSlot->SetPadding(FMargin(12.f));
		RpSlot->SetVerticalAlignment(VAlign_Center);
		RpSlot->SetHorizontalAlignment(HAlign_Center);

		USpacer* VehSpacer3 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("VehBtnSpacer3"));
		VehSpacer3->SetSize(FVector2D(8.f, 0.f));
		SelectHBox->AddChildToHorizontalBox(VehSpacer3);

		// -- Jetpack button (sky blue) --
		SelectJetpackBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectJetpackBtn"));
		{
			FButtonStyle Style = SelectJetpackBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectJetpackBtn->SetStyle(Style);
		}
		UTextBlock* JetpackLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("JetpackSelectLabel"));
		JetpackLabel->SetText(FText::FromString(TEXT("AirBoots")));
		JetpackLabel->SetFont(PopupBtnFont);
		JetpackLabel->SetColorAndOpacity(FSlateColor(White));
		JetpackLabel->SetJustification(ETextJustify::Center);
		SelectJetpackBtn->AddChild(JetpackLabel);

		UHorizontalBoxSlot* JpSlot = SelectHBox->AddChildToHorizontalBox(SelectJetpackBtn);
		JpSlot->SetPadding(FMargin(12.f));
		JpSlot->SetVerticalAlignment(VAlign_Center);
		JpSlot->SetHorizontalAlignment(HAlign_Center);

		// -- M60 Tank button (olive green) --
		SelectTankBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectTankBtn"));
		{
			FButtonStyle Style = SelectTankBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectTankBtn->SetStyle(Style);
		}
		UTextBlock* TankLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("TankSelectLabel"));
		TankLabel->SetText(FText::FromString(TEXT("M60 Tank")));
		TankLabel->SetFont(PopupBtnFont);
		TankLabel->SetColorAndOpacity(FSlateColor(White));
		TankLabel->SetJustification(ETextJustify::Center);
		SelectTankBtn->AddChild(TankLabel);

		UHorizontalBoxSlot* TankSlot = SelectHBox->AddChildToHorizontalBox(SelectTankBtn);
		TankSlot->SetPadding(FMargin(12.f));
		TankSlot->SetVerticalAlignment(VAlign_Center);
		TankSlot->SetHorizontalAlignment(HAlign_Center);

		// -- Dirt Bike button (red/orange) --
		SelectDirtBikeBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectDirtBikeBtn"));
		{
			FButtonStyle Style = SelectDirtBikeBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectDirtBikeBtn->SetStyle(Style);
		}
		UTextBlock* DirtBikeLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("DirtBikeSelectLabel"));
		DirtBikeLabel->SetText(FText::FromString(TEXT("Mountain Bike")));
		DirtBikeLabel->SetFont(PopupBtnFont);
		DirtBikeLabel->SetColorAndOpacity(FSlateColor(White));
		DirtBikeLabel->SetJustification(ETextJustify::Center);
		SelectDirtBikeBtn->AddChild(DirtBikeLabel);

		UHorizontalBoxSlot* BikeSlot = SelectHBox->AddChildToHorizontalBox(SelectDirtBikeBtn);
		BikeSlot->SetPadding(FMargin(12.f));
		BikeSlot->SetVerticalAlignment(VAlign_Center);
		BikeSlot->SetHorizontalAlignment(HAlign_Center);

		// -- Black Hawk button (dark military green) --
		SelectBlackHawkBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectBlackHawkBtn"));
		{
			FButtonStyle Style = SelectBlackHawkBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectBlackHawkBtn->SetStyle(Style);
		}
		UTextBlock* BlackHawkLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("BlackHawkSelectLabel"));
		BlackHawkLabel->SetText(FText::FromString(TEXT("Black Hawk")));
		BlackHawkLabel->SetFont(PopupBtnFont);
		BlackHawkLabel->SetColorAndOpacity(FSlateColor(White));
		BlackHawkLabel->SetJustification(ETextJustify::Center);
		SelectBlackHawkBtn->AddChild(BlackHawkLabel);

		UHorizontalBoxSlot* HawkSlot = SelectHBox->AddChildToHorizontalBox(SelectBlackHawkBtn);
		HawkSlot->SetPadding(FMargin(12.f));
		HawkSlot->SetVerticalAlignment(VAlign_Center);
		HawkSlot->SetHorizontalAlignment(HAlign_Center);

		// -- Roller Blades button (matching style) --
		SelectRollerBladesBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectRollerBladesBtn"));
		{
			FButtonStyle Style = SelectRollerBladesBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectRollerBladesBtn->SetStyle(Style);
		}
		UTextBlock* RollerBladesLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("RollerBladesSelectLabel"));
		RollerBladesLabel->SetText(FText::FromString(TEXT("Roller Blades")));
		RollerBladesLabel->SetFont(PopupBtnFont);
		RollerBladesLabel->SetColorAndOpacity(FSlateColor(White));
		RollerBladesLabel->SetJustification(ETextJustify::Center);
		SelectRollerBladesBtn->AddChild(RollerBladesLabel);

		UHorizontalBoxSlot* BladesSlot = SelectHBox->AddChildToHorizontalBox(SelectRollerBladesBtn);
		BladesSlot->SetPadding(FMargin(12.f));
		BladesSlot->SetVerticalAlignment(VAlign_Center);
		BladesSlot->SetHorizontalAlignment(HAlign_Center);

		VehicleSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  Weapon selection popup
		// ============================================================
		WeaponSelectOverlay = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("WeaponSelectOverlay"));
		WeaponSelectOverlay->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		WeaponSelectOverlay->SetPadding(FMargin(0.f));

		UCanvasPanelSlot* WepOverlaySlot = RootCanvas->AddChildToCanvas(WeaponSelectOverlay);
		WepOverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		WepOverlaySlot->SetOffsets(FMargin(0.f));

		UBorder* WepSelectPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("WeaponSelectPanel"));
		WepSelectPanel->SetBrushColor(FLinearColor(0.12f, 0.14f, 0.20f, 0.55f));
		WepSelectPanel->SetPadding(FMargin(40.f, 28.f, 40.f, 28.f));
		WepSelectPanel->SetHorizontalAlignment(HAlign_Center);
		WepSelectPanel->SetVerticalAlignment(VAlign_Center);
		WeaponSelectOverlay->SetContent(WepSelectPanel);

		UHorizontalBox* WepHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("WeaponSelectHBox"));
		WepSelectPanel->SetContent(WepHBox);

		// -- Gun button (steel gray) --
		SelectGunBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectGunBtn"));
		{
			FButtonStyle Style = SelectGunBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectGunBtn->SetStyle(Style);
		}
		UTextBlock* GunLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("GunSelectLabel"));
		GunLabel->SetText(FText::FromString(TEXT("Minigun")));
		GunLabel->SetFont(PopupBtnFont);
		GunLabel->SetColorAndOpacity(FSlateColor(White));
		GunLabel->SetJustification(ETextJustify::Center);
		SelectGunBtn->AddChild(GunLabel);

		UHorizontalBoxSlot* GunSlot = WepHBox->AddChildToHorizontalBox(SelectGunBtn);
		GunSlot->SetPadding(FMargin(12.f));
		GunSlot->SetVerticalAlignment(VAlign_Center);
		GunSlot->SetHorizontalAlignment(HAlign_Center);

		USpacer* WepSpacer1 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("WepBtnSpacer1"));
		WepSpacer1->SetSize(FVector2D(8.f, 0.f));
		WepHBox->AddChildToHorizontalBox(WepSpacer1);

		// -- RPG button (dark red) --
		SelectRPGBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectRPGBtn"));
		{
			FButtonStyle Style = SelectRPGBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectRPGBtn->SetStyle(Style);
		}
		UTextBlock* RPGLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("RPGSelectLabel"));
		RPGLabel->SetText(FText::FromString(TEXT("RPG")));
		RPGLabel->SetFont(PopupBtnFont);
		RPGLabel->SetColorAndOpacity(FSlateColor(White));
		RPGLabel->SetJustification(ETextJustify::Center);
		SelectRPGBtn->AddChild(RPGLabel);

		UHorizontalBoxSlot* RPGSlot = WepHBox->AddChildToHorizontalBox(SelectRPGBtn);
		RPGSlot->SetPadding(FMargin(12.f));
		RPGSlot->SetVerticalAlignment(VAlign_Center);
		RPGSlot->SetHorizontalAlignment(HAlign_Center);

		USpacer* WepSpacer2 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("WepBtnSpacer2"));
		WepSpacer2->SetSize(FVector2D(8.f, 0.f));
		WepHBox->AddChildToHorizontalBox(WepSpacer2);

		// -- Sword button (gold) --
		SelectSwordBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectSwordBtn"));
		{
			FButtonStyle Style = SelectSwordBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectSwordBtn->SetStyle(Style);
		}
		UTextBlock* SwordLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("SwordSelectLabel"));
		SwordLabel->SetText(FText::FromString(TEXT("Light Saber")));
		SwordLabel->SetFont(PopupBtnFont);
		SwordLabel->SetColorAndOpacity(FSlateColor(White));
		SwordLabel->SetJustification(ETextJustify::Center);
		SelectSwordBtn->AddChild(SwordLabel);

		UHorizontalBoxSlot* SwordSlot = WepHBox->AddChildToHorizontalBox(SelectSwordBtn);
		SwordSlot->SetPadding(FMargin(12.f));
		SwordSlot->SetVerticalAlignment(VAlign_Center);
		SwordSlot->SetHorizontalAlignment(HAlign_Center);

		USpacer* WepSpacer3 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("WepBtnSpacer3"));
		WepSpacer3->SetSize(FVector2D(8.f, 0.f));
		WepHBox->AddChildToHorizontalBox(WepSpacer3);

		// -- Scythe button (dark purple) --
		SelectScytheBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectScytheBtn"));
		{
			FButtonStyle Style = SelectScytheBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectScytheBtn->SetStyle(Style);
		}
		UTextBlock* ScytheLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ScytheSelectLabel"));
		ScytheLabel->SetText(FText::FromString(TEXT("Scythe")));
		ScytheLabel->SetFont(PopupBtnFont);
		ScytheLabel->SetColorAndOpacity(FSlateColor(White));
		ScytheLabel->SetJustification(ETextJustify::Center);
		SelectScytheBtn->AddChild(ScytheLabel);

		UHorizontalBoxSlot* ScytheSlot = WepHBox->AddChildToHorizontalBox(SelectScytheBtn);
		ScytheSlot->SetPadding(FMargin(12.f));
		ScytheSlot->SetVerticalAlignment(VAlign_Center);
		ScytheSlot->SetHorizontalAlignment(HAlign_Center);

		USpacer* WepSpacer4 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("WepBtnSpacer4"));
		WepSpacer4->SetSize(FVector2D(8.f, 0.f));
		WepHBox->AddChildToHorizontalBox(WepSpacer4);

		// -- Dagger button (crimson red — 1-hit kill) --
		SelectDaggerBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectDaggerBtn"));
		{
			FButtonStyle Style = SelectDaggerBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectDaggerBtn->SetStyle(Style);
		}
		UTextBlock* DaggerLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("DaggerSelectLabel"));
		DaggerLabel->SetText(FText::FromString(TEXT("1-Hit Dagger")));
		DaggerLabel->SetFont(PopupBtnFont);
		DaggerLabel->SetColorAndOpacity(FSlateColor(White));
		DaggerLabel->SetJustification(ETextJustify::Center);
		SelectDaggerBtn->AddChild(DaggerLabel);

		UHorizontalBoxSlot* DaggerSlot = WepHBox->AddChildToHorizontalBox(SelectDaggerBtn);
		DaggerSlot->SetPadding(FMargin(12.f));
		DaggerSlot->SetVerticalAlignment(VAlign_Center);
		DaggerSlot->SetHorizontalAlignment(HAlign_Center);

		USpacer* WepSpacer5 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("WepBtnSpacer5"));
		WepSpacer5->SetSize(FVector2D(8.f, 0.f));
		WepHBox->AddChildToHorizontalBox(WepSpacer5);

		// -- AK-47 button (dark olive/military green) --
		SelectAK47Btn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectAK47Btn"));
		{
			FButtonStyle Style = SelectAK47Btn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectAK47Btn->SetStyle(Style);
		}
		UTextBlock* AK47Label = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("AK47SelectLabel"));
		AK47Label->SetText(FText::FromString(TEXT("AK-47")));
		AK47Label->SetFont(PopupBtnFont);
		AK47Label->SetColorAndOpacity(FSlateColor(White));
		AK47Label->SetJustification(ETextJustify::Center);
		SelectAK47Btn->AddChild(AK47Label);

		UHorizontalBoxSlot* AK47Slot = WepHBox->AddChildToHorizontalBox(SelectAK47Btn);
		AK47Slot->SetPadding(FMargin(12.f));
		AK47Slot->SetVerticalAlignment(VAlign_Center);
		AK47Slot->SetHorizontalAlignment(HAlign_Center);
		SelectAK47Btn->SetVisibility(ESlateVisibility::Collapsed);

		// Spacer between AK-47 and Sniper
		USpacer* WepSpacer6 = WidgetTree->ConstructWidget<USpacer>(
			USpacer::StaticClass(), TEXT("WepBtnSpacer6"));
		WepSpacer6->SetSize(FVector2D(8.f, 0.f));
		WepHBox->AddChildToHorizontalBox(WepSpacer6);

		// -- 50 Cal Sniper button (dark steel/black) --
		SelectSniperBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectSniperBtn"));
		{
			FButtonStyle Style = SelectSniperBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectSniperBtn->SetStyle(Style);
		}
		UTextBlock* SniperLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("SniperSelectLabel"));
		SniperLabel->SetText(FText::FromString(TEXT("50 Cal Sniper")));
		SniperLabel->SetFont(PopupBtnFont);
		SniperLabel->SetColorAndOpacity(FSlateColor(White));
		SniperLabel->SetJustification(ETextJustify::Center);
		SelectSniperBtn->AddChild(SniperLabel);

		UHorizontalBoxSlot* SniperSlot = WepHBox->AddChildToHorizontalBox(SelectSniperBtn);
		SniperSlot->SetPadding(FMargin(12.f));
		SniperSlot->SetVerticalAlignment(VAlign_Center);
		SniperSlot->SetHorizontalAlignment(HAlign_Center);

		WeaponSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  Disaster selection overlay
		// ============================================================
		DisasterSelectOverlay = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("DisasterSelectOverlay"));
		DisasterSelectOverlay->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
		DisasterSelectOverlay->SetPadding(FMargin(0.f));

		UCanvasPanelSlot* DisOverlaySlot = RootCanvas->AddChildToCanvas(DisasterSelectOverlay);
		DisOverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		DisOverlaySlot->SetOffsets(FMargin(0.f));

		UBorder* DisSelectPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("DisasterSelectPanel"));
		DisSelectPanel->SetBrushColor(FLinearColor(0.12f, 0.14f, 0.20f, 0.55f));
		DisSelectPanel->SetPadding(FMargin(40.f, 28.f, 40.f, 28.f));
		DisSelectPanel->SetHorizontalAlignment(HAlign_Center);
		DisSelectPanel->SetVerticalAlignment(VAlign_Center);
		DisasterSelectOverlay->SetContent(DisSelectPanel);

		UHorizontalBox* DisHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("DisasterSelectHBox"));
		DisSelectPanel->SetContent(DisHBox);

		// -- Tornado button (gray) --
		SelectTornadoBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectTornadoBtn"));
		{
			FButtonStyle Style = SelectTornadoBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectTornadoBtn->SetStyle(Style);
		}
		{
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TornadoLabel"));
			L->SetText(FText::FromString(TEXT("Tornado")));
			L->SetFont(PopupBtnFont);
			L->SetColorAndOpacity(FSlateColor(White));
			L->SetJustification(ETextJustify::Center);
			SelectTornadoBtn->AddChild(L);
		}
		{
			UHorizontalBoxSlot* S = DisHBox->AddChildToHorizontalBox(SelectTornadoBtn);
			S->SetPadding(FMargin(12.f));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetHorizontalAlignment(HAlign_Center);
		}

		{ USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("DisSp1")); Sp->SetSize(FVector2D(8.f, 0.f)); DisHBox->AddChildToHorizontalBox(Sp); }

		// -- Earthquake button (brown) --
		SelectEarthquakeBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectEarthquakeBtn"));
		{
			FButtonStyle Style = SelectEarthquakeBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectEarthquakeBtn->SetStyle(Style);
		}
		{
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("QuakeLabel"));
			L->SetText(FText::FromString(TEXT("Quake")));
			L->SetFont(PopupBtnFont);
			L->SetColorAndOpacity(FSlateColor(White));
			L->SetJustification(ETextJustify::Center);
			SelectEarthquakeBtn->AddChild(L);
		}
		{
			UHorizontalBoxSlot* S = DisHBox->AddChildToHorizontalBox(SelectEarthquakeBtn);
			S->SetPadding(FMargin(12.f));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetHorizontalAlignment(HAlign_Center);
		}

		{ USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("DisSp2")); Sp->SetSize(FVector2D(8.f, 0.f)); DisHBox->AddChildToHorizontalBox(Sp); }

		// -- Thunderstorm button (purple) --
		SelectThunderstormBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectThunderstormBtn"));
		{
			FButtonStyle Style = SelectThunderstormBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectThunderstormBtn->SetStyle(Style);
		}
		{
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ThunderLabel"));
			L->SetText(FText::FromString(TEXT("Thunder")));
			L->SetFont(PopupBtnFont);
			L->SetColorAndOpacity(FSlateColor(White));
			L->SetJustification(ETextJustify::Center);
			SelectThunderstormBtn->AddChild(L);
		}
		{
			UHorizontalBoxSlot* S = DisHBox->AddChildToHorizontalBox(SelectThunderstormBtn);
			S->SetPadding(FMargin(12.f));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetHorizontalAlignment(HAlign_Center);
		}

		{ USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("DisSp3")); Sp->SetSize(FVector2D(8.f, 0.f)); DisHBox->AddChildToHorizontalBox(Sp); }

		// -- Meteor button (orange-red) --
		SelectMeteorBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("SelectMeteorBtn"));
		{
			FButtonStyle Style = SelectMeteorBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			SelectMeteorBtn->SetStyle(Style);
		}
		{
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("MeteorLabel"));
			L->SetText(FText::FromString(TEXT("Meteor")));
			L->SetFont(PopupBtnFont);
			L->SetColorAndOpacity(FSlateColor(White));
			L->SetJustification(ETextJustify::Center);
			SelectMeteorBtn->AddChild(L);
		}
		{
			UHorizontalBoxSlot* S = DisHBox->AddChildToHorizontalBox(SelectMeteorBtn);
			S->SetPadding(FMargin(12.f));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetHorizontalAlignment(HAlign_Center);
		}

		{ USpacer* Sp = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("DisSp4")); Sp->SetSize(FVector2D(8.f, 0.f)); DisHBox->AddChildToHorizontalBox(Sp); }

		// -- Stop button (dark gray) --
		StopDisasterBtn = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("StopDisasterBtn"));
		{
			FButtonStyle Style = StopDisasterBtn->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.78f, 0.82f, 0.90f, 0.18f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.85f, 0.90f, 0.97f, 0.30f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.92f, 0.95f, 1.00f, 0.45f));
			Style.NormalPadding = FMargin(28.f, 18.f);
			Style.PressedPadding = FMargin(28.f, 18.f);
			StopDisasterBtn->SetStyle(Style);
		}
		{
			UTextBlock* L = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("StopDisLabel"));
			L->SetText(FText::FromString(TEXT("Stop")));
			L->SetFont(PopupBtnFont);
			L->SetColorAndOpacity(FSlateColor(White));
			L->SetJustification(ETextJustify::Center);
			StopDisasterBtn->AddChild(L);
		}
		{
			UHorizontalBoxSlot* S = DisHBox->AddChildToHorizontalBox(StopDisasterBtn);
			S->SetPadding(FMargin(12.f));
			S->SetVerticalAlignment(VAlign_Center);
			S->SetHorizontalAlignment(HAlign_Center);
		}

		DisasterSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  Weapon fire panel (large, barely visible — like steering panel)
		//  Right 45% of screen, bottom 50% height
		// ============================================================
		WeaponFireButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("WeaponFireButton"));
		{
			FButtonStyle Style = WeaponFireButton->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.f, 0.f, 0.f, 0.f));
			WeaponFireButton->SetStyle(Style);
		}
		WeaponFireButton->SetTouchMethod(EButtonTouchMethod::DownAndUp);
		WeaponFireButton->SetClickMethod(EButtonClickMethod::MouseDown);
		WeaponFireButton->SetPressMethod(EButtonPressMethod::ButtonPress);

		WeaponFireLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("WeaponFireLabel"));
		WeaponFireLabel->SetText(FText::FromString(TEXT("SPRAY")));
		const FSlateFontInfo WepFont = FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
		WeaponFireLabel->SetFont(WepFont);
		WeaponFireLabel->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f)));
		WeaponFireLabel->SetJustification(ETextJustify::Center);
		WeaponFireButton->AddChild(WeaponFireLabel);

		UCanvasPanelSlot* WepFireSlot = RootCanvas->AddChildToCanvas(WeaponFireButton);
		WepFireSlot->SetAnchors(FAnchors(0.55f, 0.75f, 1.f, 1.f));
		WepFireSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));

		WeaponFireButton->SetVisibility(ESlateVisibility::Collapsed);

		// ============================================================
		//  TOP-LEFT: Multiplayer buttons (below Car button)
		// ============================================================
		HostButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("HostButton"));
		{
			FButtonStyle Style = HostButton->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.1f, 0.35f, 0.1f, 0.85f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.45f, 0.15f, 0.95f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.25f, 0.08f, 1.0f));
			HostButton->SetStyle(Style);
		}

		UTextBlock* HostLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("HostLabel"));
		HostLabel->SetText(FText::FromString(TEXT("Host")));
		HostLabel->SetFont(BtnFont);
		HostLabel->SetColorAndOpacity(FSlateColor(White));
		HostLabel->SetJustification(ETextJustify::Center);
		HostButton->AddChild(HostLabel);

		UCanvasPanelSlot* HostSlot = RootCanvas->AddChildToCanvas(HostButton);
		HostSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		HostSlot->SetAlignment(FVector2D(0.f, 0.f));
		HostSlot->SetOffsets(FMargin(80.f, 112.f, 80.f, 40.f));
		HostButton->SetVisibility(ESlateVisibility::Collapsed);

		JoinButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("JoinButton"));
		{
			FButtonStyle Style = JoinButton->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.1f, 0.2f, 0.45f, 0.85f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.28f, 0.55f, 0.95f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.15f, 0.35f, 1.0f));
			JoinButton->SetStyle(Style);
		}

		UTextBlock* JoinLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("JoinLabel"));
		JoinLabel->SetText(FText::FromString(TEXT("Join")));
		JoinLabel->SetFont(BtnFont);
		JoinLabel->SetColorAndOpacity(FSlateColor(White));
		JoinLabel->SetJustification(ETextJustify::Center);
		JoinButton->AddChild(JoinLabel);

		UCanvasPanelSlot* JoinSlot = RootCanvas->AddChildToCanvas(JoinButton);
		JoinSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
		JoinSlot->SetAlignment(FVector2D(0.f, 0.f));
		JoinSlot->SetOffsets(FMargin(80.f, 160.f, 80.f, 40.f));
		JoinButton->SetVisibility(ESlateVisibility::Collapsed);

		// Player count display (top-right, below currency)
		PlayerCountText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("PlayerCountText"));
		PlayerCountText->SetText(FText::FromString(TEXT("Players: 1")));
		PlayerCountText->SetFont(BtnFont);
		PlayerCountText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.7f)));

		UCanvasPanelSlot* PlayerCountSlot = RootCanvas->AddChildToCanvas(PlayerCountText);
		PlayerCountSlot->SetAnchors(FAnchors(1.f, 0.f, 1.f, 0.f));
		PlayerCountSlot->SetAlignment(FVector2D(1.f, 0.f));
		PlayerCountSlot->SetOffsets(FMargin(-80.f, 56.f, 0.f, 0.f));
		PlayerCountSlot->SetAutoSize(true);

		// -- Join IP input overlay (hidden by default) --
		JoinOverlay = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("JoinOverlay"));
		JoinOverlay->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
		JoinOverlay->SetPadding(FMargin(0.f));

		UCanvasPanelSlot* JoinOverlaySlot = RootCanvas->AddChildToCanvas(JoinOverlay);
		JoinOverlaySlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		JoinOverlaySlot->SetOffsets(FMargin(0.f));

		UBorder* JoinPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("JoinPanel"));
		JoinPanel->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.08f, 0.95f));
		JoinPanel->SetPadding(FMargin(24.f, 16.f, 24.f, 16.f));
		JoinPanel->SetHorizontalAlignment(HAlign_Center);
		JoinPanel->SetVerticalAlignment(VAlign_Center);
		JoinOverlay->SetContent(JoinPanel);

		UHorizontalBox* JoinHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(), TEXT("JoinHBox"));
		JoinPanel->SetContent(JoinHBox);

		UTextBlock* IPLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("IPLabel"));
		IPLabel->SetText(FText::FromString(TEXT("IP: ")));
		IPLabel->SetFont(BtnFont);
		IPLabel->SetColorAndOpacity(FSlateColor(White));
		UHorizontalBoxSlot* IPLabelSlot = JoinHBox->AddChildToHorizontalBox(IPLabel);
		IPLabelSlot->SetVerticalAlignment(VAlign_Center);
		IPLabelSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

		IPInputText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("IPInputText"));
		IPInputText->SetText(FText::FromString(TEXT("GAME_SERVER_IP")));
		IPInputText->SetFont(BtnFont);
		IPInputText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 1.f, 0.5f)));
		UHorizontalBoxSlot* IPInputSlot = JoinHBox->AddChildToHorizontalBox(IPInputText);
		IPInputSlot->SetVerticalAlignment(VAlign_Center);
		IPInputSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

		// Number pad buttons for IP input (0-9 + backspace + connect + cancel)
		// For V1 simplicity, just use Connect and Cancel buttons with a hardcoded IP
		JoinConnectButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("JoinConnectButton"));
		{
			FButtonStyle Style = JoinConnectButton->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.1f, 0.35f, 0.1f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.15f, 0.45f, 0.15f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.25f, 0.08f, 1.0f));
			JoinConnectButton->SetStyle(Style);
		}
		UTextBlock* ConnectLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("ConnectLabel"));
		ConnectLabel->SetText(FText::FromString(TEXT("Connect")));
		ConnectLabel->SetFont(BtnFont);
		ConnectLabel->SetColorAndOpacity(FSlateColor(White));
		ConnectLabel->SetJustification(ETextJustify::Center);
		JoinConnectButton->AddChild(ConnectLabel);

		UHorizontalBoxSlot* ConnectSlot = JoinHBox->AddChildToHorizontalBox(JoinConnectButton);
		ConnectSlot->SetPadding(FMargin(4.f));
		ConnectSlot->SetVerticalAlignment(VAlign_Center);

		JoinCancelButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(), TEXT("JoinCancelButton"));
		{
			FButtonStyle Style = JoinCancelButton->GetStyle();
			Style.Normal.TintColor  = FSlateColor(FLinearColor(0.5f, 0.1f, 0.1f, 0.9f));
			Style.Hovered.TintColor = FSlateColor(FLinearColor(0.65f, 0.15f, 0.15f, 1.0f));
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.35f, 0.08f, 0.08f, 1.0f));
			JoinCancelButton->SetStyle(Style);
		}
		UTextBlock* CancelLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), TEXT("CancelLabel"));
		CancelLabel->SetText(FText::FromString(TEXT("Cancel")));
		CancelLabel->SetFont(BtnFont);
		CancelLabel->SetColorAndOpacity(FSlateColor(White));
		CancelLabel->SetJustification(ETextJustify::Center);
		JoinCancelButton->AddChild(CancelLabel);

		UHorizontalBoxSlot* CancelSlot = JoinHBox->AddChildToHorizontalBox(JoinCancelButton);
		CancelSlot->SetPadding(FMargin(4.f));
		CancelSlot->SetVerticalAlignment(VAlign_Center);

		JoinOverlay->SetVisibility(ESlateVisibility::Collapsed);
		EnteredIP = TEXT("GAME_SERVER_IP");

		// ============================================================
		//  DAMAGE FLASH — full-screen translucent red overlay
		// ============================================================
		DamageFlashOverlay = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), TEXT("DamageFlashOverlay"));
		DamageFlashOverlay->SetBrushColor(FLinearColor(0.85f, 0.05f, 0.05f, 0.0f));
		DamageFlashOverlay->SetPadding(FMargin(0.f));

		UCanvasPanelSlot* FlashSlot = RootCanvas->AddChildToCanvas(DamageFlashOverlay);
		FlashSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); // stretch full screen
		FlashSlot->SetOffsets(FMargin(0.f));
		DamageFlashOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);

		// ============================================================
		//  COD-style CROSSHAIR — dot + 4 lines, center of screen
		// ============================================================
		{
			// Green symmetrical crosshair (same style as F-22)
			const FLinearColor CHColor(0.0f, 1.0f, 0.4f, 0.85f);
			const float DotSz = 4.f, ArmW = 2.f, ArmL = 18.f, Gap = 10.f;

			auto MakeCH = [&](const TCHAR* Name, float W, float H, float OffX, float OffY) -> UBorder*
			{
				UBorder* B = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
				B->SetBrushColor(CHColor);
				B->SetPadding(FMargin(0.f));
				UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(B);
				S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
				S->SetAlignment(FVector2D(0.f, 0.f));
				S->SetOffsets(FMargin(OffX, OffY, W, H));
				S->SetAutoSize(false);
				B->SetVisibility(ESlateVisibility::Collapsed);
				return B;
			};

			CrosshairDot    = MakeCH(TEXT("CrosshairDot"),    DotSz, DotSz, -DotSz * 0.5f, -DotSz * 0.5f);
			CrosshairTop    = MakeCH(TEXT("CrosshairTop"),    ArmW, ArmL, -ArmW * 0.5f, -(Gap + ArmL));
			CrosshairBottom = MakeCH(TEXT("CrosshairBottom"), ArmW, ArmL, -ArmW * 0.5f, Gap);
			CrosshairLeft   = MakeCH(TEXT("CrosshairLeft"),   ArmL, ArmW, -(Gap + ArmL), -ArmW * 0.5f);
			CrosshairRight  = MakeCH(TEXT("CrosshairRight"),  ArmL, ArmW, Gap, -ArmW * 0.5f);
		}

		// ============================================================
		//  HIT MARKER — 4 short bars forming X at crosshair
		// ============================================================
		{
			const FLinearColor HMColor(1.f, 1.f, 1.f, 0.95f);
			const float HML = 10.f, HMW = 2.f, HMGap = 5.f;

			auto MakeHM = [&](const FName& N, float OX, float OY) -> UBorder*
			{
				UBorder* L = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), N);
				L->SetBrushColor(HMColor);
				UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(L);
				S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
				S->SetAlignment(FVector2D(0.5f, 0.5f));
				S->SetOffsets(FMargin(OX, OY, HML, HMW));
				L->SetVisibility(ESlateVisibility::Collapsed);
				return L;
			};

			HitMarkerTL = MakeHM(TEXT("HitMarkerTL"), -(HMGap + HML), -(HMGap + HMW));
			HitMarkerTR = MakeHM(TEXT("HitMarkerTR"), HMGap, -(HMGap + HMW));
			HitMarkerBL = MakeHM(TEXT("HitMarkerBL"), -(HMGap + HML), HMGap);
			HitMarkerBR = MakeHM(TEXT("HitMarkerBR"), HMGap, HMGap);
		}

		// ============================================================
		//  Mobile ADS button (iOS/Android only)
		// ============================================================
#if PLATFORM_IOS || PLATFORM_ANDROID
		{
			ADSButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ADSButton"));
			{
				FButtonStyle Style = ADSButton->GetStyle();
				Style.Normal.TintColor  = FSlateColor(FLinearColor(0.3f, 0.3f, 0.3f, 0.5f));
				Style.Hovered.TintColor = FSlateColor(FLinearColor(0.3f, 0.3f, 0.3f, 0.5f));
				Style.Pressed.TintColor = FSlateColor(FLinearColor(0.1f, 0.4f, 0.8f, 0.7f));
				ADSButton->SetStyle(Style);
			}
			UTextBlock* ADSLabel = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("ADSLabel"));
			ADSLabel->SetText(FText::FromString(TEXT("ADS")));
			ADSLabel->SetFont(BtnFont);
			ADSLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			ADSLabel->SetJustification(ETextJustify::Center);
			ADSButton->AddChild(ADSLabel);

			UCanvasPanelSlot* ADSSlot = RootCanvas->AddChildToCanvas(ADSButton);
			ADSSlot->SetAnchors(FAnchors(0.45f, 0.85f, 0.45f, 0.85f));
			ADSSlot->SetAlignment(FVector2D(0.5f, 0.5f));
			ADSSlot->SetOffsets(FMargin(0.f, 0.f, 70.f, 70.f));
			ADSButton->SetVisibility(ESlateVisibility::Collapsed);
		}
#endif

		// ============================================================
		//  COD-style sniper scope overlay
		//  4 black bars leave a viewport opening in the center.
		//  Thin crosshair lines + center dot inside the viewport.
		//  The viewport opening: X 0.2–0.8, Y 0.15–0.85
		// ============================================================
		{
			const FLinearColor ScopeBlack(0.f, 0.f, 0.f, 1.f);

			// Top black bar
			ScopeBlackTop = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("ScopeBlackTop"));
			ScopeBlackTop->SetBrushColor(ScopeBlack);
			{
				UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(ScopeBlackTop);
				S->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.15f));
				S->SetOffsets(FMargin(0.f));
			}
			ScopeBlackTop->SetVisibility(ESlateVisibility::Collapsed);

			// Bottom black bar
			ScopeBlackBottom = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("ScopeBlackBottom"));
			ScopeBlackBottom->SetBrushColor(ScopeBlack);
			{
				UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(ScopeBlackBottom);
				S->SetAnchors(FAnchors(0.f, 0.85f, 1.f, 1.f));
				S->SetOffsets(FMargin(0.f));
			}
			ScopeBlackBottom->SetVisibility(ESlateVisibility::Collapsed);

			// Left black bar (between top and bottom bars)
			ScopeBlackLeft = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("ScopeBlackLeft"));
			ScopeBlackLeft->SetBrushColor(ScopeBlack);
			{
				UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(ScopeBlackLeft);
				S->SetAnchors(FAnchors(0.f, 0.15f, 0.2f, 0.85f));
				S->SetOffsets(FMargin(0.f));
			}
			ScopeBlackLeft->SetVisibility(ESlateVisibility::Collapsed);

			// Right black bar (between top and bottom bars)
			ScopeBlackRight = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("ScopeBlackRight"));
			ScopeBlackRight->SetBrushColor(ScopeBlack);
			{
				UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(ScopeBlackRight);
				S->SetAnchors(FAnchors(0.8f, 0.15f, 1.f, 0.85f));
				S->SetOffsets(FMargin(0.f));
			}
			ScopeBlackRight->SetVisibility(ESlateVisibility::Collapsed);

			// Horizontal crosshair line — spans viewport width (bright white, 2px)
			ScopeCrosshairH = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("ScopeCrosshairH"));
			ScopeCrosshairH->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.9f));
			{
				UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(ScopeCrosshairH);
				S->SetAnchors(FAnchors(0.2f, 0.5f, 0.8f, 0.5f));
				S->SetAlignment(FVector2D(0.f, 0.5f));
				S->SetOffsets(FMargin(0.f, 0.f, 0.f, 2.f)); // 2px height
			}
			ScopeCrosshairH->SetVisibility(ESlateVisibility::Collapsed);

			// Vertical crosshair line — spans viewport height (bright white, 2px)
			ScopeCrosshairV = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("ScopeCrosshairV"));
			ScopeCrosshairV->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.9f));
			{
				UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(ScopeCrosshairV);
				S->SetAnchors(FAnchors(0.5f, 0.15f, 0.5f, 0.85f));
				S->SetAlignment(FVector2D(0.5f, 0.f));
				S->SetOffsets(FMargin(0.f, 0.f, 2.f, 0.f)); // 2px width
			}
			ScopeCrosshairV->SetVisibility(ESlateVisibility::Collapsed);

			// Center dot — red aiming point (COD style, 5x5 px)
			ScopeCrosshairDot = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), TEXT("ScopeCrosshairDot"));
			ScopeCrosshairDot->SetBrushColor(FLinearColor(1.f, 0.1f, 0.1f, 1.f));
			{
				UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(ScopeCrosshairDot);
				S->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
				S->SetAlignment(FVector2D(0.5f, 0.5f));
				S->SetOffsets(FMargin(0.f, 0.f, 5.f, 5.f)); // 5x5 px dot
			}
			ScopeCrosshairDot->SetVisibility(ESlateVisibility::Collapsed);

			// Scope zoom buttons (+ and - on left side, zoom level label)
			{
				ScopeZoomInBtn = WidgetTree->ConstructWidget<UButton>(
					UButton::StaticClass(), TEXT("ScopeZoomInBtn"));
				FButtonStyle ZInStyle = ScopeZoomInBtn->GetStyle();
				ZInStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.15f, 0.15f, 0.15f, 0.8f));
				ZInStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.25f, 0.25f, 0.25f, 0.9f));
				ZInStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.f));
				ScopeZoomInBtn->SetStyle(ZInStyle);
				UTextBlock* ZInLabel = WidgetTree->ConstructWidget<UTextBlock>(
					UTextBlock::StaticClass(), TEXT("ZInLabel"));
				ZInLabel->SetText(FText::FromString(TEXT("+")));
				ZInLabel->SetFont(BtnFont);
				ZInLabel->SetColorAndOpacity(FSlateColor(White));
				ZInLabel->SetJustification(ETextJustify::Center);
				ScopeZoomInBtn->AddChild(ZInLabel);
				UCanvasPanelSlot* ZInSlot = RootCanvas->AddChildToCanvas(ScopeZoomInBtn);
				ZInSlot->SetAnchors(FAnchors(0.f, 0.5f, 0.f, 0.5f));
				ZInSlot->SetAlignment(FVector2D(0.f, 1.f));
				ZInSlot->SetOffsets(FMargin(20.f, -30.f, 60.f, 60.f));
				ScopeZoomInBtn->SetVisibility(ESlateVisibility::Collapsed);

				ScopeZoomOutBtn = WidgetTree->ConstructWidget<UButton>(
					UButton::StaticClass(), TEXT("ScopeZoomOutBtn"));
				FButtonStyle ZOutStyle = ScopeZoomOutBtn->GetStyle();
				ZOutStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.15f, 0.15f, 0.15f, 0.8f));
				ZOutStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.25f, 0.25f, 0.25f, 0.9f));
				ZOutStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.08f, 0.08f, 0.08f, 1.f));
				ScopeZoomOutBtn->SetStyle(ZOutStyle);
				UTextBlock* ZOutLabel = WidgetTree->ConstructWidget<UTextBlock>(
					UTextBlock::StaticClass(), TEXT("ZOutLabel"));
				ZOutLabel->SetText(FText::FromString(TEXT("-")));
				ZOutLabel->SetFont(BtnFont);
				ZOutLabel->SetColorAndOpacity(FSlateColor(White));
				ZOutLabel->SetJustification(ETextJustify::Center);
				ScopeZoomOutBtn->AddChild(ZOutLabel);
				UCanvasPanelSlot* ZOutSlot = RootCanvas->AddChildToCanvas(ScopeZoomOutBtn);
				ZOutSlot->SetAnchors(FAnchors(0.f, 0.5f, 0.f, 0.5f));
				ZOutSlot->SetAlignment(FVector2D(0.f, 0.f));
				ZOutSlot->SetOffsets(FMargin(20.f, 30.f, 60.f, 60.f));
				ScopeZoomOutBtn->SetVisibility(ESlateVisibility::Collapsed);

				ScopeZoomLabel = WidgetTree->ConstructWidget<UTextBlock>(
					UTextBlock::StaticClass(), TEXT("ScopeZoomLabel"));
				ScopeZoomLabel->SetText(FText::FromString(TEXT("10x")));
				ScopeZoomLabel->SetFont(BtnFont);
				ScopeZoomLabel->SetColorAndOpacity(FSlateColor(White));
				ScopeZoomLabel->SetJustification(ETextJustify::Center);
				UCanvasPanelSlot* ZLabelSlot = RootCanvas->AddChildToCanvas(ScopeZoomLabel);
				ZLabelSlot->SetAnchors(FAnchors(0.f, 0.5f, 0.f, 0.5f));
				ZLabelSlot->SetAlignment(FVector2D(0.f, 0.5f));
				ZLabelSlot->SetOffsets(FMargin(20.f, 0.f, 60.f, 24.f));
				ScopeZoomLabel->SetVisibility(ESlateVisibility::Collapsed);
			}

			// Thermal scope toggle button (right side when scoped)
			ThermalToggleBtn = WidgetTree->ConstructWidget<UButton>(
				UButton::StaticClass(), TEXT("ThermalToggleBtn"));
			FButtonStyle ThermStyle = ThermalToggleBtn->GetStyle();
			ThermStyle.Normal.TintColor  = FSlateColor(FLinearColor(0.12f, 0.18f, 0.20f, 0.8f));
			ThermStyle.Hovered.TintColor = FSlateColor(FLinearColor(0.18f, 0.25f, 0.28f, 0.9f));
			ThermStyle.Pressed.TintColor = FSlateColor(FLinearColor(0.06f, 0.10f, 0.12f, 1.f));
			ThermalToggleBtn->SetStyle(ThermStyle);

			ThermalToggleLabel = WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(), TEXT("ThermalToggleLabel"));
			ThermalToggleLabel->SetText(FText::FromString(TEXT("Thermal")));
			ThermalToggleLabel->SetFont(BtnFont);
			ThermalToggleLabel->SetColorAndOpacity(FSlateColor(White));
			ThermalToggleLabel->SetJustification(ETextJustify::Center);
			ThermalToggleBtn->AddChild(ThermalToggleLabel);

			UCanvasPanelSlot* ThermSlot = RootCanvas->AddChildToCanvas(ThermalToggleBtn);
			ThermSlot->SetAnchors(FAnchors(1.f, 0.5f, 1.f, 0.5f));
			ThermSlot->SetAlignment(FVector2D(1.f, 0.5f));
			ThermSlot->SetOffsets(FMargin(-20.f, 0.f, 110.f, 50.f));
			ThermalToggleBtn->SetVisibility(ESlateVisibility::Collapsed);

			// Unused — nulled out
			ScopeOverlay = nullptr;
			ScopeRingBorder = nullptr;
		}

		UE_LOG(LogPlayhouseCity, Log, TEXT("UPHCCityHUD: Widget tree built programmatically."));
	}

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct / NativeDestruct
// ---------------------------------------------------------------------------

void UPHCCityHUD::NativeConstruct()
{
	Super::NativeConstruct();


	if (MenuButton)
	{
		MenuButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnMenuButtonClicked);
	}
	if (CarButton)
	{
		CarButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnCarButtonClicked);
	}
	if (EnterVehicleButton)
	{
		EnterVehicleButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnEnterVehicleClicked);
	}
	if (SelectAventadorBtn)
	{
		SelectAventadorBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectAventador);
	}
	if (SelectBuggyBtn)
	{
		SelectBuggyBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectBuggy);
	}
	if (SelectRaptorBtn)
	{
		SelectRaptorBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectRaptor);
	}
	if (SelectJetpackBtn)
	{
		SelectJetpackBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectJetpack);
	}
	if (SelectTankBtn)
	{
		SelectTankBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectTank);
	}
	if (SelectDirtBikeBtn)
	{
		SelectDirtBikeBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectDirtBike);
	}
	if (SelectBlackHawkBtn)
	{
		SelectBlackHawkBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectBlackHawk);
	}
	if (SelectRollerBladesBtn)
	{
		SelectRollerBladesBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectRollerBlades);
	}
	if (FireButton)
	{
		FireButton->OnPressed.AddDynamic(this, &UPHCCityHUD::OnFireButtonPressed);
		FireButton->OnReleased.AddDynamic(this, &UPHCCityHUD::OnFireButtonReleased);
	}
	if (DropBombButton)
	{
		DropBombButton->OnPressed.AddDynamic(this, &UPHCCityHUD::OnDropBombClicked);
	}
	if (WeaponsButton)
	{
		WeaponsButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnWeaponsButtonClicked);
	}
	if (NightButton)
	{
		NightButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnNightButtonClicked);
	}
	if (DevModeButton)
	{
		DevModeButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDevModeButtonClicked);
	}
	// Bind dev time buttons
	if (DevTimeBtns.Num() >= 8)
	{
		DevTimeBtns[0]->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDevTimeSunrise);
		DevTimeBtns[1]->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDevTimeMorning);
		DevTimeBtns[2]->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDevTimeNoon);
		DevTimeBtns[3]->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDevTimeAfternoon);
		DevTimeBtns[4]->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDevTimeSunset);
		DevTimeBtns[5]->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDevTimeDusk);
		DevTimeBtns[6]->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDevTimeMidnight);
		DevTimeBtns[7]->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDevTimeCycle);
	}
	if (DisastersButton)
	{
		DisastersButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDisastersButtonClicked);
	}
	if (CameraModeButton)
	{
		CameraModeButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnCameraModeButtonClicked);
	}
	if (TorchButton)
	{
		TorchButton->SetTouchMethod(EButtonTouchMethod::DownAndUp);
		TorchButton->SetPressMethod(EButtonPressMethod::ButtonPress);
		TorchButton->OnPressed.AddDynamic(this, &UPHCCityHUD::OnLobbyButtonClicked);
		TorchButton->OnReleased.AddDynamic(this, &UPHCCityHUD::OnLobbyCloseClicked);
	}
	if (DailyLoginClaimBtn)
	{
		DailyLoginClaimBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnDailyLoginClaimClicked);
	}
	if (ScopeZoomInBtn)
	{
		ScopeZoomInBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnScopeZoomInPressed);
	}
	if (ScopeZoomOutBtn)
	{
		ScopeZoomOutBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnScopeZoomOutPressed);
	}
	if (ThermalToggleBtn)
	{
		ThermalToggleBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnThermalTogglePressed);
	}
	if (SelectTornadoBtn)
	{
		SelectTornadoBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectTornado);
	}
	if (SelectEarthquakeBtn)
	{
		SelectEarthquakeBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectEarthquake);
	}
	if (SelectThunderstormBtn)
	{
		SelectThunderstormBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectThunderstorm);
	}
	if (SelectMeteorBtn)
	{
		SelectMeteorBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectMeteor);
	}
	if (StopDisasterBtn)
	{
		StopDisasterBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnStopDisaster);
	}
	if (SelectGunBtn)
	{
		SelectGunBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectGun);
	}
	if (SelectRPGBtn)
	{
		SelectRPGBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectRPG);
	}
	if (SelectSwordBtn)
	{
		SelectSwordBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectSword);
	}
	if (SelectScytheBtn)
	{
		SelectScytheBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectScythe);
	}
	if (SelectDaggerBtn)
	{
		SelectDaggerBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectDagger);
	}
	if (SelectAK47Btn)
	{
		SelectAK47Btn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectAK47);
	}
	if (SelectSniperBtn)
	{
		SelectSniperBtn->OnClicked.AddDynamic(this, &UPHCCityHUD::OnSelectSniper);
	}
	if (WeaponFireButton)
	{
		WeaponFireButton->OnPressed.AddDynamic(this, &UPHCCityHUD::OnWeaponFirePressed);
		WeaponFireButton->OnReleased.AddDynamic(this, &UPHCCityHUD::OnWeaponFireReleased);
		// Scythe is default equipped — show swing button immediately
		if (WeaponFireLabel)
		{
			WeaponFireLabel->SetText(FText::FromString(TEXT("Swing")));
		}
		WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
	}
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnHostButtonClicked);
	}
	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnJoinButtonClicked);
	}
	if (JoinConnectButton)
	{
		JoinConnectButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnJoinConnectClicked);
	}
	if (JoinCancelButton)
	{
		JoinCancelButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnJoinCancelClicked);
	}

#if PLATFORM_IOS || PLATFORM_ANDROID
	// Jump/Sprint/Interact buttons removed — mobile uses touch panels.
	if (ExitVehicleButton)
	{
		ExitVehicleButton->OnClicked.AddDynamic(this, &UPHCCityHUD::OnExitVehicleClicked);
	}
	if (ADSButton)
	{
		ADSButton->OnPressed.AddDynamic(this, &UPHCCityHUD::OnADSPressed);
		ADSButton->OnReleased.AddDynamic(this, &UPHCCityHUD::OnADSReleased);
	}
#endif
}

void UPHCCityHUD::NativeDestruct()
{
	if (MenuButton)
	{
		MenuButton->OnClicked.RemoveAll(this);
	}
	if (CloseMenuButton)
	{
		CloseMenuButton->OnClicked.RemoveAll(this);
	}
	if (CarButton)
	{
		CarButton->OnClicked.RemoveAll(this);
	}
	if (SelectAventadorBtn)
	{
		SelectAventadorBtn->OnClicked.RemoveAll(this);
	}
	if (SelectBuggyBtn)
	{
		SelectBuggyBtn->OnClicked.RemoveAll(this);
	}
	if (SelectRaptorBtn)
	{
		SelectRaptorBtn->OnClicked.RemoveAll(this);
	}
	if (FireButton)
	{
		FireButton->OnPressed.RemoveAll(this);
		FireButton->OnReleased.RemoveAll(this);
	}
	if (DropBombButton)
	{
		DropBombButton->OnPressed.RemoveAll(this);
	}
	if (HostButton) { HostButton->OnClicked.RemoveAll(this); }
	if (JoinButton) { JoinButton->OnClicked.RemoveAll(this); }
	if (JoinConnectButton) { JoinConnectButton->OnClicked.RemoveAll(this); }
	if (JoinCancelButton) { JoinCancelButton->OnClicked.RemoveAll(this); }

#if PLATFORM_IOS || PLATFORM_ANDROID
	// Jump/Sprint/Interact buttons removed.
	if (ExitVehicleButton)
	{
		ExitVehicleButton->OnClicked.RemoveAll(this);
	}
#endif

	CloseMenu();

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// NativeTick -- apply mobile driving touch input
// ---------------------------------------------------------------------------

void UPHCCityHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update minimap radar with player positions
	UpdateMinimap(InDeltaTime);

	// Update speedometer + live location monitor
	if (LocationText)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APawn* P = PC->GetPawn())
			{
				const FVector Loc = P->GetActorLocation();

				// Frame-to-frame speed calculation
				if (!bSpeedInitialized)
				{
					PrevPawnLocation = Loc;
					bSpeedInitialized = true;
				}
				const float Dt = FMath::Max(InDeltaTime, 0.001f);
				const float FrameSpeed = (Loc - PrevPawnLocation).Size() / Dt;
				SmoothedSpeed = FMath::FInterpTo(SmoothedSpeed, FrameSpeed, InDeltaTime, 5.f);
				PrevPawnLocation = Loc;

				// Convert UU/s to mph (1 UU = 1 cm; 1 cm/s = 0.02237 mph)
				const float SpeedMPH = SmoothedSpeed * 0.02237f;

				// Compass direction from pawn yaw
				const float Yaw = FMath::Fmod(P->GetActorRotation().Yaw + 360.f, 360.f);
				const TCHAR* Dir;
				if (Yaw >= 337.5f || Yaw < 22.5f)        Dir = TEXT("N");
				else if (Yaw < 67.5f)                     Dir = TEXT("NE");
				else if (Yaw < 112.5f)                    Dir = TEXT("E");
				else if (Yaw < 157.5f)                    Dir = TEXT("SE");
				else if (Yaw < 202.5f)                    Dir = TEXT("S");
				else if (Yaw < 247.5f)                    Dir = TEXT("SW");
				else if (Yaw < 292.5f)                    Dir = TEXT("W");
				else                                      Dir = TEXT("NW");

				// Zone detection based on player position
				const TCHAR* Zone;
				if (Loc.Z < -500.f)
				{
					Zone = TEXT("Zone L - Underground");
				}
				else if (Loc.X >= -45000.f && Loc.X <= 75000.f && Loc.Y >= -162000.f && Loc.Y <= -106000.f)
				{
					Zone = TEXT("Zone B - Jungle");
				}
				else if (FVector2D::Distance(FVector2D(Loc.X, Loc.Y), FVector2D(22248.f, -62419.f)) < 30000.f)
				{
					Zone = TEXT("Zone C - Pyramids");
				}
				else if (FVector2D::Distance(FVector2D(Loc.X, Loc.Y), FVector2D(-248405.f, 7389.f)) < 20000.f)
				{
					Zone = TEXT("Zone I - Lake");
				}
				else if (FVector2D::Distance(FVector2D(Loc.X, Loc.Y), FVector2D(95000.f, 30000.f)) < 18000.f)
				{
					Zone = TEXT("Zone D - Sand Castle");
				}
				else if (FVector2D::Distance(FVector2D(Loc.X, Loc.Y), FVector2D(85000.f, 70000.f)) < 15000.f)
				{
					Zone = TEXT("Zone E - Mirror Castle");
				}
				else if (FVector2D::Distance(FVector2D(Loc.X, Loc.Y), FVector2D(-120000.f, 20000.f)) < 40000.f)
				{
					Zone = TEXT("Zone F - Motocross Park");
				}
				else if (Loc.Y > 70000.f)
				{
					Zone = TEXT("Zone H - Mountains");
				}
				else if (Loc.X > 30000.f && Loc.Y > 48000.f && Loc.X < 65000.f && Loc.Y < 78000.f)
				{
					Zone = TEXT("Zone K - Bay");
				}
				else if (Loc.X < -64000.f && Loc.Y > -30000.f && Loc.Y < 30000.f)
				{
					Zone = TEXT("Zone G - Highway");
				}
				else if (FMath::Abs(Loc.X) < 42000.f && Loc.Y > -36000.f && Loc.Y < 42000.f)
				{
					Zone = TEXT("Zone A - Downtown");
				}
				else if (FMath::Max(FMath::Abs(Loc.X), FMath::Abs(Loc.Y)) > 120000.f)
				{
					Zone = TEXT("Zone J - Beach / Ocean");
				}
				else if (Loc.Y < -70000.f)
				{
					Zone = TEXT("Zone M - Southern Hills");
				}
				else
				{
					Zone = TEXT("Zone N - Outskirts");
				}

				LocationText->SetText(FText::FromString(
					FString::Printf(TEXT("%s  |  %.1f mph  %s  |  X: %.0f  Y: %.0f  Z: %.0f"),
						Zone, SpeedMPH, Dir, Loc.X, Loc.Y, Loc.Z)));
			}
		}
	}

	// Update health bar
	if (HealthBarFill && HealthText)
	{
		float HP = 100.f;
		float MaxHP = 100.f;
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
			{
				HP = Char->Health;
				MaxHP = Char->MaxHealth;
			}
		}
		const float Pct = FMath::Clamp(HP / MaxHP, 0.f, 1.f);

		// Resize fill bar width (152 = full width)
		if (USizeBox* SB = Cast<USizeBox>(HealthBarFill->GetParent()))
			SB->SetWidthOverride(152.f * Pct);

		// Color: green > yellow > red
		FLinearColor BarColor;
		if (Pct > 0.5f)
			BarColor = FMath::Lerp(FLinearColor(1.f, 0.9f, 0.f), FLinearColor(0.1f, 0.75f, 0.15f), (Pct - 0.5f) * 2.f);
		else
			BarColor = FMath::Lerp(FLinearColor(0.85f, 0.1f, 0.1f), FLinearColor(1.f, 0.9f, 0.f), Pct * 2.f);
		BarColor.A = 0.9f;
		HealthBarFill->SetBrushColor(BarColor);

		HealthText->SetText(FText::FromString(
			FString::Printf(TEXT("%d"), FMath::RoundToInt(HP))));
	}

	// Update damage flash overlay
	if (DamageFlashOverlay)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
			{
				if (Char->DamageFlashTimer > 0.f)
				{
					Char->DamageFlashTimer -= InDeltaTime;
					const float Alpha = FMath::Clamp(Char->DamageFlashTimer / 0.3f, 0.f, 1.f) * 0.45f;
					DamageFlashOverlay->SetBrushColor(FLinearColor(0.85f, 0.05f, 0.05f, Alpha));
				}
				else
				{
					DamageFlashOverlay->SetBrushColor(FLinearColor(0.85f, 0.05f, 0.05f, 0.0f));
				}
			}
		}
	}

	// COMMENTED OUT — mandala HUD overlay (kept for later use)
	// // Animate trip mandala overlays — spinning layers with breathing opacity
	// if (APlayerController* TripPC = GetOwningPlayer())
	// {
	// 	if (UPHCCitySubsystem* Sub = GetWorld()->GetSubsystem<UPHCCitySubsystem>())
	// 	{
	// 		const float TripInt = Sub->TripIntensity;
	// 		if (TripInt > 0.f)
	// 		{
	// 			if (!bTripOverlaysCreated) CreateTripOverlays();
	// 			if (!bTripOverlaysVisible)
	// 			{
	// 				bTripOverlaysVisible = true;
	// 				if (TripOverlay0) TripOverlay0->SetVisibility(ESlateVisibility::HitTestInvisible);
	// 				if (TripOverlay1) TripOverlay1->SetVisibility(ESlateVisibility::HitTestInvisible);
	// 				if (TripOverlay2) TripOverlay2->SetVisibility(ESlateVisibility::HitTestInvisible);
	// 			}
	//
	// 			const float GT = GetWorld()->GetTimeSeconds();
	// 			const float Breath = FMath::Sin(GT * 0.5f) * 0.5f + 0.5f;
	// 			const float BaseAlpha = TripInt * FMath::Lerp(0.25f, 0.55f, Breath) * 0.1f;
	//
	// 			if (TripOverlay0)
	// 			{
	// 				TripOverlay0->SetRenderTransformAngle(GT * 6.f);
	// 				TripOverlay0->SetColorAndOpacity(FLinearColor(1, 1, 1, BaseAlpha * 0.85f));
	// 			}
	// 			if (TripOverlay1)
	// 			{
	// 				TripOverlay1->SetRenderTransformAngle(-GT * 4.f);
	// 				TripOverlay1->SetColorAndOpacity(FLinearColor(1, 1, 1, BaseAlpha * 0.75f));
	// 			}
	// 			if (TripOverlay2)
	// 			{
	// 				TripOverlay2->SetRenderTransformAngle(GT * 2.5f);
	// 				TripOverlay2->SetColorAndOpacity(FLinearColor(1, 1, 1, BaseAlpha * 0.65f));
	// 			}
	// 		}
	// 		else if (bTripOverlaysVisible)
	// 		{
	// 			bTripOverlaysVisible = false;
	// 			if (TripOverlay0) TripOverlay0->SetColorAndOpacity(FLinearColor(1, 1, 1, 0));
	// 			if (TripOverlay1) TripOverlay1->SetColorAndOpacity(FLinearColor(1, 1, 1, 0));
	// 			if (TripOverlay2) TripOverlay2->SetColorAndOpacity(FLinearColor(1, 1, 1, 0));
	// 		}
	// 	}
	// }

	// Update K/D display
	if (KDText)
	{
		int32 Kills = 0;
		int32 Ds = 0;
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
			{
				Kills = Char->CrabKills;
				Ds = Char->Deaths;
			}
		}
		FString PlayerName;
		if (UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this))
		{
			PlayerName = GI->LocalPlayerProfile.DisplayName;
			if (PlayerName.IsEmpty())
				PlayerName = GI->LocalPlayerProfile.Username;
		}
		if (PlayerName.IsEmpty())
			PlayerName = TEXT("Player");

		KDText->SetText(FText::FromString(
			FString::Printf(TEXT("%s  K:%d  D:%d"), *PlayerName, Kills, Ds)));
	}

	// Update player count display
	if (PlayerCountText)
	{
		if (UWorld* World = GetWorld())
		{
			if (AGameStateBase* GS = World->GetGameState())
			{
				const int32 NumPlayers = GS->PlayerArray.Num();
				PlayerCountText->SetText(FText::FromString(
					FString::Printf(TEXT("Players: %d"), NumPlayers)));
			}
		}
	}

	// =====================================================================
	//  XP Bar, Level, Kill Streak, Level-up Flash, Daily Login Popup
	// =====================================================================
	if (APlayerController* XPPC = GetOwningPlayer())
	{
		if (APHCCityCharacter* XPChar = Cast<APHCCityCharacter>(XPPC->GetPawn()))
		{
			// --- XP bar ---
			if (XPBarFill && XPText && LevelText)
			{
				const float XPPct = (XPChar->XPToNextLevel > 0)
					? FMath::Clamp((float)XPChar->PlayerXP / (float)XPChar->XPToNextLevel, 0.f, 1.f)
					: 0.f;

				if (USizeBox* SB = Cast<USizeBox>(XPBarFill->GetParent()))
					SB->SetWidthOverride(390.f * XPPct);

				// Color: blue shifting to brighter as it fills
				const FLinearColor XPColor = FMath::Lerp(
					FLinearColor(0.15f, 0.4f, 0.8f, 0.9f),
					FLinearColor(0.3f, 0.8f, 1.f, 0.95f), XPPct);
				XPBarFill->SetBrushColor(XPColor);

				XPText->SetText(FText::FromString(
					FString::Printf(TEXT("%d / %d XP"), XPChar->PlayerXP, XPChar->XPToNextLevel)));

				LevelText->SetText(FText::FromString(
					FString::Printf(TEXT("LV %d"), XPChar->PlayerLevel)));
			}

			// --- Kill streak display ---
			if (KillStreakText)
			{
				if (XPChar->KillStreakCount >= 2 && XPChar->KillStreakTimer > 0.f)
				{
					FString StreakStr;
					if (XPChar->KillStreakCount >= 10)
						StreakStr = FString::Printf(TEXT("UNSTOPPABLE x%d  (5x XP)"), XPChar->KillStreakCount);
					else if (XPChar->KillStreakCount >= 7)
						StreakStr = FString::Printf(TEXT("RAMPAGE x%d  (3x XP)"), XPChar->KillStreakCount);
					else if (XPChar->KillStreakCount >= 4)
						StreakStr = FString::Printf(TEXT("KILLING SPREE x%d  (2x XP)"), XPChar->KillStreakCount);
					else
						StreakStr = FString::Printf(TEXT("DOUBLE KILL x%d  (1.5x XP)"), XPChar->KillStreakCount);

					KillStreakText->SetText(FText::FromString(StreakStr));

					// Pulse opacity based on timer
					const float Pulse = 0.7f + 0.3f * FMath::Sin(GetWorld()->GetTimeSeconds() * 6.f);
					KillStreakText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.4f, 0.1f, Pulse)));
				}
				else
				{
					KillStreakText->SetText(FText::FromString(TEXT("")));
				}
			}

			// --- Level-up flash ---
			if (LevelUpOverlay && LevelUpText)
			{
				if (XPChar->LevelUpFlashTimer > 0.f)
				{
					const float Alpha = FMath::Clamp(XPChar->LevelUpFlashTimer / 1.5f, 0.f, 1.f) * 0.3f;
					LevelUpOverlay->SetBrushColor(FLinearColor(1.f, 0.85f, 0.f, Alpha));
					LevelUpText->SetText(FText::FromString(
						FString::Printf(TEXT("LEVEL %d!"), XPChar->PlayerLevel)));
				}
				else
				{
					LevelUpOverlay->SetBrushColor(FLinearColor(1.f, 0.85f, 0.f, 0.f));
					LevelUpText->SetText(FText::FromString(TEXT("")));
				}
			}

			// --- Daily login popup (show on first frame if pending) ---
			if (DailyLoginOverlay && !bDailyLoginShown && XPChar->bDailyRewardPending)
			{
				bDailyLoginShown = true;
				DailyLoginOverlay->SetVisibility(ESlateVisibility::Visible);

				const int32 Streak = XPChar->LoginStreak;
				static const int32 DayRewards[] = { 5, 10, 20, 40, 80, 150, 300 };
				const int32 TodayReward = DayRewards[FMath::Clamp(Streak, 0, 6)];

				if (DailyLoginStreakText)
					DailyLoginStreakText->SetText(FText::FromString(
						FString::Printf(TEXT("Day %d of 7"), Streak + 1)));
				if (DailyLoginRewardText)
					DailyLoginRewardText->SetText(FText::FromString(
						FString::Printf(TEXT("Claim %d Coins + 25 XP!"), TodayReward)));

				// Highlight completed days
				UBorder* DayBorders[] = { DailyLoginDay1, DailyLoginDay2, DailyLoginDay3,
					DailyLoginDay4, DailyLoginDay5, DailyLoginDay6, DailyLoginDay7 };
				for (int32 D = 0; D < 7; ++D)
				{
					if (DayBorders[D])
					{
						if (D < Streak)
							DayBorders[D]->SetBrushColor(FLinearColor(0.1f, 0.5f, 0.15f, 0.9f)); // completed
						else if (D == Streak)
							DayBorders[D]->SetBrushColor(FLinearColor(1.f, 0.75f, 0.f, 0.9f));   // today
						else
							DayBorders[D]->SetBrushColor(FLinearColor(0.15f, 0.15f, 0.15f, 0.9f)); // future
					}
				}
			}
		}
	}

	// =====================================================================
	//  Loot Crate Spinner + Achievement Popup + Event Banner + Currency
	// =====================================================================
	if (APlayerController* EngPC = GetOwningPlayer())
	{
		if (APHCCityCharacter* EngChar = Cast<APHCCityCharacter>(EngPC->GetPawn()))
		{
			// --- Loot crate spinner ---
			if (LootCrateOverlay && LootCrateSpinText && LootCrateRewardText)
			{
				if (EngChar->bLootCrateActive)
				{
					LootCrateOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
					// Spinning coin symbols
					static const TCHAR* SpinFrames[] = {
						TEXT("[ $ ]"), TEXT("[ $$ ]"), TEXT("[ $$$ ]"), TEXT("[ $$$$ ]"),
						TEXT("[ $$$$$ ]"), TEXT("[ $$$$ ]"), TEXT("[ $$$ ]"), TEXT("[ $$ ]") };
					const int32 Frame = (int32)(EngChar->LootCrateSpinAngle / 45.f) % 8;
					LootCrateSpinText->SetText(FText::FromString(
						FString::Printf(TEXT("LOOT CRATE!  %s"), SpinFrames[Frame])));
					LootCrateRewardText->SetText(FText::FromString(TEXT("...")));

					// Pulse the overlay border color
					const float P = 0.7f + 0.3f * FMath::Sin(GetWorld()->GetTimeSeconds() * 10.f);
					LootCrateOverlay->SetBrushColor(FLinearColor(0.4f * P, 0.15f * P, 0.0f, 0.88f));
				}
				else if (EngChar->LootCrateReward > 0 && EngChar->LootCrateTimer < 0.01f)
				{
					// Brief show of reward (show for a moment after spin completes)
					// Use a local approach: show overlay for one more frame, then hide
					if (LootCrateOverlay->GetVisibility() != ESlateVisibility::Collapsed)
					{
						LootCrateRewardText->SetText(FText::FromString(
							FString::Printf(TEXT("+%d Coins  +%d XP"),
								EngChar->LootCrateReward, EngChar->LootCrateXPReward)));
						LootCrateSpinText->SetText(FText::FromString(TEXT("LOOT CRATE!")));
						LootCrateOverlay->SetBrushColor(FLinearColor(0.05f, 0.3f, 0.05f, 0.88f));
						// Keep showing for a moment — hide on next tick when reward resets
					}
				}
				else
				{
					LootCrateOverlay->SetVisibility(ESlateVisibility::Collapsed);
				}
			}

			// --- Achievement popup ---
			if (AchievementPopupBG && AchievementPopupText)
			{
				if (EngChar->AchievementPopupTimer > 0.f && EngChar->LastUnlockedAchievement >= 0)
				{
					AchievementPopupBG->SetVisibility(ESlateVisibility::HitTestInvisible);
					const int32 Idx = EngChar->LastUnlockedAchievement;
					AchievementPopupText->SetText(FText::FromString(
						FString::Printf(TEXT("ACHIEVEMENT: %s"),
							APHCCityCharacter::AchievementDefs[Idx].Name)));

					// Fade out near end
					const float Alpha = FMath::Clamp(EngChar->AchievementPopupTimer / 0.5f, 0.f, 1.f);
					AchievementPopupBG->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.15f, 0.9f * Alpha));
					AchievementPopupText->SetColorAndOpacity(FSlateColor(
						FLinearColor(1.f, 0.85f, 0.f, Alpha)));
				}
				else
				{
					AchievementPopupBG->SetVisibility(ESlateVisibility::Collapsed);
				}
			}

			// --- Event banner ---
			if (EventBannerBG && EventBannerText && EventTimerText)
			{
				if (EngChar->CurrentEvent != APHCCityCharacter::EActiveEvent::None)
				{
					EventBannerBG->SetVisibility(ESlateVisibility::HitTestInvisible);

					FString EventName;
					FLinearColor BannerColor;
					switch (EngChar->CurrentEvent)
					{
					case APHCCityCharacter::EActiveEvent::GoldenHour:
						EventName = TEXT("GOLDEN HOUR - 3x Coins!");
						BannerColor = FLinearColor(0.7f, 0.5f, 0.0f, 0.85f);
						break;
					case APHCCityCharacter::EActiveEvent::CrabSwarm:
						EventName = TEXT("CRAB SWARM - Double Spawns!");
						BannerColor = FLinearColor(0.6f, 0.1f, 0.0f, 0.85f);
						break;
					case APHCCityCharacter::EActiveEvent::NightHunt:
						EventName = TEXT("NIGHT HUNT - 2x Kill XP!");
						BannerColor = FLinearColor(0.1f, 0.1f, 0.4f, 0.85f);
						break;
					default: break;
					}

					EventBannerBG->SetBrushColor(BannerColor);
					EventBannerText->SetText(FText::FromString(EventName));

					const int32 Mins = (int32)(EngChar->EventTimer / 60.f);
					const int32 Secs = (int32)FMath::Fmod(EngChar->EventTimer, 60.f);
					EventTimerText->SetText(FText::FromString(
						FString::Printf(TEXT("%d:%02d remaining"), Mins, Secs)));
				}
				else
				{
					EventBannerBG->SetVisibility(ESlateVisibility::Collapsed);
				}
			}

			// --- Auto-refresh currency display ---
			if (UPHCGameInstance* AutoGI = UPHCGameInstance::GetPHCGameInstance(this))
			{
				UpdateCurrencyDisplay(AutoGI->LocalPlayerProfile.Coins, AutoGI->LocalPlayerProfile.Gems);
			}
		}
	}

	// -- Proximity-based "ENTER VEHICLE" button visibility --
	if (EnterVehicleButton && !bDrivingMode)
	{
		bool bNearVehicle = false;
		if (APlayerController* PC = GetOwningPlayer())
		{
			if (APawn* P = PC->GetPawn())
			{
				constexpr float ShowRange = 2000.f;
				const FVector Loc = P->GetActorLocation();
				UWorld* W = GetWorld();

				// Scan all vehicle types in the world
				auto CheckRange = [&](AActor* A) -> bool
				{
					return A && FVector::Dist(Loc, A->GetActorLocation()) < ShowRange;
				};

				for (TActorIterator<APHCVehiclePawn> It(W); !bNearVehicle && It; ++It)
				{
					if (!It->bIsOccupied && !It->bIsDestroyed && CheckRange(*It)) bNearVehicle = true;
				}
				for (TActorIterator<APHCDuneBuggyPawn> It(W); !bNearVehicle && It; ++It)
				{
					if (!It->bIsOccupied && !It->bIsDestroyed && CheckRange(*It)) bNearVehicle = true;
				}
				for (TActorIterator<APHCRaptorPawn> It(W); !bNearVehicle && It; ++It)
				{
					if (!It->bIsOccupied && !It->bIsCrashing && CheckRange(*It)) bNearVehicle = true;
				}
				for (TActorIterator<APHCTankPawn> It(W); !bNearVehicle && It; ++It)
				{
					if (!It->bIsOccupied && !It->bIsDestroyed && CheckRange(*It)) bNearVehicle = true;
				}
				for (TActorIterator<APHCDirtBikePawn> It(W); !bNearVehicle && It; ++It)
				{
					if (!It->bIsOccupied && !It->bIsDestroyed && CheckRange(*It)) bNearVehicle = true;
				}
				for (TActorIterator<APHCBlackbirdPawn> It(W); !bNearVehicle && It; ++It)
				{
					if (!It->bIsOccupied && !It->bIsCrashing && CheckRange(*It)) bNearVehicle = true;
				}
				for (TActorIterator<APHCBlackHawkPawn> It(W); !bNearVehicle && It; ++It)
				{
					if (!It->bIsOccupied && !It->bIsCrashing && CheckRange(*It)) bNearVehicle = true;
				}
			}
		}
		EnterVehicleButton->SetVisibility(bNearVehicle ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

#if PLATFORM_IOS || PLATFORM_ANDROID
	do {
		APlayerController* PC = GetOwningPlayer();
		if (!PC) break;

		int32 VPWidth = 0;
		int32 VPHeight = 0;
		PC->GetViewportSize(VPWidth, VPHeight);
		if (VPWidth == 0 || VPHeight == 0) break;

		const float ScreenHalfX = VPWidth * 0.5f;
		const float ScreenHalfY = VPHeight * 0.5f;

		// Auto-detect vehicle possession (fallback if Client RPC timing is off).
		APawn* CurrentPawn = PC->GetPawn();
		const bool bPawnIsVehicle = CurrentPawn && !Cast<APHCCityCharacter>(CurrentPawn);

		if (bPawnIsVehicle)
		{
			if (!bDrivingMode)
			{
				SetDrivingMode(true, CurrentPawn);
			}
			else if (DrivenVehicle != CurrentPawn)
			{
				// Vehicle pawn arrived after initial SetDrivingMode(true, nullptr) —
				// update pointer and fire/drop visibility without re-running full mode switch.
				DrivenVehicle = CurrentPawn;
				if (FireButton)
				{
					const bool bShowFire = (Cast<APHCDuneBuggyPawn>(CurrentPawn) || Cast<APHCRaptorPawn>(CurrentPawn) || Cast<APHCTankPawn>(CurrentPawn) || Cast<APHCBlackbirdPawn>(CurrentPawn) || Cast<APHCBlackHawkPawn>(CurrentPawn));
					FireButton->SetVisibility(bShowFire ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
				}
				if (DropBombButton)
				{
					const bool bShowDrop = (Cast<APHCRaptorPawn>(CurrentPawn) || Cast<APHCBlackbirdPawn>(CurrentPawn) || Cast<APHCBlackHawkPawn>(CurrentPawn));
					DropBombButton->SetVisibility(bShowDrop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
				}
				// Vehicle crosshair for flight vehicles
				{
					const bool bFlight = (Cast<APHCRaptorPawn>(CurrentPawn) || Cast<APHCBlackbirdPawn>(CurrentPawn) || Cast<APHCBlackHawkPawn>(CurrentPawn) || Cast<APHCDuneBuggyPawn>(CurrentPawn));
					const ESlateVisibility VehCrossVis = bFlight ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
					if (VehicleCrosshairDot) VehicleCrosshairDot->SetVisibility(VehCrossVis);
					if (VehicleCrosshairTop) VehicleCrosshairTop->SetVisibility(VehCrossVis);
					if (VehicleCrosshairBottom) VehicleCrosshairBottom->SetVisibility(VehCrossVis);
					if (VehicleCrosshairLeft) VehicleCrosshairLeft->SetVisibility(VehCrossVis);
					if (VehicleCrosshairRight) VehicleCrosshairRight->SetVisibility(VehCrossVis);
				}
			}
			// Vehicle found — clear pending flag.
			bPendingVehicleEntry = false;
		}
		else if (bDrivingMode && !bPendingVehicleEntry && CurrentPawn && Cast<APHCCityCharacter>(CurrentPawn))
		{
			// Pawn is back to character — auto-exit driving mode.
			SetDrivingMode(false, nullptr);
		}

		float DebugThrottle = 0.f;
		float DebugSteering = 0.f;
		int32 DebugTouchCount = 0;

		if (bDrivingMode && DrivenVehicle)
		{
			// ---- DRIVING TOUCH INPUT (unchanged) ----
			float NewSteering = 0.f;
			float NewThrottle = 0.f;
			float NewPitch = 0.f;
			bool bFireTouched = false;
			bool bDropTouched = false;

			const float FireCenterX = VPWidth * 0.15f;
			const float FireBtnRadius = 84.f;
			const float FireCenterY = VPHeight * 0.48f - FireBtnRadius;
			const float FireTopY = FireCenterY - FireBtnRadius;
			const float FireBottomY = FireCenterY + FireBtnRadius;

			const float DropCenterX = VPWidth * 0.85f;
			const float DropBtnRadius = 70.f;
			const float DropCenterY = VPHeight * 0.30f;

			for (int32 i = 0; i < 5; ++i)
			{
				float TouchX = 0.f;
				float TouchY = 0.f;
				bool bPressed = false;
				PC->GetInputTouchState(static_cast<ETouchIndex::Type>(i), TouchX, TouchY, bPressed);
				if (!bPressed) { continue; }

				if (TouchX >= FireCenterX - FireBtnRadius && TouchX <= FireCenterX + FireBtnRadius &&
					TouchY >= FireTopY && TouchY <= FireBottomY)
				{
					bFireTouched = true;
					continue;
				}

				if (TouchX >= DropCenterX - DropBtnRadius && TouchX <= DropCenterX + DropBtnRadius &&
					TouchY >= DropCenterY - DropBtnRadius && TouchY <= DropCenterY + DropBtnRadius)
				{
					bDropTouched = true;
					continue;
				}

				// Skip touches on the Exit button (top-left area)
				if (TouchX >= 50.f && TouchX <= 180.f &&
					TouchY >= 64.f && TouchY <= 114.f)
				{
					continue;
				}

				if (TouchX < ScreenHalfX && TouchY > ScreenHalfY)
				{
					NewSteering = (TouchX / ScreenHalfX) * 2.f - 1.f;
					NewSteering = FMath::Clamp(NewSteering, -1.f, 1.f);

					const float NormY = (TouchY - ScreenHalfY) / ScreenHalfY;
					NewPitch = 1.f - NormY * 2.f;
					NewPitch = FMath::Clamp(NewPitch, -1.f, 1.f);
				}
				else if (TouchX >= ScreenHalfX && TouchY > ScreenHalfY)
				{
					const float ThresholdY = VPHeight * 0.75f;
					NewThrottle = (TouchY < ThresholdY) ? 1.f : -1.f;
				}
			}

			// Set input locally on the vehicle pawn for client-side prediction,
			// and relay to server via PlayerController RPC.
			if (APHCVehiclePawn* V = Cast<APHCVehiclePawn>(DrivenVehicle))
			{
				V->SteeringInput = NewSteering;
				V->ThrottleInput = NewThrottle;
			}
			else if (APHCDuneBuggyPawn* B = Cast<APHCDuneBuggyPawn>(DrivenVehicle))
			{
				B->SteeringInput = NewSteering;
				B->ThrottleInput = NewThrottle;
			}
			else if (APHCRaptorPawn* R = Cast<APHCRaptorPawn>(DrivenVehicle))
			{
				R->SteeringInput = NewSteering;
				R->ThrottleInput = NewThrottle;
				R->PitchInput = NewPitch;
			}
			else if (APHCTankPawn* T = Cast<APHCTankPawn>(DrivenVehicle))
			{
				T->SteeringInput = NewSteering;
				T->ThrottleInput = NewThrottle;
				T->TurretYawInput = NewPitch;
			}
			else if (APHCDirtBikePawn* DB = Cast<APHCDirtBikePawn>(DrivenVehicle))
			{
				DB->SteeringInput = NewSteering;
				DB->ThrottleInput = NewThrottle;
			}
			else if (APHCBlackbirdPawn* BB = Cast<APHCBlackbirdPawn>(DrivenVehicle))
			{
				BB->SteeringInput = NewSteering;
				BB->ThrottleInput = NewThrottle;
				BB->PitchInput = NewPitch;
			}
			else if (APHCBlackHawkPawn* BH = Cast<APHCBlackHawkPawn>(DrivenVehicle))
			{
				BH->SteeringInput = NewSteering;
				BH->ThrottleInput = NewThrottle;
				BH->PitchInput = NewPitch;
			}

			// Also relay to server for authoritative processing.
			if (APHCPlayerController* PHC_PC = Cast<APHCPlayerController>(PC))
			{
				if (!PHC_PC->HasAuthority())
				{
					PHC_PC->ServerSetVehicleInput(NewThrottle, NewSteering, NewPitch);
				}
			}

			if (bFireTouched)
			{
				if (APHCDuneBuggyPawn* Buggy = Cast<APHCDuneBuggyPawn>(DrivenVehicle)) { Buggy->FireMissile(); }
				else if (APHCRaptorPawn* Raptor = Cast<APHCRaptorPawn>(DrivenVehicle)) { Raptor->FireRocket(); }
				else if (APHCTankPawn* Tank = Cast<APHCTankPawn>(DrivenVehicle)) { Tank->FireCannon(); }
				else if (APHCBlackbirdPawn* BB = Cast<APHCBlackbirdPawn>(DrivenVehicle)) { BB->FireRocket(); }
				else if (APHCBlackHawkPawn* BH = Cast<APHCBlackHawkPawn>(DrivenVehicle)) { BH->FireRocket(); }
			}

			if (bDropTouched)
			{
				if (APHCRaptorPawn* Raptor = Cast<APHCRaptorPawn>(DrivenVehicle)) { Raptor->DropBomb(); }
				else if (APHCBlackbirdPawn* BB = Cast<APHCBlackbirdPawn>(DrivenVehicle)) { BB->DropBomb(); }
				else if (APHCBlackHawkPawn* BH = Cast<APHCBlackHawkPawn>(DrivenVehicle)) { BH->DropBomb(); }
			}

			DebugThrottle = NewThrottle;
			DebugSteering = NewSteering;

			// --- Update vehicle crosshair to follow aircraft nose on screen ---
			if (VehicleCrosshairDot && DrivenVehicle && PC)
			{
				const FVector NoseTarget = DrivenVehicle->GetActorLocation()
					+ DrivenVehicle->GetActorForwardVector() * 50000.f;
				FVector2D ScreenPos;
				bool bOnScreen = PC->ProjectWorldLocationToScreen(NoseTarget, ScreenPos, true);

				if (bOnScreen)
				{
					// Convert screen pos to canvas-relative offset from center
					const float CX = ScreenPos.X - VPWidth * 0.5f;
					const float CY = ScreenPos.Y - VPHeight * 0.5f;

					const float DotSz = 4.f, Gap = 10.f, ArmW = 2.f, ArmL = 18.f;
					if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(VehicleCrosshairDot->Slot))
						S->SetOffsets(FMargin(CX - DotSz * 0.5f, CY - DotSz * 0.5f, DotSz, DotSz));
					if (VehicleCrosshairTop)
						if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(VehicleCrosshairTop->Slot))
							S->SetOffsets(FMargin(CX - ArmW * 0.5f, CY - (Gap + ArmL), ArmW, ArmL));
					if (VehicleCrosshairBottom)
						if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(VehicleCrosshairBottom->Slot))
							S->SetOffsets(FMargin(CX - ArmW * 0.5f, CY + Gap, ArmW, ArmL));
					if (VehicleCrosshairLeft)
						if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(VehicleCrosshairLeft->Slot))
							S->SetOffsets(FMargin(CX - (Gap + ArmL), CY - ArmW * 0.5f, ArmL, ArmW));
					if (VehicleCrosshairRight)
						if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(VehicleCrosshairRight->Slot))
							S->SetOffsets(FMargin(CX + Gap, CY - ArmW * 0.5f, ArmL, ArmW));
				}
			}
		}
		else
		{
			// ---- WALKING TOUCH INPUT ----
			// Same pattern as driving: GetInputTouchState each frame.
			// Left side = virtual joystick (drag from initial touch).
			// Right side = camera look (drag delta).

			APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn());
			if (!Char) break;

			const float ScreenMidX = VPWidth * 0.5f;  // 50/50 split
			const float JoystickMaxRadius = VPWidth * 0.10f;
			const float DeadZone = VPWidth * 0.015f;
			float LookSensitivity = 0.15f;
			// Scale look sensitivity down when ADS is active
			if (Char->bFirstPersonMode)
			{
				LookSensitivity *= FMath::Lerp(1.f, Char->ADSSensitivityMult, Char->ADSAlpha);
			}
			const float ButtonZoneY = VPHeight * 0.25f; // top 25% reserved for buttons

			// Enter Vehicle button area (bottom-center, 200x55 px, 120px above bottom)
			const float EnterBtnLeft  = ScreenMidX - 100.f;
			const float EnterBtnRight = ScreenMidX + 100.f;
			const float EnterBtnTop   = VPHeight - 175.f;
			const float EnterBtnBot   = VPHeight - 120.f;

			float MoveX = 0.f;
			float MoveY = 0.f;
			float LookDeltaX = 0.f;
			float LookDeltaY = 0.f;
			bool bMoveFingStillDown = false;
			bool bLookFingStillDown = false;
			bool bEnterTouched = false;

			for (int32 i = 0; i < 5; ++i)
			{
				float TouchX = 0.f;
				float TouchY = 0.f;
				bool bPressed = false;
				PC->GetInputTouchState(static_cast<ETouchIndex::Type>(i), TouchX, TouchY, bPressed);

				if (!bPressed)
				{
					// Tap-to-jump: detect quick taps on release
					if (WalkFingerWasDown[i])
					{
						const double Duration = FPlatformTime::Seconds() - WalkTapStartTime[i];
						const float TapDist = FVector2D::Distance(FVector2D(TouchX, TouchY), WalkTapStartPos[i]);
						if (Duration < 0.25 && TapDist < VPWidth * 0.03f)
						{
							// Quick tap detected — trigger jump
							if (Char->bClimbingGearEquipped)
							{
								Char->ClimbingVerticalInput = 1.f;
							}
							else if (Char->bJetpackEquipped)
							{
								Char->JetpackVerticalInput = 1.f;
							}
							else if (Char->bHasBeenGrounded)
							{
								Char->ManualVelocityZ = 420.f;
								Char->bWasGrounded = false;
							}
						}
					}
					WalkFingerWasDown[i] = false;
					if (i == WalkMoveFingerIdx)  { WalkMoveFingerIdx = -1; WalkMoveOrigin = FVector2D::ZeroVector; }
					if (i == WalkLookFingerIdx)  { WalkLookFingerIdx = -1; WalkLookPrev = FVector2D::ZeroVector; }
					continue;
				}

				// Tap-to-jump: record start position on new press
				if (!WalkFingerWasDown[i])
				{
					WalkTapStartPos[i] = FVector2D(TouchX, TouchY);
					WalkTapStartTime[i] = FPlatformTime::Seconds();
					WalkFingerWasDown[i] = true;
				}

				// Skip top 25% of screen where HUD buttons live
				// But keep tracking the finger so it isn't lost
				if (TouchY < ButtonZoneY)
				{
					if (i == WalkMoveFingerIdx) bMoveFingStillDown = true;
					if (i == WalkLookFingerIdx) bLookFingStillDown = true;
					continue;
				}

				// Skip touches on the Enter Vehicle button area
				if (EnterVehicleButton &&
					EnterVehicleButton->GetVisibility() == ESlateVisibility::Visible &&
					TouchX >= EnterBtnLeft && TouchX <= EnterBtnRight &&
					TouchY >= EnterBtnTop && TouchY <= EnterBtnBot)
				{
					bEnterTouched = true;
					if (i == WalkMoveFingerIdx) bMoveFingStillDown = true;
					if (i == WalkLookFingerIdx) bLookFingStillDown = true;
					continue;
				}

				const FVector2D TouchPos(TouchX, TouchY);

				// If this finger is already tracked, keep tracking it
				// regardless of which side of the screen it drifted to
				if (i == WalkMoveFingerIdx)
				{
					bMoveFingStillDown = true;
					const FVector2D Offset = TouchPos - WalkMoveOrigin;
					const float Dist = Offset.Size();
					if (Dist > DeadZone)
					{
						FVector2D Normalized = Offset / JoystickMaxRadius;
						if (Normalized.Size() > 1.f) { Normalized = Normalized.GetSafeNormal(); }
						MoveX = Normalized.X;
						MoveY = -Normalized.Y; // screen Y is inverted
					}
				}
				else if (i == WalkLookFingerIdx)
				{
					bLookFingStillDown = true;
					const FVector2D Delta = (TouchPos - WalkLookPrev) * LookSensitivity;
					LookDeltaX = Delta.X;
					LookDeltaY = Delta.Y;
					WalkLookPrev = TouchPos;
				}
				else if (TouchX < ScreenMidX)
				{
					// --- Left half: new movement joystick finger ---
					if (WalkMoveFingerIdx < 0)
					{
						WalkMoveFingerIdx = i;
						WalkMoveOrigin = TouchPos;
						bMoveFingStillDown = true;
					}
				}
				else
				{
					// --- Right half: new camera look finger ---
					if (WalkLookFingerIdx < 0)
					{
						WalkLookFingerIdx = i;
						WalkLookPrev = TouchPos;
						bLookFingStillDown = true;
					}
				}
			}

			if (!bMoveFingStillDown && WalkMoveFingerIdx >= 0)
			{
				WalkMoveFingerIdx = -1;
				WalkMoveOrigin = FVector2D::ZeroVector;
			}
			if (!bLookFingStillDown && WalkLookFingerIdx >= 0)
			{
				WalkLookFingerIdx = -1;
				WalkLookPrev = FVector2D::ZeroVector;
			}

			Char->TouchMoveX = MoveX;
			Char->TouchMoveY = MoveY;
			Char->TouchLookX = LookDeltaX;
			Char->TouchLookY = LookDeltaY;
			Char->bHUDTouchActive = (WalkMoveFingerIdx >= 0 || WalkLookFingerIdx >= 0);

			// Handle Enter Vehicle tap (fire on release to avoid repeated triggers)
			if (bEnterTouched && !bEnterTouchActive)
			{
				if (APHCPlayerController* PCC = Cast<APHCPlayerController>(PC))
				{
					PCC->EnterNearestVehicle();
				}
			}
			bEnterTouchActive = bEnterTouched;
		}
		// DEBUG: show driving input values + vehicle position (temporary).
		if (LocationText)
		{
			FString LocStr;
			if (bDrivingMode && CurrentPawn)
			{
				FVector Pos = CurrentPawn->GetActorLocation();
				LocStr = FString::Printf(TEXT("T:%.1f S:%.1f Pos:%.0f,%.0f,%.0f"),
					DebugThrottle, DebugSteering, Pos.X, Pos.Y, Pos.Z);
			}
			else if (CurrentPawn)
			{
				FVector Pos = CurrentPawn->GetActorLocation();
				LocStr = FString::Printf(TEXT("Char:%.0f,%.0f,%.0f"),
					Pos.X, Pos.Y, Pos.Z);
			}
			LocationText->SetText(FText::FromString(LocStr));
		}
	} while (false);
#endif

	// -- COD crosshair / hit marker / ADS button dynamics -----------------------
	{
		APHCCityCharacter* Char = nullptr;
		if (APlayerController* PC = GetOwningPlayer())
		{
			Char = Cast<APHCCityCharacter>(PC->GetPawn());
		}

		const bool bRangedWeapon = Char &&
			(Char->CurrentWeaponType == 0 || Char->CurrentWeaponType == 1 || Char->CurrentWeaponType == 5 || Char->CurrentWeaponType == 6);
		const bool bFPRangedWeapon = bRangedWeapon && Char->bFirstPersonMode;

		// COD-style sniper scope: full black overlay with viewport opening
		const bool bSniperScoped = Char && Char->CurrentWeaponType == 6 && Char->ADSAlpha > 0.01f;
		const ESlateVisibility ScopeVis = bSniperScoped ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (ScopeOverlay) ScopeOverlay->SetVisibility(ScopeVis);
		if (ScopeBlackTop) ScopeBlackTop->SetVisibility(ScopeVis);
		if (ScopeBlackBottom) ScopeBlackBottom->SetVisibility(ScopeVis);
		if (ScopeBlackLeft) ScopeBlackLeft->SetVisibility(ScopeVis);
		if (ScopeBlackRight) ScopeBlackRight->SetVisibility(ScopeVis);
		if (ScopeRingBorder) ScopeRingBorder->SetVisibility(ScopeVis);
		if (ScopeCrosshairH) ScopeCrosshairH->SetVisibility(ScopeVis);
		if (ScopeCrosshairV) ScopeCrosshairV->SetVisibility(ScopeVis);
		if (ScopeCrosshairDot) ScopeCrosshairDot->SetVisibility(ScopeVis);

		// Scope zoom buttons — visible only when scoped
		const ESlateVisibility ZoomBtnVis = bSniperScoped ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
		if (ScopeZoomInBtn) ScopeZoomInBtn->SetVisibility(ZoomBtnVis);
		if (ScopeZoomOutBtn) ScopeZoomOutBtn->SetVisibility(ZoomBtnVis);
		if (ScopeZoomLabel)
		{
			ScopeZoomLabel->SetVisibility(ZoomBtnVis);
			if (bSniperScoped)
			{
				const int32 ZoomX = FMath::RoundToInt(90.f / Char->SniperZoomFOV);
				ScopeZoomLabel->SetText(FText::FromString(FString::Printf(TEXT("%dx"), ZoomX)));
			}
		}

		// Thermal toggle button
		if (ThermalToggleBtn) ThermalToggleBtn->SetVisibility(ZoomBtnVis);
		if (ThermalToggleLabel && bSniperScoped)
		{
			ThermalToggleLabel->SetText(FText::FromString(
				Char->bThermalScope ? TEXT("Thermal ON") : TEXT("Thermal")));
		}

		// Hide normal crosshair when scoped (sniper scope has its own crosshair)
		// Show normal crosshair for other ranged weapons
		const bool bShowNormalCrosshair = bRangedWeapon && !bSniperScoped;
		const ESlateVisibility CrossVis = bShowNormalCrosshair ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (CrosshairDot) CrosshairDot->SetVisibility(CrossVis);
		if (CrosshairTop) CrosshairTop->SetVisibility(CrossVis);
		if (CrosshairBottom) CrosshairBottom->SetVisibility(CrossVis);
		if (CrosshairLeft) CrosshairLeft->SetVisibility(CrossVis);
		if (CrosshairRight) CrosshairRight->SetVisibility(CrossVis);

		// Dynamic spread: reposition crosshair lines based on ADSAlpha
		if (bRangedWeapon && Char)
		{
			// Total spread = base + bloom + movement penalty (matches actual bullet spread)
			const float BaseSpread = FMath::Lerp(Char->SpreadHipfire, Char->SpreadADS, Char->ADSAlpha);
			const float SpreadDeg = FMath::Min(BaseSpread + Char->BloomAccum + Char->BloomMovePenalty, 8.f);
			// Convert degrees to screen-space pixels (~6 px per degree at default FOV)
			const float GapPx = 6.f + SpreadDeg * 6.f;

			const float AW = 2.f, AL = 18.f;
			if (CrosshairTop)
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(CrosshairTop->Slot))
					S->SetOffsets(FMargin(-AW * 0.5f, -(GapPx + AL), AW, AL));
			if (CrosshairBottom)
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(CrosshairBottom->Slot))
					S->SetOffsets(FMargin(-AW * 0.5f, GapPx, AW, AL));
			if (CrosshairLeft)
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(CrosshairLeft->Slot))
					S->SetOffsets(FMargin(-(GapPx + AL), -AW * 0.5f, AL, AW));
			if (CrosshairRight)
				if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(CrosshairRight->Slot))
					S->SetOffsets(FMargin(GapPx, -AW * 0.5f, AL, AW));
		}

		// Hit marker fade
		if (Char && Char->HitMarkerTimer > 0.f)
		{
			const float Alpha = FMath::Clamp(Char->HitMarkerTimer / 0.15f, 0.f, 1.f);
			const FLinearColor HMColor(1.f, 1.f, 1.f, Alpha);
			if (HitMarkerTL) { HitMarkerTL->SetBrushColor(HMColor); HitMarkerTL->SetVisibility(ESlateVisibility::HitTestInvisible); }
			if (HitMarkerTR) { HitMarkerTR->SetBrushColor(HMColor); HitMarkerTR->SetVisibility(ESlateVisibility::HitTestInvisible); }
			if (HitMarkerBL) { HitMarkerBL->SetBrushColor(HMColor); HitMarkerBL->SetVisibility(ESlateVisibility::HitTestInvisible); }
			if (HitMarkerBR) { HitMarkerBR->SetBrushColor(HMColor); HitMarkerBR->SetVisibility(ESlateVisibility::HitTestInvisible); }
		}
		else
		{
			if (HitMarkerTL) HitMarkerTL->SetVisibility(ESlateVisibility::Collapsed);
			if (HitMarkerTR) HitMarkerTR->SetVisibility(ESlateVisibility::Collapsed);
			if (HitMarkerBL) HitMarkerBL->SetVisibility(ESlateVisibility::Collapsed);
			if (HitMarkerBR) HitMarkerBR->SetVisibility(ESlateVisibility::Collapsed);
		}

		// ADS button visibility: show on mobile in FP mode with gun-type weapon
#if PLATFORM_IOS || PLATFORM_ANDROID
		if (ADSButton)
		{
			ADSButton->SetVisibility(bFPRangedWeapon ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
#endif
	}

	// Continuous fire while Fire UButton is held (desktop/fallback).
	if (bFireHeld && bDrivingMode)
	{
		if (APHCDuneBuggyPawn* Buggy = Cast<APHCDuneBuggyPawn>(DrivenVehicle))
		{
			Buggy->FireMissile();
		}
		else if (APHCRaptorPawn* Raptor = Cast<APHCRaptorPawn>(DrivenVehicle))
		{
			Raptor->FireRocket();
		}
		else if (APHCTankPawn* Tank = Cast<APHCTankPawn>(DrivenVehicle))
		{
			Tank->FireCannon();
		}
		else if (APHCBlackbirdPawn* BB = Cast<APHCBlackbirdPawn>(DrivenVehicle))
		{
			BB->FireRocket();
		}
		else if (APHCBlackHawkPawn* BH = Cast<APHCBlackHawkPawn>(DrivenVehicle))
		{
			BH->FireRocket();
		}
	}
}

// ---------------------------------------------------------------------------
// Driving mode
// ---------------------------------------------------------------------------

void UPHCCityHUD::SetDrivingMode(bool bDriving, APawn* Vehicle)
{
	bDrivingMode = bDriving;
	DrivenVehicle = Vehicle;

	const ESlateVisibility WalkVis = bDriving ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	const ESlateVisibility DriveVis = bDriving ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

	// Show fire button for dune buggy and raptor.
	if (FireButton)
	{
		const bool bShowFire = bDriving &&
			(Cast<APHCDuneBuggyPawn>(Vehicle) != nullptr || Cast<APHCRaptorPawn>(Vehicle) != nullptr || Cast<APHCTankPawn>(Vehicle) != nullptr || Cast<APHCBlackbirdPawn>(Vehicle) != nullptr || Cast<APHCBlackHawkPawn>(Vehicle) != nullptr);
		FireButton->SetVisibility(bShowFire ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Show drop bomb button for raptor and blackbird.
	if (DropBombButton)
	{
		const bool bShowDrop = bDriving && (Cast<APHCRaptorPawn>(Vehicle) != nullptr || Cast<APHCBlackbirdPawn>(Vehicle) != nullptr || Cast<APHCBlackHawkPawn>(Vehicle) != nullptr);
		DropBombButton->SetVisibility(bShowDrop ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}

	// Vehicle crosshair
	{
		const bool bShowVehCross = bDriving && (Cast<APHCRaptorPawn>(Vehicle) || Cast<APHCBlackbirdPawn>(Vehicle) || Cast<APHCBlackHawkPawn>(Vehicle) || Cast<APHCDuneBuggyPawn>(Vehicle));
		const ESlateVisibility VCV = bShowVehCross ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
		if (VehicleCrosshairDot) VehicleCrosshairDot->SetVisibility(VCV);
		if (VehicleCrosshairTop) VehicleCrosshairTop->SetVisibility(VCV);
		if (VehicleCrosshairBottom) VehicleCrosshairBottom->SetVisibility(VCV);
		if (VehicleCrosshairLeft) VehicleCrosshairLeft->SetVisibility(VCV);
		if (VehicleCrosshairRight) VehicleCrosshairRight->SetVisibility(VCV);
	}

	// Toggle on-foot controls.
	if (CarButton) { CarButton->SetVisibility(WalkVis); }
	// EnterVehicleButton visibility is handled by proximity check in NativeTick
	if (EnterVehicleButton && bDriving) { EnterVehicleButton->SetVisibility(ESlateVisibility::Collapsed); }
	if (WeaponsButton) { WeaponsButton->SetVisibility(WalkVis); }
	if (WeaponFireButton)
	{
		// Hide fire button while driving; restore if weapon is equipped on exit
		if (bDriving)
		{
			WeaponFireButton->SetVisibility(ESlateVisibility::Collapsed);
		}
		else if (EquippedWeaponType >= 0)
		{
			WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
		}
	}
	if (HostButton) { HostButton->SetVisibility(ESlateVisibility::Collapsed); }
	if (JoinButton) { JoinButton->SetVisibility(ESlateVisibility::Collapsed); }
#if PLATFORM_IOS || PLATFORM_ANDROID
	// Jump/Sprint/Interact buttons removed.
	// Use SelfHitTestInvisible (not Visible) so touches pass through to buttons underneath.
	// Touch input is handled via raw GetInputTouchState in NativeTick, not UMG events.
	{
		const ESlateVisibility WalkPanelVis = bDriving ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible;
		if (WalkMovePanel) { WalkMovePanel->SetVisibility(WalkPanelVis); }
		if (WalkLookPanel) { WalkLookPanel->SetVisibility(WalkPanelVis); }
	}

	// Toggle driving controls.
	// Use SelfHitTestInvisible for info panels so touches pass through to
	// GetInputTouchState (same approach as walking panels above).
	{
		const ESlateVisibility DrivePanelVis = bDriving ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;
		if (SteeringPanel) { SteeringPanel->SetVisibility(DrivePanelVis); }
		if (GasPedalPanel) { GasPedalPanel->SetVisibility(DrivePanelVis); }
		if (BrakePedalPanel) { BrakePedalPanel->SetVisibility(DrivePanelVis); }
	}
	if (ExitVehicleButton) { ExitVehicleButton->SetVisibility(DriveVis); }

	// Reset walking touch state.
	WalkMoveFingerIdx = -1;
	WalkLookFingerIdx = -1;
	WalkMoveOrigin = FVector2D::ZeroVector;
	WalkLookPrev = FVector2D::ZeroVector;
#endif

	// Zero out vehicle input when exiting.
	if (!bDriving && Vehicle)
	{
		if (APHCVehiclePawn* V = Cast<APHCVehiclePawn>(Vehicle))
		{
			V->ThrottleInput = 0.f;
			V->SteeringInput = 0.f;
		}
		else if (APHCDuneBuggyPawn* B = Cast<APHCDuneBuggyPawn>(Vehicle))
		{
			B->ThrottleInput = 0.f;
			B->SteeringInput = 0.f;
		}
		else if (APHCRaptorPawn* R = Cast<APHCRaptorPawn>(Vehicle))
		{
			R->ThrottleInput = 0.f;
			R->SteeringInput = 0.f;
			R->PitchInput = 0.f;
		}
		else if (APHCDirtBikePawn* DB = Cast<APHCDirtBikePawn>(Vehicle))
		{
			DB->ThrottleInput = 0.f;
			DB->SteeringInput = 0.f;
		}
		else if (APHCBlackbirdPawn* BB = Cast<APHCBlackbirdPawn>(Vehicle))
		{
			BB->ThrottleInput = 0.f;
			BB->SteeringInput = 0.f;
			BB->PitchInput = 0.f;
		}
		else if (APHCBlackHawkPawn* BH = Cast<APHCBlackHawkPawn>(Vehicle))
		{
			BH->ThrottleInput = 0.f;
			BH->SteeringInput = 0.f;
			BH->PitchInput = 0.f;
		}
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("UPHCCityHUD: Driving mode %s."),
		bDriving ? TEXT("ON") : TEXT("OFF"));
}

// ---------------------------------------------------------------------------
// Minimap radar
// ---------------------------------------------------------------------------

void UPHCCityHUD::UpdateMinimap(float DeltaTime)
{
	if (!MinimapCanvas || !MinimapBorder) return;

	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	APawn* LocalPawn = PC->GetPawn();
	if (!LocalPawn) return;

	const FVector LocalPos = LocalPawn->GetActorLocation();
	const float PlayerYaw = LocalPawn->GetActorRotation().Yaw;
	const float Half = MinimapSize * 0.5f;
	const FLinearColor RadarGreen(0.0f, 1.0f, 0.3f, 1.f);

	// ---- Animate radar sweep (full rotation every 3 seconds) ----
	RadarSweepAngle += DeltaTime * 120.f; // 120 deg/s = 3s per revolution
	if (RadarSweepAngle >= 360.f) RadarSweepAngle -= 360.f;

	if (RadarSweepLine)
	{
		if (UCanvasPanelSlot* SwSlot = Cast<UCanvasPanelSlot>(RadarSweepLine->Slot))
		{
			const float SwRad = FMath::DegreesToRadians(RadarSweepAngle);
			const float EndX = Half + FMath::Sin(SwRad) * Half;
			const float EndY = Half - FMath::Cos(SwRad) * Half;
			// Position the sweep line — 2px wide from center toward edge
			SwSlot->SetPosition(FVector2D(
				Half - 1.f + FMath::Sin(SwRad) * Half * 0.5f,
				Half - FMath::Cos(SwRad) * Half * 0.5f));
		}

		// Pulse the sweep line brightness
		const float Pulse = 0.4f + 0.2f * FMath::Sin(RadarSweepAngle * 0.1f);
		RadarSweepLine->SetBrushColor(FLinearColor(0.0f, 1.0f, 0.3f, Pulse));
	}

	// ---- Direction indicator (fixed pointing up — heading-up radar) ----
	if (MinimapDirectionIndicator)
	{
		// Player always faces up on heading-up radar
		const float DirX = Half - 1.f;
		const float DirY = Half - 28.f;

		if (UCanvasPanelSlot* DirSlot = Cast<UCanvasPanelSlot>(MinimapDirectionIndicator->Slot))
		{
			DirSlot->SetPosition(FVector2D(DirX, DirY));
		}
	}

	// ---- Place blips for other players and NPC helis ----
	UWorld* World = GetWorld();
	if (!World) return;

	AGameStateBase* GS = World->GetGameState();
	if (!GS) return;

	int32 DotIndex = 0;

	// Lambda to place a blip rotated by player heading (heading-up radar)
	const float YawRad = FMath::DegreesToRadians(-PlayerYaw);
	const float CY = FMath::Cos(YawRad);
	const float SY_rot = FMath::Sin(YawRad);

	auto PlaceBlip = [&](const FVector& WorldPos, const FLinearColor& Color, float DotSize)
	{
		const FVector RelPos = WorldPos - LocalPos;
		// Rotate relative position by player yaw so forward is always up
		float RX = RelPos.X * CY - RelPos.Y * SY_rot;
		float RY = RelPos.X * SY_rot + RelPos.Y * CY;
		float MmX = RX / MinimapRadius * Half;
		float MmY = -RY / MinimapRadius * Half;

		const float Dist = FMath::Sqrt(MmX * MmX + MmY * MmY);
		if (Dist > Half - 4.f)
		{
			const float Sc = (Half - 4.f) / Dist;
			MmX *= Sc;
			MmY *= Sc;
		}

		const float SX = Half + MmX - DotSize * 0.5f;
		const float SY = Half - MmY - DotSize * 0.5f;

		if (DotIndex >= MinimapPlayerDots.Num())
		{
			UBorder* NewDot = WidgetTree->ConstructWidget<UBorder>(
				UBorder::StaticClass(), *FString::Printf(TEXT("MinimapDot%d"), DotIndex));
			NewDot->SetBrushColor(Color);
			NewDot->SetPadding(FMargin(0.f));

			UCanvasPanelSlot* DS = MinimapCanvas->AddChildToCanvas(NewDot);
			DS->SetSize(FVector2D(DotSize, DotSize));
			DS->SetAutoSize(false);

			MinimapPlayerDots.Add(NewDot);
		}

		UBorder* Dot = MinimapPlayerDots[DotIndex];
		Dot->SetBrushColor(Color);
		Dot->SetVisibility(ESlateVisibility::HitTestInvisible);

		if (UCanvasPanelSlot* DS = Cast<UCanvasPanelSlot>(Dot->Slot))
		{
			DS->SetPosition(FVector2D(SX, SY));
			DS->SetSize(FVector2D(DotSize, DotSize));
		}

		DotIndex++;
	};

	// Other players — bright green blips
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) continue;
		APawn* OtherPawn = PS->GetPawn();
		if (!OtherPawn || OtherPawn == LocalPawn) continue;

		APHCCityCharacter* OtherChar = Cast<APHCCityCharacter>(OtherPawn);
		if (OtherChar && OtherChar->bIsTripping) continue;

		PlaceBlip(OtherPawn->GetActorLocation(), RadarGreen, 6.f);
	}

	// NPC helicopters — large pulsing red blips
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (CitySub)
	{
		const float Pulse = 0.6f + 0.4f * FMath::Sin(GetWorld()->GetTimeSeconds() * 4.f);
		const FLinearColor HeliRed(1.f, 0.15f, 0.1f, Pulse);
		if (CitySub->NPCHelicopter && CitySub->bNPCHeliVisible)
			PlaceBlip(CitySub->NPCHelicopter->GetActorLocation(), HeliRed, 12.f);
		if (CitySub->NPCHelicopter2 && CitySub->bNPCHeliVisible2)
			PlaceBlip(CitySub->NPCHelicopter2->GetActorLocation(), HeliRed, 12.f);

		// NPC player characters — cyan blips
		const FLinearColor NPCCyan(0.0f, 0.9f, 1.0f, 0.9f);
		for (int32 ni = 0; ni < CitySub->NumNPCPlayers; ++ni)
		{
			const auto& NP = CitySub->NPCPlayers[ni];
			if (NP.Actor && NP.bVisible)
				PlaceBlip(NP.Actor->GetActorLocation(), NPCCyan, 7.f);
		}
	}

	// Hide unused dots
	for (int32 i = DotIndex; i < MinimapPlayerDots.Num(); ++i)
	{
		MinimapPlayerDots[i]->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ---------------------------------------------------------------------------
// Currency display
// ---------------------------------------------------------------------------

void UPHCCityHUD::UpdateCurrencyDisplay(int32 Coins, int32 Gems)
{
	if (CoinText)
	{
		CoinText->SetText(FText::FromString(FString::Printf(TEXT("Coins: %d"), Coins)));
	}
	if (GemText)
	{
		GemText->SetText(FText::FromString(FString::Printf(TEXT("Gems: %d"), Gems)));
	}
}

// ---------------------------------------------------------------------------
// Menu overlay
// ---------------------------------------------------------------------------

void UPHCCityHUD::OpenMenu()
{
	if (bMenuOpen)
	{
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC || !MainHUDClass)
	{
		return;
	}

	MenuOverlay = CreateWidget<UPHCMainHUD>(PC, MainHUDClass);
	if (MenuOverlay)
	{
		MenuOverlay->AddToViewport(50); // Above the city HUD, below popups
		bMenuOpen = true;

		// Switch to Game+UI input so the menu is interactable while world is visible.
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);

		UE_LOG(LogPlayhouseCity, Log, TEXT("UPHCCityHUD: Menu opened."));
	}
}

void UPHCCityHUD::CloseMenu()
{
	if (!bMenuOpen || !MenuOverlay)
	{
		return;
	}

	MenuOverlay->RemoveFromParent();
	MenuOverlay = nullptr;
	bMenuOpen = false;

	// Restore game+UI input mode for the 3D world.
	if (APlayerController* PC = GetOwningPlayer())
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		PC->SetInputMode(InputMode);
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("UPHCCityHUD: Menu closed."));
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UPHCCityHUD::OnVoiceMuteClicked()
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (GI)
	{
		FAgoraVoiceManager& Agora = FAgoraVoiceManager::Get();

		// First press: init Agora and join channel
		if (!Agora.IsInChannel())
		{
			GI->EnableVoiceChat();
			// Show debug status
			if (VoiceMuteLabel)
			{
				FString Status = FString::Printf(TEXT("JOINING... init=%d ch=%d"),
					Agora.IsInChannel() ? 1 : 0, Agora.IsMuted() ? 1 : 0);
				VoiceMuteLabel->SetText(FText::FromString(Status));
			}

			// Delayed status update after Agora has time to connect
			if (GetWorld())
			{
				FTimerHandle DebugTimer;
				TWeakObjectPtr<UTextBlock> WeakLabel = VoiceMuteLabel;
				GetWorld()->GetTimerManager().SetTimer(DebugTimer, [WeakLabel, &Agora]()
				{
					if (WeakLabel.IsValid())
					{
						FString S = FString::Printf(TEXT("ch=%d r=%d e=%d"),
							Agora.IsInChannel() ? 1 : 0,
							Agora.GetRemoteUserCount(),
							Agora.GetLastError());
						WeakLabel->SetText(FText::FromString(S));
					}
				}, 3.0f, false);
			}
			return;
		}

		// Subsequent presses: toggle mute
		GI->ToggleVoiceMute();

		if (VoiceMuteLabel)
		{
			FString DebugText = FString::Printf(TEXT("%s ch=%d r=%d e=%d"),
				GI->bVoiceMuted ? TEXT("MUTED") : TEXT("LIVE"),
				Agora.IsInChannel() ? 1 : 0,
				Agora.GetRemoteUserCount(),
				Agora.GetLastError());
			VoiceMuteLabel->SetText(FText::FromString(DebugText));
		}
		if (VoiceMuteButton)
		{
			FButtonStyle Style = VoiceMuteButton->GetStyle();
			if (GI->bVoiceMuted)
			{
				Style.Normal.TintColor = FSlateColor(FLinearColor(0.6f, 0.1f, 0.1f, 0.5f));
			}
			else
			{
				Style.Normal.TintColor = FSlateColor(FLinearColor(0.1f, 0.6f, 0.1f, 0.5f));
			}
			VoiceMuteButton->SetStyle(Style);
		}
	}
}

void UPHCCityHUD::OnMenuButtonClicked()
{
	if (bMenuOpen)
	{
		CloseMenu();
	}
	else
	{
		OpenMenu();
	}
}

void UPHCCityHUD::OnCloseMenuButtonClicked()
{
	CloseMenu();
}

void UPHCCityHUD::OnFireButtonPressed()
{
	bFireHeld = true;
	// Fire immediately on press
	if (APHCDuneBuggyPawn* Buggy = Cast<APHCDuneBuggyPawn>(DrivenVehicle))
	{
		Buggy->FireMissile();
	}
	else if (APHCRaptorPawn* Raptor = Cast<APHCRaptorPawn>(DrivenVehicle))
	{
		Raptor->FireRocket();
	}
	else if (APHCTankPawn* Tank = Cast<APHCTankPawn>(DrivenVehicle))
	{
		Tank->FireCannon();
	}
	else if (APHCBlackbirdPawn* BB = Cast<APHCBlackbirdPawn>(DrivenVehicle))
	{
		BB->FireRocket();
	}
	else if (APHCBlackHawkPawn* BH = Cast<APHCBlackHawkPawn>(DrivenVehicle))
	{
		BH->FireRocket();
	}
}

void UPHCCityHUD::OnFireButtonReleased()
{
	bFireHeld = false;
}

void UPHCCityHUD::OnCarButtonClicked()
{
	ShowVehicleSelection();
}

void UPHCCityHUD::OnEnterVehicleClicked()
{
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		PC->EnterNearestVehicle();
	}
}

void UPHCCityHUD::ShowVehicleSelection()
{
	if (VehicleSelectOverlay)
	{
		VehicleSelectOverlay->SetVisibility(ESlateVisibility::Visible);
		bVehicleSelectOpen = true;

		// Update button labels with lock status
		APHCCityCharacter* Char = nullptr;
		if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
			Char = Cast<APHCCityCharacter>(PC->GetPawn());

		auto UpdateVehLabel = [&](UButton* Btn, int32 VehType, const TCHAR* Name)
		{
			if (!Btn || Btn->GetChildrenCount() == 0) return;
			UTextBlock* Lbl = Cast<UTextBlock>(Btn->GetChildAt(0));
			if (!Lbl) return;
			if (Char && !Char->IsVehicleUnlocked(VehType))
			{
				const int32 Req = APHCCityCharacter::VehicleUnlockLevel(VehType);
				Lbl->SetText(FText::FromString(FString::Printf(TEXT("%s\nLV %d"), Name, Req)));
				Lbl->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.5f)));
			}
			else
			{
				Lbl->SetText(FText::FromString(Name));
				Lbl->SetColorAndOpacity(FSlateColor(FLinearColor::White));
			}
		};

		UpdateVehLabel(SelectAventadorBtn, 0, TEXT("Lamborghini"));
		UpdateVehLabel(SelectBuggyBtn, 1, TEXT("Dune Buggy"));
		UpdateVehLabel(SelectRaptorBtn, 2, TEXT("F-22 Raptor"));
		UpdateVehLabel(SelectTankBtn, 3, TEXT("M60 Tank"));
		UpdateVehLabel(SelectDirtBikeBtn, 4, TEXT("Mountain Bike"));
		UpdateVehLabel(SelectBlackHawkBtn, 5, TEXT("Black Hawk"));
		UpdateVehLabel(SelectRollerBladesBtn, 7, TEXT("Roller Blades"));
	}
}

void UPHCCityHUD::HideVehicleSelection()
{
	if (VehicleSelectOverlay)
	{
		VehicleSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);
		bVehicleSelectOpen = false;
	}
}

void UPHCCityHUD::OnSelectAventador()
{
	HideVehicleSelection();
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* C = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			if (!C->IsVehicleUnlocked(0)) return; // Level 1
			C->VehiclesUsed |= (1 << 0);
		}
		PC->SpawnAndEnterVehicle(0);
	}
}

void UPHCCityHUD::OnSelectBuggy()
{
	HideVehicleSelection();
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* C = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			if (!C->IsVehicleUnlocked(1)) return; // Level 3
			C->VehiclesUsed |= (1 << 1);
		}
		PC->SpawnAndEnterVehicle(1);
	}
}

void UPHCCityHUD::OnSelectRaptor()
{
	HideVehicleSelection();
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* C = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			if (!C->IsVehicleUnlocked(2)) return; // Level 12
			C->VehiclesUsed |= (1 << 2);
		}
		PC->SpawnAndEnterVehicle(2);
	}
}

void UPHCCityHUD::OnSelectJetpack()
{
	HideVehicleSelection();
	APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer());
	if (!PC) return;
	APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn());
	if (!Char) return;

	if (Char->bJetpackEquipped)
		PC->UnequipJetpack();
	else
		PC->EquipJetpack();
}

void UPHCCityHUD::OnSelectTank()
{
	HideVehicleSelection();
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* C = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			if (!C->IsVehicleUnlocked(3)) return; // Level 8
			C->VehiclesUsed |= (1 << 3);
		}
		PC->SpawnAndEnterVehicle(3);
	}
}

void UPHCCityHUD::OnSelectDirtBike()
{
	HideVehicleSelection();
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* C = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			if (!C->IsVehicleUnlocked(4)) return; // Level 5
			C->VehiclesUsed |= (1 << 4);
		}
		PC->SpawnAndEnterVehicle(4);
	}
}

void UPHCCityHUD::OnSelectBlackHawk()
{
	HideVehicleSelection();
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* C = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			if (!C->IsVehicleUnlocked(5)) return; // Level 18
			C->VehiclesUsed |= (1 << 5);
		}
		PC->SpawnAndEnterVehicle(5);
	}
}

void UPHCCityHUD::OnSelectRollerBlades()
{
	HideVehicleSelection();
	APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer());
	if (!PC) return;
	APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn());
	if (!Char) return;

	if (!Char->IsVehicleUnlocked(7)) return; // Level 2
	Char->VehiclesUsed |= (1 << 7);

	if (Char->bRollerBladesEquipped)
		PC->UnequipRollerBlades();
	else
		PC->EquipRollerBlades();
}



void UPHCCityHUD::OnDropBombClicked()
{
	// Try cached DrivenVehicle first, fall back to current pawn
	APawn* Vehicle = DrivenVehicle;
	if (!Vehicle)
	{
		if (APlayerController* PC = GetOwningPlayer())
		{
			Vehicle = PC->GetPawn();
		}
	}
	if (!Vehicle) return;

	if (APHCRaptorPawn* Raptor = Cast<APHCRaptorPawn>(Vehicle))
	{
		Raptor->DropBomb();
	}
	else if (APHCBlackbirdPawn* BB = Cast<APHCBlackbirdPawn>(Vehicle))
	{
		BB->DropBomb();
	}
	else if (APHCBlackHawkPawn* BH = Cast<APHCBlackHawkPawn>(Vehicle))
	{
		BH->DropBomb();
	}
}

// Jump/Sprint/Interact button callbacks removed — mobile uses touch panels.

// ---------------------------------------------------------------------------
// Driving mode button callback
// ---------------------------------------------------------------------------

void UPHCCityHUD::OnExitVehicleClicked()
{
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		PC->OnVehicleExitRequested();
	}
}

// ---------------------------------------------------------------------------
// Multiplayer button callbacks
// ---------------------------------------------------------------------------

void UPHCCityHUD::OnHostButtonClicked()
{
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (GI)
	{
		if (GI->HostGame())
		{
			// Visual feedback: update button to show hosting state.
			if (HostButton)
			{
				if (UTextBlock* Label = Cast<UTextBlock>(HostButton->GetChildAt(0)))
				{
					Label->SetText(FText::FromString(TEXT("Hosting")));
				}
				HostButton->SetIsEnabled(false);
			}
			if (JoinButton)
			{
				JoinButton->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

void UPHCCityHUD::OnJoinButtonClicked()
{
	if (JoinOverlay)
	{
		JoinOverlay->SetVisibility(ESlateVisibility::Visible);
	}
}

void UPHCCityHUD::OnJoinConnectClicked()
{
	if (JoinOverlay)
	{
		JoinOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (GI && !EnteredIP.IsEmpty())
	{
		GI->JoinGame(EnteredIP);
	}
}

void UPHCCityHUD::OnJoinCancelClicked()
{
	if (JoinOverlay)
	{
		JoinOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

// ---------------------------------------------------------------------------
// Weapons
// ---------------------------------------------------------------------------

void UPHCCityHUD::OnWeaponsButtonClicked()
{
	ShowWeaponSelection();
}

void UPHCCityHUD::OnNightButtonClicked()
{
	UWorld* World = GetWorld();
	if (!World) return;

	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;

	if (!CitySub->bNightModeLocked)
	{
		CitySub->DayNightTime = 0.75f;
		CitySub->bNightModeLocked = true;
		if (NightButtonLabel)
			NightButtonLabel->SetText(FText::FromString(TEXT("Cycle")));
	}
	else
	{
		CitySub->bNightModeLocked = false;
		if (NightButtonLabel)
			NightButtonLabel->SetText(FText::FromString(TEXT("Night")));
	}
}

void UPHCCityHUD::OnDevModeButtonClicked()
{
	if (!DevTimePanel) return;
	bool bVisible = DevTimePanel->GetVisibility() == ESlateVisibility::Visible;
	DevTimePanel->SetVisibility(bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
}

void UPHCCityHUD::OnDevTimeSelected(int32 TimeIndex)
{
	UWorld* World = GetWorld();
	if (!World) return;
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;

	// Time values: 0=sunrise, 0.25=noon, 0.5=sunset, 0.75=midnight
	const float Times[] = { 0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, -1.f };

	if (TimeIndex == 7)
	{
		// Resume cycle
		CitySub->bNightModeLocked = false;
		if (DevTimePanel) DevTimePanel->SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	if (TimeIndex >= 0 && TimeIndex < 7)
	{
		CitySub->DayNightTime = Times[TimeIndex];
		CitySub->bNightModeLocked = true;
	}

	if (DevTimePanel) DevTimePanel->SetVisibility(ESlateVisibility::Collapsed);
}

// ---------------------------------------------------------------------------
// Disaster selection
// ---------------------------------------------------------------------------

void UPHCCityHUD::OnDisastersButtonClicked()
{
	if (bDisasterSelectOpen)
	{
		HideDisasterSelection();
	}
	else
	{
		ShowDisasterSelection();
	}
}

void UPHCCityHUD::ShowDisasterSelection()
{
	if (DisasterSelectOverlay)
	{
		DisasterSelectOverlay->SetVisibility(ESlateVisibility::Visible);
		bDisasterSelectOpen = true;
	}
}

void UPHCCityHUD::HideDisasterSelection()
{
	if (DisasterSelectOverlay)
	{
		DisasterSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);
		bDisasterSelectOpen = false;
	}
}

void UPHCCityHUD::OnSelectTornado()
{
	HideDisasterSelection();
	UWorld* World = GetWorld();
	if (!World) return;
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;
	CitySub->ActiveDisaster = UPHCCitySubsystem::EDisasterType::Tornado;
	CitySub->DisasterTimer = 30.f;
}

void UPHCCityHUD::OnSelectEarthquake()
{
	HideDisasterSelection();
	UWorld* World = GetWorld();
	if (!World) return;
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;
	CitySub->ActiveDisaster = UPHCCitySubsystem::EDisasterType::Earthquake;
	CitySub->DisasterTimer = 20.f;
}

void UPHCCityHUD::OnSelectThunderstorm()
{
	HideDisasterSelection();
	UWorld* World = GetWorld();
	if (!World) return;
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;
	CitySub->ActiveDisaster = UPHCCitySubsystem::EDisasterType::Thunderstorm;
	CitySub->DisasterTimer = 45.f;
}

void UPHCCityHUD::OnSelectMeteor()
{
	HideDisasterSelection();
	UWorld* World = GetWorld();
	if (!World) return;
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;
	CitySub->ActiveDisaster = UPHCCitySubsystem::EDisasterType::MeteorShower;
	CitySub->DisasterTimer = 40.f;
}

void UPHCCityHUD::OnStopDisaster()
{
	HideDisasterSelection();
	UWorld* World = GetWorld();
	if (!World) return;
	UPHCCitySubsystem* CitySub = World->GetSubsystem<UPHCCitySubsystem>();
	if (!CitySub) return;
	CitySub->ActiveDisaster = UPHCCitySubsystem::EDisasterType::None;
	CitySub->DisasterTimer = 0.f;
}

// ---------------------------------------------------------------------------

void UPHCCityHUD::ShowWeaponSelection()
{
	if (WeaponSelectOverlay)
	{
		WeaponSelectOverlay->SetVisibility(ESlateVisibility::Visible);
		bWeaponSelectOpen = true;
	}
}

void UPHCCityHUD::HideWeaponSelection()
{
	if (WeaponSelectOverlay)
	{
		WeaponSelectOverlay->SetVisibility(ESlateVisibility::Collapsed);
		bWeaponSelectOpen = false;
	}
}

void UPHCCityHUD::OnSelectGun()
{
	HideWeaponSelection();
	EquippedWeaponType = 0;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->CurrentWeaponType = 0;
		}
	}

	if (WeaponFireLabel) WeaponFireLabel->SetText(FText::FromString(TEXT("SPRAY")));
	if (WeaponFireButton) WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
}

void UPHCCityHUD::OnSelectRPG()
{
	HideWeaponSelection();
	EquippedWeaponType = 1;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->CurrentWeaponType = 1;
		}
	}

	if (WeaponFireLabel) WeaponFireLabel->SetText(FText::FromString(TEXT("Fire RPG")));
	if (WeaponFireButton) WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
}

void UPHCCityHUD::OnSelectSword()
{
	HideWeaponSelection();
	EquippedWeaponType = 2;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->CurrentWeaponType = 2;
		}
	}

	if (WeaponFireLabel) WeaponFireLabel->SetText(FText::FromString(TEXT("Swing")));
	if (WeaponFireButton) WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
}

void UPHCCityHUD::OnSelectScythe()
{
	HideWeaponSelection();
	EquippedWeaponType = 3;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->CurrentWeaponType = 3;
		}
	}

	if (WeaponFireLabel) WeaponFireLabel->SetText(FText::FromString(TEXT("Swing")));
	if (WeaponFireButton) WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
}

void UPHCCityHUD::OnSelectDagger()
{
	HideWeaponSelection();
	EquippedWeaponType = 4;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->CurrentWeaponType = 4;
		}
	}

	if (WeaponFireLabel) WeaponFireLabel->SetText(FText::FromString(TEXT("Stab")));
	if (WeaponFireButton) WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
}

void UPHCCityHUD::OnSelectAK47()
{
	HideWeaponSelection();
	EquippedWeaponType = 5;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->CurrentWeaponType = 5;
		}
	}

	if (WeaponFireLabel) WeaponFireLabel->SetText(FText::FromString(TEXT("SPRAY")));
	if (WeaponFireButton) WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
}

void UPHCCityHUD::OnSelectSniper()
{
	HideWeaponSelection();
	EquippedWeaponType = 6;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->CurrentWeaponType = 6;
		}
	}

	if (WeaponFireLabel) WeaponFireLabel->SetText(FText::FromString(TEXT("FIRE")));
	if (WeaponFireButton) WeaponFireButton->SetVisibility(ESlateVisibility::Visible);
}

void UPHCCityHUD::OnWeaponFirePressed()
{
	bWeaponFireHeld = true;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			if (EquippedWeaponType == 0) // Minigun — hold to spray
			{
				Char->bMinigunFiring = true;
			}
			else if (EquippedWeaponType == 5) // AK-47 — hold to spray
			{
				Char->bAK47Firing = true;
			}
			else // All other weapons — single fire on press
			{
				Char->FireWeapon();
			}
		}
	}
}

void UPHCCityHUD::OnWeaponFireReleased()
{
	bWeaponFireHeld = false;

	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->bMinigunFiring = false;
			Char->bAK47Firing = false;
		}
	}
}

void UPHCCityHUD::OnCameraModeButtonClicked()
{
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->SetFirstPersonMode(!Char->bFirstPersonMode);
		}
	}
}

void UPHCCityHUD::OnLobbyButtonClicked()
{
	ShowLobby();
}

void UPHCCityHUD::OnLobbyCloseClicked()
{
	HideLobby();
}

void UPHCCityHUD::ShowLobby()
{
	RefreshLobbyPlayerList();
	if (LobbyOverlay)
	{
		LobbyOverlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
	bLobbyOpen = true;
}

void UPHCCityHUD::HideLobby()
{
	if (LobbyOverlay)
	{
		LobbyOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	bLobbyOpen = false;
}

void UPHCCityHUD::RefreshLobbyPlayerList()
{
	if (!LobbyPlayerList) return;

	// Clear existing rows.
	LobbyPlayerList->ClearChildren();

	const FSlateFontInfo NameFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
	const FSlateFontInfo LevelFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 14);
	const FSlateFontInfo XPFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 12);

	// Lambda to add a player row.
	auto AddPlayerRow = [&](const FString& Name, int32 Level, int32 XP, int32 Coins)
	{
		UBorder* Row = NewObject<UBorder>(this);
		Row->SetBrushColor(FLinearColor(0.12f, 0.12f, 0.18f, 0.5f));
		Row->SetPadding(FMargin(16.f, 12.f, 16.f, 12.f));

		UHorizontalBox* HBox = NewObject<UHorizontalBox>(this);
		Row->SetContent(HBox);

		// Username
		UTextBlock* NameTB = NewObject<UTextBlock>(this);
		NameTB->SetText(FText::FromString(Name));
		NameTB->SetFont(NameFont);
		NameTB->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		UHorizontalBoxSlot* NameSlot = HBox->AddChildToHorizontalBox(NameTB);
		NameSlot->SetVerticalAlignment(VAlign_Center);
		NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); // fill

		// Level
		UTextBlock* LvlTB = NewObject<UTextBlock>(this);
		LvlTB->SetText(FText::FromString(FString::Printf(TEXT("Lv. %d"), Level)));
		LvlTB->SetFont(LevelFont);
		LvlTB->SetColorAndOpacity(FSlateColor(FLinearColor(0.6f, 0.8f, 1.0f, 1.f)));
		UHorizontalBoxSlot* LvlSlot = HBox->AddChildToHorizontalBox(LvlTB);
		LvlSlot->SetVerticalAlignment(VAlign_Center);
		LvlSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));

		// XP
		UTextBlock* XPTB = NewObject<UTextBlock>(this);
		XPTB->SetText(FText::FromString(FString::Printf(TEXT("XP: %d"), XP)));
		XPTB->SetFont(XPFont);
		XPTB->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.5f, 0.6f, 1.f)));
		UHorizontalBoxSlot* XPSlot = HBox->AddChildToHorizontalBox(XPTB);
		XPSlot->SetVerticalAlignment(VAlign_Center);
		XPSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));

		// Coins
		UTextBlock* CoinsTB = NewObject<UTextBlock>(this);
		CoinsTB->SetText(FText::FromString(FString::Printf(TEXT("%d coins"), Coins)));
		CoinsTB->SetFont(XPFont);
		CoinsTB->SetColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.85f, 0.3f, 1.f)));
		UHorizontalBoxSlot* CoinsSlot = HBox->AddChildToHorizontalBox(CoinsTB);
		CoinsSlot->SetVerticalAlignment(VAlign_Center);
		CoinsSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));

		UVerticalBoxSlot* RowSlot = LobbyPlayerList->AddChildToVerticalBox(Row);
		RowSlot->SetHorizontalAlignment(HAlign_Fill);
		RowSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	};

	// Get the local player's controller to identify which PlayerState is ours.
	APlayerController* LocalPC = GetOwningPlayer();
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);

	bool bAddedLocal = false;

	// Always add the local player first, using LocalPlayerProfile (most up-to-date data).
	if (GI)
	{
		const FPHCPlayerProfile& P = GI->LocalPlayerProfile;
		FString Name = P.DisplayName.IsEmpty() ? P.Username : P.DisplayName;
		if (Name.IsEmpty()) Name = TEXT("Player");
		AddPlayerRow(Name, P.Level, P.XP, P.Coins);
		bAddedLocal = true;
	}

	// Add other connected players from GameState->PlayerArray.
	if (UWorld* World = GetWorld())
	{
		if (AGameStateBase* GS = World->GetGameState())
		{
			for (APlayerState* PS : GS->PlayerArray)
			{
				// Skip the local player — already added above from LocalPlayerProfile.
				if (LocalPC && PS == LocalPC->PlayerState)
					continue;

				if (APHCPlayerState* PHCState = Cast<APHCPlayerState>(PS))
				{
					const FPHCPlayerProfile& P = PHCState->GetPlayerProfile();
					FString Name = P.DisplayName.IsEmpty() ? P.Username : P.DisplayName;
					if (Name.IsEmpty()) Name = PS->GetPlayerName();
					if (Name.IsEmpty()) Name = TEXT("Player");
					AddPlayerRow(Name, P.Level, P.XP, P.Coins);
				}
			}
		}
	}
}

void UPHCCityHUD::OnScopeZoomInPressed()
{
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->SniperZoomFOV = FMath::Clamp(Char->SniperZoomFOV * 0.7f,
				APHCCityCharacter::SniperZoomMin, APHCCityCharacter::SniperZoomMax);
		}
	}
}

void UPHCCityHUD::OnScopeZoomOutPressed()
{
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->SniperZoomFOV = FMath::Clamp(Char->SniperZoomFOV / 0.7f,
				APHCCityCharacter::SniperZoomMin, APHCCityCharacter::SniperZoomMax);
		}
	}
}

void UPHCCityHUD::OnThermalTogglePressed()
{
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			Char->bThermalScope = !Char->bThermalScope;
		}
	}
}

void UPHCCityHUD::OnADSPressed()
{
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			// Toggle ADS on press (tap to scope in, tap again to scope out)
			Char->SetADS(!Char->bADS);
		}
	}
}

void UPHCCityHUD::OnADSReleased()
{
	// No-op: ADS is now toggled on press, not held
}

void UPHCCityHUD::OnDailyLoginClaimClicked()
{
	if (APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer()))
	{
		if (APHCCityCharacter* Char = Cast<APHCCityCharacter>(PC->GetPawn()))
		{
			const int32 CoinsAwarded = Char->ClaimDailyReward();

			// Update HUD currency display
			if (UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this))
			{
				UpdateCurrencyDisplay(GI->LocalPlayerProfile.Coins, GI->LocalPlayerProfile.Gems);
			}

			// Hide the popup
			if (DailyLoginOverlay)
			{
				DailyLoginOverlay->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// Psychedelic trip overlay — procedural DeepDream-style textures
// ---------------------------------------------------------------------------

static FLinearColor HSVToLinear(float H, float S, float V, float A = 1.f)
{
	// H in 0..1
	H = FMath::Frac(H);
	const float C = V * S;
	const float X = C * (1.f - FMath::Abs(FMath::Fmod(H * 6.f, 2.f) - 1.f));
	const float M = V - C;
	float R, G, B;
	if (H < 1.f / 6.f)      { R = C; G = X; B = 0; }
	else if (H < 2.f / 6.f) { R = X; G = C; B = 0; }
	else if (H < 3.f / 6.f) { R = 0; G = C; B = X; }
	else if (H < 4.f / 6.f) { R = 0; G = X; B = C; }
	else if (H < 5.f / 6.f) { R = X; G = 0; B = C; }
	else                     { R = C; G = 0; B = X; }
	return FLinearColor(R + M, G + M, B + M, A);
}

UTexture2D* UPHCCityHUD::GenerateTripTexture(int32 Pattern, int32 Size)
{
	UTexture2D* Tex = UTexture2D::CreateTransient(Size, Size, PF_B8G8R8A8);
	if (!Tex) return nullptr;

	FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
	Mip.BulkData.Lock(LOCK_READ_WRITE);
	uint8* Data = static_cast<uint8*>(Mip.BulkData.Realloc(Size * Size * 4));

	// Psychedelic palette (DMT hyperspace — neon, electric, self-luminous)
	static const FLinearColor Palette[] = {
		FLinearColor(0.00f, 0.27f, 1.00f),  // 0: hyperspace blue
		FLinearColor(0.61f, 0.19f, 1.00f),  // 1: neon purple
		FLinearColor(1.00f, 0.40f, 0.00f),  // 2: deep orange
		FLinearColor(1.00f, 0.84f, 0.00f),  // 3: shimmering gold
		FLinearColor(0.00f, 1.00f, 0.50f),  // 4: emerald green
		FLinearColor(0.81f, 0.75f, 1.00f),  // 5: lavender (#cfc0ff)
		FLinearColor(0.00f, 0.40f, 0.80f),  // 6: dark electric blue
		FLinearColor(1.00f, 0.55f, 0.00f),  // 7: chrysanthemum orange
		FLinearColor(0.40f, 0.00f, 0.80f),  // 8: void purple
		FLinearColor(0.00f, 0.90f, 0.90f),  // 9: electric teal
	};
	constexpr int32 PalN = 10;

	// Helper: sample palette with smooth interpolation
	auto SamplePalette = [&](float T, float Alpha) -> FLinearColor
	{
		T = FMath::Frac(T) * PalN;
		const int32 I0 = FMath::FloorToInt32(T) % PalN;
		const int32 I1 = (I0 + 1) % PalN;
		const float F = T - FMath::FloorToFloat(T);
		FLinearColor C = FMath::Lerp(Palette[I0], Palette[I1], F);
		C.A = Alpha;
		return C;
	};

	const float InvSize = 1.f / static_cast<float>(Size);

	for (int32 Y = 0; Y < Size; ++Y)
	{
		for (int32 X = 0; X < Size; ++X)
		{
			const float U = X * InvSize;
			const float V = Y * InvSize;
			FLinearColor Col(0, 0, 0, 0);

			// Use periodic coordinates so patterns tile seamlessly
			// (all trig functions are inherently periodic over 0-1)
			const float TU = U * PI * 2.f;
			const float TV = V * PI * 2.f;

			// All 3 patterns are radial mandalas centered at (0.5, 0.5)
			const float CX = U - 0.5f;
			const float CY = V - 0.5f;
			const float Dist = FMath::Sqrt(CX * CX + CY * CY);
			const float Angle = FMath::Atan2(CY, CX);
			const float MaxR = 0.5f * 1.414f; // corner distance

			if (Pattern == 0)
			{
				// --- MANDALA 0: TRIBAL CHEVRON RINGS ---
				// Concentric rings filled with zigzag chevron teeth + radial spokes
				float Val = 0.f;
				const int32 NRings = 8;
				for (int32 R = 1; R <= NRings; ++R)
				{
					const float Radius = R * (MaxR / NRings);
					const float RDist = FMath::Abs(Dist - Radius);
					// Ring line
					Val = FMath::Max(Val, FMath::Exp(-RDist * RDist / 0.00015f) * 0.9f);

					// Chevron teeth along each ring (zigzag radially)
					if (Dist > Radius - MaxR / NRings * 0.45f && Dist < Radius)
					{
						const int32 NTeeth = 8 + R * 4;
						const float ToothAngle = FMath::Frac(Angle / (PI * 2.f) * NTeeth);
						const float ToothPeak = FMath::Abs(ToothAngle - 0.5f) * 2.f;
						const float BandPos = 1.f - (Radius - Dist) / (MaxR / NRings * 0.45f);
						const float ToothDist = FMath::Abs(BandPos - ToothPeak * 0.7f);
						Val = FMath::Max(Val, FMath::Exp(-ToothDist * ToothDist / 0.015f) * 0.7f);
					}
				}

				// Main radial spokes (12)
				const int32 NSpokes = 12;
				for (int32 S = 0; S < NSpokes; ++S)
				{
					const float SA = S * PI * 2.f / NSpokes;
					float AD = FMath::Fmod(Angle - SA + PI * 4.f, PI * 2.f) - PI;
					const float SpokeW = 0.03f + 0.015f * FMath::Sin(Dist * 80.f); // notched edges
					Val = FMath::Max(Val, FMath::Exp(-AD * AD / (SpokeW * SpokeW) * 5.f) * 0.75f *
						FMath::Clamp(Dist / 0.03f, 0.f, 1.f));
				}

				// Diamond dots at ring-spoke intersections
				for (int32 R = 1; R <= NRings; R += 2)
				{
					const float Radius = R * (MaxR / NRings);
					for (int32 S = 0; S < NSpokes; ++S)
					{
						const float SA = (S + 0.5f) * PI * 2.f / NSpokes;
						const float DotX = 0.5f + FMath::Cos(SA) * Radius;
						const float DotY = 0.5f + FMath::Sin(SA) * Radius;
						const float DD = FMath::Abs(U - DotX) + FMath::Abs(V - DotY); // diamond metric
						Val = FMath::Max(Val, FMath::Exp(-DD * DD / 0.0004f) * 0.65f);
					}
				}

				// Center ornament
				Val = FMath::Max(Val, FMath::Exp(-Dist * Dist / 0.0004f) * 0.95f);

				Val = FMath::Clamp(Val, 0.f, 1.f);
				float PalT = FMath::Frac(Angle / (PI * 2.f) * 0.4f + Dist * 3.f);
				Col = SamplePalette(PalT, Val * 0.75f);
			}
			else if (Pattern == 1)
			{
				// --- MANDALA 1: PETAL & SACRED GEOMETRY ---
				// Layered petal rings with inner star geometry
				float Val = 0.f;

				// Outer petal rings (3 nested rings of petals)
				const int32 PetalCounts[] = {16, 10, 6};
				const float PetalRadii[] = {0.42f, 0.28f, 0.15f};
				const float PetalLengths[] = {0.18f, 0.14f, 0.10f};
				for (int32 Ring = 0; Ring < 3; ++Ring)
				{
					const int32 NP = PetalCounts[Ring];
					const float PR = PetalRadii[Ring];
					const float PL = PetalLengths[Ring];
					const float Offset = Ring * 0.3f; // angular offset between rings
					for (int32 P = 0; P < NP; ++P)
					{
						const float PA = P * PI * 2.f / NP + Offset;
						float AD = FMath::Fmod(Angle - PA + PI * 4.f, PI * 2.f) - PI;
						const float PetalW = PI / NP * 0.55f;
						if (FMath::Abs(AD) < PetalW && Dist > PR - PL && Dist < PR + PL * 0.3f)
						{
							const float Along = (Dist - (PR - PL)) / (PL * 1.3f);
							const float Across = AD / PetalW;
							// Petal shape: widest at middle, pointed at tips
							const float Width = FMath::Sin(Along * PI);
							if (FMath::Abs(Across) < Width)
							{
								const float Edge = FMath::Abs(Across) / FMath::Max(Width, 0.01f);
								const float PVal = (1.f - Edge * Edge) * 0.85f;
								Val = FMath::Max(Val, PVal);
							}
						}
					}
					// Ring circle
					const float RDist = FMath::Abs(Dist - PR);
					Val = FMath::Max(Val, FMath::Exp(-RDist * RDist / 0.0001f) * 0.7f);
				}

				// Inner star (6-pointed)
				const int32 StarPoints = 6;
				for (int32 S = 0; S < StarPoints; ++S)
				{
					const float SA = S * PI * 2.f / StarPoints;
					float AD = FMath::Fmod(Angle - SA + PI * 4.f, PI * 2.f) - PI;
					// Star spoke: thin line radiating out
					if (Dist < 0.20f)
						Val = FMath::Max(Val, FMath::Exp(-AD * AD / 0.001f) * 0.7f *
							FMath::Clamp(Dist / 0.02f, 0.f, 1.f));
					// Triangular point at tip
					const float PointR = 0.18f;
					if (Dist > 0.12f && Dist < PointR && FMath::Abs(AD) < 0.15f)
					{
						const float Taper = 1.f - (Dist - 0.12f) / (PointR - 0.12f);
						if (FMath::Abs(AD) < Taper * 0.15f)
							Val = FMath::Max(Val, 0.6f);
					}
				}

				// Concentric accent rings
				for (int32 R = 1; R <= 5; ++R)
				{
					const float Radius = R * 0.09f;
					const float RDist = FMath::Abs(Dist - Radius);
					Val = FMath::Max(Val, FMath::Exp(-RDist * RDist / 0.00006f) * 0.45f);
				}

				// Center
				Val = FMath::Max(Val, FMath::Exp(-Dist * Dist / 0.0003f) * 0.9f);

				Val = FMath::Clamp(Val, 0.f, 1.f);
				float PalT = FMath::Frac(Dist * 4.f + Angle / (PI * 2.f) * 0.3f);
				Col = SamplePalette(PalT, Val * 0.7f);
			}
			else
			{
				// --- MANDALA 2: TRIBAL MAZE RINGS ---
				// Concentric bands filled with step-fret / maze patterns
				float Val = 0.f;
				const int32 NBands = 6;
				const float BandW = MaxR / NBands;

				for (int32 B = 0; B < NBands; ++B)
				{
					const float InnerR = B * BandW;
					const float OuterR = (B + 1) * BandW;
					if (Dist < InnerR || Dist > OuterR) continue;

					const float BandPos = (Dist - InnerR) / BandW; // 0-1 within band

					// Band border rings
					const float InDist = FMath::Abs(Dist - InnerR);
					const float OutDist = FMath::Abs(Dist - OuterR);
					Val = FMath::Max(Val, FMath::Exp(-InDist * InDist / 0.00008f) * 0.8f);
					Val = FMath::Max(Val, FMath::Exp(-OutDist * OutDist / 0.00008f) * 0.8f);

					// Divide band into angular cells
					const int32 NCells = 8 + B * 6;
					const float CellAngle = PI * 2.f / NCells;
					const float AngleInCell = FMath::Fmod(Angle + PI * 4.f, CellAngle) / CellAngle; // 0-1
					const int32 CellIdx = FMath::FloorToInt32(FMath::Fmod(Angle + PI * 4.f, PI * 2.f) / CellAngle);
					const float CSeed = FMath::Frac(FMath::Sin(B * 127.1f + CellIdx * 311.7f) * 43758.5f);

					// Cell divider spokes
					const float SpokeDist = FMath::Min(AngleInCell, 1.f - AngleInCell) * CellAngle * Dist;
					Val = FMath::Max(Val, FMath::Exp(-SpokeDist * SpokeDist / 0.00015f) * 0.6f);

					// Step-fret pattern within each cell
					const int32 StepType = FMath::FloorToInt32(CSeed * 3.f);
					if (StepType == 0)
					{
						// Zigzag across band
						const float ZigX = 0.5f + FMath::Sin(BandPos * PI * 3.f) * 0.3f;
						const float ZigDist = FMath::Abs(AngleInCell - ZigX);
						Val = FMath::Max(Val, FMath::Exp(-ZigDist * ZigDist / 0.003f) * 0.75f);
					}
					else if (StepType == 1)
					{
						// Concentric arcs (inner crescents)
						for (int32 A = 0; A < 3; ++A)
						{
							const float ArcR = InnerR + (A + 1) * BandW * 0.25f;
							const float ARDist = FMath::Abs(Dist - ArcR);
							if (AngleInCell > 0.15f && AngleInCell < 0.85f)
								Val = FMath::Max(Val, FMath::Exp(-ARDist * ARDist / 0.00006f) * 0.6f);
						}
					}
					else
					{
						// Diamond lattice
						const float DU = FMath::Frac(AngleInCell * 3.f);
						const float DV = FMath::Frac(BandPos * 3.f);
						const float DiamondDist = FMath::Abs(DU - 0.5f) + FMath::Abs(DV - 0.5f);
						Val = FMath::Max(Val, FMath::Exp(-FMath::Pow(DiamondDist - 0.35f, 2.f) / 0.003f) * 0.7f);
					}
				}

				// Central ornament — small starburst
				const int32 NRays = 8;
				for (int32 R = 0; R < NRays; ++R)
				{
					const float RA = R * PI * 2.f / NRays;
					float AD = FMath::Fmod(Angle - RA + PI * 4.f, PI * 2.f) - PI;
					if (Dist < 0.06f)
						Val = FMath::Max(Val, FMath::Exp(-AD * AD / 0.002f) * 0.8f *
							FMath::Clamp(Dist / 0.01f, 0.f, 1.f));
				}
				Val = FMath::Max(Val, FMath::Exp(-Dist * Dist / 0.0002f) * 0.9f);

				Val = FMath::Clamp(Val, 0.f, 1.f);
				float PalT = FMath::Frac(Angle / (PI * 2.f) * 0.5f + Dist * 2.5f);
				Col = SamplePalette(PalT, Val * 0.7f);
			}

			// Cut out center — clear everything inside the smallest ring
			const float CenterCutoff = 0.18f;
			if (Dist < CenterCutoff)
				Col.A = 0.f;

			const int32 Idx = (Y * Size + X) * 4;
			Data[Idx + 0] = static_cast<uint8>(FMath::Clamp(Col.B * 255.f, 0.f, 255.f)); // B
			Data[Idx + 1] = static_cast<uint8>(FMath::Clamp(Col.G * 255.f, 0.f, 255.f)); // G
			Data[Idx + 2] = static_cast<uint8>(FMath::Clamp(Col.R * 255.f, 0.f, 255.f)); // R
			Data[Idx + 3] = static_cast<uint8>(FMath::Clamp(Col.A * 255.f, 0.f, 255.f)); // A
		}
	}

	Mip.BulkData.Unlock();
	Tex->UpdateResource();
	return Tex;
}

void UPHCCityHUD::CreateTripOverlays()
{
	if (bTripOverlaysCreated) return;
	bTripOverlaysCreated = true;

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas) return;

	// Generate 3 mandala textures at 1024x1024 for detail
	TripTex0 = GenerateTripTexture(0, 1024);
	TripTex1 = GenerateTripTexture(1, 1024);
	TripTex2 = GenerateTripTexture(2, 1024);

	// Single centered image per layer — no tiling. Sized large enough to cover
	// screen even when rotated (diagonal coverage).
	auto MakeOverlay = [&](UTexture2D* Tex, const TCHAR* Name) -> UImage*
	{
		UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), Name);
		if (!Img || !Tex) return nullptr;

		FSlateBrush Brush;
		Brush.SetResourceObject(Tex);
		Brush.ImageSize = FVector2D(1024.f, 1024.f);
		Brush.Tiling = ESlateBrushTileType::NoTile;
		Brush.DrawAs = ESlateBrushDrawType::Image;
		Img->SetBrush(Brush);
		Img->SetColorAndOpacity(FLinearColor(1, 1, 1, 0));
		Img->SetVisibility(ESlateVisibility::HitTestInvisible);
		// Pivot at center so rotation spins in place
		Img->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));

		UCanvasPanelSlot* Slot = RootCanvas->AddChildToCanvas(Img);
		// Center-anchored, oversized so corners are covered during rotation
		// Screen diagonal ~2x height, so ±800 overflow on each side is plenty
		Slot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		Slot->SetAlignment(FVector2D(0.5f, 0.5f));
		Slot->SetOffsets(FMargin(0.f, 0.f, 2400.f, 2400.f));

		return Img;
	};

	TripOverlay0 = MakeOverlay(TripTex0, TEXT("TripOverlay0"));
	TripOverlay1 = MakeOverlay(TripTex1, TEXT("TripOverlay1"));
	TripOverlay2 = MakeOverlay(TripTex2, TEXT("TripOverlay2"));
}
