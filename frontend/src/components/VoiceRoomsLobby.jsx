import { useState, useEffect, useCallback, memo } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { roomApi } from '../api/client';
import './VoiceRoomsLobby.css';

const TABS = [
  { id: 'follow', label: 'Follow' },
  { id: 'popular', label: 'Popular' },
  { id: 'music', label: 'Music' },
  { id: 'video', label: 'Video' },
];

const GAME_MODE_LABELS = {
  bingo_normal: 'Bingo•Normal',
  bingo_powerup: 'Bingo•Power-up',
  bingo_blackout: 'Bingo•Blackout',
  connect4: 'Connect 4',
  ludo: 'Ludo',
  carrom: 'Carrom',
};

const FLAG_EMOJIS = {
  US: '🇺🇸',
  UK: '🇬🇧',
  CA: '🇨🇦',
  AU: '🇦🇺',
  IN: '🇮🇳',
  BR: '🇧🇷',
  MX: '🇲🇽',
  JP: '🇯🇵',
  KR: '🇰🇷',
  DE: '🇩🇪',
  FR: '🇫🇷',
};

// Room card component
const RoomCard = memo(({ room, onJoin }) => {
  const formatFollowers = (num) => {
    if (num >= 1000) return `${(num / 1000).toFixed(1)}K`;
    return num;
  };

  return (
    <motion.div
      className="room-card"
      onClick={() => onJoin(room)}
      whileHover={{ scale: 1.02 }}
      whileTap={{ scale: 0.98 }}
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
    >
      <div className="room-avatar">
        <div className="avatar-placeholder">
          {room.host_name?.charAt(0) || '?'}
        </div>
      </div>

      <div className="room-info">
        <div className="room-header-row">
          <span className="room-type-badge">{room.room_type === 'music' ? 'Music' : room.room_type}</span>
          <span className="room-name">{room.name}</span>
        </div>
        <div className="room-details">
          <span className="room-flag">{FLAG_EMOJIS[room.host_country] || '🌍'}</span>
          <span className="room-player-count">👤 {room.player_count || 0}</span>
          <span className="room-game-mode">🎮 {GAME_MODE_LABELS[room.game_mode] || room.game_mode}</span>
        </div>
      </div>

      {room.host_followers > 0 && (
        <div className="room-ranking">
          <span className="ranking-badge">⚡ {formatFollowers(room.host_followers)}</span>
        </div>
      )}
    </motion.div>
  );
});

RoomCard.displayName = 'RoomCard';

const VoiceRoomsLobby = ({ player, onJoinRoom, onCreateRoom }) => {
  const [activeTab, setActiveTab] = useState('popular');
  const [rooms, setRooms] = useState([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');

  const fetchRooms = useCallback(async () => {
    setLoading(true);
    try {
      const response = await roomApi.getLobby(activeTab);
      setRooms(response.data || []);
    } catch (error) {
      console.error('Failed to fetch rooms:', error);
      // Set some mock data for development
      setRooms([
        { id: 1, name: 'bingoooo🎰🎰', host_name: 'Bunny', host_country: 'US', player_count: 22, game_mode: 'bingo_normal', room_type: 'music', host_followers: 53800 },
        { id: 2, name: 'lottery bingooo🏠', host_name: 'Kitty', host_country: 'US', player_count: 36, game_mode: 'bingo_normal', room_type: 'music', host_followers: 7300 },
        { id: 3, name: 'Mini AI Comp', host_name: 'Panda', host_country: 'US', player_count: 18, game_mode: 'bingo_powerup', room_type: 'music', host_followers: 6600 },
        { id: 4, name: "Cerrrttyyy's 🏠", host_name: 'Fox', host_country: 'UK', player_count: 31, game_mode: 'bingo_blackout', room_type: 'friends', host_followers: 4200 },
        { id: 5, name: '(≧∪≦) ♡ GLITCHY ♡', host_name: 'Owl', host_country: 'CA', player_count: 7, game_mode: 'bingo_normal', room_type: 'music', host_followers: 1500 },
        { id: 6, name: '🎀🎀○ Bingo🎀🎀⭐⭐', host_name: 'Deer', host_country: 'UK', player_count: 24, game_mode: 'bingo_powerup', room_type: 'music', host_followers: 2800 },
      ]);
    } finally {
      setLoading(false);
    }
  }, [activeTab]);

  useEffect(() => {
    fetchRooms();
  }, [fetchRooms]);

  const handleJoinRoom = useCallback((room) => {
    onJoinRoom?.(room);
  }, [onJoinRoom]);

  const filteredRooms = rooms.filter(room =>
    room.name?.toLowerCase().includes(searchQuery.toLowerCase()) ||
    room.host_name?.toLowerCase().includes(searchQuery.toLowerCase())
  );

  return (
    <div className="voice-rooms-lobby">
      {/* Header */}
      <div className="lobby-header">
        <h1 className="lobby-title">Voice Rooms</h1>
        <div className="lobby-actions">
          <button className="action-btn favorite-btn">⭐</button>
          <button className="action-btn search-btn">🔍</button>
          <button className="action-btn create-btn" onClick={onCreateRoom}>+</button>
        </div>
      </div>

      {/* Banner */}
      <div className="lobby-banner">
        <div className="banner-content">
          <span className="banner-icon">🎮</span>
          <div className="banner-text">
            <h3>Join Voice Rooms</h3>
            <p>Play games with others & win big prizes!</p>
          </div>
        </div>
      </div>

      {/* Tabs */}
      <div className="lobby-tabs">
        {TABS.map((tab) => (
          <button
            key={tab.id}
            className={`tab-btn ${activeTab === tab.id ? 'active' : ''}`}
            onClick={() => setActiveTab(tab.id)}
          >
            {tab.label}
            {activeTab === tab.id && (
              <motion.div
                className="tab-indicator"
                layoutId="tabIndicator"
                transition={{ type: 'spring', stiffness: 500, damping: 30 }}
              />
            )}
          </button>
        ))}
        <button className="tab-btn globe-btn">🌐 ▼</button>
      </div>

      {/* Room List */}
      <div className="rooms-list">
        {loading ? (
          <div className="loading-rooms">
            <div className="loading-spinner"></div>
            <p>Loading rooms...</p>
          </div>
        ) : filteredRooms.length === 0 ? (
          <div className="no-rooms">
            <span className="no-rooms-icon">🎭</span>
            <p>No rooms found</p>
            <button className="create-room-btn" onClick={onCreateRoom}>
              Create a Room
            </button>
          </div>
        ) : (
          <AnimatePresence>
            {filteredRooms.map((room, index) => (
              <RoomCard
                key={room.id || index}
                room={room}
                onJoin={handleJoinRoom}
              />
            ))}
          </AnimatePresence>
        )}
      </div>
    </div>
  );
};

export default VoiceRoomsLobby;
