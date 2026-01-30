import { motion, AnimatePresence } from 'framer-motion';
import { useEffect, useState } from 'react';
import './WinModal.css';

const confettiColors = ['#FF69B4', '#FFD700', '#FF6B6B', '#4ECDC4', '#DDA0DD', '#98D8C8'];

const Confetti = () => {
  const confettiPieces = Array.from({ length: 50 }, (_, i) => ({
    id: i,
    x: Math.random() * 100,
    delay: Math.random() * 2,
    duration: 3 + Math.random() * 2,
    color: confettiColors[Math.floor(Math.random() * confettiColors.length)],
    size: 8 + Math.random() * 8,
    rotation: Math.random() * 360
  }));

  return (
    <div className="confetti-container">
      {confettiPieces.map((piece) => (
        <motion.div
          key={piece.id}
          className="confetti-piece"
          initial={{
            x: `${piece.x}vw`,
            y: -20,
            rotate: 0,
            opacity: 1
          }}
          animate={{
            y: '110vh',
            rotate: piece.rotation + 720,
            opacity: [1, 1, 0]
          }}
          transition={{
            duration: piece.duration,
            delay: piece.delay,
            repeat: Infinity,
            ease: 'linear'
          }}
          style={{
            backgroundColor: piece.color,
            width: piece.size,
            height: piece.size,
          }}
        />
      ))}
    </div>
  );
};

const WinModal = ({ isOpen, onClose, winData, isPlayerWin }) => {
  const [showConfetti, setShowConfetti] = useState(false);

  useEffect(() => {
    if (isOpen && isPlayerWin) {
      setShowConfetti(true);
    }
    return () => setShowConfetti(false);
  }, [isOpen, isPlayerWin]);

  return (
    <AnimatePresence>
      {isOpen && (
        <>
          {showConfetti && <Confetti />}
          <motion.div
            className="modal-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            onClick={onClose}
          >
            <motion.div
              className="win-modal"
              initial={{ scale: 0, rotate: -10 }}
              animate={{ scale: 1, rotate: 0 }}
              exit={{ scale: 0, rotate: 10 }}
              transition={{ type: 'spring', damping: 15 }}
              onClick={(e) => e.stopPropagation()}
            >
              {isPlayerWin ? (
                <>
                  <motion.div
                    className="win-header"
                    initial={{ y: -20, opacity: 0 }}
                    animate={{ y: 0, opacity: 1 }}
                    transition={{ delay: 0.2 }}
                  >
                    <span className="win-emoji">🎉</span>
                    <h2 className="win-title">BINGO!</h2>
                    <span className="win-emoji">🎉</span>
                  </motion.div>

                  <motion.p
                    className="win-message"
                    initial={{ y: 20, opacity: 0 }}
                    animate={{ y: 0, opacity: 1 }}
                    transition={{ delay: 0.4 }}
                  >
                    Congratulations, you won!
                  </motion.p>

                  <motion.div
                    className="rewards-section"
                    initial={{ scale: 0 }}
                    animate={{ scale: 1 }}
                    transition={{ delay: 0.6, type: 'spring' }}
                  >
                    <div className="reward-item">
                      <span className="reward-icon">🪙</span>
                      <span className="reward-amount">+{winData?.coins_earned || 0}</span>
                      <span className="reward-label">Coins</span>
                    </div>
                    {winData?.gems_earned > 0 && (
                      <div className="reward-item gems">
                        <span className="reward-icon">💎</span>
                        <span className="reward-amount">+{winData.gems_earned}</span>
                        <span className="reward-label">Gems</span>
                      </div>
                    )}
                  </motion.div>

                  {winData?.streak > 1 && (
                    <motion.div
                      className="streak-section"
                      initial={{ scale: 0 }}
                      animate={{ scale: 1 }}
                      transition={{ delay: 0.8, type: 'spring' }}
                    >
                      <span className="streak-fire-big">🔥</span>
                      <span className="streak-number">{winData.streak}</span>
                      <span className="streak-text">Win Streak!</span>
                    </motion.div>
                  )}
                </>
              ) : (
                <>
                  <motion.div
                    className="lose-header"
                    initial={{ y: -20, opacity: 0 }}
                    animate={{ y: 0, opacity: 1 }}
                    transition={{ delay: 0.2 }}
                  >
                    <span className="lose-emoji">😢</span>
                    <h2 className="lose-title">Oh no!</h2>
                  </motion.div>

                  <motion.p
                    className="lose-message"
                    initial={{ y: 20, opacity: 0 }}
                    animate={{ y: 0, opacity: 1 }}
                    transition={{ delay: 0.4 }}
                  >
                    {winData?.winner || 'Another player'} got bingo first!
                  </motion.p>

                  <motion.p
                    className="encourage-message"
                    initial={{ opacity: 0 }}
                    animate={{ opacity: 1 }}
                    transition={{ delay: 0.6 }}
                  >
                    Don't worry, you'll get them next time! 💪
                  </motion.p>
                </>
              )}

              <motion.button
                className="close-button cute-button"
                onClick={onClose}
                initial={{ y: 20, opacity: 0 }}
                animate={{ y: 0, opacity: 1 }}
                transition={{ delay: 1 }}
                whileHover={{ scale: 1.05 }}
                whileTap={{ scale: 0.95 }}
              >
                {isPlayerWin ? 'Yay! Play Again!' : 'Try Again!'}
              </motion.button>
            </motion.div>
          </motion.div>
        </>
      )}
    </AnimatePresence>
  );
};

export default WinModal;
