import { describe, it, expect } from 'vitest';
import * as THREE from 'three';
import {
  DAY_CYCLE_DURATION,
  LATITUDE_DEG,
  SUNRISE_SUNSET_ELEVATION_DEG,
  CIVIL_TWILIGHT_ELEVATION_DEG,
  RAIN_DURATION_RANGE,
  DRY_DURATION_RANGE,
  HEADLIGHT_ON_INTENSITY,
  HEADLIGHT_BEAM_INTENSITY,
  HEADLIGHT_BEAM_DAYLIGHT_FRACTION,
  advanceTimeOfDay,
  getSunElevation,
  getSunDirection,
  getSunriseTimeOfDay,
  getSunsetTimeOfDay,
  getDaylightFactor,
  isNighttime,
  formatTimeOfDay,
  createWeatherState,
  updateWeather,
  shouldHeadlightsBeOn,
  applyHeadlightState,
  getHeadlightBeamIntensity,
  createEnvironmentState,
  stepEnvironment,
  TEST_SCENARIOS,
  createScenarioState,
} from './simulatorEnvironment';

// Helpers ------------------------------------------------------------------

const at = (hours, minutes = 0) => (hours * 60 + minutes) / (24 * 60);
const GAME_SECOND = 1 / DAY_CYCLE_DURATION; // one real second, as a fraction of a day

// Deterministic RNG (mulberry32) so weather sequences are reproducible
const seededRng = (seed) => () => {
  seed |= 0;
  seed = (seed + 0x6d2b79f5) | 0;
  let t = Math.imul(seed ^ (seed >>> 15), 1 | seed);
  t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
};

const DAYTIME = {
  'early morning (07:30)': at(7, 30),
  'mid-morning (10:00)': at(10),
  'noon (12:00)': at(12),
  'afternoon (15:00)': at(15),
  'late afternoon (17:30)': at(17, 30),
};
const NIGHTTIME = {
  'evening (20:00)': at(20),
  'late night (23:00)': at(23),
  'midnight (00:00)': 0,
  'small hours (03:00)': at(3),
  'pre-dawn (05:00)': at(5),
};

// Four core cases -------------------------------------------------------------

describe('headlights: time of day x rain', () => {
  describe('daylight + raining -> ON', () => {
    it.each(Object.entries(DAYTIME))('%s', (_, timeOfDay) => {
      expect(isNighttime(timeOfDay)).toBe(false);
      expect(shouldHeadlightsBeOn({ timeOfDay, isRaining: true })).toBe(true);
    });
  });

  describe('daylight + no rain -> OFF', () => {
    it.each(Object.entries(DAYTIME))('%s', (_, timeOfDay) => {
      expect(isNighttime(timeOfDay)).toBe(false);
      expect(shouldHeadlightsBeOn({ timeOfDay, isRaining: false })).toBe(false);
    });
  });

  describe('nighttime + raining -> ON', () => {
    it.each(Object.entries(NIGHTTIME))('%s', (_, timeOfDay) => {
      expect(isNighttime(timeOfDay)).toBe(true);
      expect(shouldHeadlightsBeOn({ timeOfDay, isRaining: true })).toBe(true);
    });
  });

  describe('nighttime + no rain -> ON', () => {
    it.each(Object.entries(NIGHTTIME))('%s', (_, timeOfDay) => {
      expect(isNighttime(timeOfDay)).toBe(true);
      expect(shouldHeadlightsBeOn({ timeOfDay, isRaining: false })).toBe(true);
    });
  });
});

// Dusk / dawn ------------------------------------------------------------------

describe('headlights at dusk and dawn', () => {
  const sunrise = getSunriseTimeOfDay();
  const sunset = getSunsetTimeOfDay();

  it('uses realistic sunrise/sunset times for Tampa (equinox)', () => {
    expect(formatTimeOfDay(sunrise)).toBe('05:56');
    expect(formatTimeOfDay(sunset)).toBe('18:03');
    expect(getSunElevation(sunrise)).toBeCloseTo(SUNRISE_SUNSET_ELEVATION_DEG, 6);
    expect(getSunElevation(sunset)).toBeCloseTo(SUNRISE_SUNSET_ELEVATION_DEG, 6);
  });

  it('dry: switch ON right at sunset', () => {
    expect(shouldHeadlightsBeOn({ timeOfDay: sunset - GAME_SECOND, isRaining: false })).toBe(false);
    expect(shouldHeadlightsBeOn({ timeOfDay: sunset + GAME_SECOND, isRaining: false })).toBe(true);
  });

  it('dry: switch OFF right at sunrise', () => {
    expect(shouldHeadlightsBeOn({ timeOfDay: sunrise - GAME_SECOND, isRaining: false })).toBe(true);
    expect(shouldHeadlightsBeOn({ timeOfDay: sunrise + GAME_SECOND, isRaining: false })).toBe(false);
  });

  it('raining: stay ON through sunset and sunrise', () => {
    for (const t of [sunset - GAME_SECOND, sunset + GAME_SECOND, sunrise - GAME_SECOND, sunrise + GAME_SECOND]) {
      expect(shouldHeadlightsBeOn({ timeOfDay: t, isRaining: true })).toBe(true);
    }
  });

  it('twilight (sun set, sky not fully dark yet) counts as night -> ON', () => {
    // 10 game-minutes after sunset: sun is below the horizon but above civil twilight
    const twilight = sunset + at(0, 10);
    const elevation = getSunElevation(twilight);
    expect(elevation).toBeLessThan(SUNRISE_SUNSET_ELEVATION_DEG);
    expect(elevation).toBeGreaterThan(CIVIL_TWILIGHT_ELEVATION_DEG);
    expect(getDaylightFactor(elevation)).toBeGreaterThan(0); // scene still has some light
    expect(shouldHeadlightsBeOn({ timeOfDay: twilight, isRaining: false })).toBe(true);
  });

  it('dry day: headlights toggle exactly twice (at sunrise and sunset), in sync with the sun', () => {
    const samples = 24 * 60 * 4; // every 15 game-seconds
    let toggles = 0;
    let prev = shouldHeadlightsBeOn({ timeOfDay: 0, isRaining: false });
    for (let i = 0; i <= samples; i++) {
      const t = i / samples;
      const on = shouldHeadlightsBeOn({ timeOfDay: t, isRaining: false });
      expect(on).toBe(getSunElevation(t) < SUNRISE_SUNSET_ELEVATION_DEG);
      if (on !== prev) toggles++;
      prev = on;
    }
    expect(toggles).toBe(2);
  });
});

// Rain starting/stopping --------------------------------------------------------

describe('headlights follow rain starting and stopping', () => {
  const rng = () => 0.5;

  it('daytime: rain starts -> ON, rain stops -> OFF', () => {
    let env = createEnvironmentState({
      timeOfDay: at(11),
      weather: { isRaining: false, timeRemaining: 1 },
    });
    expect(env.headlightsOn).toBe(false);

    env = stepEnvironment(env, 1.5, rng); // dry spell ends -> raining
    expect(env.weather.isRaining).toBe(true);
    expect(isNighttime(env.timeOfDay)).toBe(false);
    expect(env.headlightsOn).toBe(true);

    env = stepEnvironment(env, env.weather.timeRemaining + 0.1, rng); // shower ends
    expect(env.weather.isRaining).toBe(false);
    expect(isNighttime(env.timeOfDay)).toBe(false);
    expect(env.headlightsOn).toBe(false);
  });

  it('nighttime: rain stopping leaves headlights ON', () => {
    let env = createEnvironmentState({
      timeOfDay: at(22),
      weather: { isRaining: true, timeRemaining: 1 },
    });
    expect(env.headlightsOn).toBe(true);

    env = stepEnvironment(env, 1.5, rng);
    expect(env.weather.isRaining).toBe(false);
    expect(isNighttime(env.timeOfDay)).toBe(true);
    expect(env.headlightsOn).toBe(true);
  });

  it('raining through sunrise: headlights stay ON until the rain stops', () => {
    let env = createEnvironmentState({
      timeOfDay: getSunriseTimeOfDay() - 5 * GAME_SECOND,
      weather: { isRaining: true, timeRemaining: 20 },
    });
    for (let i = 0; i < 19; i++) {
      env = stepEnvironment(env, 1, rng);
      expect(env.headlightsOn).toBe(true);
    }
    expect(isNighttime(env.timeOfDay)).toBe(false); // sun is now up
    env = stepEnvironment(env, 2, rng); // rain stops
    expect(env.weather.isRaining).toBe(false);
    expect(env.headlightsOn).toBe(false);
  });
});

// Full simulation ----------------------------------------------------------------

describe('game loop simulation', () => {
  it('over several game days at 60fps, headlights always match rain + daylight', () => {
    const rng = seededRng(1234);
    let env = createEnvironmentState({ timeOfDay: 0, weather: createWeatherState({ rng }) });
    const seen = new Set();
    const dt = 1 / 60;
    const frames = DAY_CYCLE_DURATION * 6 * 60; // 6 game days

    for (let i = 0; i < frames; i++) {
      env = stepEnvironment(env, dt, rng);
      const night = isNighttime(env.timeOfDay);
      const raining = env.weather.isRaining;
      if (env.headlightsOn !== (raining || night)) {
        throw new Error(`Mismatch at ${formatTimeOfDay(env.timeOfDay)}: night=${night} rain=${raining} lights=${env.headlightsOn}`);
      }
      seen.add(`${night ? 'night' : 'day'}+${raining ? 'rain' : 'dry'}`);
    }

    // Every combination actually happened during the run
    expect([...seen].sort()).toEqual(['day+dry', 'day+rain', 'night+dry', 'night+rain']);
  });
});

// Time & sun model -------------------------------------------------------------

describe('day/night cycle', () => {
  it('a full day lasts DAY_CYCLE_DURATION seconds and wraps at midnight', () => {
    expect(DAY_CYCLE_DURATION).toBe(300);
    expect(advanceTimeOfDay(0, DAY_CYCLE_DURATION / 2)).toBeCloseTo(0.5);
    expect(advanceTimeOfDay(0.9, DAY_CYCLE_DURATION * 0.2)).toBeCloseTo(0.1);
    expect(advanceTimeOfDay(0.25, DAY_CYCLE_DURATION)).toBeCloseTo(0.25);
  });

  it('sun peaks at noon at (90 - latitude) degrees and bottoms out at midnight', () => {
    expect(getSunElevation(at(12))).toBeCloseTo(90 - LATITUDE_DEG, 6);
    expect(getSunElevation(0)).toBeCloseTo(-(90 - LATITUDE_DEG), 6);
  });

  it('sun rises in the east, is due south at noon, and sets in the west', () => {
    const morning = getSunDirection(at(8));
    const noon = getSunDirection(at(12));
    const evening = getSunDirection(at(16));
    expect(morning.x).toBeGreaterThan(0); // east
    expect(evening.x).toBeLessThan(0); // west
    expect(noon.x).toBeCloseTo(0, 6);
    expect(noon.z).toBeLessThan(0); // south
    // direction is a unit vector whose height matches the elevation
    for (const d of [morning, noon, evening]) {
      expect(Math.hypot(d.x, d.y, d.z)).toBeCloseTo(1, 6);
    }
    expect(Math.asin(morning.y) * 180 / Math.PI).toBeCloseTo(getSunElevation(at(8)), 6);
  });

  it('scene daylight is 0 at night, 1 in full day, and partial at sunset', () => {
    expect(getDaylightFactor(getSunElevation(0))).toBe(0);
    expect(getDaylightFactor(getSunElevation(at(12)))).toBe(1);
    const atSunset = getDaylightFactor(getSunElevation(getSunsetTimeOfDay()));
    expect(atSunset).toBeGreaterThan(0);
    expect(atSunset).toBeLessThan(1);
  });
});

// Weather --------------------------------------------------------------------------

describe('weather', () => {
  it('starts dry by default, with a dry spell of valid length', () => {
    const w = createWeatherState({ rng: () => 0 });
    expect(w.isRaining).toBe(false);
    expect(w.timeRemaining).toBe(DRY_DURATION_RANGE[0]);
  });

  it('flips between rain and dry when each spell runs out', () => {
    const rng = () => 1;
    let w = { isRaining: false, timeRemaining: 5 };
    w = updateWeather(w, 4, rng);
    expect(w).toEqual({ isRaining: false, timeRemaining: 1 });
    w = updateWeather(w, 2, rng);
    expect(w.isRaining).toBe(true);
    expect(w.timeRemaining).toBeCloseTo(RAIN_DURATION_RANGE[1] - 1);
  });

  it('handles a step that spans several spells', () => {
    const rng = () => 0; // minimum durations: rain 30s, dry 60s
    const w = updateWeather({ isRaining: false, timeRemaining: 10 }, 10 + 30 + 60 + 5, rng);
    expect(w.isRaining).toBe(true);
    expect(w.timeRemaining).toBeCloseTo(25);
  });

  it('spell durations stay within the configured ranges', () => {
    const rng = seededRng(42);
    let w = createWeatherState({ rng });
    for (let i = 0; i < 200; i++) {
      const [min, max] = w.isRaining ? RAIN_DURATION_RANGE : DRY_DURATION_RANGE;
      expect(w.timeRemaining).toBeGreaterThanOrEqual(min);
      expect(w.timeRemaining).toBeLessThanOrEqual(max);
      w = updateWeather(w, w.timeRemaining + 1e-9, rng); // jump to the next spell
    }
  });
});

// Applying to the car --------------------------------------------------------------

describe('applyHeadlightState', () => {
  const makeHeadlight = () => new THREE.MeshStandardMaterial({ color: 0xffffff, emissive: 0xffffee });

  it('turns the headlight material on and off', () => {
    const materials = [makeHeadlight(), makeHeadlight()];
    applyHeadlightState(materials, true);
    materials.forEach((m) => expect(m.emissiveIntensity).toBe(HEADLIGHT_ON_INTENSITY));
    applyHeadlightState(materials, false);
    materials.forEach((m) => expect(m.emissiveIntensity).toBe(0));
  });

  it('sets each of the four cases correctly on a real material and beam', () => {
    const cases = [
      { name: 'day + rain', timeOfDay: at(12), isRaining: true, on: true },
      { name: 'day + dry', timeOfDay: at(12), isRaining: false, on: false },
      { name: 'night + rain', timeOfDay: at(23), isRaining: true, on: true },
      { name: 'night + dry', timeOfDay: at(23), isRaining: false, on: true },
    ];
    for (const { name, timeOfDay, isRaining, on } of cases) {
      const material = makeHeadlight();
      const beam = new THREE.SpotLight();
      const daylight = getDaylightFactor(getSunElevation(timeOfDay));
      applyHeadlightState([material], shouldHeadlightsBeOn({ timeOfDay, isRaining }), [beam], daylight);
      expect(material.emissiveIntensity, name).toBe(on ? HEADLIGHT_ON_INTENSITY : 0);
      if (on) expect(beam.intensity, name).toBeGreaterThan(0);
      else expect(beam.intensity, name).toBe(0);
    }
  });

  it('beam is full strength at night and dimmer (but still on) in daylight', () => {
    expect(getHeadlightBeamIntensity(0)).toBe(HEADLIGHT_BEAM_INTENSITY);
    expect(getHeadlightBeamIntensity(1)).toBeCloseTo(HEADLIGHT_BEAM_INTENSITY * HEADLIGHT_BEAM_DAYLIGHT_FRACTION);
    expect(getHeadlightBeamIntensity(0.5)).toBeLessThan(getHeadlightBeamIntensity(0));
    expect(getHeadlightBeamIntensity(0.5)).toBeGreaterThan(getHeadlightBeamIntensity(1));

    const beam = new THREE.SpotLight();
    applyHeadlightState([], true, [beam], 1);
    expect(beam.intensity).toBeGreaterThan(0);
    applyHeadlightState([], false, [beam], 0); // off means 0 whatever the light level
    expect(beam.intensity).toBe(0);
  });
});

// In-game test panel scenarios ---------------------------------------------------

describe('test panel scenarios', () => {
  const fixed = TEST_SCENARIOS.filter((sc) => typeof sc.expected === 'boolean');
  const byId = Object.fromEntries(TEST_SCENARIOS.map((sc) => [sc.id, sc]));
  const freezeOpts = (sc) => ({ freezeTime: !!sc.freezeTime, freezeWeather: !!sc.freezeWeather });

  it('starts with the live cycle and has unique ids', () => {
    expect(TEST_SCENARIOS[0].id).toBe('live');
    expect(new Set(TEST_SCENARIOS.map((sc) => sc.id)).size).toBe(TEST_SCENARIOS.length);
  });

  it('covers all four day/night x rain combinations', () => {
    const combos = fixed.map((sc) => `${isNighttime(sc.timeOfDay) ? 'night' : 'day'}+${sc.isRaining ? 'rain' : 'dry'}`);
    expect(new Set(combos)).toEqual(new Set(['day+dry', 'day+rain', 'night+dry', 'night+rain']));
  });

  it.each(fixed.map((sc) => [sc.label, sc]))("'%s' shows the headlight state its label promises", (_, sc) => {
    const env = createScenarioState(sc);
    expect(env.headlightsOn).toBe(sc.expected);
    expect(env.weather.isRaining).toBe(sc.isRaining);
  });

  it('fixed scenarios stay put while the game runs', () => {
    for (const sc of fixed) {
      let env = createScenarioState(sc);
      for (let i = 0; i < 600; i++) env = stepEnvironment(env, 1 / 60, Math.random, freezeOpts(sc));
      expect(env.timeOfDay).toBe(sc.timeOfDay);
      expect(env.weather.isRaining).toBe(sc.isRaining);
      expect(env.headlightsOn).toBe(sc.expected);
    }
  });

  it('sunset (live): headlights go from OFF to ON as the clock passes sunset, with no rain', () => {
    const sc = byId['sunset-live'];
    let env = createScenarioState(sc);
    expect(env.headlightsOn).toBe(false);
    let switchedAt = null;
    for (let i = 0; i < 60 * 10 && switchedAt === null; i++) {
      env = stepEnvironment(env, 1 / 60, Math.random, freezeOpts(sc));
      expect(env.weather.isRaining).toBe(false);
      if (env.headlightsOn) switchedAt = env.timeOfDay;
    }
    expect(switchedAt).not.toBeNull();
    expect(Math.abs(switchedAt - getSunsetTimeOfDay())).toBeLessThan(GAME_SECOND);
  });

  it('rain starts (live): headlights go from OFF to ON when the shower starts, clock held at noon', () => {
    const sc = byId['rain-live'];
    let env = createScenarioState(sc);
    expect(env.headlightsOn).toBe(false);
    env = stepEnvironment(env, sc.rainStartsIn - 0.1, () => 0.5, freezeOpts(sc));
    expect(env.headlightsOn).toBe(false);
    env = stepEnvironment(env, 0.2, () => 0.5, freezeOpts(sc));
    expect(env.weather.isRaining).toBe(true);
    expect(env.headlightsOn).toBe(true);
    expect(env.timeOfDay).toBe(0.5);
  });
});
