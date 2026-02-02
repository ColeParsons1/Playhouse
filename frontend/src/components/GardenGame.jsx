import { useState, useEffect, useCallback, useRef } from 'react';
import { gardenApi } from '../api/client';
import './GardenGame.css';

const PLANT_EMOJIS = {
  sunflower: '🌻',
  tomato: '🍅',
  carrot: '🥕',
  pumpkin: '🎃',
  rainbow_flower: '🌈',
};

const GROWTH_STAGES = {
  0: '🌱',   // Just planted
  25: '🌿',  // Sprouting
  50: '🪴',  // Growing
  75: '🌾',  // Almost ready
  100: null, // Use actual plant emoji
};

const getGrowthEmoji = (plantType, growthPercent) => {
  if (growthPercent >= 100) {
    return PLANT_EMOJIS[plantType] || '🌸';
  }
  // Find the appropriate growth stage
  const stages = Object.keys(GROWTH_STAGES).map(Number).sort((a, b) => b - a);
  for (const stage of stages) {
    if (growthPercent >= stage) {
      return GROWTH_STAGES[stage] || '🌱';
    }
  }
  return '🌱';
};

const formatTime = (seconds) => {
  if (seconds < 60) return `${Math.ceil(seconds)}s`;
  if (seconds < 3600) return `${Math.ceil(seconds / 60)}m`;
  return `${Math.floor(seconds / 3600)}h ${Math.ceil((seconds % 3600) / 60)}m`;
};

const GardenGame = ({ onGameEnd, onClose, player, onPlayerUpdate }) => {
  const [garden, setGarden] = useState(Array(9).fill(null));
  const [plantInfo, setPlantInfo] = useState({});
  const [coins, setCoins] = useState(player?.coins || 0);
  const [selectedSeed, setSelectedSeed] = useState(null);
  const [loading, setLoading] = useState(true);
  const [actionLoading, setActionLoading] = useState(null);
  const [message, setMessage] = useState(null);
  const playerIdRef = useRef(player?.id);

  // Fetch garden state
  const fetchGarden = useCallback(async () => {
    if (!playerIdRef.current) return;

    try {
      const response = await gardenApi.getGarden(playerIdRef.current);
      setGarden(response.data.garden);
      setPlantInfo(response.data.plant_info);
      setCoins(response.data.coins);
    } catch (error) {
      console.error('Failed to fetch garden:', error);
    } finally {
      setLoading(false);
    }
  }, []);

  // Initial fetch and polling
  useEffect(() => {
    playerIdRef.current = player?.id;
    fetchGarden();

    // Poll every 5 seconds for growth updates
    const interval = setInterval(fetchGarden, 5000);
    return () => clearInterval(interval);
  }, [player?.id, fetchGarden]);

  // Show temporary message
  const showMessage = (text, duration = 2000) => {
    setMessage(text);
    setTimeout(() => setMessage(null), duration);
  };

  // Plant a seed
  const handlePlant = async (plotIndex) => {
    if (!selectedSeed || actionLoading) return;

    const cost = plantInfo[selectedSeed]?.cost || 0;
    if (coins < cost) {
      showMessage('Not enough coins! 💰');
      return;
    }

    setActionLoading(plotIndex);
    try {
      const response = await gardenApi.plant(playerIdRef.current, plotIndex, selectedSeed);
      if (response.data.success) {
        // Update local state
        const newGarden = [...garden];
        newGarden[plotIndex] = response.data.plant;
        setGarden(newGarden);
        setCoins(response.data.coins);
        showMessage(`Planted ${PLANT_EMOJIS[selectedSeed]}!`);
        setSelectedSeed(null);
      }
    } catch (error) {
      showMessage(error.response?.data?.error || 'Failed to plant!');
    } finally {
      setActionLoading(null);
    }
  };

  // Water a plant
  const handleWater = async (plant) => {
    if (actionLoading || plant.is_ready) return;

    setActionLoading(plant.plot_index);
    try {
      const response = await gardenApi.water(playerIdRef.current, plant.id);
      if (response.data.success) {
        // Update local state
        const newGarden = [...garden];
        newGarden[plant.plot_index] = response.data.plant;
        setGarden(newGarden);
        showMessage('Watered! 💧 Growth boosted!');
      }
    } catch (error) {
      showMessage(error.response?.data?.error || 'Failed to water!');
    } finally {
      setActionLoading(null);
    }
  };

  // Harvest a plant
  const handleHarvest = async (plant) => {
    if (actionLoading || !plant.is_ready) return;

    setActionLoading(plant.plot_index);
    try {
      const response = await gardenApi.harvest(playerIdRef.current, plant.id);
      if (response.data.success) {
        // Clear the plot
        const newGarden = [...garden];
        newGarden[response.data.plot_index] = null;
        setGarden(newGarden);
        setCoins(response.data.coins);
        showMessage(`Harvested! +${response.data.reward} coins! 🪙`);

        // Update player coins in parent
        if (onPlayerUpdate) {
          onPlayerUpdate({ ...player, coins: response.data.coins });
        }
      }
    } catch (error) {
      showMessage(error.response?.data?.error || 'Failed to harvest!');
    } finally {
      setActionLoading(null);
    }
  };

  // Handle plot click
  const handlePlotClick = (plotIndex) => {
    const plant = garden[plotIndex];

    if (plant) {
      // Existing plant
      if (plant.is_ready) {
        handleHarvest(plant);
      } else {
        handleWater(plant);
      }
    } else if (selectedSeed) {
      // Empty plot with seed selected
      handlePlant(plotIndex);
    }
  };

  // Calculate remaining time for a plant
  const getRemainingTime = (plant) => {
    if (!plant || plant.is_ready) return null;
    const info = plantInfo[plant.plant_type];
    if (!info) return null;

    const effectiveTime = info.grow_time * Math.pow(0.9, plant.water_count);
    const elapsed = (Date.now() - new Date(plant.planted_at).getTime()) / 1000;
    const remaining = effectiveTime - elapsed;
    return remaining > 0 ? remaining : 0;
  };

  if (loading) {
    return (
      <div className="garden-game">
        <div className="garden-loading">
          <span className="loading-emoji">🌱</span>
          <p>Loading garden...</p>
        </div>
      </div>
    );
  }

  return (
    <div className="garden-game">
      {/* Header */}
      <div className="garden-header">
        <button className="garden-close-btn" onClick={onClose}>✕</button>
        <h1 className="garden-title">🌻 Garden 🌻</h1>
        <div className="garden-coins">
          <span className="coin-icon">🪙</span>
          <span className="coin-amount">{coins}</span>
        </div>
      </div>

      {/* Message popup */}
      {message && (
        <div className="garden-message">
          {message}
        </div>
      )}

      {/* Garden Grid */}
      <div className="garden-area">
        <div className="garden-grid">
          {garden.map((plant, index) => (
            <div
              key={index}
              className={`garden-plot ${plant ? 'has-plant' : 'empty'} ${plant?.is_ready ? 'ready' : ''} ${actionLoading === index ? 'loading' : ''} ${!plant && selectedSeed ? 'plantable' : ''}`}
              onClick={() => handlePlotClick(index)}
            >
              {plant ? (
                <div className="plant-container">
                  <span className="plant-emoji">
                    {getGrowthEmoji(plant.plant_type, plant.growth_percent)}
                  </span>
                  {plant.is_ready ? (
                    <div className="harvest-indicator">
                      <span className="sparkle">✨</span>
                      <span className="harvest-text">Tap to harvest!</span>
                    </div>
                  ) : (
                    <>
                      <div className="growth-bar">
                        <div
                          className="growth-fill"
                          style={{ width: `${plant.growth_percent}%` }}
                        />
                      </div>
                      <span className="growth-text">
                        {Math.round(plant.growth_percent)}%
                      </span>
                      <span className="time-remaining">
                        {formatTime(getRemainingTime(plant) || 0)}
                      </span>
                      <button
                        className="water-btn"
                        onClick={(e) => {
                          e.stopPropagation();
                          handleWater(plant);
                        }}
                      >
                        💧
                      </button>
                    </>
                  )}
                </div>
              ) : (
                <div className="empty-plot">
                  <span className="dirt-emoji">🟤</span>
                  {selectedSeed && (
                    <span className="plant-preview">{PLANT_EMOJIS[selectedSeed]}</span>
                  )}
                </div>
              )}
            </div>
          ))}
        </div>
      </div>

      {/* Seed Selection */}
      <div className="seed-selection">
        <h3 className="seed-title">🌱 Select a Seed</h3>
        <div className="seed-grid">
          {Object.entries(plantInfo).map(([type, info]) => (
            <button
              key={type}
              className={`seed-btn ${selectedSeed === type ? 'selected' : ''} ${coins < info.cost ? 'disabled' : ''}`}
              onClick={() => setSelectedSeed(selectedSeed === type ? null : type)}
              disabled={coins < info.cost}
            >
              <span className="seed-emoji">{info.emoji}</span>
              <span className="seed-name">{type.replace('_', ' ')}</span>
              <span className="seed-cost">🪙 {info.cost}</span>
              <span className="seed-reward">→ {info.reward}</span>
              <span className="seed-time">{formatTime(info.grow_time)}</span>
            </button>
          ))}
        </div>
      </div>

      {/* Instructions */}
      <div className="garden-instructions">
        <p>🌱 Tap a seed, then tap an empty plot to plant</p>
        <p>💧 Tap growing plants to water (speeds up 10%)</p>
        <p>✨ Tap glowing plants to harvest coins!</p>
      </div>
    </div>
  );
};

export default GardenGame;
