import { useState, useEffect, useCallback, useRef, useMemo } from 'react';
import './TowerDefenseGame.css';

const GAME_DURATION = 120000; // 2 minutes
const STARTING_CASH = 650;
const STARTING_LIVES = 100;

// Bloons-style tower types
const TOWERS = {
  dart: {
    name: 'Dart Monkey',
    emoji: '🐵',
    cost: 200,
    damage: 1,
    range: 120,
    fireRate: 850,
    color: '#8B4513',
    description: 'Shoots a single dart',
    projectileColor: '#654321',
    projectileSpeed: 18,
    pierce: 1,
  },
  tack: {
    name: 'Tack Shooter',
    emoji: '📌',
    cost: 280,
    damage: 1,
    range: 80,
    fireRate: 1200,
    color: '#888888',
    description: '8 directions at once',
    projectileColor: '#444444',
    projectileSpeed: 14,
    pierce: 1,
    multishot: 8,
  },
  sniper: {
    name: 'Sniper',
    emoji: '🎯',
    cost: 350,
    damage: 4,
    range: 999,
    fireRate: 1800,
    color: '#2E7D32',
    description: 'Long range, high damage',
    projectileColor: '#1B5E20',
    projectileSpeed: 50,
    pierce: 1,
  },
  bomb: {
    name: 'Bomb Tower',
    emoji: '💣',
    cost: 525,
    damage: 2,
    range: 100,
    fireRate: 1500,
    color: '#333333',
    description: 'Explosive splash damage',
    projectileColor: '#1a1a1a',
    projectileSpeed: 12,
    splash: 50,
    pierce: 20,
  },
  ice: {
    name: 'Ice Monkey',
    emoji: '❄️',
    cost: 300,
    damage: 0,
    range: 70,
    fireRate: 2500,
    color: '#00BCD4',
    description: 'Freezes nearby bloons',
    projectileColor: '#00ACC1',
    projectileSpeed: 0,
    freeze: true,
    freezeDuration: 2000,
  },
  super: {
    name: 'Super Monkey',
    emoji: '🦸',
    cost: 2500,
    damage: 1,
    range: 150,
    fireRate: 80,
    color: '#9C27B0',
    description: 'Extremely fast attacks',
    projectileColor: '#7B1FA2',
    projectileSpeed: 25,
    pierce: 1,
  },
};

// Bloon types (balloons)
const BLOONS = {
  red: { emoji: '🔴', color: '#FF0000', health: 1, speed: 2.0, reward: 1, children: null },
  blue: { emoji: '🔵', color: '#0066FF', health: 1, speed: 2.3, reward: 2, children: 'red' },
  green: { emoji: '🟢', color: '#00CC00', health: 1, speed: 2.6, reward: 3, children: 'blue' },
  yellow: { emoji: '🟡', color: '#FFD700', health: 1, speed: 3.2, reward: 4, children: 'green' },
  pink: { emoji: '🩷', color: '#FF69B4', health: 1, speed: 3.5, reward: 5, children: 'yellow' },
  black: { emoji: '⚫', color: '#1a1a1a', health: 1, speed: 2.2, reward: 11, children: 'pink', immuneExplosion: true },
  white: { emoji: '⚪', color: '#FFFFFF', health: 1, speed: 2.5, reward: 11, children: 'pink', immuneFreeze: true },
  zebra: { emoji: '🦓', color: '#666666', health: 1, speed: 2.2, reward: 23, children: 'black', immuneExplosion: true, immuneFreeze: true },
  rainbow: { emoji: '🌈', color: '#FF6B6B', health: 1, speed: 2.4, reward: 47, children: 'zebra', childCount: 2 },
  ceramic: { emoji: '🧱', color: '#B8860B', health: 10, speed: 2.0, reward: 95, children: 'rainbow', childCount: 2 },
  moab: { emoji: '🎈', color: '#0066CC', health: 200, speed: 1.0, reward: 381, children: 'ceramic', childCount: 4, size: 2.5, isBlimp: true },
};

// Wave definitions - Bloons style
const WAVES = [
  { bloons: [{ type: 'red', count: 20 }], delay: 600, spacing: 400 },
  { bloons: [{ type: 'red', count: 30 }], delay: 500, spacing: 300 },
  { bloons: [{ type: 'red', count: 15 }, { type: 'blue', count: 10 }], delay: 500, spacing: 350 },
  { bloons: [{ type: 'blue', count: 25 }], delay: 450, spacing: 300 },
  { bloons: [{ type: 'blue', count: 15 }, { type: 'green', count: 10 }], delay: 400, spacing: 280 },
  { bloons: [{ type: 'green', count: 20 }], delay: 400, spacing: 250 },
  { bloons: [{ type: 'green', count: 15 }, { type: 'yellow', count: 10 }], delay: 350, spacing: 220 },
  { bloons: [{ type: 'yellow', count: 25 }], delay: 300, spacing: 200 },
  { bloons: [{ type: 'yellow', count: 10 }, { type: 'pink', count: 15 }], delay: 300, spacing: 180 },
  { bloons: [{ type: 'pink', count: 25 }], delay: 250, spacing: 160 },
  { bloons: [{ type: 'black', count: 10 }, { type: 'white', count: 10 }], delay: 350, spacing: 250 },
  { bloons: [{ type: 'zebra', count: 10 }, { type: 'pink', count: 15 }], delay: 300, spacing: 220 },
  { bloons: [{ type: 'rainbow', count: 8 }], delay: 400, spacing: 500 },
  { bloons: [{ type: 'ceramic', count: 5 }, { type: 'rainbow', count: 5 }], delay: 500, spacing: 600 },
  { bloons: [{ type: 'moab', count: 1 }], delay: 0, spacing: 0 },
];

const TowerDefenseGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [cash, setCash] = useState(STARTING_CASH);
  const [lives, setLives] = useState(STARTING_LIVES);
  const [score, setScore] = useState(0);
  const [selectedTower, setSelectedTower] = useState(null);
  const [currentWave, setCurrentWave] = useState(0);
  const [waveInProgress, setWaveInProgress] = useState(false);
  const [selectedPlacedTower, setSelectedPlacedTower] = useState(null);
  const [totalPops, setTotalPops] = useState(0);

  const canvasRef = useRef(null);
  const gameAreaRef = useRef(null);
  const towersRef = useRef([]);
  const bloonsRef = useRef([]);
  const projectilesRef = useRef([]);
  const particlesRef = useRef([]);
  const pathRef = useRef([]);
  const cashRef = useRef(STARTING_CASH);
  const livesRef = useRef(STARTING_LIVES);
  const scoreRef = useRef(0);
  const gameStateRef = useRef('countdown');
  const animationFrameRef = useRef(null);
  const waveTimeoutRef = useRef(null);
  const bloonSpawnRef = useRef({ index: 0, queue: [], timeout: null });
  const lastFrameTimeRef = useRef(0);
  const totalPopsRef = useRef(0);
  const pathBoundsRef = useRef([]);

  // Generate curved path
  const generatePath = useCallback((width, height) => {
    const points = [];
    const segments = 100;

    // Define control points for a winding path
    const controlPoints = [
      { x: -30, y: height * 0.5 },
      { x: width * 0.15, y: height * 0.5 },
      { x: width * 0.2, y: height * 0.2 },
      { x: width * 0.35, y: height * 0.15 },
      { x: width * 0.45, y: height * 0.4 },
      { x: width * 0.4, y: height * 0.7 },
      { x: width * 0.55, y: height * 0.85 },
      { x: width * 0.7, y: height * 0.7 },
      { x: width * 0.75, y: height * 0.35 },
      { x: width * 0.9, y: height * 0.3 },
      { x: width + 30, y: height * 0.3 },
    ];

    // Simple interpolation between control points
    for (let i = 0; i < controlPoints.length - 1; i++) {
      const p1 = controlPoints[i];
      const p2 = controlPoints[i + 1];
      const stepsPerSegment = Math.ceil(segments / (controlPoints.length - 1));

      for (let j = 0; j < stepsPerSegment; j++) {
        const t = j / stepsPerSegment;
        points.push({
          x: p1.x + (p2.x - p1.x) * t,
          y: p1.y + (p2.y - p1.y) * t,
        });
      }
    }
    points.push(controlPoints[controlPoints.length - 1]);

    // Calculate path bounds for placement checking
    const bounds = [];
    for (let i = 0; i < points.length - 1; i++) {
      bounds.push({
        x1: points[i].x,
        y1: points[i].y,
        x2: points[i + 1].x,
        y2: points[i + 1].y,
      });
    }
    pathBoundsRef.current = bounds;

    return points;
  }, []);

  useEffect(() => {
    gameStateRef.current = gameState;
  }, [gameState]);

  // Initialize canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    const container = gameAreaRef.current;
    if (!canvas || !container) return;

    const dpr = window.devicePixelRatio || 1;
    const rect = container.getBoundingClientRect();

    canvas.width = rect.width * dpr;
    canvas.height = (rect.height - 100) * dpr;
    canvas.style.width = `${rect.width}px`;
    canvas.style.height = `${rect.height - 100}px`;

    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);

    pathRef.current = generatePath(rect.width, rect.height - 100);
  }, [generatePath]);

  // Countdown
  useEffect(() => {
    if (gameState !== 'countdown') return;
    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
      startWave(0);
    }
  }, [countdown, gameState]);

  // Game timer
  useEffect(() => {
    if (gameState !== 'playing') return;
    const timer = setInterval(() => {
      setTimeLeft(prev => {
        if (prev <= 1) {
          endGame();
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
    return () => clearInterval(timer);
  }, [gameState]);

  // Main game loop
  useEffect(() => {
    if (gameState !== 'playing' && gameState !== 'countdown') return;

    const gameLoop = (timestamp) => {
      const deltaTime = Math.min((timestamp - lastFrameTimeRef.current) / 16.67, 3);
      lastFrameTimeRef.current = timestamp;

      if (gameStateRef.current === 'playing') {
        updateGame(deltaTime);
      }
      renderGame();
      animationFrameRef.current = requestAnimationFrame(gameLoop);
    };

    lastFrameTimeRef.current = performance.now();
    animationFrameRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    };
  }, [gameState]);

  // Cleanup
  useEffect(() => {
    return () => {
      if (waveTimeoutRef.current) clearTimeout(waveTimeoutRef.current);
      if (bloonSpawnRef.current.timeout) clearTimeout(bloonSpawnRef.current.timeout);
    };
  }, []);

  const startWave = useCallback((waveIndex) => {
    if (waveIndex >= WAVES.length) {
      scoreRef.current += 500;
      setScore(scoreRef.current);
      endGame();
      return;
    }

    setCurrentWave(waveIndex);
    setWaveInProgress(true);
    const wave = WAVES[waveIndex];

    const queue = [];
    wave.bloons.forEach(group => {
      for (let i = 0; i < group.count; i++) {
        queue.push({ type: group.type, delay: wave.spacing });
      }
    });

    bloonSpawnRef.current.queue = queue;
    bloonSpawnRef.current.index = 0;

    const spawnNext = () => {
      if (gameStateRef.current !== 'playing') return;
      if (bloonSpawnRef.current.index >= bloonSpawnRef.current.queue.length) return;

      const bloonData = bloonSpawnRef.current.queue[bloonSpawnRef.current.index];
      spawnBloon(bloonData.type, 0);
      bloonSpawnRef.current.index++;

      if (bloonSpawnRef.current.index < bloonSpawnRef.current.queue.length) {
        bloonSpawnRef.current.timeout = setTimeout(spawnNext, bloonData.delay);
      }
    };

    setTimeout(spawnNext, wave.delay);
  }, []);

  const spawnBloon = useCallback((type, pathProgress, offset = 0) => {
    const bloonData = BLOONS[type];
    const path = pathRef.current;
    if (path.length === 0) return;

    const startIndex = Math.floor(pathProgress * (path.length - 1));
    const startPos = path[Math.max(0, startIndex)] || path[0];

    const bloon = {
      id: Date.now() + Math.random(),
      type,
      ...bloonData,
      x: startPos.x - offset,
      y: startPos.y,
      pathProgress: pathProgress,
      frozenUntil: 0,
      distanceTraveled: pathProgress * path.length,
    };

    bloonsRef.current.push(bloon);
  }, []);

  const createParticle = useCallback((x, y, color, count = 6) => {
    for (let i = 0; i < count; i++) {
      const angle = (i / count) * Math.PI * 2;
      const speed = 2 + Math.random() * 3;
      particlesRef.current.push({
        x, y,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        size: 3 + Math.random() * 4,
        color,
        life: 1,
      });
    }
  }, []);

  const updateGame = useCallback((deltaTime) => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const path = pathRef.current;
    const now = Date.now();
    const dpr = window.devicePixelRatio || 1;
    const width = canvas.width / dpr;
    const height = canvas.height / dpr;

    // Update bloons
    bloonsRef.current.forEach(bloon => {
      const isFrozen = bloon.frozenUntil > now;
      if (isFrozen) return;

      const speed = bloon.speed * deltaTime * 0.5;
      bloon.distanceTraveled += speed;
      bloon.pathProgress = bloon.distanceTraveled / path.length;

      const pathIndex = Math.floor(bloon.pathProgress * (path.length - 1));
      if (pathIndex < path.length - 1) {
        const p1 = path[pathIndex];
        const p2 = path[pathIndex + 1];
        const t = (bloon.pathProgress * (path.length - 1)) % 1;
        bloon.x = p1.x + (p2.x - p1.x) * t;
        bloon.y = p1.y + (p2.y - p1.y) * t;
      }

      // Check if reached end
      if (bloon.pathProgress >= 1) {
        const damage = bloon.isBlimp ? 616 :
                       bloon.type === 'ceramic' ? 104 :
                       bloon.type === 'rainbow' ? 47 :
                       bloon.type === 'zebra' ? 23 :
                       bloon.type === 'black' || bloon.type === 'white' ? 11 :
                       bloon.type === 'pink' ? 5 :
                       bloon.type === 'yellow' ? 4 :
                       bloon.type === 'green' ? 3 :
                       bloon.type === 'blue' ? 2 : 1;

        livesRef.current = Math.max(0, livesRef.current - damage);
        setLives(livesRef.current);
        bloon.health = 0;

        if (livesRef.current <= 0) {
          endGame();
        }
      }
    });

    // Remove dead bloons and spawn children
    const newBloons = [];
    bloonsRef.current = bloonsRef.current.filter(bloon => {
      if (bloon.health <= 0 && bloon.pathProgress < 1) {
        // Spawn children if applicable
        if (bloon.children) {
          const childCount = bloon.childCount || 1;
          for (let i = 0; i < childCount; i++) {
            const childData = BLOONS[bloon.children];
            if (childData) {
              newBloons.push({
                id: Date.now() + Math.random() + i,
                type: bloon.children,
                ...childData,
                x: bloon.x + (i - childCount / 2) * 10,
                y: bloon.y,
                pathProgress: bloon.pathProgress,
                frozenUntil: 0,
                distanceTraveled: bloon.distanceTraveled,
              });
            }
          }
        }

        // Reward
        cashRef.current += bloon.reward;
        setCash(cashRef.current);
        scoreRef.current += bloon.reward;
        setScore(scoreRef.current);
        totalPopsRef.current++;
        setTotalPops(totalPopsRef.current);

        createParticle(bloon.x, bloon.y, bloon.color);
        return false;
      }
      return bloon.health > 0;
    });

    bloonsRef.current = [...bloonsRef.current, ...newBloons];

    // Check wave complete
    if (waveInProgress &&
        bloonsRef.current.length === 0 &&
        bloonSpawnRef.current.index >= bloonSpawnRef.current.queue.length) {
      setWaveInProgress(false);

      const waveBonus = 100 + currentWave * 10;
      cashRef.current += waveBonus;
      setCash(cashRef.current);

      waveTimeoutRef.current = setTimeout(() => {
        if (gameStateRef.current === 'playing') {
          startWave(currentWave + 1);
        }
      }, 3000);
    }

    // Update towers and fire
    towersRef.current.forEach(tower => {
      if (now - tower.lastFired < tower.fireRate) return;

      // Ice tower special - freeze in range
      if (tower.freeze) {
        let frozeAny = false;
        bloonsRef.current.forEach(bloon => {
          if (bloon.immuneFreeze) return;
          const dx = bloon.x - tower.x;
          const dy = bloon.y - tower.y;
          const dist = Math.sqrt(dx * dx + dy * dy);
          if (dist < tower.range) {
            bloon.frozenUntil = now + tower.freezeDuration;
            frozeAny = true;
          }
        });
        if (frozeAny) {
          tower.lastFired = now;
          createParticle(tower.x, tower.y, '#00BCD4', 8);
        }
        return;
      }

      // Multi-shot towers (tack)
      if (tower.multishot) {
        let targetFound = false;
        bloonsRef.current.forEach(bloon => {
          const dx = bloon.x - tower.x;
          const dy = bloon.y - tower.y;
          const dist = Math.sqrt(dx * dx + dy * dy);
          if (dist < tower.range) targetFound = true;
        });

        if (targetFound) {
          tower.lastFired = now;
          for (let i = 0; i < tower.multishot; i++) {
            const angle = (i / tower.multishot) * Math.PI * 2;
            projectilesRef.current.push({
              id: Date.now() + Math.random(),
              x: tower.x,
              y: tower.y,
              vx: Math.cos(angle) * tower.projectileSpeed,
              vy: Math.sin(angle) * tower.projectileSpeed,
              tower,
              pierce: tower.pierce,
              hitBloons: new Set(),
            });
          }
        }
        return;
      }

      // Find target (first bloon in range by progress)
      let target = null;
      let maxProgress = -1;
      bloonsRef.current.forEach(bloon => {
        const dx = bloon.x - tower.x;
        const dy = bloon.y - tower.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < tower.range && bloon.pathProgress > maxProgress) {
          maxProgress = bloon.pathProgress;
          target = bloon;
        }
      });

      if (target) {
        tower.lastFired = now;

        // Sniper instant hit
        if (tower.range === 999) {
          target.health -= tower.damage;
          createParticle(target.x, target.y, tower.projectileColor, 4);
          return;
        }

        const dx = target.x - tower.x;
        const dy = target.y - tower.y;
        const dist = Math.sqrt(dx * dx + dy * dy);

        projectilesRef.current.push({
          id: Date.now() + Math.random(),
          x: tower.x,
          y: tower.y,
          vx: (dx / dist) * tower.projectileSpeed,
          vy: (dy / dist) * tower.projectileSpeed,
          tower,
          pierce: tower.pierce,
          hitBloons: new Set(),
        });
      }
    });

    // Update projectiles
    projectilesRef.current = projectilesRef.current.filter(proj => {
      proj.x += proj.vx * deltaTime;
      proj.y += proj.vy * deltaTime;

      // Check bounds
      if (proj.x < -20 || proj.x > width + 20 || proj.y < -20 || proj.y > height + 20) {
        return false;
      }

      // Check bloon collisions
      for (const bloon of bloonsRef.current) {
        if (proj.hitBloons.has(bloon.id)) continue;

        const dx = proj.x - bloon.x;
        const dy = proj.y - bloon.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        const bloonRadius = (bloon.size || 1) * 12;

        if (dist < bloonRadius + 6) {
          // Check immunities
          if (proj.tower.splash && bloon.immuneExplosion) continue;
          if (proj.tower.freeze && bloon.immuneFreeze) continue;

          proj.hitBloons.add(bloon.id);
          bloon.health -= proj.tower.damage;

          createParticle(bloon.x, bloon.y, bloon.color, 3);

          // Splash damage
          if (proj.tower.splash) {
            bloonsRef.current.forEach(other => {
              if (other.id === bloon.id || other.immuneExplosion) return;
              const odx = other.x - proj.x;
              const ody = other.y - proj.y;
              const odist = Math.sqrt(odx * odx + ody * ody);
              if (odist < proj.tower.splash) {
                other.health -= proj.tower.damage;
              }
            });
            createParticle(proj.x, proj.y, '#FF6600', 12);
            return false;
          }

          proj.pierce--;
          if (proj.pierce <= 0) return false;
        }
      }

      return true;
    });

    // Update particles
    particlesRef.current = particlesRef.current.filter(p => {
      p.x += p.vx * deltaTime;
      p.y += p.vy * deltaTime;
      p.vy += 0.1 * deltaTime;
      p.life -= 0.03 * deltaTime;
      return p.life > 0;
    });
  }, [currentWave, waveInProgress, startWave, createParticle]);

  const endGame = useCallback(() => {
    setGameState('ended');
    if (waveTimeoutRef.current) clearTimeout(waveTimeoutRef.current);
    if (bloonSpawnRef.current.timeout) clearTimeout(bloonSpawnRef.current.timeout);
  }, []);

  const renderGame = useCallback(() => {
    const canvas = canvasRef.current;
    const ctx = canvas?.getContext('2d');
    if (!canvas || !ctx) return;

    const dpr = window.devicePixelRatio || 1;
    const width = canvas.width / dpr;
    const height = canvas.height / dpr;
    const now = Date.now();

    ctx.save();

    // Sky background
    const skyGradient = ctx.createLinearGradient(0, 0, 0, height);
    skyGradient.addColorStop(0, '#87CEEB');
    skyGradient.addColorStop(1, '#98FB98');
    ctx.fillStyle = skyGradient;
    ctx.fillRect(0, 0, width, height);

    // Grass
    ctx.fillStyle = '#228B22';
    ctx.fillRect(0, 0, width, height);

    // Grass texture
    ctx.fillStyle = 'rgba(0, 100, 0, 0.3)';
    for (let i = 0; i < 150; i++) {
      const x = (i * 37) % width;
      const y = (i * 47) % height;
      ctx.beginPath();
      ctx.arc(x, y, 2 + Math.random(), 0, Math.PI * 2);
      ctx.fill();
    }

    // Draw path
    const path = pathRef.current;
    if (path.length > 1) {
      // Path shadow
      ctx.strokeStyle = 'rgba(0, 0, 0, 0.3)';
      ctx.lineWidth = 48;
      ctx.lineCap = 'round';
      ctx.lineJoin = 'round';
      ctx.beginPath();
      ctx.moveTo(path[0].x, path[0].y + 3);
      for (let i = 1; i < path.length; i++) {
        ctx.lineTo(path[i].x, path[i].y + 3);
      }
      ctx.stroke();

      // Path base
      ctx.strokeStyle = '#8B7355';
      ctx.lineWidth = 45;
      ctx.beginPath();
      ctx.moveTo(path[0].x, path[0].y);
      for (let i = 1; i < path.length; i++) {
        ctx.lineTo(path[i].x, path[i].y);
      }
      ctx.stroke();

      // Path surface
      ctx.strokeStyle = '#C4A574';
      ctx.lineWidth = 38;
      ctx.beginPath();
      ctx.moveTo(path[0].x, path[0].y);
      for (let i = 1; i < path.length; i++) {
        ctx.lineTo(path[i].x, path[i].y);
      }
      ctx.stroke();

      // Path dashed center line
      ctx.strokeStyle = 'rgba(255, 255, 255, 0.2)';
      ctx.lineWidth = 2;
      ctx.setLineDash([10, 10]);
      ctx.beginPath();
      ctx.moveTo(path[0].x, path[0].y);
      for (let i = 1; i < path.length; i++) {
        ctx.lineTo(path[i].x, path[i].y);
      }
      ctx.stroke();
      ctx.setLineDash([]);
    }

    // Draw tower ranges
    if (selectedPlacedTower) {
      ctx.strokeStyle = 'rgba(255, 255, 255, 0.4)';
      ctx.fillStyle = 'rgba(255, 255, 255, 0.08)';
      ctx.lineWidth = 2;
      ctx.setLineDash([8, 4]);
      ctx.beginPath();
      ctx.arc(selectedPlacedTower.x, selectedPlacedTower.y, selectedPlacedTower.range, 0, Math.PI * 2);
      ctx.fill();
      ctx.stroke();
      ctx.setLineDash([]);
    }

    // Draw towers
    towersRef.current.forEach(tower => {
      const isSelected = selectedPlacedTower?.id === tower.id;

      // Tower base
      ctx.fillStyle = '#333';
      ctx.beginPath();
      ctx.arc(tower.x, tower.y, 22, 0, Math.PI * 2);
      ctx.fill();

      // Tower body
      const gradient = ctx.createRadialGradient(tower.x - 5, tower.y - 5, 0, tower.x, tower.y, 18);
      gradient.addColorStop(0, tower.color);
      gradient.addColorStop(1, '#333');
      ctx.fillStyle = gradient;
      ctx.beginPath();
      ctx.arc(tower.x, tower.y, 18, 0, Math.PI * 2);
      ctx.fill();

      // Selection ring
      if (isSelected) {
        ctx.strokeStyle = '#FFD700';
        ctx.lineWidth = 3;
        ctx.beginPath();
        ctx.arc(tower.x, tower.y, 25, 0, Math.PI * 2);
        ctx.stroke();
      }

      // Tower emoji
      ctx.font = '20px Arial';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      ctx.fillText(tower.emoji, tower.x, tower.y);

      // Level indicator
      if (tower.level > 1) {
        ctx.fillStyle = '#FFD700';
        ctx.font = 'bold 10px Arial';
        ctx.fillText(`★${tower.level}`, tower.x, tower.y + 26);
      }
    });

    // Draw bloons
    bloonsRef.current.forEach(bloon => {
      const isFrozen = bloon.frozenUntil > now;
      const size = (bloon.size || 1) * 12;

      // Shadow
      ctx.fillStyle = 'rgba(0, 0, 0, 0.3)';
      ctx.beginPath();
      ctx.ellipse(bloon.x + 2, bloon.y + size * 0.5, size * 0.7, size * 0.3, 0, 0, Math.PI * 2);
      ctx.fill();

      // Freeze effect
      if (isFrozen) {
        ctx.fillStyle = 'rgba(100, 200, 255, 0.5)';
        ctx.beginPath();
        ctx.arc(bloon.x, bloon.y, size + 4, 0, Math.PI * 2);
        ctx.fill();
      }

      // Bloon body
      ctx.fillStyle = isFrozen ? '#88DDFF' : bloon.color;
      ctx.beginPath();
      ctx.arc(bloon.x, bloon.y, size, 0, Math.PI * 2);
      ctx.fill();

      // Bloon highlight
      ctx.fillStyle = 'rgba(255, 255, 255, 0.4)';
      ctx.beginPath();
      ctx.ellipse(bloon.x - size * 0.3, bloon.y - size * 0.3, size * 0.3, size * 0.2, -0.5, 0, Math.PI * 2);
      ctx.fill();

      // Bloon string
      ctx.strokeStyle = bloon.color;
      ctx.lineWidth = 1;
      ctx.beginPath();
      ctx.moveTo(bloon.x, bloon.y + size);
      ctx.quadraticCurveTo(bloon.x - 3, bloon.y + size + 8, bloon.x, bloon.y + size + 15);
      ctx.stroke();

      // Health bar for strong bloons
      if (bloon.isBlimp || bloon.type === 'ceramic') {
        const maxHealth = BLOONS[bloon.type].health;
        const healthPercent = bloon.health / maxHealth;
        const barWidth = size * 2;

        ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
        ctx.fillRect(bloon.x - barWidth / 2, bloon.y - size - 10, barWidth, 5);

        ctx.fillStyle = healthPercent > 0.5 ? '#4CAF50' : healthPercent > 0.25 ? '#FFC107' : '#F44336';
        ctx.fillRect(bloon.x - barWidth / 2, bloon.y - size - 10, barWidth * healthPercent, 5);
      }
    });

    // Draw projectiles
    projectilesRef.current.forEach(proj => {
      ctx.fillStyle = proj.tower.projectileColor;
      ctx.beginPath();
      ctx.arc(proj.x, proj.y, 4, 0, Math.PI * 2);
      ctx.fill();

      ctx.fillStyle = '#FFF';
      ctx.beginPath();
      ctx.arc(proj.x - 1, proj.y - 1, 2, 0, Math.PI * 2);
      ctx.fill();
    });

    // Draw particles
    particlesRef.current.forEach(p => {
      ctx.globalAlpha = p.life;
      ctx.fillStyle = p.color;
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.size * p.life, 0, Math.PI * 2);
      ctx.fill();
    });
    ctx.globalAlpha = 1;

    ctx.restore();
  }, [selectedPlacedTower]);

  const isOnPath = useCallback((x, y) => {
    for (const bound of pathBoundsRef.current) {
      const A = x - bound.x1;
      const B = y - bound.y1;
      const C = bound.x2 - bound.x1;
      const D = bound.y2 - bound.y1;

      const dot = A * C + B * D;
      const lenSq = C * C + D * D;
      let param = -1;
      if (lenSq !== 0) param = dot / lenSq;

      let xx, yy;
      if (param < 0) {
        xx = bound.x1;
        yy = bound.y1;
      } else if (param > 1) {
        xx = bound.x2;
        yy = bound.y2;
      } else {
        xx = bound.x1 + param * C;
        yy = bound.y1 + param * D;
      }

      const dist = Math.sqrt((x - xx) ** 2 + (y - yy) ** 2);
      if (dist < 35) return true;
    }
    return false;
  }, []);

  const handleCanvasClick = useCallback((e) => {
    if (gameStateRef.current !== 'playing') return;

    const canvas = canvasRef.current;
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    // Check if clicking on existing tower
    const clickedTower = towersRef.current.find(tower => {
      const dx = tower.x - x;
      const dy = tower.y - y;
      return Math.sqrt(dx * dx + dy * dy) < 25;
    });

    if (clickedTower) {
      setSelectedPlacedTower(clickedTower);
      setSelectedTower(null);
      return;
    }

    // Place new tower
    if (selectedTower) {
      const towerData = TOWERS[selectedTower];
      if (cashRef.current < towerData.cost) return;
      if (isOnPath(x, y)) return;

      // Check tower collision
      const tooCloseToTower = towersRef.current.some(tower => {
        const dx = tower.x - x;
        const dy = tower.y - y;
        return Math.sqrt(dx * dx + dy * dy) < 45;
      });
      if (tooCloseToTower) return;

      cashRef.current -= towerData.cost;
      setCash(cashRef.current);

      towersRef.current.push({
        id: Date.now(),
        type: selectedTower,
        x,
        y,
        level: 1,
        lastFired: 0,
        ...towerData,
      });

      createParticle(x, y, '#4CAF50', 10);
      setSelectedTower(null);
    } else {
      setSelectedPlacedTower(null);
    }
  }, [selectedTower, isOnPath, createParticle]);

  const upgradeTower = useCallback(() => {
    if (!selectedPlacedTower) return;

    const upgradeCost = Math.floor(selectedPlacedTower.cost * 0.6 * selectedPlacedTower.level);
    if (cashRef.current < upgradeCost) return;

    cashRef.current -= upgradeCost;
    setCash(cashRef.current);

    const tower = towersRef.current.find(t => t.id === selectedPlacedTower.id);
    if (tower) {
      tower.level++;
      tower.damage = Math.ceil(tower.damage * 1.3);
      tower.range = Math.ceil(tower.range * 1.05);
      tower.fireRate = Math.ceil(tower.fireRate * 0.9);
      if (tower.pierce) tower.pierce = Math.ceil(tower.pierce * 1.2);

      createParticle(tower.x, tower.y, '#FFD700', 12);
      setSelectedPlacedTower({ ...tower });
    }
  }, [selectedPlacedTower, createParticle]);

  const sellTower = useCallback(() => {
    if (!selectedPlacedTower) return;

    const sellValue = Math.floor(selectedPlacedTower.cost * 0.7 * selectedPlacedTower.level);
    cashRef.current += sellValue;
    setCash(cashRef.current);

    createParticle(selectedPlacedTower.x, selectedPlacedTower.y, '#FFD700', 8);
    towersRef.current = towersRef.current.filter(t => t.id !== selectedPlacedTower.id);
    setSelectedPlacedTower(null);
  }, [selectedPlacedTower, createParticle]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="tower-defense-overlay">
      <div className="tower-defense-container">
        {/* Header */}
        <div className="td-header">
          <div className="td-stat gold">
            <span className="td-stat-icon">💰</span>
            <span className="td-stat-value">{cash}</span>
          </div>
          <div className="td-stat lives">
            <span className="td-stat-icon">❤️</span>
            <span className="td-stat-value">{lives}</span>
          </div>
          <div className="td-timer">
            <span className="td-timer-icon">⏱️</span>
            <span className={`td-timer-value ${timeLeft <= 15 ? 'warning' : ''}`}>
              {timeLeft}s
            </span>
          </div>
          <div className="td-stat wave">
            <span className="td-stat-icon">🌊</span>
            <span className="td-stat-value">{currentWave + 1}/{WAVES.length}</span>
          </div>
          <div className="td-stat score">
            <span className="td-stat-icon">🎈</span>
            <span className="td-stat-value">{totalPops}</span>
          </div>
        </div>

        {/* Game Canvas */}
        <div ref={gameAreaRef} className="td-game-area">
          <canvas ref={canvasRef} onClick={handleCanvasClick} />
        </div>

        {/* Tower Selection */}
        <div className="td-tower-selection">
          {Object.entries(TOWERS).map(([key, tower]) => (
            <button
              key={key}
              className={`td-tower-btn ${selectedTower === key ? 'selected' : ''} ${cash < tower.cost ? 'disabled' : ''}`}
              onClick={() => {
                setSelectedTower(selectedTower === key ? null : key);
                setSelectedPlacedTower(null);
              }}
              disabled={gameState !== 'playing'}
            >
              <span className="tower-emoji">{tower.emoji}</span>
              <span className="tower-cost">${tower.cost}</span>
            </button>
          ))}
        </div>

        {/* Tower Info Panel */}
        {selectedTower && (
          <div className="td-tower-info">
            <div className="tower-info-header">
              <span>{TOWERS[selectedTower].emoji}</span>
              <span>{TOWERS[selectedTower].name}</span>
            </div>
            <div className="tower-info-desc">{TOWERS[selectedTower].description}</div>
            <div className="tower-info-stats">
              <span>⚔️ {TOWERS[selectedTower].damage}</span>
              <span>📍 {TOWERS[selectedTower].range === 999 ? '∞' : TOWERS[selectedTower].range}</span>
            </div>
            <div className="tower-info-hint">Tap grass to place tower</div>
          </div>
        )}

        {/* Placed Tower Panel */}
        {selectedPlacedTower && (
          <div className="td-placed-tower-panel">
            <div className="tower-panel-header">
              <span>{selectedPlacedTower.emoji}</span>
              <span>{selectedPlacedTower.name} ★{selectedPlacedTower.level}</span>
            </div>
            <div className="tower-panel-stats">
              <span>⚔️ {selectedPlacedTower.damage}</span>
              <span>📍 {selectedPlacedTower.range === 999 ? '∞' : selectedPlacedTower.range}</span>
            </div>
            <div className="tower-panel-actions">
              <button
                className="tower-action-btn upgrade"
                onClick={upgradeTower}
                disabled={cash < Math.floor(selectedPlacedTower.cost * 0.6 * selectedPlacedTower.level)}
              >
                ⬆️ Upgrade (${Math.floor(selectedPlacedTower.cost * 0.6 * selectedPlacedTower.level)})
              </button>
              <button className="tower-action-btn sell" onClick={sellTower}>
                💵 Sell (+${Math.floor(selectedPlacedTower.cost * 0.7 * selectedPlacedTower.level)})
              </button>
            </div>
          </div>
        )}

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="td-countdown" key={countdown}>
            {countdown > 0 ? countdown : 'POP!'}
          </div>
        )}

        {/* Wave announcement */}
        {waveInProgress && bloonsRef.current.length === 0 && bloonSpawnRef.current.index === 0 && (
          <div className="td-wave-announce">
            Wave {currentWave + 1} Incoming!
          </div>
        )}

        {/* Game Over */}
        {gameState === 'ended' && (
          <div className="td-game-over">
            <div className="td-game-over-content">
              <h2 className="td-game-over-title">
                {lives > 0 ? (
                  <><span>🎈</span> Victory! <span>🎈</span></>
                ) : (
                  <><span>💔</span> Bloons Escaped! <span>💔</span></>
                )}
              </h2>

              <div className="td-final-stats">
                <div className="td-final-stat">
                  <span className="td-final-icon">🌊</span>
                  <span className="td-final-label">Waves Survived</span>
                  <span className="td-final-value">{currentWave + 1}</span>
                </div>
                <div className="td-final-stat">
                  <span className="td-final-icon">🎈</span>
                  <span className="td-final-label">Bloons Popped</span>
                  <span className="td-final-value">{totalPops}</span>
                </div>
                <div className="td-final-stat highlight">
                  <span className="td-final-icon">⭐</span>
                  <span className="td-final-label">Total Score</span>
                  <span className="td-final-value coins">{score}</span>
                </div>
              </div>

              {lives > 0 && currentWave >= WAVES.length - 1 && (
                <div className="td-achievement">
                  <span>👑</span> Perfect Defense! <span>👑</span>
                </div>
              )}

              <button className="td-claim-btn cute-button" onClick={handleFinish}>
                Claim {score} Coins! 🪙
              </button>
            </div>
          </div>
        )}

        {/* Instructions */}
        {gameState === 'countdown' && countdown === 3 && (
          <div className="td-instructions">
            <p>Pop the bloons before they escape!</p>
            <p className="td-hint">Tap towers below, then tap grass to place 🐵</p>
          </div>
        )}

        {/* Close button */}
        {gameState === 'countdown' && (
          <button className="td-close-btn" onClick={onClose}>✕</button>
        )}
      </div>
    </div>
  );
};

export default TowerDefenseGame;
