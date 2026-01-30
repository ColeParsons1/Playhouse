import { useState, useEffect, useCallback, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './ColorCardsGame.css';

const COLORS = ['red', 'blue', 'green', 'yellow'];
const COLOR_HEX = {
  red: '#E74C3C',
  blue: '#3498DB',
  green: '#2ECC71',
  yellow: '#F1C40F',
  wild: '#9B59B6',
};

const COLOR_GRADIENTS = {
  red: 'linear-gradient(135deg, #E74C3C, #C0392B)',
  blue: 'linear-gradient(135deg, #3498DB, #2980B9)',
  green: 'linear-gradient(135deg, #2ECC71, #27AE60)',
  yellow: 'linear-gradient(135deg, #F1C40F, #D4AC0D)',
  wild: 'linear-gradient(135deg, #9B59B6, #8E44AD, #E74C3C, #F1C40F)',
};

const NUMBERS = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9'];
const SPECIAL_CARDS = ['skip', 'reverse', 'draw2'];

const WIN_BONUS = 100;
const CARD_BONUS = 5;
const COMBO_BONUS = 15;
const UNO_CALL_BONUS = 10;

const AI_AVATARS = ['🦊', '🐸', '🐼'];
const AI_NAMES = ['Fox', 'Frog', 'Panda'];

// Generate full deck
const generateDeck = () => {
  const deck = [];
  let id = 0;

  COLORS.forEach(color => {
    deck.push({ id: id++, color, value: '0', type: 'number' });
    NUMBERS.slice(1).forEach(num => {
      deck.push({ id: id++, color, value: num, type: 'number' });
      deck.push({ id: id++, color, value: num, type: 'number' });
    });

    SPECIAL_CARDS.forEach(special => {
      deck.push({ id: id++, color, value: special, type: 'special' });
      deck.push({ id: id++, color, value: special, type: 'special' });
    });
  });

  for (let i = 0; i < 4; i++) {
    deck.push({ id: id++, color: 'wild', value: 'wild', type: 'wild' });
    deck.push({ id: id++, color: 'wild', value: 'wild4', type: 'wild' });
  }

  return deck;
};

const shuffleDeck = (deck) => {
  const shuffled = [...deck];
  for (let i = shuffled.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [shuffled[i], shuffled[j]] = [shuffled[j], shuffled[i]];
  }
  return shuffled;
};

// Particle effect component
const Particle = ({ color, x, y, delay }) => (
  <motion.div
    className="card-particle"
    initial={{ opacity: 1, scale: 1, x: 0, y: 0 }}
    animate={{
      opacity: 0,
      scale: 0,
      x: (Math.random() - 0.5) * 150,
      y: (Math.random() - 0.5) * 150 - 50,
    }}
    transition={{ duration: 0.8, delay, ease: 'easeOut' }}
    style={{
      position: 'absolute',
      left: x,
      top: y,
      width: 10,
      height: 10,
      borderRadius: '50%',
      background: color,
      pointerEvents: 'none',
    }}
  />
);

// Card play effect
const CardPlayEffect = ({ color, position }) => {
  const particles = Array.from({ length: 12 }, (_, i) => ({
    id: i,
    delay: Math.random() * 0.2,
  }));

  return (
    <div className="card-play-effect" style={{ position: 'absolute', ...position }}>
      {particles.map(p => (
        <Particle key={p.id} color={color} x={0} y={0} delay={p.delay} />
      ))}
    </div>
  );
};

const ColorCardsGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('menu');
  const [deck, setDeck] = useState([]);
  const [discardPile, setDiscardPile] = useState([]);
  const [playerHand, setPlayerHand] = useState([]);
  const [aiHands, setAiHands] = useState([[], [], []]);
  const [currentPlayer, setCurrentPlayer] = useState(0);
  const [direction, setDirection] = useState(1);
  const [currentColor, setCurrentColor] = useState(null);
  const [pendingWildCard, setPendingWildCard] = useState(null);
  const [message, setMessage] = useState('');
  const [winner, setWinner] = useState(null);
  const [score, setScore] = useState(0);
  const [playEffect, setPlayEffect] = useState(null);
  const [comboCount, setComboCount] = useState(0);
  const [shakeWrongCard, setShakeWrongCard] = useState(null);
  const [lastPlayedCard, setLastPlayedCard] = useState(null);
  const [aiThinking, setAiThinking] = useState(false);
  const [unoCallActive, setUnoCallActive] = useState(false);
  const [gameTime, setGameTime] = useState(0);

  const scoreRef = useRef(0);
  const comboRef = useRef(0);
  const aiTimeoutRef = useRef(null);
  const timerRef = useRef(null);

  // Timer effect
  useEffect(() => {
    if (gameState === 'playing') {
      timerRef.current = setInterval(() => {
        setGameTime(prev => prev + 1);
      }, 1000);
    }
    return () => {
      if (timerRef.current) clearInterval(timerRef.current);
    };
  }, [gameState]);

  const formatTime = (seconds) => {
    const mins = Math.floor(seconds / 60);
    const secs = seconds % 60;
    return `${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`;
  };

  // Initialize game
  const startGame = useCallback(() => {
    const newDeck = shuffleDeck(generateDeck());

    const newPlayerHand = newDeck.splice(0, 7);
    const newAiHands = [
      newDeck.splice(0, 7),
      newDeck.splice(0, 7),
      newDeck.splice(0, 7),
    ];

    let startCardIndex = newDeck.findIndex(c => c.type === 'number');
    if (startCardIndex === -1) startCardIndex = 0;
    const startCard = newDeck.splice(startCardIndex, 1)[0];

    setDeck(newDeck);
    setDiscardPile([startCard]);
    setPlayerHand(newPlayerHand);
    setAiHands(newAiHands);
    setCurrentPlayer(0);
    setDirection(1);
    setCurrentColor(startCard.color);
    setWinner(null);
    setScore(0);
    scoreRef.current = 0;
    comboRef.current = 0;
    setComboCount(0);
    setLastPlayedCard(null);
    setUnoCallActive(false);
    setGameTime(0);
    setMessage("Your turn! Play a card or draw.");
    setGameState('playing');
  }, []);

  // Get top card
  const getTopCard = useCallback(() => {
    return discardPile[discardPile.length - 1];
  }, [discardPile]);

  // Check if card can be played
  const canPlayCard = useCallback((card) => {
    const topCard = getTopCard();
    if (!topCard) return true;

    if (card.type === 'wild') return true;
    if (card.color === currentColor) return true;
    if (card.value === topCard.value) return true;

    return false;
  }, [getTopCard, currentColor]);

  // Draw card from deck
  const drawCard = useCallback((count = 1) => {
    let newDeck = [...deck];
    let drawnCards = [];

    for (let i = 0; i < count; i++) {
      if (newDeck.length === 0) {
        const topCard = discardPile[discardPile.length - 1];
        newDeck = shuffleDeck(discardPile.slice(0, -1).map(c => ({ ...c, id: Math.random() })));
        setDiscardPile([topCard]);
      }

      if (newDeck.length > 0) {
        drawnCards.push(newDeck.pop());
      }
    }

    setDeck(newDeck);
    return drawnCards;
  }, [deck, discardPile]);

  // Next player
  const nextPlayer = useCallback(() => {
    setCurrentPlayer(prev => {
      let next = prev + direction;
      if (next > 3) next = 0;
      if (next < 0) next = 3;
      return next;
    });
  }, [direction]);

  // Trigger card play effect
  const triggerPlayEffect = useCallback((color) => {
    setPlayEffect({ color: COLOR_HEX[color] || COLOR_HEX.wild, id: Date.now() });
    setTimeout(() => setPlayEffect(null), 800);
  }, []);

  // Play a card
  const playCard = useCallback((card, fromHand, setHand, playerIndex) => {
    const newHand = fromHand.filter(c => c.id !== card.id);
    setHand(newHand);

    setDiscardPile(prev => [...prev, card]);
    setLastPlayedCard(card);
    triggerPlayEffect(card.color);

    if (playerIndex === 0) {
      comboRef.current++;
      setComboCount(comboRef.current);
    }

    if (card.type === 'wild') {
      if (playerIndex === 0) {
        setPendingWildCard(card);
        setGameState('choosingColor');
        return false;
      } else {
        const colorCounts = {};
        COLORS.forEach(c => colorCounts[c] = 0);
        aiHands[playerIndex - 1].forEach(c => {
          if (c.color !== 'wild') colorCounts[c.color]++;
        });
        const chosenColor = COLORS.reduce((a, b) => colorCounts[a] > colorCounts[b] ? a : b);
        setCurrentColor(chosenColor);
        setMessage(`${AI_NAMES[playerIndex - 1]} chose ${chosenColor}!`);

        if (card.value === 'wild4') {
          let nextIdx = currentPlayer + direction;
          if (nextIdx > 3) nextIdx = 0;
          if (nextIdx < 0) nextIdx = 3;

          setTimeout(() => {
            const drawnCards = drawCard(4);
            if (nextIdx === 0) {
              setPlayerHand(prev => [...prev, ...drawnCards]);
              setMessage("You drew 4 cards!");
            } else {
              const newAiHands = [...aiHands];
              newAiHands[nextIdx - 1] = [...newAiHands[nextIdx - 1], ...drawnCards];
              setAiHands(newAiHands);
            }
            setCurrentPlayer(prev => {
              let next = prev + direction * 2;
              while (next > 3) next -= 4;
              while (next < 0) next += 4;
              return next;
            });
          }, 500);
          return false;
        }
      }
    } else {
      setCurrentColor(card.color);
    }

    if (card.value === 'skip') {
      setMessage(`Skip! Next player loses their turn.`);
      setCurrentPlayer(prev => {
        let next = prev + direction * 2;
        while (next > 3) next -= 4;
        while (next < 0) next += 4;
        return next;
      });
      return false;
    }

    if (card.value === 'reverse') {
      setDirection(prev => -prev);
      setMessage("Reverse!");
    }

    if (card.value === 'draw2') {
      let nextIdx = currentPlayer + direction;
      if (nextIdx > 3) nextIdx = 0;
      if (nextIdx < 0) nextIdx = 3;

      setTimeout(() => {
        const drawnCards = drawCard(2);
        if (nextIdx === 0) {
          setPlayerHand(prev => [...prev, ...drawnCards]);
          setMessage("You drew 2 cards!");
        } else {
          const newAiHands = [...aiHands];
          newAiHands[nextIdx - 1] = [...newAiHands[nextIdx - 1], ...drawnCards];
          setAiHands(newAiHands);
        }
        setCurrentPlayer(prev => {
          let next = prev + direction * 2;
          while (next > 3) next -= 4;
          while (next < 0) next += 4;
          return next;
        });
      }, 500);
      return false;
    }

    return true;
  }, [currentPlayer, direction, aiHands, drawCard, triggerPlayEffect]);

  // Handle human card play
  const handlePlayCard = useCallback((card) => {
    if (currentPlayer !== 0 || gameState !== 'playing') return;
    if (!canPlayCard(card)) {
      setShakeWrongCard(card.id);
      setTimeout(() => setShakeWrongCard(null), 400);
      setMessage("Can't play that card!");
      return;
    }

    const shouldProceed = playCard(card, playerHand, setPlayerHand, 0);

    if (playerHand.length === 2) {
      setUnoCallActive(true);
      scoreRef.current += UNO_CALL_BONUS;
    }

    if (playerHand.length === 1) {
      setWinner(0);
      scoreRef.current = WIN_BONUS;

      if (comboRef.current >= 3) {
        scoreRef.current += comboRef.current * COMBO_BONUS;
      }

      aiHands.forEach(hand => {
        scoreRef.current += hand.length * CARD_BONUS;
      });

      setScore(scoreRef.current);
      setGameState('gameOver');
      return;
    }

    if (shouldProceed) {
      nextPlayer();
    }
  }, [currentPlayer, gameState, canPlayCard, playCard, playerHand, aiHands, nextPlayer]);

  // Handle human draw
  const handleDraw = useCallback(() => {
    if (currentPlayer !== 0 || gameState !== 'playing') return;

    const drawnCards = drawCard(1);
    setPlayerHand(prev => [...prev, ...drawnCards]);
    setMessage("You drew a card.");

    comboRef.current = 0;
    setComboCount(0);
    setUnoCallActive(false);

    nextPlayer();
  }, [currentPlayer, gameState, drawCard, nextPlayer]);

  // Choose wild color
  const handleChooseColor = useCallback((color) => {
    setCurrentColor(color);
    setMessage(`You chose ${color}!`);
    setPendingWildCard(null);
    setGameState('playing');

    if (pendingWildCard?.value === 'wild4') {
      let nextIdx = direction === 1 ? 1 : 3;

      const drawnCards = drawCard(4);
      const newAiHands = [...aiHands];
      newAiHands[nextIdx - 1] = [...newAiHands[nextIdx - 1], ...drawnCards];
      setAiHands(newAiHands);

      setCurrentPlayer(direction === 1 ? 2 : 2);
    } else {
      nextPlayer();
    }

    if (playerHand.length === 0) {
      setWinner(0);
      scoreRef.current = WIN_BONUS;
      if (comboRef.current >= 3) {
        scoreRef.current += comboRef.current * COMBO_BONUS;
      }
      aiHands.forEach(hand => {
        scoreRef.current += hand.length * CARD_BONUS;
      });
      setScore(scoreRef.current);
      setGameState('gameOver');
    }
  }, [direction, pendingWildCard, drawCard, aiHands, playerHand, nextPlayer]);

  // Handle ONE button click
  const handleOneCall = useCallback(() => {
    if (playerHand.length === 1 && !unoCallActive) {
      setUnoCallActive(true);
      scoreRef.current += UNO_CALL_BONUS;
      setMessage("ONE!");
    }
  }, [playerHand.length, unoCallActive]);

  // AI turn
  useEffect(() => {
    if (gameState !== 'playing' || currentPlayer === 0) return;

    setAiThinking(true);

    aiTimeoutRef.current = setTimeout(() => {
      const aiIndex = currentPlayer - 1;
      const hand = aiHands[aiIndex];

      const playableCards = hand.filter(canPlayCard);

      if (playableCards.length > 0) {
        const sortedPlayable = [...playableCards].sort((a, b) => {
          if (a.type === 'special' && b.type !== 'special') return -1;
          if (b.type === 'special' && a.type !== 'special') return 1;
          if (a.color === currentColor && b.color !== currentColor) return -1;
          if (b.color === currentColor && a.color !== currentColor) return 1;
          if (a.type === 'wild') return 1;
          if (b.type === 'wild') return -1;
          return 0;
        });

        const cardToPlay = sortedPlayable[0];
        const newHand = hand.filter(c => c.id !== cardToPlay.id);

        const newAiHands = [...aiHands];
        newAiHands[aiIndex] = newHand;
        setAiHands(newAiHands);

        setDiscardPile(prev => [...prev, cardToPlay]);
        setLastPlayedCard(cardToPlay);
        triggerPlayEffect(cardToPlay.color);

        setAiThinking(false);

        if (newHand.length === 0) {
          setWinner(currentPlayer);
          setMessage(`${AI_NAMES[aiIndex]} wins!`);
          setScore(0);
          setGameState('gameOver');
          return;
        }

        if (newHand.length === 1) {
          setMessage(`${AI_NAMES[aiIndex]} calls ONE!`);
        }

        if (cardToPlay.type === 'wild') {
          const colorCounts = {};
          COLORS.forEach(c => colorCounts[c] = 0);
          newHand.forEach(c => {
            if (c.color !== 'wild') colorCounts[c.color]++;
          });
          const chosenColor = COLORS.reduce((a, b) => colorCounts[a] > colorCounts[b] ? a : b);
          setCurrentColor(chosenColor);
          setMessage(`${AI_NAMES[aiIndex]} played Wild and chose ${chosenColor}!`);

          if (cardToPlay.value === 'wild4') {
            let nextIdx = currentPlayer + direction;
            if (nextIdx > 3) nextIdx = 0;
            if (nextIdx < 0) nextIdx = 3;

            setTimeout(() => {
              const drawnCards = drawCard(4);
              if (nextIdx === 0) {
                setPlayerHand(prev => [...prev, ...drawnCards]);
                setMessage("You drew 4 cards!");
              } else {
                const newer = [...newAiHands];
                newer[nextIdx - 1] = [...newer[nextIdx - 1], ...drawnCards];
                setAiHands(newer);
              }
              setCurrentPlayer(prev => {
                let next = prev + direction * 2;
                while (next > 3) next -= 4;
                while (next < 0) next += 4;
                return next;
              });
            }, 800);
            return;
          }

          nextPlayer();
          return;
        }

        setCurrentColor(cardToPlay.color);
        setMessage(`${AI_NAMES[aiIndex]} played a card.`);

        if (cardToPlay.value === 'skip') {
          setMessage(`${AI_NAMES[aiIndex]} played Skip!`);
          setCurrentPlayer(prev => {
            let next = prev + direction * 2;
            while (next > 3) next -= 4;
            while (next < 0) next += 4;
            return next;
          });
          return;
        }

        if (cardToPlay.value === 'reverse') {
          setDirection(prev => -prev);
          setMessage(`${AI_NAMES[aiIndex]} played Reverse!`);
          nextPlayer();
          return;
        }

        if (cardToPlay.value === 'draw2') {
          let nextIdx = currentPlayer + direction;
          if (nextIdx > 3) nextIdx = 0;
          if (nextIdx < 0) nextIdx = 3;

          setTimeout(() => {
            const drawnCards = drawCard(2);
            if (nextIdx === 0) {
              setPlayerHand(prev => [...prev, ...drawnCards]);
              setMessage("You drew 2 cards!");
            } else {
              const newer = [...newAiHands];
              newer[nextIdx - 1] = [...newer[nextIdx - 1], ...drawnCards];
              setAiHands(newer);
            }
            setCurrentPlayer(prev => {
              let next = prev + direction * 2;
              while (next > 3) next -= 4;
              while (next < 0) next += 4;
              return next;
            });
          }, 800);
          return;
        }

        nextPlayer();
      } else {
        const drawnCards = drawCard(1);
        const newAiHands = [...aiHands];
        newAiHands[aiIndex] = [...newAiHands[aiIndex], ...drawnCards];
        setAiHands(newAiHands);
        setMessage(`${AI_NAMES[aiIndex]} drew a card.`);
        setAiThinking(false);
        nextPlayer();
      }
    }, 1200);

    return () => {
      if (aiTimeoutRef.current) clearTimeout(aiTimeoutRef.current);
    };
  }, [currentPlayer, gameState, aiHands, canPlayCard, currentColor, direction, drawCard, nextPlayer, triggerPlayEffect]);

  // Update message for player turn
  useEffect(() => {
    if (gameState === 'playing' && currentPlayer === 0) {
      setMessage("Your turn! Play a card or draw.");
    }
  }, [currentPlayer, gameState]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  // Render card with owl mascot
  const renderCard = (card, onClick, isPlayable = true, index = 0, total = 1, isShaking = false) => {
    const bgGradient = COLOR_GRADIENTS[card.color] || COLOR_GRADIENTS.wild;
    const displayValue = {
      skip: '⊘',
      reverse: '⟲',
      draw2: '+2',
      wild: '★',
      wild4: '+4',
    }[card.value] || card.value;

    // Calculate fan layout position
    const spreadAngle = Math.min(5, 60 / total);
    const startAngle = -(total - 1) * spreadAngle / 2;
    const rotation = startAngle + index * spreadAngle;
    const xOffset = (index - (total - 1) / 2) * 30;

    return (
      <motion.button
        key={card.id}
        className={`game-card ${isPlayable ? 'playable' : ''} ${isShaking ? 'shake-card' : ''}`}
        style={{
          background: bgGradient,
          '--card-rotation': `${rotation}deg`,
          left: `calc(50% + ${xOffset}px)`,
          transform: `translateX(-50%) rotate(${rotation}deg)`,
        }}
        onClick={onClick}
        disabled={!isPlayable}
        initial={{ opacity: 0, y: 50, scale: 0.8 }}
        animate={{
          opacity: 1,
          y: 0,
          scale: 1,
        }}
        exit={{ opacity: 0, scale: 0.5, y: -50 }}
        transition={{
          duration: 0.3,
          delay: index * 0.03,
        }}
        whileHover={isPlayable ? {
          y: -25,
          scale: 1.15,
          rotate: 0,
          zIndex: 100,
          boxShadow: '0 15px 35px rgba(0, 0, 0, 0.5)',
        } : {}}
        whileTap={isPlayable ? { scale: 0.95 } : {}}
      >
        <span className="card-corner top-left">{displayValue}</span>
        <span className="card-owl">🦉</span>
        <span className="card-value">{displayValue}</span>
        <span className="card-corner bottom-right">{displayValue}</span>
        {card.type === 'wild' && (
          <div className="wild-colors">
            <span style={{ background: COLOR_HEX.red }} />
            <span style={{ background: COLOR_HEX.blue }} />
            <span style={{ background: COLOR_HEX.green }} />
            <span style={{ background: COLOR_HEX.yellow }} />
          </div>
        )}
      </motion.button>
    );
  };

  // Render discard card (no fan layout)
  const renderDiscardCard = (card) => {
    const bgGradient = COLOR_GRADIENTS[card.color] || COLOR_GRADIENTS.wild;
    const displayValue = {
      skip: '⊘',
      reverse: '⟲',
      draw2: '+2',
      wild: '★',
      wild4: '+4',
    }[card.value] || card.value;

    return (
      <div
        className="game-card"
        style={{ background: bgGradient, cursor: 'default' }}
      >
        <span className="card-corner top-left">{displayValue}</span>
        <span className="card-owl">🦉</span>
        <span className="card-value">{displayValue}</span>
        <span className="card-corner bottom-right">{displayValue}</span>
        {card.type === 'wild' && (
          <div className="wild-colors">
            <span style={{ background: COLOR_HEX.red }} />
            <span style={{ background: COLOR_HEX.blue }} />
            <span style={{ background: COLOR_HEX.green }} />
            <span style={{ background: COLOR_HEX.yellow }} />
          </div>
        )}
      </div>
    );
  };

  return (
    <div className="color-cards-overlay">
      <div className="color-cards-container">
        {/* Menu */}
        {gameState === 'menu' && (
          <motion.div
            className="cards-menu"
            initial={{ opacity: 0, scale: 0.9 }}
            animate={{ opacity: 1, scale: 1 }}
            transition={{ duration: 0.4 }}
          >
            <motion.h1
              className="cards-title"
              initial={{ y: -30 }}
              animate={{ y: 0 }}
              transition={{ type: 'spring', stiffness: 200 }}
            >
              <span>🦉</span> Color Cards <span>🦉</span>
            </motion.h1>
            <p className="cards-subtitle">Match colors or numbers to win!</p>
            <div className="cards-instructions-box">
              <p>🎴 Play cards matching the color or number</p>
              <p>⚡ Use special cards to skip or reverse</p>
              <p>🏆 Be the first to empty your hand!</p>
            </div>
            <motion.button
              className="start-button"
              onClick={startGame}
              whileHover={{ scale: 1.05 }}
              whileTap={{ scale: 0.95 }}
            >
              Start Game!
            </motion.button>
            <button className="close-button-menu" onClick={onClose}>
              Back to Games
            </button>
          </motion.div>
        )}

        {/* Playing / Choosing Color */}
        {(gameState === 'playing' || gameState === 'choosingColor') && (
          <div className="game-area">
            {/* Header with Timer */}
            <div className="game-header">
              <button className="menu-btn" onClick={onClose}>
                <div className="menu-icon">
                  <span></span>
                  <span></span>
                  <span></span>
                </div>
              </button>
              <div className="game-timer">
                <span className="timer-icon">⏱</span>
                <span>{formatTime(gameTime)}</span>
              </div>
              <div className="header-spacer" />
            </div>

            {/* Central Play Area */}
            <div className="central-area">
              <div className="play-mat">
                {/* Direction arrows */}
                <div className="direction-arrows">
                  {direction === 1 ? (
                    <>
                      <svg className="direction-arrow-svg top" viewBox="0 0 60 60">
                        <path d="M30 10 L45 35 L37 35 L37 50 L23 50 L23 35 L15 35 Z" />
                      </svg>
                      <svg className="direction-arrow-svg right" viewBox="0 0 60 60">
                        <path d="M30 10 L45 35 L37 35 L37 50 L23 50 L23 35 L15 35 Z" />
                      </svg>
                    </>
                  ) : (
                    <>
                      <svg className="direction-arrow-svg bottom" viewBox="0 0 60 60">
                        <path d="M30 10 L45 35 L37 35 L37 50 L23 50 L23 35 L15 35 Z" />
                      </svg>
                      <svg className="direction-arrow-svg left" viewBox="0 0 60 60">
                        <path d="M30 10 L45 35 L37 35 L37 50 L23 50 L23 35 L15 35 Z" />
                      </svg>
                    </>
                  )}
                </div>

                {/* Players around the mat */}
                <div className="players-circle">
                  {/* Top player (AI 2) */}
                  <div className={`player-avatar-spot top ${currentPlayer === 2 ? 'active' : ''}`}>
                    <div className="avatar-circle" style={{ background: 'linear-gradient(135deg, #9C27B0, #7B1FA2)' }}>
                      {AI_AVATARS[1]}
                      <span className="card-count-badge">{aiHands[1].length}</span>
                      {aiHands[1].length === 1 && <span className="uno-badge">ONE!</span>}
                    </div>
                    <span className="player-name">
                      {AI_NAMES[1]}
                      {currentPlayer === 2 && aiThinking && <span className="thinking-indicator">...</span>}
                    </span>
                  </div>

                  {/* Left player (AI 1) */}
                  <div className={`player-avatar-spot left ${currentPlayer === 1 ? 'active' : ''}`}>
                    <div className="avatar-circle" style={{ background: 'linear-gradient(135deg, #4CAF50, #388E3C)' }}>
                      {AI_AVATARS[0]}
                      <span className="card-count-badge">{aiHands[0].length}</span>
                      {aiHands[0].length === 1 && <span className="uno-badge">ONE!</span>}
                    </div>
                    <span className="player-name">
                      {AI_NAMES[0]}
                      {currentPlayer === 1 && aiThinking && <span className="thinking-indicator">...</span>}
                    </span>
                  </div>

                  {/* Right player (AI 3) */}
                  <div className={`player-avatar-spot right ${currentPlayer === 3 ? 'active' : ''}`}>
                    <div className="avatar-circle" style={{ background: 'linear-gradient(135deg, #2196F3, #1976D2)' }}>
                      {AI_AVATARS[2]}
                      <span className="card-count-badge">{aiHands[2].length}</span>
                      {aiHands[2].length === 1 && <span className="uno-badge">ONE!</span>}
                    </div>
                    <span className="player-name">
                      {AI_NAMES[2]}
                      {currentPlayer === 3 && aiThinking && <span className="thinking-indicator">...</span>}
                    </span>
                  </div>
                </div>

                {/* Center card piles */}
                <div className="center-cards">
                  <motion.div
                    className="deck-pile"
                    onClick={currentPlayer === 0 && gameState === 'playing' ? handleDraw : undefined}
                    whileHover={currentPlayer === 0 ? { scale: 1.05 } : {}}
                    whileTap={currentPlayer === 0 ? { scale: 0.95 } : {}}
                  >
                    <div className="deck-card">
                      <span className="deck-text">DRAW</span>
                    </div>
                    <span className="deck-count">{deck.length}</span>
                  </motion.div>

                  <div className="discard-pile">
                    <AnimatePresence mode="popLayout">
                      {discardPile.length > 0 && (
                        <motion.div
                          key={discardPile[discardPile.length - 1].id}
                          className="discard-card-wrapper"
                          initial={{ scale: 0.5, rotateZ: -20, y: -100 }}
                          animate={{ scale: 1, rotateZ: 0, y: 0 }}
                          transition={{ type: 'spring', stiffness: 300, damping: 20 }}
                        >
                          {renderDiscardCard(getTopCard())}
                        </motion.div>
                      )}
                    </AnimatePresence>
                    {currentColor && (
                      <motion.div
                        className="current-color"
                        style={{ background: COLOR_GRADIENTS[currentColor] }}
                        initial={{ scale: 0 }}
                        animate={{ scale: 1 }}
                        key={currentColor}
                        transition={{ type: 'spring', stiffness: 300 }}
                      >
                        <span className="color-dot" style={{ background: COLOR_HEX[currentColor] }} />
                        {currentColor}
                      </motion.div>
                    )}

                    <AnimatePresence>
                      {playEffect && (
                        <CardPlayEffect
                          key={playEffect.id}
                          color={playEffect.color}
                          position={{ left: '50%', top: '50%' }}
                        />
                      )}
                    </AnimatePresence>
                  </div>
                </div>
              </div>

              {/* ONE Button */}
              <div className="one-button-container">
                <button
                  className={`one-button ${playerHand.length <= 2 ? 'active' : ''}`}
                  onClick={handleOneCall}
                >
                  ONE
                </button>
              </div>
            </div>

            {/* Message */}
            <motion.div
              className="game-message"
              key={message}
              initial={{ opacity: 0, y: -10 }}
              animate={{ opacity: 1, y: 0 }}
              transition={{ duration: 0.2 }}
            >
              {message}
            </motion.div>

            {/* Combo indicator */}
            {comboCount >= 2 && (
              <motion.div
                className="combo-indicator"
                initial={{ scale: 0 }}
                animate={{ scale: 1 }}
                transition={{ type: 'spring', stiffness: 300 }}
              >
                🔥 {comboCount}x Combo!
              </motion.div>
            )}

            {/* UNO call indicator */}
            {unoCallActive && playerHand.length === 1 && (
              <motion.div
                className="uno-call-indicator"
                initial={{ scale: 0, rotate: -10 }}
                animate={{ scale: 1, rotate: 0 }}
                transition={{ type: 'spring', stiffness: 300 }}
              >
                ONE!
              </motion.div>
            )}

            {/* Player hand */}
            <div className="player-hand-section">
              <div className="player-hand">
                <AnimatePresence>
                  {playerHand.map((card, index) =>
                    renderCard(
                      card,
                      () => handlePlayCard(card),
                      currentPlayer === 0 && gameState === 'playing' && canPlayCard(card),
                      index,
                      playerHand.length,
                      shakeWrongCard === card.id
                    )
                  )}
                </AnimatePresence>
              </div>
            </div>

            {/* Your turn indicator */}
            <AnimatePresence>
              {currentPlayer === 0 && gameState === 'playing' && (
                <motion.div
                  className="turn-indicator"
                  initial={{ scale: 0, y: 20 }}
                  animate={{ scale: 1, y: 0 }}
                  exit={{ scale: 0, y: 20 }}
                  transition={{ type: 'spring', stiffness: 300 }}
                >
                  ✨ Your Turn! ✨
                </motion.div>
              )}
            </AnimatePresence>
          </div>
        )}

        {/* Color Chooser Modal */}
        <AnimatePresence>
          {gameState === 'choosingColor' && (
            <motion.div
              className="color-chooser-overlay"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
            >
              <motion.div
                className="color-chooser"
                initial={{ scale: 0.8, y: 50 }}
                animate={{ scale: 1, y: 0 }}
                exit={{ scale: 0.8, y: 50 }}
                transition={{ type: 'spring', stiffness: 300 }}
              >
                <h3>Choose a Color</h3>
                <div className="color-options">
                  {COLORS.map((color, index) => (
                    <motion.button
                      key={color}
                      className="color-option"
                      style={{ background: COLOR_GRADIENTS[color] }}
                      onClick={() => handleChooseColor(color)}
                      initial={{ opacity: 0, scale: 0 }}
                      animate={{ opacity: 1, scale: 1 }}
                      transition={{ delay: index * 0.1 }}
                      whileHover={{ scale: 1.1, rotate: 5 }}
                      whileTap={{ scale: 0.95 }}
                    >
                      {color}
                    </motion.button>
                  ))}
                </div>
              </motion.div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Game Over */}
        <AnimatePresence>
          {gameState === 'gameOver' && (
            <motion.div
              className="game-over-screen"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
            >
              <motion.div
                className="game-over-content"
                initial={{ scale: 0.8, y: 50 }}
                animate={{ scale: 1, y: 0 }}
                transition={{ type: 'spring', stiffness: 200 }}
              >
                <motion.h2
                  className="game-over-title"
                  initial={{ y: -20 }}
                  animate={{ y: 0 }}
                  transition={{ delay: 0.2 }}
                >
                  {winner === 0 ? (
                    <>
                      <span>🏆</span> You Win! <span>🏆</span>
                    </>
                  ) : (
                    <>
                      <span>😢</span> {AI_NAMES[winner - 1]} Wins <span>😢</span>
                    </>
                  )}
                </motion.h2>

                <div className="final-stats">
                  <motion.div
                    className="final-stat highlight"
                    initial={{ opacity: 0, x: -20 }}
                    animate={{ opacity: 1, x: 0 }}
                    transition={{ delay: 0.3 }}
                  >
                    <span className="final-stat-icon">🪙</span>
                    <span className="final-stat-label">Score</span>
                    <span className="final-stat-value coins">{score}</span>
                  </motion.div>
                  <motion.div
                    className="final-stat"
                    initial={{ opacity: 0, x: -20 }}
                    animate={{ opacity: 1, x: 0 }}
                    transition={{ delay: 0.35 }}
                  >
                    <span className="final-stat-icon">⏱</span>
                    <span className="final-stat-label">Time</span>
                    <span className="final-stat-value">{formatTime(gameTime)}</span>
                  </motion.div>
                  {comboCount >= 3 && (
                    <motion.div
                      className="final-stat"
                      initial={{ opacity: 0, x: -20 }}
                      animate={{ opacity: 1, x: 0 }}
                      transition={{ delay: 0.4 }}
                    >
                      <span className="final-stat-icon">🔥</span>
                      <span className="final-stat-label">Max Combo</span>
                      <span className="final-stat-value">{comboCount}x</span>
                    </motion.div>
                  )}
                </div>

                {winner === 0 && (
                  <motion.div
                    className="achievement-badge"
                    initial={{ scale: 0 }}
                    animate={{ scale: 1 }}
                    transition={{ delay: 0.5, type: 'spring', stiffness: 300 }}
                  >
                    <span>🦉</span> Card Master! <span>🦉</span>
                  </motion.div>
                )}

                <div className="game-over-buttons">
                  <motion.button
                    className="play-again-btn"
                    onClick={startGame}
                    initial={{ opacity: 0 }}
                    animate={{ opacity: 1 }}
                    transition={{ delay: 0.6 }}
                    whileHover={{ scale: 1.02 }}
                    whileTap={{ scale: 0.98 }}
                  >
                    Play Again
                  </motion.button>
                  <motion.button
                    className="claim-rewards-btn"
                    onClick={handleFinish}
                    initial={{ opacity: 0 }}
                    animate={{ opacity: 1 }}
                    transition={{ delay: 0.7 }}
                    whileHover={{ scale: 1.02 }}
                    whileTap={{ scale: 0.98 }}
                  >
                    Claim {score} Coins! 🪙
                  </motion.button>
                </div>
              </motion.div>
            </motion.div>
          )}
        </AnimatePresence>
      </div>
    </div>
  );
};

export default ColorCardsGame;
