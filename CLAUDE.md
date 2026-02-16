# PlayhouseCity - Architecture & Developer Guide

## Overview

PlayhouseCity is a **UE 5.7 iOS multiplayer open-world city game** built entirely in C++ with no Blueprint assets. Players walk around a procedurally generated city, drive vehicles (Lamborghini Aventador, Dune Buggy), and play 21 mini-games. The entire city, all vehicles, buildings, lighting, and UI are constructed programmatically at runtime using basic engine meshes (`/Engine/BasicShapes/Cube`, `Cylinder`, `Sphere`, `Plane`, `Cone`).

**Key design principle:** Zero `.uasset` dependencies for gameplay code. All input mappings, materials, meshes, and widgets are created via `NewObject<>` / `CreateDefaultSubobject<>` at runtime.

**Three custom materials** are auto-created by the module startup (`PlayhouseCity.cpp`):
- `M_PHCColor` — opaque PBR (BaseColor, Metallic, Roughness, EmissiveColor)
- `M_PHCGlass` — translucent (BaseColor, Opacity, Metallic, Roughness, EmissiveColor)
- `M_PHCClearCoat` — automotive clear coat paint (BaseColor, Metallic, Roughness, ClearCoat, ClearCoatRoughness, EmissiveColor)

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
     -> SpawnOutskirts(), SpawnHills(), SpawnStarfield(), SpawnCloudLayer()

APHCPlayerController::BeginPlay()
  -> EnterWorldMode()
     -> Creates UPHCCityHUD (all widgets built in C++)
     -> Sets Game+UI input mode
```

### Core Classes

| Class | File | Role |
|-------|------|------|
| `APHCCityGameMode` | `Core/PHCCityGameMode.cpp` | Server game mode. Sets default classes, handles PostLogin/Logout. Tracks spawned vehicles. |
| `UPHCCitySubsystem` | `Core/PHCCitySubsystem.cpp` | **Largest file (~5500 lines).** `UTickableWorldSubsystem` that generates the entire city procedurally and runs the day/night cycle. Runs identically on server and all clients (deterministic seed). |
| `APHCPlayerController` | `Core/PHCPlayerController.cpp` | Player controller. Manages UI screens (auth, main HUD, city HUD), mini-game launching, and vehicle entry/exit. |
| `UPHCGameInstance` | `Core/PHCGameInstance.cpp` | Root game instance. Owns HTTP/WebSocket/Auth/Economy/Theme subsystems. Handles multiplayer session management (HostGame/JoinGame/LeaveGame). |
| `APHCGameState` | `Core/PHCGameState.cpp` | Replicates `ServerDayNightTime` so all clients synchronize lighting. |
| `APHCCityCharacter` | `Player/PHCCityCharacter.cpp` | Third-person character with spring arm camera. Enhanced Input for WASD/gamepad/touch. Has `UPHCTouchInputComponent` for mobile virtual joystick. |
| `APHCPlayerState` | `Player/PHCPlayerState.cpp` | Per-player replicated state: profile, room ID, ready status. |
| `APHCVehiclePawn` | `Vehicle/PHCVehiclePawn.cpp` | Lamborghini Aventador. Arcade physics (no UE vehicle system). Built from ~20 static mesh components. ClearCoat shading model for automotive paint. Own camera boom + Enhanced Input context. |
| `APHCDuneBuggyPawn` | `Vehicle/PHCDuneBuggyPawn.cpp` | Off-road buggy. Similar to Aventador but with roll cage, brush guard, missile launcher. ClearCoat paint. |
| `APHCMissileProjectile` | `Vehicle/PHCMissileProjectile.cpp` | Projectile fired from dune buggy. Uses `UProjectileMovementComponent`. |
| `UPHCCityHUD` | `UI/PHCCityHUD.cpp` | Main in-game HUD. All widgets constructed in `Initialize()` via `WidgetTree->ConstructWidget<>`. Has currency display, vehicle selection popup, multiplayer Host/Join buttons, mobile touch controls for walking and driving. |
| `UPHCTouchInputComponent` | `Input/PHCTouchInputComponent.cpp` | Mobile virtual joystick (left half = move, right half = camera look). Attaches to character. |

### City Generation (PHCCitySubsystem)

The city is a **24x24 grid** of blocks. Each block is ~3600 UU. Block types are determined by grid position:

- **Downtown core (blocks 7-10, 10-15):** 27 unique skyscrapers with architectural details
- **Midtown:** Tall commercial buildings, institutional buildings
- **Residential (outer ring):** Brick houses with pitched roofs, lawns, garages, cylinder chimneys with cone caps, corner pilasters, foundation bush spheres, doorway arches
- **Waterfront:** Beach, bay, ocean with wave animation
- **Industrial:** Warehouses with loading docks, cylinder smokestacks with cone caps
- **Parks:** Trees, paths, fountains, hedge sphere rows, cone topiary trees
- **Arts district:** Buildings with murals and colorful walls

Each block has roads (dark asphalt) on all four sides with sidewalks. Street lights line intersections. Palm trees along boulevards.

**Day/night cycle** runs in `Tick()` with GTA V-style graphics:
- Sun rotates with god rays (`bEnableLightShaftBloom`)
- 4-phase color grading (Dawn/Day/Sunset/Night) via `ColorGain`/`ColorOffset`
- Filmic tone mapping (FilmSlope=0.88, FilmToe=0.55, FilmShoulder=0.26)
- Chromatic aberration, film grain, depth of field
- Directional fog inscattering (warm haze at dawn/dusk)
- Cloud layer drift animation (two translucent planes at Z=15000/16500)
- Ocean wave heave (amplitude 3 UU, base Z=-8)
- Wet roads at night (roughness drops, base color darkens)
- Street lights use `USpotLightComponent` (downward cone, warm sodium)
- Window lights toggle on at night with warm glow

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

### Visual / Rendering System

The city uses a GTA V-inspired rendering pipeline, all configured programmatically:

**Post-Process Volume** (created in `SpawnSkyAndLighting()`):
- Filmic tone mapping: `FilmSlope=0.88`, `FilmToe=0.55`, `FilmShoulder=0.26`, `FilmBlackClip=0.0`, `FilmWhiteClip=0.04`
- Chromatic aberration: `SceneFringeIntensity=0.4`
- Film grain: `FilmGrainIntensity=0.06`
- Depth of field: `FocalDistance=3000`, `Fstop=22` (walking) / `Fstop=4` (driving), `SensorWidth=24.576`
- Color grading animated per time-of-day in `Tick()` via `ColorGain`/`ColorOffset`

**Lighting:**
- Directional light (sun) with god rays (`bEnableLightShaftBloom=true`, `BloomScale=1.2`)
- `USpotLightComponent` street lights (downward cone, warm sodium `FColor(255,200,120)`, outer cone 55 deg)
- Directional fog inscattering at dawn/dusk

**Materials:**
- `M_PHCColor` — standard opaque PBR for buildings, ground, etc.
- `M_PHCGlass` — translucent for glass surfaces, cloud layers, ocean
- `M_PHCClearCoat` — `MSM_ClearCoat` shading model for vehicle body panels (Metallic=0.9, Roughness=0.35, ClearCoat=1.0, ClearCoatRoughness=0.03)

**Mesh variety (de-blockify):**
All 5 basic shapes are actively used:
- `CubeMesh` — building bodies, roofs, windows, doors, awnings, roads
- `CylinderMesh` — corner pilasters, chimneys, porch posts, columns, flagpoles, awning supports, neon tubes, smokestacks, building corner edges, lamp poles
- `SphereMesh` — foundation bushes, hedge rows, rooftop domes, fountain spouts, lamp globes, starfield, palm canopy
- `ConeMesh` — chimney caps, smokestack caps, rooftop spires, topiary trees, lamp shades
- `PlaneMesh` — ground, roads, sidewalks, lawns, ocean, cloud layers, starfield

**Residential colors** are brick tones (classic red, warm red, dark red, orange, brown, tan, burgundy, terracotta) with high roughness (0.92) for matte brick texture.

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
    PHCCitySubsystem.h/cpp     -- City generation + day/night + GTA V graphics (~5500 lines)
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

## Known Issues & Open Items

### Vehicle No-Collision (temporary)
Vehicles use `AddActorWorldOffset(bSweep=false)` so they pass through buildings. Proper collision channels need to be set up. Ground trace was removed (caused floating bug), so vehicles don't follow terrain or interact with ramps.

### Multiplayer Replication Disabled
Vehicle `bReplicates`, `SetReplicateMovement()`, etc. are commented out in both vehicle constructors. Character `bReplicates` also commented out. Re-enable when vehicle entry/exit is stable in multiplayer.

### No PlayerStart Actor
Characters spawn at world origin `(0,0,0)` via UE's default logic. A proper spawn point at a nice city location would be better.

### Mountains Removed
53 outskirts mountains were removed from `SpawnHills()` during debugging. Hills along the highway corridor and at compass points remain.

### Dead Code in Vehicle Headers
`ExhaustPuffs`, `ExhaustPuffAge`, `NextExhaustPuff`, `ExhaustSpawnTimer` members exist in both `PHCVehiclePawn.h` and `PHCDuneBuggyPawn.h` but the exhaust system was removed. These should be cleaned up.

---

## Recent Changes (latest first)

### City De-Blockify
Added cylinders, spheres, and cones throughout city to break up cubic monotony:
- **Houses:** 4 corner pilasters (cylinder), cylinder chimney + cone cap, doorway arch (cylinder), foundation bushes (spheres)
- **Shops:** Corner trim cylinders, awning support posts (cylinder), neon tube (cylinder)
- **Buildings:** Corner edge cylinders, rooftop spires (cone) and domes (sphere)
- **Institutions:** Hemisphere dome on roof, flagpole cylinder
- **Warehouses:** Cone caps on smokestacks
- **Parks:** Hedge sphere rows along paths, cone topiary trees
- **Street lights:** Inverted cone lamp shade above sphere globe

### GTA V Graphics Overhaul
- Filmic tone mapping, chromatic aberration, film grain, depth of field
- 4-phase time-of-day color grading (Dawn warm pink-amber / Day subtle / Sunset golden / Night cool blue)
- God rays on directional light, directional fog inscattering
- Spot light street pools (replaced point lights), cloud layer drift
- Ocean wave animation (3 UU amplitude), wet roads at night
- ClearCoat shading model on vehicle body panels (M_PHCClearCoat material)
- Brick residential colors (8 tones), high roughness (0.92)

### PBR Material System
- All building types, ground, roads, sidewalks, vehicles, trees, and lights use proper PBR parameters (Metallic, Roughness, EmissiveColor)
- Three auto-created materials: M_PHCColor, M_PHCGlass, M_PHCClearCoat

---

## Conventions

- **All UI built in C++** via `WidgetTree->ConstructWidget<>` in `Initialize()` overrides. No UMG Blueprint widgets.
- **All input created at runtime** via `NewObject<UInputAction>` and `NewObject<UInputMappingContext>`. No `.uasset` input assets.
- **All meshes are engine basic shapes** (`/Engine/BasicShapes/Cube`, `Cylinder`, `Sphere`, `Plane`, `Cone`) with `UMaterialInstanceDynamic` for PBR via `SetVectorParameterValue("BaseColor", Color)`, `SetScalarParameterValue("Roughness", ...)`, etc.
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
