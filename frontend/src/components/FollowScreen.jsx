import React, { useState, useEffect, useCallback } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { playerApi } from '../api/client';
import './FollowScreen.css';

// Mini Avatar component for user cards
const MiniAvatar = ({ avatar, size = 50 }) => {
  const skinColors = {
    light: '#FFE0BD',
    medium: '#E5B887',
    tan: '#C68642',
    brown: '#8D5524',
    dark: '#5C3317',
  };

  const hairColors = {
    black: '#1a1a1a',
    dark_brown: '#3d2314',
    brown: '#6a4420',
    blonde: '#d4a76a',
    red: '#8b2500',
  };

  const skinColor = skinColors[avatar?.skin_tone] || skinColors.light;
  const hairColor = hairColors[avatar?.hair_color] || hairColors.brown;

  return (
    <div
      className="mini-avatar"
      style={{
        width: size,
        height: size,
        background: `linear-gradient(180deg, ${hairColor} 0%, ${hairColor} 35%, ${skinColor} 35%, ${skinColor} 100%)`,
      }}
    >
      <div className="mini-avatar-face">
        <div className="mini-eyes">
          <span className="mini-eye" />
          <span className="mini-eye" />
        </div>
        <div className="mini-mouth" />
      </div>
    </div>
  );
};

// User Profile Modal
const UserProfileModal = ({ user, currentPlayerId, onClose, onFollowChange }) => {
  const [profile, setProfile] = useState(null);
  const [loading, setLoading] = useState(true);
  const [followLoading, setFollowLoading] = useState(false);

  useEffect(() => {
    loadProfile();
  }, [user.id]);

  const loadProfile = async () => {
    try {
      const response = await playerApi.getProfile(user.id, currentPlayerId);
      setProfile(response.data);
    } catch (error) {
      console.error('Failed to load profile:', error);
    } finally {
      setLoading(false);
    }
  };

  const handleFollow = async () => {
    if (!profile) return;
    setFollowLoading(true);

    try {
      if (profile.is_following) {
        await playerApi.unfollow(user.id, currentPlayerId);
        setProfile(prev => ({
          ...prev,
          is_following: false,
          followers_count: prev.followers_count - 1
        }));
      } else {
        await playerApi.follow(user.id, currentPlayerId);
        setProfile(prev => ({
          ...prev,
          is_following: true,
          followers_count: prev.followers_count + 1
        }));
      }
      onFollowChange?.();
    } catch (error) {
      console.error('Failed to follow/unfollow:', error);
    } finally {
      setFollowLoading(false);
    }
  };

  return (
    <motion.div
      className="profile-modal-overlay"
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
      onClick={onClose}
    >
      <motion.div
        className="profile-modal"
        initial={{ scale: 0.8, opacity: 0 }}
        animate={{ scale: 1, opacity: 1 }}
        exit={{ scale: 0.8, opacity: 0 }}
        onClick={(e) => e.stopPropagation()}
      >
        {loading ? (
          <div className="profile-loading">
            <div className="loading-spinner" />
          </div>
        ) : profile ? (
          <>
            <div className="profile-header">
              <MiniAvatar avatar={profile.avatar} size={80} />
              <h2 className="profile-name">{profile.name}</h2>
              {profile.username && (
                <span className="profile-username">@{profile.username}</span>
              )}
              <div className="profile-level">
                <span className="level-badge">Lv. {profile.level}</span>
                <span className="country-flag">{getFlagEmoji(profile.country_code)}</span>
              </div>
            </div>

            <div className="profile-stats-grid">
              <div className="stat-box">
                <span className="stat-value">{profile.followers_count}</span>
                <span className="stat-label">Followers</span>
              </div>
              <div className="stat-box">
                <span className="stat-value">{profile.following_count}</span>
                <span className="stat-label">Following</span>
              </div>
              <div className="stat-box">
                <span className="stat-value">{profile.total_wins}</span>
                <span className="stat-label">Wins</span>
              </div>
              <div className="stat-box">
                <span className="stat-value">{profile.win_rate}%</span>
                <span className="stat-label">Win Rate</span>
              </div>
            </div>

            <div className="profile-details">
              <div className="detail-row">
                <span className="detail-icon">🎮</span>
                <span className="detail-label">Games Played</span>
                <span className="detail-value">{profile.total_games}</span>
              </div>
              <div className="detail-row">
                <span className="detail-icon">🔥</span>
                <span className="detail-label">Current Streak</span>
                <span className="detail-value">{profile.current_streak}</span>
              </div>
              <div className="detail-row">
                <span className="detail-icon">🏆</span>
                <span className="detail-label">Best Streak</span>
                <span className="detail-value">{profile.best_streak}</span>
              </div>
              <div className="detail-row">
                <span className="detail-icon">⭐</span>
                <span className="detail-label">XP</span>
                <span className="detail-value">{profile.xp.toLocaleString()}</span>
              </div>
            </div>

            <div className="profile-actions">
              <motion.button
                className={`follow-btn-large ${profile.is_following ? 'following' : ''}`}
                onClick={handleFollow}
                disabled={followLoading}
                whileHover={{ scale: 1.02 }}
                whileTap={{ scale: 0.98 }}
              >
                {followLoading ? (
                  <span className="btn-spinner" />
                ) : profile.is_following ? (
                  'Following'
                ) : (
                  'Follow'
                )}
              </motion.button>
              <button className="close-btn" onClick={onClose}>
                Close
              </button>
            </div>
          </>
        ) : (
          <div className="profile-error">
            <span>Failed to load profile</span>
            <button onClick={onClose}>Close</button>
          </div>
        )}
      </motion.div>
    </motion.div>
  );
};

// Helper function to get flag emoji from country code
const getFlagEmoji = (countryCode) => {
  if (!countryCode) return '🌍';
  const codePoints = countryCode
    .toUpperCase()
    .split('')
    .map(char => 127397 + char.charCodeAt(0));
  return String.fromCodePoint(...codePoints);
};

// User Card Component
const UserCard = ({ user, currentPlayerId, onViewProfile, onFollowChange }) => {
  const [isFollowing, setIsFollowing] = useState(user.is_following);
  const [followLoading, setFollowLoading] = useState(false);

  const handleFollow = async (e) => {
    e.stopPropagation();
    setFollowLoading(true);

    try {
      if (isFollowing) {
        await playerApi.unfollow(user.id, currentPlayerId);
        setIsFollowing(false);
      } else {
        await playerApi.follow(user.id, currentPlayerId);
        setIsFollowing(true);
      }
      onFollowChange?.();
    } catch (error) {
      console.error('Failed to follow/unfollow:', error);
    } finally {
      setFollowLoading(false);
    }
  };

  return (
    <motion.div
      className="user-card"
      onClick={() => onViewProfile(user)}
      whileHover={{ scale: 1.02 }}
      whileTap={{ scale: 0.98 }}
      initial={{ opacity: 0, y: 20 }}
      animate={{ opacity: 1, y: 0 }}
    >
      <div className="user-avatar-section">
        <MiniAvatar avatar={user.avatar} size={50} />
        <span className="user-level">Lv.{user.level}</span>
      </div>

      <div className="user-info">
        <div className="user-name-row">
          <span className="user-name">{user.name}</span>
          <span className="user-flag">{getFlagEmoji(user.country_code)}</span>
        </div>
        <div className="user-stats-mini">
          <span>🏆 {user.total_wins} wins</span>
          <span>👥 {user.followers_count}</span>
        </div>
      </div>

      <motion.button
        className={`follow-btn ${isFollowing ? 'following' : ''}`}
        onClick={handleFollow}
        disabled={followLoading}
        whileHover={{ scale: 1.05 }}
        whileTap={{ scale: 0.95 }}
      >
        {followLoading ? '...' : isFollowing ? '✓' : '+'}
      </motion.button>
    </motion.div>
  );
};

const FollowScreen = ({ player, onClose }) => {
  const [activeTab, setActiveTab] = useState('discover'); // discover, followers, following
  const [users, setUsers] = useState([]);
  const [loading, setLoading] = useState(true);
  const [searchQuery, setSearchQuery] = useState('');
  const [selectedUser, setSelectedUser] = useState(null);

  const loadUsers = useCallback(async () => {
    if (!player?.id) return;
    setLoading(true);

    try {
      let response;
      if (activeTab === 'discover') {
        response = await playerApi.getUsers(player.id, searchQuery);
      } else if (activeTab === 'followers') {
        response = await playerApi.getFollowers(player.id, player.id);
      } else if (activeTab === 'following') {
        response = await playerApi.getFollowing(player.id, player.id);
      }
      setUsers(response.data);
    } catch (error) {
      console.error('Failed to load users:', error);
      setUsers([]);
    } finally {
      setLoading(false);
    }
  }, [player?.id, activeTab, searchQuery]);

  useEffect(() => {
    loadUsers();
  }, [loadUsers]);

  // Debounced search
  useEffect(() => {
    const timer = setTimeout(() => {
      if (activeTab === 'discover') {
        loadUsers();
      }
    }, 300);
    return () => clearTimeout(timer);
  }, [searchQuery]);

  const handleFollowChange = () => {
    // Reload the list after follow/unfollow
    loadUsers();
  };

  return (
    <motion.div
      className="follow-screen"
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      exit={{ opacity: 0 }}
    >
      {/* Header */}
      <div className="follow-header">
        <button className="back-btn" onClick={onClose}>
          ←
        </button>
        <h2 className="follow-title">Community</h2>
        <div className="header-spacer" />
      </div>

      {/* Tab Switcher */}
      <div className="follow-tabs">
        <button
          className={`follow-tab ${activeTab === 'discover' ? 'active' : ''}`}
          onClick={() => setActiveTab('discover')}
        >
          <span className="tab-icon">🔍</span>
          <span className="tab-label">Discover</span>
        </button>
        <button
          className={`follow-tab ${activeTab === 'followers' ? 'active' : ''}`}
          onClick={() => setActiveTab('followers')}
        >
          <span className="tab-icon">👥</span>
          <span className="tab-label">Followers</span>
        </button>
        <button
          className={`follow-tab ${activeTab === 'following' ? 'active' : ''}`}
          onClick={() => setActiveTab('following')}
        >
          <span className="tab-icon">💝</span>
          <span className="tab-label">Following</span>
        </button>
      </div>

      {/* Search Bar (only for discover) */}
      {activeTab === 'discover' && (
        <div className="search-container">
          <span className="search-icon">🔍</span>
          <input
            type="text"
            className="search-input"
            placeholder="Search users..."
            value={searchQuery}
            onChange={(e) => setSearchQuery(e.target.value)}
          />
          {searchQuery && (
            <button className="clear-search" onClick={() => setSearchQuery('')}>
              ×
            </button>
          )}
        </div>
      )}

      {/* User List */}
      <div className="users-list">
        {loading ? (
          <div className="loading-container">
            <div className="loading-spinner" />
            <p>Loading users...</p>
          </div>
        ) : users.length === 0 ? (
          <div className="empty-state">
            <span className="empty-icon">
              {activeTab === 'discover' ? '🔍' : activeTab === 'followers' ? '👥' : '💝'}
            </span>
            <h3>
              {activeTab === 'discover'
                ? 'No users found'
                : activeTab === 'followers'
                ? 'No followers yet'
                : 'Not following anyone'}
            </h3>
            <p>
              {activeTab === 'discover'
                ? 'Try a different search'
                : activeTab === 'followers'
                ? 'Share your profile to get followers!'
                : 'Discover users to follow'}
            </p>
          </div>
        ) : (
          <div className="users-grid">
            {users.map((user) => (
              <UserCard
                key={user.id}
                user={user}
                currentPlayerId={player?.id}
                onViewProfile={setSelectedUser}
                onFollowChange={handleFollowChange}
              />
            ))}
          </div>
        )}
      </div>

      {/* Profile Modal */}
      <AnimatePresence>
        {selectedUser && (
          <UserProfileModal
            user={selectedUser}
            currentPlayerId={player?.id}
            onClose={() => setSelectedUser(null)}
            onFollowChange={handleFollowChange}
          />
        )}
      </AnimatePresence>
    </motion.div>
  );
};

export default FollowScreen;
