import { useState, useEffect, useCallback, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './ASLGuessGame.css';

const GAME_DURATION = 60000; // 60 seconds
const COINS_PER_CORRECT = 10;
const STREAK_BONUS = 5;
const TIME_BONUS_THRESHOLD = 2500; // Under 2.5 seconds = bonus
const PERFECT_STREAK_BONUS = 25;

// ASL hand signs with their descriptions - Letters
const ASL_LETTERS = [
  { letter: 'A', emoji: '✊', description: 'Fist with thumb on side', difficulty: 1, type: 'letter' },
  { letter: 'B', emoji: '🤚', description: 'Flat hand, fingers together, thumb tucked', difficulty: 1, type: 'letter' },
  { letter: 'C', emoji: '🤏', description: 'Curved hand like holding a cup', difficulty: 1, type: 'letter' },
  { letter: 'D', emoji: '👆', description: 'Index finger up, others curled touching thumb', difficulty: 2, type: 'letter' },
  { letter: 'E', emoji: '🤛', description: 'Fingers curled into palm, thumb across', difficulty: 2, type: 'letter' },
  { letter: 'F', emoji: '👌', description: 'Index and thumb make circle, others up', difficulty: 2, type: 'letter' },
  { letter: 'G', emoji: '👉', description: 'Index and thumb point sideways', difficulty: 2, type: 'letter' },
  { letter: 'H', emoji: '🤞', description: 'Index and middle fingers together, sideways', difficulty: 2, type: 'letter' },
  { letter: 'I', emoji: '🤙', description: 'Pinky up, other fingers in fist', difficulty: 1, type: 'letter' },
  { letter: 'K', emoji: '✌️', description: 'Index and middle up in V, thumb between', difficulty: 3, type: 'letter' },
  { letter: 'L', emoji: '🤟', description: 'L-shape: index up, thumb out', difficulty: 1, type: 'letter' },
  { letter: 'M', emoji: '✊', description: 'Fist with three fingers over thumb', difficulty: 3, type: 'letter' },
  { letter: 'N', emoji: '✊', description: 'Fist with two fingers over thumb', difficulty: 3, type: 'letter' },
  { letter: 'O', emoji: '👌', description: 'All fingers touch thumb making O', difficulty: 1, type: 'letter' },
  { letter: 'P', emoji: '👇', description: 'Like K but pointing down', difficulty: 3, type: 'letter' },
  { letter: 'Q', emoji: '👇', description: 'Like G but pointing down', difficulty: 3, type: 'letter' },
  { letter: 'R', emoji: '🤞', description: 'Index and middle crossed', difficulty: 2, type: 'letter' },
  { letter: 'S', emoji: '✊', description: 'Fist with thumb over fingers', difficulty: 2, type: 'letter' },
  { letter: 'T', emoji: '✊', description: 'Fist with thumb between index and middle', difficulty: 3, type: 'letter' },
  { letter: 'U', emoji: '✌️', description: 'Index and middle up together', difficulty: 2, type: 'letter' },
  { letter: 'V', emoji: '✌️', description: 'Index and middle up in V shape', difficulty: 1, type: 'letter' },
  { letter: 'W', emoji: '🤟', description: 'Index, middle, ring up spread', difficulty: 2, type: 'letter' },
  { letter: 'X', emoji: '☝️', description: 'Index finger hooked/curved', difficulty: 3, type: 'letter' },
  { letter: 'Y', emoji: '🤙', description: 'Thumb and pinky out, others in fist', difficulty: 1, type: 'letter' },
];

// ASL Common Words/Phrases
const ASL_PHRASES = [
  { letter: 'HELLO', emoji: '👋', description: 'Wave hand near forehead', difficulty: 1, type: 'phrase' },
  { letter: 'THANK YOU', emoji: '🙏', description: 'Touch chin, move hand forward', difficulty: 1, type: 'phrase' },
  { letter: 'PLEASE', emoji: '🤲', description: 'Flat hand circles on chest', difficulty: 1, type: 'phrase' },
  { letter: 'SORRY', emoji: '😔', description: 'Fist circles on chest', difficulty: 1, type: 'phrase' },
  { letter: 'YES', emoji: '👍', description: 'Fist nods like a head', difficulty: 1, type: 'phrase' },
  { letter: 'NO', emoji: '👎', description: 'Index and middle tap thumb', difficulty: 1, type: 'phrase' },
  { letter: 'HELP', emoji: '🆘', description: 'Thumbs up on palm, lift up', difficulty: 2, type: 'phrase' },
  { letter: 'LOVE', emoji: '❤️', description: 'Cross arms over chest', difficulty: 1, type: 'phrase' },
  { letter: 'FRIEND', emoji: '🤝', description: 'Hook index fingers together', difficulty: 2, type: 'phrase' },
  { letter: 'FAMILY', emoji: '👨‍👩‍👧', description: 'F hands circle forward', difficulty: 2, type: 'phrase' },
  { letter: 'EAT', emoji: '🍽️', description: 'Fingers to mouth repeatedly', difficulty: 1, type: 'phrase' },
  { letter: 'DRINK', emoji: '🥤', description: 'Thumb to mouth, tilt hand', difficulty: 1, type: 'phrase' },
  { letter: 'HAPPY', emoji: '😊', description: 'Flat hands brush up on chest', difficulty: 2, type: 'phrase' },
  { letter: 'SAD', emoji: '😢', description: 'Hands move down face', difficulty: 2, type: 'phrase' },
  { letter: 'GOOD', emoji: '👍', description: 'Touch chin, move forward', difficulty: 1, type: 'phrase' },
  { letter: 'BAD', emoji: '👎', description: 'Touch chin, flip hand down', difficulty: 2, type: 'phrase' },
];

// Combine letters and phrases
const ASL_SIGNS = [...ASL_LETTERS, ...ASL_PHRASES];

// Realistic hand position SVG illustrations for each letter
// Using detailed anatomical shapes with proper shading
const HAND_ILLUSTRATIONS = {
  A: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinA" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#FDBEA0" />
          <stop offset="50%" stopColor="#E8A888" />
          <stop offset="100%" stopColor="#D4956C" />
        </linearGradient>
        <filter id="shadowA" x="-20%" y="-20%" width="140%" height="140%">
          <feDropShadow dx="2" dy="3" stdDeviation="2" floodOpacity="0.3"/>
        </filter>
      </defs>
      {/* Fist body */}
      <ellipse cx="50" cy="58" rx="28" ry="32" fill="url(#skinA)" filter="url(#shadowA)"/>
      {/* Curled fingers */}
      <ellipse cx="50" cy="35" rx="26" ry="14" fill="url(#skinA)"/>
      {/* Knuckle lines */}
      <path d="M 28 38 Q 50 32 72 38" fill="none" stroke="#C4886A" strokeWidth="1.5" opacity="0.6"/>
      <path d="M 32 42 Q 50 38 68 42" fill="none" stroke="#C4886A" strokeWidth="1" opacity="0.4"/>
      {/* Thumb on side */}
      <ellipse cx="22" cy="50" rx="10" ry="18" fill="url(#skinA)" transform="rotate(-15 22 50)"/>
      <path d="M 18 40 Q 22 55 26 62" fill="none" stroke="#C4886A" strokeWidth="1" opacity="0.5"/>
      {/* Wrist */}
      <rect x="35" y="82" width="30" height="18" rx="8" fill="url(#skinA)"/>
    </svg>
  ),
  B: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinB" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#FDBEA0" />
          <stop offset="50%" stopColor="#E8A888" />
          <stop offset="100%" stopColor="#D4956C" />
        </linearGradient>
        <filter id="shadowB" x="-20%" y="-20%" width="140%" height="140%">
          <feDropShadow dx="2" dy="3" stdDeviation="2" floodOpacity="0.3"/>
        </filter>
      </defs>
      {/* Palm */}
      <rect x="25" y="55" width="50" height="40" rx="8" fill="url(#skinB)" filter="url(#shadowB)"/>
      {/* Four fingers extended */}
      <rect x="26" y="10" width="11" height="50" rx="5" fill="url(#skinB)"/>
      <rect x="39" y="5" width="11" height="55" rx="5" fill="url(#skinB)"/>
      <rect x="52" y="8" width="11" height="52" rx="5" fill="url(#skinB)"/>
      <rect x="65" y="14" width="10" height="46" rx="5" fill="url(#skinB)"/>
      {/* Finger joints */}
      <circle cx="31" cy="22" r="2" fill="#C4886A" opacity="0.4"/>
      <circle cx="44" cy="18" r="2" fill="#C4886A" opacity="0.4"/>
      <circle cx="57" cy="20" r="2" fill="#C4886A" opacity="0.4"/>
      <circle cx="70" cy="26" r="2" fill="#C4886A" opacity="0.4"/>
      {/* Thumb tucked */}
      <ellipse cx="20" cy="72" rx="8" ry="14" fill="url(#skinB)" transform="rotate(-20 20 72)"/>
    </svg>
  ),
  C: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinC" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#FDBEA0" />
          <stop offset="50%" stopColor="#E8A888" />
          <stop offset="100%" stopColor="#D4956C" />
        </linearGradient>
        <filter id="shadowC" x="-20%" y="-20%" width="140%" height="140%">
          <feDropShadow dx="2" dy="3" stdDeviation="2" floodOpacity="0.3"/>
        </filter>
      </defs>
      {/* C-shaped hand */}
      <path d="M 30 25 C 15 40, 15 70, 35 85 Q 50 92, 65 85 C 85 70, 85 40, 70 25"
            fill="url(#skinC)" stroke="#C4886A" strokeWidth="2" filter="url(#shadowC)"/>
      {/* Inner curve detail */}
      <path d="M 38 35 C 28 48, 28 62, 40 75 Q 50 82, 60 75 C 72 62, 72 48, 62 35"
            fill="none" stroke="#C4886A" strokeWidth="1.5" opacity="0.4"/>
      {/* Fingertips */}
      <circle cx="30" cy="25" r="6" fill="url(#skinC)"/>
      <circle cx="70" cy="25" r="6" fill="url(#skinC)"/>
    </svg>
  ),
  D: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinD" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#FDBEA0" />
          <stop offset="50%" stopColor="#E8A888" />
          <stop offset="100%" stopColor="#D4956C" />
        </linearGradient>
        <filter id="shadowD" x="-20%" y="-20%" width="140%" height="140%">
          <feDropShadow dx="2" dy="3" stdDeviation="2" floodOpacity="0.3"/>
        </filter>
      </defs>
      {/* Circle made by fingers and thumb */}
      <ellipse cx="50" cy="62" rx="26" ry="28" fill="url(#skinD)" filter="url(#shadowD)"/>
      {/* Hole in center */}
      <ellipse cx="50" cy="62" rx="12" ry="14" fill="#1a1a2e"/>
      {/* Index finger pointing up */}
      <rect x="42" y="5" width="16" height="45" rx="7" fill="url(#skinD)"/>
      <circle cx="50" cy="8" r="7" fill="url(#skinD)"/>
      {/* Finger joint */}
      <ellipse cx="50" cy="25" rx="5" ry="2" fill="#C4886A" opacity="0.3"/>
      {/* Thumb touching fingers */}
      <ellipse cx="28" cy="58" rx="12" ry="16" fill="url(#skinD)" transform="rotate(20 28 58)"/>
    </svg>
  ),
  E: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinE" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#FDBEA0" />
          <stop offset="50%" stopColor="#E8A888" />
          <stop offset="100%" stopColor="#D4956C" />
        </linearGradient>
        <filter id="shadowE" x="-20%" y="-20%" width="140%" height="140%">
          <feDropShadow dx="2" dy="3" stdDeviation="2" floodOpacity="0.3"/>
        </filter>
      </defs>
      {/* Fist with fingers curled in */}
      <ellipse cx="50" cy="55" rx="30" ry="35" fill="url(#skinE)" filter="url(#shadowE)"/>
      {/* Curved fingers at top */}
      <path d="M 25 35 Q 50 20 75 35" fill="url(#skinE)" stroke="#C4886A" strokeWidth="1.5"/>
      {/* Knuckle bumps */}
      <ellipse cx="32" cy="32" rx="6" ry="4" fill="url(#skinE)"/>
      <ellipse cx="44" cy="28" rx="6" ry="4" fill="url(#skinE)"/>
      <ellipse cx="56" cy="28" rx="6" ry="4" fill="url(#skinE)"/>
      <ellipse cx="68" cy="32" rx="6" ry="4" fill="url(#skinE)"/>
      {/* Thumb across fingers */}
      <ellipse cx="50" cy="45" rx="22" ry="8" fill="url(#skinE)" transform="rotate(-5 50 45)"/>
      <path d="M 30 45 Q 50 40 70 45" fill="none" stroke="#C4886A" strokeWidth="1" opacity="0.4"/>
      <ellipse cx="50" cy="38" rx="14" ry="10" fill="#e8d4c0" stroke="#c9a67a" strokeWidth="1.5"/>
    </svg>
  ),
  F: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinF" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <circle cx="38" cy="62" r="14" fill="none" stroke="url(#skinF)" strokeWidth="12"/>
      <circle cx="38" cy="62" r="14" fill="none" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="44" y="12" width="9" height="38" rx="4" fill="url(#skinF)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="55" y="10" width="9" height="40" rx="4" fill="url(#skinF)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="66" y="16" width="9" height="34" rx="4" fill="url(#skinF)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  G: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinG" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="48" cy="55" rx="20" ry="24" fill="url(#skinG)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="62" y="42" width="28" height="10" rx="4" fill="url(#skinG)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="28" cy="48" rx="11" ry="9" fill="url(#skinG)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  H: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinH" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="48" cy="62" rx="20" ry="24" fill="url(#skinH)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="58" y="38" width="28" height="10" rx="4" fill="url(#skinH)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="58" y="52" width="28" height="10" rx="4" fill="url(#skinH)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  I: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinI" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="48" cy="55" rx="22" ry="28" fill="url(#skinI)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="66" y="12" width="9" height="38" rx="4" fill="url(#skinI)" stroke="#c9a67a" strokeWidth="2"/>
      <circle cx="70" cy="10" r="5" fill="url(#skinI)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  K: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinK" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="62" rx="20" ry="24" fill="url(#skinK)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="40" y="8" width="9" height="44" rx="4" fill="url(#skinK)" stroke="#c9a67a" strokeWidth="2" transform="rotate(-12 44 30)"/>
      <rect x="52" y="8" width="9" height="44" rx="4" fill="url(#skinK)" stroke="#c9a67a" strokeWidth="2" transform="rotate(12 56 30)"/>
      <ellipse cx="32" cy="52" rx="10" ry="12" fill="url(#skinK)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  L: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinL" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="52" cy="62" rx="20" ry="24" fill="url(#skinL)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="46" y="8" width="10" height="46" rx="4" fill="url(#skinL)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="18" y="54" width="28" height="10" rx="4" fill="url(#skinL)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  M: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinM" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="55" rx="24" ry="30" fill="url(#skinM)" stroke="#c9a67a" strokeWidth="2"/>
      <path d="M 28 38 L 38 58 M 42 38 L 50 58 M 56 38 L 62 58" stroke="#c9a67a" strokeWidth="4" strokeLinecap="round"/>
    </svg>
  ),
  N: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinN" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="55" rx="24" ry="30" fill="url(#skinN)" stroke="#c9a67a" strokeWidth="2"/>
      <path d="M 36 38 L 44 58 M 52 38 L 60 58" stroke="#c9a67a" strokeWidth="4" strokeLinecap="round"/>
    </svg>
  ),
  O: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinO" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="50" rx="28" ry="34" fill="url(#skinO)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="50" cy="50" rx="14" ry="18" fill="#f5e6d3" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  P: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinP" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="38" rx="20" ry="24" fill="url(#skinP)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="40" y="52" width="9" height="38" rx="4" fill="url(#skinP)" stroke="#c9a67a" strokeWidth="2" transform="rotate(-12 44 70)"/>
      <rect x="52" y="52" width="9" height="38" rx="4" fill="url(#skinP)" stroke="#c9a67a" strokeWidth="2" transform="rotate(12 56 70)"/>
    </svg>
  ),
  Q: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinQ" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="38" rx="20" ry="22" fill="url(#skinQ)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="44" y="55" width="10" height="35" rx="4" fill="url(#skinQ)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="32" cy="68" rx="10" ry="12" fill="url(#skinQ)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  R: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinR" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="62" rx="20" ry="24" fill="url(#skinR)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="42" y="8" width="9" height="46" rx="4" fill="url(#skinR)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="52" y="8" width="9" height="46" rx="4" fill="url(#skinR)" stroke="#c9a67a" strokeWidth="2"/>
      <path d="M 46 24 L 56 36" stroke="#c9a67a" strokeWidth="4" strokeLinecap="round"/>
    </svg>
  ),
  S: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinS" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="55" rx="24" ry="30" fill="url(#skinS)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="50" cy="30" rx="16" ry="10" fill="url(#skinS)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  T: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinT" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="55" rx="24" ry="30" fill="url(#skinT)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="40" cy="36" rx="10" ry="14" fill="url(#skinT)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  U: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinU" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="62" rx="20" ry="24" fill="url(#skinU)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="40" y="8" width="9" height="46" rx="4" fill="url(#skinU)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="52" y="8" width="9" height="46" rx="4" fill="url(#skinU)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  V: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinV" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="62" rx="20" ry="24" fill="url(#skinV)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="36" y="8" width="9" height="46" rx="4" fill="url(#skinV)" stroke="#c9a67a" strokeWidth="2" transform="rotate(-15 40 30)"/>
      <rect x="56" y="8" width="9" height="46" rx="4" fill="url(#skinV)" stroke="#c9a67a" strokeWidth="2" transform="rotate(15 60 30)"/>
    </svg>
  ),
  W: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinW" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="68" rx="20" ry="20" fill="url(#skinW)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="28" y="12" width="9" height="44" rx="4" fill="url(#skinW)" stroke="#c9a67a" strokeWidth="2" transform="rotate(-10 32 34)"/>
      <rect x="46" y="8" width="9" height="48" rx="4" fill="url(#skinW)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="64" y="12" width="9" height="44" rx="4" fill="url(#skinW)" stroke="#c9a67a" strokeWidth="2" transform="rotate(10 68 34)"/>
    </svg>
  ),
  X: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinX" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="55" rx="22" ry="28" fill="url(#skinX)" stroke="#c9a67a" strokeWidth="2"/>
      <path d="M 50 28 Q 62 18 58 34 Q 50 46 52 40" fill="url(#skinX)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  Y: (
    <svg viewBox="0 0 100 100" className="hand-svg">
      <defs>
        <linearGradient id="skinY" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="55" rx="22" ry="28" fill="url(#skinY)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="20" y="32" width="22" height="10" rx="4" fill="url(#skinY)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="70" y="12" width="9" height="34" rx="4" fill="url(#skinY)" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  // Phrase illustrations
  'HELLO': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinHello" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="60" rx="18" ry="22" fill="url(#skinHello)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="30" y="14" width="8" height="36" rx="4" fill="url(#skinHello)" stroke="#c9a67a" strokeWidth="2" transform="rotate(-15 34 32)"/>
      <rect x="40" y="10" width="8" height="40" rx="4" fill="url(#skinHello)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="50" y="12" width="8" height="38" rx="4" fill="url(#skinHello)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="60" y="16" width="8" height="34" rx="4" fill="url(#skinHello)" stroke="#c9a67a" strokeWidth="2" transform="rotate(15 64 33)"/>
      <path d="M 35 50 Q 25 55 20 65" stroke="#c9a67a" strokeWidth="8" strokeLinecap="round" fill="none" opacity="0.6"/>
      <path d="M 30 85 Q 50 75 70 85" stroke="#4ade80" strokeWidth="3" fill="none" strokeDasharray="4,4">
        <animate attributeName="stroke-dashoffset" values="0;8" dur="0.5s" repeatCount="indefinite"/>
      </path>
    </svg>
  ),
  'THANK YOU': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinThank" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="58" rx="20" ry="24" fill="url(#skinThank)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="32" y="16" width="8" height="34" rx="4" fill="url(#skinThank)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="42" y="12" width="8" height="38" rx="4" fill="url(#skinThank)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="52" y="12" width="8" height="38" rx="4" fill="url(#skinThank)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="62" y="16" width="8" height="34" rx="4" fill="url(#skinThank)" stroke="#c9a67a" strokeWidth="2"/>
      <path d="M 50 75 L 65 90" stroke="#4ade80" strokeWidth="3" fill="none" markerEnd="url(#arrow)"/>
      <circle cx="50" cy="8" r="6" fill="#fef08a" stroke="#eab308" strokeWidth="1"/>
    </svg>
  ),
  'PLEASE': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinPlease" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="50" rx="28" ry="32" fill="url(#skinPlease)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="50" cy="50" rx="12" ry="14" fill="#fef3c7" stroke="#c9a67a" strokeWidth="1.5" opacity="0.7"/>
      <path d="M 30 50 Q 50 35 70 50 Q 50 65 30 50" fill="none" stroke="#4ade80" strokeWidth="2" strokeDasharray="4,4">
        <animate attributeName="stroke-dashoffset" values="0;-8" dur="1s" repeatCount="indefinite"/>
      </path>
    </svg>
  ),
  'SORRY': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinSorry" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="55" rx="22" ry="28" fill="url(#skinSorry)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="35" y="28" width="30" height="20" rx="4" fill="url(#skinSorry)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="50" cy="50" rx="14" ry="16" fill="#fee2e2" stroke="#c9a67a" strokeWidth="1" opacity="0.5"/>
      <path d="M 35 70 Q 50 80 65 70" stroke="#4ade80" strokeWidth="2" fill="none" strokeDasharray="3,3">
        <animate attributeName="stroke-dashoffset" values="0;6" dur="0.8s" repeatCount="indefinite"/>
      </path>
    </svg>
  ),
  'YES': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinYes" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="50" rx="22" ry="28" fill="url(#skinYes)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="35" y="22" width="30" height="22" rx="4" fill="url(#skinYes)" stroke="#c9a67a" strokeWidth="2"/>
      <path d="M 50 18 L 50 10 M 50 85 L 50 75" stroke="#4ade80" strokeWidth="3" strokeLinecap="round">
        <animate attributeName="opacity" values="1;0.5;1" dur="0.6s" repeatCount="indefinite"/>
      </path>
    </svg>
  ),
  'NO': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinNo" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="58" rx="20" ry="24" fill="url(#skinNo)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="40" y="16" width="9" height="36" rx="4" fill="url(#skinNo)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="52" y="16" width="9" height="36" rx="4" fill="url(#skinNo)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="32" cy="50" rx="9" ry="12" fill="url(#skinNo)" stroke="#c9a67a" strokeWidth="2"/>
      <line x1="35" y1="80" x2="65" y2="80" stroke="#ef4444" strokeWidth="3" strokeLinecap="round"/>
    </svg>
  ),
  'HELP': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinHelp" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="60" rx="26" ry="22" fill="url(#skinHelp)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="50" cy="35" rx="16" ry="18" fill="url(#skinHelp)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="50" cy="28" rx="8" ry="10" fill="url(#skinHelp)" stroke="#c9a67a" strokeWidth="2"/>
      <path d="M 50 55 L 50 35" stroke="#4ade80" strokeWidth="3" markerEnd="url(#arrow)">
        <animate attributeName="opacity" values="1;0.4;1" dur="0.8s" repeatCount="indefinite"/>
      </path>
    </svg>
  ),
  'LOVE': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinLove" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="35" cy="50" rx="20" ry="28" fill="url(#skinLove)" stroke="#c9a67a" strokeWidth="2" transform="rotate(-20 35 50)"/>
      <ellipse cx="65" cy="50" rx="20" ry="28" fill="url(#skinLove)" stroke="#c9a67a" strokeWidth="2" transform="rotate(20 65 50)"/>
      <path d="M 50 25 L 40 35 L 50 55 L 60 35 Z" fill="#f87171" stroke="#dc2626" strokeWidth="1.5"/>
    </svg>
  ),
  'FRIEND': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinFriend" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <rect x="20" y="40" width="28" height="12" rx="5" fill="url(#skinFriend)" stroke="#c9a67a" strokeWidth="2" transform="rotate(-10 34 46)"/>
      <rect x="52" y="40" width="28" height="12" rx="5" fill="url(#skinFriend)" stroke="#c9a67a" strokeWidth="2" transform="rotate(10 66 46)"/>
      <path d="M 45 46 Q 50 50 55 46" stroke="#c9a67a" strokeWidth="3" fill="none"/>
      <circle cx="50" cy="75" r="8" fill="#fef08a" stroke="#eab308" strokeWidth="1.5"/>
    </svg>
  ),
  'FAMILY': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinFamily" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <circle cx="30" cy="35" r="12" fill="url(#skinFamily)" stroke="#c9a67a" strokeWidth="2"/>
      <circle cx="70" cy="35" r="12" fill="url(#skinFamily)" stroke="#c9a67a" strokeWidth="2"/>
      <circle cx="50" cy="65" r="10" fill="url(#skinFamily)" stroke="#c9a67a" strokeWidth="2"/>
      <path d="M 30 48 L 30 55 L 50 55 L 70 55 L 70 48" stroke="#c9a67a" strokeWidth="2" fill="none"/>
      <path d="M 50 55 L 50 55" stroke="#c9a67a" strokeWidth="2"/>
    </svg>
  ),
  'EAT': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinEat" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="70" rx="20" ry="16" fill="url(#skinEat)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="38" y="38" width="8" height="28" rx="4" fill="url(#skinEat)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="48" y="35" width="8" height="31" rx="4" fill="url(#skinEat)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="58" y="38" width="8" height="28" rx="4" fill="url(#skinEat)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="50" cy="20" rx="16" ry="12" fill="#fca5a5" stroke="#f87171" strokeWidth="2"/>
    </svg>
  ),
  'DRINK': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinDrink" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="65" rx="18" ry="22" fill="url(#skinDrink)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="35" y="30" width="30" height="26" rx="4" fill="url(#skinDrink)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="28" cy="55" rx="10" ry="12" fill="url(#skinDrink)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="40" y="8" width="20" height="20" rx="3" fill="#93c5fd" stroke="#3b82f6" strokeWidth="2"/>
    </svg>
  ),
  'HAPPY': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinHappy" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="35" cy="55" rx="18" ry="22" fill="url(#skinHappy)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="65" cy="55" rx="18" ry="22" fill="url(#skinHappy)" stroke="#c9a67a" strokeWidth="2"/>
      <path d="M 35 40 L 35 25 M 65 40 L 65 25" stroke="#4ade80" strokeWidth="3" strokeLinecap="round">
        <animate attributeName="d" values="M 35 40 L 35 30 M 65 40 L 65 30;M 35 40 L 35 20 M 65 40 L 65 20;M 35 40 L 35 30 M 65 40 L 65 30" dur="0.8s" repeatCount="indefinite"/>
      </path>
      <ellipse cx="50" cy="20" rx="20" ry="12" fill="#fef08a" stroke="#eab308" strokeWidth="1.5"/>
    </svg>
  ),
  'SAD': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinSad" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="35" cy="45" rx="16" ry="20" fill="url(#skinSad)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="65" cy="45" rx="16" ry="20" fill="url(#skinSad)" stroke="#c9a67a" strokeWidth="2"/>
      <path d="M 35 55 L 35 75 M 65 55 L 65 75" stroke="#93c5fd" strokeWidth="3" strokeLinecap="round">
        <animate attributeName="opacity" values="1;0.3;1" dur="1s" repeatCount="indefinite"/>
      </path>
      <ellipse cx="50" cy="25" rx="18" ry="14" fill="#e5e7eb" stroke="#9ca3af" strokeWidth="1.5"/>
    </svg>
  ),
  'GOOD': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinGood" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="55" rx="22" ry="28" fill="url(#skinGood)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="35" y="25" width="30" height="24" rx="4" fill="url(#skinGood)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="30" cy="45" rx="10" ry="14" fill="url(#skinGood)" stroke="#c9a67a" strokeWidth="2"/>
      <path d="M 55 85 L 70 95" stroke="#4ade80" strokeWidth="3" strokeLinecap="round"/>
      <circle cx="50" cy="12" r="8" fill="#4ade80" stroke="#22c55e" strokeWidth="1.5"/>
    </svg>
  ),
  'BAD': (
    <svg viewBox="0 0 100 100" className="hand-svg phrase-svg">
      <defs>
        <linearGradient id="skinBad" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stopColor="#ffe4c4" />
          <stop offset="100%" stopColor="#dbb896" />
        </linearGradient>
      </defs>
      <ellipse cx="50" cy="55" rx="22" ry="28" fill="url(#skinBad)" stroke="#c9a67a" strokeWidth="2"/>
      <rect x="35" y="25" width="30" height="24" rx="4" fill="url(#skinBad)" stroke="#c9a67a" strokeWidth="2"/>
      <ellipse cx="30" cy="45" rx="10" ry="14" fill="url(#skinBad)" stroke="#c9a67a" strokeWidth="2" transform="rotate(180 30 55)"/>
      <path d="M 55 85 L 70 75" stroke="#ef4444" strokeWidth="3" strokeLinecap="round"/>
      <circle cx="50" cy="12" r="8" fill="#ef4444" stroke="#dc2626" strokeWidth="1.5"/>
    </svg>
  ),
};

const shuffleArray = (arr) => {
  const shuffled = [...arr];
  for (let i = shuffled.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    [shuffled[i], shuffled[j]] = [shuffled[j], shuffled[i]];
  }
  return shuffled;
};

const ASLGuessGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('menu'); // menu, countdown, playing, ended
  const [gameMode, setGameMode] = useState('all'); // all, letters, phrases
  const [countdown, setCountdown] = useState(3);
  const [timeLeft, setTimeLeft] = useState(GAME_DURATION / 1000);
  const [score, setScore] = useState(0);
  const [currentSign, setCurrentSign] = useState(null);
  const [options, setOptions] = useState([]);
  const [streak, setStreak] = useState(0);
  const [bestStreak, setBestStreak] = useState(0);
  const [feedback, setFeedback] = useState(null);
  const [questionsAnswered, setQuestionsAnswered] = useState(0);
  const [correctAnswers, setCorrectAnswers] = useState(0);
  const [showHint, setShowHint] = useState(false);
  const [difficulty, setDifficulty] = useState(1);
  const [particles, setParticles] = useState([]);
  const [shakeWrong, setShakeWrong] = useState(false);

  const scoreRef = useRef(0);
  const streakRef = useRef(0);
  const bestStreakRef = useRef(0);
  const correctRef = useRef(0);
  const questionsRef = useRef(0);
  const questionStartTimeRef = useRef(0);
  const usedSignsRef = useRef([]);
  const particleIdRef = useRef(0);

  // Create celebration particles
  const createParticles = useCallback((x, y, type) => {
    const colors = type === 'correct'
      ? ['#4ade80', '#22c55e', '#86efac', '#fef08a', '#fde047']
      : ['#f87171', '#ef4444', '#fca5a5'];

    const newParticles = [];
    for (let i = 0; i < 12; i++) {
      newParticles.push({
        id: particleIdRef.current++,
        x,
        y,
        vx: (Math.random() - 0.5) * 10,
        vy: -Math.random() * 8 - 4,
        color: colors[Math.floor(Math.random() * colors.length)],
        size: 6 + Math.random() * 8,
        life: 1,
      });
    }
    setParticles(prev => [...prev, ...newParticles]);
  }, []);

  // Update particles
  useEffect(() => {
    if (particles.length === 0) return;

    const interval = setInterval(() => {
      setParticles(prev => prev
        .map(p => ({
          ...p,
          x: p.x + p.vx,
          y: p.y + p.vy,
          vy: p.vy + 0.5,
          life: p.life - 0.04,
        }))
        .filter(p => p.life > 0)
      );
    }, 30);

    return () => clearInterval(interval);
  }, [particles.length]);

  // Get signs based on mode
  const getSignsByMode = useCallback(() => {
    if (gameMode === 'letters') return ASL_LETTERS;
    if (gameMode === 'phrases') return ASL_PHRASES;
    return ASL_SIGNS; // all
  }, [gameMode]);

  // Generate new question
  const generateQuestion = useCallback(() => {
    // Adjust difficulty based on performance
    if (correctRef.current > 8 && questionsRef.current > 10) {
      setDifficulty(3);
    } else if (correctRef.current > 4 && questionsRef.current > 5) {
      setDifficulty(2);
    }

    const signsPool = getSignsByMode();
    let availableSigns = signsPool.filter(s =>
      !usedSignsRef.current.includes(s.letter) &&
      s.difficulty <= difficulty + 1
    );

    // Reset if we've used too many signs
    if (availableSigns.length < 4) {
      usedSignsRef.current = [];
      availableSigns = signsPool.filter(s => s.difficulty <= difficulty + 1);
    }

    const correctSign = availableSigns[Math.floor(Math.random() * availableSigns.length)];
    usedSignsRef.current.push(correctSign.letter);

    // Get 3 wrong options from the same mode (prefer similar difficulty and type)
    const wrongOptions = shuffleArray(
      signsPool.filter(s =>
        s.letter !== correctSign.letter &&
        Math.abs(s.difficulty - correctSign.difficulty) <= 1
      )
    ).slice(0, 3);

    const allOptions = shuffleArray([correctSign, ...wrongOptions]);

    setCurrentSign(correctSign);
    setOptions(allOptions);
    setFeedback(null);
    setShowHint(false);
    questionStartTimeRef.current = Date.now();
  }, [difficulty, getSignsByMode]);

  // Initialize game
  useEffect(() => {
    if (gameState === 'playing') {
      generateQuestion();
    }
  }, [gameState, generateQuestion]);

  // Countdown
  useEffect(() => {
    if (gameState !== 'countdown') return;
    if (countdown > 0) {
      const timer = setTimeout(() => setCountdown(c => c - 1), 1000);
      return () => clearTimeout(timer);
    } else {
      setGameState('playing');
    }
  }, [countdown, gameState]);

  // Game timer
  useEffect(() => {
    if (gameState !== 'playing') return;
    const timer = setInterval(() => {
      setTimeLeft(prev => {
        if (prev <= 1) {
          setGameState('ended');
          return 0;
        }
        return prev - 1;
      });
    }, 1000);
    return () => clearInterval(timer);
  }, [gameState]);

  // Handle answer selection
  const handleAnswer = useCallback((selectedLetter, event) => {
    if (feedback) return; // Prevent multiple clicks

    const isCorrect = selectedLetter === currentSign.letter;
    const responseTime = Date.now() - questionStartTimeRef.current;

    questionsRef.current += 1;
    setQuestionsAnswered(questionsRef.current);

    if (event) {
      const rect = event.target.getBoundingClientRect();
      createParticles(rect.left + rect.width / 2, rect.top, isCorrect ? 'correct' : 'wrong');
    }

    if (isCorrect) {
      correctRef.current += 1;
      setCorrectAnswers(correctRef.current);

      streakRef.current += 1;
      setStreak(streakRef.current);

      if (streakRef.current > bestStreakRef.current) {
        bestStreakRef.current = streakRef.current;
        setBestStreak(streakRef.current);
      }

      // Calculate points
      let points = COINS_PER_CORRECT;
      const isFast = responseTime < TIME_BONUS_THRESHOLD;
      const isPerfectStreak = streakRef.current >= 5 && streakRef.current % 5 === 0;

      if (streakRef.current >= 3) {
        points += STREAK_BONUS;
      }
      if (isFast) {
        points += 5; // Speed bonus
      }
      if (isPerfectStreak) {
        points += PERFECT_STREAK_BONUS;
      }

      // Difficulty bonus
      points += (currentSign.difficulty - 1) * 3;

      scoreRef.current += points;
      setScore(scoreRef.current);
      setFeedback({
        correct: true,
        points,
        fast: isFast,
        perfectStreak: isPerfectStreak
      });
    } else {
      streakRef.current = 0;
      setStreak(0);
      setFeedback({ correct: false, correctAnswer: currentSign.letter });
      setShakeWrong(true);
      setTimeout(() => setShakeWrong(false), 500);
    }

    // Next question after delay
    setTimeout(() => {
      generateQuestion();
    }, 1500);
  }, [currentSign, feedback, generateQuestion, createParticles]);

  const handleFinish = useCallback(() => {
    onGameEnd(scoreRef.current);
  }, [onGameEnd]);

  const startGame = useCallback((mode) => {
    setGameMode(mode);
    setGameState('countdown');
    setCountdown(3);
    setTimeLeft(GAME_DURATION / 1000);
    setScore(0);
    scoreRef.current = 0;
    setStreak(0);
    streakRef.current = 0;
    setBestStreak(0);
    bestStreakRef.current = 0;
    setCorrectAnswers(0);
    correctRef.current = 0;
    setQuestionsAnswered(0);
    questionsRef.current = 0;
    usedSignsRef.current = [];
    setDifficulty(1);
  }, []);

  const getDifficultyStars = (diff) => {
    return '⭐'.repeat(diff);
  };

  return (
    <div className={`asl-game-overlay ${shakeWrong ? 'shake-wrong' : ''}`}>
      {/* Particles */}
      <div className="particles-container">
        {particles.map(p => (
          <div
            key={p.id}
            className="particle"
            style={{
              left: p.x,
              top: p.y,
              width: p.size,
              height: p.size,
              backgroundColor: p.color,
              opacity: p.life,
            }}
          />
        ))}
      </div>

      <div className="asl-game-container">
        {/* Menu */}
        {gameState === 'menu' && (
          <motion.div
            className="asl-menu"
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
          >
            <h1 className="asl-title">
              <span>🤟</span> Guess the Sign <span>🤟</span>
            </h1>
            <p className="asl-subtitle">Learn ASL letters and phrases!</p>

            <div className="mode-selection">
              <h3>Choose a Mode</h3>
              <div className="mode-buttons">
                <motion.button
                  className="mode-btn letters"
                  onClick={() => startGame('letters')}
                  whileHover={{ scale: 1.02 }}
                  whileTap={{ scale: 0.98 }}
                >
                  <span className="mode-icon">🔤</span>
                  <span className="mode-name">Letters Only</span>
                  <span className="mode-desc">A-Z finger spelling</span>
                </motion.button>

                <motion.button
                  className="mode-btn phrases"
                  onClick={() => startGame('phrases')}
                  whileHover={{ scale: 1.02 }}
                  whileTap={{ scale: 0.98 }}
                >
                  <span className="mode-icon">💬</span>
                  <span className="mode-name">Phrases Only</span>
                  <span className="mode-desc">Common words & expressions</span>
                </motion.button>

                <motion.button
                  className="mode-btn all"
                  onClick={() => startGame('all')}
                  whileHover={{ scale: 1.02 }}
                  whileTap={{ scale: 0.98 }}
                >
                  <span className="mode-icon">⭐</span>
                  <span className="mode-name">Mixed Mode</span>
                  <span className="mode-desc">Letters + Phrases (Recommended)</span>
                </motion.button>
              </div>
            </div>

            <button className="close-button-menu" onClick={onClose}>
              Back to Games
            </button>
          </motion.div>
        )}

        {/* Header */}
        {gameState !== 'menu' && (
        <div className="game-header">
          <div className="game-stat">
            <span className="stat-icon">🪙</span>
            <motion.span
              className="stat-value"
              key={score}
              initial={{ scale: 1.3 }}
              animate={{ scale: 1 }}
            >
              {score}
            </motion.span>
          </div>
          <div className="game-timer">
            <span className="timer-icon">⏱️</span>
            <span className={`timer-value ${timeLeft <= 10 ? 'warning' : ''}`}>
              {timeLeft}s
            </span>
          </div>
          <div className="game-stat streak-stat">
            <span className="stat-icon">🔥</span>
            <motion.span
              className="stat-value"
              key={streak}
              initial={{ scale: streak > 0 ? 1.5 : 1 }}
              animate={{ scale: 1 }}
            >
              {streak}
            </motion.span>
          </div>
        </div>
        )}

        {/* Countdown */}
        <AnimatePresence>
          {gameState === 'countdown' && (
            <motion.div
              className="countdown-display"
              key={countdown}
              initial={{ scale: 0, opacity: 0 }}
              animate={{ scale: 1, opacity: 1 }}
              exit={{ scale: 2, opacity: 0 }}
            >
              {countdown > 0 ? countdown : 'GO!'}
            </motion.div>
          )}
        </AnimatePresence>

        {/* Main Game Area */}
        {gameState === 'playing' && currentSign && (
          <motion.div
            className="game-content"
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
          >
            {/* Sign Display */}
            <motion.div
              className={`sign-display ${feedback ? (feedback.correct ? 'correct' : 'wrong') : ''}`}
              animate={feedback?.correct ? { scale: [1, 1.05, 1] } : {}}
              transition={{ duration: 0.3 }}
            >
              <div className="sign-label">
                {currentSign.type === 'phrase' ? 'What does this sign mean?' : 'What letter is this sign?'}
              </div>
              <div className="difficulty-indicator">
                {getDifficultyStars(currentSign.difficulty)}
              </div>
              <motion.div
                className="hand-display"
                initial={{ rotateY: 90 }}
                animate={{ rotateY: 0 }}
                transition={{ duration: 0.4, type: 'spring' }}
              >
                {HAND_ILLUSTRATIONS[currentSign.letter]}
              </motion.div>
              <button
                className="hint-button"
                onClick={() => setShowHint(!showHint)}
              >
                {showHint ? '🙈 Hide Hint' : '💡 Show Hint'}
              </button>
              <AnimatePresence>
                {showHint && (
                  <motion.div
                    className="hint-text"
                    initial={{ opacity: 0, height: 0 }}
                    animate={{ opacity: 1, height: 'auto' }}
                    exit={{ opacity: 0, height: 0 }}
                  >
                    {currentSign.description}
                  </motion.div>
                )}
              </AnimatePresence>
            </motion.div>

            {/* Feedback */}
            <AnimatePresence>
              {feedback && (
                <motion.div
                  className={`feedback-banner ${feedback.correct ? 'correct' : 'wrong'}`}
                  initial={{ y: -30, opacity: 0 }}
                  animate={{ y: 0, opacity: 1 }}
                  exit={{ y: -30, opacity: 0 }}
                >
                  {feedback.correct ? (
                    <>
                      <span className="feedback-icon">✨</span>
                      <span>Correct! +{feedback.points}</span>
                      {feedback.fast && <span className="speed-bonus">⚡ Fast!</span>}
                      {feedback.perfectStreak && <span className="perfect-bonus">🏆 Perfect 5!</span>}
                    </>
                  ) : (
                    <>
                      <span className="feedback-icon">❌</span>
                      <span>It was "{feedback.correctAnswer}"</span>
                    </>
                  )}
                </motion.div>
              )}
            </AnimatePresence>

            {/* Answer Options */}
            <div className="options-grid">
              {options.map((option, index) => (
                <motion.button
                  key={option.letter}
                  className={`option-button ${
                    feedback
                      ? option.letter === currentSign.letter
                        ? 'correct'
                        : feedback.correct
                          ? ''
                          : 'faded'
                      : ''
                  }`}
                  onClick={(e) => handleAnswer(option.letter, e)}
                  disabled={!!feedback}
                  initial={{ opacity: 0, scale: 0.8 }}
                  animate={{ opacity: 1, scale: 1 }}
                  transition={{ delay: index * 0.1 }}
                  whileHover={!feedback ? { scale: 1.05 } : {}}
                  whileTap={!feedback ? { scale: 0.95 } : {}}
                >
                  <span className="option-letter">{option.letter}</span>
                </motion.button>
              ))}
            </div>

            {/* Progress */}
            <div className="progress-bar">
              <div className="progress-text">
                {correctAnswers}/{questionsAnswered} correct
                {bestStreak >= 3 && <span className="best-streak"> • Best: {bestStreak}🔥</span>}
              </div>
            </div>
          </motion.div>
        )}

        {/* Game Over Screen */}
        <AnimatePresence>
          {gameState === 'ended' && (
            <motion.div
              className="game-over-screen"
              initial={{ opacity: 0 }}
              animate={{ opacity: 1 }}
              exit={{ opacity: 0 }}
            >
              <motion.div
                className="game-over-content"
                initial={{ scale: 0.8, y: 50 }}
                animate={{ scale: 1, y: 0 }}
                transition={{ type: 'spring', damping: 15 }}
              >
                <h2 className="game-over-title">
                  <motion.span
                    animate={{ rotate: [-10, 10, -10] }}
                    transition={{ duration: 0.5, repeat: Infinity }}
                  >
                    🤟
                  </motion.span>
                  {' '}Great Signing!{' '}
                  <motion.span
                    animate={{ rotate: [10, -10, 10] }}
                    transition={{ duration: 0.5, repeat: Infinity }}
                  >
                    🤟
                  </motion.span>
                </h2>

                <div className="final-stats">
                  <motion.div
                    className="final-stat"
                    initial={{ x: -30, opacity: 0 }}
                    animate={{ x: 0, opacity: 1 }}
                    transition={{ delay: 0.2 }}
                  >
                    <span className="final-stat-icon">✅</span>
                    <span className="final-stat-label">Accuracy</span>
                    <span className="final-stat-value">
                      {questionsAnswered > 0
                        ? Math.round((correctAnswers / questionsAnswered) * 100)
                        : 0}%
                    </span>
                  </motion.div>

                  <motion.div
                    className="final-stat"
                    initial={{ x: -30, opacity: 0 }}
                    animate={{ x: 0, opacity: 1 }}
                    transition={{ delay: 0.3 }}
                  >
                    <span className="final-stat-icon">🔥</span>
                    <span className="final-stat-label">Best Streak</span>
                    <span className="final-stat-value">{bestStreak}</span>
                  </motion.div>

                  <motion.div
                    className="final-stat highlight"
                    initial={{ x: -30, opacity: 0 }}
                    animate={{ x: 0, opacity: 1 }}
                    transition={{ delay: 0.4 }}
                  >
                    <span className="final-stat-icon">🪙</span>
                    <span className="final-stat-label">Total Score</span>
                    <motion.span
                      className="final-stat-value coins"
                      animate={{ scale: [1, 1.1, 1] }}
                      transition={{ duration: 0.5, repeat: 3 }}
                    >
                      {score}
                    </motion.span>
                  </motion.div>
                </div>

                {correctAnswers >= 10 && (
                  <motion.div
                    className="achievement-badge"
                    initial={{ scale: 0 }}
                    animate={{ scale: 1 }}
                    transition={{ delay: 0.5, type: 'spring' }}
                  >
                    <span>🤟</span> ASL Expert! <span>🤟</span>
                  </motion.div>
                )}

                <motion.button
                  className="claim-rewards-btn cute-button"
                  onClick={handleFinish}
                  whileHover={{ scale: 1.05 }}
                  whileTap={{ scale: 0.95 }}
                >
                  Claim {score} Coins! 🪙
                </motion.button>
              </motion.div>
            </motion.div>
          )}
        </AnimatePresence>

        {/* Instructions */}
        {gameState === 'countdown' && countdown === 3 && (
          <motion.div
            className="game-instructions"
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
          >
            <p>Guess the ASL letter from the hand sign!</p>
            <p className="hint">Answer fast for bonus points! 🤟</p>
          </motion.div>
        )}

        {/* Close button */}
        {gameState === 'countdown' && (
          <button className="game-close-btn" onClick={onClose}>
            ✕
          </button>
        )}
      </div>
    </div>
  );
};

export default ASLGuessGame;
