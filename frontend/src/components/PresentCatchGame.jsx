import { useState, useEffect, useCallback, useRef } from 'react';
import './PresentCatchGame.css';

const GAME_DURATION = 20000;
const SPAWN_INTERVAL = 280;
const FALL_DURATION_MIN = 1912; // 10% faster than previous
const FALL_DURATION_MAX = 3060; // 10% faster than previous
const COINS_PER_CATCH = 2; // Reduced from 5
const COMBO_THRESHOLD = 400; // ms between catches for combo (tighter window)
const MAX_COMBO_MULTIPLIER = 2; // Reduced from 4

// Package size (20% smaller than original ~45px average)
const PRESENT_SIZE = 36;

// Generate random present with coin value between 2-5 (reduced from 5-12)
const pregenRandom = () => {
  const value = 2 + Math.floor(Math.random() * 4); // Random 2-5

  return {
    emoji: '🎁',
    value,
    size: PRESENT_SIZE,
    isBad: false,
    left: 8 + Math.random() * 84,
    fallDuration: FALL_DURATION_MIN + Math.random() * (FALL_DURATION_MAX - FALL_DURATION_MIN),
    rotation: -20 + Math.random() * 40,
    wobble: Math.random() * 20 + 10,
    delay: Math.random() * 0.3,
  };
};

const PresentCatchGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [score, setScore] = useState(0);
  const [caught, setCaught] = useState(0);
  const [combo, setCombo] = useState(0);
  const [showCombo, setShowCombo] = useState(false);
  const [screenShake, setScreenShake] = useState(false);

  const scoreRef = useRef(0);
  const caughtRef = useRef(0);
  const comboRef = useRef(0);
  const lastCatchTimeRef = useRef(0);
  const presentsRef = useRef(new Map());
  const presentIdRef = useRef(0);
  const gameAreaRef = useRef(null);
  const presentsContainerRef = useRef(null);
  const gameStateRef = useRef('countdown');
  const lastSpawnRef = useRef(0);
  const rafRef = useRef(null);
  const particlesRef = useRef([]);

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
          setCaught(caughtRef.current);
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
    return () => clearInterval(timer);
  }, [gameState]);

  const createPresentElement = useCallback((id, data) => {
    const el = document.createElement('div');
    el.className = `falling-present ${data.value >= 10 ? 'rare-item' : ''}`;
    el.dataset.id = id;
    el.innerHTML = `
      <span class="present-emoji">${data.emoji}</span>
      ${data.value >= 10 ? '<span class="sparkle-effect"></span>' : ''}
    `;
    el.style.cssText = `
      left: ${data.left}%;
      font-size: ${data.size}px;
      animation-duration: ${data.fallDuration}ms;
      --rotation: ${data.rotation}deg;
      --wobble: ${data.wobble}px;
      animation-delay: ${data.delay}s;
    `;
    return el;
  }, []);

  const createParticles = useCallback((x, y, color, count = 8) => {
    const container = presentsContainerRef.current;
    if (!container) return;

    for (let i = 0; i < count; i++) {
      const particle = document.createElement('div');
      particle.className = 'catch-particle';
      const angle = (i / count) * Math.PI * 2;
      const velocity = 50 + Math.random() * 50;
      const dx = Math.cos(angle) * velocity;
      const dy = Math.sin(angle) * velocity;

      particle.style.cssText = `
        left: ${x}px;
        top: ${y}px;
        --dx: ${dx}px;
        --dy: ${dy}px;
        background: ${color};
      `;
      container.appendChild(particle);
      setTimeout(() => particle.remove(), 600);
    }
  }, []);

  const handleCatch = useCallback((id, x, y) => {
    const present = presentsRef.current.get(id);
    if (!present) return;

    presentsRef.current.delete(id);
    if (present.element && present.element.parentNode) {
      present.element.classList.add('caught');
      setTimeout(() => present.element.remove(), 150);
    }

    const now = Date.now();
    const data = present.data;

    // Calculate combo
    const timeSinceLastCatch = now - lastCatchTimeRef.current;

    if (timeSinceLastCatch < COMBO_THRESHOLD) {
      comboRef.current = Math.min(comboRef.current + 1, MAX_COMBO_MULTIPLIER);
    } else {
      comboRef.current = 1;
    }

    const multiplier = comboRef.current;
    const points = data.value * multiplier;

    scoreRef.current += points;
    caughtRef.current += 1;
    lastCatchTimeRef.current = now;

    setCombo(comboRef.current);
    if (comboRef.current >= 2) {
      setShowCombo(true);
      setTimeout(() => setShowCombo(false), 800);
    }

    // Gold particles
    const colors = ['#ffd700', '#ffcc00', '#ffaa00', '#ff9500', '#ffe066'];
    createParticles(x, y, colors[Math.floor(Math.random() * colors.length)], data.value >= 10 ? 15 : 8);

    const pop = document.createElement('div');
    pop.className = `pop-effect ${multiplier > 1 ? 'combo' : ''}`;
    pop.innerHTML = `
      <span class="pop-coins">+${points} 🪙</span>
      ${multiplier > 1 ? `<span class="combo-text">x${multiplier}</span>` : ''}
    `;
    pop.style.left = `${x}px`;
    pop.style.top = `${y}px`;
    presentsContainerRef.current?.appendChild(pop);
    setTimeout(() => pop.remove(), 500);

    if (caughtRef.current % 2 === 0 || caughtRef.current <= 3) {
      setScore(scoreRef.current);
      setCaught(caughtRef.current);
    }
  }, [createParticles]);

  const handleInteraction = useCallback((e) => {
    e.preventDefault();
    e.stopPropagation();

    const touch = e.touches?.[0] || e.changedTouches?.[0] || e;
    const clientX = touch.clientX;
    const clientY = touch.clientY;

    const target = document.elementFromPoint(clientX, clientY);
    const presentEl = target?.closest('.falling-present');
    if (!presentEl) return;

    const id = parseInt(presentEl.dataset.id, 10);
    if (isNaN(id)) return;

    const rect = presentsContainerRef.current?.getBoundingClientRect();
    const x = clientX - (rect?.left || 0);
    const y = clientY - (rect?.top || 0);

    handleCatch(id, x, y);
  }, [handleCatch]);

  // Main game loop
  useEffect(() => {
    if (gameState !== 'playing') return;

    const container = presentsContainerRef.current;
    if (!container) return;

    const gameLoop = (timestamp) => {
      if (gameStateRef.current !== 'playing') return;

      if (timestamp - lastSpawnRef.current >= SPAWN_INTERVAL) {
        lastSpawnRef.current = timestamp;

        const id = presentIdRef.current++;
        const data = pregenRandom();
        const element = createPresentElement(id, data);

        presentsRef.current.set(id, {
          element,
          data,
          createdAt: timestamp,
          fallDuration: data.fallDuration,
        });

        container.appendChild(element);

        const removeTime = data.fallDuration + 500;
        setTimeout(() => {
          const present = presentsRef.current.get(id);
          if (present) {
            presentsRef.current.delete(id);
            if (present.element && present.element.parentNode) {
              present.element.remove();
            }
          }
        }, removeTime);
      }

      rafRef.current = requestAnimationFrame(gameLoop);
    };

    rafRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (rafRef.current) {
        cancelAnimationFrame(rafRef.current);
      }
    };
  }, [gameState, createPresentElement]);

  // Cleanup
  useEffect(() => {
    if (gameState === 'ended') {
      presentsRef.current.forEach((present) => {
        if (present.element && present.element.parentNode) {
          present.element.remove();
        }
      });
      presentsRef.current.clear();
      setScore(scoreRef.current);
      setCaught(caughtRef.current);
    }
  }, [gameState]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className={`present-game-overlay ${screenShake ? 'shake' : ''}`}>
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
            <span className="stat-icon">🎁</span>
            <span className="stat-value">{caught}</span>
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
            className="presents-area"
            ref={presentsContainerRef}
            onTouchStart={handleInteraction}
            onTouchEnd={(e) => e.preventDefault()}
            onClick={handleInteraction}
          />
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
                {score >= 200 ? 'PACKED!' : score >= 100 ? 'Great Catch!' : 'Nice Try!'}
              </h2>

              <div className="final-stats">
                <div className="final-stat">
                  <div className="stat-icon-wrapper">
                    <span className="final-stat-icon">🎁</span>
                  </div>
                  <div className="stat-info">
                    <span className="final-stat-label">Packages Caught</span>
                    <span className="final-stat-value">{caught}</span>
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

              {caught >= 25 && (
                <div className="achievement-badge">
                  <span className="badge-icon">👑</span>
                  <span className="badge-text">Pack Master!</span>
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
              <p className="instruction-main">Tap packages to catch them!</p>
              <p className="instruction-sub">
                <span className="good">🎁 Presents = 5-12 coins each!</span>
              </p>
              <p className="instruction-tip">Chain catches for combo bonus!</p>
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
