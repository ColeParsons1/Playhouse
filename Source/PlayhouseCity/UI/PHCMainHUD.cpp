// Copyright PlayhouseCity. All Rights Reserved.

#include "UI/PHCMainHUD.h"

#include "UI/Panels/PHCCreateRoomPanel.h"
#include "UI/Panels/PHCRoomsPanel.h"
#include "UI/Panels/PHCFollowPanel.h"
#include "UI/Panels/PHCGamesPanel.h"
#include "UI/Panels/PHCProfilePanel.h"

#include "Core/PHCPlayerController.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Spacer.h"
#include "Components/SizeBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogPHCMainHUD, Log, All);

// ===================================================================
UPHCMainHUD::UPHCMainHUD(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Wire up panel classes for pure C++ instantiation (no Blueprint needed).
	CreateRoomPanelClass = UPHCCreateRoomPanel::StaticClass();
	RoomsPanelClass      = UPHCRoomsPanel::StaticClass();
	FollowPanelClass     = UPHCFollowPanel::StaticClass();
	GamesPanelClass      = UPHCGamesPanel::StaticClass();
	ProfilePanelClass    = UPHCProfilePanel::StaticClass();
}

// ===================================================================
//  UUserWidget Overrides
// ===================================================================

bool UPHCMainHUD::Initialize()
{
	bool bResult = Super::Initialize();
	if (!bResult)
	{
		return false;
	}

	// =================================================================
	//  Build the entire widget tree in C++ (no UMG Blueprint required).
	//  This MUST happen in Initialize() -- before TakeWidget() builds
	//  the Slate representation from the WidgetTree.
	// =================================================================

	if (!ContentSwitcher)
	{
		// --- Colour palette ---
		const FLinearColor BarBg(FColor::FromHex(TEXT("1A1A2EFF")));
		const FLinearColor White(FLinearColor::White);

		const FSlateFontInfo LabelFont = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 14);
		const FSlateFontInfo EconFont  = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 16);
		const FSlateFontInfo UserFont  = FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 16);

		// ---- Root canvas (full screen) ----
		UCanvasPanel* RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
		WidgetTree->RootWidget = RootCanvas;

		// ============================================================
		//  TOP BAR  (anchored top, full width, 60 px tall)
		// ============================================================
		UBorder* TopBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("TopBar"));
		TopBar->SetBrushColor(BarBg);
		TopBar->SetPadding(FMargin(16.f, 0.f, 16.f, 0.f));

		UCanvasPanelSlot* TopBarSlot = RootCanvas->AddChildToCanvas(TopBar);
		TopBarSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 0.f));   // stretch-top
		TopBarSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 60.f));    // height = 60
		TopBarSlot->SetAlignment(FVector2D(0.f, 0.f));

		// Horizontal box inside top bar
		UHorizontalBox* TopHBox = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopHBox"));
		TopBar->SetContent(TopHBox);

		// --- Username ---
		UsernameDisplay = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("UsernameDisplay"));
		UsernameDisplay->SetText(FText::FromString(TEXT("Player")));
		UsernameDisplay->SetFont(UserFont);
		UsernameDisplay->SetColorAndOpacity(FSlateColor(White));

		UHorizontalBoxSlot* UsernameSlot = TopHBox->AddChildToHorizontalBox(UsernameDisplay);
		UsernameSlot->SetVerticalAlignment(VAlign_Center);
		UsernameSlot->SetPadding(FMargin(0.f, 0.f, 8.f, 0.f));

		// --- Spacer (fills remaining space) ---
		USpacer* TopSpacer = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("TopSpacer"));
		UHorizontalBoxSlot* TopSpacerSlot = TopHBox->AddChildToHorizontalBox(TopSpacer);
		TopSpacerSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

		// --- Coins text ---
		CoinDisplay = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("CoinDisplay"));
		CoinDisplay->SetText(FText::FromString(TEXT("Coins: 0")));
		CoinDisplay->SetFont(EconFont);
		CoinDisplay->SetColorAndOpacity(FSlateColor(White));

		UHorizontalBoxSlot* CoinSlot = TopHBox->AddChildToHorizontalBox(CoinDisplay);
		CoinSlot->SetVerticalAlignment(VAlign_Center);
		CoinSlot->SetPadding(FMargin(8.f, 0.f, 16.f, 0.f));

		// --- Gems text ---
		GemDisplay = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("GemDisplay"));
		GemDisplay->SetText(FText::FromString(TEXT("Gems: 0")));
		GemDisplay->SetFont(EconFont);
		GemDisplay->SetColorAndOpacity(FSlateColor(White));

		UHorizontalBoxSlot* GemSlot = TopHBox->AddChildToHorizontalBox(GemDisplay);
		GemSlot->SetVerticalAlignment(VAlign_Center);

		// ============================================================
		//  CONTENT SWITCHER  (fills the middle area between bars)
		// ============================================================
		ContentSwitcher = WidgetTree->ConstructWidget<UWidgetSwitcher>(UWidgetSwitcher::StaticClass(), TEXT("ContentSwitcher"));

		UCanvasPanelSlot* SwitcherSlot = RootCanvas->AddChildToCanvas(ContentSwitcher);
		SwitcherSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f)); // stretch-all
		SwitcherSlot->SetOffsets(FMargin(0.f, 60.f, 0.f, 60.f)); // inset top/bottom by 60
		SwitcherSlot->SetAlignment(FVector2D(0.f, 0.f));

		// ============================================================
		//  BOTTOM NAV BAR  (anchored bottom, full width, 60 px tall)
		// ============================================================
		UBorder* BottomBar = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BottomBar"));
		BottomBar->SetBrushColor(BarBg);

		UCanvasPanelSlot* BottomBarSlot = RootCanvas->AddChildToCanvas(BottomBar);
		BottomBarSlot->SetAnchors(FAnchors(0.f, 1.f, 1.f, 1.f)); // stretch-bottom
		BottomBarSlot->SetOffsets(FMargin(0.f, -60.f, 0.f, 60.f)); // pos Y = -60, height = 60
		BottomBarSlot->SetAlignment(FVector2D(0.f, 0.f));

		// Horizontal box for tab buttons
		BottomNavBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("BottomNavBar"));
		BottomBar->SetContent(BottomNavBar);

		// Helper lambda: create a styled tab button with a text label.
		auto MakeTabButton = [&](const FName& InName, const FString& Label) -> UButton*
		{
			UButton* Btn = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), InName);

			// Keep default brushes (ensures hit-test area) but tint them.
			FButtonStyle BtnStyle = Btn->GetStyle();
			BtnStyle.Normal.TintColor  = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.01f));
			BtnStyle.Hovered.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.15f));
			BtnStyle.Pressed.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 0.25f));
			Btn->SetStyle(BtnStyle);
			// Text child
			UTextBlock* BtnLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(*(InName.ToString() + TEXT("Label"))));
			BtnLabel->SetText(FText::FromString(Label));
			BtnLabel->SetFont(LabelFont);
			BtnLabel->SetColorAndOpacity(FSlateColor(White));
			BtnLabel->SetJustification(ETextJustify::Center);
			Btn->AddChild(BtnLabel);

			UHorizontalBoxSlot* BtnSlot = BottomNavBar->AddChildToHorizontalBox(Btn);
			BtnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BtnSlot->SetHorizontalAlignment(HAlign_Fill);
			BtnSlot->SetVerticalAlignment(VAlign_Center);

			return Btn;
		};

		CreateTabButton  = MakeTabButton(TEXT("CreateTabButton"),  TEXT("Create"));
		RoomsTabButton   = MakeTabButton(TEXT("RoomsTabButton"),   TEXT("Rooms"));
		FollowTabButton  = MakeTabButton(TEXT("FollowTabButton"),  TEXT("Follow"));
		GamesTabButton   = MakeTabButton(TEXT("GamesTabButton"),   TEXT("Games"));
		ProfileTabButton = MakeTabButton(TEXT("ProfileTabButton"), TEXT("Me"));

		UE_LOG(LogPHCMainHUD, Log, TEXT("Widget tree built programmatically in Initialize()."));
	}

	return true;
}

// ===================================================================

void UPHCMainHUD::NativeConstruct()
{
	Super::NativeConstruct();

	// --- Bind tab button clicks ---
	if (CreateTabButton)
	{
		CreateTabButton->OnClicked.AddDynamic(this, &UPHCMainHUD::OnCreateTabClicked);
	}
	if (RoomsTabButton)
	{
		RoomsTabButton->OnClicked.AddDynamic(this, &UPHCMainHUD::OnRoomsTabClicked);
	}
	if (FollowTabButton)
	{
		FollowTabButton->OnClicked.AddDynamic(this, &UPHCMainHUD::OnFollowTabClicked);
	}
	if (GamesTabButton)
	{
		GamesTabButton->OnClicked.AddDynamic(this, &UPHCMainHUD::OnGamesTabClicked);
	}
	if (ProfileTabButton)
	{
		ProfileTabButton->OnClicked.AddDynamic(this, &UPHCMainHUD::OnProfileTabClicked);
	}

	// --- Create and insert panel widgets into the switcher ---
	CreatePanels();

	// --- Default to the Rooms tab ---
	SwitchToTab(TabRooms);

	UE_LOG(LogPHCMainHUD, Log, TEXT("MainHUD constructed. Default tab: Rooms"));
}

void UPHCMainHUD::NativeDestruct()
{
	// Unbind delegates to be safe (widgets may outlive expectations in edge cases).
	if (CreateTabButton)  { CreateTabButton->OnClicked.RemoveAll(this);  }
	if (RoomsTabButton)   { RoomsTabButton->OnClicked.RemoveAll(this);   }
	if (FollowTabButton)  { FollowTabButton->OnClicked.RemoveAll(this);  }
	if (GamesTabButton)   { GamesTabButton->OnClicked.RemoveAll(this);   }
	if (ProfileTabButton) { ProfileTabButton->OnClicked.RemoveAll(this); }

	Super::NativeDestruct();
}

// ===================================================================
//  Panel Creation
// ===================================================================

void UPHCMainHUD::CreatePanels()
{
	if (!ContentSwitcher)
	{
		UE_LOG(LogPHCMainHUD, Error, TEXT("ContentSwitcher is null - cannot create panels."));
		return;
	}

	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		UE_LOG(LogPHCMainHUD, Error, TEXT("No owning PlayerController - cannot create panel widgets."));
		return;
	}

	// Helper lambda: create a panel widget if the class is set and add it to the switcher.
	auto CreateAndAdd = [&](auto& OutPtr, const auto& WidgetClass, const TCHAR* DebugName) -> bool
	{
		if (!WidgetClass)
		{
			UE_LOG(LogPHCMainHUD, Warning, TEXT("Panel class not set for %s. Skipping."), DebugName);
			return false;
		}
		OutPtr = CreateWidget<typename std::remove_pointer_t<decltype(OutPtr.Get())>>(PC, WidgetClass);
		if (OutPtr)
		{
			ContentSwitcher->AddChild(OutPtr);
			UE_LOG(LogPHCMainHUD, Verbose, TEXT("Created panel: %s"), DebugName);
			return true;
		}
		UE_LOG(LogPHCMainHUD, Error, TEXT("Failed to create panel: %s"), DebugName);
		return false;
	};

	// Order MUST match tab index constants (0-4).
	CreateAndAdd(CreateRoomPanel, CreateRoomPanelClass, TEXT("CreateRoom"));
	CreateAndAdd(RoomsPanel,      RoomsPanelClass,      TEXT("Rooms"));
	CreateAndAdd(FollowPanel,     FollowPanelClass,     TEXT("Follow"));
	CreateAndAdd(GamesPanel,      GamesPanelClass,      TEXT("Games"));
	CreateAndAdd(ProfilePanel,    ProfilePanelClass,     TEXT("Profile"));
}

// ===================================================================
//  Tab Navigation
// ===================================================================

void UPHCMainHUD::SwitchToTab(int32 TabIndex)
{
	if (TabIndex < 0 || TabIndex >= NumTabs)
	{
		UE_LOG(LogPHCMainHUD, Warning, TEXT("SwitchToTab: invalid index %d"), TabIndex);
		return;
	}

	ActiveTabIndex = TabIndex;

	if (ContentSwitcher)
	{
		ContentSwitcher->SetActiveWidgetIndex(TabIndex);
	}

	UpdateTabButtonStyles();
	OnTabChanged.Broadcast(TabIndex);

	UE_LOG(LogPHCMainHUD, Verbose, TEXT("Switched to tab %d"), TabIndex);
}

void UPHCMainHUD::UpdateTabButtonStyles()
{
	// Iterate all tab buttons and apply active/inactive tint.
	for (int32 i = 0; i < NumTabs; ++i)
	{
		UButton* Btn = GetTabButton(i);
		if (!Btn)
		{
			continue;
		}

		const bool bActive = (i == ActiveTabIndex);

		// Active tab gets full opacity; inactive gets dimmed.
		FLinearColor Tint = bActive
			? FLinearColor(1.f, 1.f, 1.f, 1.f)
			: FLinearColor(0.6f, 0.6f, 0.6f, 0.8f);

		// Apply to all button style states.
		FButtonStyle Style = Btn->GetStyle();
		Style.Normal.TintColor  = FSlateColor(Tint);
		Style.Hovered.TintColor = FSlateColor(bActive ? Tint : FLinearColor(0.8f, 0.8f, 0.8f, 1.f));
		Style.Pressed.TintColor = FSlateColor(FLinearColor(1.f, 1.f, 1.f, 1.f));
		Btn->SetStyle(Style);
	}
}

UButton* UPHCMainHUD::GetTabButton(int32 Index) const
{
	switch (Index)
	{
	case TabCreate:  return CreateTabButton;
	case TabRooms:   return RoomsTabButton;
	case TabFollow:  return FollowTabButton;
	case TabGames:   return GamesTabButton;
	case TabProfile: return ProfileTabButton;
	default:         return nullptr;
	}
}

// ===================================================================
//  Button Callbacks
// ===================================================================

void UPHCMainHUD::OnCreateTabClicked()
{
	UE_LOG(LogPHCMainHUD, Log, TEXT("Tab clicked: Create"));
	SwitchToTab(TabCreate);
}

void UPHCMainHUD::OnRoomsTabClicked()
{
	UE_LOG(LogPHCMainHUD, Log, TEXT("Tab clicked: Rooms"));
	SwitchToTab(TabRooms);
}

void UPHCMainHUD::OnFollowTabClicked()
{
	UE_LOG(LogPHCMainHUD, Log, TEXT("Tab clicked: Follow"));
	SwitchToTab(TabFollow);
}

void UPHCMainHUD::OnGamesTabClicked()
{
	UE_LOG(LogPHCMainHUD, Log, TEXT("Tab clicked: Games"));
	SwitchToTab(TabGames);
}

void UPHCMainHUD::OnProfileTabClicked()
{
	SwitchToTab(TabProfile);
}

// ===================================================================
//  Economy Display
// ===================================================================

void UPHCMainHUD::UpdateEconomyDisplay(int32 Coins, int32 Gems)
{
	if (CoinDisplay)
	{
		CoinDisplay->SetText(FText::FromString(FString::Printf(TEXT("%d"), Coins)));
	}
	if (GemDisplay)
	{
		GemDisplay->SetText(FText::FromString(FString::Printf(TEXT("%d"), Gems)));
	}
}

void UPHCMainHUD::SetUsernameDisplay(const FString& Username)
{
	if (UsernameDisplay)
	{
		UsernameDisplay->SetText(FText::FromString(Username));
	}
}

// ===================================================================
//  Navigation Helpers
// ===================================================================

void UPHCMainHUD::NavigateToRoom(int32 RoomId)
{
	UE_LOG(LogPHCMainHUD, Log, TEXT("NavigateToRoom: %d"), RoomId);

	// TODO: Phase 5 - create and show the in-room widget overlay.
	// For now we broadcast so game code can respond.
	// Example: GameInstance->JoinRoom(RoomId);
}

void UPHCMainHUD::NavigateToGame(EPHCGameType GameType)
{
	UE_LOG(LogPHCMainHUD, Log, TEXT("NavigateToGame: type %d"), static_cast<int32>(GameType));

	// Dismiss any existing overlay.
	DismissOverlay();

	// Launch the game via the PlayerController.
	APHCPlayerController* PC = Cast<APHCPlayerController>(GetOwningPlayer());
	if (PC)
	{
		PC->LaunchGame(GameType);
	}
	else
	{
		UE_LOG(LogPHCMainHUD, Error, TEXT("NavigateToGame: Could not find PHCPlayerController."));
	}
}

void UPHCMainHUD::ShowOverlayWidget(UUserWidget* Widget)
{
	if (!Widget)
	{
		return;
	}

	// Dismiss any existing overlay first.
	DismissOverlay();

	ActiveOverlay = Widget;
	ActiveOverlay->AddToViewport(100); // High Z-order so it sits above the HUD.
	UE_LOG(LogPHCMainHUD, Verbose, TEXT("Overlay shown: %s"), *Widget->GetName());
}

void UPHCMainHUD::DismissOverlay()
{
	if (ActiveOverlay)
	{
		ActiveOverlay->RemoveFromParent();
		ActiveOverlay = nullptr;
		UE_LOG(LogPHCMainHUD, Verbose, TEXT("Overlay dismissed"));
	}
}

void UPHCMainHUD::RefreshData()
{
	switch (ActiveTabIndex)
	{
	case TabCreate:
		// Create panel doesn't need a refresh per se; it's a form.
		break;
	case TabRooms:
		if (RoomsPanel) { RoomsPanel->RefreshRoomList(); }
		break;
	case TabFollow:
		if (FollowPanel) { FollowPanel->ShowFollowers(); }
		break;
	case TabGames:
		// Games panel is static content; no refresh needed.
		break;
	case TabProfile:
		if (ProfilePanel) { ProfilePanel->RefreshProfile(); }
		break;
	default:
		break;
	}
}
