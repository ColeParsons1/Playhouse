import { useState, useEffect, useCallback, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './GooBridgeGame.css';

const COINS_PER_LEVEL = 15;
const COMPLETION_BONUS = 25;
const TIME_BONUS_THRESHOLD = 20; // seconds left for time bonus
const TIME_BONUS = 10;
const PERFECT_BONUS = 20; // use minimum goo balls

// Level configurations with more variety - balanced for playability
const LEVELS = [
  {
    id: 1,
    name: 'First Steps',
    startPlatform: { x: 50, y: 350, width: 100 },
    endPlatform: { x: 250, y: 350, width: 100 },
    gooCount: 10,
    minGoo: 4,
    obstacles: [],
    goal: { x: 320, y: 320 }, // Goal on the end platform
    timeLimit: 90,
    hint: 'Tap near goo balls to connect new ones!',
  },
  {
    id: 2,
    name: 'The Gap',
    startPlatform: { x: 30, y: 350, width: 80 },
    endPlatform: { x: 240, y: 350, width: 80 },
    gooCount: 12,
    minGoo: 5,
    obstacles: [],
    goal: { x: 300, y: 320 },
    timeLimit: 90,
    hint: 'Build triangles for stability!',
  },
  {
    id: 3,
    name: 'Over The Pit',
    startPlatform: { x: 30, y: 300, width: 80 },
    endPlatform: { x: 260, y: 300, width: 80 },
    gooCount: 14,
    minGoo: 7,
    obstacles: [{ x: 120, y: 380, width: 120, height: 100, type: 'pit' }],
    goal: { x: 320, y: 270 },
    timeLimit: 90,
    hint: 'Avoid falling into the pit!',
  },
  {
    id: 4,
    name: 'High Rise',
    startPlatform: { x: 50, y: 380, width: 100 },
    endPlatform: { x: 240, y: 220, width: 100 },
    gooCount: 18,
    minGoo: 9,
    obstacles: [],
    goal: { x: 310, y: 190 },
    timeLimit: 120,
    hint: 'Build upward with strong triangles!',
  },
  {
    id: 5,
    name: 'The Chasm',
    startPlatform: { x: 20, y: 350, width: 60 },
    endPlatform: { x: 280, y: 350, width: 60 },
    gooCount: 20,
    minGoo: 11,
    obstacles: [{ x: 90, y: 380, width: 170, height: 100, type: 'pit' }],
    goal: { x: 320, y: 320 },
    timeLimit: 120,
    hint: 'A long bridge needs support!',
  },
  {
    id: 6,
    name: 'Downhill',
    startPlatform: { x: 30, y: 200, width: 80 },
    endPlatform: { x: 280, y: 380, width: 80 },
    gooCount: 16,
    minGoo: 8,
    obstacles: [],
    goal: { x: 340, y: 350 },
    timeLimit: 100,
    hint: 'Let gravity help you reach down!',
  },
  {
    id: 7,
    name: 'Twin Pits',
    startPlatform: { x: 20, y: 320, width: 70 },
    endPlatform: { x: 300, y: 320, width: 70 },
    gooCount: 22,
    minGoo: 12,
    obstacles: [
      { x: 100, y: 380, width: 70, height: 100, type: 'pit' },
      { x: 210, y: 380, width: 70, height: 100, type: 'pit' },
    ],
    goal: { x: 350, y: 290 },
    timeLimit: 120,
    hint: 'Navigate between the twin dangers!',
  },
  {
    id: 8,
    name: 'Sky Tower',
    startPlatform: { x: 160, y: 400, width: 100 },
    endPlatform: { x: 160, y: 150, width: 100 },
    gooCount: 25,
    minGoo: 14,
    obstacles: [],
    goal: { x: 210, y: 120 },
    timeLimit: 150,
    hint: 'Build straight up - stay balanced!',
  },
  {
    id: 9,
    name: 'Valley Crossing',
    startPlatform: { x: 20, y: 200, width: 70 },
    endPlatform: { x: 310, y: 200, width: 70 },
    gooCount: 24,
    minGoo: 13,
    obstacles: [{ x: 100, y: 280, width: 180, height: 200, type: 'pit' }],
    goal: { x: 360, y: 170 },
    timeLimit: 140,
    hint: 'Build high over the deep valley!',
  },
  {
    id: 10,
    name: 'The Grand Finale',
    startPlatform: { x: 20, y: 380, width: 60 },
    endPlatform: { x: 320, y: 180, width: 60 },
    gooCount: 30,
    minGoo: 16,
    obstacles: [
      { x: 80, y: 400, width: 80, height: 80, type: 'pit' },
      { x: 200, y: 350, width: 80, height: 130, type: 'pit' },
    ],
    goal: { x: 360, y: 150 },
    timeLimit: 180,
    hint: 'The ultimate challenge - reach for the stars!',
  },
];

// Physics constants - World of Goo style (balanced for playability)
const GRAVITY = 0.25; // Reduced for more floaty feel
const GOO_RADIUS = 14;
const CONNECTION_DISTANCE = 70;
const MIN_CONNECTION_DISTANCE = 30; // Allow closer connections
const MAX_CONNECTION_DISTANCE = 110; // Allow longer reach for connections
const SPRING_STIFFNESS = 0.04; // Softer springs for stability
const SPRING_DAMPING = 0.88; // More damping for less oscillation
const VELOCITY_DAMPING = 0.98;
const REPEL_FORCE = 0.8; // Softer repulsion
const REPEL_DISTANCE = GOO_RADIUS * 2.2;
const MAX_CONNECTIONS_PER_GOO = 6; // Limit connections like World of Goo
const STRAND_BREAK_DISTANCE = 180; // Much longer before breaking
const MASS = 1.0;
const AIR_RESISTANCE = 0.995;

// Gamer girl aesthetic color palette - rotating colors
const GAMER_GIRL_COLORS = [
  { color1: '#FF69B4', color2: '#FF1493', glow: 'rgba(255, 105, 180, 0.6)', name: 'Hot Pink' },
  { color1: '#DA70D6', color2: '#BA55D3', glow: 'rgba(218, 112, 214, 0.6)', name: 'Orchid' },
  { color1: '#FF00FF', color2: '#CC00CC', glow: 'rgba(255, 0, 255, 0.6)', name: 'Magenta' },
  { color1: '#9370DB', color2: '#7B68EE', glow: 'rgba(147, 112, 219, 0.6)', name: 'Purple' },
  { color1: '#00CED1', color2: '#008B8B', glow: 'rgba(0, 206, 209, 0.6)', name: 'Teal' },
  { color1: '#E0BBE4', color2: '#C8A2C8', glow: 'rgba(224, 187, 228, 0.6)', name: 'Lavender' },
  { color1: '#FFB6C1', color2: '#FF69B4', glow: 'rgba(255, 182, 193, 0.6)', name: 'Light Pink' },
  { color1: '#DDA0DD', color2: '#EE82EE', glow: 'rgba(221, 160, 221, 0.6)', name: 'Plum' },
  { color1: '#40E0D0', color2: '#00CED1', glow: 'rgba(64, 224, 208, 0.6)', name: 'Turquoise' },
  { color1: '#FF6EB4', color2: '#FF1493', glow: 'rgba(255, 110, 180, 0.6)', name: 'Rose' },
];

// Get rotating color for goo based on id
const getGooColor = (id, isAnchor = false) => {
  if (isAnchor) {
    return { color1: '#FF00FF', color2: '#AA00AA', glow: 'rgba(255, 0, 255, 0.7)' };
  }
  return GAMER_GIRL_COLORS[id % GAMER_GIRL_COLORS.length];
};

// Goo ball types (keeping for compatibility but using rotating colors)
const GOO_TYPES = {
  anchor: { color1: '#FF00FF', color2: '#AA00AA', glow: 'rgba(255, 0, 255, 0.7)' },
  normal: { color1: '#FF69B4', color2: '#FF1493', glow: 'rgba(255, 105, 180, 0.6)' },
  special: { color1: '#00CED1', color2: '#008B8B', glow: 'rgba(0, 206, 209, 0.6)' },
};

const GooBridgeGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('menu');
  const [currentLevel, setCurrentLevel] = useState(0);
  const [score, setScore] = useState(0);
  const [gooBalls, setGooBalls] = useState([]);
  const [connections, setConnections] = useState([]);
  const [selectedGoo, setSelectedGoo] = useState(null);
  const [draggingGoo, setDraggingGoo] = useState(null);
  const [availableGoo, setAvailableGoo] = useState(0);
  const [goalReached, setGoalReached] = useState(false);
  const [timeLeft, setTimeLeft] = useState(60);
  const [gooUsed, setGooUsed] = useState(0);
  const [showHint, setShowHint] = useState(false);
  const [particles, setParticles] = useState([]);

  const scoreRef = useRef(0);
  const canvasRef = useRef(null);
  const ctxRef = useRef(null);
  const gooBallsRef = useRef([]);
  const connectionsRef = useRef([]);
  const animationFrameRef = useRef(null);
  const dragOffsetRef = useRef({ x: 0, y: 0 });
  const lastTimeRef = useRef(0);
  const particlesRef = useRef([]);
  const gooUsedRef = useRef(0);

  // Create particle burst
  const createParticles = useCallback((x, y, color, count = 10) => {
    const newParticles = [];
    for (let i = 0; i < count; i++) {
      const angle = (Math.PI * 2 * i) / count + Math.random() * 0.5;
      const speed = 2 + Math.random() * 3;
      newParticles.push({
        id: Date.now() + i,
        x,
        y,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        size: 4 + Math.random() * 4,
        color,
        life: 1,
        decay: 0.02 + Math.random() * 0.02,
      });
    }
    particlesRef.current = [...particlesRef.current, ...newParticles];
  }, []);

  // Initialize level
  const initializeLevel = useCallback((levelIndex) => {
    const level = LEVELS[levelIndex];
    if (!level) return;

    const initialGoo = [];
    const startX = level.startPlatform.x + level.startPlatform.width / 2;
    const startY = level.startPlatform.y - GOO_RADIUS;

    // Create 3 anchored goo balls in triangle formation (wider for stability)
    const spacing = 30;
    for (let i = 0; i < 3; i++) {
      initialGoo.push({
        id: i,
        x: startX - spacing + i * spacing,
        y: startY - (i === 1 ? 25 : 0),
        vx: 0,
        vy: 0,
        fx: 0,
        fy: 0,
        anchored: true,
        type: 'anchor',
        pulse: Math.random() * Math.PI * 2,
      });
    }

    // Create initial connections (triangle) with rest lengths
    const calcDist = (a, b) => Math.sqrt((a.x - b.x) ** 2 + (a.y - b.y) ** 2);
    const initialConnections = [
      { from: 0, to: 1, restLength: calcDist(initialGoo[0], initialGoo[1]) },
      { from: 1, to: 2, restLength: calcDist(initialGoo[1], initialGoo[2]) },
      { from: 0, to: 2, restLength: calcDist(initialGoo[0], initialGoo[2]) },
    ];

    setGooBalls(initialGoo);
    setConnections(initialConnections);
    setAvailableGoo(level.gooCount - 3);
    setGoalReached(false);
    setTimeLeft(level.timeLimit);
    setGooUsed(3);
    setShowHint(true);
    gooUsedRef.current = 3;
    gooBallsRef.current = initialGoo;
    connectionsRef.current = initialConnections;
    particlesRef.current = [];

    setTimeout(() => setShowHint(false), 4000);
  }, []);

  // Start game
  const startGame = useCallback(() => {
    setCurrentLevel(0);
    setScore(0);
    scoreRef.current = 0;
    setGameState('playing');
    initializeLevel(0);
  }, [initializeLevel]);

  // Timer
  useEffect(() => {
    if (gameState !== 'playing' || goalReached) return;

    const timer = setInterval(() => {
      setTimeLeft((prev) => {
        if (prev <= 1) {
          setGameState('gameover');
          return 0;
        }
        return prev - 1;
      });
    }, 1000);

    return () => clearInterval(timer);
  }, [gameState, goalReached]);

  // World of Goo style physics update
  const updatePhysics = useCallback((deltaTime) => {
    const level = LEVELS[currentLevel];
    if (!level) return;

    const canvas = canvasRef.current;
    const dt = Math.min(deltaTime / 16.67, 2); // Normalize to ~60fps, cap at 2x
    const goos = [...gooBallsRef.current];
    let conns = [...connectionsRef.current];

    // Update particles
    particlesRef.current = particlesRef.current
      .map((p) => ({
        ...p,
        x: p.x + p.vx * dt,
        y: p.y + p.vy * dt,
        vy: p.vy + 0.15 * dt,
        life: p.life - p.decay * dt,
      }))
      .filter((p) => p.life > 0);

    // Update goo ball pulse animations
    goos.forEach((goo) => {
      goo.pulse = (goo.pulse + 0.06 * dt) % (Math.PI * 2);
    });

    // Reset forces
    goos.forEach((goo) => {
      goo.fx = 0;
      goo.fy = 0;
    });

    // Apply spring forces from connections (World of Goo style - stiffer, with damping)
    const brokenConnections = [];
    conns.forEach((conn, connIndex) => {
      const gooA = goos.find((g) => g.id === conn.from);
      const gooB = goos.find((g) => g.id === conn.to);
      if (!gooA || !gooB) return;

      const dx = gooB.x - gooA.x;
      const dy = gooB.y - gooA.y;
      const distance = Math.sqrt(dx * dx + dy * dy);

      // Rest length is based on where the connection was originally made
      const restLength = conn.restLength || CONNECTION_DISTANCE;

      // Check if strand should break (stretched too far)
      if (distance > STRAND_BREAK_DISTANCE) {
        brokenConnections.push(connIndex);
        if (gooA.x && gooB.x) {
          createParticles((gooA.x + gooB.x) / 2, (gooA.y + gooB.y) / 2, '#FF69B4', 6);
        }
        return;
      }

      if (distance > 0) {
        // Spring force (Hooke's law with damping)
        const displacement = distance - restLength;
        const springForce = displacement * SPRING_STIFFNESS;

        // Relative velocity for damping
        const relVelX = (gooB.vx || 0) - (gooA.vx || 0);
        const relVelY = (gooB.vy || 0) - (gooA.vy || 0);
        const relVelAlongSpring = (relVelX * dx + relVelY * dy) / distance;
        const dampingForce = relVelAlongSpring * (1 - SPRING_DAMPING);

        const totalForce = springForce + dampingForce;
        const fx = (dx / distance) * totalForce;
        const fy = (dy / distance) * totalForce;

        if (!gooA.anchored) {
          gooA.fx = (gooA.fx || 0) + fx;
          gooA.fy = (gooA.fy || 0) + fy;
        }
        if (!gooB.anchored) {
          gooB.fx = (gooB.fx || 0) - fx;
          gooB.fy = (gooB.fy || 0) - fy;
        }
      }
    });

    // Remove broken connections
    if (brokenConnections.length > 0) {
      conns = conns.filter((_, idx) => !brokenConnections.includes(idx));
      connectionsRef.current = conns;
      setConnections([...conns]);
    }

    // Repel overlapping goo balls (soft body collision)
    for (let i = 0; i < goos.length; i++) {
      for (let j = i + 1; j < goos.length; j++) {
        const gooA = goos[i];
        const gooB = goos[j];
        const dx = gooB.x - gooA.x;
        const dy = gooB.y - gooA.y;
        const dist = Math.sqrt(dx * dx + dy * dy);

        if (dist < REPEL_DISTANCE && dist > 0.1) {
          // Soft collision - stronger when closer
          const overlap = REPEL_DISTANCE - dist;
          const force = (overlap / REPEL_DISTANCE) * REPEL_FORCE * (1 + overlap * 0.1);
          const fx = (dx / dist) * force;
          const fy = (dy / dist) * force;

          if (!gooA.anchored) {
            gooA.fx = (gooA.fx || 0) - fx;
            gooA.fy = (gooA.fy || 0) - fy;
          }
          if (!gooB.anchored) {
            gooB.fx = (gooB.fx || 0) + fx;
            gooB.fy = (gooB.fy || 0) + fy;
          }
        }
      }
    }

    // Update velocities and positions (Verlet-style integration)
    goos.forEach((goo) => {
      if (goo.anchored || goo === draggingGoo) return;

      // Apply accumulated forces
      goo.vx = (goo.vx || 0) + (goo.fx || 0) * dt;
      goo.vy = (goo.vy || 0) + (goo.fy || 0) * dt;

      // Apply gravity
      goo.vy += GRAVITY * dt;

      // Apply air resistance
      goo.vx *= Math.pow(AIR_RESISTANCE, dt);
      goo.vy *= Math.pow(AIR_RESISTANCE, dt);

      // Velocity damping
      goo.vx *= Math.pow(VELOCITY_DAMPING, dt);
      goo.vy *= Math.pow(VELOCITY_DAMPING, dt);

      // Clamp max velocity to prevent wild behavior
      const maxVelocity = 8;
      const currentVel = Math.sqrt(goo.vx * goo.vx + goo.vy * goo.vy);
      if (currentVel > maxVelocity) {
        goo.vx = (goo.vx / currentVel) * maxVelocity;
        goo.vy = (goo.vy / currentVel) * maxVelocity;
      }

      // Update position
      goo.x += goo.vx * dt;
      goo.y += goo.vy * dt;

      // Platform collision with friction
      [level.startPlatform, level.endPlatform].forEach((platform) => {
        // Top of platform
        if (
          goo.x > platform.x - GOO_RADIUS &&
          goo.x < platform.x + platform.width + GOO_RADIUS &&
          goo.y + GOO_RADIUS > platform.y &&
          goo.y < platform.y + 25
        ) {
          goo.y = platform.y - GOO_RADIUS;
          goo.vy *= -0.2; // Bounce
          goo.vx *= 0.85; // Friction
          if (Math.abs(goo.vy) < 0.3) goo.vy = 0;
        }
      });

      // Pit collision - goo falls in and is lost
      level.obstacles.forEach((obs) => {
        if (obs.type === 'pit') {
          if (
            goo.x > obs.x + 5 &&
            goo.x < obs.x + obs.width - 5 &&
            goo.y > obs.y + 10
          ) {
            goo.falling = true;
          }
        }
      });

      // Boundary collision with bounce
      if (canvas) {
        if (goo.x < GOO_RADIUS) {
          goo.x = GOO_RADIUS;
          goo.vx = Math.abs(goo.vx) * 0.4;
        }
        if (goo.x > canvas.width - GOO_RADIUS) {
          goo.x = canvas.width - GOO_RADIUS;
          goo.vx = -Math.abs(goo.vx) * 0.4;
        }
        // Top boundary
        if (goo.y < GOO_RADIUS) {
          goo.y = GOO_RADIUS;
          goo.vy = Math.abs(goo.vy) * 0.3;
        }
        if (goo.y > canvas.height + 50) {
          goo.falling = true;
        }
      }
    });

    // Remove fallen goo and their connections
    const fallenGoo = goos.filter((g) => g.falling);
    if (fallenGoo.length > 0) {
      fallenGoo.forEach((g) => {
        createParticles(g.x, canvas?.height || 450, '#FF69B4', 10);
      });
      gooBallsRef.current = goos.filter((g) => !g.falling);
      connectionsRef.current = conns.filter(
        (c) => !fallenGoo.some((g) => g.id === c.from || g.id === c.to)
      );
      setConnections([...connectionsRef.current]);
    }

    // Check goal
    const goal = level.goal;
    const reachedGoal = goos.some((goo) => {
      const dx = goo.x - goal.x;
      const dy = goo.y - goal.y;
      return Math.sqrt(dx * dx + dy * dy) < GOO_RADIUS + 25;
    });

    if (reachedGoal && !goalReached) {
      setGoalReached(true);
      createParticles(goal.x, goal.y, '#FF69B4', 25);
      createParticles(goal.x, goal.y, '#00CED1', 20);

      setTimeout(() => {
        let levelScore = COINS_PER_LEVEL;

        // Time bonus
        if (timeLeft > TIME_BONUS_THRESHOLD) {
          levelScore += TIME_BONUS;
        }

        // Perfect bonus (minimum goo used)
        if (gooUsedRef.current <= level.minGoo + 2) {
          levelScore += PERFECT_BONUS;
        }

        scoreRef.current += levelScore;
        setScore(scoreRef.current);

        if (currentLevel < LEVELS.length - 1) {
          setGameState('levelComplete');
        } else {
          scoreRef.current += COMPLETION_BONUS;
          setScore(scoreRef.current);
          setGameState('gameComplete');
        }
      }, 800);
    }

    gooBallsRef.current = goos;
    setGooBalls([...goos]);
    setParticles([...particlesRef.current]);
  }, [currentLevel, draggingGoo, goalReached, timeLeft, createParticles]);

  // Render game
  const renderGame = useCallback(() => {
    const canvas = canvasRef.current;
    const ctx = ctxRef.current;
    if (!canvas || !ctx) return;

    const level = LEVELS[currentLevel];
    if (!level) return;

    // Clear with gamer girl gradient background
    const bgGradient = ctx.createLinearGradient(0, 0, canvas.width, canvas.height);
    bgGradient.addColorStop(0, '#1a0a1a');
    bgGradient.addColorStop(0.3, '#150520');
    bgGradient.addColorStop(0.6, '#0a1520');
    bgGradient.addColorStop(1, '#0a0a15');
    ctx.fillStyle = bgGradient;
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    // Animated background stars/sparkles
    const time = Date.now() * 0.001;
    for (let i = 0; i < canvas.width; i += 30) {
      for (let j = 0; j < canvas.height; j += 30) {
        const twinkle = Math.sin(time * 2 + i * 0.05 + j * 0.03) * 0.5 + 0.5;
        const hue = (i + j + time * 20) % 360;
        ctx.fillStyle = `hsla(${hue}, 70%, 70%, ${0.02 + twinkle * 0.04})`;
        const offset = Math.sin(time + i * 0.02 + j * 0.02) * 2;
        ctx.beginPath();
        ctx.arc(i + offset, j + offset, 1 + twinkle, 0, Math.PI * 2);
        ctx.fill();
      }
    }

    // Subtle gradient overlay for depth
    const overlayGradient = ctx.createRadialGradient(
      canvas.width / 2, canvas.height / 2, 50,
      canvas.width / 2, canvas.height / 2, canvas.width
    );
    overlayGradient.addColorStop(0, 'rgba(255, 105, 180, 0.03)');
    overlayGradient.addColorStop(0.5, 'rgba(138, 43, 226, 0.02)');
    overlayGradient.addColorStop(1, 'transparent');
    ctx.fillStyle = overlayGradient;
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    // Draw obstacles (pits) with gamer girl danger aesthetic
    level.obstacles.forEach((obs) => {
      if (obs.type === 'pit') {
        // Pit depth gradient - purple/magenta tones
        const pitGradient = ctx.createLinearGradient(obs.x, obs.y, obs.x, obs.y + obs.height);
        pitGradient.addColorStop(0, '#1a0520');
        pitGradient.addColorStop(0.5, '#0a0010');
        pitGradient.addColorStop(1, '#000');
        ctx.fillStyle = pitGradient;
        ctx.fillRect(obs.x, obs.y, obs.width, obs.height);

        // Animated danger glow - pink/magenta
        const glowIntensity = 0.4 + Math.sin(time * 3) * 0.15;
        ctx.shadowColor = `rgba(255, 0, 128, ${glowIntensity})`;
        ctx.shadowBlur = 20;

        // Danger stripes - pink themed
        for (let i = 0; i < obs.width; i += 20) {
          const stripeAlpha = 0.2 + Math.sin(time * 2 + i * 0.1) * 0.1;
          ctx.fillStyle = `rgba(255, 105, 180, ${stripeAlpha})`;
          ctx.beginPath();
          ctx.moveTo(obs.x + i, obs.y);
          ctx.lineTo(obs.x + i + 10, obs.y);
          ctx.lineTo(obs.x + i - 10, obs.y + 20);
          ctx.lineTo(obs.x + i - 20, obs.y + 20);
          ctx.fill();
        }
        ctx.shadowBlur = 0;

        // Spooky eyes in the pit
        const eyeCount = Math.floor(obs.width / 40);
        for (let i = 0; i < eyeCount; i++) {
          const eyeX = obs.x + 20 + i * 40 + Math.sin(time * 2 + i) * 5;
          const eyeY = obs.y + 30 + Math.sin(time * 1.5 + i * 2) * 10;
          const blink = Math.sin(time * 3 + i * 1.5) > 0.9 ? 0.2 : 1;

          // Eye glow
          ctx.fillStyle = `rgba(255, 0, 128, ${0.3 * blink})`;
          ctx.beginPath();
          ctx.arc(eyeX, eyeY, 12, 0, Math.PI * 2);
          ctx.fill();

          // Eye
          ctx.fillStyle = `rgba(255, 100, 150, ${0.8 * blink})`;
          ctx.beginPath();
          ctx.ellipse(eyeX, eyeY, 6, 8 * blink, 0, 0, Math.PI * 2);
          ctx.fill();

          // Pupil
          ctx.fillStyle = `rgba(100, 0, 50, ${blink})`;
          ctx.beginPath();
          ctx.arc(eyeX, eyeY + 2, 3, 0, Math.PI * 2);
          ctx.fill();
        }

        // Pit edge with glow
        ctx.strokeStyle = '#FF69B4';
        ctx.lineWidth = 3;
        ctx.setLineDash([8, 4]);
        ctx.beginPath();
        ctx.moveTo(obs.x, obs.y);
        ctx.lineTo(obs.x + obs.width, obs.y);
        ctx.stroke();
        ctx.setLineDash([]);

        // Corner decorations
        ctx.fillStyle = '#FF69B4';
        ctx.beginPath();
        ctx.moveTo(obs.x, obs.y);
        ctx.lineTo(obs.x + 10, obs.y);
        ctx.lineTo(obs.x, obs.y + 15);
        ctx.fill();
        ctx.beginPath();
        ctx.moveTo(obs.x + obs.width, obs.y);
        ctx.lineTo(obs.x + obs.width - 10, obs.y);
        ctx.lineTo(obs.x + obs.width, obs.y + 15);
        ctx.fill();
      }
    });

    // Draw platforms with cute aesthetic
    [level.startPlatform, level.endPlatform].forEach((platform, platformIdx) => {
      // Platform glow
      ctx.shadowColor = platformIdx === 0 ? 'rgba(255, 105, 180, 0.3)' : 'rgba(0, 206, 209, 0.3)';
      ctx.shadowBlur = 15;

      // Platform body with cute gradient
      const platGradient = ctx.createLinearGradient(
        platform.x,
        platform.y,
        platform.x,
        platform.y + 35
      );
      if (platformIdx === 0) {
        // Start platform - pink tones
        platGradient.addColorStop(0, '#FF69B4');
        platGradient.addColorStop(0.3, '#DB7093');
        platGradient.addColorStop(1, '#C71585');
      } else {
        // End platform - teal tones
        platGradient.addColorStop(0, '#40E0D0');
        platGradient.addColorStop(0.3, '#00CED1');
        platGradient.addColorStop(1, '#008B8B');
      }
      ctx.fillStyle = platGradient;

      // Rounded platform
      ctx.beginPath();
      ctx.roundRect(platform.x, platform.y, platform.width, 35, [10, 10, 4, 4]);
      ctx.fill();
      ctx.shadowBlur = 0;

      // Sparkly top edge
      ctx.strokeStyle = 'rgba(255, 255, 255, 0.5)';
      ctx.lineWidth = 2;
      ctx.beginPath();
      ctx.moveTo(platform.x + 5, platform.y + 2);
      ctx.lineTo(platform.x + platform.width - 5, platform.y + 2);
      ctx.stroke();

      // Cute flowers/decorations on top
      for (let i = 8; i < platform.width - 8; i += 12) {
        const flowerY = platform.y - 4;
        const bounce = Math.sin(time * 3 + i * 0.3) * 2;

        // Stem
        ctx.strokeStyle = platformIdx === 0 ? '#98FB98' : '#90EE90';
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(platform.x + i, platform.y);
        ctx.lineTo(platform.x + i, flowerY + bounce);
        ctx.stroke();

        // Flower/star
        const flowerHue = platformIdx === 0 ? 330 : 180;
        ctx.fillStyle = `hsl(${flowerHue + (i * 5) % 60}, 80%, 70%)`;
        ctx.beginPath();
        for (let p = 0; p < 5; p++) {
          const angle = (p * Math.PI * 2) / 5 - Math.PI / 2 + time;
          const r = 3;
          const x = platform.x + i + Math.cos(angle) * r;
          const y = flowerY + bounce + Math.sin(angle) * r;
          if (p === 0) ctx.moveTo(x, y);
          else ctx.lineTo(x, y);
        }
        ctx.closePath();
        ctx.fill();

        // Flower center
        ctx.fillStyle = '#FFE4B5';
        ctx.beginPath();
        ctx.arc(platform.x + i, flowerY + bounce, 2, 0, Math.PI * 2);
        ctx.fill();
      }

      // Cute pattern on platform
      ctx.fillStyle = 'rgba(255, 255, 255, 0.15)';
      for (let i = 0; i < platform.width; i += 15) {
        ctx.beginPath();
        ctx.arc(platform.x + 7 + i, platform.y + 18, 4, 0, Math.PI * 2);
        ctx.fill();
      }
    });

    // Draw goal with gamer girl animated glow
    const goal = level.goal;
    ctx.save();
    ctx.translate(goal.x, goal.y);

    // Multiple pulsing outer glows for rainbow effect
    for (let ring = 0; ring < 3; ring++) {
      const ringTime = time + ring * 0.5;
      const glowSize = 40 + ring * 15 + Math.sin(ringTime * 3) * 8;
      const hue = (time * 50 + ring * 60) % 360;
      const glowGradient = ctx.createRadialGradient(0, 0, 10, 0, 0, glowSize);
      glowGradient.addColorStop(0, `hsla(${hue}, 100%, 70%, ${0.4 - ring * 0.1})`);
      glowGradient.addColorStop(0.5, `hsla(${hue}, 100%, 60%, ${0.15 - ring * 0.04})`);
      glowGradient.addColorStop(1, 'transparent');
      ctx.fillStyle = glowGradient;
      ctx.beginPath();
      ctx.arc(0, 0, glowSize, 0, Math.PI * 2);
      ctx.fill();
    }

    // Rotating star with pink/purple gradient
    ctx.rotate(time * 0.8);
    const starGradient = ctx.createRadialGradient(0, 0, 5, 0, 0, 25);
    starGradient.addColorStop(0, '#FFB6C1');
    starGradient.addColorStop(0.5, '#FF69B4');
    starGradient.addColorStop(1, '#DA70D6');
    ctx.fillStyle = starGradient;

    ctx.beginPath();
    for (let i = 0; i < 10; i++) {
      const angle = (i * Math.PI) / 5 - Math.PI / 2;
      const r = i % 2 === 0 ? 25 : 12;
      if (i === 0) {
        ctx.moveTo(Math.cos(angle) * r, Math.sin(angle) * r);
      } else {
        ctx.lineTo(Math.cos(angle) * r, Math.sin(angle) * r);
      }
    }
    ctx.closePath();
    ctx.fill();

    // Sparkle outline
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.6)';
    ctx.lineWidth = 2;
    ctx.stroke();

    // Star highlight
    ctx.fillStyle = 'rgba(255, 255, 255, 0.6)';
    ctx.beginPath();
    ctx.arc(-5, -5, 6, 0, Math.PI * 2);
    ctx.fill();

    // Inner sparkle
    ctx.fillStyle = 'white';
    ctx.beginPath();
    ctx.arc(-3, -3, 3, 0, Math.PI * 2);
    ctx.fill();

    ctx.restore();

    // Draw connections with glow effect - gamer girl colors
    connectionsRef.current.forEach((conn) => {
      const gooA = gooBallsRef.current.find((g) => g.id === conn.from);
      const gooB = gooBallsRef.current.find((g) => g.id === conn.to);
      if (!gooA || !gooB) return;

      // Calculate strand stress (for color)
      const dx = gooB.x - gooA.x;
      const dy = gooB.y - gooA.y;
      const distance = Math.sqrt(dx * dx + dy * dy);
      const restLength = conn.restLength || CONNECTION_DISTANCE;
      const stress = Math.min(1, Math.abs(distance - restLength) / (STRAND_BREAK_DISTANCE - restLength));

      // Get colors from connected goo balls
      const colorA = getGooColor(gooA.id, gooA.anchored);
      const colorB = getGooColor(gooB.id, gooB.anchored);

      // Connection glow
      ctx.strokeStyle = `rgba(255, 105, 180, ${0.3 - stress * 0.15})`;
      ctx.lineWidth = 8;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(gooA.x, gooA.y);
      ctx.lineTo(gooB.x, gooB.y);
      ctx.stroke();

      // Main connection with gradient between ball colors
      const connGradient = ctx.createLinearGradient(gooA.x, gooA.y, gooB.x, gooB.y);
      connGradient.addColorStop(0, colorA.color2);
      connGradient.addColorStop(1, colorB.color2);
      ctx.strokeStyle = connGradient;
      ctx.lineWidth = 4 - stress * 1.5; // Thinner when stressed
      ctx.beginPath();
      ctx.moveTo(gooA.x, gooA.y);
      ctx.lineTo(gooB.x, gooB.y);
      ctx.stroke();

      // Inner highlight
      ctx.strokeStyle = `rgba(255, 255, 255, ${0.3 - stress * 0.2})`;
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.moveTo(gooA.x, gooA.y);
      ctx.lineTo(gooB.x, gooB.y);
      ctx.stroke();
    });

    // Draw potential connections when dragging (World of Goo style - show all possible connections)
    if (draggingGoo) {
      let hasValidConnection = false;

      gooBallsRef.current.forEach((goo) => {
        const dx = draggingGoo.x - goo.x;
        const dy = draggingGoo.y - goo.y;
        const dist = Math.sqrt(dx * dx + dy * dy);

        // Check connection limits
        const existingConnections = connectionsRef.current.filter(
          (c) => c.from === goo.id || c.to === goo.id
        ).length;
        const canConnect = existingConnections < MAX_CONNECTIONS_PER_GOO;

        const isInRange = dist < MAX_CONNECTION_DISTANCE && dist > MIN_CONNECTION_DISTANCE;
        const isValid = isInRange && canConnect;

        if (isInRange) {
          const gooColor = getGooColor(goo.id, goo.anchored);

          // Draw potential connection line
          ctx.strokeStyle = isValid
            ? `rgba(255, 105, 180, 0.5)`
            : `rgba(255, 100, 100, 0.3)`;
          ctx.lineWidth = isValid ? 3 : 2;
          ctx.setLineDash([6, 6]);
          ctx.beginPath();
          ctx.moveTo(goo.x, goo.y);
          ctx.lineTo(draggingGoo.x, draggingGoo.y);
          ctx.stroke();
          ctx.setLineDash([]);

          if (isValid) {
            hasValidConnection = true;
            // Draw connection indicator at the goo
            ctx.fillStyle = gooColor.glow;
            ctx.beginPath();
            ctx.arc(goo.x, goo.y, GOO_RADIUS + 5 + Math.sin(time * 5) * 3, 0, Math.PI * 2);
            ctx.fill();
          }
        }
      });

      // Show overall validity indicator near dragging goo
      ctx.fillStyle = hasValidConnection ? 'rgba(255, 105, 180, 0.9)' : 'rgba(255, 100, 100, 0.9)';
      ctx.font = 'bold 14px system-ui';
      ctx.textAlign = 'center';
      ctx.fillText(hasValidConnection ? '✓' : '✗', draggingGoo.x, draggingGoo.y - GOO_RADIUS - 10);
    }

    // Draw goo balls with rotating gamer girl colors
    gooBallsRef.current.forEach((goo) => {
      if (goo.falling) return;

      ctx.save();
      ctx.translate(goo.x, goo.y);

      // Get rotating color based on goo id
      const colors = getGooColor(goo.id, goo.anchored);
      const pulseScale = 1 + Math.sin(goo.pulse) * 0.08;

      // Outer glow (more vibrant)
      const glowGrad = ctx.createRadialGradient(0, 0, GOO_RADIUS * 0.3, 0, 0, GOO_RADIUS * 2.5);
      glowGrad.addColorStop(0, colors.glow);
      glowGrad.addColorStop(0.5, colors.glow.replace('0.6', '0.2').replace('0.7', '0.3'));
      glowGrad.addColorStop(1, 'transparent');
      ctx.fillStyle = glowGrad;
      ctx.beginPath();
      ctx.arc(0, 0, GOO_RADIUS * 2.5 * pulseScale, 0, Math.PI * 2);
      ctx.fill();

      // Outer ring for anchored goo
      if (goo.anchored) {
        ctx.strokeStyle = colors.color1;
        ctx.lineWidth = 2;
        ctx.setLineDash([3, 3]);
        ctx.beginPath();
        ctx.arc(0, 0, GOO_RADIUS * 1.4, 0, Math.PI * 2);
        ctx.stroke();
        ctx.setLineDash([]);
      }

      // Body with nice gradient
      const bodyGradient = ctx.createRadialGradient(-3, -3, 0, 0, 0, GOO_RADIUS);
      bodyGradient.addColorStop(0, colors.color1);
      bodyGradient.addColorStop(0.7, colors.color2);
      bodyGradient.addColorStop(1, colors.color2);
      ctx.fillStyle = bodyGradient;
      ctx.beginPath();
      ctx.arc(0, 0, GOO_RADIUS * pulseScale, 0, Math.PI * 2);
      ctx.fill();

      // Glossy highlight
      ctx.fillStyle = 'rgba(255, 255, 255, 0.5)';
      ctx.beginPath();
      ctx.ellipse(-3, -4, 5, 3, -0.4, 0, Math.PI * 2);
      ctx.fill();

      // Secondary highlight
      ctx.fillStyle = 'rgba(255, 255, 255, 0.25)';
      ctx.beginPath();
      ctx.ellipse(2, -6, 2.5, 1.5, 0.3, 0, Math.PI * 2);
      ctx.fill();

      // Eyes (follow dragging if nearby) - cuter style
      let eyeOffsetX = 0;
      let eyeOffsetY = 0;
      if (draggingGoo) {
        const dx = draggingGoo.x - goo.x;
        const dy = draggingGoo.y - goo.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < 100 && dist > 0) {
          eyeOffsetX = (dx / dist) * 2;
          eyeOffsetY = (dy / dist) * 2;
        }
      }

      // Eye whites
      ctx.fillStyle = 'white';
      ctx.beginPath();
      ctx.ellipse(-4, -1, 4.5, 5, 0, 0, Math.PI * 2);
      ctx.ellipse(4, -1, 4.5, 5, 0, 0, Math.PI * 2);
      ctx.fill();

      // Eye color (matches goo color but darker)
      ctx.fillStyle = colors.color2;
      ctx.beginPath();
      ctx.arc(-3.5 + eyeOffsetX, 0 + eyeOffsetY, 3, 0, Math.PI * 2);
      ctx.arc(4.5 + eyeOffsetX, 0 + eyeOffsetY, 3, 0, Math.PI * 2);
      ctx.fill();

      // Pupils
      ctx.fillStyle = '#1a1a2e';
      ctx.beginPath();
      ctx.arc(-3.5 + eyeOffsetX * 1.2, 0.5 + eyeOffsetY * 1.2, 1.8, 0, Math.PI * 2);
      ctx.arc(4.5 + eyeOffsetX * 1.2, 0.5 + eyeOffsetY * 1.2, 1.8, 0, Math.PI * 2);
      ctx.fill();

      // Eye shine
      ctx.fillStyle = 'white';
      ctx.beginPath();
      ctx.arc(-4.5 + eyeOffsetX * 0.5, -1, 1.2, 0, Math.PI * 2);
      ctx.arc(3.5 + eyeOffsetX * 0.5, -1, 1.2, 0, Math.PI * 2);
      ctx.fill();

      // Cute blush marks
      ctx.fillStyle = 'rgba(255, 150, 180, 0.4)';
      ctx.beginPath();
      ctx.ellipse(-7, 3, 2.5, 1.5, 0, 0, Math.PI * 2);
      ctx.ellipse(7, 3, 2.5, 1.5, 0, 0, Math.PI * 2);
      ctx.fill();

      // Smile - cuter curved smile
      ctx.strokeStyle = colors.color2;
      ctx.lineWidth = 1.5;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.arc(0, 4, 4, 0.3, Math.PI - 0.3);
      ctx.stroke();

      ctx.restore();
    });

    // Draw dragging goo preview with gamer girl colors
    if (draggingGoo) {
      ctx.save();
      ctx.translate(draggingGoo.x, draggingGoo.y);
      ctx.globalAlpha = 0.8;

      // Get color for new goo
      const previewColor = getGooColor(draggingGoo.id);

      // Glow
      const glowGrad = ctx.createRadialGradient(0, 0, GOO_RADIUS * 0.3, 0, 0, GOO_RADIUS * 2);
      glowGrad.addColorStop(0, previewColor.glow);
      glowGrad.addColorStop(1, 'transparent');
      ctx.fillStyle = glowGrad;
      ctx.beginPath();
      ctx.arc(0, 0, GOO_RADIUS * 2, 0, Math.PI * 2);
      ctx.fill();

      // Body
      const bodyGradient = ctx.createRadialGradient(-3, -3, 0, 0, 0, GOO_RADIUS);
      bodyGradient.addColorStop(0, previewColor.color1);
      bodyGradient.addColorStop(1, previewColor.color2);
      ctx.fillStyle = bodyGradient;
      ctx.beginPath();
      ctx.arc(0, 0, GOO_RADIUS, 0, Math.PI * 2);
      ctx.fill();

      // Highlight
      ctx.fillStyle = 'rgba(255, 255, 255, 0.4)';
      ctx.beginPath();
      ctx.ellipse(-3, -4, 4, 2.5, -0.4, 0, Math.PI * 2);
      ctx.fill();

      // Eyes
      ctx.fillStyle = 'white';
      ctx.beginPath();
      ctx.ellipse(-4, -1, 4, 4.5, 0, 0, Math.PI * 2);
      ctx.ellipse(4, -1, 4, 4.5, 0, 0, Math.PI * 2);
      ctx.fill();

      ctx.fillStyle = previewColor.color2;
      ctx.beginPath();
      ctx.arc(-3.5, 0, 2.5, 0, Math.PI * 2);
      ctx.arc(4.5, 0, 2.5, 0, Math.PI * 2);
      ctx.fill();

      ctx.fillStyle = '#1a1a2e';
      ctx.beginPath();
      ctx.arc(-3.5, 0.5, 1.5, 0, Math.PI * 2);
      ctx.arc(4.5, 0.5, 1.5, 0, Math.PI * 2);
      ctx.fill();

      // Blush
      ctx.fillStyle = 'rgba(255, 150, 180, 0.3)';
      ctx.beginPath();
      ctx.ellipse(-7, 3, 2, 1.2, 0, 0, Math.PI * 2);
      ctx.ellipse(7, 3, 2, 1.2, 0, 0, Math.PI * 2);
      ctx.fill();

      ctx.restore();
    }

    // Draw particles
    particlesRef.current.forEach((p) => {
      ctx.globalAlpha = p.life;
      ctx.fillStyle = p.color;
      ctx.beginPath();
      ctx.arc(p.x, p.y, p.size * p.life, 0, Math.PI * 2);
      ctx.fill();
    });
    ctx.globalAlpha = 1;
  }, [currentLevel, draggingGoo, selectedGoo]);

  // Game loop with delta time
  useEffect(() => {
    if (gameState !== 'playing') return;

    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    ctxRef.current = ctx;

    // Set canvas size with device pixel ratio
    const container = canvas.parentElement;
    if (container) {
      const dpr = window.devicePixelRatio || 1;
      const width = Math.min(container.clientWidth, 420);
      const height = Math.min(container.clientHeight - 100, 450);
      canvas.width = width * dpr;
      canvas.height = height * dpr;
      canvas.style.width = `${width}px`;
      canvas.style.height = `${height}px`;
      ctx.scale(dpr, dpr);
    }

    lastTimeRef.current = performance.now();

    const gameLoop = (timestamp) => {
      const deltaTime = timestamp - lastTimeRef.current;
      lastTimeRef.current = timestamp;

      updatePhysics(deltaTime);
      renderGame();
      animationFrameRef.current = requestAnimationFrame(gameLoop);
    };

    animationFrameRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    };
  }, [gameState, updatePhysics, renderGame]);

  // Handle pointer down
  const handlePointerDown = useCallback(
    (e) => {
      if (gameState !== 'playing') return;

      const canvas = canvasRef.current;
      if (!canvas) return;

      const rect = canvas.getBoundingClientRect();
      const x = (e.clientX || e.touches?.[0]?.clientX) - rect.left;
      const y = (e.clientY || e.touches?.[0]?.clientY) - rect.top;

      // Check if clicking on existing goo
      const clickedGoo = gooBallsRef.current.find((goo) => {
        const dx = goo.x - x;
        const dy = goo.y - y;
        return Math.sqrt(dx * dx + dy * dy) < GOO_RADIUS + 8;
      });

      if (clickedGoo) {
        setSelectedGoo(clickedGoo.id);
        dragOffsetRef.current = { x: clickedGoo.x - x, y: clickedGoo.y - y };
      } else if (availableGoo > 0) {
        // Start placing new goo
        setDraggingGoo({ x, y, id: Date.now() });
        setSelectedGoo(null);

        // Find closest goo to connect to
        let closestGoo = null;
        let closestDist = CONNECTION_DISTANCE;

        gooBallsRef.current.forEach((goo) => {
          const dx = goo.x - x;
          const dy = goo.y - y;
          const dist = Math.sqrt(dx * dx + dy * dy);
          if (dist < closestDist) {
            closestDist = dist;
            closestGoo = goo;
          }
        });

        if (closestGoo) {
          setSelectedGoo(closestGoo.id);
        }
      }
    },
    [gameState, availableGoo]
  );

  // Handle pointer move
  const handlePointerMove = useCallback(
    (e) => {
      if (!draggingGoo && selectedGoo === null) return;

      const canvas = canvasRef.current;
      if (!canvas) return;

      const rect = canvas.getBoundingClientRect();
      const x = (e.clientX || e.touches?.[0]?.clientX) - rect.left;
      const y = (e.clientY || e.touches?.[0]?.clientY) - rect.top;

      if (draggingGoo) {
        setDraggingGoo((prev) => ({ ...prev, x, y }));
      }
    },
    [draggingGoo, selectedGoo]
  );

  // Handle pointer up - World of Goo style connection logic
  const handlePointerUp = useCallback(() => {
    if (draggingGoo && availableGoo > 0) {
      // Find all nearby goo balls that can be connected
      const nearbyGoos = [];
      gooBallsRef.current.forEach((goo) => {
        const dx = draggingGoo.x - goo.x;
        const dy = draggingGoo.y - goo.y;
        const dist = Math.sqrt(dx * dx + dy * dy);

        if (dist < MAX_CONNECTION_DISTANCE && dist > MIN_CONNECTION_DISTANCE) {
          // Count existing connections for this goo
          const existingConnections = connectionsRef.current.filter(
            (c) => c.from === goo.id || c.to === goo.id
          ).length;

          // Only connect if goo doesn't have too many connections
          if (existingConnections < MAX_CONNECTIONS_PER_GOO) {
            nearbyGoos.push({ goo, dist });
          }
        }
      });

      // Sort by distance and take closest ones (up to 3 connections for triangular stability)
      nearbyGoos.sort((a, b) => a.dist - b.dist);
      const connectTo = nearbyGoos.slice(0, 3);

      // Need at least one connection to place goo
      if (connectTo.length >= 1) {
        // Add new goo
        const newGoo = {
          id: draggingGoo.id,
          x: draggingGoo.x,
          y: draggingGoo.y,
          vx: 0,
          vy: 0,
          fx: 0,
          fy: 0,
          anchored: false,
          type: 'normal',
          pulse: Math.random() * Math.PI * 2,
        };

        gooBallsRef.current.push(newGoo);

        // Add connections with rest lengths (use actual distance for stability)
        connectTo.forEach(({ goo, dist }) => {
          connectionsRef.current.push({
            from: goo.id,
            to: newGoo.id,
            restLength: dist, // Use actual distance for stable structures
          });
        });

        setConnections([...connectionsRef.current]);
        setGooBalls([...gooBallsRef.current]);
        setAvailableGoo((prev) => prev - 1);
        gooUsedRef.current += 1;
        setGooUsed(gooUsedRef.current);

        // Particle effect with gamer girl colors
        const newGooColor = getGooColor(newGoo.id);
        createParticles(draggingGoo.x, draggingGoo.y, newGooColor.color1, 8);
      }
    }

    setDraggingGoo(null);
    setSelectedGoo(null);
  }, [draggingGoo, availableGoo, createParticles]);

  // Next level
  const nextLevel = useCallback(() => {
    const next = currentLevel + 1;
    setCurrentLevel(next);
    setGameState('playing');
    initializeLevel(next);
  }, [currentLevel, initializeLevel]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  const restartGame = useCallback(() => {
    setGameState('playing');
    initializeLevel(currentLevel);
  }, [currentLevel, initializeLevel]);

  return (
    <div className="goo-bridge-overlay">
      <div className="goo-bridge-container">
        {/* Menu */}
        <AnimatePresence>
          {gameState === 'menu' && (
            <motion.div
              className="goo-menu"
              initial={{ opacity: 0, y: 20 }}
              animate={{ opacity: 1, y: 0 }}
              exit={{ opacity: 0, y: -20 }}
            >
              <h1 className="goo-title">
                <span>🟢</span> Goo Bridge <span>🟢</span>
              </h1>
              <p className="goo-subtitle">Build bridges with goo balls!</p>
              <div className="goo-instructions-box">
                <p>🎯 Tap and drag to place goo balls</p>
                <p>🔗 Connect them to build a bridge</p>
                <p>⭐ Reach the star to complete each level!</p>
                <p>⏱️ Finish fast for time bonus!</p>
              </div>
              <motion.button
                className="start-button"
                onClick={startGame}
                whileHover={{ scale: 1.05 }}
                whileTap={{ scale: 0.95 }}
              >
                Start Building!
              </motion.button>
              <button className="close-button-menu" onClick={onClose}>
                Back to Games
              </button>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Playing */}
        {gameState === 'playing' && (
          <>
            <div className="game-header">
              <div className="level-info">
                <span className="level-number">Level {currentLevel + 1}</span>
                <span className="level-name">{LEVELS[currentLevel]?.name}</span>
              </div>
              <div className={`game-timer ${timeLeft <= 10 ? 'warning' : ''}`}>
                <span className="timer-icon">⏱️</span>
                <span className="timer-value">{timeLeft}s</span>
              </div>
              <div className="game-stat">
                <span className="stat-icon">🪙</span>
                <span className="stat-value">{score}</span>
              </div>
            </div>

            <div className="canvas-container">
              <canvas
                ref={canvasRef}
                onMouseDown={handlePointerDown}
                onMouseMove={handlePointerMove}
                onMouseUp={handlePointerUp}
                onMouseLeave={handlePointerUp}
                onTouchStart={handlePointerDown}
                onTouchMove={handlePointerMove}
                onTouchEnd={handlePointerUp}
              />
            </div>

            <div className="game-footer">
              <div className="goo-remaining">
                <span className="goo-icon">🟢</span>
                <span className="goo-count">{availableGoo}</span>
                <span className="goo-label">Goo Left</span>
              </div>
              <div className="goo-used-info">
                <span className="goo-used-label">Used:</span>
                <span className="goo-used-count">{gooUsed}</span>
              </div>
            </div>

            <AnimatePresence>
              {showHint && LEVELS[currentLevel]?.hint && (
                <motion.div
                  className="hint-popup"
                  initial={{ opacity: 0, y: 20 }}
                  animate={{ opacity: 1, y: 0 }}
                  exit={{ opacity: 0, y: -20 }}
                >
                  💡 {LEVELS[currentLevel].hint}
                </motion.div>
              )}
            </AnimatePresence>

            {goalReached && (
              <motion.div
                className="goal-reached-flash"
                initial={{ scale: 0 }}
                animate={{ scale: 1 }}
              >
                <span>⭐</span> Level Complete! <span>⭐</span>
              </motion.div>
            )}
          </>
        )}

        {/* Game Over (timeout) */}
        <AnimatePresence>
          {gameState === 'gameover' && (
            <motion.div
              className="game-over-screen"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
            >
              <motion.div
                className="game-over-content"
                initial={{ scale: 0.8, y: 50 }}
                animate={{ scale: 1, y: 0 }}
              >
                <h2 className="game-over-title">
                  <span>⏱️</span> Time's Up! <span>⏱️</span>
                </h2>
                <p className="timeout-message">The bridge collapsed!</p>
                <div className="game-over-buttons">
                  <button className="retry-btn" onClick={restartGame}>
                    Try Again
                  </button>
                  <button className="claim-rewards-btn" onClick={handleFinish}>
                    Claim {score} Coins! 🪙
                  </button>
                </div>
              </motion.div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Level Complete */}
        <AnimatePresence>
          {gameState === 'levelComplete' && (
            <motion.div
              className="level-complete-screen"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
            >
              <motion.div
                className="level-complete-content"
                initial={{ scale: 0.8, y: 50 }}
                animate={{ scale: 1, y: 0 }}
              >
                <h2>
                  <span>⭐</span> Level Complete! <span>⭐</span>
                </h2>
                <div className="level-rewards">
                  <p className="coins-earned">+{COINS_PER_LEVEL} Coins!</p>
                  {timeLeft > TIME_BONUS_THRESHOLD && (
                    <p className="bonus-earned">⏱️ Time Bonus: +{TIME_BONUS}</p>
                  )}
                  {gooUsed <= LEVELS[currentLevel].minGoo + 2 && (
                    <p className="bonus-earned">🎯 Efficiency Bonus: +{PERFECT_BONUS}</p>
                  )}
                </div>
                <motion.button
                  className="next-level-btn"
                  onClick={nextLevel}
                  whileHover={{ scale: 1.05 }}
                  whileTap={{ scale: 0.95 }}
                >
                  Next Level →
                </motion.button>
              </motion.div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Game Complete */}
        <AnimatePresence>
          {gameState === 'gameComplete' && (
            <motion.div
              className="game-over-screen"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
            >
              <motion.div
                className="game-over-content"
                initial={{ scale: 0.8, y: 50 }}
                animate={{ scale: 1, y: 0 }}
              >
                <h2 className="game-over-title">
                  <span>🏆</span> All Levels Complete! <span>🏆</span>
                </h2>

                <div className="final-stats">
                  <div className="final-stat">
                    <span className="final-stat-icon">📊</span>
                    <span className="final-stat-label">Levels Beaten</span>
                    <span className="final-stat-value">{LEVELS.length}</span>
                  </div>

                  <div className="final-stat highlight">
                    <span className="final-stat-icon">🪙</span>
                    <span className="final-stat-label">Total Score</span>
                    <span className="final-stat-value coins">{score}</span>
                  </div>
                </div>

                <div className="achievement-badge">
                  <span>🌟</span> Master Builder! <span>🌟</span>
                </div>

                <motion.button
                  className="claim-rewards-btn"
                  onClick={handleFinish}
                  whileHover={{ scale: 1.02 }}
                  whileTap={{ scale: 0.98 }}
                >
                  Claim {score} Coins! 🪙
                </motion.button>
              </motion.div>
            </motion.div>
          )}
        </AnimatePresence>
      </div>
    </div>
  );
};

export default GooBridgeGame;
