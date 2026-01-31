import React, { useState, useEffect, useCallback, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './VoiceRoomGame.css';

// Generate a random bingo card
const generateBingoCard = () => {
  const card = [];
  const ranges = [
    [1, 15],   // B
    [16, 30],  // I
    [31, 45],  // N
    [46, 60],  // G
    [61, 75],  // O
  ];

  for (let col = 0; col < 5; col++) {
    const colNumbers = [];
    const [min, max] = ranges[col];
    const available = Array.from({ length: max - min + 1 }, (_, i) => min + i);

    for (let row = 0; row < 5; row++) {
      if (col === 2 && row === 2) {
        colNumbers.push('FREE');
      } else {
        const randomIndex = Math.floor(Math.random() * available.length);
        colNumbers.push(available.splice(randomIndex, 1)[0]);
      }
    }
    card.push(colNumbers);
  }

  return card;
};

// Get WebSocket URL based on environment
const getWebSocketUrl = (roomId, playerId) => {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const host = window.location.host;
  // For production, use the same host; for local, it might be different
  if (host.includes('localhost') || host.includes('127.0.0.1')) {
    return `ws://localhost:8000/ws/room/${roomId}/${playerId}/`;
  }
  return `${protocol}//${host}/ws/room/${roomId}/${playerId}/`;
};

const VoiceRoomGame = ({ room, player, onLeave }) => {
  const [gameState, setGameState] = useState('waiting'); // waiting, countdown, playing, ended
  const [bingoCard, setBingoCard] = useState(null);
  const [markedNumbers, setMarkedNumbers] = useState(new Set(['FREE']));
  const [calledNumbers, setCalledNumbers] = useState([]);
  const [currentNumber, setCurrentNumber] = useState(null);
  const [players, setPlayers] = useState([]);
  const [chat, setChat] = useState([]);
  const [newMessage, setNewMessage] = useState('');
  const [showChat, setShowChat] = useState(false);
  const [countdown, setCountdown] = useState(null);
  const [prizePool] = useState(room?.first_prize || 2500);
  const [isMuted, setIsMuted] = useState(false);
  const [purchasedCards, setPurchasedCards] = useState(1);
  const [showBuyCards, setShowBuyCards] = useState(false);
  const [isConnected, setIsConnected] = useState(false);
  const [connectionError, setConnectionError] = useState(null);
  const [myProgress, setMyProgress] = useState(0);

  // Check if current player is the host
  const isHost = room?.host?.id === player?.id || room?.host_id === player?.id;

  const wsRef = useRef(null);
  const chatRef = useRef(null);
  const autoCallRef = useRef(null);
  const countdownRef = useRef(null);

  // Initialize bingo card
  useEffect(() => {
    setBingoCard(generateBingoCard());
  }, []);

  // WebSocket connection
  useEffect(() => {
    if (!room?.room_id || !player?.id) return;

    const wsUrl = getWebSocketUrl(room.room_id, player.id);
    console.log('Connecting to WebSocket:', wsUrl);

    const ws = new WebSocket(wsUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      console.log('WebSocket connected');
      setIsConnected(true);
      setConnectionError(null);

      // Add self to players list
      setPlayers(prev => {
        const exists = prev.some(p => p.id === player.id);
        if (!exists) {
          return [...prev, {
            id: player.id,
            name: player.name,
            avatar: '👤',
            isHost: false,
            isSpeaking: false,
            progress: 0
          }];
        }
        return prev;
      });

      // Add system message
      setChat(prev => [...prev, {
        id: Date.now(),
        player: 'System',
        message: 'Connected to room!',
        isSystem: true
      }]);
    };

    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      handleWebSocketMessage(data);
    };

    ws.onclose = () => {
      console.log('WebSocket disconnected');
      setIsConnected(false);
    };

    ws.onerror = (error) => {
      console.error('WebSocket error:', error);
      setConnectionError('Connection failed. Using offline mode.');
      // Fall back to mock mode
      initializeMockMode();
    };

    return () => {
      if (ws.readyState === WebSocket.OPEN) {
        ws.close();
      }
    };
  }, [room?.room_id, player?.id]);

  // Handle incoming WebSocket messages
  const handleWebSocketMessage = useCallback((data) => {
    switch (data.type) {
      case 'player_joined':
        setPlayers(prev => {
          const exists = prev.some(p => p.id === data.player?.id);
          if (!exists && data.player) {
            return [...prev, {
              ...data.player,
              isSpeaking: false,
              progress: 0
            }];
          }
          return prev;
        });
        setChat(prev => [...prev, {
          id: Date.now(),
          player: 'System',
          message: `${data.player?.name || 'Someone'} joined the room!`,
          isSystem: true
        }]);
        break;

      case 'player_left':
        setPlayers(prev => prev.filter(p => p.id !== data.player?.id));
        setChat(prev => [...prev, {
          id: Date.now(),
          player: 'System',
          message: `${data.player?.name || 'Someone'} left the room`,
          isSystem: true
        }]);
        break;

      case 'chat_message':
        setChat(prev => [...prev, {
          id: Date.now(),
          player: data.player?.name || 'Unknown',
          message: data.message,
          isSystem: data.is_system
        }]);
        break;

      case 'game_starting':
        setCountdown(data.countdown);
        setGameState('countdown');
        break;

      case 'number_called':
        setCurrentNumber(data.number);
        setCalledNumbers(data.called_numbers || []);
        break;

      case 'progress_update':
        setPlayers(prev => prev.map(p =>
          p.id === data.player?.id
            ? { ...p, progress: data.progress }
            : p
        ));
        break;

      case 'bingo_claimed':
        if (data.valid) {
          setGameState('ended');
          setChat(prev => [...prev, {
            id: Date.now(),
            player: 'System',
            message: `🎉 ${data.player?.name || 'Someone'} got BINGO! 🎉`,
            isSystem: true
          }]);
        }
        break;

      case 'mute_changed':
        setPlayers(prev => prev.map(p =>
          p.id === data.player?.id
            ? { ...p, isMuted: data.is_muted }
            : p
        ));
        break;

      default:
        console.log('Unknown message type:', data.type);
    }
  }, []);

  // Initialize mock mode for offline/fallback
  const initializeMockMode = useCallback(() => {
    setPlayers([
      { id: player?.id || 1, name: player?.name || 'You', avatar: '👤', isHost: false, isSpeaking: false, progress: 0 },
      { id: 99, name: 'AI Player', avatar: '🤖', isHost: true, isSpeaking: true, progress: 0 },
      { id: 100, name: 'Bot1', avatar: '🎮', isHost: false, isSpeaking: false, progress: 0 },
    ]);
    setChat([{
      id: Date.now(),
      player: 'System',
      message: 'Running in offline mode',
      isSystem: true
    }]);
  }, [player]);

  // Auto-scroll chat
  useEffect(() => {
    if (chatRef.current) {
      chatRef.current.scrollTop = chatRef.current.scrollHeight;
    }
  }, [chat]);

  // Host manually starts the game
  const handleStartGame = useCallback(() => {
    if (!isHost || gameState !== 'waiting') return;

    setCountdown(10);
    setGameState('countdown');

    // Broadcast game starting
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'start_game'
      }));
    }

    // Add system message
    setChat(prev => [...prev, {
      id: Date.now(),
      player: 'System',
      message: '🎮 The host has started the game! Get ready!',
      isSystem: true
    }]);
  }, [isHost, gameState]);

  // Countdown logic
  useEffect(() => {
    if (countdown === null || countdown < 0) return;

    if (countdown > 0) {
      countdownRef.current = setTimeout(() => setCountdown(countdown - 1), 1000);
      return () => clearTimeout(countdownRef.current);
    } else if (countdown === 0) {
      setGameState('playing');
      setCountdown(null);
      startCallingNumbers();
    }
  }, [countdown]);

  // Start calling numbers
  const startCallingNumbers = useCallback(() => {
    const allNumbers = Array.from({ length: 75 }, (_, i) => i + 1);
    let numbersToCall = [...allNumbers];
    let called = [];

    const callNumber = () => {
      if (numbersToCall.length === 0 || gameState === 'ended') {
        clearInterval(autoCallRef.current);
        return;
      }

      const randomIndex = Math.floor(Math.random() * numbersToCall.length);
      const newNumber = numbersToCall.splice(randomIndex, 1)[0];
      called.push(newNumber);

      setCurrentNumber(newNumber);
      setCalledNumbers([...called]);

      // Broadcast number called
      if (wsRef.current?.readyState === WebSocket.OPEN) {
        wsRef.current.send(JSON.stringify({
          type: 'call_number',
          number: newNumber,
          called_numbers: called
        }));
      }

      // Simulate AI progress
      setPlayers(prev => prev.map(p =>
        p.id !== player?.id && Math.random() > 0.6
          ? { ...p, progress: Math.min(5, p.progress + 1) }
          : p
      ));
    };

    // Call first number immediately
    callNumber();

    // Then call every 5 seconds
    autoCallRef.current = setInterval(callNumber, 5000);

    return () => clearInterval(autoCallRef.current);
  }, [gameState, player?.id]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (autoCallRef.current) clearInterval(autoCallRef.current);
      if (countdownRef.current) clearTimeout(countdownRef.current);
    };
  }, []);

  // Get letter for bingo number
  const getBingoLetter = (num) => {
    if (num <= 15) return 'B';
    if (num <= 30) return 'I';
    if (num <= 45) return 'N';
    if (num <= 60) return 'G';
    return 'O';
  };

  // Handle marking a number on the card
  const handleMarkNumber = (number) => {
    if (number === 'FREE' || calledNumbers.includes(number)) {
      setMarkedNumbers(prev => {
        const newSet = new Set([...prev, number]);
        // Calculate progress
        const progress = calculateProgress(newSet);
        setMyProgress(progress);

        // Broadcast progress update
        if (wsRef.current?.readyState === WebSocket.OPEN) {
          wsRef.current.send(JSON.stringify({
            type: 'player_progress',
            progress: progress
          }));
        }

        return newSet;
      });
    }
  };

  // Calculate progress (max in any line)
  const calculateProgress = (marked) => {
    if (!bingoCard) return 0;

    let maxInLine = 0;

    // Check rows
    for (let row = 0; row < 5; row++) {
      let count = 0;
      for (let col = 0; col < 5; col++) {
        if (marked.has(bingoCard[col][row])) count++;
      }
      maxInLine = Math.max(maxInLine, count);
    }

    // Check columns
    for (let col = 0; col < 5; col++) {
      let count = 0;
      for (let row = 0; row < 5; row++) {
        if (marked.has(bingoCard[col][row])) count++;
      }
      maxInLine = Math.max(maxInLine, count);
    }

    // Check diagonals
    let diag1 = 0, diag2 = 0;
    for (let i = 0; i < 5; i++) {
      if (marked.has(bingoCard[i][i])) diag1++;
      if (marked.has(bingoCard[i][4 - i])) diag2++;
    }
    maxInLine = Math.max(maxInLine, diag1, diag2);

    return maxInLine;
  };

  // Check for bingo
  const checkBingo = () => {
    if (!bingoCard) return false;

    // Check rows
    for (let row = 0; row < 5; row++) {
      let rowComplete = true;
      for (let col = 0; col < 5; col++) {
        if (!markedNumbers.has(bingoCard[col][row])) {
          rowComplete = false;
          break;
        }
      }
      if (rowComplete) return true;
    }

    // Check columns
    for (let col = 0; col < 5; col++) {
      let colComplete = true;
      for (let row = 0; row < 5; row++) {
        if (!markedNumbers.has(bingoCard[col][row])) {
          colComplete = false;
          break;
        }
      }
      if (colComplete) return true;
    }

    // Check diagonals
    let diag1 = true, diag2 = true;
    for (let i = 0; i < 5; i++) {
      if (!markedNumbers.has(bingoCard[i][i])) diag1 = false;
      if (!markedNumbers.has(bingoCard[i][4 - i])) diag2 = false;
    }

    return diag1 || diag2;
  };

  // Handle claiming bingo
  const handleClaimBingo = () => {
    if (!checkBingo()) return;

    // Stop auto-calling
    if (autoCallRef.current) clearInterval(autoCallRef.current);

    // Broadcast bingo claim
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'claim_bingo',
        valid: true
      }));
    }

    setGameState('ended');
    setChat(prev => [...prev, {
      id: Date.now(),
      player: 'System',
      message: `🎉 ${player?.name || 'You'} got BINGO! 🎉`,
      isSystem: true
    }]);
  };

  // Handle sending chat message
  const handleSendMessage = () => {
    if (!newMessage.trim()) return;

    // Send via WebSocket
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'chat_message',
        message: newMessage.trim()
      }));
    } else {
      // Fallback for offline mode
      setChat(prev => [...prev, {
        id: Date.now(),
        player: player?.name || 'You',
        message: newMessage.trim(),
        isSystem: false
      }]);
    }

    setNewMessage('');
  };

  // Handle mute toggle
  const handleMuteToggle = () => {
    setIsMuted(!isMuted);

    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'mute_toggle',
        is_muted: !isMuted
      }));
    }
  };

  // Handle buying more cards
  const handleBuyCard = () => {
    if (purchasedCards < 4) {
      setPurchasedCards(prev => prev + 1);
      setShowBuyCards(false);
    }
  };

  return (
    <div className="voice-room-game">
      {/* Connection Status */}
      {connectionError && (
        <div className="connection-error">
          {connectionError}
        </div>
      )}

      {/* Header */}
      <div className="room-game-header">
        <button className="leave-btn" onClick={onLeave}>
          <span>←</span>
        </button>
        <div className="room-info">
          <h3>{room?.name || "🎵 Music Bingo Party"}</h3>
          <span className="player-count">
            {players.length} players
            {isConnected && <span className="connected-dot">●</span>}
          </span>
        </div>
        <div className="header-actions">
          <button
            className={`mute-btn ${isMuted ? 'muted' : ''}`}
            onClick={handleMuteToggle}
          >
            {isMuted ? '🔇' : '🎤'}
          </button>
          <button className="settings-btn">⚙️</button>
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

      {/* Current Number Display */}
      {gameState === 'playing' && currentNumber && (
        <motion.div
          className="current-number-display"
          initial={{ scale: 0 }}
          animate={{ scale: 1 }}
          key={currentNumber}
        >
          <div className="number-ball">
            <span className="letter">{getBingoLetter(currentNumber)}</span>
            <span className="number">{currentNumber}</span>
          </div>
          <div className="called-count">{calledNumbers.length}/75 called</div>
        </motion.div>
      )}

      {/* Countdown */}
      <AnimatePresence>
        {countdown !== null && countdown > 0 && (
          <motion.div
            className="countdown-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
          >
            <div className="countdown-content">
              <span className="countdown-label">Game starting in</span>
              <motion.span
                className="countdown-number"
                key={countdown}
                initial={{ scale: 1.5, opacity: 0 }}
                animate={{ scale: 1, opacity: 1 }}
              >
                {countdown}
              </motion.span>
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Host Start Game Button */}
      {gameState === 'waiting' && isHost && (
        <motion.div
          className="start-game-section"
          initial={{ opacity: 0, y: -20 }}
          animate={{ opacity: 1, y: 0 }}
        >
          <div className="waiting-info">
            <span className="waiting-icon">👥</span>
            <span className="waiting-text">{players.length} player{players.length !== 1 ? 's' : ''} in room</span>
          </div>
          <motion.button
            className="start-game-btn"
            onClick={handleStartGame}
            whileHover={{ scale: 1.05 }}
            whileTap={{ scale: 0.95 }}
          >
            <span className="start-icon">🎮</span>
            <span>Start Game</span>
          </motion.button>
          <p className="start-hint">Press to begin the bingo game!</p>
        </motion.div>
      )}

      {/* Waiting for Host (non-host view) */}
      {gameState === 'waiting' && !isHost && (
        <motion.div
          className="waiting-for-host"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
        >
          <div className="waiting-spinner"></div>
          <p>Waiting for host to start the game...</p>
          <span className="player-count-waiting">{players.length} player{players.length !== 1 ? 's' : ''} in room</span>
        </motion.div>
      )}

      {/* Players Strip with Progress */}
      <div className="players-strip">
        {players.map((p) => (
          <div key={p.id} className={`player-bubble ${p.isSpeaking ? 'speaking' : ''}`}>
            <div className="player-avatar-bubble">
              {p.avatar}
              {p.isHost && <span className="host-badge">👑</span>}
            </div>
            <span className="player-name-small">{p.name}</span>
            {p.isSpeaking && <div className="speaking-indicator" />}
            <div className="player-progress-mini">
              <span className="progress-mini-text">
                {p.id === player?.id ? myProgress : p.progress}/5
              </span>
            </div>
          </div>
        ))}
      </div>

      {/* Bingo Card */}
      {bingoCard && (
        <div className="bingo-card-container">
          <div className="card-header-row">
            {['B', 'I', 'N', 'G', 'O'].map(letter => (
              <div key={letter} className="bingo-letter">{letter}</div>
            ))}
          </div>
          <div className="bingo-grid">
            {[0, 1, 2, 3, 4].map(row => (
              <div key={row} className="bingo-row">
                {[0, 1, 2, 3, 4].map(col => {
                  const number = bingoCard[col][row];
                  const isMarked = markedNumbers.has(number);
                  const isCalled = number === 'FREE' || calledNumbers.includes(number);

                  return (
                    <motion.button
                      key={`${col}-${row}`}
                      className={`bingo-cell ${isMarked ? 'marked' : ''} ${isCalled ? 'called' : ''}`}
                      onClick={() => handleMarkNumber(number)}
                      whileTap={{ scale: 0.95 }}
                    >
                      {number === 'FREE' ? '⭐' : number}
                    </motion.button>
                  );
                })}
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Action Buttons */}
      <div className="game-actions">
        <button
          className="action-btn buy-cards"
          onClick={() => setShowBuyCards(true)}
        >
          <span>🎴</span>
          <span>Buy Cards ({purchasedCards}/4)</span>
        </button>
        <button
          className={`action-btn bingo-btn ${checkBingo() ? 'has-bingo' : ''}`}
          onClick={handleClaimBingo}
          disabled={!checkBingo()}
        >
          <span>🎉</span>
          <span>BINGO!</span>
        </button>
      </div>

      {/* Called Numbers History */}
      <div className="called-numbers-section">
        <h4>Called Numbers</h4>
        <div className="called-numbers-list">
          {calledNumbers.slice().reverse().slice(0, 10).map((num, idx) => (
            <span key={num} className={`called-number ${idx === 0 ? 'latest' : ''}`}>
              {getBingoLetter(num)}{num}
            </span>
          ))}
        </div>
      </div>

      {/* Chat Toggle */}
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
          >
            <div className="chat-header">
              <h4>Room Chat</h4>
              <button onClick={() => setShowChat(false)}>✕</button>
            </div>
            <div className="chat-messages" ref={chatRef}>
              {chat.map(msg => (
                <div key={msg.id} className={`chat-message ${msg.isSystem ? 'system' : ''}`}>
                  {!msg.isSystem && <span className="chat-player">{msg.player}:</span>}
                  <span className="chat-text">{msg.message}</span>
                </div>
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
              <button onClick={handleSendMessage}>Send</button>
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Buy Cards Modal */}
      <AnimatePresence>
        {showBuyCards && (
          <motion.div
            className="buy-cards-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
          >
            <motion.div
              className="buy-cards-modal"
              initial={{ scale: 0.9 }}
              animate={{ scale: 1 }}
              exit={{ scale: 0.9 }}
            >
              <h3>Buy More Cards</h3>
              <p>Increase your chances of winning!</p>
              <div className="card-options">
                <div className="card-option">
                  <span className="card-count">1 Card</span>
                  <span className="card-price">🪙 100</span>
                </div>
              </div>
              <p className="cards-owned">You have {purchasedCards}/4 cards</p>
              <div className="modal-actions">
                <button
                  className="buy-btn"
                  onClick={handleBuyCard}
                  disabled={purchasedCards >= 4}
                >
                  Buy Card (🪙 100)
                </button>
                <button className="cancel-btn" onClick={() => setShowBuyCards(false)}>
                  Cancel
                </button>
              </div>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Game Over Modal */}
      <AnimatePresence>
        {gameState === 'ended' && (
          <motion.div
            className="game-over-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
          >
            <motion.div
              className="game-over-modal"
              initial={{ scale: 0.9 }}
              animate={{ scale: 1 }}
            >
              <h2>🎉 Game Over! 🎉</h2>
              <p>Thanks for playing!</p>
              <button className="play-again-btn" onClick={onLeave}>
                Back to Lobby
              </button>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
};

export default VoiceRoomGame;
