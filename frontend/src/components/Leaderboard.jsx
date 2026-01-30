import { useState, useEffect, memo } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './Leaderboard.css';

// Get leaderboard data from localStorage
const getLeaderboardData = () => {
  try {
    const data = localStorage.getItem('game_leaderboards');
    return data ? JSON.parse(data) : {};
  } catch {
    return {};
  }
};

// Save leaderboard data to localStorage
export const saveHighScore = (gameId, gameName, score, playerName = 'Player') => {
  const leaderboards = getLeaderboardData();

  if (!leaderboards[gameId]) {
    leaderboards[gameId] = {
      name: gameName,
      scores: []
    };
  }

  // Add new score
  leaderboards[gameId].scores.push({
    score,
    playerName,
    date: new Date().toISOString()
  });

  // Sort by score descending and keep top 10
  leaderboards[gameId].scores.sort((a, b) => b.score - a.score);
  leaderboards[gameId].scores = leaderboards[gameId].scores.slice(0, 10);

  localStorage.setItem('game_leaderboards', JSON.stringify(leaderboards));

  // Return the rank (1-indexed)
  const rank = leaderboards[gameId].scores.findIndex(s => s.score === score && s.date === leaderboards[gameId].scores.find(sc => sc.score === score)?.date) + 1;
  return rank;
};

// Get high score for a specific game
export const getHighScore = (gameId) => {
  const leaderboards = getLeaderboardData();
  if (leaderboards[gameId]?.scores?.length > 0) {
    return leaderboards[gameId].scores[0].score;
  }
  return 0;
};

// Game metadata for display
const GAME_META = {
  bingo: { emoji: '🎱', color: '#764ba2' },
  presents: { emoji: '🎁', color: '#1e40af' },
  runner: { emoji: '🏃', color: '#00CED1' },
  flappy: { emoji: '🦈', color: '#0077B6' },
  tower: { emoji: '🏰', color: '#4CAF50' },
  billiards: { emoji: '🎱', color: '#228B22' },
  tanks: { emoji: '🎯', color: '#4a9fff' },
  asl: { emoji: '🤟', color: '#4ecdc4' },
  goo: { emoji: '🟢', color: '#2ecc71' },
  ducks: { emoji: '🦆', color: '#f1c40f' },
  cards: { emoji: '🃏', color: '#e74c3c' },
  slots: { emoji: '🎰', color: '#9b59b6' },
  kitchen: { emoji: '👨‍🍳', color: '#FF6B35' },
  connect4: { emoji: '🔴', color: '#3498db' },
  ludo: { emoji: '🎲', color: '#9b59b6' },
  carrom: { emoji: '⚪', color: '#d4a574' },
  jenga: { emoji: '🧱', color: '#c97f4a' },
};

const LeaderboardCard = memo(({ gameId, gameName, scores, index, isExpanded, onToggle }) => {
  const meta = GAME_META[gameId] || { emoji: '🎮', color: '#764ba2' };
  const topScore = scores[0]?.score || 0;

  return (
    <motion.div
      className={`leaderboard-card ${isExpanded ? 'expanded' : ''}`}
      style={{ '--game-color': meta.color }}
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
      transition={{ delay: index * 0.05 }}
      onClick={onToggle}
      layoutId={`leaderboard-${gameId}`}
    >
      <div className="leaderboard-card-header">
        <div className="leaderboard-game-info">
          <span className="leaderboard-emoji">{meta.emoji}</span>
          <div className="leaderboard-text">
            <span className="leaderboard-game-name">{gameName}</span>
            <span className="leaderboard-high-score">
              <span className="trophy">🏆</span> {topScore.toLocaleString()}
            </span>
          </div>
        </div>
        <div className="leaderboard-expand-icon">
          {isExpanded ? '▼' : '▶'}
        </div>
      </div>

      <AnimatePresence>
        {isExpanded && (
          <motion.div
            className="leaderboard-scores"
            initial={{ height: 0, opacity: 0 }}
            animate={{ height: 'auto', opacity: 1 }}
            exit={{ height: 0, opacity: 0 }}
            transition={{ duration: 0.3 }}
          >
            {scores.length === 0 ? (
              <div className="no-scores">No scores yet! Be the first!</div>
            ) : (
              scores.slice(0, 5).map((entry, i) => (
                <motion.div
                  key={i}
                  className={`score-entry rank-${i + 1}`}
                  initial={{ opacity: 0, x: -20 }}
                  animate={{ opacity: 1, x: 0 }}
                  transition={{ delay: i * 0.1 }}
                >
                  <div className="score-rank">
                    {i === 0 && <span className="medal gold">🥇</span>}
                    {i === 1 && <span className="medal silver">🥈</span>}
                    {i === 2 && <span className="medal bronze">🥉</span>}
                    {i > 2 && <span className="rank-number">#{i + 1}</span>}
                  </div>
                  <span className="score-name">{entry.playerName}</span>
                  <span className="score-value">{entry.score.toLocaleString()}</span>
                </motion.div>
              ))
            )}
          </motion.div>
        )}
      </AnimatePresence>
    </motion.div>
  );
});

LeaderboardCard.displayName = 'LeaderboardCard';

const Leaderboard = ({ compact = false }) => {
  const [leaderboards, setLeaderboards] = useState({});
  const [expandedGame, setExpandedGame] = useState(null);
  const [showAll, setShowAll] = useState(false);

  useEffect(() => {
    const data = getLeaderboardData();
    setLeaderboards(data);

    // Listen for storage changes
    const handleStorage = () => {
      setLeaderboards(getLeaderboardData());
    };

    window.addEventListener('storage', handleStorage);
    return () => window.removeEventListener('storage', handleStorage);
  }, []);

  // Convert to array and sort by top score
  const sortedGames = Object.entries(leaderboards)
    .map(([id, data]) => ({
      id,
      name: data.name,
      scores: data.scores || []
    }))
    .filter(game => game.scores.length > 0)
    .sort((a, b) => (b.scores[0]?.score || 0) - (a.scores[0]?.score || 0));

  const displayGames = compact && !showAll ? sortedGames.slice(0, 3) : sortedGames;

  if (sortedGames.length === 0) {
    return (
      <div className="leaderboard-section">
        <div className="leaderboard-header">
          <h3 className="leaderboard-title">
            <span className="title-icon">🏆</span>
            Leaderboards
            <span className="title-icon">🏆</span>
          </h3>
        </div>
        <div className="leaderboard-empty">
          <div className="empty-icon">🎮</div>
          <p>No high scores yet!</p>
          <span>Play games to set records</span>
        </div>
      </div>
    );
  }

  return (
    <div className="leaderboard-section">
      <div className="leaderboard-header">
        <h3 className="leaderboard-title">
          <span className="title-icon">🏆</span>
          Leaderboards
          <span className="title-icon">🏆</span>
        </h3>
        <p className="leaderboard-subtitle">Your best scores across all games</p>
      </div>

      <div className="leaderboard-grid">
        {displayGames.map((game, index) => (
          <LeaderboardCard
            key={game.id}
            gameId={game.id}
            gameName={game.name}
            scores={game.scores}
            index={index}
            isExpanded={expandedGame === game.id}
            onToggle={() => setExpandedGame(expandedGame === game.id ? null : game.id)}
          />
        ))}
      </div>

      {compact && sortedGames.length > 3 && (
        <motion.button
          className="show-all-btn"
          onClick={() => setShowAll(!showAll)}
          whileHover={{ scale: 1.02 }}
          whileTap={{ scale: 0.98 }}
        >
          {showAll ? 'Show Less' : `View All (${sortedGames.length} games)`}
        </motion.button>
      )}
    </div>
  );
};

export default Leaderboard;
