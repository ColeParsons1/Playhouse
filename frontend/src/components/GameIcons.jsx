import React from 'react';

// Bingo - Bingo ball with number
export const BingoIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="bingoBall" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#A855F7" />
        <stop offset="100%" stopColor="#7C3AED" />
      </linearGradient>
      <linearGradient id="bingoShine" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#ffffff" stopOpacity="0.6" />
        <stop offset="100%" stopColor="#ffffff" stopOpacity="0" />
      </linearGradient>
    </defs>
    <circle cx="32" cy="32" r="28" fill="url(#bingoBall)" />
    <ellipse cx="24" cy="22" rx="10" ry="8" fill="url(#bingoShine)" />
    <rect x="18" y="24" width="28" height="20" rx="2" fill="white" />
    <text x="32" y="40" textAnchor="middle" fontSize="14" fontWeight="bold" fill="#7C3AED">B7</text>
  </svg>
);

// Pack/Presents - Gift box
export const PackIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="boxGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#3B82F6" />
        <stop offset="100%" stopColor="#1D4ED8" />
      </linearGradient>
    </defs>
    <rect x="10" y="22" width="44" height="36" rx="4" fill="url(#boxGrad)" />
    <rect x="10" y="22" width="44" height="10" rx="2" fill="#60A5FA" />
    <rect x="28" y="22" width="8" height="36" fill="#FCD34D" />
    <rect x="10" y="24" width="44" height="6" fill="#FCD34D" />
    <path d="M32 8C26 8 22 14 22 22H26C26 16 28 12 32 12C36 12 38 16 38 22H42C42 14 38 8 32 8Z" fill="#F87171" />
    <circle cx="32" cy="12" r="4" fill="#FCD34D" />
  </svg>
);

// Temple Run - Running figure
export const RunnerIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="runnerGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#22C55E" />
        <stop offset="100%" stopColor="#16A34A" />
      </linearGradient>
    </defs>
    <circle cx="32" cy="12" r="8" fill="url(#runnerGrad)" />
    <path d="M32 20L38 34L48 40L44 46L34 40L32 44L24 56L18 52L26 42L22 34L12 38L10 32L24 26L32 20Z" fill="url(#runnerGrad)" />
    <path d="M32 20L40 28L50 24L52 30L40 36L32 28" fill="url(#runnerGrad)" />
    <rect x="6" y="54" width="52" height="4" rx="2" fill="#D97706" opacity="0.6" />
  </svg>
);

// Baby Shark/Flappy - Cute shark
export const SharkIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="sharkBody" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#60A5FA" />
        <stop offset="100%" stopColor="#3B82F6" />
      </linearGradient>
    </defs>
    <ellipse cx="28" cy="32" rx="22" ry="16" fill="url(#sharkBody)" />
    <path d="M50 32L60 26L58 32L60 38L50 32Z" fill="url(#sharkBody)" />
    <path d="M28 16L32 6L36 16" fill="#3B82F6" />
    <ellipse cx="28" cy="36" rx="16" ry="10" fill="#BFDBFE" />
    <circle cx="18" cy="28" r="5" fill="white" />
    <circle cx="18" cy="28" r="3" fill="#1E293B" />
    <circle cx="17" cy="27" r="1" fill="white" />
    <path d="M8 34C10 36 14 36 16 34" stroke="#1E293B" strokeWidth="2" strokeLinecap="round" />
    <ellipse cx="38" cy="38" rx="4" ry="6" fill="#60A5FA" />
  </svg>
);

// Tower Defense - Castle tower
export const TowerIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="towerGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#22C55E" />
        <stop offset="100%" stopColor="#15803D" />
      </linearGradient>
    </defs>
    <rect x="18" y="24" width="28" height="34" fill="url(#towerGrad)" />
    <rect x="14" y="18" width="8" height="10" fill="url(#towerGrad)" />
    <rect x="26" y="18" width="12" height="10" fill="url(#towerGrad)" />
    <rect x="42" y="18" width="8" height="10" fill="url(#towerGrad)" />
    <rect x="14" y="10" width="8" height="8" fill="#16A34A" />
    <rect x="26" y="10" width="12" height="8" fill="#16A34A" />
    <rect x="42" y="10" width="8" height="8" fill="#16A34A" />
    <rect x="26" y="42" width="12" height="16" rx="6" fill="#15803D" />
    <rect x="22" y="32" width="6" height="8" fill="#0D9488" />
    <rect x="36" y="32" width="6" height="8" fill="#0D9488" />
    <path d="M10 58H54" stroke="#1E293B" strokeWidth="2" />
  </svg>
);

// Pool/Billiards - 8 ball
export const PoolIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="ballShine" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#ffffff" stopOpacity="0.5" />
        <stop offset="100%" stopColor="#ffffff" stopOpacity="0" />
      </linearGradient>
      <radialGradient id="ballGrad" cx="30%" cy="30%">
        <stop offset="0%" stopColor="#374151" />
        <stop offset="100%" stopColor="#111827" />
      </radialGradient>
    </defs>
    <circle cx="32" cy="32" r="26" fill="url(#ballGrad)" />
    <ellipse cx="24" cy="24" rx="12" ry="10" fill="url(#ballShine)" />
    <circle cx="32" cy="32" r="12" fill="white" />
    <text x="32" y="38" textAnchor="middle" fontSize="16" fontWeight="bold" fill="#111827">8</text>
  </svg>
);

// Tanks - Military tank
export const TanksIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="tankGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#3B82F6" />
        <stop offset="100%" stopColor="#1D4ED8" />
      </linearGradient>
    </defs>
    <rect x="8" y="36" width="48" height="16" rx="8" fill="#475569" />
    <circle cx="16" cy="44" r="6" fill="#1E293B" />
    <circle cx="32" cy="44" r="6" fill="#1E293B" />
    <circle cx="48" cy="44" r="6" fill="#1E293B" />
    <rect x="14" y="24" width="32" height="14" rx="2" fill="url(#tankGrad)" />
    <rect x="38" y="18" width="20" height="8" rx="2" fill="url(#tankGrad)" />
    <circle cx="18" cy="44" r="3" fill="#64748B" />
    <circle cx="32" cy="44" r="3" fill="#64748B" />
    <circle cx="46" cy="44" r="3" fill="#64748B" />
  </svg>
);

// ASL/Sign Language - Hand sign
export const ASLIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="handGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#FCD34D" />
        <stop offset="100%" stopColor="#F59E0B" />
      </linearGradient>
    </defs>
    <path d="M20 58V36C20 32 24 28 28 28V18C28 14 32 12 34 12C36 12 40 14 40 18V28C44 28 48 32 48 36V58H20Z" fill="url(#handGrad)" />
    <rect x="22" y="36" width="6" height="14" rx="3" fill="#FBBF24" />
    <rect x="30" y="32" width="6" height="18" rx="3" fill="#FBBF24" />
    <rect x="38" y="36" width="6" height="14" rx="3" fill="#FBBF24" />
    <path d="M16 42C12 42 10 38 10 34C10 30 14 28 16 28L20 36" fill="url(#handGrad)" />
    <circle cx="34" cy="18" r="4" fill="#FDE68A" />
  </svg>
);

// Goo Bridge - Green goo blob
export const GooIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="gooGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#4ADE80" />
        <stop offset="100%" stopColor="#22C55E" />
      </linearGradient>
    </defs>
    <ellipse cx="32" cy="40" rx="24" ry="18" fill="url(#gooGrad)" />
    <ellipse cx="24" cy="36" rx="8" ry="6" fill="#86EFAC" opacity="0.6" />
    <circle cx="22" cy="32" r="5" fill="white" />
    <circle cx="42" cy="32" r="5" fill="white" />
    <circle cx="23" cy="33" r="3" fill="#1E293B" />
    <circle cx="43" cy="33" r="3" fill="#1E293B" />
    <ellipse cx="32" cy="44" rx="8" ry="4" fill="#16A34A" />
    <path d="M32 18C32 18 24 22 20 30" stroke="#22C55E" strokeWidth="4" strokeLinecap="round" />
    <path d="M32 18C32 18 40 22 44 30" stroke="#22C55E" strokeWidth="4" strokeLinecap="round" />
    <circle cx="32" cy="16" r="4" fill="#4ADE80" />
  </svg>
);

// Angry Ducks - Yellow duck
export const DucksIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="duckGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#FCD34D" />
        <stop offset="100%" stopColor="#F59E0B" />
      </linearGradient>
    </defs>
    <ellipse cx="32" cy="42" rx="20" ry="16" fill="url(#duckGrad)" />
    <circle cx="32" cy="22" r="14" fill="url(#duckGrad)" />
    <ellipse cx="28" cy="20" rx="3" ry="4" fill="white" />
    <ellipse cx="38" cy="20" rx="3" ry="4" fill="white" />
    <circle cx="28" cy="21" r="2" fill="#1E293B" />
    <circle cx="38" cy="21" r="2" fill="#1E293B" />
    <path d="M24 16L20 10" stroke="#1E293B" strokeWidth="3" strokeLinecap="round" />
    <path d="M40 16L44 10" stroke="#1E293B" strokeWidth="3" strokeLinecap="round" />
    <ellipse cx="32" cy="28" rx="8" ry="4" fill="#FB923C" />
    <path d="M24 48C24 48 28 54 32 54C36 54 40 48 40 48" fill="#F59E0B" />
  </svg>
);

// Color Cards - UNO-style cards
export const CardsIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <rect x="8" y="12" width="28" height="40" rx="4" fill="#EF4444" transform="rotate(-10 8 12)" />
    <rect x="20" y="10" width="28" height="40" rx="4" fill="#3B82F6" />
    <rect x="32" y="8" width="28" height="40" rx="4" fill="#22C55E" transform="rotate(10 32 8)" />
    <ellipse cx="34" cy="28" rx="8" ry="12" fill="white" />
    <text x="34" y="34" textAnchor="middle" fontSize="14" fontWeight="bold" fill="#22C55E">7</text>
  </svg>
);

// Slots - Slot machine
export const SlotsIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="slotsGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#A855F7" />
        <stop offset="100%" stopColor="#7C3AED" />
      </linearGradient>
    </defs>
    <rect x="8" y="12" width="48" height="44" rx="6" fill="url(#slotsGrad)" />
    <rect x="12" y="20" width="40" height="24" rx="2" fill="#1E293B" />
    <rect x="14" y="22" width="10" height="20" rx="1" fill="white" />
    <rect x="27" y="22" width="10" height="20" rx="1" fill="white" />
    <rect x="40" y="22" width="10" height="20" rx="1" fill="white" />
    <text x="19" y="36" textAnchor="middle" fontSize="12">7</text>
    <text x="32" y="36" textAnchor="middle" fontSize="12">7</text>
    <text x="45" y="36" textAnchor="middle" fontSize="12">7</text>
    <circle cx="54" cy="34" r="6" fill="#FCD34D" />
    <rect x="52" y="28" width="4" height="12" rx="2" fill="#F59E0B" />
  </svg>
);

// Kitchen Chaos - Chef hat
export const KitchenIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="hatGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#FFFFFF" />
        <stop offset="100%" stopColor="#E5E7EB" />
      </linearGradient>
    </defs>
    <ellipse cx="32" cy="22" rx="20" ry="14" fill="url(#hatGrad)" />
    <ellipse cx="18" cy="20" rx="8" ry="10" fill="url(#hatGrad)" />
    <ellipse cx="46" cy="20" rx="8" ry="10" fill="url(#hatGrad)" />
    <rect x="14" y="30" width="36" height="8" fill="url(#hatGrad)" />
    <ellipse cx="32" cy="50" rx="14" ry="8" fill="#FCD34D" />
    <circle cx="26" cy="48" r="3" fill="#1E293B" />
    <circle cx="38" cy="48" r="3" fill="#1E293B" />
    <path d="M28 54C30 56 34 56 36 54" stroke="#1E293B" strokeWidth="2" strokeLinecap="round" />
    <rect x="8" y="56" width="48" height="4" rx="2" fill="#FB923C" />
  </svg>
);

// Connect 4 - Grid with discs
export const Connect4Icon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <rect x="6" y="14" width="52" height="44" rx="4" fill="#3B82F6" />
    <circle cx="18" cy="26" r="6" fill="#1E293B" />
    <circle cx="32" cy="26" r="6" fill="#1E293B" />
    <circle cx="46" cy="26" r="6" fill="#EF4444" />
    <circle cx="18" cy="40" r="6" fill="#FCD34D" />
    <circle cx="32" cy="40" r="6" fill="#EF4444" />
    <circle cx="46" cy="40" r="6" fill="#FCD34D" />
    <circle cx="18" cy="54" r="4" fill="#1E293B" />
    <circle cx="32" cy="54" r="4" fill="#FCD34D" />
    <circle cx="46" cy="54" r="4" fill="#EF4444" />
  </svg>
);

// Ludo - Dice and board
export const LudoIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <rect x="8" y="8" width="48" height="48" rx="4" fill="#F3F4F6" />
    <rect x="8" y="8" width="22" height="22" fill="#EF4444" />
    <rect x="34" y="8" width="22" height="22" fill="#22C55E" />
    <rect x="8" y="34" width="22" height="22" fill="#3B82F6" />
    <rect x="34" y="34" width="22" height="22" fill="#FCD34D" />
    <rect x="26" y="26" width="12" height="12" fill="white" stroke="#E5E7EB" strokeWidth="1" />
    <circle cx="18" cy="18" r="4" fill="white" />
    <circle cx="46" cy="18" r="4" fill="white" />
    <circle cx="18" cy="46" r="4" fill="white" />
    <circle cx="46" cy="46" r="4" fill="white" />
  </svg>
);

// Carrom - Striker and coins
export const CarromIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <rect x="8" y="8" width="48" height="48" rx="2" fill="#D97706" />
    <rect x="12" y="12" width="40" height="40" fill="#FCD34D" />
    <circle cx="32" cy="32" r="16" fill="#FBBF24" stroke="#D97706" strokeWidth="1" />
    <circle cx="32" cy="26" r="5" fill="white" stroke="#E5E7EB" strokeWidth="1" />
    <circle cx="26" cy="34" r="5" fill="#1E293B" />
    <circle cx="38" cy="34" r="5" fill="white" stroke="#E5E7EB" strokeWidth="1" />
    <circle cx="32" cy="42" r="5" fill="#1E293B" />
    <circle cx="32" cy="32" r="3" fill="#EF4444" />
    <circle cx="14" cy="14" r="4" fill="#1E293B" />
    <circle cx="50" cy="14" r="4" fill="#1E293B" />
    <circle cx="14" cy="50" r="4" fill="#1E293B" />
    <circle cx="50" cy="50" r="4" fill="#1E293B" />
  </svg>
);

// Jenga - Stacked blocks
export const JengaIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="woodGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#FCD34D" />
        <stop offset="100%" stopColor="#D97706" />
      </linearGradient>
    </defs>
    <rect x="20" y="52" width="24" height="8" fill="url(#woodGrad)" />
    <rect x="18" y="44" width="28" height="8" fill="#FBBF24" transform="rotate(2 18 44)" />
    <rect x="22" y="36" width="20" height="8" fill="url(#woodGrad)" />
    <rect x="16" y="28" width="32" height="8" fill="#FBBF24" transform="rotate(-1 16 28)" />
    <rect x="24" y="20" width="16" height="8" fill="url(#woodGrad)" transform="rotate(3 24 20)" />
    <rect x="20" y="12" width="24" height="8" fill="#FBBF24" />
    <rect x="26" y="4" width="12" height="8" fill="url(#woodGrad)" transform="rotate(-2 26 4)" />
  </svg>
);

// Peg Game - Triangle with pegs
export const PegIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="pegBoardGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#FB923C" />
        <stop offset="100%" stopColor="#EA580C" />
      </linearGradient>
    </defs>
    <path d="M32 6L58 56H6L32 6Z" fill="url(#pegBoardGrad)" />
    <circle cx="32" cy="18" r="5" fill="#1E293B" />
    <circle cx="26" cy="28" r="5" fill="#EF4444" />
    <circle cx="38" cy="28" r="5" fill="#EF4444" />
    <circle cx="20" cy="38" r="5" fill="#EF4444" />
    <circle cx="32" cy="38" r="5" fill="#1E293B" />
    <circle cx="44" cy="38" r="5" fill="#EF4444" />
    <circle cx="14" cy="48" r="5" fill="#EF4444" />
    <circle cx="26" cy="48" r="5" fill="#1E293B" />
    <circle cx="38" cy="48" r="5" fill="#EF4444" />
    <circle cx="50" cy="48" r="5" fill="#EF4444" />
  </svg>
);

// Pong - Paddle and ball
export const PongIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <rect x="4" y="4" width="56" height="56" rx="4" fill="#1E293B" />
    <rect x="10" y="20" width="6" height="24" rx="2" fill="white" />
    <rect x="48" y="20" width="6" height="24" rx="2" fill="white" />
    <circle cx="32" cy="32" r="6" fill="white" />
    <line x1="32" y1="8" x2="32" y2="56" stroke="white" strokeWidth="2" strokeDasharray="4 4" />
    <text x="20" y="14" fontSize="8" fill="white" fontWeight="bold">3</text>
    <text x="42" y="14" fontSize="8" fill="white" fontWeight="bold">2</text>
  </svg>
);

// Maps - Globe with countries
export const MapsIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="oceanGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#60A5FA" />
        <stop offset="100%" stopColor="#3B82F6" />
      </linearGradient>
    </defs>
    <circle cx="32" cy="32" r="26" fill="url(#oceanGrad)" />
    <ellipse cx="24" cy="24" rx="12" ry="10" fill="white" opacity="0.2" />
    <path d="M20 18C18 22 20 28 26 30C32 32 36 28 38 22C40 16 38 12 32 12C26 12 22 14 20 18Z" fill="#22C55E" />
    <path d="M40 26C42 30 46 34 50 32C52 30 50 26 46 24C42 22 38 22 40 26Z" fill="#22C55E" />
    <path d="M18 36C14 38 12 44 16 48C20 52 28 52 32 48C36 44 34 38 28 36C24 34 22 34 18 36Z" fill="#22C55E" />
    <path d="M42 38C44 42 48 46 52 44C54 42 52 38 48 36C44 34 40 34 42 38Z" fill="#22C55E" />
    <ellipse cx="32" cy="32" rx="26" ry="26" fill="none" stroke="#1E40AF" strokeWidth="2" />
    <path d="M32 6V58" stroke="#1E40AF" strokeWidth="1" opacity="0.3" />
    <path d="M6 32H58" stroke="#1E40AF" strokeWidth="1" opacity="0.3" />
  </svg>
);

// Cornhole - Bean bag and board
export const CornholeIcon = ({ size = 64 }) => (
  <svg width={size} height={size} viewBox="0 0 64 64" fill="none" xmlns="http://www.w3.org/2000/svg">
    <defs>
      <linearGradient id="boardGrad" x1="0%" y1="0%" x2="100%" y2="100%">
        <stop offset="0%" stopColor="#FB923C" />
        <stop offset="100%" stopColor="#EA580C" />
      </linearGradient>
    </defs>
    <path d="M8 56L16 20H48L56 56H8Z" fill="url(#boardGrad)" />
    <ellipse cx="32" cy="32" rx="10" ry="8" fill="#1E293B" />
    <rect x="14" y="18" width="36" height="4" rx="1" fill="#D97706" />
    <ellipse cx="24" cy="12" rx="8" ry="6" fill="#FCD34D" />
    <ellipse cx="24" cy="12" rx="6" ry="4" fill="#FBBF24" />
    <path d="M20 10L28 14" stroke="#D97706" strokeWidth="1" />
    <path d="M20 14L28 10" stroke="#D97706" strokeWidth="1" />
    <ellipse cx="44" cy="44" rx="6" ry="5" fill="#EF4444" />
    <ellipse cx="44" cy="44" rx="4" ry="3" fill="#DC2626" />
  </svg>
);

// Simulator - 3D City Car
const SimulatorIcon = ({ size = 60 }) => (
  <svg width={size} height={size} viewBox="0 0 80 80" fill="none">
    {/* Night sky background */}
    <rect width="80" height="80" rx="12" fill="url(#nightSky)" />

    {/* City buildings silhouette */}
    <rect x="5" y="28" width="8" height="32" fill="#2d3748" />
    <rect x="15" y="20" width="10" height="40" fill="#1a202c" />
    <rect x="27" y="32" width="7" height="28" fill="#2d3748" />
    <rect x="36" y="15" width="12" height="45" fill="#1a202c" />
    <rect x="50" y="25" width="8" height="35" fill="#2d3748" />
    <rect x="60" y="18" width="10" height="42" fill="#1a202c" />
    <rect x="72" y="30" width="6" height="30" fill="#2d3748" />

    {/* Building windows (lit) */}
    <rect x="17" y="24" width="2" height="2" fill="#fef08a" opacity="0.8" />
    <rect x="21" y="28" width="2" height="2" fill="#fef08a" opacity="0.6" />
    <rect x="17" y="34" width="2" height="2" fill="#fef08a" opacity="0.9" />
    <rect x="39" y="20" width="2" height="2" fill="#fef08a" opacity="0.7" />
    <rect x="43" y="26" width="2" height="2" fill="#fef08a" opacity="0.8" />
    <rect x="39" y="32" width="2" height="2" fill="#fef08a" opacity="0.6" />
    <rect x="43" y="38" width="2" height="2" fill="#fef08a" opacity="0.9" />
    <rect x="62" y="24" width="2" height="2" fill="#fef08a" opacity="0.7" />
    <rect x="66" y="30" width="2" height="2" fill="#fef08a" opacity="0.8" />

    {/* Road */}
    <rect x="0" y="60" width="80" height="20" fill="#374151" />
    <rect x="10" y="68" width="12" height="2" fill="#fbbf24" />
    <rect x="30" y="68" width="12" height="2" fill="#fbbf24" />
    <rect x="50" y="68" width="12" height="2" fill="#fbbf24" />
    <rect x="70" y="68" width="8" height="2" fill="#fbbf24" />

    {/* Car body */}
    <rect x="28" y="52" width="24" height="10" rx="2" fill="#dc2626" />
    <rect x="31" y="47" width="18" height="7" rx="2" fill="#1f2937" />

    {/* Car windows */}
    <rect x="33" y="48" width="6" height="4" rx="1" fill="#60a5fa" opacity="0.8" />
    <rect x="41" y="48" width="6" height="4" rx="1" fill="#60a5fa" opacity="0.8" />

    {/* Car wheels */}
    <circle cx="33" cy="62" r="4" fill="#1f2937" />
    <circle cx="33" cy="62" r="2" fill="#6b7280" />
    <circle cx="47" cy="62" r="4" fill="#1f2937" />
    <circle cx="47" cy="62" r="2" fill="#6b7280" />

    {/* Headlights glow */}
    <ellipse cx="52" cy="57" rx="2" ry="1.5" fill="#fef9c3" />
    <ellipse cx="52" cy="57" rx="4" ry="3" fill="#fef9c3" opacity="0.3" />

    {/* Stars */}
    <circle cx="12" cy="10" r="1" fill="white" opacity="0.8" />
    <circle cx="35" cy="8" r="0.8" fill="white" opacity="0.6" />
    <circle cx="60" cy="12" r="1" fill="white" opacity="0.7" />
    <circle cx="72" cy="6" r="0.8" fill="white" opacity="0.9" />

    <defs>
      <linearGradient id="nightSky" x1="0" y1="0" x2="0" y2="80">
        <stop offset="0%" stopColor="#0f172a" />
        <stop offset="100%" stopColor="#1e293b" />
      </linearGradient>
    </defs>
  </svg>
);

// Export all icons as a map
export const GameIconMap = {
  bingo: BingoIcon,
  presents: PackIcon,
  runner: RunnerIcon,
  flappy: SharkIcon,
  tower: TowerIcon,
  billiards: PoolIcon,
  tanks: TanksIcon,
  asl: ASLIcon,
  goo: GooIcon,
  ducks: DucksIcon,
  cards: CardsIcon,
  slots: SlotsIcon,
  kitchen: KitchenIcon,
  connect4: Connect4Icon,
  ludo: LudoIcon,
  carrom: CarromIcon,
  jenga: JengaIcon,
  pegs: PegIcon,
  pong: PongIcon,
  maps: MapsIcon,
  cornhole: CornholeIcon,
  simulator: SimulatorIcon,
};

export default GameIconMap;
