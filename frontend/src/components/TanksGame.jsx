import { useState, useEffect, useCallback, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './TanksGame.css';

const GAME_DURATION = 150000; // 2.5 minutes
const CANVAS_WIDTH = 400;
const CANVAS_HEIGHT = 500;
const TANK_WIDTH = 22;
const TANK_HEIGHT = 16;
const GRAVITY = 0.18;
const TERRAIN_SEGMENTS = 60;
const TARGET_FPS = 60;
const FRAME_TIME = 1000 / TARGET_FPS;

// Weapon types with different effects
const WEAPONS = {
  standard: {
    name: 'Standard',
    emoji: '💥',
    damage: [28, 38],
    radius: 5,
    explosionSize: 35,
    deformRadius: 35,
    speed: 1,
    color: '#ff6b6b',
    trailColor: 'rgba(255, 107, 107, 0.6)',
  },
  heavy: {
    name: 'Heavy Shell',
    emoji: '💣',
    damage: [45, 60],
    radius: 7,
    explosionSize: 55,
    deformRadius: 50,
    speed: 0.85,
    color: '#ff4757',
    trailColor: 'rgba(255, 71, 87, 0.7)',
  },
  sniper: {
    name: 'Sniper',
    emoji: '🎯',
    damage: [35, 45],
    radius: 4,
    explosionSize: 25,
    deformRadius: 20,
    speed: 1.4,
    color: '#ffd700',
    trailColor: 'rgba(255, 215, 0, 0.6)',
  },
  cluster: {
    name: 'Cluster',
    emoji: '🔥',
    damage: [15, 22],
    radius: 4,
    explosionSize: 25,
    deformRadius: 20,
    speed: 1.1,
    color: '#ff9500',
    trailColor: 'rgba(255, 149, 0, 0.6)',
    cluster: true,
    clusterCount: 4,
  },
};

const TanksGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [playerHealth, setPlayerHealth] = useState(100);
  const [cpuHealth, setCpuHealth] = useState(100);
  const [currentTurn, setCurrentTurn] = useState('player');
  const [angle, setAngle] = useState(45);
  const [power, setPower] = useState(50);
  const [message, setMessage] = useState('');
  const [wind, setWind] = useState(0);
  const [selectedWeapon, setSelectedWeapon] = useState('standard');
  const [streak, setStreak] = useState(0);
  const [showStreak, setShowStreak] = useState(false);
  const [screenShake, setScreenShake] = useState(0);
  const [isAiming, setIsAiming] = useState(false);
  const [touchPos, setTouchPos] = useState(null);

  const canvasRef = useRef(null);
  const gameContainerRef = useRef(null);
  const animationFrameRef = useRef(null);
  const terrainRef = useRef([]);
  const playerTankRef = useRef({ x: 17, y: 0 });
  const cpuTankRef = useRef({ x: CANVAS_WIDTH - 17, y: 0 });
  const bulletsRef = useRef([]);
  const explosionsRef = useRef([]);
  const particlesRef = useRef([]);
  const gameStateRef = useRef(gameState);
  const currentTurnRef = useRef(currentTurn);
  const playerHealthRef = useRef(100);
  const cpuHealthRef = useRef(100);
  const streakRef = useRef(0);
  const lastTimeRef = useRef(0);
  const dprRef = useRef(1);
  const grassRef = useRef([]);
  const rocksRef = useRef([]);
  const cloudsRef = useRef([]);
  const angleRef = useRef(45);
  const powerRef = useRef(50);
  const touchStartRef = useRef(null);

  useEffect(() => {
    gameStateRef.current = gameState;
  }, [gameState]);

  useEffect(() => {
    currentTurnRef.current = currentTurn;
  }, [currentTurn]);

  // Generate decorations
  const generateDecorations = useCallback(() => {
    // Grass tufts
    const grass = [];
    for (let i = 0; i < 40; i++) {
      grass.push({
        x: Math.random() * CANVAS_WIDTH,
        height: 8 + Math.random() * 12,
        blades: 3 + Math.floor(Math.random() * 3),
        sway: Math.random() * Math.PI * 2,
      });
    }
    grassRef.current = grass;

    // Rocks
    const rocks = [];
    for (let i = 0; i < 8; i++) {
      rocks.push({
        x: 50 + Math.random() * (CANVAS_WIDTH - 100),
        size: 8 + Math.random() * 15,
        color: Math.random() > 0.5 ? '#555' : '#666',
      });
    }
    rocksRef.current = rocks;

    // Clouds
    const clouds = [];
    for (let i = 0; i < 5; i++) {
      clouds.push({
        x: Math.random() * CANVAS_WIDTH,
        y: 30 + Math.random() * 80,
        size: 30 + Math.random() * 40,
        speed: 0.1 + Math.random() * 0.2,
      });
    }
    cloudsRef.current = clouds;
  }, []);

  // Generate terrain
  const generateTerrain = useCallback(() => {
    const terrain = [];
    const baseHeight = CANVAS_HEIGHT * 0.58;
    const amplitude = 90;
    const frequency = 0.04;

    // Generate hills using multiple sine waves
    for (let i = 0; i <= TERRAIN_SEGMENTS; i++) {
      const x = (i / TERRAIN_SEGMENTS) * CANVAS_WIDTH;
      const noise1 = Math.sin(i * frequency * 2.5) * 35;
      const noise2 = Math.sin(i * frequency * 5.5 + 1.5) * 18;
      const noise3 = Math.sin(i * frequency * 1.2) * amplitude;
      const y = baseHeight + noise1 + noise2 + noise3;
      terrain.push({ x, y: Math.min(CANVAS_HEIGHT - 25, Math.max(180, y)) });
    }

    terrainRef.current = terrain;

    // Position tanks on terrain - player on left, CPU on right (opposite sides)
    // Tanks positioned at edges for maximum distance between them
    const playerX = 17;
    const cpuX = CANVAS_WIDTH - 17;

    playerTankRef.current = {
      x: playerX,
      y: getTerrainHeightAt(playerX, terrain) - TANK_HEIGHT / 2,
      recoil: 0,
    };

    cpuTankRef.current = {
      x: cpuX,
      y: getTerrainHeightAt(cpuX, terrain) - TANK_HEIGHT / 2,
      aimAngle: 45,
      aimPower: 50,
      recoil: 0,
    };

    // Set random wind
    setWind((Math.random() - 0.5) * 2.5);

    generateDecorations();
  }, [generateDecorations]);

  const getTerrainHeightAt = (x, terrain = terrainRef.current) => {
    if (!terrain || terrain.length === 0) return CANVAS_HEIGHT * 0.6;

    const segmentWidth = CANVAS_WIDTH / TERRAIN_SEGMENTS;
    const index = Math.floor(x / segmentWidth);
    const clampedIndex = Math.max(0, Math.min(terrain.length - 2, index));

    const p1 = terrain[clampedIndex];
    const p2 = terrain[clampedIndex + 1];

    if (!p1 || !p2) return CANVAS_HEIGHT * 0.6;

    const t = (x - p1.x) / (p2.x - p1.x);
    return p1.y + (p2.y - p1.y) * t;
  };

  // Create particles
  const createParticles = useCallback((x, y, type, count = 10) => {
    const colors = {
      explosion: ['#ff6b6b', '#ff9500', '#ffd700', '#ff4757'],
      spark: ['#ffd700', '#fff', '#ff9500'],
      smoke: ['#555', '#777', '#999', '#aaa'],
      debris: ['#654321', '#8B4513', '#556B2F', '#4a7c3f'],
      muzzle: ['#ff9500', '#ffd700', '#fff'],
    };

    for (let i = 0; i < count; i++) {
      const angle = (Math.random() * Math.PI * 2);
      const speed = 1 + Math.random() * 4;
      const color = colors[type][Math.floor(Math.random() * colors[type].length)];

      particlesRef.current.push({
        x, y,
        vx: Math.cos(angle) * speed * (type === 'muzzle' ? 2 : 1),
        vy: Math.sin(angle) * speed * (type === 'muzzle' ? 0.5 : 1) - (type === 'smoke' ? 1 : 0),
        radius: type === 'smoke' ? 4 + Math.random() * 6 : 2 + Math.random() * 4,
        color,
        alpha: 1,
        decay: type === 'smoke' ? 0.015 : 0.03 + Math.random() * 0.02,
        gravity: type === 'smoke' ? -0.02 : type === 'debris' ? 0.15 : 0.08,
        type,
      });
    }
  }, []);

  // Fire bullet
  const fireBullet = useCallback((fromPlayer, weaponKey = selectedWeapon) => {
    const tank = fromPlayer ? playerTankRef.current : cpuTankRef.current;
    const fireAngle = fromPlayer ? angle : cpuTankRef.current.aimAngle || 45;
    const firePower = fromPlayer ? power : cpuTankRef.current.aimPower || 50;
    const weapon = WEAPONS[weaponKey];

    // Player tank fires to the RIGHT (positive X), CPU fires to the LEFT (negative X)
    // Player: angle 0-90 means firing right-upward, so radians = angle directly
    // CPU: angle 0-90 means firing left-upward, so radians = 180 - angle
    const radians = (fromPlayer ? fireAngle : 180 - fireAngle) * (Math.PI / 180);
    const velocity = firePower * 0.16 * weapon.speed;

    const barrelEndX = tank.x + TANK_WIDTH / 2 + Math.cos(radians) * 30;
    const barrelEndY = tank.y - TANK_HEIGHT / 2 - Math.sin(radians) * 30;

    bulletsRef.current.push({
      x: barrelEndX,
      y: barrelEndY,
      vx: Math.cos(radians) * velocity,
      vy: -Math.sin(radians) * velocity,
      bounces: 0,
      fromPlayer,
      weapon: weaponKey,
      trail: [],
    });

    // Recoil effect
    if (fromPlayer) {
      playerTankRef.current.recoil = 8;
    } else {
      cpuTankRef.current.recoil = 8;
    }

    // Muzzle flash particles
    createParticles(barrelEndX, barrelEndY, 'muzzle', 12);

    // Screen shake
    setScreenShake(5);
  }, [angle, power, selectedWeapon, createParticles]);

  // CPU AI - calculate shot
  const cpuCalculateShot = useCallback(() => {
    const cpuTank = cpuTankRef.current;
    const playerTank = playerTankRef.current;

    const dx = playerTank.x - cpuTank.x;
    const dy = playerTank.y - cpuTank.y;
    const distance = Math.sqrt(dx * dx + dy * dy);

    // Difficulty scales with player health
    const baseDifficulty = 0.65;
    const healthFactor = (100 - playerHealthRef.current) / 200;
    const accuracy = baseDifficulty + healthFactor + Math.random() * 0.25;

    const errorAngle = (Math.random() - 0.5) * 25 * (1 - accuracy);
    const errorPower = (Math.random() - 0.5) * 18 * (1 - accuracy);

    let optimalAngle = Math.atan2(-dy, -dx) * (180 / Math.PI);
    optimalAngle = Math.max(15, Math.min(75, optimalAngle + 22));

    let optimalPower = Math.min(100, Math.max(35, distance * 0.38));
    optimalPower += wind * 12;

    // CPU weapon selection
    const weapons = Object.keys(WEAPONS);
    const cpuWeapon = weapons[Math.floor(Math.random() * weapons.length)];

    cpuTankRef.current.aimAngle = optimalAngle + errorAngle;
    cpuTankRef.current.aimPower = Math.max(25, Math.min(100, optimalPower + errorPower));
    cpuTankRef.current.weapon = cpuWeapon;
  }, [wind]);

  // CPU turn
  const executeCpuTurn = useCallback(() => {
    if (gameStateRef.current !== 'playing') return;

    setMessage('CPU is aiming...');

    cpuCalculateShot();

    setTimeout(() => {
      if (gameStateRef.current !== 'playing') return;
      const cpuWeapon = cpuTankRef.current.weapon || 'standard';
      setMessage(`CPU fires ${WEAPONS[cpuWeapon].emoji}!`);
      fireBullet(false, cpuWeapon);
    }, 1200);
  }, [cpuCalculateShot, fireBullet]);

  // Handle player fire
  const handleFire = useCallback(() => {
    if (currentTurnRef.current !== 'player' || bulletsRef.current.length > 0) return;
    setMessage('');
    setIsAiming(false);
    setTouchPos(null);
    fireBullet(true);
  }, [fireBullet]);

  // Keep refs in sync with state
  useEffect(() => {
    angleRef.current = angle;
  }, [angle]);

  useEffect(() => {
    powerRef.current = power;
  }, [power]);

  // Touch/Mouse handling for aiming - Apple-style controls
  const getCanvasCoords = useCallback((clientX, clientY) => {
    const canvas = canvasRef.current;
    if (!canvas) return null;
    const rect = canvas.getBoundingClientRect();
    const scaleX = CANVAS_WIDTH / rect.width;
    const scaleY = CANVAS_HEIGHT / rect.height;
    return {
      x: (clientX - rect.left) * scaleX,
      y: (clientY - rect.top) * scaleY
    };
  }, []);

  const handleTouchStart = useCallback((e) => {
    if (currentTurnRef.current !== 'player' || bulletsRef.current.length > 0) return;
    if (gameStateRef.current !== 'playing') return;

    e.preventDefault();
    const touch = e.touches ? e.touches[0] : e;
    const coords = getCanvasCoords(touch.clientX, touch.clientY);
    if (!coords) return;

    touchStartRef.current = {
      x: coords.x,
      y: coords.y,
      startAngle: angleRef.current,
      startPower: powerRef.current,
      time: Date.now()
    };
    setIsAiming(true);
    setTouchPos(coords);
  }, [getCanvasCoords]);

  const handleTouchMove = useCallback((e) => {
    if (!touchStartRef.current || !isAiming) return;
    if (currentTurnRef.current !== 'player' || bulletsRef.current.length > 0) return;

    e.preventDefault();
    const touch = e.touches ? e.touches[0] : e;
    const coords = getCanvasCoords(touch.clientX, touch.clientY);
    if (!coords) return;

    setTouchPos(coords);

    const tank = playerTankRef.current;
    const tankCenterX = tank.x + TANK_WIDTH / 2;
    const tankCenterY = tank.y - TANK_HEIGHT / 2;

    // Calculate angle from tank to touch point
    const dx = coords.x - tankCenterX;
    const dy = tankCenterY - coords.y; // Inverted because canvas Y is down

    // Calculate angle (0-90 degrees for firing to the right/up)
    // Player fires to the RIGHT (positive dx), so only consider positive angles
    let newAngle = Math.atan2(dy, Math.abs(dx)) * (180 / Math.PI);
    // Clamp angle between 10 and 80 degrees
    newAngle = Math.max(10, Math.min(80, newAngle));

    // Calculate power based on distance from tank
    const distance = Math.sqrt(dx * dx + dy * dy);
    const maxDistance = 200; // Max distance for 100% power
    let newPower = Math.min(100, Math.max(10, (distance / maxDistance) * 100));

    setAngle(Math.round(newAngle));
    setPower(Math.round(newPower));
  }, [isAiming, getCanvasCoords]);

  const handleTouchEnd = useCallback((e) => {
    if (!touchStartRef.current) return;

    const touchDuration = Date.now() - touchStartRef.current.time;
    const coords = touchPos;

    // If it was a quick tap (not a drag), fire!
    if (touchDuration < 200 && coords) {
      const startCoords = touchStartRef.current;
      const dx = Math.abs(coords.x - startCoords.x);
      const dy = Math.abs(coords.y - startCoords.y);

      // If didn't move much, it's a tap - fire!
      if (dx < 15 && dy < 15) {
        handleFire();
      }
    }

    touchStartRef.current = null;
    setIsAiming(false);
    setTouchPos(null);
  }, [touchPos, handleFire]);

  // Check bullet collision
  const checkBulletCollision = useCallback((bullet) => {
    const terrainY = getTerrainHeightAt(bullet.x);
    if (bullet.y >= terrainY) {
      return { hit: true, type: 'terrain', x: bullet.x, y: terrainY };
    }

    if (bullet.x <= 0 || bullet.x >= CANVAS_WIDTH) {
      if (bullet.bounces < 2) {
        bullet.vx *= -0.75;
        bullet.x = bullet.x <= 0 ? 2 : CANVAS_WIDTH - 2;
        bullet.bounces++;
        return { hit: false, bounced: true };
      }
      return { hit: true, type: 'wall' };
    }

    const playerTank = playerTankRef.current;
    const cpuTank = cpuTankRef.current;

    if (!bullet.fromPlayer) {
      const hitRadius = TANK_WIDTH / 2 + 8;
      const tankCenterX = playerTank.x + TANK_WIDTH / 2;
      const tankCenterY = playerTank.y;
      const dist = Math.sqrt((bullet.x - tankCenterX) ** 2 + (bullet.y - tankCenterY) ** 2);
      if (dist < hitRadius) {
        return { hit: true, type: 'player', x: bullet.x, y: bullet.y };
      }
    } else {
      const hitRadius = TANK_WIDTH / 2 + 8;
      const tankCenterX = cpuTank.x + TANK_WIDTH / 2;
      const tankCenterY = cpuTank.y;
      const dist = Math.sqrt((bullet.x - tankCenterX) ** 2 + (bullet.y - tankCenterY) ** 2);
      if (dist < hitRadius) {
        return { hit: true, type: 'cpu', x: bullet.x, y: bullet.y };
      }
    }

    if (bullet.y > CANVAS_HEIGHT || bullet.y < -150) {
      return { hit: true, type: 'oob' };
    }

    return { hit: false };
  }, []);

  // Create explosion
  const createExplosion = useCallback((x, y, weapon) => {
    const weaponData = WEAPONS[weapon];
    explosionsRef.current.push({
      x, y,
      radius: 0,
      maxRadius: weaponData.explosionSize,
      alpha: 1,
      color: weaponData.color,
    });

    // Particles
    createParticles(x, y, 'explosion', 20);
    createParticles(x, y, 'smoke', 8);
    createParticles(x, y, 'debris', 10);

    // Screen shake based on explosion size
    setScreenShake(Math.min(15, weaponData.explosionSize / 4));
  }, [createParticles]);

  // Deform terrain
  const deformTerrain = useCallback((x, radius) => {
    const terrain = terrainRef.current;

    terrain.forEach((point) => {
      const distance = Math.abs(point.x - x);
      if (distance < radius) {
        const deformation = (1 - distance / radius) * radius * 0.55;
        point.y = Math.min(CANVAS_HEIGHT - 15, point.y + deformation);
      }
    });

    playerTankRef.current.y = getTerrainHeightAt(playerTankRef.current.x) - TANK_HEIGHT / 2;
    cpuTankRef.current.y = getTerrainHeightAt(cpuTankRef.current.x) - TANK_HEIGHT / 2;
  }, []);

  // Spawn cluster bombs
  const spawnClusterBombs = useCallback((x, y, fromPlayer) => {
    const weapon = WEAPONS.cluster;
    for (let i = 0; i < weapon.clusterCount; i++) {
      const spread = (i - weapon.clusterCount / 2) * 0.5;
      bulletsRef.current.push({
        x,
        y: y - 10,
        vx: spread + (Math.random() - 0.5) * 2,
        vy: -3 - Math.random() * 2,
        bounces: 0,
        fromPlayer,
        weapon: 'standard',
        isCluster: true,
        trail: [],
      });
    }
  }, []);

  // Handle hit
  const handleHit = useCallback((collision, bullet) => {
    const weapon = WEAPONS[bullet.weapon];

    createExplosion(collision.x, collision.y, bullet.weapon);
    deformTerrain(collision.x, weapon.deformRadius);

    // Cluster bomb effect
    if (weapon.cluster && collision.type === 'terrain') {
      spawnClusterBombs(collision.x, collision.y, bullet.fromPlayer);
    }

    let damage = 0;
    if (collision.type === 'player') {
      damage = weapon.damage[0] + Math.floor(Math.random() * (weapon.damage[1] - weapon.damage[0]));
      playerHealthRef.current = Math.max(0, playerHealthRef.current - damage);
      setPlayerHealth(playerHealthRef.current);
      setMessage(`You got hit! -${damage} HP`);
      streakRef.current = 0;
      setStreak(0);
    } else if (collision.type === 'cpu') {
      damage = weapon.damage[0] + Math.floor(Math.random() * (weapon.damage[1] - weapon.damage[0]));

      // Streak bonus
      streakRef.current++;
      const streakBonus = Math.min(streakRef.current - 1, 3) * 5;
      damage += streakBonus;

      cpuHealthRef.current = Math.max(0, cpuHealthRef.current - damage);
      setCpuHealth(cpuHealthRef.current);
      setMessage(`Direct hit! -${damage} HP${streakBonus > 0 ? ` (+${streakBonus} streak)` : ''}`);

      setStreak(streakRef.current);
      if (streakRef.current >= 2) {
        setShowStreak(true);
        setTimeout(() => setShowStreak(false), 1500);
      }
    } else {
      setMessage(collision.type === 'terrain' ? 'Missed!' : '');
      if (bullet.fromPlayer && collision.type === 'terrain') {
        streakRef.current = 0;
        setStreak(0);
      }
    }

    // Remove this bullet
    const idx = bulletsRef.current.indexOf(bullet);
    if (idx > -1) bulletsRef.current.splice(idx, 1);

    // Check for game end
    if (playerHealthRef.current <= 0 || cpuHealthRef.current <= 0) {
      setTimeout(() => {
        setGameState('ended');
      }, 1000);
      return true;
    }

    return false;
  }, [createExplosion, deformTerrain, spawnClusterBombs]);

  // Switch turns after all bullets are gone
  const switchTurn = useCallback(() => {
    if (gameStateRef.current !== 'playing') return;

    setWind((Math.random() - 0.5) * 2.5);

    if (currentTurnRef.current === 'player') {
      setCurrentTurn('cpu');
      setTimeout(() => executeCpuTurn(), 600);
    } else {
      setCurrentTurn('player');
      setMessage('Your turn!');
    }
  }, [executeCpuTurn]);

  // Game loop
  const gameLoop = useCallback((timestamp) => {
    if (gameStateRef.current !== 'playing' && gameStateRef.current !== 'countdown') {
      return;
    }

    const deltaTime = Math.min(timestamp - lastTimeRef.current, 50) / FRAME_TIME;
    lastTimeRef.current = timestamp;

    const canvas = canvasRef.current;
    if (!canvas) {
      animationFrameRef.current = requestAnimationFrame(gameLoop);
      return;
    }

    const ctx = canvas.getContext('2d');
    const dpr = dprRef.current;

    // Reset transform and apply DPR scaling
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.scale(dpr, dpr);

    // Apply screen shake
    if (screenShake > 0) {
      const shakeX = (Math.random() - 0.5) * screenShake;
      const shakeY = (Math.random() - 0.5) * screenShake;
      ctx.translate(shakeX, shakeY);
      setScreenShake(s => Math.max(0, s - 0.5));
    }

    // Clear canvas
    ctx.clearRect(-10, -10, CANVAS_WIDTH + 20, CANVAS_HEIGHT + 20);

    // Draw sky gradient
    const skyGradient = ctx.createLinearGradient(0, 0, 0, CANVAS_HEIGHT);
    skyGradient.addColorStop(0, '#0f1a2e');
    skyGradient.addColorStop(0.3, '#1a2a4a');
    skyGradient.addColorStop(0.6, '#2d3d5a');
    skyGradient.addColorStop(1, '#4a5a7a');
    ctx.fillStyle = skyGradient;
    ctx.fillRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);

    // Draw stars with twinkling
    ctx.fillStyle = 'rgba(255, 255, 255, 0.6)';
    for (let i = 0; i < 60; i++) {
      const x = (i * 41 + 7) % CANVAS_WIDTH;
      const y = (i * 29 + 3) % (CANVAS_HEIGHT * 0.35);
      const twinkle = 0.4 + Math.sin(timestamp / 500 + i) * 0.4;
      const size = ((i % 3) + 1) * twinkle;
      ctx.globalAlpha = twinkle;
      ctx.beginPath();
      ctx.arc(x, y, size, 0, Math.PI * 2);
      ctx.fill();
    }
    ctx.globalAlpha = 1;

    // Draw moon
    ctx.fillStyle = '#f0e68c';
    ctx.beginPath();
    ctx.arc(CANVAS_WIDTH - 60, 50, 25, 0, Math.PI * 2);
    ctx.fill();
    ctx.fillStyle = '#e6dc82';
    ctx.beginPath();
    ctx.arc(CANVAS_WIDTH - 55, 45, 22, 0, Math.PI * 2);
    ctx.fill();

    // Draw clouds
    cloudsRef.current.forEach(cloud => {
      cloud.x += cloud.speed * deltaTime;
      if (cloud.x > CANVAS_WIDTH + cloud.size) cloud.x = -cloud.size;

      ctx.fillStyle = 'rgba(200, 210, 230, 0.15)';
      ctx.beginPath();
      ctx.arc(cloud.x, cloud.y, cloud.size * 0.5, 0, Math.PI * 2);
      ctx.arc(cloud.x + cloud.size * 0.35, cloud.y - 5, cloud.size * 0.4, 0, Math.PI * 2);
      ctx.arc(cloud.x + cloud.size * 0.6, cloud.y, cloud.size * 0.35, 0, Math.PI * 2);
      ctx.fill();
    });

    // Draw terrain
    const terrain = terrainRef.current;
    if (terrain.length > 0) {
      ctx.beginPath();
      ctx.moveTo(0, CANVAS_HEIGHT);
      ctx.lineTo(terrain[0].x, terrain[0].y);

      for (let i = 1; i < terrain.length; i++) {
        ctx.lineTo(terrain[i].x, terrain[i].y);
      }

      ctx.lineTo(CANVAS_WIDTH, CANVAS_HEIGHT);
      ctx.closePath();

      const terrainGradient = ctx.createLinearGradient(0, CANVAS_HEIGHT * 0.35, 0, CANVAS_HEIGHT);
      terrainGradient.addColorStop(0, '#4a8c3f');
      terrainGradient.addColorStop(0.15, '#3d7534');
      terrainGradient.addColorStop(0.5, '#2d5a26');
      terrainGradient.addColorStop(1, '#1a3a18');
      ctx.fillStyle = terrainGradient;
      ctx.fill();

      // Terrain highlights
      ctx.strokeStyle = 'rgba(120, 180, 100, 0.5)';
      ctx.lineWidth = 2;
      ctx.stroke();
    }

    // Draw rocks
    rocksRef.current.forEach(rock => {
      const terrainY = getTerrainHeightAt(rock.x);
      ctx.fillStyle = rock.color;
      ctx.beginPath();
      ctx.ellipse(rock.x, terrainY - rock.size * 0.3, rock.size, rock.size * 0.6, 0, 0, Math.PI * 2);
      ctx.fill();
      ctx.fillStyle = 'rgba(0,0,0,0.2)';
      ctx.beginPath();
      ctx.ellipse(rock.x + 2, terrainY - rock.size * 0.1, rock.size * 0.9, rock.size * 0.3, 0, 0, Math.PI * 2);
      ctx.fill();
    });

    // Draw grass
    grassRef.current.forEach(grass => {
      const terrainY = getTerrainHeightAt(grass.x);
      const sway = Math.sin(timestamp / 800 + grass.sway) * 3;

      ctx.strokeStyle = '#5a9c4f';
      ctx.lineWidth = 2;
      ctx.lineCap = 'round';

      for (let b = 0; b < grass.blades; b++) {
        const bladeX = grass.x + (b - grass.blades / 2) * 4;
        const bendFactor = (b - grass.blades / 2) * 0.3;

        ctx.beginPath();
        ctx.moveTo(bladeX, terrainY);
        ctx.quadraticCurveTo(
          bladeX + sway + bendFactor,
          terrainY - grass.height * 0.6,
          bladeX + sway * 1.5 + bendFactor * 2,
          terrainY - grass.height
        );
        ctx.stroke();
      }
    });

    // Draw tanks
    const drawTank = (tank, isPlayer) => {
      const x = tank.x - (tank.recoil || 0) * (isPlayer ? 1 : -1);
      const y = tank.y;

      // Update recoil
      if (tank.recoil > 0) {
        tank.recoil = Math.max(0, tank.recoil - 0.5 * deltaTime);
      }

      ctx.save();
      ctx.translate(x + TANK_WIDTH / 2, y);

      // Tank shadow
      ctx.fillStyle = 'rgba(0, 0, 0, 0.3)';
      ctx.beginPath();
      ctx.ellipse(2, TANK_HEIGHT / 2 + 2, TANK_WIDTH / 2, 6, 0, 0, Math.PI * 2);
      ctx.fill();

      // Tank tracks
      ctx.fillStyle = '#222';
      ctx.beginPath();
      ctx.roundRect(-TANK_WIDTH / 2 - 2, TANK_HEIGHT / 2 - 10, TANK_WIDTH + 4, 12, 4);
      ctx.fill();

      // Track details
      ctx.strokeStyle = '#444';
      ctx.lineWidth = 1;
      for (let i = -TANK_WIDTH / 2; i < TANK_WIDTH / 2; i += 6) {
        ctx.beginPath();
        ctx.moveTo(i, TANK_HEIGHT / 2 - 8);
        ctx.lineTo(i, TANK_HEIGHT / 2);
        ctx.stroke();
      }

      // Tank body
      const tankGradient = ctx.createLinearGradient(0, -TANK_HEIGHT, 0, TANK_HEIGHT / 2);
      if (isPlayer) {
        tankGradient.addColorStop(0, '#5ab0ff');
        tankGradient.addColorStop(0.5, '#3a90df');
        tankGradient.addColorStop(1, '#2a6faf');
      } else {
        tankGradient.addColorStop(0, '#ff7b7b');
        tankGradient.addColorStop(0.5, '#df4a4a');
        tankGradient.addColorStop(1, '#af3a3a');
      }

      ctx.fillStyle = tankGradient;
      ctx.beginPath();
      ctx.roundRect(-TANK_WIDTH / 2, -TANK_HEIGHT / 2 - 4, TANK_WIDTH, TANK_HEIGHT - 4, 6);
      ctx.fill();

      // Body highlight
      ctx.fillStyle = isPlayer ? 'rgba(150, 200, 255, 0.3)' : 'rgba(255, 200, 200, 0.3)';
      ctx.beginPath();
      ctx.roundRect(-TANK_WIDTH / 2 + 4, -TANK_HEIGHT / 2, TANK_WIDTH - 8, TANK_HEIGHT / 3, 3);
      ctx.fill();

      // Tank turret
      ctx.fillStyle = isPlayer ? '#4a9fef' : '#ef5a5a';
      ctx.beginPath();
      ctx.arc(0, -TANK_HEIGHT / 2 - 4, 12, 0, Math.PI * 2);
      ctx.fill();

      // Turret highlight
      ctx.fillStyle = isPlayer ? 'rgba(150, 200, 255, 0.4)' : 'rgba(255, 200, 200, 0.4)';
      ctx.beginPath();
      ctx.arc(-2, -TANK_HEIGHT / 2 - 6, 5, 0, Math.PI * 2);
      ctx.fill();

      // Tank barrel
      // Player barrel points RIGHT (positive X), CPU barrel points LEFT (negative X)
      const barrelAngle = isPlayer
        ? angle * (Math.PI / 180)  // Player fires right, angle goes up from horizontal
        : (180 - (tank.aimAngle || 45)) * (Math.PI / 180);  // CPU fires left

      const barrelLength = 28;
      const barrelEndX = Math.cos(barrelAngle) * barrelLength;
      const barrelEndY = -TANK_HEIGHT / 2 - 4 - Math.sin(barrelAngle) * barrelLength;

      // Barrel shadow
      ctx.strokeStyle = 'rgba(0,0,0,0.3)';
      ctx.lineWidth = 10;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(2, -TANK_HEIGHT / 2 - 2);
      ctx.lineTo(barrelEndX + 2, barrelEndY + 2);
      ctx.stroke();

      // Barrel
      ctx.strokeStyle = isPlayer ? '#2a6f9f' : '#9f3a3a';
      ctx.lineWidth = 8;
      ctx.beginPath();
      ctx.moveTo(0, -TANK_HEIGHT / 2 - 4);
      ctx.lineTo(barrelEndX, barrelEndY);
      ctx.stroke();

      // Barrel highlight
      ctx.strokeStyle = isPlayer ? '#5ab0ff' : '#ff7b7b';
      ctx.lineWidth = 4;
      ctx.beginPath();
      ctx.moveTo(0, -TANK_HEIGHT / 2 - 6);
      ctx.lineTo(barrelEndX * 0.7, barrelEndY + 2);
      ctx.stroke();

      // Barrel tip
      ctx.fillStyle = '#333';
      ctx.beginPath();
      ctx.arc(barrelEndX, barrelEndY, 5, 0, Math.PI * 2);
      ctx.fill();

      // Wheels
      ctx.fillStyle = '#333';
      for (let i = -1; i <= 1; i++) {
        ctx.beginPath();
        ctx.arc(i * 14, TANK_HEIGHT / 2 - 4, 7, 0, Math.PI * 2);
        ctx.fill();
        ctx.fillStyle = '#555';
        ctx.beginPath();
        ctx.arc(i * 14, TANK_HEIGHT / 2 - 4, 4, 0, Math.PI * 2);
        ctx.fill();
        ctx.fillStyle = '#333';
      }

      ctx.restore();
    };

    drawTank(playerTankRef.current, true);
    drawTank(cpuTankRef.current, false);

    // Update and draw bullets
    let bulletsProcessed = false;
    bulletsRef.current = bulletsRef.current.filter(bullet => {
      const weapon = WEAPONS[bullet.weapon];

      // Apply physics
      bullet.vy += GRAVITY * deltaTime;
      bullet.vx += wind * 0.025 * deltaTime;
      bullet.x += bullet.vx * deltaTime;
      bullet.y += bullet.vy * deltaTime;

      // Update trail
      bullet.trail.push({ x: bullet.x, y: bullet.y, alpha: 1 });
      if (bullet.trail.length > 15) bullet.trail.shift();

      // Check collision
      const collision = checkBulletCollision(bullet);
      if (collision.hit) {
        const gameEnded = handleHit(collision, bullet);
        bulletsProcessed = true;
        return !gameEnded && false;
      }

      // Draw trail
      ctx.lineWidth = 3;
      bullet.trail.forEach((pt, i) => {
        pt.alpha -= 0.07;
        if (pt.alpha > 0 && i > 0) {
          const prev = bullet.trail[i - 1];
          ctx.strokeStyle = weapon.trailColor.replace('0.6', pt.alpha.toFixed(2));
          ctx.beginPath();
          ctx.moveTo(prev.x, prev.y);
          ctx.lineTo(pt.x, pt.y);
          ctx.stroke();
        }
      });

      // Draw bullet with glow
      ctx.shadowColor = weapon.color;
      ctx.shadowBlur = 15;
      ctx.fillStyle = weapon.color;
      ctx.beginPath();
      ctx.arc(bullet.x, bullet.y, weapon.radius, 0, Math.PI * 2);
      ctx.fill();

      // Inner glow
      ctx.fillStyle = '#fff';
      ctx.beginPath();
      ctx.arc(bullet.x - 1, bullet.y - 1, weapon.radius * 0.4, 0, Math.PI * 2);
      ctx.fill();
      ctx.shadowBlur = 0;

      return true;
    });

    // Switch turn when all bullets are gone
    if (bulletsProcessed && bulletsRef.current.length === 0) {
      setTimeout(() => switchTurn(), 1200);
    }

    // Update and draw explosions
    explosionsRef.current = explosionsRef.current.filter(explosion => {
      explosion.radius += 4 * deltaTime;
      explosion.alpha -= 0.04 * deltaTime;

      if (explosion.alpha <= 0) return false;

      const gradient = ctx.createRadialGradient(
        explosion.x, explosion.y, 0,
        explosion.x, explosion.y, explosion.radius
      );
      gradient.addColorStop(0, `rgba(255, 255, 200, ${explosion.alpha})`);
      gradient.addColorStop(0.3, `rgba(255, 150, 50, ${explosion.alpha * 0.9})`);
      gradient.addColorStop(0.6, `rgba(255, 80, 30, ${explosion.alpha * 0.6})`);
      gradient.addColorStop(1, `rgba(100, 30, 30, 0)`);

      ctx.fillStyle = gradient;
      ctx.beginPath();
      ctx.arc(explosion.x, explosion.y, explosion.radius, 0, Math.PI * 2);
      ctx.fill();

      return true;
    });

    // Update and draw particles
    particlesRef.current = particlesRef.current.filter(p => {
      p.vy += p.gravity * deltaTime;
      p.x += p.vx * deltaTime;
      p.y += p.vy * deltaTime;
      p.alpha -= p.decay * deltaTime;

      if (p.alpha <= 0) return false;

      ctx.globalAlpha = p.alpha;
      ctx.fillStyle = p.color;
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.radius, 0, Math.PI * 2);
      ctx.fill();
      ctx.globalAlpha = 1;

      return true;
    });

    // Draw wind indicator
    const windArrowX = CANVAS_WIDTH / 2;
    const windArrowY = 25;

    ctx.fillStyle = 'rgba(0, 0, 0, 0.5)';
    ctx.beginPath();
    ctx.roundRect(windArrowX - 55, windArrowY - 12, 110, 24, 12);
    ctx.fill();

    ctx.fillStyle = 'rgba(255, 255, 255, 0.9)';
    ctx.font = 'bold 13px Arial';
    ctx.textAlign = 'center';
    ctx.fillText('Wind', windArrowX - 25, windArrowY + 4);

    // Wind arrow
    const windStrength = Math.abs(wind);
    const windDir = wind > 0 ? 1 : -1;
    ctx.strokeStyle = windStrength > 1.5 ? '#ff6b6b' : windStrength > 0.8 ? '#ffd700' : '#5a9c4f';
    ctx.lineWidth = 3;
    ctx.lineCap = 'round';

    const arrowStartX = windArrowX + 10;
    const arrowLength = windStrength * 20;

    ctx.beginPath();
    ctx.moveTo(arrowStartX, windArrowY);
    ctx.lineTo(arrowStartX + arrowLength * windDir, windArrowY);
    ctx.stroke();

    // Arrow head
    ctx.beginPath();
    ctx.moveTo(arrowStartX + arrowLength * windDir, windArrowY);
    ctx.lineTo(arrowStartX + (arrowLength - 6) * windDir, windArrowY - 4);
    ctx.lineTo(arrowStartX + (arrowLength - 6) * windDir, windArrowY + 4);
    ctx.closePath();
    ctx.fillStyle = ctx.strokeStyle;
    ctx.fill();

    // Draw trajectory preview for player
    if (currentTurnRef.current === 'player' && bulletsRef.current.length === 0) {
      const tank = playerTankRef.current;
      const radians = angle * (Math.PI / 180);  // Player fires right
      const velocity = power * 0.16 * WEAPONS[selectedWeapon].speed;

      let px = tank.x + TANK_WIDTH / 2 + Math.cos(radians) * 30;
      let py = tank.y - TANK_HEIGHT / 2 - Math.sin(radians) * 30;
      let pvx = Math.cos(radians) * velocity;
      let pvy = -Math.sin(radians) * velocity;

      ctx.setLineDash([4, 6]);
      ctx.lineWidth = 2;

      for (let i = 0; i < 40; i++) {
        const alpha = 0.5 - (i / 40) * 0.4;
        ctx.strokeStyle = `rgba(74, 159, 255, ${alpha})`;

        const nextPvy = pvy + GRAVITY;
        const nextPvx = pvx + wind * 0.025;
        const nextPx = px + nextPvx;
        const nextPy = py + nextPvy;

        ctx.beginPath();
        ctx.moveTo(px, py);
        ctx.lineTo(nextPx, nextPy);
        ctx.stroke();

        if (nextPy > getTerrainHeightAt(nextPx) || nextPx < 0 || nextPx > CANVAS_WIDTH) break;

        px = nextPx;
        py = nextPy;
        pvx = nextPvx;
        pvy = nextPvy;
      }

      ctx.setLineDash([]);
    }

    animationFrameRef.current = requestAnimationFrame(gameLoop);
  }, [angle, power, wind, selectedWeapon, checkBulletCollision, handleHit, switchTurn, screenShake]);

  // Initialize
  useEffect(() => {
    generateTerrain();

    const canvas = canvasRef.current;
    if (canvas) {
      const dpr = window.devicePixelRatio || 1;
      dprRef.current = dpr;
      canvas.width = CANVAS_WIDTH * dpr;
      canvas.height = CANVAS_HEIGHT * dpr;
      canvas.style.width = `${CANVAS_WIDTH}px`;
      canvas.style.height = `${CANVAS_HEIGHT}px`;
    }
  }, [generateTerrain]);

  // Countdown
  useEffect(() => {
    if (gameState !== 'countdown') return;

    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
      setMessage('Your turn!');
    }
  }, [gameState, countdown]);

  // Game timer
  useEffect(() => {
    if (gameState !== 'playing') return;

    const timer = setInterval(() => {
      setTimeLeft(prev => {
        if (prev <= 1) {
          setGameState('ended');
          return 0;
        }
        return prev - 1;
      });
    }, 1000);

    return () => clearInterval(timer);
  }, [gameState]);

  // Start game loop - runs for both countdown and playing states
  useEffect(() => {
    if (gameState !== 'playing' && gameState !== 'countdown') return;

    lastTimeRef.current = performance.now();
    animationFrameRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    };
  }, [gameState, gameLoop]);

  // Draw initial state during countdown
  useEffect(() => {
    if (gameState !== 'countdown') return;

    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    const dpr = dprRef.current;

    // Reset transform and apply DPR scaling
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.scale(dpr, dpr);
    ctx.clearRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);

    // Draw sky
    const skyGradient = ctx.createLinearGradient(0, 0, 0, CANVAS_HEIGHT);
    skyGradient.addColorStop(0, '#0f1a2e');
    skyGradient.addColorStop(0.3, '#1a2a4a');
    skyGradient.addColorStop(0.6, '#2d3d5a');
    skyGradient.addColorStop(1, '#4a5a7a');
    ctx.fillStyle = skyGradient;
    ctx.fillRect(0, 0, CANVAS_WIDTH, CANVAS_HEIGHT);

    // Draw terrain
    const terrain = terrainRef.current;
    if (terrain.length > 0) {
      ctx.beginPath();
      ctx.moveTo(0, CANVAS_HEIGHT);
      ctx.lineTo(terrain[0].x, terrain[0].y);

      for (let i = 1; i < terrain.length; i++) {
        ctx.lineTo(terrain[i].x, terrain[i].y);
      }

      ctx.lineTo(CANVAS_WIDTH, CANVAS_HEIGHT);
      ctx.closePath();

      const terrainGradient = ctx.createLinearGradient(0, CANVAS_HEIGHT * 0.35, 0, CANVAS_HEIGHT);
      terrainGradient.addColorStop(0, '#4a8c3f');
      terrainGradient.addColorStop(0.3, '#3d7534');
      terrainGradient.addColorStop(1, '#2d5a26');
      ctx.fillStyle = terrainGradient;
      ctx.fill();
    }

    // Draw tanks (simplified)
    ctx.fillStyle = '#4a9fff';
    ctx.fillRect(playerTankRef.current.x, playerTankRef.current.y - TANK_HEIGHT / 2, TANK_WIDTH, TANK_HEIGHT);

    ctx.fillStyle = '#ff6b6b';
    ctx.fillRect(cpuTankRef.current.x, cpuTankRef.current.y - TANK_HEIGHT / 2, TANK_WIDTH, TANK_HEIGHT);
  }, [gameState, countdown]);

  // Calculate score
  const calculateScore = () => {
    const healthBonus = playerHealth * 2;
    const damageDealt = (100 - cpuHealth) * 3;
    const timeBonus = Math.floor(timeLeft / 8);
    const winBonus = cpuHealth <= 0 ? 120 : 0;
    const streakBonus = streak * 15;

    return Math.max(0, healthBonus + damageDealt + timeBonus + winBonus + streakBonus);
  };

  const handleEnd = () => {
    const finalScore = calculateScore();
    onGameEnd(finalScore);
  };

  return (
    <motion.div
      className="tanks-game-overlay"
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
    >
      <div
        className="tanks-game-container"
        ref={gameContainerRef}
        onTouchStart={handleTouchStart}
        onTouchMove={handleTouchMove}
        onTouchEnd={handleTouchEnd}
        onMouseDown={handleTouchStart}
        onMouseMove={(e) => isAiming && handleTouchMove(e)}
        onMouseUp={handleTouchEnd}
        onMouseLeave={handleTouchEnd}
      >
        {/* Header - Compact for touch controls */}
        <div className="tanks-header">
          <div className={`tanks-player-stat ${currentTurn === 'player' ? 'active' : ''}`}>
            <span className="player-label">🎮 You</span>
            <div className="health-bar">
              <div
                className="health-fill player"
                style={{ width: `${playerHealth}%` }}
              />
            </div>
            <span className="health-text">{playerHealth}%</span>
          </div>

          <div className="tanks-stat timer">
            <span className="stat-icon">⏱️</span>
            <span className="stat-value">{timeLeft}s</span>
          </div>

          <div className={`tanks-player-stat cpu ${currentTurn === 'cpu' ? 'active' : ''}`}>
            <span className="player-label">🤖 CPU</span>
            <div className="health-bar">
              <div
                className="health-fill cpu"
                style={{ width: `${cpuHealth}%` }}
              />
            </div>
            <span className="health-text">{cpuHealth}%</span>
          </div>
        </div>

        {/* Streak indicator */}
        <AnimatePresence>
          {showStreak && streak >= 2 && (
            <motion.div
              className="tanks-streak"
              initial={{ scale: 0, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              exit={{ scale: 0, opacity: 0 }}
            >
              <span className="streak-fire">🔥</span>
              <span className="streak-count">{streak}x</span>
              <span className="streak-text">Hit Streak!</span>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Message */}
        <AnimatePresence>
          {message && (
            <motion.div
              className="tanks-message"
              initial={{ opacity: 0, y: -20 }}
              animate={{ opacity: 1, y: 0 }}
              exit={{ opacity: 0 }}
            >
              {message}
            </motion.div>
          )}
        </AnimatePresence>

        {/* Game canvas */}
        <canvas
          ref={canvasRef}
          width={CANVAS_WIDTH}
          height={CANVAS_HEIGHT}
          className="tanks-canvas"
        />

        {/* Touch-based HUD Overlay - shows on player's turn */}
        {gameState === 'playing' && currentTurn === 'player' && bulletsRef.current.length === 0 && (
          <>
            {/* Stats display at bottom */}
            <div className="tanks-touch-hud">
              <div className="touch-stat">
                <span className="touch-stat-icon">📐</span>
                <span className="touch-stat-value">{angle}°</span>
              </div>
              <div className="touch-stat">
                <span className="touch-stat-icon">💪</span>
                <span className="touch-stat-value">{power}%</span>
              </div>
              <div className="touch-stat weapon">
                <span className="touch-stat-icon">{WEAPONS[selectedWeapon].emoji}</span>
                <span className="touch-stat-value">{WEAPONS[selectedWeapon].name}</span>
              </div>
            </div>

            {/* Weapon selector - compact */}
            <div className="tanks-weapon-bar">
              {Object.entries(WEAPONS).map(([key, weapon]) => (
                <button
                  key={key}
                  className={`weapon-pill ${selectedWeapon === key ? 'selected' : ''}`}
                  onClick={(e) => { e.stopPropagation(); setSelectedWeapon(key); }}
                  onTouchEnd={(e) => { e.stopPropagation(); setSelectedWeapon(key); }}
                  style={{ '--weapon-color': weapon.color }}
                >
                  {weapon.emoji}
                </button>
              ))}
            </div>

            {/* Touch hint */}
            {!isAiming && (
              <motion.div
                className="touch-hint"
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={{ delay: 0.5 }}
              >
                <span>👆 Drag to aim • Tap to fire</span>
              </motion.div>
            )}

            {/* Aiming indicator when dragging */}
            {isAiming && touchPos && (
              <div
                className="aim-indicator"
                style={{
                  left: touchPos.x,
                  top: touchPos.y,
                }}
              >
                <div className="aim-crosshair" />
              </div>
            )}

            {/* Fire button - floating */}
            <motion.button
              className="fire-button-floating"
              onClick={(e) => { e.stopPropagation(); handleFire(); }}
              onTouchEnd={(e) => { e.stopPropagation(); handleFire(); }}
              whileTap={{ scale: 0.9 }}
            >
              🔥
            </motion.button>
          </>
        )}

        {/* Countdown */}
        <AnimatePresence>
          {gameState === 'countdown' && (
            <motion.div
              className="tanks-countdown"
              initial={{ scale: 0 }}
              animate={{ scale: 1 }}
              exit={{ scale: 0, opacity: 0 }}
            >
              <div className="countdown-number">{countdown || 'FIRE!'}</div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Game Over */}
        <AnimatePresence>
          {gameState === 'ended' && (
            <motion.div
              className="tanks-gameover"
              initial={{ scale: 0, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              exit={{ scale: 0, opacity: 0 }}
            >
              <div className="gameover-content">
                <h2>{cpuHealth <= 0 ? '🏆 Victory!' : playerHealth <= 0 ? '💥 Defeated!' : '⏱️ Time Up!'}</h2>

                <div className="final-stats">
                  <div className="stat-row">
                    <span>Your Health</span>
                    <span>{playerHealth}%</span>
                  </div>
                  <div className="stat-row">
                    <span>CPU Health</span>
                    <span>{cpuHealth}%</span>
                  </div>
                  <div className="stat-row">
                    <span>Best Streak</span>
                    <span>🔥 {streak}x</span>
                  </div>
                  <div className="stat-row">
                    <span>Time Bonus</span>
                    <span>+{Math.floor(timeLeft / 8)}</span>
                  </div>
                </div>

                <div className="final-score">
                  <span className="score-label">Score</span>
                  <span className="score-value">🪙 {calculateScore()}</span>
                </div>

                <motion.button
                  className="collect-button"
                  onClick={handleEnd}
                  whileHover={{ scale: 1.05 }}
                  whileTap={{ scale: 0.95 }}
                >
                  Collect Coins!
                </motion.button>
              </div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Close button */}
        {gameState !== 'ended' && (
          <button className="tanks-close-btn" onClick={onClose}>
            ✕
          </button>
        )}
      </div>
    </motion.div>
  );
};

export default TanksGame;
