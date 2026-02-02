import { useState, useEffect, useCallback, useRef } from 'react';
import './PresentCatchGame.css';

const GAME_DURATION = 30000;
const SPAWN_INTERVAL = 400;
const FALL_DURATION_MIN = 3000;
const FALL_DURATION_MAX = 4500;
const BULLET_SPEED = 12;
const FIRE_RATE = 180; // ms between shots
const SHIP_WIDTH = 50;

// Space collectibles with different values
const SPACE_ITEMS = [
  { emoji: '⭐', value: 2, rarity: 'common', size: 30 },
  { emoji: '🌟', value: 3, rarity: 'common', size: 32 },
  { emoji: '✨', value: 2, rarity: 'common', size: 28 },
  { emoji: '💫', value: 3, rarity: 'common', size: 30 },
  { emoji: '🪐', value: 5, rarity: 'uncommon', size: 38 },
  { emoji: '🌙', value: 4, rarity: 'uncommon', size: 35 },
  { emoji: '☄️', value: 6, rarity: 'uncommon', size: 36 },
  { emoji: '🚀', value: 8, rarity: 'rare', size: 34 },
  { emoji: '🛸', value: 10, rarity: 'rare', size: 40 },
  { emoji: '🌌', value: 15, rarity: 'epic', size: 42 },
];

// Generate random space item
const pregenRandom = () => {
  const rand = Math.random();
  let items;
  if (rand < 0.45) {
    items = SPACE_ITEMS.filter(i => i.rarity === 'common');
  } else if (rand < 0.75) {
    items = SPACE_ITEMS.filter(i => i.rarity === 'uncommon');
  } else if (rand < 0.92) {
    items = SPACE_ITEMS.filter(i => i.rarity === 'rare');
  } else {
    items = SPACE_ITEMS.filter(i => i.rarity === 'epic');
  }
  const item = items[Math.floor(Math.random() * items.length)];

  return {
    ...item,
    left: 5 + Math.random() * 90,
    fallDuration: FALL_DURATION_MIN + Math.random() * (FALL_DURATION_MAX - FALL_DURATION_MIN),
    rotation: -15 + Math.random() * 30,
  };
};

const PresentCatchGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [score, setScore] = useState(0);
  const [destroyed, setDestroyed] = useState(0);
  const [combo, setCombo] = useState(0);
  const [showCombo, setShowCombo] = useState(false);
  const [shipX, setShipX] = useState(50); // percentage
  const [bullets, setBullets] = useState([]);
  const [items, setItems] = useState([]);
  const [explosions, setExplosions] = useState([]);

  const scoreRef = useRef(0);
  const destroyedRef = useRef(0);
  const comboRef = useRef(0);
  const lastShotRef = useRef(0);
  const lastComboRef = useRef(0);
  const gameAreaRef = useRef(null);
  const gameStateRef = useRef('countdown');
  const shipXRef = useRef(50);
  const bulletsRef = useRef([]);
  const itemsRef = useRef([]);
  const bulletIdRef = useRef(0);
  const itemIdRef = useRef(0);
  const rafRef = useRef(null);
  const lastSpawnRef = useRef(0);
  const isTouchingRef = useRef(false);
  const touchStartXRef = useRef(0);
  const shipStartXRef = useRef(50);

  useEffect(() => {
    gameStateRef.current = gameState;
  }, [gameState]);

  useEffect(() => {
    shipXRef.current = shipX;
  }, [shipX]);

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
          setScore(scoreRef.current);
          setDestroyed(destroyedRef.current);
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
    return () => clearInterval(timer);
  }, [gameState]);

  // Create explosion effect
  const createExplosion = useCallback((x, y, rarity) => {
    const id = Date.now() + Math.random();
    const colorSets = {
      common: '#00ffff',
      uncommon: '#ff00ff',
      rare: '#ffff00',
      epic: '#ff6600',
    };
    setExplosions(prev => [...prev, { id, x, y, color: colorSets[rarity] || '#00ffff' }]);
    setTimeout(() => {
      setExplosions(prev => prev.filter(e => e.id !== id));
    }, 500);
  }, []);

  // Fire bullet
  const fireBullet = useCallback(() => {
    const now = Date.now();
    if (now - lastShotRef.current < FIRE_RATE) return;
    lastShotRef.current = now;

    const id = bulletIdRef.current++;
    const bullet = {
      id,
      x: shipXRef.current,
      y: 85,
    };
    bulletsRef.current.push(bullet);
    setBullets([...bulletsRef.current]);
  }, []);

  // Handle touch/mouse for ship movement
  const handlePointerDown = useCallback((e) => {
    if (gameStateRef.current !== 'playing') return;
    e.preventDefault();

    const touch = e.touches?.[0] || e;
    const rect = gameAreaRef.current?.getBoundingClientRect();
    if (!rect) return;

    isTouchingRef.current = true;
    touchStartXRef.current = touch.clientX;
    shipStartXRef.current = shipXRef.current;

    // Fire immediately on tap
    fireBullet();
  }, [fireBullet]);

  const handlePointerMove = useCallback((e) => {
    if (!isTouchingRef.current || gameStateRef.current !== 'playing') return;
    e.preventDefault();

    const touch = e.touches?.[0] || e;
    const rect = gameAreaRef.current?.getBoundingClientRect();
    if (!rect) return;

    const deltaX = touch.clientX - touchStartXRef.current;
    const deltaPercent = (deltaX / rect.width) * 100;
    const newX = Math.max(5, Math.min(95, shipStartXRef.current + deltaPercent));

    setShipX(newX);
  }, []);

  const handlePointerUp = useCallback(() => {
    isTouchingRef.current = false;
  }, []);

  // Auto-fire while touching
  useEffect(() => {
    if (gameState !== 'playing') return;

    const fireInterval = setInterval(() => {
      if (isTouchingRef.current) {
        fireBullet();
      }
    }, FIRE_RATE);

    return () => clearInterval(fireInterval);
  }, [gameState, fireBullet]);

  // Main game loop
  useEffect(() => {
    if (gameState !== 'playing') return;

    const gameLoop = (timestamp) => {
      if (gameStateRef.current !== 'playing') return;

      // Spawn items
      if (timestamp - lastSpawnRef.current >= SPAWN_INTERVAL) {
        lastSpawnRef.current = timestamp;
        const id = itemIdRef.current++;
        const data = pregenRandom();
        const item = {
          id,
          ...data,
          y: -5,
          createdAt: timestamp,
          speed: (100 + 5) / (data.fallDuration / 16.67), // percent per frame
        };
        itemsRef.current.push(item);
      }

      // Update items (move down)
      itemsRef.current = itemsRef.current.map(item => ({
        ...item,
        y: item.y + item.speed,
      })).filter(item => item.y < 105);

      // Update bullets (move up)
      bulletsRef.current = bulletsRef.current.map(bullet => ({
        ...bullet,
        y: bullet.y - BULLET_SPEED * 0.3,
      })).filter(bullet => bullet.y > -5);

      // Collision detection
      const bulletsToRemove = new Set();
      const itemsToRemove = new Set();

      for (const bullet of bulletsRef.current) {
        for (const item of itemsRef.current) {
          // Simple distance check (in percentage)
          const dx = Math.abs(bullet.x - item.left);
          const dy = Math.abs(bullet.y - item.y);
          const hitRadius = (item.size / 10) + 2;

          if (dx < hitRadius && dy < hitRadius) {
            bulletsToRemove.add(bullet.id);
            itemsToRemove.add(item.id);

            // Score
            const now = Date.now();
            if (now - lastComboRef.current < 800) {
              comboRef.current = Math.min(comboRef.current + 1, 5);
            } else {
              comboRef.current = 1;
            }
            lastComboRef.current = now;

            const points = item.value * comboRef.current;
            scoreRef.current += points;
            destroyedRef.current += 1;

            // Explosion
            createExplosion(item.left, item.y, item.rarity);

            // Update UI
            setScore(scoreRef.current);
            setDestroyed(destroyedRef.current);
            setCombo(comboRef.current);
            if (comboRef.current >= 2) {
              setShowCombo(true);
              setTimeout(() => setShowCombo(false), 600);
            }
          }
        }
      }

      bulletsRef.current = bulletsRef.current.filter(b => !bulletsToRemove.has(b.id));
      itemsRef.current = itemsRef.current.filter(i => !itemsToRemove.has(i.id));

      setItems([...itemsRef.current]);
      setBullets([...bulletsRef.current]);

      rafRef.current = requestAnimationFrame(gameLoop);
    };

    rafRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (rafRef.current) {
        cancelAnimationFrame(rafRef.current);
      }
    };
  }, [gameState, createExplosion]);

  // Cleanup on game end
  useEffect(() => {
    if (gameState === 'ended') {
      itemsRef.current = [];
      bulletsRef.current = [];
      setItems([]);
      setBullets([]);
      setScore(scoreRef.current);
      setDestroyed(destroyedRef.current);
    }
  }, [gameState]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="present-game-overlay shooter-mode">
      <div className="present-game-container" ref={gameAreaRef}>
        {/* Animated Background */}
        <div className="animated-bg">
          <div className="bg-gradient" />
          <div className="floating-shapes">
            {[...Array(6)].map((_, i) => (
              <div key={i} className={`shape shape-${i + 1}`} />
            ))}
          </div>
        </div>

        {/* Header */}
        <div className="game-header">
          <div className="game-stat score-stat">
            <span className="stat-icon">🪙</span>
            <span className="stat-value">{score}</span>
          </div>
          <div className="game-timer">
            <div className="timer-ring">
              <svg viewBox="0 0 36 36">
                <path
                  className="timer-bg"
                  d="M18 2.0845 a 15.9155 15.9155 0 0 1 0 31.831 a 15.9155 15.9155 0 0 1 0 -31.831"
                />
                <path
                  className="timer-progress"
                  strokeDasharray={`${(timeLeft / (GAME_DURATION / 1000)) * 100}, 100`}
                  d="M18 2.0845 a 15.9155 15.9155 0 0 1 0 31.831 a 15.9155 15.9155 0 0 1 0 -31.831"
                />
              </svg>
              <span className={`timer-value ${timeLeft <= 5 ? 'warning' : ''}`}>
                {timeLeft}
              </span>
            </div>
          </div>
          <div className="game-stat catch-stat">
            <span className="stat-icon">💥</span>
            <span className="stat-value">{destroyed}</span>
          </div>
        </div>

        {/* Combo Display */}
        {showCombo && combo >= 2 && (
          <div className="combo-display" key={combo}>
            <span className="combo-number">x{combo}</span>
            <span className="combo-label">COMBO!</span>
          </div>
        )}

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="countdown-display" key={countdown}>
            <span className="countdown-number">{countdown > 0 ? countdown : 'GO!'}</span>
            <div className="countdown-ring" />
          </div>
        )}

        {/* Game Area */}
        {gameState === 'playing' && (
          <div
            className="shooter-area"
            onTouchStart={handlePointerDown}
            onTouchMove={handlePointerMove}
            onTouchEnd={handlePointerUp}
            onMouseDown={handlePointerDown}
            onMouseMove={handlePointerMove}
            onMouseUp={handlePointerUp}
            onMouseLeave={handlePointerUp}
          >
            {/* Falling items */}
            {items.map(item => (
              <div
                key={item.id}
                className={`space-target rarity-${item.rarity}`}
                style={{
                  left: `${item.left}%`,
                  top: `${item.y}%`,
                  fontSize: `${item.size}px`,
                  transform: `translate(-50%, -50%) rotate(${item.rotation}deg)`,
                }}
              >
                <span className="target-emoji">{item.emoji}</span>
              </div>
            ))}

            {/* Bullets */}
            {bullets.map(bullet => (
              <div
                key={bullet.id}
                className="laser-bullet"
                style={{
                  left: `${bullet.x}%`,
                  top: `${bullet.y}%`,
                }}
              />
            ))}

            {/* Explosions */}
            {explosions.map(exp => (
              <div
                key={exp.id}
                className="explosion"
                style={{
                  left: `${exp.x}%`,
                  top: `${exp.y}%`,
                  '--explosion-color': exp.color,
                }}
              />
            ))}

            {/* Player Ship */}
            <div
              className="player-ship"
              style={{ left: `${shipX}%` }}
            >
              <div className="ship-body">🚀</div>
              <div className="ship-thruster" />
            </div>
          </div>
        )}

        {/* Game Over Screen */}
        {gameState === 'ended' && (
          <div className="game-over-screen">
            <div className="game-over-content">
              <div className="game-over-stars">
                <span className="star star-1">⭐</span>
                <span className="star star-2">🌟</span>
                <span className="star star-3">⭐</span>
              </div>

              <h2 className="game-over-title">
                {score >= 300 ? 'STELLAR!' : score >= 150 ? 'Space Ace!' : 'Nice Flying!'}
              </h2>

              <div className="final-stats">
                <div className="final-stat">
                  <div className="stat-icon-wrapper">
                    <span className="final-stat-icon">💥</span>
                  </div>
                  <div className="stat-info">
                    <span className="final-stat-label">Targets Destroyed</span>
                    <span className="final-stat-value">{destroyed}</span>
                  </div>
                </div>

                <div className="final-stat highlight">
                  <div className="stat-icon-wrapper gold">
                    <span className="final-stat-icon">🪙</span>
                  </div>
                  <div className="stat-info">
                    <span className="final-stat-label">Coins Earned</span>
                    <span className="final-stat-value coins">{score}</span>
                  </div>
                </div>
              </div>

              {destroyed >= 30 && (
                <div className="achievement-badge">
                  <span className="badge-icon">🌌</span>
                  <span className="badge-text">Galaxy Defender!</span>
                </div>
              )}

              <button
                className="claim-rewards-btn"
                onClick={handleFinish}
              >
                <span className="btn-text">Claim {score} Coins!</span>
                <span className="btn-icon">🪙</span>
                <div className="btn-shine" />
              </button>
            </div>
          </div>
        )}

        {/* Instructions */}
        {gameState === 'countdown' && countdown === 3 && (
          <div className="game-instructions">
            <div className="instruction-card">
              <p className="instruction-main">Drag to move, hold to shoot!</p>
              <p className="instruction-sub">
                <span className="good">🚀 Destroy cosmic targets!</span>
              </p>
              <p className="instruction-tip">Chain hits for combo bonus!</p>
            </div>
          </div>
        )}

        {/* Close button */}
        {gameState === 'countdown' && (
          <button className="game-close-btn" onClick={onClose}>
            <span>✕</span>
          </button>
        )}
      </div>
    </div>
  );
};

export default PresentCatchGame;
