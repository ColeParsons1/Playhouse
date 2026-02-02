import React, { useState, useEffect, useCallback, useRef } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import YouTubeMusicPlayer from './YouTubeMusicPlayer';
import './VoiceRoomGame.css';

// WebRTC configuration with our own TURN server for reliable cross-network connectivity
const RTC_CONFIG = {
  iceServers: [
    // Google STUN servers (free, reliable)
    { urls: 'stun:stun.l.google.com:19302' },
    { urls: 'stun:stun1.l.google.com:19302' },
    { urls: 'stun:stun2.l.google.com:19302' },
    // Our own TURN server (for NAT traversal across different networks)
    {
      urls: 'turn:GAME_SERVER_IP:3478',
      username: 'playhouse',
      credential: 'PLACEHOLDER_CREDENTIAL'
    },
    {
      urls: 'turn:GAME_SERVER_IP:3478?transport=tcp',
      username: 'playhouse',
      credential: 'PLACEHOLDER_CREDENTIAL'
    },
  ],
  iceCandidatePoolSize: 10,
  iceTransportPolicy: 'all',
  bundlePolicy: 'max-bundle',
  rtcpMuxPolicy: 'require'
};

// Generate a random bingo card
const generateBingoCard = () => {
  const card = [];
  const ranges = [
    [1, 15],   // B
    [16, 30],  // I
    [31, 45],  // N
    [46, 60],  // G
    [61, 75],  // O
  ];

  for (let col = 0; col < 5; col++) {
    const colNumbers = [];
    const [min, max] = ranges[col];
    const available = Array.from({ length: max - min + 1 }, (_, i) => min + i);

    for (let row = 0; row < 5; row++) {
      if (col === 2 && row === 2) {
        colNumbers.push('FREE');
      } else {
        const randomIndex = Math.floor(Math.random() * available.length);
        colNumbers.push(available.splice(randomIndex, 1)[0]);
      }
    }
    card.push(colNumbers);
  }

  return card;
};

// Get WebSocket URL based on environment
const getWebSocketUrl = (roomId, playerId) => {
  const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
  const host = window.location.host;
  // For production, use the same host; for local, it might be different
  if (host.includes('localhost') || host.includes('127.0.0.1')) {
    return `ws://localhost:8000/ws/room/${roomId}/${playerId}/`;
  }
  return `${protocol}//${host}/ws/room/${roomId}/${playerId}/`;
};

const VoiceRoomGame = ({ room, player, onLeave }) => {
  const [gameState, setGameState] = useState('waiting'); // waiting, countdown, playing, ended
  const [bingoCards, setBingoCards] = useState([]); // Array of cards
  const [markedNumbersByCard, setMarkedNumbersByCard] = useState([new Set(['FREE'])]); // Array of Sets
  const [calledNumbers, setCalledNumbers] = useState([]);
  const [currentNumber, setCurrentNumber] = useState(null);
  const [players, setPlayers] = useState([]);
  const [chat, setChat] = useState([]);
  const [newMessage, setNewMessage] = useState('');
  const [showChat, setShowChat] = useState(false);
  const [countdown, setCountdown] = useState(null);
  const [prizePool] = useState(room?.first_prize || 500); // Reduced from 2500
  const [packDropActive, setPackDropActive] = useState(false);
  const [packs, setPacks] = useState([]);
  const [packCoinsEarned, setPackCoinsEarned] = useState(0);
  const [isMuted, setIsMuted] = useState(false);
  const [purchasedCards, setPurchasedCards] = useState(1);
  const [showBuyCards, setShowBuyCards] = useState(false);
  const [isConnected, setIsConnected] = useState(false);
  const [connectionError, setConnectionError] = useState(null);
  const [myProgress, setMyProgress] = useState(0);
  const [activeCardIndex, setActiveCardIndex] = useState(0); // For card selector
  const [voiceStatus, setVoiceStatus] = useState('initializing'); // initializing, ready, connected, error
  const [connectedPeers, setConnectedPeers] = useState(0);

  // Check if current player is the host
  // Handles both cases: when room has host object (created locally) or host_id (from API)
  const isHost = room?.host?.id === player?.id ||
                 room?.host_id === player?.id ||
                 room?.host === player?.id;

  const wsRef = useRef(null);
  const chatRef = useRef(null);
  const autoCallRef = useRef(null);
  const countdownRef = useRef(null);
  const packDropRef = useRef(null); // For pack drop animation
  const musicPlayerRef = useRef(null); // For YouTube music player

  // Music player state
  const [musicMinimized, setMusicMinimized] = useState(true);
  const [showMusicPanel, setShowMusicPanel] = useState(false);

  // Theme state
  const [roomTheme, setRoomTheme] = useState(room?.theme || 'default');
  const [showThemeSelector, setShowThemeSelector] = useState(false);

  // Sync theme when room data loads/updates
  useEffect(() => {
    if (room?.theme) {
      setRoomTheme(room.theme);
    }
  }, [room?.theme]);

  // Available themes
  const themes = [
    { id: 'default', name: 'Default' },
    { id: 'sunset', name: 'Sunset' },
    { id: 'ocean', name: 'Ocean' },
    { id: 'forest', name: 'Forest' },
    { id: 'midnight', name: 'Midnight' },
    { id: 'cherry', name: 'Cherry Blossom' },
    { id: 'neon', name: 'Neon' },
    { id: 'galaxy', name: 'Galaxy' },
    { id: 'fire', name: 'Fire' },
    { id: 'ice', name: 'Ice' },
    { id: 'theater', name: 'Theater' },
    { id: 'anime_theater', name: 'Anime Theater' },
    { id: 'fantasy', name: 'Fantasy Castle' },
    { id: 'underwater', name: 'Under the Sea' },
    { id: 'hearts', name: 'Heart Clouds' },
  ];

  // WebRTC refs
  const localStreamRef = useRef(null);
  const peerConnectionsRef = useRef({}); // { peerId: RTCPeerConnection }
  const audioElementsRef = useRef({}); // { peerId: HTMLAudioElement }

  // Refs for WebRTC handlers to avoid circular dependencies
  const handlePeerReadyRef = useRef(null);
  const handleWebRTCOfferRef = useRef(null);
  const handleWebRTCAnswerRef = useRef(null);
  const handleWebRTCIceCandidateRef = useRef(null);
  const cleanupPeerConnectionRef = useRef(null);

  // Initialize bingo cards
  useEffect(() => {
    setBingoCards([generateBingoCard()]);
    setMarkedNumbersByCard([new Set(['FREE'])]);
  }, []);

  // Speech synthesis state
  const [speechReady, setSpeechReady] = useState(false);
  const voicesLoadedRef = useRef(false);

  // Preload speech synthesis voices (they load asynchronously in some browsers)
  useEffect(() => {
    if (window.speechSynthesis) {
      // Load voices
      const loadVoices = () => {
        const voices = window.speechSynthesis.getVoices();
        if (voices.length > 0) {
          voicesLoadedRef.current = true;
          console.log('Speech voices loaded:', voices.length);
        }
      };

      loadVoices();
      // Some browsers need this event listener
      window.speechSynthesis.onvoiceschanged = loadVoices;
    }
  }, []);

  // Initialize speech on user interaction (required for iOS/Safari)
  const initializeSpeech = useCallback(() => {
    if (speechReady || !window.speechSynthesis) return;

    // Speak a silent utterance to unlock audio on iOS
    const utterance = new SpeechSynthesisUtterance('');
    utterance.volume = 0;
    window.speechSynthesis.speak(utterance);
    setSpeechReady(true);
    console.log('Speech synthesis initialized');
  }, [speechReady]);

  // WebSocket connection
  useEffect(() => {
    if (!room?.room_id || !player?.id) return;

    const wsUrl = getWebSocketUrl(room.room_id, player.id);
    console.log('Connecting to WebSocket:', wsUrl);

    const ws = new WebSocket(wsUrl);
    wsRef.current = ws;

    ws.onopen = () => {
      console.log('WebSocket connected');
      setIsConnected(true);
      setConnectionError(null);

      // Add self to players list
      setPlayers(prev => {
        const exists = prev.some(p => p.id === player.id);
        if (!exists) {
          return [...prev, {
            id: player.id,
            name: player.name,
            avatar: '👤',
            isHost: false,
            isSpeaking: false,
            progress: 0
          }];
        }
        return prev;
      });

      // Add system message
      setChat(prev => [...prev, {
        id: Date.now(),
        player: 'System',
        message: 'Connected to room!',
        isSystem: true
      }]);
    };

    ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      handleWebSocketMessage(data);
    };

    ws.onclose = () => {
      console.log('WebSocket disconnected');
      setIsConnected(false);
    };

    ws.onerror = (error) => {
      console.error('WebSocket error:', error);
      setConnectionError('Connection failed. Using offline mode.');
      // Fall back to mock mode
      initializeMockMode();
    };

    return () => {
      if (ws.readyState === WebSocket.OPEN) {
        ws.close();
      }
    };
  }, [room?.room_id, player?.id]);

  // Handle incoming WebSocket messages
  const handleWebSocketMessage = useCallback((data) => {
    switch (data.type) {
      case 'player_joined':
        setPlayers(prev => {
          const exists = prev.some(p => p.id === data.player?.id);
          if (!exists && data.player) {
            return [...prev, {
              ...data.player,
              isSpeaking: false,
              progress: 0
            }];
          }
          return prev;
        });
        // Sync theme from room data when joining
        if (data.room?.theme) {
          setRoomTheme(data.room.theme);
        }
        setChat(prev => [...prev, {
          id: Date.now(),
          player: 'System',
          message: `${data.player?.name || 'Someone'} joined the room!`,
          isSystem: true
        }]);
        break;

      case 'player_left':
        if (data.player?.id && cleanupPeerConnectionRef.current) {
          cleanupPeerConnectionRef.current(data.player.id);
        }
        setPlayers(prev => prev.filter(p => p.id !== data.player?.id));
        setChat(prev => [...prev, {
          id: Date.now(),
          player: 'System',
          message: `${data.player?.name || 'Someone'} left the room`,
          isSystem: true
        }]);
        break;

      case 'chat_message':
        setChat(prev => [...prev, {
          id: Date.now(),
          player: data.player?.name || 'Unknown',
          message: data.message,
          isSystem: data.is_system
        }]);
        break;

      case 'game_starting':
        setCountdown(data.countdown);
        setGameState('countdown');
        break;

      case 'number_called':
        setCurrentNumber(data.number);
        setCalledNumbers(data.called_numbers || []);
        // Announce the number for non-host players receiving via WebSocket
        if (data.number && window.speechSynthesis) {
          window.speechSynthesis.cancel();
          const letter = data.number <= 15 ? 'B' : data.number <= 30 ? 'I' : data.number <= 45 ? 'N' : data.number <= 60 ? 'G' : 'O';
          const utterance = new SpeechSynthesisUtterance(`${letter} ${data.number}`);
          utterance.rate = 0.9;
          utterance.pitch = 1.0;
          utterance.volume = 1.0;
          const voices = window.speechSynthesis.getVoices();
          const preferredVoice = voices.find(v => v.lang.startsWith('en')) || voices[0];
          if (preferredVoice) utterance.voice = preferredVoice;
          window.speechSynthesis.speak(utterance);
        }
        break;

      case 'progress_update':
        setPlayers(prev => prev.map(p =>
          p.id === data.player?.id
            ? { ...p, progress: data.progress }
            : p
        ));
        break;

      case 'bingo_claimed':
        if (data.valid) {
          setGameState('ended');
          setChat(prev => [...prev, {
            id: Date.now(),
            player: 'System',
            message: `🎉 ${data.player?.name || 'Someone'} got BINGO! 🎉`,
            isSystem: true
          }]);
        }
        break;

      case 'mute_changed':
        setPlayers(prev => prev.map(p =>
          p.id === data.player?.id
            ? { ...p, isMuted: data.is_muted }
            : p
        ));
        break;

      // WebRTC signaling messages
      case 'webrtc_ready':
        if (data.player && data.player.id !== player?.id && handlePeerReadyRef.current) {
          handlePeerReadyRef.current(data.player);
        }
        break;

      case 'webrtc_offer':
        if (data.from_player && handleWebRTCOfferRef.current) {
          handleWebRTCOfferRef.current(data.from_player, data.offer);
        }
        break;

      case 'webrtc_answer':
        if (data.from_player && handleWebRTCAnswerRef.current) {
          handleWebRTCAnswerRef.current(data.from_player, data.answer);
        }
        break;

      case 'webrtc_ice_candidate':
        if (data.from_player && handleWebRTCIceCandidateRef.current) {
          handleWebRTCIceCandidateRef.current(data.from_player, data.candidate);
        }
        break;

      // Music sync messages
      case 'music_sync':
        if (musicPlayerRef.current && data.action) {
          musicPlayerRef.current.handleSyncMessage(data);
        }
        break;

      // Theme change
      case 'theme_change':
        if (data.theme) {
          setRoomTheme(data.theme);
        }
        break;

      // Pack Drop feature
      case 'pack_drop':
        // Pause bingo calling
        if (autoCallRef.current) {
          clearInterval(autoCallRef.current);
        }
        // Start pack drop with received packs
        setPacks(data.packs || []);
        setPackDropActive(true);
        setPackCoinsEarned(0);
        setChat(prev => [...prev, {
          id: Date.now(),
          player: 'System',
          message: '📦 PACK DROP! Tap the falling packs to earn coins!',
          isSystem: true
        }]);
        // End after 15 seconds (extended from 8)
        packDropRef.current = setTimeout(() => {
          setPackDropActive(false);
          setPacks([]);
        }, 15000);
        break;

      default:
        console.log('Unknown message type:', data.type);
    }
  }, [player?.id]);

  // Initialize mock mode for offline/fallback
  const initializeMockMode = useCallback(() => {
    setPlayers([
      { id: player?.id || 1, name: player?.name || 'You', avatar: '👤', isHost: false, isSpeaking: false, progress: 0 },
      { id: 99, name: 'AI Player', avatar: '🤖', isHost: true, isSpeaking: true, progress: 0 },
      { id: 100, name: 'Bot1', avatar: '🎮', isHost: false, isSpeaking: false, progress: 0 },
    ]);
    setChat([{
      id: Date.now(),
      player: 'System',
      message: 'Running in offline mode',
      isSystem: true
    }]);
  }, [player]);

  // ====== WebRTC Voice Chat Functions ======

  // Initialize local microphone stream
  const initializeLocalStream = useCallback(async () => {
    setVoiceStatus('initializing');
    try {
      console.log('Requesting microphone access...');
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: true,
          noiseSuppression: true,
          autoGainControl: true
        },
        video: false
      });
      localStreamRef.current = stream;
      console.log('Microphone initialized successfully');
      setVoiceStatus('ready');

      // If muted by default, disable audio tracks
      if (isMuted) {
        stream.getAudioTracks().forEach(track => {
          track.enabled = false;
        });
      }

      setChat(prev => [...prev, {
        id: Date.now(),
        player: 'System',
        message: '🎤 Microphone connected! Voice chat is ready.',
        isSystem: true
      }]);

      // Announce we're ready for WebRTC connections
      // Send multiple times with delay to ensure other peers receive it
      if (wsRef.current?.readyState === WebSocket.OPEN) {
        console.log('Sending webrtc_ready message');
        wsRef.current.send(JSON.stringify({ type: 'webrtc_ready' }));

        // Send again after a short delay in case other player is still initializing
        setTimeout(() => {
          if (wsRef.current?.readyState === WebSocket.OPEN) {
            console.log('Sending webrtc_ready message (retry)');
            wsRef.current.send(JSON.stringify({ type: 'webrtc_ready' }));
          }
        }, 2000);
      }

      return stream;
    } catch (error) {
      console.error('Failed to get microphone access:', error);
      setVoiceStatus('error');
      setChat(prev => [...prev, {
        id: Date.now(),
        player: 'System',
        message: '❌ Microphone access denied. Voice chat unavailable.',
        isSystem: true
      }]);
      return null;
    }
  }, [isMuted]);

  // Create a peer connection for a specific peer
  const createPeerConnection = useCallback((peerId, isInitiator) => {
    if (peerConnectionsRef.current[peerId]) {
      console.log('Peer connection already exists for:', peerId);
      return peerConnectionsRef.current[peerId];
    }

    console.log(`Creating peer connection for ${peerId}, initiator: ${isInitiator}`);
    const pc = new RTCPeerConnection(RTC_CONFIG);

    // Add local audio track to the connection
    if (localStreamRef.current) {
      console.log('Adding local tracks to peer connection');
      localStreamRef.current.getTracks().forEach(track => {
        pc.addTrack(track, localStreamRef.current);
      });
    } else {
      console.warn('No local stream available when creating peer connection');
    }

    // Handle incoming audio stream from peer
    pc.ontrack = (event) => {
      console.log('Received remote track from:', peerId, event.streams);
      const remoteStream = event.streams[0];

      // Create or reuse audio element for this peer
      if (!audioElementsRef.current[peerId]) {
        const audio = new Audio();
        audio.autoplay = true;
        audio.playsInline = true;
        audio.volume = 1.0;
        audioElementsRef.current[peerId] = audio;
      }

      audioElementsRef.current[peerId].srcObject = remoteStream;
      audioElementsRef.current[peerId].play()
        .then(() => console.log('Audio playback started for peer:', peerId))
        .catch(e => console.error('Audio play error:', e));

      // Update player speaking state based on audio activity
      setPlayers(prev => prev.map(p =>
        p.id === peerId ? { ...p, isSpeaking: true } : p
      ));

      setChat(prev => [...prev, {
        id: Date.now(),
        player: 'System',
        message: `🔊 Voice connected with another player!`,
        isSystem: true
      }]);
    };

    // Handle ICE candidates
    pc.onicecandidate = (event) => {
      if (event.candidate) {
        console.log('ICE candidate generated for peer:', peerId);
        if (wsRef.current?.readyState === WebSocket.OPEN) {
          wsRef.current.send(JSON.stringify({
            type: 'webrtc_ice_candidate',
            target_id: peerId,
            candidate: event.candidate
          }));
        }
      }
    };

    // Track retry attempts and timeout
    let retryCount = 0;
    const maxRetries = 2;
    let connectionTimeout = null;

    // Set a timeout for the connection attempt
    connectionTimeout = setTimeout(() => {
      if (pc.iceConnectionState === 'checking' || pc.iceConnectionState === 'new') {
        console.log('Connection timeout reached for peer:', peerId);
        setVoiceStatus('error');
        setChat(prev => [...prev, {
          id: Date.now(),
          player: 'System',
          message: '⏱️ Voice connection timed out. Your networks may be incompatible for direct voice chat.',
          isSystem: true
        }]);
      }
    }, 15000); // 15 second timeout

    // Handle ICE connection state changes
    pc.oniceconnectionstatechange = () => {
      console.log(`ICE connection state with ${peerId}:`, pc.iceConnectionState);
      if (pc.iceConnectionState === 'connected' || pc.iceConnectionState === 'completed') {
        clearTimeout(connectionTimeout);
        retryCount = 0; // Reset retries on success
        setVoiceStatus('connected');
        setConnectedPeers(prev => prev + 1);
        setChat(prev => [...prev, {
          id: Date.now(),
          player: 'System',
          message: '✅ Voice connected! You can now talk!',
          isSystem: true
        }]);
      } else if (pc.iceConnectionState === 'checking') {
        console.log('ICE checking - waiting for connection...');
      } else if (pc.iceConnectionState === 'failed') {
        clearTimeout(connectionTimeout);
        console.error('ICE connection failed with peer:', peerId);
        retryCount++;

        if (retryCount <= maxRetries) {
          setChat(prev => [...prev, {
            id: Date.now(),
            player: 'System',
            message: `⚠️ Retrying voice connection (${retryCount}/${maxRetries})...`,
            isSystem: true
          }]);
          // Try to restart ICE
          pc.restartIce();
        } else {
          setVoiceStatus('error');
          setChat(prev => [...prev, {
            id: Date.now(),
            player: 'System',
            message: '❌ Voice chat unavailable. Try being on the same WiFi network.',
            isSystem: true
          }]);
        }
      } else if (pc.iceConnectionState === 'disconnected') {
        console.log('ICE disconnected, may reconnect...');
      }
    };

    // Handle ICE gathering state
    pc.onicegatheringstatechange = () => {
      console.log(`ICE gathering state with ${peerId}:`, pc.iceGatheringState);
    };

    // Handle connection state changes
    pc.onconnectionstatechange = () => {
      console.log(`Connection state with ${peerId}:`, pc.connectionState);
      if (pc.connectionState === 'disconnected' || pc.connectionState === 'failed') {
        setConnectedPeers(prev => Math.max(0, prev - 1));
        cleanupPeerConnectionRef.current?.(peerId);
      }
    };

    peerConnectionsRef.current[peerId] = pc;
    return pc;
  }, []);

  // Cleanup a specific peer connection
  const cleanupPeerConnection = useCallback((peerId) => {
    const pc = peerConnectionsRef.current[peerId];
    if (pc) {
      pc.close();
      delete peerConnectionsRef.current[peerId];
    }

    const audio = audioElementsRef.current[peerId];
    if (audio) {
      audio.srcObject = null;
      delete audioElementsRef.current[peerId];
    }

    setPlayers(prev => prev.map(p =>
      p.id === peerId ? { ...p, isSpeaking: false } : p
    ));
  }, []);

  // Cleanup all WebRTC resources
  const cleanupAllWebRTC = useCallback(() => {
    // Stop local stream
    if (localStreamRef.current) {
      localStreamRef.current.getTracks().forEach(track => track.stop());
      localStreamRef.current = null;
    }

    // Close all peer connections
    Object.keys(peerConnectionsRef.current).forEach(peerId => {
      cleanupPeerConnection(peerId);
    });
  }, [cleanupPeerConnection]);

  // Handle incoming WebRTC offer
  const handleWebRTCOffer = useCallback(async (fromPlayer, offer) => {
    console.log('Received offer from:', fromPlayer.id, fromPlayer.name);

    // Wait for local stream if not ready
    if (!localStreamRef.current) {
      console.log('Waiting for local stream before handling offer...');
      // Retry after a short delay
      setTimeout(() => {
        if (handleWebRTCOfferRef.current) {
          handleWebRTCOfferRef.current(fromPlayer, offer);
        }
      }, 1000);
      return;
    }

    const pc = createPeerConnection(fromPlayer.id, false);

    try {
      await pc.setRemoteDescription(new RTCSessionDescription(offer));
      const answer = await pc.createAnswer();
      await pc.setLocalDescription(answer);

      console.log('Sending answer to:', fromPlayer.id);
      if (wsRef.current?.readyState === WebSocket.OPEN) {
        wsRef.current.send(JSON.stringify({
          type: 'webrtc_answer',
          target_id: fromPlayer.id,
          answer: answer
        }));
      }

      setChat(prev => [...prev, {
        id: Date.now(),
        player: 'System',
        message: `📞 Connecting voice with ${fromPlayer.name}...`,
        isSystem: true
      }]);
    } catch (error) {
      console.error('Error handling offer:', error);
    }
  }, [createPeerConnection]);

  // Handle incoming WebRTC answer
  const handleWebRTCAnswer = useCallback(async (fromPlayer, answer) => {
    console.log('Received answer from:', fromPlayer.id, fromPlayer.name);
    const pc = peerConnectionsRef.current[fromPlayer.id];

    if (pc) {
      try {
        await pc.setRemoteDescription(new RTCSessionDescription(answer));
        console.log('Answer processed successfully, connection should establish');

        setChat(prev => [...prev, {
          id: Date.now(),
          player: 'System',
          message: `🤝 Voice handshake complete with ${fromPlayer.name}!`,
          isSystem: true
        }]);
      } catch (error) {
        console.error('Error handling answer:', error);
      }
    } else {
      console.error('No peer connection found for:', fromPlayer.id);
    }
  }, []);

  // Handle incoming ICE candidate
  const handleWebRTCIceCandidate = useCallback(async (fromPlayer, candidate) => {
    const pc = peerConnectionsRef.current[fromPlayer.id];

    if (pc && candidate) {
      try {
        await pc.addIceCandidate(new RTCIceCandidate(candidate));
      } catch (error) {
        console.error('Error adding ICE candidate:', error);
      }
    }
  }, []);

  // Handle new peer announcing they're ready
  const handlePeerReady = useCallback(async (peerPlayer) => {
    if (peerPlayer.id === player?.id) return; // Skip self

    console.log('Peer ready:', peerPlayer.name, 'Local stream:', !!localStreamRef.current);

    // Only initiate if we have our local stream ready
    if (!localStreamRef.current) {
      console.log('Local stream not ready yet, waiting...');
      return;
    }

    // Check if we already have a connection to this peer
    if (peerConnectionsRef.current[peerPlayer.id]) {
      console.log('Already have connection to peer:', peerPlayer.id);
      return;
    }

    // As the existing peer, initiate connection to new peer
    const pc = createPeerConnection(peerPlayer.id, true);

    try {
      const offer = await pc.createOffer();
      await pc.setLocalDescription(offer);

      console.log('Sending offer to peer:', peerPlayer.id);
      if (wsRef.current?.readyState === WebSocket.OPEN) {
        wsRef.current.send(JSON.stringify({
          type: 'webrtc_offer',
          target_id: peerPlayer.id,
          offer: offer
        }));
      }
    } catch (error) {
      console.error('Error creating offer:', error);
    }
  }, [createPeerConnection, player?.id]);

  // Keep refs updated with latest handlers
  useEffect(() => {
    handlePeerReadyRef.current = handlePeerReady;
    handleWebRTCOfferRef.current = handleWebRTCOffer;
    handleWebRTCAnswerRef.current = handleWebRTCAnswer;
    handleWebRTCIceCandidateRef.current = handleWebRTCIceCandidate;
    cleanupPeerConnectionRef.current = cleanupPeerConnection;
  }, [handlePeerReady, handleWebRTCOffer, handleWebRTCAnswer, handleWebRTCIceCandidate, cleanupPeerConnection]);

  // Initialize microphone when connected
  useEffect(() => {
    if (isConnected && !localStreamRef.current) {
      initializeLocalStream();
    }
  }, [isConnected, initializeLocalStream]);

  // Cleanup WebRTC on unmount
  useEffect(() => {
    return () => {
      cleanupAllWebRTC();
    };
  }, [cleanupAllWebRTC]);

  // Auto-scroll chat
  useEffect(() => {
    if (chatRef.current) {
      chatRef.current.scrollTop = chatRef.current.scrollHeight;
    }
  }, [chat]);

  // Host manually starts the game
  const handleStartGame = useCallback(() => {
    if (!isHost || gameState !== 'waiting') return;

    // Initialize speech synthesis on user interaction (required for iOS)
    initializeSpeech();

    setCountdown(10);
    setGameState('countdown');

    // Broadcast game starting
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'start_game'
      }));
    }

    // Add system message
    setChat(prev => [...prev, {
      id: Date.now(),
      player: 'System',
      message: '🎮 The host has started the game! Get ready!',
      isSystem: true
    }]);
  }, [isHost, gameState, initializeSpeech]);

  // Get letter for bingo number (defined before announceBingoNumber which uses it)
  const getBingoLetter = (num) => {
    if (num <= 15) return 'B';
    if (num <= 30) return 'I';
    if (num <= 45) return 'N';
    if (num <= 60) return 'G';
    return 'O';
  };

  // Text-to-speech for bingo number announcements (defined before startCallingNumbers which uses it)
  const announceBingoNumber = useCallback((number) => {
    if (!window.speechSynthesis) {
      console.log('Speech synthesis not available');
      return;
    }

    try {
      // Cancel any ongoing speech
      window.speechSynthesis.cancel();

      const letter = getBingoLetter(number);
      const announcement = `${letter}... ${number}`;

      console.log('Announcing:', announcement);

      const utterance = new SpeechSynthesisUtterance(announcement);
      utterance.rate = 0.85; // Slightly slower for clarity
      utterance.pitch = 1.1; // Slightly higher pitch for better audibility
      utterance.volume = 1.0;

      // Try to get a good voice (prefer English voices)
      const voices = window.speechSynthesis.getVoices();
      console.log('Available voices:', voices.length);

      if (voices.length > 0) {
        // Prefer female English voices for clarity
        const preferredVoice = voices.find(voice =>
          voice.lang.startsWith('en') && (voice.name.includes('Samantha') || voice.name.includes('Karen') || voice.name.includes('Female'))
        ) || voices.find(voice =>
          voice.lang.startsWith('en-US')
        ) || voices.find(voice =>
          voice.lang.startsWith('en')
        ) || voices[0];

        if (preferredVoice) {
          utterance.voice = preferredVoice;
          console.log('Using voice:', preferredVoice.name);
        }
      }

      // Add event handlers for debugging
      utterance.onstart = () => console.log('Speech started');
      utterance.onend = () => console.log('Speech ended');
      utterance.onerror = (e) => console.error('Speech error:', e);

      window.speechSynthesis.speak(utterance);
    } catch (error) {
      console.error('Error in announceBingoNumber:', error);
    }
  }, []);

  // Start calling numbers (uses announceBingoNumber, so must be defined after it)
  const startCallingNumbers = useCallback(() => {
    const allNumbers = Array.from({ length: 75 }, (_, i) => i + 1);
    let numbersToCall = [...allNumbers];
    let called = [];

    const callNumber = () => {
      if (numbersToCall.length === 0 || gameState === 'ended') {
        clearInterval(autoCallRef.current);
        return;
      }

      const randomIndex = Math.floor(Math.random() * numbersToCall.length);
      const newNumber = numbersToCall.splice(randomIndex, 1)[0];
      called.push(newNumber);

      setCurrentNumber(newNumber);
      setCalledNumbers([...called]);

      // Announce the number with voice
      announceBingoNumber(newNumber);

      // Broadcast number called
      if (wsRef.current?.readyState === WebSocket.OPEN) {
        wsRef.current.send(JSON.stringify({
          type: 'call_number',
          number: newNumber,
          called_numbers: called
        }));
      }

      // Simulate AI progress
      setPlayers(prev => prev.map(p =>
        p.id !== player?.id && Math.random() > 0.6
          ? { ...p, progress: Math.min(5, p.progress + 1) }
          : p
      ));
    };

    // Call first number immediately
    callNumber();

    // Then call every 5 seconds
    autoCallRef.current = setInterval(callNumber, 5000);

    return () => clearInterval(autoCallRef.current);
  }, [gameState, player?.id, announceBingoNumber]);

  // Countdown logic
  useEffect(() => {
    if (countdown === null || countdown < 0) return;

    if (countdown > 0) {
      countdownRef.current = setTimeout(() => setCountdown(countdown - 1), 1000);
      return () => clearTimeout(countdownRef.current);
    } else if (countdown === 0) {
      setGameState('playing');
      setCountdown(null);
      startCallingNumbers();
    }
  }, [countdown, startCallingNumbers]);

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (autoCallRef.current) clearInterval(autoCallRef.current);
      if (countdownRef.current) clearTimeout(countdownRef.current);
    };
  }, []);

  // Handle marking a number on a specific card
  const handleMarkNumber = (cardIndex, number) => {
    // Initialize speech on first tap (required for iOS)
    initializeSpeech();

    if (number === 'FREE' || calledNumbers.includes(number)) {
      setMarkedNumbersByCard(prev => {
        const newMarkedNumbers = [...prev];
        newMarkedNumbers[cardIndex] = new Set([...prev[cardIndex], number]);

        // Calculate best progress across all cards
        let bestProgress = 0;
        bingoCards.forEach((card, idx) => {
          const progress = calculateProgressForCard(card, newMarkedNumbers[idx]);
          bestProgress = Math.max(bestProgress, progress);
        });
        setMyProgress(bestProgress);

        // Broadcast progress update
        if (wsRef.current?.readyState === WebSocket.OPEN) {
          wsRef.current.send(JSON.stringify({
            type: 'player_progress',
            progress: bestProgress
          }));
        }

        return newMarkedNumbers;
      });
    }
  };

  // Calculate progress for a specific card
  const calculateProgressForCard = (card, marked) => {
    if (!card || !marked) return 0;

    let maxInLine = 0;

    // Check rows
    for (let row = 0; row < 5; row++) {
      let count = 0;
      for (let col = 0; col < 5; col++) {
        if (marked.has(card[col][row])) count++;
      }
      maxInLine = Math.max(maxInLine, count);
    }

    // Check columns
    for (let col = 0; col < 5; col++) {
      let count = 0;
      for (let row = 0; row < 5; row++) {
        if (marked.has(card[col][row])) count++;
      }
      maxInLine = Math.max(maxInLine, count);
    }

    // Check diagonals
    let diag1 = 0, diag2 = 0;
    for (let i = 0; i < 5; i++) {
      if (marked.has(card[i][i])) diag1++;
      if (marked.has(card[i][4 - i])) diag2++;
    }
    maxInLine = Math.max(maxInLine, diag1, diag2);

    return maxInLine;
  };

  // Check for bingo on a specific card
  const checkBingoForCard = (card, marked) => {
    if (!card || !marked) return false;

    // Check rows
    for (let row = 0; row < 5; row++) {
      let rowComplete = true;
      for (let col = 0; col < 5; col++) {
        if (!marked.has(card[col][row])) {
          rowComplete = false;
          break;
        }
      }
      if (rowComplete) return true;
    }

    // Check columns
    for (let col = 0; col < 5; col++) {
      let colComplete = true;
      for (let row = 0; row < 5; row++) {
        if (!marked.has(card[col][row])) {
          colComplete = false;
          break;
        }
      }
      if (colComplete) return true;
    }

    // Check diagonals
    let diag1 = true, diag2 = true;
    for (let i = 0; i < 5; i++) {
      if (!marked.has(card[i][i])) diag1 = false;
      if (!marked.has(card[i][4 - i])) diag2 = false;
    }

    return diag1 || diag2;
  };

  // Check if any card has bingo
  const checkBingo = () => {
    return bingoCards.some((card, idx) =>
      checkBingoForCard(card, markedNumbersByCard[idx])
    );
  };

  // Handle claiming bingo
  const handleClaimBingo = () => {
    if (!checkBingo()) return;

    // Stop auto-calling
    if (autoCallRef.current) clearInterval(autoCallRef.current);

    // Broadcast bingo claim
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'claim_bingo',
        valid: true
      }));
    }

    setGameState('ended');
    setChat(prev => [...prev, {
      id: Date.now(),
      player: 'System',
      message: `🎉 ${player?.name || 'You'} got BINGO! 🎉`,
      isSystem: true
    }]);
  };

  // Handle sending chat message
  const handleSendMessage = () => {
    if (!newMessage.trim()) return;

    // Send via WebSocket
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'chat_message',
        message: newMessage.trim()
      }));
    } else {
      // Fallback for offline mode
      setChat(prev => [...prev, {
        id: Date.now(),
        player: player?.name || 'You',
        message: newMessage.trim(),
        isSystem: false
      }]);
    }

    setNewMessage('');
  };

  // Send music sync message via WebSocket
  const sendMusicMessage = useCallback((message) => {
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify(message));
    }
  }, []);

  // Change room theme (host only)
  const changeTheme = useCallback((themeId) => {
    if (!isHost) return;

    setRoomTheme(themeId);
    setShowThemeSelector(false);

    // Broadcast to all players
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'theme_change',
        theme: themeId
      }));
    }

    // Update in backend
    const API_BASE = import.meta.env.VITE_API_URL || 'https://tfplayhouse.com/api';
    fetch(`${API_BASE}/voice-rooms/${room?.room_id}/`, {
      method: 'PATCH',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ theme: themeId })
    }).catch(err => console.error('Failed to update theme:', err));
  }, [isHost, room?.room_id]);

  // Handle mute toggle
  const handleMuteToggle = () => {
    const newMutedState = !isMuted;
    setIsMuted(newMutedState);

    // Enable/disable local audio tracks
    if (localStreamRef.current) {
      localStreamRef.current.getAudioTracks().forEach(track => {
        track.enabled = !newMutedState;
      });
    }

    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'mute_toggle',
        is_muted: newMutedState
      }));
    }
  };

  // Handle buying more cards
  const handleBuyCard = () => {
    if (purchasedCards < 4) {
      // Generate a new card and add it
      const newCard = generateBingoCard();
      setBingoCards(prev => [...prev, newCard]);
      setMarkedNumbersByCard(prev => [...prev, new Set(['FREE'])]);
      setPurchasedCards(prev => prev + 1);
      setShowBuyCards(false);
    }
  };

  // ====== Pack Drop Feature ======
  // Generate packs with variable speeds (some fall faster as a challenge)
  const generatePacks = useCallback(() => {
    const packCount = 60 + Math.floor(Math.random() * 20); // 60-80 packs for continuous stream
    const newPacks = [];

    for (let i = 0; i < packCount; i++) {
      // 30% of packs fall faster (challenge packs)
      const isFast = Math.random() < 0.3;
      const isGolden = Math.random() < 0.1; // 10% golden packs worth more

      newPacks.push({
        id: i,
        x: 5 + Math.random() * 90, // Random horizontal position (5-95%)
        delay: Math.random() * 12000, // Staggered over 12 seconds (continuous stream throughout 15s)
        speed: isFast ? 'fast' : 'normal', // Fast packs fall 2x speed
        coins: isGolden ? (3 + Math.floor(Math.random() * 3)) : (1 + Math.floor(Math.random() * 2)), // Golden: 3-5, Normal: 1-2
        isGolden,
        caught: false,
      });
    }

    return newPacks;
  }, []);

  // Host triggers pack drop
  const handleDropPacks = useCallback(() => {
    if (!isHost || packDropActive) return;

    // Pause bingo number calling
    if (autoCallRef.current) {
      clearInterval(autoCallRef.current);
    }

    // Generate and start pack drop
    const newPacks = generatePacks();
    setPacks(newPacks);
    setPackDropActive(true);
    setPackCoinsEarned(0);

    // Broadcast pack drop to all players
    if (wsRef.current?.readyState === WebSocket.OPEN) {
      wsRef.current.send(JSON.stringify({
        type: 'pack_drop',
        packs: newPacks
      }));
    }

    // Add system message
    setChat(prev => [...prev, {
      id: Date.now(),
      player: 'System',
      message: '📦 PACK DROP! Tap the falling packs to earn coins!',
      isSystem: true
    }]);

    // End pack drop after 15 seconds (extended from 8)
    packDropRef.current = setTimeout(() => {
      endPackDrop();
    }, 15000);
  }, [isHost, packDropActive, generatePacks]);

  // Handle tapping a pack
  const handleCatchPack = useCallback((packId) => {
    setPacks(prev => {
      const pack = prev.find(p => p.id === packId);
      if (!pack || pack.caught) return prev;

      setPackCoinsEarned(coins => coins + pack.coins);

      return prev.map(p =>
        p.id === packId ? { ...p, caught: true } : p
      );
    });
  }, []);

  // End pack drop and resume bingo
  const endPackDrop = useCallback(() => {
    setPackDropActive(false);
    setPacks([]);

    // Show earned coins
    if (packCoinsEarned > 0) {
      setChat(prev => [...prev, {
        id: Date.now(),
        player: 'System',
        message: `🪙 You earned ${packCoinsEarned} coins from the pack drop!`,
        isSystem: true
      }]);
    }

    // Resume bingo calling if game is still playing
    if (gameState === 'playing') {
      // Resume calling numbers every 5 seconds
      autoCallRef.current = setInterval(() => {
        // This will be handled by the existing callNumber logic
        // For now, just restart the interval - the state is maintained
      }, 5000);

      setChat(prev => [...prev, {
        id: Date.now(),
        player: 'System',
        message: '🎮 Bingo resumes!',
        isSystem: true
      }]);
    }
  }, [packCoinsEarned, gameState]);

  // Handle incoming pack drop from other players (via WebSocket)
  useEffect(() => {
    // Cleanup pack drop timeout on unmount
    return () => {
      if (packDropRef.current) {
        clearTimeout(packDropRef.current);
      }
    };
  }, []);

  return (
    <div className={`voice-room-game theme-${roomTheme}`}>
      {/* Connection Status */}
      {connectionError && (
        <div className="connection-error">
          {connectionError}
        </div>
      )}

      {/* Header */}
      <div className="room-game-header">
        <button className="leave-btn" onClick={onLeave}>
          <span>←</span>
        </button>
        <div className="room-info">
          <h3>{room?.name || "🎵 Music Bingo Party"}</h3>
          <span className="player-count">
            {players.length} players
            {isConnected && <span className="connected-dot">●</span>}
          </span>
        </div>
        <div className="header-actions">
          <div className="voice-status-indicator">
            {voiceStatus === 'initializing' && <span className="voice-dot initializing">●</span>}
            {voiceStatus === 'ready' && <span className="voice-dot ready">●</span>}
            {voiceStatus === 'connected' && <span className="voice-dot connected">●</span>}
            {voiceStatus === 'error' && <span className="voice-dot error">●</span>}
          </div>
          <button
            className={`mute-btn ${isMuted ? 'muted' : ''}`}
            onClick={handleMuteToggle}
            title={voiceStatus === 'error' ? 'Mic unavailable' : (isMuted ? 'Unmute' : 'Mute')}
          >
            {isMuted ? '🔇' : '🎤'}
          </button>
          {/* Music Button */}
          <button
            className={`music-btn ${showMusicPanel ? 'active' : ''}`}
            onClick={() => setShowMusicPanel(!showMusicPanel)}
            title="Music Player"
          >
            🎵
          </button>
          {/* Host Drop Packs Button */}
          {isHost && gameState === 'playing' && !packDropActive && (
            <motion.button
              className="drop-packs-btn"
              onClick={handleDropPacks}
              whileHover={{ scale: 1.05 }}
              whileTap={{ scale: 0.95 }}
              title="Drop Packs"
            >
              📦
            </motion.button>
          )}
          {/* Theme Button (Host Only) */}
          {isHost && (
            <button
              className="theme-btn"
              onClick={() => setShowThemeSelector(true)}
              title="Room Theme"
            >
              🎨
            </button>
          )}
          <button className="settings-btn">⚙️</button>
        </div>
      </div>

      {/* Theme Selector Modal */}
      <AnimatePresence>
        {showThemeSelector && (
          <motion.div
            className="theme-selector-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            onClick={() => setShowThemeSelector(false)}
          >
            <motion.div
              className="theme-selector-modal"
              initial={{ opacity: 0, scale: 0.9, y: 20 }}
              animate={{ opacity: 1, scale: 1, y: 0 }}
              exit={{ opacity: 0, scale: 0.9, y: 20 }}
              onClick={(e) => e.stopPropagation()}
            >
              <div className="theme-selector-header">
                <h3>🎨 Room Theme</h3>
                <button
                  className="theme-selector-close"
                  onClick={() => setShowThemeSelector(false)}
                >
                  ✕
                </button>
              </div>
              <div className="theme-selector-grid">
                {themes.map((theme) => (
                  <motion.div
                    key={theme.id}
                    className={`theme-option theme-preview-${theme.id} ${roomTheme === theme.id ? 'active' : ''}`}
                    onClick={() => changeTheme(theme.id)}
                    whileHover={{ scale: 1.03 }}
                    whileTap={{ scale: 0.98 }}
                  >
                    <span className="theme-option-label">{theme.name}</span>
                  </motion.div>
                ))}
              </div>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Music Player Panel - Always mounted so sync works, visibility controlled by showMusicPanel */}
      <div className={`music-panel-container ${showMusicPanel ? 'visible' : 'hidden'}`}>
        <YouTubeMusicPlayer
          ref={musicPlayerRef}
          isHost={isHost}
          onSendMessage={sendMusicMessage}
          roomId={room?.room_id}
          minimized={!showMusicPanel}
          onToggleMinimize={() => setShowMusicPanel(false)}
        />
      </div>

      {/* Prize Pool */}
      <div className="prize-pool-banner">
        <div className="prize-item first">
          <span className="prize-label">1st</span>
          <span className="prize-amount">🪙 {prizePool}</span>
        </div>
        <div className="prize-item second">
          <span className="prize-label">2nd</span>
          <span className="prize-amount">🪙 {Math.floor(prizePool * 0.4)}</span>
        </div>
        <div className="prize-item third">
          <span className="prize-label">3rd</span>
          <span className="prize-amount">🪙 {Math.floor(prizePool * 0.2)}</span>
        </div>
      </div>

      {/* Current Number Display */}
      {gameState === 'playing' && currentNumber && (
        <motion.div
          className="current-number-display"
          initial={{ scale: 0 }}
          animate={{ scale: 1 }}
          key={currentNumber}
        >
          <div className="number-ball">
            <span className="letter">{getBingoLetter(currentNumber)}</span>
            <span className="number">{currentNumber}</span>
          </div>
          <div className="called-count">{calledNumbers.length}/75 called</div>
        </motion.div>
      )}

      {/* Countdown */}
      <AnimatePresence>
        {countdown !== null && countdown > 0 && (
          <motion.div
            className="countdown-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
          >
            <div className="countdown-content">
              <span className="countdown-label">Game starting in</span>
              <motion.span
                className="countdown-number"
                key={countdown}
                initial={{ scale: 1.5, opacity: 0 }}
                animate={{ scale: 1, opacity: 1 }}
              >
                {countdown}
              </motion.span>
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Host Start Game Button */}
      {gameState === 'waiting' && isHost && (
        <motion.div
          className="start-game-section"
          initial={{ opacity: 0, y: -20 }}
          animate={{ opacity: 1, y: 0 }}
        >
          <div className="waiting-info">
            <span className="waiting-icon">👥</span>
            <span className="waiting-text">{players.length} player{players.length !== 1 ? 's' : ''} in room</span>
          </div>
          <motion.button
            className="start-game-btn"
            onClick={handleStartGame}
            whileHover={{ scale: 1.05 }}
            whileTap={{ scale: 0.95 }}
          >
            <span className="start-icon">🎮</span>
            <span>Start Game</span>
          </motion.button>
          <p className="start-hint">Press to begin the bingo game!</p>
        </motion.div>
      )}

      {/* Waiting for Host (non-host view) */}
      {gameState === 'waiting' && !isHost && (
        <motion.div
          className="waiting-for-host"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
        >
          <div className="waiting-spinner"></div>
          <p>Waiting for host to start the game...</p>
          <span className="player-count-waiting">{players.length} player{players.length !== 1 ? 's' : ''} in room</span>
        </motion.div>
      )}

      {/* Players Strip with Progress */}
      <div className="players-strip">
        {players.map((p) => (
          <div key={p.id} className={`player-bubble ${p.isSpeaking ? 'speaking' : ''}`}>
            <div className="player-avatar-bubble">
              {p.avatar}
              {p.isHost && <span className="host-badge">👑</span>}
            </div>
            <span className="player-name-small">{p.name}</span>
            {p.isSpeaking && <div className="speaking-indicator" />}
            <div className="player-progress-mini">
              <span className="progress-mini-text">
                {p.id === player?.id ? myProgress : p.progress}/5
              </span>
            </div>
          </div>
        ))}
      </div>

      {/* Bingo Cards */}
      {bingoCards.length > 0 && (
        <div className="bingo-cards-wrapper">
          {/* Card selector tabs */}
          {bingoCards.length > 1 && (
            <div className="card-tabs">
              {bingoCards.map((_, idx) => (
                <button
                  key={idx}
                  className={`card-tab ${activeCardIndex === idx ? 'active' : ''} ${checkBingoForCard(bingoCards[idx], markedNumbersByCard[idx]) ? 'has-bingo' : ''}`}
                  onClick={() => setActiveCardIndex(idx)}
                >
                  Card {idx + 1}
                  {checkBingoForCard(bingoCards[idx], markedNumbersByCard[idx]) && <span className="bingo-indicator">🎉</span>}
                </button>
              ))}
            </div>
          )}

          {/* All cards in a scrollable container */}
          <div className="bingo-cards-scroll">
            {bingoCards.map((card, cardIndex) => (
              <div
                key={cardIndex}
                className={`bingo-card-container ${cardIndex === activeCardIndex ? 'active' : ''} ${checkBingoForCard(card, markedNumbersByCard[cardIndex]) ? 'has-bingo' : ''}`}
              >
                <div className="card-label">Card {cardIndex + 1}</div>
                <div className="card-header-row">
                  {['B', 'I', 'N', 'G', 'O'].map(letter => (
                    <div key={letter} className="bingo-letter">{letter}</div>
                  ))}
                </div>
                <div className="bingo-grid">
                  {[0, 1, 2, 3, 4].map(row => (
                    <div key={row} className="bingo-row">
                      {[0, 1, 2, 3, 4].map(col => {
                        const number = card[col][row];
                        const isMarked = markedNumbersByCard[cardIndex]?.has(number);
                        const isCalled = number === 'FREE' || calledNumbers.includes(number);

                        return (
                          <motion.button
                            key={`${cardIndex}-${col}-${row}`}
                            className={`bingo-cell ${isMarked ? 'marked' : ''} ${isCalled ? 'called' : ''}`}
                            onClick={() => handleMarkNumber(cardIndex, number)}
                            whileTap={{ scale: 0.95 }}
                          >
                            {number === 'FREE' ? '⭐' : number}
                          </motion.button>
                        );
                      })}
                    </div>
                  ))}
                </div>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Action Buttons */}
      <div className="game-actions">
        <button
          className="action-btn buy-cards"
          onClick={() => setShowBuyCards(true)}
        >
          <span>🎴</span>
          <span>Buy Cards ({purchasedCards}/4)</span>
        </button>
        <button
          className={`action-btn bingo-btn ${checkBingo() ? 'has-bingo' : ''}`}
          onClick={handleClaimBingo}
          disabled={!checkBingo()}
        >
          <span>🎉</span>
          <span>BINGO!</span>
        </button>
      </div>

      {/* Called Numbers History */}
      <div className="called-numbers-section">
        <h4>Called Numbers</h4>
        <div className="called-numbers-list">
          {calledNumbers.slice().reverse().slice(0, 10).map((num, idx) => (
            <span key={num} className={`called-number ${idx === 0 ? 'latest' : ''}`}>
              {getBingoLetter(num)}{num}
            </span>
          ))}
        </div>
      </div>

      {/* Chat Toggle */}
      <button className="chat-toggle" onClick={() => setShowChat(true)}>
        💬 Chat
        {chat.length > 0 && <span className="chat-badge">{chat.length}</span>}
      </button>

      {/* Chat Panel */}
      <AnimatePresence>
        {showChat && (
          <motion.div
            className="chat-panel"
            initial={{ y: '100%' }}
            animate={{ y: 0 }}
            exit={{ y: '100%' }}
          >
            <div className="chat-header">
              <h4>Room Chat</h4>
              <button onClick={() => setShowChat(false)}>✕</button>
            </div>
            <div className="chat-messages" ref={chatRef}>
              {chat.map(msg => (
                <div key={msg.id} className={`chat-message ${msg.isSystem ? 'system' : ''}`}>
                  {!msg.isSystem && <span className="chat-player">{msg.player}:</span>}
                  <span className="chat-text">{msg.message}</span>
                </div>
              ))}
            </div>
            <div className="chat-input-area">
              <input
                type="text"
                placeholder="Type a message..."
                value={newMessage}
                onChange={(e) => setNewMessage(e.target.value)}
                onKeyPress={(e) => e.key === 'Enter' && handleSendMessage()}
              />
              <button onClick={handleSendMessage}>Send</button>
            </div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Buy Cards Modal */}
      <AnimatePresence>
        {showBuyCards && (
          <motion.div
            className="buy-cards-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
          >
            <motion.div
              className="buy-cards-modal"
              initial={{ scale: 0.9 }}
              animate={{ scale: 1 }}
              exit={{ scale: 0.9 }}
            >
              <h3>Buy More Cards</h3>
              <p>Increase your chances of winning!</p>
              <div className="card-options">
                <div className="card-option">
                  <span className="card-count">1 Card</span>
                  <span className="card-price">🪙 100</span>
                </div>
              </div>
              <p className="cards-owned">You have {purchasedCards}/4 cards</p>
              <div className="modal-actions">
                <button
                  className="buy-btn"
                  onClick={handleBuyCard}
                  disabled={purchasedCards >= 4}
                >
                  Buy Card (🪙 100)
                </button>
                <button className="cancel-btn" onClick={() => setShowBuyCards(false)}>
                  Cancel
                </button>
              </div>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Game Over Modal */}
      <AnimatePresence>
        {gameState === 'ended' && (
          <motion.div
            className="game-over-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
          >
            <motion.div
              className="game-over-modal"
              initial={{ scale: 0.9 }}
              animate={{ scale: 1 }}
            >
              <h2>🎉 Game Over! 🎉</h2>
              <p>Thanks for playing!</p>
              <button className="play-again-btn" onClick={onLeave}>
                Back to Lobby
              </button>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>

      {/* Pack Drop Overlay */}
      <AnimatePresence>
        {packDropActive && (
          <motion.div
            className="pack-drop-overlay"
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
          >
            {/* Pack Drop Header */}
            <div className="pack-drop-header">
              <motion.div
                className="pack-drop-title"
                initial={{ y: -50 }}
                animate={{ y: 0 }}
              >
                📦 PACK DROP! 📦
              </motion.div>
              <div className="pack-coins-earned">
                🪙 {packCoinsEarned}
              </div>
            </div>

            {/* Falling Packs */}
            <div className="packs-container">
              {packs.map((pack) => (
                <motion.div
                  key={pack.id}
                  className={`falling-pack ${pack.speed} ${pack.isGolden ? 'golden' : ''} ${pack.caught ? 'caught' : ''}`}
                  style={{
                    left: `${pack.x}%`,
                  }}
                  initial={{ y: -80, opacity: 0 }}
                  animate={{
                    y: pack.caught ? undefined : '120vh',
                    opacity: pack.caught ? 0 : 1,
                    scale: pack.caught ? 1.5 : 1,
                  }}
                  transition={{
                    y: {
                      duration: pack.speed === 'fast' ? 3 : 6, // Fast packs fall in 3s, normal in 6s (original speeds)
                      delay: pack.delay / 1000,
                      ease: 'linear',
                    },
                    opacity: pack.caught ? { duration: 0.2 } : { duration: 0.3, delay: pack.delay / 1000 },
                    scale: { duration: 0.2 },
                  }}
                  onClick={() => !pack.caught && handleCatchPack(pack.id)}
                  onTouchStart={(e) => {
                    e.preventDefault();
                    e.stopPropagation();
                    !pack.caught && handleCatchPack(pack.id);
                  }}
                  onTouchEnd={(e) => {
                    e.preventDefault();
                    e.stopPropagation();
                  }}
                >
                  {/* Larger invisible touch target */}
                  <div className="pack-touch-target" />
                  <span className="pack-emoji">{pack.isGolden ? '🎁' : '📦'}</span>
                  {pack.caught && (
                    <motion.span
                      className="pack-coins-popup"
                      initial={{ y: 0, opacity: 1 }}
                      animate={{ y: -30, opacity: 0 }}
                      transition={{ duration: 0.5 }}
                    >
                      +{pack.coins}
                    </motion.span>
                  )}
                </motion.div>
              ))}
            </div>

            {/* Countdown timer */}
            <div className="pack-drop-timer">
              <motion.div
                className="timer-bar"
                initial={{ width: '100%' }}
                animate={{ width: '0%' }}
                transition={{ duration: 15, ease: 'linear' }}
              />
            </div>
          </motion.div>
        )}
      </AnimatePresence>
    </div>
  );
};

export default VoiceRoomGame;
