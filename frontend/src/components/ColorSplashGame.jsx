import { useState, useEffect, useCallback, useRef } from 'react';
import './ColorSplashGame.css';

const GAME_DURATION = 20000;
const COLOR_CHANGE_INTERVAL = 3000;
const ORB_SPAWN_INTERVAL = 400;
const ORB_LIFETIME = 3000;
const COINS_PER_CORRECT = 3;
const WRONG_TAP_PENALTY = 2;
const COMBO_THRESHOLD = 5;
const COMBO_BONUS = 1;

const COLORS = [
  { name: 'PINK', color: '#FF69B4', glow: 'rgba(255, 105, 180, 0.6)' },
  { name: 'BLUE', color: '#4169E1', glow: 'rgba(65, 105, 225, 0.6)' },
  { name: 'GOLD', color: '#FFD700', glow: 'rgba(255, 215, 0, 0.6)' },
  { name: 'PURPLE', color: '#9370DB', glow: 'rgba(147, 112, 219, 0.6)' },
];

const ColorSplashGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [score, setScore] = useState(0);
  const [combo, setCombo] = useState(0);
  const [maxCombo, setMaxCombo] = useState(0);
  const [targetColor, setTargetColor] = useState(COLORS[0]);
  const [colorFlash, setColorFlash] = useState(false);

  const scoreRef = useRef(0);
  const comboRef = useRef(0);
  const maxComboRef = useRef(0);
  const targetColorRef = useRef(COLORS[0]);
  const orbsRef = useRef(new Map());
  const orbIdRef = useRef(0);
  const gameAreaRef = useRef(null);
  const orbsContainerRef = useRef(null);
  const gameStateRef = useRef('countdown');
  const spawnIntervalRef = useRef(null);
  const colorIntervalRef = useRef(null);

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

  // Change target color periodically
  useEffect(() => {
    if (gameState !== 'playing') return;

    const changeColor = () => {
      const newColor = COLORS[Math.floor(Math.random() * COLORS.length)];
      targetColorRef.current = newColor;
      setTargetColor(newColor);
      setColorFlash(true);
      setTimeout(() => setColorFlash(false), 500);
    };

    // Initial color
    changeColor();

    colorIntervalRef.current = setInterval(changeColor, COLOR_CHANGE_INTERVAL);

    return () => {
      if (colorIntervalRef.current) {
        clearInterval(colorIntervalRef.current);
      }
    };
  }, [gameState]);

  // Spawn orbs
  const spawnOrb = useCallback(() => {
    if (gameStateRef.current !== 'playing') return;

    const container = orbsContainerRef.current;
    const gameArea = gameAreaRef.current;
    if (!container || !gameArea) return;

    const id = orbIdRef.current++;
    const colorData = COLORS[Math.floor(Math.random() * COLORS.length)];

    // Random position
    const x = 10 + Math.random() * 80;
    const y = 10 + Math.random() * 70;

    const orbEl = document.createElement('div');
    orbEl.className = 'color-orb';
    orbEl.dataset.id = id;
    orbEl.dataset.color = colorData.name;
    orbEl.style.left = `${x}%`;
    orbEl.style.top = `${y}%`;
    orbEl.style.setProperty('--orb-color', colorData.color);
    orbEl.style.setProperty('--orb-glow', colorData.glow);
    orbEl.style.animationDuration = `${ORB_LIFETIME}ms`;

    orbsRef.current.set(id, {
      element: orbEl,
      color: colorData.name,
      timeout: setTimeout(() => {
        if (orbsRef.current.has(id)) {
          orbsRef.current.delete(id);
          orbEl.classList.add('faded');
          setTimeout(() => orbEl.remove(), 300);
        }
      }, ORB_LIFETIME),
    });

    container.appendChild(orbEl);
  }, []);

  // Start spawning when playing
  useEffect(() => {
    if (gameState !== 'playing') return;

    spawnIntervalRef.current = setInterval(spawnOrb, ORB_SPAWN_INTERVAL);

    return () => {
      if (spawnIntervalRef.current) {
        clearInterval(spawnIntervalRef.current);
      }
    };
  }, [gameState, spawnOrb]);

  // Handle orb tap
  const handleOrbTap = useCallback((e) => {
    if (gameStateRef.current !== 'playing') return;

    const touch = e.touches?.[0] || e.changedTouches?.[0] || e;
    const target = document.elementFromPoint(touch.clientX, touch.clientY);

    if (!target || !target.classList.contains('color-orb')) return;

    e.preventDefault();
    e.stopPropagation();

    const id = parseInt(target.dataset.id, 10);
    const orbColor = target.dataset.color;

    if (isNaN(id) || !orbsRef.current.has(id)) return;

    const orb = orbsRef.current.get(id);
    clearTimeout(orb.timeout);
    orbsRef.current.delete(id);

    const isCorrect = orbColor === targetColorRef.current.name;

    if (isCorrect) {
      // Correct tap!
      comboRef.current += 1;
      if (comboRef.current > maxComboRef.current) {
        maxComboRef.current = comboRef.current;
      }

      const comboBonus = comboRef.current >= COMBO_THRESHOLD
        ? Math.floor((comboRef.current - COMBO_THRESHOLD + 1) * COMBO_BONUS)
        : 0;
      const earned = COINS_PER_CORRECT + comboBonus;
      scoreRef.current += earned;

      setScore(scoreRef.current);
      setCombo(comboRef.current);
      setMaxCombo(maxComboRef.current);

      target.classList.add('correct');
      target.innerHTML = `<span class="orb-score">+${earned}</span>`;
    } else {
      // Wrong tap - penalty
      comboRef.current = 0;
      scoreRef.current = Math.max(0, scoreRef.current - WRONG_TAP_PENALTY);

      setScore(scoreRef.current);
      setCombo(0);

      target.classList.add('wrong');
      target.innerHTML = `<span class="orb-penalty">-${WRONG_TAP_PENALTY}</span>`;
    }

    setTimeout(() => target.remove(), 400);
  }, []);

  // Cleanup
  useEffect(() => {
    if (gameState === 'ended') {
      orbsRef.current.forEach(orb => {
        clearTimeout(orb.timeout);
        orb.element?.remove();
      });
      orbsRef.current.clear();

      if (spawnIntervalRef.current) {
        clearInterval(spawnIntervalRef.current);
      }
      if (colorIntervalRef.current) {
        clearInterval(colorIntervalRef.current);
      }
    }
  }, [gameState]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="color-game-overlay">
      <div className="color-game-container">
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

        {/* Target Color Display */}
        {gameState === 'playing' && (
          <div
            className={`target-color-display ${colorFlash ? 'flash' : ''}`}
            style={{ '--target-color': targetColor.color }}
          >
            <span className="target-label">TAP</span>
            <span className="target-name">{targetColor.name}</span>
          </div>
        )}

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="countdown-display" key={countdown}>
            {countdown > 0 ? countdown : 'GO!'}
          </div>
        )}

        {/* Game Area */}
        {gameState === 'playing' && (
          <div
            className="color-game-area"
            ref={gameAreaRef}
            onTouchStart={handleOrbTap}
            onClick={handleOrbTap}
          >
            <div className="orbs-container" ref={orbsContainerRef} />
          </div>
        )}

        {/* Game Over Screen */}
        {gameState === 'ended' && (
          <div className="game-over-screen">
            <div className="game-over-content">
              <h2 className="game-over-title">
                <span>🎨</span> Colorful! <span>🎨</span>
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
                  <span>🎯</span> Color Master! <span>🎯</span>
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
            <p>Tap ONLY the matching color!</p>
            <p className="coins-hint">Wrong taps lose coins 😬</p>
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

export default ColorSplashGame;
