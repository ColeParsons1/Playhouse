import { useState, useEffect, useCallback, lazy, Suspense } from 'react';
import { AnimatePresence } from 'framer-motion';
import { playerApi } from './api/client';
import ThemeProvider from './components/ThemeProvider';
import FloatingAnimals from './components/FloatingAnimals';
import BottomNav from './components/BottomNav';
import VoiceRoomsLobby from './components/VoiceRoomsLobby';
import VoiceRoomGame from './components/VoiceRoomGame';
import './App.css';

// AnniversaryCard removed - Tanvi loved it! 💕

// Lazy load screens and modals
const GameHub = lazy(() => import('./components/GameHub'));
const ShopScreen = lazy(() => import('./components/ShopScreen'));
const AvatarEditor = lazy(() => import('./components/AvatarEditor'));
const StatsModal = lazy(() => import('./components/StatsModal'));
const CreateRoomModal = lazy(() => import('./components/CreateRoomModal'));

// Loading component
const ScreenLoading = () => (
  <div className="screen-loading">
    <div className="loading-spinner"></div>
    <p>Loading...</p>
  </div>
);

function App() {
  const [player, setPlayer] = useState(null);
  const [loading, setLoading] = useState(true);
  const [activeTab, setActiveTab] = useState('rooms');
  const [showShop, setShowShop] = useState(false);
  const [showAvatar, setShowAvatar] = useState(false);
  const [showStats, setShowStats] = useState(false);
  const [showCreateRoom, setShowCreateRoom] = useState(false);
  const [currentRoom, setCurrentRoom] = useState(null);
  const [inGame, setInGame] = useState(false);
  const [activeTheme, setActiveTheme] = useState('default');

  useEffect(() => {
    initializePlayer();
  }, []);

  const initializePlayer = async () => {
    // Load saved theme from localStorage first
    const savedTheme = localStorage.getItem('active_theme');
    if (savedTheme) {
      setActiveTheme(savedTheme);
    }

    // Stop loading immediately so users can play games
    setLoading(false);

    // Try to get player data in background (optional)
    try {
      const response = await playerApi.getOrCreateTanvi();
      setPlayer(response.data);

      // Update theme if player has one saved
      if (response.data?.id) {
        const playerTheme = localStorage.getItem(`active_theme_${response.data.id}`);
        if (playerTheme) {
          setActiveTheme(playerTheme);
        }
      }
    } catch (error) {
      console.error('Failed to initialize player (playing as guest):', error);
      // Player stays null - guest mode
    }
  };

  const handleThemeChange = useCallback((themeName) => {
    setActiveTheme(themeName);
    if (player?.id) {
      localStorage.setItem(`active_theme_${player.id}`, themeName);
    }
  }, [player?.id]);

  const handlePlayerUpdate = useCallback((updatedPlayer) => {
    setPlayer(updatedPlayer);
  }, []);

  const handleTabChange = useCallback((tab) => {
    if (tab === 'profile') {
      setShowAvatar(true);
    } else {
      setActiveTab(tab);
    }
  }, []);

  const handleJoinRoom = useCallback((room) => {
    setCurrentRoom(room);
    setInGame(true);
  }, []);

  const handleLeaveRoom = useCallback(() => {
    setCurrentRoom(null);
    setInGame(false);
  }, []);

  const handleCreateRoom = useCallback(() => {
    setShowCreateRoom(true);
  }, []);

  const handleRoomCreated = useCallback((room) => {
    setShowCreateRoom(false);
    setCurrentRoom(room);
    setInGame(true);
  }, []);

  if (loading) {
    return (
      <div className="app-loading">
        <div className="loading-content">
          <div className="loading-hearts">
            <span className="heart">💖</span>
            <span className="heart delay-1">💖</span>
            <span className="heart delay-2">💖</span>
          </div>
          <h2 className="loading-text">Loading your game...</h2>
        </div>
      </div>
    );
  }

  const renderScreen = () => {
    switch (activeTab) {
      case 'rooms':
        return (
          <VoiceRoomsLobby
            player={player}
            onJoinRoom={handleJoinRoom}
            onCreateRoom={handleCreateRoom}
          />
        );
      case 'games':
        return (
          <Suspense fallback={<ScreenLoading />}>
            <div className="games-screen">
              {/* Header for games screen */}
              <div className="games-header">
                <div className="player-info">
                  <div className="player-avatar-small" onClick={() => setShowAvatar(true)}>
                    {player?.name?.charAt(0) || 'T'}
                  </div>
                  <div className="player-details">
                    <span className="player-name">{player?.name || 'Player'}</span>
                    <span className="player-level">Lv. {player?.level || 1}</span>
                  </div>
                </div>
                <div className="header-actions">
                  <div className="currency-pill" onClick={() => setShowShop(true)}>
                    <span>💎 {player?.gems || 0}</span>
                    <span>🪙 {player?.coins?.toLocaleString() || 0}</span>
                  </div>
                </div>
              </div>

              {/* Room info if in a room */}
              {currentRoom && (
                <div className="current-room-banner">
                  <span className="room-emoji">🎮</span>
                  <span className="room-name">{currentRoom.name}</span>
                  <span className="room-players">👤 {currentRoom.player_count || 0}</span>
                </div>
              )}

              <GameHub
                player={player}
                onPlayerUpdate={handlePlayerUpdate}
              />
            </div>
          </Suspense>
        );
      case 'messages':
        return (
          <div className="coming-soon-screen">
            <span className="coming-soon-icon">💬</span>
            <h2>Messages</h2>
            <p>Coming soon!</p>
          </div>
        );
      case 'create':
        return (
          <div className="coming-soon-screen">
            <span className="coming-soon-icon">🎤</span>
            <h2>Create Room</h2>
            <p>Coming soon!</p>
          </div>
        );
      default:
        return null;
    }
  };

  return (
    <ThemeProvider theme={activeTheme}>
      <div className="app">
        {/* Voice Room Game - shows when player is in a room */}
        {inGame && currentRoom && (
          <VoiceRoomGame
            room={currentRoom}
            player={player}
            onLeave={handleLeaveRoom}
          />
        )}

        {/* Regular App UI when not in game */}
        {!inGame && (
          <>
            {/* Floating animals background (only show on some screens) */}
            {activeTab === 'games' && <FloatingAnimals />}

            {/* Main content */}
            <div className="app-content">
              {renderScreen()}
            </div>

            {/* Bottom Navigation */}
            <BottomNav activeTab={activeTab} onTabChange={handleTabChange} />
          </>
        )}

        {/* Modals */}
        <AnimatePresence>
          {showShop && (
            <Suspense fallback={null}>
              <ShopScreen
                player={player}
                onClose={() => setShowShop(false)}
                onPurchase={handlePlayerUpdate}
              />
            </Suspense>
          )}
        </AnimatePresence>

        <AnimatePresence>
          {showAvatar && (
            <Suspense fallback={null}>
              <AvatarEditor
                player={player}
                onClose={() => setShowAvatar(false)}
                onSave={(avatar) => {
                  setPlayer(prev => ({ ...prev, avatar }));
                  setShowAvatar(false);
                }}
                onThemeChange={handleThemeChange}
              />
            </Suspense>
          )}
        </AnimatePresence>

        <AnimatePresence>
          {showStats && (
            <Suspense fallback={null}>
              <StatsModal
                isOpen={showStats}
                onClose={() => setShowStats(false)}
                player={player}
              />
            </Suspense>
          )}
        </AnimatePresence>

        <AnimatePresence>
          {showCreateRoom && (
            <Suspense fallback={null}>
              <CreateRoomModal
                player={player}
                onClose={() => setShowCreateRoom(false)}
                onCreated={handleRoomCreated}
              />
            </Suspense>
          )}
        </AnimatePresence>
      </div>
    </ThemeProvider>
  );
}

export default App;
