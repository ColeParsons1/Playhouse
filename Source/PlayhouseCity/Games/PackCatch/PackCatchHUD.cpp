// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/PackCatch/PackCatchHUD.h"
#include "Engine/World.h"
#include "Games/PackCatch/PackCatchGameMode.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogPackCatchHUD, Log, All);

static const FLinearColor PackCatchBgColor(0.04f, 0.06f, 0.12f, 1.f);
static const FLinearColor CatcherColor(0.2f, 0.6f, 0.9f, 1.f);
static const FLinearColor HeartColor(1.f, 0.2f, 0.2f, 1.f);
static const FLinearColor ComboColor(1.f, 0.85f, 0.f, 1.f);

// ---------------------------------------------------------------------------
// Initialize - build the Pack Catch UI programmatically
// ---------------------------------------------------------------------------

bool UPackCatchHUD::Initialize()
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

	const FSlateFontInfo ComboFont = FSlateFontInfo(
		FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 28);

	// ---- Game content area (below top bar) ----
	UBorder* GameArea = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("GameArea"));
	GameArea->SetBrushColor(PackCatchBgColor);
	GameArea->SetPadding(FMargin(0.f));

	UCanvasPanelSlot* GameSlot = RootCanvas->AddChildToCanvas(GameArea);
	GameSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	GameSlot->SetOffsets(FMargin(0.f, 50.f, 0.f, 0.f));

	// ---- Game canvas for all dynamic items ----
	GameCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("GameCanvas"));
	GameArea->SetContent(GameCanvas);

	// ---- Catcher image ----
	CatcherImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(), TEXT("CatcherImage"));
	CatcherImage->SetColorAndOpacity(CatcherColor);
	CatcherImage->SetDesiredSizeOverride(FVector2D(80.f, 30.f));

	UCanvasPanelSlot* CatcherSlot = GameCanvas->AddChildToCanvas(CatcherImage);
	CatcherSlot->SetAutoSize(true);
	CatcherSlot->SetPosition(FVector2D(260.f, 550.f));

	// ---- Lives display (horizontal row of hearts, top-left) ----
	LivesDisplay = WidgetTree->ConstructWidget<UHorizontalBox>(
		UHorizontalBox::StaticClass(), TEXT("LivesDisplay"));

	UCanvasPanelSlot* LivesSlot = GameCanvas->AddChildToCanvas(LivesDisplay);
	LivesSlot->SetAutoSize(true);
	LivesSlot->SetPosition(FVector2D(10.f, 10.f));

	// ---- Combo text (center of screen) ----
	ComboText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("ComboText"));
	ComboText->SetText(FText::FromString(TEXT("x3 COMBO!")));
	ComboText->SetFont(ComboFont);
	ComboText->SetColorAndOpacity(FSlateColor(ComboColor));
	ComboText->SetJustification(ETextJustify::Center);
	ComboText->SetVisibility(ESlateVisibility::Collapsed);

	UCanvasPanelSlot* ComboSlot = GameCanvas->AddChildToCanvas(ComboText);
	ComboSlot->SetAutoSize(true);
	ComboSlot->SetAnchors(FAnchors(0.5f, 0.3f));
	ComboSlot->SetAlignment(FVector2D(0.5f, 0.5f));

	UE_LOG(LogPackCatchHUD, Log, TEXT("PackCatch HUD built programmatically: GameCanvas, CatcherImage, LivesDisplay, ComboText"));

	return true;
}

// ---------------------------------------------------------------------------
// Widget lifecycle
// ---------------------------------------------------------------------------

void UPackCatchHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// Cache the game mode from the base-class reference
	// (set by the PlayerController before NativeConstruct runs).
	CatchMode = Cast<APackCatchGameMode>(GameModeRef);

	// Hide combo text initially.
	if (ComboText)
	{
		ComboText->SetVisibility(ESlateVisibility::Collapsed);
	}

	// Build the hearts display.
	MaxLivesDisplay = CatchMode ? CatchMode->StartingLives : 3;
	if (LivesDisplay)
	{
		HeartImages.Empty();
		for (int32 i = 0; i < MaxLivesDisplay; ++i)
		{
			UImage* Heart = NewObject<UImage>(this);
			if (Heart)
			{
				Heart->SetColorAndOpacity(FLinearColor(1.f, 0.2f, 0.2f, 1.f));
				Heart->SetDesiredSizeOverride(FVector2D(24.f, 24.f));

				UHorizontalBoxSlot* BoxSlot = LivesDisplay->AddChildToHorizontalBox(Heart);
				if (BoxSlot)
				{
					BoxSlot->SetPadding(FMargin(4.f, 0.f));
				}

				HeartImages.Add(Heart);
			}
		}
	}

	// Bind to game mode delegates for event-driven visual effects.
	if (CatchMode)
	{
		CatchMode->OnItemCaught.AddDynamic(this, &UPackCatchHUD::HandleItemCaught);
		CatchMode->OnItemMissed.AddDynamic(this, &UPackCatchHUD::HandleItemMissed);
		CatchMode->OnItemSpawned.AddDynamic(this, &UPackCatchHUD::HandleItemSpawned);
	}

	SetIsFocusable(true);
}

void UPackCatchHUD::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!CatchMode || !GameCanvas)
	{
		return;
	}

	// Update catcher position.
	UpdateCatcherPosition(CatchMode->PlayerX);

	// Update all falling item visual positions.
	for (const FPHCFallingItem& Item : CatchMode->FallingItems)
	{
		if (TObjectPtr<UImage>* FoundImage = FallingItemImages.Find(Item.ItemId))
		{
			if (*FoundImage)
			{
				const FVector2D CanvasPos = FieldToCanvas(Item.Position);
				if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>((*FoundImage)->Slot))
				{
					const float HalfItem = CatchMode->ItemSize * 0.5f;
					Slot->SetPosition(CanvasPos - FVector2D(HalfItem, HalfItem));
				}
			}
		}
	}

	// Update lives display.
	UpdateLives(CatchMode->Lives);

	// Update score in parent.
	UpdateScore(CatchMode->GetScore());

	// Show combo if the player has a streak going.
	if (CatchMode->ConsecutiveCatches >= 3)
	{
		ShowCombo(CatchMode->ConsecutiveCatches);
	}

	// Purge visuals for items that no longer exist in the game mode.
	TArray<int32> KeysToRemove;
	for (auto& Pair : FallingItemImages)
	{
		bool bFound = false;
		for (const FPHCFallingItem& Item : CatchMode->FallingItems)
		{
			if (Item.ItemId == Pair.Key)
			{
				bFound = true;
				break;
			}
		}
		if (!bFound)
		{
			KeysToRemove.Add(Pair.Key);
		}
	}
	for (int32 Key : KeysToRemove)
	{
		RemoveItemVisual(Key);
	}
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

FReply UPackCatchHUD::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (CatchMode && CatchMode->bGameActive)
	{
		const float FieldX = ScreenXToFieldX(InGeometry, InMouseEvent.GetScreenSpacePosition().X);
		CatchMode->SetPlayerPosition(FieldX);
	}

	return FReply::Handled();
}

FReply UPackCatchHUD::NativeOnTouchMoved(const FGeometry& InGeometry, const FPointerEvent& InTouchEvent)
{
	if (CatchMode && CatchMode->bGameActive)
	{
		const float FieldX = ScreenXToFieldX(InGeometry, InTouchEvent.GetScreenSpacePosition().X);
		CatchMode->SetPlayerPosition(FieldX);
	}

	return FReply::Handled();
}

// ---------------------------------------------------------------------------
// Visual updates
// ---------------------------------------------------------------------------

void UPackCatchHUD::UpdateCatcherPosition(float X)
{
	if (!CatcherImage || !CatchMode)
	{
		return;
	}

	const FVector2D CanvasPos = FieldToCanvas(FVector2D(X, CatchMode->FieldHeight - CatchMode->CatcherHeight));
	if (UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(CatcherImage->Slot))
	{
		const float HalfCatcher = CatchMode->CatcherWidth * 0.5f;
		Slot->SetPosition(FVector2D(CanvasPos.X - HalfCatcher, CanvasPos.Y));
	}
}

void UPackCatchHUD::ShowCatchEffect(FVector2D Position, bool bGoodCatch)
{
	if (!GameCanvas)
	{
		return;
	}

	UImage* FlashImage = NewObject<UImage>(this);
	if (!FlashImage)
	{
		return;
	}

	FlashImage->SetColorAndOpacity(bGoodCatch
		? FLinearColor(1.f, 1.f, 0.2f, 0.8f)  // golden sparkle
		: FLinearColor(1.f, 0.1f, 0.1f, 0.8f)); // red explosion

	FlashImage->SetDesiredSizeOverride(FVector2D(40.f, 40.f));

	UCanvasPanelSlot* Slot = GameCanvas->AddChildToCanvas(FlashImage);
	if (Slot)
	{
		const FVector2D CanvasPos = FieldToCanvas(Position);
		Slot->SetPosition(CanvasPos - FVector2D(20.f, 20.f));
		Slot->SetAutoSize(true);
	}

	// Remove the flash after a short delay.
	if (UWorld* World = GetWorld())
	{
		FTimerHandle TempHandle;
		TWeakObjectPtr<UImage> WeakFlash(FlashImage);
		World->GetTimerManager().SetTimer(
			TempHandle,
			[WeakFlash]()
			{
				if (WeakFlash.IsValid())
				{
					WeakFlash->RemoveFromParent();
				}
			},
			0.3f,
			/*bLoop=*/ false
		);
	}
}

void UPackCatchHUD::UpdateLives(int32 CurrentLives)
{
	for (int32 i = 0; i < HeartImages.Num(); ++i)
	{
		if (HeartImages[i])
		{
			const float Alpha = (i < CurrentLives) ? 1.f : 0.2f;
			HeartImages[i]->SetColorAndOpacity(FLinearColor(1.f, 0.2f, 0.2f, Alpha));
		}
	}
}

void UPackCatchHUD::ShowCombo(int32 ComboCount)
{
	if (!ComboText)
	{
		return;
	}

	ComboText->SetText(FText::FromString(FString::Printf(TEXT("x%d COMBO!"), ComboCount)));
	ComboText->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

	// Auto-hide after 1.5 seconds.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ComboFadeHandle);
		World->GetTimerManager().SetTimer(
			ComboFadeHandle,
			[this]()
			{
				if (ComboText)
				{
					ComboText->SetVisibility(ESlateVisibility::Collapsed);
				}
			},
			1.5f,
			/*bLoop=*/ false
		);
	}
}

// ---------------------------------------------------------------------------
// Item visual management
// ---------------------------------------------------------------------------

UImage* UPackCatchHUD::SpawnItemVisual(const FPHCFallingItem& Item)
{
	if (!GameCanvas)
	{
		return nullptr;
	}

	UImage* ItemImage = NewObject<UImage>(this);
	if (!ItemImage)
	{
		return nullptr;
	}

	const float Size = CatchMode ? CatchMode->ItemSize : 30.f;
	ItemImage->SetColorAndOpacity(GetPackageColor(Item.Type));
	ItemImage->SetDesiredSizeOverride(FVector2D(Size, Size));

	UCanvasPanelSlot* Slot = GameCanvas->AddChildToCanvas(ItemImage);
	if (Slot)
	{
		const FVector2D CanvasPos = FieldToCanvas(Item.Position);
		Slot->SetPosition(CanvasPos - FVector2D(Size * 0.5f, Size * 0.5f));
		Slot->SetAutoSize(true);
	}

	FallingItemImages.Add(Item.ItemId, ItemImage);
	return ItemImage;
}

void UPackCatchHUD::RemoveItemVisual(int32 ItemId)
{
	if (TObjectPtr<UImage>* FoundImage = FallingItemImages.Find(ItemId))
	{
		if (*FoundImage)
		{
			(*FoundImage)->RemoveFromParent();
		}
		FallingItemImages.Remove(ItemId);
	}
}

// ---------------------------------------------------------------------------
// Delegate handlers
// ---------------------------------------------------------------------------

void UPackCatchHUD::HandleItemSpawned(const FPHCFallingItem& Item)
{
	// Create a visual widget for the newly spawned item.
	SpawnItemVisual(Item);
}

void UPackCatchHUD::HandleItemCaught(int32 ItemId, bool bGoodCatch)
{
	// Find the item position for the effect, then remove its visual.
	if (CatchMode)
	{
		for (const FPHCFallingItem& Item : CatchMode->FallingItems)
		{
			if (Item.ItemId == ItemId)
			{
				ShowCatchEffect(Item.Position, bGoodCatch);
				break;
			}
		}
	}

	RemoveItemVisual(ItemId);
}

void UPackCatchHUD::HandleItemMissed(int32 ItemId)
{
	RemoveItemVisual(ItemId);
}

// ---------------------------------------------------------------------------
// Coordinate conversion
// ---------------------------------------------------------------------------

FVector2D UPackCatchHUD::FieldToCanvas(FVector2D FieldPos) const
{
	if (!GameCanvas || !CatchMode)
	{
		return FieldPos;
	}

	const FGeometry& CanvasGeometry = GameCanvas->GetCachedGeometry();
	const FVector2D CanvasSize = CanvasGeometry.GetLocalSize();

	if (CanvasSize.X <= 0.f || CanvasSize.Y <= 0.f)
	{
		return FieldPos;
	}

	const float ScaleX = CanvasSize.X / CatchMode->FieldWidth;
	const float ScaleY = CanvasSize.Y / CatchMode->FieldHeight;

	return FVector2D(FieldPos.X * ScaleX, FieldPos.Y * ScaleY);
}

float UPackCatchHUD::ScreenXToFieldX(const FGeometry& Geometry, float ScreenX) const
{
	if (!CatchMode || !GameCanvas)
	{
		return 0.f;
	}

	const FGeometry& CanvasGeometry = GameCanvas->GetCachedGeometry();
	const FVector2D LocalPos = CanvasGeometry.AbsoluteToLocal(FVector2D(ScreenX, 0.f));
	const FVector2D CanvasSize = CanvasGeometry.GetLocalSize();

	if (CanvasSize.X <= 0.f)
	{
		return CatchMode->FieldWidth * 0.5f;
	}

	const float NormalizedX = FMath::Clamp(LocalPos.X / CanvasSize.X, 0.f, 1.f);
	return NormalizedX * CatchMode->FieldWidth;
}

FLinearColor UPackCatchHUD::GetPackageColor(EPHCPackageType Type) const
{
	switch (Type)
	{
	case EPHCPackageType::Normal:
		return FLinearColor(0.6f, 0.4f, 0.2f, 1.f); // brown cardboard
	case EPHCPackageType::Bonus:
		return FLinearColor(1.f, 0.85f, 0.f, 1.f);   // golden
	case EPHCPackageType::Bomb:
		return FLinearColor(0.2f, 0.2f, 0.2f, 1.f);   // dark/black
	default:
		return FLinearColor::White;
	}
}
