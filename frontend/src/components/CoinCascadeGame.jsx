import { useState, useEffect, useCallback, useRef } from 'react';
import './CoinCascadeGame.css';

const GAME_DURATION = 15000;
const COIN_SPAWN_INTERVAL = 300;
const COIN_FALL_SPEED = 4; // pixels per frame
const BASKET_WIDTH = 80;
const COIN_SIZE = 40;

// Reduced coin values for profit-focused model
const COIN_TYPES = [
  { emoji: '🪙', value: 1, weight: 70 },
  { emoji: '💰', value: 2, weight: 20 },
  { emoji: '💎', value: 3, weight: 8 },
  { emoji: '⭐', value: 5, weight: 2 },
];

const getRandomCoinType = () => {
  const rand = Math.random() * 100;
  let cumulative = 0;
  for (const coin of COIN_TYPES) {
    cumulative += coin.weight;
    if (rand <= cumulative) return coin;
  }
  return COIN_TYPES[0];
};

const CoinCascadeGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [score, setScore] = useState(0);
  const [caught, setCaught] = useState(0);
  const [basketX, setBasketX] = useState(50); // percentage

  const scoreRef = useRef(0);
  const caughtRef = useRef(0);
  const basketXRef = useRef(50);
  const coinsRef = useRef([]);
  const coinIdRef = useRef(0);
  const gameAreaRef = useRef(null);
  const coinsContainerRef = useRef(null);
  const gameStateRef = useRef('countdown');
  const lastSpawnRef = useRef(0);
  const rafRef = useRef(null);
  const isDraggingRef = useRef(false);
  const touchStartXRef = useRef(0);
  const basketStartXRef = useRef(50);

  useEffect(() => {
    gameStateRef.current = gameState;
  }, [gameState]);

  useEffect(() => {
    basketXRef.current = basketX;
  }, [basketX]);

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
          setCaught(caughtRef.current);
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
    return () => clearInterval(timer);
  }, [gameState]);

  // Main game loop
  useEffect(() => {
    if (gameState !== 'playing') return;

    const container = coinsContainerRef.current;
    const gameArea = gameAreaRef.current;
    if (!container || !gameArea) return;

    const gameRect = gameArea.getBoundingClientRect();
    const basketY = gameRect.height - 100; // Basket position from top

    const gameLoop = (timestamp) => {
      if (gameStateRef.current !== 'playing') return;

      // Spawn coins
      if (timestamp - lastSpawnRef.current >= COIN_SPAWN_INTERVAL) {
        lastSpawnRef.current = timestamp;

        const id = coinIdRef.current++;
        const coinType = getRandomCoinType();
        const x = 10 + Math.random() * 80; // 10-90% of width

        const coinEl = document.createElement('div');
        coinEl.className = 'falling-coin';
        coinEl.dataset.id = id;
        coinEl.dataset.value = coinType.value;
        coinEl.textContent = coinType.emoji;
        coinEl.style.left = `${x}%`;
        coinEl.style.top = '-40px';

        coinsRef.current.push({
          id,
          element: coinEl,
          x,
          y: -40,
          value: coinType.value,
        });

        container.appendChild(coinEl);
      }

      // Update coins
      const basketLeft = basketXRef.current - (BASKET_WIDTH / gameRect.width * 100) / 2;
      const basketRight = basketXRef.current + (BASKET_WIDTH / gameRect.width * 100) / 2;

      coinsRef.current = coinsRef.current.filter(coin => {
        coin.y += COIN_FALL_SPEED;
        coin.element.style.top = `${coin.y}px`;

        // Check if caught
        if (coin.y >= basketY - 20 && coin.y <= basketY + 30) {
          if (coin.x >= basketLeft && coin.x <= basketRight) {
            // Caught!
            scoreRef.current += coin.value;
            caughtRef.current += 1;

            // Update UI periodically
            if (caughtRef.current % 2 === 0) {
              setScore(scoreRef.current);
              setCaught(caughtRef.current);
            }

            // Show catch effect
            coin.element.classList.add('caught');
            setTimeout(() => coin.element.remove(), 200);
            return false;
          }
        }

        // Remove if off screen
        if (coin.y > gameRect.height + 50) {
          coin.element.remove();
          return false;
        }

        return true;
      });

      rafRef.current = requestAnimationFrame(gameLoop);
    };

    rafRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (rafRef.current) cancelAnimationFrame(rafRef.current);
    };
  }, [gameState]);

  // Touch/mouse handlers for basket
  const handleTouchStart = useCallback((e) => {
    if (gameState !== 'playing') return;
    e.preventDefault();
    isDraggingRef.current = true;
    const touch = e.touches?.[0] || e;
    touchStartXRef.current = touch.clientX;
    basketStartXRef.current = basketXRef.current;
  }, [gameState]);

  const handleTouchMove = useCallback((e) => {
    if (!isDraggingRef.current || gameState !== 'playing') return;
    e.preventDefault();

    const touch = e.touches?.[0] || e;
    const gameArea = gameAreaRef.current;
    if (!gameArea) return;

    const rect = gameArea.getBoundingClientRect();
    const deltaX = touch.clientX - touchStartXRef.current;
    const deltaPercent = (deltaX / rect.width) * 100;

    let newX = basketStartXRef.current + deltaPercent;
    newX = Math.max(10, Math.min(90, newX));

    setBasketX(newX);
  }, [gameState]);

  const handleTouchEnd = useCallback(() => {
    isDraggingRef.current = false;
  }, []);

  // Direct touch on game area for instant positioning
  const handleAreaTouch = useCallback((e) => {
    if (gameState !== 'playing') return;

    const touch = e.touches?.[0] || e;
    const gameArea = gameAreaRef.current;
    if (!gameArea) return;

    const rect = gameArea.getBoundingClientRect();
    let newX = ((touch.clientX - rect.left) / rect.width) * 100;
    newX = Math.max(10, Math.min(90, newX));

    setBasketX(newX);
    isDraggingRef.current = true;
    touchStartXRef.current = touch.clientX;
    basketStartXRef.current = newX;
  }, [gameState]);

  // Cleanup
  useEffect(() => {
    if (gameState === 'ended') {
      coinsRef.current.forEach(coin => coin.element?.remove());
      coinsRef.current = [];
      setScore(scoreRef.current);
      setCaught(caughtRef.current);
    }
  }, [gameState]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="cascade-game-overlay">
      <div className="cascade-game-container">
        {/* Header */}
        <div className="game-header">
          <div className="game-stat">
            <span className="stat-icon">🪙</span>
            <span className="stat-value">{score}</span>
          </div>
          <div className="game-timer">
            <span className="timer-icon">⏱️</span>
            <span className={`timer-value ${timeLeft <= 5 ? 'warning' : ''}`}>
              {timeLeft}s
            </span>
          </div>
          <div className="game-stat">
            <span className="stat-icon">✨</span>
            <span className="stat-value">{caught}</span>
          </div>
        </div>

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="countdown-display" key={countdown}>
            {countdown > 0 ? countdown : 'GO!'}
          </div>
        )}

        {/* Game Area */}
        {gameState === 'playing' && (
          <div
            className="cascade-game-area"
            ref={gameAreaRef}
            onTouchStart={handleAreaTouch}
            onTouchMove={handleTouchMove}
            onTouchEnd={handleTouchEnd}
            onMouseDown={handleAreaTouch}
            onMouseMove={(e) => isDraggingRef.current && handleTouchMove(e)}
            onMouseUp={handleTouchEnd}
            onMouseLeave={handleTouchEnd}
          >
            {/* Coins container */}
            <div className="coins-container" ref={coinsContainerRef} />

            {/* Basket */}
            <div
              className="catch-basket"
              style={{ left: `${basketX}%` }}
            >
              <span className="basket-emoji">🧺</span>
            </div>

            {/* Guide line */}
            <div className="basket-guide" style={{ left: `${basketX}%` }} />
          </div>
        )}

        {/* Game Over Screen */}
        {gameState === 'ended' && (
          <div className="game-over-screen">
            <div className="game-over-content">
              <h2 className="game-over-title">
                <span>💰</span> Great Catch! <span>💰</span>
              </h2>

              <div className="final-stats">
                <div className="final-stat">
                  <span className="final-stat-icon">✨</span>
                  <span className="final-stat-label">Items Caught</span>
                  <span className="final-stat-value">{caught}</span>
                </div>

                <div className="final-stat highlight">
                  <span className="final-stat-icon">🪙</span>
                  <span className="final-stat-label">Coins Earned</span>
                  <span className="final-stat-value coins">{score}</span>
                </div>
              </div>

              {caught >= 25 && (
                <div className="achievement-badge">
                  <span>🧺</span> Master Catcher! <span>🧺</span>
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
            <p>Drag to move the basket and catch coins!</p>
            <p className="coins-hint">💎 Gems and ⭐ Stars are worth more!</p>
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

export default CoinCascadeGame;
