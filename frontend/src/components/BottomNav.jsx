import { memo } from 'react';
import { motion } from 'framer-motion';
import './BottomNav.css';

const NAV_ITEMS = [
  { id: 'create', icon: '🎤', label: 'Create', hasNotification: false },
  { id: 'rooms', icon: '🎙️', label: 'Rooms', hasNotification: false },
  { id: 'messages', icon: '💬', label: 'Messages', hasNotification: true, notificationCount: 2 },
  { id: 'games', icon: '🎮', label: 'Games', hasNotification: false },
  { id: 'profile', icon: '👤', label: 'Me', hasNotification: true },
];

const BottomNav = memo(({ activeTab, onTabChange }) => {
  return (
    <nav className="bottom-nav">
      {NAV_ITEMS.map((item) => (
        <motion.button
          key={item.id}
          className={`nav-item ${activeTab === item.id ? 'active' : ''}`}
          onClick={() => onTabChange(item.id)}
          whileTap={{ scale: 0.9 }}
        >
          <div className="nav-icon-wrapper">
            <span className="nav-icon">{item.icon}</span>
            {item.hasNotification && (
              <span className="notification-dot">
                {item.notificationCount && (
                  <span className="notification-count">{item.notificationCount}</span>
                )}
              </span>
            )}
          </div>
          <span className="nav-label">{item.label}</span>
          {activeTab === item.id && (
            <motion.div
              className="active-indicator"
              layoutId="navIndicator"
              transition={{ type: 'spring', stiffness: 500, damping: 30 }}
            />
          )}
        </motion.button>
      ))}
    </nav>
  );
});

BottomNav.displayName = 'BottomNav';

export default BottomNav;
