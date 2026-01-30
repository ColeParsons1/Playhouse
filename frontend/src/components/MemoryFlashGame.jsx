import { useState, useEffect, useCallback, useRef } from 'react';
import './MemoryFlashGame.css';

const GRID_SIZE = 4; // 4x4 grid = 8 pairs
const PREVIEW_TIME = 2000; // Show cards for 2 seconds
const MATCH_DELAY = 500;
const COINS_PER_MATCH = 5;
const PERFECT_BONUS = 20; // Bonus for completing with minimal moves

const CARD_ICONS = ['🌸', '⭐', '💖', '🦋', '🌈', '🍀', '🎀', '💎'];

const shuffleArray = (arr) => {
  const shuffled = [...arr];
  for (let i = shuffled.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [shuffled[i], shuffled[j]] = [shuffled[j], shuffled[i]];
  }
  return shuffled;
};

const MemoryFlashGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('preview'); // preview, playing, ended
  const [cards, setCards] = useState([]);
  const [flippedCards, setFlippedCards] = useState([]);
  const [matchedPairs, setMatchedPairs] = useState([]);
  const [moves, setMoves] = useState(0);
  const [score, setScore] = useState(0);

  const scoreRef = useRef(0);
  const movesRef = useRef(0);
  const isCheckingRef = useRef(false);

  // Initialize game
  useEffect(() => {
    const icons = CARD_ICONS.slice(0, (GRID_SIZE * GRID_SIZE) / 2);
    const pairs = [...icons, ...icons];
    const shuffled = shuffleArray(pairs).map((icon, index) => ({
      id: index,
      icon,
      isFlipped: true, // Start flipped for preview
    }));
    setCards(shuffled);

    // Hide cards after preview
    const timer = setTimeout(() => {
      setCards(prev => prev.map(card => ({ ...card, isFlipped: false })));
      setGameState('playing');
    }, PREVIEW_TIME);

    return () => clearTimeout(timer);
  }, []);

  // Check for match
  useEffect(() => {
    if (flippedCards.length !== 2 || isCheckingRef.current) return;

    isCheckingRef.current = true;
    movesRef.current += 1;
    setMoves(movesRef.current);

    const [first, second] = flippedCards;
    const firstCard = cards.find(c => c.id === first);
    const secondCard = cards.find(c => c.id === second);

    if (firstCard.icon === secondCard.icon) {
      // Match found!
      scoreRef.current += COINS_PER_MATCH;
      setScore(scoreRef.current);

      setTimeout(() => {
        setMatchedPairs(prev => [...prev, firstCard.icon]);
        setFlippedCards([]);
        isCheckingRef.current = false;

        // Check if game is complete
        if (matchedPairs.length + 1 === (GRID_SIZE * GRID_SIZE) / 2) {
          // Calculate bonus for efficiency
          const minMoves = (GRID_SIZE * GRID_SIZE) / 2;
          const efficiency = Math.max(0, 1 - (movesRef.current - minMoves) / minMoves);
          const bonus = Math.floor(efficiency * PERFECT_BONUS);
          scoreRef.current += bonus;
          setScore(scoreRef.current);
          setGameState('ended');
        }
      }, MATCH_DELAY);
    } else {
      // No match - flip back
      setTimeout(() => {
        setCards(prev => prev.map(card =>
          flippedCards.includes(card.id) ? { ...card, isFlipped: false } : card
        ));
        setFlippedCards([]);
        isCheckingRef.current = false;
      }, MATCH_DELAY);
    }
  }, [flippedCards, cards, matchedPairs.length]);

  // Handle card tap
  const handleCardTap = useCallback((cardId) => {
    if (gameState !== 'playing') return;
    if (isCheckingRef.current) return;
    if (flippedCards.length >= 2) return;
    if (flippedCards.includes(cardId)) return;

    const card = cards.find(c => c.id === cardId);
    if (matchedPairs.includes(card.icon)) return;

    setCards(prev => prev.map(c =>
      c.id === cardId ? { ...c, isFlipped: true } : c
    ));
    setFlippedCards(prev => [...prev, cardId]);
  }, [gameState, flippedCards, cards, matchedPairs]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  return (
    <div className="memory-game-overlay">
      <div className="memory-game-container">
        {/* Header */}
        <div className="game-header">
          <div className="game-stat">
            <span className="stat-icon">🪙</span>
            <span className="stat-value">{score}</span>
          </div>
          <div className="game-stat moves-stat">
            <span className="stat-icon">👆</span>
            <span className="stat-value">{moves}</span>
          </div>
          <div className="game-stat">
            <span className="stat-icon">✨</span>
            <span className="stat-value">{matchedPairs.length}/{(GRID_SIZE * GRID_SIZE) / 2}</span>
          </div>
        </div>

        {/* Preview message */}
        {gameState === 'preview' && (
          <div className="preview-message">
            <span>👀</span> Memorize the cards! <span>👀</span>
          </div>
        )}

        {/* Card Grid */}
        <div className="memory-grid">
          {cards.map((card) => {
            const isMatched = matchedPairs.includes(card.icon);
            return (
              <button
                key={card.id}
                className={`memory-card ${card.isFlipped ? 'flipped' : ''} ${isMatched ? 'matched' : ''}`}
                onClick={() => handleCardTap(card.id)}
                disabled={gameState !== 'playing' || isMatched}
              >
                <div className="card-inner">
                  <div className="card-front">
                    <span>❓</span>
                  </div>
                  <div className="card-back">
                    <span>{card.icon}</span>
                  </div>
                </div>
              </button>
            );
          })}
        </div>

        {/* Game Over Screen */}
        {gameState === 'ended' && (
          <div className="game-over-screen">
            <div className="game-over-content">
              <h2 className="game-over-title">
                <span>🧠</span> Great Memory! <span>🧠</span>
              </h2>

              <div className="final-stats">
                <div className="final-stat">
                  <span className="final-stat-icon">👆</span>
                  <span className="final-stat-label">Total Moves</span>
                  <span className="final-stat-value">{moves}</span>
                </div>

                <div className="final-stat highlight">
                  <span className="final-stat-icon">🪙</span>
                  <span className="final-stat-label">Coins Earned</span>
                  <span className="final-stat-value coins">{score}</span>
                </div>
              </div>

              {moves <= (GRID_SIZE * GRID_SIZE) / 2 + 4 && (
                <div className="achievement-badge">
                  <span>🧠</span> Memory Master! <span>🧠</span>
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

        {/* Close button */}
        {gameState === 'preview' && (
          <button className="game-close-btn" onClick={onClose}>
            ✕
          </button>
        )}
      </div>
    </div>
  );
};

export default MemoryFlashGame;
