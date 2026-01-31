import { useState, useEffect, useCallback, memo, useMemo } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { avatarApi, avatarItemApi, playerApi } from '../api/client';
import RealisticAvatar, {
  SKIN_TONES,
  HAIR_COLORS,
  EYE_COLORS,
  HAIR_STYLES,
  FACE_SHAPES,
  EYE_STYLES,
  EYEBROW_STYLES,
  NOSE_STYLES,
  MOUTH_STYLES
} from './RealisticAvatar';
import { themes } from './ThemeProvider';
import './AvatarEditor.css';

// Category definitions for the editor
const FACE_CATEGORIES = [
  { id: 'skinTone', icon: '🎨', label: 'Skin' },
  { id: 'faceShape', icon: '😊', label: 'Face' },
  { id: 'hairStyle', icon: '💇', label: 'Hair' },
  { id: 'hairColor', icon: '🖌️', label: 'Color' },
  { id: 'eyeStyle', icon: '👁️', label: 'Eyes' },
  { id: 'eyeColor', icon: '🔵', label: 'Iris' },
  { id: 'eyebrowStyle', icon: '🤨', label: 'Brows' },
  { id: 'noseStyle', icon: '👃', label: 'Nose' },
  { id: 'mouthStyle', icon: '👄', label: 'Mouth' },
];

const CLOTHING_CATEGORIES = [
  { id: 'top', icon: '👕', label: 'Tops' },
  { id: 'bottom', icon: '👖', label: 'Bottoms' },
  { id: 'shoes', icon: '👟', label: 'Shoes' },
  { id: 'hat', icon: '🎩', label: 'Hats' },
  { id: 'glasses', icon: '👓', label: 'Glasses' },
  { id: 'earrings', icon: '💎', label: 'Earrings' },
];

// Convert style arrays to item format
const getItemsForCategory = (category, gender = 'neutral', ownedItems = []) => {
  switch (category) {
    case 'skinTone':
      return SKIN_TONES.map(tone => ({
        id: tone.id,
        name: tone.id.charAt(0).toUpperCase() + tone.id.slice(1).replace('_', ' '),
        color: tone.color,
        owned: true,
        type: 'color'
      }));

    case 'hairColor':
      return HAIR_COLORS.map(color => ({
        id: color.id,
        name: color.id.charAt(0).toUpperCase() + color.id.slice(1).replace('_', ' '),
        color: color.color,
        owned: ['black', 'dark_brown', 'brown', 'blonde'].includes(color.id) || ownedItems.includes(color.id),
        price: ['black', 'dark_brown', 'brown', 'blonde'].includes(color.id) ? 0 : 300,
        type: 'color'
      }));

    case 'eyeColor':
      return EYE_COLORS.map(color => ({
        id: color.id,
        name: color.id.charAt(0).toUpperCase() + color.id.slice(1).replace('_', ' '),
        color: color.color,
        owned: true,
        type: 'color'
      }));

    case 'hairStyle':
      const styles = HAIR_STYLES[gender] || HAIR_STYLES.neutral;
      return styles.map(style => ({
        id: style.id,
        name: style.name,
        owned: !style.premium || ownedItems.includes(style.id),
        price: style.price || 0,
        premium: style.premium
      }));

    case 'faceShape':
      return FACE_SHAPES.map(shape => ({
        id: shape.id,
        name: shape.name,
        owned: true,
      }));

    case 'eyeStyle':
      return EYE_STYLES.map(style => ({
        id: style.id,
        name: style.name,
        owned: true,
      }));

    case 'eyebrowStyle':
      return EYEBROW_STYLES.map(style => ({
        id: style.id,
        name: style.name,
        owned: true,
      }));

    case 'noseStyle':
      return NOSE_STYLES.map(style => ({
        id: style.id,
        name: style.name,
        owned: true,
      }));

    case 'mouthStyle':
      return MOUTH_STYLES.map(style => ({
        id: style.id,
        name: style.name,
        owned: true,
      }));

    case 'glasses':
      return [
        { id: null, name: 'None', owned: true },
        { id: 'round', name: 'Round', owned: ownedItems.includes('glasses_round'), price: 500 },
        { id: 'square', name: 'Square', owned: ownedItems.includes('glasses_square'), price: 500 },
        { id: 'aviator', name: 'Aviator', owned: ownedItems.includes('glasses_aviator'), price: 800 },
        { id: 'cat_eye', name: 'Cat Eye', owned: ownedItems.includes('glasses_cat_eye'), price: 700 },
      ];

    case 'hat':
      return [
        { id: null, name: 'None', owned: true },
        { id: 'cap', name: 'Baseball Cap', owned: ownedItems.includes('hat_cap'), price: 400 },
        { id: 'beanie', name: 'Beanie', owned: ownedItems.includes('hat_beanie'), price: 350 },
        { id: 'fedora', name: 'Fedora', owned: ownedItems.includes('hat_fedora'), price: 600 },
        { id: 'bow', name: 'Hair Bow', owned: ownedItems.includes('hat_bow'), price: 300 },
      ];

    case 'earrings':
      return [
        { id: null, name: 'None', owned: true },
        { id: 'studs', name: 'Studs', owned: ownedItems.includes('earrings_studs'), price: 200 },
        { id: 'hoops', name: 'Hoops', owned: ownedItems.includes('earrings_hoops'), price: 350 },
        { id: 'drops', name: 'Drop Earrings', owned: ownedItems.includes('earrings_drops'), price: 500 },
      ];

    case 'top':
      return [
        { id: 'tshirt_white', name: 'White T-Shirt', owned: true },
        { id: 'tshirt_black', name: 'Black T-Shirt', owned: true },
        { id: 'hoodie_gray', name: 'Gray Hoodie', owned: ownedItems.includes('top_hoodie_gray'), price: 800 },
        { id: 'blazer', name: 'Blazer', owned: ownedItems.includes('top_blazer'), price: 1200 },
        { id: 'dress', name: 'Summer Dress', owned: ownedItems.includes('top_dress'), price: 1500 },
        { id: 'sweater', name: 'Cozy Sweater', owned: ownedItems.includes('top_sweater'), price: 900 },
      ];

    case 'bottom':
      return [
        { id: 'jeans_blue', name: 'Blue Jeans', owned: true },
        { id: 'shorts', name: 'Shorts', owned: true },
        { id: 'skirt', name: 'Mini Skirt', owned: ownedItems.includes('bottom_skirt'), price: 600 },
        { id: 'joggers', name: 'Joggers', owned: ownedItems.includes('bottom_joggers'), price: 700 },
      ];

    case 'shoes':
      return [
        { id: 'sneakers', name: 'Sneakers', owned: true },
        { id: 'boots', name: 'Boots', owned: ownedItems.includes('shoes_boots'), price: 800 },
        { id: 'heels', name: 'Heels', owned: ownedItems.includes('shoes_heels'), price: 900 },
        { id: 'sandals', name: 'Sandals', owned: ownedItems.includes('shoes_sandals'), price: 400 },
      ];

    default:
      return [];
  }
};

// Color swatch component
const ColorSwatch = memo(({ color, isSelected, onClick, name }) => (
  <motion.button
    className={`color-swatch ${isSelected ? 'selected' : ''}`}
    onClick={onClick}
    whileHover={{ scale: 1.1 }}
    whileTap={{ scale: 0.95 }}
    style={{ backgroundColor: color }}
    title={name}
  >
    {isSelected && <span className="check">✓</span>}
  </motion.button>
));

ColorSwatch.displayName = 'ColorSwatch';

// Item card component
const ItemCard = memo(({ item, isSelected, onSelect, currentAvatar, category }) => {
  const isColorItem = item.type === 'color';

  if (isColorItem) {
    return (
      <ColorSwatch
        color={item.color}
        isSelected={isSelected}
        onClick={() => onSelect(item)}
        name={item.name}
      />
    );
  }

  return (
    <motion.div
      className={`item-card ${isSelected ? 'selected' : ''} ${item.owned ? 'owned' : 'locked'}`}
      onClick={() => onSelect(item)}
      whileHover={{ scale: 1.03 }}
      whileTap={{ scale: 0.97 }}
    >
      {item.premium && !item.owned && (
        <span className="premium-badge">⭐</span>
      )}

      <div className="item-preview">
        {/* Mini avatar preview showing the item */}
        <div className="mini-avatar-preview">
          <RealisticAvatar
            avatar={{ ...currentAvatar, [category]: item.id }}
            size={55}
            showBody={false}
          />
        </div>
      </div>

      <div className="item-name">{item.name}</div>

      {item.owned ? (
        <div className="item-status owned">✓</div>
      ) : item.price ? (
        <div className="item-price">
          <span className="coin-icon">🪙</span>
          {item.price}
        </div>
      ) : null}
    </motion.div>
  );
});

ItemCard.displayName = 'ItemCard';

// Purchase modal
const PurchaseModal = ({ item, player, onPurchase, onClose, loading }) => {
  const canAfford = player?.coins >= (item?.price || 0);

  return (
    <motion.div
      className="purchase-modal-overlay"
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
      onClick={onClose}
    >
      <motion.div
        className="purchase-modal"
        initial={{ scale: 0.9, y: 20 }}
        animate={{ scale: 1, y: 0 }}
        exit={{ scale: 0.9, y: 20 }}
        onClick={e => e.stopPropagation()}
      >
        <h3>Purchase Item</h3>

        <div className="purchase-item-info">
          <div className="purchase-item-name">{item?.name}</div>
          <div className="purchase-item-price">
            <span className="coin-icon">🪙</span>
            {item?.price || 0}
          </div>
        </div>

        <div className="purchase-balance">
          Your balance: <span className="coin-icon">🪙</span> {player?.coins?.toLocaleString() || 0}
        </div>

        {!canAfford && (
          <div className="insufficient-funds">
            Not enough coins! You need {(item?.price || 0) - (player?.coins || 0)} more.
          </div>
        )}

        <div className="purchase-actions">
          <button
            className="purchase-btn"
            onClick={() => onPurchase(item)}
            disabled={!canAfford || loading}
          >
            {loading ? 'Purchasing...' : canAfford ? 'Purchase' : 'Not Enough Coins'}
          </button>
          <button className="cancel-btn" onClick={onClose} disabled={loading}>
            Cancel
          </button>
        </div>
      </motion.div>
    </motion.div>
  );
};

// Gender selector
const GenderSelector = ({ gender, onChange }) => (
  <div className="gender-selector">
    {['female', 'male', 'neutral'].map(g => (
      <button
        key={g}
        className={`gender-btn ${gender === g ? 'active' : ''}`}
        onClick={() => onChange(g)}
      >
        {g === 'female' ? '👩' : g === 'male' ? '👨' : '🧑'}
        <span>{g.charAt(0).toUpperCase() + g.slice(1)}</span>
      </button>
    ))}
  </div>
);

// Theme card component
const ThemeCard = memo(({ theme, isSelected, isOwned, onSelect, price = 500 }) => (
  <motion.div
    className={`theme-card ${isSelected ? 'selected' : ''} ${isOwned ? 'owned' : 'locked'}`}
    onClick={() => onSelect(theme)}
    whileHover={{ scale: 1.03, y: -2 }}
    whileTap={{ scale: 0.97 }}
  >
    <div className={`theme-preview theme-preview-${theme.name}`}>
      <span className="theme-icon">{theme.icon}</span>
    </div>
    <div className="theme-info">
      <div className="theme-name">{theme.name.charAt(0).toUpperCase() + theme.name.slice(1)}</div>
      <div className="theme-desc">{theme.description}</div>
    </div>
    {isOwned ? (
      isSelected ? (
        <div className="theme-status active">Active</div>
      ) : (
        <div className="theme-status owned">Owned</div>
      )
    ) : (
      <div className="theme-price">
        <span className="coin-icon">🪙</span>
        {theme.premium ? 1000 : price}
      </div>
    )}
  </motion.div>
));

ThemeCard.displayName = 'ThemeCard';

// Main Avatar Editor Component
const AvatarEditor = ({ player, onClose, onSave, onThemeChange, onLogout }) => {
  const [activeTab, setActiveTab] = useState('face');
  const [activeCategory, setActiveCategory] = useState('skinTone');
  const [avatar, setAvatar] = useState({
    gender: 'neutral',
    skinTone: 'light',
    hairStyle: 'short_neat',
    hairColor: 'brown',
    faceShape: 'oval',
    eyeStyle: 'normal',
    eyeColor: 'brown',
    eyebrowStyle: 'natural',
    noseStyle: 'normal',
    mouthStyle: 'smile',
    top: 'tshirt_white',
    bottom: 'jeans_blue',
    shoes: 'sneakers',
    glasses: null,
    hat: null,
    earrings: null,
    ...(player?.avatar || {})
  });
  const [ownedItems, setOwnedItems] = useState([]);
  const [ownedThemes, setOwnedThemes] = useState(['default', 'dark', 'pastel', 'ocean', 'sakura', 'night', 'candy']);
  const [currentTheme, setCurrentTheme] = useState(player?.active_theme || 'default');
  const [purchaseItem, setPurchaseItem] = useState(null);
  const [purchaseTheme, setPurchaseTheme] = useState(null);
  const [loading, setLoading] = useState(false);
  const [saveLoading, setSaveLoading] = useState(false);
  const [hasChanges, setHasChanges] = useState(false);
  const [localPlayer, setLocalPlayer] = useState(player);

  const categories = activeTab === 'face' ? FACE_CATEGORIES : CLOTHING_CATEGORIES;

  // Load saved data on mount
  useEffect(() => {
    if (player?.id) {
      // Load owned items
      const storedOwned = localStorage.getItem(`avatar_owned_${player?.id}`);
      if (storedOwned) {
        setOwnedItems(JSON.parse(storedOwned));
      }

      // Load saved avatar
      const storedAvatar = localStorage.getItem(`avatar_data_${player?.id}`);
      if (storedAvatar) {
        setAvatar(prev => ({ ...prev, ...JSON.parse(storedAvatar) }));
      }

      // Load owned themes
      const storedThemes = localStorage.getItem(`owned_themes_${player?.id}`);
      if (storedThemes) {
        setOwnedThemes(JSON.parse(storedThemes));
      }

      // Load active theme
      const storedActiveTheme = localStorage.getItem(`active_theme_${player?.id}`);
      if (storedActiveTheme) {
        setCurrentTheme(storedActiveTheme);
      }
    }
  }, [player?.id]);

  // Update local player when prop changes
  useEffect(() => {
    setLocalPlayer(player);
  }, [player]);

  // Reset category when switching tabs
  useEffect(() => {
    setActiveCategory(categories[0].id);
  }, [activeTab, categories]);

  // Get items for current category
  const currentItems = useMemo(() => {
    const items = getItemsForCategory(activeCategory, avatar.gender, ownedItems);
    return items.map(item => ({ ...item, category: activeCategory }));
  }, [activeCategory, avatar.gender, ownedItems]);

  // Handle item selection
  const handleSelectItem = useCallback((item) => {
    if (!item.owned && item.price > 0) {
      setPurchaseItem(item);
      return;
    }

    setAvatar(prev => ({
      ...prev,
      [activeCategory]: item.id
    }));
    setHasChanges(true);
  }, [activeCategory]);

  // Handle purchase
  const handlePurchase = useCallback(async (item) => {
    if (!localPlayer || localPlayer.coins < item.price) return;

    setLoading(true);
    try {
      // Deduct coins
      const newCoins = localPlayer.coins - item.price;
      setLocalPlayer(prev => ({ ...prev, coins: newCoins }));

      // Add to owned items
      const itemKey = `${item.category}_${item.id}`;
      const newOwned = [...ownedItems, item.id, itemKey];
      setOwnedItems(newOwned);

      // Save to local storage
      localStorage.setItem(`avatar_owned_${player?.id}`, JSON.stringify(newOwned));

      // Apply item
      setAvatar(prev => ({
        ...prev,
        [activeCategory]: item.id
      }));

      setHasChanges(true);
      setPurchaseItem(null);

    } catch (error) {
      console.error('Failed to purchase:', error);
      setLocalPlayer(player);
    } finally {
      setLoading(false);
    }
  }, [localPlayer, ownedItems, player, activeCategory]);

  // Handle save
  const handleSave = useCallback(async () => {
    setSaveLoading(true);
    try {
      // Save to local storage
      localStorage.setItem(`avatar_data_${player?.id}`, JSON.stringify(avatar));

      // Try to save to backend
      if (player?.avatar?.id) {
        try {
          await avatarApi.update(player.avatar.id, avatar);
        } catch (e) {
          console.log('Backend save failed, using local storage');
        }
      }

      // Notify parent with updated player and avatar
      onSave?.({ ...avatar, player: localPlayer });
      setHasChanges(false);

    } catch (error) {
      console.error('Failed to save avatar:', error);
    } finally {
      setSaveLoading(false);
    }
  }, [avatar, player, localPlayer, onSave]);

  // Handle gender change
  const handleGenderChange = useCallback((newGender) => {
    const defaultHair = HAIR_STYLES[newGender]?.[0]?.id || 'short_neat';
    setAvatar(prev => ({
      ...prev,
      gender: newGender,
      hairStyle: defaultHair
    }));
    setHasChanges(true);
  }, []);

  // Handle theme selection
  const handleSelectTheme = useCallback((theme) => {
    const isOwned = ownedThemes.includes(theme.name);

    if (!isOwned) {
      setPurchaseTheme(theme);
      return;
    }

    setCurrentTheme(theme.name);
    localStorage.setItem(`active_theme_${player?.id}`, theme.name);

    // Apply theme immediately
    if (onThemeChange) {
      onThemeChange(theme.name);
    }

    setHasChanges(true);
  }, [ownedThemes, player?.id, onThemeChange]);

  // Handle theme purchase
  const handleThemePurchase = useCallback(async (theme) => {
    const price = theme.premium ? 1000 : 500;
    if (!localPlayer || localPlayer.coins < price) return;

    setLoading(true);
    try {
      // Deduct coins
      const newCoins = localPlayer.coins - price;
      setLocalPlayer(prev => ({ ...prev, coins: newCoins }));

      // Add to owned themes
      const newOwned = [...ownedThemes, theme.name];
      setOwnedThemes(newOwned);
      localStorage.setItem(`owned_themes_${player?.id}`, JSON.stringify(newOwned));

      // Apply theme
      setCurrentTheme(theme.name);
      localStorage.setItem(`active_theme_${player?.id}`, theme.name);

      if (onThemeChange) {
        onThemeChange(theme.name);
      }

      setPurchaseTheme(null);
      setHasChanges(true);
    } catch (error) {
      console.error('Failed to purchase theme:', error);
      setLocalPlayer(player);
    } finally {
      setLoading(false);
    }
  }, [localPlayer, ownedThemes, player, onThemeChange]);

  const isColorCategory = ['skinTone', 'hairColor', 'eyeColor'].includes(activeCategory);

  // Get sorted themes list
  const themesList = useMemo(() => {
    return Object.values(themes).sort((a, b) => {
      // Put owned themes first, then by name
      const aOwned = ownedThemes.includes(a.name);
      const bOwned = ownedThemes.includes(b.name);
      if (aOwned && !bOwned) return -1;
      if (!aOwned && bOwned) return 1;
      return a.name.localeCompare(b.name);
    });
  }, [ownedThemes]);

  return (
    <motion.div
      className="avatar-editor"
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
    >
      {/* Header */}
      <div className="editor-header">
        <button className="back-btn" onClick={onClose}>
          ←
        </button>
        <h2 className="editor-title">Customize Avatar</h2>
        <div className="header-actions">
          <div className="currency-display">
            <span className="currency gems">💎 {localPlayer?.gems || 0}</span>
            <span className="currency coins">🪙 {localPlayer?.coins?.toLocaleString() || 0}</span>
          </div>
          {onLogout && (
            <button className="logout-btn" onClick={onLogout} title="Sign Out">
              🚪
            </button>
          )}
        </div>
      </div>

      {/* Avatar Display */}
      <div className="avatar-display">
        <div className="avatar-preview-container">
          <RealisticAvatar
            avatar={avatar}
            size={160}
            showBody={true}
          />
        </div>

        {/* Gender selector */}
        <GenderSelector
          gender={avatar.gender}
          onChange={handleGenderChange}
        />

        {/* Save button */}
        <motion.button
          className={`save-btn ${hasChanges ? 'has-changes' : ''}`}
          onClick={handleSave}
          disabled={saveLoading || !hasChanges}
          whileHover={{ scale: 1.05 }}
          whileTap={{ scale: 0.95 }}
        >
          {saveLoading ? 'Saving...' : hasChanges ? 'Save Changes' : 'Saved ✓'}
        </motion.button>
      </div>

      {/* Tab Switcher */}
      <div className="editor-tabs">
        <button
          className={`editor-tab ${activeTab === 'face' ? 'active' : ''}`}
          onClick={() => setActiveTab('face')}
        >
          Face & Hair
        </button>
        <button
          className={`editor-tab ${activeTab === 'clothing' ? 'active' : ''}`}
          onClick={() => setActiveTab('clothing')}
        >
          Clothing
        </button>
        <button
          className={`editor-tab ${activeTab === 'themes' ? 'active' : ''}`}
          onClick={() => setActiveTab('themes')}
        >
          Themes
        </button>
      </div>

      {/* Items Container */}
      {activeTab === 'themes' ? (
        <div className="items-container themes-container">
          <div className="themes-header">
            <h3>Choose Your Theme</h3>
            <p>Personalize your experience with beautiful themes</p>
          </div>
          <div className="themes-grid">
            {themesList.map((theme) => (
              <ThemeCard
                key={theme.name}
                theme={theme}
                isSelected={currentTheme === theme.name}
                isOwned={ownedThemes.includes(theme.name)}
                onSelect={handleSelectTheme}
              />
            ))}
          </div>
        </div>
      ) : (
        <>
          {/* Category Bar */}
          <div className="category-bar">
            {categories.map((cat) => (
              <button
                key={cat.id}
                className={`category-btn ${activeCategory === cat.id ? 'active' : ''}`}
                onClick={() => setActiveCategory(cat.id)}
              >
                <span className="category-icon">{cat.icon}</span>
                <span className="category-label">{cat.label}</span>
              </button>
            ))}
          </div>

          <div className={`items-container ${isColorCategory ? 'color-mode' : ''}`}>
            {isColorCategory ? (
              <div className="color-swatches">
                {currentItems.map((item) => (
                  <ColorSwatch
                    key={item.id}
                    color={item.color}
                    isSelected={avatar[activeCategory] === item.id}
                    onClick={() => handleSelectItem(item)}
                    name={item.name}
                  />
                ))}
              </div>
            ) : (
              <div className="items-grid">
                {currentItems.map((item) => (
                  <ItemCard
                    key={item.id || 'none'}
                    item={item}
                    isSelected={avatar[activeCategory] === item.id}
                    onSelect={handleSelectItem}
                    currentAvatar={avatar}
                    category={activeCategory}
                  />
                ))}
              </div>
            )}
          </div>
        </>
      )}

      {/* Purchase Modal */}
      <AnimatePresence>
        {purchaseItem && (
          <PurchaseModal
            item={purchaseItem}
            player={localPlayer}
            onPurchase={handlePurchase}
            onClose={() => setPurchaseItem(null)}
            loading={loading}
          />
        )}
      </AnimatePresence>

      {/* Theme Purchase Modal */}
      <AnimatePresence>
        {purchaseTheme && (
          <motion.div
            className="purchase-modal-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            onClick={() => setPurchaseTheme(null)}
          >
            <motion.div
              className="purchase-modal theme-purchase-modal"
              initial={{ scale: 0.9, y: 20 }}
              animate={{ scale: 1, y: 0 }}
              exit={{ scale: 0.9, y: 20 }}
              onClick={e => e.stopPropagation()}
            >
              <h3>Unlock Theme</h3>

              <div className="theme-purchase-preview">
                <div className={`theme-preview-large theme-preview-${purchaseTheme.name}`}>
                  <span className="theme-icon-large">{purchaseTheme.icon}</span>
                </div>
                <div className="theme-purchase-name">
                  {purchaseTheme.name.charAt(0).toUpperCase() + purchaseTheme.name.slice(1)}
                </div>
                <div className="theme-purchase-desc">{purchaseTheme.description}</div>
              </div>

              <div className="purchase-item-price">
                <span className="coin-icon">🪙</span>
                {purchaseTheme.premium ? 1000 : 500}
              </div>

              <div className="purchase-balance">
                Your balance: <span className="coin-icon">🪙</span> {localPlayer?.coins?.toLocaleString() || 0}
              </div>

              {localPlayer?.coins < (purchaseTheme.premium ? 1000 : 500) && (
                <div className="insufficient-funds">
                  Not enough coins! You need {(purchaseTheme.premium ? 1000 : 500) - (localPlayer?.coins || 0)} more.
                </div>
              )}

              <div className="purchase-actions">
                <button
                  className="purchase-btn"
                  onClick={() => handleThemePurchase(purchaseTheme)}
                  disabled={localPlayer?.coins < (purchaseTheme.premium ? 1000 : 500) || loading}
                >
                  {loading ? 'Unlocking...' : 'Unlock Theme'}
                </button>
                <button className="cancel-btn" onClick={() => setPurchaseTheme(null)} disabled={loading}>
                  Cancel
                </button>
              </div>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>
    </motion.div>
  );
};

export default AvatarEditor;
