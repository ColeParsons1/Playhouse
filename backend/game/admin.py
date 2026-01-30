from django.contrib import admin
from .models import Player, GameSession, BingoCard, Reward, PlayerReward, DailyBonus


@admin.register(Player)
class PlayerAdmin(admin.ModelAdmin):
    list_display = ['name', 'is_cpu', 'coins', 'gems', 'current_streak', 'best_streak', 'total_wins', 'total_games']
    list_filter = ['is_cpu']
    search_fields = ['name']
    readonly_fields = ['created_at']
    fieldsets = (
        ('Basic Info', {
            'fields': ('name', 'is_cpu', 'created_at')
        }),
        ('Currency', {
            'fields': ('coins', 'gems')
        }),
        ('Stats', {
            'fields': ('current_streak', 'best_streak', 'total_wins', 'total_games')
        }),
        ('Customization', {
            'fields': ('active_theme', 'active_dauber', 'active_card_back',
                      'unlocked_themes', 'unlocked_daubers', 'unlocked_card_backs')
        }),
    )


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
