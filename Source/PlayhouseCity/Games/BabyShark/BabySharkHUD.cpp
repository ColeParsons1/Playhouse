// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/BabyShark/BabySharkHUD.h"
#include "Engine/World.h"
#include "Games/BabyShark/BabySharkGameMode.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetLayoutLibrary.h"

#include "TimerManager.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogBabySharkHUD, Log, All);

static const FLinearColor SharkBgColor(0.02f, 0.08f, 0.18f, 1.f);
static const FLinearColor SharkColor(0.3f, 0.6f, 0.95f, 1.f);
static const FLinearColor ScoreColor(1.f, 1.f, 1.f, 1.f);
static const FLinearColor TapTextColor(1.f, 0.9f, 0.3f, 1.f);
static const FLinearColor BestScoreColor(0.7f, 0.7f, 0.8f, 1.f);

// ---------------------------------------------------------------------------
// Initialize - build the Baby Shark (Flappy Bird) UI programmatically
// ---------------------------------------------------------------------------

bool UBabySharkHUD::Initialize()
{
	if (!Super::Initialize())
	{
		return false;
	}

	if (GameCanvas)
	{
		return true; // Already built.
	}

	UCanvasPanel* RootCanvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!RootCanvas)
	{
		return true;
	}

	const FSlateFontInfo ScoreFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 48);
	const FSlateFontInfo TapFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 24);
	const FSlateFontInfo BestFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);

	// ---- Game content area (below top bar) ----
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GameArea"));
	GameArea->SetBrushColor(SharkBgColor);
	GameArea->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f));

	// ---- Game canvas for all dynamic items (shark, pipes) ----
	GameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("GameCanvas"));
	GameArea->SetContent(GameCanvas);

	// ---- Shark image ----
	SharkImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("SharkImage"));
	SharkImage->SetColorAndOpacity(SharkColor);
	SharkImage->SetDesiredSizeOverride(FVector2D(30.f, 30.f));

	UCanvasPanelSlot* SharkSlot = GameCanvas->AddChildToCanvas(SharkImage);
	SharkSlot->SetAutoSize(true);
	SharkSlot->SetPosition(FVector2D(150.f, 400.f));

	// ---- Score display (large, top-center) ----
	ScoreDisplay = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ScoreDisplay"));
	ScoreDisplay->SetText(FText::FromString(TEXT("0")));
	ScoreDisplay->SetFont(ScoreFont);
	ScoreDisplay->SetColorAndOpacity(FSlateColor(ScoreColor));
	ScoreDisplay->SetJustification(ETextJustify::Center);
	ScoreDisplay->SetShadowOffset(FVector2D(2.f, 2.f));
	ScoreDisplay->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));

	UCanvasPanelSlot* ScoreSlot = GameCanvas->AddChildToCanvas(ScoreDisplay);
	ScoreSlot->SetAutoSize(true);
	ScoreSlot->SetAnchors(FAnchors(0.5f, 0.08f));
	ScoreSlot->SetAlignment(FVector2D(0.5f, 0.5f));

	// ---- Tap to Start text (center of screen) ----
	TapToStartText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("TapToStartText"));
	TapToStartText->SetText(FText::FromString(TEXT("Tap to Start")));
	TapToStartText->SetFont(TapFont);
	TapToStartText->SetColorAndOpacity(FSlateColor(TapTextColor));
	TapToStartText->SetJustification(ETextJustify::Center);

	UCanvasPanelSlot* TapSlot = GameCanvas->AddChildToCanvas(TapToStartText);
	TapSlot->SetAutoSize(true);
	TapSlot->SetAnchors(FAnchors(0.5f, 0.45f));
	TapSlot->SetAlignment(FVector2D(0.5f, 0.5f));

	// ---- Best score text (below tap text) ----
	BestScoreText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("BestScoreText"));
	BestScoreText->SetText(FText::FromString(TEXT("Best: 0")));
	BestScoreText->SetFont(BestFont);
	BestScoreText->SetColorAndOpacity(FSlateColor(BestScoreColor));
	BestScoreText->SetJustification(ETextJustify::Center);

	UCanvasPanelSlot* BestSlot = GameCanvas->AddChildToCanvas(BestScoreText);
	BestSlot->SetAutoSize(true);
	BestSlot->SetAnchors(FAnchors(0.5f, 0.55f));
	BestSlot->SetAlignment(FVector2D(0.5f, 0.5f));

	UE_LOG(LogBabySharkHUD, Log, TEXT("BabyShark HUD built programmatically: GameCanvas, SharkImage, ScoreDisplay, TapToStartText, BestScoreText"));

	return true;
}

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

void UBabySharkHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Cache the game mode from the base-class reference
	// (set by the PlayerController before NativeConstruct runs).
	SharkMode = Cast<ABabySharkGameMode>(GameModeRef);

	// Show the start prompt.
	ShowGameStart();

	// Ensure the widget captures input.
	SetIsFocusable(true);

	LastKnownPipeCount = 0;
	PipeVisuals.Empty();
}

void UBabySharkHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!SharkMode || !GameCanvas)
	{
		return;
	}

	// Update shark visual.
	UpdateSharkVisual(SharkMode->SharkY, SharkMode->SharkVelocity);

	// Update score display.
	ShowScore(SharkMode->GetScore());

	// Update best score.
	if (BestScoreText)
	{
		BestScoreText->SetText(FText::FromString(
			FString::Printf(TEXT("Best: %d"), SharkMode->BestScore)));
	}

	// Synchronize pipe visuals with the game mode's pipe array.
	// Handle newly spawned pipes.
	const int32 CurrentPipeCount = SharkMode->Pipes.Num();

	// If pipes were removed from the front (scrolled off left), remove visuals.
	while (PipeVisuals.Num() > CurrentPipeCount)
	{
		RemovePipeVisual(0);
	}

	// If new pipes were added at the end, spawn visuals.
	while (PipeVisuals.Num() < CurrentPipeCount)
	{
		const int32 NewIdx = PipeVisuals.Num();
		const FPHCPipeObstacle& Pipe = SharkMode->Pipes[NewIdx];
		SpawnPipeVisual(Pipe.XPosition, Pipe.GapCenterY, Pipe.GapHeight);
	}

	// Update positions of all existing pipe visuals.
	for (int32 i = 0; i < PipeVisuals.Num() && i < SharkMode->Pipes.Num(); ++i)
	{
		const FPHCPipeObstacle& Pipe = SharkMode->Pipes[i];
		const float HalfPipeW = SharkMode->PipeWidth * 0.5f;
		const float HalfGap = Pipe.GapHeight * 0.5f;
		const float GapTop = Pipe.GapCenterY - HalfGap;
		const float GapBottom = Pipe.GapCenterY + HalfGap;

		// Top pipe: from Y=0 to GapTop.
		if (PipeVisuals[i].TopImage)
		{
			if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(PipeVisuals[i].TopImage->Slot))
			{
				const FVector2D TopPos = FieldToCanvas(FVector2D(Pipe.XPosition - HalfPipeW, 0.f));
				const FVector2D TopSize = FieldToCanvas(FVector2D(SharkMode->PipeWidth, GapTop)) - FieldToCanvas(FVector2D(0.f, 0.f));
				Slot->SetPosition(TopPos);
				Slot->SetSize(FVector2D(FMath::Max(TopSize.X, 1.f), FMath::Max(TopSize.Y, 0.f)));
			}
		}

		// Bottom pipe: from GapBottom to FieldHeight.
		if (PipeVisuals[i].BottomImage)
		{
			if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(PipeVisuals[i].BottomImage->Slot))
			{
				const FVector2D BotPos = FieldToCanvas(FVector2D(Pipe.XPosition - HalfPipeW, GapBottom));
				const float BotHeight = SharkMode->FieldHeight - GapBottom;
				const FVector2D BotSize = FieldToCanvas(FVector2D(SharkMode->PipeWidth, BotHeight)) - FieldToCanvas(FVector2D(0.f, 0.f));
				Slot->SetPosition(BotPos);
				Slot->SetSize(FVector2D(FMath::Max(BotSize.X, 1.f), FMath::Max(BotSize.Y, 0.f)));
			}
		}
	}

	// Hide/show start text.
	if (TapToStartText)
	{
		TapToStartText->SetVisibility(
			SharkMode->bGameStarted
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible
		);
	}

	LastKnownPipeCount = CurrentPipeCount;
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

FReply UBabySharkHUD::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	OnTap();
	return FReply::Handled();
}

FReply UBabySharkHUD::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	OnTap();
	return FReply::Handled();
}

void UBabySharkHUD::OnTap()
{
	if (!SharkMode)
	{
		return;
	}

	// If the game is over, don't process taps (the game-over overlay handles restart).
	if (!SharkMode->bGameActive)
	{
		return;
	}

	SharkMode->Flap();
}

// ---------------------------------------------------------------------------
// Visual updates
// ---------------------------------------------------------------------------

void UBabySharkHUD::UpdateSharkVisual(float Y, float Velocity)
{
	if (!SharkImage || !SharkMode)
	{
		return;
	}

	const float HalfShark = SharkMode->SharkSize * 0.5f;
	const FVector2D SharkFieldPos(SharkMode->SharkX, Y);
	const FVector2D CanvasPos = FieldToCanvas(SharkFieldPos);

	if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(SharkImage->Slot))
	{
		Slot->SetPosition(CanvasPos - FVector2D(HalfShark, HalfShark));
	}

	// Tilt the shark based on velocity.
	// Positive velocity = moving up = nose tilted up (negative angle in UMG).
	// Negative velocity = falling = nose tilted down.
	const float MaxTiltDeg = 45.f;
	const float NormalizedVel = FMath::Clamp(Velocity / SharkMode->FlapForce, -1.5f, 1.5f);
	const float TiltAngle = -NormalizedVel * MaxTiltDeg;

	SharkImage->SetRenderTransformAngle(TiltAngle);
}

void UBabySharkHUD::SpawnPipeVisual(float X, float GapCenter, float GapSize)
{
	if (!GameCanvas)
	{
		return;
	}

	FPHCPipeVisual Visual;

	// Coral/seaweed green color for pipes.
	const FLinearColor PipeColor(0.15f, 0.55f, 0.3f, 1.f);

	// -- Top pipe image --
	Visual.TopImage = NewObject<UImage>(this);
	if (Visual.TopImage)
	{
		Visual.TopImage->SetColorAndOpacity(PipeColor);

		UCanvasPanelSlot* TopSlot = GameCanvas->AddChildToCanvas(Visual.TopImage);
		if (TopSlot)
		{
			TopSlot->SetAutoSize(false);
			// Position and size will be set in NativeTick.
			TopSlot->SetPosition(FVector2D::ZeroVector);
			TopSlot->SetSize(FVector2D(60.f, 1.f));
		}
	}

	// -- Bottom pipe image --
	Visual.BottomImage = NewObject<UImage>(this);
	if (Visual.BottomImage)
	{
		Visual.BottomImage->SetColorAndOpacity(PipeColor);

		UCanvasPanelSlot* BotSlot = GameCanvas->AddChildToCanvas(Visual.BottomImage);
		if (BotSlot)
		{
			BotSlot->SetAutoSize(false);
			BotSlot->SetPosition(FVector2D::ZeroVector);
			BotSlot->SetSize(FVector2D(60.f, 1.f));
		}
	}

	PipeVisuals.Add(Visual);
}

void UBabySharkHUD::RemovePipeVisual(int32 Index)
{
	if (!PipeVisuals.IsValidIndex(Index))
	{
		return;
	}

	if (PipeVisuals[Index].TopImage)
	{
		PipeVisuals[Index].TopImage->RemoveFromParent();
	}
	if (PipeVisuals[Index].BottomImage)
	{
		PipeVisuals[Index].BottomImage->RemoveFromParent();
	}

	PipeVisuals.RemoveAt(Index);
}

void UBabySharkHUD::ShowGameStart()
{
	if (TapToStartText)
	{
		TapToStartText->SetText(FText::FromString(TEXT("Tap to Start")));
		TapToStartText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}

	if (BestScoreText && SharkMode)
	{
		BestScoreText->SetText(FText::FromString(
			FString::Printf(TEXT("Best: %d"), SharkMode->BestScore)));
		BestScoreText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void UBabySharkHUD::ShowScore(int32 InScore)
{
	if (ScoreDisplay)
	{
		ScoreDisplay->SetText(FText::AsNumber(InScore));
	}
}

void UBabySharkHUD::AnimateDeath()
{
	if (!GameCanvas || !SharkImage)
	{
		return;
	}

	// Flash the screen red briefly.
	UImage* FlashOverlay = NewObject<UImage>(this);
	if (FlashOverlay)
	{
		FlashOverlay->SetColorAndOpacity(FLinearColor(1.f, 0.f, 0.f, 0.4f));

		UCanvasPanelSlot* Slot = GameCanvas->AddChildToCanvas(FlashOverlay);
		if (Slot)
		{
			Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
			Slot->SetOffsets(FMargin(0.f));
		}

		// Fade out the flash after a short delay.
		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<UImage> WeakFlash(FlashOverlay);
			World->GetTimerManager().SetTimer(
				DeathFlashHandle,
				[WeakFlash]()
				{
					if (WeakFlash.IsValid())
					{
						WeakFlash->RemoveFromParent();
					}
				},
				0.5f,
				/*bLoop=*/ false
			);
		}
	}

	// Let the shark fall off screen by not stopping the tick -- the game mode
	// has already set bGameActive = false, so the shark will no longer be
	// updated. We give it a final downward push visually.
	if (SharkMode)
	{
		SharkMode->SharkVelocity = -500.f; // Fast downward
	}
}

// ---------------------------------------------------------------------------
// Coordinate conversion
// ---------------------------------------------------------------------------

FVector2D UBabySharkHUD::FieldToCanvas(FVector2D FieldPos) const
{
	if (!GameCanvas || !SharkMode)
	{
		return FieldPos;
	}

	const FGeometry& CanvasGeometry = GameCanvas->GetCachedGeometry();
	const FVector2D CanvasSize = CanvasGeometry.GetLocalSize();

	if (CanvasSize.X <= 0.f || CanvasSize.Y <= 0.f)
	{
		return FieldPos;
	}

	const float ScaleX = CanvasSize.X / SharkMode->FieldWidth;
	const float ScaleY = CanvasSize.Y / SharkMode->FieldHeight;

	return FVector2D(FieldPos.X * ScaleX, FieldPos.Y * ScaleY);
}
