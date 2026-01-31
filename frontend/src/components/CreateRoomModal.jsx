import React, { useState } from 'react';
import { motion } from 'framer-motion';
import { roomApi } from '../api/client';
import './CreateRoomModal.css';

const ROOM_TYPES = [
  { id: 'music', label: 'Music', emoji: '🎵', description: 'Play with background music' },
  { id: 'friends', label: 'Friends', emoji: '👥', description: 'Private room for friends' },
  { id: 'video', label: 'Video', emoji: '📹', description: 'Video chat room' },
];

const GAME_MODES = [
  { id: 'bingo_normal', label: 'Bingo Normal', emoji: '🎱', description: 'Classic bingo rules' },
  { id: 'bingo_powerup', label: 'Bingo Power-up', emoji: '⚡', description: 'With special power-ups' },
  { id: 'bingo_blackout', label: 'Bingo Blackout', emoji: '🌑', description: 'Fill entire card to win' },
  { id: 'connect4', label: 'Connect 4', emoji: '🔴', description: '1v1 Connect 4 game' },
  { id: 'ludo', label: 'Ludo', emoji: '🎲', description: 'Classic board game' },
  { id: 'carrom', label: 'Carrom', emoji: '🎯', description: 'Carrom board game' },
];

const PRIZE_PRESETS = [
  { first: 1000, second: 500, third: 250, entryFee: 200, label: 'Standard' },
  { first: 2500, second: 1000, third: 500, entryFee: 500, label: 'High Stakes' },
  { first: 5000, second: 2000, third: 1000, entryFee: 1000, label: 'Jackpot' },
];

const CreateRoomModal = ({ player, onClose, onCreated }) => {
  const [step, setStep] = useState(1);
  const [roomName, setRoomName] = useState('');
  const [roomType, setRoomType] = useState('music');
  const [gameMode, setGameMode] = useState('bingo_normal');
  const [prizePreset, setPrizePreset] = useState(0);
  const [isPrivate, setIsPrivate] = useState(false);
  const [password, setPassword] = useState('');
  const [maxPlayers, setMaxPlayers] = useState(50);
  const [creating, setCreating] = useState(false);
  const [error, setError] = useState(null);

  const handleCreate = async () => {
    if (!player?.id) {
      setError('You must be logged in to create a room');
      return;
    }

    setCreating(true);
    setError(null);

    try {
      const name = roomName.trim() || `${player?.name || 'Host'}'s Room`;
      const response = await roomApi.createRoom(player.id, name, gameMode);

      // Add extra data to the room response
      const prize = PRIZE_PRESETS[prizePreset];
      const room = {
        ...response.data,
        room_type: roomType,
        entry_fee: prize.entryFee,
        first_prize: prize.first,
        second_prize: prize.second,
        third_prize: prize.third,
        max_players: maxPlayers,
        is_private: isPrivate,
        host: player,
        host_name: player.name,
        player_count: 1,
      };

      onCreated(room);
    } catch (err) {
      console.error('Failed to create room:', err);
      setError(err.response?.data?.error || 'Failed to create room. Please try again.');
      setCreating(false);
    }
  };

  const canCreate = roomName.trim() || step === 1;

  return (
    <motion.div
      className="create-room-overlay"
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
    >
      <motion.div
        className="create-room-modal"
        initial={{ y: 50, opacity: 0 }}
        animate={{ y: 0, opacity: 1 }}
        exit={{ y: 50, opacity: 0 }}
      >
        {/* Header */}
        <div className="create-modal-header">
          <button className="back-btn" onClick={step > 1 ? () => setStep(step - 1) : onClose}>
            {step > 1 ? '←' : '✕'}
          </button>
          <h2>Create Room</h2>
          <div className="step-indicator">
            <span className={step >= 1 ? 'active' : ''}>1</span>
            <span className={step >= 2 ? 'active' : ''}>2</span>
            <span className={step >= 3 ? 'active' : ''}>3</span>
          </div>
        </div>

        {/* Step 1: Room Type & Name */}
        {step === 1 && (
          <div className="create-step">
            <div className="step-section">
              <h3>Room Name</h3>
              <input
                type="text"
                placeholder={`${player?.name || 'Your'}'s Room`}
                value={roomName}
                onChange={(e) => setRoomName(e.target.value)}
                className="room-name-input"
              />
            </div>

            <div className="step-section">
              <h3>Room Type</h3>
              <div className="option-grid">
                {ROOM_TYPES.map(type => (
                  <button
                    key={type.id}
                    className={`option-card ${roomType === type.id ? 'selected' : ''}`}
                    onClick={() => setRoomType(type.id)}
                  >
                    <span className="option-emoji">{type.emoji}</span>
                    <span className="option-label">{type.label}</span>
                    <span className="option-desc">{type.description}</span>
                  </button>
                ))}
              </div>
            </div>

            <button className="next-btn" onClick={() => setStep(2)}>
              Next →
            </button>
          </div>
        )}

        {/* Step 2: Game Mode */}
        {step === 2 && (
          <div className="create-step">
            <div className="step-section">
              <h3>Game Mode</h3>
              <div className="game-mode-grid">
                {GAME_MODES.map(mode => (
                  <button
                    key={mode.id}
                    className={`game-mode-card ${gameMode === mode.id ? 'selected' : ''}`}
                    onClick={() => setGameMode(mode.id)}
                  >
                    <span className="mode-emoji">{mode.emoji}</span>
                    <div className="mode-info">
                      <span className="mode-label">{mode.label}</span>
                      <span className="mode-desc">{mode.description}</span>
                    </div>
                  </button>
                ))}
              </div>
            </div>

            <button className="next-btn" onClick={() => setStep(3)}>
              Next →
            </button>
          </div>
        )}

        {/* Step 3: Prizes & Settings */}
        {step === 3 && (
          <div className="create-step">
            <div className="step-section">
              <h3>Prize Pool</h3>
              <div className="prize-presets">
                {PRIZE_PRESETS.map((preset, idx) => (
                  <button
                    key={idx}
                    className={`prize-preset ${prizePreset === idx ? 'selected' : ''}`}
                    onClick={() => setPrizePreset(idx)}
                  >
                    <span className="preset-label">{preset.label}</span>
                    <div className="preset-prizes">
                      <span>🥇 {preset.first}</span>
                      <span>🥈 {preset.second}</span>
                      <span>🥉 {preset.third}</span>
                    </div>
                    <span className="preset-entry">Entry: 🪙 {preset.entryFee}</span>
                  </button>
                ))}
              </div>
            </div>

            <div className="step-section">
              <h3>Settings</h3>
              <div className="settings-list">
                <div className="setting-row">
                  <span>Max Players</span>
                  <select value={maxPlayers} onChange={(e) => setMaxPlayers(Number(e.target.value))}>
                    <option value={10}>10</option>
                    <option value={25}>25</option>
                    <option value={50}>50</option>
                    <option value={100}>100</option>
                  </select>
                </div>
                <div className="setting-row">
                  <span>Private Room</span>
                  <label className="toggle">
                    <input
                      type="checkbox"
                      checked={isPrivate}
                      onChange={(e) => setIsPrivate(e.target.checked)}
                    />
                    <span className="toggle-slider"></span>
                  </label>
                </div>
                {isPrivate && (
                  <div className="setting-row password">
                    <input
                      type="text"
                      placeholder="Room password"
                      value={password}
                      onChange={(e) => setPassword(e.target.value)}
                    />
                  </div>
                )}
              </div>
            </div>

            {error && (
              <div className="create-error">
                <span>⚠️</span> {error}
              </div>
            )}

            <button className="create-btn" onClick={handleCreate} disabled={creating}>
              {creating ? 'Creating...' : 'Create Room 🎉'}
            </button>
          </div>
        )}
      </motion.div>
    </motion.div>
  );
};

export default CreateRoomModal;
