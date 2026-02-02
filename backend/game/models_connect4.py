"""
Connect 4 Game Models for Playhouse
"""
from django.db import models
from django.utils import timezone


class Connect4Game(models.Model):
    """Record of a Connect 4 game played"""
    
    RESULT_CHOICES = [
        ('win', 'Player Win'),
        ('loss', 'AI Win'),
        ('draw', 'Draw'),
    ]
    
    player = models.ForeignKey('Player', on_delete=models.CASCADE, related_name='connect4_games')
    result = models.CharField(max_length=10, choices=RESULT_CHOICES)
    score = models.IntegerField(default=0)
    moves_made = models.IntegerField(default=0)
    
    # Game details
    board_state = models.JSONField(default=list)  # Final board state
    winning_cells = models.JSONField(default=list, null=True, blank=True)  # Winning 4 cells
    
    # Timestamps
    created_at = models.DateTimeField(auto_now_add=True)
    duration_seconds = models.IntegerField(default=0)
    
    class Meta:
        ordering = ['-created_at']
        verbose_name = 'Connect 4 Game'
        verbose_name_plural = 'Connect 4 Games'
    
    def __str__(self):
        return f"{self.player.name} - {self.result} - {self.score} points"


class Connect4Stats(models.Model):
    """Player statistics for Connect 4"""
    
    player = models.OneToOneField('Player', on_delete=models.CASCADE, related_name='connect4_stats')
    
    # Win/Loss tracking
    games_played = models.IntegerField(default=0)
    wins = models.IntegerField(default=0)
    losses = models.IntegerField(default=0)
    draws = models.IntegerField(default=0)
    
    # Score tracking
    total_score = models.IntegerField(default=0)
    high_score = models.IntegerField(default=0)
    
    # Streak tracking
    current_streak = models.IntegerField(default=0)
    best_streak = models.IntegerField(default=0)
    
    # Move statistics
    total_moves = models.IntegerField(default=0)
    fastest_win_moves = models.IntegerField(null=True, blank=True)  # Fewest moves to win
    
    # Timestamps
    first_played = models.DateTimeField(auto_now_add=True)
    last_played = models.DateTimeField(auto_now=True)
    
    class Meta:
        verbose_name = 'Connect 4 Stats'
        verbose_name_plural = 'Connect 4 Stats'
    
    def __str__(self):
        return f"{self.player.name} - W:{self.wins} L:{self.losses} D:{self.draws}"
    
    @property
    def win_rate(self):
        if self.games_played == 0:
            return 0
        return (self.wins / self.games_played) * 100
    
    @property
    def average_score(self):
        if self.games_played == 0:
            return 0
        return self.total_score / self.games_played
    
    def record_game(self, result, score, moves):
        """Update stats after a game"""
        self.games_played += 1
        self.total_score += score
        self.total_moves += moves
        
        if score > self.high_score:
            self.high_score = score
        
        if result == 'win':
            self.wins += 1
            self.current_streak += 1
            if self.current_streak > self.best_streak:
                self.best_streak = self.current_streak
            # Track fastest win
            if self.fastest_win_moves is None or moves < self.fastest_win_moves:
                self.fastest_win_moves = moves
        elif result == 'loss':
            self.losses += 1
            self.current_streak = 0
        else:  # draw
            self.draws += 1
            self.current_streak = 0
        
        self.last_played = timezone.now()
        self.save()


class Connect4Leaderboard(models.Model):
    """Daily/Weekly leaderboard for Connect 4"""
    
    PERIOD_CHOICES = [
        ('daily', 'Daily'),
        ('weekly', 'Weekly'),
        ('all_time', 'All Time'),
    ]
    
    player = models.ForeignKey('Player', on_delete=models.CASCADE, related_name='connect4_leaderboard_entries')
    period = models.CharField(max_length=10, choices=PERIOD_CHOICES)
    date = models.DateField()
    
    # Stats for this period
    games_played = models.IntegerField(default=0)
    total_score = models.IntegerField(default=0)
    wins = models.IntegerField(default=0)
    
    class Meta:
        unique_together = ['player', 'period', 'date']
        ordering = ['-total_score']
        verbose_name = 'Connect 4 Leaderboard'
        verbose_name_plural = 'Connect 4 Leaderboards'
    
    def __str__(self):
        return f"{self.player.name} - {self.period} - {self.total_score}"
