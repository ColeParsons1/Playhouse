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
      data-room-type={room.room_type || 'music'}
      data-game-mode={room.game_mode || 'bingo_normal'}
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
      const roomsData = response.data || [];
      setRooms(roomsData);
    } catch (error) {
      console.error('Failed to fetch rooms:', error);
      setRooms([]);
    } finally {
      setLoading(false);
    }
  }, [activeTab]);

  useEffect(() => {
    fetchRooms();
  }, [fetchRooms]);

  // Auto-refresh rooms every 10 seconds
  useEffect(() => {
    const interval = setInterval(() => {
      fetchRooms();
    }, 10000);
    return () => clearInterval(interval);
  }, [fetchRooms]);

  const handleJoinRoom = useCallback(async (room) => {
    // Call the join API if player exists
    if (player?.id && room.id) {
      try {
        await roomApi.join(room.id, player.id);
      } catch (error) {
        console.error('Failed to join room:', error);
        // Continue anyway - the WebSocket will handle the actual join
      }
    }
    onJoinRoom?.(room);
  }, [onJoinRoom, player?.id]);

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
