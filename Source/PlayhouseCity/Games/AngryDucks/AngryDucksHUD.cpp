// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/AngryDucks/AngryDucksHUD.h"
#include "Games/AngryDucks/AngryDucksGameMode.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogAngryDucksHUD, Log, All);

// ---------------------------------------------------------------------------
// Initialize - build the Angry Ducks game field
// ---------------------------------------------------------------------------

bool UAngryDucksHUD::Initialize()
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

	// -- Colors ---------------------------------------------------------------
	const FLinearColor SkyBlue(0.53f, 0.81f, 0.98f, 1.f);
	const FLinearColor GroundGreen(0.3f, 0.65f, 0.2f, 1.f);
	const FLinearColor SlingshotBrown(0.45f, 0.28f, 0.1f, 1.f);
	const FLinearColor DuckYellow(0.95f, 0.85f, 0.15f, 1.f);
	const FLinearColor BlockGray(0.55f, 0.55f, 0.55f, 1.f);
	const FLinearColor PigGreen(0.3f, 0.75f, 0.25f, 1.f);
	const FLinearColor BtnNormal(0.2f, 0.3f, 0.7f, 1.f);
	const FLinearColor BtnHover(0.3f, 0.4f, 0.85f, 1.f);
	const FLinearColor BtnPress(0.15f, 0.2f, 0.5f, 1.f);
	const FLinearColor LaunchRed(0.85f, 0.2f, 0.15f, 1.f);
	const FLinearColor LaunchRedHover(0.95f, 0.3f, 0.2f, 1.f);

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
	// GAME FIELD CANVAS (below top bar)
	// ====================================================================
	GameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("GameCanvas"));

	UCanvasPanelSlot* GameCanvasSlot = RootCanvas->AddChildToCanvas(GameCanvas);
	GameCanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameCanvasSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f)); // Below top bar

	// ---- Sky blue background ----
	GameFieldBackground = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("GameFieldBg"));
	GameFieldBackground->SetBrushColor(SkyBlue);

	UCanvasPanelSlot* BgSlot = GameCanvas->AddChildToCanvas(GameFieldBackground);
	BgSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	BgSlot->SetOffsets(FMargin(0.f));

	// ---- Ground at bottom ----
	GroundWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("Ground"));
	GroundWidget->SetBrushColor(GroundGreen);

	UCanvasPanelSlot* GroundSlot = GameCanvas->AddChildToCanvas(GroundWidget);
	GroundSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
	GroundSlot->SetOffsets(FMargin(0.f, -60.f, 0.f, 0.f)); // 60px tall at bottom

	// ====================================================================
	// SLINGSHOT (left side)
	// ====================================================================

	// Slingshot base (vertical pole)
	SlingshotWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SlingshotBase"));
	SlingshotWidget->SetBrushColor(SlingshotBrown);

	UCanvasPanelSlot* SlingSlot = GameCanvas->AddChildToCanvas(SlingshotWidget);
	SlingSlot->SetAnchors(FAnchors(0.1f, 0.5f, 0.1f, 0.5f));
	SlingSlot->SetAlignment(FVector2D(0.5f, 1.f));
	SlingSlot->SetOffsets(FMargin(0.f, 0.f, 20.f, 140.f)); // 20x140 pole

	// Slingshot fork (Y-shaped top)
	SlingshotFork = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("SlingshotFork"));
	SlingshotFork->SetBrushColor(SlingshotBrown);

	UCanvasPanelSlot* ForkSlot = GameCanvas->AddChildToCanvas(SlingshotFork);
	ForkSlot->SetAnchors(FAnchors(0.1f, 0.5f, 0.1f, 0.5f));
	ForkSlot->SetAlignment(FVector2D(0.5f, 1.f));
	ForkSlot->SetOffsets(FMargin(0.f, -140.f, 50.f, 20.f)); // Wide fork on top

	// ---- Duck on slingshot ----
	DuckWidget = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DuckOnSling"));
	DuckWidget->SetBrushColor(DuckYellow);

	UCanvasPanelSlot* DuckSlot = GameCanvas->AddChildToCanvas(DuckWidget);
	DuckSlot->SetAnchors(FAnchors(0.1f, 0.5f, 0.1f, 0.5f));
	DuckSlot->SetAlignment(FVector2D(0.5f, 0.5f));
	DuckSlot->SetOffsets(FMargin(0.f, -160.f, 30.f, 30.f)); // 30x30 duck sitting above fork

	// ---- Projectile duck (hidden by default, used during flight) ----
	ProjectileDuck = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ProjectileDuck"));
	ProjectileDuck->SetBrushColor(DuckYellow);
	ProjectileDuck->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* ProjSlot = GameCanvas->AddChildToCanvas(ProjectileDuck);
	ProjSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	ProjSlot->SetOffsets(FMargin(0.f, 0.f, 28.f, 28.f));

	// ====================================================================
	// STRUCTURES (right side - stacked gray blocks)
	// ====================================================================

	// Build a small fortress: 3 columns of blocks with pigs
	struct FBlockDef { float AnchorX; float AnchorY; float W; float H; };
	const FBlockDef BlockDefs[] = {
		// Bottom row - three pillars
		{ 0.60f, 0.72f, 20.f, 80.f },  // Left pillar
		{ 0.72f, 0.72f, 20.f, 80.f },  // Right pillar
		{ 0.60f, 0.56f, 150.f, 18.f }, // Top platform
		// Upper structure
		{ 0.64f, 0.48f, 20.f, 60.f },  // Upper left pillar
		{ 0.70f, 0.48f, 20.f, 60.f },  // Upper right pillar
		{ 0.62f, 0.38f, 100.f, 18.f }, // Upper platform
		// Loose blocks on right
		{ 0.80f, 0.72f, 30.f, 30.f },  // Right block 1
		{ 0.80f, 0.60f, 30.f, 30.f },  // Right block 2
	};

	StructureBlocks.SetNum(UE_ARRAY_COUNT(BlockDefs));
	for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(BlockDefs)); ++i)
	{
		const FBlockDef& Def = BlockDefs[i];
		UBorder* Block = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("Block%d"), i));
		Block->SetBrushColor(BlockGray);

		UCanvasPanelSlot* BlockSlot = GameCanvas->AddChildToCanvas(Block);
		BlockSlot->SetAnchors(FAnchors(Def.AnchorX, Def.AnchorY, Def.AnchorX, Def.AnchorY));
		BlockSlot->SetAlignment(FVector2D(0.5f, 1.f));
		BlockSlot->SetOffsets(FMargin(0.f, 0.f, Def.W, Def.H));

		StructureBlocks[i] = Block;
	}

	// ====================================================================
	// PIG TARGETS (green circles nestled among structures)
	// ====================================================================

	struct FPigDef { float AnchorX; float AnchorY; };
	const FPigDef PigDefs[] = {
		{ 0.66f, 0.64f },  // Pig on lower platform between pillars
		{ 0.67f, 0.40f },  // Pig on upper platform
		{ 0.80f, 0.54f },  // Pig on right loose blocks
	};

	PigTargets.SetNum(UE_ARRAY_COUNT(PigDefs));
	for (int32 i = 0; i < static_cast<int32>(UE_ARRAY_COUNT(PigDefs)); ++i)
	{
		const FPigDef& Def = PigDefs[i];
		UBorder* Pig = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(), *FString::Printf(TEXT("Pig%d"), i));
		Pig->SetBrushColor(PigGreen);

		UCanvasPanelSlot* PigSlot = GameCanvas->AddChildToCanvas(Pig);
		PigSlot->SetAnchors(FAnchors(Def.AnchorX, Def.AnchorY, Def.AnchorX, Def.AnchorY));
		PigSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		PigSlot->SetOffsets(FMargin(0.f, 0.f, 26.f, 26.f));

		// Pig label inside
		UTextBlock* PigLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(), *FString::Printf(TEXT("PigLbl%d"), i));
		PigLabel->SetText(FText::FromString(TEXT("P")));
		PigLabel->SetFont(FSlateFontInfo(
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 12));
		PigLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		PigLabel->SetJustification(ETextJustify::Center);
		Pig->SetContent(PigLabel);

		PigTargets[i] = Pig;
	}

	// ====================================================================
	// AIM LINE (initially hidden, shown during drag)
	// ====================================================================
	AimLine = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("AimLine"));
	AimLine->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 0.6f));
	AimLine->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* AimSlot = GameCanvas->AddChildToCanvas(AimLine);
	AimSlot->SetAnchors(FAnchors(0.f, 0.f, 0.f, 0.f));
	AimSlot->SetOffsets(FMargin(0.f, 0.f, 1.f, 4.f));

	// ====================================================================
	// BOTTOM CONTROLS PANEL (aim buttons, launch, info)
	// ====================================================================
	UBorder* ControlsPanel = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("ControlsPanel"));
	ControlsPanel->SetBrushColor(FLinearColor(0.05f, 0.05f, 0.1f, 0.85f));
	ControlsPanel->SetPadding(FMargin(12.f, 6.f));

	UCanvasPanelSlot* CtrlSlot = GameCanvas->AddChildToCanvas(ControlsPanel);
	CtrlSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f));
	CtrlSlot->SetOffsets(FMargin(0.f, -80.f, 0.f, 0.f)); // 80px tall at very bottom

	UHorizontalBox* CtrlHBox = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("CtrlHBox"));
	ControlsPanel->SetContent(CtrlHBox);

	// ---- Ducks remaining ----
	DucksRemainingText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DucksRemainingText"));
	DucksRemainingText->SetText(FText::FromString(TEXT("Ducks: 5")));
	DucksRemainingText->SetFont(StatusFont);
	DucksRemainingText->SetColorAndOpacity(FSlateColor(DuckYellow));

	UHorizontalBoxSlot* DucksTxtSlot = CtrlHBox->AddChildToHorizontalBox(DucksRemainingText);
	DucksTxtSlot->SetVerticalAlignment(VAlign_Center);
	DucksTxtSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

	// ---- Pigs remaining ----
	PigsRemainingText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PigsRemainingText"));
	PigsRemainingText->SetText(FText::FromString(TEXT("Pigs: 3")));
	PigsRemainingText->SetFont(StatusFont);
	PigsRemainingText->SetColorAndOpacity(FSlateColor(PigGreen));

	UHorizontalBoxSlot* PigsTxtSlot = CtrlHBox->AddChildToHorizontalBox(PigsRemainingText);
	PigsTxtSlot->SetVerticalAlignment(VAlign_Center);
	PigsTxtSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

	// ---- Level text ----
	LevelText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("LevelText"));
	LevelText->SetText(FText::FromString(TEXT("Level 1 / 3")));
	LevelText->SetFont(LabelFont);
	LevelText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	UHorizontalBoxSlot* LvlSlot = CtrlHBox->AddChildToHorizontalBox(LevelText);
	LvlSlot->SetVerticalAlignment(VAlign_Center);
	LvlSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	// ---- Spacer ----
	USpacer* CtrlSpacer = WidgetTree->ConstructWidget<USpacer>(
		USpacer::StaticClass(), TEXT("CtrlSpacer"));
	UHorizontalBoxSlot* SpacerSlot = CtrlHBox->AddChildToHorizontalBox(CtrlSpacer);
	SpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

	// ---- Angle text ----
	AngleText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("AngleText"));
	AngleText->SetText(FText::FromString(TEXT("Angle: 45")));
	AngleText->SetFont(LabelFont);
	AngleText->SetColorAndOpacity(FSlateColor(FLinearColor::White));

	UHorizontalBoxSlot* AngSlot = CtrlHBox->AddChildToHorizontalBox(AngleText);
	AngSlot->SetVerticalAlignment(VAlign_Center);
	AngSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

	// ---- Power text ----
	PowerText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("PowerText"));
	PowerText->SetText(FText::FromString(TEXT("Power: 100%")));
	PowerText->SetFont(LabelFont);
	PowerText->SetColorAndOpacity(FSlateColor(FLinearColor(1.f, 0.6f, 0.2f)));

	UHorizontalBoxSlot* PwrSlot = CtrlHBox->AddChildToHorizontalBox(PowerText);
	PwrSlot->SetVerticalAlignment(VAlign_Center);
	PwrSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));

	// ---- AIM UP button ----
	AimUpButton = MakeButton(TEXT("AimUpBtn"), TEXT("AIM UP"), BtnNormal, BtnHover, BtnPress);

	UHorizontalBoxSlot* AimUpSlot = CtrlHBox->AddChildToHorizontalBox(AimUpButton);
	AimUpSlot->SetVerticalAlignment(VAlign_Center);
	AimUpSlot->SetPadding(FMargin(4.f, 0.f));
	AimUpSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	// ---- AIM DOWN button ----
	AimDownButton = MakeButton(TEXT("AimDownBtn"), TEXT("AIM DOWN"), BtnNormal, BtnHover, BtnPress);

	UHorizontalBoxSlot* AimDownSlot = CtrlHBox->AddChildToHorizontalBox(AimDownButton);
	AimDownSlot->SetVerticalAlignment(VAlign_Center);
	AimDownSlot->SetPadding(FMargin(4.f, 0.f));
	AimDownSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	// ---- LAUNCH button ----
	LaunchButton = MakeButton(TEXT("LaunchBtn"), TEXT("LAUNCH"), LaunchRed, LaunchRedHover, BtnPress);

	UHorizontalBoxSlot* LaunchSlot = CtrlHBox->AddChildToHorizontalBox(LaunchButton);
	LaunchSlot->SetVerticalAlignment(VAlign_Center);
	LaunchSlot->SetPadding(FMargin(8.f, 0.f));
	LaunchSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));

	UE_LOG(LogAngryDucksHUD, Log,
		TEXT("AngryDucks HUD built: slingshot, %d blocks, %d pigs, aim/launch controls"),
		StructureBlocks.Num(), PigTargets.Num());

	return true;
}

// ---------------------------------------------------------------------------

void UAngryDucksHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Cast from the base-class reference (set by the PlayerController
	// before NativeConstruct runs).
	DucksMode = Cast<AAngryDucksGameMode>(GameModeRef);

	// Hide the aim line until the player starts dragging.
	if (AimLine)
	{
		AimLine->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Wire button callbacks.
	if (AimUpButton)
	{
		AimUpButton->OnClicked.AddDynamic(this, &UAngryDucksHUD::OnAimUpClicked);
	}
	if (AimDownButton)
	{
		AimDownButton->OnClicked.AddDynamic(this, &UAngryDucksHUD::OnAimDownClicked);
	}
	if (LaunchButton)
	{
		LaunchButton->OnClicked.AddDynamic(this, &UAngryDucksHUD::OnLaunchClicked);
	}

	SetIsFocusable(true);
}

// ---------------------------------------------------------------------------

void UAngryDucksHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!DucksMode)
	{
		return;
	}

	// Update duck remaining count.
	if (DucksRemainingText)
	{
		DucksRemainingText->SetText(FText::FromString(
			FString::Printf(TEXT("Ducks: %d"), DucksMode->DucksRemaining)));
	}

	// Update pig remaining count.
	if (PigsRemainingText)
	{
		PigsRemainingText->SetText(FText::FromString(
			FString::Printf(TEXT("Pigs: %d"), DucksMode->PigsRemaining)));
	}

	// Update level text.
	if (LevelText)
	{
		LevelText->SetText(FText::FromString(
			FString::Printf(TEXT("Level %d / %d"), DucksMode->CurrentLevel, DucksMode->TotalLevels)));
	}

	// Update angle display.
	if (AngleText)
	{
		AngleText->SetText(FText::FromString(
			FString::Printf(TEXT("Angle: %d"), FMath::RoundToInt32(LaunchAngle))));
	}

	// -- Projectile animation --
	if (bProjectileActive && ProjectileDuck)
	{
		// Apply gravity
		ProjectileVel.Y += ProjectileGravity * InDeltaTime;

		// Move projectile
		ProjectilePos += ProjectileVel * InDeltaTime;

		// Update the projectile widget position
		UCanvasPanelSlot* ProjSlot = Cast<UCanvasPanelSlot>(ProjectileDuck->Slot);
		if (ProjSlot)
		{
			ProjSlot->SetPosition(ProjectilePos);
		}

		// Check if projectile has gone off screen or hit the ground area
		if (ProjectilePos.Y > 600.f || ProjectilePos.X > 1200.f || ProjectilePos.X < -50.f)
		{
			bProjectileActive = false;
			ProjectileDuck->SetVisibility(ESlateVisibility::Collapsed);

			// Show the duck on slingshot again if ducks remain
			if (DuckWidget && DucksMode->DucksRemaining > 0)
			{
				DuckWidget->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
		}
	}
}

// ---------------------------------------------------------------------------
// -- Button callbacks --
// ---------------------------------------------------------------------------

void UAngryDucksHUD::OnAimUpClicked()
{
	LaunchAngle = FMath::Clamp(LaunchAngle + 5.f, 5.f, 85.f);

	if (AngleText)
	{
		AngleText->SetText(FText::FromString(
			FString::Printf(TEXT("Angle: %d"), FMath::RoundToInt32(LaunchAngle))));
	}

	// Update game mode aim direction
	if (DucksMode)
	{
		const float Rad = FMath::DegreesToRadians(LaunchAngle);
		DucksMode->SetAimDirection(FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)));
	}
}

void UAngryDucksHUD::OnAimDownClicked()
{
	LaunchAngle = FMath::Clamp(LaunchAngle - 5.f, 5.f, 85.f);

	if (AngleText)
	{
		AngleText->SetText(FText::FromString(
			FString::Printf(TEXT("Angle: %d"), FMath::RoundToInt32(LaunchAngle))));
	}

	if (DucksMode)
	{
		const float Rad = FMath::DegreesToRadians(LaunchAngle);
		DucksMode->SetAimDirection(FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)));
	}
}

void UAngryDucksHUD::OnLaunchClicked()
{
	if (!DucksMode || !DucksMode->bGameActive || !DucksMode->bReadyToLaunch)
	{
		return;
	}

	// Compute aim direction from the current angle
	const float Rad = FMath::DegreesToRadians(LaunchAngle);
	const FVector2D AimDir(FMath::Cos(Rad), FMath::Sin(Rad));

	// Launch the duck in the game mode (full power for button launch)
	DucksMode->LaunchDuck(AimDir, 1.f);

	// Start projectile animation
	if (ProjectileDuck && DuckWidget)
	{
		// Hide the duck on the slingshot
		DuckWidget->SetVisibility(ESlateVisibility::Collapsed);

		// Show and position the projectile at the slingshot location
		ProjectileDuck->SetVisibility(ESlateVisibility::HitTestInvisible);

		// Start position near the slingshot (roughly 10% from left, 40% from top)
		ProjectilePos = FVector2D(100.f, 200.f);
		ProjectileVel = FVector2D(
			FMath::Cos(Rad) * ButtonLaunchSpeed,
			-FMath::Sin(Rad) * ButtonLaunchSpeed); // Negative Y = upward in screen space

		UCanvasPanelSlot* ProjSlot = Cast<UCanvasPanelSlot>(ProjectileDuck->Slot);
		if (ProjSlot)
		{
			ProjSlot->SetPosition(ProjectilePos);
		}

		bProjectileActive = true;
	}
}

// ---------------------------------------------------------------------------
// -- Mouse input --
// ---------------------------------------------------------------------------

FReply UAngryDucksHUD::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (DucksMode && DucksMode->bGameActive && DucksMode->bReadyToLaunch)
	{
		bDragging = true;
		DragStartPos = InMouseEvent.GetScreenSpacePosition();
		DragCurrentPos = DragStartPos;

		if (AimLine)
		{
			AimLine->SetVisibility(ESlateVisibility::HitTestInvisible);
		}

		return FReply::Handled().CaptureMouse(TakeWidget());
	}
	return FReply::Handled();
}

FReply UAngryDucksHUD::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && DucksMode)
	{
		DragCurrentPos = InMouseEvent.GetScreenSpacePosition();

		const FVector2D AimDir = ComputeAimDirection(DragStartPos, DragCurrentPos);
		DucksMode->SetAimDirection(AimDir);

		// Update the aim line visual
		const FVector2D CanvasStart = ScreenToCanvas(InGeometry, DragStartPos);
		const FVector2D CanvasEnd = ScreenToCanvas(InGeometry, DragCurrentPos);
		UpdateAim(CanvasStart, CanvasEnd);
	}
	return FReply::Handled();
}

FReply UAngryDucksHUD::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bDragging && DucksMode)
	{
		bDragging = false;

		const FVector2D AimDir = ComputeAimDirection(DragStartPos, DragCurrentPos);
		const float Power = ComputeLaunchPower(DragStartPos, DragCurrentPos);

		DucksMode->LaunchDuck(AimDir, Power);

		if (AimLine)
		{
			AimLine->SetVisibility(ESlateVisibility::Collapsed);
		}

		// Start projectile animation for drag-based launch
		if (ProjectileDuck && DuckWidget && Power > 0.05f)
		{
			DuckWidget->SetVisibility(ESlateVisibility::Collapsed);
			ProjectileDuck->SetVisibility(ESlateVisibility::HitTestInvisible);

			ProjectilePos = FVector2D(100.f, 200.f);
			ProjectileVel = FVector2D(
				AimDir.X * ButtonLaunchSpeed * Power,
				-AimDir.Y * ButtonLaunchSpeed * Power);

			UCanvasPanelSlot* ProjSlot = Cast<UCanvasPanelSlot>(ProjectileDuck->Slot);
			if (ProjSlot)
			{
				ProjSlot->SetPosition(ProjectilePos);
			}

			bProjectileActive = true;
		}

		return FReply::Handled().ReleaseMouseCapture();
	}
	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// -- Touch input --
// ---------------------------------------------------------------------------

FReply UAngryDucksHUD::NativeOnTouchStarted(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	if (DucksMode && DucksMode->bGameActive && DucksMode->bReadyToLaunch)
	{
		bDragging = true;
		DragStartPos = InTouchEvent.GetScreenSpacePosition();
		DragCurrentPos = DragStartPos;

		if (AimLine)
		{
			AimLine->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
	}
	return FReply::Handled();
}

FReply UAngryDucksHUD::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	if (bDragging && DucksMode)
	{
		DragCurrentPos = InTouchEvent.GetScreenSpacePosition();

		const FVector2D AimDir = ComputeAimDirection(DragStartPos, DragCurrentPos);
		DucksMode->SetAimDirection(AimDir);

		const FVector2D CanvasStart = ScreenToCanvas(InGeometry, DragStartPos);
		const FVector2D CanvasEnd = ScreenToCanvas(InGeometry, DragCurrentPos);
		UpdateAim(CanvasStart, CanvasEnd);
	}
	return FReply::Handled();
}

FReply UAngryDucksHUD::NativeOnTouchEnded(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	if (bDragging && DucksMode)
	{
		bDragging = false;

		const FVector2D AimDir = ComputeAimDirection(DragStartPos, DragCurrentPos);
		const float Power = ComputeLaunchPower(DragStartPos, DragCurrentPos);

		DucksMode->LaunchDuck(AimDir, Power);

		if (AimLine)
		{
			AimLine->SetVisibility(ESlateVisibility::Collapsed);
		}

		// Start projectile animation for touch-based launch
		if (ProjectileDuck && DuckWidget && Power > 0.05f)
		{
			DuckWidget->SetVisibility(ESlateVisibility::Collapsed);
			ProjectileDuck->SetVisibility(ESlateVisibility::HitTestInvisible);

			ProjectilePos = FVector2D(100.f, 200.f);
			ProjectileVel = FVector2D(
				AimDir.X * ButtonLaunchSpeed * Power,
				-AimDir.Y * ButtonLaunchSpeed * Power);

			UCanvasPanelSlot* ProjSlot = Cast<UCanvasPanelSlot>(ProjectileDuck->Slot);
			if (ProjSlot)
			{
				ProjSlot->SetPosition(ProjectilePos);
			}

			bProjectileActive = true;
		}
	}
	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// -- Public helpers --
// ---------------------------------------------------------------------------

void UAngryDucksHUD::UpdateDuckCount(int32 Count)
{
	if (DucksRemainingText)
	{
		DucksRemainingText->SetText(FText::FromString(
			FString::Printf(TEXT("Ducks: %d"), Count)));
	}
}

void UAngryDucksHUD::ShowLevelClear(int32 Level)
{
	if (LevelText)
	{
		LevelText->SetText(FText::FromString(
			FString::Printf(TEXT("Level %d Cleared!"), Level)));
	}
}

void UAngryDucksHUD::UpdateAim(FVector2D AimStart, FVector2D AimEnd)
{
	if (!AimLine || !GameCanvas)
	{
		return;
	}

	// Position the aim line image at the midpoint between start and end,
	// and scale/rotate to represent the trajectory line.
	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(AimLine->Slot);
	if (!Slot)
	{
		return;
	}

	const FVector2D Midpoint = (AimStart + AimEnd) * 0.5f;
	const FVector2D Delta = AimEnd - AimStart;
	const float Length = Delta.Size();
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));

	Slot->SetPosition(Midpoint);
	Slot->SetSize(FVector2D(FMath::Max(Length, 1.f), 4.f));

	AimLine->SetRenderTransformAngle(Angle);
	AimLine->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
}

// ---------------------------------------------------------------------------
// -- Internal helpers --
// ---------------------------------------------------------------------------

FVector2D UAngryDucksHUD::ScreenToCanvas(const FGeometry& Geometry, FVector2D ScreenPos) const
{
	if (!GameCanvas)
	{
		return FVector2D::ZeroVector;
	}

	const FGeometry& CanvasGeometry = GameCanvas->GetCachedGeometry();
	return CanvasGeometry.AbsoluteToLocal(ScreenPos);
}

FVector2D UAngryDucksHUD::ComputeAimDirection(FVector2D DragStart, FVector2D DragCurrent) const
{
	// The aim direction is the inverse of the drag vector -- pull back to
	// launch forward, like a real slingshot.
	FVector2D DragDelta = DragStart - DragCurrent;

	// Remap screen Y (down is positive) so that dragging down aims upward.
	DragDelta.Y = -DragDelta.Y;

	if (DragDelta.SizeSquared() > KINDA_SMALL_NUMBER)
	{
		DragDelta.Normalize();
	}

	return DragDelta;
}

float UAngryDucksHUD::ComputeLaunchPower(FVector2D DragStart, FVector2D DragCurrent) const
{
	const float Distance = FVector2D::Distance(DragStart, DragCurrent);
	return FMath::Clamp(Distance / MaxDragDistance, 0.f, 1.f);
}
