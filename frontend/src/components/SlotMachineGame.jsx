import { useState, useEffect, useCallback, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './SlotMachineGame.css';

// Reduced multipliers for profit-focused model (house edge ~15-20%)
const SYMBOLS = [
  { id: 'cherry', emoji: '🍒', multiplier: 1.5, color: '#E74C3C' },
  { id: 'lemon', emoji: '🍋', multiplier: 2, color: '#F1C40F' },
  { id: 'orange', emoji: '🍊', multiplier: 2.5, color: '#E67E22' },
  { id: 'plum', emoji: '🍇', multiplier: 3, color: '#9B59B6' },
  { id: 'bell', emoji: '🔔', multiplier: 5, color: '#FFD700' },
  { id: 'bar', emoji: '💎', multiplier: 7, color: '#3498DB' },
  { id: 'seven', emoji: '7️⃣', multiplier: 10, color: '#E74C3C' },
  { id: 'star', emoji: '⭐', multiplier: 15, color: '#FFD700' },
];

const STARTING_COINS = 50; // Reduced starting coins
const BET_OPTIONS = [5, 10, 25, 50];
const SPIN_DURATION = 2000;
const REEL_COUNT = 3;
const LUCKY_STREAK_THRESHOLD = 3;
const LUCKY_STREAK_BONUS = 1.5;

// Particle effect generator
const createParticles = (count, color) => {
  return Array.from({ length: count }, (_, i) => ({
    id: i,
    angle: (Math.PI * 2 * i) / count + Math.random() * 0.5,
    speed: 2 + Math.random() * 3,
    size: 4 + Math.random() * 6,
    color: color || '#FFD700',
    life: 1,
  }));
};

const SlotMachineGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('playing'); // playing, spinning, gameOver
  const [coins, setCoins] = useState(STARTING_COINS);
  const [bet, setBet] = useState(BET_OPTIONS[0]);
  const [reels, setReels] = useState([
    [SYMBOLS[0], SYMBOLS[1], SYMBOLS[2]],
    [SYMBOLS[0], SYMBOLS[1], SYMBOLS[2]],
    [SYMBOLS[0], SYMBOLS[1], SYMBOLS[2]],
  ]);
  const [result, setResult] = useState(null);
  const [totalWon, setTotalWon] = useState(0);
  const [spins, setSpins] = useState(0);
  const [biggestWin, setBiggestWin] = useState(0);
  const [jackpotAnimation, setJackpotAnimation] = useState(false);
  const [winStreak, setWinStreak] = useState(0);
  const [particles, setParticles] = useState([]);
  const [reelLanding, setReelLanding] = useState([false, false, false]);
  const [nearMiss, setNearMiss] = useState(false);
  const [autoSpin, setAutoSpin] = useState(false);
  const [lightFlash, setLightFlash] = useState(false);

  const totalWonRef = useRef(0);
  const biggestWinRef = useRef(0);
  const spinsRef = useRef(0);
  const winStreakRef = useRef(0);
  const spinTimeoutsRef = useRef([]);
  const autoSpinRef = useRef(false);
  const particleAnimationRef = useRef(null);

  // Animate particles
  useEffect(() => {
    if (particles.length === 0) return;

    const animate = () => {
      setParticles(prev => {
        const updated = prev.map(p => ({
          ...p,
          x: (p.x || 0) + Math.cos(p.angle) * p.speed,
          y: (p.y || 0) + Math.sin(p.angle) * p.speed + 2, // gravity
          life: p.life - 0.02,
        })).filter(p => p.life > 0);

        if (updated.length > 0) {
          particleAnimationRef.current = requestAnimationFrame(animate);
        }
        return updated;
      });
    };

    particleAnimationRef.current = requestAnimationFrame(animate);

    return () => {
      if (particleAnimationRef.current) {
        cancelAnimationFrame(particleAnimationRef.current);
      }
    };
  }, [particles.length > 0]);

  // Get random symbol with weighted probability
  const getRandomSymbol = useCallback(() => {
    // Lower multiplier symbols are more common
    const weights = SYMBOLS.map(s => Math.max(1, 25 - s.multiplier));
    const totalWeight = weights.reduce((a, b) => a + b, 0);
    let random = Math.random() * totalWeight;

    for (let i = 0; i < SYMBOLS.length; i++) {
      random -= weights[i];
      if (random <= 0) return SYMBOLS[i];
    }
    return SYMBOLS[0];
  }, []);

  // Generate reel strip
  const generateReelStrip = useCallback(() => {
    return Array(3).fill(null).map(() => getRandomSymbol());
  }, [getRandomSymbol]);

  // Calculate winnings
  const calculateWinnings = useCallback((finalReels, currentBet) => {
    const middleRow = finalReels.map(reel => reel[1]);

    // Check for 3 of a kind
    if (middleRow[0].id === middleRow[1].id && middleRow[1].id === middleRow[2].id) {
      let winAmount = currentBet * middleRow[0].multiplier;

      // Apply streak bonus
      if (winStreakRef.current >= LUCKY_STREAK_THRESHOLD) {
        winAmount = Math.floor(winAmount * LUCKY_STREAK_BONUS);
      }

      return { amount: winAmount, type: 'triple', symbol: middleRow[0] };
    }

    // Check for 2 of a kind (first two or last two)
    if (middleRow[0].id === middleRow[1].id) {
      return {
        amount: Math.floor(currentBet * middleRow[0].multiplier * 0.3),
        type: 'double',
        symbol: middleRow[0],
      };
    }
    if (middleRow[1].id === middleRow[2].id) {
      return {
        amount: Math.floor(currentBet * middleRow[1].multiplier * 0.3),
        type: 'double',
        symbol: middleRow[1],
      };
    }

    // Check for cherries (any cherry gives small win)
    const cherryCount = middleRow.filter(s => s.id === 'cherry').length;
    if (cherryCount > 0) {
      return {
        amount: Math.floor(currentBet * 0.5 * cherryCount),
        type: 'cherry',
        symbol: SYMBOLS[0],
      };
    }

    // Check for near miss (2 matching high-value symbols)
    const valuableSymbols = ['seven', 'star', 'bar', 'bell'];
    for (const symId of valuableSymbols) {
      const matches = middleRow.filter(s => s.id === symId).length;
      if (matches === 2) {
        return { amount: 0, type: 'nearMiss', symbol: middleRow.find(s => s.id === symId) };
      }
    }

    return { amount: 0, type: 'lose', symbol: null };
  }, []);

  // Trigger win effects
  const triggerWinEffects = useCallback((winData) => {
    if (winData.amount > 0) {
      // Create particles
      const particleColor = winData.symbol?.color || '#FFD700';
      setParticles(createParticles(winData.type === 'triple' ? 30 : 15, particleColor));

      // Flash lights
      setLightFlash(true);
      setTimeout(() => setLightFlash(false), 500);
    }

    if (winData.type === 'nearMiss') {
      setNearMiss(true);
      setTimeout(() => setNearMiss(false), 1500);
    }
  }, []);

  // Spin the reels
  const spin = useCallback(() => {
    if (gameState !== 'playing' || coins < bet) return;

    // Deduct bet
    setCoins(prev => prev - bet);
    setResult(null);
    setGameState('spinning');
    setNearMiss(false);
    setReelLanding([false, false, false]);
    spinsRef.current += 1;
    setSpins(spinsRef.current);

    // Generate final results
    const finalReels = Array(REEL_COUNT).fill(null).map(() => generateReelStrip());

    // Animate each reel
    const animateReel = (reelIndex, duration) => {
      const startTime = Date.now();
      const interval = 50;

      const animate = () => {
        const elapsed = Date.now() - startTime;

        if (elapsed < duration) {
          setReels(prev => {
            const newReels = [...prev];
            newReels[reelIndex] = generateReelStrip();
            return newReels;
          });

          spinTimeoutsRef.current[reelIndex] = setTimeout(animate, interval);
        } else {
          // Set final result with landing animation
          setReels(prev => {
            const newReels = [...prev];
            newReels[reelIndex] = finalReels[reelIndex];
            return newReels;
          });

          // Trigger landing effect
          setReelLanding(prev => {
            const newLanding = [...prev];
            newLanding[reelIndex] = true;
            return newLanding;
          });

          // If last reel, calculate winnings
          if (reelIndex === REEL_COUNT - 1) {
            setTimeout(() => {
              const winData = calculateWinnings(finalReels, bet);

              if (winData.amount > 0) {
                setCoins(prev => prev + winData.amount);
                totalWonRef.current += winData.amount;
                setTotalWon(totalWonRef.current);

                // Update streak
                winStreakRef.current += 1;
                setWinStreak(winStreakRef.current);

                if (winData.amount > biggestWinRef.current) {
                  biggestWinRef.current = winData.amount;
                  setBiggestWin(winData.amount);
                }

                // Check for jackpot (3 stars or 3 sevens)
                const middleRow = finalReels.map(r => r[1]);
                if (
                  (middleRow[0].id === 'star' && middleRow[1].id === 'star' && middleRow[2].id === 'star') ||
                  (middleRow[0].id === 'seven' && middleRow[1].id === 'seven' && middleRow[2].id === 'seven')
                ) {
                  setJackpotAnimation(true);
                  setTimeout(() => setJackpotAnimation(false), 3000);
                }

                setResult({ type: 'win', amount: winData.amount, winType: winData.type });
                triggerWinEffects(winData);
              } else {
                // Reset streak on loss
                winStreakRef.current = 0;
                setWinStreak(0);

                setResult({ type: 'lose', nearMiss: winData.type === 'nearMiss' });
                triggerWinEffects(winData);
              }

              setGameState('playing');

              // Auto spin if enabled
              if (autoSpinRef.current && coins >= bet) {
                setTimeout(() => {
                  if (autoSpinRef.current) spin();
                }, 1000);
              }
            }, 300);
          }
        }
      };

      animate();
    };

    // Start each reel with staggered timing
    for (let i = 0; i < REEL_COUNT; i++) {
      const duration = SPIN_DURATION + i * 400;
      animateReel(i, duration);
    }
  }, [gameState, coins, bet, generateReelStrip, calculateWinnings, triggerWinEffects]);

  // Toggle auto spin
  const toggleAutoSpin = useCallback(() => {
    autoSpinRef.current = !autoSpinRef.current;
    setAutoSpin(autoSpinRef.current);

    if (autoSpinRef.current && gameState === 'playing' && coins >= bet) {
      spin();
    }
  }, [gameState, coins, bet, spin]);

  // Change bet
  const changeBet = useCallback((newBet) => {
    if (gameState !== 'playing') return;
    setBet(newBet);
    autoSpinRef.current = false;
    setAutoSpin(false);
  }, [gameState]);

  // Cash out
  const cashOut = useCallback(() => {
    autoSpinRef.current = false;
    setAutoSpin(false);
    setGameState('gameOver');
  }, []);

  const handleFinish = useCallback(() => {
    // Return total profit (coins gained minus starting amount)
    const profit = Math.max(0, coins - STARTING_COINS + totalWonRef.current);
    onGameEnd(profit);
  }, [coins, onGameEnd]);

  // Cleanup timeouts
  useEffect(() => {
    return () => {
      spinTimeoutsRef.current.forEach(timeout => clearTimeout(timeout));
      if (particleAnimationRef.current) {
        cancelAnimationFrame(particleAnimationRef.current);
      }
    };
  }, []);

  // Render reel
  const renderReel = (reelSymbols, reelIndex) => (
    <motion.div
      key={reelIndex}
      className={`slot-reel ${gameState === 'spinning' ? 'spinning' : ''}`}
      animate={reelLanding[reelIndex] ? {
        y: [0, 5, 0],
        transition: { duration: 0.15, ease: 'easeOut' },
      } : {}}
    >
      <div className="reel-inner">
        {reelSymbols.map((symbol, rowIndex) => (
          <motion.div
            key={rowIndex}
            className={`slot-symbol ${rowIndex === 1 ? 'middle' : ''}`}
            animate={rowIndex === 1 && result?.type === 'win' ? {
              scale: [1, 1.1, 1],
              transition: { duration: 0.3, repeat: 2 },
            } : {}}
          >
            <span className="symbol-emoji">{symbol.emoji}</span>
            {rowIndex === 1 && (
              <div
                className="symbol-glow"
                style={{ background: `radial-gradient(circle, ${symbol.color}40, transparent)` }}
              />
            )}
          </motion.div>
        ))}
      </div>
      <div className="reel-shadow" />
    </motion.div>
  );

  return (
    <div className={`slot-machine-overlay ${lightFlash ? 'flash' : ''}`}>
      {/* Ambient lights */}
      <div className="ambient-lights">
        <div className="light light-1" />
        <div className="light light-2" />
        <div className="light light-3" />
      </div>

      {/* Particles */}
      <div className="particles-layer">
        {particles.map(p => (
          <div
            key={p.id}
            className="win-particle"
            style={{
              left: `calc(50% + ${p.x || 0}px)`,
              top: `calc(40% + ${p.y || 0}px)`,
              width: p.size,
              height: p.size,
              background: p.color,
              opacity: p.life,
            }}
          />
        ))}
      </div>

      <div className="slot-machine-container">
        {/* Header */}
        <div className="slot-header">
          <motion.div
            className="coins-display"
            animate={result?.type === 'win' ? {
              scale: [1, 1.1, 1],
              transition: { duration: 0.3 },
            } : {}}
          >
            <span className="coins-icon">🪙</span>
            <motion.span
              className="coins-value"
              key={coins}
              initial={{ scale: 1.2, color: '#FFD700' }}
              animate={{ scale: 1, color: '#FFD700' }}
              transition={{ duration: 0.3 }}
            >
              {coins}
            </motion.span>
          </motion.div>

          {/* Win Streak */}
          <AnimatePresence>
            {winStreak >= 2 && (
              <motion.div
                className="win-streak"
                initial={{ scale: 0, x: -20 }}
                animate={{ scale: 1, x: 0 }}
                exit={{ scale: 0, x: -20 }}
              >
                🔥 {winStreak}x Streak
                {winStreak >= LUCKY_STREAK_THRESHOLD && (
                  <span className="streak-bonus">+50%</span>
                )}
              </motion.div>
            )}
          </AnimatePresence>

          <button className="cash-out-btn" onClick={cashOut}>
            Cash Out
          </button>
        </div>

        {/* Jackpot Animation */}
        <AnimatePresence>
          {jackpotAnimation && (
            <motion.div
              className="jackpot-celebration"
              initial={{ scale: 0, rotate: -10 }}
              animate={{ scale: 1, rotate: 0 }}
              exit={{ scale: 0, opacity: 0 }}
              transition={{ type: 'spring', stiffness: 200 }}
            >
              <span>🎰</span> JACKPOT! <span>🎰</span>
              <div className="jackpot-coins">
                {Array(10).fill('🪙').map((c, i) => (
                  <motion.span
                    key={i}
                    initial={{ y: -50, opacity: 0 }}
                    animate={{ y: 100, opacity: [1, 1, 0] }}
                    transition={{ delay: i * 0.1, duration: 1 }}
                  >
                    {c}
                  </motion.span>
                ))}
              </div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Near Miss Alert */}
        <AnimatePresence>
          {nearMiss && (
            <motion.div
              className="near-miss-alert"
              initial={{ y: -20, opacity: 0 }}
              animate={{ y: 0, opacity: 1 }}
              exit={{ y: -20, opacity: 0 }}
            >
              So close! Try again!
            </motion.div>
          )}
        </AnimatePresence>

        {/* Slot Machine */}
        <div className="slot-machine">
          <div className="slot-frame">
            <div className="slot-top-lights">
              {Array(7).fill(null).map((_, i) => (
                <div
                  key={i}
                  className={`top-light ${gameState === 'spinning' ? 'animated' : ''}`}
                  style={{ animationDelay: `${i * 0.1}s` }}
                />
              ))}
            </div>

            <div className="slot-title">
              <span className="title-icon">🎰</span>
              LUCKY SLOTS
              <span className="title-icon">🎰</span>
            </div>

            {/* Reels */}
            <div className="reels-container">
              <div className="reels-frame">
                <div className="win-line" />
                <div className="reels-viewport">
                  {reels.map((reel, i) => renderReel(reel, i))}
                </div>
              </div>
              <div className="reels-reflection" />
            </div>

            {/* Result Display */}
            <div className="result-display">
              <AnimatePresence mode="wait">
                {result && (
                  <motion.div
                    key={result.type + (result.amount || '')}
                    className={`result-message ${result.type}`}
                    initial={{ scale: 0, y: 20 }}
                    animate={{ scale: 1, y: 0 }}
                    exit={{ scale: 0, y: -20 }}
                    transition={{ type: 'spring', stiffness: 300 }}
                  >
                    {result.type === 'win' ? (
                      <>
                        <span className="result-emoji">🎉</span>
                        WIN +{result.amount}
                        <span className="result-emoji">🎉</span>
                      </>
                    ) : result.nearMiss ? (
                      <>Almost! Try again!</>
                    ) : (
                      <>Try Again!</>
                    )}
                  </motion.div>
                )}
              </AnimatePresence>
            </div>
          </div>
        </div>

        {/* Controls */}
        <div className="slot-controls">
          {/* Bet Selection */}
          <div className="bet-section">
            <span className="bet-label">BET:</span>
            <div className="bet-options">
              {BET_OPTIONS.map(option => (
                <motion.button
                  key={option}
                  className={`bet-option ${bet === option ? 'active' : ''} ${coins < option ? 'disabled' : ''}`}
                  onClick={() => changeBet(option)}
                  disabled={coins < option || gameState !== 'playing'}
                  whileHover={coins >= option && gameState === 'playing' ? { scale: 1.05 } : {}}
                  whileTap={coins >= option && gameState === 'playing' ? { scale: 0.95 } : {}}
                >
                  {option}
                </motion.button>
              ))}
            </div>
          </div>

          {/* Spin Button */}
          <motion.button
            className={`spin-button ${gameState === 'spinning' ? 'spinning' : ''}`}
            onClick={spin}
            disabled={gameState !== 'playing' || coins < bet}
            whileHover={gameState === 'playing' && coins >= bet ? { scale: 1.02 } : {}}
            whileTap={gameState === 'playing' && coins >= bet ? { scale: 0.98 } : {}}
          >
            {gameState === 'spinning' ? (
              <>
                <span className="spin-icon spinning-icon">🎰</span>
                SPINNING...
              </>
            ) : (
              <>
                <span className="spin-icon">🎰</span>
                SPIN!
              </>
            )}
          </motion.button>

          {/* Control Row */}
          <div className="control-row">
            <motion.button
              className="max-bet-btn"
              onClick={() => changeBet(Math.max(...BET_OPTIONS.filter(b => b <= coins)))}
              disabled={gameState !== 'playing'}
              whileHover={gameState === 'playing' ? { scale: 1.02 } : {}}
              whileTap={gameState === 'playing' ? { scale: 0.98 } : {}}
            >
              MAX BET
            </motion.button>

            <motion.button
              className={`auto-spin-btn ${autoSpin ? 'active' : ''}`}
              onClick={toggleAutoSpin}
              disabled={gameState === 'spinning' || coins < bet}
              whileHover={{ scale: 1.02 }}
              whileTap={{ scale: 0.98 }}
            >
              {autoSpin ? '⏹ STOP AUTO' : '▶ AUTO SPIN'}
            </motion.button>
          </div>
        </div>

        {/* Pay Table */}
        <div className="pay-table">
          <div className="pay-table-title">💎 Pay Table 💎</div>
          <div className="pay-table-items">
            {SYMBOLS.slice().reverse().map(symbol => (
              <motion.div
                key={symbol.id}
                className="pay-item"
                whileHover={{ scale: 1.05, background: 'rgba(255, 255, 255, 0.1)' }}
              >
                <span className="pay-symbols">
                  {symbol.emoji}{symbol.emoji}{symbol.emoji}
                </span>
                <span className="pay-multiplier">x{symbol.multiplier}</span>
              </motion.div>
            ))}
          </div>
        </div>

        {/* Stats */}
        <div className="slot-stats">
          <div className="stat-item">
            <span className="stat-icon">🎲</span>
            <span className="stat-label">Spins</span>
            <span className="stat-value">{spins}</span>
          </div>
          <div className="stat-item">
            <span className="stat-icon">💰</span>
            <span className="stat-label">Won</span>
            <span className="stat-value">{totalWon}</span>
          </div>
          <div className="stat-item">
            <span className="stat-icon">🏆</span>
            <span className="stat-label">Best</span>
            <span className="stat-value">{biggestWin}</span>
          </div>
        </div>

        {/* No coins warning */}
        <AnimatePresence>
          {coins < Math.min(...BET_OPTIONS) && gameState === 'playing' && (
            <motion.div
              className="no-coins-warning"
              initial={{ opacity: 0, scale: 0.9 }}
              animate={{ opacity: 1, scale: 1 }}
              exit={{ opacity: 0, scale: 0.9 }}
            >
              <p>Out of coins!</p>
              <button className="cash-out-final" onClick={cashOut}>
                Finish Playing
              </button>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Game Over */}
        <AnimatePresence>
          {gameState === 'gameOver' && (
            <motion.div
              className="game-over-screen"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
            >
              <motion.div
                className="game-over-content"
                initial={{ scale: 0.8, y: 50 }}
                animate={{ scale: 1, y: 0 }}
                transition={{ type: 'spring', stiffness: 200 }}
              >
                <motion.h2
                  className="game-over-title"
                  initial={{ y: -20 }}
                  animate={{ y: 0 }}
                  transition={{ delay: 0.2 }}
                >
                  <span>🎰</span> Cashing Out <span>🎰</span>
                </motion.h2>

                <div className="final-stats">
                  <motion.div
                    className="final-stat"
                    initial={{ opacity: 0, x: -20 }}
                    animate={{ opacity: 1, x: 0 }}
                    transition={{ delay: 0.3 }}
                  >
                    <span className="final-stat-icon">🎲</span>
                    <span className="final-stat-label">Total Spins</span>
                    <span className="final-stat-value">{spins}</span>
                  </motion.div>

                  <motion.div
                    className="final-stat"
                    initial={{ opacity: 0, x: -20 }}
                    animate={{ opacity: 1, x: 0 }}
                    transition={{ delay: 0.4 }}
                  >
                    <span className="final-stat-icon">🏆</span>
                    <span className="final-stat-label">Biggest Win</span>
                    <span className="final-stat-value">{biggestWin}</span>
                  </motion.div>

                  <motion.div
                    className="final-stat highlight"
                    initial={{ opacity: 0, x: -20 }}
                    animate={{ opacity: 1, x: 0 }}
                    transition={{ delay: 0.5 }}
                  >
                    <span className="final-stat-icon">🪙</span>
                    <span className="final-stat-label">Final Balance</span>
                    <span className="final-stat-value coins">{coins}</span>
                  </motion.div>
                </div>

                {coins > STARTING_COINS && (
                  <motion.div
                    className="achievement-badge"
                    initial={{ scale: 0 }}
                    animate={{ scale: 1 }}
                    transition={{ delay: 0.6, type: 'spring', stiffness: 300 }}
                  >
                    <span>💰</span> Big Winner! <span>💰</span>
                  </motion.div>
                )}

                <motion.button
                  className="claim-rewards-btn"
                  onClick={handleFinish}
                  initial={{ opacity: 0 }}
                  animate={{ opacity: 1 }}
                  transition={{ delay: 0.7 }}
                  whileHover={{ scale: 1.02 }}
                  whileTap={{ scale: 0.98 }}
                >
                  Claim {Math.max(0, coins - STARTING_COINS + totalWon)} Coins! 🪙
                </motion.button>
              </motion.div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Close button */}
        <button className="game-close-btn" onClick={onClose}>
          ✕
        </button>
      </div>
    </div>
  );
};

export default SlotMachineGame;
