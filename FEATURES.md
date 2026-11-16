# Playhouse Features Documentation

This document provides a comprehensive overview of all features in the Playhouse gaming platform.

---

## Table of Contents
1. [FlyGame (Flight Simulator)](#flygame-flight-simulator)
2. [SimulatorGame / DriveGame (Driving Simulator)](#simulatorgame--drivegame-driving-simulator)
3. [YouTubeMusicPlayer](#youtubemusicplayer)
4. [GameHub Updates](#gamehub-updates)
5. [App.jsx Updates](#appjsx-updates)
6. [Theme Assets](#theme-assets)

---

## FlyGame (Flight Simulator)

### Overview
A full-featured 3D flight simulator built with React and Three.js, providing realistic flying experience with detailed terrain, aircraft physics, and intuitive touch/keyboard controls.

### Game Mechanics & Physics

#### Core Physics System
- **Gravity Simulation**: Constant gravity (9.81 m/s²)
- **Air Density Model**: Realistic air density (1.225 kg/m³) for aerodynamic calculations
- **Aircraft Properties**:
  - Mass: 5000 kg
  - Maximum Thrust: 80,000 N
  - Wing Area: 50 m²
  - Drag Coefficient: 0.025
  - Lift Coefficient: 0.8
  - Stall Angle: ~20 degrees
  - Minimum Flight Speed: 50 m/s

#### Flight Dynamics
- **Throttle Control**: 0-100%
- **Pitch Control**: -60° to +60° (climb/dive)
- **Roll Control**: -90° to +90° (banking turns)
- **Yaw**: Automatically calculated from banking
- **Auto-level Roll**: Aircraft naturally levels when no input
- **Speed**: Real-time velocity in knots
- **Altitude**: Position-based in feet
- **Heading**: Compass direction 0-360°

#### Stall System
- Triggers when angle of attack exceeds 20° AND speed falls below 100 m/s
- Reduces lift to 30% during stall
- Visual warning displayed

### Visual Elements

#### Terrain System (50,000 x 50,000 units)
- **Ocean**: Deep ocean with shallow water near coasts
- **Mountain Range**: 10 major peaks (3000-5000 units tall) with snow caps
- **Desert Region**: 15,000 x 15,000 unit area with 30 dunes
- **Forests**: 4 major patches with 100-200 trees each
- **Cities**: 5 cities with buildings, roads, and windows
  - Central City (large): 150 buildings
  - East Harbor & West Valley (medium): 80 buildings each
  - Mountain View & Coastal Town (small): 40 buildings each
- **Rivers**: Main river + branches
- **Airports**: 2 runways with markings
- **Clouds**: 100 procedurally generated at varying altitudes
- **Sun**: Visual sun sphere

#### Aircraft Model
- Detailed fuselage with metallic finish
- Cockpit with transparent canopy
- Main wings with angled wing tips
- Horizontal and vertical stabilizers
- 2 engines with thrust visualization
- Landing gear (front + dual rear)
- Navigation lights (red/green/white)

### HUD (Heads-Up Display)
- **Airspeed Indicator**: Speed in knots
- **Altimeter**: Altitude in feet
- **Heading Indicator**: Compass 0-360°
- **Throttle Indicator**: Vertical bar with percentage
- **Pitch Indicator**: Angle in degrees
- **Stall Warning**: Red flashing alert
- **Center Crosshair**: Alignment reference
- **Horizon Line**: Reference across middle

### Touch Controls (Swipe-Based)

#### Flight Stick (Right Side)
- Swipe left/right for roll
- Swipe up/down for pitch
- Analog control (-1 to 1)
- Visual crosshair indicator

#### Throttle Control (Left Side)
- Swipe up to increase
- Swipe down to decrease
- Analog control
- Visual indicator

### Keyboard Controls
- **W/↑**: Pitch down (dive)
- **S/↓**: Pitch up (climb)
- **A/←**: Roll left
- **D/→**: Roll right
- **Shift**: Increase throttle
- **Ctrl**: Decrease throttle

### UI Elements
- Loading screen with spinner
- Control hint modal
- Rotate device overlay (portrait mode)
- Exit button

---

## SimulatorGame / DriveGame (Driving Simulator)

### Overview
A Vice City-themed 3D driving simulator with massive city environment, realistic car physics, neon aesthetics, and ramp jumping mechanics.

### Game Mechanics

#### Car Physics
- **Max Speed**: 180 km/h
- **Acceleration**: 0.25 units/frame
- **Braking**: 0.45 units/frame
- **Friction**: 0.03 units/frame
- **Speed-dependent steering**: Turn multiplier 1.2 to 0.6

#### Flight/Ramp System
- 4 launch ramps at strategic locations
- Triggers at speed > 30 km/h
- Launch velocity scales with car speed
- Air control reduced to 30%
- Realistic pitch angle while airborne
- Gravity: -0.8 acceleration

### Visual Elements

#### Vice City Environment (10x Scale)

##### Road Network
- Grid-based layout (every 160 units)
- Yellow center lines, white edge markings
- Concrete sidewalks with curbs
- Zebra crosswalks

##### Buildings (Miami Art Deco)
- 500+ unique structures
- Downtown skyscrapers up to 400+ units
- Window grids with randomized lighting
- Ground floor storefronts
- Rooftop elements (penthouses, antennas)

##### Neon Accents
- Magenta neon strips (#ff00ff)
- Cyan accents (#00ffff)
- Pink details (#ff1493)
- Building edge lighting

##### Environment
- Ocean with beach and boardwalk
- Palm trees (100 optimized)
- Beach umbrellas
- Street lights with neon bulbs

##### Day/Night Cycle & Weather
- Full 24h day every 5 minutes (starts at 04:48, just before dawn)
- Sun follows its real path for Tampa's latitude (equinox): sunrise ~05:56, sunset ~18:03
- Sky, fog and lighting blend through dawn/dusk (warm sunset tint), moonlight at night
- Rain showers (30-75s) alternate with dry spells (60-150s): rain streaks, overcast sky, reduced visibility
- Logic lives in `frontend/src/utils/simulatorEnvironment.js` (unit tested)

##### Performance / Quality Modes
- `high` (default): native resolution, MSAA, 4K shadow map, full draw distance
- `low`: auto-selected when WebGL is software-rendered (no GPU, e.g. VMs/CI sandboxes): reduced render scale, no shadows/MSAA, shorter foggier draw distance, simplified road markings, HUD glow animations disabled
- Force a mode with `?quality=low|high` or `localStorage.playhouse_sim_quality`
- Static city meshes are merged per material + map tile at load (~32k meshes -> a few hundred), cutting draw calls ~5x in both modes

##### Automatic Headlights
- Follow Florida's lighted-lamp rule: **on from sunset to sunrise (twilight included) and whenever it rains**, off in dry daylight
- Headlight LEDs glow and a beam lights the road ahead when on (beam is softer in daylight rain)

##### Test Panel (time/weather test cases)
- Shown in dev builds (`npm run dev`), or in any build with `?simtest` in the URL
- Click the 🧪 button (or press **T**) to cycle: Live, Day·Dry, Day·Rain, Night·Dry, Night·Rain, 2 min either side of sunset and sunrise, Sunset (live), Rain starts (live)
- Fixed cases freeze the clock/weather and show PASS/FAIL against the expected headlight state; live cases show what transition to watch for
- Scenarios are defined in `TEST_SCENARIOS` (`frontend/src/utils/simulatorEnvironment.js`) and unit tested

#### Player Car (Vice City Testarossa Style)
- White metallic body (2x scale)
- Transparent blue-tinted windshield
- Chrome wheels with detailed tires
- LED headlights and red tail lights
- Side mirrors and exhaust pipes

##### Neon Underglow
- Magenta side strips (2.0 emissive)
- Cyan front/rear strips
- Body accent lines
- Wheel well glow
- Hood accent lines
- Roof edge glow

### HUD

#### Speedometer
- Circular neon gauge
- Speed in km/h (cyan)
- Gear indicator
- Magenta/cyan glow effects

#### RPM Gauge
- Horizontal bar (cyan → magenta → red)
- Max 8000 RPM

#### Altitude Indicator (When Flying)
- Rocket emoji icon
- Pulsing animation
- Shows current altitude

#### Mini Map
- 150x150px display
- Cyan pulsing dot (player position)

### Touch Controls

#### Steering (Left Side)
- Touch and drag for analog steering
- -1 to +1 range
- Visual steering wheel indicator

#### Pedals (Right Side)
- **Gas Pedal**: Cyan themed
- **Brake Pedal**: Pink/Magenta themed
- Scale effect on press

### Keyboard Controls
- **W/↑**: Gas
- **S/↓**: Brake
- **A/←**: Steer left
- **D/→**: Steer right

### Launch Ramps
- West Ramp: (-2200, 0)
- East Ramp: (2200, 0)
- North Ramp: (0, -2200)
- South Ramp: (0, 550)
- Neon rails with magenta/cyan colors

### Traffic System
- Parked cars with Vice City 80s colors
- Moving traffic on major roads
- Random speed variation

---

## YouTubeMusicPlayer

### Overview
A sophisticated shared music player for voice rooms enabling host-controlled YouTube music playback with real-time sync for all participants.

### Features

#### Search & Discovery
- YouTube API integration
- Autocomplete suggestions with debouncing
- Full song search via backend API
- Suggestions: "official audio", "lyrics", "music video", "live"

#### Playback Control (Host Only)
- Play/Pause
- Skip track
- Volume control
- Progress bar with seek

#### Queue Management
- Add tracks to queue
- Auto-play next track
- Shows up to 5 tracks + count

#### Sync Mechanism
- WebSocket-based synchronization
- Real-time updates to all room participants
- Pending message handling for unready players

#### Views
- **Host View**: Full controls, search, queue
- **Listener View**: Badge showing they're listening to host's music

### UI Elements
- Now Playing section with album art
- Animated playing bars
- Progress bar with time display
- Volume slider
- Dark theme with Spotify-green accents (#1db954)

---

## GameHub Updates

### New Games Added
1. **FlyGame** (id: 'fly') - "Pilot a plane!"
2. **SimulatorGame** (id: 'drive') - "3D city driving!"

### Total Games: 22
- Bingo, Pack (PresentCatch), Temple Run, Baby Shark (Flappy Bird)
- Tower Defense, Pool (Billiards), Tanks, Guess the Sign (ASL)
- Goo Bridge, Angry Ducks, Color Cards, Lucky Slots
- Kitchen Chaos (Overcooked), Connect 4, Ludo, Carrom
- Jenga, Peg Game, Pong, Maps, Cornhole
- **Drive** (NEW), **Fly** (NEW)

### Architecture
- Lazy-loaded games for performance
- Featured game system
- Memoized GameCard components
- Leaderboard integration
- Reward modal with animations
- McDonald's PlayPlace color palette

---

## App.jsx Updates

### New Components
- **BuyCoinsModal**: Stripe integration for coin purchases

### New Features
- Currency display (gems + coins)
- Add coins button opening purchase modal
- Current room banner in games view

### Lazy-Loaded Components
- GameHub, ShopScreen, AvatarEditor
- StatsModal, CreateRoomModal
- FollowScreen, VoiceRoomGame

---

## Theme Assets

### Available Themes (`/frontend/public/themes/`)
1. `theater.png` - Theater/stage theme
2. `anime_theater.png` - Anime-styled theater
3. `underwater.png` - Underwater/aquatic theme
4. `hearts.jpg` - Hearts/romantic theme
5. `fantasy.png` - Fantasy/magical theme

### Integration
- Selectable in AvatarEditor
- Saved per player in localStorage
- Applied via ThemeProvider

---

## Performance Optimizations

### 3D Games (FlyGame & SimulatorGame)
- Geometry simplification
- Basic materials (no expensive shading)
- Fog system to hide distant objects
- Reduced polygon counts
- Disabled anti-aliasing
- High-performance power preference
- Pixel ratio optimization
- Proper cleanup on unmount

### React Optimization
- Lazy loading for all game components
- Memoized components
- Proper ref management
- Efficient state updates

---

## Files Changed

### New Files
- `frontend/src/components/FlyGame.jsx`
- `frontend/src/components/FlyGame.css`
- `frontend/src/components/SimulatorGame.jsx`
- `frontend/src/components/SimulatorGame.css`
- `frontend/src/components/YouTubeMusicPlayer.jsx`
- `frontend/src/components/YouTubeMusicPlayer.css`
- `frontend/public/themes/` (5 theme images)

### Modified Files
- `frontend/src/App.jsx`
- `frontend/src/components/GameHub.jsx`
- `frontend/src/components/GameIcons.jsx`
- Various other components for integration

---

*Documentation generated for Playhouse Gaming Platform*
