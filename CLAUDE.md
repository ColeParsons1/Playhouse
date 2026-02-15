# PlayhouseCity - Architecture & Developer Guide

## Overview

PlayhouseCity is a **UE 5.7 iOS multiplayer open-world city game** built entirely in C++ with no Blueprint assets. Players walk around a procedurally generated city, drive vehicles (Lamborghini Aventador, Dune Buggy), and play 21 mini-games. The entire city, all vehicles, buildings, lighting, and UI are constructed programmatically at runtime using basic engine meshes (`/Engine/BasicShapes/Cube`, `Cylinder`, `Sphere`, `Plane`, `Cone`).

**Key design principle:** Zero `.uasset` dependencies for gameplay code. All input mappings, materials, meshes, and widgets are created via `NewObject<>` / `CreateDefaultSubobject<>` at runtime.

---

## Build & Deploy

```bash
# Build for iOS
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/RunUBT.sh" \
  PlayhouseCity IOS Development \
  -Project="/Users/coleparsons/PlayhouseCity/PlayhouseCity.uproject" \
  -Game -NoHotReload

# Deploy to Cole's iPhone (15 Pro)
xcrun devicectl device install app --device 00008130-000251843CF3803A \
  /Users/coleparsons/PlayhouseCity/Binaries/IOS/PlayhouseCity.app
xcrun devicectl device process launch --device 00008130-000251843CF3803A \
  com.playhousecity.PlayhouseCity

# Deploy to Tanvi's iPhone (14 Pro Max)
xcrun devicectl device install app --device 2EC6F309-9272-5A23-BB85-C58FF82D63FA \
  /Users/coleparsons/PlayhouseCity/Binaries/IOS/PlayhouseCity.app
xcrun devicectl device process launch --device 2EC6F309-9272-5A23-BB85-C58FF82D63FA \
  com.playhousecity.PlayhouseCity

# Build dedicated server (Linux, on build server BUILD_SERVER_IP)
# Production server: GAME_SERVER_IP
```

**Important:** Always use `RunUBT.sh` for builds, NOT `xcodebuild`. Build time is ~11 seconds for incremental changes.

---

## Architecture

### Startup Flow

```
UPHCGameInstance::Init()
  -> Creates subsystems: HttpClient, WebSocketClient, ThemeManager, EconomyManager, AuthManager

APHCCityGameMode (server authority)
  -> Sets DefaultPawnClass = APHCCityCharacter
  -> Sets PlayerControllerClass = APHCPlayerController
  -> Sets PlayerStateClass = APHCPlayerState
  -> Sets GameStateClass = APHCGameState

UPHCCitySubsystem::OnWorldBeginPlay() (runs on ALL machines independently)
  -> SpawnCityEnvironment()
     -> SpawnGround(), SpawnOceanAndBeach(), SpawnSkyAndLighting()
     -> SpawnCityGrid() (24x24 block grid with roads, sidewalks, buildings)
     -> SpawnSkyscrapers() (27 unique downtown towers)
     -> SpawnStreetLights(), SpawnPalmTrees(), SpawnRamps()
     -> SpawnOutskirts(), SpawnHills(), SpawnStarfield()

APHCPlayerController::BeginPlay()
  -> EnterWorldMode()
     -> Creates UPHCCityHUD (all widgets built in C++)
     -> Sets Game+UI input mode
```

### Core Classes

| Class | File | Role |
|-------|------|------|
| `APHCCityGameMode` | `Core/PHCCityGameMode.cpp` | Server game mode. Sets default classes, handles PostLogin/Logout. Tracks spawned vehicles. |
| `UPHCCitySubsystem` | `Core/PHCCitySubsystem.cpp` | **Largest file (~4800 lines).** `UTickableWorldSubsystem` that generates the entire city procedurally and runs the day/night cycle. Runs identically on server and all clients (deterministic seed). |
| `APHCPlayerController` | `Core/PHCPlayerController.cpp` | Player controller. Manages UI screens (auth, main HUD, city HUD), mini-game launching, and vehicle entry/exit. |
| `UPHCGameInstance` | `Core/PHCGameInstance.cpp` | Root game instance. Owns HTTP/WebSocket/Auth/Economy/Theme subsystems. Handles multiplayer session management (HostGame/JoinGame/LeaveGame). |
| `APHCGameState` | `Core/PHCGameState.cpp` | Replicates `ServerDayNightTime` so all clients synchronize lighting. |
| `APHCCityCharacter` | `Player/PHCCityCharacter.cpp` | Third-person character with spring arm camera. Enhanced Input for WASD/gamepad/touch. Has `UPHCTouchInputComponent` for mobile virtual joystick. |
| `APHCPlayerState` | `Player/PHCPlayerState.cpp` | Per-player replicated state: profile, room ID, ready status. |
| `APHCVehiclePawn` | `Vehicle/PHCVehiclePawn.cpp` | Lamborghini Aventador. Arcade physics (no UE vehicle system). Built from ~20 static mesh components. Own camera boom + Enhanced Input context. |
| `APHCDuneBuggyPawn` | `Vehicle/PHCDuneBuggyPawn.cpp` | Off-road buggy. Similar to Aventador but with roll cage, brush guard, missile launcher. |
| `APHCMissileProjectile` | `Vehicle/PHCMissileProjectile.cpp` | Projectile fired from dune buggy. Uses `UProjectileMovementComponent`. |
| `UPHCCityHUD` | `UI/PHCCityHUD.cpp` | Main in-game HUD. All widgets constructed in `Initialize()` via `WidgetTree->ConstructWidget<>`. Has currency display, vehicle selection popup, multiplayer Host/Join buttons, mobile touch controls for walking and driving. |
| `UPHCTouchInputComponent` | `Input/PHCTouchInputComponent.cpp` | Mobile virtual joystick (left half = move, right half = camera look). Attaches to character. |

### City Generation (PHCCitySubsystem)

The city is a **24x24 grid** of blocks. Each block is ~3600 UU. Block types are determined by grid position:

- **Downtown core (blocks 7-10, 10-15):** 27 unique skyscrapers with architectural details
- **Midtown:** Tall commercial buildings, institutional buildings
- **Residential (outer ring):** Houses with pitched roofs, lawns, garages, chimneys
- **Waterfront:** Beach, bay, ocean with sand and shallow water
- **Industrial:** Warehouses with loading docks
- **Parks:** Trees, paths, fountains scattered throughout
- **Arts district:** Buildings with murals and colorful walls

Each block has roads (dark asphalt) on all four sides with sidewalks. Street lights line intersections. Palm trees along boulevards.

**Day/night cycle** runs in `Tick()`: sun rotates, sky light recaptures, fog color shifts, window lights toggle on at night with warm glow.

**Ground plane:** `/Engine/BasicShapes/Plane` at `(0, 0, 0)` scaled to `(1400, 1400, 1)`. Ground surface is at **Z=0**.

**Building collision:** House bodies, roofs, lawns, garages have `QueryAndPhysics` collision. Skyscraper `MakeCubeMC` components have `NoCollision`. This distinction matters for vehicle ground traces (see Known Issues).

### Vehicle System

Vehicles use a custom arcade physics model (NOT UE's vehicle system):

- **No physics simulation** (`SetSimulatePhysics(false)`)
- **Speed** builds via `ThrottleInput` -> `CurrentSpeed` with quadratic acceleration curve
- **Steering** applies yaw rotation, scaled inversely with speed
- **Movement** via `AddActorWorldOffset()` using forward vector * speed * delta
- **No ground trace** currently (removed to fix floating bug - see Known Issues)
- **Camera** has its own spring arm (`TargetArmLength=600`, `SocketOffset=(0,0,180)`, `bDoCollisionTest=false`)

**Vehicle entry flow:**
1. Player taps "Car" button -> vehicle selection popup
2. Selects Aventador or Buggy -> `PHCPlayerController::SpawnAndEnterVehicle()`
3. Vehicle spawns 300 UU in front of character, 50 UU above
4. Character's Enhanced Input mapping context removed
5. Character hidden + collision disabled
6. `UnPossess()` character, `Possess()` vehicle
7. HUD switches to driving mode (gas/brake/steering touch zones)

**Vehicle exit flow:**
1. Player presses F key or Exit button -> `PHCVehiclePawn::HandleExitVehicle()`
2. Calls `PHCPlayerController::OnVehicleExitRequested()` via `ProcessEvent`
3. Vehicle input context removed
4. `UnPossess()` vehicle, `Possess()` saved character
5. Character teleported to vehicle exit location, unhidden
6. HUD switches back to walking mode

### Input System

All input uses **UE5 Enhanced Input** created programmatically (no `.uasset` files):

- `UInputAction` objects created in `BeginPlay()` via `NewObject<>`
- `UInputMappingContext` with key mappings added via `MapKey()`
- Bindings via `UEnhancedInputComponent::BindAction()`
- Separate mapping contexts for character (IMC_CityDefault) and vehicles (IMC_Vehicle)

**Mobile touch input** is dual-system:
1. `UPHCTouchInputComponent` on the character handles walk-mode joystick/look
2. `UPHCCityHUD::NativeTick()` handles driving-mode touch zones (gas/brake/steering)

### Multiplayer

- **Architecture:** Listen server (one player hosts, others connect via IP)
- **Session management:** `UPHCGameInstance::HostGame()` / `JoinGame(IP)` / `LeaveGame()`
- **City sync:** Each machine generates identical city from deterministic `FRandomStream` seed
- **Day/night sync:** Server replicates `APHCGameState::ServerDayNightTime`
- **Player replication:** `APHCPlayerState` replicates profile, room, ready status
- **Vehicle replication:** Currently disabled (`bReplicates` commented out in vehicle constructors)
- **Dedicated server target:** `PlayhouseCityServer.Target.cs` exists but server binary deployment is WIP

### Mini-Games (21 total)

All mini-games follow the same pattern:
- `APHCMiniGameMode` base class (spawned as an actor, not the framework game mode)
- `UPHCMiniGameHUD` base class for game UI
- Launched via `APHCPlayerController::LaunchGame(EPHCGameType)`
- Each has its own GameMode + HUD + supporting actors

| Category | Games |
|----------|-------|
| **Board/Card** | Bingo, ColorCards, Connect4, Ludo, Pong, Pool |
| **Action** | BabyShark, PackCatch, TempleRun, KitchenChaos, TowerDefense, CelDamage |
| **Physics** | AngryDucks, GooBridge, Jenga, Cornhole, Carrom, Tanks |
| **Puzzle/Quiz** | GuessSign, MapsQuiz, PegGame |
| **Casino** | LuckySlots, Simulator |

---

## File Structure

```
Source/PlayhouseCity/
  PlayhouseCity.h/cpp          -- Module definition, log category
  PlayhouseCity.Build.cs       -- Build rules (EnhancedInput, Networking, etc.)

  Core/
    PHCCityGameMode.h/cpp      -- Server game mode (64 lines)
    PHCCitySubsystem.h/cpp     -- City generation + day/night (~4800 lines)
    PHCPlayerController.h/cpp  -- UI management, game launch, vehicle entry/exit
    PHCGameInstance.h/cpp      -- Root instance, subsystems, multiplayer sessions
    PHCGameState.h/cpp         -- Replicated day/night time
    PHCLobbyGameMode.h/cpp     -- Legacy lobby mode (not used in city mode)
    PHCTypes.h                 -- Enums (EPHCGameType, EPHCGameplayMode), structs (FPHCPlayerProfile)

  Player/
    PHCCityCharacter.h/cpp     -- Third-person character, Enhanced Input, touch component
    PHCPlayerState.h/cpp       -- Replicated player profile/room/ready state

  Vehicle/
    PHCVehiclePawn.h/cpp       -- Lamborghini Aventador (~700 lines)
    PHCDuneBuggyPawn.h/cpp     -- Dune buggy with missiles (~700 lines)
    PHCMissileProjectile.h/cpp -- Missile fired from buggy

  UI/
    PHCCityHUD.h/cpp           -- Main 3D world HUD overlay (~1100 lines)
    PHCMainHUD.h/cpp           -- Lobby menu (phone overlay)
    PHCLeaderboardWidget.h/cpp -- Leaderboard display
    Avatar/, Economy/, Garden/, Panels/, Rooms/, Shop/  -- Sub-widgets

  Input/
    PHCTouchInputComponent.h/cpp -- Mobile virtual joystick + camera look

  Auth/                        -- Authentication manager + widget
  Economy/                     -- Currency, store management
  Network/                     -- HTTP client, WebSocket client, API cache
  Social/                      -- Follow system, player search
  Theme/                       -- UI theming system
  Rooms/                       -- Voice room manager

  Games/
    Base/PHCMiniGameMode.h/cpp -- Base class for all mini-games
    Base/PHCMiniGameHUD.h/cpp  -- Base class for mini-game UIs
    [21 game folders]          -- Each with GameMode + HUD + supporting actors
```

---

## Known Issues & Recent Changes

### Vehicle Floating Bug (FIXED)
**Problem:** When entering a vehicle, it would fly up into the sky.
**Root cause:** The vehicle `Tick()` had a ground trace (`LineTraceSingleByChannel`) that started from `Pos.Z + 10000` downward using `ECC_Visibility`. This hit building roofs (houses, shops have `QueryAndPhysics` collision) instead of the actual ground, teleporting the vehicle to rooftop height.
**Fix:** Removed all ground trace and Z-adjustment code from both vehicle Ticks. Vehicles now move on a flat plane with no vertical adjustment. This means vehicles currently don't follow terrain or interact with ramps/launch pads.

### Vehicle Camera Clipping (FIXED)
**Problem:** After entering a vehicle, the camera showed a first-person view inside the car.
**Root cause:** The vehicle's `USpringArmComponent` had `bDoCollisionTest = true`. Building meshes with collision caused the spring arm to collapse to zero length.
**Fix:** Set `bDoCollisionTest = false` on both vehicle camera booms.

### Vehicle Movement Blocked (FIXED)
**Problem:** Gas pedal didn't work - vehicle wouldn't move.
**Root cause:** `AddActorWorldOffset()` was called with `bSweep = true`. The vehicle's collision box (`SetCollisionResponseToAllChannels(ECR_Block)`) was blocked by the ground plane's collision.
**Fix:** Changed sweep to `false`. Vehicles now pass through buildings freely (no wall collision). This is a temporary fix - proper collision channels should be set up.

### Mountains Removed
53 outskirts mountains were removed from `PHCCitySubsystem::SpawnHills()` during debugging. Hills along the highway corridor and at compass points remain.

### Multiplayer Replication Disabled
Vehicle `bReplicates`, `SetReplicateMovement()`, etc. are commented out in both vehicle constructors. Character replication properties exist (`bIsSprinting` with `DOREPLIFETIME`) but character `bReplicates` is also commented out. Re-enable these when vehicle entry/exit is stable in multiplayer.

### No PlayerStart Actor
The original `APHCCityGameMode` created a `PlayerStart` during city generation. When city gen moved to `UPHCCitySubsystem`, the PlayerStart was not recreated. Characters spawn at world origin `(0,0,0)` via UE's default logic. This works because the ground plane is at Z=0, but a proper spawn point at a nice city location would be better.

---

## Conventions

- **All UI built in C++** via `WidgetTree->ConstructWidget<>` in `Initialize()` overrides. No UMG Blueprint widgets.
- **All input created at runtime** via `NewObject<UInputAction>` and `NewObject<UInputMappingContext>`. No `.uasset` input assets.
- **All meshes are engine basic shapes** (`/Engine/BasicShapes/Cube`, etc.) with `UMaterialInstanceDynamic` for colors via `SetVectorParameterValue("BaseColor", Color)`.
- **Log category:** `LogPlayhouseCity` (declared in `PlayhouseCity.h`)
- **Class prefix:** `PHC` (PlayHouseCity) for all custom classes
- **Header guards:** `#pragma once`
- **Mobile platform checks:** `#if PLATFORM_IOS || PLATFORM_ANDROID` for touch-specific code
- **No Blueprints, no .uasset files** in the gameplay layer

---

## Servers

| Server | IP | Role |
|--------|----|------|
| Production | GAME_SERVER_IP | Dedicated game server (Linux) |
| Build | BUILD_SERVER_IP | Linux build server for server binaries |
| Backend API | tfplayhouse.com | REST API + WebSocket for auth, economy, social |
