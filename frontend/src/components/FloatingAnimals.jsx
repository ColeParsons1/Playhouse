import { useEffect, useState, useRef, memo } from 'react';

const animals = [
  { emoji: '🐰', name: 'bunny' },
  { emoji: '🐱', name: 'kitten' },
  { emoji: '🐶', name: 'puppy' },
  { emoji: '🐣', name: 'chick' },
  { emoji: '🐹', name: 'hamster' },
  { emoji: '🦊', name: 'fox' },
  { emoji: '🐼', name: 'panda' },
  { emoji: '🐨', name: 'koala' },
  { emoji: '🦋', name: 'butterfly' },
  { emoji: '🌸', name: 'blossom' },
  { emoji: '🐿️', name: 'squirrel' },
  { emoji: '🦔', name: 'hedgehog' },
];

// Pure CSS animation - no React re-renders
const FloatingAnimals = memo(() => {
  const [floatingAnimals, setFloatingAnimals] = useState([]);
  const initialized = useRef(false);

  useEffect(() => {
    if (initialized.current) return;
    initialized.current = true;

    // Create fewer animals for better performance (6 instead of 12)
    const newAnimals = [];
    for (let i = 0; i < 6; i++) {
      const animal = animals[Math.floor(Math.random() * animals.length)];
      newAnimals.push({
        id: i,
        animal,
        startX: Math.random() * 80 + 10, // percentage
        delay: i * 4 + Math.random() * 4,
        duration: 20 + Math.random() * 10,
      });
    }
    setFloatingAnimals(newAnimals);
  }, []);

  return (
    <div className="floating-animals-container">
      <style>{`
        .floating-animals-container {
          position: fixed;
          inset: 0;
          overflow: hidden;
          z-index: 0;
          pointer-events: none;
        }

        .floating-animal {
          position: fixed;
          font-size: 1.8rem;
          pointer-events: none;
          will-change: transform;
          animation: floatUp var(--duration) ease-in-out infinite;
          animation-delay: var(--delay);
          opacity: 0;
        }

        @keyframes floatUp {
          0% {
            transform: translateY(110vh) translateX(0) rotate(-10deg) scale(0.5);
            opacity: 0;
          }
          10% {
            opacity: 0.7;
          }
          50% {
            transform: translateY(50vh) translateX(30px) rotate(10deg) scale(1);
            opacity: 0.7;
          }
          90% {
            opacity: 0.7;
          }
          100% {
            transform: translateY(-10vh) translateX(-20px) rotate(0deg) scale(0.8);
            opacity: 0;
          }
        }
      `}</style>
      {floatingAnimals.map((item) => (
        <div
          key={item.id}
          className="floating-animal"
          style={{
            left: `${item.startX}%`,
            '--delay': `${item.delay}s`,
            '--duration': `${item.duration}s`,
          }}
        >
          {item.animal.emoji}
        </div>
      ))}
    </div>
  );
});

FloatingAnimals.displayName = 'FloatingAnimals';

export default FloatingAnimals;
