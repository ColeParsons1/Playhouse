import { useState, useEffect, useCallback, useRef } from 'react';
import './StarCollectorGame.css';

const GAME_DURATION = 15000;
const STAR_LIFETIME_MIN = 1200; // Minimum time star is visible
const STAR_LIFETIME_MAX = 2000; // Maximum time star is visible
const SPAWN_DELAY_MIN = 400;
const SPAWN_DELAY_MAX = 800;
const COINS_PER_STAR = 2;
const COMBO_THRESHOLD = 3;
const COMBO_BONUS = 1;

const STAR_TYPES = [
  { emoji: '⭐', color: '#FFD700', glow: 'rgba(255, 215, 0, 0.6)' },
  { emoji: '🌟', color: '#FFF8DC', glow: 'rgba(255, 248, 220, 0.6)' },
  { emoji: '✨', color: '#FFE4B5', glow: 'rgba(255, 228, 181, 0.6)' },
  { emoji: '💫', color: '#FFEFD5', glow: 'rgba(255, 239, 213, 0.6)' },
];

const StarCollectorGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [score, setScore] = useState(0);
  const [combo, setCombo] = useState(0);
  const [maxCombo, setMaxCombo] = useState(0);

  const scoreRef = useRef(0);
  const comboRef = useRef(0);
  const maxComboRef = useRef(0);
  const starsRef = useRef(new Map());
  const starIdRef = useRef(0);
  const gameAreaRef = useRef(null);
  const starsContainerRef = useRef(null);
  const gameStateRef = useRef('countdown');
  const spawnTimeoutRef = useRef(null);

  useEffect(() => {
    gameStateRef.current = gameState;
  }, [gameState]);

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
          setCombo(comboRef.current);
          setMaxCombo(maxComboRef.current);
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
    return () => clearInterval(timer);
  }, [gameState]);

  // Spawn stars
  const spawnStar = useCallback(() => {
    if (gameStateRef.current !== 'playing') return;

    const container = starsContainerRef.current;
    const gameArea = gameAreaRef.current;
    if (!container || !gameArea) return;

    const rect = gameArea.getBoundingClientRect();
    const id = starIdRef.current++;
    const starType = STAR_TYPES[Math.floor(Math.random() * STAR_TYPES.length)];
    const lifetime = STAR_LIFETIME_MIN + Math.random() * (STAR_LIFETIME_MAX - STAR_LIFETIME_MIN);

    // Random position (avoid edges)
    const x = 15 + Math.random() * 70;
    const y = 15 + Math.random() * 60;

    const starEl = document.createElement('div');
    starEl.className = 'collector-star';
    starEl.dataset.id = id;
    starEl.textContent = starType.emoji;
    starEl.style.left = `${x}%`;
    starEl.style.top = `${y}%`;
    starEl.style.setProperty('--glow-color', starType.glow);
    starEl.style.animationDuration = `${lifetime}ms`;

    starsRef.current.set(id, {
      element: starEl,
      timeout: setTimeout(() => {
        // Star expired - miss!
        if (starsRef.current.has(id)) {
          starsRef.current.delete(id);
          starEl.classList.add('missed');
          setTimeout(() => starEl.remove(), 300);

          // Reset combo
          comboRef.current = 0;
          setCombo(0);
        }
      }, lifetime),
    });

    container.appendChild(starEl);

    // Schedule next spawn
    const nextDelay = SPAWN_DELAY_MIN + Math.random() * (SPAWN_DELAY_MAX - SPAWN_DELAY_MIN);
    spawnTimeoutRef.current = setTimeout(spawnStar, nextDelay);
  }, []);

  // Start spawning when playing
  useEffect(() => {
    if (gameState !== 'playing') return;

    // Initial spawn
    spawnStar();

    return () => {
      if (spawnTimeoutRef.current) {
        clearTimeout(spawnTimeoutRef.current);
      }
    };
  }, [gameState, spawnStar]);

  // Handle star tap
  const handleStarTap = useCallback((e) => {
    if (gameState !== 'playing') return;

    const touch = e.touches?.[0] || e.changedTouches?.[0] || e;
    const target = document.elementFromPoint(touch.clientX, touch.clientY);

    if (!target || !target.classList.contains('collector-star')) return;

    e.preventDefault();
    e.stopPropagation();

    const id = parseInt(target.dataset.id, 10);
    if (isNaN(id) || !starsRef.current.has(id)) return;

    const star = starsRef.current.get(id);
    clearTimeout(star.timeout);
    starsRef.current.delete(id);

    // Update combo
    comboRef.current += 1;
    if (comboRef.current > maxComboRef.current) {
      maxComboRef.current = comboRef.current;
    }

    // Calculate score
    const comboBonus = comboRef.current >= COMBO_THRESHOLD
      ? Math.floor((comboRef.current - COMBO_THRESHOLD + 1) * COMBO_BONUS)
      : 0;
    const earned = COINS_PER_STAR + comboBonus;
    scoreRef.current += earned;

    // Update UI
    setScore(scoreRef.current);
    setCombo(comboRef.current);
    setMaxCombo(maxComboRef.current);

    // Animate catch
    target.classList.add('caught');
    target.innerHTML = `<span class="star-score">+${earned}</span>`;

    setTimeout(() => target.remove(), 400);
  }, [gameState]);

  // Cleanup
  useEffect(() => {
    if (gameState === 'ended') {
      starsRef.current.forEach(star => {
        clearTimeout(star.timeout);
        star.element?.remove();
      });
      starsRef.current.clear();

      if (spawnTimeoutRef.current) {
        clearTimeout(spawnTimeoutRef.current);
      }
    }
  }, [gameState]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="star-game-overlay">
      <div className="star-game-container">
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
          <div className={`game-stat combo-stat ${combo >= COMBO_THRESHOLD ? 'active' : ''}`}>
            <span className="stat-icon">🔥</span>
            <span className="stat-value">{combo}</span>
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
            className="star-game-area"
            ref={gameAreaRef}
            onTouchStart={handleStarTap}
            onClick={handleStarTap}
          >
            <div className="stars-container" ref={starsContainerRef} />

            {/* Combo indicator */}
            {combo >= COMBO_THRESHOLD && (
              <div className="combo-indicator">
                <span className="combo-fire">🔥</span>
                <span className="combo-text">{combo}x COMBO!</span>
              </div>
            )}
          </div>
        )}

        {/* Game Over Screen */}
        {gameState === 'ended' && (
          <div className="game-over-screen">
            <div className="game-over-content">
              <h2 className="game-over-title">
                <span>⭐</span> Stellar! <span>⭐</span>
              </h2>

              <div className="final-stats">
                <div className="final-stat">
                  <span className="final-stat-icon">🔥</span>
                  <span className="final-stat-label">Best Combo</span>
                  <span className="final-stat-value">{maxCombo}x</span>
                </div>

                <div className="final-stat highlight">
                  <span className="final-stat-icon">🪙</span>
                  <span className="final-stat-label">Coins Earned</span>
                  <span className="final-stat-value coins">{score}</span>
                </div>
              </div>

              {maxCombo >= 10 && (
                <div className="achievement-badge">
                  <span>⚡</span> Combo Master! <span>⚡</span>
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
            <p>Tap stars before they fade away!</p>
            <p className="coins-hint">Build combos for bonus coins 🔥</p>
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

export default StarCollectorGame;
