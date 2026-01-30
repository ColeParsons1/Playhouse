import { motion, AnimatePresence } from 'framer-motion';
import './StatsModal.css';

const StatsModal = ({ isOpen, onClose, player }) => {
  if (!player) return null;

  const winRate = player.total_games > 0
    ? Math.round((player.total_wins / player.total_games) * 100)
    : 0;

  const stats = [
    { label: 'Total Games', value: player.total_games, icon: '🎮', color: '#FF69B4' },
    { label: 'Total Wins', value: player.total_wins, icon: '🏆', color: '#FFD700' },
    { label: 'Win Rate', value: `${winRate}%`, icon: '📈', color: '#98D8C8' },
    { label: 'Current Streak', value: player.current_streak, icon: '🔥', color: '#FF6B6B' },
    { label: 'Best Streak', value: player.best_streak, icon: '⭐', color: '#DDA0DD' },
  ];

  return (
    <AnimatePresence>
      {isOpen && (
        <motion.div
          className="modal-overlay"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onClick={onClose}
        >
          <motion.div
            className="stats-modal"
            initial={{ scale: 0.8, y: 50 }}
            animate={{ scale: 1, y: 0 }}
            exit={{ scale: 0.8, y: 50 }}
            onClick={(e) => e.stopPropagation()}
          >
            <div className="stats-header">
              <h2 className="stats-title">
                <span className="stats-icon">📊</span>
                Your Stats
              </h2>
              <button className="close-btn" onClick={onClose}>✕</button>
            </div>

            <div className="stats-player-info">
              <div className="player-avatar">🎀</div>
              <div className="player-name">{player.name}</div>
            </div>

            <div className="stats-grid">
              {stats.map((stat, index) => (
                <motion.div
                  key={stat.label}
                  className="stat-card"
                  initial={{ opacity: 0, y: 20 }}
                  animate={{ opacity: 1, y: 0 }}
                  transition={{ delay: index * 0.1 }}
                  style={{ '--accent-color': stat.color }}
                >
                  <span className="stat-icon">{stat.icon}</span>
                  <span className="stat-value">{stat.value}</span>
                  <span className="stat-label">{stat.label}</span>
                </motion.div>
              ))}
            </div>

            <div className="stats-achievements">
              <h3 className="achievements-title">Achievements</h3>
              <div className="achievements-grid">
                <div className={`achievement ${player.total_wins >= 1 ? 'unlocked' : ''}`}>
                  <span className="achievement-icon">🌟</span>
                  <span className="achievement-name">First Win</span>
                </div>
                <div className={`achievement ${player.current_streak >= 3 ? 'unlocked' : ''}`}>
                  <span className="achievement-icon">🔥</span>
                  <span className="achievement-name">Hot Streak (3)</span>
                </div>
                <div className={`achievement ${player.best_streak >= 5 ? 'unlocked' : ''}`}>
                  <span className="achievement-icon">💫</span>
                  <span className="achievement-name">On Fire (5)</span>
                </div>
                <div className={`achievement ${player.total_wins >= 10 ? 'unlocked' : ''}`}>
                  <span className="achievement-icon">👑</span>
                  <span className="achievement-name">Bingo Master</span>
                </div>
                <div className={`achievement ${player.best_streak >= 10 ? 'unlocked' : ''}`}>
                  <span className="achievement-icon">🌈</span>
                  <span className="achievement-name">Legendary (10)</span>
                </div>
                <div className={`achievement ${player.total_games >= 50 ? 'unlocked' : ''}`}>
                  <span className="achievement-icon">💖</span>
                  <span className="achievement-name">Dedicated Player</span>
                </div>
              </div>
            </div>

            <button className="close-button cute-button" onClick={onClose}>
              Back to Game
            </button>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
};

export default StatsModal;
