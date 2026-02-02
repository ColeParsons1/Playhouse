import axios from 'axios';

// Use environment variable or fallback - production uses relative URL for nginx proxy
const API_BASE = import.meta.env.VITE_API_URL || '/api';

// Simple in-memory cache for GET requests
const cache = new Map();
const CACHE_DURATION = 30000; // 30 seconds

const client = axios.create({
  baseURL: API_BASE,
  headers: {
    'Content-Type': 'application/json',
  },
  timeout: 10000, // 10 second timeout
});

// Request deduplication - prevent duplicate concurrent requests
const pendingRequests = new Map();

// Response interceptor for caching GET requests
client.interceptors.request.use((config) => {
  // Only cache GET requests
  if (config.method === 'get') {
    const cacheKey = config.url + JSON.stringify(config.params || {});
    const cached = cache.get(cacheKey);

    if (cached && Date.now() - cached.timestamp < CACHE_DURATION) {
      // Return cached response by throwing a special error that we'll catch
      const error = new Error('CACHE_HIT');
      error.cachedResponse = cached.data;
      throw error;
    }

    // Check for pending duplicate request
    if (pendingRequests.has(cacheKey)) {
      return pendingRequests.get(cacheKey);
    }

    config._cacheKey = cacheKey;
  }
  return config;
});

// Response interceptor to store in cache
client.interceptors.response.use(
  (response) => {
    if (response.config.method === 'get' && response.config._cacheKey) {
      cache.set(response.config._cacheKey, {
        data: response,
        timestamp: Date.now(),
      });
      pendingRequests.delete(response.config._cacheKey);
    }
    return response;
  },
  (error) => {
    // Handle cache hit
    if (error.message === 'CACHE_HIT') {
      return Promise.resolve(error.cachedResponse);
    }
    // Clean up pending request on error
    if (error.config?._cacheKey) {
      pendingRequests.delete(error.config._cacheKey);
    }
    return Promise.reject(error);
  }
);

// Clear cache utility (call after mutations)
export const clearCache = (urlPattern) => {
  if (urlPattern) {
    for (const key of cache.keys()) {
      if (key.includes(urlPattern)) {
        cache.delete(key);
      }
    }
  } else {
    cache.clear();
  }
};

export const authApi = {
  register: (name, email, password) =>
    client.post('/players/register/', { name, email, password }),
  login: (email, password) =>
    client.post('/players/login/', { email, password }),
};

export const playerApi = {
  getOrCreateTanvi: () => client.post('/players/get_or_create_tanvi/'),
  getWithAvatar: (playerId) => client.get(`/players/${playerId}/with_avatar/`),
  getStats: (playerId) => client.get(`/players/${playerId}/stats/`),
  claimDailyBonus: (playerId, gameCoins = 0) =>
    client.post(`/players/${playerId}/claim_daily_bonus/`, { game_coins: gameCoins }),
  addCoins: (playerId, coins) =>
    client.post(`/players/${playerId}/add_coins/`, { coins }),
  // User discovery and follow system
  getUsers: (playerId, search = '') =>
    client.get(`/players/users/?player_id=${playerId}&search=${encodeURIComponent(search)}`),
  getProfile: (userId, requesterId) =>
    client.get(`/players/${userId}/profile/?player_id=${requesterId}`),
  follow: (userId, playerId) =>
    client.post(`/players/${userId}/follow/`, { player_id: playerId }),
  unfollow: (userId, playerId) =>
    client.post(`/players/${userId}/unfollow/`, { player_id: playerId }),
  getFollowers: (userId, requesterId) =>
    client.get(`/players/${userId}/followers/?player_id=${requesterId}`),
  getFollowing: (userId, requesterId) =>
    client.get(`/players/${userId}/following_list/?player_id=${requesterId}`),
};

export const avatarApi = {
  getForPlayer: (playerId) => client.get(`/avatars/for_player/?player_id=${playerId}`),
  update: (avatarId, data) => client.post(`/avatars/${avatarId}/update_avatar/`, data),
};

export const avatarItemApi = {
  getByCategory: (playerId, category) =>
    client.get(`/avatar-items/by_category/?player_id=${playerId}&category=${category}`),
  purchase: (itemId, playerId) =>
    client.post(`/avatar-items/${itemId}/purchase/`, { player_id: playerId }),
};

export const roomApi = {
  getLobby: (type = 'popular') => client.get(`/rooms/lobby/?type=${type}`),
  getRoom: (roomId) => client.get(`/rooms/${roomId}/`),
  createRoom: (playerId, name, gameMode) =>
    client.post('/rooms/create_room/', { player_id: playerId, name, game_mode: gameMode }),
  join: (roomId, playerId) =>
    client.post(`/rooms/${roomId}/join/`, { player_id: playerId }),
  leave: (roomId, playerId) =>
    client.post(`/rooms/${roomId}/leave/`, { player_id: playerId }),
  buyCards: (roomId, playerId, count) =>
    client.post(`/rooms/${roomId}/buy_cards/`, { player_id: playerId, count }),
  sendMessage: (roomId, playerId, message) =>
    client.post(`/rooms/${roomId}/send_message/`, { player_id: playerId, message }),
  getMessages: (roomId) => client.get(`/rooms/${roomId}/messages/`),
};

export const shopApi = {
  getByCategory: (playerId, category) =>
    client.get(`/shop-items/by_category/?player_id=${playerId}&category=${category}`),
  purchase: (itemId, playerId) =>
    client.post(`/shop-items/${itemId}/purchase/`, { player_id: playerId }),
};

export const gameApi = {
  newGame: (playerId) => client.post('/games/new_game/', { player_id: playerId }),
  callNumber: (gameId) => client.post(`/games/${gameId}/call_number/`),
  markNumber: (gameId, cardId, number) =>
    client.post(`/games/${gameId}/mark_number/`, { card_id: cardId, number }),
  claimBingo: (gameId, cardId, playerId) =>
    client.post(`/games/${gameId}/claim_bingo/`, { card_id: cardId, player_id: playerId }),
  getPlayerCards: (gameId, playerId) =>
    client.get(`/games/${gameId}/player_cards/?player_id=${playerId}`),
  getGame: (gameId) => client.get(`/games/${gameId}/`),
};

export const rewardApi = {
  getAvailable: (playerId) => client.get(`/rewards/available/?player_id=${playerId}`),
  purchase: (rewardId, playerId) =>
    client.post(`/rewards/${rewardId}/purchase/`, { player_id: playerId }),
  equip: (playerId, rewardType, rewardValue) =>
    client.post('/player-rewards/equip/', {
      player_id: playerId,
      reward_type: rewardType,
      reward_value: rewardValue
    }),
};

export const gardenApi = {
  getGarden: (playerId) => client.get(`/garden/?player_id=${playerId}`),
  plant: (playerId, plotIndex, plantType) =>
    client.post('/garden/plant/', { player_id: playerId, plot_index: plotIndex, plant_type: plantType }),
  water: (playerId, plantId) =>
    client.post('/garden/water/', { player_id: playerId, plant_id: plantId }),
  harvest: (playerId, plantId) =>
    client.post('/garden/harvest/', { player_id: playerId, plant_id: plantId }),
};

export default client;
