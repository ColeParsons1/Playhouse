import React, { useState, lazy, Suspense, memo, useCallback, useMemo } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { playerApi } from '../api/client';
import Leaderboard, { saveHighScore, getHighScore } from './Leaderboard';
import GameIconMap from './GameIcons';
import './GameHub.css';

// Lazy load all games for better initial load performance
const BingoGame = lazy(() => import('./BingoGame'));
const PresentCatchGame = lazy(() => import('./PresentCatchGame'));
const TempleRunGame = lazy(() => import('./TempleRunGame'));
const FlappyBirdGame = lazy(() => import('./FlappyBirdGame'));
const TowerDefenseGame = lazy(() => import('./TowerDefenseGame'));
const BilliardsGame = lazy(() => import('./BilliardsGame'));
const TanksGame = lazy(() => import('./TanksGame'));
const ASLGuessGame = lazy(() => import('./ASLGuessGame'));
const GooBridgeGame = lazy(() => import('./GooBridgeGame'));
const AngryDucksGame = lazy(() => import('./AngryDucksGame'));
const ColorCardsGame = lazy(() => import('./ColorCardsGame'));
const SlotMachineGame = lazy(() => import('./SlotMachineGame'));
const OvercookedGame = lazy(() => import('./OvercookedGame'));
const Connect4Game = lazy(() => import('./Connect4Game'));
const LudoGame = lazy(() => import('./LudoGame'));
const CarromGame = lazy(() => import('./CarromGame'));
const JengaGame = lazy(() => import('./JengaGame'));
const PegGame = lazy(() => import('./PegGame'));
const PongGame = lazy(() => import('./PongGame'));
const MapsGame = lazy(() => import('./MapsGame'));
const CornholeGame = lazy(() => import('./CornholeGame'));

// McDonald's PlayPlace color palette - vibrant & saturated
const PLAYPLACE_COLORS = {
  red: '#FF2D20',      // Vibrant red
  yellow: '#FFD000',   // Bright golden yellow
  orange: '#FF6B00',   // Vivid orange
  blue: '#0088FF',     // Bright blue
  green: '#00D45F',    // Vivid green
  purple: '#9B30FF',   // Bright purple
};

const GAMES = [
  {
    id: 'bingo',
    name: 'Bingo',
    emoji: '🎱',
    description: 'Classic bingo vs CPU!',
    color: PLAYPLACE_COLORS.purple,
    featured: true,
  },
  {
    id: 'presents',
    name: 'Pack',
    emoji: '📦',
    description: 'Catch falling packages!',
    color: PLAYPLACE_COLORS.blue,
  },
  {
    id: 'runner',
    name: 'Temple Run',
    emoji: '🏃',
    description: 'Endless runner adventure!',
    color: PLAYPLACE_COLORS.green,
  },
  {
    id: 'flappy',
    name: 'Baby Shark',
    emoji: '🦈',
    description: 'Swim through the ocean!',
    color: PLAYPLACE_COLORS.blue,
  },
  {
    id: 'tower',
    name: 'Tower Defense',
    emoji: '🏰',
    description: 'Build towers, stop the waves!',
    color: PLAYPLACE_COLORS.green,
  },
  {
    id: 'billiards',
    name: 'Pool',
    emoji: '🎱',
    description: 'Pocket balls vs CPU!',
    color: PLAYPLACE_COLORS.green,
  },
  {
    id: 'tanks',
    name: 'Tanks',
    emoji: '🎯',
    description: 'Artillery battle vs CPU!',
    color: PLAYPLACE_COLORS.blue,
  },
  {
    id: 'asl',
    name: 'Guess the Sign',
    emoji: '🤟',
    description: 'Learn ASL letters!',
    color: PLAYPLACE_COLORS.yellow,
  },
  {
    id: 'goo',
    name: 'Goo Bridge',
    emoji: '🟢',
    description: 'Build bridges with goo!',
    color: PLAYPLACE_COLORS.green,
  },
  {
    id: 'ducks',
    name: 'Angry Ducks',
    emoji: '🦆',
    description: 'Launch ducks at piggies!',
    color: PLAYPLACE_COLORS.yellow,
  },
  {
    id: 'cards',
    name: 'Color Cards',
    emoji: '🃏',
    description: 'Uno-style card game!',
    color: PLAYPLACE_COLORS.red,
  },
  {
    id: 'slots',
    name: 'Lucky Slots',
    emoji: '🎰',
    description: 'Spin to win coins!',
    color: PLAYPLACE_COLORS.purple,
  },
  {
    id: 'kitchen',
    name: 'Kitchen Chaos',
    emoji: '👨‍🍳',
    description: 'Cook orders fast!',
    color: PLAYPLACE_COLORS.orange,
  },
  {
    id: 'connect4',
    name: 'Connect 4',
    emoji: '🔴',
    description: 'Four in a row!',
    color: PLAYPLACE_COLORS.red,
  },
  {
    id: 'ludo',
    name: 'Ludo',
    emoji: '🎲',
    description: 'Classic board game!',
    color: PLAYPLACE_COLORS.purple,
  },
  {
    id: 'carrom',
    name: 'Carrom',
    emoji: '⚪',
    description: 'Strike and pocket!',
    color: PLAYPLACE_COLORS.orange,
  },
  {
    id: 'jenga',
    name: 'Jenga',
    emoji: '🧱',
    description: "Don't topple it!",
    color: PLAYPLACE_COLORS.yellow,
  },
  {
    id: 'pegs',
    name: 'Peg Game',
    emoji: '🔺',
    description: 'Jump to leave one!',
    color: PLAYPLACE_COLORS.orange,
  },
  {
    id: 'pong',
    name: 'Pong',
    emoji: '🏓',
    description: 'Classic Atari arcade!',
    color: PLAYPLACE_COLORS.blue,
  },
  {
    id: 'maps',
    name: 'Maps',
    emoji: '🗺️',
    description: 'Name that country!',
    color: PLAYPLACE_COLORS.green,
  },
  {
    id: 'cornhole',
    name: 'Cornhole',
    emoji: '🌽',
    description: 'Toss bags for points!',
    color: PLAYPLACE_COLORS.orange,
  },
];

// Loading fallback component
const GameLoading = () => (
  <div className="game-loading-overlay">
    <div className="game-loading-content">
      <div className="loading-spinner"></div>
      <p>Loading game...</p>
    </div>
  </div>
);

// Memoized game card component to prevent re-renders
const GameCard = memo(({ game, onSelect }) => {
  const IconComponent = GameIconMap[game.id];
  return (
    <motion.button
      className="game-card"
      onClick={() => onSelect(game.id)}
      style={{ '--game-color': game.color }}
      whileHover={{ scale: 1.05 }}
      whileTap={{ scale: 0.95 }}
    >
      <div className="game-icon-wrapper">
        {IconComponent ? <IconComponent size={105} /> : <span className="game-emoji-fallback">{game.emoji}</span>}
      </div>
      <span className="game-name">{game.name}</span>
    </motion.button>
  );
});

GameCard.displayName = 'GameCard';

const GameHub = ({ player, onPlayerUpdate }) => {
  const [activeGame, setActiveGame] = useState(null);
  const [showReward, setShowReward] = useState(false);
  const [reward, setReward] = useState(null);
  const [leaderboardKey, setLeaderboardKey] = useState(0);

  const handleSelectGame = useCallback((gameId) => {
    setActiveGame(gameId);
  }, []);

  // Use refs for stable values to prevent unnecessary re-renders
  const playerIdRef = React.useRef(player?.id);
  const playerNameRef = React.useRef(player?.name);
  const onPlayerUpdateRef = React.useRef(onPlayerUpdate);

  // Update refs when values change
  React.useEffect(() => {
    playerIdRef.current = player?.id;
    playerNameRef.current = player?.name;
    onPlayerUpdateRef.current = onPlayerUpdate;
  }, [player?.id, player?.name, onPlayerUpdate]);

  const handleGameEnd = useCallback(async (earnedCoins) => {
    const currentGame = activeGame;
    setActiveGame(null);

    if (earnedCoins > 0) {
      // Save to leaderboard
      const gameData = GAMES.find(g => g.id === currentGame);
      if (gameData) {
        const previousHigh = getHighScore(currentGame);
        saveHighScore(currentGame, gameData.name, earnedCoins, playerNameRef.current || 'Guest');
        const isNewRecord = earnedCoins > previousHigh;

        // Force leaderboard refresh
        setLeaderboardKey(k => k + 1);

        // Only try to save coins if player is logged in
        if (playerIdRef.current) {
          try {
            // Add coins to player
            const response = await playerApi.addCoins(playerIdRef.current, earnedCoins);
            setReward({ coins: earnedCoins, isNewRecord, gameName: gameData.name });
            setShowReward(true);
            onPlayerUpdateRef.current?.(response.data);
          } catch (error) {
            console.error('Failed to add coins:', error);
            // Still show the reward even if API fails
            setReward({ coins: earnedCoins, isNewRecord, gameName: gameData.name });
            setShowReward(true);
          }
        } else {
          // Guest mode - just show the reward without saving
          setReward({ coins: earnedCoins, isNewRecord, gameName: gameData.name });
          setShowReward(true);
        }
      }
    }
  }, [activeGame]); // Only depends on activeGame now - more stable

  const handleCloseGame = useCallback(() => {
    setActiveGame(null);
  }, []);

  const closeReward = useCallback(() => {
    setShowReward(false);
    setReward(null);
  }, []);

  // Render the active game
  const renderGame = () => {
    const gameProps = {
      onGameEnd: handleGameEnd,
      onClose: handleCloseGame,
      player: player,
      onPlayerUpdate: onPlayerUpdate,
    };

    const GameComponent = {
      bingo: BingoGame,
      presents: PresentCatchGame,
      runner: TempleRunGame,
      flappy: FlappyBirdGame,
      tower: TowerDefenseGame,
      billiards: BilliardsGame,
      tanks: TanksGame,
      asl: ASLGuessGame,
      goo: GooBridgeGame,
      ducks: AngryDucksGame,
      cards: ColorCardsGame,
      slots: SlotMachineGame,
      kitchen: OvercookedGame,
      connect4: Connect4Game,
      ludo: LudoGame,
      carrom: CarromGame,
      jenga: JengaGame,
      pegs: PegGame,
      pong: PongGame,
      maps: MapsGame,
      cornhole: CornholeGame,
    }[activeGame];

    if (!GameComponent) return null;

    return (
      <Suspense fallback={<GameLoading />}>
        <GameComponent {...gameProps} />
      </Suspense>
    );
  };

  // Memoize game lists to prevent recalculation on every render
  const featuredGame = useMemo(() => GAMES.find(g => g.featured), []);
  const otherGames = useMemo(() => GAMES.filter(g => !g.featured), []);

  return (
    <div className="game-hub">
      <div className="game-hub-header">
        <h2 className="hub-title">
          <span>🎮</span> Choose a Game <span>🎮</span>
        </h2>
        <p className="hub-subtitle">Play games to earn coins!</p>
      </div>

      {/* Featured Game */}
      {featuredGame && (
        <motion.button
          className="featured-game-card"
          onClick={() => handleSelectGame(featuredGame.id)}
          style={{ '--game-color': featuredGame.color }}
          whileHover={{ scale: 1.02 }}
          whileTap={{ scale: 0.98 }}
        >
          <div className="featured-badge">Featured</div>
          <div className="featured-icon-wrapper">
            {GameIconMap[featuredGame.id] ?
              React.createElement(GameIconMap[featuredGame.id], { size: 135 }) :
              <span className="featured-emoji-fallback">{featuredGame.emoji}</span>
            }
          </div>
          <div className="featured-info">
            <span className="featured-name">{featuredGame.name}</span>
            <span className="featured-desc">{featuredGame.description}</span>
          </div>
          <span className="play-arrow">▶</span>
        </motion.button>
      )}

      {/* Game Grid */}
      <div className="games-grid">
        {otherGames.map((game) => (
          <GameCard key={game.id} game={game} onSelect={handleSelectGame} />
        ))}
      </div>

      {/* Leaderboards Section */}
      <Leaderboard key={leaderboardKey} compact={true} />

      {/* Active Game */}
      <AnimatePresence>
        {activeGame && renderGame()}
      </AnimatePresence>

      {/* Reward Modal */}
      <AnimatePresence>
        {showReward && reward && (
          <motion.div
            className="reward-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            onClick={closeReward}
          >
            <motion.div
              className={`reward-modal ${reward.isNewRecord ? 'new-record' : ''}`}
              initial={{ scale: 0, rotate: -10 }}
              animate={{ scale: 1, rotate: 0 }}
              exit={{ scale: 0, rotate: 10 }}
              onClick={(e) => e.stopPropagation()}
            >
              {reward.isNewRecord && (
                <motion.div
                  className="new-record-banner"
                  initial={{ y: -20, opacity: 0 }}
                  animate={{ y: 0, opacity: 1 }}
                  transition={{ delay: 0.2 }}
                >
                  <span>🏆</span> NEW HIGH SCORE! <span>🏆</span>
                </motion.div>
              )}

              <div className="reward-header">
                <span className="reward-crown">{reward.isNewRecord ? '👑' : '🎉'}</span>
                <h2>{reward.isNewRecord ? 'Amazing!' : 'Coins Earned!'}</h2>
              </div>

              {reward.gameName && (
                <motion.div
                  className="reward-game-name"
                  initial={{ opacity: 0 }}
                  animate={{ opacity: 1 }}
                  transition={{ delay: 0.2 }}
                >
                  {reward.gameName}
                </motion.div>
              )}

              <motion.div
                className="reward-amount"
                initial={{ scale: 0 }}
                animate={{ scale: 1 }}
                transition={{ delay: 0.3, type: 'spring' }}
              >
                <span className="coin-icon">🪙</span>
                <span className="coin-value">+{reward.coins}</span>
              </motion.div>

              <motion.button
                className="reward-close-btn"
                onClick={closeReward}
                initial={{ opacity: 0 }}
                animate={{ opacity: 1 }}
                transition={{ delay: 0.6 }}
              >
                {reward.isNewRecord ? 'Legendary!' : 'Awesome!'}
              </motion.button>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
};

export default GameHub;
