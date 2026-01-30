import { memo } from 'react';
import { motion } from 'framer-motion';
import './PlayerHeader.css';

const PlayerHeader = memo(({ player, onShopClick, onStatsClick }) => {
  if (!player) return null;

  return (
    <motion.div
      className="player-header"
      initial={{ y: -50, opacity: 0 }}
      animate={{ y: 0, opacity: 1 }}
      transition={{ duration: 0.5 }}
    >
      <div className="header-left">
        <div className="player-greeting">
          <span className="wave">👋</span>
          <span className="greeting-text">Hi, {player.name}!</span>
        </div>
        {player.current_streak > 0 && (
          <motion.div
            className="streak-badge"
            initial={{ scale: 0 }}
            animate={{ scale: 1 }}
            transition={{ type: 'spring', delay: 0.3 }}
          >
            <span className="streak-fire">🔥</span>
            <span className="streak-count">{player.current_streak}</span>
            <span className="streak-label">Streak!</span>
          </motion.div>
        )}
      </div>

      <div className="header-center">
        <div className="currency-display">
          <motion.div
            className="currency coins"
            whileHover={{ scale: 1.05 }}
          >
            <span className="currency-icon">🪙</span>
            <span className="currency-amount">{player.coins?.toLocaleString() || 0}</span>
          </motion.div>
          <motion.div
            className="currency gems"
            whileHover={{ scale: 1.05 }}
          >
            <span className="currency-icon">💎</span>
            <span className="currency-amount">{player.gems || 0}</span>
          </motion.div>
        </div>
      </div>

      <div className="header-right">
        <motion.button
          className="header-button shop-button"
          onClick={onShopClick}
          whileHover={{ scale: 1.05 }}
          whileTap={{ scale: 0.95 }}
        >
          <span className="button-icon">🛍️</span>
          <span className="button-text">Shop</span>
        </motion.button>
        <motion.button
          className="header-button stats-button"
          onClick={onStatsClick}
          whileHover={{ scale: 1.05 }}
          whileTap={{ scale: 0.95 }}
        >
          <span className="button-icon">📊</span>
          <span className="button-text">Stats</span>
        </motion.button>
      </div>
    </motion.div>
  );
});

PlayerHeader.displayName = 'PlayerHeader';

export default PlayerHeader;
