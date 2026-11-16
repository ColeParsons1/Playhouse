// Environment model for the driving simulator (SimulatorGame.jsx):
// time of day / sun position, rain, and whether the car's headlights are on.
//
// Everything in here is plain math with no THREE.js or React dependency, so the
// rules can be unit tested (see simulatorEnvironment.test.js). The component
// only *applies* the results to the scene.

const DEG = Math.PI / 180;

// ---------------------------------------------------------------------------
// Day / night cycle
// ---------------------------------------------------------------------------

// One full 24h day lasts 5 real minutes.
export const DAY_CYCLE_DURATION = 300; // seconds

// Time of day is a fraction of 24h: 0 = midnight, 0.25 = 06:00, 0.5 = noon,
// 0.75 = 18:00. The game starts shortly before dawn so the player begins in
// the neon-lit night and sees the first sunrise ~15 seconds in.
export const START_TIME_OF_DAY = 0.2; // 04:48

// The city is modelled on Tampa Bay, FL (Sunshine Skyway bridge, etc.).
export const LATITUDE_DEG = 27.95;

// Official sunrise/sunset: the sun's upper limb touches the horizon, which,
// after atmospheric refraction and the sun's apparent radius, is when the
// sun's centre is 0.833 degrees below the horizon.
export const SUNRISE_SUNSET_ELEVATION_DEG = -0.833;

// End of civil twilight - the sky is effectively dark below this.
export const CIVIL_TWILIGHT_ELEVATION_DEG = -6;

// Elevation at which the scene reaches full daylight.
export const FULL_DAYLIGHT_ELEVATION_DEG = 10;

const wrap01 = (t) => ((t % 1) + 1) % 1;
const clamp01 = (v) => Math.min(1, Math.max(0, v));

/** Advance the time of day by `deltaSeconds` of real time, wrapping at midnight. */
export function advanceTimeOfDay(timeOfDay, deltaSeconds, cycleDuration = DAY_CYCLE_DURATION) {
  return wrap01(timeOfDay + deltaSeconds / cycleDuration);
}

/**
 * Sun elevation above the horizon in degrees.
 *
 * Uses the standard solar altitude formula with the solar declination fixed at
 * 0 (equinox), giving a symmetric ~12 hour day:
 *   sin(elevation) = cos(latitude) * cos(hourAngle)
 */
export function getSunElevation(timeOfDay, latitudeDeg = LATITUDE_DEG) {
  const hourAngle = (wrap01(timeOfDay) - 0.5) * 2 * Math.PI;
  return Math.asin(Math.cos(latitudeDeg * DEG) * Math.cos(hourAngle)) / DEG;
}

/**
 * Unit vector pointing at the sun, in scene space (+x = east, +y = up,
 * +z = north). The sun rises in the east, passes south of the zenith at noon
 * (northern hemisphere) and sets in the west.
 */
export function getSunDirection(timeOfDay, latitudeDeg = LATITUDE_DEG) {
  const hourAngle = (wrap01(timeOfDay) - 0.5) * 2 * Math.PI;
  const lat = latitudeDeg * DEG;
  return {
    x: -Math.sin(hourAngle),
    y: Math.cos(lat) * Math.cos(hourAngle),
    z: -Math.sin(lat) * Math.cos(hourAngle),
  };
}

// Time of day (0-1) at which the sun crosses the sunrise/sunset elevation.
function sunCrossingOffset(latitudeDeg) {
  const cosH = Math.sin(SUNRISE_SUNSET_ELEVATION_DEG * DEG) / Math.cos(latitudeDeg * DEG);
  return Math.acos(cosH) / (2 * Math.PI);
}

/** Time of day (0-1) of sunrise. ~0.2474 (05:56) at the default latitude. */
export function getSunriseTimeOfDay(latitudeDeg = LATITUDE_DEG) {
  return 0.5 - sunCrossingOffset(latitudeDeg);
}

/** Time of day (0-1) of sunset. ~0.7526 (18:04) at the default latitude. */
export function getSunsetTimeOfDay(latitudeDeg = LATITUDE_DEG) {
  return 0.5 + sunCrossingOffset(latitudeDeg);
}

/** True between sunset and sunrise (twilight included). */
export function isNighttime(timeOfDay, latitudeDeg = LATITUDE_DEG) {
  return getSunElevation(timeOfDay, latitudeDeg) < SUNRISE_SUNSET_ELEVATION_DEG;
}

/**
 * How much daylight the scene gets, 0 (night) to 1 (full day). Ramps smoothly
 * from the end of civil twilight to a little after sunrise, so the scene
 * lighting dims through dusk rather than snapping.
 */
export function getDaylightFactor(sunElevationDeg) {
  const t = clamp01(
    (sunElevationDeg - CIVIL_TWILIGHT_ELEVATION_DEG) /
      (FULL_DAYLIGHT_ELEVATION_DEG - CIVIL_TWILIGHT_ELEVATION_DEG)
  );
  return t * t * (3 - 2 * t); // smoothstep
}

/** Strength of the warm sunrise/sunset tint, peaking with the sun on the horizon. */
export function getDuskWarmth(sunElevationDeg) {
  return clamp01(1 - Math.abs(sunElevationDeg) / FULL_DAYLIGHT_ELEVATION_DEG);
}

/** "HH:MM" for a time of day (for display / debugging). */
export function formatTimeOfDay(timeOfDay) {
  const totalMinutes = Math.floor(wrap01(timeOfDay) * 24 * 60);
  const hh = String(Math.floor(totalMinutes / 60)).padStart(2, '0');
  const mm = String(totalMinutes % 60).padStart(2, '0');
  return `${hh}:${mm}`;
}

// ---------------------------------------------------------------------------
// Weather (rain showers alternating with dry spells)
// ---------------------------------------------------------------------------

export const RAIN_DURATION_RANGE = [30, 75]; // seconds of rain per shower
export const DRY_DURATION_RANGE = [60, 150]; // seconds between showers

function randomDuration([min, max], rng) {
  return min + rng() * (max - min);
}

/** Initial weather. Defaults to dry, with the first shower 60-150s in. */
export function createWeatherState({ isRaining = false, rng = Math.random } = {}) {
  return {
    isRaining,
    timeRemaining: randomDuration(isRaining ? RAIN_DURATION_RANGE : DRY_DURATION_RANGE, rng),
  };
}

/**
 * Advance the weather by `deltaSeconds`. When the current spell runs out the
 * weather flips between raining and dry. Returns a new state object.
 */
export function updateWeather(state, deltaSeconds, rng = Math.random) {
  let { isRaining, timeRemaining } = state;
  timeRemaining -= deltaSeconds;
  while (timeRemaining <= 0) {
    isRaining = !isRaining;
    timeRemaining += randomDuration(isRaining ? RAIN_DURATION_RANGE : DRY_DURATION_RANGE, rng);
  }
  return { isRaining, timeRemaining };
}

// ---------------------------------------------------------------------------
// Headlights
// ---------------------------------------------------------------------------

// Emissive intensity of the headlight LEDs when switched on.
export const HEADLIGHT_ON_INTENSITY = 1.5;
// Intensity (candela) of the headlight beam that lights the road when on, at night.
export const HEADLIGHT_BEAM_INTENSITY = 2000;
// In daylight the beam is barely noticeable on the road (e.g. driving in
// daytime rain), so it is dimmed to this fraction at full daylight.
export const HEADLIGHT_BEAM_DAYLIGHT_FRACTION = 0.3;

/**
 * Whether the car's headlights should be on. Follows Florida's lighted-lamp
 * rule (Fla. Stat. 316.217), which fits the game's Tampa setting: lamps on
 * from sunset to sunrise (twilight included), and during any rain.
 */
export function shouldHeadlightsBeOn({ timeOfDay, isRaining }) {
  return Boolean(isRaining) || isNighttime(timeOfDay);
}

/** How bright the beam looks on the road for a daylight factor (0 = night, 1 = day). */
export function getHeadlightBeamIntensity(daylight = 0) {
  return HEADLIGHT_BEAM_INTENSITY * (1 - (1 - HEADLIGHT_BEAM_DAYLIGHT_FRACTION) * clamp01(daylight));
}

/**
 * Switch the headlights on or off: the glowing LED materials and the beam
 * light(s) that illuminate the road. Lights are dimmed to 0 rather than
 * removed, so toggling never forces a shader recompile. `daylight` (0-1)
 * only changes how bright the beam looks, never whether it is on.
 */
export function applyHeadlightState(materials, on, beams = [], daylight = 0) {
  const intensity = on ? HEADLIGHT_ON_INTENSITY : 0;
  materials.forEach((material) => {
    material.emissiveIntensity = intensity;
  });
  const beamIntensity = on ? getHeadlightBeamIntensity(daylight) : 0;
  beams.forEach((beam) => {
    beam.intensity = beamIntensity;
  });
}

// ---------------------------------------------------------------------------
// Combined per-frame step
// ---------------------------------------------------------------------------

export function createEnvironmentState({
  timeOfDay = START_TIME_OF_DAY,
  weather = createWeatherState(),
} = {}) {
  return {
    timeOfDay,
    weather,
    headlightsOn: shouldHeadlightsBeOn({ timeOfDay, isRaining: weather.isRaining }),
  };
}

/**
 * Advance time and weather by `deltaSeconds`, then work out the headlight
 * state from the result. This is the single source of truth the game loop
 * uses, so the headlights always match the sky and weather on screen.
 * `freezeTime` / `freezeWeather` hold either clock still (used by the test
 * scenarios below).
 */
export function stepEnvironment(state, deltaSeconds, rng = Math.random, { freezeTime = false, freezeWeather = false } = {}) {
  const timeOfDay = freezeTime ? state.timeOfDay : advanceTimeOfDay(state.timeOfDay, deltaSeconds);
  const weather = freezeWeather ? state.weather : updateWeather(state.weather, deltaSeconds, rng);
  return {
    timeOfDay,
    weather,
    headlightsOn: shouldHeadlightsBeOn({ timeOfDay, isRaining: weather.isRaining }),
  };
}

// ---------------------------------------------------------------------------
// Test scenarios (in-game test panel)
// ---------------------------------------------------------------------------

const minutes = (m) => m / (24 * 60);

/**
 * Scenarios the in-game test panel cycles through. Each one jumps straight to
 * a time/weather combination so testers don't have to wait for the clock or
 * the weather (especially on slow machines).
 *
 * - `expected`: headlight state the scenario should show (true = ON). For the
 *   "live" scenarios it's a description of the transition to watch for.
 * - `freezeTime` / `freezeWeather`: hold that clock still so the scenario
 *   stays put while you look around.
 * - `rainStartsIn`: seconds until a shower starts (live rain scenario).
 */
export const TEST_SCENARIOS = [
  { id: 'live', label: 'Live (normal cycle)', live: true },
  { id: 'day-dry', label: 'Day · Dry', timeOfDay: 0.5, isRaining: false, freezeTime: true, freezeWeather: true, expected: false },
  { id: 'day-rain', label: 'Day · Rain', timeOfDay: 0.5, isRaining: true, freezeTime: true, freezeWeather: true, expected: true },
  { id: 'night-dry', label: 'Night · Dry', timeOfDay: minutes(23 * 60), isRaining: false, freezeTime: true, freezeWeather: true, expected: true },
  { id: 'night-rain', label: 'Night · Rain', timeOfDay: minutes(23 * 60), isRaining: true, freezeTime: true, freezeWeather: true, expected: true },
  { id: 'before-sunset', label: 'Sunset −2 min · Dry', timeOfDay: getSunsetTimeOfDay() - minutes(2), isRaining: false, freezeTime: true, freezeWeather: true, expected: false },
  { id: 'after-sunset', label: 'Sunset +2 min · Dry', timeOfDay: getSunsetTimeOfDay() + minutes(2), isRaining: false, freezeTime: true, freezeWeather: true, expected: true },
  { id: 'before-sunrise', label: 'Sunrise −2 min · Dry', timeOfDay: getSunriseTimeOfDay() - minutes(2), isRaining: false, freezeTime: true, freezeWeather: true, expected: true },
  { id: 'after-sunrise', label: 'Sunrise +2 min · Dry', timeOfDay: getSunriseTimeOfDay() + minutes(2), isRaining: false, freezeTime: true, freezeWeather: true, expected: false },
  { id: 'sunset-live', label: 'Sunset (live) · Dry', timeOfDay: getSunsetTimeOfDay() - minutes(10), isRaining: false, freezeWeather: true, expected: 'OFF → ON at sunset' },
  { id: 'rain-live', label: 'Rain starts (live) · Day', timeOfDay: 0.5, isRaining: false, rainStartsIn: 3, freezeTime: true, expected: 'OFF → ON when rain starts' },
];

/** Environment state for a test scenario (not used for the 'live' entry). */
export function createScenarioState(scenario) {
  const weather = scenario.rainStartsIn !== undefined
    ? { isRaining: false, timeRemaining: scenario.rainStartsIn }
    : { isRaining: scenario.isRaining, timeRemaining: Infinity };
  return createEnvironmentState({ timeOfDay: scenario.timeOfDay, weather });
}
