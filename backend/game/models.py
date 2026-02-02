from django.db import models
import random
import uuid


class Player(models.Model):
    name = models.CharField(max_length=100)
    username = models.CharField(max_length=50, unique=True, null=True, blank=True)
    email = models.EmailField(unique=True, null=True, blank=True)
    password_hash = models.CharField(max_length=128, null=True, blank=True)  # Simple hash for demo
    is_cpu = models.BooleanField(default=False)
    coins = models.IntegerField(default=100)
    gems = models.IntegerField(default=5)
    current_streak = models.IntegerField(default=0)
    best_streak = models.IntegerField(default=0)
    total_wins = models.IntegerField(default=0)
    total_games = models.IntegerField(default=0)
    level = models.IntegerField(default=1)
    xp = models.IntegerField(default=0)
    followers = models.IntegerField(default=0)
    country_code = models.CharField(max_length=5, default='US')
    created_at = models.DateTimeField(auto_now_add=True)

    # Unlockables (legacy)
    unlocked_themes = models.JSONField(default=list)
    unlocked_daubers = models.JSONField(default=list)
    unlocked_card_backs = models.JSONField(default=list)
    active_theme = models.CharField(max_length=50, default='default')
    active_dauber = models.CharField(max_length=50, default='heart')
    active_card_back = models.CharField(max_length=50, default='default')

    def __str__(self):
        return f"{self.name} ({'CPU' if self.is_cpu else 'Human'})"

    def add_coins(self, amount):
        self.coins += amount
        self.save()

    def add_gems(self, amount):
        self.gems += amount
        self.save()

    def add_xp(self, amount):
        self.xp += amount
        # Level up every 1000 XP
        new_level = (self.xp // 1000) + 1
        if new_level > self.level:
            self.level = new_level
        self.save()

    def record_win(self):
        self.total_wins += 1
        self.total_games += 1
        self.current_streak += 1
        if self.current_streak > self.best_streak:
            self.best_streak = self.current_streak
        self.save()

    def record_loss(self):
        self.total_games += 1
        self.current_streak = 0
        self.save()


class Follow(models.Model):
    """Track follow relationships between players"""
    follower = models.ForeignKey(Player, on_delete=models.CASCADE, related_name='following')
    following = models.ForeignKey(Player, on_delete=models.CASCADE, related_name='followers_set')
    created_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        unique_together = ['follower', 'following']
        ordering = ['-created_at']

    def __str__(self):
        return f"{self.follower.name} follows {self.following.name}"


class Avatar(models.Model):
    """Player's customizable avatar"""
    player = models.OneToOneField(Player, on_delete=models.CASCADE, related_name='avatar')

    # Base appearance
    gender = models.CharField(max_length=20, default='neutral')
    skin_tone = models.CharField(max_length=20, default='light')

    # Face customization
    hair_style = models.CharField(max_length=50, default='default')
    hair_color = models.CharField(max_length=20, default='brown')
    face_shape = models.CharField(max_length=50, default='default')
    eye_style = models.CharField(max_length=50, default='default')
    eye_color = models.CharField(max_length=20, default='blue')
    eyebrow_style = models.CharField(max_length=50, default='default')
    nose_style = models.CharField(max_length=50, default='default')
    mouth_style = models.CharField(max_length=50, default='default')

    # Clothing
    top = models.CharField(max_length=50, default='white_tshirt')
    bottom = models.CharField(max_length=50, default='dark_shorts')
    shoes = models.CharField(max_length=50, default='sneakers')

    # Accessories
    hat = models.CharField(max_length=50, null=True, blank=True)
    glasses = models.CharField(max_length=50, null=True, blank=True)
    earrings = models.CharField(max_length=50, null=True, blank=True)
    necklace = models.CharField(max_length=50, null=True, blank=True)
    wings = models.CharField(max_length=50, null=True, blank=True)
    effect = models.CharField(max_length=50, null=True, blank=True)

    # Profile decorations
    frame = models.CharField(max_length=50, null=True, blank=True)
    badge = models.CharField(max_length=50, null=True, blank=True)

    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    def __str__(self):
        return f"Avatar for {self.player.name}"


class AvatarItem(models.Model):
    """Items that can be purchased/equipped on avatars"""
    CATEGORY_CHOICES = [
        ('hair', 'Hair'),
        ('face', 'Face Shape'),
        ('eyes', 'Eyes'),
        ('eyebrows', 'Eyebrows'),
        ('nose', 'Nose'),
        ('mouth', 'Mouth'),
        ('top', 'Top'),
        ('bottom', 'Bottom'),
        ('shoes', 'Shoes'),
        ('hat', 'Hat'),
        ('glasses', 'Glasses'),
        ('earrings', 'Earrings'),
        ('necklace', 'Necklace'),
        ('wings', 'Wings'),
        ('effect', 'Effect'),
        ('frame', 'Frame'),
        ('badge', 'Badge'),
    ]

    RARITY_CHOICES = [
        ('C', 'Common'),
        ('R', 'Rare'),
        ('S', 'Super Rare'),
        ('L', 'Legendary'),
    ]

    item_id = models.CharField(max_length=50, unique=True)
    name = models.CharField(max_length=100)
    category = models.CharField(max_length=20, choices=CATEGORY_CHOICES)
    rarity = models.CharField(max_length=1, choices=RARITY_CHOICES, default='C')
    coin_cost = models.IntegerField(default=0)
    gem_cost = models.IntegerField(default=0)
    is_default = models.BooleanField(default=False)  # Free items everyone owns
    is_limited = models.BooleanField(default=False)
    limited_days = models.IntegerField(null=True, blank=True)  # Days left for limited items
    preview_image = models.CharField(max_length=255, null=True, blank=True)

    def __str__(self):
        return f"{self.name} ({self.category})"


class PlayerAvatarItem(models.Model):
    """Items owned by a player"""
    player = models.ForeignKey(Player, on_delete=models.CASCADE, related_name='avatar_items')
    item = models.ForeignKey(AvatarItem, on_delete=models.CASCADE)
    purchased_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        unique_together = ['player', 'item']


class VoiceRoom(models.Model):
    """Voice chat room where games are played"""
    ROOM_TYPE_CHOICES = [
        ('music', 'Music'),
        ('friends', 'Friends'),
        ('video', 'Video'),
    ]

    GAME_MODE_CHOICES = [
        ('bingo_normal', 'Bingo Normal'),
        ('bingo_powerup', 'Bingo Power-up'),
        ('bingo_blackout', 'Bingo Blackout'),
        ('connect4', 'Connect 4'),
        ('ludo', 'Ludo'),
        ('carrom', 'Carrom'),
    ]

    THEME_CHOICES = [
        ('default', 'Default'),
        ('sunset', 'Sunset'),
        ('ocean', 'Ocean'),
        ('forest', 'Forest'),
        ('midnight', 'Midnight'),
        ('cherry', 'Cherry Blossom'),
        ('neon', 'Neon'),
        ('galaxy', 'Galaxy'),
        ('fire', 'Fire'),
        ('ice', 'Ice'),
        ('theater', 'Theater'),
        ('anime_theater', 'Anime Theater'),
        ('fantasy', 'Fantasy Castle'),
        ('underwater', 'Under the Sea'),
        ('hearts', 'Heart Clouds'),
    ]

    room_id = models.CharField(max_length=20, unique=True, default=uuid.uuid4)
    name = models.CharField(max_length=100)
    host = models.ForeignKey(Player, on_delete=models.CASCADE, related_name='hosted_rooms')
    room_type = models.CharField(max_length=20, choices=ROOM_TYPE_CHOICES, default='music')
    game_mode = models.CharField(max_length=30, choices=GAME_MODE_CHOICES, default='bingo_normal')
    theme = models.CharField(max_length=20, choices=THEME_CHOICES, default='default')

    # Room settings
    entry_fee = models.IntegerField(default=200)  # Coins to join a game
    card_cost = models.IntegerField(default=100)  # Cost per bingo card
    max_players = models.IntegerField(default=50)
    is_active = models.BooleanField(default=True)
    is_private = models.BooleanField(default=False)
    password = models.CharField(max_length=50, null=True, blank=True)

    # Prize pool
    first_prize = models.IntegerField(default=1000)
    second_prize = models.IntegerField(default=500)
    third_prize = models.IntegerField(default=250)

    created_at = models.DateTimeField(auto_now_add=True)
    updated_at = models.DateTimeField(auto_now=True)

    def __str__(self):
        return f"{self.name} (Host: {self.host.name})"

    @property
    def player_count(self):
        return self.participants.filter(is_active=True).count()


class RoomParticipant(models.Model):
    """Players currently in a room"""
    room = models.ForeignKey(VoiceRoom, on_delete=models.CASCADE, related_name='participants')
    player = models.ForeignKey(Player, on_delete=models.CASCADE, related_name='room_participations')
    is_active = models.BooleanField(default=True)
    is_muted = models.BooleanField(default=False)
    is_playing = models.BooleanField(default=False)  # Currently in a game round
    cards_purchased = models.IntegerField(default=0)
    joined_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        unique_together = ['room', 'player']


class RoomGame(models.Model):
    """A game round within a voice room"""
    STATUS_CHOICES = [
        ('waiting', 'Waiting for Players'),
        ('starting', 'Starting Soon'),
        ('in_progress', 'In Progress'),
        ('completed', 'Completed'),
    ]

    room = models.ForeignKey(VoiceRoom, on_delete=models.CASCADE, related_name='games')
    status = models.CharField(max_length=20, choices=STATUS_CHOICES, default='waiting')
    game_data = models.JSONField(default=dict)  # Game-specific state

    # Winners
    first_place = models.ForeignKey(Player, on_delete=models.SET_NULL, null=True, blank=True, related_name='first_place_wins')
    second_place = models.ForeignKey(Player, on_delete=models.SET_NULL, null=True, blank=True, related_name='second_place_wins')
    third_place = models.ForeignKey(Player, on_delete=models.SET_NULL, null=True, blank=True, related_name='third_place_wins')

    started_at = models.DateTimeField(null=True, blank=True)
    completed_at = models.DateTimeField(null=True, blank=True)
    created_at = models.DateTimeField(auto_now_add=True)


class ChatMessage(models.Model):
    """Chat messages in a voice room"""
    room = models.ForeignKey(VoiceRoom, on_delete=models.CASCADE, related_name='messages')
    player = models.ForeignKey(Player, on_delete=models.CASCADE)
    message = models.TextField()
    is_system = models.BooleanField(default=False)
    created_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        ordering = ['-created_at']


class ShopItem(models.Model):
    """Items in the main shop (rings, decorations, etc.)"""
    CATEGORY_CHOICES = [
        ('rings', 'Rings'),
        ('decorations', 'Decorations'),
        ('items', 'Items'),
        ('games', 'Games'),
        ('family', 'Family'),
    ]

    item_id = models.CharField(max_length=50, unique=True)
    name = models.CharField(max_length=100)
    category = models.CharField(max_length=20, choices=CATEGORY_CHOICES)
    description = models.TextField(null=True, blank=True)
    coin_cost = models.IntegerField(default=0)
    gem_cost = models.IntegerField(default=0)
    image = models.CharField(max_length=255, null=True, blank=True)
    is_from_activity = models.BooleanField(default=False)
    is_limited = models.BooleanField(default=False)

    def __str__(self):
        return f"{self.name} ({self.category})"


class PlayerShopItem(models.Model):
    """Shop items owned by a player"""
    player = models.ForeignKey(Player, on_delete=models.CASCADE, related_name='shop_items')
    item = models.ForeignKey(ShopItem, on_delete=models.CASCADE)
    equipped = models.BooleanField(default=False)
    purchased_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        unique_together = ['player', 'item']


# Keep existing models for backwards compatibility

class GameSession(models.Model):
    STATUS_CHOICES = [
        ('waiting', 'Waiting'),
        ('in_progress', 'In Progress'),
        ('completed', 'Completed'),
    ]

    status = models.CharField(max_length=20, choices=STATUS_CHOICES, default='waiting')
    winner = models.ForeignKey(Player, on_delete=models.SET_NULL, null=True, blank=True, related_name='won_games')
    called_numbers = models.JSONField(default=list)
    current_number = models.IntegerField(null=True, blank=True)
    created_at = models.DateTimeField(auto_now_add=True)
    completed_at = models.DateTimeField(null=True, blank=True)

    def __str__(self):
        return f"Game {self.id} - {self.status}"

    def call_number(self):
        available = [n for n in range(1, 76) if n not in self.called_numbers]
        if not available:
            return None
        number = random.choice(available)
        self.called_numbers.append(number)
        self.current_number = number
        self.save()
        return number

    def get_letter_for_number(self, number):
        if 1 <= number <= 15:
            return 'B'
        elif 16 <= number <= 30:
            return 'I'
        elif 31 <= number <= 45:
            return 'N'
        elif 46 <= number <= 60:
            return 'G'
        else:
            return 'O'


class BingoCard(models.Model):
    player = models.ForeignKey(Player, on_delete=models.CASCADE, related_name='cards')
    game_session = models.ForeignKey(GameSession, on_delete=models.CASCADE, related_name='cards')
    numbers = models.JSONField()  # 5x5 grid stored as list of lists
    marked = models.JSONField(default=list)  # List of marked positions as [row, col]
    card_index = models.IntegerField(default=0)  # 0, 1, or 2 for the 3 cards

    def __str__(self):
        return f"Card {self.card_index + 1} for {self.player.name}"

    @staticmethod
    def generate_card():
        """Generate a valid bingo card with proper number ranges per column."""
        card = []
        ranges = [
            (1, 15),   # B
            (16, 30),  # I
            (31, 45),  # N
            (46, 60),  # G
            (61, 75),  # O
        ]

        for col_idx, (start, end) in enumerate(ranges):
            column = random.sample(range(start, end + 1), 5)
            card.append(column)

        # Transpose to get rows instead of columns
        grid = [[card[col][row] for col in range(5)] for row in range(5)]

        # Set center as FREE space (represented as 0)
        grid[2][2] = 0

        return grid

    def mark_number(self, number):
        """Mark a number on the card if it exists."""
        for row_idx, row in enumerate(self.numbers):
            for col_idx, cell in enumerate(row):
                if cell == number:
                    position = [row_idx, col_idx]
                    if position not in self.marked:
                        self.marked.append(position)
                        self.save()
                    return True
        return False

    def check_bingo(self):
        """Check if this card has a bingo."""
        marked_set = {(pos[0], pos[1]) for pos in self.marked}
        # Center is always marked (FREE space)
        marked_set.add((2, 2))

        # Check rows
        for row in range(5):
            if all((row, col) in marked_set for col in range(5)):
                return True

        # Check columns
        for col in range(5):
            if all((row, col) in marked_set for row in range(5)):
                return True

        # Check diagonals
        if all((i, i) in marked_set for i in range(5)):
            return True
        if all((i, 4 - i) in marked_set for i in range(5)):
            return True

        return False


class Reward(models.Model):
    REWARD_TYPES = [
        ('theme', 'Theme'),
        ('dauber', 'Dauber'),
        ('card_back', 'Card Back'),
        ('coins', 'Coins'),
        ('gems', 'Gems'),
    ]

    name = models.CharField(max_length=100)
    description = models.TextField()
    reward_type = models.CharField(max_length=20, choices=REWARD_TYPES)
    reward_value = models.CharField(max_length=100)  # theme name, dauber name, or amount
    coin_cost = models.IntegerField(default=0)
    gem_cost = models.IntegerField(default=0)
    streak_requirement = models.IntegerField(default=0)  # Required streak to unlock
    icon = models.CharField(max_length=50, default='star')
    is_premium = models.BooleanField(default=False)

    def __str__(self):
        return f"{self.name} ({self.reward_type})"


class PlayerReward(models.Model):
    player = models.ForeignKey(Player, on_delete=models.CASCADE, related_name='rewards')
    reward = models.ForeignKey(Reward, on_delete=models.CASCADE)
    unlocked_at = models.DateTimeField(auto_now_add=True)

    class Meta:
        unique_together = ['player', 'reward']


class DailyBonus(models.Model):
    player = models.ForeignKey(Player, on_delete=models.CASCADE, related_name='daily_bonuses')
    day_number = models.IntegerField()  # Day 1-7 of the week
    coins_earned = models.IntegerField()
    gems_earned = models.IntegerField(default=0)
    claimed_at = models.DateTimeField(auto_now_add=True)
