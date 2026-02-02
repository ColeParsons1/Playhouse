from rest_framework import serializers
from .models import (
    Player, GameSession, BingoCard, Reward, PlayerReward, DailyBonus,
    Avatar, AvatarItem, PlayerAvatarItem, VoiceRoom, RoomParticipant,
    RoomGame, ChatMessage, ShopItem, PlayerShopItem, Follow, Plant
)
from .models_connect4 import Connect4Game, Connect4Stats, Connect4Leaderboard


class PlayerSerializer(serializers.ModelSerializer):
    class Meta:
        model = Player
        fields = '__all__'


class AvatarSerializer(serializers.ModelSerializer):
    class Meta:
        model = Avatar
        fields = '__all__'


class AvatarItemSerializer(serializers.ModelSerializer):
    class Meta:
        model = AvatarItem
        fields = '__all__'


class PlayerAvatarItemSerializer(serializers.ModelSerializer):
    item = AvatarItemSerializer(read_only=True)

    class Meta:
        model = PlayerAvatarItem
        fields = ['id', 'item', 'purchased_at']


class PlayerWithAvatarSerializer(serializers.ModelSerializer):
    avatar = AvatarSerializer(read_only=True)
    avatar_items = PlayerAvatarItemSerializer(many=True, read_only=True)

    class Meta:
        model = Player
        fields = '__all__'


class RoomParticipantSerializer(serializers.ModelSerializer):
    player = PlayerSerializer(read_only=True)

    class Meta:
        model = RoomParticipant
        fields = ['id', 'player', 'is_active', 'is_muted', 'is_playing', 'cards_purchased', 'joined_at']


class VoiceRoomSerializer(serializers.ModelSerializer):
    host = PlayerSerializer(read_only=True)
    player_count = serializers.IntegerField(read_only=True)
    participants = RoomParticipantSerializer(many=True, read_only=True)

    class Meta:
        model = VoiceRoom
        fields = [
            'id', 'room_id', 'name', 'host', 'room_type', 'game_mode', 'theme',
            'entry_fee', 'card_cost', 'max_players', 'is_active', 'is_private',
            'first_prize', 'second_prize', 'third_prize',
            'player_count', 'participants', 'created_at', 'updated_at'
        ]


class VoiceRoomListSerializer(serializers.ModelSerializer):
    """Lighter serializer for room listings"""
    host_id = serializers.IntegerField(source='host.id', read_only=True)
    host_name = serializers.CharField(source='host.name', read_only=True)
    host_avatar = AvatarSerializer(source='host.avatar', read_only=True)
    host_followers = serializers.IntegerField(source='host.followers', read_only=True)
    host_country = serializers.CharField(source='host.country_code', read_only=True)
    player_count = serializers.SerializerMethodField()

    class Meta:
        model = VoiceRoom
        fields = [
            'id', 'room_id', 'name', 'host_id', 'host_name', 'host_avatar', 'host_followers',
            'host_country', 'room_type', 'game_mode', 'theme', 'player_count', 'is_active',
            'first_prize', 'entry_fee'
        ]

    def get_player_count(self, obj):
        # Use annotated count if available, otherwise query
        if hasattr(obj, 'participant_count'):
            return obj.participant_count
        return obj.participants.filter(is_active=True).count()


class RoomGameSerializer(serializers.ModelSerializer):
    first_place = PlayerSerializer(read_only=True)
    second_place = PlayerSerializer(read_only=True)
    third_place = PlayerSerializer(read_only=True)

    class Meta:
        model = RoomGame
        fields = '__all__'


class ChatMessageSerializer(serializers.ModelSerializer):
    player_name = serializers.CharField(source='player.name', read_only=True)

    class Meta:
        model = ChatMessage
        fields = ['id', 'player', 'player_name', 'message', 'is_system', 'created_at']


class ShopItemSerializer(serializers.ModelSerializer):
    class Meta:
        model = ShopItem
        fields = '__all__'


class PlayerShopItemSerializer(serializers.ModelSerializer):
    item = ShopItemSerializer(read_only=True)

    class Meta:
        model = PlayerShopItem
        fields = ['id', 'item', 'equipped', 'purchased_at']


class BingoCardSerializer(serializers.ModelSerializer):
    player_name = serializers.CharField(source='player.name', read_only=True)

    class Meta:
        model = BingoCard
        fields = ['id', 'player', 'player_name', 'numbers', 'marked', 'card_index']


class GameSessionSerializer(serializers.ModelSerializer):
    cards = BingoCardSerializer(many=True, read_only=True)
    winner_name = serializers.CharField(source='winner.name', read_only=True)

    class Meta:
        model = GameSession
        fields = ['id', 'status', 'winner', 'winner_name', 'called_numbers',
                  'current_number', 'created_at', 'completed_at', 'cards']


class RewardSerializer(serializers.ModelSerializer):
    class Meta:
        model = Reward
        fields = '__all__'


class PlayerRewardSerializer(serializers.ModelSerializer):
    reward = RewardSerializer(read_only=True)

    class Meta:
        model = PlayerReward
        fields = ['id', 'reward', 'unlocked_at']


class DailyBonusSerializer(serializers.ModelSerializer):
    class Meta:
        model = DailyBonus
        fields = '__all__'


class FollowSerializer(serializers.ModelSerializer):
    follower = PlayerSerializer(read_only=True)
    following = PlayerSerializer(read_only=True)

    class Meta:
        model = Follow
        fields = ['id', 'follower', 'following', 'created_at']


class UserListSerializer(serializers.ModelSerializer):
    """Serializer for user listings with avatar and follow status"""
    avatar = AvatarSerializer(read_only=True)
    followers_count = serializers.SerializerMethodField()
    following_count = serializers.SerializerMethodField()
    is_following = serializers.SerializerMethodField()

    class Meta:
        model = Player
        fields = [
            'id', 'name', 'username', 'level', 'xp', 'total_wins', 'total_games',
            'current_streak', 'best_streak', 'country_code', 'avatar',
            'followers_count', 'following_count', 'is_following', 'created_at'
        ]

    def get_followers_count(self, obj):
        return obj.followers_set.count()

    def get_following_count(self, obj):
        return obj.following.count()

    def get_is_following(self, obj):
        request_player_id = self.context.get('request_player_id')
        if not request_player_id:
            return False
        return Follow.objects.filter(follower_id=request_player_id, following=obj).exists()


class PlantSerializer(serializers.ModelSerializer):
    """Serializer for garden plants with computed growth data"""
    growth_percent = serializers.SerializerMethodField()
    is_ready = serializers.SerializerMethodField()
    reward = serializers.SerializerMethodField()
    emoji = serializers.SerializerMethodField()

    class Meta:
        model = Plant
        fields = [
            'id', 'player', 'plot_index', 'plant_type', 'planted_at',
            'last_watered', 'water_count', 'growth_percent', 'is_ready',
            'reward', 'emoji'
        ]

    def get_growth_percent(self, obj):
        return round(obj.get_growth_percent(), 1)

    def get_is_ready(self, obj):
        return obj.is_ready_to_harvest()

    def get_reward(self, obj):
        return obj.get_reward()

    def get_emoji(self, obj):
        data = Plant.PLANT_DATA.get(obj.plant_type, {})
        return data.get('emoji', '🌱')


# Connect 4 Serializers

class Connect4GameSerializer(serializers.ModelSerializer):
    """Serializer for Connect 4 game records"""
    player_name = serializers.CharField(source='player.name', read_only=True)
    
    class Meta:
        model = Connect4Game
        fields = ['id', 'player', 'player_name', 'result', 'score', 'moves_made', 
                  'created_at', 'duration_seconds']


class Connect4StatsSerializer(serializers.ModelSerializer):
    """Serializer for Connect 4 player statistics"""
    player_name = serializers.CharField(source='player.name', read_only=True)
    win_rate = serializers.SerializerMethodField()
    average_score = serializers.SerializerMethodField()
    
    class Meta:
        model = Connect4Stats
        fields = ['player', 'player_name', 'games_played', 'wins', 'losses', 'draws',
                  'total_score', 'high_score', 'current_streak', 'best_streak',
                  'total_moves', 'fastest_win_moves', 'win_rate', 'average_score',
                  'first_played', 'last_played']
    
    def get_win_rate(self, obj):
        return round(obj.win_rate, 1)
    
    def get_average_score(self, obj):
        return round(obj.average_score, 1)


class Connect4LeaderboardSerializer(serializers.ModelSerializer):
    """Serializer for Connect 4 leaderboard entries"""
    player_name = serializers.CharField(source='player.name', read_only=True)
    
    class Meta:
        model = Connect4Leaderboard
        fields = ['player', 'player_name', 'period', 'date', 'games_played', 
                  'total_score', 'wins']


class Connect4GameResultSerializer(serializers.Serializer):
    """Serializer for submitting a Connect 4 game result"""
    result = serializers.ChoiceField(choices=['win', 'loss', 'draw'])
    score = serializers.IntegerField(min_value=0)
    moves_made = serializers.IntegerField(min_value=0)
    board_state = serializers.ListField(child=serializers.ListField(child=serializers.CharField(allow_null=True)), required=False)
    winning_cells = serializers.ListField(child=serializers.ListField(child=serializers.IntegerField()), required=False)
    duration_seconds = serializers.IntegerField(min_value=0, default=0)
