import { useState, useEffect, useCallback, useRef } from 'react';
import * as THREE from 'three';
import './TempleRunGame.css';

const LANE_COUNT = 3;
const LANE_WIDTH = 2;
const OBSTACLE_SPAWN_INTERVAL = 1100;
const COIN_SPAWN_INTERVAL = 400;
const POWERUP_SPAWN_CHANCE = 0.1;
const BASE_SPEED = 0.207; // 15% faster (was 0.18)
const MAX_SPEED = 0.518; // 15% faster (was 0.45)
const SPEED_INCREMENT = 0.000138; // 15% faster (was 0.00012)
const COINS_PER_COLLECT = 1;
const COMBO_THRESHOLD = 8;
const COMBO_MULTIPLIER = 2;

// Zone/Location color palettes for dynamic environment transitions
const ENVIRONMENT_ZONES = [
  {
    name: 'Mystic Temple',
    fog: 0x0a0515,
    skyTop: { r: 0, g: 0.02, b: 0.06 },
    skyMid: { r: 0.04, g: 0.02, b: 0.13 },
    skyBottom: { r: 0.08, g: 0.04, b: 0.19 },
    ambient: 0x504080,
    moonColor: 0xaaccff,
    rimColor: 0x00ffff,
    fillColor: 0xaa66ff,
    neonLeft: 0x00ffff,
    neonRight: 0xff00ff,
    trackEmissive: { left: 0x004444, right: 0x440044 }
  },
  {
    name: 'Crystal Cave',
    fog: 0x051015,
    skyTop: { r: 0.01, g: 0.04, b: 0.08 },
    skyMid: { r: 0.02, g: 0.08, b: 0.12 },
    skyBottom: { r: 0.04, g: 0.12, b: 0.18 },
    ambient: 0x306080,
    moonColor: 0x88ddff,
    rimColor: 0x00aaff,
    fillColor: 0x4488ff,
    neonLeft: 0x00ccff,
    neonRight: 0x00ffaa,
    trackEmissive: { left: 0x003355, right: 0x004433 }
  },
  {
    name: 'Volcanic Ruins',
    fog: 0x150505,
    skyTop: { r: 0.06, g: 0.01, b: 0.01 },
    skyMid: { r: 0.15, g: 0.04, b: 0.02 },
    skyBottom: { r: 0.25, g: 0.08, b: 0.03 },
    ambient: 0x804030,
    moonColor: 0xffaa66,
    rimColor: 0xff6600,
    fillColor: 0xff4400,
    neonLeft: 0xff4400,
    neonRight: 0xffaa00,
    trackEmissive: { left: 0x441100, right: 0x442200 }
  },
  {
    name: 'Ethereal Garden',
    fog: 0x051510,
    skyTop: { r: 0.01, g: 0.06, b: 0.04 },
    skyMid: { r: 0.02, g: 0.10, b: 0.06 },
    skyBottom: { r: 0.04, g: 0.15, b: 0.10 },
    ambient: 0x408060,
    moonColor: 0xaaffcc,
    rimColor: 0x00ff88,
    fillColor: 0x44ff88,
    neonLeft: 0x00ff66,
    neonRight: 0x88ffaa,
    trackEmissive: { left: 0x004422, right: 0x003344 }
  },
  {
    name: 'Aurora Realm',
    fog: 0x080515,
    skyTop: { r: 0.02, g: 0.01, b: 0.08 },
    skyMid: { r: 0.06, g: 0.02, b: 0.15 },
    skyBottom: { r: 0.12, g: 0.06, b: 0.22 },
    ambient: 0x604080,
    moonColor: 0xccaaff,
    rimColor: 0xaa00ff,
    fillColor: 0xff00aa,
    neonLeft: 0xaa00ff,
    neonRight: 0xff00aa,
    trackEmissive: { left: 0x330044, right: 0x440033 }
  },
  {
    name: 'Frozen Peaks',
    fog: 0x0a1520,
    skyTop: { r: 0.04, g: 0.08, b: 0.12 },
    skyMid: { r: 0.08, g: 0.12, b: 0.18 },
    skyBottom: { r: 0.12, g: 0.18, b: 0.25 },
    ambient: 0x6080a0,
    moonColor: 0xddffff,
    rimColor: 0x88ddff,
    fillColor: 0x66aaff,
    neonLeft: 0x44ccff,
    neonRight: 0xaaeeff,
    trackEmissive: { left: 0x224455, right: 0x335566 }
  }
];

const ZONE_TRANSITION_DISTANCE = 2000; // Distance between zone changes (longer zones)
const ZONE_TRANSITION_DURATION = 600; // Distance over which transition occurs

// Coin patterns for more interesting collection
const COIN_PATTERNS = [
  [[0]], // single
  [[0], [0], [0]], // vertical line
  [[-1, 0, 1]], // horizontal line
  [[0], [-1, 1], [0]], // diamond
  [[-1], [0], [1]], // diagonal
  [[0, 0], [0, 0]], // double stack
];

const TempleRunGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [score, setScore] = useState(0);
  const [combo, setCombo] = useState(0);
  const [distance, setDistance] = useState(0);
  const [activePowerup, setActivePowerup] = useState(null);
  const [hitEffect, setHitEffect] = useState(false);
  const [speedBoost, setSpeedBoost] = useState(false);
  const [currentZoneName, setCurrentZoneName] = useState(ENVIRONMENT_ZONES[0].name);

  // Refs for game state
  const scoreRef = useRef(0);
  const comboRef = useRef(0);
  const distanceRef = useRef(0);
  const speedRef = useRef(BASE_SPEED);
  const currentLaneRef = useRef(1);
  const targetLaneRef = useRef(1);
  const isJumpingRef = useRef(false);
  const isSlidingRef = useRef(false);
  const jumpVelocityRef = useRef(0);
  const playerYRef = useRef(0);
  const activePowerupRef = useRef(null);
  const shieldActiveRef = useRef(false);
  const magnetActiveRef = useRef(false);
  const multiplierActiveRef = useRef(false);
  const speedBoostRef = useRef(false);

  // Three.js refs
  const containerRef = useRef(null);
  const sceneRef = useRef(null);
  const cameraRef = useRef(null);
  const rendererRef = useRef(null);
  const playerRef = useRef(null);
  const playerMeshRef = useRef(null);
  const trackTilesRef = useRef([]);
  const obstaclesRef = useRef([]);
  const coinsRef = useRef([]);
  const powerupsRef = useRef([]);
  const particlesRef = useRef([]);
  const lightsRef = useRef({});
  const environmentRef = useRef([]);
  const currentZoneRef = useRef(0);
  const zoneTransitionRef = useRef(0); // 0-1 progress through transition

  const gameStateRef = useRef('countdown');
  const animationFrameRef = useRef(null);
  const lastSpawnTimeRef = useRef({ obstacle: 0, coin: 0 });
  const clockRef = useRef(null);
  const patternIndexRef = useRef(0);

  // Touch handling
  const touchStartRef = useRef({ x: 0, y: 0 });

  // Helper function to interpolate between two colors
  const lerpColor = useCallback((color1, color2, t) => {
    const r = color1.r + (color2.r - color1.r) * t;
    const g = color1.g + (color2.g - color1.g) * t;
    const b = color1.b + (color2.b - color1.b) * t;
    return { r, g, b };
  }, []);

  // Helper to convert hex to rgb object
  const hexToRgb = useCallback((hex) => {
    const r = ((hex >> 16) & 255) / 255;
    const g = ((hex >> 8) & 255) / 255;
    const b = (hex & 255) / 255;
    return { r, g, b };
  }, []);

  // Helper to convert rgb object to hex
  const rgbToHex = useCallback((rgb) => {
    const r = Math.floor(rgb.r * 255);
    const g = Math.floor(rgb.g * 255);
    const b = Math.floor(rgb.b * 255);
    return (r << 16) | (g << 8) | b;
  }, []);

  // Update environment colors based on current zone and transition progress
  const updateEnvironmentColors = useCallback((currentZone, nextZone, transitionProgress) => {
    const scene = sceneRef.current;
    if (!scene) return;

    const zone1 = ENVIRONMENT_ZONES[currentZone];
    const zone2 = ENVIRONMENT_ZONES[nextZone];
    const t = transitionProgress;

    // Smooth easing function for more natural transitions
    const easeT = t < 0.5 ? 2 * t * t : 1 - Math.pow(-2 * t + 2, 2) / 2;

    // Update fog color
    const fogColor1 = hexToRgb(zone1.fog);
    const fogColor2 = hexToRgb(zone2.fog);
    const newFogColor = lerpColor(fogColor1, fogColor2, easeT);
    scene.fog.color.setRGB(newFogColor.r, newFogColor.g, newFogColor.b);

    // Update sky shader uniforms
    environmentRef.current.forEach(item => {
      if (item.type === 'sky' && item.mesh.material.uniforms) {
        const uniforms = item.mesh.material.uniforms;

        const newTopColor = lerpColor(zone1.skyTop, zone2.skyTop, easeT);
        const newMidColor = lerpColor(zone1.skyMid, zone2.skyMid, easeT);
        const newBottomColor = lerpColor(zone1.skyBottom, zone2.skyBottom, easeT);

        uniforms.topColor.value.setRGB(newTopColor.r, newTopColor.g, newTopColor.b);
        uniforms.midColor.value.setRGB(newMidColor.r, newMidColor.g, newMidColor.b);
        uniforms.bottomColor.value.setRGB(newBottomColor.r, newBottomColor.g, newBottomColor.b);
      }
    });

    // Update lighting colors
    const lights = lightsRef.current;

    // Ambient light
    if (lights.ambient) {
      const ambColor1 = hexToRgb(zone1.ambient);
      const ambColor2 = hexToRgb(zone2.ambient);
      const newAmbColor = lerpColor(ambColor1, ambColor2, easeT);
      lights.ambient.color.setRGB(newAmbColor.r, newAmbColor.g, newAmbColor.b);
    }

    // Moon/directional light
    if (lights.moon) {
      const moonColor1 = hexToRgb(zone1.moonColor);
      const moonColor2 = hexToRgb(zone2.moonColor);
      const newMoonColor = lerpColor(moonColor1, moonColor2, easeT);
      lights.moon.color.setRGB(newMoonColor.r, newMoonColor.g, newMoonColor.b);
    }

    // Rim light
    if (lights.rim) {
      const rimColor1 = hexToRgb(zone1.rimColor);
      const rimColor2 = hexToRgb(zone2.rimColor);
      const newRimColor = lerpColor(rimColor1, rimColor2, easeT);
      lights.rim.color.setRGB(newRimColor.r, newRimColor.g, newRimColor.b);
    }

    // Fill light
    if (lights.fill) {
      const fillColor1 = hexToRgb(zone1.fillColor);
      const fillColor2 = hexToRgb(zone2.fillColor);
      const newFillColor = lerpColor(fillColor1, fillColor2, easeT);
      lights.fill.color.setRGB(newFillColor.r, newFillColor.g, newFillColor.b);
    }

    // Neon lights
    if (lights.leftNeon) {
      const neonLeftColor1 = hexToRgb(zone1.neonLeft);
      const neonLeftColor2 = hexToRgb(zone2.neonLeft);
      const newNeonLeftColor = lerpColor(neonLeftColor1, neonLeftColor2, easeT);
      lights.leftNeon.color.setRGB(newNeonLeftColor.r, newNeonLeftColor.g, newNeonLeftColor.b);
    }

    if (lights.rightNeon) {
      const neonRightColor1 = hexToRgb(zone1.neonRight);
      const neonRightColor2 = hexToRgb(zone2.neonRight);
      const newNeonRightColor = lerpColor(neonRightColor1, neonRightColor2, easeT);
      lights.rightNeon.color.setRGB(newNeonRightColor.r, newNeonRightColor.g, newNeonRightColor.b);
    }

    // Update track rail emissive colors
    const leftEmissive1 = hexToRgb(zone1.trackEmissive.left);
    const leftEmissive2 = hexToRgb(zone2.trackEmissive.left);
    const rightEmissive1 = hexToRgb(zone1.trackEmissive.right);
    const rightEmissive2 = hexToRgb(zone2.trackEmissive.right);
    const newLeftEmissive = lerpColor(leftEmissive1, leftEmissive2, easeT);
    const newRightEmissive = lerpColor(rightEmissive1, rightEmissive2, easeT);

    trackTilesRef.current.forEach(tile => {
      if (tile.userData?.isLeftRail && tile.material?.emissive) {
        tile.material.emissive.setRGB(newLeftEmissive.r, newLeftEmissive.g, newLeftEmissive.b);
      } else if (tile.userData?.isRightRail && tile.material?.emissive) {
        tile.material.emissive.setRGB(newRightEmissive.r, newRightEmissive.g, newRightEmissive.b);
      }
    });
  }, [lerpColor, hexToRgb]);

  useEffect(() => {
    gameStateRef.current = gameState;
  }, [gameState]);

  // Initialize Three.js scene
  const initThreeJS = useCallback(() => {
    if (!containerRef.current) return () => {};

    const container = containerRef.current;
    const width = container.clientWidth || 400;
    const height = container.clientHeight || 600;

    // Scene
    const scene = new THREE.Scene();
    scene.fog = new THREE.FogExp2(0x0a0515, 0.025);
    sceneRef.current = scene;

    // Camera
    const camera = new THREE.PerspectiveCamera(75, width / height, 0.1, 100);
    camera.position.set(0, 3.5, 9);
    camera.lookAt(0, 0.5, -15);
    cameraRef.current = camera;

    // Renderer
    const renderer = new THREE.WebGLRenderer({
      antialias: true,
      alpha: true,
      powerPreference: 'high-performance'
    });
    renderer.setSize(width, height);
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
    renderer.shadowMap.enabled = true;
    renderer.shadowMap.type = THREE.PCFSoftShadowMap;
    renderer.toneMapping = THREE.ACESFilmicToneMapping;
    renderer.toneMappingExposure = 1.3;
    container.appendChild(renderer.domElement);
    rendererRef.current = renderer;

    // Lighting
    setupLighting(scene);

    // Create environment
    createEnvironment(scene);

    // Create player
    createPlayer(scene);

    // Create initial track
    createTrack(scene);

    // Clock for animations
    clockRef.current = new THREE.Clock();

    // Initial render
    renderer.render(scene, camera);

    // Handle resize
    const handleResize = () => {
      if (!containerRef.current) return;
      const w = containerRef.current.clientWidth;
      const h = containerRef.current.clientHeight;
      camera.aspect = w / h;
      camera.updateProjectionMatrix();
      renderer.setSize(w, h);
    };
    window.addEventListener('resize', handleResize);

    return () => {
      window.removeEventListener('resize', handleResize);
      if (renderer) {
        renderer.dispose();
      }
      if (container && renderer.domElement && container.contains(renderer.domElement)) {
        container.removeChild(renderer.domElement);
      }
    };
  }, []);

  const setupLighting = (scene) => {
    // Ambient light - slightly brighter
    const ambient = new THREE.AmbientLight(0x504080, 0.6);
    scene.add(ambient);
    lightsRef.current.ambient = ambient;

    // Main directional light (moon-like)
    const moon = new THREE.DirectionalLight(0xaaccff, 0.9);
    moon.position.set(5, 15, 5);
    moon.castShadow = true;
    moon.shadow.mapSize.width = 1024;
    moon.shadow.mapSize.height = 1024;
    moon.shadow.camera.near = 0.5;
    moon.shadow.camera.far = 50;
    moon.shadow.camera.left = -15;
    moon.shadow.camera.right = 15;
    moon.shadow.camera.top = 15;
    moon.shadow.camera.bottom = -15;
    scene.add(moon);
    lightsRef.current.moon = moon;

    // Cyan rim light from front - enhanced
    const rimLight = new THREE.DirectionalLight(0x00ffff, 0.5);
    rimLight.position.set(0, 3, -15);
    scene.add(rimLight);
    lightsRef.current.rim = rimLight;

    // Purple fill light - enhanced
    const fillLight = new THREE.DirectionalLight(0xaa66ff, 0.35);
    fillLight.position.set(-5, 4, 0);
    scene.add(fillLight);
    lightsRef.current.fill = fillLight;

    // Orange accent light
    const accentLight = new THREE.DirectionalLight(0xff6600, 0.2);
    accentLight.position.set(5, 2, 5);
    scene.add(accentLight);
    lightsRef.current.accent = accentLight;

    // Pulsing neon track lights
    const leftNeon = new THREE.PointLight(0x00ffff, 1.5, 12);
    leftNeon.position.set(-3.5, 0.8, 0);
    scene.add(leftNeon);
    lightsRef.current.leftNeon = leftNeon;

    const rightNeon = new THREE.PointLight(0xff00ff, 1.5, 12);
    rightNeon.position.set(3.5, 0.8, 0);
    scene.add(rightNeon);
    lightsRef.current.rightNeon = rightNeon;

    // Forward light showing path
    const pathLight = new THREE.SpotLight(0x00ffaa, 2, 30, Math.PI / 6, 0.5);
    pathLight.position.set(0, 8, -10);
    pathLight.target.position.set(0, 0, -30);
    scene.add(pathLight);
    scene.add(pathLight.target);
    lightsRef.current.pathLight = pathLight;
  };

  const createEnvironment = (scene) => {
    // Dynamic skybox with animated gradient
    const skyGeo = new THREE.SphereGeometry(60, 32, 32);
    const skyMat = new THREE.ShaderMaterial({
      uniforms: {
        topColor: { value: new THREE.Color(0x000510) },
        midColor: { value: new THREE.Color(0x0a0520) },
        bottomColor: { value: new THREE.Color(0x150a30) },
        time: { value: 0 }
      },
      vertexShader: `
        varying vec3 vWorldPosition;
        void main() {
          vec4 worldPosition = modelMatrix * vec4(position, 1.0);
          vWorldPosition = worldPosition.xyz;
          gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
        }
      `,
      fragmentShader: `
        uniform vec3 topColor;
        uniform vec3 midColor;
        uniform vec3 bottomColor;
        uniform float time;
        varying vec3 vWorldPosition;
        void main() {
          float h = normalize(vWorldPosition).y;
          vec3 color;
          if (h > 0.0) {
            color = mix(midColor, topColor, pow(h, 0.5));
          } else {
            color = mix(midColor, bottomColor, pow(-h, 0.5));
          }
          // Add subtle aurora effect
          float aurora = sin(vWorldPosition.x * 0.1 + time) * 0.5 + 0.5;
          aurora *= smoothstep(0.2, 0.8, h);
          color += vec3(0.0, aurora * 0.05, aurora * 0.08);
          gl_FragColor = vec4(color, 1.0);
        }
      `,
      side: THREE.BackSide
    });
    const sky = new THREE.Mesh(skyGeo, skyMat);
    scene.add(sky);
    environmentRef.current.push({ mesh: sky, type: 'sky' });

    // Enhanced stars with twinkling
    const starsGeo = new THREE.BufferGeometry();
    const starPositions = [];
    const starSizes = [];
    for (let i = 0; i < 2000; i++) {
      const theta = Math.random() * Math.PI * 2;
      const phi = Math.acos(Math.random() * 2 - 1);
      const r = 45 + Math.random() * 10;
      starPositions.push(
        r * Math.sin(phi) * Math.cos(theta),
        Math.abs(r * Math.sin(phi) * Math.sin(theta)),
        r * Math.cos(phi)
      );
      starSizes.push(Math.random() * 2 + 0.5);
    }
    starsGeo.setAttribute('position', new THREE.Float32BufferAttribute(starPositions, 3));
    starsGeo.setAttribute('size', new THREE.Float32BufferAttribute(starSizes, 1));

    const starsMat = new THREE.ShaderMaterial({
      uniforms: {
        time: { value: 0 }
      },
      vertexShader: `
        attribute float size;
        varying float vSize;
        void main() {
          vSize = size;
          vec4 mvPosition = modelViewMatrix * vec4(position, 1.0);
          gl_PointSize = size * (300.0 / -mvPosition.z);
          gl_Position = projectionMatrix * mvPosition;
        }
      `,
      fragmentShader: `
        uniform float time;
        varying float vSize;
        void main() {
          float dist = length(gl_PointCoord - vec2(0.5));
          if (dist > 0.5) discard;
          float twinkle = sin(time * (3.0 + vSize) + vSize * 10.0) * 0.3 + 0.7;
          float alpha = (1.0 - dist * 2.0) * twinkle;
          gl_FragColor = vec4(1.0, 1.0, 1.0, alpha);
        }
      `,
      transparent: true,
      blending: THREE.AdditiveBlending
    });
    const stars = new THREE.Points(starsGeo, starsMat);
    scene.add(stars);
    environmentRef.current.push({ mesh: stars, type: 'stars' });

    // Shooting stars (occasional)
    const shootingStarGeo = new THREE.BufferGeometry();
    const shootingPositions = new Float32Array(30 * 3);
    shootingStarGeo.setAttribute('position', new THREE.BufferAttribute(shootingPositions, 3));
    const shootingMat = new THREE.PointsMaterial({
      color: 0xffffff,
      size: 0.2,
      transparent: true,
      opacity: 0,
      blending: THREE.AdditiveBlending
    });
    const shootingStar = new THREE.Points(shootingStarGeo, shootingMat);
    scene.add(shootingStar);
    environmentRef.current.push({ mesh: shootingStar, type: 'shooting', active: false, timer: 0 });

    // Crystal mountains with glow
    for (let i = 0; i < 12; i++) {
      const height = 15 + Math.random() * 12;
      const mountainGeo = new THREE.ConeGeometry(5 + Math.random() * 3, height, 6);
      const isGlowing = Math.random() < 0.3;
      const mountainMat = new THREE.MeshStandardMaterial({
        color: isGlowing ? 0x2a1a4a : new THREE.Color().setHSL(0.75, 0.4, 0.08 + Math.random() * 0.05),
        roughness: 0.6,
        metalness: 0.4,
        emissive: isGlowing ? 0x4400aa : 0x000000,
        emissiveIntensity: isGlowing ? 0.3 : 0
      });
      const mountain = new THREE.Mesh(mountainGeo, mountainMat);
      mountain.position.set(
        (Math.random() - 0.5) * 80,
        height / 2 - 3,
        -35 - Math.random() * 25
      );
      mountain.rotation.y = Math.random() * Math.PI;
      scene.add(mountain);
    }

    // Floating crystals
    for (let i = 0; i < 8; i++) {
      const crystalGeo = new THREE.OctahedronGeometry(0.5 + Math.random() * 0.5, 0);
      const crystalMat = new THREE.MeshStandardMaterial({
        color: 0x00ffff,
        roughness: 0.1,
        metalness: 0.9,
        emissive: 0x004444,
        emissiveIntensity: 0.5,
        transparent: true,
        opacity: 0.8
      });
      const crystal = new THREE.Mesh(crystalGeo, crystalMat);
      crystal.position.set(
        (Math.random() - 0.5) * 20,
        3 + Math.random() * 5,
        -10 - Math.random() * 30
      );
      crystal.userData = {
        baseY: crystal.position.y,
        speed: 0.5 + Math.random() * 0.5,
        rotSpeed: 0.01 + Math.random() * 0.02
      };
      scene.add(crystal);
      environmentRef.current.push({ mesh: crystal, type: 'crystal' });
    }

    // Ground with better material
    const groundGeo = new THREE.PlaneGeometry(120, 120, 20, 20);
    const groundMat = new THREE.MeshStandardMaterial({
      color: 0x0a0a15,
      roughness: 0.95,
      metalness: 0.05
    });
    const ground = new THREE.Mesh(groundGeo, groundMat);
    ground.rotation.x = -Math.PI / 2;
    ground.position.y = -0.5;
    ground.receiveShadow = true;
    scene.add(ground);

    // Temple pillars with glowing runes
    for (let z = 0; z > -60; z -= 7) {
      [-5, 5].forEach((x, idx) => {
        // Main pillar
        const pillarGeo = new THREE.CylinderGeometry(0.35, 0.45, 7, 8);
        const pillarMat = new THREE.MeshStandardMaterial({
          color: 0x3a3a4a,
          roughness: 0.6,
          metalness: 0.4
        });
        const pillar = new THREE.Mesh(pillarGeo, pillarMat);
        pillar.position.set(x, 3, z);
        pillar.castShadow = true;
        scene.add(pillar);

        // Rune rings on pillar
        const runeGeo = new THREE.TorusGeometry(0.4, 0.03, 8, 16);
        const runeMat = new THREE.MeshBasicMaterial({
          color: idx === 0 ? 0x00ffff : 0xff00ff,
          transparent: true,
          opacity: 0.7
        });
        for (let ry = 1; ry < 6; ry += 1.5) {
          const rune = new THREE.Mesh(runeGeo, runeMat.clone());
          rune.position.set(x, ry, z);
          rune.rotation.x = Math.PI / 2;
          scene.add(rune);
          environmentRef.current.push({ mesh: rune, type: 'rune', baseY: ry, pillarX: x });
        }

        // Pillar top ornament
        const topGeo = new THREE.BoxGeometry(0.9, 0.35, 0.9);
        const top = new THREE.Mesh(topGeo, pillarMat);
        top.position.set(x, 6.5, z);
        scene.add(top);

        // Glowing orb with pulsing effect
        const orbGeo = new THREE.SphereGeometry(0.2, 16, 16);
        const orbMat = new THREE.MeshBasicMaterial({
          color: idx === 0 ? 0x00ffff : 0xff00ff,
          transparent: true,
          opacity: 0.9
        });
        const orb = new THREE.Mesh(orbGeo, orbMat);
        orb.position.set(x, 6.9, z);
        scene.add(orb);
        environmentRef.current.push({ mesh: orb, type: 'orb', baseScale: 1 });

        // Light beam from orb
        const beamGeo = new THREE.CylinderGeometry(0.02, 0.15, 2, 8);
        const beamMat = new THREE.MeshBasicMaterial({
          color: idx === 0 ? 0x00ffff : 0xff00ff,
          transparent: true,
          opacity: 0.3
        });
        const beam = new THREE.Mesh(beamGeo, beamMat);
        beam.position.set(x, 8, z);
        scene.add(beam);
      });
    }
  };

  const createTrack = (scene) => {
    // Enhanced track with neon edges
    const trackMaterial = new THREE.MeshStandardMaterial({
      color: 0x1a1a2a,
      roughness: 0.5,
      metalness: 0.5
    });

    for (let i = 0; i < 25; i++) {
      const tileGeo = new THREE.BoxGeometry(6.5, 0.35, 2.8);
      const tile = new THREE.Mesh(tileGeo, trackMaterial);
      tile.position.set(0, -0.15, -i * 2.8);
      tile.receiveShadow = true;
      scene.add(tile);
      trackTilesRef.current.push(tile);

      // Glowing lane dividers
      [-LANE_WIDTH / 3, LANE_WIDTH / 3].forEach((x, idx) => {
        const lineGeo = new THREE.BoxGeometry(0.06, 0.03, 2.7);
        const lineMat = new THREE.MeshBasicMaterial({
          color: 0x00ffff,
          transparent: true,
          opacity: 0.7
        });
        const line = new THREE.Mesh(lineGeo, lineMat);
        line.position.set(x * 3, 0.02, -i * 2.8);
        scene.add(line);
        trackTilesRef.current.push(line);
      });

      // Enhanced edge rails with glow
      [-3.25, 3.25].forEach((x, idx) => {
        const railGeo = new THREE.BoxGeometry(0.2, 0.5, 2.8);
        const railMat = new THREE.MeshStandardMaterial({
          color: idx === 0 ? 0x003333 : 0x330033,
          roughness: 0.4,
          metalness: 0.7,
          emissive: idx === 0 ? 0x004444 : 0x440044,
          emissiveIntensity: 0.4
        });
        const rail = new THREE.Mesh(railGeo, railMat);
        rail.position.set(x, 0.1, -i * 2.8);
        rail.userData = { isLeftRail: idx === 0, isRightRail: idx === 1 };
        scene.add(rail);
        trackTilesRef.current.push(rail);
      });

      // Periodic light strips on track
      if (i % 3 === 0) {
        const stripGeo = new THREE.BoxGeometry(6, 0.02, 0.1);
        const stripMat = new THREE.MeshBasicMaterial({
          color: 0x00ffaa,
          transparent: true,
          opacity: 0.5
        });
        const strip = new THREE.Mesh(stripGeo, stripMat);
        strip.position.set(0, 0.01, -i * 2.8 - 1.4);
        scene.add(strip);
        trackTilesRef.current.push(strip);
      }
    }
  };

  const createPlayer = (scene) => {
    const playerGroup = new THREE.Group();

    // Glowing aura base
    const auraGeo = new THREE.RingGeometry(0.3, 0.5, 32);
    const auraMat = new THREE.MeshBasicMaterial({
      color: 0x00ffff,
      transparent: true,
      opacity: 0.3,
      side: THREE.DoubleSide
    });
    const aura = new THREE.Mesh(auraGeo, auraMat);
    aura.rotation.x = -Math.PI / 2;
    aura.position.y = 0.01;
    playerGroup.add(aura);
    playerGroup.userData.aura = aura;

    // Body with better materials
    const bodyGeo = new THREE.CapsuleGeometry(0.28, 0.65, 12, 24);
    const bodyMat = new THREE.MeshStandardMaterial({
      color: 0xff6b6b,
      roughness: 0.3,
      metalness: 0.4,
      emissive: 0x441111,
      emissiveIntensity: 0.3
    });
    const body = new THREE.Mesh(bodyGeo, bodyMat);
    body.position.y = 0.65;
    body.castShadow = true;
    playerGroup.add(body);

    // Outfit details - belt
    const beltGeo = new THREE.TorusGeometry(0.28, 0.04, 8, 16);
    const beltMat = new THREE.MeshStandardMaterial({
      color: 0xffd700,
      roughness: 0.2,
      metalness: 0.8
    });
    const belt = new THREE.Mesh(beltGeo, beltMat);
    belt.position.y = 0.4;
    belt.rotation.x = Math.PI / 2;
    playerGroup.add(belt);

    // Head with better shape
    const headGeo = new THREE.SphereGeometry(0.22, 24, 24);
    const headMat = new THREE.MeshStandardMaterial({
      color: 0xffcc99,
      roughness: 0.6
    });
    const head = new THREE.Mesh(headGeo, headMat);
    head.position.y = 1.22;
    head.castShadow = true;
    playerGroup.add(head);

    // Hair
    const hairGeo = new THREE.SphereGeometry(0.23, 16, 16, 0, Math.PI * 2, 0, Math.PI / 2);
    const hairMat = new THREE.MeshStandardMaterial({
      color: 0x332211,
      roughness: 0.8
    });
    const hair = new THREE.Mesh(hairGeo, hairMat);
    hair.position.y = 1.25;
    hair.rotation.x = -0.2;
    playerGroup.add(hair);

    // Eyes with glow
    const eyeGeo = new THREE.SphereGeometry(0.045, 12, 12);
    const eyeMat = new THREE.MeshBasicMaterial({ color: 0x000000 });
    const eyeGlowGeo = new THREE.SphereGeometry(0.025, 8, 8);
    const eyeGlowMat = new THREE.MeshBasicMaterial({ color: 0xffffff });
    [-0.08, 0.08].forEach(x => {
      const eye = new THREE.Mesh(eyeGeo, eyeMat);
      eye.position.set(x, 1.25, 0.17);
      playerGroup.add(eye);
      const eyeGlow = new THREE.Mesh(eyeGlowGeo, eyeGlowMat);
      eyeGlow.position.set(x + 0.015, 1.26, 0.19);
      playerGroup.add(eyeGlow);
    });

    // Arms
    const armGeo = new THREE.CapsuleGeometry(0.06, 0.3, 6, 12);
    const armMat = new THREE.MeshStandardMaterial({
      color: 0xff6b6b,
      roughness: 0.3
    });
    const leftArm = new THREE.Mesh(armGeo, armMat);
    leftArm.position.set(-0.35, 0.7, 0);
    leftArm.rotation.z = 0.3;
    playerGroup.add(leftArm);
    playerGroup.userData.leftArm = leftArm;

    const rightArm = new THREE.Mesh(armGeo, armMat);
    rightArm.position.set(0.35, 0.7, 0);
    rightArm.rotation.z = -0.3;
    playerGroup.add(rightArm);
    playerGroup.userData.rightArm = rightArm;

    // Legs
    const legGeo = new THREE.CapsuleGeometry(0.07, 0.25, 6, 12);
    const legMat = new THREE.MeshStandardMaterial({
      color: 0x2a2a4a,
      roughness: 0.5
    });
    const leftLeg = new THREE.Mesh(legGeo, legMat);
    leftLeg.position.set(-0.12, 0.15, 0);
    playerGroup.add(leftLeg);
    playerGroup.userData.leftLeg = leftLeg;

    const rightLeg = new THREE.Mesh(legGeo, legMat);
    rightLeg.position.set(0.12, 0.15, 0);
    playerGroup.add(rightLeg);
    playerGroup.userData.rightLeg = rightLeg;

    // Enhanced trail effect
    const trailGeo = new THREE.BufferGeometry();
    const trailPositions = new Float32Array(50 * 3);
    const trailOpacities = new Float32Array(50);
    trailGeo.setAttribute('position', new THREE.BufferAttribute(trailPositions, 3));
    trailGeo.setAttribute('opacity', new THREE.BufferAttribute(trailOpacities, 1));
    const trailMat = new THREE.ShaderMaterial({
      uniforms: {
        color: { value: new THREE.Color(0x00ffff) }
      },
      vertexShader: `
        attribute float opacity;
        varying float vOpacity;
        void main() {
          vOpacity = opacity;
          vec4 mvPosition = modelViewMatrix * vec4(position, 1.0);
          gl_PointSize = 8.0 * (1.0 / -mvPosition.z);
          gl_Position = projectionMatrix * mvPosition;
        }
      `,
      fragmentShader: `
        uniform vec3 color;
        varying float vOpacity;
        void main() {
          float dist = length(gl_PointCoord - vec2(0.5));
          if (dist > 0.5) discard;
          gl_FragColor = vec4(color, vOpacity * (1.0 - dist * 2.0));
        }
      `,
      transparent: true,
      blending: THREE.AdditiveBlending
    });
    const trail = new THREE.Points(trailGeo, trailMat);
    scene.add(trail);
    playerGroup.userData.trail = trail;
    playerGroup.userData.trailPositions = [];

    // Shield mesh
    const shieldGeo = new THREE.SphereGeometry(0.7, 32, 32);
    const shieldMat = new THREE.MeshBasicMaterial({
      color: 0x00aaff,
      transparent: true,
      opacity: 0,
      side: THREE.DoubleSide,
      blending: THREE.AdditiveBlending
    });
    const shield = new THREE.Mesh(shieldGeo, shieldMat);
    shield.position.y = 0.6;
    playerGroup.add(shield);
    playerGroup.userData.shield = shield;

    // Speed boost effect
    const boostGeo = new THREE.ConeGeometry(0.3, 1, 8);
    const boostMat = new THREE.MeshBasicMaterial({
      color: 0xff6600,
      transparent: true,
      opacity: 0,
      blending: THREE.AdditiveBlending
    });
    const boost = new THREE.Mesh(boostGeo, boostMat);
    boost.position.set(0, 0.5, 0.8);
    boost.rotation.x = -Math.PI / 2;
    playerGroup.add(boost);
    playerGroup.userData.boost = boost;

    playerGroup.position.set(0, 0, 5);
    scene.add(playerGroup);
    playerRef.current = playerGroup;
    playerMeshRef.current = body;
  };

  const spawnObstacle = useCallback(() => {
    const scene = sceneRef.current;
    if (!scene) return;

    const lane = Math.floor(Math.random() * LANE_COUNT);
    const laneX = (lane - 1) * LANE_WIDTH;
    const types = ['crystal', 'pillar', 'fire', 'barrier', 'spikes', 'beam'];
    const type = types[Math.floor(Math.random() * types.length)];

    let obstacle;
    let height = 'low';

    switch (type) {
      case 'crystal': {
        const group = new THREE.Group();
        const crystalGeo = new THREE.OctahedronGeometry(0.45, 0);
        const crystalMat = new THREE.MeshStandardMaterial({
          color: 0xff00ff,
          roughness: 0.1,
          metalness: 0.9,
          emissive: 0x660066,
          emissiveIntensity: 0.5
        });
        const crystal = new THREE.Mesh(crystalGeo, crystalMat);
        crystal.position.y = 0.5;
        crystal.rotation.y = Math.random() * Math.PI;
        group.add(crystal);
        // Glow ring
        const ringGeo = new THREE.RingGeometry(0.3, 0.5, 16);
        const ringMat = new THREE.MeshBasicMaterial({
          color: 0xff00ff,
          transparent: true,
          opacity: 0.5,
          side: THREE.DoubleSide
        });
        const ring = new THREE.Mesh(ringGeo, ringMat);
        ring.rotation.x = -Math.PI / 2;
        ring.position.y = 0.01;
        group.add(ring);
        obstacle = group;
        obstacle.position.set(laneX, 0, -70);
        obstacle.userData = { crystal, ring };
        break;
      }
      case 'pillar': {
        const geo = new THREE.CylinderGeometry(0.35, 0.35, 2.8, 8);
        const mat = new THREE.MeshStandardMaterial({
          color: 0x4a3a2a,
          roughness: 0.7,
          metalness: 0.3
        });
        obstacle = new THREE.Mesh(geo, mat);
        obstacle.position.set(laneX, 1.4, -70);
        height = 'tall';
        break;
      }
      case 'fire': {
        const group = new THREE.Group();
        const baseGeo = new THREE.CylinderGeometry(0.35, 0.45, 0.35, 8);
        const baseMat = new THREE.MeshStandardMaterial({
          color: 0x222222,
          roughness: 0.9
        });
        const base = new THREE.Mesh(baseGeo, baseMat);
        base.position.y = 0.18;
        group.add(base);
        // Multi-layered fire
        for (let f = 0; f < 3; f++) {
          const fireGeo = new THREE.ConeGeometry(0.25 - f * 0.05, 0.9 - f * 0.15, 8);
          const fireMat = new THREE.MeshBasicMaterial({
            color: f === 0 ? 0xff2200 : f === 1 ? 0xff6600 : 0xffaa00,
            transparent: true,
            opacity: 0.9 - f * 0.2
          });
          const fire = new THREE.Mesh(fireGeo, fireMat);
          fire.position.y = 0.6 + f * 0.1;
          fire.userData = { offset: f * 0.5 };
          group.add(fire);
        }
        obstacle = group;
        obstacle.position.set(laneX, 0, -70);
        obstacle.userData = { type: 'fire' };
        break;
      }
      case 'barrier': {
        const group = new THREE.Group();
        const geo = new THREE.BoxGeometry(1.6, 0.7, 0.35);
        const mat = new THREE.MeshStandardMaterial({
          color: 0xcc0000,
          emissive: 0x440000,
          emissiveIntensity: 0.6,
          roughness: 0.3,
          metalness: 0.6
        });
        const barrier = new THREE.Mesh(geo, mat);
        barrier.position.y = 0.35;
        group.add(barrier);
        // Warning stripes
        const stripeGeo = new THREE.BoxGeometry(0.15, 0.65, 0.36);
        const stripeMat = new THREE.MeshBasicMaterial({ color: 0xffff00 });
        for (let s = -0.6; s <= 0.6; s += 0.4) {
          const stripe = new THREE.Mesh(stripeGeo, stripeMat);
          stripe.position.set(s, 0.35, 0);
          group.add(stripe);
        }
        obstacle = group;
        obstacle.position.set(laneX, 0, -70);
        break;
      }
      case 'spikes': {
        const group = new THREE.Group();
        for (let s = -0.4; s <= 0.4; s += 0.4) {
          const spikeGeo = new THREE.ConeGeometry(0.15, 0.6, 4);
          const spikeMat = new THREE.MeshStandardMaterial({
            color: 0x888888,
            roughness: 0.3,
            metalness: 0.8
          });
          const spike = new THREE.Mesh(spikeGeo, spikeMat);
          spike.position.set(s, 0.3, 0);
          group.add(spike);
        }
        obstacle = group;
        obstacle.position.set(laneX, 0, -70);
        break;
      }
      case 'beam': {
        const group = new THREE.Group();
        // Posts
        for (let p = -0.6; p <= 0.6; p += 1.2) {
          const postGeo = new THREE.CylinderGeometry(0.08, 0.08, 1.8, 8);
          const postMat = new THREE.MeshStandardMaterial({ color: 0x444444 });
          const post = new THREE.Mesh(postGeo, postMat);
          post.position.set(p, 0.9, 0);
          group.add(post);
        }
        // Laser beam
        const beamGeo = new THREE.CylinderGeometry(0.04, 0.04, 1.2, 8);
        const beamMat = new THREE.MeshBasicMaterial({
          color: 0xff0000,
          transparent: true,
          opacity: 0.9
        });
        const beam = new THREE.Mesh(beamGeo, beamMat);
        beam.position.y = 0.9;
        beam.rotation.z = Math.PI / 2;
        group.add(beam);
        obstacle = group;
        obstacle.position.set(laneX, 0, -70);
        obstacle.userData = { beam };
        break;
      }
    }

    obstacle.userData = { ...obstacle.userData, lane, type, height };
    obstacle.castShadow = true;
    scene.add(obstacle);
    obstaclesRef.current.push(obstacle);
  }, []);

  const spawnCoinPattern = useCallback(() => {
    const scene = sceneRef.current;
    if (!scene) return;

    const baseLane = Math.floor(Math.random() * LANE_COUNT);

    // Spawn powerup occasionally
    if (Math.random() < POWERUP_SPAWN_CHANCE) {
      const types = ['magnet', 'shield', 'multiplier', 'speed'];
      const type = types[Math.floor(Math.random() * types.length)];
      const laneX = (baseLane - 1) * LANE_WIDTH;

      const group = new THREE.Group();
      const geo = new THREE.OctahedronGeometry(0.35, 0);
      let color;
      switch (type) {
        case 'magnet': color = 0xff4444; break;
        case 'shield': color = 0x4444ff; break;
        case 'multiplier': color = 0xffff00; break;
        case 'speed': color = 0x00ff00; break;
      }
      const mat = new THREE.MeshBasicMaterial({
        color,
        transparent: true,
        opacity: 0.9
      });
      const powerup = new THREE.Mesh(geo, mat);
      group.add(powerup);

      // Glow effect
      const glowGeo = new THREE.SphereGeometry(0.5, 16, 16);
      const glowMat = new THREE.MeshBasicMaterial({
        color,
        transparent: true,
        opacity: 0.2,
        blending: THREE.AdditiveBlending
      });
      const glow = new THREE.Mesh(glowGeo, glowMat);
      group.add(glow);

      group.position.set(laneX, 1.2, -70);
      group.userData = { lane: baseLane, type, isPowerup: true };
      scene.add(group);
      powerupsRef.current.push(group);
      return;
    }

    // Select pattern
    const pattern = COIN_PATTERNS[patternIndexRef.current % COIN_PATTERNS.length];
    patternIndexRef.current++;

    pattern.forEach((row, rowIdx) => {
      row.forEach(laneOffset => {
        const lane = Math.max(0, Math.min(2, baseLane + laneOffset));
        const laneX = (lane - 1) * LANE_WIDTH;

        const coinGroup = new THREE.Group();

        // Coin
        const coinGeo = new THREE.CylinderGeometry(0.22, 0.22, 0.06, 20);
        const coinMat = new THREE.MeshStandardMaterial({
          color: 0xffd700,
          roughness: 0.2,
          metalness: 0.9,
          emissive: 0xaa6600,
          emissiveIntensity: 0.4
        });
        const coin = new THREE.Mesh(coinGeo, coinMat);
        coin.rotation.x = Math.PI / 2;
        coinGroup.add(coin);

        // Coin detail (center circle)
        const detailGeo = new THREE.CylinderGeometry(0.12, 0.12, 0.07, 16);
        const detailMat = new THREE.MeshStandardMaterial({
          color: 0xffaa00,
          roughness: 0.3,
          metalness: 0.8
        });
        const detail = new THREE.Mesh(detailGeo, detailMat);
        detail.rotation.x = Math.PI / 2;
        coinGroup.add(detail);

        // Glow
        const glowGeo = new THREE.SphereGeometry(0.3, 8, 8);
        const glowMat = new THREE.MeshBasicMaterial({
          color: 0xffd700,
          transparent: true,
          opacity: 0.15,
          blending: THREE.AdditiveBlending
        });
        const glow = new THREE.Mesh(glowGeo, glowMat);
        coinGroup.add(glow);

        coinGroup.position.set(laneX, 0.9, -70 - rowIdx * 2);
        coinGroup.userData = { lane };
        scene.add(coinGroup);
        coinsRef.current.push(coinGroup);
      });
    });
  }, []);

  const createParticleExplosion = useCallback((position, color, count = 20) => {
    const scene = sceneRef.current;
    if (!scene) return;

    const geo = new THREE.BufferGeometry();
    const positions = new Float32Array(count * 3);
    const velocities = [];
    const sizes = new Float32Array(count);

    for (let i = 0; i < count; i++) {
      positions[i * 3] = position.x;
      positions[i * 3 + 1] = position.y;
      positions[i * 3 + 2] = position.z;
      velocities.push(new THREE.Vector3(
        (Math.random() - 0.5) * 0.3,
        Math.random() * 0.2 + 0.1,
        (Math.random() - 0.5) * 0.3
      ));
      sizes[i] = 0.1 + Math.random() * 0.15;
    }

    geo.setAttribute('position', new THREE.BufferAttribute(positions, 3));
    geo.setAttribute('size', new THREE.BufferAttribute(sizes, 1));

    const mat = new THREE.ShaderMaterial({
      uniforms: {
        color: { value: new THREE.Color(color) },
        opacity: { value: 1.0 }
      },
      vertexShader: `
        attribute float size;
        void main() {
          vec4 mvPosition = modelViewMatrix * vec4(position, 1.0);
          gl_PointSize = size * (200.0 / -mvPosition.z);
          gl_Position = projectionMatrix * mvPosition;
        }
      `,
      fragmentShader: `
        uniform vec3 color;
        uniform float opacity;
        void main() {
          float dist = length(gl_PointCoord - vec2(0.5));
          if (dist > 0.5) discard;
          float alpha = opacity * (1.0 - dist * 2.0);
          gl_FragColor = vec4(color, alpha);
        }
      `,
      transparent: true,
      blending: THREE.AdditiveBlending
    });

    const particles = new THREE.Points(geo, mat);
    particles.userData = { velocities, life: 1 };
    scene.add(particles);
    particlesRef.current.push(particles);
  }, []);

  const collectCoin = useCallback((coin) => {
    const scene = sceneRef.current;
    if (!scene) return;

    createParticleExplosion(coin.position, 0xffd700, 15);
    scene.remove(coin);
    coinsRef.current = coinsRef.current.filter(c => c !== coin);

    comboRef.current += 1;
    const multiplier = multiplierActiveRef.current ? 2 : 1;
    const comboBonus = comboRef.current >= COMBO_THRESHOLD ? COMBO_MULTIPLIER : 1;
    const earned = COINS_PER_COLLECT * multiplier * comboBonus;

    scoreRef.current += earned;
    setScore(scoreRef.current);
    setCombo(comboRef.current);
  }, [createParticleExplosion]);

  const collectPowerup = useCallback((powerup) => {
    const scene = sceneRef.current;
    if (!scene) return;

    const type = powerup.userData.type;
    const color = powerup.children[0].material.color.getHex();
    createParticleExplosion(powerup.position, color, 25);
    scene.remove(powerup);
    powerupsRef.current = powerupsRef.current.filter(p => p !== powerup);

    const duration = type === 'shield' ? 4000 : type === 'speed' ? 3000 : 6000;
    activePowerupRef.current = { type, endTime: Date.now() + duration };
    setActivePowerup({ type, endTime: Date.now() + duration });

    if (type === 'shield') shieldActiveRef.current = true;
    if (type === 'magnet') magnetActiveRef.current = true;
    if (type === 'multiplier') multiplierActiveRef.current = true;
    if (type === 'speed') {
      speedBoostRef.current = true;
      setSpeedBoost(true);
    }

    // Update visuals
    if (type === 'shield' && playerRef.current) {
      playerRef.current.userData.shield.material.opacity = 0.4;
    }
    if (type === 'speed' && playerRef.current) {
      playerRef.current.userData.boost.material.opacity = 0.6;
    }

    setTimeout(() => {
      if (activePowerupRef.current?.type === type) {
        activePowerupRef.current = null;
        setActivePowerup(null);
        if (type === 'shield') {
          shieldActiveRef.current = false;
          if (playerRef.current) playerRef.current.userData.shield.material.opacity = 0;
        }
        if (type === 'magnet') magnetActiveRef.current = false;
        if (type === 'multiplier') multiplierActiveRef.current = false;
        if (type === 'speed') {
          speedBoostRef.current = false;
          setSpeedBoost(false);
          if (playerRef.current) playerRef.current.userData.boost.material.opacity = 0;
        }
      }
    }, duration);
  }, [createParticleExplosion]);

  const hitObstacle = useCallback((obstacle) => {
    const scene = sceneRef.current;
    if (!scene) return;

    if (shieldActiveRef.current) {
      shieldActiveRef.current = false;
      activePowerupRef.current = null;
      setActivePowerup(null);
      if (playerRef.current) playerRef.current.userData.shield.material.opacity = 0;
      createParticleExplosion(obstacle.position, 0x00aaff, 30);
      scene.remove(obstacle);
      obstaclesRef.current = obstaclesRef.current.filter(o => o !== obstacle);
      setHitEffect(true);
      setTimeout(() => setHitEffect(false), 300);
      return;
    }

    // Game over - player was hit without shield
    createParticleExplosion(playerRef.current.position, 0xff0000, 25);
    setHitEffect(true);

    // Enhanced screen shake
    if (cameraRef.current) {
      const originalPos = cameraRef.current.position.clone();
      let shakeTime = 0;
      const shake = () => {
        shakeTime += 16;
        if (shakeTime > 350) {
          cameraRef.current.position.copy(originalPos);
          return;
        }
        const decay = 1 - shakeTime / 350;
        cameraRef.current.position.x = originalPos.x + (Math.random() - 0.5) * 0.4 * decay;
        cameraRef.current.position.y = originalPos.y + (Math.random() - 0.5) * 0.3 * decay;
        requestAnimationFrame(shake);
      };
      shake();
    }

    // End the game after a short delay for the hit effect
    setTimeout(() => {
      setHitEffect(false);
      setGameState('ended');
      setScore(scoreRef.current);
      setDistance(Math.floor(distanceRef.current));
    }, 500);
  }, [createParticleExplosion]);

  // Game loop
  const gameLoop = useCallback(() => {
    if (gameStateRef.current !== 'playing') return;

    const delta = clockRef.current.getDelta();
    const time = clockRef.current.getElapsedTime();

    // Update speed and distance
    const baseMultiplier = speedBoostRef.current ? 1.5 : 1;
    speedRef.current = Math.min(MAX_SPEED * baseMultiplier, speedRef.current + SPEED_INCREMENT);
    distanceRef.current += speedRef.current * 60;
    setDistance(Math.floor(distanceRef.current));

    // Update zone/location based on distance for dynamic color transitions
    const distanceInZone = distanceRef.current % ZONE_TRANSITION_DISTANCE;
    const zoneIndex = Math.floor(distanceRef.current / ZONE_TRANSITION_DISTANCE) % ENVIRONMENT_ZONES.length;

    // Check if we're in a transition period (last ZONE_TRANSITION_DURATION of the zone)
    const transitionStartDistance = ZONE_TRANSITION_DISTANCE - ZONE_TRANSITION_DURATION;

    if (distanceInZone >= transitionStartDistance) {
      // We're in a transition
      const transitionProgress = (distanceInZone - transitionStartDistance) / ZONE_TRANSITION_DURATION;
      const nextZoneIndex = (zoneIndex + 1) % ENVIRONMENT_ZONES.length;

      if (currentZoneRef.current !== zoneIndex || Math.abs(zoneTransitionRef.current - transitionProgress) > 0.01) {
        currentZoneRef.current = zoneIndex;
        zoneTransitionRef.current = transitionProgress;
        updateEnvironmentColors(zoneIndex, nextZoneIndex, transitionProgress);

        // Update zone name when entering new zone
        if (transitionProgress > 0.5) {
          setCurrentZoneName(ENVIRONMENT_ZONES[nextZoneIndex].name);
        }
      }
    } else {
      // Not in transition, just maintain current zone colors
      if (currentZoneRef.current !== zoneIndex || zoneTransitionRef.current !== 0) {
        currentZoneRef.current = zoneIndex;
        zoneTransitionRef.current = 0;
        updateEnvironmentColors(zoneIndex, zoneIndex, 0);
        setCurrentZoneName(ENVIRONMENT_ZONES[zoneIndex].name);
      }
    }

    // Spawn objects
    const now = Date.now();
    const spawnSpeedFactor = speedRef.current / BASE_SPEED;
    if (now - lastSpawnTimeRef.current.obstacle > OBSTACLE_SPAWN_INTERVAL / spawnSpeedFactor) {
      spawnObstacle();
      lastSpawnTimeRef.current.obstacle = now;
    }
    if (now - lastSpawnTimeRef.current.coin > COIN_SPAWN_INTERVAL / spawnSpeedFactor) {
      spawnCoinPattern();
      lastSpawnTimeRef.current.coin = now;
    }

    // Update environment
    environmentRef.current.forEach(item => {
      if (item.type === 'sky') {
        item.mesh.material.uniforms.time.value = time;
      } else if (item.type === 'stars') {
        item.mesh.material.uniforms.time.value = time;
        item.mesh.rotation.y = time * 0.01;
      } else if (item.type === 'crystal') {
        item.mesh.position.y = item.mesh.userData.baseY + Math.sin(time * item.mesh.userData.speed) * 0.3;
        item.mesh.rotation.y += item.mesh.userData.rotSpeed;
      } else if (item.type === 'orb') {
        const scale = 1 + Math.sin(time * 3) * 0.15;
        item.mesh.scale.setScalar(scale);
      } else if (item.type === 'rune') {
        item.mesh.material.opacity = 0.4 + Math.sin(time * 2 + item.baseY) * 0.3;
      }
    });

    // Update player
    const player = playerRef.current;
    if (player) {
      // Smooth lane transition
      const targetX = (targetLaneRef.current - 1) * LANE_WIDTH;
      player.position.x += (targetX - player.position.x) * 0.18;
      currentLaneRef.current = Math.round(player.position.x / LANE_WIDTH) + 1;

      // Jump physics
      if (isJumpingRef.current) {
        jumpVelocityRef.current -= 0.018;
        playerYRef.current += jumpVelocityRef.current;
        if (playerYRef.current <= 0) {
          playerYRef.current = 0;
          isJumpingRef.current = false;
          jumpVelocityRef.current = 0;
        }
      }
      player.position.y = playerYRef.current;

      // Slide animation
      if (isSlidingRef.current) {
        player.scale.y = 0.45;
        player.position.y = -0.28;
      } else if (!isJumpingRef.current) {
        player.scale.y += (1 - player.scale.y) * 0.25;
      }

      // Running animation
      const runSpeed = speedRef.current / BASE_SPEED;
      if (playerMeshRef.current) {
        playerMeshRef.current.rotation.x = Math.sin(time * 18 * runSpeed) * 0.08;
      }
      // Arm swing
      if (player.userData.leftArm) {
        player.userData.leftArm.rotation.x = Math.sin(time * 18 * runSpeed) * 0.5;
        player.userData.rightArm.rotation.x = -Math.sin(time * 18 * runSpeed) * 0.5;
      }
      // Leg movement
      if (player.userData.leftLeg) {
        player.userData.leftLeg.rotation.x = Math.sin(time * 18 * runSpeed) * 0.4;
        player.userData.rightLeg.rotation.x = -Math.sin(time * 18 * runSpeed) * 0.4;
      }

      // Aura effect
      if (player.userData.aura) {
        player.userData.aura.rotation.z = time;
        player.userData.aura.material.opacity = 0.2 + Math.sin(time * 4) * 0.1;
      }

      // Shield pulse
      if (shieldActiveRef.current && player.userData.shield) {
        player.userData.shield.material.opacity = 0.25 + Math.sin(time * 6) * 0.15;
        player.userData.shield.scale.setScalar(1 + Math.sin(time * 4) * 0.1);
        player.userData.shield.rotation.y = time * 2;
      }

      // Speed boost visual
      if (speedBoostRef.current && player.userData.boost) {
        player.userData.boost.material.opacity = 0.4 + Math.sin(time * 10) * 0.2;
        player.userData.boost.scale.z = 1 + Math.sin(time * 8) * 0.3;
      }

      // Update trail
      if (player.userData.trail) {
        const trailPositions = player.userData.trailPositions;
        trailPositions.unshift(player.position.clone());
        if (trailPositions.length > 50) trailPositions.pop();

        const posArray = player.userData.trail.geometry.attributes.position.array;
        const opacityArray = player.userData.trail.geometry.attributes.opacity.array;
        trailPositions.forEach((pos, i) => {
          posArray[i * 3] = pos.x;
          posArray[i * 3 + 1] = pos.y + 0.5;
          posArray[i * 3 + 2] = pos.z + i * 0.1;
          opacityArray[i] = (1 - i / trailPositions.length) * 0.6;
        });
        player.userData.trail.geometry.attributes.position.needsUpdate = true;
        player.userData.trail.geometry.attributes.opacity.needsUpdate = true;
      }
    }

    // Update obstacles
    const playerLane = currentLaneRef.current;
    obstaclesRef.current.forEach(obstacle => {
      obstacle.position.z += speedRef.current;

      // Animate obstacles
      if (obstacle.userData?.type === 'fire') {
        obstacle.children.forEach((child, idx) => {
          if (idx > 0) { // Skip base
            child.rotation.y = time * 5 + (child.userData?.offset || 0);
            child.scale.setScalar(0.9 + Math.sin(time * 8 + idx) * 0.1);
          }
        });
      }
      if (obstacle.userData?.crystal) {
        obstacle.userData.crystal.rotation.y = time * 2;
        obstacle.userData.ring.scale.setScalar(1 + Math.sin(time * 3) * 0.2);
      }
      if (obstacle.userData?.beam) {
        obstacle.userData.beam.material.opacity = 0.6 + Math.sin(time * 10) * 0.4;
      }

      // Collision detection - check if obstacle is in collision zone and same lane
      if (player && obstacle.position.z > 4.5 && obstacle.position.z < 5.5) {
        // Get obstacle lane from stored userData
        const obstacleLane = obstacle.userData.lane;
        // Calculate player's current lane based on position (lane 0, 1, 2)
        const playerLaneFromPosition = Math.round(player.position.x / LANE_WIDTH) + 1;

        // Only check collision if player is in the same lane as obstacle
        if (obstacleLane === playerLaneFromPosition) {
          const canAvoid =
            (obstacle.userData.height === 'low' && playerYRef.current > 0.6) ||
            (obstacle.userData.height === 'tall' && isSlidingRef.current);

          if (!canAvoid) {
            hitObstacle(obstacle);
            sceneRef.current.remove(obstacle);
            obstaclesRef.current = obstaclesRef.current.filter(o => o !== obstacle);
          }
        }
      }

      // Remove off-screen
      if (obstacle.position.z > 12) {
        sceneRef.current.remove(obstacle);
        obstaclesRef.current = obstaclesRef.current.filter(o => o !== obstacle);
      }
    });

    // Update coins
    coinsRef.current.forEach(coin => {
      coin.position.z += speedRef.current;
      coin.children[0].rotation.z += 0.08;

      // Magnet attraction
      if (magnetActiveRef.current && player && coin.position.z > -5 && coin.position.z < 10) {
        const dir = new THREE.Vector3().subVectors(player.position, coin.position);
        dir.y = 0;
        const dist = dir.length();
        if (dist < 4) {
          dir.normalize().multiplyScalar(0.2 * (1 - dist / 4));
          coin.position.add(dir);
        }
      }

      // Collection
      if (coin.position.z > 4 && coin.position.z < 7 &&
          Math.abs(coin.position.x - player.position.x) < 0.9 &&
          Math.abs(coin.position.y - player.position.y - 0.5) < 0.8) {
        collectCoin(coin);
      }

      // Remove off-screen, reset combo
      if (coin.position.z > 12) {
        sceneRef.current.remove(coin);
        coinsRef.current = coinsRef.current.filter(c => c !== coin);
        comboRef.current = Math.max(0, comboRef.current - 1);
        setCombo(comboRef.current);
      }
    });

    // Update powerups
    powerupsRef.current.forEach(powerup => {
      powerup.position.z += speedRef.current;
      powerup.children[0].rotation.y += 0.04;
      powerup.children[0].rotation.x += 0.03;
      powerup.children[1].scale.setScalar(1 + Math.sin(time * 4) * 0.15);

      // Collection
      if (powerup.position.z > 4 && powerup.position.z < 7 &&
          powerup.userData.lane === playerLane - 1) {
        collectPowerup(powerup);
      }

      if (powerup.position.z > 12) {
        sceneRef.current.remove(powerup);
        powerupsRef.current = powerupsRef.current.filter(p => p !== powerup);
      }
    });

    // Update particles
    particlesRef.current = particlesRef.current.filter((particles) => {
      particles.userData.life -= delta * 2.5;
      if (particles.userData.life <= 0) {
        sceneRef.current.remove(particles);
        return false;
      }

      const positions = particles.geometry.attributes.position.array;
      particles.userData.velocities.forEach((vel, i) => {
        positions[i * 3] += vel.x;
        positions[i * 3 + 1] += vel.y;
        positions[i * 3 + 2] += vel.z;
        vel.y -= 0.008;
      });
      particles.geometry.attributes.position.needsUpdate = true;
      particles.material.uniforms.opacity.value = particles.userData.life;
      return true;
    });

    // Update track tiles (infinite scroll)
    trackTilesRef.current.forEach(tile => {
      tile.position.z += speedRef.current;
      if (tile.position.z > 12) {
        tile.position.z -= 70;
      }
    });

    // Animate neon lights
    if (lightsRef.current.leftNeon) {
      lightsRef.current.leftNeon.intensity = 1.2 + Math.sin(time * 4) * 0.3;
      lightsRef.current.rightNeon.intensity = 1.2 + Math.sin(time * 4 + Math.PI) * 0.3;
    }

    // Render
    rendererRef.current.render(sceneRef.current, cameraRef.current);
    animationFrameRef.current = requestAnimationFrame(gameLoop);
  }, [spawnObstacle, spawnCoinPattern, collectCoin, collectPowerup, hitObstacle, updateEnvironmentColors]);

  // Initialize
  useEffect(() => {
    const cleanup = initThreeJS();
    return () => {
      if (cleanup && typeof cleanup === 'function') {
        cleanup();
      }
    };
  }, [initThreeJS]);

  // Countdown
  useEffect(() => {
    if (gameState !== 'countdown') return;
    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
    }
  }, [countdown, gameState]);

  // Start game loop
  useEffect(() => {
    if (gameState !== 'playing') return;

    clockRef.current?.start();
    lastSpawnTimeRef.current = { obstacle: Date.now(), coin: Date.now() };
    animationFrameRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    };
  }, [gameState, gameLoop]);

  // No time limit - game ends when player is hit

  // Render idle scene
  useEffect(() => {
    if (gameState === 'countdown') {
      let idleAnimationFrame = null;
      const renderIdle = () => {
        if (gameStateRef.current !== 'countdown') return;
        if (rendererRef.current && sceneRef.current && cameraRef.current) {
          rendererRef.current.render(sceneRef.current, cameraRef.current);
        }
        idleAnimationFrame = requestAnimationFrame(renderIdle);
      };
      const timeoutId = setTimeout(() => {
        renderIdle();
      }, 100);
      return () => {
        clearTimeout(timeoutId);
        if (idleAnimationFrame) {
          cancelAnimationFrame(idleAnimationFrame);
        }
      };
    }
  }, [gameState]);

  // Controls
  const handleSwipe = useCallback((direction) => {
    if (gameStateRef.current !== 'playing') return;

    if (direction === 'left' && targetLaneRef.current > 0) {
      targetLaneRef.current -= 1;
    } else if (direction === 'right' && targetLaneRef.current < LANE_COUNT - 1) {
      targetLaneRef.current += 1;
    } else if (direction === 'up' && !isJumpingRef.current && playerYRef.current === 0) {
      isJumpingRef.current = true;
      jumpVelocityRef.current = 0.28;
    } else if (direction === 'down' && !isSlidingRef.current && !isJumpingRef.current) {
      isSlidingRef.current = true;
      setTimeout(() => {
        isSlidingRef.current = false;
      }, 550);
    }
  }, []);

  // Touch controls
  const handleTouchStart = useCallback((e) => {
    const touch = e.touches[0];
    touchStartRef.current = { x: touch.clientX, y: touch.clientY };
  }, []);

  const handleTouchEnd = useCallback((e) => {
    const touch = e.changedTouches[0];
    const deltaX = touch.clientX - touchStartRef.current.x;
    const deltaY = touch.clientY - touchStartRef.current.y;
    const threshold = 30;

    if (Math.abs(deltaX) > Math.abs(deltaY)) {
      if (deltaX > threshold) handleSwipe('right');
      else if (deltaX < -threshold) handleSwipe('left');
    } else {
      if (deltaY < -threshold) handleSwipe('up');
      else if (deltaY > threshold) handleSwipe('down');
    }
  }, [handleSwipe]);

  // Keyboard controls
  useEffect(() => {
    const handleKeyDown = (e) => {
      switch (e.key) {
        case 'ArrowLeft': case 'a': handleSwipe('left'); break;
        case 'ArrowRight': case 'd': handleSwipe('right'); break;
        case 'ArrowUp': case 'w': case ' ': handleSwipe('up'); break;
        case 'ArrowDown': case 's': handleSwipe('down'); break;
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [handleSwipe]);

  // Cleanup
  useEffect(() => {
    if (gameState === 'ended') {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    }
  }, [gameState]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="temple-run-overlay">
      <div className="temple-run-container">
        {/* Three.js Canvas Container */}
        <div
          ref={containerRef}
          className={`three-container ${hitEffect ? 'hit-shake' : ''}`}
          onTouchStart={handleTouchStart}
          onTouchEnd={handleTouchEnd}
        />

        {/* Header */}
        <div className="game-header">
          <div className="game-stat">
            <span className="stat-icon">🪙</span>
            <span className="stat-value">{score}</span>
          </div>
          <div className="game-stat distance-stat">
            <span className="stat-icon">📏</span>
            <span className="stat-value">{distance}m</span>
          </div>
        </div>

        {/* Active Powerup */}
        {activePowerup && (
          <div className={`active-powerup ${activePowerup.type}`}>
            <span className="powerup-icon">
              {activePowerup.type === 'magnet' && '🧲'}
              {activePowerup.type === 'shield' && '🛡️'}
              {activePowerup.type === 'multiplier' && '✨'}
              {activePowerup.type === 'speed' && '⚡'}
            </span>
            <span className="powerup-label">
              {activePowerup.type === 'magnet' && 'MAGNET'}
              {activePowerup.type === 'shield' && 'SHIELD'}
              {activePowerup.type === 'multiplier' && '2X COINS'}
              {activePowerup.type === 'speed' && 'SPEED BOOST'}
            </span>
          </div>
        )}

        {/* Combo Display */}
        {combo >= COMBO_THRESHOLD && (
          <div className="combo-display">
            <span className="combo-fire">🔥</span>
            <span className="combo-text">{combo}x COMBO!</span>
          </div>
        )}

        {/* Speed Boost Indicator */}
        {speedBoost && (
          <div className="speed-indicator">
            <span>⚡ SPEED BOOST ⚡</span>
          </div>
        )}

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="countdown-display" key={countdown}>
            {countdown > 0 ? countdown : 'RUN!'}
          </div>
        )}

        {/* Swipe Hints */}
        {gameState === 'playing' && (
          <div className="swipe-hints">
            <div className="swipe-hint left">◀</div>
            <div className="swipe-hint right">▶</div>
            <div className="swipe-hint up">▲</div>
            <div className="swipe-hint down">▼</div>
          </div>
        )}

        {/* Game Over Screen */}
        {gameState === 'ended' && (
          <div className="game-over-screen">
            <div className="game-over-content">
              <h2 className="game-over-title">
                <span>🏃</span> {distance >= 800 ? 'AMAZING!' : distance >= 500 ? 'Great Run!' : 'Nice Try!'} <span>🏃</span>
              </h2>

              <div className="final-stats">
                <div className="final-stat">
                  <span className="final-stat-icon">📏</span>
                  <span className="final-stat-label">Distance</span>
                  <span className="final-stat-value">{Math.floor(distance)}m</span>
                </div>

                <div className="final-stat highlight">
                  <span className="final-stat-icon">🪙</span>
                  <span className="final-stat-label">Coins Collected</span>
                  <span className="final-stat-value coins">{score}</span>
                </div>
              </div>

              {distance >= 600 && (
                <div className="achievement-badge">
                  <span>🏆</span> Speed Runner! <span>🏆</span>
                </div>
              )}

              <button
                className="claim-rewards-btn cute-button"
                onClick={handleFinish}
              >
                Claim {score} Coins! 🪙
              </button>
            </div>
          </div>
        )}

        {/* Instructions */}
        {gameState === 'countdown' && countdown === 3 && (
          <div className="game-instructions">
            <p>Swipe to move, jump & slide!</p>
            <p className="coins-hint">Collect coins, avoid obstacles! 🏃</p>
          </div>
        )}

        {/* Close button */}
        {gameState === 'countdown' && (
          <button className="game-close-btn" onClick={onClose}>
            ✕
          </button>
        )}
      </div>
    </div>
  );
};

export default TempleRunGame;
