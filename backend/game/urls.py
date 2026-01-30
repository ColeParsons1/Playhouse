from django.urls import path, include
from rest_framework.routers import DefaultRouter
from .views import (
    PlayerViewSet, GameSessionViewSet, RewardViewSet, PlayerRewardViewSet,
    AvatarViewSet, AvatarItemViewSet, VoiceRoomViewSet, ShopItemViewSet
)

router = DefaultRouter()
router.register(r'players', PlayerViewSet)
router.register(r'games', GameSessionViewSet)
router.register(r'rewards', RewardViewSet)
router.register(r'player-rewards', PlayerRewardViewSet)
router.register(r'avatars', AvatarViewSet)
router.register(r'avatar-items', AvatarItemViewSet)
router.register(r'rooms', VoiceRoomViewSet)
router.register(r'shop-items', ShopItemViewSet)

urlpatterns = [
    path('', include(router.urls)),
]
