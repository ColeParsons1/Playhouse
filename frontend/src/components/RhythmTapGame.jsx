import { useState, useEffect, useCallback, useRef } from 'react';
import './RhythmTapGame.css';

const GAME_DURATION = 20000;
const BEAT_SPAWN_INTERVAL = 600;
const BEAT_TRAVEL_TIME = 2000;
const HIT_ZONE_TOLERANCE = 80; // pixels from center
const COINS_PER_PERFECT = 5;
const COINS_PER_GOOD = 3;
const COINS_PER_OK = 1;
const COMBO_THRESHOLD = 5;
const COMBO_BONUS = 1;

const BEAT_COLORS = [
  { color: '#FF69B4', glow: 'rgba(255, 105, 180, 0.6)' },
  { color: '#00CED1', glow: 'rgba(0, 206, 209, 0.6)' },
  { color: '#FFD700', glow: 'rgba(255, 215, 0, 0.6)' },
  { color: '#9370DB', glow: 'rgba(147, 112, 219, 0.6)' },
];

const RhythmTapGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [score, setScore] = useState(0);
  const [combo, setCombo] = useState(0);
  const [maxCombo, setMaxCombo] = useState(0);
  const [lastHit, setLastHit] = useState(null);

  const scoreRef = useRef(0);
  const comboRef = useRef(0);
  const maxComboRef = useRef(0);
  const beatsRef = useRef(new Map());
  const beatIdRef = useRef(0);
  const gameAreaRef = useRef(null);
  const beatsContainerRef = useRef(null);
  const hitZoneRef = useRef(null);
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
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
    return () => clearInterval(timer);
  }, [gameState]);

  // Spawn beats
  const spawnBeat = useCallback(() => {
    if (gameStateRef.current !== 'playing') return;

    const container = beatsContainerRef.current;
    if (!container) return;

    const id = beatIdRef.current++;
    const colorData = BEAT_COLORS[Math.floor(Math.random() * BEAT_COLORS.length)];
    const lane = Math.floor(Math.random() * 3); // 3 lanes

    const beatEl = document.createElement('div');
    beatEl.className = 'rhythm-beat';
    beatEl.dataset.id = id;
    beatEl.dataset.lane = lane;
    beatEl.style.setProperty('--beat-color', colorData.color);
    beatEl.style.setProperty('--beat-glow', colorData.glow);
    beatEl.style.setProperty('--lane', lane);
    beatEl.style.animationDuration = `${BEAT_TRAVEL_TIME}ms`;

    beatsRef.current.set(id, {
      element: beatEl,
      lane,
      startTime: Date.now(),
      timeout: setTimeout(() => {
        // Beat passed without being hit - miss!
        if (beatsRef.current.has(id)) {
          beatsRef.current.delete(id);
          beatEl.classList.add('missed');
          setTimeout(() => beatEl.remove(), 300);

          // Reset combo
          comboRef.current = 0;
          setCombo(0);
          setLastHit({ type: 'miss', lane });
          setTimeout(() => setLastHit(null), 500);
        }
      }, BEAT_TRAVEL_TIME + 200),
    });

    container.appendChild(beatEl);
  }, []);

  // Start spawning when playing
  useEffect(() => {
    if (gameState !== 'playing') return;

    spawnIntervalRef.current = setInterval(spawnBeat, BEAT_SPAWN_INTERVAL);

    return () => {
      if (spawnIntervalRef.current) {
        clearInterval(spawnIntervalRef.current);
      }
    };
  }, [gameState, spawnBeat]);

  // Handle hit zone tap
  const handleHitZoneTap = useCallback((e) => {
    if (gameStateRef.current !== 'playing') return;

    e.preventDefault();
    e.stopPropagation();

    const hitZone = hitZoneRef.current;
    if (!hitZone) return;

    const hitZoneRect = hitZone.getBoundingClientRect();
    const hitZoneCenter = hitZoneRect.top + hitZoneRect.height / 2;

    // Find the closest beat that's near the hit zone
    let closestBeat = null;
    let closestDistance = Infinity;
    let closestId = null;

    beatsRef.current.forEach((beat, id) => {
      const beatRect = beat.element.getBoundingClientRect();
      const beatCenter = beatRect.top + beatRect.height / 2;
      const distance = Math.abs(beatCenter - hitZoneCenter);

      if (distance < closestDistance && distance < HIT_ZONE_TOLERANCE * 2) {
        closestDistance = distance;
        closestBeat = beat;
        closestId = id;
      }
    });

    if (!closestBeat || closestId === null) {
      // Missed - no beat nearby
      comboRef.current = 0;
      setCombo(0);
      setLastHit({ type: 'miss', lane: -1 });
      setTimeout(() => setLastHit(null), 500);
      return;
    }

    clearTimeout(closestBeat.timeout);
    beatsRef.current.delete(closestId);

    // Determine hit quality
    let hitType, earned;
    if (closestDistance < HIT_ZONE_TOLERANCE * 0.3) {
      hitType = 'perfect';
      earned = COINS_PER_PERFECT;
    } else if (closestDistance < HIT_ZONE_TOLERANCE * 0.7) {
      hitType = 'good';
      earned = COINS_PER_GOOD;
    } else {
      hitType = 'ok';
      earned = COINS_PER_OK;
    }

    // Update combo
    comboRef.current += 1;
    if (comboRef.current > maxComboRef.current) {
      maxComboRef.current = comboRef.current;
    }

    // Apply combo bonus
    const comboBonus = comboRef.current >= COMBO_THRESHOLD
      ? Math.floor((comboRef.current - COMBO_THRESHOLD + 1) * COMBO_BONUS)
      : 0;
    earned += comboBonus;
    scoreRef.current += earned;

    // Update UI
    setScore(scoreRef.current);
    setCombo(comboRef.current);
    setMaxCombo(maxComboRef.current);
    setLastHit({ type: hitType, lane: closestBeat.lane, earned });
    setTimeout(() => setLastHit(null), 500);

    // Animate hit
    closestBeat.element.classList.add('hit', hitType);
    closestBeat.element.innerHTML = `<span class="beat-score">+${earned}</span>`;
    setTimeout(() => closestBeat.element.remove(), 400);
  }, []);

  // Cleanup
  useEffect(() => {
    if (gameState === 'ended') {
      beatsRef.current.forEach(beat => {
        clearTimeout(beat.timeout);
        beat.element?.remove();
      });
      beatsRef.current.clear();

      if (spawnIntervalRef.current) {
        clearInterval(spawnIntervalRef.current);
      }
    }
  }, [gameState]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="rhythm-game-overlay">
      <div className="rhythm-game-container">
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
          <div className="rhythm-game-area" ref={gameAreaRef}>
            {/* Beats container */}
            <div className="beats-container" ref={beatsContainerRef} />

            {/* Hit Zone */}
            <div
              className="hit-zone"
              ref={hitZoneRef}
              onTouchStart={handleHitZoneTap}
              onClick={handleHitZoneTap}
            >
              <div className="hit-zone-glow" />
              <div className="hit-zone-line" />
              {lastHit && (
                <div className={`hit-feedback ${lastHit.type}`}>
                  {lastHit.type === 'perfect' && '✨ PERFECT!'}
                  {lastHit.type === 'good' && '👍 GOOD!'}
                  {lastHit.type === 'ok' && '👌 OK'}
                  {lastHit.type === 'miss' && '💨 MISS'}
                </div>
              )}
            </div>

            {/* Lane guides */}
            <div className="lane-guides">
              <div className="lane-guide" />
              <div className="lane-guide" />
              <div className="lane-guide" />
            </div>
          </div>
        )}

        {/* Game Over Screen */}
        {gameState === 'ended' && (
          <div className="game-over-screen">
            <div className="game-over-content">
              <h2 className="game-over-title">
                <span>🎵</span> Rhythmic! <span>🎵</span>
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

              {maxCombo >= 15 && (
                <div className="achievement-badge">
                  <span>🎶</span> Rhythm Master! <span>🎶</span>
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
            <p>Tap when beats reach the line!</p>
            <p className="coins-hint">Perfect timing = more coins 🎯</p>
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

export default RhythmTapGame;
