import React, { useEffect, useRef, useState, useCallback } from 'react';
import './SimulatorGame.css';

// Dynamically import THREE to avoid SSR issues
let THREE = null;

const SimulatorGame = ({ onClose, onExit }) => {
  // Support both onClose (from GameHub) and onExit prop names
  const handleExit = onClose || onExit;
  const containerRef = useRef(null);
  const sceneRef = useRef(null);
  const cameraRef = useRef(null);
  const rendererRef = useRef(null);
  const carRef = useRef(null);
  const animationRef = useRef(null);
  const keysRef = useRef({});
  const touchControlsRef = useRef({ steering: 0, gas: false, brake: false });
  const carPhysicsRef = useRef(null);

  const [speed, setSpeed] = useState(0);
  const [isLandscape, setIsLandscape] = useState(window.innerWidth > window.innerHeight);
  const [showControls, setShowControls] = useState(true);
  const [gear, setGear] = useState('D');
  const [rpm, setRpm] = useState(0);
  const [isLoading, setIsLoading] = useState(true);
  const [error, setError] = useState(null);
  const [altitude, setAltitude] = useState(0);
  const [isFlying, setIsFlying] = useState(false);
  const rampsRef = useRef([]);
  const neonLightsRef = useRef([]);

  // Day/night cycle refs (5 minute full cycle)
  const dayTimeRef = useRef(0); // 0 to 1 representing full day cycle
  const ambientLightRef = useRef(null);
  const hemiLightRef = useRef(null);
  const mainLightRef = useRef(null);
  const DAY_CYCLE_DURATION = 300; // 5 minutes in seconds

  // Check if current player is the host
  const isHost = true;

  // Create road network - REALISTIC CITY ROADS with proper crosswalks
  const createRoads = useCallback((scene) => {
    if (!THREE) return;

    const scale = 2;
    const citySize = 12; // Doubled downtown area

    // 4K QUALITY MATERIALS - Realistic PBR materials
    // Realistic asphalt - darker gray with roughness
    const asphaltMaterial = new THREE.MeshStandardMaterial({
      color: 0x2a2a2a,
      roughness: 0.9,
      metalness: 0.1,
    });
    // Concrete sidewalks - lighter gray with texture feel
    const sidewalkMaterial = new THREE.MeshStandardMaterial({
      color: 0x888888,
      roughness: 0.85,
      metalness: 0.05,
    });
    // Curb material - slightly darker than sidewalk
    const curbMaterial = new THREE.MeshStandardMaterial({
      color: 0x666666,
      roughness: 0.8,
      metalness: 0.05,
    });
    // Yellow center line - slight gloss for paint
    const yellowLineMat = new THREE.MeshStandardMaterial({
      color: 0xf4d03f,
      roughness: 0.6,
      metalness: 0.1,
      emissive: 0xf4d03f,
      emissiveIntensity: 0.1,
    });
    // White lane markings and crosswalks - slight gloss
    const whiteLineMat = new THREE.MeshStandardMaterial({
      color: 0xffffff,
      roughness: 0.5,
      metalness: 0.1,
      emissive: 0xffffff,
      emissiveIntensity: 0.05,
    });

    const roadWidth = 20 * scale;
    const roadLength = 500 * scale * citySize;

    // Shared geometries
    const hRoadGeom = new THREE.BoxGeometry(roadLength, 0.1, roadWidth);
    const vRoadGeom = new THREE.BoxGeometry(roadWidth, 0.1, roadLength);
    const hSidewalkGeom = new THREE.BoxGeometry(roadLength, 0.15, 5 * scale);
    const vSidewalkGeom = new THREE.BoxGeometry(5 * scale, 0.15, roadLength);
    const hCurbGeom = new THREE.BoxGeometry(roadLength, 0.25, 0.4 * scale);
    const vCurbGeom = new THREE.BoxGeometry(0.4 * scale, 0.25, roadLength);

    // Store road positions for crosswalk placement
    const hRoadPositions = [];
    const vRoadPositions = [];

    // Horizontal roads
    for (let z = -200 * scale * citySize; z <= 200 * scale * citySize; z += 80 * scale) {
      hRoadPositions.push(z);

      // Main road surface
      const road = new THREE.Mesh(hRoadGeom, asphaltMaterial);
      road.position.set(0, 0.05, z);
      scene.add(road);

      // Sidewalks (wider, more realistic)
      const leftSidewalk = new THREE.Mesh(hSidewalkGeom, sidewalkMaterial);
      leftSidewalk.position.set(0, 0.1, z - roadWidth/2 - 2.5 * scale);
      scene.add(leftSidewalk);
      const rightSidewalk = new THREE.Mesh(hSidewalkGeom, sidewalkMaterial);
      rightSidewalk.position.set(0, 0.1, z + roadWidth/2 + 2.5 * scale);
      scene.add(rightSidewalk);

      // Curbs (raised edge between road and sidewalk)
      const leftCurb = new THREE.Mesh(hCurbGeom, curbMaterial);
      leftCurb.position.set(0, 0.15, z - roadWidth/2 - 0.2 * scale);
      scene.add(leftCurb);
      const rightCurb = new THREE.Mesh(hCurbGeom, curbMaterial);
      rightCurb.position.set(0, 0.15, z + roadWidth/2 + 0.2 * scale);
      scene.add(rightCurb);
    }

    // Vertical roads
    for (let x = -200 * scale * citySize; x <= 200 * scale * citySize; x += 80 * scale) {
      vRoadPositions.push(x);

      const road = new THREE.Mesh(vRoadGeom, asphaltMaterial);
      road.position.set(x, 0.05, 0);
      scene.add(road);

      const leftSidewalk = new THREE.Mesh(vSidewalkGeom, sidewalkMaterial);
      leftSidewalk.position.set(x - roadWidth/2 - 2.5 * scale, 0.1, 0);
      scene.add(leftSidewalk);
      const rightSidewalk = new THREE.Mesh(vSidewalkGeom, sidewalkMaterial);
      rightSidewalk.position.set(x + roadWidth/2 + 2.5 * scale, 0.1, 0);
      scene.add(rightSidewalk);

      const leftCurb = new THREE.Mesh(vCurbGeom, curbMaterial);
      leftCurb.position.set(x - roadWidth/2 - 0.2 * scale, 0.15, 0);
      scene.add(leftCurb);
      const rightCurb = new THREE.Mesh(vCurbGeom, curbMaterial);
      rightCurb.position.set(x + roadWidth/2 + 0.2 * scale, 0.15, 0);
      scene.add(rightCurb);
    }

    // Add center yellow dashed lines (only on some roads for performance)
    const yellowDashGeom = new THREE.BoxGeometry(12 * scale, 0.12, 0.5 * scale);
    const yellowDashGeomV = new THREE.BoxGeometry(0.5 * scale, 0.12, 12 * scale);

    // Horizontal road center lines (every other road)
    hRoadPositions.forEach((z, idx) => {
      if (idx % 2 === 0 && z < 650) {
        for (let x = -1500; x <= 1500; x += 60 * scale) {
          const dash = new THREE.Mesh(yellowDashGeom, yellowLineMat);
          dash.position.set(x, 0.12, z);
          scene.add(dash);
        }
      }
    });

    // Vertical road center lines (every other road)
    vRoadPositions.forEach((x, idx) => {
      if (idx % 2 === 0) {
        for (let z = -1500; z <= 600; z += 60 * scale) {
          const dash = new THREE.Mesh(yellowDashGeomV, yellowLineMat);
          dash.position.set(x, 0.12, z);
          scene.add(dash);
        }
      }
    });

    // ZEBRA CROSSWALKS at intersections
    const crosswalkStripeGeom = new THREE.BoxGeometry(3 * scale, 0.12, 1.2 * scale);
    const crosswalkStripeGeomV = new THREE.BoxGeometry(1.2 * scale, 0.12, 3 * scale);

    // Add crosswalks at every 3rd intersection for performance
    let intersectionCount = 0;
    vRoadPositions.forEach(x => {
      hRoadPositions.forEach(z => {
        if (z < 650 && intersectionCount % 3 === 0) {
          // North crosswalk (horizontal stripes)
          for (let i = -4; i <= 4; i++) {
            const stripe = new THREE.Mesh(crosswalkStripeGeom, whiteLineMat);
            stripe.position.set(x + i * 2 * scale, 0.12, z + roadWidth/2 + 2 * scale);
            scene.add(stripe);
          }

          // South crosswalk (horizontal stripes)
          for (let i = -4; i <= 4; i++) {
            const stripe = new THREE.Mesh(crosswalkStripeGeom, whiteLineMat);
            stripe.position.set(x + i * 2 * scale, 0.12, z - roadWidth/2 - 2 * scale);
            scene.add(stripe);
          }

          // East crosswalk (vertical stripes)
          for (let i = -4; i <= 4; i++) {
            const stripe = new THREE.Mesh(crosswalkStripeGeomV, whiteLineMat);
            stripe.position.set(x + roadWidth/2 + 2 * scale, 0.12, z + i * 2 * scale);
            scene.add(stripe);
          }

          // West crosswalk (vertical stripes)
          for (let i = -4; i <= 4; i++) {
            const stripe = new THREE.Mesh(crosswalkStripeGeomV, whiteLineMat);
            stripe.position.set(x - roadWidth/2 - 2 * scale, 0.12, z + i * 2 * scale);
            scene.add(stripe);
          }
        }
        intersectionCount++;
      });
    });

    // Add white edge lines on roads (road edges)
    const edgeLineGeom = new THREE.BoxGeometry(roadLength, 0.12, 0.3 * scale);
    const edgeLineGeomV = new THREE.BoxGeometry(0.3 * scale, 0.12, roadLength);

    hRoadPositions.forEach((z, idx) => {
      if (idx % 3 === 0 && z < 650) {
        const leftEdge = new THREE.Mesh(edgeLineGeom, whiteLineMat);
        leftEdge.position.set(0, 0.12, z - roadWidth/2 + 1 * scale);
        scene.add(leftEdge);
        const rightEdge = new THREE.Mesh(edgeLineGeom, whiteLineMat);
        rightEdge.position.set(0, 0.12, z + roadWidth/2 - 1 * scale);
        scene.add(rightEdge);
      }
    });

    vRoadPositions.forEach((x, idx) => {
      if (idx % 3 === 0) {
        const leftEdge = new THREE.Mesh(edgeLineGeomV, whiteLineMat);
        leftEdge.position.set(x - roadWidth/2 + 1 * scale, 0.12, 0);
        scene.add(leftEdge);
        const rightEdge = new THREE.Mesh(edgeLineGeomV, whiteLineMat);
        rightEdge.position.set(x + roadWidth/2 - 1 * scale, 0.12, 0);
        scene.add(rightEdge);
      }
    });
  }, []);

  // Create buildings - REALISTIC Miami Architecture with Art Deco Details
  const createBuildings = useCallback((scene) => {
    if (!THREE) return;

    const scale = 2;
    const citySize = 12; // Doubled downtown area

    // 4K QUALITY building materials - PBR with proper roughness/metalness
    const buildingColors = [
      new THREE.MeshStandardMaterial({ color: 0xe8e4e0, roughness: 0.85, metalness: 0.05 }), // Warm concrete
      new THREE.MeshStandardMaterial({ color: 0xd4d0cc, roughness: 0.8, metalness: 0.05 }), // Cool concrete
      new THREE.MeshStandardMaterial({ color: 0xf5f0eb, roughness: 0.75, metalness: 0.05 }), // Light stucco
      new THREE.MeshStandardMaterial({ color: 0xccc8c4, roughness: 0.9, metalness: 0.1 }), // Gray stone
      new THREE.MeshStandardMaterial({ color: 0xe0dcd8, roughness: 0.8, metalness: 0.05 }), // Off-white
      new THREE.MeshStandardMaterial({ color: 0xd8d4d0, roughness: 0.85, metalness: 0.05 }), // Beige
    ];

    // Window/glass materials - realistic glass with reflections
    const windowDarkMat = new THREE.MeshPhysicalMaterial({
      color: 0x2a3a4a,
      roughness: 0.05,
      metalness: 0.9,
      reflectivity: 0.9,
    });
    const windowReflectMat = new THREE.MeshPhysicalMaterial({
      color: 0x4a6a8a,
      roughness: 0.0,
      metalness: 0.95,
      reflectivity: 1.0,
    });

    // Accent/trim materials
    const trimMat = new THREE.MeshStandardMaterial({ color: 0x888888, roughness: 0.6, metalness: 0.3 });
    const ledgeMat = new THREE.MeshStandardMaterial({ color: 0x999999, roughness: 0.5, metalness: 0.2 });
    const roofMat = new THREE.MeshStandardMaterial({ color: 0x555555, roughness: 0.9, metalness: 0.1 });

    // Glowing window materials for night effect - with emissive
    const windowGlowMaterials = [
      new THREE.MeshStandardMaterial({ color: 0xffee88, emissive: 0xffee88, emissiveIntensity: 0.8 }),
      new THREE.MeshStandardMaterial({ color: 0xffffff, emissive: 0xffffee, emissiveIntensity: 0.6 }),
      new THREE.MeshStandardMaterial({ color: 0xffcc66, emissive: 0xffcc66, emissiveIntensity: 0.7 }),
    ];

    // Neon accent materials - bright emissive
    const neonMaterials = [
      new THREE.MeshStandardMaterial({ color: 0xff00ff, emissive: 0xff00ff, emissiveIntensity: 2.0 }),
      new THREE.MeshStandardMaterial({ color: 0x00ffff, emissive: 0x00ffff, emissiveIntensity: 2.0 }),
      new THREE.MeshStandardMaterial({ color: 0xff1493, emissive: 0xff1493, emissiveIntensity: 2.0 }),
    ];

    // Road positions for collision checking
    const roadPositions = [];
    for (let i = -200 * scale * citySize; i <= 200 * scale * citySize; i += 80 * scale) {
      roadPositions.push(i);
    }

    const roadClearance = 55;

    // Tampa Bay - downtown bay area
    // Bay is an ellipse centered at (0, -200), extending from x=-400 to x=400, z=-500 to z=100
    const isInBay = (x, z) => {
      const bayX = 0, bayZ = -200;
      const bayRadiusX = 450, bayRadiusZ = 350;
      const dx = (x - bayX) / bayRadiusX;
      const dz = (z - bayZ) / bayRadiusZ;
      return (dx * dx + dz * dz) < 1;
    };

    // Channel connecting bay to ocean
    const isInChannel = (x, z) => {
      const channelWidth = 250; // Wider for building clearance
      return Math.abs(x) < channelWidth / 2 && z > 100 && z < 720;
    };

    // Wide bridge area (z=140 to z=420, x=-250 to x=250)
    const isOnWideBridge = (x, z) => {
      return Math.abs(x) < 280 && z > 120 && z < 440;
    };

    // Building grid - dense Miami feel with bay, channel, and bridge cutout
    const buildingPositions = [];
    for (let x = -2000; x <= 2000; x += 70) {
      for (let z = -2000; z <= 2000; z += 70) {
        const nearHorizontalRoad = roadPositions.some(roadZ => Math.abs(z - roadZ) < roadClearance);
        const nearVerticalRoad = roadPositions.some(roadX => Math.abs(x - roadX) < roadClearance);
        const inOcean = z > 650;
        const inBay = isInBay(x, z);
        const inChannel = isInChannel(x, z);
        const onBridge = isOnWideBridge(x, z);

        if (!nearHorizontalRoad && !nearVerticalRoad && !inOcean && !inBay && !inChannel && !onBridge) {
          buildingPositions.push({ x, z });
        }
      }
    }

    // Geometry cache for performance
    const boxGeomCache = {};
    const getBoxGeom = (w, h, d) => {
      const key = `${Math.round(w)}_${Math.round(h)}_${Math.round(d)}`;
      if (!boxGeomCache[key]) {
        boxGeomCache[key] = new THREE.BoxGeometry(w, h, d);
      }
      return boxGeomCache[key];
    };

    buildingPositions.forEach(pos => {
      const distFromCenter = Math.sqrt(pos.x * pos.x + pos.z * pos.z);

      // Condensed downtown - skyscrapers only in tight core area
      const isCoreDowntown = distFromCenter < 300; // Tight downtown core
      const isDowntown = distFromCenter < 600; // Extended downtown
      const isSuburban = distFromCenter > 1200; // Far from center

      // Skyscraper probability based on distance from center
      let skyscraperChance;
      if (isCoreDowntown) {
        skyscraperChance = 0.95; // Almost all skyscrapers in core
      } else if (isDowntown) {
        skyscraperChance = 0.5 - (distFromCenter - 300) / 600; // Gradual decrease
      } else if (isSuburban) {
        skyscraperChance = 0.02; // Very few tall buildings in suburbs
      } else {
        skyscraperChance = 0.15; // Some medium buildings in between
      }

      const isSkyscraper = Math.random() < skyscraperChance;
      const buildingType = Math.random();

      // Height multiplier - tallest in core, shorter as you go out
      const heightMultiplier = isCoreDowntown ? 1.5 : Math.max(0.2, 1.2 - distFromCenter / 800);

      // More varied building footprints like a real city
      let width, depth, baseHeight;
      const footprintType = Math.random();

      if (isSkyscraper) {
        // Skyscrapers: variety of sizes
        if (footprintType < 0.2) {
          // Slim tower
          width = (8 + Math.random() * 4) * scale;
          depth = (8 + Math.random() * 4) * scale;
          baseHeight = 120 + Math.random() * 140;
        } else if (footprintType < 0.5) {
          // Medium office tower
          width = (14 + Math.random() * 8) * scale;
          depth = (14 + Math.random() * 8) * scale;
          baseHeight = 80 + Math.random() * 120;
        } else if (footprintType < 0.8) {
          // Large corporate building
          width = (20 + Math.random() * 15) * scale;
          depth = (18 + Math.random() * 12) * scale;
          baseHeight = 60 + Math.random() * 100;
        } else {
          // Massive complex
          width = (30 + Math.random() * 20) * scale;
          depth = (25 + Math.random() * 15) * scale;
          baseHeight = 50 + Math.random() * 80;
        }
      } else {
        // Regular buildings: even more variety
        if (footprintType < 0.15) {
          // Tiny shop/house
          width = (5 + Math.random() * 3) * scale;
          depth = (5 + Math.random() * 3) * scale;
          baseHeight = 8 + Math.random() * 12;
        } else if (footprintType < 0.35) {
          // Small building
          width = (7 + Math.random() * 5) * scale;
          depth = (6 + Math.random() * 5) * scale;
          baseHeight = 12 + Math.random() * 18;
        } else if (footprintType < 0.6) {
          // Medium building
          width = (10 + Math.random() * 8) * scale;
          depth = (10 + Math.random() * 8) * scale;
          baseHeight = 15 + Math.random() * 25;
        } else if (footprintType < 0.85) {
          // Large building
          width = (15 + Math.random() * 12) * scale;
          depth = (12 + Math.random() * 10) * scale;
          baseHeight = 20 + Math.random() * 35;
        } else {
          // Wide/long building (warehouses, malls)
          width = (25 + Math.random() * 20) * scale;
          depth = (15 + Math.random() * 15) * scale;
          baseHeight = 10 + Math.random() * 15;
        }
      }

      const height = baseHeight * scale * heightMultiplier;

      const buildingMaterial = buildingColors[Math.floor(Math.random() * buildingColors.length)];

      // ========== BUILDING BASE (Ground Floor) ==========
      const baseFloorHeight = 8 * scale;
      const baseGeom = getBoxGeom(width + 2, baseFloorHeight, depth + 2);
      const base = new THREE.Mesh(baseGeom, buildingMaterial);
      base.position.set(pos.x, baseFloorHeight / 2, pos.z);
      scene.add(base);

      // Ground floor windows/storefronts (darker glass band)
      const storefrontGeom = getBoxGeom(width - 4, 5 * scale, 0.5);
      const storefront = new THREE.Mesh(storefrontGeom, windowDarkMat);
      storefront.position.set(pos.x, 4 * scale, pos.z + depth/2 + 1.3);
      scene.add(storefront);

      // ========== MAIN BUILDING BODY ==========
      const mainHeight = height - baseFloorHeight;
      const mainGeom = getBoxGeom(width, mainHeight, depth);
      const mainBuilding = new THREE.Mesh(mainGeom, buildingMaterial);
      mainBuilding.position.set(pos.x, baseFloorHeight + mainHeight / 2, pos.z);
      scene.add(mainBuilding);

      // ========== WINDOW GRID (Realistic pattern) ==========
      const floorHeight = 12; // Height per floor
      const numFloors = Math.floor(mainHeight / floorHeight);
      const windowWidth = 4;
      const windowHeight = 6;
      const windowSpacing = 8;
      const numWindowsPerRow = Math.floor((width - 8) / windowSpacing);

      // Create window grid on front face
      if (numFloors > 1 && numWindowsPerRow > 0) {
        const windowMat = Math.random() > 0.5 ? windowDarkMat : windowReflectMat;

        for (let floor = 0; floor < Math.min(numFloors, 12); floor++) { // Limit floors for performance
          const floorY = baseFloorHeight + floor * floorHeight + floorHeight/2 + 2;

          for (let w = 0; w < numWindowsPerRow; w++) {
            const windowX = pos.x - (numWindowsPerRow - 1) * windowSpacing / 2 + w * windowSpacing;

            // Window pane
            const windowGeom = getBoxGeom(windowWidth, windowHeight, 0.5);
            const window = new THREE.Mesh(windowGeom, windowMat);
            window.position.set(windowX, floorY, pos.z + depth/2 + 0.3);
            scene.add(window);

            // Add lit windows randomly at night
            if (Math.random() > 0.6) {
              const glowMat = windowGlowMaterials[Math.floor(Math.random() * windowGlowMaterials.length)];
              const glowGeom = getBoxGeom(windowWidth - 1, windowHeight - 1, 0.3);
              const glow = new THREE.Mesh(glowGeom, glowMat);
              glow.position.set(windowX, floorY, pos.z + depth/2 + 0.5);
              scene.add(glow);
            }
          }
        }

        // Side face windows (simplified - just a few)
        for (let floor = 0; floor < Math.min(numFloors, 8); floor += 2) {
          const floorY = baseFloorHeight + floor * floorHeight + floorHeight/2 + 2;
          const sideWindow = new THREE.Mesh(getBoxGeom(0.5, windowHeight, windowWidth), windowMat);
          sideWindow.position.set(pos.x + width/2 + 0.3, floorY, pos.z);
          scene.add(sideWindow);
        }
      }

      // ========== HORIZONTAL LEDGES (Every few floors) ==========
      const numLedges = Math.floor(numFloors / 4);
      for (let i = 1; i <= numLedges; i++) {
        const ledgeY = baseFloorHeight + i * 4 * floorHeight;
        if (ledgeY < height - 10) {
          const ledgeGeom = getBoxGeom(width + 3, 1.5, depth + 3);
          const ledge = new THREE.Mesh(ledgeGeom, ledgeMat);
          ledge.position.set(pos.x, ledgeY, pos.z);
          scene.add(ledge);
        }
      }

      // ========== ROOFTOP DETAILS ==========
      // Cornice/top ledge
      const corniceGeom = getBoxGeom(width + 4, 2, depth + 4);
      const cornice = new THREE.Mesh(corniceGeom, ledgeMat);
      cornice.position.set(pos.x, height - 1, pos.z);
      scene.add(cornice);

      // Rooftop structures (AC units, elevator housing)
      if (isSkyscraper && Math.random() > 0.3) {
        // Elevator penthouse
        const penthouseWidth = width * 0.4;
        const penthouseDepth = depth * 0.4;
        const penthouseHeight = 15 + Math.random() * 10;
        const penthouseGeom = getBoxGeom(penthouseWidth, penthouseHeight, penthouseDepth);
        const penthouse = new THREE.Mesh(penthouseGeom, buildingMaterial);
        penthouse.position.set(pos.x, height + penthouseHeight/2, pos.z);
        scene.add(penthouse);

        // Spire/antenna on very tall buildings
        if (height > 200 && Math.random() > 0.5) {
          const spireGeom = getBoxGeom(2, 30, 2);
          const spire = new THREE.Mesh(spireGeom, trimMat);
          spire.position.set(pos.x, height + penthouseHeight + 15, pos.z);
          scene.add(spire);

          // Blinking light on top
          const lightGeom = getBoxGeom(3, 3, 3);
          const lightMat = neonMaterials[Math.floor(Math.random() * neonMaterials.length)];
          const light = new THREE.Mesh(lightGeom, lightMat);
          light.position.set(pos.x, height + penthouseHeight + 32, pos.z);
          scene.add(light);
        }
      }

      // AC units on shorter buildings
      if (!isSkyscraper && Math.random() > 0.5) {
        const acGeom = getBoxGeom(6, 4, 6);
        const ac = new THREE.Mesh(acGeom, roofMat);
        ac.position.set(pos.x + (Math.random() - 0.5) * width * 0.4, height + 2, pos.z + (Math.random() - 0.5) * depth * 0.4);
        scene.add(ac);
      }

      // ========== NEON SIGNAGE (Miami Vice style) ==========
      if (distFromCenter < 800 && Math.random() > 0.6) {
        const neonMat = neonMaterials[Math.floor(Math.random() * neonMaterials.length)];

        // Neon strip along storefront
        const neonStripGeom = getBoxGeom(width * 0.8, 1.5, 1);
        const neonStrip = new THREE.Mesh(neonStripGeom, neonMat);
        neonStrip.position.set(pos.x, baseFloorHeight - 1, pos.z + depth/2 + 1.5);
        scene.add(neonStrip);

        // Vertical neon accent on some buildings
        if (Math.random() > 0.7 && isSkyscraper) {
          const vertNeonGeom = getBoxGeom(1.5, height * 0.6, 1);
          const vertNeon = new THREE.Mesh(vertNeonGeom, neonMat);
          vertNeon.position.set(pos.x + width/2 + 1, height * 0.5, pos.z);
          scene.add(vertNeon);
        }
      }

      // ========== BUILDING CORNER DETAILS ==========
      // Vertical corner trim strips
      if (buildingType > 0.6) {
        const cornerTrimGeom = getBoxGeom(2, mainHeight, 2);
        [[-1, -1], [-1, 1], [1, -1], [1, 1]].forEach(([xMult, zMult]) => {
          const cornerTrim = new THREE.Mesh(cornerTrimGeom, trimMat);
          cornerTrim.position.set(
            pos.x + xMult * (width/2 + 0.5),
            baseFloorHeight + mainHeight/2,
            pos.z + zMult * (depth/2 + 0.5)
          );
          scene.add(cornerTrim);
        });
      }
    });
  }, []);

  // Create street lights - OPTIMIZED (reduced count, simpler geometry)
  const createStreetLights = useCallback((scene) => {
    if (!THREE) return;

    const scale = 2;
    const citySize = 12; // Doubled downtown area

    const poleMaterial = new THREE.MeshStandardMaterial({ color: 0x1a1a1a, roughness: 0.7, metalness: 0.8 });
    const lampColors = [0xff00ff, 0x00ffff, 0xff1493];

    // Shared geometries - slightly higher poly for 4K
    const poleGeom = new THREE.CylinderGeometry(0.15 * scale, 0.25 * scale, 12 * scale, 8);
    const armGeom = new THREE.BoxGeometry(4 * scale, 0.3 * scale, 0.3 * scale);
    const lampGeom = new THREE.SphereGeometry(0.8 * scale, 8, 8);

    // Lamp materials with strong emissive glow
    const lampMaterials = lampColors.map(c => new THREE.MeshStandardMaterial({
      color: c,
      emissive: c,
      emissiveIntensity: 2.5,
    }));

    const roadZPositions = [];
    for (let i = -200 * scale * citySize; i <= 200 * scale * citySize; i += 80 * scale) {
      roadZPositions.push(i);
    }

    // Helper to check if position is in channel
    const isInChannelArea = (x, z) => {
      const channelWidth = 220;
      return Math.abs(x) < channelWidth / 2 && z > 100 && z < 720;
    };

    // REDUCED density - every 200 units instead of 100
    let lampIndex = 0;
    for (let x = -1800; x <= 1800; x += 200) {
      for (let z of roadZPositions) {
        // Skip if in ocean area OR in channel area
        if (z < 700 && !isInChannelArea(x, z)) {
          const pole = new THREE.Mesh(poleGeom, poleMaterial);
          pole.position.set(x, 6 * scale, z + 15 * scale);
          scene.add(pole);

          const arm = new THREE.Mesh(armGeom, poleMaterial);
          arm.position.set(x + 2 * scale, 11.5 * scale, z + 15 * scale);
          scene.add(arm);

          const lamp = new THREE.Mesh(lampGeom, lampMaterials[lampIndex % lampMaterials.length]);
          lamp.position.set(x + 4 * scale, 11 * scale, z + 15 * scale);
          scene.add(lamp);

          lampIndex++;
        }
      }
    }
  }, []);

  // Create traffic lights at intersections - OPTIMIZED
  const createTrafficLights = useCallback((scene) => {
    if (!THREE) return;

    const scale = 2;
    const citySize = 12; // Doubled downtown area

    const poleMaterial = new THREE.MeshStandardMaterial({ color: 0x222222, roughness: 0.7, metalness: 0.8 });
    const boxMat = new THREE.MeshStandardMaterial({ color: 0x1a1a1a, roughness: 0.8, metalness: 0.5 });

    // Shared geometries - higher poly for 4K
    const poleGeom = new THREE.CylinderGeometry(0.25 * scale, 0.35 * scale, 8 * scale, 8);
    const boxGeom = new THREE.BoxGeometry(1 * scale, 3 * scale, 0.8 * scale);

    const roadPositions = [];
    for (let i = -200 * scale * citySize; i <= 200 * scale * citySize; i += 80 * scale) {
      roadPositions.push(i);
    }

    // Helper to check if position is in channel
    const isInChannelArea = (x, z) => {
      const channelWidth = 220;
      return Math.abs(x) < channelWidth / 2 && z > 100 && z < 720;
    };

    // Only add traffic lights at every 4th intersection for performance
    let count = 0;
    for (let x of roadPositions) {
      for (let z of roadPositions) {
        // Skip if in ocean area OR in channel area
        if (z < 700 && !isInChannelArea(x, z) && count % 4 === 0) {
          const pole = new THREE.Mesh(poleGeom, poleMaterial);
          pole.position.set(x + 12 * scale, 4 * scale, z + 12 * scale);
          scene.add(pole);

          const box = new THREE.Mesh(boxGeom, boxMat);
          box.position.set(x + 6.5 * scale, 7.5 * scale, z + 12 * scale);
          scene.add(box);
        }
        count++;
      }
    }
  }, []);

  // Create other vehicles - VICE CITY 80s STYLE, 10X LARGER AREA
  const trafficCarsRef = useRef([]);

  const createTrafficCars = useCallback((scene) => {
    if (!THREE) return;

    const scale = 2;
    const citySize = 12; // Doubled downtown area

    // Vice City 80s car colors - white, red, teal, pink, gold
    const carColors = [0xffffff, 0xf5f5f5, 0xff0044, 0x00cccc, 0xff69b4, 0xffd700, 0x8b0000, 0x00ced1, 0xff1493, 0xc0c0c0];
    const cars = [];

    const roadPositions = [];
    for (let i = -200 * scale * citySize; i <= 200 * scale * citySize; i += 80 * scale) {
      roadPositions.push(i);
    }

    // Parked cars - spread across larger area
    const parkedPositions = [];
    roadPositions.forEach(roadZ => {
      if (roadZ < 650) { // Skip ocean
        for (let x = -1800; x <= 1800; x += 80) {
          if (Math.random() > 0.7) {
            parkedPositions.push({ x, z: roadZ - 15 * scale, rot: 0 });
          }
        }
      }
    });

    parkedPositions.forEach(pos => {
      const car = createSimpleCar(carColors[Math.floor(Math.random() * carColors.length)], scale);
      if (car) {
        car.position.set(pos.x, 0, pos.z);
        car.rotation.y = pos.rot;
        scene.add(car);
      }
    });

    // Moving traffic - larger routes
    const trafficRoutes = [];
    const routeLength = 1000 * citySize;

    // Only add traffic on some roads for performance
    roadPositions.forEach((roadZ, i) => {
      if (i % 2 === 0 && roadZ < 650) {
        trafficRoutes.push({ startX: -routeLength/2, startZ: roadZ - 6 * scale, endX: routeLength/2, endZ: roadZ - 6 * scale, speed: 0.5 + Math.random() * 0.4 });
        trafficRoutes.push({ startX: routeLength/2, startZ: roadZ + 6 * scale, endX: -routeLength/2, endZ: roadZ + 6 * scale, speed: 0.45 + Math.random() * 0.35 });
      }
    });

    roadPositions.forEach((roadX, i) => {
      if (i % 2 === 0) {
        trafficRoutes.push({ startX: roadX - 6 * scale, startZ: -routeLength/2, endX: roadX - 6 * scale, endZ: 650, speed: 0.5 + Math.random() * 0.4 });
        trafficRoutes.push({ startX: roadX + 6 * scale, startZ: 650, endX: roadX + 6 * scale, endZ: -routeLength/2, speed: 0.45 + Math.random() * 0.35 });
      }
    });

    trafficRoutes.forEach((route) => {
      const car = createSimpleCar(carColors[Math.floor(Math.random() * carColors.length)], scale);
      if (car) {
        const progress = Math.random();
        car.position.set(
          route.startX + (route.endX - route.startX) * progress,
          0,
          route.startZ + (route.endZ - route.startZ) * progress
        );
        car.rotation.y = Math.atan2(route.endX - route.startX, route.endZ - route.startZ);
        scene.add(car);
        cars.push({ mesh: car, route, progress });
      }
    });

    trafficCarsRef.current = cars;
  }, []);

  // Shared car geometry/materials for traffic (created once)
  const trafficCarAssetsRef = useRef(null);

  // Helper to create a simple car - OPTIMIZED with shared geometry
  const createSimpleCar = (color, scale = 2) => {
    if (!THREE) return null;

    // Initialize shared assets once
    if (!trafficCarAssetsRef.current) {
      trafficCarAssetsRef.current = {
        bodyGeom: new THREE.BoxGeometry(1.8, 0.5, 4),
        cabinGeom: new THREE.BoxGeometry(1.6, 0.45, 2),
        wheelGeom: new THREE.CylinderGeometry(0.3, 0.3, 0.2, 6), // Low poly wheels
        cabinMat: new THREE.MeshBasicMaterial({ color: 0x111122 }),
        wheelMat: new THREE.MeshBasicMaterial({ color: 0x111111 }),
      };
    }

    const assets = trafficCarAssetsRef.current;
    const car = new THREE.Group();

    // Body - use BasicMaterial (no lighting calculations)
    const bodyMat = new THREE.MeshBasicMaterial({ color });
    const body = new THREE.Mesh(assets.bodyGeom, bodyMat);
    body.position.y = 0.45 * scale;
    body.scale.set(scale, scale, scale);
    car.add(body);

    // Cabin
    const cabin = new THREE.Mesh(assets.cabinGeom, assets.cabinMat);
    cabin.position.set(0, 0.92 * scale, -0.2 * scale);
    cabin.scale.set(scale, scale, scale);
    car.add(cabin);

    // Only 2 wheels visible (front and back, one side) - halves wheel count
    [[0.85, 1.2], [0.85, -1.2]].forEach(([x, z]) => {
      const wheel = new THREE.Mesh(assets.wheelGeom, assets.wheelMat);
      wheel.position.set(x * scale, 0.3 * scale, z * scale);
      wheel.rotation.z = Math.PI / 2;
      wheel.scale.set(scale, scale, scale);
      car.add(wheel);
    });

    return car;
  };

  // Shared palm tree geometry and materials (created once)
  const palmTreeGeomRef = useRef(null);
  const palmTreeMatsRef = useRef(null);

  // Create palm tree helper - OPTIMIZED with shared geometry
  const createPalmTree = (scene, x, z, scale) => {
    if (!THREE) return;

    // Initialize shared resources once - DOUBLED HEIGHT
    if (!palmTreeGeomRef.current) {
      palmTreeGeomRef.current = {
        trunk: new THREE.CylinderGeometry(0.4, 0.6, 30, 4), // Low poly trunk - doubled from 15 to 30
        frond: new THREE.ConeGeometry(2, 18, 3), // Larger fronds - scaled up
      };
      palmTreeMatsRef.current = {
        trunk: new THREE.MeshBasicMaterial({ color: 0x8b7355 }),
        leaf: new THREE.MeshBasicMaterial({ color: 0x228b22 }),
      };
    }

    const geoms = palmTreeGeomRef.current;
    const mats = palmTreeMatsRef.current;

    // Trunk - doubled height
    const trunk = new THREE.Mesh(geoms.trunk, mats.trunk);
    trunk.position.set(x, 15 * scale, z); // Doubled from 7.5 to 15
    trunk.scale.set(scale, scale, scale);
    scene.add(trunk);

    // Simplified fronds - only 4 instead of 8, positioned at top of taller trunk
    const numFronds = 4;
    for (let i = 0; i < numFronds; i++) {
      const frond = new THREE.Mesh(geoms.frond, mats.leaf);
      const angle = (i / numFronds) * Math.PI * 2;
      frond.position.set(
        x + Math.cos(angle) * 4 * scale,
        30 * scale, // Doubled from 15 to 30
        z + Math.sin(angle) * 4 * scale
      );
      frond.rotation.x = Math.PI / 2 + 0.5;
      frond.rotation.z = angle;
      frond.scale.set(scale, scale, scale);
      scene.add(frond);
    }
  };

  // Create the city environment - VICE CITY STYLE, 10X LARGER AREA
  const createCity = useCallback((scene) => {
    if (!THREE) return;

    const scale = 2;
    const citySize = 12; // Doubled downtown area
    const groundSize = 2500 * citySize;

    // Main ground - use BasicMaterial (no lighting calculations)
    const groundGeometry = new THREE.PlaneGeometry(groundSize, groundSize);
    const groundMaterial = new THREE.MeshBasicMaterial({ color: 0x2d4a2d });
    const ground = new THREE.Mesh(groundGeometry, groundMaterial);
    ground.rotation.x = -Math.PI / 2;
    scene.add(ground);

    // OCEAN - simplified
    const oceanGeometry = new THREE.PlaneGeometry(groundSize, 800);
    const oceanMaterial = new THREE.MeshBasicMaterial({ color: 0x1a3a5c });
    const ocean = new THREE.Mesh(oceanGeometry, oceanMaterial);
    ocean.rotation.x = -Math.PI / 2;
    ocean.position.set(0, 0.1, 1100);
    scene.add(ocean);

    // Shallow water
    const shallowGeometry = new THREE.PlaneGeometry(groundSize, 200);
    const shallowMaterial = new THREE.MeshBasicMaterial({ color: 0x2d5a6a });
    const shallowWater = new THREE.Mesh(shallowGeometry, shallowMaterial);
    shallowWater.rotation.x = -Math.PI / 2;
    shallowWater.position.set(0, 0.15, 800);
    scene.add(shallowWater);

    // BEACH
    const beachGeometry = new THREE.PlaneGeometry(groundSize, 150);
    const beachMaterial = new THREE.MeshBasicMaterial({ color: 0xd4b896 });
    const beach = new THREE.Mesh(beachGeometry, beachMaterial);
    beach.rotation.x = -Math.PI / 2;
    beach.position.set(0, 0.2, 700);
    scene.add(beach);

    // Boardwalk
    const boardwalkGeometry = new THREE.BoxGeometry(groundSize, 0.3, 15);
    const boardwalkMaterial = new THREE.MeshBasicMaterial({ color: 0xb89468 });
    const boardwalk = new THREE.Mesh(boardwalkGeometry, boardwalkMaterial);
    boardwalk.position.set(0, 0.15, 620);
    scene.add(boardwalk);

    // Neon strip along boardwalk
    const boardwalkNeonGeom = new THREE.BoxGeometry(groundSize, 0.3, 0.5);
    const boardwalkNeonMat = new THREE.MeshBasicMaterial({ color: 0xff00ff });
    const boardwalkNeon = new THREE.Mesh(boardwalkNeonGeom, boardwalkNeonMat);
    boardwalkNeon.position.set(0, 0.35, 612);
    scene.add(boardwalkNeon);

    // ========== TAMPA BAY - Downtown Bay Area ==========
    const bayMaterial = new THREE.MeshBasicMaterial({ color: 0x1a4a6a }); // Deep bay water
    const bayShallowMaterial = new THREE.MeshBasicMaterial({ color: 0x2a6a8a }); // Shallow edges

    // Create elliptical bay using circle geometry scaled
    const bayGeometry = new THREE.CircleGeometry(450, 64);
    bayGeometry.scale(1, 0.78, 1); // Make it elliptical (350/450 ratio)
    const bay = new THREE.Mesh(bayGeometry, bayMaterial);
    bay.rotation.x = -Math.PI / 2;
    bay.position.set(0, 0.3, -200);
    scene.add(bay);

    // Shallow water ring around bay
    const bayShallowGeometry = new THREE.RingGeometry(420, 480, 64);
    bayShallowGeometry.scale(1, 0.78, 1);
    const bayShallow = new THREE.Mesh(bayShallowGeometry, bayShallowMaterial);
    bayShallow.rotation.x = -Math.PI / 2;
    bayShallow.position.set(0, 0.25, -200);
    scene.add(bayShallow);

    // ========== CHANNEL connecting bay to ocean ==========
    // Channel runs from bay (z=150) to ocean (z=700)
    const channelLength = 570; // From bay edge to ocean
    const channelWidth = 180;

    // Main channel water
    const channelGeometry = new THREE.PlaneGeometry(channelWidth, channelLength);
    const channel = new THREE.Mesh(channelGeometry, bayMaterial);
    channel.rotation.x = -Math.PI / 2;
    channel.position.set(0, 0.3, 150 + channelLength / 2); // Positioned between bay and ocean
    scene.add(channel);

    // Channel shallow edges (left and right banks)
    const channelBankGeom = new THREE.PlaneGeometry(40, channelLength);
    const channelBankLeft = new THREE.Mesh(channelBankGeom, bayShallowMaterial);
    channelBankLeft.rotation.x = -Math.PI / 2;
    channelBankLeft.position.set(-channelWidth / 2 - 20, 0.25, 150 + channelLength / 2);
    scene.add(channelBankLeft);

    const channelBankRight = new THREE.Mesh(channelBankGeom, bayShallowMaterial);
    channelBankRight.rotation.x = -Math.PI / 2;
    channelBankRight.position.set(channelWidth / 2 + 20, 0.25, 150 + channelLength / 2);
    scene.add(channelBankRight);

    // ========== TAMPA BAY BRIDGE (Inspired by Sunshine Skyway) ==========
    const bridgeMaterial = new THREE.MeshStandardMaterial({ color: 0xcccccc, roughness: 0.6, metalness: 0.4 });
    const cableMaterial = new THREE.MeshBasicMaterial({ color: 0xffdd00 }); // Yellow cables like Skyway
    const pillarMaterial = new THREE.MeshStandardMaterial({ color: 0xeeeeee, roughness: 0.5, metalness: 0.3 });

    // Bridge deck - spans across the bay (east-west)
    const bridgeLength = 950;
    const bridgeWidth = 45;
    const bridgeHeight = 8;

    // Main bridge deck
    const bridgeDeckGeom = new THREE.BoxGeometry(bridgeLength, 3, bridgeWidth);
    const bridgeDeck = new THREE.Mesh(bridgeDeckGeom, bridgeMaterial);
    bridgeDeck.position.set(0, bridgeHeight, -200);
    bridgeDeck.castShadow = true;
    bridgeDeck.receiveShadow = true;
    scene.add(bridgeDeck);

    // Road surface on bridge
    const bridgeRoadGeom = new THREE.BoxGeometry(bridgeLength - 10, 0.5, bridgeWidth - 10);
    const bridgeRoadMat = new THREE.MeshStandardMaterial({ color: 0x333333, roughness: 0.9 });
    const bridgeRoad = new THREE.Mesh(bridgeRoadGeom, bridgeRoadMat);
    bridgeRoad.position.set(0, bridgeHeight + 1.8, -200);
    scene.add(bridgeRoad);

    // Center line on bridge
    const bridgeLineGeom = new THREE.BoxGeometry(bridgeLength - 20, 0.6, 1);
    const bridgeLineMat = new THREE.MeshBasicMaterial({ color: 0xffdd44 });
    const bridgeLine = new THREE.Mesh(bridgeLineGeom, bridgeLineMat);
    bridgeLine.position.set(0, bridgeHeight + 2.1, -200);
    scene.add(bridgeLine);

    // Bridge support pillars
    const pillarGeom = new THREE.CylinderGeometry(4, 5, bridgeHeight + 5, 8);
    const pillarPositions = [-400, -200, 0, 200, 400];
    pillarPositions.forEach(x => {
      const pillar = new THREE.Mesh(pillarGeom, pillarMaterial);
      pillar.position.set(x, (bridgeHeight + 5) / 2 - 3, -200);
      pillar.castShadow = true;
      scene.add(pillar);
    });

    // Cable-stayed towers (like Sunshine Skyway)
    const towerHeight = 80;
    const towerGeom = new THREE.BoxGeometry(6, towerHeight, 6);

    // Two main towers
    [-150, 150].forEach(x => {
      const tower = new THREE.Mesh(towerGeom, pillarMaterial);
      tower.position.set(x, bridgeHeight + towerHeight / 2, -200);
      tower.castShadow = true;
      scene.add(tower);

      // Tower top pyramid
      const towerTopGeom = new THREE.ConeGeometry(5, 15, 4);
      const towerTop = new THREE.Mesh(towerTopGeom, pillarMaterial);
      towerTop.position.set(x, bridgeHeight + towerHeight + 7, -200);
      scene.add(towerTop);

      // Cables from tower to bridge deck
      const cableGeom = new THREE.CylinderGeometry(0.3, 0.3, 1, 4);
      for (let i = 0; i < 8; i++) {
        const cableLength = 20 + i * 8;
        const cableX = x + (x > 0 ? 1 : -1) * (15 + i * 18);
        const cableAngle = Math.atan2(towerHeight - 10, Math.abs(cableX - x));

        const cable = new THREE.Mesh(
          new THREE.CylinderGeometry(0.4, 0.4, cableLength, 4),
          cableMaterial
        );
        cable.position.set(
          (x + cableX) / 2,
          bridgeHeight + towerHeight / 2 + 20 - i * 3,
          -200
        );
        cable.rotation.z = (x > 0 ? -1 : 1) * (Math.PI / 2 - cableAngle);
        scene.add(cable);
      }
    });

    // Bridge railings
    const railingGeom = new THREE.BoxGeometry(bridgeLength, 2, 0.5);
    const railingMat = new THREE.MeshBasicMaterial({ color: 0x888888 });
    [-1, 1].forEach(side => {
      const railing = new THREE.Mesh(railingGeom, railingMat);
      railing.position.set(0, bridgeHeight + 3, -200 + side * (bridgeWidth / 2 - 2));
      scene.add(railing);
    });

    // Bridge approach ramps (connect to ground level)
    const bridgeRampLength = 200;
    const bridgeRampGeom = new THREE.BoxGeometry(bridgeRampLength, 3, bridgeWidth);

    // West ramp
    const westRamp = new THREE.Mesh(bridgeRampGeom, bridgeMaterial);
    westRamp.position.set(-bridgeLength / 2 - bridgeRampLength / 2 + 20, bridgeHeight / 2, -200);
    westRamp.rotation.z = Math.atan2(bridgeHeight, bridgeRampLength);
    scene.add(westRamp);

    // East ramp
    const eastRamp = new THREE.Mesh(bridgeRampGeom, bridgeMaterial);
    eastRamp.position.set(bridgeLength / 2 + bridgeRampLength / 2 - 20, bridgeHeight / 2, -200);
    eastRamp.rotation.z = -Math.atan2(bridgeHeight, bridgeRampLength);
    scene.add(eastRamp);

    // ========== WIDE CHANNEL BRIDGE (spans from z=160 to z=400) ==========
    // Single wide bridge covering the entire channel area
    const wideBridgeLength = 350; // Length spanning east-west across channel
    const wideBridgeDepth = 280; // Depth from z=160 to z=440 (north-south)
    const wideBridgeHeight = 12;
    const wideBridgeCenterZ = 280; // Center between 160 and 400

    console.log('Creating wide channel bridge from z=140 to z=420');

    // Bridge material - gray concrete look
    const wideBridgeMaterial = new THREE.MeshStandardMaterial({ color: 0x888888, roughness: 0.8, metalness: 0.2 });

    // Main bridge deck - wide platform
    const wideBridgeDeckGeom = new THREE.BoxGeometry(wideBridgeLength, 6, wideBridgeDepth);
    const wideBridgeDeck = new THREE.Mesh(wideBridgeDeckGeom, wideBridgeMaterial);
    wideBridgeDeck.position.set(0, wideBridgeHeight, wideBridgeCenterZ);
    wideBridgeDeck.castShadow = true;
    wideBridgeDeck.receiveShadow = true;
    scene.add(wideBridgeDeck);

    // Road surface on bridge
    const wideBridgeRoadGeom = new THREE.BoxGeometry(wideBridgeLength - 20, 0.5, wideBridgeDepth - 20);
    const wideBridgeRoad = new THREE.Mesh(wideBridgeRoadGeom, bridgeRoadMat);
    wideBridgeRoad.position.set(0, wideBridgeHeight + 3.2, wideBridgeCenterZ);
    scene.add(wideBridgeRoad);

    // Lane markings (multiple lanes running east-west)
    const laneLineGeom = new THREE.BoxGeometry(wideBridgeLength - 30, 0.6, 1.5);
    [-80, 0, 80].forEach(z => {
      const laneLine = new THREE.Mesh(laneLineGeom, bridgeLineMat);
      laneLine.position.set(0, wideBridgeHeight + 3.5, wideBridgeCenterZ + z);
      scene.add(laneLine);
    });

    // Support pillars - grid pattern
    const widePillarGeom = new THREE.CylinderGeometry(5, 6, wideBridgeHeight + 8, 8);
    [-120, 0, 120].forEach(x => {
      [180, 280, 380].forEach(z => {
        const pillar = new THREE.Mesh(widePillarGeom, pillarMaterial);
        pillar.position.set(x, (wideBridgeHeight + 8) / 2 - 4, z);
        pillar.castShadow = true;
        scene.add(pillar);
      });
    });

    // Railings on all four sides
    const wideRailingMat = new THREE.MeshStandardMaterial({ color: 0xffdd00 });

    // North and south railings (run east-west)
    const nsRailingGeom = new THREE.BoxGeometry(wideBridgeLength, 3, 1);
    [-1, 1].forEach(side => {
      const railing = new THREE.Mesh(nsRailingGeom, wideRailingMat);
      railing.position.set(0, wideBridgeHeight + 4.5, wideBridgeCenterZ + side * (wideBridgeDepth / 2 - 2));
      scene.add(railing);
    });

    // East and west railings (run north-south)
    const ewRailingGeom = new THREE.BoxGeometry(1, 3, wideBridgeDepth);
    [-1, 1].forEach(side => {
      const railing = new THREE.Mesh(ewRailingGeom, wideRailingMat);
      railing.position.set(side * (wideBridgeLength / 2 - 2), wideBridgeHeight + 4.5, wideBridgeCenterZ);
      scene.add(railing);
    });

    // Approach ramps on east and west sides
    const wideRampLength = 150;
    const wideRampGeom = new THREE.BoxGeometry(wideRampLength, 4, wideBridgeDepth);

    // West ramp
    const wideWestRamp = new THREE.Mesh(wideRampGeom, wideBridgeMaterial);
    wideWestRamp.position.set(-wideBridgeLength / 2 - wideRampLength / 2 + 20, wideBridgeHeight / 2, wideBridgeCenterZ);
    wideWestRamp.rotation.z = Math.atan2(wideBridgeHeight, wideRampLength);
    scene.add(wideWestRamp);

    // East ramp
    const wideEastRamp = new THREE.Mesh(wideRampGeom, wideBridgeMaterial);
    wideEastRamp.position.set(wideBridgeLength / 2 + wideRampLength / 2 - 20, wideBridgeHeight / 2, wideBridgeCenterZ);
    wideEastRamp.rotation.z = -Math.atan2(wideBridgeHeight, wideRampLength);
    scene.add(wideEastRamp);

    console.log('Wide channel bridge created');

    createRoads(scene);
    createBuildings(scene);
    createStreetLights(scene);
    createTrafficLights(scene);
    // createTrafficCars(scene); // Disabled for performance

    const roadPositions = [];
    for (let i = -200 * scale * citySize; i <= 200 * scale * citySize; i += 80 * scale) {
      roadPositions.push(i);
    }

    // PALM TREES - REDUCED count for performance
    // Beach palm trees - every 80 units instead of 40
    for (let x = -2400; x <= 2400; x += 80) {
      createPalmTree(scene, x + (Math.random() - 0.5) * 10, 660 + (Math.random() - 0.5) * 20, scale);
    }

    // Second row of beach palms - every 120 units instead of 60
    for (let x = -2400; x <= 2400; x += 120) {
      createPalmTree(scene, x + (Math.random() - 0.5) * 15, 590 + (Math.random() - 0.5) * 15, scale);
    }

    // Bay check function for palm trees
    const isInBayArea = (x, z) => {
      const bayX = 0, bayZ = -200;
      const bayRadiusX = 500, bayRadiusZ = 400; // Slightly larger to keep trees away from edge
      const dx = (x - bayX) / bayRadiusX;
      const dz = (z - bayZ) / bayRadiusZ;
      return (dx * dx + dz * dz) < 1;
    };

    // Channel check for palm trees
    const isInChannelArea = (x, z) => {
      const channelWidth = 240; // Keep trees away from channel
      return Math.abs(x) < channelWidth / 2 && z > 80 && z < 720;
    };

    // Palm trees scattered in city - REDUCED from 400 to 100
    for (let i = 0; i < 100; i++) {
      const x = (Math.random() - 0.5) * 4000;
      const z = (Math.random() - 0.5) * 2800 - 100;

      const nearHorizontalRoad = roadPositions.some(roadZ => Math.abs(z - roadZ) < 25);
      const nearVerticalRoad = roadPositions.some(roadX => Math.abs(x - roadX) < 25);
      const inOcean = z > 600;
      const inBay = isInBayArea(x, z);
      const inChannel = isInChannelArea(x, z);

      if (!nearHorizontalRoad && !nearVerticalRoad && !inOcean && !inBay && !inChannel) {
        createPalmTree(scene, x, z, scale * (0.8 + Math.random() * 0.4));
      }
    }

    // Beach umbrellas - OPTIMIZED with shared geometry and BasicMaterial
    const umbrellaMat = new THREE.MeshBasicMaterial({ color: 0xff1493 });
    const umbrellaMat2 = new THREE.MeshBasicMaterial({ color: 0x00ced1 });
    const poleMat = new THREE.MeshBasicMaterial({ color: 0xffffff });

    // Shared geometries
    const umbrellaGeom = new THREE.ConeGeometry(8 * scale, 4 * scale, 4); // Low poly
    const poleGeom = new THREE.CylinderGeometry(0.2 * scale, 0.2 * scale, 6 * scale, 4);

    // REDUCED count - every 150 units instead of 80
    for (let x = -2000; x <= 2000; x += 150) {
      const mat = Math.random() > 0.5 ? umbrellaMat : umbrellaMat2;
      const umbrella = new THREE.Mesh(umbrellaGeom, mat);
      umbrella.position.set(x + (Math.random() - 0.5) * 30, 6 * scale, 720 + (Math.random() - 0.5) * 40);
      umbrella.rotation.x = Math.PI;
      scene.add(umbrella);

      const pole = new THREE.Mesh(poleGeom, poleMat);
      pole.position.set(umbrella.position.x, 3 * scale, umbrella.position.z);
      scene.add(pole);
    }

    // Neon signs - REDUCED and simplified
    const neonSignMats = [
      new THREE.MeshBasicMaterial({ color: 0xff00ff }),
      new THREE.MeshBasicMaterial({ color: 0x00ffff }),
    ];
    const postMat = new THREE.MeshBasicMaterial({ color: 0x333333 });
    const postGeom = new THREE.BoxGeometry(0.5 * scale, 20 * scale, 0.5 * scale);
    const signGeom = new THREE.BoxGeometry(15 * scale, 3 * scale, 0.5 * scale);

    // Every 400 units instead of 200
    for (let x = -1600; x <= 1600; x += 400) {
      const signMat = neonSignMats[Math.floor(Math.random() * neonSignMats.length)];

      const post = new THREE.Mesh(postGeom, postMat);
      post.position.set(x, 10 * scale, 580);
      scene.add(post);

      const sign = new THREE.Mesh(signGeom, signMat);
      sign.position.set(x, 18 * scale, 580);
      scene.add(sign);
    }

    // ========== LAUNCH RAMPS - OPTIMIZED ==========
    const ramps = [];
    const rampMaterial = new THREE.MeshBasicMaterial({ color: 0x222222 });

    // Shared neon materials
    const rampNeonMats = [
      new THREE.MeshBasicMaterial({ color: 0xff00ff }),
      new THREE.MeshBasicMaterial({ color: 0x00ffff }),
    ];

    const rampPositions = [
      { x: -2200, z: 0, rotY: Math.PI / 2 },
      { x: 2200, z: 0, rotY: -Math.PI / 2 },
      { x: 0, z: -2200, rotY: 0 },
      { x: 0, z: 550, rotY: Math.PI },
    ];

    // Shared ramp geometries
    const rampLength = 80 * scale;
    const rampWidth = 30 * scale;
    const rampHeight = 50 * scale;
    const rampGeom = new THREE.BoxGeometry(rampWidth, 3 * scale, rampLength);
    const supportGeom = new THREE.BoxGeometry(rampWidth * 0.9, rampHeight, 10 * scale);
    const railGeom = new THREE.BoxGeometry(1.5 * scale, 6 * scale, rampLength * 1.2);

    rampPositions.forEach((pos, index) => {
      const rampGroup = new THREE.Group();
      const neonMat = rampNeonMats[index % rampNeonMats.length];

      // Main ramp surface
      const ramp = new THREE.Mesh(rampGeom, rampMaterial);
      ramp.rotation.x = -0.4;
      ramp.position.y = rampHeight / 2;
      ramp.position.z = rampLength / 4;
      rampGroup.add(ramp);

      // Support
      const support = new THREE.Mesh(supportGeom, rampMaterial);
      support.position.y = rampHeight / 2;
      support.position.z = rampLength / 2;
      rampGroup.add(support);

      // Side rails with neon
      [-1, 1].forEach(side => {
        const rail = new THREE.Mesh(railGeom, neonMat);
        rail.position.set(side * (rampWidth / 2 + 1 * scale), rampHeight / 2 + 5 * scale, rampLength / 4);
        rail.rotation.x = -0.4;
        rampGroup.add(rail);
      });

      rampGroup.position.set(pos.x, 0, pos.z);
      rampGroup.rotation.y = pos.rotY;
      scene.add(rampGroup);

      // Store ramp data for collision detection
      ramps.push({
        position: new THREE.Vector3(pos.x, 0, pos.z),
        rotation: pos.rotY,
        width: rampWidth,
        length: rampLength,
        launchVelocity: 200, // Reduced since no rainbow road to reach
        launchAngle: 0.5
      });
    });

    rampsRef.current = ramps;

  }, [createRoads, createBuildings, createStreetLights, createTrafficLights, createTrafficCars]);

  // Create the player's car - VICE CITY 80s TESTAROSSA STYLE
  const createCar = useCallback((scene) => {
    if (!THREE) return null;

    const car = new THREE.Group();
    const scale = 2; // Scale factor for bigger car

    // Main body paint - Iconic Vice City WHITE with high-quality reflections
    const bodyMaterial = new THREE.MeshPhysicalMaterial({
      color: 0xf8f8ff,
      roughness: 0.1,
      metalness: 0.9,
      clearcoat: 1.0,
      clearcoatRoughness: 0.1,
      reflectivity: 1.0,
    });

    // Lower body / chassis
    const chassisGeom = new THREE.BoxGeometry(2.1 * scale, 0.4 * scale, 4.8 * scale);
    const chassis = new THREE.Mesh(chassisGeom, bodyMaterial);
    chassis.position.y = 0.35 * scale;
    chassis.castShadow = true;
    car.add(chassis);

    // Hood (sloped front)
    const hoodGeom = new THREE.BoxGeometry(2 * scale, 0.25 * scale, 1.8 * scale);
    const hood = new THREE.Mesh(hoodGeom, bodyMaterial);
    hood.position.set(0, 0.68 * scale, 1.3 * scale);
    hood.rotation.x = -0.15;
    car.add(hood);

    // Main body upper
    const upperBodyGeom = new THREE.BoxGeometry(2 * scale, 0.35 * scale, 2.5 * scale);
    const upperBody = new THREE.Mesh(upperBodyGeom, bodyMaterial);
    upperBody.position.set(0, 0.72 * scale, -0.3 * scale);
    car.add(upperBody);

    // Trunk (rear)
    const trunkGeom = new THREE.BoxGeometry(1.9 * scale, 0.2 * scale, 1 * scale);
    const trunk = new THREE.Mesh(trunkGeom, bodyMaterial);
    trunk.position.set(0, 0.65 * scale, -2 * scale);
    car.add(trunk);

    // Windshield - realistic glass with reflections
    const glassMaterial = new THREE.MeshPhysicalMaterial({
      color: 0x88aacc,
      roughness: 0.0,
      metalness: 0.0,
      transparent: true,
      opacity: 0.4,
      transmission: 0.9,
      thickness: 0.1,
      reflectivity: 0.9,
      clearcoat: 1.0,
    });

    const windshieldGeom = new THREE.BoxGeometry(1.8 * scale, 0.6 * scale, 0.1 * scale);
    const windshield = new THREE.Mesh(windshieldGeom, glassMaterial);
    windshield.position.set(0, 1.05 * scale, 0.55 * scale);
    windshield.rotation.x = -0.5;
    car.add(windshield);

    // Rear window
    const rearWindowGeom = new THREE.BoxGeometry(1.7 * scale, 0.45 * scale, 0.1 * scale);
    const rearWindow = new THREE.Mesh(rearWindowGeom, glassMaterial);
    rearWindow.position.set(0, 0.95 * scale, -1.25 * scale);
    rearWindow.rotation.x = 0.4;
    car.add(rearWindow);

    // Side windows
    const sideWindowGeom = new THREE.BoxGeometry(0.05 * scale, 0.4 * scale, 1.5 * scale);
    [-1, 1].forEach(side => {
      const sideWindow = new THREE.Mesh(sideWindowGeom, glassMaterial);
      sideWindow.position.set(side * 1 * scale, 0.95 * scale, -0.3 * scale);
      car.add(sideWindow);
    });

    // Roof
    const roofMat = new THREE.MeshStandardMaterial({ color: 0x111111, roughness: 0.3, metalness: 0.8 });
    const roofGeom = new THREE.BoxGeometry(1.85 * scale, 0.1 * scale, 1.3 * scale);
    const roof = new THREE.Mesh(roofGeom, roofMat);
    roof.position.set(0, 1.15 * scale, -0.35 * scale);
    car.add(roof);

    // Wheels with rims - realistic rubber and chrome
    const wheelMaterial = new THREE.MeshStandardMaterial({
      color: 0x1a1a1a,
      roughness: 0.9,
      metalness: 0.0,
    });
    const rimMaterial = new THREE.MeshPhysicalMaterial({
      color: 0xdddddd,
      roughness: 0.1,
      metalness: 1.0,
      clearcoat: 0.8,
      reflectivity: 1.0,
    });

    const wheelPositions = [
      { x: -1 * scale, y: 0.35 * scale, z: 1.4 * scale },
      { x: 1 * scale, y: 0.35 * scale, z: 1.4 * scale },
      { x: -1 * scale, y: 0.35 * scale, z: -1.4 * scale },
      { x: 1 * scale, y: 0.35 * scale, z: -1.4 * scale }
    ];

    wheelPositions.forEach(pos => {
      // Tire
      const tireGeom = new THREE.TorusGeometry(0.35 * scale, 0.12 * scale, 8, 24);
      const tire = new THREE.Mesh(tireGeom, wheelMaterial);
      tire.position.set(pos.x, pos.y, pos.z);
      tire.rotation.y = Math.PI / 2;
      car.add(tire);

      // Rim
      const rimGeom = new THREE.CylinderGeometry(0.25 * scale, 0.25 * scale, 0.15 * scale, 12);
      const rim = new THREE.Mesh(rimGeom, rimMaterial);
      rim.position.set(pos.x * 1.05, pos.y, pos.z);
      rim.rotation.z = Math.PI / 2;
      car.add(rim);
    });

    // Headlights (more realistic with strong glow)
    const headlightMaterial = new THREE.MeshStandardMaterial({
      color: 0xffffff,
      emissive: 0xffffee,
      emissiveIntensity: 1.5,
    });

    [-0.7, 0.7].forEach(x => {
      // Headlight housing
      const housingGeom = new THREE.BoxGeometry(0.5 * scale, 0.2 * scale, 0.15 * scale);
      const housingMat = new THREE.MeshStandardMaterial({ color: 0x222222 });
      const housing = new THREE.Mesh(housingGeom, housingMat);
      housing.position.set(x * scale, 0.5 * scale, 2.45 * scale);
      car.add(housing);

      // LED strip
      const ledGeom = new THREE.BoxGeometry(0.4 * scale, 0.08 * scale, 0.05 * scale);
      const led = new THREE.Mesh(ledGeom, headlightMaterial);
      led.position.set(x * scale, 0.5 * scale, 2.5 * scale);
      car.add(led);
    });

    // Tail lights - brighter glow
    const tailLightMat = new THREE.MeshStandardMaterial({
      color: 0xff0000,
      emissive: 0xff0000,
      emissiveIntensity: 1.0,
    });

    [-0.7, 0.7].forEach(x => {
      const tailGeom = new THREE.BoxGeometry(0.4 * scale, 0.15 * scale, 0.05 * scale);
      const tailLight = new THREE.Mesh(tailGeom, tailLightMat);
      tailLight.position.set(x * scale, 0.55 * scale, -2.45 * scale);
      car.add(tailLight);
    });

    // Side mirrors
    const mirrorMat = new THREE.MeshStandardMaterial({ color: 0x111111 });
    [-1.1, 1.1].forEach(x => {
      const mirrorGeom = new THREE.BoxGeometry(0.15 * scale, 0.1 * scale, 0.2 * scale);
      const mirror = new THREE.Mesh(mirrorGeom, mirrorMat);
      mirror.position.set(x * scale, 0.8 * scale, 0.7 * scale);
      car.add(mirror);
    });

    // Grille
    const grilleMat = new THREE.MeshStandardMaterial({ color: 0x111111, roughness: 0.8 });
    const grilleGeom = new THREE.BoxGeometry(1.2 * scale, 0.25 * scale, 0.05 * scale);
    const grille = new THREE.Mesh(grilleGeom, grilleMat);
    grille.position.set(0, 0.35 * scale, 2.45 * scale);
    car.add(grille);

    // Exhaust pipes
    const exhaustMat = new THREE.MeshStandardMaterial({ color: 0x444444, metalness: 0.9 });
    [-0.4, 0.4].forEach(x => {
      const exhaustGeom = new THREE.CylinderGeometry(0.08 * scale, 0.08 * scale, 0.3 * scale, 12);
      const exhaust = new THREE.Mesh(exhaustGeom, exhaustMat);
      exhaust.position.set(x * scale, 0.2 * scale, -2.5 * scale);
      exhaust.rotation.x = Math.PI / 2;
      car.add(exhaust);
    });

    // ========== VICE CITY NEON UNDERGLOW - GLOW IN THE DARK ==========
    // Magenta neon material (primary glow)
    const neonMagentaMat = new THREE.MeshStandardMaterial({
      color: 0xff00ff,
      emissive: 0xff00ff,
      emissiveIntensity: 2.0,
      roughness: 0.1,
      metalness: 0.5
    });

    // Cyan neon material (accent glow)
    const neonCyanMat = new THREE.MeshStandardMaterial({
      color: 0x00ffff,
      emissive: 0x00ffff,
      emissiveIntensity: 2.0,
      roughness: 0.1,
      metalness: 0.5
    });

    // Underglow - Side strips (magenta)
    [-1.05, 1.05].forEach(x => {
      const underglowSideGeom = new THREE.BoxGeometry(0.08 * scale, 0.05 * scale, 4.5 * scale);
      const underglowSide = new THREE.Mesh(underglowSideGeom, neonMagentaMat);
      underglowSide.position.set(x * scale, 0.12 * scale, 0);
      car.add(underglowSide);
    });

    // Underglow - Front strip (cyan)
    const underglowFrontGeom = new THREE.BoxGeometry(2 * scale, 0.05 * scale, 0.08 * scale);
    const underglowFront = new THREE.Mesh(underglowFrontGeom, neonCyanMat);
    underglowFront.position.set(0, 0.12 * scale, 2.3 * scale);
    car.add(underglowFront);

    // Underglow - Rear strip (cyan)
    const underglowRearGeom = new THREE.BoxGeometry(1.8 * scale, 0.05 * scale, 0.08 * scale);
    const underglowRear = new THREE.Mesh(underglowRearGeom, neonCyanMat);
    underglowRear.position.set(0, 0.12 * scale, -2.3 * scale);
    car.add(underglowRear);

    // Body accent strips - Side body lines (magenta)
    [-1.02, 1.02].forEach(x => {
      const bodyLineGeom = new THREE.BoxGeometry(0.03 * scale, 0.08 * scale, 4.2 * scale);
      const bodyLine = new THREE.Mesh(bodyLineGeom, neonMagentaMat);
      bodyLine.position.set(x * scale, 0.55 * scale, 0);
      car.add(bodyLine);
    });

    // Wheel well glow (cyan) - illuminates the wheels
    wheelPositions.forEach(pos => {
      const wheelGlowGeom = new THREE.TorusGeometry(0.4 * scale, 0.03 * scale, 8, 16);
      const wheelGlow = new THREE.Mesh(wheelGlowGeom, neonCyanMat);
      wheelGlow.position.set(pos.x * 0.95, pos.y, pos.z);
      wheelGlow.rotation.y = Math.PI / 2;
      car.add(wheelGlow);
    });

    // Hood accent lines (magenta)
    [-0.6, 0.6].forEach(x => {
      const hoodLineGeom = new THREE.BoxGeometry(0.04 * scale, 0.02 * scale, 1.5 * scale);
      const hoodLine = new THREE.Mesh(hoodLineGeom, neonMagentaMat);
      hoodLine.position.set(x * scale, 0.82 * scale, 1.3 * scale);
      hoodLine.rotation.x = -0.15;
      car.add(hoodLine);
    });

    // Roof edge glow (cyan)
    const roofGlowGeom = new THREE.BoxGeometry(1.9 * scale, 0.02 * scale, 0.04 * scale);
    const roofGlowFront = new THREE.Mesh(roofGlowGeom, neonCyanMat);
    roofGlowFront.position.set(0, 1.17 * scale, 0.3 * scale);
    car.add(roofGlowFront);

    const roofGlowRear = new THREE.Mesh(roofGlowGeom, neonCyanMat);
    roofGlowRear.position.set(0, 1.17 * scale, -1 * scale);
    car.add(roofGlowRear);

    // REMOVED point lights for performance - the emissive materials provide enough glow effect

    car.position.set(0, 0, 0);
    scene.add(car);

    return car;
  }, []);

  // Update car physics - OPTIMIZED with better steering + FLYING MECHANICS
  const updateCarPhysics = useCallback(() => {
    if (!carRef.current || !carPhysicsRef.current || !THREE) return;

    const physics = carPhysicsRef.current;
    const keys = keysRef.current;
    const touch = touchControlsRef.current;
    const scale = 2;

    const accelerating = keys['KeyW'] || keys['ArrowUp'] || touch.gas;
    const braking = keys['KeyS'] || keys['ArrowDown'] || touch.brake;
    const turningLeft = keys['KeyA'] || keys['ArrowLeft'] || touch.steering < -0.1;
    const turningRight = keys['KeyD'] || keys['ArrowRight'] || touch.steering > 0.1;

    // Better steering input handling
    let steerAmount = 0;
    if (touch.steering !== 0) {
      steerAmount = touch.steering; // Use analog touch steering
    } else if (turningLeft) {
      steerAmount = -1;
    } else if (turningRight) {
      steerAmount = 1;
    }

    // Initialize vertical velocity if not exists
    if (physics.verticalVelocity === undefined) {
      physics.verticalVelocity = 0;
      physics.isAirborne = false;
      physics.groundLevel = 1; // Default ground level
    }

    const gravity = -0.8; // Gravity acceleration

    // Check for ramp collision (only when on ground)
    if (!physics.isAirborne) {
      rampsRef.current.forEach(ramp => {
        const dx = physics.position.x - ramp.position.x;
        const dz = physics.position.z - ramp.position.z;
        const distance = Math.sqrt(dx * dx + dz * dz);

        // Check if car is on the ramp
        if (distance < ramp.length * 0.8 && Math.abs(physics.speed) > 30) {
          // Launch the car!
          physics.isAirborne = true;
          // Higher minimum launch velocity, scales with speed
          const speedFactor = 0.5 + (Math.abs(physics.speed) / physics.maxSpeed) * 0.5;
          physics.verticalVelocity = ramp.launchVelocity * speedFactor;
          // Boost forward speed when launching
          physics.speed = Math.max(physics.speed, 100);
        }
      });
    }

    // Apply gravity when airborne
    if (physics.isAirborne) {
      physics.verticalVelocity += gravity;
      physics.position.y += physics.verticalVelocity * 0.016;

      // Check if fallen back to ground
      if (physics.position.y <= 1 && physics.verticalVelocity < 0) {
        physics.position.y = 1;
        physics.verticalVelocity = 0;
        physics.isAirborne = false;
        physics.groundLevel = 1;
      }
    }

    // Acceleration and braking (reduced air control)
    const controlMultiplier = physics.isAirborne ? 0.3 : 1.0;

    if (accelerating) {
      physics.speed = Math.min(physics.speed + physics.acceleration * controlMultiplier, physics.maxSpeed);
    } else if (braking) {
      physics.speed = Math.max(physics.speed - physics.braking * controlMultiplier, -physics.maxSpeed * 0.3);
    } else {
      // Natural deceleration (less in air)
      const frictionMult = physics.isAirborne ? 0.3 : 1.0;
      if (physics.speed > 0) {
        physics.speed = Math.max(physics.speed - physics.friction * frictionMult, 0);
      } else if (physics.speed < 0) {
        physics.speed = Math.min(physics.speed + physics.friction * frictionMult, 0);
      }
    }

    // IMPROVED STEERING: More responsive, speed-dependent turning (reduced in air)
    if (Math.abs(physics.speed) > 0.3) {
      const speedRatio = Math.abs(physics.speed) / physics.maxSpeed;
      const turnMultiplier = (1.2 - (speedRatio * 0.6)) * controlMultiplier;
      const baseTurnSpeed = 0.045;

      physics.rotation += steerAmount * baseTurnSpeed * turnMultiplier * Math.sign(physics.speed);
    }

    // Update horizontal position
    const direction = new THREE.Vector3(
      Math.sin(physics.rotation),
      0,
      Math.cos(physics.rotation)
    );

    physics.position.x += direction.x * physics.speed * 0.016;
    physics.position.z += direction.z * physics.speed * 0.016;

    // Map boundaries - allow beach driving, stop at shallow water
    const mapBoundary = 4800; // Doubled for larger downtown area
    const oceanBoundary = 780; // Allow beach (z=700), stop at shallow water edge

    // Tampa Bay check - ellipse centered at (0, -200)
    const bayX = 0, bayZ = -200;
    const bayRadiusX = 450, bayRadiusZ = 350;
    const dxBay = (physics.position.x - bayX) / bayRadiusX;
    const dzBay = (physics.position.z - bayZ) / bayRadiusZ;
    const inBay = (dxBay * dxBay + dzBay * dzBay) < 1;

    // Check if on bay bridge (bridge is at z=-200, x from -475 to 475, width 45)
    const onBayBridge = Math.abs(physics.position.z - (-200)) < 25 && Math.abs(physics.position.x) < 500;

    // Check if on wide channel bridge (spans z=140 to z=420, centered at z=280)
    const wideBridgeCenterZ = 280;
    const wideBridgeLength = 350;
    const wideBridgeDepth = 280;
    const onWideBridge = Math.abs(physics.position.x) < (wideBridgeLength / 2 + 80) &&
                         physics.position.z > (wideBridgeCenterZ - wideBridgeDepth / 2 - 20) &&
                         physics.position.z < (wideBridgeCenterZ + wideBridgeDepth / 2 + 20);

    // Check if in channel (connects bay to ocean) - stops before beach at z=650
    const channelWidth = 180;
    const inChannel = Math.abs(physics.position.x) < channelWidth / 2 && physics.position.z > 100 && physics.position.z < 650;

    // Handle bay bridge height
    if (onBayBridge && !physics.isAirborne) {
      // Bridge deck is at y=10 (bridgeHeight + road surface)
      const bridgeY = 10;
      if (physics.position.y < bridgeY) {
        physics.position.y = bridgeY;
      }
    }
    // Handle wide channel bridge height
    else if (onWideBridge && !physics.isAirborne) {
      const wideBridgeY = 15;
      if (physics.position.y < wideBridgeY) {
        physics.position.y = wideBridgeY;
      }
    }
    else if (inBay && !onBayBridge) {
      // Push car out of bay if not on bridge
      const angle = Math.atan2(physics.position.z - bayZ, physics.position.x - bayX);
      physics.position.x = bayX + Math.cos(angle) * (bayRadiusX + 10);
      physics.position.z = bayZ + Math.sin(angle) * (bayRadiusZ + 10);
      physics.speed *= 0.3; // Slow down significantly
    } else if (inChannel && !onWideBridge && !physics.isAirborne) {
      // Push car out of channel to nearest bank (unless on bridge)
      if (physics.position.x >= 0) {
        physics.position.x = channelWidth / 2 + 10;
      } else {
        physics.position.x = -channelWidth / 2 - 10;
      }
      physics.speed *= 0.3;
    }

    if (physics.position.z > oceanBoundary) {
      physics.position.z = oceanBoundary;
      physics.speed *= 0.5; // Slow down at boundary
    }
    if (physics.position.x > mapBoundary) physics.position.x = mapBoundary;
    if (physics.position.x < -mapBoundary) physics.position.x = -mapBoundary;
    if (physics.position.z < -mapBoundary) physics.position.z = -mapBoundary;

    // Update car mesh position and rotation
    carRef.current.position.copy(physics.position);
    carRef.current.rotation.y = physics.rotation;

    // Add pitch rotation when airborne (nose up when going up, down when falling)
    if (physics.isAirborne) {
      const pitchAmount = Math.max(-0.3, Math.min(0.3, physics.verticalVelocity * 0.003));
      carRef.current.rotation.x = pitchAmount;
    } else {
      carRef.current.rotation.x = 0;
    }

    // Update HUD
    const newSpeed = Math.abs(Math.round(physics.speed));
    const newRpm = Math.min(Math.abs(physics.speed) * 80, 8000);

    setSpeed(newSpeed);
    setRpm(newRpm);
    setAltitude(Math.max(0, Math.round(physics.position.y)));
    setIsFlying(physics.isAirborne || physics.position.y > 10);
  }, []);

  // Update camera - adjusted for bigger car, FIXED DISTANCE regardless of speed
  const updateCamera = useCallback(() => {
    if (!carRef.current || !cameraRef.current || !carPhysicsRef.current || !THREE) return;

    const physics = carPhysicsRef.current;
    const camera = cameraRef.current;

    // Fixed camera distance - same at all speeds
    // Slightly higher when airborne to see more of the world
    const heightOffset = physics.isAirborne ? 12 : 8;
    const distanceOffset = physics.isAirborne ? -22 : -18;

    const cameraOffset = new THREE.Vector3(0, heightOffset, distanceOffset);
    cameraOffset.applyAxisAngle(new THREE.Vector3(0, 1, 0), physics.rotation);

    const targetPosition = physics.position.clone().add(cameraOffset);

    // Lock camera tightly to car - no lag at high speeds
    camera.position.copy(targetPosition);

    const lookAtPosition = physics.position.clone();
    lookAtPosition.y += 2;
    camera.lookAt(lookAtPosition);
  }, []);
  // Update day/night cycle - 5 minute full cycle
  const updateDayNightCycle = useCallback(() => {
    if (!sceneRef.current || !ambientLightRef.current || !hemiLightRef.current || !mainLightRef.current || !THREE) return;

    // Advance time (60fps assumed, so 1/60 second per frame)
    dayTimeRef.current += (1 / 60) / DAY_CYCLE_DURATION;
    if (dayTimeRef.current > 1) dayTimeRef.current = 0;

    const t = dayTimeRef.current;
    const scene = sceneRef.current;
    const ambientLight = ambientLightRef.current;
    const hemiLight = hemiLightRef.current;
    const mainLight = mainLightRef.current;

    // Time of day phases:
    // 0.0 - 0.25: Sunrise (dark to bright)
    // 0.25 - 0.5: Day (bright)
    // 0.5 - 0.75: Sunset (bright to dark)
    // 0.75 - 1.0: Night (dark)

    let skyColor, fogColor, ambientColor, sunColor, sunIntensity, ambientIntensity;
    let sunY, sunX;

    if (t < 0.25) {
      // Sunrise: 0 to 0.25
      const phase = t / 0.25;
      skyColor = new THREE.Color().lerpColors(new THREE.Color(0x0a0a1a), new THREE.Color(0xff7744), phase);
      fogColor = new THREE.Color().lerpColors(new THREE.Color(0x0a0a1a), new THREE.Color(0xffaa66), phase);
      ambientColor = new THREE.Color().lerpColors(new THREE.Color(0x202040), new THREE.Color(0xffddbb), phase);
      sunColor = new THREE.Color(0xffaa44);
      sunIntensity = 0.3 + phase * 0.7;
      ambientIntensity = 0.2 + phase * 0.4;
      sunY = phase * 100;
      sunX = -50 + phase * 50;
    } else if (t < 0.5) {
      // Day: 0.25 to 0.5
      const phase = (t - 0.25) / 0.25;
      skyColor = new THREE.Color().lerpColors(new THREE.Color(0xff7744), new THREE.Color(0x66ccff), phase);
      fogColor = new THREE.Color().lerpColors(new THREE.Color(0xffaa66), new THREE.Color(0xaaddff), phase);
      ambientColor = new THREE.Color().lerpColors(new THREE.Color(0xffddbb), new THREE.Color(0xffffff), phase);
      sunColor = new THREE.Color().lerpColors(new THREE.Color(0xffaa44), new THREE.Color(0xffffee), phase);
      sunIntensity = 1.0;
      ambientIntensity = 0.6;
      sunY = 100;
      sunX = phase * 100;
    } else if (t < 0.75) {
      // Sunset: 0.5 to 0.75
      const phase = (t - 0.5) / 0.25;
      skyColor = new THREE.Color().lerpColors(new THREE.Color(0x66ccff), new THREE.Color(0xff5500), phase);
      fogColor = new THREE.Color().lerpColors(new THREE.Color(0xaaddff), new THREE.Color(0xff6622), phase);
      ambientColor = new THREE.Color().lerpColors(new THREE.Color(0xffffff), new THREE.Color(0xff8844), phase);
      sunColor = new THREE.Color().lerpColors(new THREE.Color(0xffffee), new THREE.Color(0xff4400), phase);
      sunIntensity = 1.0 - phase * 0.7;
      ambientIntensity = 0.6 - phase * 0.4;
      sunY = 100 - phase * 100;
      sunX = 100 - phase * 50;
    } else {
      // Night: 0.75 to 1.0
      const phase = (t - 0.75) / 0.25;
      skyColor = new THREE.Color().lerpColors(new THREE.Color(0xff5500), new THREE.Color(0x0a0a1a), phase);
      fogColor = new THREE.Color().lerpColors(new THREE.Color(0xff6622), new THREE.Color(0x0a0a1a), phase);
      ambientColor = new THREE.Color().lerpColors(new THREE.Color(0xff8844), new THREE.Color(0x202040), phase);
      sunColor = new THREE.Color(0x4444ff); // Moonlight
      sunIntensity = 0.3;
      ambientIntensity = 0.2;
      sunY = phase * 50;
      sunX = -50;
    }

    // Apply colors
    scene.background = skyColor;
    scene.fog.color = fogColor;
    ambientLight.color = ambientColor;
    ambientLight.intensity = ambientIntensity;
    hemiLight.color = skyColor;
    hemiLight.groundColor = new THREE.Color(0x444422);
    mainLight.color = sunColor;
    mainLight.intensity = sunIntensity;
    mainLight.position.set(sunX, Math.max(10, sunY), 50);
  }, []);

  // Initialize Three.js
  useEffect(() => {
    let mounted = true;

    const initThree = async () => {
      try {
        // Dynamic import of THREE
        const threeModule = await import('three');
        THREE = threeModule;

        if (!mounted || !containerRef.current) return;

        // Initialize physics with THREE now available - scaled for bigger world
        carPhysicsRef.current = {
          velocity: new THREE.Vector3(),
          angularVelocity: 0,
          speed: 0,
          maxSpeed: 360, // Doubled from 180
          acceleration: 0.5, // Doubled for faster acceleration
          braking: 0.45,
          friction: 0.03,
          turnSpeed: 0.025,
          position: new THREE.Vector3(0, 1, 0),
          rotation: 0
        };

        // Scene - VICE CITY SUNSET SKY (iconic purple/orange gradient feel)
        const scene = new THREE.Scene();
        scene.background = new THREE.Color(0x1a0a2e); // Deep purple night sky
        scene.fog = new THREE.Fog(0x1a0a2e, 100, 2500); // Extended fog for 4K detail visibility
        sceneRef.current = scene;

        // Camera - extended far plane for 4K detail
        const camera = new THREE.PerspectiveCamera(
          75,
          window.innerWidth / window.innerHeight,
          0.5,  // Lower near plane for sharper close-ups
          4000  // Extended far plane for more detail
        );
        camera.position.set(0, 5, -10);
        cameraRef.current = camera;

        // Renderer - 4K QUALITY settings
        const renderer = new THREE.WebGLRenderer({
          antialias: true, // Smooth edges
          powerPreference: 'high-performance',
          stencil: true,
          depth: true,
          alpha: false,
          preserveDrawingBuffer: false,
        });
        renderer.setSize(window.innerWidth, window.innerHeight);
        renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2)); // Cap at 2x for performance
        renderer.shadowMap.enabled = true;
        renderer.shadowMap.type = THREE.PCFShadowMap; // Faster shadows
        renderer.toneMapping = THREE.ACESFilmicToneMapping; // Cinematic tone mapping
        renderer.toneMappingExposure = 1.2;
        renderer.outputColorSpace = THREE.SRGBColorSpace;
        renderer.sortObjects = false; // Disable sorting for performance
        containerRef.current.appendChild(renderer.domElement);
        rendererRef.current = renderer;

        // ENHANCED LIGHTING for day/night cycle
        // Ambient light - will change color throughout the day
        const ambientLight = new THREE.AmbientLight(0x404060, 0.4);
        scene.add(ambientLight);
        ambientLightRef.current = ambientLight;

        // Hemisphere light for natural sky/ground color blend
        const hemiLight = new THREE.HemisphereLight(0x6666aa, 0x444422, 0.5);
        scene.add(hemiLight);
        hemiLightRef.current = hemiLight;

        // Main directional light (sun) with shadows
        const mainLight = new THREE.DirectionalLight(0xffeedd, 0.8);
        mainLight.position.set(50, 100, 50);
        mainLight.castShadow = true;
        mainLight.shadow.mapSize.width = 1024;
        mainLight.shadow.mapSize.height = 1024;
        mainLight.shadow.camera.near = 1;
        mainLight.shadow.camera.far = 500;
        mainLight.shadow.camera.left = -100;
        mainLight.shadow.camera.right = 100;
        mainLight.shadow.camera.top = 100;
        mainLight.shadow.camera.bottom = -100;
        mainLight.shadow.bias = -0.001;
        scene.add(mainLight);
        mainLightRef.current = mainLight;

        // Fill light from opposite side
        const fillLight = new THREE.DirectionalLight(0x8888ff, 0.3);
        fillLight.position.set(-30, 50, -30);
        scene.add(fillLight);

        // Rim light for car highlights
        const rimLight = new THREE.DirectionalLight(0xff8866, 0.4);
        rimLight.position.set(0, 20, -50);
        scene.add(rimLight);

        // Create city and car
        createCity(scene);
        const car = createCar(scene);
        carRef.current = car;

        setIsLoading(false);

        // Animation loop
        const animate = () => {
          if (!mounted) return;
          animationRef.current = requestAnimationFrame(animate);
          updateCarPhysics();
          updateCamera();
          updateDayNightCycle();

          // Update traffic cars
          trafficCarsRef.current.forEach(tc => {
            tc.progress += tc.route.speed * 0.016;
            if (tc.progress > 1) tc.progress = 0;
            if (tc.progress < 0) tc.progress = 1;
            tc.mesh.position.set(
              tc.route.startX + (tc.route.endX - tc.route.startX) * tc.progress,
              0,
              tc.route.startZ + (tc.route.endZ - tc.route.startZ) * tc.progress
            );
          });

          renderer.render(scene, camera);
        };
        animate();

      } catch (err) {
        console.error('Failed to initialize Three.js:', err);
        setError(err.message);
        setIsLoading(false);
      }
    };

    initThree();

    // Handle resize
    const handleResize = () => {
      if (cameraRef.current && rendererRef.current) {
        cameraRef.current.aspect = window.innerWidth / window.innerHeight;
        cameraRef.current.updateProjectionMatrix();
        rendererRef.current.setSize(window.innerWidth, window.innerHeight);
      }
      setIsLandscape(window.innerWidth > window.innerHeight);
    };
    window.addEventListener('resize', handleResize);

    // Keyboard controls
    const handleKeyDown = (e) => { keysRef.current[e.code] = true; };
    const handleKeyUp = (e) => { keysRef.current[e.code] = false; };
    window.addEventListener('keydown', handleKeyDown);
    window.addEventListener('keyup', handleKeyUp);

    return () => {
      mounted = false;
      window.removeEventListener('resize', handleResize);
      window.removeEventListener('keydown', handleKeyDown);
      window.removeEventListener('keyup', handleKeyUp);

      if (animationRef.current) {
        cancelAnimationFrame(animationRef.current);
      }

      if (rendererRef.current && containerRef.current) {
        containerRef.current.removeChild(rendererRef.current.domElement);
        rendererRef.current.dispose();
      }
    };
  }, [createCity, createCar, updateCarPhysics, updateCamera]);

  // Touch controls
  const handleSteeringMove = useCallback((e) => {
    const touch = e.touches[0];
    const rect = e.currentTarget.getBoundingClientRect();
    const centerX = rect.left + rect.width / 2;
    const deltaX = touch.clientX - centerX;
    touchControlsRef.current.steering = Math.max(-1, Math.min(1, deltaX / (rect.width / 2)));
  }, []);

  const handleSteeringEnd = useCallback(() => {
    touchControlsRef.current.steering = 0;
  }, []);

  const handleGasStart = useCallback(() => {
    touchControlsRef.current.gas = true;
  }, []);

  const handleGasEnd = useCallback(() => {
    touchControlsRef.current.gas = false;
  }, []);

  if (error) {
    return (
      <div className="simulator-game">
        <div className="simulator-error">
          <h2>Failed to load game</h2>
          <p>{error}</p>
          <button onClick={handleExit}>Go Back</button>
        </div>
      </div>
    );
  }

  return (
    <div className="simulator-game">
      {isLoading && (
        <div className="simulator-loading">
          <div className="loading-spinner"></div>
          <p>Welcome to Playhouse City...</p>
        </div>
      )}

      {!isLandscape && !isLoading && (
        <div className="rotate-device-overlay">
          <div className="rotate-content">
            <div className="rotate-icon">📱↻</div>
            <h2>Rotate Your Device</h2>
            <p>Please rotate your phone to landscape mode for the best driving experience</p>
          </div>
        </div>
      )}

      <div ref={containerRef} className="game-canvas" />

      {/* HUD */}
      {!isLoading && (
        <div className="simulator-hud">
          <button className="exit-btn" onClick={handleExit}>✕</button>

          {/* Altitude indicator - shows when flying */}
          {isFlying && (
            <div className="altitude-indicator">
              <div className="altitude-icon">🚀</div>
              <div className="altitude-value">{altitude}</div>
              <div className="altitude-unit">ALT</div>
            </div>
          )}

          <div className="mini-map">
            <div className="map-dot" />
          </div>
        </div>
      )}

      {/* Touch Controls */}
      {isLandscape && !isLoading && (
        <div className="touch-controls">
          <div
            className="steering-area"
            onTouchMove={handleSteeringMove}
            onTouchEnd={handleSteeringEnd}
          >
            <div className="steering-wheel">
              <div className="steering-indicator" style={{ transform: `rotate(${touchControlsRef.current.steering * 90}deg)` }} />
            </div>
          </div>

          <div className="pedals">
            <button
              className="pedal gas-pedal"
              onTouchStart={handleGasStart}
              onTouchEnd={handleGasEnd}
              onMouseDown={handleGasStart}
              onMouseUp={handleGasEnd}
              onMouseLeave={handleGasEnd}
            >
              GAS
            </button>
          </div>
        </div>
      )}

      {/* Controls hint */}
      {showControls && isLandscape && !isLoading && (
        <div className="controls-hint" onClick={() => setShowControls(false)}>
          <div className="hint-content">
            <h3>Controls</h3>
            <p><strong>Desktop:</strong> W/↑ = Gas, S/↓ = Brake, A/D or ←/→ = Steer</p>
            <p><strong>Mobile:</strong> Use on-screen controls</p>
            <button onClick={() => setShowControls(false)}>Got it!</button>
          </div>
        </div>
      )}
    </div>
  );
};

export default SimulatorGame;
