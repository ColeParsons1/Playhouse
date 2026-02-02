import React, { useEffect, useRef, useState, useCallback } from 'react';
import './FlyGame.css';

const FlyGame = ({ onExit }) => {
  const containerRef = useRef(null);
  const sceneRef = useRef(null);
  const cameraRef = useRef(null);
  const rendererRef = useRef(null);
  const planeRef = useRef(null);
  const physicsRef = useRef(null);
  const keysRef = useRef({});
  const animationRef = useRef(null);
  const THREE = useRef(null);
  const gameRunningRef = useRef(false); // Track if game should be running (controls dismissed)

  const [speed, setSpeed] = useState(0);
  const [altitude, setAltitude] = useState(0);
  const [heading, setHeading] = useState(0);
  const [pitch, setPitch] = useState(0);
  const [throttle, setThrottle] = useState(50);
  const [isLandscape, setIsLandscape] = useState(window.innerWidth > window.innerHeight);
  const [showControls, setShowControls] = useState(true);
  const [isLoading, setIsLoading] = useState(true);
  const [stalled, setStalled] = useState(false);
  const [ammo, setAmmo] = useState(500);
  const [score, setScore] = useState(0);
  const [targetsDestroyed, setTargetsDestroyed] = useState(0);

  // Bullets and targets refs
  const bulletsRef = useRef([]);
  const targetsRef = useRef([]);
  const lastFireTimeRef = useRef(0);
  const isFiringRef = useRef(false);
  const ammoRef = useRef(500); // Track ammo in ref to avoid re-renders causing issues

  // Touch controls state - analog values for smooth control
  const touchControlsRef = useRef({
    pitch: 0,      // -1 (dive) to 1 (climb)
    roll: 0,       // -1 (left) to 1 (right)
    throttleChange: 0, // -1 (decrease) to 1 (increase)
  });

  // Create LASER TAG ARENA - dark glowing neon environment
  const createTerrain = useCallback((scene) => {
    if (!THREE.current) return;
    const T = THREE.current;

    const arenaSize = 8000; // Smaller, contained arena
    const wallHeight = 2000;

    // === NEON COLORS ===
    const neonMagenta = 0xff00ff;
    const neonCyan = 0x00ffff;
    const neonGreen = 0x00ff00;
    const neonPink = 0xff1493;
    const neonBlue = 0x0066ff;
    const neonPurple = 0x9900ff;

    // === FLOOR - Dark reflective with grid ===
    const floorGeom = new T.PlaneGeometry(arenaSize * 3, arenaSize * 3);
    const floorMat = new T.MeshStandardMaterial({
      color: 0x0a0a0a,
      metalness: 0.9,
      roughness: 0.1,
    });
    const floor = new T.Mesh(floorGeom, floorMat);
    floor.rotation.x = -Math.PI / 2;
    floor.position.y = 0;
    scene.add(floor);

    // Grid lines on floor - CYAN
    const gridMat = new T.MeshBasicMaterial({ color: neonCyan });
    const gridSpacing = 500;
    for (let i = -arenaSize * 1.5; i <= arenaSize * 1.5; i += gridSpacing) {
      // X lines
      const lineGeomX = new T.BoxGeometry(arenaSize * 3, 2, 4);
      const lineX = new T.Mesh(lineGeomX, gridMat);
      lineX.position.set(0, 1, i);
      scene.add(lineX);
      // Z lines
      const lineGeomZ = new T.BoxGeometry(4, 2, arenaSize * 3);
      const lineZ = new T.Mesh(lineGeomZ, gridMat);
      lineZ.position.set(i, 1, 0);
      scene.add(lineZ);
    }

    // === OUTER WALLS - Dark with neon edges ===
    const wallMat = new T.MeshStandardMaterial({
      color: 0x111111,
      metalness: 0.5,
      roughness: 0.5,
    });

    const createGlowingWall = (width, height, depth, x, y, z, rotY = 0, color = neonMagenta) => {
      // Main wall
      const wallGeom = new T.BoxGeometry(width, height, depth);
      const wall = new T.Mesh(wallGeom, wallMat);
      wall.position.set(x, y, z);
      wall.rotation.y = rotY;
      scene.add(wall);

      // Neon edge strips
      const stripMat = new T.MeshBasicMaterial({ color: color });

      // Top edge
      const topStrip = new T.Mesh(new T.BoxGeometry(width + 10, 15, depth + 10), stripMat);
      topStrip.position.set(x, y + height / 2 + 5, z);
      topStrip.rotation.y = rotY;
      scene.add(topStrip);

      // Bottom edge
      const bottomStrip = new T.Mesh(new T.BoxGeometry(width + 10, 15, depth + 10), stripMat);
      bottomStrip.position.set(x, y - height / 2 + 5, z);
      bottomStrip.rotation.y = rotY;
      scene.add(bottomStrip);
    };

    // Arena boundary walls
    createGlowingWall(arenaSize * 2, wallHeight, 50, 0, wallHeight / 2, -arenaSize, 0, neonMagenta);
    createGlowingWall(arenaSize * 2, wallHeight, 50, 0, wallHeight / 2, arenaSize, 0, neonMagenta);
    createGlowingWall(50, wallHeight, arenaSize * 2, -arenaSize, wallHeight / 2, 0, 0, neonCyan);
    createGlowingWall(50, wallHeight, arenaSize * 2, arenaSize, wallHeight / 2, 0, 0, neonCyan);

    // === CEILING with neon beams ===
    const ceilingGeom = new T.PlaneGeometry(arenaSize * 3, arenaSize * 3);
    const ceilingMat = new T.MeshStandardMaterial({ color: 0x050505, metalness: 0.8, roughness: 0.2 });
    const ceiling = new T.Mesh(ceilingGeom, ceilingMat);
    ceiling.rotation.x = Math.PI / 2;
    ceiling.position.y = wallHeight + 500;
    scene.add(ceiling);

    // Ceiling neon strips
    const ceilingStripMat = new T.MeshBasicMaterial({ color: neonPurple });
    for (let i = -arenaSize; i <= arenaSize; i += 800) {
      const ceilingStrip = new T.Mesh(new T.BoxGeometry(30, 20, arenaSize * 2.5), ceilingStripMat);
      ceilingStrip.position.set(i, wallHeight + 480, 0);
      scene.add(ceilingStrip);
    }

    // === OBSTACLE PILLARS with neon accents ===
    const pillarPositions = [
      { x: -2000, z: -2000 }, { x: 2000, z: -2000 },
      { x: -2000, z: 2000 }, { x: 2000, z: 2000 },
      { x: 0, z: -3500 }, { x: 0, z: 3500 },
      { x: -3500, z: 0 }, { x: 3500, z: 0 },
      { x: -4500, z: -4500 }, { x: 4500, z: -4500 },
      { x: -4500, z: 4500 }, { x: 4500, z: 4500 },
    ];

    const pillarMat = new T.MeshStandardMaterial({ color: 0x1a1a1a, metalness: 0.7, roughness: 0.3 });

    pillarPositions.forEach((pos, i) => {
      const pillarHeight = 800 + Math.random() * 600;
      const pillarGeom = new T.CylinderGeometry(150, 150, pillarHeight, 8);
      const pillar = new T.Mesh(pillarGeom, pillarMat);
      pillar.position.set(pos.x, pillarHeight / 2, pos.z);
      scene.add(pillar);

      // Neon ring around pillar (alternating colors)
      const ringColor = i % 3 === 0 ? neonMagenta : i % 3 === 1 ? neonCyan : neonGreen;
      const ringMat = new T.MeshBasicMaterial({ color: ringColor });

      // Multiple rings up the pillar
      for (let h = 100; h < pillarHeight; h += 200) {
        const ring = new T.Mesh(new T.TorusGeometry(160, 8, 8, 16), ringMat);
        ring.position.set(pos.x, h, pos.z);
        ring.rotation.x = Math.PI / 2;
        scene.add(ring);
      }

      // Glowing top
      const topGlow = new T.Mesh(new T.CylinderGeometry(155, 155, 20, 8), ringMat);
      topGlow.position.set(pos.x, pillarHeight, pos.z);
      scene.add(topGlow);
    });

    // === FLOATING PLATFORMS with neon edges ===
    const platformPositions = [
      { x: -3000, y: 400, z: -1500, w: 600, d: 600 },
      { x: 3000, y: 500, z: -1500, w: 600, d: 600 },
      { x: -3000, y: 600, z: 1500, w: 600, d: 600 },
      { x: 3000, y: 450, z: 1500, w: 600, d: 600 },
      { x: 0, y: 700, z: 0, w: 1000, d: 1000 }, // Center platform
      { x: -5000, y: 300, z: 0, w: 800, d: 400 },
      { x: 5000, y: 350, z: 0, w: 800, d: 400 },
      { x: 0, y: 400, z: -5000, w: 400, d: 800 },
      { x: 0, y: 400, z: 5000, w: 400, d: 800 },
    ];

    const platformMat = new T.MeshStandardMaterial({ color: 0x151515, metalness: 0.6, roughness: 0.4 });

    platformPositions.forEach((plat, i) => {
      const platGeom = new T.BoxGeometry(plat.w, 40, plat.d);
      const platform = new T.Mesh(platGeom, platformMat);
      platform.position.set(plat.x, plat.y, plat.z);
      scene.add(platform);

      // Neon edge around platform
      const edgeColor = i % 2 === 0 ? neonPink : neonBlue;
      const edgeMat = new T.MeshBasicMaterial({ color: edgeColor });

      // Edge strips
      const edgeThickness = 15;
      // Front/back edges
      scene.add(new T.Mesh(new T.BoxGeometry(plat.w + 20, edgeThickness, edgeThickness), edgeMat)).position.set(plat.x, plat.y, plat.z + plat.d / 2);
      scene.add(new T.Mesh(new T.BoxGeometry(plat.w + 20, edgeThickness, edgeThickness), edgeMat)).position.set(plat.x, plat.y, plat.z - plat.d / 2);
      // Left/right edges
      scene.add(new T.Mesh(new T.BoxGeometry(edgeThickness, edgeThickness, plat.d + 20), edgeMat)).position.set(plat.x + plat.w / 2, plat.y, plat.z);
      scene.add(new T.Mesh(new T.BoxGeometry(edgeThickness, edgeThickness, plat.d + 20), edgeMat)).position.set(plat.x - plat.w / 2, plat.y, plat.z);

      // Underside glow
      const glowGeom = new T.BoxGeometry(plat.w * 0.8, 5, plat.d * 0.8);
      const glowMat = new T.MeshBasicMaterial({ color: edgeColor, transparent: true, opacity: 0.6 });
      const glow = new T.Mesh(glowGeom, glowMat);
      glow.position.set(plat.x, plat.y - 25, plat.z);
      scene.add(glow);
    });

    // === NEON LIGHT TUBES floating in air ===
    const tubeColors = [neonMagenta, neonCyan, neonGreen, neonPink, neonPurple];
    for (let i = 0; i < 40; i++) {
      const tubeLength = 200 + Math.random() * 800;
      const tubeGeom = new T.CylinderGeometry(8, 8, tubeLength, 6);
      const tubeMat = new T.MeshBasicMaterial({ color: tubeColors[i % tubeColors.length] });
      const tube = new T.Mesh(tubeGeom, tubeMat);
      tube.position.set(
        (Math.random() - 0.5) * arenaSize * 1.8,
        200 + Math.random() * 1500,
        (Math.random() - 0.5) * arenaSize * 1.8
      );
      tube.rotation.x = Math.random() * Math.PI;
      tube.rotation.z = Math.random() * Math.PI;
      scene.add(tube);
    }

    // === BARRIER WALLS (cover obstacles) ===
    const barrierMat = new T.MeshStandardMaterial({ color: 0x0a0a0a, metalness: 0.6, roughness: 0.5 });
    const barrierPositions = [
      { x: -1000, z: 0, w: 100, h: 300, d: 1200, color: neonCyan },
      { x: 1000, z: 0, w: 100, h: 300, d: 1200, color: neonMagenta },
      { x: 0, z: -1000, w: 1200, h: 300, d: 100, color: neonGreen },
      { x: 0, z: 1000, w: 1200, h: 300, d: 100, color: neonPink },
      { x: -5000, z: -3000, w: 800, h: 400, d: 100, color: neonBlue },
      { x: 5000, z: -3000, w: 800, h: 400, d: 100, color: neonPurple },
      { x: -5000, z: 3000, w: 800, h: 400, d: 100, color: neonCyan },
      { x: 5000, z: 3000, w: 800, h: 400, d: 100, color: neonMagenta },
    ];

    barrierPositions.forEach(bar => {
      const barGeom = new T.BoxGeometry(bar.w, bar.h, bar.d);
      const barrier = new T.Mesh(barGeom, barrierMat);
      barrier.position.set(bar.x, bar.h / 2, bar.z);
      scene.add(barrier);

      // Top neon strip
      const stripMat = new T.MeshBasicMaterial({ color: bar.color });
      const strip = new T.Mesh(new T.BoxGeometry(bar.w + 20, 12, bar.d + 20), stripMat);
      strip.position.set(bar.x, bar.h, bar.z);
      scene.add(strip);
    });

    // === CORNER TOWERS ===
    const towerCorners = [
      { x: -arenaSize + 300, z: -arenaSize + 300 },
      { x: arenaSize - 300, z: -arenaSize + 300 },
      { x: -arenaSize + 300, z: arenaSize - 300 },
      { x: arenaSize - 300, z: arenaSize - 300 },
    ];

    towerCorners.forEach((corner, i) => {
      const towerGeom = new T.BoxGeometry(400, 1200, 400);
      const towerMat = new T.MeshStandardMaterial({ color: 0x0d0d0d, metalness: 0.7, roughness: 0.3 });
      const tower = new T.Mesh(towerGeom, towerMat);
      tower.position.set(corner.x, 600, corner.z);
      scene.add(tower);

      // Vertical neon strips on corners
      const cornerColor = i % 2 === 0 ? neonMagenta : neonCyan;
      const vStripMat = new T.MeshBasicMaterial({ color: cornerColor });
      for (let edge = 0; edge < 4; edge++) {
        const vStrip = new T.Mesh(new T.BoxGeometry(15, 1200, 15), vStripMat);
        const offsetX = edge < 2 ? -190 : 190;
        const offsetZ = edge % 2 === 0 ? -190 : 190;
        vStrip.position.set(corner.x + offsetX, 600, corner.z + offsetZ);
        scene.add(vStrip);
      }

      // Beacon light on top
      const beaconGeom = new T.SphereGeometry(60, 8, 8);
      const beaconMat = new T.MeshBasicMaterial({ color: cornerColor });
      const beacon = new T.Mesh(beaconGeom, beaconMat);
      beacon.position.set(corner.x, 1250, corner.z);
      scene.add(beacon);
    });

  }, []);

  // Create the drone - FULLY GLOWING NEON for dark arena
  const createPlane = useCallback((scene) => {
    if (!THREE.current) return null;
    const T = THREE.current;

    const drone = new T.Group();

    // ALL materials use MeshBasicMaterial for guaranteed visibility in dark arena
    const bodyMat = new T.MeshBasicMaterial({ color: 0x224466 }); // Dark blue body
    const accentMat = new T.MeshBasicMaterial({ color: 0x00ffff }); // Bright cyan
    const frameMat = new T.MeshBasicMaterial({ color: 0x333355 }); // Frame color

    // Central body - flat oval shape
    const bodyGeom = new T.CylinderGeometry(8, 8, 4, 12);
    const body = new T.Mesh(bodyGeom, bodyMat);
    body.rotation.x = Math.PI / 2;
    drone.add(body);

    // Glowing ring around body
    const ringGeom = new T.TorusGeometry(9, 1, 8, 24);
    const ring = new T.Mesh(ringGeom, accentMat);
    ring.rotation.x = Math.PI / 2;
    drone.add(ring);

    // Top dome
    const domeGeom = new T.SphereGeometry(4, 12, 8, 0, Math.PI * 2, 0, Math.PI / 2);
    const domeMat = new T.MeshBasicMaterial({ color: 0x3366aa });
    const dome = new T.Mesh(domeGeom, domeMat);
    dome.position.set(0, 2, 0);
    drone.add(dome);

    // Camera lens - bright glowing
    const lensMat = new T.MeshBasicMaterial({ color: 0x00ffff });
    const lensGeom = new T.SphereGeometry(3, 8, 6);
    const lens = new T.Mesh(lensGeom, lensMat);
    lens.position.set(0, 0, 7);
    drone.add(lens);

    // Four arms with neon edges
    const armPositions = [
      { x: 18, z: 18, angle: Math.PI / 4 },
      { x: -18, z: 18, angle: -Math.PI / 4 },
      { x: 18, z: -18, angle: -Math.PI / 4 },
      { x: -18, z: -18, angle: Math.PI / 4 },
    ];

    armPositions.forEach((pos, i) => {
      // Arm - glowing frame
      const armGeom = new T.BoxGeometry(20, 2, 3);
      const arm = new T.Mesh(armGeom, frameMat);
      arm.position.set(pos.x / 2, 0, pos.z / 2);
      arm.rotation.y = pos.angle;
      drone.add(arm);

      // Neon edge on arm
      const armEdgeGeom = new T.BoxGeometry(20, 0.5, 0.5);
      const armEdge = new T.Mesh(armEdgeGeom, accentMat);
      armEdge.position.set(pos.x / 2, 1.2, pos.z / 2);
      armEdge.rotation.y = pos.angle;
      drone.add(armEdge);

      // Motor housing
      const motorGeom = new T.CylinderGeometry(4, 4, 4, 8);
      const motor = new T.Mesh(motorGeom, bodyMat);
      motor.position.set(pos.x, 0, pos.z);
      drone.add(motor);

      // Glowing motor ring
      const motorRingGeom = new T.TorusGeometry(4.5, 0.5, 6, 16);
      const motorRing = new T.Mesh(motorRingGeom, accentMat);
      motorRing.position.set(pos.x, 0, pos.z);
      motorRing.rotation.x = Math.PI / 2;
      drone.add(motorRing);

      // Propeller disc (simplified, always visible)
      const propGeom = new T.CylinderGeometry(6, 6, 0.3, 16);
      const propMat = new T.MeshBasicMaterial({ color: 0x88aacc, transparent: true, opacity: 0.4 });
      const prop = new T.Mesh(propGeom, propMat);
      prop.position.set(pos.x, 3, pos.z);
      prop.name = `prop${i}`;
      drone.add(prop);

      // LED light under each motor - BIG AND BRIGHT
      const ledColor = i < 2 ? 0x00ff44 : 0xff0044; // Front green, back red
      const ledMat = new T.MeshBasicMaterial({ color: ledColor });
      const ledGeom = new T.SphereGeometry(3, 8, 8);
      const led = new T.Mesh(ledGeom, ledMat);
      led.position.set(pos.x, -3, pos.z);
      drone.add(led);

      // Glow ring around LED
      const glowRingGeom = new T.TorusGeometry(5, 0.8, 6, 16);
      const glowRingMat = new T.MeshBasicMaterial({ color: ledColor, transparent: true, opacity: 0.6 });
      const glowRing = new T.Mesh(glowRingGeom, glowRingMat);
      glowRing.position.set(pos.x, -3, pos.z);
      glowRing.rotation.x = Math.PI / 2;
      drone.add(glowRing);
    });

    // Bottom sensor - bright orange
    const sensorMat = new T.MeshBasicMaterial({ color: 0xff6600 });
    const sensorGeom = new T.BoxGeometry(6, 2, 6);
    const sensor = new T.Mesh(sensorGeom, sensorMat);
    sensor.position.set(0, -3, 0);
    drone.add(sensor);

    // Glowing strips along bottom
    [-6, 6].forEach(offset => {
      const stripGeom = new T.BoxGeometry(2, 1, 25);
      const strip = new T.Mesh(stripGeom, accentMat);
      strip.position.set(offset, -2, 5);
      drone.add(strip);
    });

    // Gun pods - glowing
    const gunMat = new T.MeshBasicMaterial({ color: 0x445566 });
    [-10, 10].forEach(x => {
      // Gun pod body
      const gunPodGeom = new T.CylinderGeometry(1.5, 1.5, 12, 8);
      const gunPod = new T.Mesh(gunPodGeom, gunMat);
      gunPod.rotation.x = Math.PI / 2;
      gunPod.position.set(x, -1, 10);
      drone.add(gunPod);

      // Glowing gun barrel
      const barrelGeom = new T.CylinderGeometry(0.8, 0.8, 8, 6);
      const barrel = new T.Mesh(barrelGeom, accentMat);
      barrel.rotation.x = Math.PI / 2;
      barrel.position.set(x, -1, 18);
      drone.add(barrel);

      // Gun tip glow
      const gunTipGeom = new T.SphereGeometry(1.5, 8, 8);
      const gunTip = new T.Mesh(gunTipGeom, accentMat);
      gunTip.position.set(x, -1, 22);
      drone.add(gunTip);
    });

    // Add a point light to the drone so it illuminates nearby objects
    const droneLight = new T.PointLight(0x00ffff, 2, 500);
    droneLight.position.set(0, 0, 0);
    drone.add(droneLight);

    // SCALE UP the drone for visibility in the large arena
    drone.scale.set(5, 5, 5);

    // Initial position (in the laser tag arena)
    drone.position.set(0, 400, 5000);
    scene.add(drone);

    return drone;
  }, []);

  // Create LASER TAG targets - neon glowing orbs within the arena
  const createTargets = useCallback((scene) => {
    if (!THREE.current) return;
    const T = THREE.current;

    const targets = [];

    // Neon target colors
    const neonRed = 0xff0044;
    const neonOrange = 0xff6600;
    const neonYellow = 0xffff00;
    const neonGreen = 0x00ff44;

    // Targets positioned within the laser tag arena (arena is ~8000 units)
    const targetPositions = [
      // Center area - easy targets
      { x: 0, y: 500, z: 0, size: 80, health: 2, color: neonRed, ring: 0xff00ff },
      { x: 500, y: 600, z: -500, size: 60, health: 1, color: neonOrange, ring: 0x00ffff },
      { x: -500, y: 550, z: 500, size: 70, health: 2, color: neonYellow, ring: 0xff00ff },

      // Around the pillars
      { x: -2000, y: 700, z: -2000, size: 70, health: 2, color: neonRed, ring: 0x00ffff },
      { x: 2000, y: 650, z: -2000, size: 75, health: 2, color: neonOrange, ring: 0xff00ff },
      { x: -2000, y: 800, z: 2000, size: 65, health: 2, color: neonYellow, ring: 0x00ffff },
      { x: 2000, y: 750, z: 2000, size: 80, health: 3, color: neonRed, ring: 0xff00ff },

      // Near walls - harder to hit
      { x: -6000, y: 400, z: 0, size: 90, health: 3, color: neonGreen, ring: 0xff00ff },
      { x: 6000, y: 450, z: 0, size: 85, health: 3, color: neonGreen, ring: 0x00ffff },
      { x: 0, y: 500, z: -6000, size: 95, health: 4, color: neonOrange, ring: 0xff00ff },
      { x: 0, y: 550, z: 6000, size: 90, health: 3, color: neonYellow, ring: 0x00ffff },

      // High altitude - above platforms
      { x: -3000, y: 1000, z: 1500, size: 100, health: 4, color: neonRed, ring: 0x00ffff },
      { x: 3000, y: 1100, z: -1500, size: 110, health: 5, color: neonOrange, ring: 0xff00ff },
      { x: 0, y: 1200, z: 0, size: 120, health: 5, color: neonYellow, ring: 0x00ff00 },

      // Corner targets
      { x: -5500, y: 300, z: -5500, size: 60, health: 2, color: neonGreen, ring: 0xff00ff },
      { x: 5500, y: 350, z: -5500, size: 65, health: 2, color: neonGreen, ring: 0x00ffff },
      { x: -5500, y: 380, z: 5500, size: 70, health: 2, color: neonGreen, ring: 0xff00ff },
    ];

    targetPositions.forEach((pos, index) => {
      const targetGroup = new T.Group();

      // Main target sphere - glowing neon
      const targetGeom = new T.SphereGeometry(pos.size, 16, 12);
      const targetMat = new T.MeshBasicMaterial({
        color: pos.color,
        transparent: true,
        opacity: 0.9,
      });
      const targetMesh = new T.Mesh(targetGeom, targetMat);
      targetGroup.add(targetMesh);

      // Inner glowing core - bright white
      const coreGeom = new T.SphereGeometry(pos.size * 0.4, 12, 8);
      const coreMat = new T.MeshBasicMaterial({
        color: 0xffffff,
        transparent: true,
        opacity: 0.8,
      });
      const core = new T.Mesh(coreGeom, coreMat);
      targetGroup.add(core);

      // Outer neon ring
      const ringGeom = new T.TorusGeometry(pos.size * 1.4, pos.size * 0.08, 8, 24);
      const ringMat = new T.MeshBasicMaterial({ color: pos.ring });
      const ring = new T.Mesh(ringGeom, ringMat);
      ring.rotation.x = Math.PI / 2;
      targetGroup.add(ring);

      // Second ring (perpendicular) - different color
      const ring2Mat = new T.MeshBasicMaterial({ color: pos.color });
      const ring2 = new T.Mesh(ringGeom, ring2Mat);
      ring2.rotation.y = Math.PI / 2;
      targetGroup.add(ring2);

      // Pulsing outer glow effect
      const glowGeom = new T.SphereGeometry(pos.size * 1.6, 8, 6);
      const glowMat = new T.MeshBasicMaterial({
        color: pos.ring,
        transparent: true,
        opacity: 0.15,
      });
      const glow = new T.Mesh(glowGeom, glowMat);
      targetGroup.add(glow);

      targetGroup.position.set(pos.x, pos.y, pos.z);
      scene.add(targetGroup);

      targets.push({
        mesh: targetGroup,
        mainMesh: targetMesh,
        core: core,
        health: pos.health,
        maxHealth: pos.health,
        size: pos.size,
        position: { x: pos.x, y: pos.y, z: pos.z },
        id: index,
        destroyed: false,
      });
    });

    targetsRef.current = targets;
    return targets;
  }, []);

  // Fire bullets from the drone
  const fireBullets = useCallback(() => {
    if (!THREE.current || !planeRef.current || !physicsRef.current || !sceneRef.current) return;
    if (ammoRef.current <= 0) return;

    const T = THREE.current;
    const drone = planeRef.current;
    const physics = physicsRef.current;
    const scene = sceneRef.current;

    // Rate limiting - can fire every 100ms
    const now = Date.now();
    if (now - lastFireTimeRef.current < 100) return;
    lastFireTimeRef.current = now;

    // Create bullet - MUCH larger with GLOW for visibility
    const bulletGeom = new T.SphereGeometry(12, 8, 6); // Much bigger bullets
    const bulletMat = new T.MeshBasicMaterial({
      color: 0x00ffff, // Cyan to match drone
    });

    // Outer glow sphere for bullets
    const bulletGlowGeom = new T.SphereGeometry(18, 8, 6);
    const bulletGlowMat = new T.MeshBasicMaterial({
      color: 0x00ffff,
      transparent: true,
      opacity: 0.3,
    });

    // Fire from both gun pod positions (drone guns are at x: -8, 8, z: 14) - scaled by drone scale
    [-8, 8].forEach(gunOffset => {
      // Create bullet group with core and glow
      const bulletGroup = new T.Group();
      const bulletCore = new T.Mesh(bulletGeom, bulletMat.clone());
      const bulletGlow = new T.Mesh(bulletGlowGeom, bulletGlowMat.clone());
      bulletGroup.add(bulletCore);
      bulletGroup.add(bulletGlow);

      // Calculate bullet starting position (at the gun barrels, accounting for drone scale of 4)
      const gunPos = new T.Vector3(gunOffset * 4, -1 * 4, 16 * 4);
      gunPos.applyQuaternion(drone.quaternion);
      gunPos.add(drone.position);
      bulletGroup.position.copy(gunPos);

      // Calculate bullet velocity - shoot straight forward from where drone is facing
      const bulletSpeed = 600; // Good speed for drone bullets
      const forward = new T.Vector3(
        Math.sin(physics.yaw),
        0, // Bullets fly level
        Math.cos(physics.yaw)
      );

      const velocity = {
        x: forward.x * bulletSpeed + physics.velocity.x * 0.5,
        y: physics.velocity.y * 0.3, // Slight vertical influence
        z: forward.z * bulletSpeed + physics.velocity.z * 0.5,
      };

      scene.add(bulletGroup);

      bulletsRef.current.push({
        mesh: bulletGroup,
        velocity: velocity,
        lifetime: 4, // seconds - longer lifetime
        created: now,
      });
    });

    // Update both ref and state
    ammoRef.current = Math.max(0, ammoRef.current - 2);
    setAmmo(ammoRef.current);
  }, []);

  // Update bullets and check collisions
  const updateBullets = useCallback((createExplosionFn) => {
    if (!THREE.current || !sceneRef.current) return;

    const scene = sceneRef.current;
    const now = Date.now();
    const dt = 0.016;

    // Update each bullet
    bulletsRef.current = bulletsRef.current.filter(bullet => {
      // Check lifetime
      if ((now - bullet.created) / 1000 > bullet.lifetime) {
        scene.remove(bullet.mesh);
        // Dispose of all children in the group
        bullet.mesh.traverse((child) => {
          if (child.geometry) child.geometry.dispose();
          if (child.material) child.material.dispose();
        });
        return false;
      }

      // Update position
      bullet.mesh.position.x += bullet.velocity.x * dt;
      bullet.mesh.position.y += bullet.velocity.y * dt;
      bullet.mesh.position.z += bullet.velocity.z * dt;

      // Check collision with targets
      for (const target of targetsRef.current) {
        if (target.destroyed) continue;

        const dx = bullet.mesh.position.x - target.position.x;
        const dy = bullet.mesh.position.y - target.position.y;
        const dz = bullet.mesh.position.z - target.position.z;
        const distance = Math.sqrt(dx * dx + dy * dy + dz * dz);

        if (distance < target.size * 1.5) {
          // Hit! - increased hit detection radius
          target.health--;

          // Flash the target
          if (target.mainMesh && target.mainMesh.material) {
            target.mainMesh.material.color.setHex(0xffffff);
            setTimeout(() => {
              if (target.mainMesh && target.mainMesh.material && !target.destroyed) {
                const healthRatio = target.health / target.maxHealth;
                const r = 1;
                const g = healthRatio * 0.5;
                const b = healthRatio * 0.3;
                target.mainMesh.material.color.setRGB(r, g, b);
              }
            }, 50);
          }

          // Check if destroyed
          if (target.health <= 0) {
            target.destroyed = true;
            scene.remove(target.mesh);
            setScore(prev => prev + target.maxHealth * 100);
            setTargetsDestroyed(prev => prev + 1);

            // Create explosion effect
            if (createExplosionFn) {
              createExplosionFn(target.position);
            }
          }

          // Remove bullet
          scene.remove(bullet.mesh);
          bullet.mesh.traverse((child) => {
            if (child.geometry) child.geometry.dispose();
            if (child.material) child.material.dispose();
          });
          return false;
        }
      }

      return true;
    });
  }, []);

  // Create explosion effect
  const createExplosion = useCallback((position) => {
    if (!THREE.current || !sceneRef.current) return;

    const T = THREE.current;
    const scene = sceneRef.current;

    // Create explosion particles
    const particles = [];
    const numParticles = 20;

    for (let i = 0; i < numParticles; i++) {
      const size = 10 + Math.random() * 20;
      const particleGeom = new T.SphereGeometry(size, 6, 4);
      const particleMat = new T.MeshBasicMaterial({
        color: Math.random() > 0.5 ? 0xff6600 : 0xffaa00,
        transparent: true,
        opacity: 1,
      });
      const particle = new T.Mesh(particleGeom, particleMat);

      particle.position.set(position.x, position.y, position.z);

      const speed = 50 + Math.random() * 100;
      const theta = Math.random() * Math.PI * 2;
      const phi = Math.random() * Math.PI;

      particle.userData.velocity = {
        x: Math.sin(phi) * Math.cos(theta) * speed,
        y: Math.sin(phi) * Math.sin(theta) * speed,
        z: Math.cos(phi) * speed,
      };
      particle.userData.created = Date.now();

      scene.add(particle);
      particles.push(particle);
    }

    // Animate explosion
    const animateExplosion = () => {
      const now = Date.now();
      let allDone = true;

      particles.forEach(particle => {
        const age = (now - particle.userData.created) / 1000;
        if (age < 1) {
          allDone = false;
          particle.position.x += particle.userData.velocity.x * 0.016;
          particle.position.y += particle.userData.velocity.y * 0.016;
          particle.position.z += particle.userData.velocity.z * 0.016;
          particle.material.opacity = 1 - age;
          particle.scale.setScalar(1 + age * 2);
        } else {
          scene.remove(particle);
          particle.geometry.dispose();
          particle.material.dispose();
        }
      });

      if (!allDone) {
        requestAnimationFrame(animateExplosion);
      }
    };

    animateExplosion();
  }, []);

  // Drone physics update - SUPER SIMPLE kid-friendly controls
  // Swipe/drag in any direction = drone moves that direction
  // No complex steering - just point and go!
  const updateFlightPhysics = useCallback(() => {
    if (!planeRef.current || !physicsRef.current) return;

    const physics = physicsRef.current;
    const keys = keysRef.current;
    const touch = touchControlsRef.current;
    const drone = planeRef.current;

    const dt = 0.016; // ~60fps

    // Simple drone constants - easy to control
    const MAX_SPEED = 120; // Not too fast for kids
    const ACCELERATION = 8; // Responsive but not twitchy
    const FRICTION = 0.92; // Slows down quickly when you let go
    const TILT_AMOUNT = 0.2; // Gentle visual tilt

    // === SIMPLE DIRECT MOVEMENT ===
    // Touch controls: swipe in any direction to move that way
    // The drone faces the direction it's moving
    let moveX = 0;
    let moveZ = 0;
    let moveY = 0;

    // Keyboard controls - W/S for forward/back, A/D for left/right
    if (keys['KeyW'] || keys['ArrowUp']) moveZ = 1;
    if (keys['KeyS'] || keys['ArrowDown']) moveZ = -1;
    if (keys['KeyA'] || keys['ArrowLeft']) moveX = -1;
    if (keys['KeyD'] || keys['ArrowRight']) moveX = 1;
    if (keys['ShiftLeft'] || keys['ShiftRight']) moveY = 1;
    if (keys['ControlLeft'] || keys['ControlRight']) moveY = -1;

    // Unified joystick: swipe controls all movement
    // Left/Right = strafe, Up/Down = altitude (push up to go up, push down to go down)
    if (Math.abs(touch.roll) > 0.05) {
      moveX = touch.roll; // Push right = strafe right
    }
    if (Math.abs(touch.pitch) > 0.05) {
      moveY = -touch.pitch; // Push up = fly up, push down = fly down
    }

    // Apply movement in world space (simple, direct)
    physics.velocity.x += moveX * ACCELERATION;
    physics.velocity.z += moveZ * ACCELERATION;
    physics.velocity.y += moveY * ACCELERATION * 0.5; // Slower vertical

    // Apply friction (smooth stop when you let go)
    physics.velocity.x *= FRICTION;
    physics.velocity.z *= FRICTION;
    physics.velocity.y *= FRICTION;

    // Clamp speeds
    const horizontalSpeed = Math.sqrt(physics.velocity.x ** 2 + physics.velocity.z ** 2);
    if (horizontalSpeed > MAX_SPEED) {
      const scale = MAX_SPEED / horizontalSpeed;
      physics.velocity.x *= scale;
      physics.velocity.z *= scale;
    }
    physics.velocity.y = Math.max(-30, Math.min(30, physics.velocity.y));

    // Update position
    physics.position.x += physics.velocity.x * dt;
    physics.position.y += physics.velocity.y * dt;
    physics.position.z += physics.velocity.z * dt;

    // Ground collision - bounce gently
    const groundLevel = 30;
    if (physics.position.y < groundLevel) {
      physics.position.y = groundLevel;
      physics.velocity.y = Math.abs(physics.velocity.y) * 0.3; // Gentle bounce
    }

    // Sky limit
    if (physics.position.y > 3000) {
      physics.position.y = 3000;
      physics.velocity.y = Math.min(0, physics.velocity.y);
    }

    // Auto-rotate to face movement direction (smooth)
    if (horizontalSpeed > 5) {
      const targetYaw = Math.atan2(physics.velocity.x, physics.velocity.z);
      // Smooth rotation towards movement direction
      let yawDiff = targetYaw - physics.yaw;
      // Normalize angle difference
      while (yawDiff > Math.PI) yawDiff -= Math.PI * 2;
      while (yawDiff < -Math.PI) yawDiff += Math.PI * 2;
      physics.yaw += yawDiff * 0.1; // Smooth turn
    }

    // Visual tilt based on movement (makes it feel alive)
    const targetPitch = (moveZ * TILT_AMOUNT); // Tilt forward/back
    const targetRoll = (-moveX * TILT_AMOUNT); // Tilt left/right

    physics.pitch = physics.pitch * 0.9 + targetPitch * 0.1;
    physics.roll = physics.roll * 0.9 + targetRoll * 0.1;

    // Calculate speed for display
    physics.speed = Math.sqrt(
      physics.velocity.x ** 2 +
      physics.velocity.y ** 2 +
      physics.velocity.z ** 2
    );

    // Update drone visual
    drone.position.set(physics.position.x, physics.position.y, physics.position.z);
    drone.rotation.set(physics.pitch, physics.yaw, physics.roll);

    // Spin all propellers!
    drone.children.forEach(child => {
      if (child.name && child.name.startsWith('prop')) {
        child.rotation.y += 0.8;
      }
    });

    // Update HUD
    setSpeed(Math.round(physics.speed * 2.237)); // m/s to mph
    setAltitude(Math.round(physics.position.y * 3.281)); // meters to feet
    setHeading(Math.round(((physics.yaw * 180 / Math.PI) % 360 + 360) % 360));
    setPitch(Math.round(physics.pitch * 180 / Math.PI));
    setThrottle(Math.round(Math.min(100, horizontalSpeed / MAX_SPEED * 100)));
    setStalled(false);

  }, []);

  // Initialize Three.js scene
  useEffect(() => {
    let isMounted = true;

    const init = async () => {
      try {
        const ThreeModule = await import('three');
        THREE.current = ThreeModule;
        const T = THREE.current;

        if (!isMounted || !containerRef.current) return;

        // Initialize physics - helicopter starts hovering
        physicsRef.current = {
          position: { x: 0, y: 400, z: 5000 }, // Match drone starting position
          velocity: { x: 0, y: 0, z: 0 }, // Hovering, no initial velocity
          pitch: 0,
          yaw: Math.PI, // Face towards center of arena
          roll: 0,
          throttle: 60,
          speed: 80,
        };

        // Scene - Dark for laser tag
        const scene = new T.Scene();
        scene.background = new T.Color(0x000000); // Pure black
        scene.fog = new T.Fog(0x000011, 500, 12000); // Dark fog, shorter distance for arena
        sceneRef.current = scene;

        // Camera - initialize near the plane's starting position
        const camera = new T.PerspectiveCamera(
          70,
          window.innerWidth / window.innerHeight,
          0.1,
          60000
        );
        // Set initial camera position behind the plane (plane starts at 3000, 500, -1000)
        camera.position.set(3000, 530, -1080);
        camera.lookAt(3000, 500, -1000);
        cameraRef.current = camera;

        // Renderer
        const renderer = new T.WebGLRenderer({
          antialias: false,
          powerPreference: 'high-performance',
          alpha: false,
        });
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        renderer.setClearColor(0x000000, 1); // Pure black background for laser tag

        // Ensure container is empty before adding
        while (containerRef.current.firstChild) {
          containerRef.current.removeChild(containerRef.current.firstChild);
        }
        containerRef.current.appendChild(renderer.domElement);
        rendererRef.current = renderer;

        // Debug: Log container and renderer dimensions
        console.log('FlyGame: Renderer initialized');
        console.log('FlyGame: Container size:', containerRef.current.offsetWidth, 'x', containerRef.current.offsetHeight);
        console.log('FlyGame: Window size:', window.innerWidth, 'x', window.innerHeight);
        console.log('FlyGame: Canvas size:', renderer.domElement.width, 'x', renderer.domElement.height);

        // Lighting - Dark ambient for laser tag feel
        const ambientLight = new T.AmbientLight(0x111122, 0.3); // Very dim bluish ambient
        scene.add(ambientLight);

        // Point lights for neon glow effect
        const magentaLight = new T.PointLight(0xff00ff, 0.8, 5000);
        magentaLight.position.set(-3000, 1000, 0);
        scene.add(magentaLight);

        const cyanLight = new T.PointLight(0x00ffff, 0.8, 5000);
        cyanLight.position.set(3000, 1000, 0);
        scene.add(cyanLight);

        const greenLight = new T.PointLight(0x00ff00, 0.5, 4000);
        greenLight.position.set(0, 500, -3000);
        scene.add(greenLight);

        const purpleLight = new T.PointLight(0x9900ff, 0.6, 4000);
        purpleLight.position.set(0, 1500, 3000);
        scene.add(purpleLight);

        // Create terrain and plane
        console.log('FlyGame: Creating terrain...');
        createTerrain(scene);
        console.log('FlyGame: Creating plane...');
        const plane = createPlane(scene);
        planeRef.current = plane;
        console.log('FlyGame: Plane created at', plane.position);

        // Create targets
        console.log('FlyGame: Creating targets...');
        createTargets(scene);
        console.log('FlyGame: Targets created:', targetsRef.current.length);

        // Force multiple initial renders to ensure scene is visible
        renderer.render(scene, camera);
        requestAnimationFrame(() => renderer.render(scene, camera));
        console.log('FlyGame: Initial render complete, scene children:', scene.children.length);

        // Add floating neon particles/dust for atmosphere
        const particleColors = [0xff00ff, 0x00ffff, 0x00ff00, 0xff1493, 0x9900ff];
        for (let i = 0; i < 80; i++) {
          const particleGeom = new T.SphereGeometry(5 + Math.random() * 10, 4, 4);
          const particleMat = new T.MeshBasicMaterial({
            color: particleColors[Math.floor(Math.random() * particleColors.length)],
            transparent: true,
            opacity: 0.6 + Math.random() * 0.4,
          });
          const particle = new T.Mesh(particleGeom, particleMat);
          particle.position.set(
            (Math.random() - 0.5) * 15000,
            100 + Math.random() * 1800,
            (Math.random() - 0.5) * 15000
          );
          scene.add(particle);
        }

        setIsLoading(false);
        console.log('FlyGame: Loading complete, starting animation');

        // Animation loop
        let frameCount = 0;
        const animate = () => {
          if (!isMounted) return;
          animationRef.current = requestAnimationFrame(animate);

          // Only update physics when game is running (controls dismissed)
          if (planeRef.current && physicsRef.current && gameRunningRef.current) {
            updateFlightPhysics();

            // Handle firing
            if (keysRef.current['Space'] || isFiringRef.current) {
              fireBullets();
            }

            // Update bullets (pass createExplosion function)
            updateBullets(createExplosion);
          }

          // Camera follows plane (chase camera)
          if (planeRef.current && cameraRef.current && physicsRef.current) {
            const planeObj = planeRef.current;
            const physics = physicsRef.current;

            // Calculate camera position behind and above plane
            const cameraDistance = 80;
            const cameraHeight = 30;

            const camX = planeObj.position.x - Math.sin(physics.yaw) * cameraDistance;
            const camY = planeObj.position.y + cameraHeight;
            const camZ = planeObj.position.z - Math.cos(physics.yaw) * cameraDistance;

            // Smooth camera movement
            cameraRef.current.position.lerp(new T.Vector3(camX, camY, camZ), 0.1);
            cameraRef.current.lookAt(planeObj.position);
          }

          renderer.render(scene, camera);

          frameCount++;
          if (frameCount <= 5 || frameCount % 100 === 0) {
            console.log('FlyGame: Frame', frameCount, 'Camera:', camera.position, 'Plane:', planeRef.current?.position);
          }
        };

        // Start animation loop
        console.log('FlyGame: Starting animation loop');
        animate();

        // Keyboard controls
        const handleKeyDown = (e) => {
          keysRef.current[e.code] = true;
          if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'Space'].includes(e.code)) {
            e.preventDefault();
          }
        };

        const handleKeyUp = (e) => {
          keysRef.current[e.code] = false;
        };

        window.addEventListener('keydown', handleKeyDown);
        window.addEventListener('keyup', handleKeyUp);

        // Resize handler
        const handleResize = () => {
          if (!cameraRef.current || !rendererRef.current) return;
          const width = window.innerWidth;
          const height = window.innerHeight;
          cameraRef.current.aspect = width / height;
          cameraRef.current.updateProjectionMatrix();
          rendererRef.current.setSize(width, height);
          setIsLandscape(width > height);
        };

        window.addEventListener('resize', handleResize);

        // Cleanup
        return () => {
          window.removeEventListener('keydown', handleKeyDown);
          window.removeEventListener('keyup', handleKeyUp);
          window.removeEventListener('resize', handleResize);
        };

      } catch (err) {
        console.error('Failed to initialize FlyGame:', err);
      }
    };

    init();

    return () => {
      isMounted = false;
      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }
      if (rendererRef.current && containerRef.current) {
        containerRef.current.removeChild(rendererRef.current.domElement);
        rendererRef.current.dispose();
      }
      // Clean up bullets
      bulletsRef.current = [];
      targetsRef.current = [];
    };
  // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [createTerrain, createPlane, createTargets]);

  // Touch controls - Flight stick (right side)
  const flightStickStartRef = useRef(null);

  const handleFlightStickStart = useCallback((e) => {
    e.preventDefault();
    const touch = e.touches[0];
    flightStickStartRef.current = { x: touch.clientX, y: touch.clientY };
  }, []);

  const handleFlightStickMove = useCallback((e) => {
    e.preventDefault();
    if (!flightStickStartRef.current) return;

    const touch = e.touches[0];
    const rect = e.currentTarget.getBoundingClientRect();
    const maxOffset = Math.min(rect.width, rect.height) / 3;

    const deltaX = touch.clientX - flightStickStartRef.current.x;
    const deltaY = touch.clientY - flightStickStartRef.current.y;

    // Roll: left/right movement (-1 to 1)
    touchControlsRef.current.roll = Math.max(-1, Math.min(1, deltaX / maxOffset));
    // Pitch: up/down movement (-1 to 1) - up is negative (dive), down is positive (climb)
    touchControlsRef.current.pitch = Math.max(-1, Math.min(1, deltaY / maxOffset));
  }, []);

  const handleFlightStickEnd = useCallback(() => {
    flightStickStartRef.current = null;
    touchControlsRef.current.roll = 0;
    touchControlsRef.current.pitch = 0;
  }, []);

  // Touch controls - Throttle (left side)
  const throttleStartRef = useRef(null);

  const handleThrottleStart = useCallback((e) => {
    e.preventDefault();
    const touch = e.touches[0];
    throttleStartRef.current = { y: touch.clientY };
  }, []);

  const handleThrottleMove = useCallback((e) => {
    e.preventDefault();
    if (!throttleStartRef.current) return;

    const touch = e.touches[0];
    const rect = e.currentTarget.getBoundingClientRect();
    const maxOffset = rect.height / 3;

    const deltaY = throttleStartRef.current.y - touch.clientY; // Inverted: drag up = increase

    // Throttle change: -1 (decrease) to 1 (increase)
    touchControlsRef.current.throttleChange = Math.max(-1, Math.min(1, deltaY / maxOffset));
  }, []);

  const handleThrottleEnd = useCallback(() => {
    throttleStartRef.current = null;
    touchControlsRef.current.throttleChange = 0;
  }, []);

  return (
    <div className="fly-game">
      {/* Loading overlay */}
      {isLoading && (
        <div className="fly-loading">
          <div className="loading-spinner"></div>
          <p>Preparing for takeoff...</p>
        </div>
      )}

      {/* Rotate device overlay - shown when in portrait mode */}
      {!isLandscape && !isLoading && (
        <div className="rotate-device-overlay">
          <div className="rotate-content">
            <div className="rotate-icon">📱↻</div>
            <h2>Rotate Your Device</h2>
            <p>Please rotate to landscape mode for the best flying experience</p>
          </div>
        </div>
      )}

      {/* Always render canvas container so Three.js scene persists */}
      <div ref={containerRef} className="fly-canvas-container" />

      {/* Exit Button - always visible */}
      <button className="exit-btn" onClick={onExit}>✕</button>

      {/* Mini HUD - show when not loading and controls hint is dismissed */}
      {!isLoading && !showControls && (
      <div className="flight-hud mini-hud">
        {/* Compact Weapons/Score display */}
        <div className="hud-panel weapons-panel-mini">
          <div className="mini-stat">
            <span className="mini-icon">🎯</span>
            <span className="mini-value">{targetsDestroyed}/17</span>
          </div>
          <div className="mini-stat">
            <span className="mini-icon">⭐</span>
            <span className="mini-value">{score.toLocaleString()}</span>
          </div>
          <div className="mini-stat">
            <span className="mini-icon">💥</span>
            <span className="mini-value">{ammo}</span>
          </div>
        </div>
      </div>
      )}

      {/* Touch Controls - show when not loading and controls hint is dismissed */}
      {!isLoading && !showControls && isLandscape && (
      <div className="touch-controls unified-controls">
        {/* Left side - Unified joystick for ALL movement */}
        <div
          className="joystick-area unified-joystick"
          onTouchStart={handleFlightStickStart}
          onTouchMove={handleFlightStickMove}
          onTouchEnd={handleFlightStickEnd}
          onTouchCancel={handleFlightStickEnd}
        >
          <div className="joystick-zone large-zone">
            <div className="joystick-ring"></div>
            <div className="joystick-arrows">
              <span className="arrow up">▲</span>
              <span className="arrow down">▼</span>
              <span className="arrow left">◀</span>
              <span className="arrow right">▶</span>
            </div>
            <div className="joystick-center"></div>
            <span className="joystick-label">FLY</span>
          </div>
        </div>

        {/* Right side - Big Fire button */}
        <div className="fire-button-area">
          <button
            className="fire-btn big-fire-btn"
            onTouchStart={(e) => { e.preventDefault(); isFiringRef.current = true; }}
            onTouchEnd={(e) => { e.preventDefault(); isFiringRef.current = false; }}
            onTouchCancel={() => { isFiringRef.current = false; }}
            onMouseDown={() => { isFiringRef.current = true; }}
            onMouseUp={() => { isFiringRef.current = false; }}
            onMouseLeave={() => { isFiringRef.current = false; }}
          >
            <span className="fire-icon">🎯</span>
            <span className="fire-label">SHOOT!</span>
          </button>
        </div>
      </div>
      )}

      {/* Controls Help - show when not loading */}
      {showControls && !isLoading && (
        <div
          className="controls-hint"
          onClick={() => { gameRunningRef.current = true; setShowControls(false); }}
          onTouchEnd={(e) => { e.preventDefault(); gameRunningRef.current = true; setShowControls(false); }}
        >
          <div className="hint-content" onClick={(e) => e.stopPropagation()} onTouchEnd={(e) => e.stopPropagation()}>
            <h3>🛸 Fly the Drone!</h3>
            <button
              className="takeoff-btn"
              onClick={(e) => { e.stopPropagation(); gameRunningRef.current = true; setShowControls(false); }}
              onTouchStart={(e) => { e.stopPropagation(); }}
              onTouchEnd={(e) => { e.preventDefault(); e.stopPropagation(); gameRunningRef.current = true; setShowControls(false); }}
              onPointerDown={(e) => { e.stopPropagation(); }}
              onPointerUp={(e) => { e.preventDefault(); e.stopPropagation(); gameRunningRef.current = true; setShowControls(false); }}
            >
              🚀 START!
            </button>
            <div className="controls-grid simple-grid">
              <div className="control-item big-item">
                <span className="key">🕹️ JOYSTICK</span>
                <span>Swipe to fly anywhere!</span>
              </div>
              <div className="control-item big-item">
                <span className="key">🎯 SHOOT</span>
                <span>Tap to blast targets!</span>
              </div>
            </div>
            <p className="hint-tip">🎮 One touch to fly, one to shoot!</p>
          </div>
        </div>
      )}
    </div>
  );
};

export default FlyGame;
