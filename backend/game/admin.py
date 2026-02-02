from django.contrib import admin
from .models import (
    Player, GameSession, BingoCard, Reward, PlayerReward, DailyBonus,
    Follow, Avatar, AvatarItem, PlayerAvatarItem,
    VoiceRoom, RoomParticipant, RoomGame, ChatMessage,
    ShopItem, PlayerShopItem
)


@admin.register(Player)
class PlayerAdmin(admin.ModelAdmin):
    list_display = ['id', 'name', 'username', 'is_cpu', 'coins', 'gems', 'level', 'total_wins', 'total_games', 'created_at']
    list_filter = ['is_cpu', 'level']
    search_fields = ['name', 'username', 'email']
    readonly_fields = ['created_at']
    fieldsets = (
        ('Basic Info', {
            'fields': ('name', 'username', 'email', 'password_hash', 'is_cpu', 'created_at')
        }),
        ('Currency', {
            'fields': ('coins', 'gems')
        }),
        ('Stats', {
            'fields': ('level', 'xp', 'current_streak', 'best_streak', 'total_wins', 'total_games', 'followers', 'country_code')
        }),
        ('Customization (Legacy)', {
            'fields': ('active_theme', 'active_dauber', 'active_card_back',
                      'unlocked_themes', 'unlocked_daubers', 'unlocked_card_backs'),
            'classes': ('collapse',)
        }),
    )


@admin.register(Follow)
class FollowAdmin(admin.ModelAdmin):
    list_display = ['follower', 'following', 'created_at']
    list_filter = ['created_at']
    search_fields = ['follower__name', 'following__name']


@admin.register(Avatar)
class AvatarAdmin(admin.ModelAdmin):
    list_display = ['player', 'gender', 'skin_tone', 'hair_style', 'updated_at']
    list_filter = ['gender', 'skin_tone']
    search_fields = ['player__name']


@admin.register(AvatarItem)
class AvatarItemAdmin(admin.ModelAdmin):
    list_display = ['item_id', 'name', 'category', 'rarity', 'coin_cost', 'gem_cost', 'is_default', 'is_limited']
    list_filter = ['category', 'rarity', 'is_default', 'is_limited']
    search_fields = ['item_id', 'name']


@admin.register(PlayerAvatarItem)
class PlayerAvatarItemAdmin(admin.ModelAdmin):
    list_display = ['player', 'item', 'purchased_at']
    list_filter = ['item__category', 'item__rarity']
    search_fields = ['player__name', 'item__name']


@admin.register(VoiceRoom)
class VoiceRoomAdmin(admin.ModelAdmin):
    list_display = ['room_id', 'name', 'host', 'room_type', 'game_mode', 'theme', 'is_active', 'player_count', 'created_at']
    list_filter = ['room_type', 'game_mode', 'theme', 'is_active', 'is_private']
    search_fields = ['room_id', 'name', 'host__name']
    readonly_fields = ['created_at', 'updated_at']


@admin.register(RoomParticipant)
class RoomParticipantAdmin(admin.ModelAdmin):
    list_display = ['room', 'player', 'is_active', 'is_muted', 'is_playing', 'cards_purchased', 'joined_at']
    list_filter = ['is_active', 'is_muted', 'is_playing']
    search_fields = ['room__name', 'player__name']


@admin.register(RoomGame)
class RoomGameAdmin(admin.ModelAdmin):
    list_display = ['id', 'room', 'status', 'first_place', 'second_place', 'third_place', 'created_at']
    list_filter = ['status']
    search_fields = ['room__name']


@admin.register(ChatMessage)
class ChatMessageAdmin(admin.ModelAdmin):
    list_display = ['room', 'player', 'message', 'is_system', 'created_at']
    list_filter = ['is_system', 'created_at']
    search_fields = ['room__name', 'player__name', 'message']


@admin.register(ShopItem)
class ShopItemAdmin(admin.ModelAdmin):
    list_display = ['item_id', 'name', 'category', 'coin_cost', 'gem_cost', 'is_limited']
    list_filter = ['category', 'is_limited', 'is_from_activity']
    search_fields = ['item_id', 'name']


@admin.register(PlayerShopItem)
class PlayerShopItemAdmin(admin.ModelAdmin):
    list_display = ['player', 'item', 'equipped', 'purchased_at']
    list_filter = ['equipped', 'item__category']
    search_fields = ['player__name', 'item__name']


@admin.register(GameSession)
class GameSessionAdmin(admin.ModelAdmin):
    list_display = ['id', 'status', 'winner', 'created_at', 'completed_at']
    list_filter = ['status']
    readonly_fields = ['created_at']


@admin.register(BingoCard)
class BingoCardAdmin(admin.ModelAdmin):
    list_display = ['id', 'player', 'game_session', 'card_index']
    list_filter = ['game_session']


@admin.register(Reward)
class RewardAdmin(admin.ModelAdmin):
    list_display = ['name', 'reward_type', 'coin_cost', 'gem_cost', 'streak_requirement', 'is_premium']
    list_filter = ['reward_type', 'is_premium']
    search_fields = ['name']


@admin.register(PlayerReward)
class PlayerRewardAdmin(admin.ModelAdmin):
    list_display = ['player', 'reward', 'unlocked_at']
    list_filter = ['reward__reward_type']


@admin.register(DailyBonus)
class DailyBonusAdmin(admin.ModelAdmin):
    list_display = ['player', 'day_number', 'coins_earned', 'gems_earned', 'claimed_at']
    list_filter = ['day_number']
