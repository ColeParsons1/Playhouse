import { useState, useEffect, useCallback, useRef } from 'react';
import './LudoGame.css';

// Simplified 2-player Ludo
const PLAYER_COLORS = {
  player: { main: '#3498db', light: '#5dade2', name: 'Blue' },
  ai: { main: '#e74c3c', light: '#ec7063', name: 'Red' },
};

// Path positions for each player (simplified)
const PATH_LENGTH = 52;
const HOME_STRETCH = 6;

const LudoGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown'); // countdown, playing, ended
  const [countdown, setCountdown] = useState(3);
  const [currentPlayer, setCurrentPlayer] = useState('player');
  const [diceValue, setDiceValue] = useState(null);
  const [isRolling, setIsRolling] = useState(false);
  const [canRollAgain, setCanRollAgain] = useState(false);
  const [message, setMessage] = useState('');
  const [score, setScore] = useState(0);

  // Pieces: -1 = home, 0-51 = on path, 52-57 = home stretch, 58 = finished
  const [playerPieces, setPlayerPieces] = useState([-1, -1, -1, -1]);
  const [aiPieces, setAiPieces] = useState([-1, -1, -1, -1]);

  const [selectedPiece, setSelectedPiece] = useState(null);
  const [validMoves, setValidMoves] = useState([]);

  const aiTimeoutRef = useRef(null);
  const diceAnimationRef = useRef(null);

  // Countdown
  useEffect(() => {
    if (gameState !== 'countdown') return;
    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
      setMessage("Roll the dice! 🎲");
    }
  }, [countdown, gameState]);

  // Cleanup
  useEffect(() => {
    return () => {
      if (aiTimeoutRef.current) clearTimeout(aiTimeoutRef.current);
      if (diceAnimationRef.current) clearTimeout(diceAnimationRef.current);
    };
  }, []);

  // Check if piece can move
  const canPieceMove = useCallback((pieces, pieceIdx, dice, isPlayer) => {
    const pos = pieces[pieceIdx];

    // If at home, need 6 to come out
    if (pos === -1) {
      return dice === 6;
    }

    // If finished, can't move
    if (pos >= 58) return false;

    const newPos = pos + dice;

    // Check if would overshoot finish
    if (pos >= PATH_LENGTH && newPos > PATH_LENGTH + HOME_STRETCH - 1) {
      return false;
    }

    // Check if landing on own piece
    const ownPositions = pieces.filter((p, i) => i !== pieceIdx && p >= 0 && p < 58);
    if (ownPositions.includes(newPos) && newPos < PATH_LENGTH) {
      return false;
    }

    return true;
  }, []);

  // Get valid moves for current player
  const getValidMoves = useCallback((dice) => {
    const pieces = currentPlayer === 'player' ? playerPieces : aiPieces;
    const moves = [];

    for (let i = 0; i < 4; i++) {
      if (canPieceMove(pieces, i, dice, currentPlayer === 'player')) {
        moves.push(i);
      }
    }

    return moves;
  }, [currentPlayer, playerPieces, aiPieces, canPieceMove]);

  // Roll dice
  const rollDice = useCallback(() => {
    if (isRolling || gameState !== 'playing') return;
    if (currentPlayer === 'player' && diceValue !== null && !canRollAgain) return;

    setIsRolling(true);
    setSelectedPiece(null);
    setValidMoves([]);

    // Dice animation
    let rolls = 0;
    const animateDice = () => {
      setDiceValue(Math.floor(Math.random() * 6) + 1);
      rolls++;
      if (rolls < 10) {
        diceAnimationRef.current = setTimeout(animateDice, 80);
      } else {
        const finalValue = Math.floor(Math.random() * 6) + 1;
        setDiceValue(finalValue);
        setIsRolling(false);
        setCanRollAgain(false);

        const moves = getValidMoves(finalValue);
        setValidMoves(moves);

        if (moves.length === 0) {
          setMessage(currentPlayer === 'player' ? "No valid moves! 😢" : "AI has no moves!");
          setTimeout(() => {
            if (finalValue === 6) {
              setCanRollAgain(true);
              setMessage("Roll again! 🎲");
            } else {
              switchTurn();
            }
          }, 1000);
        } else if (currentPlayer === 'ai') {
          // AI makes move
          setTimeout(() => aiMakeMove(finalValue, moves), 800);
        } else {
          setMessage("Select a piece to move! 🎯");
        }
      }
    };
    animateDice();
  }, [isRolling, gameState, currentPlayer, diceValue, canRollAgain, getValidMoves]);

  // Switch turn
  const switchTurn = useCallback(() => {
    setDiceValue(null);
    setSelectedPiece(null);
    setValidMoves([]);

    if (currentPlayer === 'player') {
      setCurrentPlayer('ai');
      setMessage("AI's turn! 🤖");
      aiTimeoutRef.current = setTimeout(() => rollDice(), 1000);
    } else {
      setCurrentPlayer('player');
      setMessage("Your turn! Roll the dice 🎲");
    }
  }, [currentPlayer, rollDice]);

  // Move piece
  const movePiece = useCallback((pieceIdx) => {
    const isPlayer = currentPlayer === 'player';
    const pieces = isPlayer ? [...playerPieces] : [...aiPieces];
    const enemyPieces = isPlayer ? [...aiPieces] : [...playerPieces];
    const setOwnPieces = isPlayer ? setPlayerPieces : setAiPieces;
    const setEnemyPieces = isPlayer ? setAiPieces : setPlayerPieces;

    const currentPos = pieces[pieceIdx];
    let newPos;

    if (currentPos === -1) {
      // Coming out of home
      newPos = 0;
    } else {
      newPos = currentPos + diceValue;
    }

    // Check for capture (if on main path)
    if (newPos < PATH_LENGTH) {
      const capturedIdx = enemyPieces.findIndex(p => {
        // Convert enemy position to our reference
        const enemyPathPos = (p + PATH_LENGTH / 2) % PATH_LENGTH;
        return enemyPathPos === newPos && p >= 0 && p < PATH_LENGTH;
      });

      if (capturedIdx >= 0) {
        const newEnemyPieces = [...enemyPieces];
        newEnemyPieces[capturedIdx] = -1;
        setEnemyPieces(newEnemyPieces);
        setMessage(isPlayer ? "Captured! Nice! 🎯" : "AI captured your piece! 😱");
        if (isPlayer) setScore(s => s + 50);
        setCanRollAgain(true);
      }
    }

    // Check if reached home
    if (newPos >= PATH_LENGTH + HOME_STRETCH - 1) {
      newPos = 58; // Finished
      setMessage(isPlayer ? "One home! 🏠" : "AI got one home!");
      if (isPlayer) setScore(s => s + 100);
    }

    pieces[pieceIdx] = newPos;
    setOwnPieces(pieces);

    // Check win condition
    const finishedCount = pieces.filter(p => p >= 58).length;
    if (finishedCount === 4) {
      setGameState('ended');
      if (isPlayer) {
        setScore(s => s + 300);
        setMessage("You Win! 🎉");
      } else {
        setMessage("AI Wins! 🤖");
      }
      return;
    }

    // Check for another turn
    if (diceValue === 6 || canRollAgain) {
      setCanRollAgain(true);
      setDiceValue(null);
      setValidMoves([]);
      setTimeout(() => {
        if (currentPlayer === 'ai') {
          rollDice();
        } else {
          setMessage("Roll again! 🎲");
        }
      }, 500);
    } else {
      setTimeout(() => switchTurn(), 500);
    }
  }, [currentPlayer, playerPieces, aiPieces, diceValue, canRollAgain, switchTurn, rollDice]);

  // AI move
  const aiMakeMove = useCallback((dice, moves) => {
    if (moves.length === 0) return;

    const pieces = aiPieces;

    // AI strategy: prioritize
    // 1. Finishing a piece
    // 2. Capturing
    // 3. Moving furthest piece
    // 4. Coming out of home

    let bestMove = moves[0];
    let bestScore = -1;

    for (const idx of moves) {
      let moveScore = 0;
      const pos = pieces[idx];
      const newPos = pos === -1 ? 0 : pos + dice;

      // Finishing
      if (newPos >= PATH_LENGTH + HOME_STRETCH - 1) {
        moveScore += 1000;
      }

      // Capture possibility
      const playerPathPositions = playerPieces
        .filter(p => p >= 0 && p < PATH_LENGTH)
        .map(p => (p + PATH_LENGTH / 2) % PATH_LENGTH);

      if (newPos < PATH_LENGTH && playerPathPositions.includes(newPos)) {
        moveScore += 500;
      }

      // Progress
      if (pos === -1) {
        moveScore += 100; // Coming out
      } else {
        moveScore += pos; // Prefer moving advanced pieces
      }

      // Safety (being on home stretch)
      if (newPos >= PATH_LENGTH) {
        moveScore += 50;
      }

      if (moveScore > bestScore) {
        bestScore = moveScore;
        bestMove = idx;
      }
    }

    setMessage("AI moves! 🤖");
    movePiece(bestMove);
  }, [aiPieces, playerPieces, movePiece]);

  // Player selects piece
  const handlePieceClick = useCallback((pieceIdx) => {
    if (currentPlayer !== 'player' || !validMoves.includes(pieceIdx)) return;

    setSelectedPiece(pieceIdx);
    movePiece(pieceIdx);
  }, [currentPlayer, validMoves, movePiece]);

  const handleFinish = useCallback(() => {
    onGameEnd(score);
  }, [score, onGameEnd]);

  // Render piece position
  const getPieceStyle = useCallback((pos, pieceIdx, isPlayer) => {
    const boardSize = 300;
    const cellSize = boardSize / 15;

    // Home positions
    if (pos === -1) {
      const homeX = isPlayer ? 1.5 : 10.5;
      const homeY = isPlayer ? 10.5 : 1.5;
      const offset = pieceIdx % 2 === 0 ? 0 : 1.5;
      const offsetY = pieceIdx < 2 ? 0 : 1.5;
      return {
        left: (homeX + offset) * cellSize,
        top: (homeY + offsetY) * cellSize,
      };
    }

    // Finished
    if (pos >= 58) {
      const finishX = isPlayer ? 7 : 7;
      const finishY = isPlayer ? 8 : 6;
      return {
        left: (finishX + (pieceIdx % 2) * 0.5) * cellSize,
        top: (finishY + Math.floor(pieceIdx / 2) * 0.5) * cellSize,
      };
    }

    // On path
    let x, y;

    // Simplified path calculation for 2-player
    const actualPos = isPlayer ? pos : (pos + PATH_LENGTH / 2) % PATH_LENGTH;

    // Path coordinates (simplified square path)
    if (actualPos < 5) {
      x = 6; y = 13 - actualPos;
    } else if (actualPos < 6) {
      x = 6; y = 8;
    } else if (actualPos < 11) {
      x = 5 - (actualPos - 6); y = 6;
    } else if (actualPos < 13) {
      x = 0; y = 5 - (actualPos - 11);
    } else if (actualPos < 18) {
      x = actualPos - 13; y = 6;
    } else if (actualPos < 19) {
      x = 6; y = 6;
    } else if (actualPos < 24) {
      x = 6; y = 5 - (actualPos - 19);
    } else if (actualPos < 26) {
      x = 7 + (actualPos - 24); y = 0;
    } else if (actualPos < 31) {
      x = 8; y = actualPos - 26;
    } else if (actualPos < 32) {
      x = 8; y = 6;
    } else if (actualPos < 37) {
      x = 9 + (actualPos - 32); y = 6;
    } else if (actualPos < 39) {
      x = 14; y = 7 + (actualPos - 37);
    } else if (actualPos < 44) {
      x = 13 - (actualPos - 39); y = 8;
    } else if (actualPos < 45) {
      x = 8; y = 8;
    } else if (actualPos < 50) {
      x = 8; y = 9 + (actualPos - 45);
    } else {
      x = 7 - (actualPos - 50); y = 14;
    }

    // Home stretch
    if (pos >= PATH_LENGTH) {
      const stretchPos = pos - PATH_LENGTH;
      if (isPlayer) {
        x = 7; y = 13 - stretchPos;
      } else {
        x = 7; y = 1 + stretchPos;
      }
    }

    return {
      left: x * cellSize + cellSize / 4,
      top: y * cellSize + cellSize / 4,
    };
  }, []);

  // Dice faces
  const DiceFace = ({ value }) => {
    const dots = {
      1: [[1, 1]],
      2: [[0, 0], [2, 2]],
      3: [[0, 0], [1, 1], [2, 2]],
      4: [[0, 0], [0, 2], [2, 0], [2, 2]],
      5: [[0, 0], [0, 2], [1, 1], [2, 0], [2, 2]],
      6: [[0, 0], [0, 1], [0, 2], [2, 0], [2, 1], [2, 2]],
    };

    return (
      <div className="ludo-dice-face">
        {dots[value]?.map(([x, y], i) => (
          <div
            key={i}
            className="dice-dot"
            style={{
              left: `${x * 30 + 10}%`,
              top: `${y * 30 + 10}%`,
            }}
          />
        ))}
      </div>
    );
  };

  const playerFinished = playerPieces.filter(p => p >= 58).length;
  const aiFinished = aiPieces.filter(p => p >= 58).length;

  return (
    <div className="ludo-overlay">
      <div className="ludo-container">
        {/* Header */}
        <div className="ludo-header">
          <div className={`ludo-player-stat ${currentPlayer === 'player' ? 'active' : ''}`}>
            <div className="ludo-player-color" style={{ background: PLAYER_COLORS.player.main }} />
            <span>You ({playerFinished}/4)</span>
          </div>
          <div className="ludo-score">
            <span>🪙</span> {score}
          </div>
          <div className={`ludo-player-stat ai ${currentPlayer === 'ai' ? 'active' : ''}`}>
            <span>AI ({aiFinished}/4)</span>
            <div className="ludo-player-color" style={{ background: PLAYER_COLORS.ai.main }} />
          </div>
        </div>

        {/* Message */}
        {message && (
          <div className="ludo-message">{message}</div>
        )}

        {/* Game Board */}
        <div className="ludo-board">
          {/* Board background */}
          <div className="ludo-board-bg">
            {/* Home bases */}
            <div className="ludo-home player-home" />
            <div className="ludo-home ai-home" />

            {/* Center */}
            <div className="ludo-center">
              <div className="ludo-center-triangle player-triangle" />
              <div className="ludo-center-triangle ai-triangle" />
            </div>
          </div>

          {/* Player pieces */}
          {playerPieces.map((pos, idx) => (
            <div
              key={`player-${idx}`}
              className={`ludo-piece player-piece ${validMoves.includes(idx) && currentPlayer === 'player' ? 'can-move' : ''} ${selectedPiece === idx ? 'selected' : ''}`}
              style={getPieceStyle(pos, idx, true)}
              onClick={() => handlePieceClick(idx)}
            >
              <div className="piece-inner" />
            </div>
          ))}

          {/* AI pieces */}
          {aiPieces.map((pos, idx) => (
            <div
              key={`ai-${idx}`}
              className="ludo-piece ai-piece"
              style={getPieceStyle(pos, idx, false)}
            >
              <div className="piece-inner" />
            </div>
          ))}
        </div>

        {/* Dice and controls */}
        <div className="ludo-controls">
          <div
            className={`ludo-dice ${isRolling ? 'rolling' : ''}`}
            onClick={currentPlayer === 'player' ? rollDice : undefined}
          >
            {diceValue ? (
              <DiceFace value={diceValue} />
            ) : (
              <div className="dice-prompt">🎲</div>
            )}
          </div>

          {currentPlayer === 'player' && !diceValue && !isRolling && (
            <button className="roll-btn" onClick={rollDice}>
              Roll Dice
            </button>
          )}
        </div>

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="ludo-countdown">
            {countdown > 0 ? countdown : 'GO!'}
          </div>
        )}

        {/* Game Over */}
        {gameState === 'ended' && (
          <div className="ludo-game-over">
            <div className="ludo-game-over-content">
              <h2>
                {playerFinished === 4 ? (
                  <><span>🎉</span> You Win! <span>🎉</span></>
                ) : (
                  <><span>🤖</span> AI Wins <span>🤖</span></>
                )}
              </h2>

              <div className="ludo-final-stats">
                <div className="ludo-final-stat">
                  <span className="ludo-final-icon">🏠</span>
                  <span className="ludo-final-label">Pieces Home</span>
                  <span className="ludo-final-value">{playerFinished}/4</span>
                </div>
                <div className="ludo-final-stat highlight">
                  <span className="ludo-final-icon">⭐</span>
                  <span className="ludo-final-label">Score</span>
                  <span className="ludo-final-value coins">{score}</span>
                </div>
              </div>

              <button className="ludo-claim-btn" onClick={handleFinish}>
                Claim {score} Coins! 🪙
              </button>
            </div>
          </div>
        )}

        {/* Close button */}
        {gameState !== 'ended' && (
          <button className="ludo-close-btn" onClick={onClose}>✕</button>
        )}
      </div>
    </div>
  );
};

export default LudoGame;
