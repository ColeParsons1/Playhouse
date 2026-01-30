import { useState, useEffect, useCallback, useRef } from 'react';
import './BubbleMatchGame.css';

const GAME_DURATION = 15000;
const INITIAL_TIME_LIMIT = 2000; // 2 seconds per match
const MIN_TIME_LIMIT = 800; // Minimum time limit
const TIME_DECREASE = 50; // Decrease time by 50ms each correct match
const COINS_PER_MATCH = 3;
const COMBO_BONUS = 2; // Extra coins per combo level

const ICONS = ['💖', '⭐', '🌸', '🦋', '🌈', '🍀', '🎀', '💎', '🌙', '🔮', '🍭', '🧁'];

const shuffleArray = (arr) => {
  const shuffled = [...arr];
  for (let i = shuffled.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [shuffled[i], shuffled[j]] = [shuffled[j], shuffled[i]];
  }
  return shuffled;
};

const BubbleMatchGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [score, setScore] = useState(0);
  const [combo, setCombo] = useState(0);
  const [target, setTarget] = useState(null);
  const [bubbles, setBubbles] = useState([]);
  const [matchTimeLeft, setMatchTimeLeft] = useState(100);
  const [feedback, setFeedback] = useState(null);
  const [currentTimeLimit, setCurrentTimeLimit] = useState(INITIAL_TIME_LIMIT);

  const scoreRef = useRef(0);
  const comboRef = useRef(0);
  const matchesRef = useRef(0);
  const gameStateRef = useRef('countdown');
  const timeLimitRef = useRef(INITIAL_TIME_LIMIT);
  const matchStartRef = useRef(0);
  const rafRef = useRef(null);

  useEffect(() => {
    gameStateRef.current = gameState;
  }, [gameState]);

  // Generate new round
  const generateRound = useCallback(() => {
    const availableIcons = shuffleArray(ICONS).slice(0, 3);
    const targetIcon = availableIcons[Math.floor(Math.random() * 3)];

    setTarget(targetIcon);
    setBubbles(shuffleArray(availableIcons));
    setMatchTimeLeft(100);
    matchStartRef.current = performance.now();
  }, []);

  // Countdown
  useEffect(() => {
    if (gameState !== 'countdown') return;
    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
      generateRound();
    }
  }, [countdown, gameState, generateRound]);

  // Game timer
  useEffect(() => {
    if (gameState !== 'playing') return;
    const timer = setInterval(() => {
      setTimeLeft(prev => {
        if (prev <= 1) {
          setGameState('ended');
          setScore(scoreRef.current);
          setCombo(comboRef.current);
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
    return () => clearInterval(timer);
  }, [gameState]);

  // Match timer with RAF
  useEffect(() => {
    if (gameState !== 'playing') return;

    const updateMatchTimer = () => {
      if (gameStateRef.current !== 'playing') return;

      const elapsed = performance.now() - matchStartRef.current;
      const remaining = Math.max(0, 100 - (elapsed / timeLimitRef.current) * 100);

      setMatchTimeLeft(remaining);

      if (remaining <= 0) {
        // Time's up - wrong answer
        comboRef.current = 0;
        setCombo(0);
        setFeedback({ type: 'timeout', text: "Time's up!" });
        setTimeout(() => setFeedback(null), 500);
        generateRound();
        matchStartRef.current = performance.now();
      }

      rafRef.current = requestAnimationFrame(updateMatchTimer);
    };

    rafRef.current = requestAnimationFrame(updateMatchTimer);

    return () => {
      if (rafRef.current) cancelAnimationFrame(rafRef.current);
    };
  }, [gameState, generateRound]);

  // Handle bubble tap
  const handleBubbleTap = useCallback((icon, index) => {
    if (gameState !== 'playing') return;

    if (icon === target) {
      // Correct!
      comboRef.current += 1;
      matchesRef.current += 1;
      const comboBonus = Math.floor(comboRef.current / 3) * COMBO_BONUS;
      const earned = COINS_PER_MATCH + comboBonus;
      scoreRef.current += earned;

      // Speed up the game
      timeLimitRef.current = Math.max(MIN_TIME_LIMIT, timeLimitRef.current - TIME_DECREASE);
      setCurrentTimeLimit(timeLimitRef.current);

      setScore(scoreRef.current);
      setCombo(comboRef.current);
      setFeedback({
        type: 'correct',
        text: comboRef.current >= 3 ? `+${earned} 🔥x${comboRef.current}` : `+${earned}`,
        index
      });
    } else {
      // Wrong!
      comboRef.current = 0;
      setCombo(0);
      setFeedback({ type: 'wrong', text: 'Nope!', index });
    }

    setTimeout(() => {
      setFeedback(null);
      generateRound();
    }, 300);
  }, [gameState, target, generateRound]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="bubble-game-overlay">
      <div className="bubble-game-container">
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
          <div className="game-stat combo-stat">
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
          <div className="bubble-game-area">
            {/* Match Timer Bar */}
            <div className="match-timer-container">
              <div
                className={`match-timer-bar ${matchTimeLeft < 30 ? 'urgent' : ''}`}
                style={{ width: `${matchTimeLeft}%` }}
              />
            </div>

            {/* Target */}
            <div className="target-section">
              <span className="target-label">Find:</span>
              <div className="target-bubble">
                {target}
              </div>
            </div>

            {/* Bubbles */}
            <div className="bubbles-container">
              {bubbles.map((icon, index) => (
                <button
                  key={`${icon}-${index}`}
                  className={`bubble ${feedback?.index === index ? feedback.type : ''}`}
                  onClick={() => handleBubbleTap(icon, index)}
                >
                  <span className="bubble-icon">{icon}</span>
                </button>
              ))}
            </div>

            {/* Feedback */}
            {feedback && (
              <div className={`match-feedback ${feedback.type}`}>
                {feedback.text}
              </div>
            )}
          </div>
        )}

        {/* Game Over Screen */}
        {gameState === 'ended' && (
          <div className="game-over-screen">
            <div className="game-over-content">
              <h2 className="game-over-title">
                <span>🎯</span> Great Matching! <span>🎯</span>
              </h2>

              <div className="final-stats">
                <div className="final-stat">
                  <span className="final-stat-icon">✨</span>
                  <span className="final-stat-label">Matches</span>
                  <span className="final-stat-value">{matchesRef.current}</span>
                </div>

                <div className="final-stat highlight">
                  <span className="final-stat-icon">🪙</span>
                  <span className="final-stat-label">Coins Earned</span>
                  <span className="final-stat-value coins">{score}</span>
                </div>
              </div>

              {matchesRef.current >= 15 && (
                <div className="achievement-badge">
                  <span>⚡</span> Speed Matcher! <span>⚡</span>
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
            <p>Tap the matching bubble as fast as you can!</p>
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

export default BubbleMatchGame;
