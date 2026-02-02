// Sound effects using Web Audio API - no files needed
// McDonald's PlayPlace themed - fun and playful sounds

const audioContext = typeof window !== 'undefined'
  ? new (window.AudioContext || window.webkitAudioContext)()
  : null;

// Resume audio context on user interaction (browser requirement)
export const initAudio = () => {
  if (audioContext?.state === 'suspended') {
    audioContext.resume();
  }
};

// Play a tone with specific frequency and duration
const playTone = (frequency, duration, type = 'sine', volume = 0.3) => {
  if (!audioContext) return;

  const oscillator = audioContext.createOscillator();
  const gainNode = audioContext.createGain();

  oscillator.connect(gainNode);
  gainNode.connect(audioContext.destination);

  oscillator.frequency.value = frequency;
  oscillator.type = type;

  gainNode.gain.setValueAtTime(volume, audioContext.currentTime);
  gainNode.gain.exponentialRampToValueAtTime(0.01, audioContext.currentTime + duration);

  oscillator.start(audioContext.currentTime);
  oscillator.stop(audioContext.currentTime + duration);
};

// Play a sequence of tones
const playSequence = (notes, baseDelay = 0.1) => {
  notes.forEach(([freq, duration, delay], i) => {
    setTimeout(() => {
      playTone(freq, duration);
    }, (delay || i * baseDelay) * 1000);
  });
};

// WIN SOUND - Triumphant ascending arpeggio
export const playWinSound = () => {
  initAudio();
  // C major arpeggio going up - celebratory!
  const notes = [
    [523, 0.15, 0],      // C5
    [659, 0.15, 0.1],    // E5
    [784, 0.15, 0.2],    // G5
    [1047, 0.3, 0.3],    // C6 (hold longer)
  ];
  playSequence(notes);
};

// BINGO WIN - Extra exciting fanfare
export const playBingoSound = () => {
  initAudio();
  // Fanfare style
  const notes = [
    [523, 0.1, 0],       // C5
    [523, 0.1, 0.1],     // C5
    [523, 0.1, 0.2],     // C5
    [659, 0.2, 0.3],     // E5
    [784, 0.15, 0.5],    // G5
    [659, 0.1, 0.65],    // E5
    [784, 0.4, 0.75],    // G5 (long)
  ];
  playSequence(notes);
};

// REWARD/COINS - Coin collection sound
export const playRewardSound = () => {
  initAudio();
  // Quick ascending sparkle
  const notes = [
    [880, 0.08, 0],      // A5
    [1109, 0.08, 0.06],  // C#6
    [1319, 0.08, 0.12],  // E6
    [1760, 0.15, 0.18],  // A6
  ];
  playSequence(notes);
};

// GEMS - Magical sparkle sound
export const playGemSound = () => {
  initAudio();
  // Higher, more magical
  const notes = [
    [1319, 0.1, 0],      // E6
    [1568, 0.1, 0.08],   // G6
    [1976, 0.1, 0.16],   // B6
    [2093, 0.2, 0.24],   // C7
  ];
  playSequence(notes);
};

// STREAK - Power up sound
export const playStreakSound = () => {
  initAudio();
  // Rising power-up
  const notes = [
    [330, 0.1, 0],       // E4
    [440, 0.1, 0.08],    // A4
    [554, 0.1, 0.16],    // C#5
    [659, 0.1, 0.24],    // E5
    [880, 0.2, 0.32],    // A5
  ];
  playSequence(notes);
};

// DAILY BONUS - Happy jingle
export const playDailyBonusSound = () => {
  initAudio();
  // Happy little tune
  const notes = [
    [523, 0.12, 0],      // C5
    [659, 0.12, 0.12],   // E5
    [784, 0.12, 0.24],   // G5
    [880, 0.12, 0.36],   // A5
    [784, 0.12, 0.48],   // G5
    [1047, 0.3, 0.6],    // C6
  ];
  playSequence(notes);
};

// LEVEL UP - Epic ascending
export const playLevelUpSound = () => {
  initAudio();
  const notes = [
    [262, 0.15, 0],      // C4
    [330, 0.15, 0.12],   // E4
    [392, 0.15, 0.24],   // G4
    [523, 0.15, 0.36],   // C5
    [659, 0.15, 0.48],   // E5
    [784, 0.15, 0.6],    // G5
    [1047, 0.4, 0.72],   // C6
  ];
  playSequence(notes);
};

// BUTTON CLICK - Subtle pop
export const playClickSound = () => {
  initAudio();
  playTone(600, 0.05, 'sine', 0.15);
};

// NUMBER CALLED - Attention sound
export const playNumberCallSound = () => {
  initAudio();
  playTone(880, 0.1, 'sine', 0.2);
  setTimeout(() => playTone(1100, 0.08, 'sine', 0.15), 80);
};

// ERROR/WRONG - Gentle buzz
export const playErrorSound = () => {
  initAudio();
  playTone(200, 0.15, 'sawtooth', 0.1);
  setTimeout(() => playTone(180, 0.15, 'sawtooth', 0.1), 100);
};

export default {
  initAudio,
  playWinSound,
  playBingoSound,
  playRewardSound,
  playGemSound,
  playStreakSound,
  playDailyBonusSound,
  playLevelUpSound,
  playClickSound,
  playNumberCallSound,
  playErrorSound,
};
