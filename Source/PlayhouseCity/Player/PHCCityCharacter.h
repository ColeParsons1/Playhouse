// Copyright PlayhouseCity. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "PHCCityCharacter.generated.h"

class UBoxComponent;
class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class UInputMappingContext;
class UInputAction;
class UPointLightComponent;
class USpotLightComponent;
class UMaterialInstanceDynamic;
class UTextRenderComponent;
class UNiagaraComponent;
class UNiagaraSystem;
struct FInputActionValue;

/**
 * Third-person player character for the PlayhouseCity open world.
 *
 * Derives from APawn (NOT ACharacter) to avoid the capsule/CMC physics
 * integration that launches the actor to Z≈90000 via depenetration.
 * All movement is manual — same pattern as the vehicle pawns.
 */
UCLASS(Blueprintable)
class PLAYHOUSECITY_API APHCCityCharacter : public APawn
{
	GENERATED_BODY()

public:
	APHCCityCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -- Components -----------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Collision")
	TObjectPtr<UBoxComponent> CharacterCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "PHC|Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	/** Floating player name above head. */
	UPROPERTY()
	TObjectPtr<UTextRenderComponent> NameTextComp;

	/** Liquid glass arrow floating above head — visible to other players only. */
	UPROPERTY()
	TObjectPtr<UStaticMeshComponent> ArrowIndicator;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ArrowMID;

	// -- Mixamo Skeletal Mesh Character ----------------------------------------
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<USkeletalMeshComponent> CharacterSkelMesh;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> IdleAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> WalkAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> RunAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> JumpAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> RollerBladeAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> WallRunLeftAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> WallRunRightAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> RifleRunAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> RunRightAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> RunLeftAnim;
	UPROPERTY(Transient) TObjectPtr<UAnimSequence> SwordRunAnim;
	int32 CurrentAnimState = 0; // 0=idle, 1=walk, 2=run, 3=jump, 5=wallRunL, 6=wallRunR, 7=rifleRun, 8=swordRun, 9=runRight, 10=runLeft

	// -- Legacy body components (hidden, kept for NPC compatibility) ----------

	// Head & Face
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> HeadMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> JawMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> NeckMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftEyeWhiteMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightEyeWhiteMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftPupilMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightPupilMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> NoseMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> MouthMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftEarMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightEarMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftBrowMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightBrowMesh;

	// Hair
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> HairTopMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> HairBackMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> HairSideLMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> HairSideRMesh;

	// Torso (sphere-based organic)
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> ChestMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> AbsMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> PelvisMesh;

	// Clothing details
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> CollarMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BeltMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BeltBuckleMesh;

	// Left Arm
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftShoulderMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftUpperArmMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftElbowMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftForearmMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftHandMesh;

	// Right Arm
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightShoulderMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightUpperArmMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightElbowMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightForearmMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightHandMesh;

	// Left Leg
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftThighMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftKneeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftShinMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftFootMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftToeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> LeftSoleMesh;

	// Right Leg
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightThighMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightKneeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightShinMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightFootMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightToeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> RightSoleMesh;

	// -- Enhanced Input assets (created at runtime) ---------------------------

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputAction> InteractAction;

	// -- Movement parameters --------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Movement")
	float WalkSpeed = 2360.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "PHC|Movement")
	float SprintSpeed = 2600.f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "PHC|Movement")
	bool bIsSprinting = false;

	/** True while the player is on a mushroom trip — replicated so other
	 *  clients can hide this character (invisible to others while tripping). */
	UPROPERTY(Replicated)
	bool bIsTripping = false;

	/** Local tracking: true when we've hidden this pawn because it's tripping
	 *  (only set on non-owning clients to avoid re-setting visibility each frame). */
	bool bTripHiddenLocally = false;

	bool bPendingVehicleInteract = false;

	/** Accumulated movement direction from HandleMove (camera-relative). */
	FVector PendingMoveInput = FVector::ZeroVector;

	/** Current movement speed fraction (0-1) for smooth acceleration. */
	float MoveSpeedAlpha = 0.f;

	/** Manual vertical velocity for gravity + jump. */
	float ManualVelocityZ = 0.f;

	/** True after the character has touched ground at least once. */
	bool bHasBeenGrounded = false;

	/** True once the floating name text has been set from the player profile. */
	bool bNameTextSet = false;

	/** True when grounded on previous frame. */
	bool bWasGrounded = false;

	// -- Parkour system -------------------------------------------------------

	enum class EParkourState : uint8
	{
		None,
		WallRunL,    // running along wall on character's left
		WallRunR,    // running along wall on character's right
		Sliding,
		Vaulting
	};

	EParkourState ParkourState = EParkourState::None;
	float ParkourTimer = 0.f;           // time spent in current parkour state
	FVector ParkourWallNormal = FVector::ZeroVector; // outward normal of wall being run on
	FVector ParkourWallPoint = FVector::ZeroVector;  // closest point on wall
	FVector ParkourForward = FVector::ZeroVector;    // direction of travel along wall
	FVector ParkourStartPos = FVector::ZeroVector;   // position when parkour state began
	float ParkourWallRunSpeed = 2800.f;  // target wall run speed (NOSHOT: 17 m/s scaled to UU)
	float ParkourWallRunMaxTime = 1.2f;  // max wall run duration (NOSHOT: 1.0s, ours slightly longer)
	float ParkourWallJumpUp = 500.f;     // vertical impulse on wall jump
	float ParkourWallJumpOut = 500.f;    // horizontal impulse away from wall on wall jump
	float ParkourSlideMaxTime = 1.0f;    // slide timer (NOSHOT: 1.0s countdown)
	FVector ParkourSlideDir = FVector::ZeroVector;
	float ParkourVaultMaxTime = 0.4f;    // vault Bezier duration
	FVector ParkourVaultStart = FVector::ZeroVector;
	FVector ParkourVaultEnd = FVector::ZeroVector;
	FVector ParkourVaultMid = FVector::ZeroVector;   // Bezier control point
	float ParkourCooldown = 0.f;
	float ParkourAirTime = 0.f;          // time spent airborne (NOSHOT: must be < 2.0s for wall run)
	float ParkourWallRunCurrentSpeed = 0.f; // current speed during wall run (ramps up)

	/** Try to enter wall run. Called every frame while moving near a wall. */
	bool TryStartWallRun();

	/** Try to detect and start a vault over an obstacle ahead. Returns true if vault started. */
	bool TryStartVault();

	/** Exit current parkour state and return to normal movement. */
	void EndParkour();

	// -- R6 walk animation state ----------------------------------------------

	float WalkCyclePhase = 0.f;
	float WalkAnimBlend = 0.f;

	// -- Jetpack --------------------------------------------------------------

	bool bJetpackEquipped = false;
	float JetpackVerticalInput = 0.f;  // +1 = ascend, -1 = descend
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackBodyMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackNozzleL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackNozzleR;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackFlameL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackFlameR;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackStrapL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> JetpackStrapR;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> JetpackGlowL;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> JetpackGlowR;

	void EquipJetpack();
	void UnequipJetpack();

	// -- Roller Blades --------------------------------------------------------

	bool bRollerBladesEquipped = false;

	void EquipRollerBlades();
	void UnequipRollerBlades();

	// -- Climbing Gear --------------------------------------------------------

	bool bClimbingGearEquipped = false;
	float ClimbingVerticalInput = 0.f;  // +1 = ascend, -1 = descend
	bool bIsClinging = false;           // true when surface detected nearby

	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ClimbHarnessFrontMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ClimbHarnessBackMesh;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ClimbHarnessStrapL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ClimbHarnessStrapR;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ClimbCarabinerL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ClimbCarabinerR;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ClimbRopeCoilL;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> ClimbRopeCoilR;

	void EquipClimbingGear();
	void UnequipClimbingGear();

	// -- Dagger weapon mesh ---------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> DaggerHandleMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> DaggerBladeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> DaggerGuardMesh;

	float DaggerStabTimer = 0.f;  // >0 = currently stabbing

	// -- Lightsaber weapon mesh ------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> SaberHiltMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> SaberEmitterMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> SaberBladeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> SaberCoreMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> SaberPommelMesh;
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> SaberGlow;

	float SaberSwingTimer = 0.f;

	// -- Scythe weapon mesh ---------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> ScytheHandleMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> ScytheBladeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> ScytheBladeEdgeMesh;

	// -- Minigun weapon mesh --------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> MinigunBodyMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> MinigunBarrelMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> MinigunMuzzleMesh;

	// -- Bazooka (RPG) weapon mesh --------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaTubeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaRearEndMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaFrontSightMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaRearSightMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaGripMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaScopeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> BazookaMuzzleMesh;

	// -- AK-47 weapon mesh ---------------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> AK47BodyMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> AK47BarrelMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> AK47MagMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> AK47StockMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> AK47MuzzleMesh;

	// -- 50 Cal Sniper Rifle mesh -----------------------------------------------

	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> SniperBodyMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> SniperBarrelMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> SniperScopeMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> SniperStockMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> SniperMagMesh;
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> SniperMuzzleMesh;

	// -- Flamethrower weapon mesh --
	UPROPERTY(VisibleAnywhere, Category = "PHC|Mesh") TObjectPtr<UStaticMeshComponent> FlamethrowerMesh;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> FlamethrowerFlameComp;
	bool bFlamethrowerFiring = false;
	float FlamethrowerFuelTimer = 0.f; // continuous fire timer

	// -- Health ---------------------------------------------------------------
	float Health = 100.f;
	float MaxHealth = 100.f;

	// -- Kill / Death tracking ------------------------------------------------
	int32 CrabKills = 0;
	int32 Deaths = 0;

	// -- XP / Leveling system -------------------------------------------------
	int32 PlayerLevel = 1;
	int32 PlayerXP = 0;
	int32 XPToNextLevel = 100;   // formula: 100 * Level
	float XPAccumTimer = 0.f;    // passive XP timer (1 XP/sec while moving)
	float LevelUpFlashTimer = 0.f; // visual feedback on level-up

	/** Add XP and handle level-up. Returns true if leveled up. */
	bool AddXP(int32 Amount);

	// -- Daily login reward ---------------------------------------------------
	int32 LoginStreak = 0;         // current consecutive day streak (0-6)
	int32 LastLoginJulianDay = 0;  // Julian day number of last login
	bool bDailyRewardPending = false; // true = popup should show
	bool bDailyRewardClaimed = false; // true = already claimed today

	/** Check daily login and set bDailyRewardPending if new day. */
	void CheckDailyLogin();

	/** Claim daily reward and advance streak. */
	int32 ClaimDailyReward();

	// -- Kill streak multiplier -----------------------------------------------
	int32 KillStreakCount = 0;
	float KillStreakTimer = 0.f;  // resets to 5s on each kill, decrements
	float KillStreakMultiplier() const;

	// -- Kill Streak Perks ----------------------------------------------------
	enum class EKillStreakPerk : uint8 { None, ChopperGunner, AC130, TacticalNuke };
	EKillStreakPerk ActiveKillStreak = EKillStreakPerk::None;
	float KillStreakPerkTimer = 0.f;
	bool bTacticalNukeCountdown = false;
	float TacticalNukeCountdownTimer = 0.f;
	bool bTacticalNukeDetonated = false;
	float TacticalNukeFlashTimer = 0.f;

	// Chopper Gunner state
	bool bChopperGunnerActive = false;
	float ChopperGunnerTimer = 0.f;
	FVector ChopperGunnerPos = FVector::ZeroVector;
	float ChopperGunnerYaw = 0.f;
	float ChopperGunnerPitch = -60.f;
	float SavedArmLength = 320.f;
	FVector SavedSocketOffset = FVector::ZeroVector;

	// AC-130 state
	bool bAC130Active = false;
	float AC130Timer = 0.f;
	int32 AC130WeaponMode = 0; // 0=25mm, 1=40mm, 2=105mm
	float AC130OrbitAngle = 0.f;
	float AC130Pitch = -70.f;
	float AC130FireCooldown = 0.f;

	void ActivateChopperGunner();
	void ActivateAC130();
	void ActivateTacticalNuke();
	void DeactivateKillStreak();

	// -- Loot crate system ----------------------------------------------------
	bool bLootCrateActive = false;
	float LootCrateTimer = 0.f;     // total time spinning (2s)
	float LootCrateSpinAngle = 0.f; // visual spinner rotation
	int32 LootCrateReward = 0;      // final coin reward
	int32 LootCrateXPReward = 0;    // final XP reward

	/** Roll a random loot crate reward and start the spin animation. */
	void StartLootCrate();

	// -- Achievement system ---------------------------------------------------
	static constexpr int32 NumAchievements = 20;

	struct FAchievement
	{
		const TCHAR* Name;
		const TCHAR* Desc;
		int32 Target;
	};

	static const FAchievement AchievementDefs[NumAchievements];
	bool AchievementUnlocked[NumAchievements] = {};
	float AchievementPopupTimer = 0.f;
	int32 LastUnlockedAchievement = -1;
	int32 TotalCoinsCollected = 0;
	float TotalDistanceWalked = 0.f;
	FVector PrevTickLocation = FVector::ZeroVector;
	int32 VehiclesUsed = 0;          // bitmask of vehicle types driven
	int32 MiniGamesPlayed = 0;

	/** Check all achievements and unlock any newly completed ones. */
	void CheckAchievements();

	// -- Limited-time events --------------------------------------------------
	enum class EActiveEvent : uint8 { None, GoldenHour, CrabSwarm, NightHunt };
	EActiveEvent CurrentEvent = EActiveEvent::None;
	float EventTimer = 0.f;         // time remaining on current event
	float EventCooldown = 0.f;      // gap between events
	float EventCoinMultiplier() const;

	// -- Vehicle unlock progression -------------------------------------------
	/** Returns true if the given vehicle type is unlocked at current level. */
	bool IsVehicleUnlocked(int32 VehicleType) const;
	/** Returns the level required to unlock a vehicle type. */
	static int32 VehicleUnlockLevel(int32 VehicleType);

	// -- Weekly stats (leaderboard) -------------------------------------------
	int32 WeeklyXP = 0;
	int32 WeeklyKills = 0;
	int32 WeeklyCoins = 0;
	int32 LastResetWeek = 0; // ISO week number * year

	/** Check and reset weekly stats if new week. */
	void CheckWeeklyReset();

	/** >0 = screen should flash red (set by subsystem on crab contact). */
	float DamageFlashTimer = 0.f;

	// -- Weapon state ---------------------------------------------------------

	int32 CurrentWeaponType = 3; // -1=none, 0=Gun, 1=RPG, 2=Sword, 3=Scythe, 4=Dagger, 5=AK47, 6=Sniper, 7=Flamethrower

	float ScytheSwingTimer = 0.f;  // >0 = currently swinging

	float MinigunSpinAngle = 0.f;
	bool bMinigunFiring = false;
	float MinigunFireTimer = 0.f;

	float BazookaFireTimer = 0.f;
	float BazookaMuzzleFlashTimer = 0.f;
	float CameraShakeTimer = 0.f;
	float CameraShakeIntensity = 0.f;

	bool bAK47Firing = false;
	float AK47FireTimer = 0.f;

	float SniperFireTimer = 0.f; // bolt-action cooldown (1.8s)
	FVector2D ScopedLookSmooth = FVector2D::ZeroVector; // smoothed look input when scoped

	// -- COD-style weapon feel ------------------------------------------------

	// Recoil (per-shot pitch kick + yaw scatter, auto-recovery)
	float RecoilPitchAccum = 0.f;
	float RecoilYawAccum = 0.f;
	float RecoilRecoverySpeed = 10.f;

	// Weapon viewmodel bob (uses existing WalkCyclePhase/WalkAnimBlend)
	float WeaponBobH = 0.f;
	float WeaponBobV = 0.f;

	// ADS (Aim Down Sights)
	bool bADS = false;
	float ADSAlpha = 0.f;
	float ADSInterpSpeed = 14.f;
	float ADSFOVHip = 90.f;
	float ADSFOVAim = 65.f;
	float ADSSensitivityMult = 0.6f;

	// Sniper scope zoom (adjustable by user)
	float SniperZoomFOV = 10.f;
	static constexpr float SniperZoomMin = 1.f;
	static constexpr float SniperZoomMax = 40.f;

	// Thermal scope (MW2 style)
	bool bThermalScope = false;

	// Bullet spread (half-angle degrees) — base values
	float SpreadHipfire = 3.0f;
	float SpreadADS = 0.5f;

	// Fortnite-style bloom: spread grows per shot, recovers over time
	float BloomAccum = 0.f;           // current accumulated bloom (degrees)
	float BloomPerShot = 0.4f;        // bloom added per shot
	float BloomMax = 5.0f;            // max bloom cap
	float BloomRecoveryRate = 4.0f;   // degrees/sec recovery when not firing
	float BloomMovePenalty = 0.f;     // extra spread from movement (0-2 degrees)

	// Aim assist (Fortnite-style target magnetism for mobile)
	float AimAssistRadius = 150.f;    // screen-space radius in pixels for slowdown
	float AimAssistSlowdown = 0.4f;   // look sensitivity multiplier when near target (0.4 = 60% slower)
	float AimAssistPullStrength = 2.0f; // degrees/sec pull toward nearest target
	float AimAssistMaxRange = 15000.f;  // max world distance for aim assist

	// Per-shot micro-shake
	float ShotShakeTimer = 0.f;

	// Hit marker
	float HitMarkerTimer = 0.f;

	// Headshot multiplier
	static constexpr float HeadshotMultiplier = 2.0f;

	// Muzzle flash dynamic light
	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> MuzzleFlashLight;

	// Holographic ground ring (Madden-style player indicator)
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> GroundRing;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> GroundRingInner;
	float GroundRingPulseTime = 0.f;
	float MuzzleFlashLightTimer = 0.f;

	// -- Flashlight --------------------------------------------------------------
	UPROPERTY(Transient) TObjectPtr<USpotLightComponent> FlashlightSpot;
	bool bFlashlightOn = false;

	// ADS input
	UPROPERTY(Transient, BlueprintReadOnly, Category = "PHC|Input")
	TObjectPtr<UInputAction> ADSAction;

	void SetADS(bool bNewADS);

	void FireWeapon();

	// -- Spring damper for secondary body motion ----------------------------

	struct FSimpleSpring
	{
		float Pos = 0.f;
		float Vel = 0.f;
		float Update(float Target, float Stiff, float Damp, float DT)
		{
			Vel += ((Target - Pos) * Stiff - Vel * Damp) * DT;
			Pos += Vel * DT;
			return Pos;
		}
	};

	FVector PrevFrameWorldVel = FVector::ZeroVector;
	FSimpleSpring LeanPitchSpring;
	FSimpleSpring LeanRollSpring;
	FSimpleSpring LHandLagX, LHandLagZ;
	FSimpleSpring RHandLagX, RHandLagZ;
	FSimpleSpring HairLagZ;
	FSimpleSpring WeaponSwayX, WeaponSwayY;
	FSimpleSpring WeaponKickZ, WeaponKickX;
	float PrevAimYaw = 0.f;
	float PrevAimPitch = 0.f;
	float IdleTimer = 0.f;

	// -- Mobile touch input (written by HUD each frame, read by Tick) ------

	float TouchMoveX = 0.f;
	float TouchMoveY = 0.f;
	float TouchLookX = 0.f;
	float TouchLookY = 0.f;
	bool bHUDTouchActive = false;

	// -- Camera perspective toggle -----------------------------------------

	bool bFirstPersonMode = false;
	void SetFirstPersonMode(bool bFirstPerson);
	void HideOldBodyMeshes();

	// -- Network movement replication (client → server) --------------------

	/** Server RPC: owning client sends position/rotation so server can
	 *  update the authoritative actor, which then replicates to all others. */
	UFUNCTION(Server, Unreliable)
	void ServerUpdateMovement(FVector_NetQuantize NewLocation, FRotator NewRotation);

	float NetMoveSendTimer = 0.f;

	// -- PvP damage (server-authoritative) ---------------------------------

	/** Called by the attacker's client to request damage on another player. */
	UFUNCTION(Server, Reliable)
	void ServerInflictDamage(APHCCityCharacter* Victim, float Damage);

	/** Multicast: all clients flash the victim's body red. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastDamageFlash();

	/** Called on the victim's owning client to show the red screen overlay. */
	UFUNCTION(Client, Reliable)
	void ClientOnDamaged(float NewHealth);

	/** Called on the victim's owning client on death. */
	UFUNCTION(Client, Reliable)
	void ClientOnKilled();

	/** Array of body MIDs for emissive flash (populated in BeginPlay). */
	TArray<TObjectPtr<UMaterialInstanceDynamic>> BodyMIDs;

	/** Timer for red body flash visible to all players. */
	float PvPDamageFlashTimer = 0.f;

protected:
	void HandleMove(const FInputActionValue& Value);
	void HandleLook(const FInputActionValue& Value);
	void HandleJumpStarted(const FInputActionValue& Value);
	void HandleJumpCompleted(const FInputActionValue& Value);
	void HandleSprintStarted(const FInputActionValue& Value);
	void HandleSprintCompleted(const FInputActionValue& Value);
	void HandleInteract(const FInputActionValue& Value);
	void HandleADSStarted(const FInputActionValue& Value);
	void HandleADSCompleted(const FInputActionValue& Value);

private:
	void CreateInputAssets();
};
