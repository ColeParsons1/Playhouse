# Claude Context File

> **IMPORTANT INSTRUCTIONS FOR CLAUDE AI**
>
> 1. **ON SESSION START**: Read this entire file before doing any work.
> 2. **BEFORE EVERY PUSH**:
>    - Re-read this file to check for updates from the other collaborator
>    - Update SECTION 9 (Session Log) with what you changed
>    - Update SECTION 4 (Current State) if needed
> 3. **AFTER EVERY PUSH**: Ask your user:
>    *"I've pushed the changes. Would you like to add any context to CLAUDE.md for the other collaborator's Claude to know about?"*
> 4. **CHECK FOR CONFLICTS**: If another session recently logged changes, inform your user before proceeding.

---

# SECTION 1: PROJECT OVERVIEW

**Playhouse** - A multiplayer bingo game made for Tanvi featuring cute baby animals, streaks, rewards, and social features.

### Tech Stack
- **Backend**: Django 6.0 + Django REST Framework + Channels + Daphne
- **Frontend**: React + Vite + Framer Motion
- **Database**: SQLite
- **Payments**: Stripe (optional)

---

# SECTION 2: PROJECT STRUCTURE

```
Playhouse/
├── backend/
│   ├── bingo_project/     # Django project settings
│   ├── game/              # Main app (models, views, serializers)
│   │   ├── models.py      # Player, GameSession, BingoCard, VoiceRoom, etc.
│   │   ├── views.py       # API endpoints
│   │   ├── serializers.py # DRF serializers
│   │   └── urls.py        # API routes
│   ├── manage.py
│   └── venv/              # Python virtual environment (not in git)
├── frontend/
│   ├── src/
│   │   ├── components/    # React components
│   │   ├── api/           # API client
│   │   └── App.jsx        # Main app component
│   └── package.json
├── README.md
├── CONTRIBUTING.md        # Guide for Claude instances
└── start.sh               # Startup script (Linux/Mac)
```

---

# SECTION 3: RUNNING LOCALLY

### Backend (Windows)
```bash
cd backend
venv/Scripts/python.exe manage.py runserver 0.0.0.0:8000
```

### Backend (Mac/Linux)
```bash
cd backend
source venv/bin/activate
python manage.py runserver
```

### Frontend
```bash
cd frontend
npm install   # first time only
npm run dev
```

### URLs
- Frontend: http://localhost:5173
- Backend API: http://localhost:8000/api/

---

# SECTION 4: CURRENT STATE

### Last Updated: 2026-02-01

### Active Work
| Feature | Who | Status | Notes |
|---------|-----|--------|-------|
| - | - | - | - |

### Recent Changes
- Initial setup on Tols' machine
- Database migrations applied
- Dependencies installed (Django, DRF, channels, daphne, stripe)

### Known Issues
- None currently documented

---

# SECTION 5: DEPENDENCIES

### Backend (in venv)
- django
- djangorestframework
- django-cors-headers
- daphne
- channels
- stripe

### Frontend
- See `frontend/package.json`
- Run `npm install` to install

---

# SECTION 6: COLLABORATORS

| Name | Machine | Email | Location |
|------|---------|-------|----------|
| Tols | Windows PC | collaborator@example.com | Primary |
| Friend | TBD | TBD | Remote |

---

# SECTION 7: RULES FOR CLAUDE

1. **Always pull before making changes**: `git pull origin main`
2. **Read files before editing**: Never assume current state
3. **Check this file first**: For project context and current state
4. **Update Active Work table**: When starting/finishing features
5. **Windows paths**: Use forward slashes or escaped backslashes
6. **Venv on Windows**: Use `venv/Scripts/python.exe` directly
7. **Venv on Mac/Linux**: Use `source venv/bin/activate`
8. **Commit format**: Include `Co-Authored-By: Claude <noreply@anthropic.com>`

---

# SECTION 8: API REFERENCE

Base URL: `http://localhost:8000/api/`

Key endpoints defined in `backend/game/urls.py`

---

# SECTION 9: CONTEXT SHARING

Use this section to leave notes for the other collaborator's Claude.

### From Tols to Friend:
- Backend venv is NOT in git - you'll need to create it and install dependencies
- I'm on Windows, commands may differ for you
- Let me know your machine type so I can add it to collaborators

### From Friend to Tols:
*(Add your notes here)*

---

# SECTION 10: SESSION LOG

Use this section to log major changes made in each session.

### 2026-02-01 - Tols
- Cloned repository
- Set up SSH keys for GitHub
- Installed backend dependencies (django, drf, channels, daphne, stripe)
- Installed frontend dependencies (npm install)
- Applied database migrations
- Created CLAUDE.md and CONTRIBUTING.md
