import { motion, AnimatePresence } from 'framer-motion';
import { useState, useEffect } from 'react';
import { rewardApi } from '../api/client';
import './ShopModal.css';

const REWARD_ICONS = {
  heart: '💖',
  star: '⭐',
  paw: '🐾',
  flower: '🌸',
  rainbow: '🌈',
  crown: '👑',
  palette: '🎨',
  water: '🌊',
  sun: '☀️',
  moon: '🌙',
  rabbit: '🐰',
  cat: '🐱',
  dog: '🐶',
  sparkles: '✨',
};

const ShopModal = ({ isOpen, onClose, player, onPurchase }) => {
  const [rewards, setRewards] = useState({ available: [], owned: [] });
  const [activeTab, setActiveTab] = useState('daubers');
  const [viewMode, setViewMode] = useState('shop'); // 'shop' or 'inventory'
  const [loading, setLoading] = useState(false);
  const [message, setMessage] = useState(null);

  useEffect(() => {
    if (isOpen && player?.id) {
      fetchRewards();
    }
  }, [isOpen, player?.id]);

  const fetchRewards = async () => {
    try {
      const response = await rewardApi.getAvailable(player.id);
      setRewards(response.data);
    } catch (error) {
      console.error('Failed to fetch rewards:', error);
    }
  };

  const handlePurchase = async (reward) => {
    setLoading(true);
    setMessage(null);

    try {
      const response = await rewardApi.purchase(reward.id, player.id);
      setMessage({ type: 'success', text: response.data.message });
      onPurchase?.(response.data.player);
      fetchRewards();
    } catch (error) {
      setMessage({
        type: 'error',
        text: error.response?.data?.error || 'Purchase failed!'
      });
    } finally {
      setLoading(false);
    }
  };

  const handleEquip = async (rewardType, rewardValue) => {
    setLoading(true);
    setMessage(null);

    try {
      const response = await rewardApi.equip(player.id, rewardType, rewardValue);
      setMessage({ type: 'success', text: `Equipped successfully!` });
      onPurchase?.(response.data);
    } catch (error) {
      setMessage({
        type: 'error',
        text: error.response?.data?.error || 'Failed to equip!'
      });
    } finally {
      setLoading(false);
    }
  };

  const filterRewards = (type) => {
    const typeMap = {
      daubers: 'dauber',
      themes: 'theme',
      cardBacks: 'card_back'
    };
    return rewards.available.filter(r => r.reward_type === typeMap[type]);
  };

  const filterOwned = (type) => {
    const typeMap = {
      daubers: 'dauber',
      themes: 'theme',
      cardBacks: 'card_back'
    };
    return rewards.owned.filter(r => r.reward_type === typeMap[type]);
  };

  const isEquipped = (reward) => {
    if (reward.reward_type === 'theme') return player?.active_theme === reward.reward_value;
    if (reward.reward_type === 'dauber') return player?.active_dauber === reward.reward_value;
    if (reward.reward_type === 'card_back') return player?.active_card_back === reward.reward_value;
    return false;
  };

  const tabs = [
    { id: 'daubers', label: 'Daubers', icon: '💖' },
    { id: 'themes', label: 'Themes', icon: '🎨' },
    { id: 'cardBacks', label: 'Card Backs', icon: '🃏' },
  ];

  return (
    <AnimatePresence>
      {isOpen && (
        <motion.div
          className="modal-overlay"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onClick={onClose}
        >
          <motion.div
            className="shop-modal"
            initial={{ scale: 0.8, y: 50 }}
            animate={{ scale: 1, y: 0 }}
            exit={{ scale: 0.8, y: 50 }}
            onClick={(e) => e.stopPropagation()}
          >
            <div className="shop-header">
              <h2 className="shop-title">
                <span className="shop-icon">{viewMode === 'shop' ? '🛍️' : '🎒'}</span>
                {viewMode === 'shop' ? 'Cute Shop' : 'My Inventory'}
              </h2>
              <button className="close-btn" onClick={onClose}>✕</button>
            </div>

            <div className="view-toggle">
              <button
                className={`toggle-btn ${viewMode === 'shop' ? 'active' : ''}`}
                onClick={() => setViewMode('shop')}
              >
                🛍️ Shop
              </button>
              <button
                className={`toggle-btn ${viewMode === 'inventory' ? 'active' : ''}`}
                onClick={() => setViewMode('inventory')}
              >
                🎒 Inventory
              </button>
            </div>

            <div className="player-currency-bar">
              <div className="currency-item">
                <span>🪙</span>
                <span>{player?.coins?.toLocaleString() || 0}</span>
              </div>
              <div className="currency-item">
                <span>💎</span>
                <span>{player?.gems || 0}</span>
              </div>
            </div>

            <div className="shop-tabs">
              {tabs.map((tab) => (
                <button
                  key={tab.id}
                  className={`shop-tab ${activeTab === tab.id ? 'active' : ''}`}
                  onClick={() => setActiveTab(tab.id)}
                >
                  <span className="tab-icon">{tab.icon}</span>
                  <span className="tab-label">{tab.label}</span>
                </button>
              ))}
            </div>

            {message && (
              <motion.div
                className={`shop-message ${message.type}`}
                initial={{ opacity: 0, y: -10 }}
                animate={{ opacity: 1, y: 0 }}
              >
                {message.text}
              </motion.div>
            )}

            <div className="shop-items">
              {viewMode === 'shop' ? (
                // Shop View - Items to buy
                filterRewards(activeTab).length === 0 ? (
                  <div className="no-items">
                    <span className="no-items-icon">✨</span>
                    <p>You own everything here! Check your inventory to equip items!</p>
                  </div>
                ) : (
                  filterRewards(activeTab).map((reward, index) => (
                    <motion.div
                      key={reward.id}
                      className={`shop-item ${reward.is_premium ? 'premium' : ''}`}
                      initial={{ opacity: 0, y: 20 }}
                      animate={{ opacity: 1, y: 0 }}
                      transition={{ delay: index * 0.1 }}
                    >
                      <div className="item-icon">
                        {REWARD_ICONS[reward.icon] || '🎁'}
                      </div>
                      <div className="item-info">
                        <h3 className="item-name">{reward.name}</h3>
                        <p className="item-description">{reward.description}</p>
                        {reward.streak_requirement > 0 && (
                          <div className="streak-requirement">
                            <span className="requirement-icon">🔥</span>
                            <span>Requires {reward.streak_requirement} streak</span>
                          </div>
                        )}
                      </div>
                      <div className="item-price">
                        {reward.coin_cost > 0 && (
                          <span className="price coins">
                            🪙 {reward.coin_cost}
                          </span>
                        )}
                        {reward.gem_cost > 0 && (
                          <span className="price gems">
                            💎 {reward.gem_cost}
                          </span>
                        )}
                      </div>
                      <button
                        className="buy-button"
                        onClick={() => handlePurchase(reward)}
                        disabled={loading}
                      >
                        {loading ? '...' : 'Buy'}
                      </button>
                    </motion.div>
                  ))
                )
              ) : (
                // Inventory View - Owned items to equip
                filterOwned(activeTab).length === 0 ? (
                  <div className="no-items">
                    <span className="no-items-icon">🛍️</span>
                    <p>No items yet! Visit the shop to buy some!</p>
                  </div>
                ) : (
                  filterOwned(activeTab).map((reward, index) => (
                    <motion.div
                      key={reward.id}
                      className={`shop-item ${isEquipped(reward) ? 'equipped' : ''}`}
                      initial={{ opacity: 0, y: 20 }}
                      animate={{ opacity: 1, y: 0 }}
                      transition={{ delay: index * 0.1 }}
                    >
                      <div className="item-icon">
                        {REWARD_ICONS[reward.icon] || '🎁'}
                      </div>
                      <div className="item-info">
                        <h3 className="item-name">{reward.name}</h3>
                        <p className="item-description">{reward.description}</p>
                        {isEquipped(reward) && (
                          <div className="equipped-badge">
                            <span>✓ Equipped</span>
                          </div>
                        )}
                      </div>
                      <button
                        className={`equip-button ${isEquipped(reward) ? 'equipped' : ''}`}
                        onClick={() => handleEquip(reward.reward_type, reward.reward_value)}
                        disabled={loading || isEquipped(reward)}
                      >
                        {isEquipped(reward) ? '✓ Active' : 'Equip'}
                      </button>
                    </motion.div>
                  ))
                )
              )}
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
};

export default ShopModal;
