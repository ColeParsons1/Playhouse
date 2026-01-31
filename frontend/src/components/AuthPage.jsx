import React, { useState } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './AuthPage.css';

const AuthPage = ({ onLogin, isLoading }) => {
  const [mode, setMode] = useState('login'); // 'login' or 'register'
  const [formData, setFormData] = useState({
    name: '',
    email: '',
    password: '',
    confirmPassword: '',
  });
  const [error, setError] = useState('');
  const [showPassword, setShowPassword] = useState(false);

  const handleChange = (e) => {
    const { name, value } = e.target;
    setFormData(prev => ({ ...prev, [name]: value }));
    setError(''); // Clear error on input change
  };

  const handleSubmit = async (e) => {
    e.preventDefault();
    setError('');

    // Validation
    if (mode === 'register') {
      if (!formData.name.trim()) {
        setError('Please enter your name');
        return;
      }
      if (formData.name.trim().length < 2) {
        setError('Name must be at least 2 characters');
        return;
      }
      if (formData.password !== formData.confirmPassword) {
        setError('Passwords do not match');
        return;
      }
    }

    if (!formData.email.trim() || !formData.email.includes('@')) {
      setError('Please enter a valid email');
      return;
    }

    if (formData.password.length < 4) {
      setError('Password must be at least 4 characters');
      return;
    }

    try {
      await onLogin(mode, formData);
    } catch (err) {
      setError(err.message || 'Something went wrong');
    }
  };

  const switchMode = () => {
    setMode(mode === 'login' ? 'register' : 'login');
    setError('');
    setFormData({
      name: '',
      email: '',
      password: '',
      confirmPassword: '',
    });
  };

  return (
    <div className="auth-page">
      {/* Background decorations */}
      <div className="auth-bg-decoration">
        <div className="auth-circle circle-1" />
        <div className="auth-circle circle-2" />
        <div className="auth-circle circle-3" />
      </div>

      <motion.div
        className="auth-container"
        initial={{ opacity: 0, y: 20 }}
        animate={{ opacity: 1, y: 0 }}
        transition={{ duration: 0.5 }}
      >
        {/* Logo/Header */}
        <div className="auth-header">
          <motion.div
            className="auth-logo"
            initial={{ scale: 0 }}
            animate={{ scale: 1 }}
            transition={{ type: 'spring', stiffness: 200, delay: 0.2 }}
          >
            🎮
          </motion.div>
          <h1 className="auth-title">Playhouse</h1>
          <p className="auth-subtitle">
            {mode === 'login' ? 'Welcome back!' : 'Join the fun!'}
          </p>
        </div>

        {/* Mode Toggle */}
        <div className="auth-mode-toggle">
          <button
            className={`mode-btn ${mode === 'login' ? 'active' : ''}`}
            onClick={() => mode !== 'login' && switchMode()}
          >
            Sign In
          </button>
          <button
            className={`mode-btn ${mode === 'register' ? 'active' : ''}`}
            onClick={() => mode !== 'register' && switchMode()}
          >
            Register
          </button>
          <div className={`mode-slider ${mode === 'register' ? 'right' : ''}`} />
        </div>

        {/* Form */}
        <form className="auth-form" onSubmit={handleSubmit}>
          <AnimatePresence mode="wait">
            {mode === 'register' && (
              <motion.div
                key="name-field"
                initial={{ opacity: 0, height: 0 }}
                animate={{ opacity: 1, height: 'auto' }}
                exit={{ opacity: 0, height: 0 }}
                transition={{ duration: 0.2 }}
              >
                <div className="form-group">
                  <label htmlFor="name">Display Name</label>
                  <div className="input-wrapper">
                    <span className="input-icon">👤</span>
                    <input
                      type="text"
                      id="name"
                      name="name"
                      value={formData.name}
                      onChange={handleChange}
                      placeholder="Enter your name"
                      autoComplete="name"
                    />
                  </div>
                </div>
              </motion.div>
            )}
          </AnimatePresence>

          <div className="form-group">
            <label htmlFor="email">Email</label>
            <div className="input-wrapper">
              <span className="input-icon">📧</span>
              <input
                type="email"
                id="email"
                name="email"
                value={formData.email}
                onChange={handleChange}
                placeholder="Enter your email"
                autoComplete="email"
              />
            </div>
          </div>

          <div className="form-group">
            <label htmlFor="password">Password</label>
            <div className="input-wrapper">
              <span className="input-icon">🔒</span>
              <input
                type={showPassword ? 'text' : 'password'}
                id="password"
                name="password"
                value={formData.password}
                onChange={handleChange}
                placeholder="Enter your password"
                autoComplete={mode === 'login' ? 'current-password' : 'new-password'}
              />
              <button
                type="button"
                className="password-toggle"
                onClick={() => setShowPassword(!showPassword)}
              >
                {showPassword ? '🙈' : '👁️'}
              </button>
            </div>
          </div>

          <AnimatePresence mode="wait">
            {mode === 'register' && (
              <motion.div
                key="confirm-field"
                initial={{ opacity: 0, height: 0 }}
                animate={{ opacity: 1, height: 'auto' }}
                exit={{ opacity: 0, height: 0 }}
                transition={{ duration: 0.2 }}
              >
                <div className="form-group">
                  <label htmlFor="confirmPassword">Confirm Password</label>
                  <div className="input-wrapper">
                    <span className="input-icon">🔐</span>
                    <input
                      type={showPassword ? 'text' : 'password'}
                      id="confirmPassword"
                      name="confirmPassword"
                      value={formData.confirmPassword}
                      onChange={handleChange}
                      placeholder="Confirm your password"
                      autoComplete="new-password"
                    />
                  </div>
                </div>
              </motion.div>
            )}
          </AnimatePresence>

          {/* Error Message */}
          <AnimatePresence>
            {error && (
              <motion.div
                className="auth-error"
                initial={{ opacity: 0, y: -10 }}
                animate={{ opacity: 1, y: 0 }}
                exit={{ opacity: 0, y: -10 }}
              >
                <span>⚠️</span> {error}
              </motion.div>
            )}
          </AnimatePresence>

          {/* Submit Button */}
          <motion.button
            type="submit"
            className="auth-submit-btn"
            disabled={isLoading}
            whileHover={{ scale: 1.02 }}
            whileTap={{ scale: 0.98 }}
          >
            {isLoading ? (
              <span className="loading-spinner-small" />
            ) : mode === 'login' ? (
              'Sign In'
            ) : (
              'Create Account'
            )}
          </motion.button>
        </form>

        {/* Footer */}
        <div className="auth-footer">
          <p>
            {mode === 'login' ? "Don't have an account?" : 'Already have an account?'}
            <button className="switch-mode-link" onClick={switchMode}>
              {mode === 'login' ? 'Register' : 'Sign In'}
            </button>
          </p>
        </div>

        {/* Social login hint */}
        <div className="auth-divider">
          <span>or continue as</span>
        </div>

        <motion.button
          type="button"
          className="guest-btn"
          onClick={() => onLogin('guest', {})}
          whileHover={{ scale: 1.02 }}
          whileTap={{ scale: 0.98 }}
        >
          🎭 Guest
        </motion.button>
      </motion.div>
    </div>
  );
};

export default AuthPage;
