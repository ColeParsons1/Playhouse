from django.urls import path, include
from rest_framework.routers import DefaultRouter
from .views import (
    PlayerViewSet, GameSessionViewSet, RewardViewSet, PlayerRewardViewSet,
    AvatarViewSet, AvatarItemViewSet, VoiceRoomViewSet, ShopItemViewSet,
    GardenViewSet, create_payment_intent, confirm_payment, youtube_search, youtube_suggestions
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
router.register(r'garden', GardenViewSet, basename='garden')

urlpatterns = [
    path('', include(router.urls)),
    path('payments/create-intent/', create_payment_intent, name='create_payment_intent'),
    path('payments/confirm/', confirm_payment, name='confirm_payment'),
    path('youtube/search/', youtube_search, name='youtube_search'),
    path('youtube/suggestions/', youtube_suggestions, name='youtube_suggestions'),
]
