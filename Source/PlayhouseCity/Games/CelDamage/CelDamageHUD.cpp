// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/CelDamage/CelDamageHUD.h"
#include "Games/CelDamage/CelDamageGameMode.h"
#include "Games/CelDamage/CelDamageVehicle.h"
#include "Games/CelDamage/CelDamageWeapon.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogCelDamageHUD, Log, All);

// -- Colour palette ---------------------------------------------------------

static const FLinearColor CDBlue(0.15f, 0.45f, 0.85f, 1.f);
static const FLinearColor CDRed(0.85f, 0.20f, 0.15f, 1.f);
static const FLinearColor HPGreen(0.2f, 0.85f, 0.2f, 1.f);
static const FLinearColor HPRedFill(0.85f, 0.2f, 0.2f, 1.f);
static const FLinearColor FireOrange(0.9f, 0.35f, 0.1f, 1.f);
static const FLinearColor FireHover(0.95f, 0.45f, 0.15f, 1.f);
static const FLinearColor FirePress(0.7f, 0.25f, 0.08f, 1.f);
static const FLinearColor DarkBg(0.05f, 0.05f, 0.1f, 0.7f);

// ---------------------------------------------------------------------------
// Initialize - build the Cel Damage HUD
// ---------------------------------------------------------------------------

bool UCelDamageHUD::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (PlayerHPBar)
	{
		return true; // Already built
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo ScoreFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);
	const FSlateFontInfo LabelFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 13);
	const FSlateFontInfo WeaponFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);
	const FSlateFontInfo FireFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);

	// ===================================================================
	// SCORE DISPLAY (top center)
	// ===================================================================
	UBorder* ScoreBg = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ScoreBg"));
	ScoreBg->SetBrushColor(DarkBg);
	ScoreBg->SetPadding(FMargin(12.f, 4.f));

	UCanvasPanelSlot* ScoreSlot = RootCanvas->AddChildToCanvas(ScoreBg);
	ScoreSlot->SetAnchors(FAnchors(0.5f, 0.f, 0.5f, 0.f));
	ScoreSlot->SetOffsets(FMargin(-120.f, 52.f, 240.f, 40.f));

	UHorizontalBox* ScoreHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("ScoreHBox"));
	ScoreBg->SetContent(ScoreHBox);

	// Player label
	UTextBlock* PlayerLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PlayerLabel"));
	PlayerLabel->SetText(FText::FromString(TEXT("YOU: ")));
	PlayerLabel->SetFont(ScoreFont);
	PlayerLabel->SetColorAndOpacity(FSlateColor(CDBlue));
	UHorizontalBoxSlot* PLSlot = ScoreHBox->AddChildToHorizontalBox(PlayerLabel);
	PLSlot->SetVerticalAlignment(VAlign_Center);

	// Player smacks
	PlayerSmacksText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PlayerSmacksText"));
	PlayerSmacksText->SetText(FText::FromString(TEXT("0")));
	PlayerSmacksText->SetFont(ScoreFont);
	PlayerSmacksText->SetColorAndOpacity(FSlateColor(CDBlue));
	UHorizontalBoxSlot* PSSlot = ScoreHBox->AddChildToHorizontalBox(PlayerSmacksText);
	PSSlot->SetVerticalAlignment(VAlign_Center);
	PSSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

	// Separator
	UTextBlock* VsText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("VsText"));
	VsText->SetText(FText::FromString(TEXT(" - ")));
	VsText->SetFont(ScoreFont);
	VsText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UHorizontalBoxSlot* VsSlot = ScoreHBox->AddChildToHorizontalBox(VsText);
	VsSlot->SetVerticalAlignment(VAlign_Center);
	VsSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

	// AI smacks
	AISmacksText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AISmacksText"));
	AISmacksText->SetText(FText::FromString(TEXT("0")));
	AISmacksText->SetFont(ScoreFont);
	AISmacksText->SetColorAndOpacity(FSlateColor(CDRed));
	UHorizontalBoxSlot* ASSlot = ScoreHBox->AddChildToHorizontalBox(AISmacksText);
	ASSlot->SetVerticalAlignment(VAlign_Center);

	// AI label
	UTextBlock* AILabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AILabel"));
	AILabel->SetText(FText::FromString(TEXT(" :CPU")));
	AILabel->SetFont(ScoreFont);
	AILabel->SetColorAndOpacity(FSlateColor(CDRed));
	UHorizontalBoxSlot* ALSlot = ScoreHBox->AddChildToHorizontalBox(AILabel);
	ALSlot->SetVerticalAlignment(VAlign_Center);

	// ===================================================================
	// PLAYER HP BAR (top left)
	// ===================================================================
	UTextBlock* PlayerHPLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PlayerHPLabel"));
	PlayerHPLabel->SetText(FText::FromString(TEXT("YOUR HP")));
	PlayerHPLabel->SetFont(LabelFont);
	PlayerHPLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	UCanvasPanelSlot* PHPLblSlot = RootCanvas->AddChildToCanvas(PlayerHPLabel);
	PHPLblSlot->SetAnchors(FAnchors(0.02f, 0.f, 0.02f, 0.f));
	PHPLblSlot->SetOffsets(FMargin(0.f, 54.f, 100.f, 18.f));

	PlayerHPBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("PlayerHPBar"));
	PlayerHPBar->SetPercent(1.f);
	PlayerHPBar->SetFillColorAndOpacity(HPGreen);

	UCanvasPanelSlot* PHPSlot = RootCanvas->AddChildToCanvas(PlayerHPBar);
	PHPSlot->SetAnchors(FAnchors(0.02f, 0.f, 0.02f, 0.f));
	PHPSlot->SetOffsets(FMargin(0.f, 74.f, 130.f, 14.f));

	// ===================================================================
	// ENEMY HP BAR (top right)
	// ===================================================================
	UTextBlock* EnemyHPLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("EnemyHPLabel"));
	EnemyHPLabel->SetText(FText::FromString(TEXT("ENEMY HP")));
	EnemyHPLabel->SetFont(LabelFont);
	EnemyHPLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.6f, 0.6f)));

	UCanvasPanelSlot* EHPLblSlot = RootCanvas->AddChildToCanvas(EnemyHPLabel);
	EHPLblSlot->SetAnchors(FAnchors(0.85f, 0.f, 0.85f, 0.f));
	EHPLblSlot->SetOffsets(FMargin(0.f, 54.f, 100.f, 18.f));

	EnemyHPBar = WidgetTree->ConstructWidget<UProgressBar>(
		UProgressBar::StaticClass(), TEXT("EnemyHPBar"));
	EnemyHPBar->SetPercent(1.f);
	EnemyHPBar->SetFillColorAndOpacity(HPRedFill);

	UCanvasPanelSlot* EHPSlot = RootCanvas->AddChildToCanvas(EnemyHPBar);
	EHPSlot->SetAnchors(FAnchors(0.85f, 0.f, 0.85f, 0.f));
	EHPSlot->SetOffsets(FMargin(0.f, 74.f, 130.f, 14.f));

	// ===================================================================
	// WEAPON INDICATOR (bottom center)
	// ===================================================================
	WeaponIndicator = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("WeaponIndicator"));
	WeaponIndicator->SetBrushColor(FLinearColor(0.5f, 0.5f, 0.5f, 1.f));

	UCanvasPanelSlot* WISlot = RootCanvas->AddChildToCanvas(WeaponIndicator);
	WISlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
	WISlot->SetOffsets(FMargin(-30.f, -90.f, 60.f, 60.f));

	WeaponNameText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("WeaponNameText"));
	WeaponNameText->SetText(FText::FromString(TEXT("Fists")));
	WeaponNameText->SetFont(WeaponFont);
	WeaponNameText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	WeaponNameText->SetJustification(ETextJustify::Center);

	UCanvasPanelSlot* WNSlot = RootCanvas->AddChildToCanvas(WeaponNameText);
	WNSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
	WNSlot->SetOffsets(FMargin(-50.f, -26.f, 100.f, 22.f));

	// ===================================================================
	// FIRE BUTTON (bottom right)
	// ===================================================================
	FireButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("FireButton"));
	FButtonStyle FireStyle = FireButton->GetStyle();
	FireStyle.Normal.TintColor = FSlateColor(FireOrange);
	FireStyle.Hovered.TintColor = FSlateColor(FireHover);
	FireStyle.Pressed.TintColor = FSlateColor(FirePress);
	FireButton->SetStyle(FireStyle);

	UTextBlock* FireLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("FireLabel"));
	FireLabel->SetText(FText::FromString(TEXT("FIRE")));
	FireLabel->SetFont(FireFont);
	FireLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FireLabel->SetJustification(ETextJustify::Center);
	FireButton->AddChild(FireLabel);

	USizeBox* FireSizeBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("FireSizeBox"));
	FireSizeBox->SetMinDesiredWidth(90.f);
	FireSizeBox->SetMinDesiredHeight(55.f);
	FireSizeBox->AddChild(FireButton);

	UCanvasPanelSlot* FireSlot = RootCanvas->AddChildToCanvas(FireSizeBox);
	FireSlot->SetAnchors(FAnchors(0.85f, 1.f, 0.85f, 1.f));
	FireSlot->SetOffsets(FMargin(0.f, -80.f, 90.f, 55.f));

	UE_LOG(LogCelDamageHUD, Log,
		TEXT("Cel Damage HUD built: score display, HP bars, weapon indicator, fire button."));

	return true;
}

// ---------------------------------------------------------------------------
// NativeConstruct
// ---------------------------------------------------------------------------

void UCelDamageHUD::NativeConstruct()
{
	Super::NativeConstruct();

	CelDamageMode = Cast<ACelDamageGameMode>(GameModeRef);

	if (FireButton)
	{
		FireButton->OnClicked.AddDynamic(this, &UCelDamageHUD::OnFireClicked);
	}

	SetIsFocusable(true);
}

// ---------------------------------------------------------------------------
// NativeTick
// ---------------------------------------------------------------------------

void UCelDamageHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CelDamageMode)
	{
		return;
	}

	// Update HP bars
	if (PlayerHPBar && CelDamageMode->PlayerVehicle)
	{
		const float Frac = CelDamageMode->PlayerVehicle->Health
			/ CelDamageMode->PlayerVehicle->MaxHealth;
		PlayerHPBar->SetPercent(FMath::Clamp(Frac, 0.f, 1.f));
	}
	if (EnemyHPBar && CelDamageMode->AIVehicle)
	{
		const float Frac = CelDamageMode->AIVehicle->Health
			/ CelDamageMode->AIVehicle->MaxHealth;
		EnemyHPBar->SetPercent(FMath::Clamp(Frac, 0.f, 1.f));
	}

	// Update smack scores
	if (PlayerSmacksText)
	{
		PlayerSmacksText->SetText(FText::FromString(
			FString::Printf(TEXT("%d"), CelDamageMode->PlayerSmacks)));
	}
	if (AISmacksText)
	{
		AISmacksText->SetText(FText::FromString(
			FString::Printf(TEXT("%d"), CelDamageMode->AISmacks)));
	}

	// Update weapon name and indicator
	if (CelDamageMode->PlayerVehicle)
	{
		const ECelDamageWeaponType Wpn = CelDamageMode->PlayerVehicle->CurrentWeapon;

		if (WeaponNameText)
		{
			const UEnum* WeaponEnum = StaticEnum<ECelDamageWeaponType>();
			if (WeaponEnum)
			{
				const FString Name = WeaponEnum->GetDisplayNameTextByValue(
					static_cast<int64>(Wpn)).ToString();
				WeaponNameText->SetText(FText::FromString(Name));
			}
		}

		if (WeaponIndicator)
		{
			WeaponIndicator->SetBrushColor(ACelDamageWeapon::GetColorForWeapon(Wpn));
		}
	}

	// Process virtual joystick input
	if (bJoystickActive && CelDamageMode->bGameActive && !CelDamageMode->bGamePaused)
	{
		const FVector2D Delta = JoystickCurrent - JoystickOrigin;
		const float Magnitude = Delta.Size();

		if (Magnitude > JoystickDeadzone)
		{
			const float ClampedMag = FMath::Min(Magnitude, JoystickMaxRadius);
			const FVector2D NormDelta = Delta / ClampedMag;

			CelDamageMode->PlayerThrottle(-NormDelta.Y); // Up = forward (Y inverted)
			CelDamageMode->PlayerSteer(NormDelta.X);
		}
		else
		{
			CelDamageMode->PlayerThrottle(0.f);
			CelDamageMode->PlayerSteer(0.f);
		}
	}
	else if (CelDamageMode->bGameActive && !CelDamageMode->bGamePaused)
	{
		// No joystick active, zero controls
		CelDamageMode->PlayerThrottle(0.f);
		CelDamageMode->PlayerSteer(0.f);
	}
}

// ---------------------------------------------------------------------------
// Touch input - virtual joystick
// ---------------------------------------------------------------------------

FReply UCelDamageHUD::NativeOnTouchStarted(const FGeometry& InGeometry,
	const FPointerEvent& InTouchEvent)
{
	const FVector2D ScreenPos = InTouchEvent.GetScreenSpacePosition();
	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(ScreenPos);
	const FVector2D Size = InGeometry.GetLocalSize();

	if (Size.X <= 0.f)
	{
		return FReply::Handled();
	}

	const float NormX = LocalPos.X / Size.X;

	if (NormX < 0.5f)
	{
		// Left half: start joystick
		bJoystickActive = true;
		JoystickOrigin = ScreenPos;
		JoystickCurrent = ScreenPos;
		JoystickFingerIndex = InTouchEvent.GetPointerIndex();
	}
	else
	{
		// Right half: fire
		if (CelDamageMode && CelDamageMode->bGameActive && !CelDamageMode->bGamePaused)
		{
			CelDamageMode->PlayerFire();
		}
	}

	return FReply::Handled();
}

FReply UCelDamageHUD::NativeOnTouchMoved(const FGeometry& InGeometry,
	const FPointerEvent& InTouchEvent)
{
	if (bJoystickActive && InTouchEvent.GetPointerIndex() == JoystickFingerIndex)
	{
		JoystickCurrent = InTouchEvent.GetScreenSpacePosition();
	}
	return FReply::Handled();
}

FReply UCelDamageHUD::NativeOnTouchEnded(const FGeometry& InGeometry,
	const FPointerEvent& InTouchEvent)
{
	if (bJoystickActive && InTouchEvent.GetPointerIndex() == JoystickFingerIndex)
	{
		bJoystickActive = false;
		JoystickFingerIndex = -1;

		if (CelDamageMode)
		{
			CelDamageMode->PlayerThrottle(0.f);
			CelDamageMode->PlayerSteer(0.f);
		}
	}
	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Mouse input fallback (for editor/desktop testing)
// ---------------------------------------------------------------------------

FReply UCelDamageHUD::NativeOnMouseButtonDown(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	const FVector2D ScreenPos = InMouseEvent.GetScreenSpacePosition();
	const FVector2D LocalPos = InGeometry.AbsoluteToLocal(ScreenPos);
	const FVector2D Size = InGeometry.GetLocalSize();

	if (Size.X <= 0.f)
	{
		return FReply::Handled();
	}

	const float NormX = LocalPos.X / Size.X;

	if (NormX < 0.5f)
	{
		bJoystickActive = true;
		JoystickOrigin = ScreenPos;
		JoystickCurrent = ScreenPos;
	}
	else
	{
		if (CelDamageMode && CelDamageMode->bGameActive && !CelDamageMode->bGamePaused)
		{
			CelDamageMode->PlayerFire();
		}
	}

	return FReply::Handled();
}

FReply UCelDamageHUD::NativeOnMouseMove(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bJoystickActive)
	{
		JoystickCurrent = InMouseEvent.GetScreenSpacePosition();
	}
	return FReply::Handled();
}

FReply UCelDamageHUD::NativeOnMouseButtonUp(const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bJoystickActive)
	{
		bJoystickActive = false;

		if (CelDamageMode)
		{
			CelDamageMode->PlayerThrottle(0.f);
			CelDamageMode->PlayerSteer(0.f);
		}
	}
	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Fire button callback
// ---------------------------------------------------------------------------

void UCelDamageHUD::OnFireClicked()
{
	if (CelDamageMode && CelDamageMode->bGameActive && !CelDamageMode->bGamePaused)
	{
		CelDamageMode->PlayerFire();
	}
}
