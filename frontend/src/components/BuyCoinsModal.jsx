import { useState, useEffect, useCallback } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import { loadStripe } from '@stripe/stripe-js';
import {
  Elements,
  PaymentRequestButtonElement,
  useStripe,
} from '@stripe/react-stripe-js';
import './BuyCoinsModal.css';

// Initialize Stripe
const stripePromise = loadStripe('pk_test_placeholder');

const COIN_PACKAGES = [
  { id: 1, coins: 800, price: 0.99, bonus: null },
  { id: 2, coins: 2800, price: 2.99, bonus: '+16%' },
  { id: 3, coins: 5000, price: 4.99, bonus: '+24%', popular: true },
  { id: 4, coins: 12000, price: 9.99, bonus: '+49%' },
  { id: 5, coins: 75000, price: 49.99, bonus: '+86%', bestValue: true },
];

// Get API URL
const getApiUrl = () => {
  const host = window.location.host;
  if (host.includes('localhost') || host.includes('127.0.0.1')) {
    return 'http://localhost:8000/api';
  }
  return `${window.location.protocol}//${host}/api`;
};

// Payment Button Component (handles Apple Pay / Google Pay)
const PaymentButton = ({ pkg, player, onSuccess, onError, onProcessing, onAvailabilityChange }) => {
  const stripe = useStripe();
  const [paymentRequest, setPaymentRequest] = useState(null);
  const [canMakePayment, setCanMakePayment] = useState(null);
  const [clientSecret, setClientSecret] = useState(null);

  // Create payment intent when package is selected
  useEffect(() => {
    if (!pkg || !player?.id) return;

    const createIntent = async () => {
      try {
        const response = await fetch(`${getApiUrl()}/payments/create-intent/`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            package_id: pkg.id,
            player_id: player.id,
          }),
        });

        const data = await response.json();
        if (data.clientSecret) {
          setClientSecret(data.clientSecret);
          console.log('Payment intent created:', data);
        } else if (data.error) {
          console.error('Error creating payment intent:', data.error);
          onError(data.error);
        }
      } catch (err) {
        console.error('Failed to create payment intent:', err);
        onError('Failed to initialize payment');
      }
    };

    createIntent();
  }, [pkg, player?.id, onError]);

  // Set up payment request after we have stripe and client secret
  useEffect(() => {
    if (!stripe || !pkg || !clientSecret) return;

    const pr = stripe.paymentRequest({
      country: 'US',
      currency: 'usd',
      total: {
        label: `${pkg.coins.toLocaleString()} Playhouse Coins`,
        amount: Math.round(pkg.price * 100),
      },
      requestPayerName: true,
      requestPayerEmail: true,
    });

    // Check if Apple Pay / Google Pay is available
    pr.canMakePayment().then((result) => {
      console.log('canMakePayment result:', result);
      if (result) {
        setPaymentRequest(pr);
        setCanMakePayment(true);
        onAvailabilityChange?.(true, result.applePay ? 'apple' : 'google');
      } else {
        setCanMakePayment(false);
        onAvailabilityChange?.(false, null);
      }
    });

    // Handle the payment when user authenticates
    pr.on('paymentmethod', async (ev) => {
      console.log('Payment method received:', ev.paymentMethod.id);
      onProcessing(true);

      try {
        // Confirm the payment with the payment method from Apple Pay
        const { paymentIntent, error: confirmError } = await stripe.confirmCardPayment(
          clientSecret,
          { payment_method: ev.paymentMethod.id },
          { handleActions: false }
        );

        if (confirmError) {
          console.error('Payment confirmation error:', confirmError);
          ev.complete('fail');
          onError(confirmError.message);
          onProcessing(false);
          return;
        }

        console.log('Payment intent status:', paymentIntent.status);

        if (paymentIntent.status === 'requires_action') {
          // Handle 3D Secure if needed
          const { error: actionError } = await stripe.confirmCardPayment(clientSecret);
          if (actionError) {
            ev.complete('fail');
            onError(actionError.message);
            onProcessing(false);
            return;
          }
        }

        // Payment successful
        ev.complete('success');

        // Confirm with backend and add coins
        const confirmResponse = await fetch(`${getApiUrl()}/payments/confirm/`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            payment_intent_id: paymentIntent.id,
            player_id: player?.id,
          }),
        });

        const confirmData = await confirmResponse.json();
        console.log('Backend confirm response:', confirmData);

        if (confirmData.success) {
          onSuccess(confirmData);
        } else {
          onError(confirmData.error || 'Failed to add coins');
        }
      } catch (err) {
        console.error('Payment error:', err);
        ev.complete('fail');
        onError(err.message || 'Payment failed');
      } finally {
        onProcessing(false);
      }
    });

    return () => {
      pr.off('paymentmethod');
    };
  }, [stripe, pkg, clientSecret, player, onSuccess, onError, onProcessing, onAvailabilityChange]);

  // Still creating payment intent
  if (!clientSecret) {
    return (
      <div className="payment-checking">
        <div className="mini-spinner"></div>
        <span>Preparing payment...</span>
      </div>
    );
  }

  // Still checking payment availability
  if (canMakePayment === null) {
    return (
      <div className="payment-checking">
        <div className="mini-spinner"></div>
        <span>Checking payment options...</span>
      </div>
    );
  }

  // Not available
  if (!canMakePayment || !paymentRequest) {
    return (
      <div className="payment-not-available">
        <p>Apple Pay / Google Pay not available on this device.</p>
        <p className="payment-setup-hint">
          Make sure you have Apple Pay or Google Pay set up in your device settings, and you're using Safari (iOS) or Chrome (Android).
        </p>
      </div>
    );
  }

  return (
    <div className="apple-pay-button-container">
      <PaymentRequestButtonElement
        options={{
          paymentRequest,
          style: {
            paymentRequestButton: {
              type: 'buy',
              theme: 'dark',
              height: '48px',
            },
          },
        }}
      />
    </div>
  );
};

// Main Modal Component
const BuyCoinsModalContent = ({ player, onClose, onPurchaseComplete }) => {
  const [selectedPackage, setSelectedPackage] = useState(null);
  const [processing, setProcessing] = useState(false);
  const [error, setError] = useState(null);
  const [success, setSuccess] = useState(null);
  const [paymentType, setPaymentType] = useState(null);

  const handleSuccess = useCallback((data) => {
    setSuccess({
      coins: data.coins_added,
      newBalance: data.new_balance,
    });
    setSelectedPackage(null);
    onPurchaseComplete?.(data.player);
  }, [onPurchaseComplete]);

  const handleError = useCallback((message) => {
    console.error('Payment error:', message);
    setError(message);
    setTimeout(() => setError(null), 5000);
  }, []);

  const handleSelectPackage = (pkg) => {
    setSelectedPackage(pkg);
    setError(null);
  };

  const handleAvailabilityChange = useCallback((available, type) => {
    setPaymentType(type);
  }, []);

  return (
    <motion.div
      className="buy-coins-modal"
      initial={{ scale: 0.8, y: 50 }}
      animate={{ scale: 1, y: 0 }}
      exit={{ scale: 0.8, y: 50 }}
      onClick={(e) => e.stopPropagation()}
    >
      <div className="buy-coins-header">
        <h2>
          <span className="coin-icon-large">🪙</span>
          Buy Coins
        </h2>
        <button className="close-btn" onClick={onClose}>✕</button>
      </div>

      <div className="current-balance">
        <span>Current Balance:</span>
        <span className="balance-amount">
          🪙 {player?.coins?.toLocaleString() || 0}
        </span>
      </div>

      {/* Success Message */}
      <AnimatePresence>
        {success && (
          <motion.div
            className="purchase-success"
            initial={{ opacity: 0, y: -10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0 }}
          >
            <span className="success-icon">✅</span>
            <span>+{success.coins.toLocaleString()} coins added!</span>
            <button onClick={() => setSuccess(null)}>OK</button>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Error Message */}
      <AnimatePresence>
        {error && (
          <motion.div
            className="purchase-error"
            initial={{ opacity: 0, y: -10 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0 }}
          >
            <span className="error-icon">❌</span>
            <span>{error}</span>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Processing Overlay */}
      {processing && (
        <div className="processing-overlay">
          <div className="processing-spinner"></div>
          <span>Processing payment...</span>
        </div>
      )}

      <div className="coin-packages">
        {COIN_PACKAGES.map((pkg) => (
          <motion.button
            key={pkg.id}
            className={`coin-package ${pkg.popular ? 'popular' : ''} ${pkg.bestValue ? 'best-value' : ''} ${selectedPackage?.id === pkg.id ? 'selected' : ''}`}
            onClick={() => handleSelectPackage(pkg)}
            whileHover={{ scale: 1.02 }}
            whileTap={{ scale: 0.98 }}
            disabled={processing}
          >
            {pkg.popular && <div className="package-badge popular-badge">POPULAR</div>}
            {pkg.bestValue && <div className="package-badge best-value-badge">BEST VALUE</div>}

            <div className="package-coins">
              <span className="coins-icon">🪙</span>
              <span className="coins-amount">{pkg.coins.toLocaleString()}</span>
              {pkg.bonus && <span className="bonus-tag">{pkg.bonus}</span>}
            </div>

            <div className="package-price">
              ${pkg.price.toFixed(2)}
            </div>
          </motion.button>
        ))}
      </div>

      {/* Apple Pay Button appears when package selected */}
      {selectedPackage && (
        <motion.div
          className="payment-section"
          initial={{ opacity: 0, height: 0 }}
          animate={{ opacity: 1, height: 'auto' }}
        >
          <div className="selected-package-summary">
            <span>Selected: {selectedPackage.coins.toLocaleString()} coins for ${selectedPackage.price.toFixed(2)}</span>
          </div>

          <PaymentButton
            key={selectedPackage.id} // Force remount when package changes
            pkg={selectedPackage}
            player={player}
            onSuccess={handleSuccess}
            onError={handleError}
            onProcessing={setProcessing}
            onAvailabilityChange={handleAvailabilityChange}
          />
        </motion.div>
      )}

      <p className="purchase-note">
        {paymentType === 'apple' ? ' Pay with Apple Pay' :
         paymentType === 'google' ? ' Pay with Google Pay' :
         'Secure payment powered by Stripe'}
      </p>
    </motion.div>
  );
};

// Wrapper with Stripe Elements
const BuyCoinsModal = ({ isOpen, onClose, player, onPurchaseComplete }) => {
  return (
    <AnimatePresence>
      {isOpen && (
        <motion.div
          className="buy-coins-overlay"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          onClick={onClose}
        >
          <Elements stripe={stripePromise}>
            <BuyCoinsModalContent
              player={player}
              onClose={onClose}
              onPurchaseComplete={onPurchaseComplete}
            />
          </Elements>
        </motion.div>
      )}
    </AnimatePresence>
  );
};

export default BuyCoinsModal;
