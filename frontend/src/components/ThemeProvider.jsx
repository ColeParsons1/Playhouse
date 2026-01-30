import { useEffect } from 'react';
import './ThemeProvider.css';

const themes = {
  default: {
    name: 'default',
    class: 'theme-default',
    description: 'Sweet pink dreams',
    icon: '🌸',
    premium: false
  },
  dark: {
    name: 'dark',
    class: 'theme-dark',
    description: 'Sleek dark mode',
    icon: '🌙',
    premium: false
  },
  pastel: {
    name: 'pastel',
    class: 'theme-pastel',
    description: 'Soft pastel paradise',
    icon: '☁️',
    premium: false
  },
  ocean: {
    name: 'ocean',
    class: 'theme-ocean',
    description: 'Calming ocean waves',
    icon: '🌊',
    premium: false
  },
  sakura: {
    name: 'sakura',
    class: 'theme-sakura',
    description: 'Cherry blossom magic',
    icon: '🌸',
    premium: false
  },
  sunset: {
    name: 'sunset',
    class: 'theme-sunset',
    description: 'Golden hour glow',
    icon: '🌅',
    premium: false
  },
  night: {
    name: 'night',
    class: 'theme-night',
    description: 'Mystical starlight',
    icon: '✨',
    premium: false
  },
  rainbow: {
    name: 'rainbow',
    class: 'theme-rainbow',
    description: 'Ultimate rainbow magic',
    icon: '🌈',
    premium: true
  },
  aurora: {
    name: 'aurora',
    class: 'theme-aurora',
    description: 'Dancing northern lights',
    icon: '💫',
    premium: true
  },
  candy: {
    name: 'candy',
    class: 'theme-candy',
    description: 'Sweet cotton candy swirls',
    icon: '🍬',
    premium: false
  },
  lavender: {
    name: 'lavender',
    class: 'theme-lavender',
    description: 'Peaceful lavender fields',
    icon: '💜',
    premium: false
  },
  mint: {
    name: 'mint',
    class: 'theme-mint',
    description: 'Cool refreshing mint',
    icon: '🌿',
    premium: false
  },
  rose: {
    name: 'rose',
    class: 'theme-rose',
    description: 'Romantic rose garden',
    icon: '🌹',
    premium: false
  },
  galaxy: {
    name: 'galaxy',
    class: 'theme-galaxy',
    description: 'Cosmic nebula dreams',
    icon: '🌌',
    premium: true
  },
  peach: {
    name: 'peach',
    class: 'theme-peach',
    description: 'Soft peachy warmth',
    icon: '🍑',
    premium: false
  },
  butterfly: {
    name: 'butterfly',
    class: 'theme-butterfly',
    description: 'Magical butterfly garden',
    icon: '🦋',
    premium: false
  },
  coral: {
    name: 'coral',
    class: 'theme-coral',
    description: 'Vibrant coral reef',
    icon: '🐠',
    premium: false
  },
  strawberry: {
    name: 'strawberry',
    class: 'theme-strawberry',
    description: 'Sweet strawberry fields',
    icon: '🍓',
    premium: false
  },
  forest: {
    name: 'forest',
    class: 'theme-forest',
    description: 'Enchanted woodland magic',
    icon: '🌲',
    premium: true
  },
  farm: {
    name: 'farm',
    class: 'theme-farm',
    description: 'Adorable animal friends',
    icon: '🐮',
    premium: false
  },
  neon: {
    name: 'neon',
    class: 'theme-neon',
    description: 'Cyberpunk neon glow',
    icon: '⚡',
    premium: true
  }
};

// Export themes for use in other components
export { themes };

const ThemeProvider = ({ theme = 'default', children }) => {
  useEffect(() => {
    // Remove all theme classes
    Object.values(themes).forEach(t => {
      document.body.classList.remove(t.class);
    });

    // Add the current theme class
    const themeConfig = themes[theme] || themes.default;
    document.body.classList.add(themeConfig.class);

    return () => {
      document.body.classList.remove(themeConfig.class);
    };
  }, [theme]);

  return (
    <div className={`theme-wrapper ${themes[theme]?.class || 'theme-default'}`}>
      {children}
    </div>
  );
};

export default ThemeProvider;
