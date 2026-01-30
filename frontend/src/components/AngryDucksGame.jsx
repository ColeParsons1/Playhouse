import { useState, useEffect, useCallback, useRef } from 'react';
import './AngryDucksGame.css';

// Game constants
const GRAVITY = 0.4;
const FRICTION = 0.98;
const LAUNCH_POWER = 0.22;
const MAX_PULL_DISTANCE = 120;

// Level definitions with piggies and blocks
const LEVELS = [
  {
    name: 'Easy Start',
    ducks: 3,
    piggies: [
      { x: 320, y: 280, size: 25 },
    ],
    blocks: [
      { x: 300, y: 310, width: 60, height: 20, type: 'wood' },
    ],
    stars: [300, 200, 100],
  },
  {
    name: 'Double Trouble',
    ducks: 3,
    piggies: [
      { x: 300, y: 260, size: 22 },
      { x: 350, y: 260, size: 22 },
    ],
    blocks: [
      { x: 280, y: 290, width: 100, height: 20, type: 'wood' },
      { x: 280, y: 260, width: 20, height: 50, type: 'wood' },
      { x: 360, y: 260, width: 20, height: 50, type: 'wood' },
      { x: 280, y: 230, width: 100, height: 15, type: 'wood' },
    ],
    stars: [500, 300, 150],
  },
  {
    name: 'Stone Wall',
    ducks: 4,
    piggies: [
      { x: 340, y: 270, size: 28 },
    ],
    blocks: [
      { x: 300, y: 310, width: 80, height: 25, type: 'stone' },
      { x: 300, y: 270, width: 20, height: 60, type: 'stone' },
      { x: 360, y: 270, width: 20, height: 60, type: 'stone' },
      { x: 300, y: 230, width: 80, height: 20, type: 'stone' },
    ],
    stars: [600, 400, 200],
  },
  {
    name: 'Tower of Pigs',
    ducks: 4,
    piggies: [
      { x: 340, y: 300, size: 20 },
      { x: 340, y: 240, size: 20 },
      { x: 340, y: 180, size: 20 },
    ],
    blocks: [
      { x: 310, y: 320, width: 60, height: 15, type: 'wood' },
      { x: 310, y: 260, width: 60, height: 15, type: 'wood' },
      { x: 310, y: 200, width: 60, height: 15, type: 'wood' },
      { x: 310, y: 140, width: 60, height: 15, type: 'wood' },
      { x: 310, y: 320, width: 15, height: 60, type: 'wood' },
      { x: 355, y: 320, width: 15, height: 60, type: 'wood' },
      { x: 310, y: 260, width: 15, height: 60, type: 'wood' },
      { x: 355, y: 260, width: 15, height: 60, type: 'wood' },
      { x: 310, y: 200, width: 15, height: 60, type: 'wood' },
      { x: 355, y: 200, width: 15, height: 60, type: 'wood' },
    ],
    stars: [1000, 700, 400],
  },
  {
    name: 'Fortress',
    ducks: 5,
    piggies: [
      { x: 320, y: 280, size: 30 },
      { x: 280, y: 320, size: 18 },
      { x: 360, y: 320, size: 18 },
    ],
    blocks: [
      { x: 250, y: 340, width: 140, height: 20, type: 'stone' },
      { x: 250, y: 280, width: 20, height: 80, type: 'stone' },
      { x: 370, y: 280, width: 20, height: 80, type: 'stone' },
      { x: 250, y: 250, width: 140, height: 20, type: 'wood' },
      { x: 290, y: 320, width: 60, height: 15, type: 'wood' },
    ],
    stars: [1200, 800, 500],
  },
];

const AngryDucksGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('menu'); // menu, playing, levelComplete, gameOver
  const [currentLevel, setCurrentLevel] = useState(0);
  const [score, setScore] = useState(0);
  const [ducksLeft, setDucksLeft] = useState(3);
  const [isDragging, setIsDragging] = useState(false);
  const [dragStart, setDragStart] = useState({ x: 0, y: 0 });
  const [dragEnd, setDragEnd] = useState({ x: 0, y: 0 });

  const canvasRef = useRef(null);
  const gameAreaRef = useRef(null);
  const animationFrameRef = useRef(null);
  const duckRef = useRef(null);
  const piggiesRef = useRef([]);
  const blocksRef = useRef([]);
  const debrisRef = useRef([]);
  const scoreRef = useRef(0);
  const ducksLeftRef = useRef(3);
  const slingshotPos = useRef({ x: 80, y: 280 });
  const groundY = useRef(340);
  const canvasSize = useRef({ width: 400, height: 380 });
  const levelLoadedRef = useRef(false);

  // Initialize canvas when playing state is entered
  useEffect(() => {
    if (gameState !== 'playing') return;

    // Use a small delay to ensure DOM is ready
    const initTimeout = setTimeout(() => {
      const canvas = canvasRef.current;
      const container = gameAreaRef.current;
      if (!canvas || !container) return;

      const dpr = window.devicePixelRatio || 1;
      const rect = container.getBoundingClientRect();
      const width = Math.min(rect.width, 450);
      const height = Math.min(rect.height - 80, 400);

      canvas.width = width * dpr;
      canvas.height = height * dpr;
      canvas.style.width = `${width}px`;
      canvas.style.height = `${height}px`;

      canvasSize.current = { width, height };
      groundY.current = height - 40;
      slingshotPos.current = { x: width * 0.18, y: groundY.current - 60 };

      const ctx = canvas.getContext('2d');
      ctx.scale(dpr, dpr);

      // Now load the level with correct canvas dimensions
      if (!levelLoadedRef.current) {
        loadLevel(currentLevel);
      }
    }, 50);

    return () => clearTimeout(initTimeout);
  }, [gameState, currentLevel, loadLevel]);

  // Load level
  const loadLevel = useCallback((levelIndex) => {
    if (levelIndex >= LEVELS.length) {
      setGameState('gameOver');
      return;
    }

    const level = LEVELS[levelIndex];
    const { width, height } = canvasSize.current;
    const scaleX = width / 400;
    const scaleY = (height - 40) / 340;

    ducksLeftRef.current = level.ducks;
    setDucksLeft(level.ducks);

    // Scale piggies
    piggiesRef.current = level.piggies.map((p, i) => ({
      id: i,
      x: p.x * scaleX,
      y: p.y * scaleY,
      size: p.size * Math.min(scaleX, scaleY),
      health: 100,
      vx: 0,
      vy: 0,
    }));

    // Scale blocks
    blocksRef.current = level.blocks.map((b, i) => ({
      id: i,
      x: b.x * scaleX,
      y: b.y * scaleY,
      width: b.width * scaleX,
      height: b.height * scaleY,
      type: b.type,
      health: b.type === 'stone' ? 200 : 100,
      vx: 0,
      vy: 0,
      rotation: 0,
      rotationVel: 0,
    }));

    debrisRef.current = [];
    duckRef.current = null;
    levelLoadedRef.current = true;
  }, []);

  // Start game
  const startGame = useCallback((levelIndex = 0) => {
    setCurrentLevel(levelIndex);
    scoreRef.current = 0;
    setScore(0);
    levelLoadedRef.current = false; // Reset so level will be loaded after canvas init
    setGameState('playing');
  }, []);

  // Create debris particles
  const createDebris = useCallback((x, y, color, count = 5) => {
    for (let i = 0; i < count; i++) {
      const angle = (Math.PI * 2 * i) / count + Math.random() * 0.5;
      const speed = 3 + Math.random() * 4;
      debrisRef.current.push({
        x,
        y,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed - 2,
        size: 3 + Math.random() * 5,
        color,
        life: 1,
        rotation: Math.random() * Math.PI * 2,
      });
    }
  }, []);

  // Launch duck
  const launchDuck = useCallback(() => {
    if (!isDragging || ducksLeftRef.current <= 0) return;

    const dx = dragStart.x - dragEnd.x;
    const dy = dragStart.y - dragEnd.y;
    const distance = Math.min(Math.sqrt(dx * dx + dy * dy), MAX_PULL_DISTANCE);

    if (distance < 20) {
      setIsDragging(false);
      return;
    }

    const angle = Math.atan2(dy, dx);
    const power = distance * LAUNCH_POWER;

    duckRef.current = {
      x: slingshotPos.current.x,
      y: slingshotPos.current.y,
      vx: Math.cos(angle) * power,
      vy: Math.sin(angle) * power,
      size: 18,
      rotation: 0,
      rotationVel: (Math.random() - 0.5) * 0.3,
      launched: true,
      bounces: 0,
    };

    ducksLeftRef.current--;
    setDucksLeft(ducksLeftRef.current);
    setIsDragging(false);
  }, [isDragging, dragStart, dragEnd]);

  // Check collision between two circles
  const circleCollision = useCallback((c1, c2) => {
    const dx = c1.x - c2.x;
    const dy = c1.y - c2.y;
    const dist = Math.sqrt(dx * dx + dy * dy);
    return dist < c1.size + c2.size;
  }, []);

  // Check collision between circle and rectangle
  const circleRectCollision = useCallback((circle, rect) => {
    const closestX = Math.max(rect.x - rect.width / 2, Math.min(circle.x, rect.x + rect.width / 2));
    const closestY = Math.max(rect.y - rect.height / 2, Math.min(circle.y, rect.y + rect.height / 2));
    const dx = circle.x - closestX;
    const dy = circle.y - closestY;
    return Math.sqrt(dx * dx + dy * dy) < circle.size;
  }, []);

  // Game loop
  useEffect(() => {
    if (gameState !== 'playing') return;

    const updateGame = () => {
      const { width, height } = canvasSize.current;
      const ground = groundY.current;

      // Update duck
      if (duckRef.current?.launched) {
        const duck = duckRef.current;
        duck.vy += GRAVITY;
        duck.x += duck.vx;
        duck.y += duck.vy;
        duck.vx *= FRICTION;
        duck.rotation += duck.rotationVel;

        // Ground collision
        if (duck.y > ground - duck.size) {
          duck.y = ground - duck.size;
          duck.vy *= -0.5;
          duck.vx *= 0.8;
          duck.bounces++;
          if (Math.abs(duck.vy) < 1) duck.vy = 0;
        }

        // Wall collisions
        if (duck.x < duck.size) {
          duck.x = duck.size;
          duck.vx *= -0.5;
        }
        if (duck.x > width - duck.size) {
          duck.x = width - duck.size;
          duck.vx *= -0.5;
        }

        // Check if duck stopped
        const speed = Math.sqrt(duck.vx * duck.vx + duck.vy * duck.vy);
        if ((speed < 0.5 && duck.y >= ground - duck.size - 5) || duck.bounces > 8) {
          createDebris(duck.x, duck.y, '#F4D03F', 6);
          duckRef.current = null;

          // Check level status after duck stops
          setTimeout(() => {
            if (piggiesRef.current.length === 0) {
              // Level complete!
              const level = LEVELS[currentLevel];
              const stars = ducksLeftRef.current >= 2 ? 3 :
                           ducksLeftRef.current >= 1 ? 2 : 1;
              const bonus = level.stars[3 - stars] || 100;
              scoreRef.current += bonus;
              setScore(scoreRef.current);
              setGameState('levelComplete');
            } else if (ducksLeftRef.current <= 0) {
              // Game over
              setGameState('gameOver');
            }
          }, 500);
        }

        // Collision with piggies
        piggiesRef.current.forEach(piggy => {
          if (circleCollision(duck, { x: piggy.x, y: piggy.y, size: piggy.size })) {
            const damage = Math.sqrt(duck.vx * duck.vx + duck.vy * duck.vy) * 15;
            piggy.health -= damage;
            piggy.vx += duck.vx * 0.5;
            piggy.vy += duck.vy * 0.5;
            duck.vx *= 0.5;
            duck.vy *= 0.5;
          }
        });

        // Collision with blocks
        blocksRef.current.forEach(block => {
          if (circleRectCollision(duck, block)) {
            const damage = Math.sqrt(duck.vx * duck.vx + duck.vy * duck.vy) * 12;
            block.health -= damage;
            block.vx += duck.vx * 0.3;
            block.vy += duck.vy * 0.3;
            block.rotationVel += (Math.random() - 0.5) * 0.1;
            duck.vx *= -0.4;
            duck.vy *= -0.4;
          }
        });
      }

      // Update piggies
      piggiesRef.current.forEach(piggy => {
        piggy.vy += GRAVITY * 0.5;
        piggy.x += piggy.vx;
        piggy.y += piggy.vy;
        piggy.vx *= 0.95;

        if (piggy.y > ground - piggy.size) {
          piggy.y = ground - piggy.size;
          piggy.vy *= -0.3;
          if (Math.abs(piggy.vy) < 1) piggy.vy = 0;
        }

        // Piggy-block collision
        blocksRef.current.forEach(block => {
          if (circleRectCollision({ x: piggy.x, y: piggy.y, size: piggy.size }, block)) {
            const impact = Math.sqrt(block.vx * block.vx + block.vy * block.vy);
            if (impact > 1) {
              piggy.health -= impact * 8;
            }
          }
        });
      });

      // Remove dead piggies
      piggiesRef.current = piggiesRef.current.filter(piggy => {
        if (piggy.health <= 0) {
          scoreRef.current += 100;
          setScore(scoreRef.current);
          createDebris(piggy.x, piggy.y, '#27AE60', 10);
          return false;
        }
        return true;
      });

      // Update blocks
      blocksRef.current.forEach(block => {
        block.vy += GRAVITY * 0.3;
        block.x += block.vx;
        block.y += block.vy;
        block.vx *= 0.95;
        block.rotation += block.rotationVel;
        block.rotationVel *= 0.95;

        if (block.y > ground - block.height / 2) {
          block.y = ground - block.height / 2;
          block.vy *= -0.3;
          block.rotationVel *= 0.5;
          if (Math.abs(block.vy) < 1) block.vy = 0;
        }

        if (block.x < block.width / 2) {
          block.x = block.width / 2;
          block.vx *= -0.5;
        }
        if (block.x > width - block.width / 2) {
          block.x = width - block.width / 2;
          block.vx *= -0.5;
        }
      });

      // Remove destroyed blocks
      blocksRef.current = blocksRef.current.filter(block => {
        if (block.health <= 0) {
          scoreRef.current += 50;
          setScore(scoreRef.current);
          createDebris(block.x, block.y, block.type === 'stone' ? '#7F8C8D' : '#D35400', 8);
          return false;
        }
        return true;
      });

      // Update debris
      debrisRef.current = debrisRef.current.filter(d => {
        d.vy += GRAVITY * 0.5;
        d.x += d.vx;
        d.y += d.vy;
        d.life -= 0.02;
        d.rotation += 0.1;
        return d.life > 0;
      });
    };

    const render = () => {
      const canvas = canvasRef.current;
      const ctx = canvas?.getContext('2d');
      if (!ctx) return;

      const dpr = window.devicePixelRatio || 1;
      const { width, height } = canvasSize.current;
      const ground = groundY.current;

      ctx.save();

      // Sky gradient
      const skyGradient = ctx.createLinearGradient(0, 0, 0, height);
      skyGradient.addColorStop(0, '#87CEEB');
      skyGradient.addColorStop(0.6, '#B0E0E6');
      skyGradient.addColorStop(1, '#98FB98');
      ctx.fillStyle = skyGradient;
      ctx.fillRect(0, 0, width, height);

      // Sun
      ctx.fillStyle = '#FFD700';
      ctx.beginPath();
      ctx.arc(width - 50, 50, 30, 0, Math.PI * 2);
      ctx.fill();
      ctx.fillStyle = 'rgba(255, 215, 0, 0.3)';
      ctx.beginPath();
      ctx.arc(width - 50, 50, 45, 0, Math.PI * 2);
      ctx.fill();

      // Clouds
      ctx.fillStyle = 'rgba(255, 255, 255, 0.8)';
      [
        { x: width * 0.2, y: 60 },
        { x: width * 0.5, y: 40 },
        { x: width * 0.75, y: 70 },
      ].forEach(cloud => {
        ctx.beginPath();
        ctx.arc(cloud.x, cloud.y, 20, 0, Math.PI * 2);
        ctx.arc(cloud.x + 25, cloud.y + 5, 18, 0, Math.PI * 2);
        ctx.arc(cloud.x - 20, cloud.y + 5, 15, 0, Math.PI * 2);
        ctx.fill();
      });

      // Ground
      ctx.fillStyle = '#228B22';
      ctx.fillRect(0, ground, width, height - ground);

      // Grass details
      ctx.fillStyle = '#1E7B1E';
      for (let i = 0; i < width; i += 8) {
        const grassHeight = 5 + Math.random() * 5;
        ctx.fillRect(i, ground - grassHeight, 3, grassHeight);
      }

      // Draw slingshot
      const sling = slingshotPos.current;

      // Slingshot post
      ctx.fillStyle = '#8B4513';
      ctx.fillRect(sling.x - 8, sling.y - 5, 6, 65);
      ctx.fillRect(sling.x + 2, sling.y - 5, 6, 65);

      // Slingshot Y shape
      ctx.fillStyle = '#654321';
      ctx.fillRect(sling.x - 12, sling.y - 30, 8, 30);
      ctx.fillRect(sling.x + 4, sling.y - 30, 8, 30);

      // Band anchor points
      ctx.fillStyle = '#8B0000';
      ctx.beginPath();
      ctx.arc(sling.x - 8, sling.y - 30, 5, 0, Math.PI * 2);
      ctx.arc(sling.x + 8, sling.y - 30, 5, 0, Math.PI * 2);
      ctx.fill();

      // Draw rubber band and duck in slingshot
      if (isDragging && ducksLeftRef.current > 0) {
        const pullX = Math.max(sling.x - MAX_PULL_DISTANCE, Math.min(dragEnd.x, sling.x + 30));
        const pullY = Math.max(sling.y - MAX_PULL_DISTANCE, Math.min(dragEnd.y, sling.y + MAX_PULL_DISTANCE));

        // Rubber bands
        ctx.strokeStyle = '#8B0000';
        ctx.lineWidth = 4;
        ctx.beginPath();
        ctx.moveTo(sling.x - 8, sling.y - 30);
        ctx.lineTo(pullX, pullY);
        ctx.lineTo(sling.x + 8, sling.y - 30);
        ctx.stroke();

        // Trajectory preview
        const dx = dragStart.x - dragEnd.x;
        const dy = dragStart.y - dragEnd.y;
        const distance = Math.min(Math.sqrt(dx * dx + dy * dy), MAX_PULL_DISTANCE);
        if (distance > 20) {
          const angle = Math.atan2(dy, dx);
          const power = distance * LAUNCH_POWER;
          let prevX = sling.x;
          let prevY = sling.y;
          let vx = Math.cos(angle) * power;
          let vy = Math.sin(angle) * power;

          ctx.setLineDash([5, 5]);
          ctx.strokeStyle = 'rgba(255, 255, 255, 0.5)';
          ctx.lineWidth = 2;
          ctx.beginPath();
          ctx.moveTo(prevX, prevY);

          for (let i = 0; i < 25; i++) {
            vy += GRAVITY;
            prevX += vx;
            prevY += vy;
            if (prevY > ground) break;
            ctx.lineTo(prevX, prevY);
          }
          ctx.stroke();
          ctx.setLineDash([]);
        }

        // Duck being pulled
        drawDuck(ctx, pullX, pullY, 18, 0);
      } else if (!duckRef.current?.launched && ducksLeftRef.current > 0) {
        // Duck waiting in slingshot
        drawDuck(ctx, sling.x, sling.y, 18, 0);
      }

      // Draw launched duck
      if (duckRef.current?.launched) {
        drawDuck(ctx, duckRef.current.x, duckRef.current.y, duckRef.current.size, duckRef.current.rotation);
      }

      // Draw blocks
      blocksRef.current.forEach(block => {
        ctx.save();
        ctx.translate(block.x, block.y);
        ctx.rotate(block.rotation);

        const healthPercent = block.health / (block.type === 'stone' ? 200 : 100);

        if (block.type === 'stone') {
          ctx.fillStyle = `rgba(127, 140, 141, ${0.5 + healthPercent * 0.5})`;
        } else {
          ctx.fillStyle = `rgba(211, 84, 0, ${0.5 + healthPercent * 0.5})`;
        }
        ctx.fillRect(-block.width / 2, -block.height / 2, block.width, block.height);

        // Block outline
        ctx.strokeStyle = block.type === 'stone' ? '#5D6D7E' : '#A04000';
        ctx.lineWidth = 2;
        ctx.strokeRect(-block.width / 2, -block.height / 2, block.width, block.height);

        // Damage cracks
        if (healthPercent < 0.5) {
          ctx.strokeStyle = 'rgba(0, 0, 0, 0.3)';
          ctx.lineWidth = 1;
          ctx.beginPath();
          ctx.moveTo(-block.width * 0.3, -block.height * 0.2);
          ctx.lineTo(0, block.height * 0.1);
          ctx.lineTo(block.width * 0.2, -block.height * 0.3);
          ctx.stroke();
        }

        ctx.restore();
      });

      // Draw piggies
      piggiesRef.current.forEach(piggy => {
        const healthPercent = piggy.health / 100;

        // Body
        ctx.fillStyle = `rgba(39, 174, 96, ${0.6 + healthPercent * 0.4})`;
        ctx.beginPath();
        ctx.arc(piggy.x, piggy.y, piggy.size, 0, Math.PI * 2);
        ctx.fill();

        // Outline
        ctx.strokeStyle = '#1E8449';
        ctx.lineWidth = 2;
        ctx.stroke();

        // Snout
        ctx.fillStyle = '#229954';
        ctx.beginPath();
        ctx.ellipse(piggy.x, piggy.y + piggy.size * 0.3, piggy.size * 0.4, piggy.size * 0.25, 0, 0, Math.PI * 2);
        ctx.fill();

        // Nostrils
        ctx.fillStyle = '#1D8348';
        ctx.beginPath();
        ctx.arc(piggy.x - piggy.size * 0.15, piggy.y + piggy.size * 0.3, 3, 0, Math.PI * 2);
        ctx.arc(piggy.x + piggy.size * 0.15, piggy.y + piggy.size * 0.3, 3, 0, Math.PI * 2);
        ctx.fill();

        // Eyes
        ctx.fillStyle = 'white';
        ctx.beginPath();
        ctx.arc(piggy.x - piggy.size * 0.3, piggy.y - piggy.size * 0.15, piggy.size * 0.22, 0, Math.PI * 2);
        ctx.arc(piggy.x + piggy.size * 0.3, piggy.y - piggy.size * 0.15, piggy.size * 0.22, 0, Math.PI * 2);
        ctx.fill();

        // Pupils
        ctx.fillStyle = '#1a1a1a';
        ctx.beginPath();
        ctx.arc(piggy.x - piggy.size * 0.3, piggy.y - piggy.size * 0.15, piggy.size * 0.1, 0, Math.PI * 2);
        ctx.arc(piggy.x + piggy.size * 0.3, piggy.y - piggy.size * 0.15, piggy.size * 0.1, 0, Math.PI * 2);
        ctx.fill();

        // Ears
        ctx.fillStyle = '#27AE60';
        ctx.beginPath();
        ctx.ellipse(piggy.x - piggy.size * 0.7, piggy.y - piggy.size * 0.5, piggy.size * 0.2, piggy.size * 0.3, -0.3, 0, Math.PI * 2);
        ctx.ellipse(piggy.x + piggy.size * 0.7, piggy.y - piggy.size * 0.5, piggy.size * 0.2, piggy.size * 0.3, 0.3, 0, Math.PI * 2);
        ctx.fill();

        // Damage indicator
        if (healthPercent < 1) {
          ctx.fillStyle = 'rgba(255, 0, 0, 0.3)';
          ctx.beginPath();
          ctx.arc(piggy.x, piggy.y, piggy.size * (1 - healthPercent), 0, Math.PI * 2);
          ctx.fill();
        }
      });

      // Draw debris
      debrisRef.current.forEach(d => {
        ctx.save();
        ctx.globalAlpha = d.life;
        ctx.translate(d.x, d.y);
        ctx.rotate(d.rotation);
        ctx.fillStyle = d.color;
        ctx.fillRect(-d.size / 2, -d.size / 2, d.size, d.size);
        ctx.restore();
      });

      ctx.restore();
    };

    const drawDuck = (ctx, x, y, size, rotation) => {
      ctx.save();
      ctx.translate(x, y);
      ctx.rotate(rotation);

      // Body
      ctx.fillStyle = '#F4D03F';
      ctx.beginPath();
      ctx.arc(0, 0, size, 0, Math.PI * 2);
      ctx.fill();

      // Outline
      ctx.strokeStyle = '#D4AC0D';
      ctx.lineWidth = 2;
      ctx.stroke();

      // Belly
      ctx.fillStyle = '#F9E79F';
      ctx.beginPath();
      ctx.arc(0, size * 0.15, size * 0.6, 0, Math.PI * 2);
      ctx.fill();

      // Beak
      ctx.fillStyle = '#E67E22';
      ctx.beginPath();
      ctx.moveTo(size * 0.6, -size * 0.1);
      ctx.lineTo(size * 1.1, 0);
      ctx.lineTo(size * 0.6, size * 0.15);
      ctx.closePath();
      ctx.fill();

      // Eyes
      ctx.fillStyle = 'white';
      ctx.beginPath();
      ctx.arc(-size * 0.25, -size * 0.25, size * 0.25, 0, Math.PI * 2);
      ctx.arc(size * 0.15, -size * 0.25, size * 0.25, 0, Math.PI * 2);
      ctx.fill();

      // Pupils
      ctx.fillStyle = '#1a1a1a';
      ctx.beginPath();
      ctx.arc(-size * 0.2, -size * 0.25, size * 0.12, 0, Math.PI * 2);
      ctx.arc(size * 0.2, -size * 0.25, size * 0.12, 0, Math.PI * 2);
      ctx.fill();

      // Angry eyebrows
      ctx.strokeStyle = '#C0392B';
      ctx.lineWidth = 3;
      ctx.beginPath();
      ctx.moveTo(-size * 0.45, -size * 0.5);
      ctx.lineTo(-size * 0.05, -size * 0.35);
      ctx.moveTo(size * 0.05, -size * 0.35);
      ctx.lineTo(size * 0.45, -size * 0.5);
      ctx.stroke();

      ctx.restore();
    };

    const gameLoop = () => {
      updateGame();
      render();
      animationFrameRef.current = requestAnimationFrame(gameLoop);
    };

    animationFrameRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    };
  }, [gameState, currentLevel, circleCollision, circleRectCollision, createDebris]);

  // Mouse/touch handlers
  const handlePointerDown = useCallback((e) => {
    if (gameState !== 'playing' || duckRef.current?.launched || ducksLeftRef.current <= 0) return;

    const canvas = canvasRef.current;
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    const sling = slingshotPos.current;
    const dist = Math.sqrt((x - sling.x) ** 2 + (y - sling.y) ** 2);

    if (dist < 50) {
      setIsDragging(true);
      setDragStart({ x: sling.x, y: sling.y });
      setDragEnd({ x, y });
    }
  }, [gameState]);

  const handlePointerMove = useCallback((e) => {
    if (!isDragging) return;

    const canvas = canvasRef.current;
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    setDragEnd({ x, y });
  }, [isDragging]);

  const handlePointerUp = useCallback(() => {
    if (isDragging) {
      launchDuck();
    }
  }, [isDragging, launchDuck]);

  // Next level
  const handleNextLevel = useCallback(() => {
    const nextLevel = currentLevel + 1;
    if (nextLevel >= LEVELS.length) {
      setGameState('gameOver');
    } else {
      setCurrentLevel(nextLevel);
      // Canvas is already initialized, so directly load the level
      loadLevel(nextLevel);
      // Already in playing state, just reset level loaded flag
      levelLoadedRef.current = true;
    }
  }, [currentLevel, loadLevel]);

  // Finish game
  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="angry-ducks-overlay">
      <div className="angry-ducks-container">
        {/* Menu */}
        {gameState === 'menu' && (
          <div className="angry-ducks-menu">
            <h1 className="angry-ducks-title">
              <span>🦆</span> Angry Ducks <span>🦆</span>
            </h1>
            <p className="angry-ducks-subtitle">Launch ducks to defeat the piggies!</p>

            <div className="level-select">
              <h3>Select Level</h3>
              <div className="level-buttons">
                {LEVELS.map((level, i) => (
                  <button
                    key={i}
                    className="level-btn"
                    onClick={() => startGame(i)}
                  >
                    <span className="level-num">{i + 1}</span>
                    <span className="level-name">{level.name}</span>
                  </button>
                ))}
              </div>
            </div>

            <button className="close-btn-menu" onClick={onClose}>
              Back to Games
            </button>
          </div>
        )}

        {/* Playing */}
        {gameState === 'playing' && (
          <div className="angry-ducks-game">
            {/* Header */}
            <div className="ad-header">
              <div className="ad-stat">
                <span>🦆</span>
                <span>{ducksLeft}</span>
              </div>
              <div className="ad-level">Level {currentLevel + 1}</div>
              <div className="ad-stat">
                <span>⭐</span>
                <span>{score}</span>
              </div>
            </div>

            {/* Game Canvas */}
            <div
              ref={gameAreaRef}
              className="ad-game-area"
              onPointerDown={handlePointerDown}
              onPointerMove={handlePointerMove}
              onPointerUp={handlePointerUp}
              onPointerLeave={handlePointerUp}
            >
              <canvas ref={canvasRef} />
            </div>

            {/* Instructions */}
            {!duckRef.current && ducksLeft > 0 && (
              <div className="ad-instructions">
                Pull back the duck and release to launch! 🎯
              </div>
            )}

            {/* Close button */}
            <button className="ad-close-btn" onClick={onClose}>✕</button>
          </div>
        )}

        {/* Level Complete */}
        {gameState === 'levelComplete' && (
          <div className="ad-level-complete">
            <div className="ad-complete-content">
              <h2>
                <span>⭐</span> Level Complete! <span>⭐</span>
              </h2>

              <div className="ad-stars">
                {[1, 2, 3].map(star => (
                  <span
                    key={star}
                    className={`star ${ducksLeftRef.current >= star ? 'earned' : ''}`}
                  >
                    ⭐
                  </span>
                ))}
              </div>

              <div className="ad-final-score">
                Score: {score}
              </div>

              <div className="ad-complete-buttons">
                {currentLevel < LEVELS.length - 1 ? (
                  <button className="ad-next-btn" onClick={handleNextLevel}>
                    Next Level ▶
                  </button>
                ) : (
                  <button className="ad-claim-btn" onClick={handleFinish}>
                    Claim {score} Coins! 🪙
                  </button>
                )}
                <button className="ad-replay-btn" onClick={() => startGame(currentLevel)}>
                  Replay
                </button>
              </div>
            </div>
          </div>
        )}

        {/* Game Over */}
        {gameState === 'gameOver' && (
          <div className="ad-game-over">
            <div className="ad-game-over-content">
              <h2>
                {piggiesRef.current.length === 0 ? (
                  <><span>🎉</span> All Levels Complete! <span>🎉</span></>
                ) : (
                  <><span>💔</span> Out of Ducks! <span>💔</span></>
                )}
              </h2>

              <div className="ad-final-score">
                Final Score: {score}
              </div>

              <div className="ad-game-over-buttons">
                <button className="ad-claim-btn" onClick={handleFinish}>
                  Claim {score} Coins! 🪙
                </button>
                <button className="ad-retry-btn" onClick={() => startGame(0)}>
                  Play Again
                </button>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default AngryDucksGame;
