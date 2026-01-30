#!/bin/bash

# Colors for output
PINK='\033[1;35m'
NC='\033[0m' # No Color

echo -e "${PINK}💖 Starting Tanvi's Bingo Game... 💖${NC}"
echo ""

# Start backend
echo -e "${PINK}Starting Django backend...${NC}"
cd backend
source venv/bin/activate
python manage.py runserver 172.20.4.93:8001 &
BACKEND_PID=$!
cd ..

# Wait a moment for backend to start
sleep 2

# Start frontend
echo -e "${PINK}Starting React frontend...${NC}"
cd frontend
npm run dev -- --host 172.20.4.93 --port 5174 &
FRONTEND_PID=$!
cd ..

echo ""
echo -e "${PINK}✨ Game is starting up! ✨${NC}"
echo ""
echo "Backend running at: http://172.20.4.93:8001"
echo "Frontend running at: http://172.20.4.93:5174"
echo ""
echo "Press Ctrl+C to stop both servers"

# Wait for Ctrl+C
trap "kill $BACKEND_PID $FRONTEND_PID 2>/dev/null; echo ''; echo -e '${PINK}💖 Thanks for playing! 💖${NC}'" SIGINT
wait
