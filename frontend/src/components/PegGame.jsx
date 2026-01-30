import { useState, useCallback, useEffect } from 'react';
import './PegGame.css';

// Triangle board with 15 holes (like Cracker Barrel)
// Row 0: 1 hole, Row 1: 2 holes, Row 2: 3 holes, Row 3: 4 holes, Row 4: 5 holes
const BOARD_POSITIONS = [
  { row: 0, col: 0, id: 0 },
  { row: 1, col: 0, id: 1 }, { row: 1, col: 1, id: 2 },
  { row: 2, col: 0, id: 3 }, { row: 2, col: 1, id: 4 }, { row: 2, col: 2, id: 5 },
  { row: 3, col: 0, id: 6 }, { row: 3, col: 1, id: 7 }, { row: 3, col: 2, id: 8 }, { row: 3, col: 3, id: 9 },
  { row: 4, col: 0, id: 10 }, { row: 4, col: 1, id: 11 }, { row: 4, col: 2, id: 12 }, { row: 4, col: 3, id: 13 }, { row: 4, col: 4, id: 14 },
];

// Valid jumps: [from, over, to]
const VALID_JUMPS = [
  // Horizontal jumps
  [0, 1, 3], [3, 1, 0], [0, 2, 5], [5, 2, 0],
  [1, 3, 6], [6, 3, 1], [2, 5, 9], [9, 5, 2],
  [3, 4, 5], [5, 4, 3], [6, 7, 8], [8, 7, 6], [7, 8, 9], [9, 8, 7],
  [10, 11, 12], [12, 11, 10], [11, 12, 13], [13, 12, 11], [12, 13, 14], [14, 13, 12],
  // Diagonal jumps (left-to-right)
  [0, 1, 3], [3, 1, 0], [1, 3, 6], [6, 3, 1], [3, 6, 10], [10, 6, 3],
  [1, 4, 8], [8, 4, 1], [4, 8, 13], [13, 8, 4],
  [2, 4, 7], [7, 4, 2], [4, 7, 11], [11, 7, 4],
  [3, 7, 12], [12, 7, 3], [5, 8, 12], [12, 8, 5],
  // Diagonal jumps (right-to-left)
  [0, 2, 5], [5, 2, 0], [2, 5, 9], [9, 5, 2], [5, 9, 14], [14, 9, 5],
  [1, 4, 8], [8, 4, 1], [4, 8, 13], [13, 8, 4],
  [2, 4, 7], [7, 4, 2], [4, 7, 11], [11, 7, 4],
  [3, 7, 12], [12, 7, 3], [5, 8, 12], [12, 8, 5],
  // More diagonal jumps
  [6, 7, 8], [8, 7, 6], [7, 8, 9], [9, 8, 7],
  [6, 11, 13], [13, 11, 6], [9, 13, 12], [12, 13, 9],
];

// Deduplicate jumps
const getUniqueJumps = () => {
  const seen = new Set();
  return VALID_JUMPS.filter(jump => {
    const key = jump.join('-');
    if (seen.has(key)) return false;
    seen.add(key);
    return true;
  });
};

const JUMPS = getUniqueJumps();

// Rating based on pegs remaining
const getRating = (pegsLeft) => {
  if (pegsLeft === 1) return { title: "Genius!", emoji: "🧠", stars: 5 };
  if (pegsLeft === 2) return { title: "Pretty Smart", emoji: "🎓", stars: 4 };
  if (pegsLeft === 3) return { title: "Just Plain Dumb", emoji: "😅", stars: 3 };
  if (pegsLeft === 4) return { title: "Eg-no-ra-moose", emoji: "🫏", stars: 2 };
  return { title: "Keep Trying!", emoji: "💪", stars: 1 };
};

const PegGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('menu'); // menu, playing, ended
  const [pegs, setPegs] = useState(Array(15).fill(true));
  const [selectedPeg, setSelectedPeg] = useState(null);
  const [validMoves, setValidMoves] = useState([]);
  const [moveCount, setMoveCount] = useState(0);
  const [startingHole, setStartingHole] = useState(null);

  // Initialize game with one empty hole
  const startGame = useCallback((emptyHole = 0) => {
    const newPegs = Array(15).fill(true);
    newPegs[emptyHole] = false;
    setPegs(newPegs);
    setSelectedPeg(null);
    setValidMoves([]);
    setMoveCount(0);
    setStartingHole(emptyHole);
    setGameState('playing');
  }, []);

  // Find valid moves for a selected peg
  const findValidMoves = useCallback((pegId, currentPegs) => {
    return JUMPS.filter(([from, over, to]) =>
      from === pegId &&
      currentPegs[from] &&
      currentPegs[over] &&
      !currentPegs[to]
    ).map(([, , to]) => to);
  }, []);

  // Check if any moves are available
  const hasAvailableMoves = useCallback((currentPegs) => {
    for (let i = 0; i < 15; i++) {
      if (currentPegs[i] && findValidMoves(i, currentPegs).length > 0) {
        return true;
      }
    }
    return false;
  }, [findValidMoves]);

  // Handle peg click
  const handlePegClick = useCallback((id) => {
    if (gameState !== 'playing') return;

    // If clicking on an empty hole that's a valid move
    if (!pegs[id] && validMoves.includes(id) && selectedPeg !== null) {
      // Find the jump that makes this move
      const jump = JUMPS.find(([from, , to]) => from === selectedPeg && to === id);
      if (jump) {
        const [from, over, to] = jump;
        const newPegs = [...pegs];
        newPegs[from] = false;
        newPegs[over] = false;
        newPegs[to] = true;
        setPegs(newPegs);
        setSelectedPeg(null);
        setValidMoves([]);
        setMoveCount(m => m + 1);

        // Check if game is over
        if (!hasAvailableMoves(newPegs)) {
          setGameState('ended');
        }
      }
      return;
    }

    // If clicking on a peg
    if (pegs[id]) {
      const moves = findValidMoves(id, pegs);
      if (moves.length > 0) {
        setSelectedPeg(id);
        setValidMoves(moves);
      } else {
        setSelectedPeg(null);
        setValidMoves([]);
      }
    } else {
      setSelectedPeg(null);
      setValidMoves([]);
    }
  }, [gameState, pegs, selectedPeg, validMoves, findValidMoves, hasAvailableMoves]);

  // Calculate remaining pegs
  const pegsRemaining = pegs.filter(p => p).length;
  const rating = getRating(pegsRemaining);

  // Calculate score for coins
  const calculateScore = useCallback(() => {
    const baseScore = (14 - pegsRemaining) * 10;
    const bonus = pegsRemaining === 1 ? 100 : pegsRemaining === 2 ? 50 : 0;
    return baseScore + bonus;
  }, [pegsRemaining]);

  return (
    <div className="peg-game-overlay">
      <div className="peg-game-container">
        {/* Menu */}
        {gameState === 'menu' && (
          <div className="peg-game-menu">
            <h1 className="peg-game-title">
              <span>🔺</span> Peg Game <span>🔺</span>
            </h1>
            <p className="peg-game-subtitle">Jump pegs to leave just one!</p>

            <div className="peg-game-rules">
              <h3>How to Play</h3>
              <ul>
                <li>Jump one peg over another into an empty hole</li>
                <li>The jumped peg is removed</li>
                <li>Try to leave only ONE peg!</li>
              </ul>
            </div>

            <div className="starting-hole-select">
              <h3>Select Starting Empty Hole</h3>
              <div className="mini-board">
                {BOARD_POSITIONS.map(pos => (
                  <button
                    key={pos.id}
                    className="mini-hole"
                    style={{
                      left: `${50 + (pos.col - pos.row / 2) * 30}px`,
                      top: `${10 + pos.row * 26}px`,
                    }}
                    onClick={() => startGame(pos.id)}
                  >
                    {pos.id === 0 ? '?' : ''}
                  </button>
                ))}
              </div>
              <p className="hint-text">Tap any hole to start with it empty</p>
            </div>

            <button className="peg-close-btn-menu" onClick={onClose}>
              Back to Games
            </button>
          </div>
        )}

        {/* Playing */}
        {gameState === 'playing' && (
          <div className="peg-game-playing">
            {/* Header */}
            <div className="peg-header">
              <div className="peg-stat">
                <span>🔴</span>
                <span>{pegsRemaining}</span>
              </div>
              <div className="peg-moves">Moves: {moveCount}</div>
              <button className="peg-reset-btn" onClick={() => startGame(startingHole)}>
                🔄
              </button>
            </div>

            {/* Board */}
            <div className="peg-board-container">
              <div className="peg-board">
                <svg className="board-triangle" viewBox="0 0 200 180">
                  <polygon
                    points="100,10 10,170 190,170"
                    fill="#8B4513"
                    stroke="#654321"
                    strokeWidth="4"
                  />
                  {/* Wood grain effect */}
                  <line x1="30" y1="150" x2="170" y2="150" stroke="#7a3d11" strokeWidth="1" opacity="0.5" />
                  <line x1="40" y1="130" x2="160" y2="130" stroke="#7a3d11" strokeWidth="1" opacity="0.5" />
                  <line x1="50" y1="110" x2="150" y2="110" stroke="#7a3d11" strokeWidth="1" opacity="0.5" />
                  <line x1="60" y1="90" x2="140" y2="90" stroke="#7a3d11" strokeWidth="1" opacity="0.5" />
                  <line x1="70" y1="70" x2="130" y2="70" stroke="#7a3d11" strokeWidth="1" opacity="0.5" />
                </svg>

                {BOARD_POSITIONS.map(pos => {
                  const x = 100 + (pos.col - pos.row / 2) * 32;
                  const y = 35 + pos.row * 32;
                  const hasPeg = pegs[pos.id];
                  const isSelected = selectedPeg === pos.id;
                  const isValidMove = validMoves.includes(pos.id);

                  return (
                    <div
                      key={pos.id}
                      className={`peg-hole ${hasPeg ? 'has-peg' : ''} ${isSelected ? 'selected' : ''} ${isValidMove ? 'valid-move' : ''}`}
                      style={{
                        left: `${x}px`,
                        top: `${y}px`,
                      }}
                      onClick={() => handlePegClick(pos.id)}
                    >
                      {hasPeg && (
                        <div className={`peg ${isSelected ? 'peg-selected' : ''}`}>
                          <div className="peg-top" />
                          <div className="peg-body" />
                        </div>
                      )}
                      {isValidMove && <div className="valid-move-indicator" />}
                    </div>
                  );
                })}
              </div>
            </div>

            {/* Instructions */}
            <div className="peg-instructions">
              {selectedPeg !== null
                ? "Tap a highlighted hole to jump there!"
                : "Tap a peg to select it"
              }
            </div>

            {/* Close button */}
            <button className="peg-close-btn" onClick={onClose}>✕</button>
          </div>
        )}

        {/* Game Over */}
        {gameState === 'ended' && (
          <div className="peg-game-over">
            <div className="peg-game-over-content">
              <div className="rating-emoji">{rating.emoji}</div>
              <h2>{rating.title}</h2>

              <div className="peg-stars">
                {[1, 2, 3, 4, 5].map(star => (
                  <span key={star} className={`star ${star <= rating.stars ? 'earned' : ''}`}>
                    ⭐
                  </span>
                ))}
              </div>

              <div className="pegs-remaining">
                {pegsRemaining} peg{pegsRemaining !== 1 ? 's' : ''} remaining
              </div>

              <div className="final-score">
                Score: {calculateScore()}
              </div>

              <div className="peg-game-over-buttons">
                <button className="peg-claim-btn" onClick={() => onGameEnd(calculateScore())}>
                  Claim {calculateScore()} Coins! 🪙
                </button>
                <button className="peg-retry-btn" onClick={() => startGame(startingHole)}>
                  Try Again
                </button>
                <button className="peg-menu-btn" onClick={() => setGameState('menu')}>
                  Change Starting Hole
                </button>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default PegGame;
