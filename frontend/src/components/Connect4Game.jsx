import { useState, useEffect, useCallback, useRef } from 'react';
import './Connect4Game.css';

const ROWS = 6;
const COLS = 7;

const Connect4Game = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('countdown'); // countdown, playing, ended
  const [countdown, setCountdown] = useState(3);
  const [board, setBoard] = useState(Array(ROWS).fill(null).map(() => Array(COLS).fill(null)));
  const [currentPlayer, setCurrentPlayer] = useState('player');
  const [winner, setWinner] = useState(null);
  const [winningCells, setWinningCells] = useState([]);
  const [score, setScore] = useState(0);
  const [lastMove, setLastMove] = useState(null);
  const [hoveredCol, setHoveredCol] = useState(null);
  const [moveCount, setMoveCount] = useState(0);

  const aiTimeoutRef = useRef(null);

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

  // Cleanup
  useEffect(() => {
    return () => {
      if (aiTimeoutRef.current) clearTimeout(aiTimeoutRef.current);
    };
  }, []);

  // Check for winner
  const checkWinner = useCallback((boardState, row, col) => {
    const player = boardState[row][col];
    if (!player) return null;

    const directions = [
      [0, 1],   // horizontal
      [1, 0],   // vertical
      [1, 1],   // diagonal down-right
      [1, -1],  // diagonal down-left
    ];

    for (const [dr, dc] of directions) {
      let count = 1;
      const cells = [[row, col]];

      // Check in positive direction
      for (let i = 1; i < 4; i++) {
        const r = row + dr * i;
        const c = col + dc * i;
        if (r >= 0 && r < ROWS && c >= 0 && c < COLS && boardState[r][c] === player) {
          count++;
          cells.push([r, c]);
        } else break;
      }

      // Check in negative direction
      for (let i = 1; i < 4; i++) {
        const r = row - dr * i;
        const c = col - dc * i;
        if (r >= 0 && r < ROWS && c >= 0 && c < COLS && boardState[r][c] === player) {
          count++;
          cells.push([r, c]);
        } else break;
      }

      if (count >= 4) {
        return { winner: player, cells };
      }
    }

    return null;
  }, []);

  // Get available columns
  const getAvailableColumns = useCallback((boardState) => {
    const available = [];
    for (let col = 0; col < COLS; col++) {
      if (boardState[0][col] === null) {
        available.push(col);
      }
    }
    return available;
  }, []);

  // Get row for drop
  const getDropRow = useCallback((boardState, col) => {
    for (let row = ROWS - 1; row >= 0; row--) {
      if (boardState[row][col] === null) {
        return row;
      }
    }
    return -1;
  }, []);

  // AI evaluation function
  const evaluatePosition = useCallback((boardState, player) => {
    let score = 0;
    const opponent = player === 'ai' ? 'player' : 'ai';

    // Check all possible 4-in-a-row windows
    const evaluateWindow = (cells) => {
      const playerCount = cells.filter(c => c === player).length;
      const opponentCount = cells.filter(c => c === opponent).length;
      const emptyCount = cells.filter(c => c === null).length;

      if (playerCount === 4) return 1000;
      if (opponentCount === 4) return -1000;
      if (playerCount === 3 && emptyCount === 1) return 50;
      if (opponentCount === 3 && emptyCount === 1) return -80; // Block more aggressively
      if (playerCount === 2 && emptyCount === 2) return 10;
      if (opponentCount === 2 && emptyCount === 2) return -10;
      return 0;
    };

    // Horizontal
    for (let row = 0; row < ROWS; row++) {
      for (let col = 0; col <= COLS - 4; col++) {
        const window = [boardState[row][col], boardState[row][col + 1], boardState[row][col + 2], boardState[row][col + 3]];
        score += evaluateWindow(window);
      }
    }

    // Vertical
    for (let col = 0; col < COLS; col++) {
      for (let row = 0; row <= ROWS - 4; row++) {
        const window = [boardState[row][col], boardState[row + 1][col], boardState[row + 2][col], boardState[row + 3][col]];
        score += evaluateWindow(window);
      }
    }

    // Diagonal (down-right)
    for (let row = 0; row <= ROWS - 4; row++) {
      for (let col = 0; col <= COLS - 4; col++) {
        const window = [boardState[row][col], boardState[row + 1][col + 1], boardState[row + 2][col + 2], boardState[row + 3][col + 3]];
        score += evaluateWindow(window);
      }
    }

    // Diagonal (down-left)
    for (let row = 0; row <= ROWS - 4; row++) {
      for (let col = 3; col < COLS; col++) {
        const window = [boardState[row][col], boardState[row + 1][col - 1], boardState[row + 2][col - 2], boardState[row + 3][col - 3]];
        score += evaluateWindow(window);
      }
    }

    // Center column preference
    const centerCol = Math.floor(COLS / 2);
    for (let row = 0; row < ROWS; row++) {
      if (boardState[row][centerCol] === player) score += 3;
    }

    return score;
  }, []);

  // Minimax with alpha-beta pruning
  const minimax = useCallback((boardState, depth, alpha, beta, isMaximizing) => {
    const availableCols = getAvailableColumns(boardState);

    // Check terminal states
    for (let col = 0; col < COLS; col++) {
      for (let row = 0; row < ROWS; row++) {
        if (boardState[row][col]) {
          const result = checkWinner(boardState, row, col);
          if (result) {
            return result.winner === 'ai' ? 10000 - depth : -10000 + depth;
          }
        }
      }
    }

    if (depth === 0 || availableCols.length === 0) {
      return evaluatePosition(boardState, 'ai');
    }

    if (isMaximizing) {
      let maxEval = -Infinity;
      for (const col of availableCols) {
        const row = getDropRow(boardState, col);
        const newBoard = boardState.map(r => [...r]);
        newBoard[row][col] = 'ai';
        const evalScore = minimax(newBoard, depth - 1, alpha, beta, false);
        maxEval = Math.max(maxEval, evalScore);
        alpha = Math.max(alpha, evalScore);
        if (beta <= alpha) break;
      }
      return maxEval;
    } else {
      let minEval = Infinity;
      for (const col of availableCols) {
        const row = getDropRow(boardState, col);
        const newBoard = boardState.map(r => [...r]);
        newBoard[row][col] = 'player';
        const evalScore = minimax(newBoard, depth - 1, alpha, beta, true);
        minEval = Math.min(minEval, evalScore);
        beta = Math.min(beta, evalScore);
        if (beta <= alpha) break;
      }
      return minEval;
    }
  }, [getAvailableColumns, getDropRow, checkWinner, evaluatePosition]);

  // AI move
  const aiMove = useCallback(() => {
    const availableCols = getAvailableColumns(board);
    if (availableCols.length === 0) return;

    let bestCol = availableCols[Math.floor(availableCols.length / 2)];
    let bestScore = -Infinity;

    // Adjust depth based on game progress for better performance
    const depth = moveCount < 8 ? 4 : moveCount < 16 ? 5 : 6;

    for (const col of availableCols) {
      const row = getDropRow(board, col);
      const newBoard = board.map(r => [...r]);
      newBoard[row][col] = 'ai';

      // Check for immediate win
      const winResult = checkWinner(newBoard, row, col);
      if (winResult && winResult.winner === 'ai') {
        bestCol = col;
        break;
      }

      const score = minimax(newBoard, depth, -Infinity, Infinity, false);
      if (score > bestScore) {
        bestScore = score;
        bestCol = col;
      }
    }

    // Make the move
    const row = getDropRow(board, bestCol);
    if (row >= 0) {
      const newBoard = board.map(r => [...r]);
      newBoard[row][bestCol] = 'ai';
      setBoard(newBoard);
      setLastMove({ row, col: bestCol });
      setMoveCount(m => m + 1);

      const result = checkWinner(newBoard, row, bestCol);
      if (result) {
        setWinner(result.winner);
        setWinningCells(result.cells);
        setGameState('ended');
      } else if (getAvailableColumns(newBoard).length === 0) {
        setWinner('draw');
        setGameState('ended');
      } else {
        setCurrentPlayer('player');
      }
    }
  }, [board, moveCount, getAvailableColumns, getDropRow, checkWinner, minimax]);

  // Player move
  const handleColumnClick = useCallback((col) => {
    if (gameState !== 'playing' || currentPlayer !== 'player' || winner) return;

    const row = getDropRow(board, col);
    if (row < 0) return;

    const newBoard = board.map(r => [...r]);
    newBoard[row][col] = 'player';
    setBoard(newBoard);
    setLastMove({ row, col });
    setMoveCount(m => m + 1);

    const result = checkWinner(newBoard, row, col);
    if (result) {
      setWinner(result.winner);
      setWinningCells(result.cells);
      setScore(200 + (42 - moveCount) * 10);
      setGameState('ended');
    } else if (getAvailableColumns(newBoard).length === 0) {
      setWinner('draw');
      setScore(50);
      setGameState('ended');
    } else {
      setCurrentPlayer('ai');
      aiTimeoutRef.current = setTimeout(aiMove, 800);
    }
  }, [gameState, currentPlayer, winner, board, moveCount, getDropRow, checkWinner, getAvailableColumns, aiMove]);

  const handleFinish = useCallback(() => {
    onGameEnd(score);
  }, [score, onGameEnd]);

  const isWinningCell = (row, col) => {
    return winningCells.some(([r, c]) => r === row && c === col);
  };

  return (
    <div className="connect4-overlay">
      <div className="connect4-container">
        {/* Header */}
        <div className="c4-header">
          <div className={`c4-player-indicator ${currentPlayer === 'player' ? 'active' : ''}`}>
            <div className="c4-chip player-chip"></div>
            <span>You</span>
          </div>
          <div className="c4-status">
            {gameState === 'playing' && (
              currentPlayer === 'player' ? "Your Turn" : "AI Thinking..."
            )}
          </div>
          <div className={`c4-player-indicator ${currentPlayer === 'ai' ? 'active' : ''}`}>
            <div className="c4-chip ai-chip"></div>
            <span>AI</span>
          </div>
        </div>

        {/* Game Board */}
        <div className="c4-board-container">
          {/* Column hover indicators */}
          <div className="c4-hover-row">
            {Array(COLS).fill(null).map((_, col) => (
              <div
                key={col}
                className={`c4-hover-cell ${hoveredCol === col && currentPlayer === 'player' ? 'active' : ''}`}
              >
                {hoveredCol === col && currentPlayer === 'player' && (
                  <div className="c4-preview-chip"></div>
                )}
              </div>
            ))}
          </div>

          {/* Board */}
          <div className="c4-board">
            {board.map((row, rowIdx) => (
              <div key={rowIdx} className="c4-row">
                {row.map((cell, colIdx) => (
                  <div
                    key={colIdx}
                    className={`c4-cell ${isWinningCell(rowIdx, colIdx) ? 'winning' : ''}`}
                    onClick={() => handleColumnClick(colIdx)}
                    onMouseEnter={() => setHoveredCol(colIdx)}
                    onMouseLeave={() => setHoveredCol(null)}
                  >
                    <div className="c4-cell-hole">
                      {cell && (
                        <div
                          className={`c4-chip ${cell === 'player' ? 'player-chip' : 'ai-chip'} ${
                            lastMove?.row === rowIdx && lastMove?.col === colIdx ? 'drop-animation' : ''
                          } ${isWinningCell(rowIdx, colIdx) ? 'winning-chip' : ''}`}
                        />
                      )}
                    </div>
                  </div>
                ))}
              </div>
            ))}
          </div>
        </div>

        {/* Countdown */}
        {gameState === 'countdown' && (
          <div className="c4-countdown">
            {countdown > 0 ? countdown : 'GO!'}
          </div>
        )}

        {/* Game Over */}
        {gameState === 'ended' && (
          <div className="c4-game-over">
            <div className="c4-game-over-content">
              <h2>
                {winner === 'player' ? (
                  <><span>🎉</span> You Win! <span>🎉</span></>
                ) : winner === 'ai' ? (
                  <><span>🤖</span> AI Wins <span>🤖</span></>
                ) : (
                  <><span>🤝</span> It's a Draw! <span>🤝</span></>
                )}
              </h2>

              <div className="c4-final-stats">
                <div className="c4-final-stat">
                  <span className="c4-final-icon">🎯</span>
                  <span className="c4-final-label">Moves Made</span>
                  <span className="c4-final-value">{moveCount}</span>
                </div>
                <div className="c4-final-stat highlight">
                  <span className="c4-final-icon">⭐</span>
                  <span className="c4-final-label">Score</span>
                  <span className="c4-final-value coins">{score}</span>
                </div>
              </div>

              <button className="c4-claim-btn" onClick={handleFinish}>
                Claim {score} Coins! 🪙
              </button>
            </div>
          </div>
        )}

        {/* Close button */}
        {gameState !== 'ended' && (
          <button className="c4-close-btn" onClick={onClose}>✕</button>
        )}
      </div>
    </div>
  );
};

export default Connect4Game;
