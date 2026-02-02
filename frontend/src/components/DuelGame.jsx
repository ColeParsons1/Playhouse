import React, { useState, useEffect, useRef, useCallback, useMemo } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './DuelGame.css';

// Three.js loaded dynamically
let THREE = null;

// Game phases
const PHASES = {
  CLASS_SELECT: 'class_select',
  STAKE_SELECT: 'stake_select',
  LOADOUT: 'loadout',
  COUNTDOWN: 'countdown',
  FIGHTING: 'fighting',
  VICTORY: 'victory',
  DEFEAT: 'defeat',
};

// Character classes with unique stats and bonuses - Playhouse themed!
const CLASSES = {
  action_hero: {
    id: 'action_hero',
    name: 'Action Figure',
    emoji: '🦸',
    description: 'Plastic power! Strong melee attacks',
    color: '#FF2D20',
    hp: 99,
    prayer: 70,
    special: 100,
    damageBonus: 1.15,
    defenseBonus: 1.0,
    speedBonus: 1.0,
    weapons: ['foam_sword', 'pool_noodle', 'toy_hammer', 'pillow'],
    food: 12,
    specialty: 'Bonk damage +15%',
  },
  nerf_kid: {
    id: 'nerf_kid',
    name: 'Nerf Master',
    emoji: '🔫',
    description: 'Pew pew! Rapid foam darts',
    color: '#FF6B00',
    hp: 90,
    prayer: 80,
    special: 100,
    damageBonus: 1.0,
    defenseBonus: 1.0,
    speedBonus: 0.8,
    weapons: ['nerf_blaster', 'water_gun', 'pool_noodle'],
    food: 10,
    specialty: 'Attack speed +20%',
  },
  wizard_plush: {
    id: 'wizard_plush',
    name: 'Wizard Plushie',
    emoji: '🧸',
    description: 'Magical stuffing! Drains energy',
    color: '#9B30FF',
    hp: 85,
    prayer: 99,
    special: 100,
    damageBonus: 1.1,
    defenseBonus: 0.9,
    speedBonus: 1.0,
    weapons: ['magic_wand', 'glitter_staff', 'pool_noodle'],
    food: 10,
    specialty: 'Magic +10%, 2x energy drain',
  },
  teddy_tank: {
    id: 'teddy_tank',
    name: 'Teddy Tank',
    emoji: '🧸',
    description: 'Extra fluffy! Super cuddly defense',
    color: '#0088FF',
    hp: 115,
    prayer: 99,
    special: 100,
    damageBonus: 0.9,
    defenseBonus: 1.3,
    speedBonus: 1.1,
    weapons: ['pillow', 'pool_noodle', 'toy_hammer'],
    food: 16,
    specialty: 'Defense +30%, HP 115',
  },
  speed_racer: {
    id: 'speed_racer',
    name: 'Speed Racer',
    emoji: '🏎️',
    description: 'Zoom zoom! Fast but fragile',
    color: '#FFD000',
    hp: 75,
    prayer: 52,
    special: 100,
    damageBonus: 1.25,
    defenseBonus: 0.7,
    speedBonus: 0.9,
    weapons: ['foam_sword', 'pool_noodle', 'nerf_blaster', 'magic_wand'],
    food: 8,
    specialty: 'All damage +25%, fragile',
  },
};

// Combat styles - Playhouse battle types!
const COMBAT_STYLES = {
  MELEE: 'bonk',      // Close range bonking
  RANGED: 'pewpew',   // Foam darts & water
  MAGIC: 'sparkle',   // Glitter & magic
};

// Weapons database - Playhouse toy weapons!
const WEAPONS = {
  // Bonk (Melee)
  foam_sword: {
    id: 'foam_sword',
    name: 'Foam Sword',
    style: COMBAT_STYLES.MELEE,
    attack: 132,
    strength: 132,
    speed: 6,
    specCost: 50,
    specEffect: 'ags_spec',
    color: 0x44aaff,
    emoji: '🗡️',
  },
  pillow: {
    id: 'pillow',
    name: 'Fluffy Pillow',
    style: COMBAT_STYLES.MELEE,
    attack: 82,
    strength: 82,
    speed: 4,
    specCost: 0,
    specEffect: null,
    color: 0xffccff,
    emoji: '🛏️',
  },
  pool_noodle: {
    id: 'pool_noodle',
    name: 'Pool Noodle',
    style: COMBAT_STYLES.MELEE,
    attack: 40,
    strength: 40,
    speed: 4,
    specCost: 25,
    specEffect: 'double_hit_boosted',
    color: 0x00ff88,
    emoji: '🏊',
  },
  toy_hammer: {
    id: 'toy_hammer',
    name: 'Squeaky Hammer',
    style: COMBAT_STYLES.MELEE,
    attack: 81,
    strength: 79,
    speed: 7,
    specCost: 50,
    specEffect: 'instant_hit',
    color: 0xffcc00,
    emoji: '🔨',
  },
  // Pew Pew (Ranged)
  nerf_blaster: {
    id: 'nerf_blaster',
    name: 'Nerf Blaster',
    style: COMBAT_STYLES.RANGED,
    attack: 90,
    strength: 100,
    speed: 5,
    specCost: 0,
    specEffect: null,
    color: 0xff6600,
    emoji: '🔫',
  },
  water_gun: {
    id: 'water_gun',
    name: 'Super Soaker',
    style: COMBAT_STYLES.RANGED,
    attack: 95,
    strength: 95,
    speed: 8,
    specCost: 55,
    specEffect: 'double_arrow',
    color: 0x00ccff,
    emoji: '💦',
  },
  // Magic
  magic_wand: {
    id: 'magic_wand',
    name: 'Magic Wand',
    style: COMBAT_STYLES.MAGIC,
    attack: 65,
    strength: 15,
    speed: 5,
    specCost: 0,
    specEffect: null,
    color: 0xff66ff,
    emoji: '🪄',
  },
  glitter_staff: {
    id: 'glitter_staff',
    name: 'Glitter Staff',
    style: COMBAT_STYLES.MAGIC,
    attack: 85,
    strength: 25,
    speed: 5,
    specCost: 55,
    specEffect: 'volatile_spec',
    color: 0xff00ff,
    emoji: '✨',
  },
};

// Food items - Playhouse snacks!
const FOOD = {
  cookie: { id: 'cookie', name: 'Cookie', heal: 20, emoji: '🍪' },
  pizza: { id: 'pizza', name: 'Pizza Slice', heal: 22, emoji: '🍕' },
  juice: { id: 'juice', name: 'Juice Box', heal: 16, emoji: '🧃' },
  gummy: { id: 'gummy', name: 'Gummy Bears', heal: 18, emoji: '🐻', combo: true },
};

// Contract types - Playhouse challenges!
const CONTRACTS = {
  champion: {
    id: 'champion',
    name: 'Champion',
    emoji: '🏆',
    description: 'Win the battle!',
    checkSuccess: (playerHP, enemyHP, stats) => enemyHP <= 0,
    multiplier: 2.0,
    color: '#FF2D20',
  },
  showoff: {
    id: 'showoff',
    name: 'Show-Off',
    emoji: '😎',
    description: 'Win with 50%+ HP',
    checkSuccess: (playerHP, enemyHP, stats) => enemyHP <= 0 && playerHP >= (stats.playerMaxHP || 99) * 0.5,
    multiplier: 2.5,
    color: '#FFD000',
  },
  no_snacks: {
    id: 'no_snacks',
    name: 'No Snacks',
    emoji: '🚫',
    description: 'Win without eating',
    checkSuccess: (playerHP, enemyHP, stats) => enemyHP <= 0 && stats.foodEaten === 0,
    multiplier: 3.0,
    color: '#00D45F',
  },
  tire_out: {
    id: 'tire_out',
    name: 'Tire Out',
    emoji: '😴',
    description: 'Drain enemy energy to 0',
    checkSuccess: (playerHP, enemyHP, stats) => enemyHP <= 0 && stats.enemyPrayerDrained,
    multiplier: 2.2,
    color: '#0088FF',
  },
};

const STAKE_TIERS = [10, 25, 50, 100, 250];
const TICK_DURATION = 600; // ms per game tick
const MAX_HP = 99;
const MAX_PRAYER = 99;
const MAX_SPECIAL = 100;

// AI Difficulty levels - rewardMult affects stake payout!
const AI_DIFFICULTY = {
  EASY: { prayerAccuracy: 0.5, reactSpeed: 4, eatThreshold: 25, specUseChance: 0.15, weaponSwitchChance: 0.02, rewardMult: 0.5, name: 'Easy' },
  MEDIUM: { prayerAccuracy: 0.7, reactSpeed: 3, eatThreshold: 35, specUseChance: 0.25, weaponSwitchChance: 0.03, rewardMult: 1.0, name: 'Medium' },
  HARD: { prayerAccuracy: 0.85, reactSpeed: 2, eatThreshold: 45, specUseChance: 0.4, weaponSwitchChance: 0.05, rewardMult: 1.5, name: 'Hard' },
  EXPERT: { prayerAccuracy: 0.95, reactSpeed: 1, eatThreshold: 50, specUseChance: 0.5, weaponSwitchChance: 0.08, rewardMult: 2.5, name: 'Expert' },
};

// Critical hit configuration
const CRIT_CHANCE = 0.12; // 12% base crit chance
const CRIT_MULTIPLIER = 1.5; // 50% extra damage on crit

const DuelGame = ({ onGameEnd, onClose, player }) => {
  // Phase management
  const [phase, setPhase] = useState(PHASES.CLASS_SELECT);
  const [countdown, setCountdown] = useState(3);

  // Class and contract state
  const [selectedClass, setSelectedClass] = useState(null);
  const [enemyClass, setEnemyClass] = useState(null);
  const [selectedContract, setSelectedContract] = useState(null);
  const [selectedStake, setSelectedStake] = useState(null);

  // Player state
  const [playerHP, setPlayerHP] = useState(MAX_HP);
  const [playerPrayer, setPlayerPrayer] = useState(MAX_PRAYER);
  const [playerSpecial, setPlayerSpecial] = useState(MAX_SPECIAL);
  const [playerStyle, setPlayerStyle] = useState(COMBAT_STYLES.MELEE);
  const [playerWeapon, setPlayerWeapon] = useState(WEAPONS.foam_sword);
  const [playerPrayers, setPlayerPrayers] = useState({
    melee: false,
    ranged: false,
    magic: false,
  });
  const [playerInventory, setPlayerInventory] = useState([
    FOOD.cookie, FOOD.cookie, FOOD.cookie, FOOD.cookie,
    FOOD.cookie, FOOD.cookie, FOOD.cookie, FOOD.cookie,
    FOOD.pizza, FOOD.pizza, FOOD.pizza, FOOD.pizza,
    FOOD.gummy, FOOD.gummy, FOOD.gummy, FOOD.gummy,
    WEAPONS.foam_sword, WEAPONS.pool_noodle, WEAPONS.nerf_blaster, WEAPONS.magic_wand,
    null, null, null, null,
    null, null, null, null,
  ]);

  // Enemy state
  const [enemyHP, setEnemyHP] = useState(MAX_HP);
  const [enemyPrayer, setEnemyPrayer] = useState(MAX_PRAYER);
  const [enemySpecial, setEnemySpecial] = useState(MAX_SPECIAL);
  const [enemyStyle, setEnemyStyle] = useState(COMBAT_STYLES.MELEE);
  const [enemyWeapon, setEnemyWeapon] = useState(WEAPONS.foam_sword);
  const [enemyPrayers, setEnemyPrayers] = useState({
    melee: false,
    ranged: false,
    magic: false,
  });

  // Combat state
  const [inCombat, setInCombat] = useState(false);
  const [playerAttackTick, setPlayerAttackTick] = useState(0);
  const [enemyAttackTick, setEnemyAttackTick] = useState(0);
  const [hitSplats, setHitSplats] = useState([]);
  const [specEnabled, setSpecEnabled] = useState(false);
  const [canAttack, setCanAttack] = useState(true);
  const [hudMinimized, setHudMinimized] = useState(false);

  // Stats tracking
  const [stats, setStats] = useState({
    foodEaten: 0,
    damageDealt: 0,
    damageTaken: 0,
    enemyPrayerDrained: false,
  });

  // Combo system
  const [playerCombo, setPlayerCombo] = useState(0);
  const [enemyCombo, setEnemyCombo] = useState(0);
  const [lastCrit, setLastCrit] = useState(null); // { target: 'player'|'enemy', damage: number }

  // AI learning - tracks player patterns
  const [aiDifficulty, setAiDifficulty] = useState(AI_DIFFICULTY.MEDIUM);
  const playerPatternRef = useRef({
    styleHistory: [], // Last 10 attack styles used
    weaponSwitchFrequency: 0,
    avgTimeBetweenEats: 0,
    preferredStyle: null,
    lastEatTick: 0,
    eatCount: 0,
  });

  // Screen shake state
  const [screenShake, setScreenShake] = useState(0);

  // Three.js refs
  const containerRef = useRef(null);
  const sceneRef = useRef(null);
  const cameraRef = useRef(null);
  const rendererRef = useRef(null);
  const animationRef = useRef(null);
  const playerModelRef = useRef(null);
  const enemyModelRef = useRef(null);
  const raycasterRef = useRef(null);
  const mouseRef = useRef({ x: 0, y: 0 });
  const groundPlaneRef = useRef(null);
  const clickIndicatorRef = useRef(null);

  // Movement refs - start in attack range (dist=2 which is < 2.5)
  const playerTargetRef = useRef({ x: -1, z: 0 });
  const playerPosRef = useRef({ x: -1, z: 0 });
  const enemyTargetRef = useRef({ x: 1, z: 0 });
  const enemyPosRef = useRef({ x: 1, z: 0 });
  const isAttackingRef = useRef(false);
  const playerMovingRef = useRef(false);

  // Tick system ref
  const tickRef = useRef(0);
  const lastTickTimeRef = useRef(0);

  // Combat refs for tick access
  const playerHPRef = useRef(MAX_HP);
  const enemyHPRef = useRef(MAX_HP);
  const playerPrayerRef = useRef(MAX_PRAYER);
  const enemyPrayerRef = useRef(MAX_PRAYER);
  const playerSpecialRef = useRef(MAX_SPECIAL);
  const enemySpecialRef = useRef(MAX_SPECIAL);
  const playerStyleRef = useRef(COMBAT_STYLES.MELEE);
  const enemyStyleRef = useRef(COMBAT_STYLES.MELEE);
  const playerWeaponRef = useRef(WEAPONS.foam_sword);
  const enemyWeaponRef = useRef(WEAPONS.foam_sword);
  const playerPrayersRef = useRef({ melee: false, ranged: false, magic: false });
  const enemyPrayersRef = useRef({ melee: false, ranged: false, magic: false });
  const specEnabledRef = useRef(false);
  const playerAttackTickRef = useRef(0);
  const enemyAttackTickRef = useRef(0);
  const phaseRef = useRef(PHASES.CLASS_SELECT);
  const playerClassRef = useRef(null);
  const enemyClassRef = useRef(null);

  // Sync refs
  useEffect(() => {
    playerHPRef.current = playerHP;
    enemyHPRef.current = enemyHP;
    playerPrayerRef.current = playerPrayer;
    enemyPrayerRef.current = enemyPrayer;
    playerSpecialRef.current = playerSpecial;
    enemySpecialRef.current = enemySpecial;
    playerStyleRef.current = playerStyle;
    enemyStyleRef.current = enemyStyle;
    playerWeaponRef.current = playerWeapon;
    enemyWeaponRef.current = enemyWeapon;
    playerPrayersRef.current = playerPrayers;
    enemyPrayersRef.current = enemyPrayers;
    specEnabledRef.current = specEnabled;
    playerAttackTickRef.current = playerAttackTick;
    enemyAttackTickRef.current = enemyAttackTick;
    phaseRef.current = phase;
  }, [playerHP, enemyHP, playerPrayer, enemyPrayer, playerSpecial, enemySpecial,
      playerStyle, enemyStyle, playerWeapon, enemyWeapon, playerPrayers, enemyPrayers,
      specEnabled, playerAttackTick, enemyAttackTick, phase]);

  // Available stakes
  const availableStakes = useMemo(() => {
    const coins = player?.coins || 0;
    return STAKE_TIERS.filter(tier => tier <= coins);
  }, [player?.coins]);

  // Handler refs for cleanup
  const handleClickRef = useRef(null);
  const handleResizeRef = useRef(null);

  // Initialize Three.js scene
  useEffect(() => {
    if (phase !== PHASES.FIGHTING && phase !== PHASES.COUNTDOWN &&
        phase !== PHASES.VICTORY && phase !== PHASES.DEFEAT) return;

    let isMounted = true;

    const init = async () => {
      const ThreeModule = await import('three');
      THREE = ThreeModule;

      if (!isMounted || !containerRef.current) return;

      // Scene
      const scene = new THREE.Scene();
      scene.background = new THREE.Color(0x87ceeb); // Sky blue
      sceneRef.current = scene;

      // Camera (isometric-ish view)
      const camera = new THREE.PerspectiveCamera(
        50,
        window.innerWidth / window.innerHeight,
        0.1,
        1000
      );
      camera.position.set(0, 8, 12);
      camera.lookAt(0, 1, 0);
      cameraRef.current = camera;

      // Renderer
      const renderer = new THREE.WebGLRenderer({ antialias: true });
      renderer.setSize(window.innerWidth, window.innerHeight);
      renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
      renderer.shadowMap.enabled = true;
      containerRef.current.appendChild(renderer.domElement);
      rendererRef.current = renderer;

      // Create arena
      createArena(scene);

      // Create characters at their ref positions
      playerModelRef.current = createCharacter(scene, playerPosRef.current.x, 0x3366ff, 'player');
      playerModelRef.current.position.z = playerPosRef.current.z;

      enemyModelRef.current = createCharacter(scene, enemyPosRef.current.x, 0xff3333, 'enemy');
      enemyModelRef.current.position.z = enemyPosRef.current.z;


      // Raycaster for click detection
      raycasterRef.current = new THREE.Raycaster();

      // Invisible ground plane for raycasting
      const groundPlane = new THREE.Mesh(
        new THREE.PlaneGeometry(20, 20),
        new THREE.MeshBasicMaterial({ visible: false })
      );
      groundPlane.rotation.x = -Math.PI / 2;
      groundPlane.position.y = 0.01;
      groundPlane.name = 'ground';
      scene.add(groundPlane);
      groundPlaneRef.current = groundPlane;

      // Click indicator (yellow X marker)
      const indicatorGroup = new THREE.Group();
      const indicatorMat = new THREE.MeshBasicMaterial({ color: 0xffff00 });
      const bar1 = new THREE.Mesh(new THREE.BoxGeometry(0.6, 0.05, 0.1), indicatorMat);
      bar1.rotation.y = Math.PI / 4;
      const bar2 = new THREE.Mesh(new THREE.BoxGeometry(0.6, 0.05, 0.1), indicatorMat);
      bar2.rotation.y = -Math.PI / 4;
      indicatorGroup.add(bar1, bar2);
      indicatorGroup.position.y = 0.05;
      indicatorGroup.visible = false;
      scene.add(indicatorGroup);
      clickIndicatorRef.current = indicatorGroup;

      // Click/touch handler
      const handleClick = (event) => {
        if (phaseRef.current !== PHASES.FIGHTING) return;

        // Handle touch events
        let clientX, clientY;
        if (event.type === 'touchstart' && event.touches && event.touches.length > 0) {
          event.preventDefault();
          clientX = event.touches[0].clientX;
          clientY = event.touches[0].clientY;
        } else {
          clientX = event.clientX;
          clientY = event.clientY;
        }

        const rect = renderer.domElement.getBoundingClientRect();
        mouseRef.current.x = ((clientX - rect.left) / rect.width) * 2 - 1;
        mouseRef.current.y = -((clientY - rect.top) / rect.height) * 2 + 1;

        raycasterRef.current.setFromCamera(mouseRef.current, camera);

        // Check if clicked on enemy (use hitbox for easier clicking)
        if (enemyModelRef.current && enemyModelRef.current.hitbox) {
          // Update hitbox world position
          enemyModelRef.current.hitbox.updateMatrixWorld();
          const enemyIntersects = raycasterRef.current.intersectObject(enemyModelRef.current.hitbox);

          if (enemyIntersects.length > 0) {
            isAttackingRef.current = true;
            playerMovingRef.current = true;

            // Show red attack indicator on enemy
            if (clickIndicatorRef.current) {
              clickIndicatorRef.current.children.forEach(c => c.material.color.setHex(0xff0000));
              clickIndicatorRef.current.position.x = enemyPosRef.current.x;
              clickIndicatorRef.current.position.z = enemyPosRef.current.z;
              clickIndicatorRef.current.visible = true;
              clickIndicatorRef.current.scale.set(1.5, 1, 1.5);

              setTimeout(() => {
                if (clickIndicatorRef.current) {
                  clickIndicatorRef.current.children.forEach(c => c.material.color.setHex(0xffff00));
                  clickIndicatorRef.current.visible = false;
                }
              }, 300);
            }
            return;
          }
        }

        // Otherwise, click on ground to move
        const groundIntersects = raycasterRef.current.intersectObject(groundPlane);
        if (groundIntersects.length > 0) {
          const point = groundIntersects[0].point;

          // Check if click is near enemy (within 1.5 units = clicking on enemy)
          const distToEnemy = Math.sqrt(
            Math.pow(point.x - enemyPosRef.current.x, 2) +
            Math.pow(point.z - enemyPosRef.current.z, 2)
          );

          if (distToEnemy < 1.5) {
            isAttackingRef.current = true;
            playerMovingRef.current = true;

            // Show red attack indicator
            if (clickIndicatorRef.current) {
              clickIndicatorRef.current.children.forEach(c => c.material.color.setHex(0xff0000));
              clickIndicatorRef.current.position.x = enemyPosRef.current.x;
              clickIndicatorRef.current.position.z = enemyPosRef.current.z;
              clickIndicatorRef.current.visible = true;
              clickIndicatorRef.current.scale.set(1.5, 1, 1.5);
              setTimeout(() => {
                if (clickIndicatorRef.current) {
                  clickIndicatorRef.current.children.forEach(c => c.material.color.setHex(0xffff00));
                  clickIndicatorRef.current.visible = false;
                }
              }, 300);
            }
            return;
          }

          // Clamp to arena bounds
          const dist = Math.sqrt(point.x * point.x + point.z * point.z);
          if (dist < 7) {
            playerTargetRef.current = { x: point.x, z: point.z };
            playerMovingRef.current = true;
            isAttackingRef.current = false;

            // Show click indicator
            if (clickIndicatorRef.current) {
              clickIndicatorRef.current.position.x = point.x;
              clickIndicatorRef.current.position.z = point.z;
              clickIndicatorRef.current.visible = true;
              clickIndicatorRef.current.scale.set(1, 1, 1);

              // Fade out indicator
              let fade = 1;
              const fadeInterval = setInterval(() => {
                fade -= 0.05;
                if (fade <= 0) {
                  clearInterval(fadeInterval);
                  if (clickIndicatorRef.current) {
                    clickIndicatorRef.current.visible = false;
                  }
                } else if (clickIndicatorRef.current) {
                  clickIndicatorRef.current.scale.set(fade, 1, fade);
                }
              }, 30);
            }
          }
        }
      };

      // Store handler ref for cleanup
      handleClickRef.current = handleClick;

      renderer.domElement.addEventListener('click', handleClick);
      renderer.domElement.addEventListener('touchstart', handleClick, { passive: false });
      renderer.domElement.style.cursor = 'pointer';

      // Handle window resize for mobile orientation changes
      const handleResize = () => {
        if (!isMounted || !containerRef.current) return;
        const width = containerRef.current.clientWidth || window.innerWidth;
        const height = containerRef.current.clientHeight || window.innerHeight;
        camera.aspect = width / height;
        camera.updateProjectionMatrix();
        renderer.setSize(width, height);
      };
      handleResizeRef.current = handleResize;

      window.addEventListener('resize', handleResize);
      window.addEventListener('orientationchange', handleResize);

      // Lighting
      const ambientLight = new THREE.AmbientLight(0xffffff, 0.6);
      scene.add(ambientLight);

      const sunLight = new THREE.DirectionalLight(0xffffee, 1);
      sunLight.position.set(10, 20, 10);
      sunLight.castShadow = true;
      sunLight.shadow.mapSize.width = 1024;
      sunLight.shadow.mapSize.height = 1024;
      scene.add(sunLight);

      // Animation loop
      const animate = (currentTime) => {
        if (!isMounted) return;
        animationRef.current = requestAnimationFrame(animate);

        // Process ticks
        if (phaseRef.current === PHASES.FIGHTING) {
          if (currentTime - lastTickTimeRef.current >= TICK_DURATION) {
            processTick();
            lastTickTimeRef.current = currentTime;
            tickRef.current++;
          }

          // Update movement
          updateMovement();
        }

        // Animate characters
        animateCharacters(currentTime);

        renderer.render(scene, camera);
      };
      animate(performance.now());
    };

    init();

    return () => {
      isMounted = false;
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }
      if (handleResizeRef.current) {
        window.removeEventListener('resize', handleResizeRef.current);
        window.removeEventListener('orientationchange', handleResizeRef.current);
      }
      if (rendererRef.current && containerRef.current) {
        if (handleClickRef.current) {
          rendererRef.current.domElement.removeEventListener('click', handleClickRef.current);
          rendererRef.current.domElement.removeEventListener('touchstart', handleClickRef.current);
        }
        containerRef.current.removeChild(rendererRef.current.domElement);
        rendererRef.current.dispose();
      }
    };
  }, [phase === PHASES.FIGHTING || phase === PHASES.COUNTDOWN ||
      phase === PHASES.VICTORY || phase === PHASES.DEFEAT]);

  // Create arena
  const createArena = useCallback((scene) => {
    const T = THREE;

    // Ground (sand)
    const groundGeom = new T.CircleGeometry(8, 32);
    const groundMat = new T.MeshStandardMaterial({
      color: 0xd4a574,
      roughness: 0.9,
    });
    const ground = new T.Mesh(groundGeom, groundMat);
    ground.rotation.x = -Math.PI / 2;
    ground.receiveShadow = true;
    scene.add(ground);

    // Arena walls (stone)
    const wallMat = new T.MeshStandardMaterial({
      color: 0x888888,
      roughness: 0.8,
    });

    for (let i = 0; i < 16; i++) {
      const angle = (i / 16) * Math.PI * 2;
      const wall = new T.Mesh(
        new T.BoxGeometry(1.2, 2, 0.5),
        wallMat
      );
      wall.position.set(
        Math.cos(angle) * 8.5,
        1,
        Math.sin(angle) * 8.5
      );
      wall.rotation.y = -angle + Math.PI / 2;
      wall.castShadow = true;
      scene.add(wall);
    }

    // Pillars at corners
    const pillarMat = new T.MeshStandardMaterial({ color: 0x666666 });
    for (let i = 0; i < 4; i++) {
      const angle = (i / 4) * Math.PI * 2 + Math.PI / 4;
      const pillar = new T.Mesh(
        new T.BoxGeometry(1, 3.5, 1),
        pillarMat
      );
      pillar.position.set(
        Math.cos(angle) * 8.5,
        1.75,
        Math.sin(angle) * 8.5
      );
      pillar.castShadow = true;
      scene.add(pillar);
    }
  }, []);

  // Create blocky OSRS-style character
  const createCharacter = useCallback((scene, xPos, armorColor, id) => {
    const T = THREE;
    const group = new T.Group();

    // Head
    const headGeom = new T.BoxGeometry(0.7, 0.7, 0.7);
    const skinMat = new T.MeshStandardMaterial({ color: 0xffdbac });
    const head = new T.Mesh(headGeom, skinMat);
    head.position.y = 2.15;
    head.castShadow = true;
    group.add(head);

    // Helmet
    const helmetGeom = new T.BoxGeometry(0.75, 0.4, 0.75);
    const helmetMat = new T.MeshStandardMaterial({ color: armorColor });
    const helmet = new T.Mesh(helmetGeom, helmetMat);
    helmet.position.y = 2.5;
    helmet.castShadow = true;
    group.add(helmet);

    // Body
    const bodyGeom = new T.BoxGeometry(0.9, 1.1, 0.5);
    const armorMat = new T.MeshStandardMaterial({ color: armorColor });
    const body = new T.Mesh(bodyGeom, armorMat);
    body.position.y = 1.35;
    body.castShadow = true;
    group.add(body);

    // Arms
    const armGeom = new T.BoxGeometry(0.25, 0.9, 0.25);
    const leftArm = new T.Mesh(armGeom, skinMat);
    leftArm.position.set(-0.575, 1.35, 0);
    leftArm.castShadow = true;
    group.add(leftArm);

    const rightArm = new T.Mesh(armGeom, skinMat);
    rightArm.position.set(0.575, 1.35, 0);
    rightArm.castShadow = true;
    group.add(rightArm);
    group.rightArm = rightArm;

    // Legs
    const legGeom = new T.BoxGeometry(0.3, 0.8, 0.3);
    const legMat = new T.MeshStandardMaterial({ color: 0x333366 });
    const leftLeg = new T.Mesh(legGeom, legMat);
    leftLeg.position.set(-0.2, 0.4, 0);
    leftLeg.castShadow = true;
    group.add(leftLeg);
    group.leftLeg = leftLeg;

    const rightLeg = new T.Mesh(legGeom, legMat);
    rightLeg.position.set(0.2, 0.4, 0);
    rightLeg.castShadow = true;
    group.add(rightLeg);
    group.rightLeg = rightLeg;

    // Weapon (sword)
    const weaponGeom = new T.BoxGeometry(0.1, 1.2, 0.1);
    const weaponMat = new T.MeshStandardMaterial({ color: 0xcccccc });
    const weapon = new T.Mesh(weaponGeom, weaponMat);
    weapon.position.set(0.7, 1.2, 0);
    weapon.rotation.z = -0.3;
    weapon.castShadow = true;
    group.add(weapon);
    group.weapon = weapon;

    // Cape
    const capeGeom = new T.BoxGeometry(0.8, 1.0, 0.1);
    const capeMat = new T.MeshStandardMaterial({ color: id === 'player' ? 0x0066cc : 0xcc0000 });
    const cape = new T.Mesh(capeGeom, capeMat);
    cape.position.set(0, 1.3, -0.3);
    cape.castShadow = true;
    group.add(cape);

    group.position.x = xPos;
    // Face each other: player faces right (+X), enemy faces left (-X)
    group.rotation.y = id === 'player' ? -Math.PI / 2 : Math.PI / 2;
    group.userData = { id, bobOffset: Math.random() * Math.PI * 2 };

    // Add invisible hitbox for easier clicking
    const hitbox = new T.Mesh(
      new T.BoxGeometry(1.5, 3, 1.5),
      new T.MeshBasicMaterial({ visible: false })
    );
    hitbox.position.y = 1.5;
    hitbox.name = 'hitbox';
    hitbox.userData = { id };
    group.add(hitbox);
    group.hitbox = hitbox;

    scene.add(group);
    return group;
  }, []);

  // Update 3D weapon model appearance
  const updateWeaponModel = useCallback((characterModel, weapon) => {
    if (!characterModel?.weapon || !THREE) return;

    const weaponMesh = characterModel.weapon;

    // Update color based on weapon
    weaponMesh.material.color.setHex(weapon.color);

    // Update shape based on weapon type
    if (weapon.style === COMBAT_STYLES.MELEE) {
      // Sword shape
      weaponMesh.scale.set(1, 1.2, 1);
      weaponMesh.rotation.z = -0.3;
    } else if (weapon.style === COMBAT_STYLES.RANGED) {
      // Bow/crossbow shape
      weaponMesh.scale.set(0.8, 1.5, 0.3);
      weaponMesh.rotation.z = -0.5;
    } else {
      // Staff shape
      weaponMesh.scale.set(0.6, 1.8, 0.6);
      weaponMesh.rotation.z = -0.1;
    }
  }, []);

  // Update weapon models when weapons change
  useEffect(() => {
    if (playerModelRef.current) {
      updateWeaponModel(playerModelRef.current, playerWeapon);
    }
  }, [playerWeapon, updateWeaponModel]);

  useEffect(() => {
    if (enemyModelRef.current) {
      updateWeaponModel(enemyModelRef.current, enemyWeapon);
    }
  }, [enemyWeapon, updateWeaponModel]);

  // Animate characters (idle bob + walking)
  const animateCharacters = useCallback((time) => {
    if (playerModelRef.current) {
      const bob = Math.sin(time * 0.003 + playerModelRef.current.userData.bobOffset) * 0.05;
      playerModelRef.current.position.y = bob;

      // Walking leg animation
      if (playerMovingRef.current || isAttackingRef.current) {
        const walkCycle = Math.sin(time * 0.015) * 0.5;
        if (playerModelRef.current.leftLeg) {
          playerModelRef.current.leftLeg.rotation.x = walkCycle;
        }
        if (playerModelRef.current.rightLeg) {
          playerModelRef.current.rightLeg.rotation.x = -walkCycle;
        }
      }
    }
    if (enemyModelRef.current) {
      const bob = Math.sin(time * 0.003 + enemyModelRef.current.userData.bobOffset) * 0.05;
      enemyModelRef.current.position.y = bob;

      // Enemy walking animation
      const walkCycle = Math.sin(time * 0.012) * 0.4;
      if (enemyModelRef.current.leftLeg) {
        enemyModelRef.current.leftLeg.rotation.x = walkCycle;
      }
      if (enemyModelRef.current.rightLeg) {
        enemyModelRef.current.rightLeg.rotation.x = -walkCycle;
      }
    }
  }, []);

  // Update movement each frame
  const updateMovement = useCallback(() => {
    const MOVE_SPEED = 0.08;
    const ATTACK_RANGE = 2.0;
    const ENEMY_SPEED = 0.04;

    // If attacking, target the enemy
    if (isAttackingRef.current && enemyModelRef.current) {
      playerTargetRef.current = {
        x: enemyPosRef.current.x,
        z: enemyPosRef.current.z,
      };
    }

    // Move player toward target
    if (playerModelRef.current) {
      const dx = playerTargetRef.current.x - playerPosRef.current.x;
      const dz = playerTargetRef.current.z - playerPosRef.current.z;
      const dist = Math.sqrt(dx * dx + dz * dz);

      // Check if in attack range
      const enemyDx = enemyPosRef.current.x - playerPosRef.current.x;
      const enemyDz = enemyPosRef.current.z - playerPosRef.current.z;
      const enemyDist = Math.sqrt(enemyDx * enemyDx + enemyDz * enemyDz);

      if (isAttackingRef.current && enemyDist <= ATTACK_RANGE) {
        // In range - stop moving and fight
        playerMovingRef.current = false;
        // Face enemy
        playerModelRef.current.rotation.y = Math.atan2(-enemyDx, -enemyDz);
      } else if (dist > 0.1) {
        // Move toward target
        const moveX = (dx / dist) * MOVE_SPEED;
        const moveZ = (dz / dist) * MOVE_SPEED;
        playerPosRef.current.x += moveX;
        playerPosRef.current.z += moveZ;
        playerModelRef.current.position.x = playerPosRef.current.x;
        playerModelRef.current.position.z = playerPosRef.current.z;

        // Face movement direction
        playerModelRef.current.rotation.y = Math.atan2(-dx, -dz);
      } else {
        playerMovingRef.current = false;
      }
    }

    // Enemy AI - chase player
    if (enemyModelRef.current && playerModelRef.current) {
      const dx = playerPosRef.current.x - enemyPosRef.current.x;
      const dz = playerPosRef.current.z - enemyPosRef.current.z;
      const dist = Math.sqrt(dx * dx + dz * dz);

      if (dist > ATTACK_RANGE) {
        // Move toward player
        const moveX = (dx / dist) * ENEMY_SPEED;
        const moveZ = (dz / dist) * ENEMY_SPEED;
        enemyPosRef.current.x += moveX;
        enemyPosRef.current.z += moveZ;
        enemyModelRef.current.position.x = enemyPosRef.current.x;
        enemyModelRef.current.position.z = enemyPosRef.current.z;
      }

      // Face player
      enemyModelRef.current.rotation.y = Math.atan2(-dx, -dz);

      // Start combat when in range - auto-engage
      if (dist <= ATTACK_RANGE) {
        if (!inCombat) {
          setInCombat(true);
        }
        // Auto-target enemy when in range
        if (!playerMovingRef.current) {
          isAttackingRef.current = true;
        }
      }
    }
  }, [inCombat]);

  // Process game tick
  const processTick = useCallback(() => {
    if (enemyHPRef.current <= 0 || playerHPRef.current <= 0) return;

    const currentTick = tickRef.current;

    // Check if in attack range
    const dx = enemyPosRef.current.x - playerPosRef.current.x;
    const dz = enemyPosRef.current.z - playerPosRef.current.z;
    const dist = Math.sqrt(dx * dx + dz * dz);
    const inRange = dist <= 2.5;

    // Update canAttack state for UI
    const attackReady = currentTick >= playerAttackTickRef.current;
    setCanAttack(attackReady);

    // SIMPLIFIED: Both characters auto-attack when in range
    // Player auto-attacks
    if (inRange && attackReady) {
      performPlayerAttack();
      // Apply class speed bonus (lower = faster)
      const playerSpeedMod = playerClassRef.current?.speedBonus || 1.0;
      const nextPlayerTick = currentTick + Math.ceil(playerWeaponRef.current.speed * playerSpeedMod);
      playerAttackTickRef.current = nextPlayerTick;
      setPlayerAttackTick(nextPlayerTick);
      setCanAttack(false);
    }

    // Enemy attack (always attacks when in range)
    if (inRange && currentTick >= enemyAttackTickRef.current) {
      performEnemyAttack();
      // Apply class speed bonus (lower = faster)
      const enemySpeedMod = enemyClassRef.current?.speedBonus || 1.0;
      const nextEnemyTick = currentTick + Math.ceil(enemyWeaponRef.current.speed * enemySpeedMod);
      enemyAttackTickRef.current = nextEnemyTick; // Update ref directly!
      setEnemyAttackTick(nextEnemyTick);
    }

    // Prayer drain
    if (playerPrayersRef.current.melee || playerPrayersRef.current.ranged || playerPrayersRef.current.magic) {
      setPlayerPrayer(prev => Math.max(0, prev - 1));
    }
    if (enemyPrayersRef.current.melee || enemyPrayersRef.current.ranged || enemyPrayersRef.current.magic) {
      setEnemyPrayer(prev => Math.max(0, prev - 1));
    }

    // Special energy regen
    if (currentTick % 50 === 0) {
      setPlayerSpecial(prev => Math.min(MAX_SPECIAL, prev + 10));
      setEnemySpecial(prev => Math.min(MAX_SPECIAL, prev + 10));
    }

    // Enemy AI
    processEnemyAI();
  }, []);

  // Calculate hit (with combat triangle, class bonuses, crits, and combos!)
  const calculateHit = useCallback((attacker, defender, weapon, isSpec = false, comboCount = 0) => {
    const style = weapon.style;
    let defenderPraying = false;
    let defenderStyle = COMBAT_STYLES.MELEE;
    let attackerClass = playerClassRef.current;
    let defenderClass = enemyClassRef.current;

    if (attacker === 'player') {
      defenderPraying = enemyPrayersRef.current[style];
      defenderStyle = enemyStyleRef.current;
      attackerClass = playerClassRef.current;
      defenderClass = enemyClassRef.current;
    } else {
      defenderPraying = playerPrayersRef.current[style];
      defenderStyle = playerStyleRef.current;
      attackerClass = enemyClassRef.current;
      defenderClass = playerClassRef.current;
    }

    // Class bonuses
    const damageBonus = attackerClass?.damageBonus || 1.0;
    const defenseBonus = defenderClass?.defenseBonus || 1.0;

    // Combat triangle bonus/penalty
    // Melee > Ranged > Magic > Melee
    let triangleBonus = 1.0;
    if (style === COMBAT_STYLES.MELEE && defenderStyle === COMBAT_STYLES.RANGED) {
      triangleBonus = 1.2;
    } else if (style === COMBAT_STYLES.RANGED && defenderStyle === COMBAT_STYLES.MAGIC) {
      triangleBonus = 1.2;
    } else if (style === COMBAT_STYLES.MAGIC && defenderStyle === COMBAT_STYLES.MELEE) {
      triangleBonus = 1.2;
    } else if (style === COMBAT_STYLES.MELEE && defenderStyle === COMBAT_STYLES.MAGIC) {
      triangleBonus = 0.8;
    } else if (style === COMBAT_STYLES.RANGED && defenderStyle === COMBAT_STYLES.MELEE) {
      triangleBonus = 0.8;
    } else if (style === COMBAT_STYLES.MAGIC && defenderStyle === COMBAT_STYLES.RANGED) {
      triangleBonus = 0.8;
    }

    // Combo bonus: +5% damage per combo, capped at +25%
    const comboBonus = 1 + Math.min(comboCount * 0.05, 0.25);

    // Accuracy roll (with triangle bonus and class bonus)
    const attackRoll = (weapon.attack * triangleBonus * damageBonus) + Math.random() * 50;
    const defenseRoll = (50 * defenseBonus) + Math.random() * 30;

    // Prayer blocks 40% of hits
    if (defenderPraying && Math.random() < 0.4) {
      return { damage: 0, isCrit: false, isBlocked: true };
    }

    // Hit check
    if (attackRoll < defenseRoll && !isSpec) {
      return { damage: 0, isCrit: false, isMiss: true };
    }

    // Damage roll (with triangle bonus, class damage bonus, and combo bonus)
    let maxHit = Math.floor((weapon.strength * triangleBonus * damageBonus * comboBonus) / 3) + 10;

    // Apply defender's defense bonus (reduces damage taken)
    maxHit = Math.floor(maxHit / defenseBonus);

    // Spec modifiers
    if (isSpec) {
      if (weapon.specEffect === 'ags_spec') {
        maxHit = Math.floor(maxHit * 1.375);
      } else if (weapon.specEffect === 'double_hit_boosted') {
        maxHit = Math.floor(maxHit * 1.15);
      } else if (weapon.specEffect === 'volatile_spec') {
        maxHit = Math.floor(maxHit * 1.5);
      }
    }

    // Prayer reduction
    if (defenderPraying) {
      maxHit = Math.floor(maxHit * 0.6);
    }

    // Calculate base damage
    let damage = Math.floor(Math.random() * (maxHit + 1));

    // Critical hit check - higher combo = higher crit chance (up to +10%)
    const critChance = CRIT_CHANCE + Math.min(comboCount * 0.02, 0.10);
    const isCrit = Math.random() < critChance;

    if (isCrit && damage > 0) {
      damage = Math.floor(damage * CRIT_MULTIPLIER);
    }

    return { damage, isCrit, comboBonus };
  }, []);

  // Perform player attack
  const performPlayerAttack = useCallback(() => {
    if (enemyHPRef.current <= 0) return;

    const weapon = playerWeaponRef.current;
    const isSpec = specEnabledRef.current && playerSpecialRef.current >= weapon.specCost;
    const currentCombo = playerCombo;

    // Track player style pattern for AI learning
    const pattern = playerPatternRef.current;
    pattern.styleHistory.push(weapon.style);
    if (pattern.styleHistory.length > 10) pattern.styleHistory.shift();
    // Calculate preferred style
    const styleCounts = pattern.styleHistory.reduce((acc, s) => {
      acc[s] = (acc[s] || 0) + 1;
      return acc;
    }, {});
    pattern.preferredStyle = Object.entries(styleCounts).sort((a, b) => b[1] - a[1])[0]?.[0];

    let hits = [];

    if (isSpec) {
      setPlayerSpecial(prev => prev - weapon.specCost);
      setSpecEnabled(false);

      if (weapon.specEffect === 'double_hit' || weapon.specEffect === 'double_hit_boosted') {
        hits = [
          calculateHit('player', 'enemy', weapon, true, currentCombo),
          calculateHit('player', 'enemy', weapon, true, currentCombo + 1),
        ];
      } else if (weapon.specEffect === 'instant_hit') {
        hits = [calculateHit('player', 'enemy', weapon, true, currentCombo)];
      } else {
        hits = [calculateHit('player', 'enemy', weapon, true, currentCombo)];
      }
    } else {
      hits = [calculateHit('player', 'enemy', weapon, false, currentCombo)];
    }

    // Track if any hit landed for combo
    let anyHitLanded = false;

    // Apply damage
    hits.forEach((hitResult, i) => {
      const { damage, isCrit, isMiss, isBlocked } = hitResult;

      setTimeout(() => {
        if (damage > 0) {
          anyHitLanded = true;

          setEnemyHP(prev => {
            const newHP = Math.max(0, prev - damage);
            if (newHP <= 0) {
              // Death animation - enemy falls over
              if (enemyModelRef.current) {
                const fallAnimation = () => {
                  let rotation = 0;
                  const fall = setInterval(() => {
                    rotation += 0.1;
                    if (enemyModelRef.current) {
                      enemyModelRef.current.rotation.z = rotation;
                      enemyModelRef.current.position.y -= 0.02;
                    }
                    if (rotation >= Math.PI / 2) {
                      clearInterval(fall);
                    }
                  }, 20);
                };
                fallAnimation();
              }
              setTimeout(() => setPhase(PHASES.VICTORY), 1000);
            }
            return newHP;
          });

          // Critical hit effects
          if (isCrit) {
            setLastCrit({ target: 'enemy', damage });
            setScreenShake(10); // Trigger screen shake
            setTimeout(() => setScreenShake(0), 200);
          }

          // Smite effect (drain 1 prayer per 4 damage, Mage class gets 2x)
          const isMage = playerClassRef.current?.id === 'wizard_plush';
          const prayerDrain = Math.floor(damage / 4) * (isMage ? 2 : 1);
          setEnemyPrayer(prev => {
            const newPrayer = Math.max(0, prev - prayerDrain);
            if (newPrayer === 0 && prev > 0) {
              setStats(s => ({ ...s, enemyPrayerDrained: true }));
            }
            return newPrayer;
          });

          setStats(s => ({ ...s, damageDealt: s.damageDealt + damage }));
        }

        // Add hit splat (with crit indicator)
        addHitSplat('enemy', damage, isCrit);
      }, i * 300);
    });

    // Update combo counter
    setTimeout(() => {
      if (anyHitLanded || hits.some(h => h.damage > 0)) {
        setPlayerCombo(prev => prev + hits.filter(h => h.damage > 0).length);
        setEnemyCombo(0); // Reset enemy combo on successful hit
      } else {
        setPlayerCombo(0); // Reset combo on miss
      }
    }, hits.length * 300);

    // Attack animation
    if (playerModelRef.current?.rightArm) {
      const arm = playerModelRef.current.rightArm;
      arm.rotation.x = -1;
      setTimeout(() => { arm.rotation.x = 0; }, 200);
    }
  }, [calculateHit, playerCombo]);

  // Perform enemy attack
  const performEnemyAttack = useCallback(() => {
    if (playerHPRef.current <= 0) return;

    const weapon = enemyWeaponRef.current;
    const currentCombo = enemyCombo;
    const isSpec = enemySpecialRef.current >= weapon.specCost && Math.random() < aiDifficulty.specUseChance;

    let hits = [];

    if (isSpec && weapon.specCost > 0) {
      setEnemySpecial(prev => prev - weapon.specCost);

      if (weapon.specEffect === 'double_hit' || weapon.specEffect === 'double_hit_boosted') {
        hits = [
          calculateHit('enemy', 'player', weapon, true, currentCombo),
          calculateHit('enemy', 'player', weapon, true, currentCombo + 1),
        ];
      } else {
        hits = [calculateHit('enemy', 'player', weapon, true, currentCombo)];
      }
    } else {
      hits = [calculateHit('enemy', 'player', weapon, false, currentCombo)];
    }

    // Track if any hit landed for combo
    let anyHitLanded = false;

    // Apply damage
    hits.forEach((hitResult, i) => {
      const { damage, isCrit } = hitResult;

      setTimeout(() => {
        if (damage > 0) {
          anyHitLanded = true;

          setPlayerHP(prev => {
            const newHP = Math.max(0, prev - damage);
            if (newHP <= 0) {
              // Death animation - player falls over
              if (playerModelRef.current) {
                const fallAnimation = () => {
                  let rotation = 0;
                  const fall = setInterval(() => {
                    rotation += 0.1;
                    if (playerModelRef.current) {
                      playerModelRef.current.rotation.z = -rotation;
                      playerModelRef.current.position.y -= 0.02;
                    }
                    if (rotation >= Math.PI / 2) {
                      clearInterval(fall);
                    }
                  }, 20);
                };
                fallAnimation();
              }
              setTimeout(() => setPhase(PHASES.DEFEAT), 1000);
            }
            return newHP;
          });

          // Critical hit effects
          if (isCrit) {
            setLastCrit({ target: 'player', damage });
            setScreenShake(8);
            setTimeout(() => setScreenShake(0), 200);
          }

          setStats(s => ({ ...s, damageTaken: s.damageTaken + damage }));
        }

        addHitSplat('player', damage, isCrit);
      }, i * 300);
    });

    // Update combo counter
    setTimeout(() => {
      if (anyHitLanded || hits.some(h => h.damage > 0)) {
        setEnemyCombo(prev => prev + hits.filter(h => h.damage > 0).length);
        setPlayerCombo(0); // Reset player combo when enemy hits
      } else {
        setEnemyCombo(0); // Reset combo on miss
      }
    }, hits.length * 300);

    // Attack animation
    if (enemyModelRef.current?.rightArm) {
      const arm = enemyModelRef.current.rightArm;
      arm.rotation.x = -1;
      setTimeout(() => { arm.rotation.x = 0; }, 200);
    }
  }, [calculateHit, enemyCombo, aiDifficulty]);

  // Add hit splat with random offset to prevent overlap
  const addHitSplat = useCallback((target, damage, isCrit = false) => {
    const id = Date.now() + Math.random();
    const offsetX = (Math.random() - 0.5) * 60; // Random horizontal offset
    const offsetY = Math.random() * 30; // Random vertical offset
    setHitSplats(prev => [...prev, { id, target, damage, offsetX, offsetY, isCrit }]);
    setTimeout(() => {
      setHitSplats(prev => prev.filter(s => s.id !== id));
    }, 1500);
  }, []);

  // Smart Enemy AI with pattern recognition and adaptive behavior
  const processEnemyAI = useCallback(() => {
    const diff = aiDifficulty;
    const pattern = playerPatternRef.current;

    // SMART PRAYER SWITCHING
    // Uses player pattern to predict next attack style
    if (tickRef.current % diff.reactSpeed === 0) {
      let predictedStyle = playerStyleRef.current; // Default: current style

      // If we have enough history, predict based on patterns
      if (pattern.styleHistory.length >= 3 && Math.random() < diff.prayerAccuracy) {
        // Check if player tends to stick with one style
        if (pattern.preferredStyle) {
          predictedStyle = pattern.preferredStyle;
        }

        // Check for style-switching patterns (e.g., alternating melee-ranged)
        const recent = pattern.styleHistory.slice(-3);
        if (recent[0] === recent[2] && recent[0] !== recent[1]) {
          // Alternating pattern detected - predict the next one
          predictedStyle = recent[1];
        }
      } else if (Math.random() < diff.prayerAccuracy) {
        // Use current player style if no pattern detected
        predictedStyle = playerStyleRef.current;
      }

      // Apply prayer with some randomness based on difficulty
      if (Math.random() < diff.prayerAccuracy) {
        setEnemyPrayers({
          melee: predictedStyle === COMBAT_STYLES.MELEE,
          ranged: predictedStyle === COMBAT_STYLES.RANGED,
          magic: predictedStyle === COMBAT_STYLES.MAGIC,
        });
      }
    }

    // SMART EATING - More strategic timing
    const hpPercent = enemyHPRef.current / (enemyClassRef.current?.hp || MAX_HP);
    const shouldEat = hpPercent < (diff.eatThreshold / 100);

    if (shouldEat && enemyHPRef.current > 0) {
      // Higher difficulty = smarter eat timing (waits for safe moments)
      const safeToEat = tickRef.current >= enemyAttackTickRef.current - 1;
      if (safeToEat && Math.random() < 0.3) {
        const maxHeal = enemyClassRef.current?.hp || MAX_HP;
        setEnemyHP(prev => Math.min(maxHeal, prev + 20));
        addHitSplat('enemy', -20);
      }
    }

    // SMART WEAPON SWITCHING - Counter player's style
    if (Math.random() < diff.weaponSwitchChance) {
      const playerStyle = pattern.preferredStyle || playerStyleRef.current;
      let counterWeapons = [];

      // Combat triangle: pick weapon that beats player's style
      if (playerStyle === COMBAT_STYLES.MELEE) {
        // Magic beats Melee
        counterWeapons = [WEAPONS.magic_wand, WEAPONS.glitter_staff];
      } else if (playerStyle === COMBAT_STYLES.RANGED) {
        // Melee beats Ranged
        counterWeapons = [WEAPONS.foam_sword, WEAPONS.toy_hammer, WEAPONS.pillow];
      } else if (playerStyle === COMBAT_STYLES.MAGIC) {
        // Ranged beats Magic
        counterWeapons = [WEAPONS.nerf_blaster, WEAPONS.water_gun];
      }

      // Filter to weapons the enemy class can use
      const classWeapons = enemyClassRef.current?.weapons || [];
      const available = counterWeapons.filter(w => classWeapons.includes(w.id));

      if (available.length > 0) {
        const newWeapon = available[Math.floor(Math.random() * available.length)];
        setEnemyWeapon(newWeapon);
        setEnemyStyle(newWeapon.style);
      }
    }

    // COMBO BREAKER - Try to break player combos
    if (playerCombo >= 3 && Math.random() < 0.4) {
      // Switch to defensive style when player is on a streak
      const defensiveWeapons = [WEAPONS.pillow]; // Best for defense
      const classWeapons = enemyClassRef.current?.weapons || [];
      const available = defensiveWeapons.filter(w => classWeapons.includes(w.id));

      if (available.length > 0) {
        setEnemyWeapon(available[0]);
        setEnemyStyle(available[0].style);
      }
    }
  }, [addHitSplat, aiDifficulty, playerCombo]);

  // Eat food
  const eatFood = useCallback((index) => {
    const item = playerInventory[index];
    if (!item || !item.heal) return;
    if (playerHP >= MAX_HP) return;

    setPlayerHP(prev => Math.min(MAX_HP, prev + item.heal));
    setPlayerInventory(prev => {
      const next = [...prev];
      next[index] = null;
      return next;
    });
    setStats(s => ({ ...s, foodEaten: s.foodEaten + 1 }));
    addHitSplat('player', -item.heal);
  }, [playerInventory, playerHP, addHitSplat]);

  // Switch weapon
  const switchWeapon = useCallback((index) => {
    const item = playerInventory[index];
    if (!item || !item.attack) return;

    // Swap with current weapon
    setPlayerInventory(prev => {
      const next = [...prev];
      next[index] = playerWeapon;
      return next;
    });
    setPlayerWeapon(item);
    setPlayerStyle(item.style);
  }, [playerInventory, playerWeapon]);

  // Toggle prayer
  const togglePrayer = useCallback((prayer) => {
    if (playerPrayer <= 0) return;

    setPlayerPrayers(prev => {
      // Turn off all other protection prayers
      const next = { melee: false, ranged: false, magic: false };
      if (!prev[prayer]) {
        next[prayer] = true;
      }
      return next;
    });
  }, [playerPrayer]);

  // Start fight - initialize based on class
  const startFight = useCallback(() => {
    if (!selectedClass) return;

    // Pick random enemy class
    const classIds = Object.keys(CLASSES);
    const randomEnemyClass = CLASSES[classIds[Math.floor(Math.random() * classIds.length)]];
    setEnemyClass(randomEnemyClass);
    enemyClassRef.current = randomEnemyClass;
    playerClassRef.current = selectedClass;

    // Set player stats based on class
    setPlayerHP(selectedClass.hp);
    setPlayerPrayer(selectedClass.prayer);
    setPlayerSpecial(selectedClass.special);
    playerHPRef.current = selectedClass.hp;
    playerPrayerRef.current = selectedClass.prayer;
    playerSpecialRef.current = selectedClass.special;

    // Set player inventory based on class
    const classWeapons = selectedClass.weapons.map(wId => WEAPONS[wId]);
    const foodCount = selectedClass.food;
    const newInventory = [];
    for (let i = 0; i < foodCount; i++) {
      newInventory.push(i % 4 === 3 ? FOOD.gummy : FOOD.cookie);
    }
    classWeapons.forEach(w => newInventory.push(w));
    while (newInventory.length < 28) newInventory.push(null);
    setPlayerInventory(newInventory);
    setPlayerWeapon(classWeapons[0]);
    setPlayerStyle(classWeapons[0].style);
    playerWeaponRef.current = classWeapons[0];
    playerStyleRef.current = classWeapons[0].style;

    // Set enemy stats based on random class
    setEnemyHP(randomEnemyClass.hp);
    setEnemyPrayer(randomEnemyClass.prayer);
    setEnemySpecial(randomEnemyClass.special);
    enemyHPRef.current = randomEnemyClass.hp;
    enemyPrayerRef.current = randomEnemyClass.prayer;
    enemySpecialRef.current = randomEnemyClass.special;

    // Set enemy weapon
    const enemyWeapons = randomEnemyClass.weapons.map(wId => WEAPONS[wId]);
    setEnemyWeapon(enemyWeapons[0]);
    setEnemyStyle(enemyWeapons[0].style);
    enemyWeaponRef.current = enemyWeapons[0];
    enemyStyleRef.current = enemyWeapons[0].style;

    // Reset positions
    playerPosRef.current = { x: -1, z: 0 };
    enemyPosRef.current = { x: 1, z: 0 };
    playerTargetRef.current = { x: -1, z: 0 };
    enemyTargetRef.current = { x: 1, z: 0 };
    isAttackingRef.current = true;
    playerMovingRef.current = false;
    playerAttackTickRef.current = 0;
    enemyAttackTickRef.current = 0;

    // Reset stats tracking (include playerMaxHP for contract checks)
    setStats({ foodEaten: 0, damageDealt: 0, damageTaken: 0, enemyPrayerDrained: false, playerMaxHP: selectedClass.hp });

    // Reset combo counters
    setPlayerCombo(0);
    setEnemyCombo(0);

    // Reset player pattern tracking
    playerPatternRef.current = {
      styleHistory: [],
      weaponSwitchFrequency: 0,
      avgTimeBetweenEats: 0,
      preferredStyle: null,
      lastEatTick: 0,
      eatCount: 0,
    };

    setPhase(PHASES.COUNTDOWN);
    setCountdown(3);
  }, [selectedClass]);

  // Manual attack handler (with cooldown)
  const handleManualAttack = useCallback(() => {
    if (phaseRef.current !== PHASES.FIGHTING) return;
    if (enemyHPRef.current <= 0) return;

    // Check cooldown - can only attack if current tick >= next attack tick
    if (tickRef.current < playerAttackTickRef.current) {
      return;
    }

    isAttackingRef.current = true;
    performPlayerAttack();

    // Set next attack tick (respects weapon speed and class speed bonus)
    const playerSpeedMod = playerClassRef.current?.speedBonus || 1.0;
    const nextTick = tickRef.current + Math.ceil(playerWeaponRef.current.speed * playerSpeedMod);
    playerAttackTickRef.current = nextTick;
    setPlayerAttackTick(nextTick);
    setCanAttack(false); // Disable button until cooldown
  }, [performPlayerAttack]);

  // Keyboard support (spacebar to attack)
  useEffect(() => {
    const handleKeyDown = (e) => {
      if (e.code === 'Space' && phaseRef.current === PHASES.FIGHTING) {
        e.preventDefault();
        handleManualAttack();
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [handleManualAttack]);

  // Countdown effect
  useEffect(() => {
    if (phase !== PHASES.COUNTDOWN) return;

    const interval = setInterval(() => {
      setCountdown(prev => {
        if (prev <= 1) {
          clearInterval(interval);
          setPhase(PHASES.FIGHTING);
          setInCombat(true);
          lastTickTimeRef.current = performance.now();
          tickRef.current = 0;
          return 0;
        }
        return prev - 1;
      });
    }, 1000);

    return () => clearInterval(interval);
  }, [phase]);

  // Collect reward - applies difficulty multiplier!
  const collectReward = useCallback(() => {
    const won = phase === PHASES.VICTORY;
    let earnedCoins;

    if (won) {
      const success = selectedContract.checkSuccess(playerHP, enemyHP, stats);
      if (success) {
        // Total multiplier = contract multiplier × difficulty multiplier
        const totalMult = selectedContract.multiplier * aiDifficulty.rewardMult;
        earnedCoins = Math.floor(selectedStake * totalMult);
      } else {
        // Won but didn't meet contract - still get difficulty bonus on returned stake
        earnedCoins = Math.floor(selectedStake * aiDifficulty.rewardMult);
      }
    } else {
      earnedCoins = -selectedStake;
    }

    onGameEnd(earnedCoins);
  }, [phase, selectedContract, selectedStake, playerHP, enemyHP, stats, onGameEnd, aiDifficulty]);

  // Render class selection
  const renderClassSelect = () => (
    <motion.div
      className="duel-overlay class-select"
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
    >
      <button className="duel-close-btn" onClick={onClose}>X</button>

      <div className="class-container">
        <h1 className="duel-title">
          <span>🧸</span> PICK YOUR TOY <span>🧸</span>
        </h1>
        <p className="duel-subtitle">Each toy has unique powers!</p>

        <div className="classes-grid">
          {Object.values(CLASSES).map(cls => (
            <motion.button
              key={cls.id}
              className={`class-card ${selectedClass?.id === cls.id ? 'selected' : ''}`}
              style={{ '--class-color': cls.color }}
              onClick={() => setSelectedClass(cls)}
              whileHover={{ scale: 1.03 }}
              whileTap={{ scale: 0.97 }}
            >
              <div className="class-header">
                <span className="class-emoji">{cls.emoji}</span>
                <span className="class-name">{cls.name}</span>
              </div>
              <p className="class-desc">{cls.description}</p>
              <div className="class-stats">
                <div className="class-stat">
                  <span className="stat-label">HP</span>
                  <span className="stat-num">{cls.hp}</span>
                </div>
                <div className="class-stat">
                  <span className="stat-label">Prayer</span>
                  <span className="stat-num">{cls.prayer}</span>
                </div>
                <div className="class-stat">
                  <span className="stat-label">Food</span>
                  <span className="stat-num">{cls.food}</span>
                </div>
              </div>
              <div className="class-specialty">
                {cls.specialty}
              </div>
            </motion.button>
          ))}
        </div>

        {selectedClass && (
          <motion.button
            className="confirm-class-btn"
            onClick={() => setPhase(PHASES.STAKE_SELECT)}
            initial={{ scale: 0 }}
            animate={{ scale: 1 }}
            whileHover={{ scale: 1.05 }}
          >
            Confirm {selectedClass.name}
          </motion.button>
        )}
      </div>
    </motion.div>
  );

  // Render stake selection
  const renderStakeSelect = () => (
    <motion.div
      className="duel-overlay stake-select"
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
    >
      <button className="duel-close-btn" onClick={onClose}>X</button>

      <div className="stake-container">
        <h1 className="duel-title">
          <span>🎪</span> TOY BOX BATTLE <span>🎪</span>
        </h1>
        <p className="duel-subtitle">Epic 1v1 toy showdown!</p>

        <div className="selected-class-display" style={{ '--class-color': selectedClass?.color }}>
          <span>{selectedClass?.emoji}</span>
          <span className="selected-class-name">{selectedClass?.name}</span>
          <button
            className="change-class-btn"
            onClick={() => setPhase(PHASES.CLASS_SELECT)}
          >
            Change
          </button>
        </div>

        <div className="your-coins">
          <span>🪙</span> {player?.coins || 0} coins
        </div>

        <h2 className="section-title">Enemy Difficulty</h2>
        <div className="difficulty-grid">
          {Object.entries(AI_DIFFICULTY).map(([key, diff]) => (
            <motion.button
              key={key}
              className={`difficulty-btn ${aiDifficulty === diff ? 'selected' : ''} ${key.toLowerCase()}`}
              onClick={() => setAiDifficulty(diff)}
              whileHover={{ scale: 1.05 }}
              whileTap={{ scale: 0.95 }}
            >
              <span className="diff-name">{key}</span>
              <span className="diff-reward">{diff.rewardMult}x coins</span>
              <span className="diff-desc">
                {key === 'EASY' && '🌟 For beginners'}
                {key === 'MEDIUM' && '⭐ Balanced fight'}
                {key === 'HARD' && '💪 Tough opponent'}
                {key === 'EXPERT' && '🔥 Ultimate challenge'}
              </span>
            </motion.button>
          ))}
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
              <span className="contract-desc">{contract.description}</span>
              <span className="contract-mult">{contract.multiplier}x</span>
            </motion.button>
          ))}
        </div>

        {selectedContract && (
          <>
            <h2 className="section-title">Select Stake</h2>
            <div className="stakes-grid">
              {availableStakes.length > 0 ? (
                availableStakes.map(stake => {
                  const totalMult = selectedContract.multiplier * aiDifficulty.rewardMult;
                  const reward = Math.floor(stake * totalMult);
                  return (
                    <motion.button
                      key={stake}
                      className={`stake-btn ${selectedStake === stake ? 'selected' : ''}`}
                      onClick={() => setSelectedStake(stake)}
                      whileHover={{ scale: 1.05 }}
                      whileTap={{ scale: 0.95 }}
                    >
                      <span className="stake-amount">🪙 {stake}</span>
                      <span className="stake-reward">Win: {reward}</span>
                      <span className="stake-mult">({selectedContract.multiplier}x × {aiDifficulty.rewardMult}x)</span>
                    </motion.button>
                  );
                })
              ) : (
                <p className="no-stakes">Not enough coins!</p>
              )}
            </div>
          </>
        )}

        {selectedContract && selectedStake && (
          <motion.button
            className="start-btn"
            onClick={startFight}
            initial={{ scale: 0 }}
            animate={{ scale: 1 }}
            whileHover={{ scale: 1.05 }}
          >
            Enter Arena!
          </motion.button>
        )}
      </div>
    </motion.div>
  );

  // Render game HUD
  const renderGameHUD = () => {
    const playerMaxHP = selectedClass?.hp || MAX_HP;
    const playerMaxPrayer = selectedClass?.prayer || MAX_PRAYER;
    const enemyMaxHP = enemyClass?.hp || MAX_HP;
    const enemyMaxPrayer = enemyClass?.prayer || MAX_PRAYER;

    return (
    <div className="duel-hud">
      {/* Top bar */}
      <div className="hud-top">
        <div className="player-stats">
          <div className="player-class-badge" style={{ '--class-color': selectedClass?.color }}>
            <span>{selectedClass?.emoji}</span>
            <span>{selectedClass?.name}</span>
          </div>
          <div className="stat-bar hp">
            <span className="stat-icon">❤️</span>
            <div className="bar-bg">
              <div className="bar-fill" style={{ width: `${(playerHP / playerMaxHP) * 100}%` }} />
            </div>
            <span className="stat-value">{playerHP}/{playerMaxHP}</span>
          </div>
          <div className="stat-bar prayer">
            <span className="stat-icon">🔋</span>
            <div className="bar-bg">
              <div className="bar-fill" style={{ width: `${(playerPrayer / playerMaxPrayer) * 100}%` }} />
            </div>
            <span className="stat-value">{playerPrayer}/{playerMaxPrayer}</span>
          </div>
          <div className="stat-bar special">
            <span className="stat-icon">⚡</span>
            <div className="bar-bg">
              <div className="bar-fill" style={{ width: `${playerSpecial}%` }} />
            </div>
            <span className="stat-value">{playerSpecial}%</span>
          </div>
        </div>

        <div className="contract-display" style={{ '--contract-color': selectedContract?.color }}>
          <span>{selectedContract?.emoji}</span>
          <span>{selectedContract?.name}</span>
        </div>

        <div className="enemy-stats">
          <div className="enemy-class-badge" style={{ '--class-color': enemyClass?.color }}>
            <span>{enemyClass?.emoji}</span>
            <span>{enemyClass?.name || 'Enemy'}</span>
          </div>
          <div className="enemy-weapon">
            {enemyWeapon.style === COMBAT_STYLES.MELEE ? '🗡️' :
             enemyWeapon.style === COMBAT_STYLES.RANGED ? '🔫' : '✨'}
            {' '}{enemyWeapon.name}
          </div>
          <div className="stat-bar hp enemy">
            <span className="stat-value">{enemyHP}/{enemyMaxHP}</span>
            <div className="bar-bg">
              <div className="bar-fill" style={{ width: `${(enemyHP / enemyMaxHP) * 100}%` }} />
            </div>
            <span className="stat-icon">❤️</span>
          </div>
          <div className="stat-bar prayer enemy">
            <span className="stat-value">{enemyPrayer}/{enemyMaxPrayer}</span>
            <div className="bar-bg">
              <div className="bar-fill" style={{ width: `${(enemyPrayer / enemyMaxPrayer) * 100}%` }} />
            </div>
            <span className="stat-icon">🔋</span>
          </div>
          <div className="stat-bar special enemy">
            <span className="stat-value">{enemySpecial}%</span>
            <div className="bar-bg">
              <div className="bar-fill" style={{ width: `${enemySpecial}%` }} />
            </div>
            <span className="stat-icon">⚡</span>
          </div>
          <div className="enemy-prayer-icons">
            {enemyPrayers.melee && <span title="Block Bonk">🛡️🗡️</span>}
            {enemyPrayers.ranged && <span title="Block Pew Pew">🛡️🔫</span>}
            {enemyPrayers.magic && <span title="Block Sparkle">🛡️✨</span>}
          </div>
        </div>
      </div>

      {/* Hit splats */}
      <div className="hit-splats">
        {hitSplats.map(splat => (
          <motion.div
            key={splat.id}
            className={`hit-splat ${splat.target} ${splat.damage === 0 ? 'zero' : splat.damage < 0 ? 'heal' : 'damage'} ${splat.isCrit ? 'crit' : ''}`}
            style={{
              marginLeft: splat.offsetX || 0,
              marginTop: splat.offsetY || 0
            }}
            initial={{ y: 0, opacity: 1, scale: splat.isCrit ? 2 : 1.5 }}
            animate={{ y: -60, opacity: 0, scale: 1 }}
            transition={{ duration: splat.isCrit ? 1.5 : 1.2 }}
          >
            {splat.isCrit && '💥'}
            {splat.damage < 0 ? `+${-splat.damage}` : splat.damage}
            {splat.isCrit && '!'}
          </motion.div>
        ))}
      </div>

      {/* Combo display */}
      {playerCombo >= 2 && (
        <motion.div
          className="combo-display player-combo"
          initial={{ scale: 0 }}
          animate={{ scale: 1 }}
          key={playerCombo}
        >
          🔥 {playerCombo}x COMBO!
        </motion.div>
      )}
      {enemyCombo >= 2 && (
        <motion.div
          className="combo-display enemy-combo"
          initial={{ scale: 0 }}
          animate={{ scale: 1 }}
          key={enemyCombo}
        >
          {enemyCombo}x 💀
        </motion.div>
      )}

      {/* Bottom controls */}
      <div className={`hud-bottom ${hudMinimized ? 'minimized' : ''}`}>
        {/* Toggle button */}
        <button
          className="hud-toggle-btn"
          onClick={() => setHudMinimized(!hudMinimized)}
        >
          {hudMinimized ? '☰ Controls' : '▼ Hide'}
        </button>

        {/* Minimized quick actions */}
        {hudMinimized && (
          <div className="hud-minimized-bar">
            <button
              className={`quick-attack-btn ${canAttack ? 'ready' : ''}`}
              onClick={handleManualAttack}
              disabled={enemyHP <= 0 || !canAttack}
            >
              💥 {canAttack ? 'BONK!' : '...'}
            </button>
            <button
              className={`quick-spec-btn ${specEnabled ? 'active' : ''}`}
              onClick={() => setSpecEnabled(!specEnabled)}
              disabled={playerSpecial < playerWeapon.specCost}
            >
              ⚡
            </button>
            <div className="quick-food">
              {playerInventory.slice(0, 4).map((item, i) => (
                item?.heal && (
                  <button
                    key={i}
                    className="quick-food-btn"
                    onClick={() => eatFood(i)}
                  >
                    {item.emoji}
                  </button>
                )
              ))}
            </div>
          </div>
        )}

        {/* Full controls - hidden when minimized */}
        {!hudMinimized && (
          <>
        {/* Combat styles */}
        <div className="combat-panel">
          <div className="current-weapon">
            <span className="weapon-icon">
              {playerWeapon.style === COMBAT_STYLES.MELEE ? '🗡️' :
               playerWeapon.style === COMBAT_STYLES.RANGED ? '🔫' : '✨'}
            </span>
            <span className="weapon-name">{playerWeapon.name}</span>
          </div>
          <h3>Attack Type</h3>
          <div className="style-buttons">
            <button
              className={`style-btn ${playerStyle === COMBAT_STYLES.MELEE ? 'active' : ''}`}
              onClick={() => setPlayerStyle(COMBAT_STYLES.MELEE)}
            >
              🗡️ Bonk
            </button>
            <button
              className={`style-btn ${playerStyle === COMBAT_STYLES.RANGED ? 'active' : ''}`}
              onClick={() => setPlayerStyle(COMBAT_STYLES.RANGED)}
            >
              🔫 Pew
            </button>
            <button
              className={`style-btn ${playerStyle === COMBAT_STYLES.MAGIC ? 'active' : ''}`}
              onClick={() => setPlayerStyle(COMBAT_STYLES.MAGIC)}
            >
              ✨ Sparkle
            </button>
          </div>

          <h3>Block</h3>
          <div className="prayer-buttons">
            <button
              className={`prayer-btn ${playerPrayers.melee ? 'active' : ''}`}
              onClick={() => togglePrayer('melee')}
              disabled={playerPrayer <= 0}
            >
              🛡️🗡️
            </button>
            <button
              className={`prayer-btn ${playerPrayers.ranged ? 'active' : ''}`}
              onClick={() => togglePrayer('ranged')}
              disabled={playerPrayer <= 0}
            >
              🛡️🔫
            </button>
            <button
              className={`prayer-btn ${playerPrayers.magic ? 'active' : ''}`}
              onClick={() => togglePrayer('magic')}
              disabled={playerPrayer <= 0}
            >
              🛡️✨
            </button>
          </div>

          <button
            className={`spec-btn ${specEnabled ? 'active' : ''}`}
            onClick={() => setSpecEnabled(!specEnabled)}
            disabled={playerSpecial < playerWeapon.specCost || playerWeapon.specCost === 0}
          >
            ⚡ POWER MOVE ({playerWeapon.specCost}%)
          </button>

          <button
            className={`attack-btn ${canAttack ? 'ready' : 'on-cooldown'}`}
            onClick={handleManualAttack}
            disabled={enemyHP <= 0 || !canAttack}
          >
            💥 {canAttack ? 'BONK!' : 'WAIT...'}
          </button>
        </div>

        {/* Inventory */}
        <div className="inventory-panel">
          <h3>Toy Box</h3>
          <div className="inventory-grid">
            {playerInventory.map((item, i) => (
              <button
                key={i}
                className={`inv-slot ${item ? (item.heal ? 'food' : 'weapon') : 'empty'}`}
                onClick={() => item?.heal ? eatFood(i) : item?.attack ? switchWeapon(i) : null}
                disabled={!item}
                title={item?.name || ''}
              >
                {item?.emoji || (item?.attack ? (
                  item.style === COMBAT_STYLES.MELEE ? '🗡️' :
                  item.style === COMBAT_STYLES.RANGED ? '🔫' : '✨'
                ) : '')}
              </button>
            ))}
          </div>
        </div>
          </>
        )}
      </div>
    </div>
  );
  };

  // Render countdown
  const renderCountdown = () => (
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
      <p>Get ready to play!</p>
    </motion.div>
  );

  // Render result
  const renderResult = () => {
    const won = phase === PHASES.VICTORY;
    const contractMet = won && selectedContract.checkSuccess(playerHP, enemyHP, stats);

    return (
      <motion.div
        className="duel-overlay result"
        initial={{ opacity: 0 }}
        animate={{ opacity: 1 }}
      >
        <motion.div
          className={`result-container ${won ? 'victory' : 'defeat'}`}
          initial={{ scale: 0.5 }}
          animate={{ scale: 1 }}
        >
          <h1>{won ? '🎉 YOU WIN!' : '😵 OH NO!'}</h1>

          {won && (
            <div className={`contract-result ${contractMet ? 'met' : 'failed'}`}>
              {contractMet ? (
                <>
                  <span>{selectedContract.emoji} Challenge Complete!</span>
                  <span className="reward">🪙 +{Math.floor(selectedStake * selectedContract.multiplier * aiDifficulty.rewardMult)}</span>
                  <span className="reward-breakdown">
                    ({selectedContract.multiplier}x contract × {aiDifficulty.rewardMult}x {aiDifficulty.name})
                  </span>
                </>
              ) : (
                <>
                  <span>Challenge not met</span>
                  <span className="reward">🪙 +{Math.floor(selectedStake * aiDifficulty.rewardMult)}</span>
                  <span className="reward-breakdown">({aiDifficulty.rewardMult}x {aiDifficulty.name} bonus)</span>
                </>
              )}
            </div>
          )}

          {!won && (
            <div className="loss-display">
              <span>Bet Lost</span>
              <span className="loss">🪙 -{selectedStake}</span>
            </div>
          )}

          <div className="stats-summary">
            <div>Bonks Given: {stats.damageDealt}</div>
            <div>Bonks Taken: {stats.damageTaken}</div>
            <div>Snacks Eaten: {stats.foodEaten}</div>
          </div>

          <motion.button
            className="collect-btn"
            onClick={collectReward}
            whileHover={{ scale: 1.05 }}
          >
            {won ? 'Yay!' : 'Play Again'}
          </motion.button>
        </motion.div>
      </motion.div>
    );
  };

  return (
    <div className="duel-game">
      {phase === PHASES.CLASS_SELECT && renderClassSelect()}
      {phase === PHASES.STAKE_SELECT && renderStakeSelect()}

      {(phase === PHASES.COUNTDOWN || phase === PHASES.FIGHTING ||
        phase === PHASES.VICTORY || phase === PHASES.DEFEAT) && (
        <div
          className="duel-arena"
          ref={containerRef}
          style={{
            transform: screenShake > 0 ? `translate(${(Math.random() - 0.5) * screenShake}px, ${(Math.random() - 0.5) * screenShake}px)` : 'none'
          }}
        >
          {phase === PHASES.COUNTDOWN && renderCountdown()}
          {phase === PHASES.FIGHTING && renderGameHUD()}
          {(phase === PHASES.VICTORY || phase === PHASES.DEFEAT) && renderResult()}
        </div>
      )}
    </div>
  );
};

export default DuelGame;
