// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/Base/PHCMiniGameHUD.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "Core/PHCPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPHCMiniGameHUD, Log, All);

// ---------------------------------------------------------------------------
// Initialize - build the widget tree programmatically
// ---------------------------------------------------------------------------

bool UPHCMiniGameHUD::Initialize()
{
	bool bResult = Super::Initialize();
	if (!bResult) return false;

	if (ScoreText)
	{
		return true; // Already built (Blueprint-provided).
	}

	const FLinearColor BarBg(0.05f, 0.05f, 0.1f, 0.9f);
	const FLinearColor OverlayBg(0.02f, 0.02f, 0.06f, 0.92f);
	const FLinearColor BtnColor(0.2f, 0.3f, 0.7f, 1.f);
	const FLinearColor BtnHover(0.3f, 0.4f, 0.85f, 1.f);
	const FLinearColor BtnPress(0.15f, 0.2f, 0.5f, 1.f);
	const FLinearColor DangerColor(0.8f, 0.15f, 0.15f, 1.f);
	const FLinearColor DangerHover(0.9f, 0.25f, 0.25f, 1.f);

	const FSlateFontInfo ScoreFont = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 20);
	const FSlateFontInfo TimerFont = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 18);
	const FSlateFontInfo BtnFont = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);
	const FSlateFontInfo TitleFont = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 28);
	const FSlateFontInfo ResultFont = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 22);

	// Helper: create a styled button
	auto MakeButton = [&](const FName& Name, const FString& Label, FLinearColor NormalCol, FLinearColor HoverCol, FLinearColor PressCol) -> UButton*
	{
		UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor = FSlateColor(NormalCol);
		Style.Hovered.TintColor = FSlateColor(HoverCol);
		Style.Pressed.TintColor = FSlateColor(PressCol);
		Btn->SetStyle(Style);

		UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*(Name.ToString() + TEXT("Lbl"))));
		BtnLabel->SetText(FText::FromString(Label));
		BtnLabel->SetFont(BtnFont);
		BtnLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		BtnLabel->SetJustification(ETextJustify::Center);
		Btn->AddChild(BtnLabel);

		return Btn;
	};

	// ---- Root Canvas (full screen) ----
	UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = RootCanvas;

	// ============================================================
	// TOP BAR (score + timer + pause/quit buttons)
	// ============================================================
	UBorder* TopBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TopBar"));
	TopBar->SetBrushColor(BarBg);
	TopBar->SetPadding(FMargin(16.f, 8.f));

	UCanvasPanelSlot* TopBarSlot = RootCanvas->AddChildToCanvas(TopBar);
	TopBarSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));
	TopBarSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 50.f));

	UHorizontalBox* TopHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopHBox"));
	TopBar->SetContent(TopHBox);

	// Game name label (populated by SetGameMode)
	GameNameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GameNameText"));
	GameNameText->SetText(FText::FromString(TEXT("")));
	GameNameText->SetFont(ScoreFont);
	GameNameText->SetColorAndOpacity(FSlateColor(FLinearColor(0.4f, 0.8f, 1.f)));
	UHorizontalBoxSlot* GameNameSlot = TopHBox->AddChildToHorizontalBox(GameNameText);
	GameNameSlot->SetVerticalAlignment(VAlign_Center);
	GameNameSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

	// Score
	ScoreText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ScoreText"));
	ScoreText->SetText(FText::FromString(TEXT("Score: 0")));
	ScoreText->SetFont(ScoreFont);
	ScoreText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	UHorizontalBoxSlot* ScoreSlot = TopHBox->AddChildToHorizontalBox(ScoreText);
	ScoreSlot->SetVerticalAlignment(VAlign_Center);
	ScoreSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

	// Spacer
	USpacer* TopSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("TopSpacer"));
	UHorizontalBoxSlot* SpacerSlot = TopHBox->AddChildToHorizontalBox(TopSpacer);
	SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// Timer
	TimerText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("TimerText"));
	TimerText->SetText(FText::FromString(TEXT("00:00")));
	TimerText->SetFont(TimerFont);
	TimerText->SetColorAndOpacity(FSlateColor(FLinearColor(0.5f, 0.8f, 1.f)));
	UHorizontalBoxSlot* TimerSlot = TopHBox->AddChildToHorizontalBox(TimerText);
	TimerSlot->SetVerticalAlignment(VAlign_Center);
	TimerSlot->SetPadding(FMargin(16.f, 0.f));

	// Pause button
	PauseButton = MakeButton(TEXT("PauseButton"), TEXT("Pause"), BtnColor, BtnHover, BtnPress);
	UHorizontalBoxSlot* PauseSlot = TopHBox->AddChildToHorizontalBox(PauseButton);
	PauseSlot->SetVerticalAlignment(VAlign_Center);
	PauseSlot->SetPadding(FMargin(4.f, 0.f));
	PauseSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	// Quit button
	QuitButton = MakeButton(TEXT("QuitButton"), TEXT("Quit"), DangerColor, DangerHover, BtnPress);
	UHorizontalBoxSlot* QuitSlot = TopHBox->AddChildToHorizontalBox(QuitButton);
	QuitSlot->SetVerticalAlignment(VAlign_Center);
	QuitSlot->SetPadding(FMargin(4.f, 0.f));
	QuitSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	// ============================================================
	// PAUSE OVERLAY (centered, hidden by default)
	// ============================================================
	PauseOverlay = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PauseOverlay"));

	UCanvasPanelSlot* PauseOvSlot = RootCanvas->AddChildToCanvas(PauseOverlay);
	PauseOvSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	PauseOvSlot->SetOffsets(FMargin(0.f));

	UBorder* PauseBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PauseBg"));
	PauseBg->SetBrushColor(OverlayBg);
	PauseBg->SetPadding(FMargin(40.f));
	UCanvasPanelSlot* PauseBgSlot = PauseOverlay->AddChildToCanvas(PauseBg);
	PauseBgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	PauseBgSlot->SetOffsets(FMargin(0.f));

	UVerticalBox* PauseVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PauseVBox"));
	PauseBg->SetContent(PauseVBox);

	USpacer* PS1 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("PS1"));
	UVerticalBoxSlot* PS1Slot = PauseVBox->AddChildToVerticalBox(PS1);
	PS1Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UTextBlock* PauseTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PauseTitle"));
	PauseTitle->SetText(FText::FromString(TEXT("PAUSED")));
	PauseTitle->SetFont(TitleFont);
	PauseTitle->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	PauseTitle->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* PTSlot = PauseVBox->AddChildToVerticalBox(PauseTitle);
	PTSlot->SetHorizontalAlignment(HAlign_Center);
	PTSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 30.f));

	ResumeButton = MakeButton(TEXT("ResumeButton"), TEXT("Resume"), BtnColor, BtnHover, BtnPress);
	UVerticalBoxSlot* ResSlot = PauseVBox->AddChildToVerticalBox(ResumeButton);
	ResSlot->SetHorizontalAlignment(HAlign_Center);
	ResSlot->SetPadding(FMargin(60.f, 8.f));

	QuitConfirmButton = MakeButton(TEXT("QuitConfirmButton"), TEXT("Quit Game"), DangerColor, DangerHover, BtnPress);
	UVerticalBoxSlot* QCSlot = PauseVBox->AddChildToVerticalBox(QuitConfirmButton);
	QCSlot->SetHorizontalAlignment(HAlign_Center);
	QCSlot->SetPadding(FMargin(60.f, 8.f));

	USpacer* PS2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("PS2"));
	UVerticalBoxSlot* PS2Slot = PauseVBox->AddChildToVerticalBox(PS2);
	PS2Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// ============================================================
	// GAME OVER OVERLAY (centered, hidden by default)
	// ============================================================
	GameOverOverlay = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GameOverOverlay"));

	UCanvasPanelSlot* GOOvSlot = RootCanvas->AddChildToCanvas(GameOverOverlay);
	GOOvSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GOOvSlot->SetOffsets(FMargin(0.f));

	UBorder* GOBg = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GOBg"));
	GOBg->SetBrushColor(OverlayBg);
	GOBg->SetPadding(FMargin(40.f));
	UCanvasPanelSlot* GOBgSlot = GameOverOverlay->AddChildToCanvas(GOBg);
	GOBgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GOBgSlot->SetOffsets(FMargin(0.f));

	UVerticalBox* GOVBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("GOVBox"));
	GOBg->SetContent(GOVBox);

	USpacer* GS1 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("GS1"));
	UVerticalBoxSlot* GS1Slot = GOVBox->AddChildToVerticalBox(GS1);
	GS1Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UTextBlock* GOTitle = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GOTitle"));
	GOTitle->SetText(FText::FromString(TEXT("GAME OVER")));
	GOTitle->SetFont(TitleFont);
	GOTitle->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	GOTitle->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* GOTSlot = GOVBox->AddChildToVerticalBox(GOTitle);
	GOTSlot->SetHorizontalAlignment(HAlign_Center);
	GOTSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 20.f));

	FinalScoreText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("FinalScoreText"));
	FinalScoreText->SetText(FText::FromString(TEXT("Final Score: 0")));
	FinalScoreText->SetFont(ResultFont);
	FinalScoreText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	FinalScoreText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* FSSlot = GOVBox->AddChildToVerticalBox(FinalScoreText);
	FSSlot->SetHorizontalAlignment(HAlign_Center);
	FSSlot->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	CoinsEarnedText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CoinsEarnedText"));
	CoinsEarnedText->SetText(FText::FromString(TEXT("Coins Earned: 0")));
	CoinsEarnedText->SetFont(ResultFont);
	CoinsEarnedText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.85f, 0.f)));
	CoinsEarnedText->SetJustification(ETextJustify::Center);
	UVerticalBoxSlot* CESlot = GOVBox->AddChildToVerticalBox(CoinsEarnedText);
	CESlot->SetHorizontalAlignment(HAlign_Center);
	CESlot->SetPadding(FMargin(0.f, 0.f, 0.f, 30.f));

	PlayAgainButton = MakeButton(TEXT("PlayAgainButton"), TEXT("Play Again"), BtnColor, BtnHover, BtnPress);
	UVerticalBoxSlot* PASlot = GOVBox->AddChildToVerticalBox(PlayAgainButton);
	PASlot->SetHorizontalAlignment(HAlign_Center);
	PASlot->SetPadding(FMargin(60.f, 8.f));

	ReturnButton = MakeButton(TEXT("ReturnButton"), TEXT("Return to Lobby"), DangerColor, DangerHover, BtnPress);
	UVerticalBoxSlot* RBSlot = GOVBox->AddChildToVerticalBox(ReturnButton);
	RBSlot->SetHorizontalAlignment(HAlign_Center);
	RBSlot->SetPadding(FMargin(60.f, 8.f));

	USpacer* GS2 = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("GS2"));
	UVerticalBoxSlot* GS2Slot = GOVBox->AddChildToVerticalBox(GS2);
	GS2Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	UE_LOG(LogPHCMiniGameHUD, Log, TEXT("MiniGameHUD widget tree built programmatically."));

	return true;
}

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

void UPHCMiniGameHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Wire up button delegates.
	if (PauseButton)
	{
		PauseButton->OnClicked.AddDynamic(this, &UPHCMiniGameHUD::OnPauseClicked);
	}
	if (QuitButton)
	{
		QuitButton->OnClicked.AddDynamic(this, &UPHCMiniGameHUD::OnQuitClicked);
	}
	if (ResumeButton)
	{
		ResumeButton->OnClicked.AddDynamic(this, &UPHCMiniGameHUD::OnResumeClicked);
	}
	if (QuitConfirmButton)
	{
		QuitConfirmButton->OnClicked.AddDynamic(this, &UPHCMiniGameHUD::OnQuitConfirmed);
	}
	if (PlayAgainButton)
	{
		PlayAgainButton->OnClicked.AddDynamic(this, &UPHCMiniGameHUD::OnPlayAgainClicked);
	}
	if (ReturnButton)
	{
		ReturnButton->OnClicked.AddDynamic(this, &UPHCMiniGameHUD::OnReturnClicked);
	}

	// Hide overlays by default.
	if (PauseOverlay)
	{
		PauseOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
	if (GameOverOverlay)
	{
		GameOverOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}
}

void UPHCMiniGameHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (GameModeRef && GameModeRef->bGameActive && !GameModeRef->bGamePaused)
	{
		UpdateScore(GameModeRef->GetScore());
		UpdateTimer(GameModeRef->GetTimeRemaining());
	}
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void UPHCMiniGameHUD::SetGameMode(APHCMiniGameMode* InGameMode)
{
	GameModeRef = InGameMode;
	if (GameModeRef)
	{
		GameModeRef->OnGameEnded.AddDynamic(this, &UPHCMiniGameHUD::HandleGameEnded);

		// Show the game name in the top bar.
		if (GameNameText)
		{
			const UEnum* GameEnum = StaticEnum<EPHCGameType>();
			if (GameEnum)
			{
				const FString DisplayName = GameEnum->GetDisplayNameTextByValue(static_cast<int64>(GameModeRef->GameType)).ToString();
				GameNameText->SetText(FText::FromString(DisplayName));
				UE_LOG(LogPHCMiniGameHUD, Log, TEXT("Game name set to: %s (type %d)"), *DisplayName, static_cast<int32>(GameModeRef->GameType));
			}
		}
	}
}

void UPHCMiniGameHUD::UpdateScore(int32 Score)
{
	if (ScoreText)
	{
		ScoreText->SetText(FText::FromString(FString::Printf(TEXT("Score: %d"), Score)));
	}
}

void UPHCMiniGameHUD::UpdateTimer(float SecondsRemaining)
{
	if (TimerText)
	{
		const int32 Minutes = FMath::FloorToInt32(SecondsRemaining / 60.f);
		const int32 Seconds = FMath::FloorToInt32(FMath::Fmod(SecondsRemaining, 60.f));
		TimerText->SetText(FText::FromString(FString::Printf(TEXT("%02d:%02d"), Minutes, Seconds)));
	}
}

void UPHCMiniGameHUD::ShowGameOver(const FPHCGameResult& Result)
{
	if (GameOverOverlay)
	{
		GameOverOverlay->SetVisibility(ESlateVisibility::Visible);
	}

	if (FinalScoreText)
	{
		FinalScoreText->SetText(FText::FromString(
			FString::Printf(TEXT("Final Score: %d"), Result.Score)));
	}

	if (CoinsEarnedText)
	{
		CoinsEarnedText->SetText(FText::FromString(
			FString::Printf(TEXT("Coins Earned: %d"), Result.CoinsEarned)));
	}
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------

void UPHCMiniGameHUD::OnPauseClicked()
{
	if (GameModeRef)
	{
		GameModeRef->PauseGame();
	}

	if (PauseOverlay)
	{
		PauseOverlay->SetVisibility(ESlateVisibility::Visible);
	}
}

void UPHCMiniGameHUD::OnResumeClicked()
{
	if (PauseOverlay)
	{
		PauseOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (GameModeRef)
	{
		GameModeRef->ResumeGame();
	}
}

void UPHCMiniGameHUD::OnQuitClicked()
{
	// Pause first, then show the confirmation overlay.
	if (GameModeRef && GameModeRef->bGameActive && !GameModeRef->bGamePaused)
	{
		GameModeRef->PauseGame();
	}

	if (PauseOverlay)
	{
		PauseOverlay->SetVisibility(ESlateVisibility::Visible);
	}
}

void UPHCMiniGameHUD::OnQuitConfirmed()
{
	if (GameModeRef)
	{
		GameModeRef->EndGame(/*bWon=*/ false);
	}

	// Return to lobby via PlayerController (avoids level travel).
	APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->ReturnFromGame();
	}
	else if (GameModeRef)
	{
		GameModeRef->ReturnToLobby();
	}
}

void UPHCMiniGameHUD::OnPlayAgainClicked()
{
	if (GameOverOverlay)
	{
		GameOverOverlay->SetVisibility(ESlateVisibility::Collapsed);
	}

	if (GameModeRef)
	{
		GameModeRef->StartGame();
	}
}

void UPHCMiniGameHUD::OnReturnClicked()
{
	APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->ReturnFromGame();
	}
	else if (GameModeRef)
	{
		GameModeRef->ReturnToLobby();
	}
}

// ---------------------------------------------------------------------------
// Delegate handler
// ---------------------------------------------------------------------------

void UPHCMiniGameHUD::HandleGameEnded(const FPHCGameResult& Result)
{
	ShowGameOver(Result);
}
