import { useState } from 'react';
import './MinigameSelector.css';

const MINIGAMES = [
  {
    id: 'bingo',
    name: 'Bingo',
    emoji: '🎱',
    description: 'Classic bingo vs CPU!',
    color: '#764ba2',
    featured: true,
  },
  {
    id: 'presents',
    name: 'Purple Nurple',
    emoji: '🎁',
    description: 'Catch glowing presents!',
    color: '#1e40af',
  },
  {
    id: 'bubbles',
    name: 'Bubble Match',
    emoji: '💖',
    description: 'Match the bubbles fast!',
    color: '#667eea',
  },
  {
    id: 'scratch',
    name: 'Lucky Scratch',
    emoji: '🎫',
    description: 'Scratch to reveal prizes!',
    color: '#FFD700',
  },
  {
    id: 'cascade',
    name: 'Coin Cascade',
    emoji: '🪙',
    description: 'Catch coins in your basket!',
    color: '#FFA500',
  },
  {
    id: 'stars',
    name: 'Star Collector',
    emoji: '⭐',
    description: 'Tap stars before they fade!',
    color: '#9370DB',
  },
  {
    id: 'memory',
    name: 'Memory Flash',
    emoji: '🧠',
    description: 'Match pairs from memory!',
    color: '#667eea',
  },
  {
    id: 'colors',
    name: 'Color Splash',
    emoji: '🎨',
    description: 'Tap only matching colors!',
    color: '#FF69B4',
  },
  {
    id: 'rhythm',
    name: 'Rhythm Tap',
    emoji: '🎵',
    description: 'Hit beats on the line!',
    color: '#00CED1',
  },
  {
    id: 'float',
    name: 'Bubble Float',
    emoji: '🫧',
    description: 'Pop bubbles before they escape!',
    color: '#87CEEB',
  },
  {
    id: 'quickdraw',
    name: 'Quick Draw',
    emoji: '⚡',
    description: 'Test your reaction speed!',
    color: '#FFD700',
  },
  {
    id: 'runner',
    name: 'Temple Run',
    emoji: '🏃',
    description: 'Endless runner adventure!',
    color: '#00CED1',
  },
  {
    id: 'flappy',
    name: 'Baby Shark',
    emoji: '🦈',
    description: 'Swim through the ocean!',
    color: '#0077B6',
  },
  {
    id: 'tower',
    name: 'Tower Defense',
    emoji: '🏰',
    description: 'Build towers, stop the waves!',
    color: '#4CAF50',
  },
  {
    id: 'billiards',
    name: 'Pool',
    emoji: '🎱',
    description: 'Pocket balls like a pro!',
    color: '#228B22',
  },
  {
    id: 'tanks',
    name: 'Tanks',
    emoji: '🎯',
    description: 'Artillery battle vs CPU!',
    color: '#4a9fff',
  },
];

const MinigameSelector = ({ onSelect, onClose }) => {
  const [selectedGame, setSelectedGame] = useState(null);

  const handleSelect = (gameId) => {
    setSelectedGame(gameId);
  };

  const handlePlay = () => {
    if (selectedGame) {
      onSelect(selectedGame);
    }
  };

  return (
    <div className="minigame-selector-overlay">
      <div className="minigame-selector-container">
        <button className="selector-close-btn" onClick={onClose}>
          ✕
        </button>

        <div className="selector-header">
          <h2 className="selector-title">
            <span>🎮</span> Choose Your Game <span>🎮</span>
          </h2>
          <p className="selector-subtitle">Pick a minigame to earn bonus coins!</p>
        </div>

        <div className="minigame-grid">
          {MINIGAMES.map((game) => (
            <button
              key={game.id}
              className={`minigame-card ${selectedGame === game.id ? 'selected' : ''}`}
              onClick={() => handleSelect(game.id)}
              style={{ '--game-color': game.color }}
            >
              <span className="minigame-emoji">{game.emoji}</span>
              <span className="minigame-name">{game.name}</span>
              <span className="minigame-desc">{game.description}</span>
            </button>
          ))}
        </div>

        <button
          className={`play-btn cute-button ${selectedGame ? 'ready' : 'disabled'}`}
          onClick={handlePlay}
          disabled={!selectedGame}
        >
          {selectedGame ? "Let's Play! 🚀" : 'Select a Game'}
        </button>
      </div>
    </div>
  );
};

export default MinigameSelector;
