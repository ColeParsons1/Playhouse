# Tanvi's Bingo Game

A special bingo game made with love, featuring cute baby animals, streaks, rewards, and lots of dopamine-releasing fun!

## Features

- **20 Player Bingo**: Play against 19 adorable CPU opponents (Bunny, Kitty, Puppy, Panda, and more!)
- **3 Cards Per Player**: More chances to win!
- **Streak System**: Build your win streak for bonus rewards
- **Currency System**:
  - Coins: Earned from winning games
  - Gems: Premium currency for special rewards
- **Daily Bonuses**: Claim daily rewards with increasing bonuses for consecutive days
- **Shop**: Purchase cute daubers, themes, and card backs
- **Baby Animal Background**: Floating cute animals to brighten your day

## Getting Started

### Prerequisites

- Python 3.8+
- Node.js 18+
- npm

### Quick Start

Run the start script:

```bash
./start.sh
```

This will start both the backend and frontend servers.

### Manual Start

**Backend:**
```bash
cd backend
source venv/bin/activate
python manage.py runserver
```

**Frontend:**
```bash
cd frontend
npm run dev
```

### Access the Game

- Frontend: http://localhost:5173
- Backend API: http://localhost:8000

## How to Play

1. The game starts automatically with you and 19 CPU players
2. Click "Call Number" to draw a bingo ball
3. Click on matching numbers on your cards to mark them
4. Get 5 in a row (horizontal, vertical, or diagonal) to win!
5. Click the "BINGO!" button when you have a winning pattern
6. Earn coins and gems based on your performance and streak

## Rewards

### Streak Bonuses
- Win multiple games in a row for bonus coins
- At 3+ streak: Earn bonus gems
- At 5+ streak: Earn even more gems
- At 10+ streak: Maximum gem bonus!

### Daily Bonus
- Claim once per day for coins
- Consecutive day claims increase rewards
- Day 7 gives bonus gems!

### Shop Items
- **Daubers**: Heart, Star, Paw Print, Flower, Rainbow, Crown
- **Themes**: Pastel Dreams, Ocean Breeze, Cherry Blossom, Sunset, Magical Night, Rainbow Paradise
- **Card Backs**: Bunny, Kitten, Puppy, Sparkle, Royal Velvet

## Tech Stack

- **Backend**: Django + Django REST Framework
- **Frontend**: React + Vite + Framer Motion
- **Database**: SQLite (for easy setup)

Made with love for Tanvi
