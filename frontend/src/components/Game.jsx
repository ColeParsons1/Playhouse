import { useState, useEffect, useCallback } from 'react';
import { motion } from 'framer-motion';
import { gameApi } from '../api/client';
import BingoCard from './BingoCard';
import NumberCaller from './NumberCaller';
import WinModal from './WinModal';
import './Game.css';

const Game = ({ player, onPlayerUpdate }) => {
  const [game, setGame] = useState(null);
  const [playerCards, setPlayerCards] = useState([]);
  const [isLoading, setIsLoading] = useState(false);
  const [isCallingNumber, setIsCallingNumber] = useState(false);
  const [winModalOpen, setWinModalOpen] = useState(false);
  const [winData, setWinData] = useState(null);
  const [isPlayerWin, setIsPlayerWin] = useState(false);

  const startNewGame = useCallback(async () => {
    if (!player?.id) return;

    setIsLoading(true);
    try {
      const response = await gameApi.newGame(player.id);
      setGame(response.data);

      // Filter cards for the current player
      const myCards = response.data.cards.filter(
        card => card.player === player.id
      );
      setPlayerCards(myCards);
      setWinModalOpen(false);
      setWinData(null);
    } catch (error) {
      console.error('Failed to start new game:', error);
    } finally {
      setIsLoading(false);
    }
  }, [player?.id]);

  useEffect(() => {
    if (player?.id && !game) {
      startNewGame();
    }
  }, [player?.id, game, startNewGame]);

  const handleCallNumber = async () => {
    if (!game || isCallingNumber) return;

    setIsCallingNumber(true);
    try {
      const response = await gameApi.callNumber(game.id);
      const data = response.data;

      setGame(prev => ({
        ...prev,
        current_number: data.number,
        called_numbers: data.called_numbers,
        status: data.game_over ? 'completed' : 'in_progress'
      }));

      if (data.game_over) {
        setIsPlayerWin(false);
        setWinData({ winner: data.winner });
        setWinModalOpen(true);
      }
    } catch (error) {
      console.error('Failed to call number:', error);
    } finally {
      setIsCallingNumber(false);
    }
  };

  const handleMarkNumber = async (cardId, number) => {
    if (!game) return;

    try {
      const response = await gameApi.markNumber(game.id, cardId, number);
      if (response.data.marked) {
        setPlayerCards(prev =>
          prev.map(card =>
            card.id === cardId ? response.data.card : card
          )
        );
      }
    } catch (error) {
      console.error('Failed to mark number:', error);
    }
  };

  const handleClaimBingo = async (cardId) => {
    if (!game || !player) return;

    try {
      const response = await gameApi.claimBingo(game.id, cardId, player.id);
      const data = response.data;

      if (data.valid) {
        setIsPlayerWin(true);
        setWinData(data);
        setWinModalOpen(true);
        onPlayerUpdate?.(data.player);
        setGame(prev => ({ ...prev, status: 'completed' }));
      } else {
        // Show a cute message that it's not a valid bingo yet
        alert(data.message || "Not quite a bingo yet! Keep playing!");
      }
    } catch (error) {
      console.error('Failed to claim bingo:', error);
    }
  };

  const checkForBingo = (card) => {
    const markedSet = new Set(card.marked?.map(pos => `${pos[0]}-${pos[1]}`) || []);
    // Add free space
    markedSet.add('2-2');

    // Check rows
    for (let row = 0; row < 5; row++) {
      if ([0, 1, 2, 3, 4].every(col => markedSet.has(`${row}-${col}`))) return true;
    }

    // Check columns
    for (let col = 0; col < 5; col++) {
      if ([0, 1, 2, 3, 4].every(row => markedSet.has(`${row}-${col}`))) return true;
    }

    // Check diagonals
    if ([0, 1, 2, 3, 4].every(i => markedSet.has(`${i}-${i}`))) return true;
    if ([0, 1, 2, 3, 4].every(i => markedSet.has(`${i}-${4 - i}`))) return true;

    return false;
  };

  const handleWinModalClose = () => {
    setWinModalOpen(false);
    setWinData(null);
    startNewGame();
  };

  if (isLoading || !game) {
    return (
      <div className="game-loading">
        <motion.div
          className="loading-spinner-cute"
          animate={{ rotate: 360 }}
          transition={{ duration: 2, repeat: Infinity, ease: 'linear' }}
        >
          🌸
        </motion.div>
        <p>Setting up your game...</p>
      </div>
    );
  }

  return (
    <div className="game-container">
      <div className="game-content">
        {/* Number Caller */}
        <div className="caller-section">
          <NumberCaller
            currentNumber={game.current_number}
            calledNumbers={game.called_numbers || []}
            onCallNumber={handleCallNumber}
            isLoading={isCallingNumber}
            gameOver={game.status === 'completed'}
          />
        </div>

        {/* Player's Cards */}
        <div className="cards-section">
          <h2 className="cards-title">Your Cards</h2>
          <div className="cards-grid">
            {playerCards.map((card, index) => (
              <div key={card.id} className="card-wrapper">
                <BingoCard
                  card={card}
                  cardIndex={index}
                  calledNumbers={game.called_numbers || []}
                  onNumberClick={handleMarkNumber}
                  dauber={player?.active_dauber || 'heart'}
                  isActive={game.status === 'in_progress'}
                />
                {checkForBingo(card) && game.status === 'in_progress' && (
                  <motion.button
                    className="bingo-button cute-button"
                    onClick={() => handleClaimBingo(card.id)}
                    initial={{ scale: 0 }}
                    animate={{ scale: [1, 1.1, 1] }}
                    transition={{ duration: 0.5, repeat: Infinity }}
                  >
                    BINGO! 🎉
                  </motion.button>
                )}
              </div>
            ))}
          </div>
        </div>
      </div>

      {/* Win Modal */}
      <WinModal
        isOpen={winModalOpen}
        onClose={handleWinModalClose}
        winData={winData}
        isPlayerWin={isPlayerWin}
      />
    </div>
  );
};

export default Game;
