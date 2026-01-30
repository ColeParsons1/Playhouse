import { useState, useEffect, useCallback, useRef } from 'react';
import './BubbleFloatGame.css';

const GAME_DURATION = 20000;
const BUBBLE_SPAWN_INTERVAL = 350;
const BUBBLE_FLOAT_SPEED_MIN = 3000;
const BUBBLE_FLOAT_SPEED_MAX = 5000;
const COINS_PER_POP = 2;
const COMBO_THRESHOLD = 5;
const COMBO_BONUS = 1;

const BUBBLE_TYPES = [
  { emoji: '🫧', size: 50, coins: 2 },
  { emoji: '💖', size: 45, coins: 3 },
  { emoji: '⭐', size: 40, coins: 4 },
  { emoji: '💎', size: 35, coins: 5 },
];

const BubbleFloatGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [score, setScore] = useState(0);
  const [combo, setCombo] = useState(0);
  const [maxCombo, setMaxCombo] = useState(0);
  const [bubblesPoppedCount, setBubblesPoppedCount] = useState(0);

  const scoreRef = useRef(0);
  const comboRef = useRef(0);
  const maxComboRef = useRef(0);
  const bubblesPoppedRef = useRef(0);
  const bubblesRef = useRef(new Map());
  const bubbleIdRef = useRef(0);
  const gameAreaRef = useRef(null);
  const bubblesContainerRef = useRef(null);
  const gameStateRef = useRef('countdown');
  const spawnIntervalRef = useRef(null);

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
          setBubblesPoppedCount(bubblesPoppedRef.current);
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
    return () => clearInterval(timer);
  }, [gameState]);

  // Spawn bubbles
  const spawnBubble = useCallback(() => {
    if (gameStateRef.current !== 'playing') return;

    const container = bubblesContainerRef.current;
    const gameArea = gameAreaRef.current;
    if (!container || !gameArea) return;

    const id = bubbleIdRef.current++;

    // Weighted random selection - smaller bubbles are rarer
    const rand = Math.random();
    let bubbleType;
    if (rand < 0.5) bubbleType = BUBBLE_TYPES[0];      // 50% - normal
    else if (rand < 0.75) bubbleType = BUBBLE_TYPES[1]; // 25% - hearts
    else if (rand < 0.9) bubbleType = BUBBLE_TYPES[2];  // 15% - stars
    else bubbleType = BUBBLE_TYPES[3];                  // 10% - diamonds

    const floatDuration = BUBBLE_FLOAT_SPEED_MIN + Math.random() * (BUBBLE_FLOAT_SPEED_MAX - BUBBLE_FLOAT_SPEED_MIN);
    const x = 10 + Math.random() * 80;
    const wobble = Math.random() * 30 - 15; // Random wobble direction

    const bubbleEl = document.createElement('div');
    bubbleEl.className = 'float-bubble';
    bubbleEl.dataset.id = id;
    bubbleEl.dataset.coins = bubbleType.coins;
    bubbleEl.textContent = bubbleType.emoji;
    bubbleEl.style.left = `${x}%`;
    bubbleEl.style.setProperty('--size', `${bubbleType.size}px`);
    bubbleEl.style.setProperty('--wobble', `${wobble}px`);
    bubbleEl.style.animationDuration = `${floatDuration}ms`;

    bubblesRef.current.set(id, {
      element: bubbleEl,
      coins: bubbleType.coins,
      timeout: setTimeout(() => {
        // Bubble escaped - no penalty, just remove
        if (bubblesRef.current.has(id)) {
          bubblesRef.current.delete(id);
          bubbleEl.remove();
        }
      }, floatDuration),
    });

    container.appendChild(bubbleEl);
  }, []);

  // Start spawning when playing
  useEffect(() => {
    if (gameState !== 'playing') return;

    spawnIntervalRef.current = setInterval(spawnBubble, BUBBLE_SPAWN_INTERVAL);

    return () => {
      if (spawnIntervalRef.current) {
        clearInterval(spawnIntervalRef.current);
      }
    };
  }, [gameState, spawnBubble]);

  // Handle bubble pop
  const handleBubblePop = useCallback((e) => {
    if (gameStateRef.current !== 'playing') return;

    const touch = e.touches?.[0] || e.changedTouches?.[0] || e;
    const target = document.elementFromPoint(touch.clientX, touch.clientY);

    if (!target || !target.classList.contains('float-bubble')) return;

    e.preventDefault();
    e.stopPropagation();

    const id = parseInt(target.dataset.id, 10);
    const bubbleCoins = parseInt(target.dataset.coins, 10) || COINS_PER_POP;

    if (isNaN(id) || !bubblesRef.current.has(id)) return;

    const bubble = bubblesRef.current.get(id);
    clearTimeout(bubble.timeout);
    bubblesRef.current.delete(id);

    // Update combo
    comboRef.current += 1;
    if (comboRef.current > maxComboRef.current) {
      maxComboRef.current = comboRef.current;
    }

    // Calculate score
    const comboBonus = comboRef.current >= COMBO_THRESHOLD
      ? Math.floor((comboRef.current - COMBO_THRESHOLD + 1) * COMBO_BONUS)
      : 0;
    const earned = bubbleCoins + comboBonus;
    scoreRef.current += earned;
    bubblesPoppedRef.current += 1;

    // Update UI
    setScore(scoreRef.current);
    setCombo(comboRef.current);
    setMaxCombo(maxComboRef.current);

    // Animate pop
    target.classList.add('popped');
    target.innerHTML = `<span class="bubble-score">+${earned}</span>`;
    setTimeout(() => target.remove(), 400);
  }, []);

  // Cleanup
  useEffect(() => {
    if (gameState === 'ended') {
      bubblesRef.current.forEach(bubble => {
        clearTimeout(bubble.timeout);
        bubble.element?.remove();
      });
      bubblesRef.current.clear();

      if (spawnIntervalRef.current) {
        clearInterval(spawnIntervalRef.current);
      }
    }
  }, [gameState]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="bubble-float-overlay">
      <div className="bubble-float-container">
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
            className="bubble-game-area"
            ref={gameAreaRef}
            onTouchStart={handleBubblePop}
            onClick={handleBubblePop}
          >
            <div className="bubbles-container" ref={bubblesContainerRef} />
          </div>
        )}

        {/* Game Over Screen */}
        {gameState === 'ended' && (
          <div className="game-over-screen">
            <div className="game-over-content">
              <h2 className="game-over-title">
                <span>🫧</span> Bubble Pop! <span>🫧</span>
              </h2>

              <div className="final-stats">
                <div className="final-stat">
                  <span className="final-stat-icon">🫧</span>
                  <span className="final-stat-label">Bubbles Popped</span>
                  <span className="final-stat-value">{bubblesPoppedCount}</span>
                </div>

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

              {bubblesPoppedCount >= 40 && (
                <div className="achievement-badge">
                  <span>🌊</span> Bubble Master! <span>🌊</span>
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
            <p>Pop the bubbles before they float away!</p>
            <p className="coins-hint">💎 Diamonds = more coins!</p>
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

export default BubbleFloatGame;
