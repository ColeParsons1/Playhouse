import { useState, useCallback, useEffect, useRef } from 'react';
import './MapsGame.css';

// All countries with their ISO codes for flag display and SVG paths
const COUNTRIES = [
  { name: 'Afghanistan', code: 'AF' },
  { name: 'Albania', code: 'AL' },
  { name: 'Algeria', code: 'DZ' },
  { name: 'Andorra', code: 'AD' },
  { name: 'Angola', code: 'AO' },
  { name: 'Antigua and Barbuda', code: 'AG', alt: ['antigua', 'barbuda'] },
  { name: 'Argentina', code: 'AR' },
  { name: 'Armenia', code: 'AM' },
  { name: 'Australia', code: 'AU' },
  { name: 'Austria', code: 'AT' },
  { name: 'Azerbaijan', code: 'AZ' },
  { name: 'Bahamas', code: 'BS', alt: ['the bahamas'] },
  { name: 'Bahrain', code: 'BH' },
  { name: 'Bangladesh', code: 'BD' },
  { name: 'Barbados', code: 'BB' },
  { name: 'Belarus', code: 'BY' },
  { name: 'Belgium', code: 'BE' },
  { name: 'Belize', code: 'BZ' },
  { name: 'Benin', code: 'BJ' },
  { name: 'Bhutan', code: 'BT' },
  { name: 'Bolivia', code: 'BO' },
  { name: 'Bosnia and Herzegovina', code: 'BA', alt: ['bosnia', 'herzegovina'] },
  { name: 'Botswana', code: 'BW' },
  { name: 'Brazil', code: 'BR' },
  { name: 'Brunei', code: 'BN' },
  { name: 'Bulgaria', code: 'BG' },
  { name: 'Burkina Faso', code: 'BF' },
  { name: 'Burundi', code: 'BI' },
  { name: 'Cambodia', code: 'KH' },
  { name: 'Cameroon', code: 'CM' },
  { name: 'Canada', code: 'CA' },
  { name: 'Cape Verde', code: 'CV', alt: ['cabo verde'] },
  { name: 'Central African Republic', code: 'CF', alt: ['car'] },
  { name: 'Chad', code: 'TD' },
  { name: 'Chile', code: 'CL' },
  { name: 'China', code: 'CN' },
  { name: 'Colombia', code: 'CO' },
  { name: 'Comoros', code: 'KM' },
  { name: 'Congo', code: 'CG', alt: ['republic of congo', 'congo-brazzaville'] },
  { name: 'Costa Rica', code: 'CR' },
  { name: 'Croatia', code: 'HR' },
  { name: 'Cuba', code: 'CU' },
  { name: 'Cyprus', code: 'CY' },
  { name: 'Czech Republic', code: 'CZ', alt: ['czechia'] },
  { name: 'Democratic Republic of the Congo', code: 'CD', alt: ['drc', 'dr congo', 'congo-kinshasa'] },
  { name: 'Denmark', code: 'DK' },
  { name: 'Djibouti', code: 'DJ' },
  { name: 'Dominica', code: 'DM' },
  { name: 'Dominican Republic', code: 'DO' },
  { name: 'East Timor', code: 'TL', alt: ['timor-leste', 'timor leste'] },
  { name: 'Ecuador', code: 'EC' },
  { name: 'Egypt', code: 'EG' },
  { name: 'El Salvador', code: 'SV' },
  { name: 'Equatorial Guinea', code: 'GQ' },
  { name: 'Eritrea', code: 'ER' },
  { name: 'Estonia', code: 'EE' },
  { name: 'Eswatini', code: 'SZ', alt: ['swaziland'] },
  { name: 'Ethiopia', code: 'ET' },
  { name: 'Fiji', code: 'FJ' },
  { name: 'Finland', code: 'FI' },
  { name: 'France', code: 'FR' },
  { name: 'Gabon', code: 'GA' },
  { name: 'Gambia', code: 'GM', alt: ['the gambia'] },
  { name: 'Georgia', code: 'GE' },
  { name: 'Germany', code: 'DE' },
  { name: 'Ghana', code: 'GH' },
  { name: 'Greece', code: 'GR' },
  { name: 'Grenada', code: 'GD' },
  { name: 'Guatemala', code: 'GT' },
  { name: 'Guinea', code: 'GN' },
  { name: 'Guinea-Bissau', code: 'GW' },
  { name: 'Guyana', code: 'GY' },
  { name: 'Haiti', code: 'HT' },
  { name: 'Honduras', code: 'HN' },
  { name: 'Hungary', code: 'HU' },
  { name: 'Iceland', code: 'IS' },
  { name: 'India', code: 'IN' },
  { name: 'Indonesia', code: 'ID' },
  { name: 'Iran', code: 'IR' },
  { name: 'Iraq', code: 'IQ' },
  { name: 'Ireland', code: 'IE' },
  { name: 'Israel', code: 'IL' },
  { name: 'Italy', code: 'IT' },
  { name: 'Ivory Coast', code: 'CI', alt: ['cote d\'ivoire', 'cote divoire'] },
  { name: 'Jamaica', code: 'JM' },
  { name: 'Japan', code: 'JP' },
  { name: 'Jordan', code: 'JO' },
  { name: 'Kazakhstan', code: 'KZ' },
  { name: 'Kenya', code: 'KE' },
  { name: 'Kiribati', code: 'KI' },
  { name: 'Kosovo', code: 'XK' },
  { name: 'Kuwait', code: 'KW' },
  { name: 'Kyrgyzstan', code: 'KG' },
  { name: 'Laos', code: 'LA' },
  { name: 'Latvia', code: 'LV' },
  { name: 'Lebanon', code: 'LB' },
  { name: 'Lesotho', code: 'LS' },
  { name: 'Liberia', code: 'LR' },
  { name: 'Libya', code: 'LY' },
  { name: 'Liechtenstein', code: 'LI' },
  { name: 'Lithuania', code: 'LT' },
  { name: 'Luxembourg', code: 'LU' },
  { name: 'Madagascar', code: 'MG' },
  { name: 'Malawi', code: 'MW' },
  { name: 'Malaysia', code: 'MY' },
  { name: 'Maldives', code: 'MV' },
  { name: 'Mali', code: 'ML' },
  { name: 'Malta', code: 'MT' },
  { name: 'Marshall Islands', code: 'MH' },
  { name: 'Mauritania', code: 'MR' },
  { name: 'Mauritius', code: 'MU' },
  { name: 'Mexico', code: 'MX' },
  { name: 'Micronesia', code: 'FM' },
  { name: 'Moldova', code: 'MD' },
  { name: 'Monaco', code: 'MC' },
  { name: 'Mongolia', code: 'MN' },
  { name: 'Montenegro', code: 'ME' },
  { name: 'Morocco', code: 'MA' },
  { name: 'Mozambique', code: 'MZ' },
  { name: 'Myanmar', code: 'MM', alt: ['burma'] },
  { name: 'Namibia', code: 'NA' },
  { name: 'Nauru', code: 'NR' },
  { name: 'Nepal', code: 'NP' },
  { name: 'Netherlands', code: 'NL', alt: ['holland'] },
  { name: 'New Zealand', code: 'NZ' },
  { name: 'Nicaragua', code: 'NI' },
  { name: 'Niger', code: 'NE' },
  { name: 'Nigeria', code: 'NG' },
  { name: 'North Korea', code: 'KP', alt: ['dprk'] },
  { name: 'North Macedonia', code: 'MK', alt: ['macedonia'] },
  { name: 'Norway', code: 'NO' },
  { name: 'Oman', code: 'OM' },
  { name: 'Pakistan', code: 'PK' },
  { name: 'Palau', code: 'PW' },
  { name: 'Palestine', code: 'PS' },
  { name: 'Panama', code: 'PA' },
  { name: 'Papua New Guinea', code: 'PG', alt: ['png'] },
  { name: 'Paraguay', code: 'PY' },
  { name: 'Peru', code: 'PE' },
  { name: 'Philippines', code: 'PH' },
  { name: 'Poland', code: 'PL' },
  { name: 'Portugal', code: 'PT' },
  { name: 'Qatar', code: 'QA' },
  { name: 'Romania', code: 'RO' },
  { name: 'Russia', code: 'RU' },
  { name: 'Rwanda', code: 'RW' },
  { name: 'Saint Kitts and Nevis', code: 'KN', alt: ['st kitts', 'st kitts and nevis'] },
  { name: 'Saint Lucia', code: 'LC', alt: ['st lucia'] },
  { name: 'Saint Vincent and the Grenadines', code: 'VC', alt: ['st vincent', 'saint vincent'] },
  { name: 'Samoa', code: 'WS' },
  { name: 'San Marino', code: 'SM' },
  { name: 'Sao Tome and Principe', code: 'ST' },
  { name: 'Saudi Arabia', code: 'SA' },
  { name: 'Senegal', code: 'SN' },
  { name: 'Serbia', code: 'RS' },
  { name: 'Seychelles', code: 'SC' },
  { name: 'Sierra Leone', code: 'SL' },
  { name: 'Singapore', code: 'SG' },
  { name: 'Slovakia', code: 'SK' },
  { name: 'Slovenia', code: 'SI' },
  { name: 'Solomon Islands', code: 'SB' },
  { name: 'Somalia', code: 'SO' },
  { name: 'South Africa', code: 'ZA' },
  { name: 'South Korea', code: 'KR', alt: ['korea'] },
  { name: 'South Sudan', code: 'SS' },
  { name: 'Spain', code: 'ES' },
  { name: 'Sri Lanka', code: 'LK' },
  { name: 'Sudan', code: 'SD' },
  { name: 'Suriname', code: 'SR' },
  { name: 'Sweden', code: 'SE' },
  { name: 'Switzerland', code: 'CH' },
  { name: 'Syria', code: 'SY' },
  { name: 'Taiwan', code: 'TW' },
  { name: 'Tajikistan', code: 'TJ' },
  { name: 'Tanzania', code: 'TZ' },
  { name: 'Thailand', code: 'TH' },
  { name: 'Togo', code: 'TG' },
  { name: 'Tonga', code: 'TO' },
  { name: 'Trinidad and Tobago', code: 'TT', alt: ['trinidad'] },
  { name: 'Tunisia', code: 'TN' },
  { name: 'Turkey', code: 'TR' },
  { name: 'Turkmenistan', code: 'TM' },
  { name: 'Tuvalu', code: 'TV' },
  { name: 'Uganda', code: 'UG' },
  { name: 'Ukraine', code: 'UA' },
  { name: 'United Arab Emirates', code: 'AE', alt: ['uae'] },
  { name: 'United Kingdom', code: 'GB', alt: ['uk', 'britain', 'great britain', 'england'] },
  { name: 'United States', code: 'US', alt: ['usa', 'america', 'united states of america'] },
  { name: 'Uruguay', code: 'UY' },
  { name: 'Uzbekistan', code: 'UZ' },
  { name: 'Vanuatu', code: 'VU' },
  { name: 'Vatican City', code: 'VA', alt: ['vatican', 'holy see'] },
  { name: 'Venezuela', code: 'VE' },
  { name: 'Vietnam', code: 'VN' },
  { name: 'Yemen', code: 'YE' },
  { name: 'Zambia', code: 'ZM' },
  { name: 'Zimbabwe', code: 'ZW' },
];

const ROUNDS_PER_GAME = 10;
const POINTS_CORRECT = 100;
const POINTS_SKIP = 0;

const MapsGame = ({ onGameEnd, onClose }) => {
  const [gameState, setGameState] = useState('menu'); // menu, playing, result, ended
  const [currentRound, setCurrentRound] = useState(0);
  const [score, setScore] = useState(0);
  const [answer, setAnswer] = useState('');
  const [showResult, setShowResult] = useState(false);
  const [isCorrect, setIsCorrect] = useState(false);
  const [roundCountries, setRoundCountries] = useState([]);
  const [results, setResults] = useState([]);
  const inputRef = useRef(null);

  // Select random countries for the game
  const startGame = useCallback(() => {
    const shuffled = [...COUNTRIES].sort(() => Math.random() - 0.5);
    setRoundCountries(shuffled.slice(0, ROUNDS_PER_GAME));
    setCurrentRound(0);
    setScore(0);
    setAnswer('');
    setShowResult(false);
    setResults([]);
    setGameState('playing');
  }, []);

  // Focus input when round starts
  useEffect(() => {
    if (gameState === 'playing' && !showResult && inputRef.current) {
      setTimeout(() => inputRef.current?.focus(), 100);
    }
  }, [gameState, showResult, currentRound]);

  // Check answer
  const checkAnswer = useCallback(() => {
    if (!answer.trim()) return;

    const currentCountry = roundCountries[currentRound];
    const userAnswer = answer.trim().toLowerCase();
    const correctName = currentCountry.name.toLowerCase();
    const altNames = currentCountry.alt || [];

    const correct = userAnswer === correctName || altNames.some(alt => alt.toLowerCase() === userAnswer);

    setIsCorrect(correct);
    setShowResult(true);

    if (correct) {
      setScore(s => s + POINTS_CORRECT);
    }

    setResults(prev => [...prev, {
      country: currentCountry,
      userAnswer: answer.trim(),
      correct
    }]);
  }, [answer, roundCountries, currentRound]);

  // Skip current round
  const skipRound = useCallback(() => {
    const currentCountry = roundCountries[currentRound];
    setIsCorrect(false);
    setShowResult(true);
    setResults(prev => [...prev, {
      country: currentCountry,
      userAnswer: '(skipped)',
      correct: false
    }]);
  }, [roundCountries, currentRound]);

  // Next round
  const nextRound = useCallback(() => {
    if (currentRound + 1 >= ROUNDS_PER_GAME) {
      setGameState('ended');
    } else {
      setCurrentRound(r => r + 1);
      setAnswer('');
      setShowResult(false);
    }
  }, [currentRound]);

  // Handle enter key
  const handleKeyPress = useCallback((e) => {
    if (e.key === 'Enter') {
      if (showResult) {
        nextRound();
      } else {
        checkAnswer();
      }
    }
  }, [showResult, nextRound, checkAnswer]);

  // Get flag URL
  const getFlagUrl = (code) => {
    return `https://flagcdn.com/w320/${code.toLowerCase()}.png`;
  };

  // Get country shape URL (using a simple silhouette service)
  const getCountryShapeUrl = (code) => {
    return `https://raw.githubusercontent.com/djaiss/mapsicon/master/all/${code.toLowerCase()}/vector.svg`;
  };

  const currentCountry = roundCountries[currentRound];

  return (
    <div className="maps-game-overlay">
      <div className="maps-game-container">
        {/* Menu */}
        {gameState === 'menu' && (
          <div className="maps-game-menu">
            <h1 className="maps-game-title">
              <span>🗺️</span> Maps <span>🌍</span>
            </h1>
            <p className="maps-game-subtitle">Can you identify the country?</p>

            <div className="maps-game-info">
              <div className="info-item">
                <span className="info-icon">🎯</span>
                <span>{ROUNDS_PER_GAME} Rounds</span>
              </div>
              <div className="info-item">
                <span className="info-icon">⭐</span>
                <span>{POINTS_CORRECT} pts each</span>
              </div>
              <div className="info-item">
                <span className="info-icon">🌐</span>
                <span>{COUNTRIES.length} Countries</span>
              </div>
            </div>

            <div className="maps-game-rules">
              <h3>How to Play</h3>
              <ul>
                <li>Look at the country shape</li>
                <li>Type your answer</li>
                <li>Not case-sensitive!</li>
              </ul>
            </div>

            <button className="maps-start-btn" onClick={startGame}>
              Start Game
            </button>

            <button className="maps-close-btn-menu" onClick={onClose}>
              Back to Games
            </button>
          </div>
        )}

        {/* Playing */}
        {gameState === 'playing' && currentCountry && (
          <div className="maps-game-playing">
            {/* Header */}
            <div className="maps-header">
              <div className="maps-stat">
                <span>🎯</span>
                <span>{currentRound + 1}/{ROUNDS_PER_GAME}</span>
              </div>
              <div className="maps-score">
                <span>⭐</span>
                <span>{score}</span>
              </div>
            </div>

            {/* Country Shape */}
            <div className="country-display">
              <div className="country-shape-container">
                <img
                  src={getCountryShapeUrl(currentCountry.code)}
                  alt="Country shape"
                  className="country-shape"
                  onError={(e) => {
                    e.target.style.display = 'none';
                    e.target.nextSibling.style.display = 'flex';
                  }}
                />
                <div className="country-shape-fallback" style={{ display: 'none' }}>
                  <span className="fallback-icon">🗺️</span>
                  <span className="fallback-text">Shape unavailable</span>
                  <span className="hint-text">Hint: {currentCountry.code}</span>
                </div>
              </div>
            </div>

            {/* Answer Input */}
            {!showResult ? (
              <div className="answer-section">
                <input
                  ref={inputRef}
                  type="text"
                  className="answer-input"
                  placeholder="Type country name..."
                  value={answer}
                  onChange={(e) => setAnswer(e.target.value)}
                  onKeyPress={handleKeyPress}
                  autoComplete="off"
                  autoCapitalize="off"
                />
                <div className="answer-buttons">
                  <button className="submit-btn" onClick={checkAnswer} disabled={!answer.trim()}>
                    Submit
                  </button>
                  <button className="skip-btn" onClick={skipRound}>
                    Skip
                  </button>
                </div>
              </div>
            ) : (
              <div className={`result-section ${isCorrect ? 'correct' : 'incorrect'}`}>
                <div className="result-icon">
                  {isCorrect ? '✅' : '❌'}
                </div>
                <div className="result-text">
                  {isCorrect ? 'Correct!' : 'Incorrect'}
                </div>
                <div className="correct-answer">
                  <img src={getFlagUrl(currentCountry.code)} alt="Flag" className="result-flag" />
                  <span>{currentCountry.name}</span>
                </div>
                <button className="next-btn" onClick={nextRound}>
                  {currentRound + 1 >= ROUNDS_PER_GAME ? 'See Results' : 'Next Country'}
                </button>
              </div>
            )}

            {/* Close button */}
            <button className="maps-close-btn" onClick={onClose}>✕</button>
          </div>
        )}

        {/* Game Over */}
        {gameState === 'ended' && (
          <div className="maps-game-over">
            <div className="maps-game-over-content">
              <h2>
                <span>🏆</span> Game Complete! <span>🏆</span>
              </h2>

              <div className="final-score-display">
                <div className="score-circle">
                  <span className="score-number">{results.filter(r => r.correct).length}</span>
                  <span className="score-total">/{ROUNDS_PER_GAME}</span>
                </div>
                <div className="score-label">Correct</div>
              </div>

              <div className="maps-final-score">
                Score: {score}
              </div>

              {/* Results Summary */}
              <div className="results-summary">
                {results.map((result, i) => (
                  <div key={i} className={`result-row ${result.correct ? 'correct' : 'incorrect'}`}>
                    <img src={getFlagUrl(result.country.code)} alt="" className="result-mini-flag" />
                    <span className="result-country-name">{result.country.name}</span>
                    <span className="result-status">{result.correct ? '✓' : '✗'}</span>
                  </div>
                ))}
              </div>

              <div className="maps-game-over-buttons">
                <button className="maps-claim-btn" onClick={() => onGameEnd(score)}>
                  Claim {score} Coins! 🪙
                </button>
                <button className="maps-retry-btn" onClick={startGame}>
                  Play Again
                </button>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default MapsGame;
