import React, { useState, useEffect, useCallback, useRef, forwardRef, useImperativeHandle } from 'react';
import { motion, AnimatePresence } from 'framer-motion';
import './YouTubeMusicPlayer.css';

// YouTube IFrame API loader
const loadYouTubeAPI = () => {
  return new Promise((resolve) => {
    if (window.YT && window.YT.Player) {
      resolve(window.YT);
      return;
    }

    const tag = document.createElement('script');
    tag.src = 'https://www.youtube.com/iframe_api';
    const firstScriptTag = document.getElementsByTagName('script')[0];
    firstScriptTag.parentNode.insertBefore(tag, firstScriptTag);

    window.onYouTubeIframeAPIReady = () => {
      resolve(window.YT);
    };
  });
};

const YouTubeMusicPlayer = forwardRef(({
  isHost,
  onSendMessage, // WebSocket send function
  roomId,
  minimized = false,
  onToggleMinimize
}, ref) => {
  const [isAPIReady, setIsAPIReady] = useState(false);
  const [searchQuery, setSearchQuery] = useState('');
  const [searchResults, setSearchResults] = useState([]);
  const [suggestions, setSuggestions] = useState([]);
  const [isSearching, setIsSearching] = useState(false);
  const [showSuggestions, setShowSuggestions] = useState(false);
  const [queue, setQueue] = useState([]);
  const [currentTrack, setCurrentTrack] = useState(null);
  const [isPlaying, setIsPlaying] = useState(false);
  const [currentTime, setCurrentTime] = useState(0);
  const [duration, setDuration] = useState(0);
  const [volume, setVolume] = useState(50);

  const playerRef = useRef(null);
  const playerContainerRef = useRef(null);
  const timeUpdateRef = useRef(null);
  const searchTimeoutRef = useRef(null);
  const suggestTimeoutRef = useRef(null);

  // Refs to hold latest function references for sync handling
  const playVideoRef = useRef(null);
  const playNextInQueueRef = useRef(null);

  // Pending sync message if player not ready yet
  const pendingSyncRef = useRef(null);

  // Track if player is ready
  const playerReadyRef = useRef(false);

  // Queue ref for accessing in callbacks
  const queueRef = useRef([]);

  // Ref for onSendMessage to avoid stale closures
  const onSendMessageRef = useRef(onSendMessage);

  // Current track ref to avoid stale closures
  const currentTrackRef = useRef(null);

  // Expose methods to parent component
  useImperativeHandle(ref, () => ({
    // Handle incoming sync messages from WebSocket
    handleSyncMessage: (data) => {
      console.log('Music sync received:', data);

      // If player not ready yet, store for later
      if (!playerRef.current) {
        console.log('Player not ready, storing sync message for later');
        pendingSyncRef.current = data;
        return;
      }

      switch (data.action) {
        case 'play':
          console.log('Sync: Playing video', data.videoId, 'playerRef:', !!playerRef.current);
          if (data.videoId && playerRef.current) {
            // Always load the video for sync (don't skip if same videoId)
            setCurrentTrack({
              videoId: data.videoId,
              title: data.title || 'Unknown',
              thumbnail: data.thumbnail || `https://i.ytimg.com/vi/${data.videoId}/mqdefault.jpg`
            });
            try {
              playerRef.current.loadVideoById({
                videoId: data.videoId,
                startSeconds: data.startTime || 0
              });
              console.log('Sync: Video loaded successfully');
            } catch (err) {
              console.error('Sync: Error loading video:', err);
            }
          } else if (playerRef.current) {
            playerRef.current.seekTo(data.startTime || 0);
            playerRef.current.playVideo();
          } else {
            console.warn('Sync: playerRef not available, cannot play');
          }
          setIsPlaying(true);
          break;
        case 'pause':
          console.log('Sync: Pausing');
          if (playerRef.current) {
            playerRef.current.pauseVideo();
          }
          setIsPlaying(false);
          break;
        case 'seek':
          console.log('Sync: Seeking to', data.time);
          if (playerRef.current) {
            playerRef.current.seekTo(data.time);
          }
          break;
        case 'queue_add':
          setQueue(prev => [...prev, data.track]);
          break;
        case 'queue_update':
          setQueue(data.queue || []);
          break;
        case 'skip':
          if (playNextInQueueRef.current) {
            playNextInQueueRef.current();
          }
          break;
        default:
          break;
      }
    },
    getCurrentState: () => ({
      currentTrack,
      isPlaying,
      currentTime: playerRef.current?.getCurrentTime() || 0,
      queue
    })
  }));

  // Initialize YouTube API
  useEffect(() => {
    loadYouTubeAPI().then(() => {
      setIsAPIReady(true);
    });

    return () => {
      if (timeUpdateRef.current) {
        clearInterval(timeUpdateRef.current);
      }
    };
  }, []);

  // Create YouTube player when API is ready
  useEffect(() => {
    console.log('YouTube player effect:', { isAPIReady, hasContainer: !!playerContainerRef.current, hasPlayer: !!playerRef.current, isHost });
    if (!isAPIReady || !playerContainerRef.current || playerRef.current) return;

    console.log('Creating YouTube player for', isHost ? 'host' : 'listener');
    playerRef.current = new window.YT.Player(playerContainerRef.current, {
      height: '0',
      width: '0',
      playerVars: {
        autoplay: 0,
        controls: 0,
        disablekb: 1,
        fs: 0,
        modestbranding: 1,
        playsinline: 1,
        rel: 0,
      },
      events: {
        onReady: (event) => {
          event.target.setVolume(volume);
          console.log('YouTube player ready');
          playerReadyRef.current = true;

          // Process any pending sync message (from host selecting a track before player was ready)
          if (pendingSyncRef.current) {
            console.log('Processing pending track:', pendingSyncRef.current);
            const pendingData = pendingSyncRef.current;
            pendingSyncRef.current = null;

            // Handle the pending play action
            if (pendingData.action === 'play' && pendingData.videoId) {
              setCurrentTrack({
                videoId: pendingData.videoId,
                title: pendingData.title || 'Unknown',
                thumbnail: pendingData.thumbnail || `https://i.ytimg.com/vi/${pendingData.videoId}/mqdefault.jpg`
              });
              playerRef.current.loadVideoById({
                videoId: pendingData.videoId,
                startSeconds: pendingData.startTime || 0
              });
              setIsPlaying(true);

              // If host, sync to other players
              if (isHost && onSendMessageRef.current) {
                console.log('Sending sync message to other players');
                onSendMessageRef.current({
                  type: 'music_sync',
                  action: 'play',
                  videoId: pendingData.videoId,
                  title: pendingData.title,
                  thumbnail: pendingData.thumbnail,
                  startTime: 0
                });
              }
            }
          } else if (isHost && queueRef.current.length > 0) {
            // If host has queued tracks but nothing playing, play the first one
            console.log('Player ready, playing first queued track');
            const firstTrack = queueRef.current[0];
            const newQueue = queueRef.current.slice(1);
            setQueue(newQueue);
            queueRef.current = newQueue;

            setCurrentTrack({
              videoId: firstTrack.videoId,
              title: firstTrack.title,
              thumbnail: firstTrack.thumbnail
            });
            playerRef.current.loadVideoById({
              videoId: firstTrack.videoId,
              startSeconds: 0
            });
            setIsPlaying(true);

            // Sync to other players
            if (onSendMessageRef.current) {
              onSendMessageRef.current({
                type: 'music_sync',
                action: 'play',
                videoId: firstTrack.videoId,
                title: firstTrack.title,
                thumbnail: firstTrack.thumbnail,
                startTime: 0
              });
            }
          }
        },
        onStateChange: (event) => {
          const state = event.data;
          if (state === window.YT.PlayerState.PLAYING) {
            setIsPlaying(true);
            setDuration(playerRef.current.getDuration());
            // Start time updates
            timeUpdateRef.current = setInterval(() => {
              if (playerRef.current) {
                setCurrentTime(playerRef.current.getCurrentTime());
              }
            }, 1000);
          } else if (state === window.YT.PlayerState.PAUSED) {
            setIsPlaying(false);
            if (timeUpdateRef.current) {
              clearInterval(timeUpdateRef.current);
            }
          } else if (state === window.YT.PlayerState.ENDED) {
            setIsPlaying(false);
            if (timeUpdateRef.current) {
              clearInterval(timeUpdateRef.current);
            }
            // Auto-play next in queue
            if (isHost) {
              playNextInQueue();
            }
          }
        },
        onError: (event) => {
          console.error('YouTube player error:', event.data);
        }
      }
    });
  }, [isAPIReady, volume, isHost]);

  // API base URL
  const API_BASE = import.meta.env.VITE_API_URL || 'http://localhost:8001/api';

  // Fetch autocomplete suggestions
  const fetchSuggestions = async (query) => {
    if (!query.trim() || query.length < 2) {
      setSuggestions([]);
      return;
    }

    try {
      // Use backend API for suggestions
      const response = await fetch(
        `${API_BASE}/youtube/suggestions/?q=${encodeURIComponent(query)}`,
        { signal: AbortSignal.timeout(5000) }
      );
      if (response.ok) {
        const data = await response.json();
        if (Array.isArray(data) && data.length > 0) {
          setSuggestions(data.slice(0, 8));
          setShowSuggestions(true);
          return;
        }
      }
    } catch (error) {
      console.log('Suggestions fetch failed:', error);
    }

    // Fallback suggestions
    setSuggestions([
      query + ' official audio',
      query + ' lyrics',
      query + ' music video',
      query + ' live'
    ]);
    setShowSuggestions(true);
  };

  // Handle search input change with debounced suggestions
  const handleSearchInputChange = (e) => {
    const value = e.target.value;
    setSearchQuery(value);

    // Clear previous timeout
    if (suggestTimeoutRef.current) {
      clearTimeout(suggestTimeoutRef.current);
    }

    // Debounce suggestions
    suggestTimeoutRef.current = setTimeout(() => {
      fetchSuggestions(value);
    }, 250);
  };

  // Select a suggestion
  const selectSuggestion = (suggestion) => {
    setSearchQuery(suggestion);
    setSuggestions([]);
    setShowSuggestions(false);
    searchYouTube(suggestion);
  };

  // Search YouTube using backend API
  const searchYouTube = async (query) => {
    if (!query.trim()) return;

    setIsSearching(true);
    setSuggestions([]);
    setShowSuggestions(false);
    setSearchResults([]);

    try {
      console.log('Searching via backend API...');
      const response = await fetch(
        `${API_BASE}/youtube/search/?q=${encodeURIComponent(query)}`,
        { signal: AbortSignal.timeout(20000) }
      );

      if (!response.ok) {
        console.log(`Search returned ${response.status}`);
        setIsSearching(false);
        return;
      }

      const data = await response.json();
      console.log('Search response:', data);

      if (data.items && Array.isArray(data.items) && data.items.length > 0) {
        setSearchResults(data.items);
        console.log(`Search successful, found ${data.items.length} results`);
      } else if (data.error) {
        console.error('Search error:', data.error);
      }
    } catch (error) {
      console.error('Search failed:', error.message);
    }

    setIsSearching(false);
  };

  // Format seconds to MM:SS
  const formatDuration = (seconds) => {
    if (!seconds) return '0:00';
    const mins = Math.floor(seconds / 60);
    const secs = Math.floor(seconds % 60);
    return `${mins}:${secs.toString().padStart(2, '0')}`;
  };

  // Play a video
  const playVideo = useCallback((videoId, title, thumbnail, startTime = 0) => {
    console.log('playVideo called:', { videoId, title, playerReady: !!playerRef.current });

    if (!playerRef.current) {
      console.warn('YouTube player not ready yet, cannot play video');
      return false;
    }

    setCurrentTrack({ videoId, title, thumbnail });
    try {
      playerRef.current.loadVideoById({
        videoId: videoId,
        startSeconds: startTime
      });
      setIsPlaying(true);
      console.log('Video loaded successfully:', videoId);
      return true;
    } catch (error) {
      console.error('Error loading video:', error);
      return false;
    }
  }, []);

  // Play next track in queue
  const playNextInQueue = useCallback(() => {
    if (queue.length === 0) {
      setCurrentTrack(null);
      setIsPlaying(false);
      return;
    }

    const nextTrack = queue[0];
    const newQueue = queue.slice(1);
    setQueue(newQueue);

    playVideo(nextTrack.videoId, nextTrack.title, nextTrack.thumbnail);

    // Broadcast to all players
    if (isHost && onSendMessage) {
      onSendMessage({
        type: 'music_sync',
        action: 'play',
        videoId: nextTrack.videoId,
        title: nextTrack.title,
        thumbnail: nextTrack.thumbnail,
        startTime: 0
      });
      onSendMessage({
        type: 'music_sync',
        action: 'queue_update',
        queue: newQueue
      });
    }
  }, [queue, playVideo, isHost, onSendMessage]);

  // Keep refs updated for use in useImperativeHandle
  useEffect(() => {
    playVideoRef.current = playVideo;
    playNextInQueueRef.current = playNextInQueue;
  }, [playVideo, playNextInQueue]);

  // Keep queue ref in sync with state
  useEffect(() => {
    queueRef.current = queue;
  }, [queue]);

  // Keep onSendMessage ref updated
  useEffect(() => {
    onSendMessageRef.current = onSendMessage;
  }, [onSendMessage]);

  // Keep currentTrack ref updated
  useEffect(() => {
    currentTrackRef.current = currentTrack;
  }, [currentTrack]);

  // Add track to queue (or play immediately if nothing is playing)
  const addToQueue = useCallback((track) => {
    const hasCurrentTrack = !!currentTrackRef.current;
    console.log('addToQueue called:', { track: track.title, hasCurrentTrack, isHost, playerReady: playerReadyRef.current, playerRef: !!playerRef.current });

    // If nothing is playing and we're the host, play immediately (don't add to queue)
    if (!hasCurrentTrack && isHost) {
      console.log('Nothing playing, attempting to play immediately');

      // Try to play directly
      const success = playVideo(track.videoId, track.title, track.thumbnail);

      if (success) {
        console.log('Playback started successfully');
        // Send sync message to other players
        if (onSendMessage) {
          onSendMessage({
            type: 'music_sync',
            action: 'play',
            videoId: track.videoId,
            title: track.title,
            thumbnail: track.thumbnail,
            startTime: 0
          });
        }
      } else {
        // Player not ready, store for later
        console.warn('Player not ready, storing for auto-play when ready');
        pendingSyncRef.current = {
          action: 'play',
          videoId: track.videoId,
          title: track.title,
          thumbnail: track.thumbnail,
          startTime: 0
        };
      }
    } else {
      // Add to queue
      console.log('Adding to queue');
      setQueue(prev => [...prev, track]);

      // Broadcast queue update
      if (isHost && onSendMessage) {
        onSendMessage({
          type: 'music_sync',
          action: 'queue_add',
          track: track
        });
      }
    }

    // Clear search after selecting
    setSearchQuery('');
    setSearchResults([]);
  }, [isHost, onSendMessage, playVideo]);

  // Host controls
  const handlePlayPause = useCallback(() => {
    if (!playerRef.current || !isHost) return;

    if (isPlaying) {
      playerRef.current.pauseVideo();
      if (onSendMessage) {
        onSendMessage({
          type: 'music_sync',
          action: 'pause'
        });
      }
    } else {
      playerRef.current.playVideo();
      if (onSendMessage) {
        onSendMessage({
          type: 'music_sync',
          action: 'play',
          videoId: currentTrack?.videoId,
          title: currentTrack?.title,
          thumbnail: currentTrack?.thumbnail,
          startTime: playerRef.current.getCurrentTime()
        });
      }
    }
  }, [isPlaying, isHost, onSendMessage, currentTrack]);

  const handleSkip = useCallback(() => {
    if (!isHost) return;

    playNextInQueue();

    if (onSendMessage) {
      onSendMessage({
        type: 'music_sync',
        action: 'skip'
      });
    }
  }, [isHost, playNextInQueue, onSendMessage]);

  const handleVolumeChange = useCallback((e) => {
    const newVolume = parseInt(e.target.value);
    setVolume(newVolume);
    if (playerRef.current) {
      playerRef.current.setVolume(newVolume);
    }
  }, []);

  const handleSeek = useCallback((e) => {
    if (!isHost || !playerRef.current) return;

    const seekTime = parseFloat(e.target.value);
    playerRef.current.seekTo(seekTime);
    setCurrentTime(seekTime);

    if (onSendMessage) {
      onSendMessage({
        type: 'music_sync',
        action: 'seek',
        time: seekTime
      });
    }
  }, [isHost, onSendMessage]);

  // Always render the full player - visibility is controlled by parent container
  return (
    <div className="youtube-music-player">
      {/* Hidden YouTube Player - always rendered for initialization */}
      <div id="youtube-player-container" ref={playerContainerRef} style={{ position: 'absolute', left: '-9999px', width: '1px', height: '1px', pointerEvents: 'none' }} />

      {/* Close button */}
      {onToggleMinimize && (
        <button className="close-player-btn" onClick={onToggleMinimize}>
          ✕
        </button>
      )}

      {/* Search Section - Always visible for host at the top */}
      {isHost && (
        <div className="search-section">
          <h3 className="section-title">🎵 Music Player</h3>
          <div className="search-input-wrapper">
            <div className="search-input-container">
              <input
                type="text"
                placeholder="Search for songs on YouTube..."
                value={searchQuery}
                onChange={handleSearchInputChange}
                onKeyPress={(e) => e.key === 'Enter' && searchYouTube(searchQuery)}
                onFocus={() => searchQuery.length >= 2 && setShowSuggestions(true)}
                onBlur={() => setTimeout(() => setShowSuggestions(false), 200)}
              />
              <button
                onClick={() => searchYouTube(searchQuery)}
                disabled={isSearching}
              >
                {isSearching ? '...' : '🔍'}
              </button>
            </div>

            {/* Autocomplete Suggestions */}
            {showSuggestions && suggestions.length > 0 && (
              <div className="suggestions-dropdown">
                {suggestions.map((suggestion, index) => (
                  <div
                    key={index}
                    className="suggestion-item"
                    onMouseDown={() => selectSuggestion(suggestion)}
                  >
                    <span className="suggestion-icon">🔍</span>
                    <span className="suggestion-text">{suggestion}</span>
                  </div>
                ))}
              </div>
            )}
          </div>

          {/* Search Results */}
          <div className="search-results">
            {isSearching && (
              <div className="search-loading">
                <span>Searching...</span>
              </div>
            )}
            {!isSearching && searchResults.length === 0 && !searchQuery && (
              <div className="search-hint">
                <span>Type to search for songs</span>
              </div>
            )}
            {!isSearching && searchResults.length === 0 && searchQuery && (
              <div className="no-results">
                <span>No results found. Try different keywords.</span>
              </div>
            )}
            {searchResults.map((result) => (
              <div
                key={result.videoId}
                className="search-result-item"
                onClick={() => addToQueue(result)}
              >
                <img src={result.thumbnail} alt={result.title} />
                <div className="result-info">
                  <span className="result-title">{result.title}</span>
                  <span className="result-meta">{result.author} • {result.duration}</span>
                </div>
                <button className="add-btn" title="Play now">
                  <svg width="16" height="16" viewBox="0 0 24 24" fill="white">
                    <path d="M8 5v14l11-7z"/>
                  </svg>
                </button>
              </div>
            ))}
          </div>
        </div>
      )}

      {/* Non-host view */}
      {!isHost && (
        <div className="listener-view">
          <h3 className="section-title">🎵 Music Player</h3>
          <div className="listener-badge-large">
            <span>🎧</span>
            <span>Listening to host's music</span>
          </div>
        </div>
      )}

      {/* Now Playing Section */}
      {currentTrack && (
        <div className="now-playing-section">
          <div className="now-playing-header">Now Playing</div>
          <div className="now-playing-content">
            <div className="album-art">
              <img src={currentTrack.thumbnail} alt="Album art" />
              {isPlaying && (
                <div className="playing-animation">
                  <span></span><span></span><span></span><span></span>
                </div>
              )}
            </div>

            <div className="track-info">
              <h3 className="track-title">{currentTrack.title}</h3>

              {/* Progress Bar */}
              <div className="progress-container">
                <span className="time-current">{formatDuration(currentTime)}</span>
                <input
                  type="range"
                  className="progress-bar"
                  min="0"
                  max={duration || 100}
                  value={currentTime}
                  onChange={handleSeek}
                  disabled={!isHost}
                />
                <span className="time-duration">{formatDuration(duration)}</span>
              </div>

              {/* Controls */}
              <div className="player-controls">
                {isHost && (
                  <>
                    <button
                      className="control-btn play-btn"
                      onClick={handlePlayPause}
                    >
                      {isPlaying ? '⏸️' : '▶️'}
                    </button>
                    <button
                      className="control-btn skip-btn"
                      onClick={handleSkip}
                      disabled={queue.length === 0}
                    >
                      ⏭️
                    </button>
                  </>
                )}

                {/* Volume */}
                <div className="volume-control">
                  <span>🔊</span>
                  <input
                    type="range"
                    className="volume-slider"
                    min="0"
                    max="100"
                    value={volume}
                    onChange={handleVolumeChange}
                  />
                </div>
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Queue Section */}
      {queue.length > 0 && (
        <div className="queue-section">
          <h4>Up Next ({queue.length})</h4>
          <div className="queue-list">
            {queue.slice(0, 5).map((track, index) => (
              <div key={`${track.videoId}-${index}`} className="queue-item">
                <span className="queue-number">{index + 1}</span>
                <img src={track.thumbnail} alt={track.title} />
                <span className="queue-title">{track.title}</span>
              </div>
            ))}
            {queue.length > 5 && (
              <div className="queue-more">+{queue.length - 5} more</div>
            )}
          </div>
        </div>
      )}
    </div>
  );
});

YouTubeMusicPlayer.displayName = 'YouTubeMusicPlayer';

export default YouTubeMusicPlayer;
