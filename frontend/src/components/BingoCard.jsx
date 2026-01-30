import { motion } from 'framer-motion';
import './BingoCard.css';

const BingoCard = ({
  card,
  onNumberClick,
  calledNumbers = [],
  dauber = 'star',
  isActive = true,
  cardIndex = 0
}) => {
  if (!card || !card.numbers) return null;

  const markedPositions = new Set(card.marked?.map(pos => `${pos[0]}-${pos[1]}`) || []);

  const isNumberCalled = (num) => calledNumbers.includes(num);
  const isMarked = (row, col) => markedPositions.has(`${row}-${col}`);
  const isFreeSpace = (row, col) => row === 2 && col === 2;

  const handleCellClick = (num, row, col) => {
    if (!isActive) return;
    if (isFreeSpace(row, col)) return;
    if (!isNumberCalled(num)) return;
    if (isMarked(row, col)) return;

    onNumberClick?.(card.id, num);
  };

  const getCellClass = (num, row, col) => {
    let classes = ['bingo-cell'];

    if (isFreeSpace(row, col)) {
      classes.push('free-space');
    } else if (isMarked(row, col)) {
      classes.push('marked');
    } else if (isNumberCalled(num)) {
      classes.push('called');
    }

    return classes.join(' ');
  };

  return (
    <motion.div
      className="bingo-card-container"
      initial={{ opacity: 0, scale: 0.8, y: 20 }}
      animate={{ opacity: 1, scale: 1, y: 0 }}
      transition={{ delay: cardIndex * 0.15, duration: 0.4 }}
    >
      <div className="card-label">Card {cardIndex + 1}</div>
      <div className="bingo-card">
        <div className="bingo-card-inner">
          {/* Header */}
          <div className="bingo-header">
            {['B', 'I', 'N', 'G', 'O'].map((letter, i) => (
              <motion.div
                key={letter}
                className="header-cell"
                initial={{ opacity: 0, y: -20 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ delay: 0.1 * i }}
              >
                {letter}
              </motion.div>
            ))}
          </div>

          {/* Grid */}
          <div className="bingo-grid">
            {card.numbers.map((row, rowIndex) =>
              row.map((num, colIndex) => (
                <motion.div
                  key={`${rowIndex}-${colIndex}`}
                  className={getCellClass(num, rowIndex, colIndex)}
                  onClick={() => handleCellClick(num, rowIndex, colIndex)}
                  whileHover={isActive && isNumberCalled(num) && !isMarked(rowIndex, colIndex) ? { scale: 1.05 } : {}}
                  whileTap={isActive && isNumberCalled(num) && !isMarked(rowIndex, colIndex) ? { scale: 0.95 } : {}}
                >
                  {isFreeSpace(rowIndex, colIndex) ? (
                    <span className="free-text">FREE</span>
                  ) : (
                    <>
                      <span className="cell-number">{num}</span>
                      {isMarked(rowIndex, colIndex) && (
                        <motion.span
                          className="dauber-mark"
                          initial={{ scale: 0, rotate: -180 }}
                          animate={{ scale: 1, rotate: 0 }}
                          transition={{ type: 'spring', stiffness: 300, damping: 15 }}
                        >
                          ⭐
                        </motion.span>
                      )}
                    </>
                  )}
                </motion.div>
              ))
            )}
          </div>
        </div>
      </div>
    </motion.div>
  );
};

export default BingoCard;
