import { motion, AnimatePresence } from 'framer-motion';
import './NumberCaller.css';

const getLetterForNumber = (num) => {
  if (num >= 1 && num <= 15) return 'B';
  if (num >= 16 && num <= 30) return 'I';
  if (num >= 31 && num <= 45) return 'N';
  if (num >= 46 && num <= 60) return 'G';
  return 'O';
};

const NumberCaller = ({
  currentNumber,
  calledNumbers = [],
  onCallNumber,
  isLoading = false,
  gameOver = false,
  nextCallIn = null,
  gameStarted = false
}) => {
  // Show last 6 called numbers as balls at the top
  const displayBalls = calledNumbers.slice(-6).reverse();
  const recentNumbers = calledNumbers.slice(-5).reverse();
  const currentLetter = currentNumber ? getLetterForNumber(currentNumber) : null;

  return (
    <div className="number-caller">
      {/* Called balls row - like the screenshot */}
      <div className="called-balls-row">
        {displayBalls.map((num, index) => {
          const letter = getLetterForNumber(num);
          return (
            <motion.div
              key={num}
              className={`called-ball letter-${letter}`}
              initial={{ scale: 0, y: -20 }}
              animate={{ scale: 1 - index * 0.05, y: 0 }}
              style={{ opacity: 1 - index * 0.1 }}
            >
              <span className="ball-letter">{letter}</span>
              <span className="ball-number">{num}</span>
            </motion.div>
          );
        })}
      </div>

      {/* Current Number Display */}
      <div className="current-number-section">
        <div className="caller-label">Current Number</div>
        <AnimatePresence mode="wait">
          {currentNumber ? (
            <motion.div
              key={currentNumber}
              className={`current-number-ball letter-${currentLetter}`}
              initial={{ scale: 0, rotate: -180 }}
              animate={{ scale: 1, rotate: 0 }}
              exit={{ scale: 0, rotate: 180 }}
              transition={{
                type: 'spring',
                stiffness: 300,
                damping: 20
              }}
            >
              <span className="letter">{currentLetter}</span>
              <span className="number">{currentNumber}</span>
            </motion.div>
          ) : (
            <motion.div
              className="current-number-ball waiting"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
            >
              <span className="waiting-text">?</span>
            </motion.div>
          )}
        </AnimatePresence>
      </div>

      {/* Auto-call indicator or Game Over */}
      <div className="auto-call-indicator">
        {gameOver ? (
          <div className="game-over-text">Game Over!</div>
        ) : gameStarted ? (
          <div className="next-call-display">
            <span className="next-label">Next number in</span>
            <motion.span
              className="next-countdown"
              key={nextCallIn}
              initial={{ scale: 1.2 }}
              animate={{ scale: 1 }}
            >
              {nextCallIn}s
            </motion.span>
          </div>
        ) : (
          <div className="waiting-to-start">Waiting to start...</div>
        )}
      </div>

      {/* Recent Numbers */}
      <div className="recent-numbers">
        <div className="recent-label">Recent Numbers</div>
        <div className="recent-balls">
          {recentNumbers.map((num, index) => {
            const letter = getLetterForNumber(num);
            return (
              <motion.div
                key={num}
                className={`recent-ball letter-${letter}`}
                initial={{ scale: 0, x: -20 }}
                animate={{ scale: 1 - index * 0.08, x: 0 }}
                style={{ opacity: 1 - index * 0.12 }}
              >
                <span className="recent-letter">{letter}</span>
                <span className="recent-number">{num}</span>
              </motion.div>
            );
          })}
        </div>
      </div>

      {/* Called Numbers Count */}
      <div className="numbers-called-count">
        {calledNumbers.length} / 75 numbers called
      </div>
    </div>
  );
};

export default NumberCaller;
