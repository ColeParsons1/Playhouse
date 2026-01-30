import { useState, useEffect, useCallback, useRef } from 'react';
import './CornholeGame.css';

// Game constants
const GRAVITY = 0.3;
const FRICTION = 0.85;
const BAGS_PER_ROUND = 4;
const TOTAL_ROUNDS = 3;
const POINTS_HOLE = 3;
const POINTS_BOARD = 1;

const CornholeGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('menu'); // menu, aiming, throwing, scoring, roundEnd, ended
  const [currentRound, setCurrentRound] = useState(1);
  const [playerBags, setPlayerBags] = useState(BAGS_PER_ROUND);
  const [cpuBags, setCpuBags] = useState(BAGS_PER_ROUND);
  const [playerScore, setPlayerScore] = useState(0);
  const [cpuScore, setCpuScore] = useState(0);
  const [roundPlayerScore, setRoundPlayerScore] = useState(0);
  const [roundCpuScore, setRoundCpuScore] = useState(0);
  const [isPlayerTurn, setIsPlayerTurn] = useState(true);
  const [power, setPower] = useState(50);
  const [angle, setAngle] = useState(0);
  const [showPowerMeter, setShowPowerMeter] = useState(false);
  const [lastThrowResult, setLastThrowResult] = useState(null);

  const canvasRef = useRef(null);
  const gameAreaRef = useRef(null);
  const animationFrameRef = useRef(null);
  const bagRef = useRef(null);
  const thrownBagsRef = useRef([]);
  const canvasSize = useRef({ width: 400, height: 500 });

  // Initialize canvas
  useEffect(() => {
    if (gameState === 'menu' || gameState === 'ended') return;

    const initCanvas = () => {
      const canvas = canvasRef.current;
      const container = gameAreaRef.current;
      if (!canvas || !container) return;

      const rect = container.getBoundingClientRect();
      const width = Math.min(rect.width - 20, 400);
      const height = Math.min(rect.height - 20, 500);

      const dpr = window.devicePixelRatio || 1;
      canvas.width = width * dpr;
      canvas.height = height * dpr;
      canvas.style.width = `${width}px`;
      canvas.style.height = `${height}px`;

      canvasSize.current = { width, height };

      const ctx = canvas.getContext('2d');
      ctx.scale(dpr, dpr);
    };

    const timeout = setTimeout(initCanvas, 50);
    return () => clearTimeout(timeout);
  }, [gameState]);

  // Start game
  const startGame = useCallback(() => {
    setCurrentRound(1);
    setPlayerBags(BAGS_PER_ROUND);
    setCpuBags(BAGS_PER_ROUND);
    setPlayerScore(0);
    setCpuScore(0);
    setRoundPlayerScore(0);
    setRoundCpuScore(0);
    setIsPlayerTurn(true);
    setPower(50);
    setAngle(0);
    thrownBagsRef.current = [];
    bagRef.current = null;
    setLastThrowResult(null);
    setGameState('aiming');
  }, []);

  // Throw bag
  const throwBag = useCallback(() => {
    if (gameState !== 'aiming' || !isPlayerTurn) return;

    const { width, height } = canvasSize.current;
    const startX = width / 2 + angle * 1.5;
    const startY = height - 60;

    // Calculate velocity based on power and angle
    const powerMultiplier = power / 50;
    const vx = angle * 0.08 * powerMultiplier;
    const vy = -8 * powerMultiplier;

    bagRef.current = {
      x: startX,
      y: startY,
      vx,
      vy,
      rotation: 0,
      rotationVel: (Math.random() - 0.5) * 0.2,
      isPlayer: true,
      landed: false,
      scored: false,
    };

    setPlayerBags(b => b - 1);
    setShowPowerMeter(false);
    setGameState('throwing');
  }, [gameState, isPlayerTurn, power, angle]);

  // CPU throw
  const cpuThrow = useCallback(() => {
    const { width, height } = canvasSize.current;

    // CPU aims with some randomness
    const targetAngle = (Math.random() - 0.5) * 40;
    const targetPower = 45 + Math.random() * 20;
    const accuracy = 0.7 + Math.random() * 0.25; // 70-95% accuracy

    const actualAngle = targetAngle * accuracy + (Math.random() - 0.5) * 30 * (1 - accuracy);
    const actualPower = targetPower * accuracy + (Math.random() - 0.5) * 20 * (1 - accuracy);

    const startX = width / 2 + actualAngle * 1.5;
    const startY = height - 60;
    const powerMultiplier = actualPower / 50;

    bagRef.current = {
      x: startX,
      y: startY,
      vx: actualAngle * 0.08 * powerMultiplier,
      vy: -8 * powerMultiplier,
      rotation: 0,
      rotationVel: (Math.random() - 0.5) * 0.2,
      isPlayer: false,
      landed: false,
      scored: false,
    };

    setCpuBags(b => b - 1);
    setGameState('throwing');
  }, []);

  // Check scoring
  const checkScoring = useCallback((bag) => {
    const { width, height } = canvasSize.current;

    // Board position and dimensions
    const boardY = height * 0.25;
    const boardWidth = width * 0.5;
    const boardHeight = height * 0.3;
    const boardLeft = (width - boardWidth) / 2;
    const boardRight = boardLeft + boardWidth;
    const boardTop = boardY;
    const boardBottom = boardY + boardHeight;

    // Hole position (centered, near top of board)
    const holeX = width / 2;
    const holeY = boardY + boardHeight * 0.3;
    const holeRadius = 22;

    // Check if in hole
    const distToHole = Math.sqrt((bag.x - holeX) ** 2 + (bag.y - holeY) ** 2);
    if (distToHole < holeRadius) {
      return 'hole';
    }

    // Check if on board
    if (bag.x >= boardLeft && bag.x <= boardRight &&
        bag.y >= boardTop && bag.y <= boardBottom) {
      return 'board';
    }

    return 'miss';
  }, []);

  // Process throw result
  const processThrowResult = useCallback((result, isPlayer) => {
    let points = 0;
    if (result === 'hole') {
      points = POINTS_HOLE;
    } else if (result === 'board') {
      points = POINTS_BOARD;
    }

    if (isPlayer) {
      setRoundPlayerScore(s => s + points);
    } else {
      setRoundCpuScore(s => s + points);
    }

    setLastThrowResult({ result, isPlayer, points });

    // Check if round continues
    setTimeout(() => {
      if (isPlayer && cpuBags > 0) {
        setIsPlayerTurn(false);
        setTimeout(() => cpuThrow(), 1000);
      } else if (!isPlayer && playerBags > 0) {
        setIsPlayerTurn(true);
        setGameState('aiming');
        setLastThrowResult(null);
      } else if (playerBags === 0 && (isPlayer ? cpuBags : cpuBags - 1) === 0) {
        // Round over
        setGameState('roundEnd');
      } else if (!isPlayer && playerBags > 0) {
        setIsPlayerTurn(true);
        setGameState('aiming');
        setLastThrowResult(null);
      } else {
        setGameState('roundEnd');
      }
    }, 1500);
  }, [playerBags, cpuBags, cpuThrow]);

  // Game loop
  useEffect(() => {
    if (gameState !== 'throwing' && gameState !== 'aiming') return;

    const update = () => {
      if (bagRef.current && !bagRef.current.landed) {
        const bag = bagRef.current;
        const { width, height } = canvasSize.current;

        // Apply gravity
        bag.vy += GRAVITY;
        bag.x += bag.vx;
        bag.y += bag.vy;
        bag.rotation += bag.rotationVel;

        // Board dimensions for collision
        const boardY = height * 0.25;
        const boardHeight = height * 0.3;

        // Check if bag has landed
        if (bag.y >= boardY + boardHeight + 20 || bag.y <= boardY - 50) {
          bag.landed = true;
          const result = checkScoring(bag);
          bag.scored = result !== 'miss';

          if (result !== 'hole') {
            thrownBagsRef.current.push({ ...bag });
          }

          processThrowResult(result, bag.isPlayer);
          bagRef.current = null;
        }

        // Side boundaries
        if (bag.x < 20 || bag.x > width - 20) {
          bag.vx *= -0.5;
          bag.x = Math.max(20, Math.min(width - 20, bag.x));
        }
      }
    };

    const render = () => {
      const canvas = canvasRef.current;
      const ctx = canvas?.getContext('2d');
      if (!ctx) return;

      const { width, height } = canvasSize.current;

      // Clear
      ctx.fillStyle = '#2D5A27';
      ctx.fillRect(0, 0, width, height);

      // Grass texture
      ctx.fillStyle = '#3D6A37';
      for (let i = 0; i < 50; i++) {
        ctx.fillRect(
          Math.random() * width,
          Math.random() * height,
          2,
          8 + Math.random() * 8
        );
      }

      // Draw board
      const boardY = height * 0.25;
      const boardWidth = width * 0.5;
      const boardHeight = height * 0.3;
      const boardLeft = (width - boardWidth) / 2;

      // Board shadow
      ctx.fillStyle = 'rgba(0, 0, 0, 0.3)';
      ctx.fillRect(boardLeft + 8, boardY + 8, boardWidth, boardHeight);

      // Board surface
      ctx.fillStyle = '#D4A574';
      ctx.fillRect(boardLeft, boardY, boardWidth, boardHeight);

      // Board grain
      ctx.strokeStyle = '#C19A6B';
      ctx.lineWidth = 1;
      for (let i = 0; i < 8; i++) {
        ctx.beginPath();
        ctx.moveTo(boardLeft, boardY + (boardHeight / 8) * i);
        ctx.lineTo(boardLeft + boardWidth, boardY + (boardHeight / 8) * i);
        ctx.stroke();
      }

      // Board border
      ctx.strokeStyle = '#8B4513';
      ctx.lineWidth = 4;
      ctx.strokeRect(boardLeft, boardY, boardWidth, boardHeight);

      // Hole
      const holeX = width / 2;
      const holeY = boardY + boardHeight * 0.3;
      const holeRadius = 22;

      ctx.fillStyle = '#1a1a1a';
      ctx.beginPath();
      ctx.arc(holeX, holeY, holeRadius, 0, Math.PI * 2);
      ctx.fill();

      // Hole rim
      ctx.strokeStyle = '#654321';
      ctx.lineWidth = 3;
      ctx.stroke();

      // Draw thrown bags on board
      thrownBagsRef.current.forEach(bag => {
        drawBag(ctx, bag.x, bag.y, bag.rotation, bag.isPlayer);
      });

      // Draw flying bag
      if (bagRef.current && !bagRef.current.landed) {
        drawBag(ctx, bagRef.current.x, bagRef.current.y, bagRef.current.rotation, bagRef.current.isPlayer);

        // Trajectory shadow
        ctx.fillStyle = 'rgba(0, 0, 0, 0.2)';
        ctx.beginPath();
        ctx.ellipse(bagRef.current.x, height - 30, 15, 5, 0, 0, Math.PI * 2);
        ctx.fill();
      }

      // Draw throw area
      if (gameState === 'aiming' && isPlayerTurn) {
        const throwX = width / 2 + angle * 1.5;
        const throwY = height - 60;

        // Aim line
        ctx.strokeStyle = 'rgba(255, 255, 255, 0.5)';
        ctx.setLineDash([5, 5]);
        ctx.lineWidth = 2;
        ctx.beginPath();
        ctx.moveTo(throwX, throwY);
        ctx.lineTo(width / 2, boardY + boardHeight);
        ctx.stroke();
        ctx.setLineDash([]);

        // Player bag ready to throw
        drawBag(ctx, throwX, throwY, 0, true);
      }
    };

    const drawBag = (ctx, x, y, rotation, isPlayer) => {
      ctx.save();
      ctx.translate(x, y);
      ctx.rotate(rotation);

      // Bag shape
      const bagWidth = 28;
      const bagHeight = 20;

      // Shadow
      ctx.fillStyle = 'rgba(0, 0, 0, 0.3)';
      ctx.fillRect(-bagWidth / 2 + 2, -bagHeight / 2 + 2, bagWidth, bagHeight);

      // Bag color
      ctx.fillStyle = isPlayer ? '#0088FF' : '#FF2D20';
      ctx.fillRect(-bagWidth / 2, -bagHeight / 2, bagWidth, bagHeight);

      // Bag texture
      ctx.strokeStyle = isPlayer ? '#0066CC' : '#CC2419';
      ctx.lineWidth = 1;
      ctx.strokeRect(-bagWidth / 2, -bagHeight / 2, bagWidth, bagHeight);

      // Cross-stitch
      ctx.strokeStyle = isPlayer ? '#66B3FF' : '#FF6B6B';
      ctx.lineWidth = 1.5;
      ctx.beginPath();
      ctx.moveTo(-bagWidth / 4, -bagHeight / 4);
      ctx.lineTo(bagWidth / 4, bagHeight / 4);
      ctx.moveTo(bagWidth / 4, -bagHeight / 4);
      ctx.lineTo(-bagWidth / 4, bagHeight / 4);
      ctx.stroke();

      ctx.restore();
    };

    const gameLoop = () => {
      update();
      render();
      animationFrameRef.current = requestAnimationFrame(gameLoop);
    };

    animationFrameRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    };
  }, [gameState, isPlayerTurn, angle, checkScoring, processThrowResult]);

  // Handle aim controls
  const handleAimLeft = useCallback(() => {
    setAngle(a => Math.max(-50, a - 5));
  }, []);

  const handleAimRight = useCallback(() => {
    setAngle(a => Math.min(50, a + 5));
  }, []);

  // End round
  const endRound = useCallback(() => {
    // Calculate round winner using cancellation scoring
    const diff = roundPlayerScore - roundCpuScore;
    if (diff > 0) {
      setPlayerScore(s => s + diff);
    } else if (diff < 0) {
      setCpuScore(s => s - diff);
    }

    if (currentRound >= TOTAL_ROUNDS) {
      setGameState('ended');
    } else {
      setCurrentRound(r => r + 1);
      setPlayerBags(BAGS_PER_ROUND);
      setCpuBags(BAGS_PER_ROUND);
      setRoundPlayerScore(0);
      setRoundCpuScore(0);
      setIsPlayerTurn(true);
      thrownBagsRef.current = [];
      setLastThrowResult(null);
      setGameState('aiming');
    }
  }, [currentRound, roundPlayerScore, roundCpuScore]);

  // Calculate final score for coins
  const calculateFinalScore = useCallback(() => {
    const won = playerScore > cpuScore;
    const margin = Math.abs(playerScore - cpuScore);
    if (won) {
      return 50 + margin * 10;
    }
    return playerScore * 5;
  }, [playerScore, cpuScore]);

  return (
    <div className="cornhole-game-overlay">
      <div className="cornhole-game-container">
        {/* Menu */}
        {gameState === 'menu' && (
          <div className="cornhole-game-menu">
            <h1 className="cornhole-game-title">
              <span>🎯</span> Cornhole <span>🌽</span>
            </h1>
            <p className="cornhole-game-subtitle">Toss bags into the hole!</p>

            <div className="cornhole-game-rules">
              <h3>Scoring</h3>
              <ul>
                <li>🕳️ In the hole = 3 points</li>
                <li>📦 On the board = 1 point</li>
                <li>❌ Off the board = 0 points</li>
              </ul>
              <p className="rules-note">Cancellation scoring: Only the difference counts!</p>
            </div>

            <div className="cornhole-game-info">
              <div className="info-item">
                <span>🎯</span>
                <span>{TOTAL_ROUNDS} Rounds</span>
              </div>
              <div className="info-item">
                <span>📦</span>
                <span>{BAGS_PER_ROUND} Bags each</span>
              </div>
            </div>

            <button className="cornhole-start-btn" onClick={startGame}>
              Start Game
            </button>

            <button className="cornhole-close-btn-menu" onClick={onClose}>
              Back to Games
            </button>
          </div>
        )}

        {/* Playing */}
        {(gameState === 'aiming' || gameState === 'throwing' || gameState === 'scoring') && (
          <div className="cornhole-game-playing">
            {/* Header */}
            <div className="cornhole-header">
              <div className="player-info you">
                <span className="player-label">YOU</span>
                <span className="player-score">{playerScore}</span>
                <div className="bags-left">
                  {Array(playerBags).fill('📦').join('')}
                </div>
              </div>
              <div className="round-info">
                Round {currentRound}/{TOTAL_ROUNDS}
              </div>
              <div className="player-info cpu">
                <span className="player-label">CPU</span>
                <span className="player-score">{cpuScore}</span>
                <div className="bags-left">
                  {Array(cpuBags).fill('📦').join('')}
                </div>
              </div>
            </div>

            {/* Round scores */}
            <div className="round-scores">
              <span className="round-score you">{roundPlayerScore}</span>
              <span className="vs">vs</span>
              <span className="round-score cpu">{roundCpuScore}</span>
            </div>

            {/* Game Canvas */}
            <div ref={gameAreaRef} className="cornhole-game-area">
              <canvas ref={canvasRef} />
            </div>

            {/* Controls */}
            {gameState === 'aiming' && isPlayerTurn && (
              <div className="cornhole-controls">
                <div className="aim-controls">
                  <button className="aim-btn" onClick={handleAimLeft}>◀</button>
                  <div className="aim-indicator">
                    <span>Aim: {angle > 0 ? '+' : ''}{angle}</span>
                  </div>
                  <button className="aim-btn" onClick={handleAimRight}>▶</button>
                </div>

                <div className="power-section">
                  <label>Power: {power}%</label>
                  <input
                    type="range"
                    min="20"
                    max="100"
                    value={power}
                    onChange={(e) => setPower(Number(e.target.value))}
                    className="power-slider"
                  />
                </div>

                <button className="throw-btn" onClick={throwBag}>
                  Throw! 🎯
                </button>
              </div>
            )}

            {/* Turn indicator */}
            {gameState === 'throwing' && (
              <div className="turn-indicator">
                {bagRef.current?.isPlayer ? 'Your throw...' : 'CPU throwing...'}
              </div>
            )}

            {/* Last throw result */}
            {lastThrowResult && (
              <div className={`throw-result ${lastThrowResult.result}`}>
                {lastThrowResult.result === 'hole' && '🎯 HOLE! +3'}
                {lastThrowResult.result === 'board' && '📦 On Board! +1'}
                {lastThrowResult.result === 'miss' && '❌ Miss!'}
                <span className="thrower">{lastThrowResult.isPlayer ? '(You)' : '(CPU)'}</span>
              </div>
            )}

            {/* Close button */}
            <button className="cornhole-close-btn" onClick={onClose}>✕</button>
          </div>
        )}

        {/* Round End */}
        {gameState === 'roundEnd' && (
          <div className="cornhole-round-end">
            <div className="round-end-content">
              <h2>Round {currentRound} Complete!</h2>

              <div className="round-final-scores">
                <div className="round-player-result">
                  <span className="label">You</span>
                  <span className="score">{roundPlayerScore}</span>
                </div>
                <span className="vs-big">vs</span>
                <div className="round-player-result">
                  <span className="label">CPU</span>
                  <span className="score">{roundCpuScore}</span>
                </div>
              </div>

              <div className="round-winner">
                {roundPlayerScore > roundCpuScore && `You score ${roundPlayerScore - roundCpuScore} points!`}
                {roundPlayerScore < roundCpuScore && `CPU scores ${roundCpuScore - roundPlayerScore} points!`}
                {roundPlayerScore === roundCpuScore && 'Tie! No points awarded.'}
              </div>

              <button className="next-round-btn" onClick={endRound}>
                {currentRound >= TOTAL_ROUNDS ? 'See Final Results' : 'Next Round'}
              </button>
            </div>
          </div>
        )}

        {/* Game Over */}
        {gameState === 'ended' && (
          <div className="cornhole-game-over">
            <div className="cornhole-game-over-content">
              <div className="winner-display">
                {playerScore > cpuScore ? (
                  <>
                    <span className="winner-emoji">🏆</span>
                    <h2>You Win!</h2>
                  </>
                ) : playerScore < cpuScore ? (
                  <>
                    <span className="winner-emoji">😢</span>
                    <h2>CPU Wins</h2>
                  </>
                ) : (
                  <>
                    <span className="winner-emoji">🤝</span>
                    <h2>It's a Tie!</h2>
                  </>
                )}
              </div>

              <div className="final-scores">
                <div className="final-score-item">
                  <span className="label">You</span>
                  <span className="score">{playerScore}</span>
                </div>
                <span className="divider">-</span>
                <div className="final-score-item">
                  <span className="label">CPU</span>
                  <span className="score">{cpuScore}</span>
                </div>
              </div>

              <div className="cornhole-final-score">
                Score: {calculateFinalScore()}
              </div>

              <div className="cornhole-game-over-buttons">
                <button className="cornhole-claim-btn" onClick={() => onGameEnd(calculateFinalScore())}>
                  Claim {calculateFinalScore()} Coins! 🪙
                </button>
                <button className="cornhole-retry-btn" onClick={startGame}>
                  Play Again
                </button>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default CornholeGame;
