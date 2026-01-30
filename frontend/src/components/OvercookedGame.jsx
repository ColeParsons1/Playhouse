import { useState, useEffect, useCallback, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './OvercookedGame.css';

// Game constants
const TILE_SIZE = 60;
const PLAYER_SPEED = 4;
const COOK_TIME = 3000; // 3 seconds to cook
const BURN_TIME = 6000; // 6 seconds until burn
const ORDER_TIME = 60000; // 60 seconds per order
const ORDER_INTERVAL = 15000; // New order every 15 seconds
const MAX_ORDERS = 4;

// Indian Ingredients
const INGREDIENTS = {
  rice: { emoji: '🍚', color: '#F5F5DC', name: 'Basmati Rice' },
  naan: { emoji: '🫓', color: '#D4A574', name: 'Naan' },
  chicken: { emoji: '🍗', color: '#C0392B', name: 'Chicken' },
  paneer: { emoji: '🧀', color: '#FFF8DC', name: 'Paneer' },
  onion: { emoji: '🧅', color: '#E8D5B7', name: 'Onion' },
  tomato: { emoji: '🍅', color: '#E74C3C', name: 'Tomato' },
  spices: { emoji: '🌶️', color: '#FF6B35', name: 'Masala' },
  lentils: { emoji: '🫘', color: '#DAA520', name: 'Dal' },
  spinach: { emoji: '🥬', color: '#228B22', name: 'Palak' },
  potato: { emoji: '🥔', color: '#C4A35A', name: 'Aloo' },
};

// Indian Recipes
const RECIPES = {
  butterChicken: {
    name: 'Butter Chicken',
    emoji: '🍛',
    ingredients: ['chicken', 'tomato', 'spices', 'onion'],
    points: 120,
    tip: 60,
  },
  dalMakhani: {
    name: 'Dal Makhani',
    emoji: '🥘',
    ingredients: ['lentils', 'tomato', 'spices'],
    points: 80,
    tip: 40,
  },
  palakPaneer: {
    name: 'Palak Paneer',
    emoji: '🥗',
    ingredients: ['paneer', 'spinach', 'spices', 'onion'],
    points: 100,
    tip: 50,
  },
  chickenBiryani: {
    name: 'Chicken Biryani',
    emoji: '🍚',
    ingredients: ['rice', 'chicken', 'spices', 'onion'],
    points: 150,
    tip: 75,
  },
  alooParatha: {
    name: 'Aloo Paratha',
    emoji: '🫓',
    ingredients: ['naan', 'potato', 'spices'],
    points: 70,
    tip: 35,
  },
  vegBiryani: {
    name: 'Veg Biryani',
    emoji: '🍚',
    ingredients: ['rice', 'potato', 'spices', 'onion'],
    points: 110,
    tip: 55,
  },
  paneerTikka: {
    name: 'Paneer Tikka',
    emoji: '🧀',
    ingredients: ['paneer', 'spices', 'onion'],
    points: 90,
    tip: 45,
  },
  chickenCurry: {
    name: 'Chicken Curry',
    emoji: '🍛',
    ingredients: ['chicken', 'tomato', 'spices'],
    points: 100,
    tip: 50,
  },
};

// Kitchen layouts for different levels - Indian themed
const KITCHEN_LAYOUTS = [
  // Level 1 - Dhaba (Roadside Eatery)
  {
    width: 7,
    height: 6,
    tiles: [
      'WRTHDOW',
      'I.....P',
      'W.....W',
      'W.....W',
      'W.....W',
      'WXXSXXW',
    ],
    recipes: ['dalMakhani', 'alooParatha', 'chickenCurry'],
    timeLimit: 180,
    name: 'Dhaba Kitchen',
  },
  // Level 2 - Mughlai Restaurant
  {
    width: 8,
    height: 7,
    tiles: [
      'WRTHWWWW',
      'I.....NL',
      'O......P',
      'W..K...W',
      'W......W',
      'W......W',
      'WXXSSXXW',
    ],
    recipes: ['butterChicken', 'chickenBiryani', 'dalMakhani'],
    timeLimit: 180,
    name: 'Mughlai Kitchen',
  },
  // Level 3 - Grand Indian Restaurant
  {
    width: 9,
    height: 7,
    tiles: [
      'WRTIOHNDL',
      'W.......P',
      'W.KKKKK.W',
      'W.......W',
      'W.KKKKK.W',
      'W.......W',
      'WXXSSSXXW',
    ],
    recipes: ['butterChicken', 'palakPaneer', 'chickenBiryani', 'vegBiryani', 'paneerTikka'],
    timeLimit: 180,
    name: 'Royal Indian Kitchen',
  },
];

// Tile types - Indian Kitchen
const TILE_TYPES = {
  W: { type: 'wall', emoji: '', color: '#8B4513', solid: true }, // Wood/clay wall
  '.': { type: 'floor', emoji: '', color: '#D2B48C', solid: false }, // Terracotta floor
  R: { type: 'ingredient', ingredient: 'rice', color: '#CD853F', solid: true },
  N: { type: 'ingredient', ingredient: 'naan', color: '#CD853F', solid: true },
  H: { type: 'ingredient', ingredient: 'chicken', color: '#CD853F', solid: true },
  A: { type: 'ingredient', ingredient: 'paneer', color: '#CD853F', solid: true },
  O: { type: 'ingredient', ingredient: 'onion', color: '#CD853F', solid: true },
  T: { type: 'ingredient', ingredient: 'tomato', color: '#CD853F', solid: true },
  I: { type: 'ingredient', ingredient: 'spices', color: '#CD853F', solid: true },
  L: { type: 'ingredient', ingredient: 'lentils', color: '#CD853F', solid: true },
  D: { type: 'ingredient', ingredient: 'spinach', color: '#CD853F', solid: true },
  U: { type: 'ingredient', ingredient: 'potato', color: '#CD853F', solid: true },
  S: { type: 'stove', emoji: '🔥', color: '#8B0000', solid: true }, // Tandoor/stove
  P: { type: 'plate', emoji: '🍽️', color: '#CD853F', solid: true }, // Thali plate
  K: { type: 'counter', emoji: '', color: '#A0522D', solid: true }, // Wooden counter
  X: { type: 'serve', emoji: '📤', color: '#FF8C00', solid: true }, // Serve window (saffron)
};

const OvercookedGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('menu'); // menu, playing, paused, levelComplete, gameOver
  const [level, setLevel] = useState(0);
  const [score, setScore] = useState(0);
  const [timeLeft, setTimeLeft] = useState(0);
  const [orders, setOrders] = useState([]);
  const [player, setPlayer] = useState({ x: 3, y: 3, direction: 'down', holding: null });
  const [counters, setCounters] = useState({}); // Items on counters/stoves
  const [cooking, setCooking] = useState({}); // Cooking progress on stoves
  const [particles, setParticles] = useState([]);
  const [comboCount, setComboCount] = useState(0);
  const [lastOrderTime, setLastOrderTime] = useState(0);

  const gameRef = useRef(null);
  const keysPressed = useRef({});
  const animationRef = useRef(null);
  const lastTimeRef = useRef(0);
  const orderTimerRef = useRef(null);
  const containerRef = useRef(null);
  const [kitchenScale, setKitchenScale] = useState(1);

  const currentLayout = KITCHEN_LAYOUTS[level] || KITCHEN_LAYOUTS[0];

  // Calculate optimal scale for kitchen to fit in available space
  useEffect(() => {
    const calculateScale = () => {
      if (!containerRef.current || !currentLayout) return;

      const container = containerRef.current;
      const availableWidth = container.clientWidth - 20; // padding
      const availableHeight = container.clientHeight - 20;

      const kitchenWidth = currentLayout.width * TILE_SIZE;
      const kitchenHeight = currentLayout.height * TILE_SIZE;

      const scaleX = availableWidth / kitchenWidth;
      const scaleY = availableHeight / kitchenHeight;
      const scale = Math.min(scaleX, scaleY, 1); // Don't scale up, only down

      setKitchenScale(Math.max(0.5, scale)); // Minimum scale of 0.5
    };

    calculateScale();
    window.addEventListener('resize', calculateScale);
    return () => window.removeEventListener('resize', calculateScale);
  }, [currentLayout, gameState]);

  // Create particles
  const createParticles = useCallback((x, y, color, count = 8) => {
    const newParticles = Array.from({ length: count }, (_, i) => ({
      id: Date.now() + i,
      x,
      y,
      vx: (Math.random() - 0.5) * 6,
      vy: (Math.random() - 0.5) * 6 - 2,
      color,
      life: 1,
      size: 4 + Math.random() * 4,
    }));
    setParticles(prev => [...prev, ...newParticles]);
  }, []);

  // Get tile at position
  const getTile = useCallback((x, y) => {
    if (!currentLayout) return null;
    const row = currentLayout.tiles[y];
    if (!row) return null;
    const tileChar = row[x];
    return TILE_TYPES[tileChar] || null;
  }, [currentLayout]);

  // Check if position is walkable
  const isWalkable = useCallback((x, y) => {
    const tile = getTile(Math.floor(x), Math.floor(y));
    return tile && !tile.solid;
  }, [getTile]);

  // Get facing tile position
  const getFacingTile = useCallback(() => {
    const dirOffsets = {
      up: { x: 0, y: -1 },
      down: { x: 0, y: 1 },
      left: { x: -1, y: 0 },
      right: { x: 1, y: 0 },
    };
    const offset = dirOffsets[player.direction];
    return {
      x: Math.floor(player.x) + offset.x,
      y: Math.floor(player.y) + offset.y,
    };
  }, [player]);

  // Get counter key
  const getCounterKey = useCallback((x, y) => `${x},${y}`, []);

  // Check if ingredients match a recipe
  const checkRecipe = useCallback((ingredients) => {
    for (const [recipeId, recipe] of Object.entries(RECIPES)) {
      const sorted1 = [...ingredients].sort();
      const sorted2 = [...recipe.ingredients].sort();
      if (sorted1.length === sorted2.length && sorted1.every((v, i) => v === sorted2[i])) {
        return { id: recipeId, ...recipe };
      }
    }
    return null;
  }, []);

  // Generate a new order
  const generateOrder = useCallback(() => {
    if (!currentLayout || orders.length >= MAX_ORDERS) return;

    const availableRecipes = currentLayout.recipes;
    const recipeId = availableRecipes[Math.floor(Math.random() * availableRecipes.length)];
    const recipe = RECIPES[recipeId];

    const newOrder = {
      id: Date.now(),
      recipeId,
      recipe,
      timeLeft: ORDER_TIME,
      createdAt: Date.now(),
    };

    setOrders(prev => [...prev, newOrder]);
  }, [currentLayout, orders.length]);

  // Start game
  const startGame = useCallback((levelIndex = 0) => {
    const layout = KITCHEN_LAYOUTS[levelIndex];
    if (!layout) {
      console.error('No layout found for level', levelIndex);
      return;
    }

    setLevel(levelIndex);
    setScore(0);
    setTimeLeft(layout.timeLimit);
    setOrders([]);

    // Find a walkable starting position
    let startX = 1.5, startY = 1.5;
    let foundSpot = false;
    for (let y = 0; y < layout.height && !foundSpot; y++) {
      for (let x = 0; x < layout.width && !foundSpot; x++) {
        if (layout.tiles[y] && layout.tiles[y][x] === '.') {
          startX = x + 0.5;
          startY = y + 0.5;
          foundSpot = true;
        }
      }
    }

    setPlayer({ x: startX, y: startY, direction: 'down', holding: null });
    setCounters({});
    setCooking({});
    setParticles([]);
    setComboCount(0);
    setLastOrderTime(0);
    setGameState('playing');

    // Generate initial orders after a short delay
    setTimeout(() => {
      generateOrder();
      setTimeout(() => generateOrder(), 2000);
    }, 500);
  }, [generateOrder]);

  // Interact with facing tile
  const interact = useCallback(() => {
    const facing = getFacingTile();
    const tile = getTile(facing.x, facing.y);
    if (!tile) return;

    const counterKey = getCounterKey(facing.x, facing.y);
    const counterItem = counters[counterKey];

    // Ingredient dispenser
    if (tile.type === 'ingredient') {
      if (!player.holding) {
        setPlayer(prev => ({ ...prev, holding: { type: 'ingredient', item: tile.ingredient } }));
        createParticles(facing.x * TILE_SIZE + TILE_SIZE / 2, facing.y * TILE_SIZE + TILE_SIZE / 2, INGREDIENTS[tile.ingredient].color);
      }
      return;
    }

    // Plate dispenser
    if (tile.type === 'plate') {
      if (!player.holding) {
        setPlayer(prev => ({ ...prev, holding: { type: 'plate', items: [] } }));
        createParticles(facing.x * TILE_SIZE + TILE_SIZE / 2, facing.y * TILE_SIZE + TILE_SIZE / 2, '#FFFFFF');
      }
      return;
    }

    // Stove/Tandoor
    if (tile.type === 'stove') {
      // Items that can be cooked: chicken, paneer, naan
      const cookableItems = ['chicken', 'paneer', 'naan'];
      if (player.holding?.type === 'ingredient' && cookableItems.includes(player.holding.item)) {
        // Put item in tandoor/stove
        if (!counterItem) {
          setCounters(prev => ({
            ...prev,
            [counterKey]: { type: 'cooking', item: player.holding.item, startTime: Date.now() },
          }));
          setCooking(prev => ({ ...prev, [counterKey]: { progress: 0, startTime: Date.now() } }));
          setPlayer(prev => ({ ...prev, holding: null }));
          createParticles(facing.x * TILE_SIZE + TILE_SIZE / 2, facing.y * TILE_SIZE + TILE_SIZE / 2, '#FF6B6B');
        }
      } else if (!player.holding && counterItem?.type === 'cooking' && counterItem.cooked && !counterItem.burnt) {
        // Pick up cooked item
        const cookedName = `cooked${counterItem.item.charAt(0).toUpperCase() + counterItem.item.slice(1)}`;
        setPlayer(prev => ({ ...prev, holding: { type: 'ingredient', item: cookedName } }));
        setCounters(prev => {
          const newCounters = { ...prev };
          delete newCounters[counterKey];
          return newCounters;
        });
        setCooking(prev => {
          const newCooking = { ...prev };
          delete newCooking[counterKey];
          return newCooking;
        });
      }
      return;
    }

    // Counter / Serve
    if (tile.type === 'counter' || tile.type === 'serve') {
      // Serving window
      if (tile.type === 'serve' && player.holding?.type === 'plate' && player.holding.items.length > 0) {
        const dish = checkRecipe(player.holding.items);
        if (dish) {
          // Find matching order
          const orderIndex = orders.findIndex(o => o.recipeId === dish.id);
          if (orderIndex >= 0) {
            const order = orders[orderIndex];
            const timeBonus = Math.floor((order.timeLeft / ORDER_TIME) * dish.tip);
            const comboBonus = comboCount >= 2 ? Math.floor(dish.points * 0.25 * comboCount) : 0;
            const totalPoints = dish.points + timeBonus + comboBonus;

            setScore(prev => prev + totalPoints);
            setComboCount(prev => prev + 1);
            setOrders(prev => prev.filter((_, i) => i !== orderIndex));

            createParticles(facing.x * TILE_SIZE + TILE_SIZE / 2, facing.y * TILE_SIZE + TILE_SIZE / 2, '#FFD700', 15);

            // Clear plate
            setPlayer(prev => ({ ...prev, holding: null }));
            return;
          }
        }
        // Wrong dish
        createParticles(facing.x * TILE_SIZE + TILE_SIZE / 2, facing.y * TILE_SIZE + TILE_SIZE / 2, '#E74C3C', 10);
        setComboCount(0);
        return;
      }

      // Place item on counter
      if (player.holding && !counterItem) {
        setCounters(prev => ({ ...prev, [counterKey]: player.holding }));
        setPlayer(prev => ({ ...prev, holding: null }));
        return;
      }

      // Pick up item from counter
      if (!player.holding && counterItem) {
        setPlayer(prev => ({ ...prev, holding: counterItem }));
        setCounters(prev => {
          const newCounters = { ...prev };
          delete newCounters[counterKey];
          return newCounters;
        });
        return;
      }

      // Combine items
      if (player.holding && counterItem) {
        // Helper to get base ingredient from cooked version
        const getBaseIngredient = (item) => {
          if (item.startsWith('cooked')) {
            return item.replace('cooked', '').toLowerCase();
          }
          return item;
        };

        // Add ingredient to plate (thali)
        if (player.holding.type === 'ingredient' && counterItem.type === 'plate') {
          const ingredient = getBaseIngredient(player.holding.item);
          if (!counterItem.items.includes(ingredient)) {
            setCounters(prev => ({
              ...prev,
              [counterKey]: {
                ...counterItem,
                items: [...counterItem.items, ingredient],
              },
            }));
            setPlayer(prev => ({ ...prev, holding: null }));
            createParticles(facing.x * TILE_SIZE + TILE_SIZE / 2, facing.y * TILE_SIZE + TILE_SIZE / 2, INGREDIENTS[ingredient]?.color || '#FFF');
          }
          return;
        }

        // Add counter item to held plate (thali)
        if (player.holding.type === 'plate' && counterItem.type === 'ingredient') {
          const ingredient = getBaseIngredient(counterItem.item);
          if (!player.holding.items.includes(ingredient)) {
            setPlayer(prev => ({
              ...prev,
              holding: {
                ...prev.holding,
                items: [...prev.holding.items, ingredient],
              },
            }));
            setCounters(prev => {
              const newCounters = { ...prev };
              delete newCounters[counterKey];
              return newCounters;
            });
            createParticles(facing.x * TILE_SIZE + TILE_SIZE / 2, facing.y * TILE_SIZE + TILE_SIZE / 2, INGREDIENTS[ingredient]?.color || '#FFF');
          }
          return;
        }
      }
    }
  }, [player, counters, orders, comboCount, getFacingTile, getTile, getCounterKey, checkRecipe, createParticles]);

  // Handle key down
  useEffect(() => {
    const handleKeyDown = (e) => {
      if (gameState !== 'playing') return;

      keysPressed.current[e.key.toLowerCase()] = true;

      if (e.key === ' ' || e.key === 'e' || e.key === 'Enter') {
        e.preventDefault();
        interact();
      }

      if (e.key === 'Escape') {
        setGameState('paused');
      }
    };

    const handleKeyUp = (e) => {
      keysPressed.current[e.key.toLowerCase()] = false;
    };

    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);

    return () => {
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);
    };
  }, [gameState, interact]);

  // Game loop
  useEffect(() => {
    if (gameState !== 'playing') return;

    const gameLoop = (timestamp) => {
      const deltaTime = Math.min((timestamp - lastTimeRef.current) / 1000, 0.1);
      lastTimeRef.current = timestamp;

      // Player movement
      let dx = 0;
      let dy = 0;
      let newDirection = player.direction;

      if (keysPressed.current['w'] || keysPressed.current['arrowup']) {
        dy -= PLAYER_SPEED * deltaTime;
        newDirection = 'up';
      }
      if (keysPressed.current['s'] || keysPressed.current['arrowdown']) {
        dy += PLAYER_SPEED * deltaTime;
        newDirection = 'down';
      }
      if (keysPressed.current['a'] || keysPressed.current['arrowleft']) {
        dx -= PLAYER_SPEED * deltaTime;
        newDirection = 'left';
      }
      if (keysPressed.current['d'] || keysPressed.current['arrowright']) {
        dx += PLAYER_SPEED * deltaTime;
        newDirection = 'right';
      }

      if (dx !== 0 || dy !== 0) {
        setPlayer(prev => {
          let newX = prev.x + dx;
          let newY = prev.y + dy;

          // Check collision
          const checkX = dx > 0 ? newX + 0.4 : newX - 0.4;
          const checkY = dy > 0 ? newY + 0.4 : newY - 0.4;

          if (!isWalkable(checkX, prev.y)) newX = prev.x;
          if (!isWalkable(prev.x, checkY)) newY = prev.y;

          // Bounds
          newX = Math.max(0.5, Math.min(currentLayout.width - 0.5, newX));
          newY = Math.max(0.5, Math.min(currentLayout.height - 0.5, newY));

          return { ...prev, x: newX, y: newY, direction: newDirection };
        });
      } else if (newDirection !== player.direction) {
        setPlayer(prev => ({ ...prev, direction: newDirection }));
      }

      // Update particles
      setParticles(prev =>
        prev
          .map(p => ({
            ...p,
            x: p.x + p.vx,
            y: p.y + p.vy,
            vy: p.vy + 0.2,
            life: p.life - 0.02,
          }))
          .filter(p => p.life > 0)
      );

      // Update cooking
      setCooking(prev => {
        const newCooking = { ...prev };
        Object.keys(newCooking).forEach(key => {
          const elapsed = Date.now() - newCooking[key].startTime;
          newCooking[key].progress = Math.min(elapsed / COOK_TIME, 1);
        });
        return newCooking;
      });

      // Update counters for cooking state
      setCounters(prev => {
        const newCounters = { ...prev };
        Object.keys(newCounters).forEach(key => {
          if (newCounters[key]?.type === 'cooking') {
            const elapsed = Date.now() - newCounters[key].startTime;
            if (elapsed >= BURN_TIME) {
              newCounters[key].burnt = true;
            } else if (elapsed >= COOK_TIME) {
              newCounters[key].cooked = true;
            }
          }
        });
        return newCounters;
      });

      animationRef.current = requestAnimationFrame(gameLoop);
    };

    animationRef.current = requestAnimationFrame(gameLoop);

    return () => {
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }
    };
  }, [gameState, player.direction, currentLayout, isWalkable]);

  // Timer and order generation
  useEffect(() => {
    if (gameState !== 'playing') return;

    const timer = setInterval(() => {
      setTimeLeft(prev => {
        if (prev <= 1) {
          setGameState('levelComplete');
          return 0;
        }
        return prev - 1;
      });

      // Update order timers
      setOrders(prev =>
        prev.map(order => ({
          ...order,
          timeLeft: Math.max(0, ORDER_TIME - (Date.now() - order.createdAt)),
        })).filter(order => order.timeLeft > 0)
      );

      // Failed orders reset combo
      const expiredCount = orders.filter(o => ORDER_TIME - (Date.now() - o.createdAt) <= 0).length;
      if (expiredCount > 0) {
        setComboCount(0);
      }
    }, 1000);

    // Order generation
    orderTimerRef.current = setInterval(() => {
      if (orders.length < MAX_ORDERS) {
        generateOrder();
      }
    }, ORDER_INTERVAL);

    return () => {
      clearInterval(timer);
      if (orderTimerRef.current) {
        clearInterval(orderTimerRef.current);
      }
    };
  }, [gameState, orders, generateOrder]);

  // Touch controls
  const handleTouch = useCallback((direction) => {
    if (gameState !== 'playing') return;

    setPlayer(prev => {
      const dirOffsets = {
        up: { x: 0, y: -0.5 },
        down: { x: 0, y: 0.5 },
        left: { x: -0.5, y: 0 },
        right: { x: 0.5, y: 0 },
      };
      const offset = dirOffsets[direction];
      let newX = prev.x + offset.x;
      let newY = prev.y + offset.y;

      if (!isWalkable(newX, prev.y)) newX = prev.x;
      if (!isWalkable(prev.x, newY)) newY = prev.y;

      newX = Math.max(0.5, Math.min(currentLayout.width - 0.5, newX));
      newY = Math.max(0.5, Math.min(currentLayout.height - 0.5, newY));

      return { ...prev, x: newX, y: newY, direction };
    });
  }, [gameState, currentLayout, isWalkable]);

  // Next level
  const nextLevel = useCallback(() => {
    const nextLevelIndex = level + 1;
    if (nextLevelIndex < KITCHEN_LAYOUTS.length) {
      startGame(nextLevelIndex);
    } else {
      setGameState('gameOver');
    }
  }, [level, startGame]);

  // Render
  const renderTile = (tileChar, x, y) => {
    const tile = TILE_TYPES[tileChar];
    if (!tile) return null;

    const counterKey = getCounterKey(x, y);
    const counterItem = counters[counterKey];
    const cookingState = cooking[counterKey];

    return (
      <div
        key={`${x}-${y}`}
        className={`tile tile-${tile.type}`}
        style={{
          left: x * TILE_SIZE,
          top: y * TILE_SIZE,
          width: TILE_SIZE,
          height: TILE_SIZE,
          background: tile.color,
        }}
      >
        {/* Ingredient dispenser */}
        {tile.type === 'ingredient' && (
          <div className="dispenser">
            <span className="dispenser-emoji">{INGREDIENTS[tile.ingredient].emoji}</span>
            <div className="dispenser-label">{INGREDIENTS[tile.ingredient].name}</div>
          </div>
        )}

        {/* Stove flame */}
        {tile.type === 'stove' && (
          <div className={`stove ${counterItem ? 'active' : ''}`}>
            <span className="stove-flame">🔥</span>
          </div>
        )}

        {/* Plate dispenser */}
        {tile.type === 'plate' && (
          <div className="plate-stack">
            <span>🍽️</span>
          </div>
        )}

        {/* Serve window */}
        {tile.type === 'serve' && (
          <div className="serve-window">
            <span>📤</span>
          </div>
        )}

        {/* Counter items */}
        {counterItem && tile.type !== 'stove' && (
          <div className="counter-item">
            {counterItem.type === 'ingredient' && (
              <span className="item-emoji">
                {counterItem.item.startsWith('cooked')
                  ? (counterItem.item === 'cookedChicken' ? '🍗' : counterItem.item === 'cookedPaneer' ? '🧈' : '🫓')
                  : INGREDIENTS[counterItem.item]?.emoji}
              </span>
            )}
            {counterItem.type === 'plate' && (
              <div className="plate-items">
                <span className="plate-base">🍽️</span>
                {counterItem.items.map((item, i) => (
                  <span key={i} className="plate-ingredient" style={{ animationDelay: `${i * 0.1}s` }}>
                    {INGREDIENTS[item]?.emoji}
                  </span>
                ))}
              </div>
            )}
          </div>
        )}

        {/* Cooking in Tandoor/Stove */}
        {counterItem?.type === 'cooking' && tile.type === 'stove' && (
          <div className={`cooking-item ${counterItem.burnt ? 'burnt' : counterItem.cooked ? 'cooked' : ''}`}>
            <span className="cooking-emoji">
              {counterItem.burnt ? '🔥' : counterItem.cooked
                ? (counterItem.item === 'chicken' ? '🍗' : counterItem.item === 'paneer' ? '🧈' : '🫓')
                : INGREDIENTS[counterItem.item]?.emoji}
            </span>
            {!counterItem.cooked && !counterItem.burnt && cookingState && (
              <div className="cook-progress">
                <div
                  className="cook-progress-bar"
                  style={{ width: `${cookingState.progress * 100}%` }}
                />
              </div>
            )}
            {counterItem.cooked && !counterItem.burnt && (
              <div className="cooked-indicator">✓</div>
            )}
            {counterItem.burnt && (
              <div className="burnt-indicator">💨</div>
            )}
          </div>
        )}
      </div>
    );
  };

  const handleFinish = useCallback(() => {
    onGameEnd(score);
  }, [score, onGameEnd]);

  return (
    <div className="overcooked-overlay">
      <div className="overcooked-container">
        {/* Menu */}
        {gameState === 'menu' && (
          <motion.div
            className="overcooked-menu"
            initial={{ opacity: 0, scale: 0.9 }}
            animate={{ opacity: 1, scale: 1 }}
          >
            <motion.h1
              className="overcooked-title"
              initial={{ y: -30 }}
              animate={{ y: 0 }}
              transition={{ type: 'spring', stiffness: 200 }}
            >
              <span>🍛</span> Masala Kitchen <span>🌶️</span>
            </motion.h1>
            <p className="overcooked-subtitle">Cook authentic Indian dishes before time runs out!</p>

            <div className="overcooked-instructions">
              <p>🎮 WASD or Arrow keys to move</p>
              <p>⌨️ Space/E to interact</p>
              <p>🔥 Cook in the Tandoor</p>
              <p>🍽️ Assemble dishes on thali</p>
              <p>📤 Serve to hungry customers</p>
            </div>

            <div className="level-select">
              <h3>Select Restaurant</h3>
              <div className="level-buttons">
                {KITCHEN_LAYOUTS.map((layout, i) => (
                  <motion.button
                    key={i}
                    className="level-btn"
                    onClick={() => startGame(i)}
                    whileHover={{ scale: 1.05 }}
                    whileTap={{ scale: 0.95 }}
                  >
                    <span className="level-num">Level {i + 1}</span>
                    <span className="level-name">{layout.name}</span>
                  </motion.button>
                ))}
              </div>
            </div>

            <button className="close-button-menu" onClick={onClose}>
              Back to Games
            </button>
          </motion.div>
        )}

        {/* Playing */}
        {(gameState === 'playing' || gameState === 'paused') && currentLayout && currentLayout.tiles && (
          <div className="game-area">
            {/* Header */}
            <div className="overcooked-header">
              <div className="header-left">
                <div className="level-badge">
                  <span>🏠</span> {currentLayout.name}
                </div>
                <div className="score-display">
                  <span>🪙</span> {score}
                </div>
              </div>

              <div className="timer-display" style={{ color: timeLeft < 30 ? '#E74C3C' : 'white' }}>
                <span>⏱️</span> {Math.floor(timeLeft / 60)}:{(timeLeft % 60).toString().padStart(2, '0')}
              </div>

              <div className="header-right">
                {comboCount >= 2 && (
                  <motion.div
                    className="combo-badge"
                    initial={{ scale: 0 }}
                    animate={{ scale: 1 }}
                  >
                    🔥 {comboCount}x Combo!
                  </motion.div>
                )}
              </div>
            </div>

            {/* Orders */}
            <div className="orders-bar">
              <AnimatePresence>
                {orders.map(order => (
                  <motion.div
                    key={order.id}
                    className="order-card"
                    initial={{ x: 100, opacity: 0 }}
                    animate={{ x: 0, opacity: 1 }}
                    exit={{ x: -100, opacity: 0 }}
                  >
                    <div className="order-icon">{order.recipe.emoji}</div>
                    <div className="order-name">{order.recipe.name}</div>
                    <div className="order-ingredients">
                      {order.recipe.ingredients.map((ing, i) => (
                        <span key={i}>{INGREDIENTS[ing]?.emoji}</span>
                      ))}
                    </div>
                    <div
                      className="order-timer"
                      style={{
                        width: `${(order.timeLeft / ORDER_TIME) * 100}%`,
                        background: order.timeLeft < 10000 ? '#E74C3C' : order.timeLeft < 30000 ? '#F39C12' : '#2ECC71',
                      }}
                    />
                  </motion.div>
                ))}
              </AnimatePresence>
            </div>

            {/* Kitchen Container */}
            <div className="kitchen-container" ref={containerRef}>
              <div
                className="kitchen"
                ref={gameRef}
                style={{
                  width: currentLayout.width * TILE_SIZE,
                  height: currentLayout.height * TILE_SIZE,
                  transform: `scale(${kitchenScale})`,
                  transformOrigin: 'center center',
                }}
              >
              {/* Tiles */}
              {currentLayout.tiles.map((row, y) => {
                const chars = typeof row === 'string' ? row.split('') : [];
                return chars.map((tileChar, x) => renderTile(tileChar, x, y));
              })}

              {/* Player */}
              <motion.div
                className={`player direction-${player.direction}`}
                style={{
                  left: player.x * TILE_SIZE - TILE_SIZE / 2,
                  top: player.y * TILE_SIZE - TILE_SIZE / 2,
                  width: TILE_SIZE,
                  height: TILE_SIZE,
                }}
                animate={{
                  scale: player.holding ? 1.1 : 1,
                }}
              >
                <div className="player-body">
                  <div className="player-hat">👨‍🍳</div>
                </div>
                {player.holding && (
                  <div className="player-holding">
                    {player.holding.type === 'ingredient' && (
                      <span>
                        {player.holding.item.startsWith('cooked')
                          ? (player.holding.item === 'cookedChicken' ? '🍗' : player.holding.item === 'cookedPaneer' ? '🧈' : '🫓')
                          : INGREDIENTS[player.holding.item]?.emoji}
                      </span>
                    )}
                    {player.holding.type === 'plate' && (
                      <span>🍽️</span>
                    )}
                  </div>
                )}
              </motion.div>

              {/* Particles */}
              {particles.map(p => (
                <div
                  key={p.id}
                  className="particle"
                  style={{
                    left: p.x,
                    top: p.y,
                    width: p.size,
                    height: p.size,
                    background: p.color,
                    opacity: p.life,
                  }}
                />
              ))}
              </div>
            </div>

            {/* Touch Controls */}
            <div className="touch-controls">
              <div className="touch-dpad">
                <button className="touch-btn up" onTouchStart={() => handleTouch('up')} onClick={() => handleTouch('up')}>
                  ▲
                </button>
                <div className="touch-row">
                  <button className="touch-btn left" onTouchStart={() => handleTouch('left')} onClick={() => handleTouch('left')}>
                    ◀
                  </button>
                  <button className="touch-btn action" onTouchStart={interact} onClick={interact}>
                    ✋
                  </button>
                  <button className="touch-btn right" onTouchStart={() => handleTouch('right')} onClick={() => handleTouch('right')}>
                    ▶
                  </button>
                </div>
                <button className="touch-btn down" onTouchStart={() => handleTouch('down')} onClick={() => handleTouch('down')}>
                  ▼
                </button>
              </div>
            </div>

            {/* Held Item Display */}
            {player.holding && (
              <div className="held-item-display">
                <span>Holding: </span>
                {player.holding.type === 'ingredient' && (
                  <span>
                    {player.holding.item.startsWith('cooked')
                      ? `${player.holding.item === 'cookedChicken' ? '🍗 Tandoori Chicken' : player.holding.item === 'cookedPaneer' ? '🧈 Grilled Paneer' : '🫓 Fresh Naan'}`
                      : `${INGREDIENTS[player.holding.item]?.emoji} ${INGREDIENTS[player.holding.item]?.name}`}
                  </span>
                )}
                {player.holding.type === 'plate' && (
                  <span>🍽️ Thali {player.holding.items.length > 0 ? `(${player.holding.items.map(i => INGREDIENTS[i]?.emoji).join('')})` : '(empty)'}</span>
                )}
              </div>
            )}

            {/* Pause overlay */}
            <AnimatePresence>
              {gameState === 'paused' && (
                <motion.div
                  className="pause-overlay"
                  initial={{ opacity: 0 }}
                  animate={{ opacity: 1 }}
                  exit={{ opacity: 0 }}
                >
                  <div className="pause-content">
                    <h2>⏸️ Paused</h2>
                    <button onClick={() => setGameState('playing')}>Resume</button>
                    <button onClick={() => setGameState('menu')}>Quit to Menu</button>
                  </div>
                </motion.div>
              )}
            </AnimatePresence>
          </div>
        )}

        {/* Level Complete */}
        <AnimatePresence>
          {gameState === 'levelComplete' && (
            <motion.div
              className="level-complete-screen"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
            >
              <motion.div
                className="level-complete-content"
                initial={{ scale: 0.8, y: 50 }}
                animate={{ scale: 1, y: 0 }}
                transition={{ type: 'spring', stiffness: 200 }}
              >
                <h2>
                  <span>🌟</span> Service Complete! <span>🌟</span>
                </h2>

                <div className="final-stats">
                  <div className="final-stat highlight">
                    <span className="final-stat-icon">🪙</span>
                    <span className="final-stat-label">Score</span>
                    <span className="final-stat-value">{score}</span>
                  </div>
                  {comboCount >= 2 && (
                    <div className="final-stat">
                      <span className="final-stat-icon">🔥</span>
                      <span className="final-stat-label">Best Combo</span>
                      <span className="final-stat-value">{comboCount}x</span>
                    </div>
                  )}
                </div>

                <div className="level-complete-buttons">
                  {level < KITCHEN_LAYOUTS.length - 1 ? (
                    <motion.button
                      className="next-level-btn"
                      onClick={nextLevel}
                      whileHover={{ scale: 1.02 }}
                      whileTap={{ scale: 0.98 }}
                    >
                      Next Restaurant ▶
                    </motion.button>
                  ) : (
                    <motion.button
                      className="claim-rewards-btn"
                      onClick={handleFinish}
                      whileHover={{ scale: 1.02 }}
                      whileTap={{ scale: 0.98 }}
                    >
                      Claim {score} Coins! 🪙
                    </motion.button>
                  )}
                  <button className="replay-btn" onClick={() => startGame(level)}>
                    Replay Level
                  </button>
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
              >
                <h2>
                  <span>🍛</span> All Restaurants Complete! <span>🌶️</span>
                </h2>

                <div className="achievement-badge">
                  <span>👑</span> Master Indian Chef! <span>👑</span>
                </div>

                <div className="final-stats">
                  <div className="final-stat highlight">
                    <span className="final-stat-icon">🪙</span>
                    <span className="final-stat-label">Total Score</span>
                    <span className="final-stat-value">{score}</span>
                  </div>
                </div>

                <motion.button
                  className="claim-rewards-btn"
                  onClick={handleFinish}
                  whileHover={{ scale: 1.02 }}
                  whileTap={{ scale: 0.98 }}
                >
                  Claim {score} Coins! 🪙
                </motion.button>
              </motion.div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Close button */}
        {gameState !== 'menu' && (
          <button className="game-close-btn" onClick={onClose}>
            ✕
          </button>
        )}
      </div>
    </div>
  );
};

export default OvercookedGame;
