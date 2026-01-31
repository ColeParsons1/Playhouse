import { useState, useEffect, useCallback, lazy, Suspense } from 'react';
import { AnimatePresence } from 'framer-motion';
import { playerApi, authApi } from './api/client';
import ThemeProvider from './components/ThemeProvider';
import FloatingAnimals from './components/FloatingAnimals';
import BottomNav from './components/BottomNav';
import VoiceRoomsLobby from './components/VoiceRoomsLobby';
import VoiceRoomGame from './components/VoiceRoomGame';
import AuthPage from './components/AuthPage';
import './App.css';

// AnniversaryCard removed - Tanvi loved it!

// Lazy load screens and modals
const GameHub = lazy(() => import('./components/GameHub'));
const ShopScreen = lazy(() => import('./components/ShopScreen'));
const AvatarEditor = lazy(() => import('./components/AvatarEditor'));
const StatsModal = lazy(() => import('./components/StatsModal'));
const CreateRoomModal = lazy(() => import('./components/CreateRoomModal'));
const FollowScreen = lazy(() => import('./components/FollowScreen'));

// Loading component
const ScreenLoading = () => (
  <div className="screen-loading">
    <div className="loading-spinner"></div>
    <p>Loading...</p>
  </div>
);

// Session storage key
const SESSION_KEY = 'playhouse_session';

function App() {
  const [player, setPlayer] = useState(null);
  const [loading, setLoading] = useState(true);
  const [authLoading, setAuthLoading] = useState(false);
  const [isAuthenticated, setIsAuthenticated] = useState(false);
  const [activeTab, setActiveTab] = useState('rooms');
  const [showShop, setShowShop] = useState(false);
  const [showAvatar, setShowAvatar] = useState(false);
  const [showStats, setShowStats] = useState(false);
  const [showCreateRoom, setShowCreateRoom] = useState(false);
  const [showFollow, setShowFollow] = useState(false);
  const [currentRoom, setCurrentRoom] = useState(null);
  const [inGame, setInGame] = useState(false);
  const [activeTheme, setActiveTheme] = useState('default');

  useEffect(() => {
    initializeApp();
  }, []);

  const initializeApp = async () => {
    // Load saved theme from localStorage first
    const savedTheme = localStorage.getItem('active_theme');
    if (savedTheme) {
      setActiveTheme(savedTheme);
    }

    // Check for existing session
    const savedSession = localStorage.getItem(SESSION_KEY);
    if (savedSession) {
      try {
        const sessionData = JSON.parse(savedSession);
        if (sessionData?.player?.id) {
          // Verify player still exists by fetching fresh data
          const response = await playerApi.getWithAvatar(sessionData.player.id);
          setPlayer(response.data);
          setIsAuthenticated(true);

          // Update theme if player has one saved
          const playerTheme = localStorage.getItem(`active_theme_${response.data.id}`);
          if (playerTheme) {
            setActiveTheme(playerTheme);
          }
        }
      } catch (error) {
        console.error('Session invalid, clearing:', error);
        localStorage.removeItem(SESSION_KEY);
      }
    }

    setLoading(false);
  };

  const handleAuth = async (mode, formData) => {
    setAuthLoading(true);

    try {
      let response;

      if (mode === 'guest') {
        // Guest mode - create a temporary player or use Tanvi
        response = await playerApi.getOrCreateTanvi();
        setPlayer(response.data);
        setIsAuthenticated(true);
        // Don't save guest session
        return;
      }

      if (mode === 'login') {
        response = await authApi.login(formData.email, formData.password);
      } else if (mode === 'register') {
        response = await authApi.register(formData.name, formData.email, formData.password);
      }

      if (response.data.success) {
        const playerData = response.data.player;
        setPlayer(playerData);
        setIsAuthenticated(true);

        // Save session
        localStorage.setItem(SESSION_KEY, JSON.stringify({
          player: playerData,
          timestamp: Date.now(),
        }));

        // Load player theme
        const playerTheme = localStorage.getItem(`active_theme_${playerData.id}`);
        if (playerTheme) {
          setActiveTheme(playerTheme);
        }
      } else {
        throw new Error(response.data.error || 'Authentication failed');
      }
    } catch (error) {
      const message = error.response?.data?.error || error.message || 'Something went wrong';
      throw new Error(message);
    } finally {
      setAuthLoading(false);
    }
  };

  const handleLogout = useCallback(() => {
    localStorage.removeItem(SESSION_KEY);
    setPlayer(null);
    setIsAuthenticated(false);
    setCurrentRoom(null);
    setInGame(false);
  }, []);

  const handleThemeChange = useCallback((themeName) => {
    setActiveTheme(themeName);
    if (player?.id) {
      localStorage.setItem(`active_theme_${player.id}`, themeName);
    }
  }, [player?.id]);

  const handlePlayerUpdate = useCallback((updatedPlayer) => {
    setPlayer(updatedPlayer);
    // Update session with new player data
    const savedSession = localStorage.getItem(SESSION_KEY);
    if (savedSession) {
      const sessionData = JSON.parse(savedSession);
      sessionData.player = updatedPlayer;
      localStorage.setItem(SESSION_KEY, JSON.stringify(sessionData));
    }
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

  // Show loading screen
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

  // Show auth page if not authenticated
  if (!isAuthenticated) {
    return <AuthPage onLogin={handleAuth} isLoading={authLoading} />;
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
      case 'follow':
        return (
          <Suspense fallback={<ScreenLoading />}>
            <FollowScreen
              player={player}
              onClose={() => setActiveTab('rooms')}
            />
          </Suspense>
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
                onLogout={handleLogout}
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
