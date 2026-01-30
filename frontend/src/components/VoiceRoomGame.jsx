import React, { useState, useEffect, useCallback } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './VoiceRoomGame.css';

// Mock data for development
const MOCK_PLAYERS = [
  { id: 1, name: 'Host', avatar: '👸', isHost: true, isSpeaking: true },
  { id: 2, name: 'Player2', avatar: '🧑', isHost: false, isSpeaking: false },
  { id: 3, name: 'GamerGirl', avatar: '👧', isHost: false, isSpeaking: true },
  { id: 4, name: 'CoolDude', avatar: '🧔', isHost: false, isSpeaking: false },
  { id: 5, name: 'StarPlayer', avatar: '👨', isHost: false, isSpeaking: false },
  { id: 6, name: 'LuckyOne', avatar: '👩', isHost: false, isSpeaking: false },
];

const MOCK_CHAT = [
  { id: 1, player: 'Host', message: 'Welcome everyone! 🎉', isSystem: false },
  { id: 2, player: 'System', message: 'Game starting in 30 seconds...', isSystem: true },
  { id: 3, player: 'GamerGirl', message: 'Good luck all!', isSystem: false },
  { id: 4, player: 'CoolDude', message: 'Let\'s go! 🔥', isSystem: false },
];

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

const VoiceRoomGame = ({ room, player, onLeave }) => {
  const [gameState, setGameState] = useState('waiting'); // waiting, playing, ended
  const [bingoCard, setBingoCard] = useState(null);
  const [markedNumbers, setMarkedNumbers] = useState(new Set(['FREE']));
  const [calledNumbers, setCalledNumbers] = useState([]);
  const [currentNumber, setCurrentNumber] = useState(null);
  const [players, setPlayers] = useState(MOCK_PLAYERS);
  const [chat, setChat] = useState(MOCK_CHAT);
  const [newMessage, setNewMessage] = useState('');
  const [showChat, setShowChat] = useState(false);
  const [countdown, setCountdown] = useState(null);
  const [prizePool, setPrizePool] = useState(2500);
  const [isMuted, setIsMuted] = useState(false);
  const [purchasedCards, setPurchasedCards] = useState(1);
  const [showBuyCards, setShowBuyCards] = useState(false);

  // Initialize bingo card
  useEffect(() => {
    setBingoCard(generateBingoCard());
  }, []);

  // Simulate game start countdown
  useEffect(() => {
    if (gameState === 'waiting' && players.length >= 2) {
      const timer = setTimeout(() => {
        setCountdown(10);
      }, 3000);
      return () => clearTimeout(timer);
    }
  }, [gameState, players.length]);

  // Countdown logic
  useEffect(() => {
    if (countdown === null) return;

    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(countdown - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
      startCallingNumbers();
    }
  }, [countdown]);

  // Start calling numbers (simulation)
  const startCallingNumbers = useCallback(() => {
    const callNumber = () => {
      const available = Array.from({ length: 75 }, (_, i) => i + 1)
        .filter(n => !calledNumbers.includes(n));

      if (available.length === 0) return;

      const randomIndex = Math.floor(Math.random() * available.length);
      const newNumber = available[randomIndex];

      setCurrentNumber(newNumber);
      setCalledNumbers(prev => [...prev, newNumber]);
    };

    // Call a number every 5 seconds
    const interval = setInterval(callNumber, 5000);
    callNumber(); // Call first number immediately

    return () => clearInterval(interval);
  }, [calledNumbers]);

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
      setMarkedNumbers(prev => new Set([...prev, number]));
    }
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

  // Handle sending chat message
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

  // Handle buying more cards
  const handleBuyCard = () => {
    if (purchasedCards < 4) {
      setPurchasedCards(prev => prev + 1);
      setShowBuyCards(false);
    }
  };

  return (
    <div className="voice-room-game">
      {/* Header */}
      <div className="room-game-header">
        <button className="leave-btn" onClick={onLeave}>
          <span>←</span>
        </button>
        <div className="room-info">
          <h3>{room?.name || "🎵 Music Bingo Party"}</h3>
          <span className="player-count">{players.length} players</span>
        </div>
        <div className="header-actions">
          <button
            className={`mute-btn ${isMuted ? 'muted' : ''}`}
            onClick={() => setIsMuted(!isMuted)}
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
      {countdown !== null && countdown > 0 && (
        <motion.div
          className="countdown-overlay"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
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

      {/* Players Strip */}
      <div className="players-strip">
        {players.map((p) => (
          <div key={p.id} className={`player-bubble ${p.isSpeaking ? 'speaking' : ''}`}>
            <div className="player-avatar-bubble">
              {p.avatar}
              {p.isHost && <span className="host-badge">👑</span>}
            </div>
            <span className="player-name-small">{p.name}</span>
            {p.isSpeaking && <div className="speaking-indicator" />}
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
            <div className="chat-messages">
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
    </div>
  );
};

export default VoiceRoomGame;
