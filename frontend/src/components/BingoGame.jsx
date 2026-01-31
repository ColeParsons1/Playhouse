import { useState, useEffect, useCallback, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { gameApi } from '../api/client';
import BingoCard from './BingoCard';
import NumberCaller from './NumberCaller';
import './BingoGame.css';

// Mock players for voice room
const MOCK_PLAYERS = [
  { id: 1, name: 'Host', avatar: '👸', isHost: true, isSpeaking: true, progress: 0 },
  { id: 2, name: 'Player2', avatar: '🧑', isHost: false, isSpeaking: false, progress: 0 },
  { id: 3, name: 'GamerGirl', avatar: '👧', isHost: false, isSpeaking: true, progress: 0 },
  { id: 4, name: 'CoolDude', avatar: '🧔', isHost: false, isSpeaking: false, progress: 0 },
  { id: 5, name: 'StarPlayer', avatar: '👨', isHost: false, isSpeaking: false, progress: 0 },
  { id: 6, name: 'LuckyOne', avatar: '👩', isHost: false, isSpeaking: false, progress: 0 },
];

// Mock chat messages
const MOCK_CHAT = [
  { id: 1, player: 'Host', message: 'Welcome everyone! 🎉', isSystem: false },
  { id: 2, player: 'System', message: 'Game starting soon...', isSystem: true },
  { id: 3, player: 'GamerGirl', message: 'Good luck all!', isSystem: false },
  { id: 4, player: 'CoolDude', message: "Let's go! 🔥", isSystem: false },
];

const CALL_INTERVAL = 5000; // Call a number every 5 seconds
const COUNTDOWN_START = 10; // 10 second countdown before game starts

const BingoGame = ({ player, onGameEnd, onClose, onPlayerUpdate }) => {
  const [game, setGame] = useState(null);
  const [playerCards, setPlayerCards] = useState([]);
  const [isLoading, setIsLoading] = useState(true);
  const [isCallingNumber, setIsCallingNumber] = useState(false);
  const [gameOver, setGameOver] = useState(false);
  const [isPlayerWin, setIsPlayerWin] = useState(false);
  const [winData, setWinData] = useState(null);
  const [coinsEarned, setCoinsEarned] = useState(0);
  const [nearBingo, setNearBingo] = useState(false);
  const [matchedInRow, setMatchedInRow] = useState(0);
  const [showConfetti, setShowConfetti] = useState(false);
  const [lastMarkedNumber, setLastMarkedNumber] = useState(null);
  const [cpuProgress, setCpuProgress] = useState(0);

  // Countdown and auto-call state
  const [countdown, setCountdown] = useState(null);
  const [gameStarted, setGameStarted] = useState(false);
  const [nextCallIn, setNextCallIn] = useState(null);
  const autoCallRef = useRef(null);
  const nextCallTimerRef = useRef(null);

  // Voice room features
  const [players, setPlayers] = useState(MOCK_PLAYERS);
  const [chat, setChat] = useState(MOCK_CHAT);
  const [newMessage, setNewMessage] = useState('');
  const [showChat, setShowChat] = useState(false);
  const [isMuted, setIsMuted] = useState(false);
  const [prizePool] = useState(2500);
  const [selectedPlayer, setSelectedPlayer] = useState(null);

  const confettiRef = useRef(null);
  const chatMessagesRef = useRef(null);

  // Auto-scroll chat to bottom
  useEffect(() => {
    if (chatMessagesRef.current) {
      chatMessagesRef.current.scrollTop = chatMessagesRef.current.scrollHeight;
    }
  }, [chat]);

  // Simulate players joining/speaking and progress
  useEffect(() => {
    const interval = setInterval(() => {
      setPlayers(prev => prev.map(p => ({
        ...p,
        isSpeaking: Math.random() > 0.7,
        // Update progress randomly during game (simulating other players marking numbers)
        progress: gameStarted && !gameOver
          ? Math.min(5, p.progress + (Math.random() > 0.7 ? 1 : 0))
          : p.progress
      })));
    }, 3000);
    return () => clearInterval(interval);
  }, [gameStarted, gameOver]);

  // Simulate incoming chat messages
  useEffect(() => {
    const messages = [
      "Nice call! 🎯",
      "Almost got it!",
      "Good luck everyone!",
      "One more for bingo! 😱",
      "Let's goooo! 🚀",
      "So close!",
    ];
    const players = ['GamerGirl', 'CoolDude', 'StarPlayer', 'LuckyOne'];

    const interval = setInterval(() => {
      if (Math.random() > 0.6) {
        const randomPlayer = players[Math.floor(Math.random() * players.length)];
        const randomMessage = messages[Math.floor(Math.random() * messages.length)];
        setChat(prev => [...prev, {
          id: Date.now(),
          player: randomPlayer,
          message: randomMessage,
          isSystem: false
        }]);
      }
    }, 8000);
    return () => clearInterval(interval);
  }, []);

  const startNewGame = useCallback(async () => {
    if (!player?.id) return;

    // Clear any existing timers
    if (autoCallRef.current) clearInterval(autoCallRef.current);
    if (nextCallTimerRef.current) clearInterval(nextCallTimerRef.current);

    setIsLoading(true);
    try {
      const response = await gameApi.newGame(player.id);
      setGame(response.data);

      const myCards = response.data.cards.filter(
        card => card.player === player.id
      );
      setPlayerCards(myCards);
      setGameOver(false);
      setIsPlayerWin(false);
      setWinData(null);
      setCoinsEarned(0);
      setNearBingo(false);
      setMatchedInRow(0);
      setShowConfetti(false);
      setLastMarkedNumber(null);
      setCpuProgress(0);
      setGameStarted(false);
      setNextCallIn(null);
      setSelectedPlayer(null);

      // Reset all player progress
      setPlayers(prev => prev.map(p => ({ ...p, progress: 0 })));

      // Start countdown
      setCountdown(COUNTDOWN_START);

      // Add system message
      setChat(prev => [...prev, {
        id: Date.now(),
        player: 'System',
        message: 'Game starting in 10 seconds! Get ready! 🎲',
        isSystem: true
      }]);
    } catch (error) {
      console.error('Failed to start new game:', error);
    } finally {
      setIsLoading(false);
    }
  }, [player?.id]);

  useEffect(() => {
    startNewGame();

    // Cleanup on unmount
    return () => {
      if (autoCallRef.current) clearInterval(autoCallRef.current);
      if (nextCallTimerRef.current) clearInterval(nextCallTimerRef.current);
    };
  }, [startNewGame]);

  // Countdown timer effect
  useEffect(() => {
    if (countdown === null || countdown < 0) return;

    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(countdown - 1), 1000);
      return () => clearTimeout(timer);
    } else if (countdown === 0) {
      // Countdown finished - start the game!
      setGameStarted(true);
      setCountdown(null);
      setChat(prev => [...prev, {
        id: Date.now(),
        player: 'System',
        message: 'Game started! Numbers will be called automatically. Good luck! 🍀',
        isSystem: true
      }]);
    }
  }, [countdown]);

  // Auto-call numbers effect
  useEffect(() => {
    if (!gameStarted || gameOver || !game) return;

    // Function to call the next number
    const callNextNumber = async () => {
      if (gameOver) return;

      setIsCallingNumber(true);
      try {
        const response = await gameApi.callNumber(game.id);
        const data = response.data;

        setGame(prev => ({
          ...prev,
          current_number: data.number,
          called_numbers: data.called_numbers,
          status: data.game_over ? 'completed' : 'in_progress'
        }));

        setCpuProgress(prev => Math.min(5, prev + Math.floor(Math.random() * 2)));

        if (data.game_over) {
          setIsPlayerWin(false);
          setWinData({ winner: data.winner });
          setGameOver(true);
          setCoinsEarned(10);

          setChat(prev => [...prev, {
            id: Date.now(),
            player: 'System',
            message: `${data.winner} got BINGO! 🎉`,
            isSystem: true
          }]);

          // Clear auto-call timer
          if (autoCallRef.current) clearInterval(autoCallRef.current);
          if (nextCallTimerRef.current) clearInterval(nextCallTimerRef.current);
        }
      } catch (error) {
        console.error('Failed to call number:', error);
      } finally {
        setIsCallingNumber(false);
      }
    };

    // Call first number immediately
    callNextNumber();
    setNextCallIn(CALL_INTERVAL / 1000);

    // Set up interval for auto-calling
    autoCallRef.current = setInterval(callNextNumber, CALL_INTERVAL);

    // Set up countdown to next call
    nextCallTimerRef.current = setInterval(() => {
      setNextCallIn(prev => {
        if (prev <= 1) return CALL_INTERVAL / 1000;
        return prev - 1;
      });
    }, 1000);

    return () => {
      if (autoCallRef.current) clearInterval(autoCallRef.current);
      if (nextCallTimerRef.current) clearInterval(nextCallTimerRef.current);
    };
  }, [gameStarted, game?.id, gameOver]);

  const checkProgress = useCallback((card) => {
    const markedSet = new Set(card.marked?.map(pos => `${pos[0]}-${pos[1]}`) || []);
    markedSet.add('2-2');

    let maxInLine = 0;

    for (let row = 0; row < 5; row++) {
      const count = [0, 1, 2, 3, 4].filter(col => markedSet.has(`${row}-${col}`)).length;
      maxInLine = Math.max(maxInLine, count);
    }

    for (let col = 0; col < 5; col++) {
      const count = [0, 1, 2, 3, 4].filter(row => markedSet.has(`${row}-${col}`)).length;
      maxInLine = Math.max(maxInLine, count);
    }

    const diag1 = [0, 1, 2, 3, 4].filter(i => markedSet.has(`${i}-${i}`)).length;
    const diag2 = [0, 1, 2, 3, 4].filter(i => markedSet.has(`${i}-${4 - i}`)).length;
    maxInLine = Math.max(maxInLine, diag1, diag2);

    return maxInLine;
  }, []);

  const handleCallNumber = async () => {
    if (!game || isCallingNumber || gameOver) return;

    setIsCallingNumber(true);
    try {
      const response = await gameApi.callNumber(game.id);
      const data = response.data;

      setGame(prev => ({
        ...prev,
        current_number: data.number,
        called_numbers: data.called_numbers,
        status: data.game_over ? 'completed' : 'in_progress'
      }));

      setCpuProgress(prev => Math.min(5, prev + Math.floor(Math.random() * 2)));

      if (data.game_over) {
        setIsPlayerWin(false);
        setWinData({ winner: data.winner });
        setGameOver(true);
        setCoinsEarned(10);

        setChat(prev => [...prev, {
          id: Date.now(),
          player: 'System',
          message: `${data.winner} got BINGO! 🎉`,
          isSystem: true
        }]);
      }
    } catch (error) {
      console.error('Failed to call number:', error);
    } finally {
      setIsCallingNumber(false);
    }
  };

  const handleMarkNumber = async (cardId, number) => {
    if (!game || gameOver) return;

    try {
      const response = await gameApi.markNumber(game.id, cardId, number);
      if (response.data.marked) {
        setLastMarkedNumber(number);
        setTimeout(() => setLastMarkedNumber(null), 500);

        const updatedCards = playerCards.map(card =>
          card.id === cardId ? response.data.card : card
        );
        setPlayerCards(updatedCards);

        const maxProgress = Math.max(...updatedCards.map(checkProgress));
        setMatchedInRow(maxProgress);

        if (maxProgress >= 4 && !nearBingo) {
          setNearBingo(true);
          setChat(prev => [...prev, {
            id: Date.now(),
            player: 'System',
            message: `${player?.name || 'You'} is ONE away from BINGO! 🔥`,
            isSystem: true
          }]);
        }
      }
    } catch (error) {
      console.error('Failed to mark number:', error);
    }
  };

  const handleClaimBingo = async (cardId) => {
    if (!game || !player || gameOver) return;

    try {
      const response = await gameApi.claimBingo(game.id, cardId, player.id);
      const data = response.data;

      if (data.valid) {
        setShowConfetti(true);
        setIsPlayerWin(true);
        setWinData(data);
        setGameOver(true);
        setCoinsEarned(data.coins_earned || 50);
        onPlayerUpdate?.(data.player);
        setGame(prev => ({ ...prev, status: 'completed' }));

        setChat(prev => [...prev, {
          id: Date.now(),
          player: 'System',
          message: `🎉 ${player?.name || 'You'} WON BINGO! Congratulations! 🎉`,
          isSystem: true
        }]);
      } else {
        alert(data.message || "Not quite a bingo yet! Keep playing!");
      }
    } catch (error) {
      console.error('Failed to claim bingo:', error);
    }
  };

  const checkForBingo = (card) => {
    const markedSet = new Set(card.marked?.map(pos => `${pos[0]}-${pos[1]}`) || []);
    markedSet.add('2-2');

    for (let row = 0; row < 5; row++) {
      if ([0, 1, 2, 3, 4].every(col => markedSet.has(`${row}-${col}`))) return true;
    }

    for (let col = 0; col < 5; col++) {
      if ([0, 1, 2, 3, 4].every(row => markedSet.has(`${row}-${col}`))) return true;
    }

    if ([0, 1, 2, 3, 4].every(i => markedSet.has(`${i}-${i}`))) return true;
    if ([0, 1, 2, 3, 4].every(i => markedSet.has(`${i}-${4 - i}`))) return true;

    return false;
  };

  const handleCollectAndClose = () => {
    onGameEnd(coinsEarned);
  };

  const handlePlayAgain = () => {
    startNewGame();
  };

  const handleSendMessage = () => {
    if (!newMessage.trim()) return;

    setChat(prev => [...prev, {
      id: Date.now(),
      player: player?.name || 'You',
      message: newMessage.trim(),
      isSystem: false
    }]);
    setNewMessage('');
  };

  const confettiParticles = showConfetti ? Array.from({ length: 50 }, (_, i) => ({
    id: i,
    x: Math.random() * 100,
    delay: Math.random() * 0.5,
    duration: 2 + Math.random() * 2,
    color: ['#FFD700', '#FF69B4', '#00CED1', '#FF6B6B', '#98FB98', '#DDA0DD'][Math.floor(Math.random() * 6)],
    size: 8 + Math.random() * 8,
  })) : [];

  if (isLoading) {
    return (
      <motion.div
        className="bingo-game-overlay"
        initial={{ opacity: 0 }}
        animate={{ opacity: 1 }}
        exit={{ opacity: 0 }}
      >
        <div className="bingo-game-container">
          <div className="bingo-loading">
            <motion.div
              className="loading-balls"
              animate={{ rotate: 360 }}
              transition={{ duration: 3, repeat: Infinity, ease: 'linear' }}
            >
              {['B', 'I', 'N', 'G', 'O'].map((letter, i) => (
                <motion.div
                  key={letter}
                  className="loading-ball"
                  animate={{
                    y: [0, -20, 0],
                    scale: [1, 1.1, 1],
                  }}
                  transition={{
                    duration: 0.6,
                    repeat: Infinity,
                    delay: i * 0.1,
                  }}
                >
                  {letter}
                </motion.div>
              ))}
            </motion.div>
            <p>Setting up your bingo game...</p>
          </div>
        </div>
      </motion.div>
    );
  }

  return (
    <motion.div
      className="bingo-game-overlay"
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
    >
      {/* Confetti */}
      <AnimatePresence>
        {showConfetti && (
          <div className="confetti-container" ref={confettiRef}>
            {confettiParticles.map((particle) => (
              <motion.div
                key={particle.id}
                className="confetti-particle"
                style={{
                  left: `${particle.x}%`,
                  backgroundColor: particle.color,
                  width: particle.size,
                  height: particle.size,
                }}
                initial={{ y: -20, opacity: 1, rotate: 0 }}
                animate={{
                  y: '100vh',
                  opacity: [1, 1, 0],
                  rotate: 360 * (Math.random() > 0.5 ? 1 : -1),
                }}
                transition={{
                  duration: particle.duration,
                  delay: particle.delay,
                  ease: 'linear',
                }}
              />
            ))}
          </div>
        )}
      </AnimatePresence>

      {/* Countdown Overlay */}
      <AnimatePresence>
        {countdown !== null && countdown > 0 && (
          <motion.div
            className="countdown-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
          >
            <motion.div className="countdown-content">
              <div className="countdown-title">🎱 Get Ready! 🎱</div>
              <motion.div
                className="countdown-number"
                key={countdown}
                initial={{ scale: 2, opacity: 0 }}
                animate={{ scale: 1, opacity: 1 }}
                exit={{ scale: 0.5, opacity: 0 }}
                transition={{ type: 'spring', stiffness: 300, damping: 20 }}
              >
                {countdown}
              </motion.div>
              <div className="countdown-subtitle">Numbers will be called automatically!</div>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>

      <div className="bingo-game-container">
        {/* Voice Room Header */}
        <div className="voice-room-header">
          <button className="leave-btn" onClick={onClose}>
            <span>←</span>
          </button>
          <div className="room-info">
            <h3>🎱 Bingo Room</h3>
            <span className="player-count">{players.length} players</span>
          </div>
          <div className="header-actions">
            <button
              className={`mute-btn ${isMuted ? 'muted' : ''}`}
              onClick={() => setIsMuted(!isMuted)}
            >
              {isMuted ? '🔇' : '🎤'}
            </button>
          </div>
        </div>

        {/* Prize Pool */}
        <div className="prize-pool-banner">
          <div className="prize-item first">
            <span className="prize-label">1st</span>
            <span className="prize-amount">🪙 {prizePool}</span>
          </div>
          <div className="prize-item second">
            <span className="prize-label">2nd</span>
            <span className="prize-amount">🪙 {Math.floor(prizePool * 0.4)}</span>
          </div>
          <div className="prize-item third">
            <span className="prize-label">3rd</span>
            <span className="prize-amount">🪙 {Math.floor(prizePool * 0.2)}</span>
          </div>
        </div>

        {/* Players Strip */}
        <div className="players-strip">
          {players.map((p) => (
            <motion.div
              key={p.id}
              className={`player-bubble ${p.isSpeaking ? 'speaking' : ''} ${selectedPlayer?.id === p.id ? 'selected' : ''}`}
              onClick={() => setSelectedPlayer(selectedPlayer?.id === p.id ? null : p)}
              whileTap={{ scale: 0.95 }}
            >
              <div className="player-avatar-bubble">
                {p.avatar}
                {p.isHost && <span className="host-badge">👑</span>}
              </div>
              <span className="player-name-small">{p.name}</span>
              {p.isSpeaking && <div className="speaking-indicator" />}
              <div className="player-progress-mini">
                <span className="progress-mini-text">{p.progress}/5</span>
              </div>
            </motion.div>
          ))}
        </div>

        {/* Selected Player Modal */}
        <AnimatePresence>
          {selectedPlayer && (
            <motion.div
              className="player-modal-overlay"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
              onClick={() => setSelectedPlayer(null)}
            >
              <motion.div
                className="player-modal"
                initial={{ scale: 0.8, opacity: 0 }}
                animate={{ scale: 1, opacity: 1 }}
                exit={{ scale: 0.8, opacity: 0 }}
                onClick={(e) => e.stopPropagation()}
              >
                <div className="player-modal-header">
                  <div className="player-modal-avatar">{selectedPlayer.avatar}</div>
                  <h3>{selectedPlayer.name}</h3>
                  {selectedPlayer.isHost && <span className="host-tag">👑 Host</span>}
                </div>
                <div className="player-modal-progress">
                  <span className="progress-title">Bingo Progress</span>
                  <div className="progress-dots-large">
                    {[1, 2, 3, 4, 5].map((i) => (
                      <div
                        key={i}
                        className={`progress-dot-large ${i <= selectedPlayer.progress ? 'filled' : ''}`}
                      />
                    ))}
                  </div>
                  <span className="progress-value">{selectedPlayer.progress}/5 in a row</span>
                </div>
                <button className="player-modal-close" onClick={() => setSelectedPlayer(null)}>
                  Close
                </button>
              </motion.div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Progress Indicators */}
        <div className="bingo-progress-section">
          <div className="progress-item you">
            <span className="progress-label">🎮 You</span>
            <div className="progress-dots">
              {[1, 2, 3, 4, 5].map((i) => (
                <motion.div
                  key={i}
                  className={`progress-dot ${i <= matchedInRow ? 'filled' : ''}`}
                  animate={i <= matchedInRow ? { scale: [1, 1.2, 1] } : {}}
                  transition={{ duration: 0.3 }}
                />
              ))}
            </div>
            <span className="progress-count">{matchedInRow}/5</span>
          </div>
          <div className="progress-item cpu">
            <span className="progress-label">🤖 CPU</span>
            <div className="progress-dots">
              {[1, 2, 3, 4, 5].map((i) => (
                <div
                  key={i}
                  className={`progress-dot cpu ${i <= cpuProgress ? 'filled' : ''}`}
                />
              ))}
            </div>
            <span className="progress-count">{cpuProgress}/5</span>
          </div>
        </div>

        {/* Near Bingo Alert */}
        <AnimatePresence>
          {nearBingo && !gameOver && (
            <motion.div
              className="near-bingo-alert"
              initial={{ scale: 0, y: -20 }}
              animate={{ scale: 1, y: 0 }}
              exit={{ scale: 0, y: -20 }}
            >
              <span className="alert-emoji">🔥</span>
              <span className="alert-text">ONE MORE FOR BINGO!</span>
              <span className="alert-emoji">🔥</span>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Number Caller */}
        <div className="bingo-caller-section">
          <NumberCaller
            currentNumber={game?.current_number}
            calledNumbers={game?.called_numbers || []}
            onCallNumber={handleCallNumber}
            isLoading={isCallingNumber}
            gameOver={gameOver}
            lastMarkedNumber={lastMarkedNumber}
            nextCallIn={nextCallIn}
            gameStarted={gameStarted}
          />
        </div>

        {/* Player's Cards */}
        <div className="bingo-cards-section">
          <div className="bingo-cards-grid">
            {playerCards.map((card, index) => (
              <div key={card.id} className="bingo-card-wrapper">
                <BingoCard
                  card={card}
                  cardIndex={index}
                  calledNumbers={game?.called_numbers || []}
                  onNumberClick={handleMarkNumber}
                  dauber={player?.active_dauber || 'star'}
                  isActive={!gameOver}
                  lastMarkedNumber={lastMarkedNumber}
                />
                {checkForBingo(card) && !gameOver && (
                  <motion.button
                    className="claim-bingo-button"
                    onClick={() => handleClaimBingo(card.id)}
                    initial={{ scale: 0 }}
                    animate={{
                      scale: [1, 1.1, 1],
                      boxShadow: [
                        '0 4px 20px rgba(255, 215, 0, 0.5)',
                        '0 8px 40px rgba(255, 215, 0, 0.8)',
                        '0 4px 20px rgba(255, 215, 0, 0.5)',
                      ]
                    }}
                    transition={{ duration: 0.5, repeat: Infinity }}
                  >
                    <span className="bingo-text">BINGO!</span>
                    <span className="bingo-celebration">🎉✨🎉</span>
                  </motion.button>
                )}
              </div>
            ))}
          </div>
        </div>

        {/* Chat Toggle Button */}
        <button className="chat-toggle" onClick={() => setShowChat(true)}>
          💬 Chat
          {chat.length > 0 && <span className="chat-badge">{chat.length}</span>}
        </button>

        {/* Chat Panel */}
        <AnimatePresence>
          {showChat && (
            <motion.div
              className="chat-panel"
              initial={{ y: '100%' }}
              animate={{ y: 0 }}
              exit={{ y: '100%' }}
              transition={{ type: 'spring', damping: 25, stiffness: 300 }}
            >
              <div className="chat-header">
                <h4>💬 Room Chat</h4>
                <button className="chat-close-btn" onClick={() => setShowChat(false)}>✕</button>
              </div>
              <div className="chat-messages" ref={chatMessagesRef}>
                {chat.map(msg => (
                  <motion.div
                    key={msg.id}
                    className={`chat-message ${msg.isSystem ? 'system' : ''}`}
                    initial={{ opacity: 0, x: -20 }}
                    animate={{ opacity: 1, x: 0 }}
                  >
                    {!msg.isSystem && <span className="chat-player">{msg.player}:</span>}
                    <span className="chat-text">{msg.message}</span>
                  </motion.div>
                ))}
              </div>
              <div className="chat-input-area">
                <input
                  type="text"
                  placeholder="Type a message..."
                  value={newMessage}
                  onChange={(e) => setNewMessage(e.target.value)}
                  onKeyPress={(e) => e.key === 'Enter' && handleSendMessage()}
                />
                <button className="send-btn" onClick={handleSendMessage}>
                  Send
                </button>
              </div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Game Over Modal */}
        <AnimatePresence>
          {gameOver && (
            <motion.div
              className="bingo-gameover-overlay"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
            >
              <motion.div
                className="bingo-gameover-modal"
                initial={{ scale: 0, rotate: -10 }}
                animate={{ scale: 1, rotate: 0 }}
                exit={{ scale: 0 }}
              >
                <div className="gameover-header">
                  {isPlayerWin ? (
                    <>
                      <motion.span
                        className="gameover-emoji"
                        animate={{ rotate: [0, 15, -15, 0], scale: [1, 1.2, 1.2, 1] }}
                        transition={{ duration: 0.5, repeat: Infinity }}
                      >
                        🎉
                      </motion.span>
                      <h2>BINGO!</h2>
                      <motion.span
                        className="gameover-emoji"
                        animate={{ rotate: [0, -15, 15, 0], scale: [1, 1.2, 1.2, 1] }}
                        transition={{ duration: 0.5, repeat: Infinity }}
                      >
                        🎉
                      </motion.span>
                    </>
                  ) : (
                    <>
                      <span className="gameover-emoji">😢</span>
                      <h2>So Close!</h2>
                      <span className="gameover-emoji">😢</span>
                    </>
                  )}
                </div>

                <p className="gameover-message">
                  {isPlayerWin
                    ? "You got BINGO! Amazing!"
                    : `${winData?.winner || 'CPU'} got bingo first. Better luck next time!`}
                </p>

                <motion.div
                  className="gameover-reward"
                  initial={{ scale: 0 }}
                  animate={{ scale: 1 }}
                  transition={{ delay: 0.3, type: 'spring' }}
                >
                  <span className="reward-label">{isPlayerWin ? 'You won:' : 'Consolation:'}</span>
                  <motion.span
                    className="reward-value"
                    animate={{ scale: [1, 1.1, 1] }}
                    transition={{ duration: 0.5, repeat: Infinity }}
                  >
                    🪙 {coinsEarned}
                  </motion.span>
                </motion.div>

                <div className="gameover-buttons">
                  <motion.button
                    className="play-again-btn"
                    onClick={handlePlayAgain}
                    whileHover={{ scale: 1.05 }}
                    whileTap={{ scale: 0.95 }}
                  >
                    🔄 Play Again
                  </motion.button>
                  <motion.button
                    className="collect-btn"
                    onClick={handleCollectAndClose}
                    whileHover={{ scale: 1.05 }}
                    whileTap={{ scale: 0.95 }}
                  >
                    ✅ Collect & Exit
                  </motion.button>
                </div>
              </motion.div>
            </motion.div>
          )}
        </AnimatePresence>
      </div>
    </motion.div>
  );
};

export default BingoGame;
