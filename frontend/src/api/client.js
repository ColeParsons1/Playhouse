import axios from 'axios';

const API_BASE = 'http://172.20.4.93:8001/api';

const client = axios.create({
  baseURL: API_BASE,
  headers: {
    'Content-Type': 'application/json',
  },
});

export const playerApi = {
  getOrCreateTanvi: () => client.post('/players/get_or_create_tanvi/'),
  getWithAvatar: (playerId) => client.get(`/players/${playerId}/with_avatar/`),
  getStats: (playerId) => client.get(`/players/${playerId}/stats/`),
  claimDailyBonus: (playerId, gameCoins = 0) =>
    client.post(`/players/${playerId}/claim_daily_bonus/`, { game_coins: gameCoins }),
  addCoins: (playerId, coins) =>
    client.post(`/players/${playerId}/add_coins/`, { coins }),
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

export default client;
