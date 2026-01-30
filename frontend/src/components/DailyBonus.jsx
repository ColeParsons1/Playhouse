import { motion, AnimatePresence } from 'framer-motion';
import { useState } from 'react';
import { playerApi } from '../api/client';
import MinigameSelector from './MinigameSelector';
import PresentCatchGame from './PresentCatchGame';
import BubbleMatchGame from './BubbleMatchGame';
import ScratchCardGame from './ScratchCardGame';
import CoinCascadeGame from './CoinCascadeGame';
import StarCollectorGame from './StarCollectorGame';
import MemoryFlashGame from './MemoryFlashGame';
import ColorSplashGame from './ColorSplashGame';
import RhythmTapGame from './RhythmTapGame';
import BubbleFloatGame from './BubbleFloatGame';
import QuickDrawGame from './QuickDrawGame';
import TempleRunGame from './TempleRunGame';
import FlappyBirdGame from './FlappyBirdGame';
import TowerDefenseGame from './TowerDefenseGame';
import BilliardsGame from './BilliardsGame';
import TanksGame from './TanksGame';
import BingoGame from './BingoGame';
import './DailyBonus.css';

const DailyBonus = ({ player, onClaim }) => {
  const [claiming, setClaiming] = useState(false);
  const [showSelector, setShowSelector] = useState(false);
  const [activeGame, setActiveGame] = useState(null);
  const [showReward, setShowReward] = useState(false);
  const [reward, setReward] = useState(null);

  const handleOpenSelector = () => {
    if (!player?.id || claiming) return;
    setShowSelector(true);
  };

  const handleSelectGame = (gameId) => {
    setShowSelector(false);
    setActiveGame(gameId);
  };

  const handleCloseSelector = () => {
    setShowSelector(false);
  };

  const handleGameEnd = async (earnedCoins) => {
    setActiveGame(null);
    setClaiming(true);

    try {
      // Claim daily bonus (base reward) + game coins
      const response = await playerApi.claimDailyBonus(player.id, earnedCoins);
      setReward({
        ...response.data,
        game_coins: earnedCoins,
        total_coins: response.data.coins_earned + earnedCoins
      });
      setShowReward(true);
      onClaim?.(response.data.player);
    } catch (error) {
      console.error('Failed to claim bonus:', error);
      // If it fails (e.g., already claimed), still show what they earned from the game
      if (earnedCoins > 0) {
        setReward({
          day_number: 0,
          coins_earned: 0,
          gems_earned: 0,
          game_coins: earnedCoins,
          total_coins: earnedCoins,
          already_claimed: true
        });
        setShowReward(true);
      }
    } finally {
      setClaiming(false);
    }
  };

  const handleCloseGame = () => {
    setActiveGame(null);
  };

  const closeReward = () => {
    setShowReward(false);
    setReward(null);
  };

  const dayIcons = ['🌱', '🌿', '🌳', '🌸', '🌺', '🌻', '🌈'];

  // Render the active game
  const renderGame = () => {
    const gameProps = {
      onGameEnd: handleGameEnd,
      onClose: handleCloseGame,
      player: player,
      onPlayerUpdate: onClaim,
    };

    switch (activeGame) {
      case 'bingo':
        return <BingoGame {...gameProps} />;
      case 'presents':
        return <PresentCatchGame {...gameProps} />;
      case 'bubbles':
        return <BubbleMatchGame {...gameProps} />;
      case 'scratch':
        return <ScratchCardGame {...gameProps} />;
      case 'cascade':
        return <CoinCascadeGame {...gameProps} />;
      case 'stars':
        return <StarCollectorGame {...gameProps} />;
      case 'memory':
        return <MemoryFlashGame {...gameProps} />;
      case 'colors':
        return <ColorSplashGame {...gameProps} />;
      case 'rhythm':
        return <RhythmTapGame {...gameProps} />;
      case 'float':
        return <BubbleFloatGame {...gameProps} />;
      case 'quickdraw':
        return <QuickDrawGame {...gameProps} />;
      case 'runner':
        return <TempleRunGame {...gameProps} />;
      case 'flappy':
        return <FlappyBirdGame {...gameProps} />;
      case 'tower':
        return <TowerDefenseGame {...gameProps} />;
      case 'billiards':
        return <BilliardsGame {...gameProps} />;
      case 'tanks':
        return <TanksGame {...gameProps} />;
      default:
        return null;
    }
  };

  return (
    <>
      <motion.button
        className="daily-bonus-btn"
        onClick={handleOpenSelector}
        disabled={claiming}
        whileHover={{ scale: 1.05 }}
        whileTap={{ scale: 0.95 }}
      >
        <span className="bonus-icon">🎁</span>
        <span className="bonus-text">Daily Bonus</span>
      </motion.button>

      {/* Minigame Selector */}
      <AnimatePresence>
        {showSelector && (
          <MinigameSelector
            onSelect={handleSelectGame}
            onClose={handleCloseSelector}
          />
        )}
      </AnimatePresence>

      {/* Active Minigame */}
      <AnimatePresence>
        {activeGame && renderGame()}
      </AnimatePresence>

      {/* Reward Modal */}
      <AnimatePresence>
        {showReward && reward && (
          <motion.div
            className="modal-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            onClick={closeReward}
          >
            <motion.div
              className="daily-bonus-modal"
              initial={{ scale: 0, rotate: -10 }}
              animate={{ scale: 1, rotate: 0 }}
              exit={{ scale: 0, rotate: 10 }}
              onClick={(e) => e.stopPropagation()}
            >
              <div className="bonus-header">
                <span className="bonus-crown">👑</span>
                <h2>{reward.already_claimed ? 'Game Rewards!' : 'Daily Bonus!'}</h2>
              </div>

              {!reward.already_claimed && (
                <div className="day-tracker">
                  {[1, 2, 3, 4, 5, 6, 7].map((day) => (
                    <motion.div
                      key={day}
                      className={`day-badge ${day <= reward.day_number ? 'active' : ''} ${day === reward.day_number ? 'current' : ''}`}
                      initial={day === reward.day_number ? { scale: 0 } : {}}
                      animate={day === reward.day_number ? { scale: 1 } : {}}
                      transition={{ delay: 0.5, type: 'spring' }}
                    >
                      <span className="day-icon">{dayIcons[day - 1]}</span>
                      <span className="day-number">Day {day}</span>
                    </motion.div>
                  ))}
                </div>
              )}

              <motion.div
                className="bonus-rewards"
                initial={{ y: 20, opacity: 0 }}
                animate={{ y: 0, opacity: 1 }}
                transition={{ delay: 0.8 }}
              >
                {/* Game coins */}
                {reward.game_coins > 0 && (
                  <div className="reward-earned game-reward">
                    <span className="reward-label">🎮 Game</span>
                    <span className="reward-icon">🪙</span>
                    <span className="reward-value">+{reward.game_coins}</span>
                  </div>
                )}

                {/* Daily bonus coins */}
                {reward.coins_earned > 0 && (
                  <div className="reward-earned">
                    <span className="reward-label">📅 Daily</span>
                    <span className="reward-icon">🪙</span>
                    <span className="reward-value">+{reward.coins_earned}</span>
                  </div>
                )}

                {/* Gems */}
                {reward.gems_earned > 0 && (
                  <div className="reward-earned gems">
                    <span className="reward-icon">💎</span>
                    <span className="reward-value">+{reward.gems_earned}</span>
                  </div>
                )}
              </motion.div>

              {/* Total */}
              {reward.total_coins > 0 && (reward.game_coins > 0 && reward.coins_earned > 0) && (
                <motion.div
                  className="total-earned"
                  initial={{ scale: 0 }}
                  animate={{ scale: 1 }}
                  transition={{ delay: 1.0, type: 'spring' }}
                >
                  <span className="total-label">Total:</span>
                  <span className="total-value">🪙 {reward.total_coins}</span>
                </motion.div>
              )}

              {reward.day_number === 7 && (
                <motion.div
                  className="streak-complete"
                  initial={{ scale: 0 }}
                  animate={{ scale: 1 }}
                  transition={{ delay: 1.2, type: 'spring' }}
                >
                  <span>🎉</span> Week Complete! <span>🎉</span>
                </motion.div>
              )}

              <motion.button
                className="close-button cute-button"
                onClick={closeReward}
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={{ delay: 1.4 }}
              >
                Yay!
              </motion.button>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>
    </>
  );
};

export default DailyBonus;
