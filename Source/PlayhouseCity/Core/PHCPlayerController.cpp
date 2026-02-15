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
#include "Vehicle/PHCVehiclePawn.h"
#include "Vehicle/PHCDuneBuggyPawn.h"
#include "Player/PHCCityCharacter.h"
#include "Core/PHCCityGameMode.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

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
			// Auto guest-login so we go straight to the main HUD for testing.
			UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController -- Not logged in, auto guest login."));
			UPHCAuthManager* AuthManager = GI->GetAuthManager();
			if (AuthManager)
			{
				AuthManager->GuestLogin();
			}
			else
			{
				ShowMainHUD();
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
		}
	}

	SetGameAndUIInputMode();
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

	// Find nearest vehicle within range.
	constexpr float EnterRange = 500.f;
	const FVector CharLoc = Character->GetActorLocation();

	APHCVehiclePawn* NearestVehicle = nullptr;
	float NearestDist = EnterRange;

	// Get vehicles from the city game mode.
	if (APHCCityGameMode* CityGM = Cast<APHCCityGameMode>(GetWorld()->GetAuthGameMode()))
	{
		for (APHCVehiclePawn* Vehicle : CityGM->SpawnedVehicles)
		{
			if (!Vehicle || Vehicle->bIsOccupied) continue;

			const float Dist = FVector::Dist(CharLoc, Vehicle->GetActorLocation());
			if (Dist < NearestDist)
			{
				NearestDist = Dist;
				NearestVehicle = Vehicle;
			}
		}
	}

	if (!NearestVehicle)
	{
		UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::EnterNearestVehicle -- No vehicle in range."));
		return;
	}

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::EnterNearestVehicle -- Entering vehicle at %s"),
		*NearestVehicle->GetActorLocation().ToString());

	// Save character reference and hide it.
	SavedCharacterPawn = Character;
	CurrentVehicle = Cast<APawn>(NearestVehicle);

	// Remove character's mapping context.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (Character->DefaultMappingContext)
		{
			Subsystem->RemoveMappingContext(Character->DefaultMappingContext);
		}
	}

	// Hide character.
	Character->SetActorHiddenInGame(true);
	Character->SetActorEnableCollision(false);

	// Possess the vehicle.
	NearestVehicle->bIsOccupied = true;
	NearestVehicle->CurrentSpeed = 0.f;
	NearestVehicle->SetHeadlightsEnabled(true);
	UnPossess();
	Possess(NearestVehicle);

	// Tell the city HUD to switch to driving controls.
	if (UPHCCityHUD* CityHUD = Cast<UPHCCityHUD>(ActiveWidget))
	{
		CityHUD->SetDrivingMode(true, NearestVehicle);
	}

	SetGameAndUIInputMode();
}

// ---------------------------------------------------------------------------

void APHCPlayerController::SpawnAndEnterVehicle(int32 VehicleType)
{
	APHCCityCharacter* Character = Cast<APHCCityCharacter>(GetPawn());
	if (!Character) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const FVector SpawnLoc = Character->GetActorLocation() + Character->GetActorForwardVector() * 300.f + FVector(0.f, 0.f, 50.f);
	const FRotator SpawnRot(0.f, Character->GetActorRotation().Yaw, 0.f);

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APawn* SpawnedVehicle = nullptr;

	if (VehicleType == 1)
	{
		// Dune Buggy.
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
		// Aventador (default).
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

	// Enter the vehicle.
	SavedCharacterPawn = Character;
	CurrentVehicle = SpawnedVehicle;

	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (Character->DefaultMappingContext)
		{
			Subsystem->RemoveMappingContext(Character->DefaultMappingContext);
		}
	}

	Character->SetActorHiddenInGame(true);
	Character->SetActorEnableCollision(false);

	UnPossess();
	Possess(SpawnedVehicle);

	if (UPHCCityHUD* CityHUD = Cast<UPHCCityHUD>(ActiveWidget))
	{
		CityHUD->SetDrivingMode(true, SpawnedVehicle);
	}

	SetGameAndUIInputMode();

	UE_LOG(LogPlayhouseCity, Log, TEXT("SpawnAndEnterVehicle: Spawned vehicle type %d at %s"),
		VehicleType, *SpawnedVehicle->GetActorLocation().ToString());
}

void APHCPlayerController::OnVehicleExitRequested()
{
	if (!CurrentVehicle || !SavedCharacterPawn) return;

	UE_LOG(LogPlayhouseCity, Log, TEXT("PHCPlayerController::OnVehicleExitRequested -- Exiting vehicle."));

	// Access vehicle-specific members via cast.
	FVector ExitLoc = CurrentVehicle->GetActorLocation() + FVector(0.f, -300.f, 100.f);
	UInputMappingContext* VehicleMC = nullptr;

	if (APHCVehiclePawn* V = Cast<APHCVehiclePawn>(CurrentVehicle))
	{
		ExitLoc = V->GetExitLocation();
		V->bIsOccupied = false;
		V->ThrottleInput = 0.f;
		V->SteeringInput = 0.f;
		V->CurrentSpeed = 0.f;
		V->SetHeadlightsEnabled(false);
		VehicleMC = V->VehicleMappingContext;
	}
	else if (APHCDuneBuggyPawn* B = Cast<APHCDuneBuggyPawn>(CurrentVehicle))
	{
		ExitLoc = B->GetExitLocation();
		B->bIsOccupied = false;
		B->ThrottleInput = 0.f;
		B->SteeringInput = 0.f;
		B->CurrentSpeed = 0.f;
		B->SetHeadlightsEnabled(false);
		VehicleMC = B->VehicleMappingContext;
	}

	// Remove vehicle mapping context.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (VehicleMC)
		{
			Subsystem->RemoveMappingContext(VehicleMC);
		}
	}

	// Unpossess vehicle and repossess character.
	UnPossess();
	Possess(SavedCharacterPawn);

	// Restore character.
	SavedCharacterPawn->SetActorLocation(ExitLoc);
	SavedCharacterPawn->SetActorHiddenInGame(false);
	SavedCharacterPawn->SetActorEnableCollision(true);

	// Re-register character's mapping context.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
			ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (SavedCharacterPawn->DefaultMappingContext)
		{
			Subsystem->AddMappingContext(SavedCharacterPawn->DefaultMappingContext, 0);
		}
	}

	// Tell the city HUD to switch back to walk controls.
	if (UPHCCityHUD* CityHUD = Cast<UPHCCityHUD>(ActiveWidget))
	{
		CityHUD->SetDrivingMode(false, nullptr);
	}

	CurrentVehicle = nullptr;
	SavedCharacterPawn = nullptr;

	SetGameAndUIInputMode();
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
