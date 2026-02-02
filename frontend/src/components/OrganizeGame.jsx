import { useState, useEffect, useCallback, useRef } from 'react';
import './OrganizeGame.css';

// Room scenes with different items to organize
const SCENES = [
  {
    id: 'bedroom',
    name: 'Messy Bedroom',
    background: 'linear-gradient(180deg, #E8D4C4 0%, #D4C4B0 100%)',
    items: [
      { id: 'book1', emoji: '📚', type: 'shelf', name: 'Books' },
      { id: 'book2', emoji: '📖', type: 'shelf', name: 'Novel' },
      { id: 'book3', emoji: '📕', type: 'shelf', name: 'Diary' },
      { id: 'toy1', emoji: '🧸', type: 'toybox', name: 'Teddy' },
      { id: 'toy2', emoji: '🎮', type: 'toybox', name: 'Controller' },
      { id: 'toy3', emoji: '🪀', type: 'toybox', name: 'Yo-yo' },
      { id: 'clothes1', emoji: '👕', type: 'drawer', name: 'Shirt' },
      { id: 'clothes2', emoji: '👖', type: 'drawer', name: 'Pants' },
      { id: 'clothes3', emoji: '🧦', type: 'drawer', name: 'Socks' },
    ],
    zones: [
      { id: 'shelf', emoji: '📚', name: 'Bookshelf', color: '#8B7355', slots: 3 },
      { id: 'toybox', emoji: '🎁', name: 'Toy Box', color: '#E57373', slots: 3 },
      { id: 'drawer', emoji: '🗄️', name: 'Drawer', color: '#81C784', slots: 3 },
    ],
  },
  {
    id: 'kitchen',
    name: 'Cluttered Kitchen',
    background: 'linear-gradient(180deg, #FFF8E1 0%, #FFE0B2 100%)',
    items: [
      { id: 'fruit1', emoji: '🍎', type: 'bowl', name: 'Apple' },
      { id: 'fruit2', emoji: '🍊', type: 'bowl', name: 'Orange' },
      { id: 'fruit3', emoji: '🍋', type: 'bowl', name: 'Lemon' },
      { id: 'utensil1', emoji: '🍴', type: 'holder', name: 'Fork' },
      { id: 'utensil2', emoji: '🥄', type: 'holder', name: 'Spoon' },
      { id: 'utensil3', emoji: '🔪', type: 'holder', name: 'Knife' },
      { id: 'dish1', emoji: '🍽️', type: 'rack', name: 'Plate' },
      { id: 'dish2', emoji: '🥣', type: 'rack', name: 'Bowl' },
      { id: 'dish3', emoji: '☕', type: 'rack', name: 'Mug' },
    ],
    zones: [
      { id: 'bowl', emoji: '🍇', name: 'Fruit Bowl', color: '#A5D6A7', slots: 3 },
      { id: 'holder', emoji: '🥢', name: 'Utensil Holder', color: '#90CAF9', slots: 3 },
      { id: 'rack', emoji: '🍽️', name: 'Dish Rack', color: '#CE93D8', slots: 3 },
    ],
  },
  {
    id: 'office',
    name: 'Messy Desk',
    background: 'linear-gradient(180deg, #E3F2FD 0%, #BBDEFB 100%)',
    items: [
      { id: 'pen1', emoji: '✏️', type: 'cup', name: 'Pencil' },
      { id: 'pen2', emoji: '🖊️', type: 'cup', name: 'Pen' },
      { id: 'pen3', emoji: '📏', type: 'cup', name: 'Ruler' },
      { id: 'paper1', emoji: '📄', type: 'tray', name: 'Paper' },
      { id: 'paper2', emoji: '📑', type: 'tray', name: 'Documents' },
      { id: 'paper3', emoji: '📋', type: 'tray', name: 'Clipboard' },
      { id: 'tech1', emoji: '💻', type: 'stand', name: 'Laptop' },
      { id: 'tech2', emoji: '📱', type: 'stand', name: 'Phone' },
      { id: 'tech3', emoji: '🎧', type: 'stand', name: 'Headphones' },
    ],
    zones: [
      { id: 'cup', emoji: '🖊️', name: 'Pencil Cup', color: '#FFCC80', slots: 3 },
      { id: 'tray', emoji: '📥', name: 'Paper Tray', color: '#80DEEA', slots: 3 },
      { id: 'stand', emoji: '💼', name: 'Tech Stand', color: '#B39DDB', slots: 3 },
    ],
  },
];

// Fisher-Yates shuffle
const shuffleArray = (array) => {
  const arr = [...array];
  for (let i = arr.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [arr[i], arr[j]] = [arr[j], arr[i]];
  }
  return arr;
};

// Generate scattered positions
const generatePositions = (count, areaWidth, areaHeight, itemSize) => {
  const positions = [];
  const padding = 15;

  for (let i = 0; i < count; i++) {
    // Create a grid-like scatter
    const cols = 3;
    const row = Math.floor(i / cols);
    const col = i % cols;
    const cellWidth = (areaWidth - padding * 2) / cols;
    const cellHeight = (areaHeight - padding * 2) / Math.ceil(count / cols);

    // Add some randomness within cell
    const x = padding + col * cellWidth + (Math.random() * 0.5 + 0.25) * cellWidth - itemSize / 2;
    const y = padding + row * cellHeight + (Math.random() * 0.5 + 0.25) * cellHeight - itemSize / 2;

    // Random rotation for messiness
    const rotation = (Math.random() - 0.5) * 30;

    positions.push({ x, y, rotation });
  }
  return positions;
};

const OrganizeGame = ({ onGameEnd, onClose }) => {
  const [currentSceneIndex, setCurrentSceneIndex] = useState(0);
  const [items, setItems] = useState([]);
  const [selectedItem, setSelectedItem] = useState(null);
  const [placedItems, setPlacedItems] = useState({}); // { zoneId: [itemIds] }
  const [flyingItem, setFlyingItem] = useState(null);
  const [sparkles, setSparkles] = useState([]);
  const [completed, setCompleted] = useState(false);
  const [totalPlaced, setTotalPlaced] = useState(0);

  const gameAreaRef = useRef(null);
  const zoneRefs = useRef({});

  const currentScene = SCENES[currentSceneIndex];
  const totalItems = SCENES.reduce((acc, scene) => acc + scene.items.length, 0);

  // Initialize scene
  useEffect(() => {
    const scene = SCENES[currentSceneIndex];
    const shuffled = shuffleArray(scene.items);
    const positions = generatePositions(shuffled.length, 320, 200, 55);

    const itemsWithPos = shuffled.map((item, i) => ({
      ...item,
      x: positions[i].x,
      y: positions[i].y,
      rotation: positions[i].rotation,
      visible: true,
    }));

    setItems(itemsWithPos);
    setPlacedItems({});
    setSelectedItem(null);
  }, [currentSceneIndex]);

  // Check scene completion
  useEffect(() => {
    const scene = SCENES[currentSceneIndex];
    const totalInScene = scene.items.length;
    const placedInScene = Object.values(placedItems).flat().length;

    if (placedInScene === totalInScene && totalInScene > 0) {
      // Scene complete - move to next or finish
      setTimeout(() => {
        if (currentSceneIndex < SCENES.length - 1) {
          setCurrentSceneIndex(i => i + 1);
        } else {
          setCompleted(true);
        }
      }, 800);
    }
  }, [placedItems, currentSceneIndex]);

  // Create sparkle effect
  const createSparkles = (x, y) => {
    const newSparkles = Array.from({ length: 8 }, (_, i) => ({
      id: Date.now() + i,
      x: x + (Math.random() - 0.5) * 60,
      y: y + (Math.random() - 0.5) * 60,
      delay: i * 0.05,
    }));
    setSparkles(prev => [...prev, ...newSparkles]);
    setTimeout(() => {
      setSparkles(prev => prev.filter(s => !newSparkles.find(n => n.id === s.id)));
    }, 1000);
  };

  // Handle item tap
  const handleItemTap = useCallback((item) => {
    if (flyingItem) return;
    setSelectedItem(item.id === selectedItem ? null : item.id);
  }, [selectedItem, flyingItem]);

  // Handle zone tap
  const handleZoneTap = useCallback((zone) => {
    if (!selectedItem || flyingItem) return;

    const item = items.find(i => i.id === selectedItem);
    if (!item) return;

    // Check if item type matches zone
    if (item.type !== zone.id) {
      // Wrong zone - gentle bounce back
      setSelectedItem(null);
      return;
    }

    // Check if zone is full
    const zoneItems = placedItems[zone.id] || [];
    if (zoneItems.length >= zone.slots) {
      setSelectedItem(null);
      return;
    }

    // Get zone position for animation
    const zoneEl = zoneRefs.current[zone.id];
    if (!zoneEl) return;

    const zoneRect = zoneEl.getBoundingClientRect();
    const gameRect = gameAreaRef.current?.getBoundingClientRect();
    if (!gameRect) return;

    // Calculate target position in the zone
    const slotIndex = zoneItems.length;
    const slotWidth = 45;
    const targetX = zoneRect.left - gameRect.left + 15 + slotIndex * slotWidth;
    const targetY = zoneRect.top - gameRect.top + 45;

    // Start flying animation
    setFlyingItem({
      ...item,
      targetX,
      targetY,
      targetZone: zone.id,
    });

    // Hide original item
    setItems(prev => prev.map(i =>
      i.id === item.id ? { ...i, visible: false } : i
    ));

    setSelectedItem(null);

    // Complete placement after animation
    setTimeout(() => {
      setPlacedItems(prev => ({
        ...prev,
        [zone.id]: [...(prev[zone.id] || []), item],
      }));
      setFlyingItem(null);
      setTotalPlaced(t => t + 1);

      // Sparkle effect at destination
      createSparkles(targetX + 25, targetY + 25);
    }, 500);
  }, [selectedItem, items, placedItems, flyingItem]);

  // Calculate coins earned
  const calculateCoins = () => {
    return totalPlaced * 5 + (completed ? 50 : 0);
  };

  // Handle finish
  const handleFinish = () => {
    onGameEnd(calculateCoins());
  };

  return (
    <div
      className="organize-game relaxing"
      style={{ background: currentScene.background }}
    >
      {/* Header */}
      <div className="organize-header relaxing">
        <button className="organize-close-btn" onClick={onClose}>✕</button>
        <div className="scene-info">
          <span className="scene-name">{currentScene.name}</span>
          <span className="scene-progress">
            Room {currentSceneIndex + 1} of {SCENES.length}
          </span>
        </div>
        <div className="organize-progress">
          <div className="progress-bar">
            <div
              className="progress-fill"
              style={{ width: `${(totalPlaced / totalItems) * 100}%` }}
            />
          </div>
          <span className="progress-text">{totalPlaced}/{totalItems}</span>
        </div>
      </div>

      {/* Game content */}
      <div className="organize-content relaxing" ref={gameAreaRef}>
        {/* Drop zones */}
        <div className="zones-container">
          {currentScene.zones.map(zone => {
            const zoneItems = placedItems[zone.id] || [];
            return (
              <div
                key={zone.id}
                ref={el => zoneRefs.current[zone.id] = el}
                className={`drop-zone relaxing ${selectedItem && items.find(i => i.id === selectedItem)?.type === zone.id ? 'accepting' : ''}`}
                style={{ '--zone-color': zone.color }}
                onClick={() => handleZoneTap(zone)}
              >
                <div className="zone-header">
                  <span className="zone-emoji">{zone.emoji}</span>
                  <span className="zone-name">{zone.name}</span>
                </div>
                <div className="zone-slots">
                  {Array.from({ length: zone.slots }).map((_, i) => (
                    <div key={i} className={`slot ${zoneItems[i] ? 'filled' : ''}`}>
                      {zoneItems[i] && (
                        <span className="placed-item">{zoneItems[i].emoji}</span>
                      )}
                    </div>
                  ))}
                </div>
              </div>
            );
          })}
        </div>

        {/* Messy items area */}
        <div className="items-area relaxing">
          <div className="messy-area">
            {items.filter(item => item.visible).map(item => (
              <div
                key={item.id}
                className={`item-card ${selectedItem === item.id ? 'selected' : ''}`}
                style={{
                  left: item.x,
                  top: item.y,
                  transform: `rotate(${item.rotation}deg)`,
                }}
                onClick={() => handleItemTap(item)}
              >
                <span className="item-emoji">{item.emoji}</span>
              </div>
            ))}

            {/* Flying item animation */}
            {flyingItem && (
              <div
                className="item-card flying"
                style={{
                  '--start-x': `${flyingItem.x}px`,
                  '--start-y': `${flyingItem.y}px`,
                  '--end-x': `${flyingItem.targetX}px`,
                  '--end-y': `${flyingItem.targetY}px`,
                }}
              >
                <span className="item-emoji">{flyingItem.emoji}</span>
              </div>
            )}

            {/* Sparkles */}
            {sparkles.map(sparkle => (
              <div
                key={sparkle.id}
                className="sparkle"
                style={{
                  left: sparkle.x,
                  top: sparkle.y,
                  animationDelay: `${sparkle.delay}s`,
                }}
              >
                ✨
              </div>
            ))}
          </div>
        </div>

        {/* Instructions */}
        {!selectedItem && items.some(i => i.visible) && (
          <div className="hint-text">
            Tap an item to pick it up
          </div>
        )}
        {selectedItem && (
          <div className="hint-text active">
            Now tap where it belongs!
          </div>
        )}
      </div>

      {/* Completion overlay */}
      {completed && (
        <div className="organize-complete-overlay">
          <div className="complete-content">
            <div className="complete-icon">🏠</div>
            <h1>All Tidy!</h1>
            <p className="complete-subtitle">Everything is in its place</p>

            <div className="complete-stats">
              <div className="stat-row">
                <span>Items Organized</span>
                <span>{totalPlaced}</span>
              </div>
              <div className="stat-row">
                <span>Rooms Cleaned</span>
                <span>{SCENES.length}</span>
              </div>
              <div className="stat-row highlight">
                <span>Coins Earned</span>
                <span>🪙 {calculateCoins()}</span>
              </div>
            </div>

            <button className="complete-btn" onClick={handleFinish}>
              Satisfying! ✨
            </button>
          </div>
        </div>
      )}
    </div>
  );
};

export default OrganizeGame;
