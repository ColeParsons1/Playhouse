# PlayhouseCity — Complete Architecture & Developer Guide

## Overview

PlayhouseCity is a **UE 5.7 iOS multiplayer open-world city game** built entirely in C++ with no Blueprint assets. Players walk around a procedurally generated city, drive 7 vehicles, fight NPC enemies with 6 weapon types, and play 23 mini-games. The entire city, all vehicles, buildings, lighting, and UI are constructed programmatically at runtime using basic engine meshes (`/Engine/BasicShapes/Cube`, `Cylinder`, `Sphere`, `Plane`, `Cone`).

**Key design principle:** Zero `.uasset` dependencies for gameplay code. All input mappings, materials, meshes, and widgets are created via `NewObject<>` / `CreateDefaultSubobject<>` at runtime.

**Four custom materials** are auto-created by the module startup (`PlayhouseCity.cpp`):
- `M_PHCColor` — opaque PBR (BaseColor, Metallic, Roughness, EmissiveColor)
- `M_PHCGlass` — translucent (BaseColor, Opacity, Metallic, Roughness, EmissiveColor)
- `M_PHCClearCoat` — automotive clear coat paint (BaseColor, Metallic, Roughness, ClearCoat, ClearCoatRoughness, EmissiveColor)
- `M_PHCSkin` — subsurface skin (BaseColor, Roughness, SubsurfaceColor, Opacity, EmissiveColor)

**Bundle ID:** `com.playhousecity.PlayhouseCity`
**Backend:** Django REST API + Django Channels WebSocket at `tfplayhouse.com`
**Total C++ lines:** ~55,000+

---

## Build & Deploy

```bash
# Build for iOS (incremental ~11 seconds)
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

# TestFlight Deploy (full build + upload)
./deploy_testflight.sh                    # Full build + upload
./deploy_testflight.sh --build-only       # Build IPA without uploading
./deploy_testflight.sh --upload-only      # Upload existing IPA

# Build dedicated server (Linux, on build server BUILD_SERVER_IP)
ssh root@BUILD_SERVER_IP 'cd /root/UE5 && ./Engine/Build/BatchFiles/RunUBT.sh \
  PlayhouseCityServer Linux Development \
  -Project="/root/PlayhouseCity/PlayhouseCity.uproject" -Server -NoHotReload'

# Sync source to build server
rsync -az --delete /Users/coleparsons/PlayhouseCity/Source/ \
  root@BUILD_SERVER_IP:/root/PlayhouseCity/Source/

# Deploy server binary to production
scp root@BUILD_SERVER_IP:/root/PlayhouseCity/Binaries/Linux/PlayhouseCityServer \
  /tmp/PlayhouseCityServer
scp /tmp/PlayhouseCityServer \
  root@GAME_SERVER_IP:/home/gameserver/PlayhouseCity/Binaries/Linux/PlayhouseCityServer

# Start dedicated server
ssh root@GAME_SERVER_IP 'su - gameserver -c "cd /home/gameserver/PlayhouseCity/Binaries/Linux && \
  nohup stdbuf -oL ./PlayhouseCityServer /Game/Maps/City_Main -server -log -port=7777 \
  -nullrhi -nosplash -nosound -nopause -NoTextureStreaming > /home/gameserver/server.log 2>&1 &"'
```

**Important:** Always use `RunUBT.sh` for builds, NOT `xcodebuild`. Build time is ~11 seconds for incremental changes.

### Deploying Staged Builds to Device (Critical Steps)

After `RunUAT BuildCookRun -cook -stage`, the staged `.app` requires **three manual steps** before it will run on device:

```bash
APP="/Users/coleparsons/PlayhouseCity/Saved/StagedBuilds/IOS/PlayhouseCity.app"
XCENT="/Users/coleparsons/PlayhouseCity/Saved/StagedBuilds/PlayhouseCity (IOS).build/IOS/PlayhouseCity.build/PlayhouseCity.app.xcent"

# 1. Clear uecommandline.txt — contains -project path that doesn't exist on device
#    WITHOUT THIS: app crashes with "Failed to open descriptor file"
printf '' > "$APP/uecommandline.txt"

# 2. Delete Saved/Cooked BEFORE signing — codesign needs ~500MB+ free disk space
#    WITHOUT THIS: codesign fails with "internal error in Code Signing subsystem"
rm -rf /Users/coleparsons/PlayhouseCity/Saved/Cooked

# 3. Re-sign frameworks then main app with entitlements from the build
for FW in "$APP/Frameworks"/*.framework; do
    /usr/bin/codesign --force --sign 482BFA54A521F21BAA12ECC68F45469EDF3F4F48 --timestamp=none "$FW"
done
/usr/bin/codesign --force --sign 482BFA54A521F21BAA12ECC68F45469EDF3F4F48 \
    --entitlements "$XCENT" --timestamp=none --generate-entitlement-der "$APP"

# 4. Install and launch
xcrun devicectl device install app --device 00008130-000251843CF3803A "$APP"
xcrun devicectl device process launch --device 00008130-000251843CF3803A com.playhousecity.PlayhouseCity
```

**Build artifacts on external drive:** Heavy build outputs are symlinked to `/Volumes/Untitled/PlayhouseCity/` (466 GB external drive) to avoid filling the 228 GB internal SSD:
- `Saved/Cooked` -> `/Volumes/Untitled/PlayhouseCity/Cooked` (~2.5 GB per cook)
- `Saved/StagedBuilds` -> `/Volumes/Untitled/PlayhouseCity/StagedBuilds` (~3.3 GB per stage)
- `TestFlightBuild` -> `/Volumes/Untitled/PlayhouseCity/TestFlightBuild`

**Important:** The external drive must be mounted at `/Volumes/Untitled` before running builds. The symlinks are transparent — all existing build commands and paths work unchanged.

---

## Architecture

### Startup Flow

```
UPHCGameInstance::Init()
  -> Network version override (FNetworkVersion, fixed version=1)
  -> Creates subsystems: HttpClient, WebSocketClient, ThemeManager, EconomyManager, AuthManager

APHCCityGameMode (server authority)
  -> Sets DefaultPawnClass = APHCCityCharacter
  -> Sets PlayerControllerClass = APHCPlayerController
  -> Sets PlayerStateClass = APHCPlayerState
  -> Sets GameStateClass = APHCGameState

UPHCCitySubsystem::OnWorldBeginPlay() (runs on ALL machines independently)
  -> Skips on NM_DedicatedServer (no visuals needed)
  -> Skips if bPendingServerConnect (avoids double city gen during ClientTravel)
  -> SpawnCityEnvironment()
     -> SpawnGround(), SpawnOceanAndBeach(), SpawnSkyAndLighting()
     -> SpawnCityGrid() (24x24 block grid with roads, sidewalks, buildings)
     -> SpawnSkyscrapers() (27 unique downtown towers)
     -> SpawnStreetLights(), SpawnPalmTrees(), SpawnRamps()
     -> SpawnOutskirts(), SpawnHills(), SpawnStarfield(), SpawnCloudLayer()
     -> SpawnGiantCrab() (15 crabs), SpawnNPCPlayers() (5 NPCs)
     -> SpawnNPCHelicopter(), SpawnGoldCoins() (14 coins)
     -> SpawnLightRail(), SpawnLangtree(), SpawnTerrainGrid()
     -> SpawnGrassSystem(), SpawnDirtBikePark(), SpawnJungle()

APHCPlayerController::BeginPlay()
  -> Skips UI on server-side controllers (!IsLocalController())
  -> EnterWorldMode()
     -> Creates UPHCCityHUD (all widgets built in C++)
     -> Sets Game+UI input mode

APHCCityCharacter::BeginPlay()
  -> Skips mesh/material setup on NM_DedicatedServer
  -> Creates input assets, registers mapping context
  -> Applies PBR materials to 45 mesh components
```

### Core Classes

| Class | File | Lines | Role |
|-------|------|-------|------|
| `FPlayhouseCityModule` | `PlayhouseCity.cpp` | 435 | Module startup, creates 4 materials with Animal Crossing world curvature |
| `APHCCityGameMode` | `Core/PHCCityGameMode.cpp` | 91 | Server game mode. Sets default classes, handles PostLogin/Logout. Tracks spawned vehicles. |
| `UPHCCitySubsystem` | `Core/PHCCitySubsystem.cpp` | **16,620** | `UTickableWorldSubsystem` — generates entire city procedurally, runs day/night cycle, manages all NPC systems |
| `APHCPlayerController` | `Core/PHCPlayerController.cpp` | 952 | Player controller. Manages UI screens, mini-game launching, vehicle entry/exit, server RPCs |
| `UPHCGameInstance` | `Core/PHCGameInstance.cpp` | 277 | Root game instance. Owns HTTP/WebSocket/Auth/Economy/Theme subsystems. Multiplayer sessions. Network version override. |
| `APHCGameState` | `Core/PHCGameState.cpp` | 15 | Replicates `ServerDayNightTime` for lighting sync |
| `APHCCityCharacter` | `Player/PHCCityCharacter.cpp` | 2,424 | 45-component character with procedural animation, 6 weapons, jetpack, COD-style combat |
| `APHCPlayerState` | `Player/PHCPlayerState.cpp` | 84 | Replicated profile, room ID, ready status |
| `UPHCCityHUD` | `UI/PHCCityHUD.cpp` | 3,111 | Main in-game HUD — all widgets, vehicle selection, weapon selection, mobile touch controls |
| `UPHCTouchInputComponent` | `Input/PHCTouchInputComponent.cpp` | 226 | Mobile virtual joystick (left=move, right=camera look) |

---

## City Generation (PHCCitySubsystem)

### Grid & Dimensions
- **24x24 block grid**, each block ~3600 UU
- Ground plane at (0,0,0), scaled (1400,1400,1). Ground surface at **Z=0**
- **Deterministic:** `FRandomStream CityRandom` (seed 42) + `DetailRandom` (seed 137)

### District Types
- **Downtown Core (blocks 7-10, 10-15):** 27 unique skyscrapers
- **Midtown:** Tall commercial/institutional buildings
- **Residential (outer ring):** Brick houses (8 tones, roughness 0.92), pitched roofs, cylinder chimneys + cone caps, corner pilasters, foundation bush spheres
- **Waterfront:** Beach, bay, ocean with wave animation (amplitude 3 UU)
- **Industrial:** Warehouses, cylinder smokestacks + cone caps
- **Parks:** Trees, hedge sphere rows, cone topiary, fountains
- **Arts District:** Colorful murals

### Day/Night Cycle
- `DayNightTime`: 0=sunrise, **0.25=noon (default start)**, 0.5=sunset, 0.75=midnight
- `bNightModeLocked`: toggleable via HUD button
- 4-phase color grading: Dawn (warm pink-amber) / Day (subtle) / Sunset (golden) / Night (cool blue)
- Sun god rays (`bEnableLightShaftBloom=true`, `BloomScale=1.2`)
- Filmic tone mapping: Slope=0.88, Toe=0.55, Shoulder=0.26
- Chromatic aberration: 0.4, Film grain: 0.06, DoF: FocalDistance=3000
- Street lights: USpotLightComponent, warm sodium FColor(255,200,120), outer cone 55°
- Window glow at night, wet roads at night, cloud drift animation

### Natural Disasters
| Type | Duration | Behavior |
|------|----------|----------|
| Tornado | 30s | Spinning actor circling the city |
| Earthquake | 20s | Camera shake, intensity phasing |
| Thunderstorm | 45s | Lightning flash effects |
| Meteor Shower | 40s | Spawned meteor actors with velocity |
Auto-trigger cooldown: 120-300s. Also manual via HUD disaster buttons.

---

## Vehicle System (7 Vehicles)

All vehicles use custom arcade physics (NOT UE vehicle system). `SetSimulatePhysics(false)`. Movement via `AddActorWorldOffset(ForwardVector * Speed * DeltaTime, bSweep=false)`. All use M_PHCClearCoat material.

### Speed Comparison

| Vehicle | MaxSpeed | Acceleration | Steering | Weapons | Components |
|---------|----------|-------------|----------|---------|------------|
| SR-71 Blackbird | 28,700 | 13,000 | 100 | Rockets + Bomb | 27 |
| F-22 Raptor | 14,350 | 6,500 | 120 | Rockets + Bomb | 23 |
| Dirt Bike | 8,125 | 5,625 | 200 | None | 22 |
| Aventador | 8,375 | 5,500 | 150 | None | 20 |
| Black Hawk | 7,175 | 3,250 | 120 | Rockets + Bomb | 33 |
| Dune Buggy | 5,100 | 3,200 | 180 | Missiles | 24 |
| M60 Tank | 3,000 | 1,800 | 100 | 105mm Cannon | 40+ |

### Vehicle Details

- **Lamborghini Aventador** (`PHCVehiclePawn`): Pure driving, ClearCoat paint, 20 components
- **Dune Buggy** (`PHCDuneBuggyPawn`): Roll cage, brush guard, missile launcher (FireCooldown=0.05s)
- **F-22 Raptor** (`PHCRaptorPawn`): Full 3D flight (pitch ±60°), rockets + gravity bomb (spawns APHCNukeExplosion, 3s cooldown), afterburner animation
- **M60 Tank** (`PHCTankPawn`): Independent turret yaw (60°/s), 105mm cannon with recoil animation (2s cooldown), 6 road wheels per side
- **Dirt Bike** (`PHCDirtBikePawn`): Player visible as rider, fastest steering
- **SR-71 Blackbird** (`PHCBlackbirdPawn`): Double Raptor speed, gold-plated, stored in underground airport hangar
- **Black Hawk UH-60** (`PHCBlackHawkPawn`): 4 main rotor blades + 2 tail blades (animated), rockets + bomb, also used as NPC patrol helicopter

### Flight Mechanics (Raptor, Blackbird, Black Hawk)
- PitchSpeed=80°/s, MaxPitchAngle=60°, AutoLevelSpeed=2°/s, MinAltitude=50 UU
- Pitch controls: Space/Shift keys or left touch vertical axis

### Vehicle Entry Flow
1. Player taps "Car" button → vehicle selection popup (7 options)
2. Selects vehicle → `PHCPlayerController::SpawnAndEnterVehicle(type)`
3. Vehicle spawns 300 UU ahead, 50 UU above character
4. Character hidden + collision disabled, `UnPossess()` → `Possess()` vehicle
5. HUD switches to driving mode (gas/brake/steering touch zones)

### Vehicle Type Mapping
| Type ID | Vehicle | HUD Button |
|---------|---------|------------|
| 0 | Aventador | SelectAventadorBtn |
| 1 | Dune Buggy | SelectBuggyBtn |
| 2 | F-22 Raptor | SelectRaptorBtn |
| 3 | M60 Tank | SelectTankBtn |
| 4 | Dirt Bike | SelectDirtBikeBtn |
| 5 | Black Hawk | SelectBlackHawkBtn |
| — | Jetpack | SelectJetpackBtn |

### Support Actors
- **APHCMissileProjectile:** UProjectileMovementComponent, Speed=15000, LifeSpan=1.5s, trail system, RPG rocket mode, manual gravity mode, bomb detonation at altitude
- **APHCNukeExplosion:** 5s duration, FireballCore, 3 FlameBursts, SmokePlume, DebrisRing, ShockwaveRing, 10 Embers, all animated per-frame

---

## Character System (PHCCityCharacter)

### 45 Mesh Components (Fortnite-quality organic character)
- **Head (13):** Head, Jaw, Neck, Eyes (L/R whites + pupils), Nose, Mouth, Ears (L/R), Brows (L/R)
- **Hair (4):** Top, Back, SideL, SideR
- **Torso (6):** Chest, Abs, Pelvis, Collar, Belt, BeltBuckle
- **Arms (10):** L/R × Shoulder, UpperArm, Elbow, Forearm, Hand
- **Legs (12):** L/R × Thigh, Knee, Shin, Foot, Toe, Sole
- 180 UU tall, root at hips (Z=0 relative)

### Color Palette
| Part | Color (RGB) | Roughness |
|------|-------------|-----------|
| Skin | (0.55, 0.38, 0.26) | 0.45 |
| Hair | (0.02, 0.02, 0.03) | 0.85 |
| Shirt | (0.12, 0.38, 0.18) | 0.82 |
| Denim | (0.08, 0.10, 0.20) | 0.78 |
| Belt | (0.06, 0.04, 0.03) | 0.35 |
| Shoes | (0.92, 0.92, 0.90) | 0.45 |
| Sole | (0.05, 0.05, 0.05) | — |

### Animation System
- Procedural walk animation via `WalkCyclePhase` / `WalkAnimBlend`
- Spring damper system: LeanPitch/Roll, HandLag L/R, HairLag, WeaponSway, WeaponKick
- Idle breathing animation

### Movement
- Derives from `APawn` (NOT ACharacter) — manual movement, no capsule/CMC
- `UBoxComponent` collision (22×26×90)
- Manual gravity + jump via `ManualVelocityZ`
- WalkSpeed=1788, SprintSpeed=2000
- `bIsSprinting` (Replicated)

### COD-Style Combat
- **Recoil:** Per-shot pitch kick + yaw scatter, auto-recovery (speed=6)
- **ADS:** FOV hip=90 → aim=65, sensitivity ×0.6, interpolation speed=12
- **Bullet Spread:** Hip=3.5° half-angle, ADS=0.8°
- **Hit Marker:** White X flash at crosshair (0.25s timer)
- **Muzzle Flash:** Dynamic UPointLightComponent

### Jetpack System
- 9 components: Body, NozzleL/R, FlameL/R, StrapL/R, GlowL/R (UPointLightComponent)
- `bJetpackEquipped`, `JetpackVerticalInput` (+1 ascend, -1 descend)

### Health
- Health=100, MaxHealth=100, CrabKills (int32), Deaths (int32)
- DamageFlashTimer for red screen overlay
- On death: increment Deaths, restore MaxHealth, teleport to (0,0,100)

---

## Weapon System (6 Weapons)

### Weapon Type IDs
`CurrentWeaponType`: -1=None, 0=Minigun, 1=RPG/Bazooka, 2=Sword, 3=Scythe (default), 4=Dagger, 5=AK-47

### Damage Table

| Weapon | Damage/Hit | Hits to Kill (100hp) | Range | Mechanism |
|--------|-----------|---------------------|-------|-----------|
| Minigun | 8 (hitscan) | 13 | Infinite (line trace) | Continuous fire, spinning barrel |
| AK-47 | 12 (hitscan) | 9 | Infinite (line trace) | Continuous fire |
| RPG | 100 | 1 | Projectile (15000 speed) | GTA V explosion on impact |
| Sword | 50 | 2 | Melee | Sweep attack |
| Scythe | 34 | 3 | Melee | Swing animation |
| Dagger | Instant kill | 1 | 800 UU, 120° cone | 0.25s stab |

### Weapon Meshes
- **Minigun (3):** Body, Barrel (spinning), Muzzle
- **AK-47 (5):** Body, Barrel, Magazine, Stock, Muzzle
- **RPG/Bazooka (7):** Tube, RearEnd, FrontSight, RearSight, Grip, Scope, Muzzle
- **Scythe (3):** Handle, Blade, BladeEdge
- **Dagger (3):** Handle, Blade, Guard

### NPC Helicopter Special Damage
- NPCHeliHealth=240: Minigun needs 30 hits, AK-47 needs 20 hits, RPG needs 3 hits

### Crab Contact Damage to Player
- 25 damage per contact, 450 UU radius, 0.6s cooldown (4 hits to kill player)

---

## NPC Systems

### Giant Crabs (15 enemies)
- **Data:** `FCrabData` — Actor, HipJoints[], KneeJoints[], AnimTime, Health=100, GlowLight (intensity=4000)
- **Visibility:** Night-only (sun sine < -0.3 or bNightModeLocked)
- **AI:** Chase player at 2094 UU/s, avoid safe zones (sand castle radius 20000, underground airport)
- **Contact:** 25 damage, 450 UU radius, 0.6s cooldown
- **Death:** Explosion, +20 coins per kill
- **Wave System:** When all 15 dead → 60s rest → all respawn

### NPC Patrol Helicopter
- Uses `APHCBlackHawkPawn` as NPC, Health=240
- **Visibility:** Daytime only
- **Patrol:** Circular path at fixed altitude, `NPCHeliPatrolAngle` animated
- **Crash:** On death: falls (800 + timer×200 UU/s), tumbles, spawns APHCNukeExplosion on ground impact
- **Respawn:** 60s timer after crash

### NPC Player Characters (5 wanderers)
- **Data:** `FNPCPlayerData` — 28 body mesh components, GlowLight (cyan, intensity=8000, attenuation=600), Health=100, WanderTarget, WanderWaitTimer
- **Visibility:** Always visible (cyan underglow for night visibility)
- **AI:** Random wander, walk toward target, idle pauses
- **Spawn positions:** Close to origin: (500,300), (-400,600), (800,-500), (-600,-400), (200,900) at Z=75
- **Damage:** Takes all weapon types (hitscan, melee, projectile proximity)
- **Death:** Explosion, respawn at original position

---

## HUD System (PHCCityHUD)

### Top-Level Elements
- Currency display (coins/gems), Menu button, Location/username text
- Health bar (BG + fill + text), K/D display, Player count

### Vehicle Selection Popup
- 7 buttons: Aventador, Buggy, Raptor, Jetpack, Tank, Dirt Bike, Black Hawk

### Weapon Selection Popup
- 6 buttons: Minigun, RPG, Sword, Scythe, Dagger, AK-47
- Fire button for shooting

### Disaster Selection
- 5 buttons: Tornado, Earthquake, Thunderstorm, Meteor, Stop

### Night Mode / Camera Mode Toggles

### Driving Mode Controls (Mobile Touch)
- Steering panel, Gas pedal, Brake pedal, Exit button, Fire button, Drop bomb button

### Walking Mode Controls (Mobile Touch)
- Left half: movement joystick
- Right half: camera look

### Combat UI
- Crosshair (dot + 4 arms), Hit marker (4 diagonal lines), ADS button
- Damage flash overlay (full-screen red)

### Multiplayer UI
- Host/Join buttons, IP input, Connect/Cancel

---

## Input System

### Enhanced Input (All Runtime-Created — No .uasset Files)

#### Character (IMC_CityDefault)
| Action | Keys | Type |
|--------|------|------|
| Move | WASD | Axis2D |
| Look | Mouse XY | Axis2D |
| Jump | Spacebar | Bool |
| Sprint | Left Shift | Bool (Started/Completed) |
| Interact | E | Bool |
| ADS | Right Mouse | Bool (Started/Completed) |

#### Vehicles (IMC_Vehicle)
| Action | Keys | Type |
|--------|------|------|
| Throttle | W / Up | Bool |
| Brake | S / Down | Bool |
| Steer | A/D or Left/Right | Axis1D |
| Exit | F | Bool |
| PitchUp (flight) | Space | Bool |
| PitchDown (flight) | Left Shift | Bool |

### Mobile Touch
1. **UPHCTouchInputComponent:** Left half=movement joystick (deadzone=0.02, radius=0.08), Right half=camera look (sensitivity=0.15)
2. **PHCCityHUD::NativeTick():** Driving mode touch zones for steering/gas/brake

---

## Material System

### Custom Materials (5, created by StartupModule in PlayhouseCity.cpp)

All include **Animal Crossing-style world curvature** WorldPositionOffset (curvature=-0.000020).

| Material | Type | Key Params | Usage |
|----------|------|-----------|-------|
| M_PHCColor | Opaque PBR | BaseColor, Metallic, Roughness, EmissiveColor + Custom HLSL procedural detail | Buildings, fallback for large meshes |
| M_PHCGlass | Translucent | BaseColor, Opacity, Metallic, Roughness, EmissiveColor | Glass, clouds, ocean, canopies |
| M_PHCClearCoat | ClearCoat | BaseColor, Metallic(0.9), ClearCoat(1.0), ClearCoatRoughness(0.03) | Vehicle body panels |
| M_PHCSkin | Subsurface | BaseColor, Roughness, SubsurfaceColor, Opacity | Character skin |
| M_PHCTextured | Opaque PBR | BaseColorTex, NormalTex, RoughnessTex, Tiling(scalar), Metallic | Megascans texture sampling (legacy, mostly replaced by Fab MI approach) |

**Important:** M_PHCColor and M_PHCTextured `.uasset` files are created by `StartupModule` only if they don't exist. If you change the material code, **delete the .uasset** and run the cook **twice** (first cook creates the file, second cook packs it).

### Fab Megascans Material System (production approach for iOS)

On iOS, `WITH_EDITORONLY_DATA=0` so materials cannot be created at runtime. The **Fab MI approach** is the only way to get 4K textures on device:

1. Import textures via **Fab > My Library > Add to Project** in UE editor
2. Fab creates a `MI_xxxxx.uasset` Material Instance with pre-compiled iOS Metal shaders
3. Code loads the MI via `LoadObject<UMaterialInterface>` and applies with `SetMaterial(0, MI)`
4. For texture swapping: create `UMaterialInstanceDynamic` from the Fab MI and override `BaseColorTexture`, `NormalTexture`, `MetallicRoughnessTexture` params

**Key rule:** Fab MIs use **mesh UVs** (0-1 range). On large meshes (scale > 50x), the texture stretches/smears. Large meshes fall back to `ApplyPBR` (M_PHCColor with world-position procedural detail).

### Fab MIDs (created in LoadMegascansTextures)

| MID | Base MI | Textures Swapped | Applied to |
|-----|---------|-----------------|------------|
| FabGrassMI | MI_oeeb70 (Uncut Grass) | None (used directly) | Ground slabs, lawns, parks, terrain grass chunks |
| FabSandMID | MI_oeeb70 | Rippled Sand BC/N/R | Beaches, sand castle, desert terrain chunks, shore strips |
| FabConcreteMID | MI_oeeb70 | Cast In Situ Concrete BC/N/R | Sidewalks, curbs, park paths |
| FabRockMID | MI_oeeb70 | Mossy Rocky Ground BC/N/R | Rock clutter, rocky hills, mountains, terrain rock chunks |
| FabBarkMID | MI_oeeb70 | Tree Bark BC/N/R | Palm tree trunk segments |
| FabWildGrassMID | MI_oeeb70 | Wild Grass BC/N/R | Grass blade HISMs, hedges, topiary, green hills |
| FabRoadMID | MI_sk0mci2c (Asphalt) | None (used directly) | Roads |
| FabStoneTilesMID | MI_oeeb70 | Stone Tiles Facade BC/N/R | Pyramid limestone blocks (small scale only) |

### Megascans Texture Assets

**Fab-imported (Content/Fab/Megascans/Surfaces/):**
- Uncut Grass (`oeeb70`) — base MI for all MID swaps
- Asphalt (`sk0mci2c`) — roads
- Stone Tiles Facade (`ub4nbiag`) — pyramid blocks
- Muddy Stone Wall (`uekoajlcw`) — imported, not yet applied

**Legacy imports (Content/Megascans/):**
- Grass (`ofivdfoh2`), Mossy Rocky Ground (`vcqhcig`), Rippled Sand (`vd3lecfs`)
- Cast In Situ Concrete (`uhwnbdoew`), Fine American Road (`sjfnch0a`)
- Tree Bark (`vimmdcofw`), Wild Grass (`xbreagf`)

### Build Workflow for Textures

```bash
# 1. Build code
RunUBT.sh PlayhouseCity IOS Development ...

# 2. Cook content (must use -build flag so binary matches)
RunUAT.sh BuildCookRun -build -cook -stage -pak -platform=IOS -clientconfig=Development

# 3. Deploy staged build (has correct binary + cooked paks)
xcrun devicectl device install app --device <ID> Saved/StagedBuilds/IOS/PlayhouseCity.app

# If you deleted a material .uasset, cook TWICE:
RunUAT.sh BuildCookRun ...  # First cook: StartupModule recreates the .uasset
RunUAT.sh BuildCookRun ...  # Second cook: packs the new .uasset into the pak
```

**Important:** `RunUAT BuildCookRun -skipbuild` produces a DIFFERENT binary than `RunUBT`. Always use `-build` with the cook to ensure the staged binary has your latest code.

---

## Multiplayer

### Architecture
- **Listen server model:** One player hosts, others connect via IP
- **Dedicated server:** Linux binary on GAME_SERVER_IP, port 7777
- **Session management:** `UPHCGameInstance::HostGame()` / `JoinGame(IP)` / `LeaveGame()`
- **Network version = git commit SHA:** `PlayhouseCity.Build.cs` reads `.git/HEAD` at compile time and injects the first 12 chars of the commit hash as `PHC_GIT_SHA`. `PHCGameInstance::Init` hashes that string for the network version and requires `LocalVersion == RemoteVersion`. **Effect: iOS / Mac / Windows can only cross-play when all three were built from the same commit.** Mismatch produces a clean "incompatible version" handshake rejection instead of silent replication breakage.

### Cross-Platform Playtest Workflow (iOS + Mac + Windows) — READ THIS FIRST

**TL;DR for the Claude session picking this up on the other side:** the user (on Windows, branch `tolsdev`) and Cole (on Mac, branch `DevCole`) could not cross-play because each platform was being built from a different commit. Replication layouts diverged silently. Fix landed in `tolsdev` and you (Cole's Claude) are being asked to replicate the build on this side so the two platforms can actually connect.

#### What changed (files to know about)

1. **`Source/PlayhouseCity/PlayhouseCity.Build.cs`** — reads `.git/HEAD` at compile time, injects first 12 chars of commit SHA as `PHC_GIT_SHA` preprocessor define. No action needed from you; it just works during build.
2. **`Source/PlayhouseCity/Core/PHCGameInstance.cpp::Init`** — network version = `GetTypeHash(FString(TEXT(PHC_GIT_SHA)))`. `IsNetworkCompatibleOverride` now returns `LocalVersion == RemoteVersion` (previously `true` unconditionally). **Effect: iOS/Mac/Windows only handshake when built from the identical commit.** Mismatch = clean "incompatible version" rejection in logs instead of mystery mid-game disconnects.
3. **`Source/PlayhouseCity/Core/PHCCitySubsystem.{h,cpp}`** — new public method `EnsureCityGenerated()`. Called from `HostGame` to force city generation on cooked hosts (previously `bPendingServerConnect=true` made `OnWorldBeginPlay` skip generation, so a cooked host would listen on an empty map).
4. **`Source/PlayhouseCity/Core/PHCGameInstance.cpp::HostGame`** — now sets `bPendingServerConnect=false` and calls `City->EnsureCityGenerated()` before `World->Listen(7777)`.
5. **`Config/DefaultEngine.ini`** — iOS `AdditionalPlistData` gained `NSLocalNetworkUsageDescription` + `NSBonjourServices`. Required on iOS 14+ for LAN connections. iOS will prompt user on first host/join; if denied, LAN is dead forever until they go into Settings.

#### The one rule for cross-play to work

**Everyone on the same commit, full rebuild, fresh install.** The SHA check is a gatekeeper, not a sync mechanism. Pulling code without rebuilding = useless (the SHA is baked into the binary at compile time).

#### Pre-session checklist (the Claude on each side should run this)

```bash
# 1. Get the shared commit. If user's branches have diverged (tolsdev vs DevCole),
#    merge them into a playtest commit FIRST and push. Don't build from divergent
#    branches expecting cross-play to work — it won't.
git fetch --all
git checkout <shared-branch>         # e.g. tolsdev after merge
git pull

# 2. CONFIRM both sides are on the exact same commit before building.
git rev-parse --short=12 HEAD
# ^ This 12-char hash MUST match on both user's machine and Cole's machine.
#   If they don't match, stop — fix the branch situation first.

# 3. Full rebuild on this platform. On Mac (this side), that means:
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/RunUBT.sh" \
  PlayhouseCity IOS Development \
  -Project="/Users/coleparsons/PlayhouseCity/PlayhouseCity.uproject" \
  -Game -NoHotReload
# (Plus the staged-build + codesign + install steps from the "Build & Deploy"
#  section above — the uecommandline.txt clear, the Saved/Cooked delete before
#  signing, and the framework re-sign are all still required.)

# For Mac play:
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/RunUBT.sh" \
  PlayhouseCity Mac Development \
  -Project="/Users/coleparsons/PlayhouseCity/PlayhouseCity.uproject" \
  -Game -NoHotReload

# 4. Install on every test device. Old binaries are not compatible — they
#    have a different PHC_GIT_SHA baked in and will be rejected at handshake.
```

#### If SHAs match but cross-play still fails

Check in this order:

1. **Engine patch parity.** All platforms on same UE 5.7.x (5.7.3 on Mac, same on Windows, same source build on Linux dedicated server if used). Patch-level diffs can break CoreUObject layout even with the SHA check passing.
2. **Windows firewall.** First `HostGame` on Windows pops a Defender prompt on port 7777. If the user clicks Cancel or dismisses it, UDP 7777 inbound is silently blocked forever. Ask the user to check Windows Defender Firewall → inbound rules → allow the PlayhouseCity executable.
3. **iOS Local Network permission.** First run after this fix, iOS shows a Local Network prompt referencing the new `NSLocalNetworkUsageDescription`. If denied, LAN host/join fails. User must toggle it on in iOS Settings → Privacy → Local Network → PlayhouseCity.
4. **IPv4 address.** Have the host read their IPv4 (System Settings → Network on Mac, `ipconfig` on Windows, Settings → Wi-Fi → i → IP Address on iOS). If they paste an IPv6, `ClientTravel` will fail.
5. **Same Wi-Fi network.** No hotel captive portals, no "guest network" isolation, no VPN splitting traffic.
6. **UE built-in VOIP vs Agora conflict** (known cruft). `DefaultEngine.ini` has `[Voice] bEnabled=true` and `bHasVoiceEnabled=true` but the project uses Agora for voice (off-band). UE's built-in VOIP tries to open a channel through OSS on iOS (Game Center) and can stall the connection. If you see hangs during `ClientTravel` on iOS specifically, set both to `false` and retest.

#### DO NOT

- Do not revert `IsNetworkCompatibleOverride` back to always returning `true`. That hid the exact bug we just fixed and produces silent replication breakage.
- Do not re-introduce the `bPendingServerConnect`-skip in `OnWorldBeginPlay` without also wiring city generation into whatever new entry path you add. `HostGame` currently depends on `EnsureCityGenerated`.
- Do not bump `PHC_GIT_SHA` manually or fake it. The whole point is that it's derived from actual git state.

#### Branch hygiene (for the humans, tell them if asked)

- User pushes to `tolsdev` only (per user's standing instruction — do not push to `DevCole` or other branches from user's machine).
- Cole pushes to `DevCole`.
- For cross-play sessions: merge both branches into one commit, push, both pull, both rebuild. Easiest cadence is a dedicated `playtest` branch that gets fast-forwarded from both sides before a session.

### City Sync
- Each machine generates identical city from deterministic `FRandomStream` seeds — **no city data replicated**
- Server replicates `APHCGameState::ServerDayNightTime` for lighting sync

### Replication
| Class | Replicated Properties |
|-------|----------------------|
| APHCGameState | ServerDayNightTime |
| APHCPlayerState | PlayerProfile, CurrentRoomId, bIsReady |
| APHCCityCharacter | bIsSprinting |
| Vehicle Pawns | bIsOccupied |

### RPCs
| RPC | Direction | Reliability | Purpose |
|-----|-----------|------------|---------|
| ServerSetThrottle/Steering/Pitch | Server | Unreliable | Vehicle input sync |
| ServerFireMissile/Rocket/Cannon | Server | Reliable | Vehicle weapon fire |
| ServerDropBomb | Server | Reliable | Gravity bomb |
| ServerSpawnAndEnterVehicle | Server | Reliable | Vehicle entry |
| ServerExitVehicle | Server | Reliable | Vehicle exit |
| ClientOnVehicleEntered/Exited | Client | Reliable | HUD mode switch |

### Dedicated Server Notes
- Auto-connect currently disabled (vehicles need replication first)
- `PHCCitySubsystem` skips city generation on NM_DedicatedServer
- `PHCPlayerController` skips UI on non-local controllers
- `PHCCityCharacter` skips mesh/material setup on dedicated server
- `bPendingServerConnect` flag defers initial city gen during ClientTravel

---

## Servers

| Server | IP | Role |
|--------|-----|------|
| **Production** | GAME_SERVER_IP | Dedicated game server (Linux), runs as `gameserver` user |
| **Build** | BUILD_SERVER_IP | UE5 source build at `/root/UE5/`, project at `/root/PlayhouseCity/` |
| **Backend API** | tfplayhouse.com | Django REST API + WebSocket (auth, economy, social, rooms) |

### Server Launch Command
```bash
stdbuf -oL ./PlayhouseCityServer /Game/Maps/City_Main -server -log -port=7777 \
  -nullrhi -nosplash -nosound -nopause -NoTextureStreaming
```

### TestFlight Configuration
- Distribution cert SHA-1: `REDACTED_CERT_SHA1`
- API Key ID: `REDACTED_KEY_ID`, Issuer: `REDACTED_ISSUER_ID`
- Uses `RunUAT.sh BuildCookRun -platform=IOS -clientconfig=Shipping -distribution`

---

## Mini-Games (23 Games)

All extend `APHCMiniGameMode` (base) + `UPHCMiniGameHUD` (base HUD). Launched via `APHCPlayerController::LaunchGame(EPHCGameType)`.

| # | Game | Folder | Category |
|---|------|--------|----------|
| 1 | Angry Ducks | AngryDucks | Physics |
| 2 | Baby Shark | BabyShark | Action |
| 3 | Bingo | Bingo | Board |
| 4 | Carrom | Carrom | Physics |
| 5 | Cel Damage | CelDamage | Action |
| 6 | Color Cards | ColorCards | Card |
| 7 | Connect 4 | Connect4 | Board |
| 8 | Cornhole | Cornhole | Physics |
| 9 | Goo Bridge | GooBridge | Physics |
| 10 | Guess Sign | GuessSign | Puzzle |
| 11 | Jenga | Jenga | Physics |
| 12 | Kitchen Chaos | KitchenChaos | Action |
| 13 | Ludo | Ludo | Board |
| 14 | Lucky Slots | LuckySlots | Casino |
| 15 | Maps Quiz | MapsQuiz | Quiz |
| 16 | Pack Catch | PackCatch | Action |
| 17 | Peg Game | PegGame | Puzzle |
| 18 | Pong | Pong | Board |
| 19 | Pool | Pool | Board |
| 20 | Simulator | Simulator | Sim |
| 21 | Tanks | Tanks | Physics |
| 22 | Temple Run | TempleRun | Action |
| 23 | Tower Defense | TowerDefense | Action |

---

## File Structure

```
Source/PlayhouseCity/
  PlayhouseCity.h/cpp          -- Module definition, log category, 4 material creation (435 lines)
  PlayhouseCity.Build.cs       -- Build rules (C++20, EnhancedInput, Networking, ProceduralMeshComponent)

  Core/
    PHCCityGameMode.h/cpp      -- Server game mode (91 lines)
    PHCCitySubsystem.h/cpp     -- City generation + day/night + NPCs + disasters (16,620 lines)
    PHCPlayerController.h/cpp  -- UI management, game launch, vehicle entry/exit (952 lines)
    PHCGameInstance.h/cpp      -- Root instance, subsystems, multiplayer, network version (277 lines)
    PHCGameState.h/cpp         -- Replicated day/night time (15 lines)
    PHCLobbyGameMode.h/cpp     -- Legacy lobby mode (21 lines)
    PHCTypes.h                 -- Enums, structs (428 lines)

  Player/
    PHCCityCharacter.h/cpp     -- 45-component character, 6 weapons, jetpack (2,424 lines)
    PHCPlayerState.h/cpp       -- Replicated player profile/room/ready (84 lines)

  Vehicle/
    PHCVehiclePawn.h/cpp       -- Lamborghini Aventador (827 lines)
    PHCDuneBuggyPawn.h/cpp     -- Dune buggy with missiles (817 lines)
    PHCRaptorPawn.h/cpp        -- F-22 Raptor fighter jet (986 lines)
    PHCTankPawn.h/cpp          -- M60A1 Tank (985 lines)
    PHCDirtBikePawn.h/cpp      -- Dirt bike (759 lines)
    PHCBlackbirdPawn.h/cpp     -- SR-71 Blackbird (892 lines)
    PHCBlackHawkPawn.h/cpp     -- UH-60 Black Hawk (879 lines)
    PHCMissileProjectile.h/cpp -- Projectile with trails (394 lines)
    PHCNukeExplosion.h/cpp     -- GTA V-style nuke explosion (478 lines)

  UI/
    PHCCityHUD.h/cpp           -- Main 3D world HUD (3,111 lines)
    PHCMainHUD.h/cpp           -- Lobby menu phone overlay (484 lines)
    PHCLeaderboardWidget.h/cpp -- Leaderboard display (327 lines)
    Avatar/, Economy/, Garden/, Panels/, Rooms/, Shop/  -- Sub-widgets

  Input/
    PHCTouchInputComponent.h/cpp -- Mobile virtual joystick + camera look (226 lines)

  Auth/                        -- Authentication manager + widget (1,263 lines total)
  Economy/                     -- Currency, store management (846 lines total)
  Network/                     -- HTTP client, WebSocket client, API cache (1,346 lines total)
  Social/                      -- Follow system, player search (925 lines total)
  Theme/                       -- UI theming system (777 lines total)
  Rooms/                       -- Voice room manager + themes (718 lines total)

  Games/
    Base/PHCMiniGameMode.h/cpp -- Base class for all mini-games (198 lines)
    Base/PHCMiniGameHUD.h/cpp  -- Base class for mini-game UIs (465 lines)
    [23 game folders]          -- Each with GameMode + HUD + supporting actors
```

---

## Data Structs (PHCTypes.h)

- `FPHCPlayerProfile`: PlayerId, Username, DisplayName, AvatarUrl, Coins, Gems, Level, XP, FollowerCount, FollowingCount, bIsGuest
- `FPHCAvatarData`: Layers (TArray of FPHCAvatarLayer), SkinTone
- `FPHCShopItem`: ItemId, Name, Description, ImageUrl, Category, CurrencyType, Price, bOwned
- `FPHCRoomInfo`: RoomId, RoomName, HostName, HostId, ThemeId, Status, PlayerCount, MaxPlayers(=8)
- `FPHCThemeData`: ThemeId, ThemeName, 12 color slots
- `FPHCGameResult`: GameType, Score, CoinsEarned, XPEarned, bWon, TimePlayed
- `FPHCDailyBonus`, `FPHCGardenPlot`, `FPHCFollowInfo`, `FPHCLeaderboardEntry`, `FPHCChatMessage`

---

## Voice Chat (Agora)

**App ID:** `REDACTED_AGORA_APP_ID`
**App Certificate:** `REDACTED_AGORA_APP_CERTIFICATE` (server-side only, never in client code)

### Production Token Flow
1. Client calls `FAgoraVoiceManager::JoinChannel("PlayhouseCity_Global")`
2. Client HTTP POSTs to `https://tfplayhouse.com/api/agora/token/` with `{"channel":"PlayhouseCity_Global","uid":0}`
3. Django backend generates 24-hour token using `agora-token-builder` + App Certificate
4. Client joins channel with server-generated token
5. Falls back to tokenless join if backend unreachable

### Key Files
- `Voice/AgoraVoiceManager.mm` — iOS + Mac (ObjC SDK)
- `Voice/AgoraVoiceManager_Native.cpp` — Windows + Android (C++ SDK, gated by `AGORA_NATIVE_CPP`)
- `Voice/AgoraVoiceManager_Stub.cpp` — fallback for platforms without SDK
- Backend endpoint: `/var/www/Playhouse/backend/game/views.py` → `agora_token` function

### Platform Status
- iOS: Working (ObjC SDK)
- Mac: Working (ObjC SDK, shared .mm with `#if PLATFORM_MAC`)
- Windows/Android: C++ SDK integrated, untested
- Linux: Stub only — `.mm` files must be excluded from rsync to build server

---

## Dev Mode

Green "Dev" button in top bar (left of mic). Tap to show time-of-day panel:

| Button | DayNightTime | Effect |
|--------|-------------|--------|
| Sunrise | 0.0 | Locks to sunrise |
| Morning | 0.125 | Locks to morning |
| Noon | 0.25 | Locks to noon |
| Afternoon | 0.375 | Locks to afternoon |
| Sunset | 0.5 | Locks to sunset |
| Dusk | 0.625 | Locks to dusk |
| Midnight | 0.75 | Locks to midnight |
| Cycle | — | Resumes normal day/night progression |

Implemented in `PHCCityHUD.cpp` — `OnDevModeButtonClicked()` / `OnDevTimeSelected()`.

---

## Known Issues & Open Items

### Texture UV Smearing
- Fab MI textures use mesh UVs (0-1 range), causing stretching on large meshes
- Size check (scale < 50x) mitigates but doesn't fully solve for medium-sized meshes
- Need world-space UV material variant for proper tiling on all mesh sizes

### Vehicle Replication
- Vehicle `bReplicates` and `SetReplicateMovement()` are NOT enabled in vehicle constructors
- Auto-connect to dedicated server is disabled until vehicle replication is implemented
- Character `bReplicates=true` and `SetReplicateMovement=true` ARE set (30Hz net update)

### No PlayerStart Actor
- Characters spawn at (-28000, -28000, 58) via `SpawnDefaultPawnAtTransform_Implementation()`

### Dead Code
- `ExhaustPuffs`/`ExhaustPuffAge`/`NextExhaustPuff`/`ExhaustSpawnTimer` in VehiclePawn and DuneBuggyPawn headers

---

## Conventions

- **All UI built in C++** via `WidgetTree->ConstructWidget<>` — no UMG Blueprint widgets
- **All input created at runtime** via `NewObject<UInputAction>` — no .uasset input assets
- **All meshes are engine basic shapes** with `UMaterialInstanceDynamic` for PBR
- **Log category:** `LogPlayhouseCity` (declared in `PlayhouseCity.h`)
- **Class prefix:** `PHC` (PlayHouseCity)
- **Header guards:** `#pragma once`
- **Mobile checks:** `#if PLATFORM_IOS || PLATFORM_ANDROID`
- **No Blueprints, no .uasset files** in the gameplay layer
