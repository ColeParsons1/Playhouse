# Playhouse

A playground of games and live voice rooms, made with love for Tanvi.

It started as a single bingo game and has grown into a mobile-friendly web app with:

- **26 games**, from bingo, pool and Connect 4 to a 3D flight simulator and a neon city driving game
- **Live voice rooms**, where people talk, chat, play bingo together and listen to music the host picks
- **A coin economy**: you earn coins from games, daily bonuses and win streaks, then spend them in the shop
- **Profiles**: avatars, stats, followers and leaderboards

This repository also holds **PlayhouseCity**, a 3D open-world city game for iOS built in Unreal Engine 5, on separate branches. See [Branches](#branches).

---

## What's inside

The app has five tabs along the bottom:

| Tab | What it does |
|---|---|
| 🎤 **Create** | Start a new voice room |
| 🎙️ **Rooms** | Browse and join live voice rooms |
| 👥 **Follow** | Find and follow other players |
| 🎮 **Games** | The game hub (see below) |
| 👤 **Me** | Your avatar, stats, shop and settings |

### Voice rooms

- **Voice chat** between everyone in the room, peer to peer over WebRTC
- **Text chat** alongside the voice
- **Multiplayer bingo** in three modes: Normal, Power-up and Blackout. Rooms can also run Connect 4, Ludo or Carrom
- **Shared music**: the host searches YouTube and controls playback, and everyone hears the same track in sync
- **15 room themes**, including Sunset, Ocean, Galaxy, Neon, Theater, Anime Theater, Fantasy Castle, Under the Sea and Heart Clouds
- **Room settings**: an entry fee and a per-card cost in coins, a player limit, and private rooms with a password

### Game hub

Every game pays out coins, and your best score goes on the leaderboard.

| # | Game | What it is |
|---|---|---|
| 1 | 🎱 Bingo | Classic bingo against 19 CPU players, with 3 cards each |
| 2 | 🌌 Galaxy Catch | Collect falling cosmic treasures |
| 3 | 🏃 Temple Run | Endless runner |
| 4 | 🦈 Baby Shark | Swim through gaps in the ocean (Flappy Bird style) |
| 5 | 🏰 Tower Defense | Build towers to stop waves of enemies |
| 6 | 🎱 Pool | Billiards against the CPU |
| 7 | 🎯 Tanks | Artillery battle against the CPU |
| 8 | 🤟 Guess the Sign | Learn the ASL alphabet |
| 9 | 🟢 Goo Bridge | Build bridges out of goo |
| 10 | 🦆 Angry Ducks | Launch ducks at piggies |
| 11 | 🃏 Color Cards | An Uno-style card game |
| 12 | 🎰 Lucky Slots | Spin to win coins |
| 13 | 👨‍🍳 Kitchen Chaos | Cook orders against the clock |
| 14 | 🔴 Connect 4 | Get four in a row |
| 15 | 🎲 Ludo | The classic board game |
| 16 | ⚪ Carrom | Strike and pocket the pieces |
| 17 | 🧱 Jenga | Pull blocks without toppling the tower |
| 18 | 🔺 Peg Game | Jump pegs until only one is left |
| 19 | 🏓 Pong | The classic arcade game |
| 20 | 🗺️ Maps | Name the country |
| 21 | 🌽 Cornhole | Toss bags for points |
| 22 | 🚗 Playhouse City | 3D driving through a neon, Miami-style city, with launch ramps |
| 23 | ✈️ Fly | 3D flight simulator with mountains, deserts, cities and airports |
| 24 | 🧸 Toy Box Battle | A 1v1 toy showdown with character classes |
| 25 | 🌻 Garden | Plant, water and harvest crops for coins |
| 26 | 🏠 Tidy Up | A relaxing room-organizing game |

The two 3D games (Playhouse City and Fly) are built with Three.js. [FEATURES.md](FEATURES.md) covers their physics, controls and HUD in detail.

### Daily bonus

Claim a bonus once a day. Claiming on consecutive days raises the reward, and day 7 adds gems. The bonus also offers quick minigames: Bubble Match, Lucky Scratch, Coin Cascade, Star Collector, Memory Flash, Color Splash, Rhythm Tap, Bubble Float and Quick Draw.

### Coins, gems and the shop

- **Coins** come from games, daily bonuses and win streaks. You can also buy them with a card (through Stripe).
- **Gems** are the premium currency. You get them from long streaks (3, 5 and 10 wins in a row) and from the 7th day of the daily bonus.
- **The shop** sells bingo daubers, card backs, themes, avatar items, rings and decorations.

---

## Tech stack

| Part | Built with |
|---|---|
| Frontend | React 19, Vite 7, Framer Motion, Three.js (3D games), Axios |
| Backend | Django 6, Django REST Framework, Django Channels and Daphne (WebSockets) |
| Database | SQLite |
| Real time | WebSockets for room events, WebRTC for voice |
| Payments | Stripe (optional) |
| Music search | yt-dlp on the server (optional) |

---

## Running it locally

### Prerequisites

- Python 3.12 or newer (Django 6 needs it)
- Node.js 20.19 or newer (Vite 7 needs it)
- Optional: [yt-dlp](https://github.com/yt-dlp/yt-dlp) on your `PATH`, if you want music search to work in voice rooms

### 1. Backend

```bash
cd backend
python3 -m venv venv
source venv/bin/activate
pip install django djangorestframework django-cors-headers channels daphne stripe
python manage.py migrate
python manage.py seed_rewards
python manage.py runserver
```

The API is now at http://localhost:8000/api/. Because Daphne is installed, `runserver` serves the WebSockets too.

On Windows, run `venv\Scripts\activate` in place of `source venv/bin/activate`.

### 2. Frontend

The frontend sends API calls to `/api` by default, which only works in production behind nginx. For local development, point it at the Django server.

Create `frontend/.env` with this line:

```
VITE_API_URL=http://localhost:8000/api
```

Then, in a second terminal:

```bash
cd frontend
npm install
npm run dev
```

Open http://localhost:5173.

On `localhost`, voice rooms connect their WebSocket to `ws://localhost:8000` automatically.

### About `start.sh`

`./start.sh` starts both servers at once, but it binds them to `localhost` (backend on port 8001, frontend on 5174). Edit those addresses to match your machine, or follow the two steps above.

### Settings and keys

| Setting | Where | What it's for |
|---|---|---|
| `STRIPE_SECRET_KEY` | Backend environment variable | Buying coins. Leave it unset if you don't need payments |
| Stripe publishable key | `frontend/src/components/BuyCoinsModal.jsx` | The browser side of Stripe checkout |
| `VITE_API_URL` | `frontend/.env` | Where the frontend sends API calls |
| `ALLOWED_HOSTS`, `CORS_ALLOWED_ORIGINS` | `backend/bingo_project/settings.py` | Add your own host here if you serve the app from another address |

---

## Project structure

```
Playhouse/
├── backend/
│   ├── bingo_project/        Django settings, URLs, ASGI app
│   ├── game/                 The main Django app
│   │   ├── models.py         Players, rooms, bingo cards, shop, rewards, garden
│   │   ├── views.py          REST API, Stripe payments, YouTube search
│   │   ├── consumers.py      WebSocket handler for voice rooms
│   │   ├── routing.py        WebSocket URLs
│   │   └── management/commands/seed_rewards.py
│   └── manage.py
├── frontend/
│   ├── public/themes/        Room and profile theme images
│   └── src/
│       ├── App.jsx           App shell and the bottom tabs
│       ├── api/client.js     API client (with caching and request de-duplication)
│       └── components/       One component per game and screen
├── FEATURES.md               In-depth notes on Fly, Playhouse City and the music player
├── CLAUDE.md                 Context for AI assistants working in this repo
├── CONTRIBUTING.md           Rules for collaborators
└── start.sh                  Starts both servers
```

### API overview

REST endpoints live under `/api/`:

| Endpoint | Purpose |
|---|---|
| `players/` | Accounts, coins, stats, follows |
| `games/` | Single-player bingo sessions |
| `rewards/`, `player-rewards/` | Shop rewards and what each player owns |
| `avatars/`, `avatar-items/` | Avatar customization |
| `rooms/` | Voice rooms |
| `shop-items/` | Shop catalog |
| `garden/` | Garden game state |
| `payments/create-intent/`, `payments/confirm/` | Stripe coin purchases |
| `youtube/search/`, `youtube/suggestions/` | Music search for voice rooms |

Voice rooms use a WebSocket at `/ws/room/<room_id>/<player_id>/`. It carries chat, bingo calls and claims, mute state, music sync, theme changes and WebRTC signaling.

---

## Branches

| Branch | Contents |
|---|---|
| `main` | This web app |
| `DevCole`, `tolsdev`, `Dev`, `IOS`, `DEV` | **PlayhouseCity**: an Unreal Engine 5 multiplayer open-world city game for iOS, written in C++. It has its own `CLAUDE.md` with build and deploy steps |
| `server-snapshots`, `server/*` | Snapshots of the production servers, taken on 2026-10-09 before they were rebuilt |

---

## Contributing

Several people work on this repo, often each with their own AI assistant. Read [CONTRIBUTING.md](CONTRIBUTING.md) and [CLAUDE.md](CLAUDE.md) before you make changes.

Design changes should stick to the McDonald's PlayPlace color palette listed in `CLAUDE.md`. Keep things bright, playful and fun.

## License

[MIT](LICENSE)

---

Made with love for Tanvi 💖
