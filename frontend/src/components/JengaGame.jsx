import { useState, useEffect, useCallback, useRef } from 'react';
import './JengaGame.css';

// Simplified 2D Jenga
const BLOCK_WIDTH = 70;
const BLOCK_HEIGHT = 20;
const INITIAL_ROWS = 10;

const JengaGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [currentTurn, setCurrentTurn] = useState('player');
  const [score, setScore] = useState(0);
  const [message, setMessage] = useState('');
  const [selectedBlock, setSelectedBlock] = useState(null);
  const [towerFallen, setTowerFallen] = useState(false);

  const canvasRef = useRef(null);
  const gameAreaRef = useRef(null);
  const animationFrameRef = useRef(null);
  const blocksRef = useRef([]);
  const canvasSize = useRef({ width: 350, height: 500 });
  const lastTimeRef = useRef(0);
  const aiTimeoutRef = useRef(null);
  const pullingBlockRef = useRef(null);
  const placingBlockRef = useRef(null);
  const turnsCompletedRef = useRef(0);

  // Initialize tower
  const initializeTower = useCallback(() => {
    const { width, height } = canvasSize.current;
    const blocks = [];
    const centerX = width / 2;
    const baseY = height - 50;

    for (let row = 0; row < INITIAL_ROWS; row++) {
      const y = baseY - row * BLOCK_HEIGHT;
      const isHorizontal = row % 2 === 0;

      if (isHorizontal) {
        // 3 horizontal blocks
        for (let i = 0; i < 3; i++) {
          blocks.push({
            id: `block-${row}-${i}`,
            x: centerX - BLOCK_WIDTH * 1.5 + i * BLOCK_WIDTH + BLOCK_WIDTH / 2,
            y: y,
            width: BLOCK_WIDTH - 2,
            height: BLOCK_HEIGHT - 2,
            rotation: 0,
            vx: 0,
            vy: 0,
            vr: 0,
            row,
            removed: false,
            falling: false,
            placed: false,
          });
        }
      } else {
        // 3 vertical blocks (rotated 90 degrees)
        for (let i = 0; i < 3; i++) {
          blocks.push({
            id: `block-${row}-${i}`,
            x: centerX - BLOCK_HEIGHT * 1.5 + i * BLOCK_HEIGHT + BLOCK_HEIGHT / 2,
            y: y,
            width: BLOCK_HEIGHT - 2,
            height: BLOCK_WIDTH - 2,
            rotation: 0,
            vx: 0,
            vy: 0,
            vr: 0,
            row,
            removed: false,
            falling: false,
            placed: false,
          });
        }
      }
    }

    blocksRef.current = blocks;
  }, []);

  // Initialize canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    const container = gameAreaRef.current;
    if (!canvas || !container) return;

    const dpr = window.devicePixelRatio || 1;
    const width = Math.min(container.clientWidth - 20, 380);
    const height = Math.min(container.clientHeight - 80, 520);

    canvas.width = width * dpr;
    canvas.height = height * dpr;
    canvas.style.width = `${width}px`;
    canvas.style.height = `${height}px`;

    canvasSize.current = { width, height };

    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);

    initializeTower();
  }, [initializeTower]);

  // Countdown
  useEffect(() => {
    if (gameState !== 'countdown') return;
    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
      setMessage("Tap a block to remove it! 🧱");
    }
  }, [countdown, gameState]);

  // Cleanup
  useEffect(() => {
    return () => {
      if (aiTimeoutRef.current) clearTimeout(aiTimeoutRef.current);
    };
  }, []);

  // Check tower stability
  const checkTowerStability = useCallback(() => {
    const blocks = blocksRef.current.filter(b => !b.removed && !b.falling);
    const { width } = canvasSize.current;
    const centerX = width / 2;

    // Check center of mass
    let totalMass = 0;
    let centerOfMassX = 0;

    blocks.forEach(block => {
      totalMass += 1;
      centerOfMassX += block.x;
    });

    centerOfMassX /= totalMass || 1;

    // If center of mass is too far from center, tower falls
    const deviation = Math.abs(centerOfMassX - centerX);
    return deviation < 60; // Allow some deviation
  }, []);

  // Remove block
  const removeBlock = useCallback((blockId, isPlayer) => {
    const block = blocksRef.current.find(b => b.id === blockId);
    if (!block || block.removed) return;

    block.removed = true;
    pullingBlockRef.current = { ...block, progress: 0, isPlayer };

    setMessage(isPlayer ? "Pulling block... 🤞" : "AI removing block...");

    // Simulate pulling animation
    setTimeout(() => {
      pullingBlockRef.current = null;

      // Check stability with small random factor
      const isStable = checkTowerStability() && Math.random() > 0.15;

      if (!isStable) {
        setTowerFallen(true);
        setMessage(isPlayer ? "Tower fell! Game Over 💥" : "AI knocked it down! You win! 🎉");

        // Animate falling
        blocksRef.current.forEach(b => {
          if (!b.removed) {
            b.falling = true;
            b.vx = (Math.random() - 0.5) * 5;
            b.vy = -Math.random() * 5;
            b.vr = (Math.random() - 0.5) * 0.2;
          }
        });

        setTimeout(() => {
          if (isPlayer) {
            // Player caused fall - lower score
            setScore(s => Math.max(0, s));
          } else {
            // AI caused fall - bonus!
            setScore(s => s + 100);
          }
          setGameState('ended');
        }, 2000);

        return;
      }

      // Block successfully removed - place on top
      placingBlockRef.current = {
        ...block,
        isPlayer,
        y: 80,
        targetY: getTopY() - BLOCK_HEIGHT,
      };

      setMessage(isPlayer ? "Now place it on top! ⬆️" : "AI placing block...");

      setTimeout(() => {
        placeBlockOnTop(block, isPlayer);
      }, isPlayer ? 0 : 1000);

    }, 800);
  }, [checkTowerStability]);

  // Get top Y position
  const getTopY = useCallback(() => {
    const blocks = blocksRef.current.filter(b => !b.removed && !b.falling);
    if (blocks.length === 0) return canvasSize.current.height - 50;

    return Math.min(...blocks.map(b => b.y)) - BLOCK_HEIGHT;
  }, []);

  // Place block on top
  const placeBlockOnTop = useCallback((block, isPlayer) => {
    const { width } = canvasSize.current;
    const topY = getTopY();

    // Determine new block position
    const topBlocks = blocksRef.current.filter(b => !b.removed && !b.falling && b.y < topY + BLOCK_HEIGHT * 2);
    const isNewRowHorizontal = topBlocks.length > 0 && topBlocks[0].width < topBlocks[0].height;

    const newBlock = {
      ...block,
      removed: false,
      placed: true,
      y: topY,
      x: width / 2 + (Math.random() - 0.5) * 30,
      width: isNewRowHorizontal ? BLOCK_WIDTH - 2 : BLOCK_HEIGHT - 2,
      height: isNewRowHorizontal ? BLOCK_HEIGHT - 2 : BLOCK_WIDTH - 2,
      rotation: 0,
      vx: 0,
      vy: 0,
      vr: 0,
    };

    const idx = blocksRef.current.findIndex(b => b.id === block.id);
    if (idx >= 0) {
      blocksRef.current[idx] = newBlock;
    }

    placingBlockRef.current = null;
    turnsCompletedRef.current++;

    // Score for successful turn
    if (isPlayer) {
      setScore(s => s + 20);
    }

    // Check stability again after placing
    setTimeout(() => {
      const stable = checkTowerStability();
      if (!stable) {
        setTowerFallen(true);
        blocksRef.current.forEach(b => {
          if (!b.removed) {
            b.falling = true;
            b.vx = (Math.random() - 0.5) * 5;
            b.vy = -Math.random() * 3;
            b.vr = (Math.random() - 0.5) * 0.15;
          }
        });

        setMessage(isPlayer ? "Tower fell while placing! 💥" : "AI's placement failed! 🎉");

        setTimeout(() => {
          if (!isPlayer) setScore(s => s + 100);
          setGameState('ended');
        }, 2000);
        return;
      }

      // Switch turns
      if (isPlayer) {
        setCurrentTurn('ai');
        setMessage("AI's turn... 🤖");
        aiTimeoutRef.current = setTimeout(aiTakeTurn, 1500);
      } else {
        setCurrentTurn('player');
        setMessage("Your turn! Tap a block 🧱");
      }
    }, 500);
  }, [getTopY, checkTowerStability]);

  // AI turn
  const aiTakeTurn = useCallback(() => {
    const availableBlocks = blocksRef.current.filter(b => !b.removed && !b.falling);

    if (availableBlocks.length <= 3) {
      // AI wins by default if tower is too small
      setMessage("Tower too small! You win! 🎉");
      setScore(s => s + 50);
      setGameState('ended');
      return;
    }

    // AI strategy: pick from lower-middle rows (safer)
    const rows = [...new Set(availableBlocks.map(b => b.row))].sort((a, b) => a - b);
    const safeRows = rows.slice(1, Math.max(2, rows.length - 2)); // Avoid bottom and top

    const safeBlocks = availableBlocks.filter(b => safeRows.includes(b.row));
    const candidates = safeBlocks.length > 0 ? safeBlocks : availableBlocks;

    // Pick a random block from candidates
    const targetBlock = candidates[Math.floor(Math.random() * candidates.length)];

    setSelectedBlock(targetBlock.id);
    setMessage("AI selecting block... 🤖");

    setTimeout(() => {
      setSelectedBlock(null);
      removeBlock(targetBlock.id, false);
    }, 1000);
  }, [removeBlock]);

  // Handle block click
  const handleBlockClick = useCallback((blockId) => {
    if (gameState !== 'playing' || currentTurn !== 'player' || pullingBlockRef.current || placingBlockRef.current) return;

    const block = blocksRef.current.find(b => b.id === blockId);
    if (!block || block.removed || block.falling) return;

    // Can't remove from top row
    const topY = Math.min(...blocksRef.current.filter(b => !b.removed && !b.falling).map(b => b.y));
    if (block.y <= topY + 5) {
      setMessage("Can't remove from top! Try another 🚫");
      return;
    }

    setSelectedBlock(blockId);
    setTimeout(() => {
      setSelectedBlock(null);
      removeBlock(blockId, true);
    }, 300);
  }, [gameState, currentTurn, removeBlock]);

  // Game loop
  useEffect(() => {
    if (gameState !== 'playing' && gameState !== 'countdown') return;

    const gameLoop = (timestamp) => {
      const deltaTime = Math.min((timestamp - lastTimeRef.current) / 16.67, 3);
      lastTimeRef.current = timestamp;

      const canvas = canvasRef.current;
      const ctx = canvas?.getContext('2d');
      if (!ctx) {
        animationFrameRef.current = requestAnimationFrame(gameLoop);
        return;
      }

      const { width, height } = canvasSize.current;

      ctx.save();

      // Background
      const bgGradient = ctx.createLinearGradient(0, 0, 0, height);
      bgGradient.addColorStop(0, '#2c3e50');
      bgGradient.addColorStop(1, '#1a252f');
      ctx.fillStyle = bgGradient;
      ctx.fillRect(0, 0, width, height);

      // Floor
      ctx.fillStyle = '#5d4e37';
      ctx.fillRect(0, height - 40, width, 40);
      ctx.fillStyle = '#4a3f2f';
      ctx.fillRect(0, height - 40, width, 5);

      // Update falling blocks
      blocksRef.current.forEach(block => {
        if (block.falling) {
          block.vy += 0.5 * deltaTime;
          block.x += block.vx * deltaTime;
          block.y += block.vy * deltaTime;
          block.rotation += block.vr * deltaTime;

          // Ground collision
          if (block.y > height - 50) {
            block.y = height - 50;
            block.vy *= -0.3;
            block.vx *= 0.8;
            block.vr *= 0.5;
          }

          // Wall collision
          if (block.x < 20) { block.x = 20; block.vx *= -0.5; }
          if (block.x > width - 20) { block.x = width - 20; block.vx *= -0.5; }
        }
      });

      // Draw blocks
      blocksRef.current.forEach(block => {
        if (block.removed && !block.falling) return;

        ctx.save();
        ctx.translate(block.x, block.y);
        ctx.rotate(block.rotation);

        // Block shadow
        ctx.fillStyle = 'rgba(0, 0, 0, 0.3)';
        ctx.fillRect(-block.width / 2 + 3, -block.height / 2 + 3, block.width, block.height);

        // Block body
        const blockGradient = ctx.createLinearGradient(-block.width / 2, 0, block.width / 2, 0);
        blockGradient.addColorStop(0, '#d4a574');
        blockGradient.addColorStop(0.5, '#e8c99b');
        blockGradient.addColorStop(1, '#c9985c');
        ctx.fillStyle = blockGradient;
        ctx.fillRect(-block.width / 2, -block.height / 2, block.width, block.height);

        // Wood grain
        ctx.strokeStyle = 'rgba(139, 90, 43, 0.3)';
        ctx.lineWidth = 1;
        for (let i = 0; i < 3; i++) {
          const y = -block.height / 2 + (block.height / 3) * i + block.height / 6;
          ctx.beginPath();
          ctx.moveTo(-block.width / 2 + 2, y);
          ctx.lineTo(block.width / 2 - 2, y);
          ctx.stroke();
        }

        // Border
        ctx.strokeStyle = '#8B5A2B';
        ctx.lineWidth = 2;
        ctx.strokeRect(-block.width / 2, -block.height / 2, block.width, block.height);

        // Highlight selected
        if (selectedBlock === block.id) {
          ctx.strokeStyle = '#2ecc71';
          ctx.lineWidth = 3;
          ctx.strokeRect(-block.width / 2 - 2, -block.height / 2 - 2, block.width + 4, block.height + 4);
        }

        ctx.restore();
      });

      // Draw pulling animation
      if (pullingBlockRef.current) {
        const pulling = pullingBlockRef.current;
        const offsetX = pulling.isPlayer ? -50 : 50;

        ctx.save();
        ctx.globalAlpha = 0.7;
        ctx.translate(pulling.x + offsetX, pulling.y);

        ctx.fillStyle = '#d4a574';
        ctx.fillRect(-pulling.width / 2, -pulling.height / 2, pulling.width, pulling.height);
        ctx.strokeStyle = '#8B5A2B';
        ctx.lineWidth = 2;
        ctx.strokeRect(-pulling.width / 2, -pulling.height / 2, pulling.width, pulling.height);

        ctx.restore();
      }

      // Draw placing animation
      if (placingBlockRef.current) {
        const placing = placingBlockRef.current;

        ctx.save();
        ctx.globalAlpha = 0.8;
        ctx.translate(width / 2, placing.y);

        ctx.fillStyle = '#2ecc71';
        ctx.fillRect(-30, -10, 60, 20);

        ctx.restore();
      }

      ctx.restore();
      animationFrameRef.current = requestAnimationFrame(gameLoop);
    };

    lastTimeRef.current = performance.now();
    animationFrameRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    };
  }, [gameState, selectedBlock]);

  // Handle canvas click
  const handleCanvasClick = useCallback((e) => {
    if (gameState !== 'playing' || currentTurn !== 'player') return;

    const canvas = canvasRef.current;
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    // Find clicked block
    const clickedBlock = blocksRef.current.find(block => {
      if (block.removed || block.falling) return false;

      const dx = x - block.x;
      const dy = y - block.y;

      return Math.abs(dx) < block.width / 2 + 5 && Math.abs(dy) < block.height / 2 + 5;
    });

    if (clickedBlock) {
      handleBlockClick(clickedBlock.id);
    }
  }, [gameState, currentTurn, handleBlockClick]);

  const handleFinish = useCallback(() => {
    onGameEnd(score);
  }, [score, onGameEnd]);

  return (
    <div className="jenga-overlay">
      <div className="jenga-container">
        {/* Header */}
        <div className="jenga-header">
          <div className={`jenga-player ${currentTurn === 'player' ? 'active' : ''}`}>
            <span>👤 You</span>
          </div>
          <div className="jenga-score">
            <span>🏆</span> {score}
          </div>
          <div className={`jenga-player ai ${currentTurn === 'ai' ? 'active' : ''}`}>
            <span>🤖 AI</span>
          </div>
        </div>

        <div className="jenga-turns">
          Turns completed: {turnsCompletedRef.current}
        </div>

        {/* Message */}
        {message && (
          <div className="jenga-message">{message}</div>
        )}

        {/* Game Canvas */}
        <div
          ref={gameAreaRef}
          className="jenga-game-area"
          onClick={handleCanvasClick}
        >
          <canvas ref={canvasRef} />
        </div>

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="jenga-countdown">
            {countdown > 0 ? countdown : 'GO!'}
          </div>
        )}

        {/* Game Over */}
        {gameState === 'ended' && (
          <div className="jenga-game-over">
            <div className="jenga-game-over-content">
              <h2>
                {towerFallen && currentTurn === 'ai' ? (
                  <><span>🎉</span> You Win! <span>🎉</span></>
                ) : towerFallen ? (
                  <><span>💥</span> Tower Fell! <span>💥</span></>
                ) : (
                  <><span>🏆</span> Game Over <span>🏆</span></>
                )}
              </h2>

              <div className="jenga-final-stats">
                <div className="jenga-final-stat">
                  <span className="jenga-final-icon">🧱</span>
                  <span className="jenga-final-label">Turns Survived</span>
                  <span className="jenga-final-value">{turnsCompletedRef.current}</span>
                </div>
                <div className="jenga-final-stat highlight">
                  <span className="jenga-final-icon">⭐</span>
                  <span className="jenga-final-label">Score</span>
                  <span className="jenga-final-value coins">{score}</span>
                </div>
              </div>

              <button className="jenga-claim-btn" onClick={handleFinish}>
                Claim {score} Coins! 🪙
              </button>
            </div>
          </div>
        )}

        {/* Close button */}
        {gameState !== 'ended' && (
          <button className="jenga-close-btn" onClick={onClose}>✕</button>
        )}
      </div>
    </div>
  );
};

export default JengaGame;
