import { useState, useEffect, useCallback, useRef } from 'react';
import './PongGame.css';

// Game constants
const PADDLE_HEIGHT = 60;
const PADDLE_WIDTH = 10;
const BALL_SIZE = 12;
const PADDLE_SPEED = 8;
const INITIAL_BALL_SPEED = 5;
const BALL_SPEED_INCREMENT = 0.5;
const MAX_BALL_SPEED = 12;
const WINNING_SCORE = 7;

const PongGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('menu'); // menu, countdown, playing, paused, ended
  const [countdown, setCountdown] = useState(3);
  const [playerScore, setPlayerScore] = useState(0);
  const [cpuScore, setCpuScore] = useState(0);
  const [winner, setWinner] = useState(null);

  const canvasRef = useRef(null);
  const gameAreaRef = useRef(null);
  const animationFrameRef = useRef(null);
  const canvasSize = useRef({ width: 400, height: 300 });

  // Game state refs (for animation loop)
  const playerPaddleRef = useRef({ y: 120 });
  const cpuPaddleRef = useRef({ y: 120 });
  const ballRef = useRef({ x: 200, y: 150, vx: 5, vy: 3 });
  const ballSpeedRef = useRef(INITIAL_BALL_SPEED);
  const keysRef = useRef({ up: false, down: false });
  const touchYRef = useRef(null);
  const playerScoreRef = useRef(0);
  const cpuScoreRef = useRef(0);

  // Initialize canvas
  useEffect(() => {
    if (gameState !== 'playing' && gameState !== 'countdown') return;

    const initCanvas = () => {
      const canvas = canvasRef.current;
      const container = gameAreaRef.current;
      if (!canvas || !container) return;

      const rect = container.getBoundingClientRect();
      const width = Math.min(rect.width - 20, 450);
      const height = Math.min(width * 0.75, rect.height - 100);

      const dpr = window.devicePixelRatio || 1;
      canvas.width = width * dpr;
      canvas.height = height * dpr;
      canvas.style.width = `${width}px`;
      canvas.style.height = `${height}px`;

      canvasSize.current = { width, height };

      const ctx = canvas.getContext('2d');
      ctx.scale(dpr, dpr);

      // Reset positions
      playerPaddleRef.current.y = height / 2 - PADDLE_HEIGHT / 2;
      cpuPaddleRef.current.y = height / 2 - PADDLE_HEIGHT / 2;
      resetBall();
    };

    const timeout = setTimeout(initCanvas, 50);
    return () => clearTimeout(timeout);
  }, [gameState]);

  // Reset ball to center
  const resetBall = useCallback(() => {
    const { width, height } = canvasSize.current;
    ballRef.current = {
      x: width / 2,
      y: height / 2,
      vx: (Math.random() > 0.5 ? 1 : -1) * ballSpeedRef.current,
      vy: (Math.random() - 0.5) * ballSpeedRef.current,
    };
  }, []);

  // Countdown effect
  useEffect(() => {
    if (gameState !== 'countdown') return;
    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
    }
  }, [countdown, gameState]);

  // Keyboard controls
  useEffect(() => {
    const handleKeyDown = (e) => {
      if (e.key === 'ArrowUp' || e.key === 'w' || e.key === 'W') {
        keysRef.current.up = true;
      }
      if (e.key === 'ArrowDown' || e.key === 's' || e.key === 'S') {
        keysRef.current.down = true;
      }
    };

    const handleKeyUp = (e) => {
      if (e.key === 'ArrowUp' || e.key === 'w' || e.key === 'W') {
        keysRef.current.up = false;
      }
      if (e.key === 'ArrowDown' || e.key === 's' || e.key === 'S') {
        keysRef.current.down = false;
      }
    };

    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);

    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);
    };
  }, []);

  // Touch controls
  const handleTouchStart = useCallback((e) => {
    const touch = e.touches[0];
    touchYRef.current = touch.clientY;
  }, []);

  const handleTouchMove = useCallback((e) => {
    if (touchYRef.current === null) return;
    e.preventDefault();

    const touch = e.touches[0];
    const deltaY = touch.clientY - touchYRef.current;
    touchYRef.current = touch.clientY;

    const { height } = canvasSize.current;
    playerPaddleRef.current.y = Math.max(
      0,
      Math.min(height - PADDLE_HEIGHT, playerPaddleRef.current.y + deltaY)
    );
  }, []);

  const handleTouchEnd = useCallback(() => {
    touchYRef.current = null;
  }, []);

  // Game loop
  useEffect(() => {
    if (gameState !== 'playing') return;

    const update = () => {
      const { width, height } = canvasSize.current;
      const ball = ballRef.current;
      const playerPaddle = playerPaddleRef.current;
      const cpuPaddle = cpuPaddleRef.current;

      // Update player paddle from keyboard
      if (keysRef.current.up) {
        playerPaddle.y = Math.max(0, playerPaddle.y - PADDLE_SPEED);
      }
      if (keysRef.current.down) {
        playerPaddle.y = Math.min(height - PADDLE_HEIGHT, playerPaddle.y + PADDLE_SPEED);
      }

      // CPU AI - follows ball with some delay
      const cpuCenter = cpuPaddle.y + PADDLE_HEIGHT / 2;
      const cpuSpeed = PADDLE_SPEED * 0.7; // Slightly slower than player
      if (ball.vx > 0) { // Only track when ball is coming toward CPU
        if (cpuCenter < ball.y - 10) {
          cpuPaddle.y = Math.min(height - PADDLE_HEIGHT, cpuPaddle.y + cpuSpeed);
        } else if (cpuCenter > ball.y + 10) {
          cpuPaddle.y = Math.max(0, cpuPaddle.y - cpuSpeed);
        }
      } else {
        // Return to center when ball is going away
        const centerY = height / 2 - PADDLE_HEIGHT / 2;
        if (cpuPaddle.y < centerY - 5) {
          cpuPaddle.y += cpuSpeed * 0.5;
        } else if (cpuPaddle.y > centerY + 5) {
          cpuPaddle.y -= cpuSpeed * 0.5;
        }
      }

      // Update ball position
      ball.x += ball.vx;
      ball.y += ball.vy;

      // Ball collision with top/bottom walls
      if (ball.y <= BALL_SIZE / 2) {
        ball.y = BALL_SIZE / 2;
        ball.vy = -ball.vy;
      }
      if (ball.y >= height - BALL_SIZE / 2) {
        ball.y = height - BALL_SIZE / 2;
        ball.vy = -ball.vy;
      }

      // Ball collision with player paddle (left)
      if (
        ball.x - BALL_SIZE / 2 <= PADDLE_WIDTH + 15 &&
        ball.x - BALL_SIZE / 2 >= 15 &&
        ball.y >= playerPaddle.y &&
        ball.y <= playerPaddle.y + PADDLE_HEIGHT &&
        ball.vx < 0
      ) {
        ball.vx = -ball.vx;
        // Add spin based on where ball hit paddle
        const hitPos = (ball.y - playerPaddle.y) / PADDLE_HEIGHT;
        ball.vy = (hitPos - 0.5) * ballSpeedRef.current * 1.5;
        // Speed up
        if (ballSpeedRef.current < MAX_BALL_SPEED) {
          ballSpeedRef.current += BALL_SPEED_INCREMENT;
          ball.vx = Math.sign(ball.vx) * ballSpeedRef.current;
        }
      }

      // Ball collision with CPU paddle (right)
      if (
        ball.x + BALL_SIZE / 2 >= width - PADDLE_WIDTH - 15 &&
        ball.x + BALL_SIZE / 2 <= width - 15 &&
        ball.y >= cpuPaddle.y &&
        ball.y <= cpuPaddle.y + PADDLE_HEIGHT &&
        ball.vx > 0
      ) {
        ball.vx = -ball.vx;
        const hitPos = (ball.y - cpuPaddle.y) / PADDLE_HEIGHT;
        ball.vy = (hitPos - 0.5) * ballSpeedRef.current * 1.5;
        if (ballSpeedRef.current < MAX_BALL_SPEED) {
          ballSpeedRef.current += BALL_SPEED_INCREMENT;
          ball.vx = Math.sign(ball.vx) * ballSpeedRef.current;
        }
      }

      // Score - ball goes past paddles
      if (ball.x < 0) {
        // CPU scores
        cpuScoreRef.current++;
        setCpuScore(cpuScoreRef.current);
        ballSpeedRef.current = INITIAL_BALL_SPEED;
        resetBall();
        if (cpuScoreRef.current >= WINNING_SCORE) {
          setWinner('cpu');
          setGameState('ended');
        }
      }

      if (ball.x > width) {
        // Player scores
        playerScoreRef.current++;
        setPlayerScore(playerScoreRef.current);
        ballSpeedRef.current = INITIAL_BALL_SPEED;
        resetBall();
        if (playerScoreRef.current >= WINNING_SCORE) {
          setWinner('player');
          setGameState('ended');
        }
      }
    };

    const render = () => {
      const canvas = canvasRef.current;
      const ctx = canvas?.getContext('2d');
      if (!ctx) return;

      const { width, height } = canvasSize.current;
      const ball = ballRef.current;
      const playerPaddle = playerPaddleRef.current;
      const cpuPaddle = cpuPaddleRef.current;

      // Clear canvas - classic black background
      ctx.fillStyle = '#000000';
      ctx.fillRect(0, 0, width, height);

      // Draw center line (dashed)
      ctx.strokeStyle = '#FFFFFF';
      ctx.lineWidth = 2;
      ctx.setLineDash([10, 10]);
      ctx.beginPath();
      ctx.moveTo(width / 2, 0);
      ctx.lineTo(width / 2, height);
      ctx.stroke();
      ctx.setLineDash([]);

      // Draw scores
      ctx.fillStyle = '#FFFFFF';
      ctx.font = 'bold 48px "Courier New", monospace';
      ctx.textAlign = 'center';
      ctx.fillText(playerScoreRef.current.toString(), width / 4, 60);
      ctx.fillText(cpuScoreRef.current.toString(), (width * 3) / 4, 60);

      // Draw paddles - classic white rectangles
      ctx.fillStyle = '#FFFFFF';
      // Player paddle (left)
      ctx.fillRect(15, playerPaddle.y, PADDLE_WIDTH, PADDLE_HEIGHT);
      // CPU paddle (right)
      ctx.fillRect(width - 15 - PADDLE_WIDTH, cpuPaddle.y, PADDLE_WIDTH, PADDLE_HEIGHT);

      // Draw ball - classic square ball
      ctx.fillStyle = '#FFFFFF';
      ctx.fillRect(
        ball.x - BALL_SIZE / 2,
        ball.y - BALL_SIZE / 2,
        BALL_SIZE,
        BALL_SIZE
      );
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
  }, [gameState, resetBall]);

  // Start game
  const startGame = useCallback(() => {
    playerScoreRef.current = 0;
    cpuScoreRef.current = 0;
    setPlayerScore(0);
    setCpuScore(0);
    setWinner(null);
    ballSpeedRef.current = INITIAL_BALL_SPEED;
    setCountdown(3);
    setGameState('countdown');
  }, []);

  // Calculate score
  const calculateScore = useCallback(() => {
    if (winner === 'player') {
      const margin = playerScoreRef.current - cpuScoreRef.current;
      return 50 + margin * 20;
    }
    return playerScoreRef.current * 10;
  }, [winner]);

  return (
    <div className="pong-game-overlay">
      <div className="pong-game-container">
        {/* Menu */}
        {gameState === 'menu' && (
          <div className="pong-game-menu">
            <h1 className="pong-game-title">
              <span>🏓</span> PONG <span>🏓</span>
            </h1>
            <p className="pong-game-subtitle">Classic Atari Arcade</p>

            <div className="pong-game-rules">
              <h3>Controls</h3>
              <div className="control-row">
                <span className="control-key">↑ / W</span>
                <span>Move Up</span>
              </div>
              <div className="control-row">
                <span className="control-key">↓ / S</span>
                <span>Move Down</span>
              </div>
              <div className="control-row">
                <span className="control-key">Touch</span>
                <span>Drag to move</span>
              </div>
            </div>

            <div className="pong-objective">
              <p>First to <strong>{WINNING_SCORE}</strong> wins!</p>
            </div>

            <button className="pong-start-btn" onClick={startGame}>
              START GAME
            </button>

            <button className="pong-close-btn-menu" onClick={onClose}>
              Back to Games
            </button>
          </div>
        )}

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="pong-game-playing">
            <div
              ref={gameAreaRef}
              className="pong-game-area"
            >
              <canvas ref={canvasRef} />
              <div className="pong-countdown">
                <span>{countdown > 0 ? countdown : 'GO!'}</span>
              </div>
            </div>
          </div>
        )}

        {/* Playing */}
        {gameState === 'playing' && (
          <div className="pong-game-playing">
            <div
              ref={gameAreaRef}
              className="pong-game-area"
              onTouchStart={handleTouchStart}
              onTouchMove={handleTouchMove}
              onTouchEnd={handleTouchEnd}
            >
              <canvas ref={canvasRef} />
            </div>

            <div className="pong-labels">
              <span className="pong-label">YOU</span>
              <span className="pong-label">CPU</span>
            </div>

            {/* Mobile control hint */}
            <div className="pong-mobile-hint">
              Drag on screen to move your paddle
            </div>

            <button className="pong-close-btn" onClick={onClose}>✕</button>
          </div>
        )}

        {/* Game Over */}
        {gameState === 'ended' && (
          <div className="pong-game-over">
            <div className="pong-game-over-content">
              <div className="winner-display">
                {winner === 'player' ? (
                  <>
                    <span className="winner-emoji">🏆</span>
                    <h2>YOU WIN!</h2>
                  </>
                ) : (
                  <>
                    <span className="winner-emoji">😢</span>
                    <h2>CPU WINS</h2>
                  </>
                )}
              </div>

              <div className="final-scores">
                <div className="score-column">
                  <span className="score-label">YOU</span>
                  <span className="score-value">{playerScore}</span>
                </div>
                <span className="score-divider">-</span>
                <div className="score-column">
                  <span className="score-label">CPU</span>
                  <span className="score-value">{cpuScore}</span>
                </div>
              </div>

              <div className="pong-final-score">
                Score: {calculateScore()}
              </div>

              <div className="pong-game-over-buttons">
                <button className="pong-claim-btn" onClick={() => onGameEnd(calculateScore())}>
                  Claim {calculateScore()} Coins! 🪙
                </button>
                <button className="pong-retry-btn" onClick={startGame}>
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

export default PongGame;
