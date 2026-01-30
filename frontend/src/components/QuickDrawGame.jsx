import { useState, useEffect, useCallback, useRef } from 'react';
import './QuickDrawGame.css';

const NUM_ROUNDS = 10;
const MIN_WAIT_TIME = 1000;
const MAX_WAIT_TIME = 3500;
const MAX_REACTION_TIME = 1500;
const PERFECT_TIME = 200;
const GOOD_TIME = 400;
const COINS_PERFECT = 10;
const COINS_GOOD = 6;
const COINS_OK = 3;
const FALSE_START_PENALTY = 5;

const TARGETS = ['🎯', '⭐', '💎', '🌟', '💖'];

const QuickDrawGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [round, setRound] = useState(0);
  const [score, setScore] = useState(0);
  const [roundState, setRoundState] = useState('waiting'); // waiting, ready, react, result
  const [reactionTime, setReactionTime] = useState(null);
  const [currentTarget, setCurrentTarget] = useState('🎯');
  const [resultType, setResultType] = useState(null);
  const [bestTime, setBestTime] = useState(null);
  const [perfectCount, setPerfectCount] = useState(0);

  const scoreRef = useRef(0);
  const bestTimeRef = useRef(null);
  const perfectCountRef = useRef(0);
  const roundRef = useRef(0);
  const roundStateRef = useRef('waiting');
  const targetAppearTimeRef = useRef(null);
  const waitTimeoutRef = useRef(null);
  const resultTimeoutRef = useRef(null);
  const gameStateRef = useRef('countdown');

  useEffect(() => {
    gameStateRef.current = gameState;
  }, [gameState]);

  useEffect(() => {
    roundStateRef.current = roundState;
  }, [roundState]);

  // Countdown
  useEffect(() => {
    if (gameState !== 'countdown') return;
    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
      startRound();
    }
  }, [countdown, gameState]);

  const startRound = useCallback(() => {
    if (roundRef.current >= NUM_ROUNDS) {
      setGameState('ended');
      setScore(scoreRef.current);
      setBestTime(bestTimeRef.current);
      setPerfectCount(perfectCountRef.current);
      return;
    }

    roundRef.current += 1;
    setRound(roundRef.current);
    setRoundState('waiting');
    setReactionTime(null);
    setResultType(null);
    setCurrentTarget(TARGETS[Math.floor(Math.random() * TARGETS.length)]);

    // Random wait time before showing target
    const waitTime = MIN_WAIT_TIME + Math.random() * (MAX_WAIT_TIME - MIN_WAIT_TIME);
    waitTimeoutRef.current = setTimeout(() => {
      if (gameStateRef.current === 'playing') {
        targetAppearTimeRef.current = Date.now();
        setRoundState('react');

        // Auto-miss if they don't react in time
        resultTimeoutRef.current = setTimeout(() => {
          if (roundStateRef.current === 'react') {
            handleMiss();
          }
        }, MAX_REACTION_TIME);
      }
    }, waitTime);
  }, []);

  const handleTap = useCallback(() => {
    if (gameStateRef.current !== 'playing') return;

    if (roundStateRef.current === 'waiting') {
      // False start!
      clearTimeout(waitTimeoutRef.current);
      scoreRef.current = Math.max(0, scoreRef.current - FALSE_START_PENALTY);
      setScore(scoreRef.current);
      setResultType('false_start');
      setRoundState('result');

      resultTimeoutRef.current = setTimeout(() => {
        startRound();
      }, 1500);
      return;
    }

    if (roundStateRef.current === 'react') {
      // Calculate reaction time
      clearTimeout(resultTimeoutRef.current);
      const reaction = Date.now() - targetAppearTimeRef.current;
      setReactionTime(reaction);

      // Update best time
      if (bestTimeRef.current === null || reaction < bestTimeRef.current) {
        bestTimeRef.current = reaction;
      }

      // Calculate score
      let earned = 0;
      let type = 'ok';
      if (reaction <= PERFECT_TIME) {
        earned = COINS_PERFECT;
        type = 'perfect';
        perfectCountRef.current += 1;
      } else if (reaction <= GOOD_TIME) {
        earned = COINS_GOOD;
        type = 'good';
      } else {
        earned = COINS_OK;
        type = 'ok';
      }

      scoreRef.current += earned;
      setScore(scoreRef.current);
      setResultType(type);
      setRoundState('result');

      resultTimeoutRef.current = setTimeout(() => {
        startRound();
      }, 1500);
    }
  }, [startRound]);

  const handleMiss = useCallback(() => {
    setResultType('miss');
    setRoundState('result');
    setReactionTime(MAX_REACTION_TIME);

    resultTimeoutRef.current = setTimeout(() => {
      startRound();
    }, 1500);
  }, [startRound]);

  // Cleanup
  useEffect(() => {
    return () => {
      if (waitTimeoutRef.current) clearTimeout(waitTimeoutRef.current);
      if (resultTimeoutRef.current) clearTimeout(resultTimeoutRef.current);
    };
  }, []);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="quickdraw-overlay">
      <div className="quickdraw-container">
        {/* Header */}
        <div className="game-header">
          <div className="game-stat">
            <span className="stat-icon">🪙</span>
            <span className="stat-value">{score}</span>
          </div>
          <div className="round-indicator">
            <span className="round-text">{round}/{NUM_ROUNDS}</span>
          </div>
          <div className="game-stat">
            <span className="stat-icon">⚡</span>
            <span className="stat-value">{bestTime ? `${bestTime}ms` : '---'}</span>
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
            className={`quickdraw-area ${roundState}`}
            onTouchStart={handleTap}
            onClick={handleTap}
          >
            {roundState === 'waiting' && (
              <div className="wait-message">
                <span className="wait-icon">👀</span>
                <span className="wait-text">Wait for it...</span>
              </div>
            )}

            {roundState === 'react' && (
              <div className="target-area">
                <span className="target-emoji">{currentTarget}</span>
                <span className="tap-text">TAP NOW!</span>
              </div>
            )}

            {roundState === 'result' && (
              <div className={`result-display ${resultType}`}>
                {resultType === 'perfect' && (
                  <>
                    <span className="result-emoji">⚡</span>
                    <span className="result-text">PERFECT!</span>
                    <span className="result-time">{reactionTime}ms</span>
                    <span className="result-coins">+{COINS_PERFECT} 🪙</span>
                  </>
                )}
                {resultType === 'good' && (
                  <>
                    <span className="result-emoji">✨</span>
                    <span className="result-text">GOOD!</span>
                    <span className="result-time">{reactionTime}ms</span>
                    <span className="result-coins">+{COINS_GOOD} 🪙</span>
                  </>
                )}
                {resultType === 'ok' && (
                  <>
                    <span className="result-emoji">👍</span>
                    <span className="result-text">OK</span>
                    <span className="result-time">{reactionTime}ms</span>
                    <span className="result-coins">+{COINS_OK} 🪙</span>
                  </>
                )}
                {resultType === 'miss' && (
                  <>
                    <span className="result-emoji">😴</span>
                    <span className="result-text">TOO SLOW!</span>
                    <span className="result-time">&gt;{MAX_REACTION_TIME}ms</span>
                  </>
                )}
                {resultType === 'false_start' && (
                  <>
                    <span className="result-emoji">🚫</span>
                    <span className="result-text">TOO EARLY!</span>
                    <span className="result-coins">-{FALSE_START_PENALTY} 🪙</span>
                  </>
                )}
              </div>
            )}
          </div>
        )}

        {/* Game Over Screen */}
        {gameState === 'ended' && (
          <div className="game-over-screen">
            <div className="game-over-content">
              <h2 className="game-over-title">
                <span>⚡</span> Quick Draw! <span>⚡</span>
              </h2>

              <div className="final-stats">
                <div className="final-stat">
                  <span className="final-stat-icon">⚡</span>
                  <span className="final-stat-label">Best Time</span>
                  <span className="final-stat-value">{bestTime || '---'}ms</span>
                </div>

                <div className="final-stat">
                  <span className="final-stat-icon">🎯</span>
                  <span className="final-stat-label">Perfect Draws</span>
                  <span className="final-stat-value">{perfectCount}/{NUM_ROUNDS}</span>
                </div>

                <div className="final-stat highlight">
                  <span className="final-stat-icon">🪙</span>
                  <span className="final-stat-label">Coins Earned</span>
                  <span className="final-stat-value coins">{score}</span>
                </div>
              </div>

              {perfectCount >= 5 && (
                <div className="achievement-badge">
                  <span>🤠</span> Fastest Finger! <span>🤠</span>
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
            <p>Wait for the target, then tap as fast as you can!</p>
            <p className="coins-hint">Don't tap too early! ⚡</p>
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

export default QuickDrawGame;
