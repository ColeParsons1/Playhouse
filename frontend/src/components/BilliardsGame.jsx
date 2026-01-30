import { useState, useEffect, useCallback, useRef } from 'react';
import './BilliardsGame.css';

const GAME_DURATION = 150000; // 2.5 minutes
const TABLE_FRICTION = 0.988;
const BALL_RADIUS = 12;
const POCKET_RADIUS = 24;
const MAX_POWER = 28;
const MIN_VELOCITY = 0.08;
const CPU_THINK_TIME = 1200;
const SPIN_DECAY = 0.95;

// Ball colors for pool with enhanced visuals
const BALL_COLORS = {
  0: { color: '#FFFFFF', stripe: false, name: 'Cue', secondaryColor: '#F0F0F0' },
  1: { color: '#FFD700', stripe: false, name: '1', secondaryColor: '#DAA520' },
  2: { color: '#0066CC', stripe: false, name: '2', secondaryColor: '#004499' },
  3: { color: '#FF3333', stripe: false, name: '3', secondaryColor: '#CC0000' },
  4: { color: '#9932CC', stripe: false, name: '4', secondaryColor: '#7B1FA2' },
  5: { color: '#FF6B35', stripe: false, name: '5', secondaryColor: '#E65100' },
  6: { color: '#228B22', stripe: false, name: '6', secondaryColor: '#1B5E20' },
  7: { color: '#8B0000', stripe: false, name: '7', secondaryColor: '#5D0000' },
  8: { color: '#1A1A1A', stripe: false, name: '8', secondaryColor: '#000000' },
  9: { color: '#FFD700', stripe: true, name: '9', secondaryColor: '#DAA520' },
  10: { color: '#0066CC', stripe: true, name: '10', secondaryColor: '#004499' },
  11: { color: '#FF3333', stripe: true, name: '11', secondaryColor: '#CC0000' },
  12: { color: '#9932CC', stripe: true, name: '12', secondaryColor: '#7B1FA2' },
  13: { color: '#FF6B35', stripe: true, name: '13', secondaryColor: '#E65100' },
  14: { color: '#228B22', stripe: true, name: '14', secondaryColor: '#1B5E20' },
  15: { color: '#8B0000', stripe: true, name: '15', secondaryColor: '#5D0000' },
};

const BilliardsGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [playerScore, setPlayerScore] = useState(0);
  const [cpuScore, setCpuScore] = useState(0);
  const [pocketed, setPocketed] = useState([]);
  const [message, setMessage] = useState('');
  const [isAiming, setIsAiming] = useState(false);
  const [canShoot, setCanShoot] = useState(true);
  const [currentTurn, setCurrentTurn] = useState('player');
  const [playerType, setPlayerType] = useState(null);
  const [cpuType, setCpuType] = useState(null);
  const [cpuAiming, setCpuAiming] = useState(false);
  const [turnMessage, setTurnMessage] = useState('');
  const [consecutivePockets, setConsecutivePockets] = useState(0);
  const [showTrajectory, setShowTrajectory] = useState(true);

  // Refs
  const canvasRef = useRef(null);
  const gameAreaRef = useRef(null);
  const ballsRef = useRef([]);
  const pocketsRef = useRef([]);
  const cueRef = useRef({ angle: 0, power: 0, visible: false, spin: { x: 0, y: 0 } });
  const dragStartRef = useRef(null);
  const playerScoreRef = useRef(0);
  const cpuScoreRef = useRef(0);
  const pocketedRef = useRef([]);
  const gameStateRef = useRef('countdown');
  const animationFrameRef = useRef(null);
  const tableBoundsRef = useRef({ left: 0, top: 0, right: 0, bottom: 0 });
  const currentTurnRef = useRef('player');
  const playerTypeRef = useRef(null);
  const cpuTypeRef = useRef(null);
  const pocketedThisTurnRef = useRef([]);
  const scratchedRef = useRef(false);
  const cpuTimeoutRef = useRef(null);
  const particlesRef = useRef([]);
  const consecutivePocketsRef = useRef(0);
  const lastFrameTimeRef = useRef(0);
  const collisionSoundRef = useRef(0);
  const turnEndingRef = useRef(false);  // Prevent multiple turn end calls

  useEffect(() => {
    gameStateRef.current = gameState;
  }, [gameState]);

  useEffect(() => {
    currentTurnRef.current = currentTurn;
  }, [currentTurn]);

  useEffect(() => {
    playerTypeRef.current = playerType;
    cpuTypeRef.current = cpuType;
  }, [playerType, cpuType]);

  // Initialize game
  const initGame = useCallback(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    // Use CSS dimensions, not canvas.width which includes DPR scaling
    const dpr = window.devicePixelRatio || 1;
    const width = canvas.width / dpr;
    const height = canvas.height / dpr;

    const cushion = 35;
    tableBoundsRef.current = {
      left: cushion,
      top: cushion,
      right: width - cushion,
      bottom: height - cushion,
    };

    // Corner and side pockets
    const pocketPositions = [
      { x: cushion - 8, y: cushion - 8, type: 'corner' },
      { x: width / 2, y: cushion - 10, type: 'side' },
      { x: width - cushion + 8, y: cushion - 8, type: 'corner' },
      { x: cushion - 8, y: height - cushion + 8, type: 'corner' },
      { x: width / 2, y: height - cushion + 10, type: 'side' },
      { x: width - cushion + 8, y: height - cushion + 8, type: 'corner' },
    ];
    pocketsRef.current = pocketPositions;

    const balls = [];
    const startX = width * 0.72;
    const startY = height / 2;
    const spacing = BALL_RADIUS * 2.08;

    // Cue ball with spin properties
    balls.push({
      id: 0,
      x: width * 0.22,
      y: height / 2,
      vx: 0,
      vy: 0,
      spin: { x: 0, y: 0 },
      rotation: 0,
      ...BALL_COLORS[0],
    });

    // Rack order (8-ball in middle of 3rd row)
    // Standard 8-ball rack: apex ball at front, 8-ball in center,
    // corners of back row must be opposite types (solid/stripe)
    const rackOrder = [1, 9, 2, 10, 8, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15];
    let ballIndex = 0;

    // Perfect triangle formation - no random offsets
    for (let row = 0; row < 5; row++) {
      for (let col = 0; col <= row; col++) {
        // Calculate exact positions for tight triangle
        const x = startX + row * spacing * 0.866; // 0.866 = cos(30°) for equilateral triangle
        const y = startY + (col - row / 2) * spacing;
        const ballNum = rackOrder[ballIndex];

        balls.push({
          id: ballNum,
          x: x,  // Exact position - no random offset
          y: y,  // Exact position - no random offset
          vx: 0,
          vy: 0,
          spin: { x: 0, y: 0 },
          rotation: Math.random() * Math.PI * 2,
          ...BALL_COLORS[ballNum],
        });
        ballIndex++;
      }
    }

    ballsRef.current = balls;
    pocketedRef.current = [];
    setPocketed([]);
    particlesRef.current = [];
  }, []);

  useEffect(() => {
    const canvas = canvasRef.current;
    const container = gameAreaRef.current;
    if (!canvas || !container) return;

    const dpr = window.devicePixelRatio || 1;
    // Use fallback values if container dimensions are 0
    const containerWidth = container.clientWidth || 400;
    const containerHeight = container.clientHeight || 350;
    const width = Math.min(containerWidth - 20, 650);
    const height = Math.min(containerHeight - 20, 380);

    // Ensure minimum dimensions
    const finalWidth = Math.max(width, 300);
    const finalHeight = Math.max(height, 200);

    canvas.width = finalWidth * dpr;
    canvas.height = finalHeight * dpr;
    canvas.style.width = `${finalWidth}px`;
    canvas.style.height = `${finalHeight}px`;

    const ctx = canvas.getContext('2d');
    ctx.setTransform(1, 0, 0, 1, 0, 0); // Reset transform before scaling
    ctx.scale(dpr, dpr);

    initGame();
  }, [initGame]);

  useEffect(() => {
    if (gameState !== 'countdown') return;
    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
      setMessage('Your break! 🎱');
      setTurnMessage('Your Turn');
      setTimeout(() => setMessage(''), 2000);
    }
  }, [countdown, gameState]);

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

  useEffect(() => {
    return () => {
      if (cpuTimeoutRef.current) {
        clearTimeout(cpuTimeoutRef.current);
      }
    };
  }, []);

  const createParticles = useCallback((x, y, color, count = 8, type = 'spark') => {
    for (let i = 0; i < count; i++) {
      const angle = (i / count) * Math.PI * 2 + Math.random() * 0.5;
      const speed = 2 + Math.random() * 4;
      particlesRef.current.push({
        x,
        y,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        size: type === 'pocket' ? 4 + Math.random() * 4 : 2 + Math.random() * 3,
        color,
        life: 1,
        type,
      });
    }
  }, []);

  const checkBallsStopped = useCallback(() => {
    return ballsRef.current.every(ball =>
      Math.abs(ball.vx) < MIN_VELOCITY && Math.abs(ball.vy) < MIN_VELOCITY
    );
  }, []);

  // Enhanced CPU AI
  const findBestShot = useCallback(() => {
    const cueBall = ballsRef.current.find(b => b.id === 0);
    if (!cueBall) return null;

    const pockets = pocketsRef.current;
    const cpuBallType = cpuTypeRef.current;
    let bestShot = null;
    let bestScore = -Infinity;

    let targetBalls = ballsRef.current.filter(ball => {
      if (ball.id === 0 || ball.id === 8) return false;
      if (!cpuBallType) return true;
      return cpuBallType === 'solids' ? !ball.stripe : ball.stripe;
    });

    const canShoot8Ball = targetBalls.length === 0 && cpuBallType;
    if (canShoot8Ball) {
      const eightBall = ballsRef.current.find(b => b.id === 8);
      if (eightBall) {
        targetBalls = [eightBall];
      }
    }

    targetBalls.forEach(targetBall => {
      pockets.forEach(pocket => {
        const ballToPocketX = pocket.x - targetBall.x;
        const ballToPocketY = pocket.y - targetBall.y;
        const ballToPocketDist = Math.sqrt(ballToPocketX * ballToPocketX + ballToPocketY * ballToPocketY);

        const hitPointX = targetBall.x - (ballToPocketX / ballToPocketDist) * BALL_RADIUS * 2;
        const hitPointY = targetBall.y - (ballToPocketY / ballToPocketDist) * BALL_RADIUS * 2;

        const cueToBallX = hitPointX - cueBall.x;
        const cueToBallY = hitPointY - cueBall.y;
        const cueToBallDist = Math.sqrt(cueToBallX * cueToBallX + cueToBallY * cueToBallY);

        // Check if path is clear
        let pathClear = true;
        let obstacleCount = 0;

        ballsRef.current.forEach(ball => {
          if (ball.id === 0 || ball.id === targetBall.id) return;

          const t = Math.max(0, Math.min(1,
            ((ball.x - cueBall.x) * cueToBallX + (ball.y - cueBall.y) * cueToBallY) /
            (cueToBallDist * cueToBallDist)
          ));
          const closestX = cueBall.x + t * cueToBallX;
          const closestY = cueBall.y + t * cueToBallY;
          const dist = Math.sqrt((ball.x - closestX) ** 2 + (ball.y - closestY) ** 2);

          if (dist < BALL_RADIUS * 2.3) {
            pathClear = false;
            obstacleCount++;
          }
        });

        if (!pathClear && obstacleCount > 1) return;

        // Calculate angle difficulty
        const cueAngle = Math.atan2(cueToBallY, cueToBallX);
        const pocketAngle = Math.atan2(ballToPocketY, ballToPocketX);
        let angleDiff = Math.abs(cueAngle - pocketAngle);
        if (angleDiff > Math.PI) angleDiff = Math.PI * 2 - angleDiff;

        // Score this shot
        let score = 100;
        score -= cueToBallDist * 0.15;
        score -= ballToPocketDist * 0.1;
        score -= angleDiff * 20;

        if (cueToBallDist < 80) score += 25;
        if (ballToPocketDist < 60) score += 35;
        if (pocket.type === 'corner') score += 10;

        // Penalty for obstacles
        score -= obstacleCount * 30;

        // Add slight randomness
        score += Math.random() * 12;

        if (score > bestScore) {
          bestScore = score;
          bestShot = {
            angle: Math.atan2(cueToBallY, cueToBallX),
            power: Math.min(MAX_POWER, Math.max(10, cueToBallDist * 0.1 + ballToPocketDist * 0.05 + 8)),
            targetBall,
            pocket,
            difficulty: angleDiff,
          };
        }
      });
    });

    // Fallback shot
    if (!bestShot && ballsRef.current.length > 1) {
      const randomBall = ballsRef.current.find(b => b.id !== 0 && b.id !== 8);
      if (randomBall) {
        const dx = randomBall.x - cueBall.x;
        const dy = randomBall.y - cueBall.y;
        bestShot = {
          angle: Math.atan2(dy, dx),
          power: 12 + Math.random() * 6,
          targetBall: randomBall,
          pocket: null,
        };
      }
    }

    return bestShot;
  }, []);

  const cpuTakeTurn = useCallback(() => {
    // Strict turn validation - only proceed if it's actually CPU's turn and game is playing
    if (gameStateRef.current !== 'playing') return;
    if (currentTurnRef.current !== 'cpu') return;

    // Make sure all balls are stopped before CPU takes action
    const allStopped = ballsRef.current.every(ball =>
      Math.abs(ball.vx) < MIN_VELOCITY && Math.abs(ball.vy) < MIN_VELOCITY
    );
    if (!allStopped) {
      // Retry after a short delay if balls are still moving
      cpuTimeoutRef.current = setTimeout(cpuTakeTurn, 300);
      return;
    }

    setTurnMessage('CPU Analyzing...');
    setCpuAiming(true);

    cpuTimeoutRef.current = setTimeout(() => {
      // Double-check it's still CPU's turn (player might have somehow acted)
      if (currentTurnRef.current !== 'cpu' || gameStateRef.current !== 'playing') {
        setCpuAiming(false);
        return;
      }

      const shot = findBestShot();
      if (!shot) {
        setCpuAiming(false);
        switchTurn();
        return;
      }

      cueRef.current.angle = shot.angle;
      cueRef.current.power = 0;
      cueRef.current.visible = true;

      setTurnMessage('CPU Aiming...');

      let currentPower = 0;
      const targetPower = shot.power * (0.85 + Math.random() * 0.3); // Slight variance
      const powerInterval = setInterval(() => {
        // Stop if game state changed
        if (currentTurnRef.current !== 'cpu' || gameStateRef.current !== 'playing') {
          clearInterval(powerInterval);
          cueRef.current.visible = false;
          cueRef.current.power = 0;
          setCpuAiming(false);
          return;
        }

        currentPower += targetPower / 25;
        cueRef.current.power = Math.min(targetPower, currentPower);

        if (currentPower >= targetPower) {
          clearInterval(powerInterval);

          const cueBall = ballsRef.current.find(b => b.id === 0);
          if (cueBall) {
            // Add slight inaccuracy
            const inaccuracy = (Math.random() - 0.5) * 0.08;
            const finalAngle = shot.angle + inaccuracy;

            cueBall.vx = Math.cos(finalAngle) * targetPower;
            cueBall.vy = Math.sin(finalAngle) * targetPower;

            // Add small spin
            cueBall.spin = {
              x: (Math.random() - 0.5) * 0.3,
              y: (Math.random() - 0.5) * 0.3,
            };

            createParticles(cueBall.x, cueBall.y, '#FFFFFF', 6, 'hit');
          }

          cueRef.current.visible = false;
          cueRef.current.power = 0;
          setCpuAiming(false);
          setCanShoot(false);
          pocketedThisTurnRef.current = [];
          scratchedRef.current = false;
        }
      }, 40);
    }, CPU_THINK_TIME);
  }, [findBestShot, createParticles]);

  const switchTurn = useCallback(() => {
    // Clear any pending CPU timeouts before switching
    if (cpuTimeoutRef.current) {
      clearTimeout(cpuTimeoutRef.current);
      cpuTimeoutRef.current = null;
    }

    const newTurn = currentTurnRef.current === 'player' ? 'cpu' : 'player';
    setCurrentTurn(newTurn);
    currentTurnRef.current = newTurn;
    consecutivePocketsRef.current = 0;
    setConsecutivePockets(0);

    if (newTurn === 'player') {
      setTurnMessage('Your Turn');
      setCanShoot(true);
      setCpuAiming(false);
    } else {
      setTurnMessage('CPU Turn');
      setCanShoot(false);
      // Longer delay before CPU starts to make turn transition clear
      cpuTimeoutRef.current = setTimeout(cpuTakeTurn, 1200);
    }
  }, [cpuTakeTurn]);

  const handleTurnEnd = useCallback(() => {
    const pocketedBalls = pocketedThisTurnRef.current;
    const scratched = scratchedRef.current;
    const currentPlayer = currentTurnRef.current;

    // Clear turn state first
    pocketedThisTurnRef.current = [];
    scratchedRef.current = false;

    // Assign ball types on first pocket
    if (!playerTypeRef.current && pocketedBalls.length > 0) {
      const firstBall = pocketedBalls.find(b => b.id !== 8);
      if (firstBall) {
        if (currentPlayer === 'player') {
          const type = firstBall.stripe ? 'stripes' : 'solids';
          setPlayerType(type);
          setCpuType(type === 'solids' ? 'stripes' : 'solids');
          playerTypeRef.current = type;
          cpuTypeRef.current = type === 'solids' ? 'stripes' : 'solids';
          setMessage(`You're ${type}! 🎯`);
          setTimeout(() => setMessage(''), 2500);
        } else {
          const type = firstBall.stripe ? 'stripes' : 'solids';
          setCpuType(type);
          setPlayerType(type === 'solids' ? 'stripes' : 'solids');
          cpuTypeRef.current = type;
          playerTypeRef.current = type === 'solids' ? 'stripes' : 'solids';
          setMessage(`CPU chose ${type}!`);
          setTimeout(() => setMessage(''), 2500);
        }
      }
    }

    // If scratched, always switch turn
    if (scratched) {
      setMessage('Scratch! Other player\'s turn');
      setTimeout(() => setMessage(''), 2000);
      switchTurn();
      return;
    }

    // Check if current player pocketed their ball type
    let pocketedOwnBall = false;
    pocketedBalls.forEach(ball => {
      if (ball.id === 8) return;
      if (!playerTypeRef.current) {
        // No type assigned yet, any ball counts
        pocketedOwnBall = true;
      } else {
        const ballIsSolid = !ball.stripe;
        const playerWantsSolids = playerTypeRef.current === 'solids';
        const isPlayerBall = (playerWantsSolids && ballIsSolid) || (!playerWantsSolids && !ballIsSolid);

        if ((currentPlayer === 'player' && isPlayerBall) ||
            (currentPlayer === 'cpu' && !isPlayerBall)) {
          pocketedOwnBall = true;
        }
      }
    });

    if (pocketedOwnBall && pocketedBalls.length > 0) {
      if (currentPlayer === 'player') {
        setTurnMessage('Nice shot! Go again! 🔥');
        setCanShoot(true);
        setCpuAiming(false);
      } else {
        setTurnMessage('CPU continues...');
        setCanShoot(false);
        // Clear any existing timeout before setting new one
        if (cpuTimeoutRef.current) {
          clearTimeout(cpuTimeoutRef.current);
        }
        cpuTimeoutRef.current = setTimeout(cpuTakeTurn, 1500);
      }
    } else {
      switchTurn();
    }
  }, [switchTurn, cpuTakeTurn]);

  const updatePhysics = useCallback((deltaTime) => {
    const balls = ballsRef.current;
    const bounds = tableBoundsRef.current;
    const pockets = pocketsRef.current;

    let anyMoving = false;

    // Update particles
    particlesRef.current = particlesRef.current.filter(p => {
      p.x += p.vx * deltaTime;
      p.y += p.vy * deltaTime;
      p.life -= 0.025 * deltaTime;
      p.vy += 0.1 * deltaTime;
      return p.life > 0;
    });

    balls.forEach(ball => {
      // Apply velocity
      ball.x += ball.vx * deltaTime;
      ball.y += ball.vy * deltaTime;

      // Apply spin effect on movement
      if (ball.spin) {
        ball.vx += ball.spin.x * 0.02 * deltaTime;
        ball.vy += ball.spin.y * 0.02 * deltaTime;
        ball.spin.x *= SPIN_DECAY;
        ball.spin.y *= SPIN_DECAY;
      }

      // Update rotation based on movement
      const speed = Math.sqrt(ball.vx * ball.vx + ball.vy * ball.vy);
      ball.rotation += speed * 0.03 * deltaTime;

      // Apply friction
      ball.vx *= Math.pow(TABLE_FRICTION, deltaTime);
      ball.vy *= Math.pow(TABLE_FRICTION, deltaTime);

      if (Math.abs(ball.vx) < MIN_VELOCITY) ball.vx = 0;
      if (Math.abs(ball.vy) < MIN_VELOCITY) ball.vy = 0;

      if (Math.abs(ball.vx) > MIN_VELOCITY || Math.abs(ball.vy) > MIN_VELOCITY) {
        anyMoving = true;
      }

      // Cushion collisions with improved bounce
      if (ball.x - BALL_RADIUS < bounds.left) {
        ball.x = bounds.left + BALL_RADIUS;
        ball.vx = -ball.vx * 0.85;
        if (ball.spin) ball.spin.y *= -0.5;
        createParticles(ball.x, ball.y, '#4a2b1a', 4, 'cushion');
      }
      if (ball.x + BALL_RADIUS > bounds.right) {
        ball.x = bounds.right - BALL_RADIUS;
        ball.vx = -ball.vx * 0.85;
        if (ball.spin) ball.spin.y *= -0.5;
        createParticles(ball.x, ball.y, '#4a2b1a', 4, 'cushion');
      }
      if (ball.y - BALL_RADIUS < bounds.top) {
        ball.y = bounds.top + BALL_RADIUS;
        ball.vy = -ball.vy * 0.85;
        if (ball.spin) ball.spin.x *= -0.5;
        createParticles(ball.x, ball.y, '#4a2b1a', 4, 'cushion');
      }
      if (ball.y + BALL_RADIUS > bounds.bottom) {
        ball.y = bounds.bottom - BALL_RADIUS;
        ball.vy = -ball.vy * 0.85;
        if (ball.spin) ball.spin.x *= -0.5;
        createParticles(ball.x, ball.y, '#4a2b1a', 4, 'cushion');
      }
    });

    // Ball-to-ball collisions with improved physics
    for (let i = 0; i < balls.length; i++) {
      for (let j = i + 1; j < balls.length; j++) {
        const b1 = balls[i];
        const b2 = balls[j];

        const dx = b2.x - b1.x;
        const dy = b2.y - b1.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        const minDist = BALL_RADIUS * 2;

        if (dist < minDist && dist > 0) {
          const nx = dx / dist;
          const ny = dy / dist;

          const dvx = b1.vx - b2.vx;
          const dvy = b1.vy - b2.vy;
          const dvn = dvx * nx + dvy * ny;

          if (dvn > 0) {
            const restitution = 0.96;
            const impulse = dvn * restitution;

            b1.vx -= impulse * nx;
            b1.vy -= impulse * ny;
            b2.vx += impulse * nx;
            b2.vy += impulse * ny;

            // Transfer spin
            if (b1.spin && b2.spin) {
              const spinTransfer = 0.2;
              b2.spin.x += b1.spin.x * spinTransfer;
              b2.spin.y += b1.spin.y * spinTransfer;
              b1.spin.x *= 0.7;
              b1.spin.y *= 0.7;
            }

            // Separate overlapping balls
            const overlap = minDist - dist;
            b1.x -= overlap * nx * 0.5;
            b1.y -= overlap * ny * 0.5;
            b2.x += overlap * nx * 0.5;
            b2.y += overlap * ny * 0.5;

            // Create collision particles
            const impactSpeed = Math.abs(dvn);
            if (impactSpeed > 3) {
              const midX = (b1.x + b2.x) / 2;
              const midY = (b1.y + b2.y) / 2;
              createParticles(midX, midY, '#FFFFFF', Math.min(8, Math.floor(impactSpeed)), 'spark');
            }
          }
        }
      }
    }

    // Check pockets
    const ballsToRemove = [];
    balls.forEach(ball => {
      pockets.forEach(pocket => {
        const dx = ball.x - pocket.x;
        const dy = ball.y - pocket.y;
        const dist = Math.sqrt(dx * dx + dy * dy);
        const pocketSize = pocket.type === 'corner' ? POCKET_RADIUS + 4 : POCKET_RADIUS;

        if (dist < pocketSize) {
          ballsToRemove.push(ball);
        }
      });
    });

    // Process pocketed balls
    ballsToRemove.forEach(ball => {
      // Pocket animation particles
      createParticles(ball.x, ball.y, ball.color, 12, 'pocket');

      if (ball.id === 0) {
        setMessage('Scratch! 💥');
        setTimeout(() => setMessage(''), 1800);
        scratchedRef.current = true;
        consecutivePocketsRef.current = 0;
        setConsecutivePockets(0);

        // Respawn cue ball
        const canvas = canvasRef.current;
        ball.x = canvas ? (canvas.width / (window.devicePixelRatio || 1)) * 0.22 : 100;
        ball.y = canvas ? (canvas.height / (window.devicePixelRatio || 1)) / 2 : 150;
        ball.vx = 0;
        ball.vy = 0;
        ball.spin = { x: 0, y: 0 };
      } else if (ball.id === 8) {
        // 8 ball logic
        const currentPlayer = currentTurnRef.current;
        const playerBallsRemaining = ballsRef.current.filter(b => {
          if (b.id === 0 || b.id === 8) return false;
          return playerTypeRef.current === 'solids' ? !b.stripe : b.stripe;
        }).length - ballsToRemove.filter(b => {
          if (b.id === 0 || b.id === 8) return false;
          return playerTypeRef.current === 'solids' ? !b.stripe : b.stripe;
        }).length;

        const cpuBallsRemaining = ballsRef.current.filter(b => {
          if (b.id === 0 || b.id === 8) return false;
          return cpuTypeRef.current === 'solids' ? !b.stripe : b.stripe;
        }).length - ballsToRemove.filter(b => {
          if (b.id === 0 || b.id === 8) return false;
          return cpuTypeRef.current === 'solids' ? !b.stripe : b.stripe;
        }).length;

        if (currentPlayer === 'player') {
          if (playerBallsRemaining === 0) {
            setMessage('You win! 8-ball in! 🏆 +150');
            playerScoreRef.current += 150;
            setPlayerScore(playerScoreRef.current);
          } else {
            setMessage('8-ball too early! CPU wins! 💔');
            cpuScoreRef.current += 75;
            setCpuScore(cpuScoreRef.current);
          }
        } else {
          if (cpuBallsRemaining === 0) {
            setMessage('CPU wins with 8-ball! 🤖');
            cpuScoreRef.current += 150;
            setCpuScore(cpuScoreRef.current);
          } else {
            setMessage('CPU scratched on 8-ball! You win! 🎉 +75');
            playerScoreRef.current += 75;
            setPlayerScore(playerScoreRef.current);
          }
        }

        ballsRef.current = balls.filter(b => b.id !== 8);
        pocketedRef.current.push(ball.id);
        setPocketed([...pocketedRef.current]);
        pocketedThisTurnRef.current.push(ball);

        setTimeout(() => endGame(), 2500);
      } else {
        // Regular ball
        const currentPlayer = currentTurnRef.current;
        const isPlayerBall = playerTypeRef.current === 'solids' ? !ball.stripe : ball.stripe;

        if (currentPlayer === 'player' && (isPlayerBall || !playerTypeRef.current)) {
          consecutivePocketsRef.current++;
          setConsecutivePockets(consecutivePocketsRef.current);

          const basePoints = 15;
          const streakBonus = Math.min(consecutivePocketsRef.current - 1, 3) * 5;
          const points = basePoints + streakBonus;

          playerScoreRef.current += points;
          setPlayerScore(playerScoreRef.current);

          if (consecutivePocketsRef.current >= 2) {
            setMessage(`${ball.stripe ? 'Stripe' : 'Solid'} ${ball.name}! +${points} 🔥x${consecutivePocketsRef.current}`);
          } else {
            setMessage(`${ball.stripe ? 'Stripe' : 'Solid'} ${ball.name}! +${points}`);
          }
        } else if (currentPlayer === 'cpu' && (!isPlayerBall || !cpuTypeRef.current)) {
          const points = 15;
          cpuScoreRef.current += points;
          setCpuScore(cpuScoreRef.current);
          setMessage(`CPU pockets ${ball.stripe ? 'stripe' : 'solid'} ${ball.name}!`);
        } else {
          if (currentPlayer === 'player') {
            cpuScoreRef.current += 8;
            setCpuScore(cpuScoreRef.current);
            setMessage(`Oops! That was CPU's ball! 😬`);
            consecutivePocketsRef.current = 0;
            setConsecutivePockets(0);
          } else {
            playerScoreRef.current += 8;
            setPlayerScore(playerScoreRef.current);
            setMessage(`CPU pocketed your ball! Lucky! 🍀`);
          }
        }
        setTimeout(() => setMessage(''), 2000);

        ballsRef.current = balls.filter(b => b.id !== ball.id);
        pocketedRef.current.push(ball.id);
        setPocketed([...pocketedRef.current]);
        pocketedThisTurnRef.current.push(ball);
      }
    });

    // Check if balls stopped and handle turn end
    // Only trigger once when balls come to a stop, not continuously
    if (!anyMoving && !canShoot && !cpuAiming && gameStateRef.current === 'playing') {
      // Use a flag to prevent multiple handleTurnEnd calls
      if (!turnEndingRef.current) {
        turnEndingRef.current = true;
        // Small delay to ensure all state is settled
        setTimeout(() => {
          if (gameStateRef.current === 'playing') {
            handleTurnEnd();
          }
          turnEndingRef.current = false;
        }, 200);
      }
    }

    // Win condition
    if (ballsRef.current.length === 1 && ballsRef.current[0].id === 0) {
      endGame();
    }
  }, [canShoot, cpuAiming, handleTurnEnd, createParticles]);

  const renderGame = useCallback(() => {
    const canvas = canvasRef.current;
    const ctx = canvas?.getContext('2d');
    if (!canvas || !ctx) return;

    const dpr = window.devicePixelRatio || 1;
    const width = canvas.width / dpr;
    const height = canvas.height / dpr;

    // Reset transform and apply DPR scaling
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.scale(dpr, dpr);

    // Clear and draw background
    ctx.fillStyle = '#0a3d0a';
    ctx.fillRect(0, 0, width, height);

    // Felt with texture
    const feltGradient = ctx.createRadialGradient(
      width / 2, height / 2, 0,
      width / 2, height / 2, width * 0.6
    );
    feltGradient.addColorStop(0, '#1a7a1a');
    feltGradient.addColorStop(0.5, '#156515');
    feltGradient.addColorStop(1, '#0d4f0d');
    ctx.fillStyle = feltGradient;
    ctx.fillRect(30, 30, width - 60, height - 60);

    // Felt texture dots
    ctx.fillStyle = 'rgba(0, 60, 0, 0.3)';
    for (let i = 0; i < 200; i++) {
      const x = 35 + Math.random() * (width - 70);
      const y = 35 + Math.random() * (height - 70);
      ctx.beginPath();
      ctx.arc(x, y, 0.8, 0, Math.PI * 2);
      ctx.fill();
    }

    // Rails with wood grain effect
    const railGradient = ctx.createLinearGradient(0, 0, 0, 35);
    railGradient.addColorStop(0, '#5a2d0a');
    railGradient.addColorStop(0.5, '#7a4a20');
    railGradient.addColorStop(1, '#4a2008');

    ctx.fillStyle = railGradient;
    ctx.fillRect(0, 0, width, 35);
    ctx.fillRect(0, height - 35, width, 35);

    const sideRailGradient = ctx.createLinearGradient(0, 0, 35, 0);
    sideRailGradient.addColorStop(0, '#5a2d0a');
    sideRailGradient.addColorStop(0.5, '#7a4a20');
    sideRailGradient.addColorStop(1, '#4a2008');

    ctx.fillStyle = sideRailGradient;
    ctx.fillRect(0, 0, 35, height);
    ctx.fillRect(width - 35, 0, 35, height);

    // Wood grain lines
    ctx.strokeStyle = 'rgba(90, 50, 20, 0.4)';
    ctx.lineWidth = 1;
    for (let i = 0; i < width; i += 12) {
      ctx.beginPath();
      ctx.moveTo(i, 0);
      ctx.lineTo(i + 3, 35);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(i, height - 35);
      ctx.lineTo(i + 3, height);
      ctx.stroke();
    }

    // Inner cushion edge (green rubber)
    ctx.fillStyle = '#2d8b2d';
    ctx.fillRect(32, 32, width - 64, 5);
    ctx.fillRect(32, height - 37, width - 64, 5);
    ctx.fillRect(32, 32, 5, height - 64);
    ctx.fillRect(width - 37, 32, 5, height - 64);

    // Pockets with depth effect
    pocketsRef.current.forEach(pocket => {
      // Outer shadow
      ctx.fillStyle = 'rgba(0, 0, 0, 0.5)';
      ctx.beginPath();
      ctx.arc(pocket.x, pocket.y, POCKET_RADIUS + 8, 0, Math.PI * 2);
      ctx.fill();

      // Pocket opening
      const pocketGradient = ctx.createRadialGradient(
        pocket.x, pocket.y, 0,
        pocket.x, pocket.y, POCKET_RADIUS + 4
      );
      pocketGradient.addColorStop(0, '#000000');
      pocketGradient.addColorStop(0.7, '#1a1a1a');
      pocketGradient.addColorStop(1, '#333333');
      ctx.fillStyle = pocketGradient;
      ctx.beginPath();
      ctx.arc(pocket.x, pocket.y, POCKET_RADIUS + 4, 0, Math.PI * 2);
      ctx.fill();

      // Inner pocket
      ctx.fillStyle = '#000';
      ctx.beginPath();
      ctx.arc(pocket.x, pocket.y, POCKET_RADIUS - 2, 0, Math.PI * 2);
      ctx.fill();
    });

    // Diamond markers
    ctx.fillStyle = '#D4AF37';
    ctx.strokeStyle = '#8B6914';
    ctx.lineWidth = 1;
    [0.25, 0.5, 0.75].forEach(pos => {
      [{ x: width * pos, y: 17 }, { x: width * pos, y: height - 17 }].forEach(p => {
        ctx.beginPath();
        ctx.moveTo(p.x, p.y - 5);
        ctx.lineTo(p.x + 4, p.y);
        ctx.lineTo(p.x, p.y + 5);
        ctx.lineTo(p.x - 4, p.y);
        ctx.closePath();
        ctx.fill();
        ctx.stroke();
      });
    });
    [0.33, 0.67].forEach(pos => {
      [{ x: 17, y: height * pos }, { x: width - 17, y: height * pos }].forEach(p => {
        ctx.beginPath();
        ctx.moveTo(p.x, p.y - 5);
        ctx.lineTo(p.x + 4, p.y);
        ctx.lineTo(p.x, p.y + 5);
        ctx.lineTo(p.x - 4, p.y);
        ctx.closePath();
        ctx.fill();
        ctx.stroke();
      });
    });

    // Head string
    ctx.strokeStyle = 'rgba(255, 255, 255, 0.15)';
    ctx.setLineDash([4, 6]);
    ctx.lineWidth = 1;
    ctx.beginPath();
    ctx.moveTo(width * 0.25, 35);
    ctx.lineTo(width * 0.25, height - 35);
    ctx.stroke();
    ctx.setLineDash([]);

    // Draw balls with enhanced visuals
    ballsRef.current.forEach(ball => {
      // Shadow
      ctx.fillStyle = 'rgba(0, 0, 0, 0.35)';
      ctx.beginPath();
      ctx.ellipse(ball.x + 4, ball.y + 4, BALL_RADIUS, BALL_RADIUS * 0.6, 0, 0, Math.PI * 2);
      ctx.fill();

      // Ball base gradient
      const ballGradient = ctx.createRadialGradient(
        ball.x - BALL_RADIUS * 0.35, ball.y - BALL_RADIUS * 0.35, 0,
        ball.x, ball.y, BALL_RADIUS * 1.1
      );

      if (ball.id === 0) {
        ballGradient.addColorStop(0, '#FFFFFF');
        ballGradient.addColorStop(0.6, '#F5F5F5');
        ballGradient.addColorStop(1, '#D0D0D0');
      } else {
        ballGradient.addColorStop(0, lightenColor(ball.color, 30));
        ballGradient.addColorStop(0.5, ball.color);
        ballGradient.addColorStop(1, ball.secondaryColor || darkenColor(ball.color, 40));
      }

      ctx.fillStyle = ballGradient;
      ctx.beginPath();
      ctx.arc(ball.x, ball.y, BALL_RADIUS, 0, Math.PI * 2);
      ctx.fill();

      // Stripe pattern for striped balls
      if (ball.stripe) {
        ctx.save();
        ctx.beginPath();
        ctx.arc(ball.x, ball.y, BALL_RADIUS, 0, Math.PI * 2);
        ctx.clip();

        ctx.fillStyle = '#FFFFFF';
        ctx.fillRect(ball.x - BALL_RADIUS, ball.y - BALL_RADIUS * 0.4, BALL_RADIUS * 2, BALL_RADIUS * 0.8);

        ctx.restore();
      }

      // Number circle and number
      if (ball.id !== 0) {
        // White circle
        ctx.fillStyle = '#FFFFFF';
        ctx.beginPath();
        ctx.arc(ball.x, ball.y, BALL_RADIUS * 0.42, 0, Math.PI * 2);
        ctx.fill();

        // Number
        ctx.fillStyle = '#000000';
        ctx.font = `bold ${BALL_RADIUS * 0.55}px Arial`;
        ctx.textAlign = 'center';
        ctx.textBaseline = 'middle';
        ctx.fillText(ball.id.toString(), ball.x, ball.y + 1);
      }

      // Highlight/shine
      ctx.fillStyle = 'rgba(255, 255, 255, 0.45)';
      ctx.beginPath();
      ctx.ellipse(
        ball.x - BALL_RADIUS * 0.35,
        ball.y - BALL_RADIUS * 0.35,
        BALL_RADIUS * 0.3,
        BALL_RADIUS * 0.2,
        -Math.PI / 4,
        0,
        Math.PI * 2
      );
      ctx.fill();

      // Secondary small highlight
      ctx.fillStyle = 'rgba(255, 255, 255, 0.25)';
      ctx.beginPath();
      ctx.arc(ball.x - BALL_RADIUS * 0.2, ball.y - BALL_RADIUS * 0.4, BALL_RADIUS * 0.12, 0, Math.PI * 2);
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

    // Cue stick and aiming
    const cueBall = ballsRef.current.find(b => b.id === 0);
    if (cueBall && cueRef.current.visible && gameStateRef.current === 'playing') {
      const angle = cueRef.current.angle;
      const power = cueRef.current.power;
      const pullBack = power * 3.5;

      // Trajectory prediction
      if (showTrajectory && currentTurnRef.current === 'player') {
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.25)';
        ctx.setLineDash([6, 8]);
        ctx.lineWidth = 1.5;
        ctx.beginPath();
        ctx.moveTo(cueBall.x, cueBall.y);

        let predictX = cueBall.x;
        let predictY = cueBall.y;
        let predictVx = Math.cos(angle) * power * 0.8;
        let predictVy = Math.sin(angle) * power * 0.8;

        for (let i = 0; i < 50; i++) {
          predictX += predictVx * 0.5;
          predictY += predictVy * 0.5;
          predictVx *= 0.99;
          predictVy *= 0.99;

          // Check bounds
          if (predictX < tableBoundsRef.current.left + BALL_RADIUS ||
              predictX > tableBoundsRef.current.right - BALL_RADIUS ||
              predictY < tableBoundsRef.current.top + BALL_RADIUS ||
              predictY > tableBoundsRef.current.bottom - BALL_RADIUS) {
            break;
          }

          ctx.lineTo(predictX, predictY);
        }
        ctx.stroke();
        ctx.setLineDash([]);

        // Ghost ball at end
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.2)';
        ctx.lineWidth = 1;
        ctx.beginPath();
        ctx.arc(predictX, predictY, BALL_RADIUS, 0, Math.PI * 2);
        ctx.stroke();
      }

      // Aim line
      ctx.strokeStyle = currentTurnRef.current === 'player'
        ? 'rgba(100, 255, 100, 0.4)'
        : 'rgba(255, 100, 100, 0.4)';
      ctx.lineWidth = 2;
      ctx.setLineDash([8, 6]);
      ctx.beginPath();
      ctx.moveTo(cueBall.x, cueBall.y);
      ctx.lineTo(
        cueBall.x + Math.cos(angle) * 180,
        cueBall.y + Math.sin(angle) * 180
      );
      ctx.stroke();
      ctx.setLineDash([]);

      // Cue stick shadow
      const cueLength = 220;
      const cueStart = BALL_RADIUS + 8 + pullBack;

      ctx.strokeStyle = 'rgba(0, 0, 0, 0.35)';
      ctx.lineWidth = 12;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(
        cueBall.x - Math.cos(angle) * cueStart + 3,
        cueBall.y - Math.sin(angle) * cueStart + 3
      );
      ctx.lineTo(
        cueBall.x - Math.cos(angle) * (cueStart + cueLength) + 3,
        cueBall.y - Math.sin(angle) * (cueStart + cueLength) + 3
      );
      ctx.stroke();

      // Cue stick main body
      const cueGradient = ctx.createLinearGradient(
        cueBall.x - Math.cos(angle) * cueStart,
        cueBall.y - Math.sin(angle) * cueStart,
        cueBall.x - Math.cos(angle) * (cueStart + cueLength),
        cueBall.y - Math.sin(angle) * (cueStart + cueLength)
      );
      cueGradient.addColorStop(0, '#E8D4B8');
      cueGradient.addColorStop(0.02, '#F5DEB3');
      cueGradient.addColorStop(0.15, '#DEB887');
      cueGradient.addColorStop(0.7, '#A0522D');
      cueGradient.addColorStop(1, '#5D3A1A');

      ctx.strokeStyle = cueGradient;
      ctx.lineWidth = 10;
      ctx.beginPath();
      ctx.moveTo(
        cueBall.x - Math.cos(angle) * cueStart,
        cueBall.y - Math.sin(angle) * cueStart
      );
      ctx.lineTo(
        cueBall.x - Math.cos(angle) * (cueStart + cueLength),
        cueBall.y - Math.sin(angle) * (cueStart + cueLength)
      );
      ctx.stroke();

      // Cue tip
      ctx.fillStyle = '#6BB5FF';
      ctx.beginPath();
      ctx.arc(
        cueBall.x - Math.cos(angle) * cueStart,
        cueBall.y - Math.sin(angle) * cueStart,
        5, 0, Math.PI * 2
      );
      ctx.fill();

      // Cue wrap decoration
      const wrapStart = cueStart + cueLength * 0.6;
      const wrapEnd = cueStart + cueLength * 0.75;
      ctx.strokeStyle = '#2F2F2F';
      ctx.lineWidth = 11;
      ctx.beginPath();
      ctx.moveTo(
        cueBall.x - Math.cos(angle) * wrapStart,
        cueBall.y - Math.sin(angle) * wrapStart
      );
      ctx.lineTo(
        cueBall.x - Math.cos(angle) * wrapEnd,
        cueBall.y - Math.sin(angle) * wrapEnd
      );
      ctx.stroke();

      // Power meter
      if (currentTurnRef.current === 'player') {
        const meterX = 25;
        const meterY = height - 55;
        const meterWidth = 110;
        const meterHeight = 16;

        // Meter background
        ctx.fillStyle = 'rgba(0, 0, 0, 0.7)';
        ctx.beginPath();
        ctx.roundRect(meterX - 3, meterY - 3, meterWidth + 6, meterHeight + 6, 8);
        ctx.fill();

        ctx.fillStyle = '#222';
        ctx.beginPath();
        ctx.roundRect(meterX, meterY, meterWidth, meterHeight, 5);
        ctx.fill();

        // Power fill
        const powerPercent = power / MAX_POWER;
        const powerGradient = ctx.createLinearGradient(meterX, 0, meterX + meterWidth, 0);
        powerGradient.addColorStop(0, '#00DD00');
        powerGradient.addColorStop(0.4, '#AADD00');
        powerGradient.addColorStop(0.7, '#FFAA00');
        powerGradient.addColorStop(1, '#FF3300');
        ctx.fillStyle = powerGradient;
        ctx.beginPath();
        ctx.roundRect(meterX + 2, meterY + 2, (meterWidth - 4) * powerPercent, meterHeight - 4, 3);
        ctx.fill();

        // Label
        ctx.fillStyle = 'white';
        ctx.font = 'bold 11px Arial';
        ctx.textAlign = 'center';
        ctx.fillText('POWER', meterX + meterWidth / 2, meterY - 8);
      }
    }
  }, [showTrajectory]);

  // Helper functions for colors
  const lightenColor = (color, percent) => {
    const num = parseInt(color.replace('#', ''), 16);
    const amt = Math.round(2.55 * percent);
    const R = Math.min(255, (num >> 16) + amt);
    const G = Math.min(255, (num >> 8 & 0x00FF) + amt);
    const B = Math.min(255, (num & 0x0000FF) + amt);
    return '#' + (0x1000000 + R * 0x10000 + G * 0x100 + B).toString(16).slice(1);
  };

  const darkenColor = (color, percent) => {
    const num = parseInt(color.replace('#', ''), 16);
    const amt = Math.round(2.55 * percent);
    const R = Math.max(0, (num >> 16) - amt);
    const G = Math.max(0, (num >> 8 & 0x00FF) - amt);
    const B = Math.max(0, (num & 0x0000FF) - amt);
    return '#' + (0x1000000 + R * 0x10000 + G * 0x100 + B).toString(16).slice(1);
  };

  useEffect(() => {
    if (gameState !== 'playing' && gameState !== 'countdown') return;

    const gameLoop = (timestamp) => {
      const deltaTime = Math.min((timestamp - lastFrameTimeRef.current) / 16.67, 2.5);
      lastFrameTimeRef.current = timestamp;

      if (gameStateRef.current === 'playing') {
        updatePhysics(deltaTime);
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
  }, [gameState, updatePhysics, renderGame]);

  const getCanvasCoords = useCallback((e) => {
    const canvas = canvasRef.current;
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const scaleX = (canvas.width / dpr) / rect.width;
    const scaleY = (canvas.height / dpr) / rect.height;

    const clientX = e.touches ? e.touches[0].clientX : e.clientX;
    const clientY = e.touches ? e.touches[0].clientY : e.clientY;

    return {
      x: (clientX - rect.left) * scaleX,
      y: (clientY - rect.top) * scaleY,
    };
  }, []);

  const handleMouseDown = useCallback((e) => {
    // Strict validation: must be playing, player's turn, canShoot true, and not CPU aiming
    if (gameStateRef.current !== 'playing') return;
    if (currentTurnRef.current !== 'player') return;
    if (!canShoot) return;
    if (cpuAiming) return;

    // Also check balls are stopped
    const allStopped = ballsRef.current.every(ball =>
      Math.abs(ball.vx) < MIN_VELOCITY && Math.abs(ball.vy) < MIN_VELOCITY
    );
    if (!allStopped) return;

    e.preventDefault();

    const coords = getCanvasCoords(e);
    const cueBall = ballsRef.current.find(b => b.id === 0);
    if (!cueBall) return;

    const dx = coords.x - cueBall.x;
    const dy = coords.y - cueBall.y;
    const dist = Math.sqrt(dx * dx + dy * dy);

    if (dist < 120) {
      setIsAiming(true);
      dragStartRef.current = coords;
      cueRef.current.visible = true;
      cueRef.current.angle = Math.atan2(dy, dx);
    }
  }, [canShoot, cpuAiming, getCanvasCoords]);

  const handleMouseMove = useCallback((e) => {
    if (!isAiming) return;
    e.preventDefault();

    const coords = getCanvasCoords(e);
    const cueBall = ballsRef.current.find(b => b.id === 0);
    if (!cueBall || !dragStartRef.current) return;

    const dx = coords.x - cueBall.x;
    const dy = coords.y - cueBall.y;
    cueRef.current.angle = Math.atan2(dy, dx);

    const dragDx = dragStartRef.current.x - coords.x;
    const dragDy = dragStartRef.current.y - coords.y;
    const dragDist = Math.sqrt(dragDx * dragDx + dragDy * dragDy);
    cueRef.current.power = Math.min(MAX_POWER, dragDist / 4.5);
  }, [isAiming, getCanvasCoords]);

  const handleMouseUp = useCallback((e) => {
    if (!isAiming) return;
    e.preventDefault();

    const cueBall = ballsRef.current.find(b => b.id === 0);
    if (cueBall && cueRef.current.power > 1.5) {
      const angle = cueRef.current.angle;
      const power = cueRef.current.power;

      cueBall.vx = Math.cos(angle) * power;
      cueBall.vy = Math.sin(angle) * power;
      cueBall.spin = { x: 0, y: 0 };

      createParticles(cueBall.x, cueBall.y, '#FFFFFF', 6, 'hit');
      setCanShoot(false);
      pocketedThisTurnRef.current = [];
      scratchedRef.current = false;
    }

    setIsAiming(false);
    cueRef.current.visible = false;
    cueRef.current.power = 0;
    dragStartRef.current = null;
  }, [isAiming, createParticles]);

  const endGame = useCallback(() => {
    setGameState('ended');
    if (cpuTimeoutRef.current) {
      clearTimeout(cpuTimeoutRef.current);
    }
  }, []);

  const handleFinish = useCallback(() => {
    onGameEnd(playerScoreRef.current);
  }, [onGameEnd]);

  const playerWon = playerScore > cpuScore;

  return (
    <div className="billiards-overlay">
      <div className="billiards-container">
        {/* Header */}
        <div className="billiards-header">
          <div className={`billiards-player-stat ${currentTurn === 'player' ? 'active' : ''}`}>
            <span className="player-label">You {playerType ? `(${playerType})` : ''}</span>
            <span className="player-score">{playerScore}</span>
          </div>
          <div className="billiards-timer">
            <span className="billiards-timer-icon">⏱️</span>
            <span className={`billiards-timer-value ${timeLeft <= 15 ? 'warning' : ''}`}>
              {Math.floor(timeLeft / 60)}:{(timeLeft % 60).toString().padStart(2, '0')}
            </span>
          </div>
          <div className={`billiards-player-stat cpu ${currentTurn === 'cpu' ? 'active' : ''}`}>
            <span className="player-label">CPU {cpuType ? `(${cpuType})` : ''}</span>
            <span className="player-score">{cpuScore}</span>
          </div>
        </div>

        {/* Streak indicator */}
        {consecutivePockets >= 2 && currentTurn === 'player' && gameState === 'playing' && (
          <div className="billiards-streak">
            🔥 {consecutivePockets}x Streak!
          </div>
        )}

        {/* Turn indicator */}
        {turnMessage && gameState === 'playing' && (
          <div className={`billiards-turn-indicator ${currentTurn}`}>
            {turnMessage}
          </div>
        )}

        {/* Game message */}
        {message && (
          <div className="billiards-message">
            {message}
          </div>
        )}

        {/* Game Canvas */}
        <div ref={gameAreaRef} className="billiards-game-area">
          <canvas
            ref={canvasRef}
            onMouseDown={handleMouseDown}
            onMouseMove={handleMouseMove}
            onMouseUp={handleMouseUp}
            onMouseLeave={handleMouseUp}
            onTouchStart={handleMouseDown}
            onTouchMove={handleMouseMove}
            onTouchEnd={handleMouseUp}
          />
        </div>

        {/* Pocketed balls */}
        <div className="billiards-pocketed">
          <div className="pocketed-section">
            <span className="pocketed-label">You:</span>
            {pocketed.filter(id => {
              if (!playerType || id === 8) return false;
              return playerType === 'solids' ? id <= 7 : id >= 9 && id <= 15;
            }).map(ballId => (
              <div
                key={ballId}
                className="pocketed-ball"
                style={{
                  backgroundColor: BALL_COLORS[ballId].color,
                  border: BALL_COLORS[ballId].stripe ? '3px solid white' : 'none',
                }}
              >
                <span>{ballId}</span>
              </div>
            ))}
          </div>
          <div className="pocketed-section">
            <span className="pocketed-label">CPU:</span>
            {pocketed.filter(id => {
              if (!cpuType || id === 8) return false;
              return cpuType === 'solids' ? id <= 7 : id >= 9 && id <= 15;
            }).map(ballId => (
              <div
                key={ballId}
                className="pocketed-ball"
                style={{
                  backgroundColor: BALL_COLORS[ballId].color,
                  border: BALL_COLORS[ballId].stripe ? '3px solid white' : 'none',
                }}
              >
                <span>{ballId}</span>
              </div>
            ))}
          </div>
        </div>

        {/* Hint */}
        {canShoot && currentTurn === 'player' && gameState === 'playing' && !isAiming && (
          <div className="billiards-hint">
            Drag from cue ball to aim & shoot 🎱
          </div>
        )}

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="billiards-countdown" key={countdown}>
            {countdown > 0 ? countdown : 'BREAK!'}
          </div>
        )}

        {/* Game Over */}
        {gameState === 'ended' && (
          <div className="billiards-game-over">
            <div className="billiards-game-over-content">
              <h2 className="billiards-game-over-title">
                {playerWon ? (
                  <><span>🏆</span> You Win! <span>🏆</span></>
                ) : playerScore === cpuScore ? (
                  <><span>🤝</span> It's a Tie! <span>🤝</span></>
                ) : (
                  <><span>🎱</span> CPU Wins <span>🎱</span></>
                )}
              </h2>

              <div className="billiards-final-stats">
                <div className={`billiards-final-stat ${playerWon ? 'highlight' : ''}`}>
                  <span className="billiards-final-icon">👤</span>
                  <span className="billiards-final-label">Your Score</span>
                  <span className="billiards-final-value">{playerScore}</span>
                </div>
                <div className={`billiards-final-stat ${!playerWon && cpuScore > playerScore ? 'cpu-highlight' : ''}`}>
                  <span className="billiards-final-icon">🤖</span>
                  <span className="billiards-final-label">CPU Score</span>
                  <span className="billiards-final-value">{cpuScore}</span>
                </div>
              </div>

              {playerWon && (
                <div className="billiards-achievement">
                  <span>🎱</span> Pool Shark! <span>🎱</span>
                </div>
              )}

              <button
                className="billiards-claim-btn cute-button"
                onClick={handleFinish}
              >
                Claim {playerScore} Coins! 🪙
              </button>
            </div>
          </div>
        )}

        {/* Close button */}
        {gameState === 'countdown' && (
          <button className="billiards-close-btn" onClick={onClose}>
            ✕
          </button>
        )}
      </div>
    </div>
  );
};

export default BilliardsGame;
