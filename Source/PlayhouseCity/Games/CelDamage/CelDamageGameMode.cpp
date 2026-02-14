// Copyright PlayhouseCity. All Rights Reserved.

#include "Games/CelDamage/CelDamageGameMode.h"
#include "Games/CelDamage/CelDamageHUD.h"
#include "Games/CelDamage/CelDamageVehicle.h"
#include "Games/CelDamage/CelDamageWeapon.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/Material.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

// ---------------------------------------------------------------------------

ACelDamageGameMode::ACelDamageGameMode()
{
	GameType = EPHCGameType::CelDamage;
	GameHUDClass = UCelDamageHUD::StaticClass();
	MaxGameTime = 120.f;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
}

// ---------------------------------------------------------------------------

void ACelDamageGameMode::BeginPlay()
{
	Super::BeginPlay();
	SpawnArena();
}

// ---------------------------------------------------------------------------

void ACelDamageGameMode::StartGame()
{
	Super::StartGame();

	PlayerSmacks = 0;
	AISmacks = 0;
	bPlayerNeedsRespawn = false;
	bAINeedsRespawn = false;

	// Clean up previous vehicles.
	if (PlayerVehicle)
	{
		PlayerVehicle->Destroy();
		PlayerVehicle = nullptr;
	}
	if (AIVehicle)
	{
		AIVehicle->Destroy();
		AIVehicle = nullptr;
	}

	// Clean up previous pickups.
	for (ACelDamageWeapon* WP : WeaponPickups)
	{
		if (WP)
		{
			WP->Destroy();
		}
	}
	WeaponPickups.Empty();

	SpawnVehicles();
	SpawnWeaponPickups();
}

// ---------------------------------------------------------------------------

void ACelDamageGameMode::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bGameActive || bGamePaused)
	{
		return;
	}

	HandleRespawns(DeltaTime);

	// Update AI pickup target.
	if (AIVehicle && AIVehicle->Health > 0.f)
	{
		AActor* NearestPickup = nullptr;
		float NearestDist = FLT_MAX;
		for (ACelDamageWeapon* WP : WeaponPickups)
		{
			if (WP && WP->bIsActive)
			{
				const float Dist = FVector::Dist(AIVehicle->GetActorLocation(),
					WP->GetActorLocation());
				if (Dist < NearestDist)
				{
					NearestDist = Dist;
					NearestPickup = WP;
				}
			}
		}
		AIVehicle->SetNearestPickup(NearestPickup);
	}

	// Check score limit.
	if (PlayerSmacks >= ScoreLimit)
	{
		EndGame(/*bWon=*/ true);
	}
	else if (AISmacks >= ScoreLimit)
	{
		EndGame(/*bWon=*/ false);
	}
}

// ---------------------------------------------------------------------------

void ACelDamageGameMode::EndGame(bool bWon)
{
	// If called from base class timeout, determine winner by smacks.
	if (!bWon && GameTimer >= MaxGameTime)
	{
		bWon = (PlayerSmacks > AISmacks);
	}

	CurrentScore = PlayerSmacks * 100;

	Super::EndGame(bWon);
}

// ---------------------------------------------------------------------------
// Player input proxies
// ---------------------------------------------------------------------------

void ACelDamageGameMode::PlayerThrottle(float Value)
{
	if (PlayerVehicle && bGameActive && !bGamePaused)
	{
		PlayerVehicle->SetThrottle(Value);
	}
}

void ACelDamageGameMode::PlayerSteer(float Value)
{
	if (PlayerVehicle && bGameActive && !bGamePaused)
	{
		PlayerVehicle->SetSteering(Value);
	}
}

void ACelDamageGameMode::PlayerFire()
{
	if (PlayerVehicle && bGameActive && !bGamePaused)
	{
		PlayerVehicle->FireWeapon();
	}
}

// ---------------------------------------------------------------------------
// Arena setup
// ---------------------------------------------------------------------------

void ACelDamageGameMode::SpawnArena()
{
	const FVector Center = ArenaCenter();
	const float Half = ArenaSize / 2.f;
	const float WallThick = 50.f;

	// Floor
	SpawnArenaBlock(
		Center,
		FVector(ArenaSize / 100.f, ArenaSize / 100.f, 1.f),
		FRotator::ZeroRotator,
		FLinearColor(0.2f, 0.75f, 0.15f, 1.f));

	// North wall
	SpawnArenaBlock(
		Center + FVector(0.f, Half, WallHeight / 2.f),
		FVector(ArenaSize / 100.f, WallThick / 100.f, WallHeight / 100.f),
		FRotator::ZeroRotator,
		FLinearColor(0.15f, 0.3f, 0.85f, 1.f));

	// South wall
	SpawnArenaBlock(
		Center + FVector(0.f, -Half, WallHeight / 2.f),
		FVector(ArenaSize / 100.f, WallThick / 100.f, WallHeight / 100.f),
		FRotator::ZeroRotator,
		FLinearColor(0.15f, 0.3f, 0.85f, 1.f));

	// East wall
	SpawnArenaBlock(
		Center + FVector(Half, 0.f, WallHeight / 2.f),
		FVector(WallThick / 100.f, ArenaSize / 100.f, WallHeight / 100.f),
		FRotator::ZeroRotator,
		FLinearColor(0.15f, 0.3f, 0.85f, 1.f));

	// West wall
	SpawnArenaBlock(
		Center + FVector(-Half, 0.f, WallHeight / 2.f),
		FVector(WallThick / 100.f, ArenaSize / 100.f, WallHeight / 100.f),
		FRotator::ZeroRotator,
		FLinearColor(0.15f, 0.3f, 0.85f, 1.f));

	// Ramps (6 around the arena)
	const float RampRadius = 2500.f;
	for (int32 i = 0; i < 6; ++i)
	{
		const float Angle = (static_cast<float>(i) / 6.f) * 360.f;
		const float Rad = FMath::DegreesToRadians(Angle);
		const FVector RampPos = Center + FVector(
			FMath::Cos(Rad) * RampRadius,
			FMath::Sin(Rad) * RampRadius,
			10.f);
		SpawnArenaBlock(
			RampPos,
			FVector(3.f, 2.f, 0.2f),
			FRotator(-25.f, Angle, 0.f),
			FLinearColor(0.95f, 0.55f, 0.1f, 1.f));
	}

	// Central obstacle cluster
	SpawnArenaBlock(
		Center + FVector(0.f, 0.f, 150.f),
		FVector(4.f, 4.f, 3.f),
		FRotator::ZeroRotator,
		FLinearColor(0.9f, 0.8f, 0.15f, 1.f));

	// 4 smaller cover blocks around center
	SpawnArenaBlock(Center + FVector(350.f, 0.f, 75.f), FVector(1.5f, 1.5f, 1.5f),
		FRotator::ZeroRotator, FLinearColor(0.85f, 0.7f, 0.1f, 1.f));
	SpawnArenaBlock(Center + FVector(-350.f, 0.f, 75.f), FVector(1.5f, 1.5f, 1.5f),
		FRotator::ZeroRotator, FLinearColor(0.85f, 0.7f, 0.1f, 1.f));
	SpawnArenaBlock(Center + FVector(0.f, 350.f, 75.f), FVector(1.5f, 1.5f, 1.5f),
		FRotator::ZeroRotator, FLinearColor(0.85f, 0.7f, 0.1f, 1.f));
	SpawnArenaBlock(Center + FVector(0.f, -350.f, 75.f), FVector(1.5f, 1.5f, 1.5f),
		FRotator::ZeroRotator, FLinearColor(0.85f, 0.7f, 0.1f, 1.f));

	// Respawn points (4 corners, away from center)
	RespawnPoints.Empty();
	RespawnPoints.Add(Center + FVector(-3000.f, -3000.f, 100.f));
	RespawnPoints.Add(Center + FVector(3000.f, -3000.f, 100.f));
	RespawnPoints.Add(Center + FVector(-3000.f, 3000.f, 100.f));
	RespawnPoints.Add(Center + FVector(3000.f, 3000.f, 100.f));
}

AActor* ACelDamageGameMode::SpawnArenaBlock(const FVector& Location, const FVector& Scale,
											 const FRotator& Rotation, const FLinearColor& Color)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Block = World->SpawnActor<AActor>(AActor::StaticClass(), Location, Rotation, SP);
	if (!Block)
	{
		return nullptr;
	}

	USceneComponent* Root = NewObject<USceneComponent>(Block, TEXT("Root"));
	Block->SetRootComponent(Root);
	Root->RegisterComponent();

	UStaticMeshComponent* Mesh = NewObject<UStaticMeshComponent>(Block, TEXT("Mesh"));
	Mesh->SetupAttachment(Root);

	UStaticMesh* CubeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh)
	{
		Mesh->SetStaticMesh(CubeMesh);
	}
	Mesh->SetRelativeScale3D(Scale);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

	UMaterialInterface* BaseMat = LoadObject<UMaterialInterface>(nullptr,
		TEXT("/Game/Materials/M_PHCColor.M_PHCColor"));
	if (BaseMat)
	{
		UMaterialInstanceDynamic* DynMat = UMaterialInstanceDynamic::Create(BaseMat, Block);
		DynMat->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Mesh->SetMaterial(0, DynMat);
	}

	Mesh->RegisterComponent();

	ArenaActors.Add(Block);
	return Block;
}

// ---------------------------------------------------------------------------
// Vehicle spawning
// ---------------------------------------------------------------------------

void ACelDamageGameMode::SpawnVehicles()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = ArenaCenter();
	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	// Player vehicle (southwest)
	PlayerVehicle = World->SpawnActor<ACelDamageVehicle>(
		ACelDamageVehicle::StaticClass(),
		Center + FVector(-2500.f, -2500.f, 100.f),
		FRotator(0.f, 45.f, 0.f), SP);
	if (PlayerVehicle)
	{
		PlayerVehicle->bIsCPU = false;
		PlayerVehicle->VehicleColor = FLinearColor(0.15f, 0.55f, 0.85f, 1.f);
		PlayerVehicle->Health = PlayerVehicle->MaxHealth;
		PlayerVehicle->OnVehicleDestroyed.AddDynamic(this,
			&ACelDamageGameMode::OnPlayerVehicleDestroyed);
		PlayerVehicle->OnVehicleHit.AddDynamic(this,
			&ACelDamageGameMode::OnVehicleHit);

		// Apply body color
		if (PlayerVehicle->BodyMesh && PlayerVehicle->BodyMesh->GetNumMaterials() > 0)
		{
			UMaterialInstanceDynamic* DynMat = PlayerVehicle->BodyMesh->CreateDynamicMaterialInstance(0);
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), PlayerVehicle->VehicleColor);
			}
		}

		// Set view target to player vehicle for 3D camera
		APlayerController* PC = World->GetFirstPlayerController();
		if (PC)
		{
			PC->SetViewTarget(PlayerVehicle);
		}
	}

	// AI vehicle (northeast)
	AIVehicle = World->SpawnActor<ACelDamageVehicle>(
		ACelDamageVehicle::StaticClass(),
		Center + FVector(2500.f, 2500.f, 100.f),
		FRotator(0.f, -135.f, 0.f), SP);
	if (AIVehicle)
	{
		AIVehicle->bIsCPU = true;
		AIVehicle->VehicleColor = FLinearColor(0.85f, 0.2f, 0.15f, 1.f);
		AIVehicle->Health = AIVehicle->MaxHealth;
		AIVehicle->SetCPUTarget(PlayerVehicle);
		AIVehicle->OnVehicleDestroyed.AddDynamic(this,
			&ACelDamageGameMode::OnAIVehicleDestroyed);
		AIVehicle->OnVehicleHit.AddDynamic(this,
			&ACelDamageGameMode::OnVehicleHit);

		// Apply body color
		if (AIVehicle->BodyMesh && AIVehicle->BodyMesh->GetNumMaterials() > 0)
		{
			UMaterialInstanceDynamic* DynMat = AIVehicle->BodyMesh->CreateDynamicMaterialInstance(0);
			if (DynMat)
			{
				DynMat->SetVectorParameterValue(TEXT("BaseColor"), AIVehicle->VehicleColor);
			}
		}
	}

	// Set AI homing target for player's homing missiles
	if (PlayerVehicle && AIVehicle)
	{
		PlayerVehicle->SetCPUTarget(AIVehicle);
	}
}

// ---------------------------------------------------------------------------
// Weapon pickups
// ---------------------------------------------------------------------------

void ACelDamageGameMode::SpawnWeaponPickups()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Center = ArenaCenter();
	const TArray<FVector> SpawnPositions = {
		Center + FVector(0.f, 0.f, 80.f),
		Center + FVector(2000.f, 0.f, 80.f),
		Center + FVector(-2000.f, 0.f, 80.f),
		Center + FVector(0.f, 2000.f, 80.f),
		Center + FVector(0.f, -2000.f, 80.f),
		Center + FVector(1500.f, 1500.f, 80.f),
		Center + FVector(-1500.f, 1500.f, 80.f),
		Center + FVector(1500.f, -1500.f, 80.f),
		Center + FVector(-1500.f, -1500.f, 80.f),
		Center + FVector(3000.f, 0.f, 80.f),
	};

	const TArray<ECelDamageWeaponType> WeaponPool = {
		ECelDamageWeaponType::Bat,
		ECelDamageWeaponType::FreezeRay,
		ECelDamageWeaponType::Cannon,
		ECelDamageWeaponType::TNT,
		ECelDamageWeaponType::Homing
	};

	FActorSpawnParameters SP;
	SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	for (int32 i = 0; i < SpawnPositions.Num() && i < NumWeaponSpawns; ++i)
	{
		ACelDamageWeapon* Pickup = World->SpawnActor<ACelDamageWeapon>(
			ACelDamageWeapon::StaticClass(), SpawnPositions[i],
			FRotator::ZeroRotator, SP);
		if (Pickup)
		{
			Pickup->WeaponType = WeaponPool[FMath::RandRange(0, WeaponPool.Num() - 1)];
			WeaponPickups.Add(Pickup);
		}
	}
}

// ---------------------------------------------------------------------------
// Respawn handling
// ---------------------------------------------------------------------------

void ACelDamageGameMode::HandleRespawns(float DT)
{
	if (bPlayerNeedsRespawn)
	{
		PlayerRespawnTimer -= DT;
		if (PlayerRespawnTimer <= 0.f)
		{
			bPlayerNeedsRespawn = false;
			if (PlayerVehicle)
			{
				PlayerVehicle->Respawn(GetSafeRespawnPoint(AIVehicle));

				// Re-set view target
				APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
				if (PC)
				{
					PC->SetViewTarget(PlayerVehicle);
				}
			}
		}
	}

	if (bAINeedsRespawn)
	{
		AIRespawnTimer -= DT;
		if (AIRespawnTimer <= 0.f)
		{
			bAINeedsRespawn = false;
			if (AIVehicle)
			{
				AIVehicle->Respawn(GetSafeRespawnPoint(PlayerVehicle));
				AIVehicle->SetCPUTarget(PlayerVehicle);
			}
		}
	}
}

FVector ACelDamageGameMode::GetSafeRespawnPoint(const ACelDamageVehicle* AwayFrom) const
{
	if (RespawnPoints.Num() == 0)
	{
		return ArenaCenter() + FVector(0.f, 0.f, 100.f);
	}

	if (!AwayFrom)
	{
		return RespawnPoints[FMath::RandRange(0, RespawnPoints.Num() - 1)];
	}

	// Pick the farthest respawn point from the other vehicle.
	const FVector OtherLoc = AwayFrom->GetActorLocation();
	int32 BestIdx = 0;
	float BestDist = 0.f;
	for (int32 i = 0; i < RespawnPoints.Num(); ++i)
	{
		const float Dist = FVector::Dist(RespawnPoints[i], OtherLoc);
		if (Dist > BestDist)
		{
			BestDist = Dist;
			BestIdx = i;
		}
	}
	return RespawnPoints[BestIdx];
}

// ---------------------------------------------------------------------------
// Destruction / Hit callbacks
// ---------------------------------------------------------------------------

void ACelDamageGameMode::OnPlayerVehicleDestroyed(ACelDamageVehicle* /*Vehicle*/)
{
	bPlayerNeedsRespawn = true;
	PlayerRespawnTimer = RespawnDelay;
}

void ACelDamageGameMode::OnAIVehicleDestroyed(ACelDamageVehicle* /*Vehicle*/)
{
	bAINeedsRespawn = true;
	AIRespawnTimer = RespawnDelay;
}

void ACelDamageGameMode::OnVehicleHit(ACelDamageVehicle* Attacker, ACelDamageVehicle* Victim)
{
	if (!Attacker || !Victim)
	{
		return;
	}

	// Determine smack value based on the weapon that was used.
	// Special weapons (TNT, Homing) are worth 2, others worth 1.
	// But since weapons revert to Fists after firing, we check projectile SmackValue
	// via the delegate. For melee (Fists/Bat), award 1 smack here.
	const int32 SmackValue = 1;

	if (Attacker == PlayerVehicle)
	{
		PlayerSmacks += SmackValue;
		AddScore(SmackValue * 100);
	}
	else if (Attacker == AIVehicle)
	{
		AISmacks += SmackValue;
	}
}
