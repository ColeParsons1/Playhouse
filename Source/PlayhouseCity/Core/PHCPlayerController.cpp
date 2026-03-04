// Copyright PlayhouseCity. All Rights Reserved.

#include "Core/PHCPlayerController.h"
#include "Core/PHCGameInstance.h"
#include "Auth/PHCAuthManager.h"
#include "Core/PHCTypes.h"
#include "Auth/PHCAuthWidget.h"
#include "UI/PHCMainHUD.h"
#include "UI/PHCCityHUD.h"
#include "Games/Base/PHCMiniGameMode.h"
#include "Games/Base/PHCMiniGameHUD.h"
#include "PlayhouseCity.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"

// Game mode includes for the class mapping.
#include "Games/Bingo/BingoGameMode.h"
#include "Games/ColorCards/ColorCardsGameMode.h"
#include "Games/LuckySlots/SlotsGameMode.h"
#include "Games/Connect4/Connect4GameMode.h"
#include "Games/Ludo/LudoGameMode.h"
#include "Games/PegGame/PegGameMode.h"
#include "Games/GuessSign/GuessSignGameMode.h"
#include "Games/MapsQuiz/MapsGameMode.h"
#include "Games/Pong/PongGameMode.h"
#include "Games/PackCatch/PackCatchGameMode.h"
#include "Games/BabyShark/BabySharkGameMode.h"
#include "Games/TempleRun/TempleRunGameMode.h"
#include "Games/TowerDefense/TDGameMode.h"
#include "Games/KitchenChaos/KitchenGameMode.h"
#include "Games/Pool/PoolGameMode.h"
#include "Games/Carrom/CarromGameMode.h"
#include "Games/Tanks/TanksGameMode.h"
#include "Games/AngryDucks/AngryDucksGameMode.h"
#include "Games/GooBridge/GooBridgeGameMode.h"
#include "Games/Jenga/JengaGameMode.h"
#include "Games/Cornhole/CornholeGameMode.h"
#include "Games/CelDamage/CelDamageGameMode.h"
#include "Games/Simulator/SimulatorGameMode.h"
#include "Core/PHCCitySubsystem.h"
#include "Vehicle/PHCVehiclePawn.h"
#include "Vehicle/PHCDuneBuggyPawn.h"
#include "Vehicle/PHCRaptorPawn.h"
#include "Vehicle/PHCTankPawn.h"
#include "Vehicle/PHCDirtBikePawn.h"
#include "Vehicle/PHCBlackbirdPawn.h"
#include "Vehicle/PHCBlackHawkPawn.h"
#include "Player/PHCCityCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Core/PHCCityGameMode.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

APHCPlayerController::APHCPlayerController()
{
	bShowMouseCursor = true;

	// Wire up C++ widget classes as defaults.
	AuthWidgetClass = UPHCAuthWidget::StaticClass();
	MainHUDWidgetClass = UPHCMainHUD::StaticClass();
	CityHUDWidgetClass = UPHCCityHUD::StaticClass();
}

// ---------------------------------------------------------------------------
// AActor overrides
// ---------------------------------------------------------------------------

void APHCPlayerController::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::BeginPlay"));

	// On a dedicated server, this controller is a server-side proxy for a remote
	// player — skip all UI/input setup since there's no local viewport.
	if (!IsLocalController())
	{
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::BeginPlay -- Server-side controller, skipping UI setup."));
		return;
	}

	// Ensure the cursor is visible and input is in UI mode.
	SetUIOnlyInputMode();

	// Bind to GameInstance delegates so we react to login / logout from anywhere.
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (GI)
	{
		GI->OnLoginSuccess.AddDynamic(this, &APHCPlayerController::OnLoginSuccess);
		GI->OnLogoutComplete.AddDynamic(this, &APHCPlayerController::OnLogoutComplete);

		// Route to the appropriate screen based on current auth state.
		if (GI->bIsLoggedIn)
		{
			// If we have a possessed pawn, we're in the 3D city world.
			if (GetPawn() != nullptr)
			{
				UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController -- Already logged in with pawn, entering world mode."));
				EnterWorldMode();
			}
			else
			{
				UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController -- Already logged in, showing main HUD."));
				ShowMainHUD();
			}
		}
		else
		{
			// Try auto-login with stored Apple credentials.
			UPHCAuthManager* AuthManager = GI->GetAuthManager();
			if (AuthManager && AuthManager->AutoLoginWithApple())
			{
				UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController -- Attempting auto-login with stored Apple credentials."));
			}
			else
			{
				UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController -- No stored credentials, showing auth screen."));
				ShowAuthScreen();
			}
		}
	}
	else
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCPlayerController::BeginPlay -- Could not retrieve PHCGameInstance. Defaulting to auth screen."));
		ShowAuthScreen();
	}
}

void APHCPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unbind delegates to avoid dangling references.
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (GI)
	{
		GI->OnLoginSuccess.RemoveDynamic(this, &APHCPlayerController::OnLoginSuccess);
		GI->OnLogoutComplete.RemoveDynamic(this, &APHCPlayerController::OnLogoutComplete);
	}

	ClearActiveWidget();

	Super::EndPlay(EndPlayReason);
}

void APHCPlayerController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	APawn* CurrentPawn = GetPawn();

	// -- Track vehicle position while driving --
	// Move the hidden character to follow the vehicle every tick.  This way
	// when the server Possess()es the character back, it's ALREADY at the
	// vehicle's position — no need to set it after the fact.
	if (bWasDrivingVehicle && CurrentPawn && !Cast<APHCCityCharacter>(CurrentPawn))
	{
		LastVehiclePosition = CurrentPawn->GetActorLocation();
		if (SavedCharacterPawn)
		{
			SavedCharacterPawn->SetActorLocation(CurrentPawn->GetActorLocation());
		}
	}

	// -- Detect vehicle→character pawn transition --
	// When the server Possess()es our character back, the pawn changes from
	// vehicle to character. Detect this and handle ALL exit logic locally,
	// without relying on the ClientOnVehicleExited RPC (which may fail due
	// to parameter mismatch with an older server build).
	if (bWasDrivingVehicle && CurrentPawn && Cast<APHCCityCharacter>(CurrentPawn))
	{
		HandleLocalVehicleExit();
	}

	// -- Enforce exit position for several frames --
	if (ExitTeleportFrames > 0)
	{
		if (CurrentPawn)
		{
			CurrentPawn->SetActorLocation(PendingExitLocation);
		}
		--ExitTeleportFrames;
	}
}

// ---------------------------------------------------------------------------
// Screen management
// ---------------------------------------------------------------------------

void APHCPlayerController::ShowAuthScreen()
{
	ClearActiveWidget();

	if (!AuthWidgetClass)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCPlayerController::ShowAuthScreen -- AuthWidgetClass is not set. Assign it in the Blueprint defaults."));
		return;
	}

	ActiveWidget = CreateWidget<UUserWidget>(this, AuthWidgetClass);
	if (ActiveWidget)
	{
		ActiveWidget->AddToViewport(0);
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::ShowAuthScreen -- Auth widget added to viewport."));
	}

	SetUIOnlyInputMode();
}

void APHCPlayerController::ShowMainHUD()
{
	ClearActiveWidget();

	if (!MainHUDWidgetClass)
	{
		UE_LOG(LogPlayhouseCity, Warning,
			TEXT("PHCPlayerController::ShowMainHUD -- MainHUDWidgetClass is not set. Assign it in the Blueprint defaults."));
		return;
	}

	ActiveWidget = CreateWidget<UUserWidget>(this, MainHUDWidgetClass);
	if (ActiveWidget)
	{
		ActiveWidget->AddToViewport(0);
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::ShowMainHUD -- Main HUD widget added to viewport."));
	}

	SetUIOnlyInputMode();
}

// ---------------------------------------------------------------------------
// World mode
// ---------------------------------------------------------------------------

void APHCPlayerController::EnterWorldMode()
{
	ClearActiveWidget();
	bIsInWorldMode = true;

	if (CityHUDWidgetClass)
	{
		ActiveWidget = CreateWidget<UUserWidget>(this, CityHUDWidgetClass);
		if (ActiveWidget)
		{
			ActiveWidget->AddToViewport(0);
			UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::EnterWorldMode -- City HUD added to viewport."));

			// Seed currency display from player profile
			if (UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this))
			{
				if (UPHCCityHUD* CityHUD = Cast<UPHCCityHUD>(ActiveWidget))
				{
					CityHUD->UpdateCurrencyDisplay(
						GI->LocalPlayerProfile.Coins,
						GI->LocalPlayerProfile.Gems);
				}
			}
		}
	}

	SetGameAndUIInputMode();

	// Auto-connect to the dedicated server.
	if (UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this))
	{
		if (!GI->bInMultiplayerSession)
		{
			GI->JoinGame(TEXT("GAME_SERVER_IP"));
		}
	}
}

void APHCPlayerController::EnterMenuMode()
{
	bIsInWorldMode = false;
	SetUIOnlyInputMode();
}

// ---------------------------------------------------------------------------
// Game launch
// ---------------------------------------------------------------------------

void APHCPlayerController::LaunchGame(EPHCGameType GameType)
{
	const UEnum* GameEnum = StaticEnum<EPHCGameType>();
	const FString GameName = GameEnum ? GameEnum->GetDisplayNameTextByValue(static_cast<int64>(GameType)).ToString() : TEXT("Unknown");
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::LaunchGame -- Launching '%s' (type %d)"), *GameName, static_cast<int32>(GameType));

	// Get the game mode class for this game type.
	TSubclassOf<APHCMiniGameMode> GameModeClass = GetGameModeClassForType(GameType);
	if (!GameModeClass)
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("PHCPlayerController::LaunchGame -- No game mode class for type %d"), static_cast<int32>(GameType));
		return;
	}

	// If we're in the 3D world, hide the character during the mini-game.
	if (APawn* MyPawn = GetPawn())
	{
		MyPawn->SetActorHiddenInGame(true);
		MyPawn->SetActorEnableCollision(false);
		DisableInput(this);
	}

	// Remove the lobby HUD.
	ClearActiveWidget();

	// Mark that we're in a mini-game.
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (GI)
	{
		GI->bInMiniGame = true;
	}

	// Ensure the world is unpaused (in case a previous game left it paused).
	UWorld* World = GetWorld();
	if (World)
	{
		UGameplayStatics::SetGamePaused(World, false);
	}

	// Spawn the game mode as a regular actor in the current world.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ActiveMiniGame = World ? World->SpawnActor<APHCMiniGameMode>(GameModeClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParams) : nullptr;

	if (!ActiveMiniGame)
	{
		UE_LOG(LogPlayhouseCity, Error, TEXT("PHCPlayerController::LaunchGame -- Failed to spawn game mode actor."));
		ShowMainHUD();
		return;
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::LaunchGame -- Game mode spawned: %s"), *ActiveMiniGame->GetName());

	// Create the game HUD widget (use game-specific HUD class if set).
	TSubclassOf<UPHCMiniGameHUD> HUDClass = ActiveMiniGame->GameHUDClass;
	if (!HUDClass)
	{
		HUDClass = UPHCMiniGameHUD::StaticClass();
	}
	ActiveGameHUD = CreateWidget<UPHCMiniGameHUD>(this, HUDClass);
	if (ActiveGameHUD)
	{
		// SetGameMode BEFORE AddToViewport so NativeConstruct can access GameModeRef.
		ActiveGameHUD->SetGameMode(ActiveMiniGame);
		ActiveGameHUD->AddToViewport(0);
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::LaunchGame -- Game HUD (%s) added to viewport."), *HUDClass->GetName());
	}

	SetUIOnlyInputMode();

	// Start the game.
	ActiveMiniGame->StartGame();

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::LaunchGame -- Game started!"));
}

void APHCPlayerController::ReturnFromGame()
{
	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::ReturnFromGame"));

	// Ensure the world is unpaused.
	UWorld* World = GetWorld();
	if (World)
	{
		UGameplayStatics::SetGamePaused(World, false);
	}

	// Remove game HUD.
	if (ActiveGameHUD)
	{
		ActiveGameHUD->RemoveFromParent();
		ActiveGameHUD = nullptr;
	}

	// Destroy game mode actor.
	if (ActiveMiniGame)
	{
		if (ActiveMiniGame->bGameActive)
		{
			ActiveMiniGame->EndGame(/*bWon=*/ false);
		}
		ActiveMiniGame->Destroy();
		ActiveMiniGame = nullptr;
	}

	// Mark that we're no longer in a mini-game.
	UPHCGameInstance* GI = UPHCGameInstance::GetPHCGameInstance(this);
	if (GI)
	{
		GI->bInMiniGame = false;
	}

	// Re-show the character if we're in the 3D world.
	if (APawn* MyPawn = GetPawn())
	{
		MyPawn->SetActorHiddenInGame(false);
		MyPawn->SetActorEnableCollision(true);
		EnableInput(this);
		EnterWorldMode();
	}
	else
	{
		// Legacy UI-only path.
		ShowMainHUD();
	}
}

// ---------------------------------------------------------------------------
// Game mode class mapping
// ---------------------------------------------------------------------------

TSubclassOf<APHCMiniGameMode> APHCPlayerController::GetGameModeClassForType(EPHCGameType GameType) const
{
	switch (GameType)
	{
	case EPHCGameType::Bingo:         return ABingoGameMode::StaticClass();
	case EPHCGameType::ColorCards:    return AColorCardsGameMode::StaticClass();
	case EPHCGameType::LuckySlots:   return ASlotsGameMode::StaticClass();
	case EPHCGameType::Connect4:     return AConnect4GameMode::StaticClass();
	case EPHCGameType::Ludo:         return ALudoGameMode::StaticClass();
	case EPHCGameType::PegGame:      return APegGameMode::StaticClass();
	case EPHCGameType::GuessSign:    return AGuessSignGameMode::StaticClass();
	case EPHCGameType::MapsQuiz:     return AMapsGameMode::StaticClass();
	case EPHCGameType::Pong:         return APongGameMode::StaticClass();
	case EPHCGameType::PackCatch:    return APackCatchGameMode::StaticClass();
	case EPHCGameType::BabyShark:    return ABabySharkGameMode::StaticClass();
	case EPHCGameType::TempleRun:    return ATempleRunGameMode::StaticClass();
	case EPHCGameType::TowerDefense: return ATDGameMode::StaticClass();
	case EPHCGameType::KitchenChaos: return AKitchenGameMode::StaticClass();
	case EPHCGameType::Pool:         return APoolGameMode::StaticClass();
	case EPHCGameType::Carrom:       return ACarromGameMode::StaticClass();
	case EPHCGameType::Tanks:        return ATanksGameMode::StaticClass();
	case EPHCGameType::AngryDucks:   return AAngryDucksGameMode::StaticClass();
	case EPHCGameType::GooBridge:    return AGooBridgeGameMode::StaticClass();
	case EPHCGameType::Jenga:        return AJengaGameMode::StaticClass();
	case EPHCGameType::Cornhole:     return ACornholeGameMode::StaticClass();
	case EPHCGameType::CelDamage:    return ACelDamageGameMode::StaticClass();
	case EPHCGameType::Simulator:    return ASimulatorGameMode::StaticClass();
	default:                         return nullptr;
	}
}

// ---------------------------------------------------------------------------
// Delegate callbacks
// ---------------------------------------------------------------------------

void APHCPlayerController::OnLoginSuccess(const FPHCPlayerProfile& PlayerProfile)
{
	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCPlayerController::OnLoginSuccess -- Login succeeded for '%s'."),
		*PlayerProfile.Username);

	// If we have a possessed pawn, we're in the 3D city world.
	if (GetPawn() != nullptr)
	{
		EnterWorldMode();
	}
	else
	{
		ShowMainHUD();
	}
}

void APHCPlayerController::OnLogoutComplete()
{
	UE_LOG(LogPlayhouseCity, Log,
		TEXT("PHCPlayerController::OnLogoutComplete -- Switching to auth screen."));
	ShowAuthScreen();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void APHCPlayerController::ClearActiveWidget()
{
	if (ActiveWidget)
	{
		ActiveWidget->RemoveFromParent();
		ActiveWidget = nullptr;
	}
}

void APHCPlayerController::SetGameAndUIInputMode()
{
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
	SetInputMode(InputMode);

#if PLATFORM_IOS || PLATFORM_ANDROID
	bShowMouseCursor = false;
#else
	bShowMouseCursor = true;
#endif
}

// ---------------------------------------------------------------------------
// Vehicle interaction
// ---------------------------------------------------------------------------

void APHCPlayerController::EnterNearestVehicle()
{
	APHCCityCharacter* Character = Cast<APHCCityCharacter>(GetPawn());
	if (!Character) return;

	// Find nearest unoccupied vehicle within range (any type).
	constexpr float EnterRange = 2000.f;
	const FVector CharLoc = Character->GetActorLocation();
	UWorld* World = GetWorld();

	APawn* NearestPawn = nullptr;
	float NearestDist = EnterRange;

	auto TryNearest = [&](APawn* V, bool bOccupied)
	{
		if (!V || bOccupied) return;
		const float Dist = FVector::Dist(CharLoc, V->GetActorLocation());
		if (Dist < NearestDist) { NearestDist = Dist; NearestPawn = V; }
	};

	for (TActorIterator<APHCVehiclePawn> It(World); It; ++It) { TryNearest(*It, It->bIsOccupied); }
	for (TActorIterator<APHCDuneBuggyPawn> It(World); It; ++It) { TryNearest(*It, It->bIsOccupied); }
	for (TActorIterator<APHCRaptorPawn> It(World); It; ++It) { TryNearest(*It, It->bIsOccupied); }
	for (TActorIterator<APHCTankPawn> It(World); It; ++It) { TryNearest(*It, It->bIsOccupied); }
	for (TActorIterator<APHCDirtBikePawn> It(World); It; ++It) { TryNearest(*It, It->bIsOccupied); }
	for (TActorIterator<APHCBlackbirdPawn> It(World); It; ++It) { TryNearest(*It, It->bIsOccupied); }
	for (TActorIterator<APHCBlackHawkPawn> It(World); It; ++It) { TryNearest(*It, It->bIsOccupied); }

	if (!NearestPawn)
	{
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::EnterNearestVehicle -- No vehicle in range."));
		return;
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::EnterNearestVehicle -- Entering vehicle at %s"),
		*NearestPawn->GetActorLocation().ToString());

	// Save character reference and hide it.
	SavedCharacterPawn = Character;
	CurrentVehicle = NearestPawn;
	bWasDrivingVehicle = true;

	// Remove character's mapping context.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (Character->DefaultMappingContext)
		{
			Subsystem->RemoveMappingContext(Character->DefaultMappingContext);
		}
	}

	// Hide character (dirt bike keeps it visible as rider).
	Character->SetActorEnableCollision(false);
	const bool bIsDirtBike = Cast<APHCDirtBikePawn>(NearestPawn) != nullptr;
	Character->SetActorHiddenInGame(!bIsDirtBike);

	// Vehicle-specific entry setup.
	if (APHCVehiclePawn* V = Cast<APHCVehiclePawn>(NearestPawn))
	{
		V->bIsOccupied = true;
		V->CurrentSpeed = 0.f;
		V->SetHeadlightsEnabled(true);
	}
	else if (APHCDuneBuggyPawn* B = Cast<APHCDuneBuggyPawn>(NearestPawn))
	{
		B->bIsOccupied = true;
		B->CurrentSpeed = 0.f;
		B->SetHeadlightsEnabled(true);
	}
	else if (APHCRaptorPawn* R = Cast<APHCRaptorPawn>(NearestPawn))
	{
		R->bIsOccupied = true;
		R->CurrentSpeed = 0.f;
	}
	else if (APHCTankPawn* T = Cast<APHCTankPawn>(NearestPawn))
	{
		T->bIsOccupied = true;
		T->CurrentSpeed = 0.f;
		T->SetHeadlightsEnabled(true);
	}
	else if (APHCDirtBikePawn* DB = Cast<APHCDirtBikePawn>(NearestPawn))
	{
		DB->bIsOccupied = true;
		DB->CurrentSpeed = 0.f;
		DB->SetHeadlightsEnabled(true);
	}
	else if (APHCBlackbirdPawn* BB = Cast<APHCBlackbirdPawn>(NearestPawn))
	{
		BB->bIsOccupied = true;
		BB->CurrentSpeed = 0.f;
	}
	else if (APHCBlackHawkPawn* BH = Cast<APHCBlackHawkPawn>(NearestPawn))
	{
		BH->bIsOccupied = true;
		BH->CurrentSpeed = 0.f;
	}

	UnPossess();
	Possess(NearestPawn);

	// Dirt bike: attach character as visible rider.
	if (bIsDirtBike)
	{
		Character->AttachToActor(NearestPawn, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Character->SetActorRelativeLocation(FVector(10.f, 0.f, 30.f));
		Character->SetActorRelativeRotation(FRotator::ZeroRotator);
	}

	// Tell the city HUD to switch to driving controls.
	if (UPHCCityHUD* CityHUD = Cast<UPHCCityHUD>(ActiveWidget))
	{
		CityHUD->SetDrivingMode(true, NearestPawn);
	}

	SetGameAndUIInputMode();
}

// ---------------------------------------------------------------------------

void APHCPlayerController::SpawnAndEnterVehicle(int32 VehicleType)
{
	APHCCityCharacter* Character = Cast<APHCCityCharacter>(GetPawn());
	if (!Character) return;

	// Save character reference on this machine (needed for exit flow).
	SavedCharacterPawn = Character;
	bWasDrivingVehicle = true;

	if (!HasAuthority())
	{
		// Client: immediately switch HUD to driving mode so controls appear.
		// DrivenVehicle will be filled in by auto-detection when pawn changes.
		if (UPHCCityHUD* CityHUD = Cast<UPHCCityHUD>(ActiveWidget))
		{
			CityHUD->bPendingVehicleEntry = true;
			CityHUD->SetDrivingMode(true, nullptr);
		}
		ServerSpawnAndEnterVehicle(VehicleType);
		return;
	}

	// ---- Server-side: spawn and possess ----
	UWorld* World = GetWorld();
	if (!World) return;

	const FVector SpawnLoc = Character->GetActorLocation() + Character->GetActorForwardVector() * 300.f + FVector(0.f, 0.f, 50.f);
	const FRotator SpawnRot(0.f, Character->GetActorRotation().Yaw, 0.f);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APawn* SpawnedVehicle = nullptr;

	if (VehicleType == 2)
	{
		APHCRaptorPawn* Raptor = World->SpawnActor<APHCRaptorPawn>(
			APHCRaptorPawn::StaticClass(), SpawnLoc, SpawnRot, SP);
		if (Raptor)
		{
			Raptor->SetBodyColor(FLinearColor(0.35f, 0.37f, 0.38f));
			Raptor->bIsOccupied = true;
			Raptor->CurrentSpeed = 0.f;
			SpawnedVehicle = Raptor;
		}
	}
	else if (VehicleType == 3)
	{
		APHCTankPawn* Tank = World->SpawnActor<APHCTankPawn>(
			APHCTankPawn::StaticClass(), SpawnLoc, SpawnRot, SP);
		if (Tank)
		{
			Tank->SetBodyColor(FLinearColor(0.22f, 0.24f, 0.14f));
			Tank->bIsOccupied = true;
			Tank->CurrentSpeed = 0.f;
			Tank->SetHeadlightsEnabled(true);
			SpawnedVehicle = Tank;
		}
	}
	else if (VehicleType == 4)
	{
		APHCDirtBikePawn* Bike = World->SpawnActor<APHCDirtBikePawn>(
			APHCDirtBikePawn::StaticClass(), SpawnLoc, SpawnRot, SP);
		if (Bike)
		{
			Bike->SetBodyColor(FLinearColor(0.75f, 0.12f, 0.05f));
			Bike->bIsOccupied = true;
			Bike->CurrentSpeed = 0.f;
			Bike->SetHeadlightsEnabled(true);
			SpawnedVehicle = Bike;
		}
	}
	else if (VehicleType == 5)
	{
		APHCBlackHawkPawn* Hawk = World->SpawnActor<APHCBlackHawkPawn>(
			APHCBlackHawkPawn::StaticClass(), SpawnLoc, SpawnRot, SP);
		if (Hawk)
		{
			Hawk->bIsOccupied = true;
			Hawk->CurrentSpeed = 0.f;
			SpawnedVehicle = Hawk;
		}
	}
	else if (VehicleType == 1)
	{
		APHCDuneBuggyPawn* Buggy = World->SpawnActor<APHCDuneBuggyPawn>(
			APHCDuneBuggyPawn::StaticClass(), SpawnLoc, SpawnRot, SP);
		if (Buggy)
		{
			Buggy->SetBodyColor(FLinearColor(0.35f, 0.42f, 0.18f));
			Buggy->bIsOccupied = true;
			Buggy->CurrentSpeed = 0.f;
			Buggy->SetHeadlightsEnabled(true);
			SpawnedVehicle = Buggy;
		}
	}
	else
	{
		APHCVehiclePawn* Aventador = World->SpawnActor<APHCVehiclePawn>(
			APHCVehiclePawn::StaticClass(), SpawnLoc, SpawnRot, SP);
		if (Aventador)
		{
			Aventador->SetBodyColor(FLinearColor(0.72f, 0.30f, 0.03f));
			Aventador->bIsOccupied = true;
			Aventador->CurrentSpeed = 0.f;
			Aventador->SetHeadlightsEnabled(true);

			if (APHCCityGameMode* CityGM = Cast<APHCCityGameMode>(World->GetAuthGameMode()))
			{
				CityGM->SpawnedVehicles.Add(Aventador);
			}
			SpawnedVehicle = Aventador;
		}
	}

	if (!SpawnedVehicle)
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("SpawnAndEnterVehicle: Failed to spawn vehicle (type %d)."), VehicleType);
		return;
	}

	// Server-side: hide character, swap possession.
	CurrentVehicle = SpawnedVehicle;
	Character->SetActorEnableCollision(false);

	if (VehicleType == 4)
	{
		// Dirt bike: attach character visibly to the bike as rider.
		Character->SetActorHiddenInGame(false);
		UnPossess();
		Possess(SpawnedVehicle);
		Character->AttachToActor(SpawnedVehicle, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		Character->SetActorRelativeLocation(FVector(10.f, 0.f, 30.f));
		Character->SetActorRelativeRotation(FRotator::ZeroRotator);
	}
	else
	{
		Character->SetActorHiddenInGame(true);
		UnPossess();
		Possess(SpawnedVehicle);
	}

	// Tell owning client to update HUD.
	ClientOnVehicleEntered(SpawnedVehicle);

	UE_LOG(LogPlayhouseCity, Log, TEXT("SpawnAndEnterVehicle: Spawned vehicle type %d at %s"),
		VehicleType, *SpawnedVehicle->GetActorLocation().ToString());
}

void APHCPlayerController::EquipJetpack()
{
	APHCCityCharacter* Char = Cast<APHCCityCharacter>(GetPawn());
	if (Char) Char->EquipJetpack();
}

void APHCPlayerController::UnequipJetpack()
{
	APHCCityCharacter* Char = Cast<APHCCityCharacter>(GetPawn());
	if (Char) Char->UnequipJetpack();
}

void APHCPlayerController::OnVehicleExitRequested()
{
	if (!HasAuthority())
	{
		// Client: LastVehiclePosition is already up-to-date from Tick().
		// The server's vehicle position may be wrong (Unreliable input RPCs),
		// but we'll use our stored position in ClientOnVehicleExited.
		ServerExitVehicle();
		return;
	}

	if (!CurrentVehicle || !SavedCharacterPawn) return;

	APawn* DrivenPawn = GetPawn();
	if (!DrivenPawn) DrivenPawn = CurrentVehicle;

	// Prefer LastVehiclePosition (updated every tick from client-side prediction)
	// over GetActorLocation() which may be stale on the server.
	FVector VehicleLoc = (!LastVehiclePosition.IsZero())
		? LastVehiclePosition
		: DrivenPawn->GetActorLocation();

	UE_LOG(LogPlayhouseCity, Log, TEXT("OnVehicleExitRequested -- VehicleLoc=%s (LastVehicle=%s, DrivenPawn=%s)"),
		*VehicleLoc.ToString(), *LastVehiclePosition.ToString(),
		*DrivenPawn->GetActorLocation().ToString());

	FVector ExitLoc = VehicleLoc + FVector(0.f, -300.f, 100.f);

	// Helper: clear vehicle state on whichever reference is valid.
	auto ClearVehicleState = [&](APawn* VPawn)
	{
		if (APHCVehiclePawn* V = Cast<APHCVehiclePawn>(VPawn))
		{
			V->bIsOccupied = false;
			V->ThrottleInput = 0.f;
			V->SteeringInput = 0.f;
			V->CurrentSpeed = 0.f;
			V->SetHeadlightsEnabled(false);
		}
		else if (APHCDuneBuggyPawn* B = Cast<APHCDuneBuggyPawn>(VPawn))
		{
			B->bIsOccupied = false;
			B->ThrottleInput = 0.f;
			B->SteeringInput = 0.f;
			B->CurrentSpeed = 0.f;
			B->SetHeadlightsEnabled(false);
		}
		else if (APHCRaptorPawn* R = Cast<APHCRaptorPawn>(VPawn))
		{
			R->bIsOccupied = false;
			R->ThrottleInput = 0.f;
			R->SteeringInput = 0.f;
			R->PitchInput = 0.f;
			R->CurrentSpeed = 0.f;
		}
		else if (APHCTankPawn* T = Cast<APHCTankPawn>(VPawn))
		{
			T->bIsOccupied = false;
			T->ThrottleInput = 0.f;
			T->SteeringInput = 0.f;
			T->CurrentSpeed = 0.f;
			T->SetHeadlightsEnabled(false);
		}
		else if (APHCDirtBikePawn* DB = Cast<APHCDirtBikePawn>(VPawn))
		{
			DB->bIsOccupied = false;
			DB->ThrottleInput = 0.f;
			DB->SteeringInput = 0.f;
			DB->CurrentSpeed = 0.f;
			DB->SetHeadlightsEnabled(false);
			SavedCharacterPawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		}
		else if (APHCBlackbirdPawn* BB = Cast<APHCBlackbirdPawn>(VPawn))
		{
			BB->bIsOccupied = false;
			BB->ThrottleInput = 0.f;
			BB->SteeringInput = 0.f;
			BB->PitchInput = 0.f;
			BB->CurrentSpeed = 0.f;
		}
		else if (APHCBlackHawkPawn* BH = Cast<APHCBlackHawkPawn>(VPawn))
		{
			BH->bIsOccupied = false;
			BH->ThrottleInput = 0.f;
			BH->SteeringInput = 0.f;
			BH->PitchInput = 0.f;
			BH->CurrentSpeed = 0.f;
		}
	};

	// Clear state on both references (they may be the same or different pawns).
	ClearVehicleState(DrivenPawn);
	if (CurrentVehicle != DrivenPawn)
		ClearVehicleState(CurrentVehicle);

	// Move character to exit location BEFORE repossessing — Possess() snapshots
	// the pawn's position for replication, so it must already be at ExitLoc.
	SavedCharacterPawn->SetActorLocation(ExitLoc);
	SavedCharacterPawn->SetActorHiddenInGame(false);
	SavedCharacterPawn->SetActorEnableCollision(true);

	// Unpossess vehicle, repossess character (server authority).
	UnPossess();
	Possess(SavedCharacterPawn);

	// Tell owning client to update HUD and set character position.
	ClientOnVehicleExited(ExitLoc);

	CurrentVehicle = nullptr;
	LastVehiclePosition = FVector::ZeroVector;
}

void APHCPlayerController::ServerSpawnAndEnterVehicle_Implementation(int32 VehicleType)
{
	SpawnAndEnterVehicle(VehicleType);
}

void APHCPlayerController::ClientOnVehicleEntered_Implementation(APawn* NewVehicle)
{
	// Local: swap input contexts and update HUD.
	CurrentVehicle = NewVehicle;

	if (SavedCharacterPawn)
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
		{
			if (SavedCharacterPawn->DefaultMappingContext)
			{
				Subsystem->RemoveMappingContext(SavedCharacterPawn->DefaultMappingContext);
			}
		}

		// Disable character collision on client (server already does this, but it doesn't replicate).
		SavedCharacterPawn->SetActorEnableCollision(false);

		// Dirt bike: make character body visible as rider (even in first person).
		if (Cast<APHCDirtBikePawn>(NewVehicle))
		{
			TArray<UStaticMeshComponent*> Meshes;
			SavedCharacterPawn->GetComponents<UStaticMeshComponent>(Meshes);
			for (UStaticMeshComponent* M : Meshes)
			{
				if (M) M->SetVisibility(true);
			}
		}
	}

	if (UPHCCityHUD* CityHUD = Cast<UPHCCityHUD>(ActiveWidget))
	{
		CityHUD->SetDrivingMode(true, NewVehicle);
	}

	SetGameAndUIInputMode();
}

void APHCPlayerController::ServerExitVehicle_Implementation()
{
	OnVehicleExitRequested();
}

void APHCPlayerController::ServerSetVehicleInput_Implementation(float Throttle, float Steering, float Pitch)
{
	APawn* P = GetPawn();
	if (!P)
	{
		UE_LOG(LogPlayhouseCity, Warning, TEXT("ServerSetVehicleInput: GetPawn() is NULL!"));
		return;
	}
	if (FMath::Abs(Throttle) > 0.01f)
	{
		UE_LOG(LogPlayhouseCity, Log, TEXT("ServerSetVehicleInput: T=%.2f S=%.2f Pawn=%s"), Throttle, Steering, *P->GetClass()->GetName());
	}

	if (APHCVehiclePawn* V = Cast<APHCVehiclePawn>(P))
	{
		V->ThrottleInput = Throttle;
		V->SteeringInput = Steering;
	}
	else if (APHCDuneBuggyPawn* B = Cast<APHCDuneBuggyPawn>(P))
	{
		B->ThrottleInput = Throttle;
		B->SteeringInput = Steering;
	}
	else if (APHCRaptorPawn* R = Cast<APHCRaptorPawn>(P))
	{
		R->ThrottleInput = Throttle;
		R->SteeringInput = Steering;
		R->PitchInput = Pitch;
	}
	else if (APHCTankPawn* T = Cast<APHCTankPawn>(P))
	{
		T->ThrottleInput = Throttle;
		T->SteeringInput = Steering;
		T->TurretYawInput = Pitch;
	}
	else if (APHCDirtBikePawn* DB = Cast<APHCDirtBikePawn>(P))
	{
		DB->ThrottleInput = Throttle;
		DB->SteeringInput = Steering;
	}
	else if (APHCBlackbirdPawn* BB = Cast<APHCBlackbirdPawn>(P))
	{
		BB->ThrottleInput = Throttle;
		BB->SteeringInput = Steering;
		BB->PitchInput = Pitch;
	}
	else if (APHCBlackHawkPawn* BH = Cast<APHCBlackHawkPawn>(P))
	{
		BH->ThrottleInput = Throttle;
		BH->SteeringInput = Steering;
		BH->PitchInput = Pitch;
	}
}

void APHCPlayerController::HandleLocalVehicleExit()
{
	// This runs on the CLIENT when Tick detects the pawn changed from vehicle
	// back to character.  It handles ALL exit cleanup without relying on the
	// ClientOnVehicleExited RPC (which may not fire if the server has a
	// different RPC signature).

	bWasDrivingVehicle = false;

	// Compute exit location from the last known vehicle position.
	FVector ExitLoc = FVector::ZeroVector;
	if (!LastVehiclePosition.IsZero())
	{
		ExitLoc = LastVehiclePosition + FVector(0.f, -300.f, 100.f);
	}

	APHCCityCharacter* Character = Cast<APHCCityCharacter>(GetPawn());

	// Set character position and make visible.
	if (Character && !ExitLoc.IsZero())
	{
		Character->SetActorLocation(ExitLoc);
		Character->SetActorHiddenInGame(false);
		Character->SetActorEnableCollision(true);

		// Enforce position for ~60 frames (~1 second) to override server
		// replication until ServerUpdateMovement corrects the server.
		PendingExitLocation = ExitLoc;
		ExitTeleportFrames = 60;
	}

	// Swap input contexts: remove vehicle, add character.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		// Try to remove whichever vehicle mapping context is active.
		auto TryRemoveVehicleMC = [&](APawn* VPawn)
		{
			UInputMappingContext* MC = nullptr;
			if (APHCVehiclePawn* V = Cast<APHCVehiclePawn>(VPawn)) MC = V->VehicleMappingContext;
			else if (APHCDuneBuggyPawn* B = Cast<APHCDuneBuggyPawn>(VPawn)) MC = B->VehicleMappingContext;
			else if (APHCRaptorPawn* R = Cast<APHCRaptorPawn>(VPawn)) MC = R->VehicleMappingContext;
			else if (APHCTankPawn* T = Cast<APHCTankPawn>(VPawn)) MC = T->VehicleMappingContext;
			else if (APHCDirtBikePawn* DB = Cast<APHCDirtBikePawn>(VPawn)) MC = DB->VehicleMappingContext;
			else if (APHCBlackbirdPawn* BB = Cast<APHCBlackbirdPawn>(VPawn)) MC = BB->VehicleMappingContext;
			else if (APHCBlackHawkPawn* BH = Cast<APHCBlackHawkPawn>(VPawn)) MC = BH->VehicleMappingContext;
			if (MC) Subsystem->RemoveMappingContext(MC);
		};
		if (CurrentVehicle) TryRemoveVehicleMC(CurrentVehicle);

		// Restore character input.
		if (Character && Character->DefaultMappingContext)
		{
			Subsystem->AddMappingContext(Character->DefaultMappingContext, 0);
		}
	}

	// Dirt bike: re-apply first person if needed.
	if (Cast<APHCDirtBikePawn>(CurrentVehicle) && Character && Character->bFirstPersonMode)
	{
		Character->SetFirstPersonMode(true);
	}

	// Switch HUD back to walking mode.
	if (UPHCCityHUD* CityHUD = Cast<UPHCCityHUD>(ActiveWidget))
	{
		CityHUD->SetDrivingMode(false, nullptr);
	}

	SetGameAndUIInputMode();
	CurrentVehicle = nullptr;
	LastVehiclePosition = FVector::ZeroVector;
}

void APHCPlayerController::ClientOnVehicleExited_Implementation(FVector ExitLocation)
{
	// If HandleLocalVehicleExit already ran (from Tick-based detection),
	// skip — everything is already handled.
	if (!bWasDrivingVehicle) return;

	// Otherwise run the same exit logic.
	HandleLocalVehicleExit();
}

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

void APHCPlayerController::SetUIOnlyInputMode()
{
	if (ActiveWidget)
	{
		ActiveWidget->SetIsFocusable(true);

		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ActiveWidget->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else if (ActiveGameHUD)
	{
		ActiveGameHUD->SetIsFocusable(true);

		FInputModeUIOnly InputMode;
		InputMode.SetWidgetToFocus(ActiveGameHUD->TakeWidget());
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	else
	{
		FInputModeUIOnly InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		SetInputMode(InputMode);
	}
	bShowMouseCursor = true;
}
