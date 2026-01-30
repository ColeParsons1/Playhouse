from rest_framework import serializers
from .models import (
    Player, GameSession, BingoCard, Reward, PlayerReward, DailyBonus,
    Avatar, AvatarItem, PlayerAvatarItem, VoiceRoom, RoomParticipant,
    RoomGame, ChatMessage, ShopItem, PlayerShopItem
)


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
            'id', 'room_id', 'name', 'host', 'room_type', 'game_mode',
            'entry_fee', 'card_cost', 'max_players', 'is_active', 'is_private',
            'first_prize', 'second_prize', 'third_prize',
            'player_count', 'participants', 'created_at', 'updated_at'
        ]


class VoiceRoomListSerializer(serializers.ModelSerializer):
    """Lighter serializer for room listings"""
    host_name = serializers.CharField(source='host.name', read_only=True)
    host_avatar = AvatarSerializer(source='host.avatar', read_only=True)
    host_followers = serializers.IntegerField(source='host.followers', read_only=True)
    host_country = serializers.CharField(source='host.country_code', read_only=True)
    player_count = serializers.IntegerField(read_only=True)

    class Meta:
        model = VoiceRoom
        fields = [
            'id', 'room_id', 'name', 'host_name', 'host_avatar', 'host_followers',
            'host_country', 'room_type', 'game_mode', 'player_count', 'is_active'
        ]


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
