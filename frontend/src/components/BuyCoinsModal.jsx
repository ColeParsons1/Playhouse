import { motion, AnimatePresence } from 'framer-motion';
import './BuyCoinsModal.css';

const COIN_PACKAGES = [
  { id: 1, coins: 800, price: 0.99, bonus: null },
  { id: 2, coins: 2800, price: 2.99, bonus: '+16%' },
  { id: 3, coins: 5000, price: 4.99, bonus: '+24%', popular: true },
  { id: 4, coins: 12000, price: 9.99, bonus: '+49%' },
  { id: 5, coins: 75000, price: 49.99, bonus: '+86%', bestValue: true },
];

const BuyCoinsModal = ({ isOpen, onClose, player }) => {
  const handlePurchase = (pkg) => {
    // For now, just show alert - real implementation would use Stripe/Apple Pay
    alert(`Purchase ${pkg.coins.toLocaleString()} coins for $${pkg.price.toFixed(2)}\n\nPayment integration coming soon!`);
  };

  return (
    <AnimatePresence>
      {isOpen && (
        <motion.div
          className="buy-coins-overlay"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onClick={onClose}
        >
          <motion.div
            className="buy-coins-modal"
            initial={{ scale: 0.8, y: 50 }}
            animate={{ scale: 1, y: 0 }}
            exit={{ scale: 0.8, y: 50 }}
            onClick={(e) => e.stopPropagation()}
          >
            <div className="buy-coins-header">
              <h2>
                <span className="coin-icon-large">🪙</span>
                Buy Coins
              </h2>
              <button className="close-btn" onClick={onClose}>✕</button>
            </div>

            <div className="current-balance">
              <span>Current Balance:</span>
              <span className="balance-amount">
                🪙 {player?.coins?.toLocaleString() || 0}
              </span>
            </div>

            <div className="coin-packages">
              {COIN_PACKAGES.map((pkg) => (
                <motion.button
                  key={pkg.id}
                  className={`coin-package ${pkg.popular ? 'popular' : ''} ${pkg.bestValue ? 'best-value' : ''}`}
                  onClick={() => handlePurchase(pkg)}
                  whileHover={{ scale: 1.02 }}
                  whileTap={{ scale: 0.98 }}
                >
                  {pkg.popular && <div className="package-badge popular-badge">POPULAR</div>}
                  {pkg.bestValue && <div className="package-badge best-value-badge">BEST VALUE</div>}

                  <div className="package-coins">
                    <span className="coins-icon">🪙</span>
                    <span className="coins-amount">{pkg.coins.toLocaleString()}</span>
                    {pkg.bonus && <span className="bonus-tag">{pkg.bonus}</span>}
                  </div>

                  <div className="package-price">
                    ${pkg.price.toFixed(2)}
                  </div>
                </motion.button>
              ))}
            </div>

            <p className="purchase-note">
              Secure payment powered by Stripe
            </p>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
};

export default BuyCoinsModal;
