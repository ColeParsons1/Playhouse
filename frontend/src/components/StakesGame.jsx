import React, { useState, useEffect, useRef, useCallback, useMemo } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './StakesGame.css';

// Three.js loaded dynamically
let THREE = null;
let EffectComposer = null;
let RenderPass = null;
let UnrealBloomPass = null;

// Procedural texture generators for CS:GO Dust2 style
const createSandTexture = (T) => {
  const canvas = document.createElement('canvas');
  canvas.width = 512;
  canvas.height = 512;
  const ctx = canvas.getContext('2d');

  // Base sandy tan color
  ctx.fillStyle = '#C4A574';
  ctx.fillRect(0, 0, 512, 512);

  // Add noise grain for sand effect
  for (let i = 0; i < 40000; i++) {
    const x = Math.random() * 512;
    const y = Math.random() * 512;
    const shade = Math.random() * 40 - 20;
    const r = Math.min(255, Math.max(0, 196 + shade));
    const g = Math.min(255, Math.max(0, 165 + shade));
    const b = Math.min(255, Math.max(0, 116 + shade));
    ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
    ctx.fillRect(x, y, 2, 2);
  }

  const texture = new T.CanvasTexture(canvas);
  texture.wrapS = T.RepeatWrapping;
  texture.wrapT = T.RepeatWrapping;
  texture.repeat.set(8, 8);
  return texture;
};

const createConcreteTexture = (T) => {
  const canvas = document.createElement('canvas');
  canvas.width = 256;
  canvas.height = 256;
  const ctx = canvas.getContext('2d');

  // Base concrete gray-tan
  ctx.fillStyle = '#A89888';
  ctx.fillRect(0, 0, 256, 256);

  // Add subtle variation
  for (let i = 0; i < 8000; i++) {
    const x = Math.random() * 256;
    const y = Math.random() * 256;
    const shade = Math.random() * 30 - 15;
    const r = Math.min(255, Math.max(0, 168 + shade));
    const g = Math.min(255, Math.max(0, 152 + shade));
    const b = Math.min(255, Math.max(0, 136 + shade));
    ctx.fillStyle = `rgb(${r}, ${g}, ${b})`;
    ctx.fillRect(x, y, 3, 3);
  }

  // Add cracks
  ctx.strokeStyle = '#7B6B5B';
  ctx.lineWidth = 1;
  for (let i = 0; i < 15; i++) {
    ctx.beginPath();
    ctx.moveTo(Math.random() * 256, Math.random() * 256);
    ctx.lineTo(Math.random() * 256, Math.random() * 256);
    ctx.stroke();
  }

  const texture = new T.CanvasTexture(canvas);
  texture.wrapS = T.RepeatWrapping;
  texture.wrapT = T.RepeatWrapping;
  texture.repeat.set(4, 1);
  return texture;
};

const createWoodTexture = (T) => {
  const canvas = document.createElement('canvas');
  canvas.width = 128;
  canvas.height = 128;
  const ctx = canvas.getContext('2d');

  // Base wood brown
  ctx.fillStyle = '#8B5A2B';
  ctx.fillRect(0, 0, 128, 128);

  // Wood grain lines
  ctx.strokeStyle = '#6B4423';
  ctx.lineWidth = 2;
  for (let y = 0; y < 128; y += 6) {
    ctx.beginPath();
    ctx.moveTo(0, y + Math.random() * 3);
    for (let x = 0; x < 128; x += 15) {
      ctx.lineTo(x, y + Math.random() * 3);
    }
    ctx.stroke();
  }

  // Plank border and divisions
  ctx.strokeStyle = '#4A3015';
  ctx.lineWidth = 3;
  ctx.strokeRect(2, 2, 124, 124);
  ctx.beginPath();
  ctx.moveTo(64, 0);
  ctx.lineTo(64, 128);
  ctx.moveTo(0, 64);
  ctx.lineTo(128, 64);
  ctx.stroke();

  const texture = new T.CanvasTexture(canvas);
  texture.wrapS = T.RepeatWrapping;
  texture.wrapT = T.RepeatWrapping;
  return texture;
};

// Game phases
const PHASES = {
  CONTRACT_SELECT: 'contract_select',
  COUNTDOWN: 'countdown',
  PLAYING: 'playing',
  MATCH_END: 'match_end',
};

// Contract definitions
const CONTRACTS = {
  slayer: {
    id: 'slayer',
    name: 'Slayer',
    emoji: '💀',
    description: 'Get X kills',
    getTarget: (avgKills) => Math.max(3, Math.floor(avgKills * 1.2)),
    checkSuccess: (kills, deaths, target) => kills >= target,
    getProgress: (kills) => kills,
    multiplier: 2.0,
    color: '#FF2D20',
  },
  survivor: {
    id: 'survivor',
    name: 'Survivor',
    emoji: '🛡️',
    description: 'Die less than X times',
    getTarget: (avgDeaths) => Math.max(3, Math.ceil(avgDeaths * 0.8)),
    checkSuccess: (kills, deaths, target) => deaths < target,
    getProgress: (kills, deaths, target) => Math.max(0, target - deaths),
    multiplier: 1.8,
    color: '#0088FF',
  },
  dominator: {
    id: 'dominator',
    name: 'Dominator',
    emoji: '👑',
    description: 'Achieve X+ K/D ratio',
    getTarget: (avgKD) => Math.max(1.5, Math.round((avgKD + 0.3) * 10) / 10),
    checkSuccess: (kills, deaths, target) => (deaths > 0 ? kills / deaths : kills) >= target,
    getProgress: (kills, deaths) => deaths > 0 ? Math.round((kills / deaths) * 100) / 100 : kills,
    multiplier: 2.5,
    color: '#FFD000',
  },
};

// Stake tiers
const STAKE_TIERS = [10, 25, 50, 100, 250];

// Match duration in seconds
const MATCH_DURATION = 300; // 5 minutes

// Player stats storage key
const STATS_KEY = 'stakes_player_stats';

// Spawn points for bots and player
const SPAWN_POINTS = [
  { x: -18, z: -18 },
  { x: 18, z: -18 },
  { x: -18, z: 18 },
  { x: 18, z: 18 },
  { x: 0, z: -20 },
  { x: 0, z: 20 },
  { x: -20, z: 0 },
  { x: 20, z: 0 },
];

// Bot states
const BOT_STATES = {
  PATROL: 'patrol',
  CHASE: 'chase',
  ATTACK: 'attack',
  DEAD: 'dead',
};

// Get/update player stats from localStorage
const getPlayerStats = () => {
  try {
    const stored = localStorage.getItem(STATS_KEY);
    if (stored) return JSON.parse(stored);
  } catch (e) { /* ignore */ }
  return { gamesPlayed: 0, avgKills: 5, avgDeaths: 4, avgKD: 1.25 };
};

const updatePlayerStats = (kills, deaths) => {
  const stats = getPlayerStats();
  const alpha = Math.min(0.3, 1 / (stats.gamesPlayed + 1));

  stats.avgKills = stats.avgKills * (1 - alpha) + kills * alpha;
  stats.avgDeaths = stats.avgDeaths * (1 - alpha) + deaths * alpha;
  stats.avgKD = stats.avgDeaths > 0 ? stats.avgKills / stats.avgDeaths : stats.avgKills;
  stats.gamesPlayed++;

  localStorage.setItem(STATS_KEY, JSON.stringify(stats));
};

const StakesGame = ({ onGameEnd, onClose, player }) => {
  // Phase management
  const [phase, setPhase] = useState(PHASES.CONTRACT_SELECT);
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(MATCH_DURATION);

  // Player stats
  const [health, setHealth] = useState(100);
  const [ammo, setAmmo] = useState(50);
  const [kills, setKills] = useState(0);
  const [deaths, setDeaths] = useState(0);
  const [isRespawning, setIsRespawning] = useState(false);

  // Contract state
  const [selectedContract, setSelectedContract] = useState(null);
  const [selectedStake, setSelectedStake] = useState(null);
  const [contractTarget, setContractTarget] = useState(0);
  const [contractSuccess, setContractSuccess] = useState(false);

  // UI state
  const [isLandscape, setIsLandscape] = useState(window.innerWidth > window.innerHeight);
  const [isLoading, setIsLoading] = useState(true);

  // Three.js refs
  const containerRef = useRef(null);
  const sceneRef = useRef(null);
  const cameraRef = useRef(null);
  const rendererRef = useRef(null);
  const animationRef = useRef(null);

  // Game state refs
  const playerRef = useRef({
    position: new (THREE?.Vector3 || function(){})(),
    velocity: new (THREE?.Vector3 || function(){})(),
    yaw: 0,
    pitch: 0,
    health: 100,
    canShoot: true,
    isDead: false,
  });
  const botsRef = useRef([]);
  const bulletsRef = useRef([]);
  const collidersRef = useRef([]);

  // Input refs
  const keysRef = useRef({});
  const mouseRef = useRef({ x: 0, y: 0, locked: false });
  const touchControlsRef = useRef({
    moveX: 0,
    moveY: 0,
    aimX: 0,
    aimY: 0,
    shooting: false,
  });

  // Stats refs (for animation frame access)
  const killsRef = useRef(0);
  const deathsRef = useRef(0);
  const healthRef = useRef(100);
  const ammoRef = useRef(50);
  const phaseRef = useRef(PHASES.CONTRACT_SELECT);

  // Contract targets based on player history
  const contractTargets = useMemo(() => {
    const stats = getPlayerStats();
    return {
      slayer: CONTRACTS.slayer.getTarget(stats.avgKills),
      survivor: CONTRACTS.survivor.getTarget(stats.avgDeaths),
      dominator: CONTRACTS.dominator.getTarget(stats.avgKD),
    };
  }, []);

  // Available stakes (only show what player can afford)
  const availableStakes = useMemo(() => {
    const coins = player?.coins || 0;
    return STAKE_TIERS.filter(tier => tier <= coins);
  }, [player?.coins]);

  // Sync refs with state
  useEffect(() => {
    killsRef.current = kills;
    deathsRef.current = deaths;
    healthRef.current = health;
    ammoRef.current = ammo;
    phaseRef.current = phase;
  }, [kills, deaths, health, ammo, phase]);

  // Handle orientation changes
  useEffect(() => {
    const handleResize = () => {
      setIsLandscape(window.innerWidth > window.innerHeight);
      if (cameraRef.current && rendererRef.current) {
        const width = window.innerWidth;
        const height = window.innerHeight;
        cameraRef.current.aspect = width / height;
        cameraRef.current.updateProjectionMatrix();
        rendererRef.current.setSize(width, height);
      }
    };

    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  // Initialize Three.js scene
  useEffect(() => {
    if (phase !== PHASES.PLAYING && phase !== PHASES.COUNTDOWN) return;

    let isMounted = true;

    const init = async () => {
      // Dynamic import Three.js and post-processing
      const ThreeModule = await import('three');
      THREE = ThreeModule;

      // Import post-processing
      const [{ EffectComposer: EC }, { RenderPass: RP }, { UnrealBloomPass: UBP }] = await Promise.all([
        import('three/examples/jsm/postprocessing/EffectComposer.js'),
        import('three/examples/jsm/postprocessing/RenderPass.js'),
        import('three/examples/jsm/postprocessing/UnrealBloomPass.js'),
      ]);
      EffectComposer = EC;
      RenderPass = RP;
      UnrealBloomPass = UBP;

      if (!isMounted || !containerRef.current) return;

      // Scene setup - CS:GO Dust2 style desert environment
      const scene = new THREE.Scene();
      scene.background = new THREE.Color(0x87CEEB); // Sky blue
      scene.fog = new THREE.Fog(0xD4B896, 30, 120); // Sandy haze
      sceneRef.current = scene;

      // Camera (first-person)
      const camera = new THREE.PerspectiveCamera(
        75,
        window.innerWidth / window.innerHeight,
        0.1,
        200
      );
      camera.position.set(0, 1.7, 0);
      cameraRef.current = camera;

      // High Quality Renderer
      const renderer = new THREE.WebGLRenderer({
        antialias: true,
        powerPreference: 'high-performance',
        stencil: true,
        alpha: false,
      });
      renderer.setSize(window.innerWidth, window.innerHeight);
      renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
      renderer.shadowMap.enabled = true;
      renderer.shadowMap.type = THREE.PCFSoftShadowMap;
      renderer.toneMapping = THREE.ACESFilmicToneMapping;
      renderer.toneMappingExposure = 1.2;
      renderer.outputColorSpace = THREE.SRGBColorSpace;
      containerRef.current.appendChild(renderer.domElement);
      rendererRef.current = renderer;

      // Post-processing (minimal bloom for CS:GO realism)
      const composer = new EffectComposer(renderer);
      const renderPass = new RenderPass(scene, camera);
      composer.addPass(renderPass);

      // Very subtle bloom for sun glare only
      const bloomPass = new UnrealBloomPass(
        new THREE.Vector2(window.innerWidth, window.innerHeight),
        0.1,  // Very low strength
        0.3,  // radius
        0.95  // High threshold (only brightest spots)
      );
      composer.addPass(bloomPass);

      // Store composer for animation loop
      const composerRef = { current: composer };

      // Initialize player
      playerRef.current = {
        position: new THREE.Vector3(0, 0, 0),
        velocity: new THREE.Vector3(0, 0, 0),
        yaw: 0,
        pitch: 0,
        health: 100,
        canShoot: true,
        isDead: false,
      };

      // Create arena
      createArena(scene);

      // Spawn bots
      spawnBots(scene, 4);

      // CS:GO Dust2 Style Lighting - Warm desert sun
      // Warm ambient for desert feel
      const ambientLight = new THREE.AmbientLight(0xFFF8E7, 0.5);
      scene.add(ambientLight);

      // Strong directional sun - warm yellow
      const sunLight = new THREE.DirectionalLight(0xFFE4B5, 1.4);
      sunLight.position.set(20, 40, 10);
      sunLight.castShadow = true;
      sunLight.shadow.mapSize.width = 2048;
      sunLight.shadow.mapSize.height = 2048;
      sunLight.shadow.camera.near = 1;
      sunLight.shadow.camera.far = 100;
      sunLight.shadow.camera.left = -35;
      sunLight.shadow.camera.right = 35;
      sunLight.shadow.camera.top = 35;
      sunLight.shadow.camera.bottom = -35;
      sunLight.shadow.bias = -0.001;
      scene.add(sunLight);

      // Secondary fill light (bounced light from ground)
      const fillLight = new THREE.DirectionalLight(0xD4B896, 0.3);
      fillLight.position.set(-10, 10, -10);
      scene.add(fillLight);

      // Hemisphere light for natural sky/ground color blending
      const hemiLight = new THREE.HemisphereLight(0x87CEEB, 0xC4A574, 0.4);
      scene.add(hemiLight);

      setIsLoading(false);

      // Animation loop with post-processing
      let lastTime = performance.now();
      const animate = (currentTime) => {
        if (!isMounted) return;
        animationRef.current = requestAnimationFrame(animate);

        const deltaTime = Math.min((currentTime - lastTime) / 1000, 0.1);
        lastTime = currentTime;

        if (phaseRef.current === PHASES.PLAYING) {
          updatePlayer(deltaTime);
          updateBots(deltaTime);
          updateBullets(deltaTime);
        }

        // Render with post-processing
        composerRef.current.render();
      };
      animate(performance.now());
    };

    init();

    return () => {
      isMounted = false;
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }
      if (rendererRef.current && containerRef.current) {
        containerRef.current.removeChild(rendererRef.current.domElement);
        rendererRef.current.dispose();
      }
      // Clean up bots
      botsRef.current = [];
      bulletsRef.current = [];
    };
  }, [phase === PHASES.PLAYING || phase === PHASES.COUNTDOWN]);

  // Create arena geometry - CS:GO Dust2 style
  const createArena = useCallback((scene) => {
    const T = THREE;

    // Generate textures
    const sandTexture = createSandTexture(T);
    const concreteTexture = createConcreteTexture(T);
    const woodTexture = createWoodTexture(T);

    // Sandy ground floor
    const floorGeom = new T.PlaneGeometry(50, 50);
    const floorMat = new T.MeshStandardMaterial({
      map: sandTexture,
      color: 0xC4A574,
      roughness: 0.9,
      metalness: 0.0,
    });
    const floor = new T.Mesh(floorGeom, floorMat);
    floor.rotation.x = -Math.PI / 2;
    floor.receiveShadow = true;
    scene.add(floor);

    // Concrete/adobe walls (no glowing trim)
    const wallMat = new T.MeshStandardMaterial({
      map: concreteTexture,
      color: 0xA89888,
      roughness: 0.85,
      metalness: 0.0,
    });
    const wallHeight = 5;
    const arenaSize = 25;

    const createWall = (x, z, rotY, width) => {
      const wall = new T.Mesh(
        new T.BoxGeometry(width, wallHeight, 0.8),
        wallMat
      );
      wall.position.set(x, wallHeight / 2, z);
      wall.rotation.y = rotY;
      wall.castShadow = true;
      wall.receiveShadow = true;
      scene.add(wall);

      // Add collider
      const halfW = width / 2;
      if (rotY === 0 || rotY === Math.PI) {
        collidersRef.current.push({
          minX: x - halfW, maxX: x + halfW,
          minZ: z - 0.5, maxZ: z + 0.5,
        });
      } else {
        collidersRef.current.push({
          minX: x - 0.5, maxX: x + 0.5,
          minZ: z - halfW, maxZ: z + halfW,
        });
      }
    };

    createWall(0, -arenaSize, 0, 50);
    createWall(0, arenaSize, 0, 50);
    createWall(-arenaSize, 0, Math.PI / 2, 50);
    createWall(arenaSize, 0, Math.PI / 2, 50);

    // Wooden crate cover objects (no glow)
    const crateMat = new T.MeshStandardMaterial({
      map: woodTexture,
      color: 0x8B5A2B,
      roughness: 0.8,
      metalness: 0.0,
    });

    // Metal barrel material for variety
    const barrelMat = new T.MeshStandardMaterial({
      color: 0x4A5568,
      roughness: 0.6,
      metalness: 0.4,
    });

    const covers = [
      { x: -10, z: -10, w: 3, h: 2, d: 3 },
      { x: 10, z: -10, w: 3, h: 2, d: 3 },
      { x: -10, z: 10, w: 3, h: 2, d: 3 },
      { x: 10, z: 10, w: 3, h: 2, d: 3 },
      { x: 0, z: 0, w: 4, h: 1.5, d: 4 },
      { x: -18, z: 0, w: 2, h: 2.5, d: 6 },
      { x: 18, z: 0, w: 2, h: 2.5, d: 6 },
      { x: 0, z: -15, w: 6, h: 1.8, d: 2 },
      { x: 0, z: 15, w: 6, h: 1.8, d: 2 },
    ];

    covers.forEach((c, i) => {
      // Use barrel for center piece, crates for everything else
      const cover = new T.Mesh(
        new T.BoxGeometry(c.w, c.h, c.d),
        i === 4 ? barrelMat : crateMat
      );
      cover.position.set(c.x, c.h / 2, c.z);
      cover.castShadow = true;
      cover.receiveShadow = true;
      scene.add(cover);

      // Add collider
      collidersRef.current.push({
        minX: c.x - c.w / 2, maxX: c.x + c.w / 2,
        minZ: c.z - c.d / 2, maxZ: c.z + c.d / 2,
      });
    });
  }, []);

  // Spawn AI bots - Military soldier style
  const spawnBots = useCallback((scene, count) => {
    const T = THREE;
    const bots = [];

    // Military/tactical colors (no emissive glow)
    const botColors = [
      { body: 0x4A5D4A, head: 0x3A4A3A }, // Olive drab
      { body: 0x5C5C5C, head: 0x4C4C4C }, // Gray tactical
      { body: 0x8B7355, head: 0x6B5335 }, // Desert tan
      { body: 0x556B2F, head: 0x3B4B1F }, // Dark olive
    ];

    for (let i = 0; i < count; i++) {
      const spawn = SPAWN_POINTS[i % SPAWN_POINTS.length];
      const colorScheme = botColors[i % botColors.length];

      // Body - more humanoid cylinder shape
      const bodyGeom = new T.CylinderGeometry(0.35, 0.4, 1.4, 8);
      const bodyMat = new T.MeshStandardMaterial({
        color: colorScheme.body,
        roughness: 0.8,
        metalness: 0.1,
      });
      const body = new T.Mesh(bodyGeom, bodyMat);
      body.castShadow = true;
      body.receiveShadow = true;

      // Head - helmet sphere
      const headGeom = new T.SphereGeometry(0.25, 12, 8);
      const headMat = new T.MeshStandardMaterial({
        color: colorScheme.head,
        roughness: 0.7,
        metalness: 0.2,
      });
      const head = new T.Mesh(headGeom, headMat);
      head.position.y = 1.0;
      head.castShadow = true;

      // Face visor (dark, non-emissive)
      const visorGeom = new T.BoxGeometry(0.3, 0.1, 0.1);
      const visorMat = new T.MeshStandardMaterial({
        color: 0x111111,
        roughness: 0.3,
        metalness: 0.5,
      });
      const visor = new T.Mesh(visorGeom, visorMat);
      visor.position.set(0, 0.95, 0.2);

      // Shoulder pads (tactical gear)
      const shoulderGeom = new T.BoxGeometry(0.25, 0.15, 0.2);
      const shoulderMat = new T.MeshStandardMaterial({
        color: 0x333333,
        roughness: 0.8,
        metalness: 0.3,
      });
      const leftShoulder = new T.Mesh(shoulderGeom, shoulderMat);
      leftShoulder.position.set(-0.4, 0.5, 0);
      const rightShoulder = new T.Mesh(shoulderGeom, shoulderMat);
      rightShoulder.position.set(0.4, 0.5, 0);

      // Weapon on back (visual only)
      const weaponGeom = new T.BoxGeometry(0.08, 0.7, 0.08);
      const weaponMat = new T.MeshStandardMaterial({
        color: 0x222222,
        metalness: 0.6,
        roughness: 0.4,
      });
      const weapon = new T.Mesh(weaponGeom, weaponMat);
      weapon.position.set(-0.2, 0.3, -0.25);
      weapon.rotation.z = 0.2;

      // Bot group
      const botGroup = new T.Group();
      botGroup.add(body);
      botGroup.add(head);
      botGroup.add(visor);
      botGroup.add(leftShoulder);
      botGroup.add(rightShoulder);
      botGroup.add(weapon);
      botGroup.position.set(spawn.x, 0.7, spawn.z);
      scene.add(botGroup);

      bots.push({
        mesh: botGroup,
        position: new T.Vector3(spawn.x, 0, spawn.z),
        health: 100,
        state: BOT_STATES.PATROL,
        targetWaypoint: null,
        lastShotTime: 0,
        difficulty: 0.4 + Math.random() * 0.3,
        respawnTime: 0,
        id: i,
        color: colorScheme,
      });
    }

    botsRef.current = bots;
  }, []);

  // Update player movement and shooting
  const updatePlayer = useCallback((dt) => {
    if (!THREE || !playerRef.current || playerRef.current.isDead) return;

    const player = playerRef.current;
    const keys = keysRef.current;
    const touch = touchControlsRef.current;

    // Movement
    const moveSpeed = 8;
    let moveX = 0, moveZ = 0;

    if (keys['KeyW'] || keys['ArrowUp']) moveZ -= 1;
    if (keys['KeyS'] || keys['ArrowDown']) moveZ += 1;
    if (keys['KeyA'] || keys['ArrowLeft']) moveX -= 1;
    if (keys['KeyD'] || keys['ArrowRight']) moveX += 1;

    // Touch overrides
    if (Math.abs(touch.moveX) > 0.1) moveX = touch.moveX;
    if (Math.abs(touch.moveY) > 0.1) moveZ = touch.moveY;

    if (moveX !== 0 || moveZ !== 0) {
      const moveVec = new THREE.Vector3(moveX, 0, moveZ);
      moveVec.normalize();
      moveVec.applyAxisAngle(new THREE.Vector3(0, 1, 0), player.yaw);
      moveVec.multiplyScalar(moveSpeed * dt);

      const newPos = player.position.clone().add(moveVec);

      // Collision check
      let canMove = true;
      const playerRadius = 0.4;

      // Arena bounds
      if (newPos.x < -24 || newPos.x > 24 || newPos.z < -24 || newPos.z > 24) {
        canMove = false;
      }

      // Cover collision
      for (const col of collidersRef.current) {
        if (newPos.x + playerRadius > col.minX && newPos.x - playerRadius < col.maxX &&
            newPos.z + playerRadius > col.minZ && newPos.z - playerRadius < col.maxZ) {
          canMove = false;
          break;
        }
      }

      if (canMove) {
        player.position.copy(newPos);
      }
    }

    // Aiming (touch)
    player.yaw -= touch.aimX * 3 * dt;
    player.pitch = Math.max(-Math.PI / 3, Math.min(Math.PI / 3, player.pitch - touch.aimY * 2 * dt));

    // Update camera
    if (cameraRef.current) {
      cameraRef.current.position.copy(player.position);
      cameraRef.current.position.y = 1.7;
      cameraRef.current.rotation.order = 'YXZ';
      cameraRef.current.rotation.y = player.yaw;
      cameraRef.current.rotation.x = player.pitch;
    }

    // Shooting - faster fire rate
    if ((touch.shooting || keys['Space']) && player.canShoot && ammoRef.current > 0) {
      fireWeapon();
      player.canShoot = false;
      setTimeout(() => {
        if (playerRef.current) playerRef.current.canShoot = true;
      }, 80); // Faster fire rate
    }

    // Reload
    if (keys['KeyR'] && ammoRef.current < 50) {
      setAmmo(50);
      ammoRef.current = 50;
    }
  }, []);

  // Fire weapon - High quality CS:GO style with tracer rounds
  const fireWeapon = useCallback(() => {
    if (!THREE || !cameraRef.current || !sceneRef.current) return;

    setAmmo(prev => {
      const newAmmo = prev - 1;
      ammoRef.current = newAmmo;
      return newAmmo;
    });

    const camera = cameraRef.current;
    const scene = sceneRef.current;
    const direction = new THREE.Vector3(0, 0, -1);
    direction.applyQuaternion(camera.quaternion);

    // Create bullet group for better organization
    const bulletGroup = new THREE.Group();
    const startPos = camera.position.clone();
    startPos.add(direction.clone().multiplyScalar(0.5));
    bulletGroup.position.copy(startPos);

    // Bright tracer round core (highly visible)
    const tracerGeom = new THREE.CylinderGeometry(0.02, 0.015, 1.2, 8);
    const tracerMat = new THREE.MeshBasicMaterial({
      color: 0xFFAA00,
      transparent: true,
      opacity: 0.95,
    });
    const tracer = new THREE.Mesh(tracerGeom, tracerMat);
    tracer.rotation.x = Math.PI / 2;
    tracer.position.z = -0.6;
    bulletGroup.add(tracer);

    // Tracer glow (outer layer)
    const glowGeom = new THREE.CylinderGeometry(0.04, 0.03, 1.0, 8);
    const glowMat = new THREE.MeshBasicMaterial({
      color: 0xFFDD44,
      transparent: true,
      opacity: 0.4,
    });
    const glow = new THREE.Mesh(glowGeom, glowMat);
    glow.rotation.x = Math.PI / 2;
    glow.position.z = -0.5;
    bulletGroup.add(glow);

    // Bullet tip
    const tipGeom = new THREE.SphereGeometry(0.025, 8, 8);
    const tipMat = new THREE.MeshBasicMaterial({
      color: 0xFFFFAA,
    });
    const tip = new THREE.Mesh(tipGeom, tipMat);
    tip.position.z = -1.2;
    bulletGroup.add(tip);

    // Orient bullet group to face direction
    bulletGroup.lookAt(startPos.clone().add(direction));
    scene.add(bulletGroup);

    // === MUZZLE FLASH - Multi-layered for realism ===
    const flashGroup = new THREE.Group();
    flashGroup.position.copy(startPos);

    // Core flash (bright white-yellow)
    const coreFlashGeom = new THREE.SphereGeometry(0.12, 8, 8);
    const coreFlashMat = new THREE.MeshBasicMaterial({
      color: 0xFFFFEE,
      transparent: true,
      opacity: 0.95,
    });
    const coreFlash = new THREE.Mesh(coreFlashGeom, coreFlashMat);
    flashGroup.add(coreFlash);

    // Outer flash (orange)
    const outerFlashGeom = new THREE.SphereGeometry(0.2, 8, 8);
    const outerFlashMat = new THREE.MeshBasicMaterial({
      color: 0xFF8800,
      transparent: true,
      opacity: 0.7,
    });
    const outerFlash = new THREE.Mesh(outerFlashGeom, outerFlashMat);
    flashGroup.add(outerFlash);

    // Flash spikes (star pattern)
    for (let i = 0; i < 4; i++) {
      const spikeGeom = new THREE.ConeGeometry(0.03, 0.25, 4);
      const spikeMat = new THREE.MeshBasicMaterial({
        color: 0xFFAA00,
        transparent: true,
        opacity: 0.8,
      });
      const spike = new THREE.Mesh(spikeGeom, spikeMat);
      spike.rotation.z = (i * Math.PI / 2) + Math.random() * 0.3;
      spike.rotation.x = Math.PI / 2;
      spike.position.z = -0.1;
      flashGroup.add(spike);
    }

    flashGroup.lookAt(startPos.clone().add(direction));
    scene.add(flashGroup);

    // === SHELL CASING EJECTION ===
    const casingGeom = new THREE.CylinderGeometry(0.015, 0.015, 0.06, 6);
    const casingMat = new THREE.MeshStandardMaterial({
      color: 0xDAA520,
      metalness: 0.9,
      roughness: 0.2,
    });
    const casing = new THREE.Mesh(casingGeom, casingMat);
    const casingStart = startPos.clone();
    // Eject to the right side of player view
    const rightVec = new THREE.Vector3(1, 0, 0).applyQuaternion(camera.quaternion);
    casingStart.add(rightVec.multiplyScalar(0.2));
    casing.position.copy(casingStart);
    scene.add(casing);

    // Animate shell casing
    const casingVel = new THREE.Vector3(
      (Math.random() - 0.3) * 3,
      Math.random() * 2 + 1,
      (Math.random() - 0.5) * 2
    ).applyQuaternion(camera.quaternion);
    let casingLife = 0.8;
    const animateCasing = () => {
      casingLife -= 0.016;
      if (casingLife <= 0 || casing.position.y < 0) {
        scene.remove(casing);
        casing.geometry.dispose();
        casing.material.dispose();
        return;
      }
      casingVel.y -= 12 * 0.016;
      casing.position.add(casingVel.clone().multiplyScalar(0.016));
      casing.rotation.x += 0.3;
      casing.rotation.z += 0.2;
      requestAnimationFrame(animateCasing);
    };
    animateCasing();

    // Remove muzzle flash
    setTimeout(() => {
      scene.remove(flashGroup);
      flashGroup.traverse((child) => {
        if (child.geometry) child.geometry.dispose();
        if (child.material) child.material.dispose();
      });
    }, 50);

    bulletsRef.current.push({
      mesh: bulletGroup,
      position: startPos.clone(),
      direction: direction.clone(),
      speed: 120, // Faster bullets
      damage: 25,
      fromPlayer: true,
      lifetime: 2,
    });
  }, []);

  // Update AI bots
  const updateBots = useCallback((dt) => {
    if (!THREE || !playerRef.current) return;

    const playerPos = playerRef.current.position;

    botsRef.current.forEach((bot) => {
      if (bot.state === BOT_STATES.DEAD) {
        bot.respawnTime -= dt;
        if (bot.respawnTime <= 0) {
          // Respawn
          const spawn = SPAWN_POINTS[Math.floor(Math.random() * SPAWN_POINTS.length)];
          bot.position.set(spawn.x, 0, spawn.z);
          bot.mesh.position.set(spawn.x, 0.7, spawn.z);
          bot.health = 100;
          bot.state = BOT_STATES.PATROL;
          bot.mesh.visible = true;
        }
        return;
      }

      const distToPlayer = bot.position.distanceTo(playerPos);
      const detectionRange = 25;
      const attackRange = 18;

      // State transitions
      if (distToPlayer < attackRange && !playerRef.current.isDead) {
        bot.state = BOT_STATES.ATTACK;
      } else if (distToPlayer < detectionRange && !playerRef.current.isDead) {
        bot.state = BOT_STATES.CHASE;
      } else {
        bot.state = BOT_STATES.PATROL;
      }

      // Behavior
      switch (bot.state) {
        case BOT_STATES.PATROL:
          // Random wandering
          if (!bot.targetWaypoint || bot.position.distanceTo(bot.targetWaypoint) < 2) {
            bot.targetWaypoint = new THREE.Vector3(
              (Math.random() - 0.5) * 40,
              0,
              (Math.random() - 0.5) * 40
            );
          }
          moveBot(bot, bot.targetWaypoint, dt, 3);
          break;

        case BOT_STATES.CHASE:
          moveBot(bot, playerPos, dt, 5);
          break;

        case BOT_STATES.ATTACK:
          // Face player
          const toPlayer = playerPos.clone().sub(bot.position);
          bot.mesh.rotation.y = Math.atan2(toPlayer.x, toPlayer.z);

          // Shoot
          const now = Date.now();
          const fireRate = 800 + (1 - bot.difficulty) * 600;
          if (now - bot.lastShotTime > fireRate) {
            botShoot(bot, playerPos);
            bot.lastShotTime = now;
          }
          break;
      }

      // Update mesh position
      bot.mesh.position.x = bot.position.x;
      bot.mesh.position.z = bot.position.z;
    });
  }, []);

  // Move bot toward target
  const moveBot = useCallback((bot, target, dt, speed) => {
    if (!THREE) return;

    const direction = target.clone().sub(bot.position);
    direction.y = 0;
    direction.normalize();

    const newPos = bot.position.clone().add(direction.multiplyScalar(speed * dt));

    // Simple bounds check
    if (newPos.x > -23 && newPos.x < 23 && newPos.z > -23 && newPos.z < 23) {
      bot.position.copy(newPos);
      bot.mesh.rotation.y = Math.atan2(direction.x, direction.z);
    }
  }, []);

  // Bot shooting - High quality tracer rounds
  const botShoot = useCallback((bot, targetPos) => {
    if (!THREE || !sceneRef.current) return;

    const scene = sceneRef.current;

    // Add accuracy variance based on difficulty
    const spread = (1 - bot.difficulty) * 0.2;
    const direction = targetPos.clone().sub(bot.position);
    direction.y = 0.3;
    direction.normalize();
    direction.x += (Math.random() - 0.5) * spread;
    direction.z += (Math.random() - 0.5) * spread;
    direction.normalize();

    const startPos = bot.position.clone();
    startPos.y = 1.5;

    // Create bullet group
    const bulletGroup = new THREE.Group();
    bulletGroup.position.copy(startPos);

    // Enemy tracer (red/orange tint to distinguish from player)
    const tracerGeom = new THREE.CylinderGeometry(0.018, 0.012, 0.9, 6);
    const tracerMat = new THREE.MeshBasicMaterial({
      color: 0xFF6600,
      transparent: true,
      opacity: 0.9,
    });
    const tracer = new THREE.Mesh(tracerGeom, tracerMat);
    tracer.rotation.x = Math.PI / 2;
    tracer.position.z = -0.45;
    bulletGroup.add(tracer);

    // Tracer glow
    const glowGeom = new THREE.CylinderGeometry(0.035, 0.025, 0.7, 6);
    const glowMat = new THREE.MeshBasicMaterial({
      color: 0xFF8844,
      transparent: true,
      opacity: 0.35,
    });
    const glow = new THREE.Mesh(glowGeom, glowMat);
    glow.rotation.x = Math.PI / 2;
    glow.position.z = -0.35;
    bulletGroup.add(glow);

    // Bullet tip
    const tipGeom = new THREE.SphereGeometry(0.02, 6, 6);
    const tipMat = new THREE.MeshBasicMaterial({ color: 0xFFCC88 });
    const tip = new THREE.Mesh(tipGeom, tipMat);
    tip.position.z = -0.9;
    bulletGroup.add(tip);

    bulletGroup.lookAt(startPos.clone().add(direction));
    scene.add(bulletGroup);

    // Muzzle flash for bot
    const flashGroup = new THREE.Group();
    flashGroup.position.copy(startPos);
    flashGroup.position.add(direction.clone().multiplyScalar(0.3));

    const coreFlash = new THREE.Mesh(
      new THREE.SphereGeometry(0.1, 6, 6),
      new THREE.MeshBasicMaterial({ color: 0xFFEEDD, transparent: true, opacity: 0.9 })
    );
    flashGroup.add(coreFlash);

    const outerFlash = new THREE.Mesh(
      new THREE.SphereGeometry(0.15, 6, 6),
      new THREE.MeshBasicMaterial({ color: 0xFF6600, transparent: true, opacity: 0.6 })
    );
    flashGroup.add(outerFlash);

    scene.add(flashGroup);

    setTimeout(() => {
      scene.remove(flashGroup);
      flashGroup.traverse((child) => {
        if (child.geometry) child.geometry.dispose();
        if (child.material) child.material.dispose();
      });
    }, 40);

    bulletsRef.current.push({
      mesh: bulletGroup,
      position: startPos.clone(),
      direction: direction,
      speed: 70, // Faster
      damage: 15 + Math.floor(Math.random() * 10),
      fromPlayer: false,
      ownerId: bot.id,
      lifetime: 2,
    });
  }, []);

  // Create high-quality impact effects (sparks, debris, dust cloud)
  const createHitSparks = useCallback((position, color, count = 12) => {
    if (!THREE || !sceneRef.current) return;

    const scene = sceneRef.current;

    // === BRIGHT SPARKS (metal/ricochet effect) ===
    for (let i = 0; i < Math.floor(count / 2); i++) {
      const sparkGeom = new THREE.SphereGeometry(0.03, 4, 4);
      const sparkMat = new THREE.MeshBasicMaterial({
        color: 0xFFDD44,
        transparent: true,
        opacity: 1.0,
      });
      const spark = new THREE.Mesh(sparkGeom, sparkMat);
      spark.position.copy(position);
      scene.add(spark);

      const velocity = new THREE.Vector3(
        (Math.random() - 0.5) * 8,
        Math.random() * 6 + 2,
        (Math.random() - 0.5) * 8
      );

      let life = 0.3 + Math.random() * 0.2;
      const animateSpark = () => {
        life -= 0.016;
        if (life <= 0) {
          scene.remove(spark);
          spark.geometry.dispose();
          spark.material.dispose();
          return;
        }
        velocity.y -= 20 * 0.016;
        spark.position.add(velocity.clone().multiplyScalar(0.016));
        spark.scale.setScalar(life * 2);
        spark.material.opacity = life * 2;
        requestAnimationFrame(animateSpark);
      };
      animateSpark();
    }

    // === DEBRIS CHUNKS ===
    for (let i = 0; i < Math.floor(count / 3); i++) {
      const debrisGeom = new THREE.BoxGeometry(0.04, 0.04, 0.04);
      const debrisMat = new THREE.MeshStandardMaterial({
        color: 0x8B7355,
        roughness: 0.9,
      });
      const debris = new THREE.Mesh(debrisGeom, debrisMat);
      debris.position.copy(position);
      debris.rotation.set(Math.random() * Math.PI, Math.random() * Math.PI, Math.random() * Math.PI);
      scene.add(debris);

      const velocity = new THREE.Vector3(
        (Math.random() - 0.5) * 5,
        Math.random() * 4 + 1,
        (Math.random() - 0.5) * 5
      );

      let life = 0.5;
      const animateDebris = () => {
        life -= 0.016;
        if (life <= 0 || debris.position.y < 0) {
          scene.remove(debris);
          debris.geometry.dispose();
          debris.material.dispose();
          return;
        }
        velocity.y -= 15 * 0.016;
        debris.position.add(velocity.clone().multiplyScalar(0.016));
        debris.rotation.x += 0.2;
        debris.rotation.z += 0.15;
        requestAnimationFrame(animateDebris);
      };
      animateDebris();
    }

    // === DUST CLOUD ===
    for (let i = 0; i < count; i++) {
      const dustGeom = new THREE.SphereGeometry(0.08 + Math.random() * 0.05, 6, 6);
      const dustMat = new THREE.MeshBasicMaterial({
        color: 0xC4A574,
        transparent: true,
        opacity: 0.5,
      });
      const dust = new THREE.Mesh(dustGeom, dustMat);
      dust.position.copy(position);
      dust.position.add(new THREE.Vector3(
        (Math.random() - 0.5) * 0.3,
        Math.random() * 0.2,
        (Math.random() - 0.5) * 0.3
      ));
      scene.add(dust);

      const velocity = new THREE.Vector3(
        (Math.random() - 0.5) * 2,
        Math.random() * 1.5 + 0.5,
        (Math.random() - 0.5) * 2
      );

      let life = 0.6;
      const animateDust = () => {
        life -= 0.012;
        if (life <= 0) {
          scene.remove(dust);
          dust.geometry.dispose();
          dust.material.dispose();
          return;
        }
        velocity.y -= 2 * 0.016;
        dust.position.add(velocity.clone().multiplyScalar(0.016));
        dust.scale.setScalar(1 + (0.6 - life) * 3);
        dust.material.opacity = life * 0.8;
        requestAnimationFrame(animateDust);
      };
      animateDust();
    }
  }, []);

  // Update bullets
  const updateBullets = useCallback((dt) => {
    if (!THREE || !sceneRef.current) return;

    const bulletsToRemove = [];

    bulletsRef.current.forEach((bullet, index) => {
      // Move bullet
      const movement = bullet.direction.clone().multiplyScalar(bullet.speed * dt);
      bullet.position.add(movement);
      bullet.mesh.position.copy(bullet.position);
      bullet.lifetime -= dt;

      // Check lifetime
      if (bullet.lifetime <= 0) {
        bulletsToRemove.push(index);
        return;
      }

      // Check bounds
      if (Math.abs(bullet.position.x) > 30 || Math.abs(bullet.position.z) > 30) {
        bulletsToRemove.push(index);
        return;
      }

      if (bullet.fromPlayer) {
        // Check hit on bots
        botsRef.current.forEach((bot) => {
          if (bot.state === BOT_STATES.DEAD) return;

          const dist = bullet.position.distanceTo(new THREE.Vector3(bot.position.x, 1, bot.position.z));
          if (dist < 1) {
            bot.health -= bullet.damage;
            bulletsToRemove.push(index);

            // Create hit sparks
            createHitSparks(bullet.position.clone(), 0x00ffff);

            if (bot.health <= 0) {
              bot.state = BOT_STATES.DEAD;
              bot.mesh.visible = false;
              bot.respawnTime = 3;

              // Create death explosion
              createHitSparks(bot.position.clone(), 0xff6600, 15);

              // Increment kills
              setKills(prev => {
                killsRef.current = prev + 1;
                return prev + 1;
              });
            }
          }
        });
      } else {
        // Check hit on player
        if (!playerRef.current.isDead) {
          const playerPos = playerRef.current.position.clone();
          playerPos.y = 1;
          const dist = bullet.position.distanceTo(playerPos);

          if (dist < 0.8) {
            bulletsToRemove.push(index);

            // Create hit sparks
            createHitSparks(bullet.position.clone(), 0xff2d20);

            setHealth(prev => {
              const newHealth = Math.max(0, prev - bullet.damage);
              healthRef.current = newHealth;

              if (newHealth <= 0) {
                handlePlayerDeath();
              }

              return newHealth;
            });
          }
        }
      }
    });

    // Remove bullets
    bulletsToRemove.sort((a, b) => b - a).forEach((index) => {
      const bullet = bulletsRef.current[index];
      if (bullet && bullet.mesh) {
        sceneRef.current.remove(bullet.mesh);
        // Handle both Group and Mesh types
        bullet.mesh.traverse((child) => {
          if (child.geometry) child.geometry.dispose();
          if (child.material) child.material.dispose();
        });
      }
      bulletsRef.current.splice(index, 1);
    });
  }, []);

  // Handle player death
  const handlePlayerDeath = useCallback(() => {
    if (!playerRef.current || playerRef.current.isDead) return;

    playerRef.current.isDead = true;
    setIsRespawning(true);
    setDeaths(prev => {
      deathsRef.current = prev + 1;
      return prev + 1;
    });

    // Respawn after 2 seconds
    setTimeout(() => {
      if (!playerRef.current) return;

      // Find spawn point furthest from bots
      let bestSpawn = SPAWN_POINTS[0];
      let bestDist = 0;

      SPAWN_POINTS.forEach(spawn => {
        let minDist = Infinity;
        botsRef.current.forEach(bot => {
          const dist = Math.sqrt(
            Math.pow(spawn.x - bot.position.x, 2) +
            Math.pow(spawn.z - bot.position.z, 2)
          );
          minDist = Math.min(minDist, dist);
        });
        if (minDist > bestDist) {
          bestDist = minDist;
          bestSpawn = spawn;
        }
      });

      playerRef.current.position.set(bestSpawn.x, 0, bestSpawn.z);
      playerRef.current.health = 100;
      playerRef.current.isDead = false;
      setHealth(100);
      healthRef.current = 100;
      setAmmo(50);
      ammoRef.current = 50;
      setIsRespawning(false);
    }, 2000);
  }, []);

  // Keyboard handlers
  useEffect(() => {
    const handleKeyDown = (e) => {
      keysRef.current[e.code] = true;
      if (['Space', 'ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight'].includes(e.code)) {
        e.preventDefault();
      }
    };

    const handleKeyUp = (e) => {
      keysRef.current[e.code] = false;
    };

    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);

    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);
    };
  }, []);

  // Mouse look (pointer lock)
  useEffect(() => {
    if (phase !== PHASES.PLAYING) return;

    const handleMouseMove = (e) => {
      if (!document.pointerLockElement || !playerRef.current) return;

      playerRef.current.yaw -= e.movementX * 0.002;
      playerRef.current.pitch = Math.max(
        -Math.PI / 3,
        Math.min(Math.PI / 3, playerRef.current.pitch - e.movementY * 0.002)
      );
    };

    const handleClick = () => {
      if (containerRef.current && !document.pointerLockElement) {
        containerRef.current.requestPointerLock?.();
      } else if (playerRef.current?.canShoot && ammoRef.current > 0) {
        fireWeapon();
        playerRef.current.canShoot = false;
        setTimeout(() => {
          if (playerRef.current) playerRef.current.canShoot = true;
        }, 150);
      }
    };

    document.addEventListener('mousemove', handleMouseMove);
    document.addEventListener('click', handleClick);

    return () => {
      document.removeEventListener('mousemove', handleMouseMove);
      document.removeEventListener('click', handleClick);
      document.exitPointerLock?.();
    };
  }, [phase, fireWeapon]);

  // Match timer
  useEffect(() => {
    if (phase !== PHASES.PLAYING) return;

    const interval = setInterval(() => {
      setTimeLeft(prev => {
        if (prev <= 1) {
          clearInterval(interval);
          endMatch();
          return 0;
        }
        return prev - 1;
      });
    }, 1000);

    return () => clearInterval(interval);
  }, [phase]);

  // Countdown timer
  useEffect(() => {
    if (phase !== PHASES.COUNTDOWN) return;

    const interval = setInterval(() => {
      setCountdown(prev => {
        if (prev <= 1) {
          clearInterval(interval);
          setPhase(PHASES.PLAYING);
          return 0;
        }
        return prev - 1;
      });
    }, 1000);

    return () => clearInterval(interval);
  }, [phase]);

  // Start match after contract selection
  const startMatch = useCallback(() => {
    if (!selectedContract || !selectedStake) return;

    // Set contract target
    setContractTarget(contractTargets[selectedContract.id]);

    // Reset game state
    setKills(0);
    setDeaths(0);
    setHealth(100);
    setAmmo(50);
    setTimeLeft(300); // Reset match timer
    killsRef.current = 0;
    deathsRef.current = 0;
    healthRef.current = 100;
    ammoRef.current = 50;

    // Clear any existing bullets from previous game
    bulletsRef.current = [];

    setPhase(PHASES.COUNTDOWN);
    setCountdown(3);
  }, [selectedContract, selectedStake, contractTargets]);

  // End match
  const endMatch = useCallback(() => {
    setPhase(PHASES.MATCH_END);

    // Check contract success
    const contract = selectedContract;
    const success = contract.checkSuccess(killsRef.current, deathsRef.current, contractTarget);
    setContractSuccess(success);

    // Update stats
    updatePlayerStats(killsRef.current, deathsRef.current);

    // Exit pointer lock
    document.exitPointerLock?.();
  }, [selectedContract, contractTarget]);

  // Collect reward and exit
  const collectReward = useCallback(() => {
    let earnedCoins;

    if (contractSuccess) {
      // Win: return stake + profit
      earnedCoins = Math.floor(selectedStake * selectedContract.multiplier);
    } else {
      // Lose: stake was pre-deducted, report as negative to deduct
      earnedCoins = -selectedStake;
    }

    onGameEnd(earnedCoins);
  }, [contractSuccess, selectedStake, selectedContract, onGameEnd]);

  // Touch control handlers
  const handleMoveStart = useCallback((e) => {
    e.preventDefault();
    const touch = e.touches[0];
    touchControlsRef.current.moveStartX = touch.clientX;
    touchControlsRef.current.moveStartY = touch.clientY;
  }, []);

  const handleMoveChange = useCallback((e) => {
    e.preventDefault();
    const touch = e.touches[0];
    const rect = e.currentTarget.getBoundingClientRect();
    const maxOffset = rect.width / 3;

    const deltaX = touch.clientX - touchControlsRef.current.moveStartX;
    const deltaY = touch.clientY - touchControlsRef.current.moveStartY;

    touchControlsRef.current.moveX = Math.max(-1, Math.min(1, deltaX / maxOffset));
    touchControlsRef.current.moveY = Math.max(-1, Math.min(1, deltaY / maxOffset));
  }, []);

  const handleMoveEnd = useCallback(() => {
    touchControlsRef.current.moveX = 0;
    touchControlsRef.current.moveY = 0;
  }, []);

  const handleAimStart = useCallback((e) => {
    e.preventDefault();
    const touch = e.touches[0];
    touchControlsRef.current.aimStartX = touch.clientX;
    touchControlsRef.current.aimStartY = touch.clientY;
  }, []);

  const handleAimChange = useCallback((e) => {
    e.preventDefault();
    const touch = e.touches[0];

    const deltaX = touch.clientX - touchControlsRef.current.aimStartX;
    const deltaY = touch.clientY - touchControlsRef.current.aimStartY;

    touchControlsRef.current.aimX = deltaX * 0.01;
    touchControlsRef.current.aimY = deltaY * 0.01;
    touchControlsRef.current.aimStartX = touch.clientX;
    touchControlsRef.current.aimStartY = touch.clientY;
  }, []);

  const handleAimEnd = useCallback(() => {
    touchControlsRef.current.aimX = 0;
    touchControlsRef.current.aimY = 0;
  }, []);

  // Calculate current contract progress for display
  const currentProgress = useMemo(() => {
    if (!selectedContract) return 0;
    return selectedContract.getProgress(kills, deaths, contractTarget);
  }, [selectedContract, kills, deaths, contractTarget]);

  // Render contract selection
  const renderContractSelect = () => (
    <motion.div
      className="stakes-overlay contract-select"
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
    >
      <button className="stakes-close-btn" onClick={onClose}>X</button>

      <div className="contract-select-container">
        <h1 className="stakes-title">
          <span>🎯</span> STAKES <span>🎯</span>
        </h1>
        <p className="stakes-subtitle">Bet on your performance!</p>

        <div className="your-coins">
          <span>🪙</span> {player?.coins || 0} coins
        </div>

        <h2 className="section-title">Choose Contract</h2>
        <div className="contracts-grid">
          {Object.values(CONTRACTS).map(contract => (
            <motion.button
              key={contract.id}
              className={`contract-card ${selectedContract?.id === contract.id ? 'selected' : ''}`}
              style={{ '--contract-color': contract.color }}
              onClick={() => setSelectedContract(contract)}
              whileHover={{ scale: 1.03 }}
              whileTap={{ scale: 0.97 }}
            >
              <span className="contract-emoji">{contract.emoji}</span>
              <span className="contract-name">{contract.name}</span>
              <span className="contract-desc">{contract.description.replace('X', contractTargets[contract.id])}</span>
              <span className="contract-target">Target: {contractTargets[contract.id]}{contract.id === 'dominator' ? ' K/D' : ''}</span>
              <span className="contract-mult">{contract.multiplier}x Reward</span>
            </motion.button>
          ))}
        </div>

        {selectedContract && (
          <>
            <h2 className="section-title">Select Stake</h2>
            <div className="stakes-grid">
              {availableStakes.length > 0 ? (
                availableStakes.map(stake => (
                  <motion.button
                    key={stake}
                    className={`stake-btn ${selectedStake === stake ? 'selected' : ''}`}
                    onClick={() => setSelectedStake(stake)}
                    whileHover={{ scale: 1.05 }}
                    whileTap={{ scale: 0.95 }}
                  >
                    <span className="stake-amount">🪙 {stake}</span>
                    <span className="stake-reward">Win: {Math.floor(stake * selectedContract.multiplier)}</span>
                  </motion.button>
                ))
              ) : (
                <p className="no-stakes">Not enough coins to stake!</p>
              )}
            </div>
          </>
        )}

        {selectedContract && selectedStake && (
          <motion.button
            className="start-match-btn"
            onClick={startMatch}
            initial={{ scale: 0 }}
            animate={{ scale: 1 }}
            whileHover={{ scale: 1.05 }}
            whileTap={{ scale: 0.95 }}
          >
            Start Match!
          </motion.button>
        )}
      </div>
    </motion.div>
  );

  // Render countdown
  const renderCountdown = () => (
    <div className="stakes-game-container" ref={containerRef}>
      {isLoading && (
        <div className="stakes-loading">
          <div className="loading-spinner"></div>
          <p>Loading arena...</p>
        </div>
      )}

      <motion.div
        className="countdown-overlay"
        initial={{ opacity: 0 }}
        animate={{ opacity: 1 }}
      >
        <motion.div
          className="countdown-number"
          key={countdown}
          initial={{ scale: 2, opacity: 0 }}
          animate={{ scale: 1, opacity: 1 }}
          exit={{ scale: 0, opacity: 0 }}
        >
          {countdown}
        </motion.div>
        <p className="countdown-contract">
          {selectedContract?.emoji} {selectedContract?.name}: {contractTargets[selectedContract?.id]}
        </p>
      </motion.div>
    </div>
  );

  // Render game HUD
  const renderGameHUD = () => (
    <>
      <div className="stakes-hud">
        {/* Top bar */}
        <div className="hud-top">
          <div className="contract-tracker" style={{ '--contract-color': selectedContract?.color }}>
            <span className="contract-emoji">{selectedContract?.emoji}</span>
            <div className="progress-bar">
              <div
                className="progress-fill"
                style={{
                  width: `${Math.min(100, (currentProgress / contractTarget) * 100)}%`,
                  backgroundColor: selectedContract?.color
                }}
              />
            </div>
            <span className="progress-text">
              {selectedContract?.id === 'dominator'
                ? `${currentProgress.toFixed(1)} / ${contractTarget}`
                : `${currentProgress} / ${contractTarget}`
              }
            </span>
          </div>

          <div className="timer">
            <span className="timer-icon">⏱️</span>
            <span className="timer-value">
              {Math.floor(timeLeft / 60)}:{String(timeLeft % 60).padStart(2, '0')}
            </span>
          </div>
        </div>

        {/* Bottom left */}
        <div className="hud-bottom-left">
          <div className="health-display">
            <span className="health-icon">❤️</span>
            <div className="health-bar">
              <div className="health-fill" style={{ width: `${health}%` }} />
            </div>
            <span className="health-value">{health}</span>
          </div>
          <div className="ammo-display">
            <span className="ammo-icon">🔫</span>
            <span className="ammo-value">{ammo}/50</span>
          </div>
        </div>

        {/* Bottom right */}
        <div className="hud-bottom-right">
          <div className="kd-display">
            <span className="kills">💀 {kills}</span>
            <span className="separator">/</span>
            <span className="deaths">☠️ {deaths}</span>
          </div>
        </div>

        {/* Crosshair */}
        <div className="crosshair">
          <div className="crosshair-h"></div>
          <div className="crosshair-v"></div>
        </div>
      </div>

      {/* Respawn overlay */}
      {isRespawning && (
        <div className="respawn-overlay">
          <p>Respawning...</p>
        </div>
      )}

      {/* Touch controls */}
      {isLandscape && (
        <div className="touch-controls">
          <div
            className="joystick-area left"
            onTouchStart={handleMoveStart}
            onTouchMove={handleMoveChange}
            onTouchEnd={handleMoveEnd}
          >
            <div className="joystick-base">
              <div className="joystick-label">MOVE</div>
            </div>
          </div>

          <div
            className="joystick-area right"
            onTouchStart={handleAimStart}
            onTouchMove={handleAimChange}
            onTouchEnd={handleAimEnd}
          >
            <div className="joystick-base">
              <div className="joystick-label">AIM</div>
            </div>
          </div>

          <button
            className="fire-button"
            onTouchStart={() => { touchControlsRef.current.shooting = true; }}
            onTouchEnd={() => { touchControlsRef.current.shooting = false; }}
          >
            🔥
          </button>

          <button
            className="reload-button"
            onTouchStart={() => { setAmmo(50); ammoRef.current = 50; }}
          >
            🔄
          </button>
        </div>
      )}
    </>
  );

  // Render match end
  const renderMatchEnd = () => (
    <motion.div
      className="stakes-overlay match-end"
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
    >
      <motion.div
        className={`match-end-container ${contractSuccess ? 'success' : 'failed'}`}
        initial={{ scale: 0.5, opacity: 0 }}
        animate={{ scale: 1, opacity: 1 }}
      >
        <h1 className="result-title">
          {contractSuccess ? '🏆 CONTRACT COMPLETE!' : '❌ CONTRACT FAILED'}
        </h1>

        <div className="contract-summary">
          <span className="contract-emoji large">{selectedContract?.emoji}</span>
          <span className="contract-name">{selectedContract?.name}</span>
        </div>

        <div className="stats-grid">
          <div className="stat-row">
            <span className="stat-label">Kills</span>
            <span className="stat-value">💀 {kills}</span>
          </div>
          <div className="stat-row">
            <span className="stat-label">Deaths</span>
            <span className="stat-value">☠️ {deaths}</span>
          </div>
          <div className="stat-row">
            <span className="stat-label">K/D Ratio</span>
            <span className="stat-value">{(deaths > 0 ? kills / deaths : kills).toFixed(2)}</span>
          </div>
          <div className="stat-row">
            <span className="stat-label">Target</span>
            <span className="stat-value">{contractTarget}{selectedContract?.id === 'dominator' ? ' K/D' : ''}</span>
          </div>
        </div>

        <div className={`reward-section ${contractSuccess ? 'success' : 'failed'}`}>
          {contractSuccess ? (
            <motion.div
              className="reward-display"
              initial={{ scale: 0 }}
              animate={{ scale: 1 }}
              transition={{ delay: 0.3, type: 'spring' }}
            >
              <span className="reward-label">REWARD</span>
              <span className="reward-value">🪙 +{Math.floor(selectedStake * selectedContract.multiplier)}</span>
            </motion.div>
          ) : (
            <motion.div
              className="reward-display"
              initial={{ scale: 0 }}
              animate={{ scale: 1 }}
              transition={{ delay: 0.3 }}
            >
              <span className="reward-label">STAKE LOST</span>
              <span className="reward-value lost">🪙 -{selectedStake}</span>
            </motion.div>
          )}
        </div>

        <motion.button
          className="collect-btn"
          onClick={collectReward}
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          transition={{ delay: 0.6 }}
          whileHover={{ scale: 1.05 }}
          whileTap={{ scale: 0.95 }}
        >
          {contractSuccess ? 'Collect Reward!' : 'Try Again'}
        </motion.button>
      </motion.div>
    </motion.div>
  );

  // Landscape requirement overlay
  if (!isLandscape && phase !== PHASES.CONTRACT_SELECT) {
    return (
      <div className="stakes-overlay rotate-device">
        <div className="rotate-content">
          <span className="rotate-icon">📱</span>
          <p>Please rotate your device to landscape mode</p>
        </div>
      </div>
    );
  }

  return (
    <div className="stakes-game">
      {phase === PHASES.CONTRACT_SELECT && renderContractSelect()}

      {(phase === PHASES.COUNTDOWN || phase === PHASES.PLAYING) && (
        <div className="stakes-game-container" ref={containerRef}>
          {/* Loading spinner (shows first, behind countdown) */}
          {isLoading && (
            <div className="stakes-loading">
              <div className="loading-spinner"></div>
              <p>Loading arena...</p>
            </div>
          )}

          {/* Countdown overlay (shows on top of loading if needed) */}
          {phase === PHASES.COUNTDOWN && (
            <motion.div
              className="countdown-overlay"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
            >
              <motion.div
                className="countdown-number"
                key={countdown}
                initial={{ scale: 2, opacity: 0 }}
                animate={{ scale: 1, opacity: 1 }}
              >
                {countdown}
              </motion.div>
              {isLoading && <p style={{ marginTop: '20px', color: '#aaa' }}>Loading arena...</p>}
            </motion.div>
          )}

          {phase === PHASES.PLAYING && renderGameHUD()}
        </div>
      )}

      {phase === PHASES.MATCH_END && renderMatchEnd()}
    </div>
  );
};

export default StakesGame;
