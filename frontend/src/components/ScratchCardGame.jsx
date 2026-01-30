import { useState, useEffect, useCallback, useRef } from 'react';
import './ScratchCardGame.css';

const SCRATCH_THRESHOLD = 60; // Percentage needed to reveal
const BASE_COINS = 15;
const MAX_BONUS_COINS = 35;

const CARD_THEMES = [
  { emoji: '💎', name: 'Diamond', color: '#E0F7FA', multiplier: 2.0 },
  { emoji: '🌈', name: 'Rainbow', color: '#FCE4EC', multiplier: 1.8 },
  { emoji: '⭐', name: 'Star', color: '#FFF8E1', multiplier: 1.5 },
  { emoji: '🍀', name: 'Lucky', color: '#E8F5E9', multiplier: 1.3 },
  { emoji: '💖', name: 'Heart', color: '#F3E5F5', multiplier: 1.2 },
  { emoji: '🎀', name: 'Ribbon', color: '#FFEBEE', multiplier: 1.0 },
];

const ScratchCardGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('scratching'); // scratching, revealed
  const [scratchPercent, setScratchPercent] = useState(0);
  const [reward, setReward] = useState(null);
  const [cardTheme, setCardTheme] = useState(null);

  const canvasRef = useRef(null);
  const contextRef = useRef(null);
  const isDrawingRef = useRef(false);
  const scratchedPixelsRef = useRef(0);
  const totalPixelsRef = useRef(0);
  const lastPosRef = useRef({ x: 0, y: 0 });

  // Initialize game
  useEffect(() => {
    // Pick random theme
    const theme = CARD_THEMES[Math.floor(Math.random() * CARD_THEMES.length)];
    setCardTheme(theme);

    // Calculate reward
    const bonusCoins = Math.floor(Math.random() * MAX_BONUS_COINS);
    const totalCoins = Math.floor((BASE_COINS + bonusCoins) * theme.multiplier);
    setReward({ coins: totalCoins, theme });
  }, []);

  // Setup canvas
  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas || !cardTheme) return;

    const ctx = canvas.getContext('2d', { willReadFrequently: true });
    contextRef.current = ctx;

    // Set canvas size
    const rect = canvas.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    canvas.width = rect.width * dpr;
    canvas.height = rect.height * dpr;
    ctx.scale(dpr, dpr);

    // Fill with scratch layer (shimmery gradient)
    const gradient = ctx.createLinearGradient(0, 0, rect.width, rect.height);
    gradient.addColorStop(0, '#C0C0C0');
    gradient.addColorStop(0.3, '#E8E8E8');
    gradient.addColorStop(0.5, '#D4D4D4');
    gradient.addColorStop(0.7, '#E8E8E8');
    gradient.addColorStop(1, '#C0C0C0');

    ctx.fillStyle = gradient;
    ctx.fillRect(0, 0, rect.width, rect.height);

    // Add some sparkle dots
    for (let i = 0; i < 50; i++) {
      const x = Math.random() * rect.width;
      const y = Math.random() * rect.height;
      ctx.beginPath();
      ctx.arc(x, y, 1 + Math.random() * 2, 0, Math.PI * 2);
      ctx.fillStyle = `rgba(255, 255, 255, ${0.3 + Math.random() * 0.5})`;
      ctx.fill();
    }

    // Add "SCRATCH ME!" text
    ctx.font = 'bold 24px system-ui';
    ctx.fillStyle = 'rgba(150, 150, 150, 0.8)';
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';
    ctx.fillText('✨ SCRATCH ME! ✨', rect.width / 2, rect.height / 2);

    totalPixelsRef.current = rect.width * rect.height;

    // Set composite operation for scratching
    ctx.globalCompositeOperation = 'destination-out';
  }, [cardTheme]);

  // Calculate scratch percentage
  const calculateScratchPercent = useCallback(() => {
    const canvas = canvasRef.current;
    const ctx = contextRef.current;
    if (!canvas || !ctx) return 0;

    const imageData = ctx.getImageData(0, 0, canvas.width, canvas.height);
    const pixels = imageData.data;
    let transparent = 0;

    // Sample every 4th pixel for performance
    for (let i = 3; i < pixels.length; i += 16) {
      if (pixels[i] === 0) transparent++;
    }

    const percent = (transparent / (pixels.length / 16)) * 100;
    return percent;
  }, []);

  // Draw scratch
  const scratch = useCallback((x, y) => {
    const ctx = contextRef.current;
    if (!ctx) return;

    ctx.beginPath();
    ctx.arc(x, y, 25, 0, Math.PI * 2);
    ctx.fill();

    // Draw line from last position for smoother scratching
    if (lastPosRef.current.x && lastPosRef.current.y) {
      ctx.lineWidth = 50;
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(lastPosRef.current.x, lastPosRef.current.y);
      ctx.lineTo(x, y);
      ctx.stroke();
    }

    lastPosRef.current = { x, y };
  }, []);

  // Get position from event
  const getPosition = useCallback((e) => {
    const canvas = canvasRef.current;
    if (!canvas) return { x: 0, y: 0 };

    const rect = canvas.getBoundingClientRect();
    const touch = e.touches?.[0] || e.changedTouches?.[0] || e;

    return {
      x: touch.clientX - rect.left,
      y: touch.clientY - rect.top,
    };
  }, []);

  // Event handlers
  const handleStart = useCallback((e) => {
    e.preventDefault();
    if (gameState !== 'scratching') return;
    isDrawingRef.current = true;
    const pos = getPosition(e);
    lastPosRef.current = pos;
    scratch(pos.x, pos.y);
  }, [gameState, getPosition, scratch]);

  const handleMove = useCallback((e) => {
    e.preventDefault();
    if (!isDrawingRef.current || gameState !== 'scratching') return;
    const pos = getPosition(e);
    scratch(pos.x, pos.y);

    // Update scratch percentage periodically
    const percent = calculateScratchPercent();
    setScratchPercent(Math.min(100, Math.round(percent)));

    if (percent >= SCRATCH_THRESHOLD) {
      setGameState('revealed');
      isDrawingRef.current = false;
    }
  }, [gameState, getPosition, scratch, calculateScratchPercent]);

  const handleEnd = useCallback(() => {
    isDrawingRef.current = false;
    lastPosRef.current = { x: 0, y: 0 };
  }, []);

  const handleFinish = useCallback(() => {
    onGameEnd(reward?.coins || 0);
  }, [onGameEnd, reward]);

  if (!cardTheme) return null;

  return (
    <div className="scratch-game-overlay">
      <div className="scratch-game-container">
        {/* Header */}
        <div className="scratch-header">
          <h2 className="scratch-title">
            <span>🎫</span> Lucky Scratch Card <span>🎫</span>
          </h2>
          <p className="scratch-subtitle">Scratch to reveal your prize!</p>
        </div>

        {/* Scratch Card */}
        <div className="scratch-card" style={{ backgroundColor: cardTheme.color }}>
          {/* Reward underneath */}
          <div className={`scratch-reward ${gameState === 'revealed' ? 'revealed' : ''}`}>
            <div className="reward-emoji">{cardTheme.emoji}</div>
            <div className="reward-amount">
              <span className="coin-icon">🪙</span>
              <span className="coin-value">{reward?.coins}</span>
            </div>
            <div className="reward-label">{cardTheme.name} Prize!</div>
            {cardTheme.multiplier >= 1.5 && (
              <div className="bonus-badge">
                {cardTheme.multiplier}x Bonus!
              </div>
            )}
          </div>

          {/* Scratch canvas overlay */}
          {gameState === 'scratching' && (
            <canvas
              ref={canvasRef}
              className="scratch-canvas"
              onMouseDown={handleStart}
              onMouseMove={handleMove}
              onMouseUp={handleEnd}
              onMouseLeave={handleEnd}
              onTouchStart={handleStart}
              onTouchMove={handleMove}
              onTouchEnd={handleEnd}
            />
          )}

          {/* Celebration overlay when revealed */}
          {gameState === 'revealed' && (
            <div className="reveal-celebration">
              {[...Array(12)].map((_, i) => (
                <span
                  key={i}
                  className="confetti"
                  style={{
                    '--delay': `${i * 0.1}s`,
                    '--x': `${Math.random() * 100}%`,
                  }}
                >
                  {['✨', '⭐', '🎉', '💫'][i % 4]}
                </span>
              ))}
            </div>
          )}
        </div>

        {/* Progress indicator */}
        {gameState === 'scratching' && (
          <div className="scratch-progress">
            <div className="progress-bar">
              <div
                className="progress-fill"
                style={{ width: `${scratchPercent}%` }}
              />
            </div>
            <span className="progress-text">
              {scratchPercent < SCRATCH_THRESHOLD
                ? `Scratch more! ${scratchPercent}%`
                : 'Almost there!'}
            </span>
          </div>
        )}

        {/* Claim button when revealed */}
        {gameState === 'revealed' && (
          <button
            className="claim-rewards-btn cute-button"
            onClick={handleFinish}
          >
            Claim {reward?.coins} Coins! 🪙
          </button>
        )}

        {/* Close button */}
        {gameState === 'scratching' && (
          <button className="game-close-btn" onClick={onClose}>
            ✕
          </button>
        )}
      </div>
    </div>
  );
};

export default ScratchCardGame;
