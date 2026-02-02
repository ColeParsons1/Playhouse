import { useState, useEffect, useCallback, useRef } from 'react';
import './FlappyBirdGame.css';

const GAME_DURATION = 35000;
const GRAVITY = 0.28;
const FLAP_POWER = -6.5;
const PIPE_SPEED = 3.2;
const PIPE_WIDTH = 65;
const PIPE_GAP = 200;
const PIPE_SPAWN_INTERVAL = 2000;
const COIN_SPAWN_CHANCE = 0.6;
const POWERUP_SPAWN_CHANCE = 0.18;
const SHARK_SIZE = 42;
const COIN_VALUE = 1; // Reduced from 3
const PIPE_PASS_VALUE = 2; // Reduced from 5

// Baby shark colors
const SHARK_COLORS = [
  { body: '#4DA6FF', belly: '#B8E0FF', fin: '#2980B9' }, // Classic blue
  { body: '#FF6B9D', belly: '#FFB8D4', fin: '#E84393' }, // Pink
  { body: '#9B59B6', belly: '#D4A8E8', fin: '#6C3483' }, // Purple
  { body: '#2ECC71', belly: '#A8E6CF', fin: '#1ABC9C' }, // Green
  { body: '#F39C12', belly: '#FFEAA7', fin: '#E67E22' }, // Orange
];

const FlappyBirdGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [score, setScore] = useState(0);
  const [pipesPassed, setPipesPassed] = useState(0);
  const [activePowerup, setActivePowerup] = useState(null);
  const [combo, setCombo] = useState(0);

  // Refs for game state
  const scoreRef = useRef(0);
  const pipesPassedRef = useRef(0);
  const comboRef = useRef(0);
  const sharkRef = useRef({ y: 250, velocity: 0 });
  const pipesRef = useRef([]);
  const coinsRef = useRef([]);
  const powerupsRef = useRef([]);
  const particlesRef = useRef([]);
  const bubblesRef = useRef([]);
  const shieldActiveRef = useRef(false);
  const slowMotionRef = useRef(false);
  const smallSharkRef = useRef(false);
  const magnetActiveRef = useRef(false);
  const activePowerupRef = useRef(null);
  const sharkColorRef = useRef(SHARK_COLORS[Math.floor(Math.random() * SHARK_COLORS.length)]);

  const gameAreaRef = useRef(null);
  const canvasRef = useRef(null);
  const ctxRef = useRef(null);
  const gameStateRef = useRef('countdown');
  const animationFrameRef = useRef(null);
  const lastPipeSpawnRef = useRef(0);
  const waterOffsetRef = useRef(0);
  const tailAngleRef = useRef(0);
  const timeRef = useRef(0);
  const lastCoinTimeRef = useRef(0);

  // Initialize bubbles
  useEffect(() => {
    const bubbles = [];
    for (let i = 0; i < 25; i++) {
      bubbles.push({
        x: Math.random() * 500,
        y: Math.random() * 600,
        size: Math.random() * 8 + 3,
        speed: Math.random() * 1.5 + 0.5,
        wobble: Math.random() * 2,
        opacity: Math.random() * 0.4 + 0.2
      });
    }
    bubblesRef.current = bubbles;
  }, []);

  useEffect(() => {
    gameStateRef.current = gameState;
  }, [gameState]);

  // Initialize canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext('2d');
    ctxRef.current = ctx;

    const container = gameAreaRef.current;
    if (container) {
      canvas.width = container.clientWidth;
      canvas.height = container.clientHeight;
      sharkRef.current.y = canvas.height / 2;
    }

    renderGame();
  }, []);

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

  // Main game loop
  useEffect(() => {
    if (gameState !== 'playing' && gameState !== 'countdown') return;

    const gameLoop = () => {
      timeRef.current += 0.016;
      if (gameStateRef.current === 'playing') {
        updateGame();
      }
      renderGame();
      animationFrameRef.current = requestAnimationFrame(gameLoop);
    };

    animationFrameRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    };
  }, [gameState]);

  const swim = useCallback(() => {
    if (gameStateRef.current !== 'playing') return;
    sharkRef.current.velocity = FLAP_POWER;
    tailAngleRef.current = -25;

    // Create bubble particles when swimming
    const canvas = canvasRef.current;
    if (canvas) {
      const colors = ['rgba(255, 255, 255, 0.8)', 'rgba(200, 240, 255, 0.7)', 'rgba(150, 220, 255, 0.6)'];
      for (let i = 0; i < 6; i++) {
        const angle = Math.PI + (Math.random() - 0.5) * 1.2;
        const speed = Math.random() * 3 + 2;
        particlesRef.current.push({
          x: 80 - 20,
          y: sharkRef.current.y + SHARK_SIZE / 2,
          vx: Math.cos(angle) * speed,
          vy: Math.sin(angle) * speed - 1,
          size: Math.random() * 8 + 4,
          color: colors[Math.floor(Math.random() * colors.length)],
          life: 1,
          type: 'bubble'
        });
      }
    }
  }, []);

  const updateGame = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const speed = slowMotionRef.current ? PIPE_SPEED * 0.5 : PIPE_SPEED;
    const gravity = slowMotionRef.current ? GRAVITY * 0.5 : GRAVITY;

    // Update shark (buoyancy in water - less gravity effect)
    sharkRef.current.velocity += gravity;
    sharkRef.current.velocity = Math.max(-10, Math.min(10, sharkRef.current.velocity));
    sharkRef.current.y += sharkRef.current.velocity;

    // Tail animation
    tailAngleRef.current += (15 - tailAngleRef.current) * 0.1;

    // Water current offset
    waterOffsetRef.current = (waterOffsetRef.current + speed * 0.5) % canvas.width;

    // Update bubbles
    bubblesRef.current.forEach(b => {
      b.y -= b.speed;
      b.x += Math.sin(timeRef.current * b.wobble + b.y * 0.02) * 0.5;
      if (b.y < -20) {
        b.y = canvas.height + 20;
        b.x = Math.random() * canvas.width;
      }
    });

    // Spawn seaweed/coral obstacles
    const now = Date.now();
    if (now - lastPipeSpawnRef.current > PIPE_SPAWN_INTERVAL) {
      const gapY = Math.random() * (canvas.height - PIPE_GAP - 200) + 100;
      pipesRef.current.push({
        x: canvas.width,
        gapY,
        passed: false,
        id: now,
        type: Math.random() < 0.3 ? 'coral' : 'seaweed',
        color: Math.random() < 0.2 ? 'gold' : 'normal'
      });

      // Spawn coins (shells/pearls)
      if (Math.random() < COIN_SPAWN_CHANCE) {
        const pattern = Math.floor(Math.random() * 4);
        const centerY = gapY + PIPE_GAP / 2;

        if (pattern === 0) {
          coinsRef.current.push(createCoin(canvas.width + PIPE_WIDTH / 2, centerY, now));
        } else if (pattern === 1) {
          for (let i = -1; i <= 1; i++) {
            coinsRef.current.push(createCoin(canvas.width + PIPE_WIDTH / 2, centerY + i * 35, now + i));
          }
        } else if (pattern === 2) {
          for (let i = 0; i < 3; i++) {
            coinsRef.current.push(createCoin(canvas.width + PIPE_WIDTH / 2 + i * 30, centerY, now + i));
          }
        } else {
          for (let i = -1; i <= 1; i++) {
            const offsetY = Math.abs(i) * 25;
            coinsRef.current.push(createCoin(canvas.width + PIPE_WIDTH / 2 + i * 30, centerY - offsetY, now + i));
          }
        }
      }

      // Spawn powerup
      if (Math.random() < POWERUP_SPAWN_CHANCE) {
        const types = ['shield', 'slow', 'small', 'magnet'];
        const type = types[Math.floor(Math.random() * types.length)];
        powerupsRef.current.push({
          x: canvas.width + PIPE_WIDTH / 2 + 60,
          y: gapY + PIPE_GAP / 2 + (Math.random() - 0.5) * 80,
          type,
          collected: false,
          id: now + 1,
          pulse: 0,
          rotation: 0
        });
      }

      lastPipeSpawnRef.current = now;
    }

    // Update obstacles
    const sharkSize = smallSharkRef.current ? SHARK_SIZE * 0.55 : SHARK_SIZE;
    const sharkX = 80;
    const sharkY = sharkRef.current.y;
    const sharkRadius = sharkSize / 2 - 8;

    pipesRef.current.forEach(pipe => {
      pipe.x -= speed;

      // Check collision
      if (!shieldActiveRef.current) {
        const pipeLeft = pipe.x;
        const pipeRight = pipe.x + PIPE_WIDTH;

        if (sharkX + sharkRadius > pipeLeft && sharkX - sharkRadius < pipeRight) {
          if (sharkY - sharkRadius < pipe.gapY || sharkY + sharkRadius > pipe.gapY + PIPE_GAP) {
            handleCollision();
          }
        }
      }

      // Score for passing
      if (!pipe.passed && pipe.x + PIPE_WIDTH < sharkX) {
        pipe.passed = true;
        pipesPassedRef.current += 1;
        setPipesPassed(pipesPassedRef.current);

        const bonusMultiplier = pipe.color === 'gold' ? 2 : 1;
        const points = PIPE_PASS_VALUE * bonusMultiplier;
        scoreRef.current += points;
        setScore(scoreRef.current);

        createScoreParticles(sharkX, sharkY, pipe.color === 'gold' ? '#FFD700' : '#00CED1');
      }
    });

    // Update coins with magnet effect
    coinsRef.current.forEach(coin => {
      coin.x -= speed;
      coin.rotation += 0.08;
      coin.floatOffset = Math.sin(timeRef.current * 3 + coin.id) * 4;

      // Magnet attraction
      if (magnetActiveRef.current && !coin.collected) {
        const dx = sharkX - coin.x;
        const dy = sharkY - coin.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        if (dist < 150) {
          const force = (150 - dist) / 150 * 0.3;
          coin.x += dx / dist * force * 10;
          coin.y += dy / dist * force * 10;
        }
      }

      // Check collection
      if (!coin.collected) {
        const dx = coin.x - sharkX;
        const dy = coin.y - sharkY;
        if (Math.sqrt(dx * dx + dy * dy) < sharkRadius + 18) {
          coin.collected = true;

          const currentTime = Date.now();
          if (currentTime - lastCoinTimeRef.current < 800) {
            comboRef.current = Math.min(comboRef.current + 1, 5);
          } else {
            comboRef.current = 1;
          }
          lastCoinTimeRef.current = currentTime;
          setCombo(comboRef.current);

          const points = COIN_VALUE * comboRef.current;
          scoreRef.current += points;
          setScore(scoreRef.current);

          createCoinParticles(coin.x, coin.y);
        }
      }
    });

    // Update powerups
    powerupsRef.current.forEach(powerup => {
      powerup.x -= speed;
      powerup.pulse = (powerup.pulse + 0.12) % (Math.PI * 2);
      powerup.rotation += 0.03;

      if (!powerup.collected) {
        const dx = powerup.x - sharkX;
        const dy = powerup.y - sharkY;
        if (Math.sqrt(dx * dx + dy * dy) < sharkRadius + 25) {
          powerup.collected = true;
          activatePowerup(powerup.type);
          createPowerupParticles(powerup.x, powerup.y, powerup.type);
        }
      }
    });

    // Update particles
    particlesRef.current = particlesRef.current.filter(p => {
      p.x += p.vx;
      p.y += p.vy;
      p.life -= 0.02;
      if (p.type === 'bubble') {
        p.vy -= 0.05; // Bubbles float up
      } else {
        p.vy += 0.05;
      }
      return p.life > 0;
    });

    // Clean up off-screen objects
    pipesRef.current = pipesRef.current.filter(p => p.x > -PIPE_WIDTH);
    coinsRef.current = coinsRef.current.filter(c => c.x > -30 && !c.collected);
    powerupsRef.current = powerupsRef.current.filter(p => p.x > -30 && !p.collected);

    // Check boundaries
    if (!shieldActiveRef.current) {
      if (sharkY < 0 || sharkY > canvas.height - 60) {
        handleCollision();
      }
    } else {
      if (sharkY < sharkRadius) {
        sharkRef.current.y = sharkRadius;
        sharkRef.current.velocity = Math.abs(sharkRef.current.velocity) * 0.4;
      }
      if (sharkY > canvas.height - 60 - sharkRadius) {
        sharkRef.current.y = canvas.height - 60 - sharkRadius;
        sharkRef.current.velocity = -Math.abs(sharkRef.current.velocity) * 0.4;
      }
    }
  }, []);

  const createCoin = (x, y, id) => ({
    x, y, id,
    collected: false,
    rotation: Math.random() * Math.PI * 2,
    floatOffset: 0,
    scale: 1,
    type: Math.random() < 0.3 ? 'pearl' : 'shell'
  });

  const createScoreParticles = (x, y, color) => {
    for (let i = 0; i < 12; i++) {
      const angle = (i / 12) * Math.PI * 2;
      particlesRef.current.push({
        x, y,
        vx: Math.cos(angle) * (3 + Math.random() * 2),
        vy: Math.sin(angle) * (3 + Math.random() * 2) - 1,
        size: Math.random() * 5 + 3,
        color,
        life: 1,
        type: 'score'
      });
    }
  };

  const createCoinParticles = (x, y) => {
    const colors = ['#FFD700', '#FFA500', '#FFE066', '#FFCC00', '#E0FFFF'];
    for (let i = 0; i < 15; i++) {
      const angle = Math.random() * Math.PI * 2;
      const speed = Math.random() * 5 + 2;
      particlesRef.current.push({
        x, y,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed - 1,
        size: Math.random() * 7 + 3,
        color: colors[Math.floor(Math.random() * colors.length)],
        life: 1,
        type: 'coin'
      });
    }
  };

  const createPowerupParticles = (x, y, type) => {
    const colors = {
      shield: ['#00BFFF', '#1E90FF', '#87CEEB'],
      slow: ['#DA70D6', '#BA55D3', '#EE82EE'],
      small: ['#98FB98', '#00FF7F', '#7CFC00'],
      magnet: ['#FF6B6B', '#FF4500', '#FF7F50']
    };
    const typeColors = colors[type] || colors.shield;

    for (let i = 0; i < 20; i++) {
      const angle = Math.random() * Math.PI * 2;
      const speed = Math.random() * 6 + 3;
      particlesRef.current.push({
        x, y,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        size: Math.random() * 8 + 4,
        color: typeColors[Math.floor(Math.random() * typeColors.length)],
        life: 1,
        type: 'powerup'
      });
    }
  };

  const handleCollision = useCallback(() => {
    sharkRef.current.y = canvasRef.current.height / 2;
    sharkRef.current.velocity = 0;
    scoreRef.current = Math.max(0, scoreRef.current - 1);
    setScore(scoreRef.current);
    comboRef.current = 0;
    setCombo(0);

    // Hit particles
    for (let i = 0; i < 15; i++) {
      const angle = Math.random() * Math.PI * 2;
      const speed = Math.random() * 6 + 3;
      particlesRef.current.push({
        x: 80,
        y: sharkRef.current.y,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        size: Math.random() * 6 + 3,
        color: `hsl(0, 80%, ${50 + Math.random() * 20}%)`,
        life: 1,
        type: 'hit'
      });
    }
  }, []);

  const activatePowerup = useCallback((type) => {
    const duration = type === 'small' ? 6000 : 5000;
    activePowerupRef.current = { type, endTime: Date.now() + duration };
    setActivePowerup({ type, endTime: Date.now() + duration });

    if (type === 'shield') shieldActiveRef.current = true;
    if (type === 'slow') slowMotionRef.current = true;
    if (type === 'small') smallSharkRef.current = true;
    if (type === 'magnet') magnetActiveRef.current = true;

    setTimeout(() => {
      if (activePowerupRef.current?.type === type) {
        activePowerupRef.current = null;
        setActivePowerup(null);
        if (type === 'shield') shieldActiveRef.current = false;
        if (type === 'slow') slowMotionRef.current = false;
        if (type === 'small') smallSharkRef.current = false;
        if (type === 'magnet') magnetActiveRef.current = false;
      }
    }, duration);
  }, []);

  const renderGame = useCallback(() => {
    const canvas = canvasRef.current;
    const ctx = ctxRef.current;
    if (!canvas || !ctx) return;

    const width = canvas.width;
    const height = canvas.height;
    const time = timeRef.current;

    // Deep ocean gradient background
    const gradient = ctx.createLinearGradient(0, 0, 0, height);
    gradient.addColorStop(0, '#001a33');
    gradient.addColorStop(0.3, '#003366');
    gradient.addColorStop(0.6, '#004080');
    gradient.addColorStop(0.85, '#0059b3');
    gradient.addColorStop(1, '#006699');
    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, width, height);

    // Light rays from surface
    ctx.save();
    for (let i = 0; i < 5; i++) {
      const rayX = (i * width / 4 - waterOffsetRef.current * 0.2 + width * 2) % (width + 200) - 100;
      const rayGradient = ctx.createLinearGradient(rayX, 0, rayX + 80, height * 0.7);
      rayGradient.addColorStop(0, 'rgba(100, 200, 255, 0.15)');
      rayGradient.addColorStop(0.5, 'rgba(100, 200, 255, 0.05)');
      rayGradient.addColorStop(1, 'transparent');
      ctx.fillStyle = rayGradient;
      ctx.beginPath();
      ctx.moveTo(rayX, 0);
      ctx.lineTo(rayX + 60, 0);
      ctx.lineTo(rayX + 120 + Math.sin(time + i) * 20, height * 0.7);
      ctx.lineTo(rayX + 30 + Math.sin(time + i) * 20, height * 0.7);
      ctx.closePath();
      ctx.fill();
    }
    ctx.restore();

    // Floating bubbles
    bubblesRef.current.forEach(b => {
      ctx.fillStyle = `rgba(255, 255, 255, ${b.opacity})`;
      ctx.beginPath();
      ctx.arc(b.x, b.y, b.size, 0, Math.PI * 2);
      ctx.fill();
      // Bubble shine
      ctx.fillStyle = `rgba(255, 255, 255, ${b.opacity + 0.2})`;
      ctx.beginPath();
      ctx.arc(b.x - b.size * 0.3, b.y - b.size * 0.3, b.size * 0.3, 0, Math.PI * 2);
      ctx.fill();
    });

    // Draw seaweed/coral obstacles
    pipesRef.current.forEach(pipe => {
      const isGold = pipe.color === 'gold';
      const isSeaweed = pipe.type === 'seaweed';

      if (isSeaweed) {
        // Seaweed from bottom
        drawSeaweed(ctx, pipe.x, height - 60, pipe.gapY - (height - 60), isGold, time, 'up');
        // Seaweed from top (hanging kelp)
        drawSeaweed(ctx, pipe.x, 0, pipe.gapY, isGold, time, 'down');
      } else {
        // Coral
        drawCoral(ctx, pipe.x, pipe.gapY, height - 60 - pipe.gapY - PIPE_GAP, isGold, time, 'bottom');
        drawCoral(ctx, pipe.x, 0, pipe.gapY, isGold, time, 'top');
      }

      // Gold sparkle
      if (isGold) {
        ctx.fillStyle = `rgba(255, 255, 200, ${0.3 + Math.sin(time * 5 + pipe.x) * 0.2})`;
        for (let i = 0; i < 3; i++) {
          const sparkleY = (time * 30 + i * 80) % (pipe.gapY - 40) + 20;
          ctx.beginPath();
          ctx.arc(pipe.x + PIPE_WIDTH / 2, sparkleY, 4, 0, Math.PI * 2);
          ctx.fill();
        }
      }
    });

    // Draw shells/pearls (coins)
    coinsRef.current.forEach(coin => {
      if (coin.collected) return;

      ctx.save();
      ctx.translate(coin.x, coin.y + coin.floatOffset);

      // Glow
      const glowSize = 28 + Math.sin(time * 4 + coin.id) * 4;
      const coinGlow = ctx.createRadialGradient(0, 0, 5, 0, 0, glowSize);
      if (coin.type === 'pearl') {
        coinGlow.addColorStop(0, 'rgba(255, 255, 255, 0.6)');
        coinGlow.addColorStop(0.5, 'rgba(200, 220, 255, 0.3)');
        coinGlow.addColorStop(1, 'transparent');
      } else {
        coinGlow.addColorStop(0, 'rgba(255, 215, 0, 0.5)');
        coinGlow.addColorStop(0.5, 'rgba(255, 215, 0, 0.2)');
        coinGlow.addColorStop(1, 'transparent');
      }
      ctx.fillStyle = coinGlow;
      ctx.fillRect(-glowSize, -glowSize, glowSize * 2, glowSize * 2);

      if (coin.type === 'pearl') {
        // Draw pearl
        const pearlGradient = ctx.createRadialGradient(-4, -4, 0, 0, 0, 14);
        pearlGradient.addColorStop(0, '#FFFFFF');
        pearlGradient.addColorStop(0.5, '#E8E8FF');
        pearlGradient.addColorStop(1, '#C0C0E0');
        ctx.fillStyle = pearlGradient;
        ctx.beginPath();
        ctx.arc(0, 0, 14, 0, Math.PI * 2);
        ctx.fill();

        // Pearl shine
        ctx.fillStyle = 'rgba(255, 255, 255, 0.8)';
        ctx.beginPath();
        ctx.ellipse(-4, -4, 5, 4, -0.3, 0, Math.PI * 2);
        ctx.fill();
      } else {
        // Draw shell
        ctx.rotate(coin.rotation * 0.2);
        const shellGradient = ctx.createRadialGradient(-3, -3, 0, 0, 0, 16);
        shellGradient.addColorStop(0, '#FFE4B5');
        shellGradient.addColorStop(0.5, '#FFD700');
        shellGradient.addColorStop(1, '#DAA520');
        ctx.fillStyle = shellGradient;

        // Shell shape
        ctx.beginPath();
        ctx.moveTo(0, -14);
        for (let i = 0; i <= 10; i++) {
          const angle = (i / 10) * Math.PI;
          const r = 14 + Math.sin(i * 1.5) * 3;
          ctx.lineTo(Math.cos(angle - Math.PI/2) * r, Math.sin(angle - Math.PI/2) * r + 2);
        }
        ctx.closePath();
        ctx.fill();

        // Shell lines
        ctx.strokeStyle = 'rgba(139, 69, 19, 0.3)';
        ctx.lineWidth = 1;
        for (let i = 0; i < 5; i++) {
          ctx.beginPath();
          ctx.moveTo(0, -12);
          const angle = (i - 2) * 0.3;
          ctx.quadraticCurveTo(Math.sin(angle) * 8, 0, Math.sin(angle) * 12, 12);
          ctx.stroke();
        }
      }

      ctx.restore();
    });

    // Draw powerups (treasure chests, etc)
    powerupsRef.current.forEach(powerup => {
      if (powerup.collected) return;

      ctx.save();
      ctx.translate(powerup.x, powerup.y);

      const pulseScale = 1 + Math.sin(powerup.pulse) * 0.12;
      ctx.scale(pulseScale, pulseScale);

      // Glow
      const colors = {
        shield: { glow: 'rgba(0, 150, 255, 0.6)', main: '#00BFFF', accent: '#1E90FF' },
        slow: { glow: 'rgba(186, 85, 211, 0.6)', main: '#BA55D3', accent: '#9932CC' },
        small: { glow: 'rgba(50, 205, 50, 0.6)', main: '#32CD32', accent: '#228B22' },
        magnet: { glow: 'rgba(255, 99, 71, 0.6)', main: '#FF6347', accent: '#DC143C' }
      };
      const c = colors[powerup.type];

      const glow = ctx.createRadialGradient(0, 0, 5, 0, 0, 35);
      glow.addColorStop(0, c.glow);
      glow.addColorStop(1, 'transparent');
      ctx.fillStyle = glow;
      ctx.fillRect(-35, -35, 70, 70);

      // Starfish shape for powerups
      ctx.fillStyle = c.main;
      ctx.beginPath();
      for (let i = 0; i < 10; i++) {
        const angle = (i * Math.PI) / 5 - Math.PI / 2 + powerup.rotation;
        const r = i % 2 === 0 ? 20 : 10;
        if (i === 0) ctx.moveTo(Math.cos(angle) * r, Math.sin(angle) * r);
        else ctx.lineTo(Math.cos(angle) * r, Math.sin(angle) * r);
      }
      ctx.closePath();
      ctx.fill();

      // Inner
      ctx.fillStyle = c.accent;
      ctx.beginPath();
      ctx.arc(0, 0, 8, 0, Math.PI * 2);
      ctx.fill();

      // Icon
      ctx.fillStyle = 'white';
      ctx.font = 'bold 12px Arial';
      ctx.textAlign = 'center';
      ctx.textBaseline = 'middle';
      const icons = { shield: '🛡', slow: '⏱', small: '🔹', magnet: '🧲' };
      ctx.fillText(icons[powerup.type], 0, 0);

      ctx.restore();
    });

    // Draw baby shark
    const sharkSize = smallSharkRef.current ? SHARK_SIZE * 0.55 : SHARK_SIZE;
    const sharkX = 80;
    const sharkY = sharkRef.current.y;
    const sharkColor = sharkColorRef.current;

    ctx.save();
    ctx.translate(sharkX, sharkY);

    // Rotation based on velocity
    const rotation = Math.max(-20, Math.min(50, sharkRef.current.velocity * 3)) * Math.PI / 180;
    ctx.rotate(rotation);

    // Shield effect
    if (shieldActiveRef.current) {
      const shieldPulse = Math.sin(time * 6) * 0.15 + 0.85;
      const shieldGlow = ctx.createRadialGradient(0, 0, sharkSize / 2, 0, 0, sharkSize * 1.3);
      shieldGlow.addColorStop(0, 'rgba(0, 200, 255, 0.1)');
      shieldGlow.addColorStop(0.6, `rgba(0, 200, 255, ${0.25 * shieldPulse})`);
      shieldGlow.addColorStop(1, 'rgba(0, 200, 255, 0)');
      ctx.fillStyle = shieldGlow;
      ctx.beginPath();
      ctx.arc(0, 0, sharkSize * 1.3, 0, Math.PI * 2);
      ctx.fill();

      ctx.strokeStyle = `rgba(100, 220, 255, ${0.6 * shieldPulse})`;
      ctx.lineWidth = 3;
      ctx.setLineDash([8, 4]);
      ctx.beginPath();
      ctx.arc(0, 0, sharkSize, 0, Math.PI * 2);
      ctx.stroke();
      ctx.setLineDash([]);
    }

    // Magnet effect
    if (magnetActiveRef.current) {
      ctx.strokeStyle = `rgba(255, 99, 71, ${0.3 + Math.sin(time * 8) * 0.2})`;
      ctx.lineWidth = 2;
      for (let i = 0; i < 3; i++) {
        const ringRadius = sharkSize * (1.2 + i * 0.3) + Math.sin(time * 4 + i) * 5;
        ctx.beginPath();
        ctx.arc(0, 0, ringRadius, 0, Math.PI * 2);
        ctx.stroke();
      }
    }

    // Baby shark body
    ctx.save();

    // Tail
    ctx.save();
    ctx.translate(-sharkSize * 0.7, 0);
    ctx.rotate(Math.sin(time * 12 + tailAngleRef.current * 0.1) * 0.3);

    // Tail fin
    ctx.fillStyle = sharkColor.fin;
    ctx.beginPath();
    ctx.moveTo(0, 0);
    ctx.quadraticCurveTo(-sharkSize * 0.4, -sharkSize * 0.4, -sharkSize * 0.5, -sharkSize * 0.2);
    ctx.lineTo(-sharkSize * 0.3, 0);
    ctx.lineTo(-sharkSize * 0.5, sharkSize * 0.2);
    ctx.quadraticCurveTo(-sharkSize * 0.4, sharkSize * 0.4, 0, 0);
    ctx.fill();
    ctx.restore();

    // Main body
    const bodyGradient = ctx.createRadialGradient(0, -5, 0, 0, 0, sharkSize * 0.6);
    bodyGradient.addColorStop(0, sharkColor.body);
    bodyGradient.addColorStop(1, sharkColor.fin);
    ctx.fillStyle = bodyGradient;
    ctx.beginPath();
    ctx.ellipse(0, 0, sharkSize * 0.6, sharkSize * 0.35, 0, 0, Math.PI * 2);
    ctx.fill();

    // Belly
    ctx.fillStyle = sharkColor.belly;
    ctx.beginPath();
    ctx.ellipse(sharkSize * 0.05, sharkSize * 0.1, sharkSize * 0.4, sharkSize * 0.18, 0, 0, Math.PI * 2);
    ctx.fill();

    // Dorsal fin
    ctx.fillStyle = sharkColor.fin;
    ctx.beginPath();
    ctx.moveTo(-sharkSize * 0.1, -sharkSize * 0.3);
    ctx.quadraticCurveTo(0, -sharkSize * 0.65, sharkSize * 0.15, -sharkSize * 0.35);
    ctx.lineTo(sharkSize * 0.05, -sharkSize * 0.28);
    ctx.closePath();
    ctx.fill();

    // Side fins
    ctx.fillStyle = sharkColor.fin;
    ctx.beginPath();
    ctx.ellipse(-sharkSize * 0.1, sharkSize * 0.25, sharkSize * 0.25, sharkSize * 0.1, 0.3, 0, Math.PI * 2);
    ctx.fill();

    // Cute face - snout
    ctx.fillStyle = sharkColor.body;
    ctx.beginPath();
    ctx.ellipse(sharkSize * 0.5, 0, sharkSize * 0.25, sharkSize * 0.22, 0, 0, Math.PI * 2);
    ctx.fill();

    // Eye white
    ctx.fillStyle = 'white';
    ctx.beginPath();
    ctx.arc(sharkSize * 0.25, -sharkSize * 0.08, sharkSize * 0.14, 0, Math.PI * 2);
    ctx.fill();

    // Eye pupil
    ctx.fillStyle = '#222';
    ctx.beginPath();
    ctx.arc(sharkSize * 0.28, -sharkSize * 0.06, sharkSize * 0.08, 0, Math.PI * 2);
    ctx.fill();

    // Eye shine
    ctx.fillStyle = 'white';
    ctx.beginPath();
    ctx.arc(sharkSize * 0.26, -sharkSize * 0.1, sharkSize * 0.04, 0, Math.PI * 2);
    ctx.fill();

    // Cute blush
    ctx.fillStyle = 'rgba(255, 150, 150, 0.4)';
    ctx.beginPath();
    ctx.ellipse(sharkSize * 0.15, sharkSize * 0.08, sharkSize * 0.1, sharkSize * 0.06, 0, 0, Math.PI * 2);
    ctx.fill();

    // Smile
    ctx.strokeStyle = sharkColor.fin;
    ctx.lineWidth = 2;
    ctx.lineCap = 'round';
    ctx.beginPath();
    ctx.arc(sharkSize * 0.45, sharkSize * 0.05, sharkSize * 0.12, 0.2, Math.PI - 0.2);
    ctx.stroke();

    // Little tooth
    ctx.fillStyle = 'white';
    ctx.beginPath();
    ctx.moveTo(sharkSize * 0.5, sharkSize * 0.08);
    ctx.lineTo(sharkSize * 0.53, sharkSize * 0.15);
    ctx.lineTo(sharkSize * 0.47, sharkSize * 0.08);
    ctx.fill();

    ctx.restore();
    ctx.restore();

    // Draw particles
    particlesRef.current.forEach(p => {
      ctx.globalAlpha = p.life;
      if (p.type === 'bubble') {
        ctx.fillStyle = p.color;
        ctx.beginPath();
        ctx.arc(p.x, p.y, p.size * p.life, 0, Math.PI * 2);
        ctx.fill();
        // Bubble shine
        ctx.fillStyle = 'rgba(255, 255, 255, 0.5)';
        ctx.beginPath();
        ctx.arc(p.x - p.size * 0.2, p.y - p.size * 0.2, p.size * 0.2 * p.life, 0, Math.PI * 2);
        ctx.fill();
      } else {
        ctx.fillStyle = p.color;
        ctx.beginPath();
        ctx.arc(p.x, p.y, p.size * p.life, 0, Math.PI * 2);
        ctx.fill();
      }
    });
    ctx.globalAlpha = 1;

    // Draw sandy ocean floor
    const floorGradient = ctx.createLinearGradient(0, height - 60, 0, height);
    floorGradient.addColorStop(0, '#C2A878');
    floorGradient.addColorStop(0.3, '#B8956E');
    floorGradient.addColorStop(1, '#8B7355');
    ctx.fillStyle = floorGradient;
    ctx.fillRect(0, height - 60, width, 60);

    // Sand ripples
    ctx.strokeStyle = 'rgba(139, 115, 85, 0.4)';
    ctx.lineWidth = 2;
    for (let i = -waterOffsetRef.current; i < width + 50; i += 40) {
      ctx.beginPath();
      ctx.moveTo(i, height - 50);
      ctx.quadraticCurveTo(i + 20, height - 55, i + 40, height - 50);
      ctx.stroke();
    }

    // Small shells on floor
    for (let i = 0; i < 5; i++) {
      const shellX = ((i * 120 + 50 - waterOffsetRef.current * 0.3) % (width + 100)) - 50;
      ctx.fillStyle = '#FFE4C4';
      ctx.beginPath();
      ctx.arc(shellX, height - 40, 5, 0, Math.PI, true);
      ctx.fill();
    }

    // Small rocks
    for (let i = 0; i < 4; i++) {
      const rockX = ((i * 150 + 80 - waterOffsetRef.current * 0.2) % (width + 100)) - 50;
      ctx.fillStyle = '#696969';
      ctx.beginPath();
      ctx.ellipse(rockX, height - 35, 12, 8, 0, 0, Math.PI * 2);
      ctx.fill();
      ctx.fillStyle = '#808080';
      ctx.beginPath();
      ctx.ellipse(rockX - 2, height - 37, 8, 5, 0, 0, Math.PI * 2);
      ctx.fill();
    }

    // Combo display
    if (comboRef.current > 1 && gameStateRef.current === 'playing') {
      ctx.save();
      ctx.translate(sharkX, sharkY - 50);
      ctx.fillStyle = `rgba(255, 215, 0, ${0.7 + Math.sin(time * 6) * 0.3})`;
      ctx.font = 'bold 18px Arial';
      ctx.textAlign = 'center';
      ctx.strokeStyle = 'rgba(0, 0, 0, 0.5)';
      ctx.lineWidth = 3;
      ctx.strokeText(`x${comboRef.current}`, 0, 0);
      ctx.fillText(`x${comboRef.current}`, 0, 0);
      ctx.restore();
    }
  }, []);

  // Draw seaweed function
  const drawSeaweed = (ctx, x, startY, length, isGold, time, direction) => {
    const segments = Math.floor(length / 30);
    if (segments < 1) return;

    for (let s = 0; s < 3; s++) {
      const offsetX = (s - 1) * 20;
      ctx.beginPath();

      if (direction === 'up') {
        ctx.moveTo(x + PIPE_WIDTH / 2 + offsetX, startY);
      } else {
        ctx.moveTo(x + PIPE_WIDTH / 2 + offsetX, startY + length);
      }

      for (let i = 0; i <= segments; i++) {
        const progress = i / segments;
        const waveOffset = Math.sin(time * 2 + i * 0.5 + s) * (10 + progress * 15);
        const segmentX = x + PIPE_WIDTH / 2 + offsetX + waveOffset;
        let segmentY;

        if (direction === 'up') {
          segmentY = startY - progress * length;
        } else {
          segmentY = startY + length - progress * length;
        }

        if (i === 0) {
          ctx.moveTo(segmentX, segmentY);
        } else {
          ctx.lineTo(segmentX, segmentY);
        }
      }

      const seaweedGradient = ctx.createLinearGradient(x, startY, x, startY + (direction === 'up' ? -length : length));
      if (isGold) {
        seaweedGradient.addColorStop(0, '#DAA520');
        seaweedGradient.addColorStop(0.5, '#FFD700');
        seaweedGradient.addColorStop(1, '#B8860B');
      } else {
        seaweedGradient.addColorStop(0, '#1B5E20');
        seaweedGradient.addColorStop(0.5, '#2E7D32');
        seaweedGradient.addColorStop(1, '#1B5E20');
      }

      ctx.strokeStyle = seaweedGradient;
      ctx.lineWidth = 12 - s * 3;
      ctx.lineCap = 'round';
      ctx.stroke();
    }
  };

  // Draw coral function
  const drawCoral = (ctx, x, startY, length, isGold, time, position) => {
    // Coral base
    const coralGradient = ctx.createLinearGradient(x, startY, x + PIPE_WIDTH, startY);
    if (isGold) {
      coralGradient.addColorStop(0, '#B8860B');
      coralGradient.addColorStop(0.5, '#FFD700');
      coralGradient.addColorStop(1, '#B8860B');
    } else {
      coralGradient.addColorStop(0, '#FF6B6B');
      coralGradient.addColorStop(0.3, '#FF8E8E');
      coralGradient.addColorStop(0.7, '#FF8E8E');
      coralGradient.addColorStop(1, '#FF6B6B');
    }

    if (position === 'top') {
      ctx.fillStyle = coralGradient;
      ctx.fillRect(x, startY, PIPE_WIDTH, length);

      // Coral bumps at bottom
      for (let i = 0; i < 4; i++) {
        const bumpX = x + (i + 0.5) * (PIPE_WIDTH / 4);
        const bumpSize = 15 + Math.sin(time + i) * 3;
        ctx.beginPath();
        ctx.arc(bumpX, length, bumpSize, 0, Math.PI);
        ctx.fill();
      }
    } else {
      const floorY = startY + length + PIPE_GAP;
      ctx.fillStyle = coralGradient;
      ctx.fillRect(x, floorY, PIPE_WIDTH, length);

      // Coral bumps at top
      for (let i = 0; i < 4; i++) {
        const bumpX = x + (i + 0.5) * (PIPE_WIDTH / 4);
        const bumpSize = 15 + Math.sin(time + i) * 3;
        ctx.beginPath();
        ctx.arc(bumpX, floorY, bumpSize, Math.PI, Math.PI * 2);
        ctx.fill();
      }
    }

    // Coral texture dots
    ctx.fillStyle = isGold ? 'rgba(255, 255, 200, 0.3)' : 'rgba(255, 255, 255, 0.2)';
    for (let i = 0; i < 8; i++) {
      const dotX = x + Math.random() * PIPE_WIDTH;
      const dotY = position === 'top' ? startY + Math.random() * length : startY + length + PIPE_GAP + Math.random() * length;
      ctx.beginPath();
      ctx.arc(dotX, dotY, 3, 0, Math.PI * 2);
      ctx.fill();
    }
  };

  // Touch/click controls
  const handleInteraction = useCallback((e) => {
    e.preventDefault();
    swim();
  }, [swim]);

  // Keyboard controls
  useEffect(() => {
    const handleKeyDown = (e) => {
      if (e.code === 'Space' || e.key === 'ArrowUp' || e.key === 'w') {
        e.preventDefault();
        swim();
      }
    };
    window.addEventListener('keydown', handleKeyDown);
    return () => window.removeEventListener('keydown', handleKeyDown);
  }, [swim]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="flappy-bird-overlay baby-shark-theme">
      <div className="flappy-bird-container">
        {/* Game Canvas */}
        <div
          ref={gameAreaRef}
          className="game-area"
          onMouseDown={handleInteraction}
          onTouchStart={handleInteraction}
        >
          <canvas ref={canvasRef} />
        </div>

        {/* Header */}
        <div className="game-header">
          <div className="game-stat">
            <span className="stat-icon">🐚</span>
            <span className="stat-value">{score}</span>
          </div>
          <div className="game-timer">
            <span className="timer-icon">⏱️</span>
            <span className={`timer-value ${timeLeft <= 5 ? 'warning' : ''}`}>
              {timeLeft}s
            </span>
          </div>
          <div className="game-stat pipes-stat">
            <span className="stat-icon">🌊</span>
            <span className="stat-value">{pipesPassed}</span>
          </div>
        </div>

        {/* Active Powerup */}
        {activePowerup && (
          <div className={`active-powerup ${activePowerup.type}`}>
            <span className="powerup-icon">
              {activePowerup.type === 'shield' && '🛡️'}
              {activePowerup.type === 'slow' && '⏱️'}
              {activePowerup.type === 'small' && '🔹'}
              {activePowerup.type === 'magnet' && '🧲'}
            </span>
            <span className="powerup-label">
              {activePowerup.type === 'shield' && 'BUBBLE SHIELD'}
              {activePowerup.type === 'slow' && 'SLOW CURRENT'}
              {activePowerup.type === 'small' && 'TINY SHARK'}
              {activePowerup.type === 'magnet' && 'PEARL MAGNET'}
            </span>
          </div>
        )}

        {/* Combo Display */}
        {combo > 1 && gameState === 'playing' && (
          <div className="combo-display">
            <span className="combo-text">x{combo} COMBO!</span>
          </div>
        )}

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="countdown-display" key={countdown}>
            {countdown > 0 ? countdown : 'SWIM!'}
          </div>
        )}

        {/* Game Over Screen */}
        {gameState === 'ended' && (
          <div className="game-over-screen">
            <div className="game-over-content">
              <h2 className="game-over-title">
                <span>🦈</span> {score >= 50 ? 'DOO DOO DOO!' : score >= 30 ? 'Great Swim!' : 'Nice Try!'} <span>🦈</span>
              </h2>

              <div className="final-stats">
                <div className="final-stat">
                  <span className="final-stat-icon">🌊</span>
                  <span className="final-stat-label">Obstacles Passed</span>
                  <span className="final-stat-value">{pipesPassed}</span>
                </div>

                <div className="final-stat highlight">
                  <span className="final-stat-icon">🐚</span>
                  <span className="final-stat-label">Shells Collected</span>
                  <span className="final-stat-value coins">{score}</span>
                </div>
              </div>

              {pipesPassed >= 12 && (
                <div className="achievement-badge">
                  <span>🏆</span> Ocean Master! <span>🏆</span>
                </div>
              )}

              <button
                className="claim-rewards-btn cute-button"
                onClick={handleFinish}
              >
                Claim {score} Coins! 🐚
              </button>
            </div>
          </div>
        )}

        {/* Instructions */}
        {gameState === 'countdown' && countdown === 3 && (
          <div className="game-instructions">
            <p>Tap or press Space to swim!</p>
            <p className="hint">Dodge obstacles & collect shells! 🦈</p>
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

export default FlappyBirdGame;
