import React, { useMemo, memo } from 'react';

// Skin tone palette - realistic human skin tones
export const SKIN_TONES = [
  { id: 'fair', color: '#FDEBD0', shadow: '#E5C8A8', highlight: '#FFF5E6' },
  { id: 'light', color: '#F5DEB3', shadow: '#D4B896', highlight: '#FFF0D4' },
  { id: 'medium', color: '#DEB887', shadow: '#C19A6B', highlight: '#EED9B6' },
  { id: 'olive', color: '#C4A77D', shadow: '#A68A5B', highlight: '#D9C4A5' },
  { id: 'tan', color: '#B87333', shadow: '#8B5A2B', highlight: '#D4915C' },
  { id: 'brown', color: '#8B5A2B', shadow: '#654321', highlight: '#A0724B' },
  { id: 'dark', color: '#654321', shadow: '#3D2914', highlight: '#7D5A3C' },
  { id: 'deep', color: '#4A2C2A', shadow: '#2D1A19', highlight: '#5D3D3B' },
];

// Hair colors
export const HAIR_COLORS = [
  { id: 'black', color: '#1a1a1a', highlight: '#333' },
  { id: 'dark_brown', color: '#3D2314', highlight: '#5C3A2D' },
  { id: 'brown', color: '#6B4423', highlight: '#8B6344' },
  { id: 'light_brown', color: '#A67B5B', highlight: '#C49B7B' },
  { id: 'auburn', color: '#922724', highlight: '#B24743' },
  { id: 'red', color: '#B7410E', highlight: '#D7612E' },
  { id: 'blonde', color: '#E6BE8A', highlight: '#F0D4AA' },
  { id: 'platinum', color: '#E8E4C9', highlight: '#F5F2E0' },
  { id: 'gray', color: '#808080', highlight: '#A0A0A0' },
  { id: 'white', color: '#F0F0F0', highlight: '#FFFFFF' },
  { id: 'pink', color: '#FF69B4', highlight: '#FF8DC7' },
  { id: 'blue', color: '#4169E1', highlight: '#6B8DF5' },
  { id: 'purple', color: '#8B008B', highlight: '#AB29AB' },
  { id: 'green', color: '#228B22', highlight: '#42AB42' },
];

// Eye colors
export const EYE_COLORS = [
  { id: 'brown', color: '#634e34', inner: '#8B6914' },
  { id: 'hazel', color: '#5E7D3A', inner: '#7D9D5A' },
  { id: 'green', color: '#3D8B37', inner: '#5DAB57' },
  { id: 'blue', color: '#3B83BD', inner: '#5BA3DD' },
  { id: 'gray', color: '#6E7B8B', inner: '#8E9BAB' },
  { id: 'amber', color: '#FFBF00', inner: '#FFD740' },
];

// Hair styles data
export const HAIR_STYLES = {
  male: [
    { id: 'short_neat', name: 'Short & Neat', premium: false },
    { id: 'buzz_cut', name: 'Buzz Cut', premium: false },
    { id: 'side_part', name: 'Side Part', premium: false },
    { id: 'quiff', name: 'Quiff', premium: false },
    { id: 'textured_crop', name: 'Textured Crop', premium: false },
    { id: 'slick_back', name: 'Slick Back', premium: true, price: 500 },
    { id: 'mohawk', name: 'Mohawk', premium: true, price: 800 },
    { id: 'long_wavy', name: 'Long & Wavy', premium: true, price: 600 },
    { id: 'man_bun', name: 'Man Bun', premium: true, price: 700 },
    { id: 'afro', name: 'Afro', premium: true, price: 900 },
  ],
  female: [
    { id: 'long_straight', name: 'Long & Straight', premium: false },
    { id: 'bob', name: 'Bob Cut', premium: false },
    { id: 'pixie', name: 'Pixie Cut', premium: false },
    { id: 'ponytail', name: 'Ponytail', premium: false },
    { id: 'wavy', name: 'Wavy', premium: false },
    { id: 'curly', name: 'Curly', premium: true, price: 500 },
    { id: 'braids', name: 'Braids', premium: true, price: 800 },
    { id: 'bun', name: 'Bun', premium: true, price: 400 },
    { id: 'twin_tails', name: 'Twin Tails', premium: true, price: 600 },
    { id: 'messy', name: 'Messy', premium: true, price: 500 },
  ],
  neutral: [
    { id: 'short_neat', name: 'Short & Neat', premium: false },
    { id: 'medium_length', name: 'Medium Length', premium: false },
    { id: 'shaggy', name: 'Shaggy', premium: false },
    { id: 'undercut', name: 'Undercut', premium: true, price: 600 },
    { id: 'curly_short', name: 'Curly Short', premium: true, price: 500 },
  ],
};

// Face shapes
export const FACE_SHAPES = [
  { id: 'oval', name: 'Oval', premium: false },
  { id: 'round', name: 'Round', premium: false },
  { id: 'square', name: 'Square', premium: false },
  { id: 'heart', name: 'Heart', premium: false },
  { id: 'oblong', name: 'Oblong', premium: false },
];

// Eye styles
export const EYE_STYLES = [
  { id: 'normal', name: 'Normal', premium: false },
  { id: 'almond', name: 'Almond', premium: false },
  { id: 'round', name: 'Round', premium: false },
  { id: 'hooded', name: 'Hooded', premium: false },
  { id: 'upturned', name: 'Upturned', premium: false },
  { id: 'downturned', name: 'Downturned', premium: false },
];

// Eyebrow styles
export const EYEBROW_STYLES = [
  { id: 'natural', name: 'Natural', premium: false },
  { id: 'arched', name: 'Arched', premium: false },
  { id: 'straight', name: 'Straight', premium: false },
  { id: 'thick', name: 'Thick', premium: false },
  { id: 'thin', name: 'Thin', premium: false },
  { id: 'rounded', name: 'Rounded', premium: false },
];

// Nose styles
export const NOSE_STYLES = [
  { id: 'normal', name: 'Normal', premium: false },
  { id: 'small', name: 'Small', premium: false },
  { id: 'button', name: 'Button', premium: false },
  { id: 'pointed', name: 'Pointed', premium: false },
  { id: 'wide', name: 'Wide', premium: false },
];

// Mouth styles
export const MOUTH_STYLES = [
  { id: 'smile', name: 'Smile', premium: false },
  { id: 'neutral', name: 'Neutral', premium: false },
  { id: 'small', name: 'Small', premium: false },
  { id: 'full', name: 'Full Lips', premium: false },
  { id: 'thin', name: 'Thin Lips', premium: false },
];

// Render hair based on style
const renderHair = (style, hairColor, gender) => {
  const color = HAIR_COLORS.find(c => c.id === hairColor) || HAIR_COLORS[0];

  const hairPaths = {
    // Male styles
    short_neat: (
      <g className="hair">
        <ellipse cx="100" cy="45" rx="55" ry="30" fill={color.color} />
        <path d="M 50 60 Q 50 30, 100 25 Q 150 30, 150 60" fill={color.color} />
        <path d="M 55 55 Q 55 35, 100 30 Q 145 35, 145 55" fill={color.highlight} opacity="0.3" />
      </g>
    ),
    buzz_cut: (
      <g className="hair">
        <ellipse cx="100" cy="50" rx="50" ry="28" fill={color.color} />
        <ellipse cx="100" cy="48" rx="45" ry="24" fill={color.highlight} opacity="0.2" />
      </g>
    ),
    side_part: (
      <g className="hair">
        <path d="M 45 70 Q 40 40, 70 30 Q 100 25, 130 30 Q 155 40, 155 65 L 155 75 Q 100 60, 45 70" fill={color.color} />
        <path d="M 50 65 Q 50 40, 80 32 L 70 55 Q 50 60, 50 65" fill={color.highlight} opacity="0.3" />
      </g>
    ),
    quiff: (
      <g className="hair">
        <path d="M 50 70 Q 45 50, 60 35 Q 90 15, 120 35 Q 140 50, 150 70" fill={color.color} />
        <path d="M 70 25 Q 90 10, 110 25 Q 100 35, 70 25" fill={color.color} />
        <path d="M 75 30 Q 90 18, 105 30" fill={color.highlight} opacity="0.3" />
      </g>
    ),
    textured_crop: (
      <g className="hair">
        <ellipse cx="100" cy="48" rx="52" ry="32" fill={color.color} />
        <path d="M 55 55 Q 70 30, 100 28 Q 130 30, 145 55" fill={color.color} />
        <path d="M 65 40 L 70 50 M 80 35 L 85 48 M 95 32 L 100 45 M 115 35 L 110 48 M 130 40 L 125 50"
              stroke={color.highlight} strokeWidth="3" opacity="0.4" />
      </g>
    ),
    slick_back: (
      <g className="hair">
        <path d="M 48 75 Q 45 50, 60 35 Q 100 20, 140 35 Q 155 50, 152 75 Q 100 65, 48 75" fill={color.color} />
        <path d="M 55 60 Q 60 40, 100 30 Q 140 40, 145 60" fill="none" stroke={color.highlight} strokeWidth="2" opacity="0.4" />
      </g>
    ),
    mohawk: (
      <g className="hair">
        <path d="M 80 80 Q 75 50, 85 20 Q 100 5, 115 20 Q 125 50, 120 80 Q 100 75, 80 80" fill={color.color} />
        <path d="M 90 75 Q 85 50, 95 25 Q 100 15, 105 25 Q 115 50, 110 75" fill={color.highlight} opacity="0.3" />
      </g>
    ),
    long_wavy: (
      <g className="hair">
        <path d="M 40 180 Q 30 100, 50 50 Q 70 25, 100 22 Q 130 25, 150 50 Q 170 100, 160 180 Q 140 170, 100 175 Q 60 170, 40 180" fill={color.color} />
        <path d="M 50 60 Q 70 30, 100 28 Q 130 30, 150 60" fill={color.color} />
        <path d="M 55 70 Q 50 120, 55 160 M 145 70 Q 150 120, 145 160" stroke={color.highlight} strokeWidth="4" opacity="0.3" />
      </g>
    ),
    man_bun: (
      <g className="hair">
        <ellipse cx="100" cy="50" rx="52" ry="30" fill={color.color} />
        <circle cx="100" cy="25" r="18" fill={color.color} />
        <ellipse cx="100" cy="23" rx="14" ry="12" fill={color.highlight} opacity="0.2" />
      </g>
    ),
    afro: (
      <g className="hair">
        <ellipse cx="100" cy="55" rx="70" ry="55" fill={color.color} />
        <ellipse cx="100" cy="50" rx="65" ry="50" fill={color.highlight} opacity="0.15" />
        {/* Texture */}
        <circle cx="60" cy="40" r="5" fill={color.highlight} opacity="0.2" />
        <circle cx="140" cy="40" r="5" fill={color.highlight} opacity="0.2" />
        <circle cx="100" cy="25" r="5" fill={color.highlight} opacity="0.2" />
      </g>
    ),

    // Female styles
    long_straight: (
      <g className="hair">
        <path d="M 35 200 Q 30 100, 50 55 Q 70 30, 100 25 Q 130 30, 150 55 Q 170 100, 165 200 L 155 200 Q 150 120, 145 80 Q 100 70, 55 80 Q 50 120, 45 200 Z" fill={color.color} />
        <path d="M 50 60 Q 70 32, 100 28 Q 130 32, 150 60" fill={color.color} />
        <path d="M 60 80 L 55 180 M 140 80 L 145 180" stroke={color.highlight} strokeWidth="5" opacity="0.3" />
      </g>
    ),
    bob: (
      <g className="hair">
        <path d="M 40 130 Q 35 80, 55 50 Q 75 30, 100 27 Q 125 30, 145 50 Q 165 80, 160 130 Q 130 140, 100 142 Q 70 140, 40 130" fill={color.color} />
        <path d="M 50 55 Q 75 32, 100 30 Q 125 32, 150 55" fill={color.color} />
        <path d="M 55 65 Q 50 100, 50 120" stroke={color.highlight} strokeWidth="4" opacity="0.3" />
      </g>
    ),
    pixie: (
      <g className="hair">
        <ellipse cx="100" cy="48" rx="55" ry="32" fill={color.color} />
        <path d="M 45 60 Q 40 40, 60 30 Q 80 22, 100 20 Q 120 22, 140 30 Q 155 45, 155 65" fill={color.color} />
        <path d="M 55 50 Q 60 35, 90 30" fill={color.highlight} opacity="0.3" />
      </g>
    ),
    ponytail: (
      <g className="hair">
        <ellipse cx="100" cy="48" rx="52" ry="30" fill={color.color} />
        <path d="M 50 60 Q 50 38, 100 32 Q 150 38, 150 60" fill={color.color} />
        {/* Ponytail */}
        <ellipse cx="100" cy="35" rx="12" ry="8" fill={color.color} />
        <path d="M 88 35 Q 85 60, 90 120 Q 100 140, 110 120 Q 115 60, 112 35" fill={color.color} />
        <path d="M 95 45 Q 95 80, 100 110" stroke={color.highlight} strokeWidth="3" opacity="0.3" />
      </g>
    ),
    wavy: (
      <g className="hair">
        <path d="M 35 180 Q 25 100, 50 55 Q 70 30, 100 25 Q 130 30, 150 55 Q 175 100, 165 180 Q 130 170, 100 175 Q 70 170, 35 180" fill={color.color} />
        <path d="M 48 58 Q 70 32, 100 28 Q 130 32, 152 58" fill={color.color} />
        <path d="M 45 80 Q 40 100, 45 130 Q 40 150, 45 170" stroke={color.highlight} strokeWidth="4" opacity="0.3" />
        <path d="M 155 80 Q 160 100, 155 130 Q 160 150, 155 170" stroke={color.highlight} strokeWidth="4" opacity="0.3" />
      </g>
    ),
    curly: (
      <g className="hair">
        <path d="M 30 170 Q 20 100, 50 55 Q 75 28, 100 25 Q 125 28, 150 55 Q 180 100, 170 170 Q 130 165, 100 170 Q 70 165, 30 170" fill={color.color} />
        <path d="M 48 58 Q 75 30, 100 27 Q 125 30, 152 58" fill={color.color} />
        {/* Curly texture */}
        <circle cx="45" cy="90" r="8" fill={color.highlight} opacity="0.2" />
        <circle cx="155" cy="90" r="8" fill={color.highlight} opacity="0.2" />
        <circle cx="40" cy="130" r="7" fill={color.highlight} opacity="0.2" />
        <circle cx="160" cy="130" r="7" fill={color.highlight} opacity="0.2" />
      </g>
    ),
    braids: (
      <g className="hair">
        <ellipse cx="100" cy="48" rx="52" ry="30" fill={color.color} />
        {/* Left braid */}
        <path d="M 55 60 Q 45 100, 50 160 Q 55 180, 45 200" fill="none" stroke={color.color} strokeWidth="12" strokeLinecap="round" />
        <path d="M 55 70 L 50 85 L 55 100 L 50 115 L 55 130 L 50 145 L 55 160" stroke={color.highlight} strokeWidth="2" opacity="0.4" />
        {/* Right braid */}
        <path d="M 145 60 Q 155 100, 150 160 Q 145 180, 155 200" fill="none" stroke={color.color} strokeWidth="12" strokeLinecap="round" />
        <path d="M 145 70 L 150 85 L 145 100 L 150 115 L 145 130 L 150 145 L 145 160" stroke={color.highlight} strokeWidth="2" opacity="0.4" />
      </g>
    ),
    bun: (
      <g className="hair">
        <ellipse cx="100" cy="50" rx="52" ry="30" fill={color.color} />
        <path d="M 50 60 Q 50 38, 100 32 Q 150 38, 150 60" fill={color.color} />
        {/* Bun */}
        <ellipse cx="100" cy="30" rx="22" ry="18" fill={color.color} />
        <ellipse cx="100" cy="28" rx="18" ry="14" fill={color.highlight} opacity="0.2" />
      </g>
    ),
    twin_tails: (
      <g className="hair">
        <ellipse cx="100" cy="50" rx="52" ry="30" fill={color.color} />
        <path d="M 50 60 Q 50 38, 100 32 Q 150 38, 150 60" fill={color.color} />
        {/* Left tail */}
        <circle cx="55" cy="55" r="10" fill={color.color} />
        <path d="M 55 65 Q 45 120, 50 180" fill="none" stroke={color.color} strokeWidth="14" strokeLinecap="round" />
        {/* Right tail */}
        <circle cx="145" cy="55" r="10" fill={color.color} />
        <path d="M 145 65 Q 155 120, 150 180" fill="none" stroke={color.color} strokeWidth="14" strokeLinecap="round" />
      </g>
    ),
    messy: (
      <g className="hair">
        <path d="M 40 170 Q 25 100, 50 55 Q 75 25, 100 22 Q 125 25, 150 55 Q 175 100, 160 170" fill={color.color} />
        <path d="M 48 58 Q 75 28, 100 25 Q 125 28, 152 58" fill={color.color} />
        {/* Messy strands */}
        <path d="M 45 50 Q 30 35, 35 55" stroke={color.color} strokeWidth="6" fill="none" />
        <path d="M 155 50 Q 170 35, 165 55" stroke={color.color} strokeWidth="6" fill="none" />
        <path d="M 80 30 Q 75 15, 85 20" stroke={color.color} strokeWidth="5" fill="none" />
        <path d="M 120 30 Q 125 15, 115 20" stroke={color.color} strokeWidth="5" fill="none" />
      </g>
    ),

    // Neutral styles
    medium_length: (
      <g className="hair">
        <path d="M 42 130 Q 35 80, 55 50 Q 75 28, 100 25 Q 125 28, 145 50 Q 165 80, 158 130 Q 130 125, 100 128 Q 70 125, 42 130" fill={color.color} />
        <path d="M 50 55 Q 75 30, 100 27 Q 125 30, 150 55" fill={color.color} />
      </g>
    ),
    shaggy: (
      <g className="hair">
        <path d="M 40 120 Q 30 70, 55 45 Q 80 25, 100 22 Q 120 25, 145 45 Q 170 70, 160 120" fill={color.color} />
        <path d="M 48 55 Q 75 28, 100 25 Q 125 28, 152 55" fill={color.color} />
        {/* Shaggy strands */}
        <path d="M 50 70 Q 40 90, 45 110" stroke={color.color} strokeWidth="8" fill="none" strokeLinecap="round" />
        <path d="M 150 70 Q 160 90, 155 110" stroke={color.color} strokeWidth="8" fill="none" strokeLinecap="round" />
        <path d="M 65 55 Q 55 75, 60 95" stroke={color.color} strokeWidth="6" fill="none" strokeLinecap="round" />
        <path d="M 135 55 Q 145 75, 140 95" stroke={color.color} strokeWidth="6" fill="none" strokeLinecap="round" />
      </g>
    ),
    undercut: (
      <g className="hair">
        <ellipse cx="100" cy="50" rx="50" ry="28" fill={color.color} opacity="0.4" />
        <path d="M 55 55 Q 55 35, 90 28 Q 100 22, 110 28 Q 145 35, 145 55" fill={color.color} />
        <path d="M 60 50 Q 80 25, 100 22 Q 120 25, 140 50 L 130 60 Q 100 55, 70 60 Z" fill={color.color} />
      </g>
    ),
    curly_short: (
      <g className="hair">
        <ellipse cx="100" cy="48" rx="55" ry="35" fill={color.color} />
        {/* Curly texture */}
        <circle cx="65" cy="40" r="7" fill={color.highlight} opacity="0.2" />
        <circle cx="135" cy="40" r="7" fill={color.highlight} opacity="0.2" />
        <circle cx="100" cy="30" r="8" fill={color.highlight} opacity="0.2" />
        <circle cx="80" cy="55" r="6" fill={color.highlight} opacity="0.2" />
        <circle cx="120" cy="55" r="6" fill={color.highlight} opacity="0.2" />
      </g>
    ),
  };

  return hairPaths[style] || hairPaths.short_neat;
};

// Render eyes based on style
const renderEyes = (style, eyeColor, skinTone) => {
  const color = EYE_COLORS.find(c => c.id === eyeColor) || EYE_COLORS[0];
  const skin = SKIN_TONES.find(s => s.id === skinTone) || SKIN_TONES[0];

  const eyeBase = {
    normal: { width: 16, height: 12, y: 95 },
    almond: { width: 18, height: 10, y: 95 },
    round: { width: 14, height: 14, y: 95 },
    hooded: { width: 16, height: 10, y: 96 },
    upturned: { width: 16, height: 11, y: 94 },
    downturned: { width: 16, height: 11, y: 96 },
  };

  const eye = eyeBase[style] || eyeBase.normal;

  return (
    <g className="eyes">
      {/* Left eye */}
      <ellipse cx="75" cy={eye.y} rx={eye.width} ry={eye.height} fill="white" />
      <ellipse cx="75" cy={eye.y} rx={eye.width - 2} ry={eye.height - 1} fill="white" />
      <circle cx="75" cy={eye.y} r="6" fill={color.color} />
      <circle cx="75" cy={eye.y} r="3" fill="#000" />
      <circle cx="77" cy={eye.y - 2} r="1.5" fill="white" />

      {/* Right eye */}
      <ellipse cx="125" cy={eye.y} rx={eye.width} ry={eye.height} fill="white" />
      <ellipse cx="125" cy={eye.y} rx={eye.width - 2} ry={eye.height - 1} fill="white" />
      <circle cx="125" cy={eye.y} r="6" fill={color.color} />
      <circle cx="125" cy={eye.y} r="3" fill="#000" />
      <circle cx="127" cy={eye.y - 2} r="1.5" fill="white" />

      {/* Eyelids for certain styles */}
      {style === 'hooded' && (
        <>
          <path d="M 60 90 Q 75 85, 90 90" fill={skin.color} />
          <path d="M 110 90 Q 125 85, 140 90" fill={skin.color} />
        </>
      )}
    </g>
  );
};

// Render eyebrows
const renderEyebrows = (style, hairColor) => {
  const color = HAIR_COLORS.find(c => c.id === hairColor) || HAIR_COLORS[0];
  const browColor = color.color;

  const brows = {
    natural: (
      <>
        <path d="M 60 80 Q 75 76, 90 80" stroke={browColor} strokeWidth="3" fill="none" strokeLinecap="round" />
        <path d="M 110 80 Q 125 76, 140 80" stroke={browColor} strokeWidth="3" fill="none" strokeLinecap="round" />
      </>
    ),
    arched: (
      <>
        <path d="M 60 82 Q 75 74, 90 80" stroke={browColor} strokeWidth="3" fill="none" strokeLinecap="round" />
        <path d="M 110 80 Q 125 74, 140 82" stroke={browColor} strokeWidth="3" fill="none" strokeLinecap="round" />
      </>
    ),
    straight: (
      <>
        <path d="M 60 80 L 90 80" stroke={browColor} strokeWidth="3" fill="none" strokeLinecap="round" />
        <path d="M 110 80 L 140 80" stroke={browColor} strokeWidth="3" fill="none" strokeLinecap="round" />
      </>
    ),
    thick: (
      <>
        <path d="M 60 80 Q 75 75, 90 80" stroke={browColor} strokeWidth="5" fill="none" strokeLinecap="round" />
        <path d="M 110 80 Q 125 75, 140 80" stroke={browColor} strokeWidth="5" fill="none" strokeLinecap="round" />
      </>
    ),
    thin: (
      <>
        <path d="M 60 80 Q 75 77, 90 80" stroke={browColor} strokeWidth="1.5" fill="none" strokeLinecap="round" />
        <path d="M 110 80 Q 125 77, 140 80" stroke={browColor} strokeWidth="1.5" fill="none" strokeLinecap="round" />
      </>
    ),
    rounded: (
      <>
        <path d="M 62 82 Q 75 78, 88 82" stroke={browColor} strokeWidth="3" fill="none" strokeLinecap="round" />
        <path d="M 112 82 Q 125 78, 138 82" stroke={browColor} strokeWidth="3" fill="none" strokeLinecap="round" />
      </>
    ),
  };

  return <g className="eyebrows">{brows[style] || brows.natural}</g>;
};

// Render nose
const renderNose = (style, skinTone) => {
  const skin = SKIN_TONES.find(s => s.id === skinTone) || SKIN_TONES[0];

  const noses = {
    normal: (
      <g className="nose">
        <path d="M 100 95 L 100 115" stroke={skin.shadow} strokeWidth="2" fill="none" />
        <ellipse cx="100" cy="118" rx="8" ry="5" fill="none" stroke={skin.shadow} strokeWidth="1.5" />
      </g>
    ),
    small: (
      <g className="nose">
        <path d="M 100 98 L 100 112" stroke={skin.shadow} strokeWidth="1.5" fill="none" />
        <ellipse cx="100" cy="114" rx="5" ry="3" fill="none" stroke={skin.shadow} strokeWidth="1" />
      </g>
    ),
    button: (
      <g className="nose">
        <circle cx="100" cy="115" r="6" fill="none" stroke={skin.shadow} strokeWidth="1.5" />
      </g>
    ),
    pointed: (
      <g className="nose">
        <path d="M 100 92 L 100 118" stroke={skin.shadow} strokeWidth="2" fill="none" />
        <path d="M 93 118 L 100 122 L 107 118" fill="none" stroke={skin.shadow} strokeWidth="1.5" />
      </g>
    ),
    wide: (
      <g className="nose">
        <path d="M 100 95 L 100 115" stroke={skin.shadow} strokeWidth="2.5" fill="none" />
        <ellipse cx="100" cy="118" rx="12" ry="6" fill="none" stroke={skin.shadow} strokeWidth="1.5" />
      </g>
    ),
  };

  return noses[style] || noses.normal;
};

// Render mouth
const renderMouth = (style, skinTone) => {
  const skin = SKIN_TONES.find(s => s.id === skinTone) || SKIN_TONES[0];
  const lipColor = '#C77B7B';

  const mouths = {
    smile: (
      <g className="mouth">
        <path d="M 82 135 Q 100 148, 118 135" stroke={lipColor} strokeWidth="3" fill="none" strokeLinecap="round" />
        <path d="M 85 135 Q 100 143, 115 135" fill={lipColor} opacity="0.5" />
      </g>
    ),
    neutral: (
      <g className="mouth">
        <path d="M 85 138 Q 100 140, 115 138" stroke={lipColor} strokeWidth="3" fill="none" strokeLinecap="round" />
      </g>
    ),
    small: (
      <g className="mouth">
        <path d="M 90 138 Q 100 142, 110 138" stroke={lipColor} strokeWidth="2.5" fill="none" strokeLinecap="round" />
      </g>
    ),
    full: (
      <g className="mouth">
        <ellipse cx="100" cy="138" rx="15" ry="8" fill={lipColor} opacity="0.8" />
        <path d="M 85 138 Q 100 145, 115 138" fill="none" stroke={lipColor} strokeWidth="2" />
      </g>
    ),
    thin: (
      <g className="mouth">
        <path d="M 82 138 Q 100 141, 118 138" stroke={lipColor} strokeWidth="2" fill="none" strokeLinecap="round" />
      </g>
    ),
  };

  return mouths[style] || mouths.smile;
};

// Render face shape
const renderFaceShape = (shape, skinTone) => {
  const skin = SKIN_TONES.find(s => s.id === skinTone) || SKIN_TONES[0];

  const faces = {
    oval: (
      <ellipse cx="100" cy="105" rx="50" ry="65" fill={skin.color} />
    ),
    round: (
      <ellipse cx="100" cy="105" rx="55" ry="60" fill={skin.color} />
    ),
    square: (
      <rect x="50" y="45" width="100" height="115" rx="20" fill={skin.color} />
    ),
    heart: (
      <path d="M 100 165 Q 45 130, 50 80 Q 55 50, 100 45 Q 145 50, 150 80 Q 155 130, 100 165" fill={skin.color} />
    ),
    oblong: (
      <ellipse cx="100" cy="105" rx="45" ry="70" fill={skin.color} />
    ),
  };

  return (
    <g className="face-base">
      {faces[shape] || faces.oval}
      {/* Add subtle shading */}
      <ellipse cx="70" cy="100" rx="8" ry="12" fill={skin.shadow} opacity="0.15" />
      <ellipse cx="130" cy="100" rx="8" ry="12" fill={skin.shadow} opacity="0.15" />
    </g>
  );
};

// Main Avatar Component
const RealisticAvatar = memo(({
  avatar = {},
  size = 200,
  showBody = true,
  className = '',
}) => {
  const {
    gender = 'neutral',
    skinTone = 'light',
    hairStyle = 'short_neat',
    hairColor = 'brown',
    faceShape = 'oval',
    eyeStyle = 'normal',
    eyeColor = 'brown',
    eyebrowStyle = 'natural',
    noseStyle = 'normal',
    mouthStyle = 'smile',
    top = null,
    bottom = null,
    glasses = null,
    earrings = null,
    hat = null,
  } = avatar;

  const skin = useMemo(() =>
    SKIN_TONES.find(s => s.id === skinTone) || SKIN_TONES[0],
    [skinTone]
  );

  const viewBox = showBody ? "0 0 200 350" : "0 0 200 200";
  const height = showBody ? size * 1.75 : size;

  return (
    <svg
      viewBox={viewBox}
      width={size}
      height={height}
      className={`realistic-avatar ${className}`}
      style={{ overflow: 'visible' }}
    >
      <defs>
        {/* Subtle gradient for skin depth */}
        <radialGradient id={`skinGradient-${skinTone}`} cx="50%" cy="30%" r="70%">
          <stop offset="0%" stopColor={skin.highlight} />
          <stop offset="100%" stopColor={skin.color} />
        </radialGradient>

        {/* Shadow filter */}
        <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
          <feDropShadow dx="0" dy="2" stdDeviation="3" floodOpacity="0.2" />
        </filter>
      </defs>

      {/* Body (if showing full avatar) */}
      {showBody && (
        <g className="body" transform="translate(0, 155)">
          {/* Neck */}
          <rect x="85" y="0" width="30" height="25" fill={skin.color} />

          {/* Body/Torso */}
          <path
            d="M 40 45 Q 40 25, 85 20 L 115 20 Q 160 25, 160 45 L 160 180 L 40 180 Z"
            fill={top ? '#3B82F6' : '#6B7280'}
          />

          {/* Arms */}
          <path d="M 40 45 Q 20 50, 15 100 Q 10 130, 20 160" fill={skin.color} stroke={skin.shadow} strokeWidth="1" />
          <path d="M 160 45 Q 180 50, 185 100 Q 190 130, 180 160" fill={skin.color} stroke={skin.shadow} strokeWidth="1" />

          {/* Hands */}
          <ellipse cx="20" cy="165" rx="12" ry="15" fill={skin.color} />
          <ellipse cx="180" cy="165" rx="12" ry="15" fill={skin.color} />
        </g>
      )}

      {/* Head group */}
      <g className="head" filter="url(#shadow)">
        {/* Ears */}
        <ellipse cx="48" cy="100" rx="8" ry="12" fill={skin.color} />
        <ellipse cx="152" cy="100" rx="8" ry="12" fill={skin.color} />
        <ellipse cx="48" cy="100" rx="5" ry="8" fill={skin.shadow} opacity="0.3" />
        <ellipse cx="152" cy="100" rx="5" ry="8" fill={skin.shadow} opacity="0.3" />

        {/* Earrings */}
        {earrings && (
          <>
            <circle cx="48" cy="115" r="4" fill="#FFD700" />
            <circle cx="152" cy="115" r="4" fill="#FFD700" />
          </>
        )}

        {/* Face */}
        {renderFaceShape(faceShape, skinTone)}

        {/* Hair (back layer for some styles) */}
        {['long_straight', 'wavy', 'curly', 'long_wavy', 'braids', 'twin_tails', 'messy'].includes(hairStyle) && (
          <g className="hair-back" opacity="0.9">
            {renderHair(hairStyle, hairColor, gender)}
          </g>
        )}

        {/* Facial features */}
        {renderEyebrows(eyebrowStyle, hairColor)}
        {renderEyes(eyeStyle, eyeColor, skinTone)}
        {renderNose(noseStyle, skinTone)}
        {renderMouth(mouthStyle, skinTone)}

        {/* Hair (front layer) */}
        <g className="hair-front">
          {renderHair(hairStyle, hairColor, gender)}
        </g>

        {/* Glasses */}
        {glasses && (
          <g className="glasses">
            <circle cx="75" cy="95" r="18" fill="none" stroke="#333" strokeWidth="2" />
            <circle cx="125" cy="95" r="18" fill="none" stroke="#333" strokeWidth="2" />
            <path d="M 93 95 L 107 95" stroke="#333" strokeWidth="2" />
            <path d="M 57 95 L 48 92" stroke="#333" strokeWidth="2" />
            <path d="M 143 95 L 152 92" stroke="#333" strokeWidth="2" />
          </g>
        )}

        {/* Hat */}
        {hat && (
          <g className="hat">
            <ellipse cx="100" cy="50" rx="60" ry="15" fill="#1a1a1a" />
            <rect x="60" y="25" width="80" height="30" rx="5" fill="#1a1a1a" />
          </g>
        )}
      </g>
    </svg>
  );
});

RealisticAvatar.displayName = 'RealisticAvatar';

export default RealisticAvatar;
