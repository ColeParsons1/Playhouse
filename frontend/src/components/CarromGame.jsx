import { useState, useEffect, useCallback, useRef } from 'react';
import './CarromGame.css';

const CarromGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown');
  const [countdown, setCountdown] = useState(3);
  const [score, setScore] = useState(0);
  const [aiScore, setAiScore] = useState(0);
  const [currentTurn, setCurrentTurn] = useState('player');
  const [isDragging, setIsDragging] = useState(false);
  const [dragStart, setDragStart] = useState({ x: 0, y: 0 });
  const [dragEnd, setDragEnd] = useState({ x: 0, y: 0 });
  const [message, setMessage] = useState('');
  const [strikerPos, setStrikerPos] = useState({ x: 175, y: 290 });

  const canvasRef = useRef(null);
  const gameAreaRef = useRef(null);
  const animationFrameRef = useRef(null);
  const piecesRef = useRef([]);
  const strikerRef = useRef(null);
  const particlesRef = useRef([]);
  const canvasSize = useRef({ width: 350, height: 350 });
  const lastTimeRef = useRef(0);
  const pocketedThisTurnRef = useRef([]);
  const aiTimeoutRef = useRef(null);

  // Board dimensions
  const BOARD_PADDING = 25;
  const POCKET_RADIUS = 20;
  const STRIKER_RADIUS = 15;
  const PIECE_RADIUS = 10;

  // Initialize pieces
  const initializePieces = useCallback(() => {
    const { width, height } = canvasSize.current;
    const centerX = width / 2;
    const centerY = height / 2;

    const pieces = [];

    // Queen (red) in center
    pieces.push({
      id: 'queen',
      x: centerX,
      y: centerY,
      vx: 0,
      vy: 0,
      radius: PIECE_RADIUS,
      color: '#e74c3c',
      type: 'queen',
      pocketed: false,
    });

    // Arrange pieces in circle around queen
    const colors = ['#2c3e50', '#ecf0f1']; // Black and white
    let colorIdx = 0;
    const rings = [6, 12]; // Inner ring 6, outer ring 12
    const radii = [25, 45];

    rings.forEach((count, ringIdx) => {
      for (let i = 0; i < count; i++) {
        const angle = (Math.PI * 2 * i) / count;
        pieces.push({
          id: `piece-${ringIdx}-${i}`,
          x: centerX + Math.cos(angle) * radii[ringIdx],
          y: centerY + Math.sin(angle) * radii[ringIdx],
          vx: 0,
          vy: 0,
          radius: PIECE_RADIUS,
          color: colors[colorIdx % 2],
          type: colorIdx % 2 === 0 ? 'black' : 'white',
          pocketed: false,
        });
        colorIdx++;
      }
    });

    piecesRef.current = pieces;
  }, []);

  // Initialize canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    const container = gameAreaRef.current;
    if (!canvas || !container) return;

    const dpr = window.devicePixelRatio || 1;
    const size = Math.min(container.clientWidth - 20, container.clientHeight - 100, 380);

    canvas.width = size * dpr;
    canvas.height = size * dpr;
    canvas.style.width = `${size}px`;
    canvas.style.height = `${size}px`;

    canvasSize.current = { width: size, height: size };
    setStrikerPos({ x: size / 2, y: size - 60 });

    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);

    initializePieces();
  }, [initializePieces]);

  // Countdown
  useEffect(() => {
    if (gameState !== 'countdown') return;
    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
      setMessage("Your turn! Aim and shoot 🎯");
    }
  }, [countdown, gameState]);

  // Cleanup
  useEffect(() => {
    return () => {
      if (aiTimeoutRef.current) clearTimeout(aiTimeoutRef.current);
    };
  }, []);

  // Get pocket positions
  const getPockets = useCallback(() => {
    const { width, height } = canvasSize.current;
    return [
      { x: BOARD_PADDING, y: BOARD_PADDING },
      { x: width - BOARD_PADDING, y: BOARD_PADDING },
      { x: BOARD_PADDING, y: height - BOARD_PADDING },
      { x: width - BOARD_PADDING, y: height - BOARD_PADDING },
    ];
  }, []);

  // Create particles
  const createParticles = useCallback((x, y, color, count = 8) => {
    for (let i = 0; i < count; i++) {
      const angle = (Math.PI * 2 * i) / count;
      const speed = 2 + Math.random() * 3;
      particlesRef.current.push({
        x, y,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        size: 3 + Math.random() * 4,
        color,
        life: 1,
      });
    }
  }, []);

  // Shoot striker
  const shootStriker = useCallback((fromPlayer) => {
    const { width, height } = canvasSize.current;

    if (fromPlayer) {
      const dx = dragStart.x - dragEnd.x;
      const dy = dragStart.y - dragEnd.y;
      const power = Math.min(Math.sqrt(dx * dx + dy * dy), 120);

      if (power < 20) {
        setIsDragging(false);
        return;
      }

      const angle = Math.atan2(dy, dx);
      const speed = power * 0.15;

      strikerRef.current = {
        x: strikerPos.x,
        y: strikerPos.y,
        vx: Math.cos(angle) * speed,
        vy: Math.sin(angle) * speed,
        radius: STRIKER_RADIUS,
        fromPlayer: true,
      };

      setIsDragging(false);
    } else {
      // AI shot
      const activePieces = piecesRef.current.filter(p => !p.pocketed);
      if (activePieces.length === 0) return;

      // AI aims at closest piece to pocket
      let bestTarget = null;
      let bestScore = -Infinity;

      const pockets = getPockets();

      activePieces.forEach(piece => {
        pockets.forEach(pocket => {
          const distToPocket = Math.sqrt((piece.x - pocket.x) ** 2 + (piece.y - pocket.y) ** 2);
          const score = 1000 - distToPocket + (piece.type === 'queen' ? 200 : 0);
          if (score > bestScore) {
            bestScore = score;
            bestTarget = piece;
          }
        });
      });

      if (bestTarget) {
        const aiStrikerY = 60;
        const dx = bestTarget.x - width / 2;
        const dy = bestTarget.y - aiStrikerY;
        const dist = Math.sqrt(dx * dx + dy * dy);
        const speed = 8 + Math.random() * 4;

        // Add some inaccuracy
        const accuracy = 0.85 + Math.random() * 0.15;
        const angleOffset = (Math.random() - 0.5) * 0.3 * (1 - accuracy);

        strikerRef.current = {
          x: width / 2 + (Math.random() - 0.5) * 60,
          y: aiStrikerY,
          vx: (dx / dist) * speed + Math.cos(angleOffset) * 2,
          vy: (dy / dist) * speed + Math.sin(angleOffset) * 2,
          radius: STRIKER_RADIUS,
          fromPlayer: false,
        };
      }
    }

    pocketedThisTurnRef.current = [];
  }, [dragStart, dragEnd, strikerPos, getPockets]);

  // AI turn
  const aiTakeTurn = useCallback(() => {
    setMessage("AI is aiming... 🤖");
    aiTimeoutRef.current = setTimeout(() => {
      if (gameState === 'playing') {
        setMessage("AI shoots!");
        shootStriker(false);
      }
    }, 1200);
  }, [gameState, shootStriker]);

  // Handle turn end
  const handleTurnEnd = useCallback(() => {
    const pocketed = pocketedThisTurnRef.current;
    const isPlayer = currentTurn === 'player';

    let gotOwnPiece = false;
    pocketed.forEach(p => {
      if (p.type === 'queen') {
        if (isPlayer) {
          setScore(s => s + 50);
        } else {
          setAiScore(s => s + 50);
        }
        gotOwnPiece = true;
      } else if ((isPlayer && p.type === 'white') || (!isPlayer && p.type === 'black')) {
        if (isPlayer) {
          setScore(s => s + 10);
        } else {
          setAiScore(s => s + 10);
        }
        gotOwnPiece = true;
      }
    });

    // Check game end
    const remaining = piecesRef.current.filter(p => !p.pocketed);
    if (remaining.length === 0 || remaining.every(p => p.type === 'queen')) {
      setGameState('ended');
      return;
    }

    // Switch turn
    if (gotOwnPiece && pocketed.length > 0) {
      setMessage(isPlayer ? "Nice! Go again! 🔥" : "AI continues...");
      if (!isPlayer) {
        aiTimeoutRef.current = setTimeout(aiTakeTurn, 1000);
      }
    } else {
      if (isPlayer) {
        setCurrentTurn('ai');
        aiTimeoutRef.current = setTimeout(aiTakeTurn, 800);
      } else {
        setCurrentTurn('player');
        setMessage("Your turn! 🎯");
      }
    }

    pocketedThisTurnRef.current = [];
  }, [currentTurn, aiTakeTurn]);

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

      const dpr = window.devicePixelRatio || 1;
      const { width, height } = canvasSize.current;

      ctx.save();

      // Draw board
      // Outer border
      ctx.fillStyle = '#8B4513';
      ctx.fillRect(0, 0, width, height);

      // Playing surface
      ctx.fillStyle = '#DEB887';
      ctx.fillRect(BOARD_PADDING - 5, BOARD_PADDING - 5, width - 2 * BOARD_PADDING + 10, height - 2 * BOARD_PADDING + 10);

      // Inner lines
      ctx.strokeStyle = '#8B4513';
      ctx.lineWidth = 2;

      // Baseline circles
      ctx.beginPath();
      ctx.arc(width / 2, height - 60, 40, 0, Math.PI * 2);
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(width / 2, 60, 40, 0, Math.PI * 2);
      ctx.stroke();

      // Center circle
      ctx.beginPath();
      ctx.arc(width / 2, height / 2, 30, 0, Math.PI * 2);
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(width / 2, height / 2, 50, 0, Math.PI * 2);
      ctx.stroke();

      // Diagonal lines to pockets
      ctx.beginPath();
      ctx.moveTo(BOARD_PADDING + 30, BOARD_PADDING + 30);
      ctx.lineTo(width / 2 - 50, height / 2 - 50);
      ctx.moveTo(width - BOARD_PADDING - 30, BOARD_PADDING + 30);
      ctx.lineTo(width / 2 + 50, height / 2 - 50);
      ctx.moveTo(BOARD_PADDING + 30, height - BOARD_PADDING - 30);
      ctx.lineTo(width / 2 - 50, height / 2 + 50);
      ctx.moveTo(width - BOARD_PADDING - 30, height - BOARD_PADDING - 30);
      ctx.lineTo(width / 2 + 50, height / 2 + 50);
      ctx.stroke();

      // Draw pockets
      const pockets = getPockets();
      pockets.forEach(pocket => {
        ctx.fillStyle = '#1a1a1a';
        ctx.beginPath();
        ctx.arc(pocket.x, pocket.y, POCKET_RADIUS, 0, Math.PI * 2);
        ctx.fill();

        // Pocket rim
        ctx.strokeStyle = '#8B4513';
        ctx.lineWidth = 3;
        ctx.stroke();
      });

      // Update physics
      if (strikerRef.current && gameState === 'playing') {
        const striker = strikerRef.current;

        // Move striker
        striker.x += striker.vx * deltaTime;
        striker.y += striker.vy * deltaTime;

        // Friction
        striker.vx *= 0.985;
        striker.vy *= 0.985;

        // Wall collisions
        if (striker.x < BOARD_PADDING + striker.radius) {
          striker.x = BOARD_PADDING + striker.radius;
          striker.vx *= -0.8;
        }
        if (striker.x > width - BOARD_PADDING - striker.radius) {
          striker.x = width - BOARD_PADDING - striker.radius;
          striker.vx *= -0.8;
        }
        if (striker.y < BOARD_PADDING + striker.radius) {
          striker.y = BOARD_PADDING + striker.radius;
          striker.vy *= -0.8;
        }
        if (striker.y > height - BOARD_PADDING - striker.radius) {
          striker.y = height - BOARD_PADDING - striker.radius;
          striker.vy *= -0.8;
        }

        // Check pocket for striker (foul)
        pockets.forEach(pocket => {
          const dist = Math.sqrt((striker.x - pocket.x) ** 2 + (striker.y - pocket.y) ** 2);
          if (dist < POCKET_RADIUS) {
            setMessage(striker.fromPlayer ? "Foul! Striker pocketed 😬" : "AI fouled!");
            createParticles(pocket.x, pocket.y, '#666', 10);
            strikerRef.current = null;
          }
        });

        // Piece collisions
        piecesRef.current.forEach(piece => {
          if (piece.pocketed) return;

          const dx = piece.x - striker.x;
          const dy = piece.y - striker.y;
          const dist = Math.sqrt(dx * dx + dy * dy);
          const minDist = striker.radius + piece.radius;

          if (dist < minDist) {
            // Collision response
            const nx = dx / dist;
            const ny = dy / dist;
            const relVx = striker.vx - piece.vx;
            const relVy = striker.vy - piece.vy;
            const relVn = relVx * nx + relVy * ny;

            if (relVn > 0) {
              const impulse = relVn * 0.9;
              striker.vx -= impulse * nx * 0.5;
              striker.vy -= impulse * ny * 0.5;
              piece.vx += impulse * nx;
              piece.vy += impulse * ny;

              // Separate
              const overlap = minDist - dist;
              piece.x += nx * overlap * 0.5;
              piece.y += ny * overlap * 0.5;
              striker.x -= nx * overlap * 0.5;
              striker.y -= ny * overlap * 0.5;
            }
          }
        });

        // Check if stopped
        const speed = Math.sqrt(striker.vx ** 2 + striker.vy ** 2);
        if (speed < 0.1) {
          strikerRef.current = null;
        }
      }

      // Update pieces
      let anyMoving = false;
      piecesRef.current.forEach(piece => {
        if (piece.pocketed) return;

        piece.x += piece.vx * deltaTime;
        piece.y += piece.vy * deltaTime;
        piece.vx *= 0.98;
        piece.vy *= 0.98;

        // Wall collisions
        if (piece.x < BOARD_PADDING + piece.radius) {
          piece.x = BOARD_PADDING + piece.radius;
          piece.vx *= -0.7;
        }
        if (piece.x > width - BOARD_PADDING - piece.radius) {
          piece.x = width - BOARD_PADDING - piece.radius;
          piece.vx *= -0.7;
        }
        if (piece.y < BOARD_PADDING + piece.radius) {
          piece.y = BOARD_PADDING + piece.radius;
          piece.vy *= -0.7;
        }
        if (piece.y > height - BOARD_PADDING - piece.radius) {
          piece.y = height - BOARD_PADDING - piece.radius;
          piece.vy *= -0.7;
        }

        // Piece-piece collisions
        piecesRef.current.forEach(other => {
          if (other.id === piece.id || other.pocketed) return;

          const dx = other.x - piece.x;
          const dy = other.y - piece.y;
          const dist = Math.sqrt(dx * dx + dy * dy);
          const minDist = piece.radius + other.radius;

          if (dist < minDist && dist > 0) {
            const nx = dx / dist;
            const ny = dy / dist;
            const relVx = piece.vx - other.vx;
            const relVy = piece.vy - other.vy;
            const relVn = relVx * nx + relVy * ny;

            if (relVn > 0) {
              const impulse = relVn * 0.5;
              piece.vx -= impulse * nx;
              piece.vy -= impulse * ny;
              other.vx += impulse * nx;
              other.vy += impulse * ny;

              const overlap = minDist - dist;
              piece.x -= nx * overlap * 0.5;
              piece.y -= ny * overlap * 0.5;
              other.x += nx * overlap * 0.5;
              other.y += ny * overlap * 0.5;
            }
          }
        });

        // Check pockets
        pockets.forEach(pocket => {
          const dist = Math.sqrt((piece.x - pocket.x) ** 2 + (piece.y - pocket.y) ** 2);
          if (dist < POCKET_RADIUS - 5) {
            piece.pocketed = true;
            pocketedThisTurnRef.current.push(piece);
            createParticles(pocket.x, pocket.y, piece.color, 12);
          }
        });

        const speed = Math.sqrt(piece.vx ** 2 + piece.vy ** 2);
        if (speed > 0.1) anyMoving = true;
      });

      // Check turn end
      if (!strikerRef.current && !anyMoving && gameState === 'playing') {
        setTimeout(handleTurnEnd, 300);
      }

      // Draw pieces
      piecesRef.current.forEach(piece => {
        if (piece.pocketed) return;

        // Shadow
        ctx.fillStyle = 'rgba(0, 0, 0, 0.2)';
        ctx.beginPath();
        ctx.arc(piece.x + 2, piece.y + 2, piece.radius, 0, Math.PI * 2);
        ctx.fill();

        // Piece body
        const gradient = ctx.createRadialGradient(piece.x - 3, piece.y - 3, 0, piece.x, piece.y, piece.radius);
        gradient.addColorStop(0, piece.color === '#2c3e50' ? '#34495e' : piece.color === '#ecf0f1' ? '#fff' : '#ff6b6b');
        gradient.addColorStop(1, piece.color);
        ctx.fillStyle = gradient;
        ctx.beginPath();
        ctx.arc(piece.x, piece.y, piece.radius, 0, Math.PI * 2);
        ctx.fill();

        // Highlight
        ctx.fillStyle = 'rgba(255, 255, 255, 0.3)';
        ctx.beginPath();
        ctx.arc(piece.x - 3, piece.y - 3, piece.radius * 0.4, 0, Math.PI * 2);
        ctx.fill();
      });

      // Draw striker
      if (strikerRef.current) {
        const striker = strikerRef.current;

        ctx.fillStyle = 'rgba(0, 0, 0, 0.2)';
        ctx.beginPath();
        ctx.arc(striker.x + 2, striker.y + 2, striker.radius, 0, Math.PI * 2);
        ctx.fill();

        const gradient = ctx.createRadialGradient(striker.x - 4, striker.y - 4, 0, striker.x, striker.y, striker.radius);
        gradient.addColorStop(0, '#fff');
        gradient.addColorStop(1, '#bdc3c7');
        ctx.fillStyle = gradient;
        ctx.beginPath();
        ctx.arc(striker.x, striker.y, striker.radius, 0, Math.PI * 2);
        ctx.fill();

        ctx.strokeStyle = '#7f8c8d';
        ctx.lineWidth = 2;
        ctx.stroke();
      }

      // Draw striker position (when not shooting)
      if (!strikerRef.current && currentTurn === 'player' && gameState === 'playing') {
        ctx.fillStyle = 'rgba(255, 255, 255, 0.5)';
        ctx.beginPath();
        ctx.arc(strikerPos.x, strikerPos.y, STRIKER_RADIUS, 0, Math.PI * 2);
        ctx.fill();
        ctx.strokeStyle = '#fff';
        ctx.lineWidth = 2;
        ctx.stroke();
      }

      // Draw aim line
      if (isDragging && currentTurn === 'player' && !strikerRef.current) {
        const dx = dragStart.x - dragEnd.x;
        const dy = dragStart.y - dragEnd.y;
        const power = Math.min(Math.sqrt(dx * dx + dy * dy), 120);

        if (power > 20) {
          const angle = Math.atan2(dy, dx);

          ctx.setLineDash([5, 5]);
          ctx.strokeStyle = 'rgba(255, 255, 255, 0.7)';
          ctx.lineWidth = 2;
          ctx.beginPath();
          ctx.moveTo(strikerPos.x, strikerPos.y);
          ctx.lineTo(strikerPos.x + Math.cos(angle) * power * 2, strikerPos.y + Math.sin(angle) * power * 2);
          ctx.stroke();
          ctx.setLineDash([]);

          // Power indicator
          ctx.fillStyle = power > 80 ? '#e74c3c' : power > 50 ? '#f39c12' : '#2ecc71';
          ctx.fillRect(width / 2 - 40, height - 30, (power / 120) * 80, 8);
          ctx.strokeStyle = '#fff';
          ctx.strokeRect(width / 2 - 40, height - 30, 80, 8);
        }
      }

      // Update particles
      particlesRef.current = particlesRef.current.filter(p => {
        p.x += p.vx * deltaTime;
        p.y += p.vy * deltaTime;
        p.life -= 0.02 * deltaTime;

        if (p.life > 0) {
          ctx.globalAlpha = p.life;
          ctx.fillStyle = p.color;
          ctx.beginPath();
          ctx.arc(p.x, p.y, p.size * p.life, 0, Math.PI * 2);
          ctx.fill();
          ctx.globalAlpha = 1;
          return true;
        }
        return false;
      });

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
  }, [gameState, isDragging, dragStart, dragEnd, currentTurn, strikerPos, getPockets, createParticles, handleTurnEnd]);

  // Touch handlers
  const handlePointerDown = useCallback((e) => {
    if (gameState !== 'playing' || currentTurn !== 'player' || strikerRef.current) return;

    const canvas = canvasRef.current;
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    setIsDragging(true);
    setDragStart({ x, y });
    setDragEnd({ x, y });
  }, [gameState, currentTurn]);

  const handlePointerMove = useCallback((e) => {
    if (!isDragging) return;

    const canvas = canvasRef.current;
    const rect = canvas.getBoundingClientRect();
    const x = e.clientX - rect.left;
    const y = e.clientY - rect.top;

    setDragEnd({ x, y });

    // Allow moving striker along baseline
    const { width } = canvasSize.current;
    const newX = Math.max(80, Math.min(width - 80, x));
    setStrikerPos(prev => ({ ...prev, x: newX }));
  }, [isDragging]);

  const handlePointerUp = useCallback(() => {
    if (isDragging && currentTurn === 'player' && !strikerRef.current) {
      shootStriker(true);
    }
    setIsDragging(false);
  }, [isDragging, currentTurn, shootStriker]);

  const handleFinish = useCallback(() => {
    onGameEnd(score);
  }, [score, onGameEnd]);

  return (
    <div className="carrom-overlay">
      <div className="carrom-container">
        {/* Header */}
        <div className="carrom-header">
          <div className={`carrom-score-box ${currentTurn === 'player' ? 'active' : ''}`}>
            <span>You</span>
            <span className="score-value">{score}</span>
          </div>
          <div className="carrom-turn">
            {currentTurn === 'player' ? '🎯 Your Turn' : '🤖 AI Turn'}
          </div>
          <div className={`carrom-score-box ai ${currentTurn === 'ai' ? 'active' : ''}`}>
            <span>AI</span>
            <span className="score-value">{aiScore}</span>
          </div>
        </div>

        {/* Message */}
        {message && (
          <div className="carrom-message">{message}</div>
        )}

        {/* Game Canvas */}
        <div
          ref={gameAreaRef}
          className="carrom-game-area"
          onPointerDown={handlePointerDown}
          onPointerMove={handlePointerMove}
          onPointerUp={handlePointerUp}
          onPointerLeave={handlePointerUp}
        >
          <canvas ref={canvasRef} />
        </div>

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="carrom-countdown">
            {countdown > 0 ? countdown : 'STRIKE!'}
          </div>
        )}

        {/* Game Over */}
        {gameState === 'ended' && (
          <div className="carrom-game-over">
            <div className="carrom-game-over-content">
              <h2>
                {score > aiScore ? (
                  <><span>🎉</span> You Win! <span>🎉</span></>
                ) : score < aiScore ? (
                  <><span>🤖</span> AI Wins <span>🤖</span></>
                ) : (
                  <><span>🤝</span> It's a Tie! <span>🤝</span></>
                )}
              </h2>

              <div className="carrom-final-stats">
                <div className="carrom-final-stat">
                  <span>Your Score</span>
                  <span>{score}</span>
                </div>
                <div className="carrom-final-stat">
                  <span>AI Score</span>
                  <span>{aiScore}</span>
                </div>
              </div>

              <button className="carrom-claim-btn" onClick={handleFinish}>
                Claim {score} Coins! 🪙
              </button>
            </div>
          </div>
        )}

        {/* Close button */}
        {gameState !== 'ended' && (
          <button className="carrom-close-btn" onClick={onClose}>✕</button>
        )}
      </div>
    </div>
  );
};

export default CarromGame;
