import { useState, useEffect, useCallback, memo } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { shopApi } from '../api/client';
import BuyCoinsModal from './BuyCoinsModal';
import './ShopScreen.css';

const SHOP_CATEGORIES = [
  { id: 'rings', label: 'Rings' },
  { id: 'decorations', label: 'Decorations' },
  { id: 'items', label: 'Items' },
  { id: 'games', label: 'Games' },
  { id: 'family', label: 'Family' },
];

// Mock shop items (would come from backend)
const MOCK_ITEMS = {
  rings: [
    { id: 1, name: 'Ring of Frost and Flame', image: '💍', fromActivity: true },
    { id: 2, name: 'Royal Shield', image: '🛡️', fromActivity: true },
    { id: 3, name: 'Serpent Ring', image: '🐍', fromActivity: true },
    { id: 4, name: 'Crown Ring', image: '👑', price: 520920 },
    { id: 5, name: 'Feathers Ring', image: '🪶', price: 334420 },
    { id: 6, name: 'Crystal Ring', image: '💎', price: 334420 },
  ],
  decorations: [
    { id: 10, name: 'Star Frame', image: '⭐', price: 50000 },
    { id: 11, name: 'Heart Frame', image: '❤️', price: 45000 },
    { id: 12, name: 'Sparkle Effect', image: '✨', price: 80000 },
  ],
  items: [
    { id: 20, name: 'Lucky Charm', image: '🍀', price: 25000 },
    { id: 21, name: 'Gift Box', image: '🎁', price: 15000 },
    { id: 22, name: 'Mystery Key', image: '🔑', price: 100000 },
  ],
  games: [
    { id: 30, name: 'Power-Up Pack', image: '⚡', price: 5000 },
    { id: 31, name: 'Extra Cards', image: '🎴', price: 2000 },
  ],
  family: [
    { id: 40, name: 'Family Crest', image: '🏰', price: 200000 },
    { id: 41, name: 'Family Badge', image: '📛', price: 150000 },
  ],
};

const ShopItemCard = memo(({ item, onPurchase }) => (
  <motion.div
    className="shop-item-card"
    whileHover={{ scale: 1.03 }}
    whileTap={{ scale: 0.98 }}
    onClick={() => onPurchase(item)}
  >
    <div className="shop-item-image">
      <span className="item-emoji">{item.image}</span>
    </div>
    <div className="shop-item-info">
      <h4 className="shop-item-name">{item.name}</h4>
      {item.fromActivity ? (
        <span className="from-activity-badge">From Activity</span>
      ) : (
        <div className="shop-item-price">
          <span className="coin-icon">🪙</span>
          <span className="price-amount">{item.price?.toLocaleString()}</span>
        </div>
      )}
    </div>
  </motion.div>
));

ShopItemCard.displayName = 'ShopItemCard';

const ShopScreen = ({ player, onClose, onPurchase }) => {
  const [activeCategory, setActiveCategory] = useState('rings');
  const [items, setItems] = useState(MOCK_ITEMS);
  const [loading, setLoading] = useState(false);
  const [purchaseModal, setPurchaseModal] = useState(null);
  const [showBuyCoins, setShowBuyCoins] = useState(false);

  const fetchItems = useCallback(async () => {
    if (!player?.id) return;

    setLoading(true);
    try {
      const response = await shopApi.getByCategory(player.id, activeCategory);
      if (response.data?.length > 0) {
        setItems(prev => ({
          ...prev,
          [activeCategory]: response.data
        }));
      }
    } catch (error) {
      console.error('Failed to fetch shop items:', error);
      // Keep mock data on error
    } finally {
      setLoading(false);
    }
  }, [player?.id, activeCategory]);

  useEffect(() => {
    fetchItems();
  }, [fetchItems]);

  const handlePurchase = (item) => {
    if (item.fromActivity) {
      // Show activity info or redirect
      return;
    }
    setPurchaseModal(item);
  };

  const confirmPurchase = async () => {
    if (!purchaseModal) return;

    try {
      const response = await shopApi.purchase(purchaseModal.id, player.id);
      onPurchase?.(response.data.player);
      setPurchaseModal(null);
    } catch (error) {
      alert(error.response?.data?.error || 'Purchase failed');
    }
  };

  const currentItems = items[activeCategory] || [];

  return (
    <motion.div
      className="shop-screen"
      initial={{ opacity: 0, y: 50 }}
      animate={{ opacity: 1, y: 0 }}
      exit={{ opacity: 0, y: 50 }}
    >
      {/* Header */}
      <div className="shop-header">
        <button className="back-btn" onClick={onClose}>←</button>
        <h1 className="shop-title">Shop</h1>
        <button className="bag-btn">🎒</button>
      </div>

      {/* Balance */}
      <div className="shop-balance">
        <span className="balance-label">Balance:</span>
        <div className="balance-items">
          <span className="balance-item gems">💎 {player?.gems || 0}</span>
          <span className="balance-item coins">🪙 {player?.coins?.toLocaleString() || 0}</span>
          <button className="add-btn" onClick={() => setShowBuyCoins(true)}>+</button>
        </div>
      </div>

      {/* Banner */}
      <div className="shop-banner">
        <div className="banner-avatars">
          <span className="banner-avatar">👧</span>
          <span className="banner-avatar">👦</span>
        </div>
        <div className="banner-text">
          <h2>3D Avatar</h2>
          <p>Dollify Your New Self</p>
        </div>
      </div>

      {/* Categories */}
      <div className="shop-categories">
        {SHOP_CATEGORIES.map((cat) => (
          <button
            key={cat.id}
            className={`category-tab ${activeCategory === cat.id ? 'active' : ''}`}
            onClick={() => setActiveCategory(cat.id)}
          >
            {cat.label}
            {activeCategory === cat.id && (
              <motion.div
                className="category-indicator"
                layoutId="shopCategoryIndicator"
              />
            )}
          </button>
        ))}
      </div>

      {/* Section Title */}
      <div className="shop-section-title">
        <div className="title-line"></div>
        <span className="title-text">
          {activeCategory === 'rings' ? 'Wedding Ring' : activeCategory.charAt(0).toUpperCase() + activeCategory.slice(1)}
        </span>
        <div className="title-line"></div>
      </div>

      {/* Items Grid */}
      <div className="shop-items-grid">
        {loading ? (
          <div className="shop-loading">
            <div className="loading-spinner"></div>
          </div>
        ) : (
          currentItems.map((item) => (
            <ShopItemCard
              key={item.id}
              item={item}
              onPurchase={handlePurchase}
            />
          ))
        )}
      </div>

      {/* View More */}
      {currentItems.length > 0 && (
        <button className="view-more-btn">
          View more <span>›</span>
        </button>
      )}

      {/* Buy Coins Modal */}
      <BuyCoinsModal
        isOpen={showBuyCoins}
        onClose={() => setShowBuyCoins(false)}
        player={player}
      />

      {/* Purchase Modal */}
      <AnimatePresence>
        {purchaseModal && (
          <motion.div
            className="purchase-modal-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            onClick={() => setPurchaseModal(null)}
          >
            <motion.div
              className="purchase-modal"
              initial={{ scale: 0.8, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              exit={{ scale: 0.8, opacity: 0 }}
              onClick={(e) => e.stopPropagation()}
            >
              <div className="purchase-item-preview">
                <span className="purchase-emoji">{purchaseModal.image}</span>
              </div>
              <h3>{purchaseModal.name}</h3>
              <p className="purchase-price">
                🪙 {purchaseModal.price?.toLocaleString()}
              </p>
              {player?.coins >= purchaseModal.price ? (
                <button className="confirm-purchase-btn" onClick={confirmPurchase}>
                  Purchase
                </button>
              ) : (
                <p className="insufficient-funds">Not enough coins!</p>
              )}
              <button className="cancel-btn" onClick={() => setPurchaseModal(null)}>
                Cancel
              </button>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>
    </motion.div>
  );
};

export default ShopScreen;
