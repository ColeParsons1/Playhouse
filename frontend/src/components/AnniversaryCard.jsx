import { useState, useEffect } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './AnniversaryCard.css';

const AnniversaryCard = ({ onClose }) => {
  const [isOpen, setIsOpen] = useState(false);
  const [showHearts, setShowHearts] = useState(false);
  const [showMessage, setShowMessage] = useState(false);

  useEffect(() => {
    // Auto-open card after a moment
    const timer = setTimeout(() => setIsOpen(true), 800);
    return () => clearTimeout(timer);
  }, []);

  useEffect(() => {
    if (isOpen) {
      const heartTimer = setTimeout(() => setShowHearts(true), 500);
      const messageTimer = setTimeout(() => setShowMessage(true), 1000);
      return () => {
        clearTimeout(heartTimer);
        clearTimeout(messageTimer);
      };
    }
  }, [isOpen]);

  // Floating hearts
  const hearts = Array.from({ length: 20 }, (_, i) => ({
    id: i,
    left: Math.random() * 100,
    delay: Math.random() * 3,
    duration: 3 + Math.random() * 2,
    size: 15 + Math.random() * 20,
  }));

  return (
    <div className="anniversary-overlay">
      <div className="anniversary-scene">
        {/* Floating hearts background */}
        <AnimatePresence>
          {showHearts && hearts.map((heart) => (
            <motion.div
              key={heart.id}
              className="floating-heart"
              initial={{ y: '110vh', opacity: 0 }}
              animate={{ y: '-10vh', opacity: [0, 1, 1, 0] }}
              transition={{
                duration: heart.duration,
                delay: heart.delay,
                repeat: Infinity,
                ease: 'linear',
              }}
              style={{
                left: `${heart.left}%`,
                fontSize: `${heart.size}px`,
              }}
            >
              💕
            </motion.div>
          ))}
        </AnimatePresence>

        {/* The Card */}
        <motion.div
          className={`anniversary-card ${isOpen ? 'open' : ''}`}
          initial={{ scale: 0.5, rotateY: 0 }}
          animate={{ scale: 1, rotateY: isOpen ? 180 : 0 }}
          transition={{ duration: 0.8, type: 'spring', stiffness: 100 }}
          onClick={() => !isOpen && setIsOpen(true)}
        >
          {/* Card Front */}
          <div className="card-front">
            <div className="card-front-content">
              <motion.div
                className="front-decoration"
                animate={{ scale: [1, 1.05, 1] }}
                transition={{ duration: 2, repeat: Infinity }}
              >
                <span className="big-heart">💝</span>
              </motion.div>
              <h2>Happy 1 Year!</h2>
              <p className="tap-hint">tap to open</p>

              {/* Cute decorative elements */}
              <div className="front-flowers">
                <span>🌸</span>
                <span>🌷</span>
                <span>🌺</span>
              </div>
            </div>
          </div>

          {/* Card Inside */}
          <div className="card-inside">
            <AnimatePresence>
              {showMessage && (
                <motion.div
                  className="card-message"
                  initial={{ opacity: 0, y: 20 }}
                  animate={{ opacity: 1, y: 0 }}
                  transition={{ duration: 0.5 }}
                >
                  {/* Mittens Drawing */}
                  <div className="mittens-drawing">
                    <svg viewBox="0 0 200 180" className="mittens-svg">
                      {/* Dog body */}
                      <ellipse cx="100" cy="140" rx="45" ry="30" fill="#1a1a1a" />

                      {/* White chest */}
                      <ellipse cx="100" cy="145" rx="20" ry="18" fill="#ffffff" />

                      {/* Head */}
                      <ellipse cx="100" cy="80" rx="40" ry="35" fill="#1a1a1a" />

                      {/* Snout */}
                      <ellipse cx="100" cy="95" rx="18" ry="12" fill="#2a2a2a" />

                      {/* White blaze on face */}
                      <path d="M100 55 Q95 70 92 85 Q100 90 108 85 Q105 70 100 55" fill="#ffffff" />

                      {/* Nose */}
                      <ellipse cx="100" cy="98" rx="8" ry="5" fill="#333" />
                      <ellipse cx="100" cy="97" rx="3" ry="2" fill="#666" />

                      {/* Eyes */}
                      <ellipse cx="82" cy="75" rx="10" ry="9" fill="#3d2314" />
                      <ellipse cx="118" cy="75" rx="10" ry="9" fill="#3d2314" />
                      {/* Eye shine */}
                      <circle cx="85" cy="73" r="3" fill="#ffffff" opacity="0.8" />
                      <circle cx="121" cy="73" r="3" fill="#ffffff" opacity="0.8" />
                      {/* Pupils */}
                      <circle cx="82" cy="76" r="5" fill="#1a1a1a" />
                      <circle cx="118" cy="76" r="5" fill="#1a1a1a" />

                      {/* Ears */}
                      <path d="M55 60 Q50 35 65 45 Q70 55 65 70 Z" fill="#1a1a1a" />
                      <path d="M145 60 Q150 35 135 45 Q130 55 135 70 Z" fill="#1a1a1a" />

                      {/* Inner ears */}
                      <path d="M58 55 Q55 42 65 48 Q67 55 65 62 Z" fill="#d4a574" />
                      <path d="M142 55 Q145 42 135 48 Q133 55 135 62 Z" fill="#d4a574" />

                      {/* Front paws */}
                      <ellipse cx="75" cy="165" rx="12" ry="8" fill="#1a1a1a" />
                      <ellipse cx="125" cy="165" rx="12" ry="8" fill="#1a1a1a" />
                      {/* White paw tips */}
                      <ellipse cx="75" cy="168" rx="8" ry="5" fill="#ffffff" />
                      <ellipse cx="125" cy="168" rx="8" ry="5" fill="#ffffff" />

                      {/* Cute mouth/smile */}
                      <path d="M92 102 Q100 108 108 102" stroke="#1a1a1a" strokeWidth="2" fill="none" />

                      {/* Tongue */}
                      <ellipse cx="100" cy="107" rx="5" ry="4" fill="#ff6b8a" />

                      {/* Eyebrows for expression */}
                      <path d="M72 65 Q77 62 87 66" stroke="#1a1a1a" strokeWidth="2" fill="none" />
                      <path d="M128 65 Q123 62 113 66" stroke="#1a1a1a" strokeWidth="2" fill="none" />

                      {/* Fluffy chest detail */}
                      <path d="M85 120 Q90 115 100 118 Q110 115 115 120" stroke="#ffffff" strokeWidth="3" fill="none" />

                      {/* Tail wagging */}
                      <motion.path
                        d="M145 130 Q165 120 170 135 Q175 145 165 150"
                        stroke="#1a1a1a"
                        strokeWidth="12"
                        strokeLinecap="round"
                        fill="none"
                        animate={{ d: [
                          "M145 130 Q165 120 170 135 Q175 145 165 150",
                          "M145 130 Q160 115 175 125 Q180 135 170 145",
                          "M145 130 Q165 120 170 135 Q175 145 165 150"
                        ]}}
                        transition={{ duration: 0.5, repeat: Infinity }}
                      />
                    </svg>
                    <motion.div
                      className="mittens-hearts"
                      animate={{ scale: [1, 1.2, 1] }}
                      transition={{ duration: 1, repeat: Infinity }}
                    >
                      💕
                    </motion.div>
                  </div>

                  <h1 className="anniversary-title">
                    Happy 1 Year Tanvi!
                  </h1>

                  <div className="message-text">
                    <p>
                      I've really enjoyed this past year and I'm excited for you to see this!
                    </p>
                    <p>
                      Although it's not quite complete, I've been working very hard on this, and I want it to be something you actually enjoy!
                    </p>
                    <p>
                      Thank you for being such a joy to us both.
                    </p>
                    <p className="love-line">
                      We love you!
                    </p>
                  </div>

                  <div className="signature">
                    <span>~Cole & Mittens</span>
                    <span className="paw-print">🐾</span>
                  </div>

                  <div className="ps-message">
                    <p>
                      <em>P.S. - Don't think about work too much! Maybe you can distract yourself by helping me debug these games, if you want!</em>
                    </p>
                  </div>

                  {/* Decorative elements */}
                  <div className="card-decorations">
                    <span className="deco deco-1">✨</span>
                    <span className="deco deco-2">🌟</span>
                    <span className="deco deco-3">💫</span>
                    <span className="deco deco-4">🦋</span>
                    <span className="deco deco-5">🌸</span>
                  </div>
                </motion.div>
              )}
            </AnimatePresence>
          </div>
        </motion.div>

        {/* Close button */}
        {isOpen && (
          <motion.button
            className="close-card-btn"
            onClick={onClose}
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            transition={{ delay: 2 }}
          >
            Continue to Games 🎮
          </motion.button>
        )}

        {/* Cute animal friends at the bottom */}
        <div className="animal-friends">
          <motion.span
            animate={{ y: [0, -10, 0] }}
            transition={{ duration: 2, repeat: Infinity, delay: 0 }}
          >
            🐰
          </motion.span>
          <motion.span
            animate={{ y: [0, -10, 0] }}
            transition={{ duration: 2, repeat: Infinity, delay: 0.3 }}
          >
            🦊
          </motion.span>
          <motion.span
            animate={{ y: [0, -10, 0] }}
            transition={{ duration: 2, repeat: Infinity, delay: 0.6 }}
          >
            🐻
          </motion.span>
          <motion.span
            animate={{ y: [0, -10, 0] }}
            transition={{ duration: 2, repeat: Infinity, delay: 0.9 }}
          >
            🐼
          </motion.span>
          <motion.span
            animate={{ y: [0, -10, 0] }}
            transition={{ duration: 2, repeat: Infinity, delay: 1.2 }}
          >
            🦋
          </motion.span>
        </div>
      </div>
    </div>
  );
};

export default AnniversaryCard;
