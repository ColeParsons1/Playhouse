from rest_framework import viewsets, status
from rest_framework.decorators import action, api_view
from rest_framework.response import Response
from django.utils import timezone
from django.db import models
from django.db.models import Count, Q
from django.conf import settings
from datetime import timedelta
import random
import uuid
import hashlib
import stripe
import os

# Configure Stripe
stripe.api_key = os.environ.get('STRIPE_SECRET_KEY', '')

from .models import (
    Player, GameSession, BingoCard, Reward, PlayerReward, DailyBonus,
    Avatar, AvatarItem, PlayerAvatarItem, VoiceRoom, RoomParticipant,
    RoomGame, ChatMessage, ShopItem, PlayerShopItem, Follow
)
from .serializers import (
    PlayerSerializer, GameSessionSerializer, BingoCardSerializer,
    RewardSerializer, PlayerRewardSerializer, DailyBonusSerializer,
    AvatarSerializer, AvatarItemSerializer, PlayerAvatarItemSerializer,
    PlayerWithAvatarSerializer, VoiceRoomSerializer, VoiceRoomListSerializer,
    RoomParticipantSerializer, RoomGameSerializer, ChatMessageSerializer,
    ShopItemSerializer, PlayerShopItemSerializer, FollowSerializer, UserListSerializer
)


CPU_NAMES = [
    "Bunny", "Kitty", "Puppy", "Duckling", "Hamster",
    "Panda", "Koala", "Penguin", "Otter", "Fox",
    "Deer", "Owl", "Hedgehog", "Squirrel", "Rabbit",
    "Seal", "Bear Cub", "Lamb", "Piglet"
]

# AI room names for fallback
AI_ROOM_NAMES = [
    "bingoooo🎰🎰", "lottery bingooo🏠", "Mini AI Comp",
    "GLITCHY ♡", "Bingo🎀🎀⭐⭐", "Cerrrttyyy's 🏠",
    "Fun House 🎉", "Lucky Stars ⭐", "Game Night 🌙",
    "Party Room 🎊", "Winners Circle 🏆", "Chill Zone 😎"
]


class PlayerViewSet(viewsets.ModelViewSet):
    queryset = Player.objects.all()
    serializer_class = PlayerSerializer

    @action(detail=False, methods=['post'])
    def get_or_create_tanvi(self, request):
        """Get or create the main player (Tanvi)."""
        # Handle case where duplicates exist
        existing = Player.objects.filter(name="Tanvi", is_cpu=False)
        if existing.count() > 1:
            # Keep the first one, delete the rest
            player = existing.first()
            existing.exclude(id=player.id).delete()
        elif existing.exists():
            player = existing.first()
        else:
            player = Player.objects.create(
                name="Tanvi",
                username="tanvi",
                is_cpu=False,
                coins=500,
                gems=10,
                level=1,
                unlocked_themes=['default'],
                unlocked_daubers=['heart'],
                unlocked_card_backs=['default'],
            )

        # Create avatar if doesn't exist
        if not hasattr(player, 'avatar'):
            Avatar.objects.create(player=player)

        return Response(PlayerWithAvatarSerializer(player).data)

    @action(detail=False, methods=['post'])
    def get_or_create_test_user(self, request):
        """Get or create a test user for multiplayer testing."""
        test_name = request.data.get('name', 'TestPlayer')

        # Try to find existing test user
        existing = Player.objects.filter(name=test_name, is_cpu=False)
        if existing.exists():
            player = existing.first()
        else:
            player = Player.objects.create(
                name=test_name,
                username=test_name.lower().replace(' ', '_'),
                is_cpu=False,
                coins=500,
                gems=10,
                level=1,
                unlocked_themes=['default'],
                unlocked_daubers=['heart'],
                unlocked_card_backs=['default'],
            )

        # Create avatar if doesn't exist
        if not hasattr(player, 'avatar'):
            Avatar.objects.create(player=player)

        return Response(PlayerWithAvatarSerializer(player).data)

    @action(detail=False, methods=['get'])
    def list_all(self, request):
        """List all non-CPU players for testing."""
        players = Player.objects.filter(is_cpu=False)
        return Response(PlayerSerializer(players, many=True).data)

    @action(detail=False, methods=['post'])
    def register(self, request):
        """Register a new player account."""
        name = request.data.get('name', '').strip()
        email = request.data.get('email', '').strip().lower()
        password = request.data.get('password', '')

        # Validation
        if not name or len(name) < 2:
            return Response({'error': 'Name must be at least 2 characters'}, status=status.HTTP_400_BAD_REQUEST)
        if not email or '@' not in email:
            return Response({'error': 'Valid email is required'}, status=status.HTTP_400_BAD_REQUEST)
        if not password or len(password) < 4:
            return Response({'error': 'Password must be at least 4 characters'}, status=status.HTTP_400_BAD_REQUEST)

        # Check if email already exists
        if Player.objects.filter(email=email).exists():
            return Response({'error': 'Email already registered'}, status=status.HTTP_400_BAD_REQUEST)

        # Create username from name
        base_username = name.lower().replace(' ', '_')
        username = base_username
        counter = 1
        while Player.objects.filter(username=username).exists():
            username = f"{base_username}{counter}"
            counter += 1

        # Hash password (simple hash for demo - use proper bcrypt in production)
        password_hash = hashlib.sha256(password.encode()).hexdigest()

        # Create player
        player = Player.objects.create(
            name=name,
            username=username,
            email=email,
            password_hash=password_hash,
            is_cpu=False,
            coins=500,
            gems=10,
            level=1,
            unlocked_themes=['default'],
            unlocked_daubers=['heart'],
            unlocked_card_backs=['default'],
        )

        # Create avatar
        Avatar.objects.create(player=player)

        return Response({
            'success': True,
            'player': PlayerWithAvatarSerializer(player).data
        })

    @action(detail=False, methods=['post'])
    def login(self, request):
        """Login with email and password."""
        email = request.data.get('email', '').strip().lower()
        password = request.data.get('password', '')

        if not email or not password:
            return Response({'error': 'Email and password are required'}, status=status.HTTP_400_BAD_REQUEST)

        # Hash password to compare
        password_hash = hashlib.sha256(password.encode()).hexdigest()

        # Find player by email
        try:
            player = Player.objects.get(email=email)
        except Player.DoesNotExist:
            return Response({'error': 'Invalid email or password'}, status=status.HTTP_401_UNAUTHORIZED)

        # Check password
        if player.password_hash != password_hash:
            return Response({'error': 'Invalid email or password'}, status=status.HTTP_401_UNAUTHORIZED)

        # Create avatar if doesn't exist (for legacy accounts)
        if not hasattr(player, 'avatar'):
            Avatar.objects.create(player=player)

        return Response({
            'success': True,
            'player': PlayerWithAvatarSerializer(player).data
        })

    @action(detail=True, methods=['get'])
    def with_avatar(self, request, pk=None):
        """Get player with avatar data."""
        player = self.get_object()
        return Response(PlayerWithAvatarSerializer(player).data)

    @action(detail=True, methods=['post'])
    def claim_daily_bonus(self, request, pk=None):
        """Claim daily bonus with streak multiplier plus minigame coins."""
        player = self.get_object()

        # Get game coins from request (from present catch minigame)
        game_coins = request.data.get('game_coins', 0)

        # Check if already claimed today
        today = timezone.now().date()
        last_bonus = player.daily_bonuses.order_by('-claimed_at').first()

        if last_bonus and last_bonus.claimed_at.date() == today:
            # Already claimed daily bonus, but still add game coins if any
            if game_coins > 0:
                player.add_coins(game_coins)
            return Response(
                {'error': 'Already claimed today!', 'game_coins_added': game_coins},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Calculate day number (1-7 streak)
        if last_bonus and last_bonus.claimed_at.date() == today - timedelta(days=1):
            day_number = (last_bonus.day_number % 7) + 1
        else:
            day_number = 1

        # Rewards increase with streak
        base_coins = 50
        coins = base_coins * day_number
        gems = 1 if day_number >= 5 else 0
        gems += 2 if day_number == 7 else 0

        # Add daily bonus coins + game coins
        total_coins = coins + game_coins
        player.add_coins(total_coins)
        player.add_gems(gems)

        bonus = DailyBonus.objects.create(
            player=player,
            day_number=day_number,
            coins_earned=coins,
            gems_earned=gems
        )

        return Response({
            'day_number': day_number,
            'coins_earned': coins,
            'gems_earned': gems,
            'game_coins': game_coins,
            'player': PlayerSerializer(player).data
        })

    @action(detail=True, methods=['get'])
    def stats(self, request, pk=None):
        """Get player stats including streak info."""
        player = self.get_object()
        return Response({
            'name': player.name,
            'coins': player.coins,
            'gems': player.gems,
            'level': player.level,
            'xp': player.xp,
            'current_streak': player.current_streak,
            'best_streak': player.best_streak,
            'total_wins': player.total_wins,
            'total_games': player.total_games,
            'win_rate': round(player.total_wins / max(player.total_games, 1) * 100, 1),
            'active_theme': player.active_theme,
            'active_dauber': player.active_dauber,
            'active_card_back': player.active_card_back,
        })

    @action(detail=True, methods=['post'])
    def add_coins(self, request, pk=None):
        """Add coins to player (from minigames)."""
        player = self.get_object()
        coins = request.data.get('coins', 0)

        if coins > 0:
            player.add_coins(coins)

        return Response(PlayerSerializer(player).data)

    @action(detail=False, methods=['get'])
    def users(self, request):
        """Get list of all users for the follow screen."""
        player_id = request.query_params.get('player_id')
        search = request.query_params.get('search', '').strip()

        # Get all non-CPU players except the current user
        users = Player.objects.filter(is_cpu=False).exclude(id=player_id)

        # Apply search filter if provided
        if search:
            users = users.filter(
                Q(name__icontains=search) | Q(username__icontains=search)
            )

        # Order by level and total wins
        users = users.order_by('-level', '-total_wins')[:50]

        serializer = UserListSerializer(
            users, many=True,
            context={'request_player_id': int(player_id) if player_id else None}
        )
        return Response(serializer.data)

    @action(detail=True, methods=['post'])
    def follow(self, request, pk=None):
        """Follow a user."""
        target_player = self.get_object()
        follower_id = request.data.get('player_id')

        if not follower_id:
            return Response({'error': 'player_id is required'}, status=status.HTTP_400_BAD_REQUEST)

        if int(follower_id) == target_player.id:
            return Response({'error': 'Cannot follow yourself'}, status=status.HTTP_400_BAD_REQUEST)

        try:
            follower = Player.objects.get(id=follower_id)
        except Player.DoesNotExist:
            return Response({'error': 'Follower not found'}, status=status.HTTP_404_NOT_FOUND)

        # Create follow relationship if it doesn't exist
        follow, created = Follow.objects.get_or_create(
            follower=follower,
            following=target_player
        )

        if created:
            return Response({
                'success': True,
                'message': f'Now following {target_player.name}',
                'followers_count': target_player.followers_set.count()
            })
        else:
            return Response({
                'success': True,
                'message': 'Already following',
                'followers_count': target_player.followers_set.count()
            })

    @action(detail=True, methods=['post'])
    def unfollow(self, request, pk=None):
        """Unfollow a user."""
        target_player = self.get_object()
        follower_id = request.data.get('player_id')

        if not follower_id:
            return Response({'error': 'player_id is required'}, status=status.HTTP_400_BAD_REQUEST)

        deleted, _ = Follow.objects.filter(
            follower_id=follower_id,
            following=target_player
        ).delete()

        return Response({
            'success': True,
            'message': f'Unfollowed {target_player.name}' if deleted else 'Was not following',
            'followers_count': target_player.followers_set.count()
        })

    @action(detail=True, methods=['get'])
    def followers(self, request, pk=None):
        """Get list of followers for a user."""
        player = self.get_object()
        request_player_id = request.query_params.get('player_id')

        followers = Player.objects.filter(
            following__following=player
        ).distinct()

        serializer = UserListSerializer(
            followers, many=True,
            context={'request_player_id': int(request_player_id) if request_player_id else None}
        )
        return Response(serializer.data)

    @action(detail=True, methods=['get'])
    def following_list(self, request, pk=None):
        """Get list of users that a player is following."""
        player = self.get_object()
        request_player_id = request.query_params.get('player_id')

        following = Player.objects.filter(
            followers_set__follower=player
        ).distinct()

        serializer = UserListSerializer(
            following, many=True,
            context={'request_player_id': int(request_player_id) if request_player_id else None}
        )
        return Response(serializer.data)

    @action(detail=True, methods=['get'])
    def profile(self, request, pk=None):
        """Get detailed profile for a user including stats."""
        player = self.get_object()
        request_player_id = request.query_params.get('player_id')

        # Get follow counts
        followers_count = player.followers_set.count()
        following_count = player.following.count()

        # Check if requester is following this user
        is_following = False
        if request_player_id:
            is_following = Follow.objects.filter(
                follower_id=request_player_id,
                following=player
            ).exists()

        # Get avatar
        avatar_data = None
        if hasattr(player, 'avatar'):
            avatar_data = AvatarSerializer(player.avatar).data

        # Calculate win rate
        win_rate = round(player.total_wins / max(player.total_games, 1) * 100, 1)

        return Response({
            'id': player.id,
            'name': player.name,
            'username': player.username,
            'level': player.level,
            'xp': player.xp,
            'total_wins': player.total_wins,
            'total_games': player.total_games,
            'win_rate': win_rate,
            'current_streak': player.current_streak,
            'best_streak': player.best_streak,
            'country_code': player.country_code,
            'avatar': avatar_data,
            'followers_count': followers_count,
            'following_count': following_count,
            'is_following': is_following,
            'created_at': player.created_at,
        })


class AvatarViewSet(viewsets.ModelViewSet):
    queryset = Avatar.objects.all()
    serializer_class = AvatarSerializer

    @action(detail=False, methods=['get'])
    def for_player(self, request):
        """Get avatar for a specific player."""
        player_id = request.query_params.get('player_id')
        try:
            avatar = Avatar.objects.get(player_id=player_id)
            return Response(AvatarSerializer(avatar).data)
        except Avatar.DoesNotExist:
            return Response({'error': 'Avatar not found'}, status=status.HTTP_404_NOT_FOUND)

    @action(detail=True, methods=['post'])
    def update_avatar(self, request, pk=None):
        """Update avatar customization."""
        avatar = self.get_object()

        # Update fields from request
        updatable_fields = [
            'gender', 'skin_tone', 'hair_style', 'hair_color', 'face_shape',
            'eye_style', 'eye_color', 'eyebrow_style', 'nose_style', 'mouth_style',
            'top', 'bottom', 'shoes', 'hat', 'glasses', 'earrings', 'necklace',
            'wings', 'effect', 'frame', 'badge'
        ]

        for field in updatable_fields:
            if field in request.data:
                setattr(avatar, field, request.data[field])

        avatar.save()
        return Response(AvatarSerializer(avatar).data)


class AvatarItemViewSet(viewsets.ModelViewSet):
    queryset = AvatarItem.objects.all()
    serializer_class = AvatarItemSerializer

    @action(detail=False, methods=['get'])
    def by_category(self, request):
        """Get avatar items grouped by category."""
        player_id = request.query_params.get('player_id')
        category = request.query_params.get('category')

        items = AvatarItem.objects.all()
        if category:
            items = items.filter(category=category)

        # Get owned items for this player
        owned_ids = []
        if player_id:
            owned_ids = list(
                PlayerAvatarItem.objects.filter(player_id=player_id)
                .values_list('item_id', flat=True)
            )

        # Add owned status to items
        items_data = AvatarItemSerializer(items, many=True).data
        for item in items_data:
            item['owned'] = item['id'] in owned_ids or item['is_default']

        return Response(items_data)

    @action(detail=True, methods=['post'])
    def purchase(self, request, pk=None):
        """Purchase an avatar item."""
        item = self.get_object()
        player_id = request.data.get('player_id')

        try:
            player = Player.objects.get(id=player_id)
        except Player.DoesNotExist:
            return Response({'error': 'Player not found'}, status=status.HTTP_404_NOT_FOUND)

        # Check if already owned
        if PlayerAvatarItem.objects.filter(player=player, item=item).exists():
            return Response({'error': 'You already own this item!'}, status=status.HTTP_400_BAD_REQUEST)

        # Check currency
        if player.coins < item.coin_cost:
            return Response({'error': 'Not enough coins!'}, status=status.HTTP_400_BAD_REQUEST)
        if player.gems < item.gem_cost:
            return Response({'error': 'Not enough gems!'}, status=status.HTTP_400_BAD_REQUEST)

        # Deduct currency and create ownership
        player.coins -= item.coin_cost
        player.gems -= item.gem_cost
        player.save()

        PlayerAvatarItem.objects.create(player=player, item=item)

        return Response({
            'success': True,
            'message': f'Purchased {item.name}!',
            'player': PlayerSerializer(player).data
        })


class VoiceRoomViewSet(viewsets.ModelViewSet):
    queryset = VoiceRoom.objects.filter(is_active=True)
    serializer_class = VoiceRoomSerializer

    def get_serializer_class(self):
        if self.action == 'list':
            return VoiceRoomListSerializer
        return VoiceRoomSerializer

    @action(detail=False, methods=['get'])
    def lobby(self, request):
        """Get rooms for the lobby view with AI fallback."""
        room_type = request.query_params.get('type', 'popular')

        # Get real rooms
        rooms = VoiceRoom.objects.filter(is_active=True).annotate(
            participant_count=Count('participants', filter=Q(participants__is_active=True))
        )

        if room_type == 'follow':
            # For now, just return empty (would need follow system)
            rooms = rooms.none()
        elif room_type == 'music':
            rooms = rooms.filter(room_type='music')
        elif room_type == 'video':
            rooms = rooms.filter(room_type='video')
        else:  # popular
            rooms = rooms.order_by('-participant_count')

        # If no real rooms, create AI rooms as fallback
        if not rooms.exists():
            rooms = self._get_or_create_ai_rooms()

        return Response(VoiceRoomListSerializer(rooms[:20], many=True).data)

    def _get_or_create_ai_rooms(self):
        """Create AI-hosted rooms for development/fallback."""
        # Check if AI rooms already exist
        ai_rooms = VoiceRoom.objects.filter(host__is_cpu=True, is_active=True)
        if ai_rooms.count() >= 6:
            return ai_rooms

        # Get or create AI hosts
        ai_hosts = list(Player.objects.filter(is_cpu=True)[:6])
        while len(ai_hosts) < 6:
            host = Player.objects.create(
                name=CPU_NAMES[len(ai_hosts)],
                is_cpu=True,
                followers=random.randint(1000, 60000),
                level=random.randint(1, 50)
            )
            Avatar.objects.create(player=host)
            ai_hosts.append(host)

        # Create rooms for hosts without rooms
        game_modes = ['bingo_normal', 'bingo_powerup', 'bingo_blackout']
        for i, host in enumerate(ai_hosts):
            if not VoiceRoom.objects.filter(host=host, is_active=True).exists():
                VoiceRoom.objects.create(
                    name=AI_ROOM_NAMES[i % len(AI_ROOM_NAMES)],
                    host=host,
                    room_type='music',
                    game_mode=random.choice(game_modes),
                    entry_fee=200,
                    first_prize=random.choice([700, 800, 900, 1000]),
                )

        return VoiceRoom.objects.filter(host__is_cpu=True, is_active=True)

    @action(detail=False, methods=['post'])
    def create_room(self, request):
        """Create a new voice room."""
        player_id = request.data.get('player_id')
        name = request.data.get('name', 'My Room')
        game_mode = request.data.get('game_mode', 'bingo_normal')
        room_type = request.data.get('room_type', 'music')
        entry_fee = request.data.get('entry_fee', 200)
        first_prize = request.data.get('first_prize', 1000)

        try:
            player = Player.objects.get(id=player_id)
        except Player.DoesNotExist:
            return Response({'error': 'Player not found'}, status=status.HTTP_404_NOT_FOUND)

        room = VoiceRoom.objects.create(
            name=name,
            host=player,
            game_mode=game_mode,
            room_type=room_type,
            entry_fee=entry_fee,
            first_prize=first_prize,
            room_id=str(uuid.uuid4())[:8].upper()
        )

        # Host automatically joins
        RoomParticipant.objects.create(room=room, player=player, is_active=True)

        return Response(VoiceRoomSerializer(room).data)

    @action(detail=True, methods=['post'])
    def join(self, request, pk=None):
        """Join a voice room."""
        room = self.get_object()
        player_id = request.data.get('player_id')

        try:
            player = Player.objects.get(id=player_id)
        except Player.DoesNotExist:
            return Response({'error': 'Player not found'}, status=status.HTTP_404_NOT_FOUND)

        # Check if already in room
        participant, created = RoomParticipant.objects.get_or_create(
            room=room, player=player,
            defaults={'is_active': True}
        )

        if not created:
            participant.is_active = True
            participant.save()

        # Add AI players to simulate activity
        if room.host.is_cpu:
            self._add_ai_participants(room)

        return Response(VoiceRoomSerializer(room).data)

    def _add_ai_participants(self, room):
        """Add AI participants to a room."""
        current_count = room.participants.filter(is_active=True).count()
        target_count = random.randint(15, 35)

        if current_count >= target_count:
            return

        ai_players = Player.objects.filter(is_cpu=True).exclude(
            id__in=room.participants.values_list('player_id', flat=True)
        )[:target_count - current_count]

        for ai_player in ai_players:
            RoomParticipant.objects.get_or_create(
                room=room, player=ai_player,
                defaults={'is_active': True}
            )

    @action(detail=True, methods=['post'])
    def leave(self, request, pk=None):
        """Leave a voice room."""
        room = self.get_object()
        player_id = request.data.get('player_id')

        RoomParticipant.objects.filter(room=room, player_id=player_id).update(is_active=False)

        return Response({'success': True})

    @action(detail=True, methods=['post'])
    def buy_cards(self, request, pk=None):
        """Buy bingo cards in a room."""
        room = self.get_object()
        player_id = request.data.get('player_id')
        card_count = request.data.get('count', 1)

        try:
            player = Player.objects.get(id=player_id)
            participant = RoomParticipant.objects.get(room=room, player=player, is_active=True)
        except (Player.DoesNotExist, RoomParticipant.DoesNotExist):
            return Response({'error': 'Player not in room'}, status=status.HTTP_400_BAD_REQUEST)

        total_cost = room.card_cost * card_count

        if player.coins < total_cost:
            return Response({'error': 'Not enough coins!'}, status=status.HTTP_400_BAD_REQUEST)

        player.coins -= total_cost
        player.save()

        participant.cards_purchased += card_count
        participant.is_playing = True
        participant.save()

        return Response({
            'success': True,
            'cards_purchased': participant.cards_purchased,
            'player': PlayerSerializer(player).data
        })

    @action(detail=True, methods=['post'])
    def send_message(self, request, pk=None):
        """Send a chat message in the room."""
        room = self.get_object()
        player_id = request.data.get('player_id')
        message = request.data.get('message', '')

        try:
            player = Player.objects.get(id=player_id)
        except Player.DoesNotExist:
            return Response({'error': 'Player not found'}, status=status.HTTP_404_NOT_FOUND)

        chat_msg = ChatMessage.objects.create(
            room=room,
            player=player,
            message=message
        )

        return Response(ChatMessageSerializer(chat_msg).data)

    @action(detail=True, methods=['get'])
    def messages(self, request, pk=None):
        """Get recent chat messages."""
        room = self.get_object()
        messages = room.messages.order_by('-created_at')[:50]
        return Response(ChatMessageSerializer(messages, many=True).data)


class ShopItemViewSet(viewsets.ModelViewSet):
    queryset = ShopItem.objects.all()
    serializer_class = ShopItemSerializer

    @action(detail=False, methods=['get'])
    def by_category(self, request):
        """Get shop items by category."""
        category = request.query_params.get('category', 'rings')
        player_id = request.query_params.get('player_id')

        items = ShopItem.objects.filter(category=category)

        # Get owned items
        owned_ids = []
        if player_id:
            owned_ids = list(
                PlayerShopItem.objects.filter(player_id=player_id)
                .values_list('item_id', flat=True)
            )

        items_data = ShopItemSerializer(items, many=True).data
        for item in items_data:
            item['owned'] = item['id'] in owned_ids

        return Response(items_data)

    @action(detail=True, methods=['post'])
    def purchase(self, request, pk=None):
        """Purchase a shop item."""
        item = self.get_object()
        player_id = request.data.get('player_id')

        try:
            player = Player.objects.get(id=player_id)
        except Player.DoesNotExist:
            return Response({'error': 'Player not found'}, status=status.HTTP_404_NOT_FOUND)

        # Check if already owned
        if PlayerShopItem.objects.filter(player=player, item=item).exists():
            return Response({'error': 'You already own this item!'}, status=status.HTTP_400_BAD_REQUEST)

        # Check currency
        if player.coins < item.coin_cost:
            return Response({'error': 'Not enough coins!'}, status=status.HTTP_400_BAD_REQUEST)
        if player.gems < item.gem_cost:
            return Response({'error': 'Not enough gems!'}, status=status.HTTP_400_BAD_REQUEST)

        # Deduct currency
        player.coins -= item.coin_cost
        player.gems -= item.gem_cost
        player.save()

        PlayerShopItem.objects.create(player=player, item=item)

        return Response({
            'success': True,
            'message': f'Purchased {item.name}!',
            'player': PlayerSerializer(player).data
        })


# Keep existing viewsets for backwards compatibility

class GameSessionViewSet(viewsets.ModelViewSet):
    queryset = GameSession.objects.all()
    serializer_class = GameSessionSerializer

    @action(detail=False, methods=['post'])
    def new_game(self, request):
        """Start a new game with 20 players (1 human + 19 CPU)."""
        player_id = request.data.get('player_id')

        try:
            human_player = Player.objects.get(id=player_id, is_cpu=False)
        except Player.DoesNotExist:
            return Response(
                {'error': 'Player not found'},
                status=status.HTTP_404_NOT_FOUND
            )

        # Create game session
        game = GameSession.objects.create(status='in_progress')

        # Get or create CPU players
        cpu_players = list(Player.objects.filter(is_cpu=True)[:19])
        while len(cpu_players) < 19:
            name = CPU_NAMES[len(cpu_players)]
            cpu = Player.objects.create(name=name, is_cpu=True)
            cpu_players.append(cpu)

        # Create 3 cards for each player
        all_players = [human_player] + cpu_players

        for player in all_players:
            for card_idx in range(3):
                BingoCard.objects.create(
                    player=player,
                    game_session=game,
                    numbers=BingoCard.generate_card(),
                    marked=[],
                    card_index=card_idx
                )

        return Response(GameSessionSerializer(game).data)

    @action(detail=True, methods=['post'])
    def call_number(self, request, pk=None):
        """Call the next number and process CPU auto-marks."""
        game = self.get_object()

        if game.status != 'in_progress':
            return Response(
                {'error': 'Game is not in progress'},
                status=status.HTTP_400_BAD_REQUEST
            )

        number = game.call_number()
        if number is None:
            return Response(
                {'error': 'All numbers have been called'},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Auto-mark for CPU players
        cpu_cards = game.cards.filter(player__is_cpu=True)
        for card in cpu_cards:
            card.mark_number(number)

        # Check for CPU bingos
        for card in cpu_cards:
            if card.check_bingo():
                game.status = 'completed'
                game.winner = card.player
                game.completed_at = timezone.now()
                game.save()

                return Response({
                    'number': number,
                    'letter': game.get_letter_for_number(number),
                    'called_numbers': game.called_numbers,
                    'game_over': True,
                    'winner': card.player.name,
                    'winner_is_cpu': True,
                    'winning_card': BingoCardSerializer(card).data
                })

        return Response({
            'number': number,
            'letter': game.get_letter_for_number(number),
            'called_numbers': game.called_numbers,
            'game_over': False
        })

    @action(detail=True, methods=['post'])
    def mark_number(self, request, pk=None):
        """Mark a number on player's card."""
        game = self.get_object()
        card_id = request.data.get('card_id')
        number = request.data.get('number')

        if game.status != 'in_progress':
            return Response(
                {'error': 'Game is not in progress'},
                status=status.HTTP_400_BAD_REQUEST
            )

        if number not in game.called_numbers:
            return Response(
                {'error': 'This number has not been called yet'},
                status=status.HTTP_400_BAD_REQUEST
            )

        try:
            card = BingoCard.objects.get(id=card_id, game_session=game)
        except BingoCard.DoesNotExist:
            return Response(
                {'error': 'Card not found'},
                status=status.HTTP_404_NOT_FOUND
            )

        if card.player.is_cpu:
            return Response(
                {'error': 'Cannot mark CPU cards'},
                status=status.HTTP_400_BAD_REQUEST
            )

        marked = card.mark_number(number)

        return Response({
            'marked': marked,
            'card': BingoCardSerializer(card).data
        })

    @action(detail=True, methods=['post'])
    def claim_bingo(self, request, pk=None):
        """Claim a bingo win."""
        game = self.get_object()
        card_id = request.data.get('card_id')
        player_id = request.data.get('player_id')

        if game.status != 'in_progress':
            return Response(
                {'error': 'Game is not in progress'},
                status=status.HTTP_400_BAD_REQUEST
            )

        try:
            card = BingoCard.objects.get(id=card_id, game_session=game)
            player = Player.objects.get(id=player_id)
        except (BingoCard.DoesNotExist, Player.DoesNotExist):
            return Response(
                {'error': 'Card or player not found'},
                status=status.HTTP_404_NOT_FOUND
            )

        if card.player.id != player.id:
            return Response(
                {'error': 'This card does not belong to you'},
                status=status.HTTP_400_BAD_REQUEST
            )

        if not card.check_bingo():
            return Response({
                'valid': False,
                'message': "Not a valid bingo yet! Keep playing!"
            })

        # Valid bingo! Update game and player
        game.status = 'completed'
        game.winner = player
        game.completed_at = timezone.now()
        game.save()

        # Update player stats
        player.record_win()

        # Calculate rewards based on streak
        base_coins = 100
        streak_bonus = min(player.current_streak * 25, 200)
        total_coins = base_coins + streak_bonus

        gems_earned = 0
        if player.current_streak >= 3:
            gems_earned = 1
        if player.current_streak >= 5:
            gems_earned = 2
        if player.current_streak >= 10:
            gems_earned = 5

        player.add_coins(total_coins)
        player.add_gems(gems_earned)

        # Update other players' stats
        for other_card in game.cards.exclude(player=player).values('player').distinct():
            other_player = Player.objects.get(id=other_card['player'])
            if not other_player.is_cpu:
                other_player.record_loss()

        return Response({
            'valid': True,
            'message': "BINGO! You won!",
            'coins_earned': total_coins,
            'gems_earned': gems_earned,
            'streak': player.current_streak,
            'player': PlayerSerializer(player).data
        })

    @action(detail=True, methods=['get'])
    def player_cards(self, request, pk=None):
        """Get cards for a specific player in this game."""
        game = self.get_object()
        player_id = request.query_params.get('player_id')

        cards = game.cards.filter(player_id=player_id)
        return Response(BingoCardSerializer(cards, many=True).data)


class RewardViewSet(viewsets.ModelViewSet):
    queryset = Reward.objects.all()
    serializer_class = RewardSerializer

    @action(detail=True, methods=['post'])
    def purchase(self, request, pk=None):
        """Purchase a reward."""
        reward = self.get_object()
        player_id = request.data.get('player_id')

        try:
            player = Player.objects.get(id=player_id)
        except Player.DoesNotExist:
            return Response(
                {'error': 'Player not found'},
                status=status.HTTP_404_NOT_FOUND
            )

        # Check if already owned
        if PlayerReward.objects.filter(player=player, reward=reward).exists():
            return Response(
                {'error': 'You already own this reward!'},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Check streak requirement
        if reward.streak_requirement > player.best_streak:
            return Response(
                {'error': f'Requires {reward.streak_requirement} streak to unlock!'},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Check currency
        if player.coins < reward.coin_cost:
            return Response(
                {'error': 'Not enough coins!'},
                status=status.HTTP_400_BAD_REQUEST
            )

        if player.gems < reward.gem_cost:
            return Response(
                {'error': 'Not enough gems!'},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Deduct currency
        player.coins -= reward.coin_cost
        player.gems -= reward.gem_cost

        # Add to unlocked items
        if reward.reward_type == 'theme':
            if reward.reward_value not in player.unlocked_themes:
                player.unlocked_themes.append(reward.reward_value)
        elif reward.reward_type == 'dauber':
            if reward.reward_value not in player.unlocked_daubers:
                player.unlocked_daubers.append(reward.reward_value)
        elif reward.reward_type == 'card_back':
            if reward.reward_value not in player.unlocked_card_backs:
                player.unlocked_card_backs.append(reward.reward_value)

        player.save()

        # Create player reward record
        PlayerReward.objects.create(player=player, reward=reward)

        return Response({
            'success': True,
            'message': f'Unlocked {reward.name}!',
            'player': PlayerSerializer(player).data
        })

    @action(detail=False, methods=['get'])
    def available(self, request):
        """Get rewards available for purchase."""
        player_id = request.query_params.get('player_id')

        try:
            player = Player.objects.get(id=player_id)
        except Player.DoesNotExist:
            return Response(
                {'error': 'Player not found'},
                status=status.HTTP_404_NOT_FOUND
            )

        owned_ids = PlayerReward.objects.filter(player=player).values_list('reward_id', flat=True)
        available = Reward.objects.exclude(id__in=owned_ids)

        return Response({
            'available': RewardSerializer(available, many=True).data,
            'owned': RewardSerializer(Reward.objects.filter(id__in=owned_ids), many=True).data
        })


class PlayerRewardViewSet(viewsets.ModelViewSet):
    queryset = PlayerReward.objects.all()
    serializer_class = PlayerRewardSerializer

    @action(detail=False, methods=['post'])
    def equip(self, request):
        """Equip a reward (theme, dauber, or card back)."""
        player_id = request.data.get('player_id')
        reward_type = request.data.get('reward_type')
        reward_value = request.data.get('reward_value')

        try:
            player = Player.objects.get(id=player_id)
        except Player.DoesNotExist:
            return Response(
                {'error': 'Player not found'},
                status=status.HTTP_404_NOT_FOUND
            )

        if reward_type == 'theme':
            if reward_value in player.unlocked_themes:
                player.active_theme = reward_value
        elif reward_type == 'dauber':
            if reward_value in player.unlocked_daubers:
                player.active_dauber = reward_value
        elif reward_type == 'card_back':
            if reward_value in player.unlocked_card_backs:
                player.active_card_back = reward_value
        else:
            return Response(
                {'error': 'Invalid reward type'},
                status=status.HTTP_400_BAD_REQUEST
            )

        player.save()
        return Response(PlayerSerializer(player).data)


# Coin package definitions (must match frontend)
COIN_PACKAGES = {
    1: {'coins': 800, 'price': 99},      # $0.99
    2: {'coins': 2800, 'price': 299},    # $2.99
    3: {'coins': 5000, 'price': 499},    # $4.99
    4: {'coins': 12000, 'price': 999},   # $9.99
    5: {'coins': 75000, 'price': 4999},  # $49.99
}


@api_view(['POST'])
def create_payment_intent(request):
    """Create a Stripe Payment Intent for coin purchase."""
    package_id = request.data.get('package_id')
    player_id = request.data.get('player_id')

    if not package_id or package_id not in COIN_PACKAGES:
        return Response(
            {'error': 'Invalid package'},
            status=status.HTTP_400_BAD_REQUEST
        )

    if not player_id:
        return Response(
            {'error': 'Player ID required'},
            status=status.HTTP_400_BAD_REQUEST
        )

    package = COIN_PACKAGES[package_id]

    try:
        # Create a Payment Intent
        intent = stripe.PaymentIntent.create(
            amount=package['price'],  # Amount in cents
            currency='usd',
            automatic_payment_methods={
                'enabled': True,
            },
            metadata={
                'player_id': str(player_id),
                'package_id': str(package_id),
                'coins': str(package['coins']),
            }
        )

        return Response({
            'clientSecret': intent.client_secret,
            'amount': package['price'],
            'coins': package['coins'],
        })

    except stripe.error.StripeError as e:
        return Response(
            {'error': str(e)},
            status=status.HTTP_400_BAD_REQUEST
        )


@api_view(['POST'])
def confirm_payment(request):
    """Confirm payment was successful and add coins to player."""
    payment_intent_id = request.data.get('payment_intent_id')
    player_id = request.data.get('player_id')

    if not payment_intent_id or not player_id:
        return Response(
            {'error': 'Missing required fields'},
            status=status.HTTP_400_BAD_REQUEST
        )

    try:
        # Verify the payment intent
        intent = stripe.PaymentIntent.retrieve(payment_intent_id)

        if intent.status != 'succeeded':
            return Response(
                {'error': 'Payment not completed'},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Verify player ID matches
        if intent.metadata.get('player_id') != str(player_id):
            return Response(
                {'error': 'Player mismatch'},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Get coins from metadata
        coins = int(intent.metadata.get('coins', 0))

        if coins <= 0:
            return Response(
                {'error': 'Invalid coin amount'},
                status=status.HTTP_400_BAD_REQUEST
            )

        # Add coins to player
        try:
            player = Player.objects.get(id=player_id)
            player.add_coins(coins)

            return Response({
                'success': True,
                'coins_added': coins,
                'new_balance': player.coins,
                'player': PlayerSerializer(player).data
            })

        except Player.DoesNotExist:
            return Response(
                {'error': 'Player not found'},
                status=status.HTTP_404_NOT_FOUND
            )

    except stripe.error.StripeError as e:
        return Response(
            {'error': str(e)},
            status=status.HTTP_400_BAD_REQUEST
        )
