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

  // Touch controls state - analog values for smooth control
  const touchControlsRef = useRef({
    pitch: 0,      // -1 (dive) to 1 (climb)
    roll: 0,       // -1 (left) to 1 (right)
    throttleChange: 0, // -1 (decrease) to 1 (increase)
  });

  // Create terrain with mountains, desert, ocean, and cities
  const createTerrain = useCallback((scene) => {
    if (!THREE.current) return;
    const T = THREE.current;

    const worldSize = 50000;

    // === OCEAN (base layer) ===
    const oceanGeom = new T.PlaneGeometry(worldSize * 2, worldSize * 2);
    const oceanMat = new T.MeshBasicMaterial({ color: 0x0a4a6e });
    const ocean = new T.Mesh(oceanGeom, oceanMat);
    ocean.rotation.x = -Math.PI / 2;
    ocean.position.y = -5;
    scene.add(ocean);

    // Shallow water near coasts
    const shallowGeom = new T.PlaneGeometry(worldSize, 8000);
    const shallowMat = new T.MeshBasicMaterial({ color: 0x1a6a8e });
    const shallowWater = new T.Mesh(shallowGeom, shallowMat);
    shallowWater.rotation.x = -Math.PI / 2;
    shallowWater.position.set(0, -2, worldSize / 2 - 4000);
    scene.add(shallowWater);

    // === MAIN LANDMASS ===
    const landGeom = new T.PlaneGeometry(worldSize, worldSize);
    const landMat = new T.MeshBasicMaterial({ color: 0x3d6b3d });
    const land = new T.Mesh(landGeom, landMat);
    land.rotation.x = -Math.PI / 2;
    land.position.y = 0;
    scene.add(land);

    // === DESERT REGION (southwest) ===
    const desertGeom = new T.PlaneGeometry(15000, 15000);
    const desertMat = new T.MeshBasicMaterial({ color: 0xd4a574 });
    const desert = new T.Mesh(desertGeom, desertMat);
    desert.rotation.x = -Math.PI / 2;
    desert.position.set(-12000, 1, -12000);
    scene.add(desert);

    // Desert dunes
    const duneMat = new T.MeshBasicMaterial({ color: 0xe8c090 });
    for (let i = 0; i < 30; i++) {
      const duneGeom = new T.ConeGeometry(300 + Math.random() * 400, 50 + Math.random() * 80, 6);
      const dune = new T.Mesh(duneGeom, duneMat);
      dune.position.set(
        -12000 + (Math.random() - 0.5) * 14000,
        25 + Math.random() * 40,
        -12000 + (Math.random() - 0.5) * 14000
      );
      dune.scale.set(1, 0.3, 1);
      scene.add(dune);
    }

    // === MOUNTAIN RANGE (north) ===
    const mountainMat = new T.MeshBasicMaterial({ color: 0x6b6b6b });
    const snowMat = new T.MeshBasicMaterial({ color: 0xf0f0f0 });
    const rockMat = new T.MeshBasicMaterial({ color: 0x4a4a4a });

    // Main mountain peaks
    const mountainPositions = [
      { x: 0, z: -18000, height: 4000, radius: 3000 },
      { x: -5000, z: -16000, height: 3500, radius: 2500 },
      { x: 5000, z: -17000, height: 3800, radius: 2800 },
      { x: -10000, z: -15000, height: 3000, radius: 2000 },
      { x: 10000, z: -16000, height: 3200, radius: 2200 },
      { x: -3000, z: -20000, height: 4500, radius: 3500 },
      { x: 3000, z: -19000, height: 4200, radius: 3200 },
      { x: -8000, z: -18000, height: 3600, radius: 2600 },
      { x: 8000, z: -19000, height: 3400, radius: 2400 },
      { x: 0, z: -22000, height: 5000, radius: 4000 }, // Tallest peak
    ];

    mountainPositions.forEach(mt => {
      // Mountain base
      const baseGeom = new T.ConeGeometry(mt.radius, mt.height, 8);
      const base = new T.Mesh(baseGeom, mountainMat);
      base.position.set(mt.x, mt.height / 2, mt.z);
      scene.add(base);

      // Snow cap (top 30%)
      const snowGeom = new T.ConeGeometry(mt.radius * 0.35, mt.height * 0.3, 8);
      const snow = new T.Mesh(snowGeom, snowMat);
      snow.position.set(mt.x, mt.height * 0.85, mt.z);
      scene.add(snow);

      // Rocky outcrops
      for (let i = 0; i < 5; i++) {
        const rockGeom = new T.ConeGeometry(mt.radius * 0.15, mt.height * 0.2, 5);
        const rock = new T.Mesh(rockGeom, rockMat);
        const angle = (i / 5) * Math.PI * 2;
        rock.position.set(
          mt.x + Math.cos(angle) * mt.radius * 0.6,
          mt.height * 0.3,
          mt.z + Math.sin(angle) * mt.radius * 0.6
        );
        scene.add(rock);
      }
    });

    // Foothills
    for (let i = 0; i < 50; i++) {
      const hillGeom = new T.ConeGeometry(400 + Math.random() * 600, 200 + Math.random() * 400, 6);
      const hillMat = new T.MeshBasicMaterial({ color: Math.random() > 0.5 ? 0x5a7a5a : 0x6b6b6b });
      const hill = new T.Mesh(hillGeom, hillMat);
      hill.position.set(
        (Math.random() - 0.5) * 25000,
        100 + Math.random() * 200,
        -10000 - Math.random() * 8000
      );
      scene.add(hill);
    }

    // === FORESTS ===
    const treeMat = new T.MeshBasicMaterial({ color: 0x1a5a1a });
    const trunkMat = new T.MeshBasicMaterial({ color: 0x4a3020 });
    const treeGeom = new T.ConeGeometry(80, 200, 5);
    const trunkGeom = new T.CylinderGeometry(15, 20, 60, 5);

    // Forest patches
    const forestCenters = [
      { x: 8000, z: 5000, count: 200 },
      { x: -5000, z: 8000, count: 150 },
      { x: 12000, z: -5000, count: 180 },
      { x: -15000, z: 3000, count: 100 },
    ];

    forestCenters.forEach(forest => {
      for (let i = 0; i < forest.count; i++) {
        const treeGroup = new T.Group();
        const tree = new T.Mesh(treeGeom, treeMat);
        tree.position.y = 130;
        treeGroup.add(tree);

        const trunk = new T.Mesh(trunkGeom, trunkMat);
        trunk.position.y = 30;
        treeGroup.add(trunk);

        const scale = 0.8 + Math.random() * 0.6;
        treeGroup.scale.set(scale, scale, scale);
        treeGroup.position.set(
          forest.x + (Math.random() - 0.5) * 5000,
          0,
          forest.z + (Math.random() - 0.5) * 5000
        );
        scene.add(treeGroup);
      }
    });

    // === CITIES ===
    const createCity = (centerX, centerZ, size, name) => {
      const buildingColors = [
        new T.MeshBasicMaterial({ color: 0xd0d0d0 }),
        new T.MeshBasicMaterial({ color: 0xc0c0c0 }),
        new T.MeshBasicMaterial({ color: 0xb0b0b0 }),
        new T.MeshBasicMaterial({ color: 0xe0e0e0 }),
      ];
      const windowMat = new T.MeshBasicMaterial({ color: 0x3a5a7a });
      const glowMat = new T.MeshBasicMaterial({ color: 0xffffaa });

      const numBuildings = size === 'large' ? 150 : size === 'medium' ? 80 : 40;
      const spread = size === 'large' ? 4000 : size === 'medium' ? 2500 : 1500;
      const maxHeight = size === 'large' ? 600 : size === 'medium' ? 400 : 200;

      for (let i = 0; i < numBuildings; i++) {
        const distFromCenter = Math.sqrt(
          Math.pow((Math.random() - 0.5) * spread, 2) +
          Math.pow((Math.random() - 0.5) * spread, 2)
        );
        const heightFactor = 1 - (distFromCenter / (spread * 0.7));
        const height = Math.max(30, (50 + Math.random() * maxHeight) * Math.max(0.2, heightFactor));
        const width = 40 + Math.random() * 80;
        const depth = 40 + Math.random() * 80;

        const buildingGeom = new T.BoxGeometry(width, height, depth);
        const building = new T.Mesh(buildingGeom, buildingColors[Math.floor(Math.random() * buildingColors.length)]);
        building.position.set(
          centerX + (Math.random() - 0.5) * spread,
          height / 2,
          centerZ + (Math.random() - 0.5) * spread
        );
        scene.add(building);

        // Windows
        if (height > 100) {
          const numFloors = Math.floor(height / 25);
          for (let floor = 1; floor < numFloors; floor++) {
            if (Math.random() > 0.3) {
              const windowGeom = new T.BoxGeometry(width * 0.8, 8, 1);
              const window = new T.Mesh(windowGeom, Math.random() > 0.4 ? windowMat : glowMat);
              window.position.set(
                building.position.x,
                floor * 25,
                building.position.z + depth / 2 + 0.5
              );
              scene.add(window);
            }
          }
        }
      }

      // Roads grid
      const roadMat = new T.MeshBasicMaterial({ color: 0x333333 });
      const roadWidth = 30;
      const roadSpacing = size === 'large' ? 400 : 300;

      for (let x = -spread / 2; x <= spread / 2; x += roadSpacing) {
        const roadGeom = new T.BoxGeometry(roadWidth, 0.5, spread);
        const road = new T.Mesh(roadGeom, roadMat);
        road.position.set(centerX + x, 0.3, centerZ);
        scene.add(road);
      }
      for (let z = -spread / 2; z <= spread / 2; z += roadSpacing) {
        const roadGeom = new T.BoxGeometry(spread, 0.5, roadWidth);
        const road = new T.Mesh(roadGeom, roadMat);
        road.position.set(centerX, 0.3, centerZ + z);
        scene.add(road);
      }
    };

    // Create multiple cities
    createCity(0, 0, 'large', 'Central City'); // Main city at origin
    createCity(15000, 10000, 'medium', 'East Harbor');
    createCity(-18000, 5000, 'medium', 'West Valley');
    createCity(8000, -8000, 'small', 'Mountain View');
    createCity(-8000, 15000, 'small', 'Coastal Town');

    // === COASTAL FEATURES ===
    // Beach
    const beachGeom = new T.PlaneGeometry(worldSize, 2000);
    const beachMat = new T.MeshBasicMaterial({ color: 0xe8d4a8 });
    const beach = new T.Mesh(beachGeom, beachMat);
    beach.rotation.x = -Math.PI / 2;
    beach.position.set(0, 0.5, worldSize / 2 - 1000);
    scene.add(beach);

    // === RIVERS ===
    const riverMat = new T.MeshBasicMaterial({ color: 0x2a6a8a });
    // Main river
    const riverGeom = new T.BoxGeometry(200, 1, 30000);
    const river = new T.Mesh(riverGeom, riverMat);
    river.position.set(5000, 0.2, -5000);
    river.rotation.y = 0.3;
    scene.add(river);

    // River branches
    const river2Geom = new T.BoxGeometry(150, 1, 15000);
    const river2 = new T.Mesh(river2Geom, riverMat);
    river2.position.set(-3000, 0.2, 5000);
    river2.rotation.y = -0.5;
    scene.add(river2);

    // === AIRPORTS/RUNWAYS ===
    const runwayMat = new T.MeshBasicMaterial({ color: 0x2a2a2a });
    const markingMat = new T.MeshBasicMaterial({ color: 0xffffff });

    // Main airport near central city
    const runwayGeom = new T.BoxGeometry(80, 0.5, 3000);
    const runway = new T.Mesh(runwayGeom, runwayMat);
    runway.position.set(3000, 0.3, 0);
    scene.add(runway);

    // Runway markings
    for (let z = -1400; z <= 1400; z += 100) {
      const markingGeom = new T.BoxGeometry(10, 0.2, 30);
      const marking = new T.Mesh(markingGeom, markingMat);
      marking.position.set(3000, 0.5, z);
      scene.add(marking);
    }

    // Second runway
    const runway2 = new T.Mesh(runwayGeom, runwayMat);
    runway2.position.set(-10000, 0.3, 8000);
    runway2.rotation.y = Math.PI / 4;
    scene.add(runway2);

  }, []);

  // Create the airplane
  const createPlane = useCallback((scene) => {
    if (!THREE.current) return null;
    const T = THREE.current;

    const plane = new T.Group();

    // Fuselage (body)
    const fuselageMat = new T.MeshStandardMaterial({
      color: 0xf0f0f0,
      metalness: 0.7,
      roughness: 0.3,
    });
    const fuselageGeom = new T.CylinderGeometry(3, 2.5, 30, 12);
    const fuselage = new T.Mesh(fuselageGeom, fuselageMat);
    fuselage.rotation.x = Math.PI / 2;
    plane.add(fuselage);

    // Nose cone
    const noseMat = new T.MeshStandardMaterial({
      color: 0x333333,
      metalness: 0.8,
      roughness: 0.2,
    });
    const noseGeom = new T.ConeGeometry(2.5, 8, 12);
    const nose = new T.Mesh(noseGeom, noseMat);
    nose.rotation.x = -Math.PI / 2;
    nose.position.z = 19;
    plane.add(nose);

    // Cockpit (glass)
    const cockpitMat = new T.MeshStandardMaterial({
      color: 0x4488aa,
      metalness: 0.9,
      roughness: 0.1,
      transparent: true,
      opacity: 0.7,
    });
    const cockpitGeom = new T.SphereGeometry(2.5, 12, 8, 0, Math.PI * 2, 0, Math.PI / 2);
    const cockpit = new T.Mesh(cockpitGeom, cockpitMat);
    cockpit.position.set(0, 1.5, 10);
    cockpit.rotation.x = -Math.PI / 2;
    plane.add(cockpit);

    // Main wings
    const wingMat = new T.MeshStandardMaterial({
      color: 0xe8e8e8,
      metalness: 0.6,
      roughness: 0.4,
    });
    const wingGeom = new T.BoxGeometry(40, 0.5, 8);
    const wings = new T.Mesh(wingGeom, wingMat);
    wings.position.set(0, 0, -2);
    plane.add(wings);

    // Wing tips (angled up)
    const wingTipGeom = new T.BoxGeometry(3, 4, 4);
    [-20, 20].forEach(x => {
      const wingTip = new T.Mesh(wingTipGeom, wingMat);
      wingTip.position.set(x, 2, -2);
      wingTip.rotation.z = x > 0 ? 0.3 : -0.3;
      plane.add(wingTip);
    });

    // Horizontal stabilizer (tail wings)
    const hStabGeom = new T.BoxGeometry(15, 0.4, 4);
    const hStab = new T.Mesh(hStabGeom, wingMat);
    hStab.position.set(0, 0, -14);
    plane.add(hStab);

    // Vertical stabilizer (tail fin)
    const vStabGeom = new T.BoxGeometry(0.5, 8, 6);
    const vStab = new T.Mesh(vStabGeom, wingMat);
    vStab.position.set(0, 4, -14);
    plane.add(vStab);

    // Engines (under wings)
    const engineMat = new T.MeshStandardMaterial({
      color: 0x444444,
      metalness: 0.8,
      roughness: 0.3,
    });
    const engineGeom = new T.CylinderGeometry(1.5, 1.8, 8, 8);
    [-12, 12].forEach(x => {
      const engine = new T.Mesh(engineGeom, engineMat);
      engine.rotation.x = Math.PI / 2;
      engine.position.set(x, -2, 0);
      plane.add(engine);

      // Engine intake
      const intakeMat = new T.MeshBasicMaterial({ color: 0x222222 });
      const intakeGeom = new T.CircleGeometry(1.5, 8);
      const intake = new T.Mesh(intakeGeom, intakeMat);
      intake.position.set(x, -2, 4);
      plane.add(intake);

      // Engine glow (thrust)
      const thrustMat = new T.MeshBasicMaterial({ color: 0xff6600, transparent: true, opacity: 0.6 });
      const thrustGeom = new T.ConeGeometry(1.2, 4, 8);
      const thrust = new T.Mesh(thrustGeom, thrustMat);
      thrust.rotation.x = Math.PI / 2;
      thrust.position.set(x, -2, -6);
      thrust.name = 'thrust';
      plane.add(thrust);
    });

    // Landing gear (simplified)
    const gearMat = new T.MeshBasicMaterial({ color: 0x333333 });
    const wheelMat = new T.MeshBasicMaterial({ color: 0x111111 });

    // Front gear
    const frontStrut = new T.Mesh(new T.CylinderGeometry(0.3, 0.3, 4, 6), gearMat);
    frontStrut.position.set(0, -4, 8);
    plane.add(frontStrut);
    const frontWheel = new T.Mesh(new T.CylinderGeometry(1, 1, 0.5, 8), wheelMat);
    frontWheel.rotation.z = Math.PI / 2;
    frontWheel.position.set(0, -6, 8);
    plane.add(frontWheel);

    // Rear gear
    [-5, 5].forEach(x => {
      const rearStrut = new T.Mesh(new T.CylinderGeometry(0.4, 0.4, 5, 6), gearMat);
      rearStrut.position.set(x, -4.5, -4);
      plane.add(rearStrut);
      const rearWheel = new T.Mesh(new T.CylinderGeometry(1.2, 1.2, 0.8, 8), wheelMat);
      rearWheel.rotation.z = Math.PI / 2;
      rearWheel.position.set(x, -7, -4);
      plane.add(rearWheel);
    });

    // Navigation lights
    const redLight = new T.Mesh(
      new T.SphereGeometry(0.5, 6, 6),
      new T.MeshBasicMaterial({ color: 0xff0000 })
    );
    redLight.position.set(-20, 0, -2);
    plane.add(redLight);

    const greenLight = new T.Mesh(
      new T.SphereGeometry(0.5, 6, 6),
      new T.MeshBasicMaterial({ color: 0x00ff00 })
    );
    greenLight.position.set(20, 0, -2);
    plane.add(greenLight);

    const whiteLight = new T.Mesh(
      new T.SphereGeometry(0.5, 6, 6),
      new T.MeshBasicMaterial({ color: 0xffffff })
    );
    whiteLight.position.set(0, 0, -17);
    plane.add(whiteLight);

    // Initial position (in the air, ready to fly)
    plane.position.set(3000, 500, -1000);
    scene.add(plane);

    return plane;
  }, []);

  // Flight physics update
  const updateFlightPhysics = useCallback(() => {
    if (!planeRef.current || !physicsRef.current) return;

    const physics = physicsRef.current;
    const keys = keysRef.current;
    const touch = touchControlsRef.current;
    const plane = planeRef.current;

    // Physics constants
    const GRAVITY = 9.81;
    const AIR_DENSITY = 1.225;
    const WING_AREA = 50;
    const MASS = 5000;
    const MAX_THRUST = 80000;
    const DRAG_COEFFICIENT = 0.025;
    const LIFT_COEFFICIENT = 0.8;
    const STALL_ANGLE = 0.35; // ~20 degrees
    const MIN_FLIGHT_SPEED = 50;

    const dt = 0.016; // ~60fps

    // Throttle control - keyboard or touch
    if (keys['ShiftLeft'] || keys['ShiftRight'] || touch.throttleChange > 0.1) {
      const throttleRate = touch.throttleChange > 0.1 ? touch.throttleChange * 1.5 : 0.5;
      physics.throttle = Math.min(100, physics.throttle + throttleRate);
    }
    if (keys['ControlLeft'] || keys['ControlRight'] || touch.throttleChange < -0.1) {
      const throttleRate = touch.throttleChange < -0.1 ? Math.abs(touch.throttleChange) * 1.5 : 0.5;
      physics.throttle = Math.max(0, physics.throttle - throttleRate);
    }

    // Calculate thrust
    const thrust = (physics.throttle / 100) * MAX_THRUST;

    // Pitch control - keyboard or touch (W/S or ArrowUp/ArrowDown or touch.pitch)
    let pitchInput = ((keys['KeyS'] || keys['ArrowDown']) ? 1 : 0) - ((keys['KeyW'] || keys['ArrowUp']) ? 1 : 0);
    // Add touch pitch input (inverted: touch up = dive down, touch down = climb up)
    if (Math.abs(touch.pitch) > 0.1) {
      pitchInput = -touch.pitch; // Inverted for natural feel
    }
    const pitchRate = 0.025 * (physics.speed / 100); // More responsive at higher speeds
    physics.pitch += pitchInput * pitchRate;
    physics.pitch = Math.max(-Math.PI / 3, Math.min(Math.PI / 3, physics.pitch)); // Limit pitch

    // Roll control - keyboard or touch (A/D or ArrowLeft/ArrowRight or touch.roll)
    let rollInput = ((keys['KeyD'] || keys['ArrowRight']) ? 1 : 0) - ((keys['KeyA'] || keys['ArrowLeft']) ? 1 : 0);
    // Add touch roll input
    if (Math.abs(touch.roll) > 0.1) {
      rollInput = touch.roll;
    }
    const rollRate = 0.035;
    physics.roll += rollInput * rollRate;
    physics.roll = Math.max(-Math.PI / 2, Math.min(Math.PI / 2, physics.roll)); // Limit roll

    // Auto-level roll when no input
    if (!rollInput && Math.abs(touch.roll) < 0.1) {
      physics.roll *= 0.98;
    }

    // Yaw from roll (banking turns)
    const yawRate = Math.sin(physics.roll) * 0.015 * (physics.speed / 100);
    physics.yaw += yawRate;

    // Calculate speed from velocity
    physics.speed = Math.sqrt(
      physics.velocity.x ** 2 +
      physics.velocity.y ** 2 +
      physics.velocity.z ** 2
    );

    // Angle of attack
    const aoa = physics.pitch;
    const isStalled = Math.abs(aoa) > STALL_ANGLE && physics.speed < MIN_FLIGHT_SPEED * 2;

    // Lift calculation
    let lift = 0;
    if (physics.speed > 10) {
      const effectiveLift = isStalled ? LIFT_COEFFICIENT * 0.3 : LIFT_COEFFICIENT;
      lift = 0.5 * AIR_DENSITY * physics.speed ** 2 * WING_AREA * effectiveLift * Math.cos(aoa);
    }

    // Drag calculation
    const dragArea = WING_AREA * (1 + Math.abs(Math.sin(aoa)) * 2);
    const drag = 0.5 * AIR_DENSITY * physics.speed ** 2 * dragArea * DRAG_COEFFICIENT;

    // Weight
    const weight = MASS * GRAVITY;

    // Direction vectors
    const forward = {
      x: Math.sin(physics.yaw) * Math.cos(physics.pitch),
      y: Math.sin(physics.pitch),
      z: Math.cos(physics.yaw) * Math.cos(physics.pitch),
    };

    const up = {
      x: -Math.sin(physics.yaw) * Math.sin(physics.pitch) * Math.cos(physics.roll) - Math.cos(physics.yaw) * Math.sin(physics.roll),
      y: Math.cos(physics.pitch) * Math.cos(physics.roll),
      z: -Math.cos(physics.yaw) * Math.sin(physics.pitch) * Math.cos(physics.roll) + Math.sin(physics.yaw) * Math.sin(physics.roll),
    };

    // Apply forces
    // Thrust
    physics.velocity.x += (thrust * forward.x / MASS) * dt;
    physics.velocity.y += (thrust * forward.y / MASS) * dt;
    physics.velocity.z += (thrust * forward.z / MASS) * dt;

    // Lift
    physics.velocity.x += (lift * up.x / MASS) * dt;
    physics.velocity.y += (lift * up.y / MASS) * dt;
    physics.velocity.z += (lift * up.z / MASS) * dt;

    // Drag (opposes velocity)
    if (physics.speed > 0) {
      const dragForce = drag / MASS * dt;
      physics.velocity.x -= (physics.velocity.x / physics.speed) * dragForce;
      physics.velocity.y -= (physics.velocity.y / physics.speed) * dragForce;
      physics.velocity.z -= (physics.velocity.z / physics.speed) * dragForce;
    }

    // Gravity
    physics.velocity.y -= GRAVITY * dt;

    // Update position
    physics.position.x += physics.velocity.x * dt;
    physics.position.y += physics.velocity.y * dt;
    physics.position.z += physics.velocity.z * dt;

    // Ground collision
    const groundLevel = 10; // Minimum altitude
    if (physics.position.y < groundLevel) {
      physics.position.y = groundLevel;
      physics.velocity.y = Math.max(0, physics.velocity.y);

      // Ground friction when on ground
      if (physics.speed > 0) {
        physics.velocity.x *= 0.995;
        physics.velocity.z *= 0.995;
      }
    }

    // Update plane visual
    plane.position.set(physics.position.x, physics.position.y, physics.position.z);
    plane.rotation.set(physics.pitch, physics.yaw, physics.roll);

    // Update thrust visuals
    plane.children.forEach(child => {
      if (child.name === 'thrust') {
        child.scale.z = 0.5 + (physics.throttle / 100) * 1.5;
        child.material.opacity = 0.3 + (physics.throttle / 100) * 0.5;
      }
    });

    // Update HUD
    setSpeed(Math.round(physics.speed * 1.944)); // m/s to knots
    setAltitude(Math.round(physics.position.y * 3.281)); // meters to feet
    setHeading(Math.round(((physics.yaw * 180 / Math.PI) % 360 + 360) % 360));
    setPitch(Math.round(physics.pitch * 180 / Math.PI));
    setThrottle(Math.round(physics.throttle));
    setStalled(isStalled);

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

        // Initialize physics - start higher in the air with good speed
        physicsRef.current = {
          position: { x: 3000, y: 500, z: -1000 },
          velocity: { x: 0, y: 0, z: 80 }, // Good initial forward velocity
          pitch: 0,
          yaw: 0,
          roll: 0,
          throttle: 60,
          speed: 80,
        };

        // Scene
        const scene = new T.Scene();
        scene.background = new T.Color(0x87CEEB); // Sky blue
        scene.fog = new T.Fog(0x87CEEB, 5000, 40000);
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
        renderer.setClearColor(0x87CEEB, 1); // Sky blue background

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

        // Lighting
        const ambientLight = new T.AmbientLight(0xffffff, 0.6);
        scene.add(ambientLight);

        const sunLight = new T.DirectionalLight(0xfffacd, 1.0);
        sunLight.position.set(10000, 15000, 5000);
        scene.add(sunLight);

        // Hemisphere light for sky/ground
        const hemiLight = new T.HemisphereLight(0x87CEEB, 0x3d6b3d, 0.4);
        scene.add(hemiLight);

        // Create a simple test cube to verify rendering works
        const testCubeGeom = new T.BoxGeometry(100, 100, 100);
        const testCubeMat = new T.MeshBasicMaterial({ color: 0xff0000 });
        const testCube = new T.Mesh(testCubeGeom, testCubeMat);
        testCube.position.set(3000, 500, -900); // In front of plane starting position
        scene.add(testCube);
        console.log('FlyGame: Test cube added at', testCube.position);

        // Create terrain and plane
        console.log('FlyGame: Creating terrain...');
        createTerrain(scene);
        console.log('FlyGame: Creating plane...');
        const plane = createPlane(scene);
        planeRef.current = plane;
        console.log('FlyGame: Plane created at', plane.position);

        // Force multiple initial renders to ensure scene is visible
        renderer.render(scene, camera);
        requestAnimationFrame(() => renderer.render(scene, camera));
        console.log('FlyGame: Initial render complete, scene children:', scene.children.length);

        // Add clouds
        const cloudMat = new T.MeshBasicMaterial({
          color: 0xffffff,
          transparent: true,
          opacity: 0.8,
        });
        for (let i = 0; i < 100; i++) {
          const cloudGeom = new T.SphereGeometry(200 + Math.random() * 400, 8, 6);
          const cloud = new T.Mesh(cloudGeom, cloudMat);
          cloud.position.set(
            (Math.random() - 0.5) * 40000,
            1500 + Math.random() * 3000,
            (Math.random() - 0.5) * 40000
          );
          cloud.scale.set(2 + Math.random(), 0.5 + Math.random() * 0.5, 1 + Math.random());
          scene.add(cloud);
        }

        // Add sun
        const sunGeom = new T.SphereGeometry(500, 16, 16);
        const sunMat = new T.MeshBasicMaterial({ color: 0xffff88 });
        const sun = new T.Mesh(sunGeom, sunMat);
        sun.position.set(10000, 15000, 5000);
        scene.add(sun);

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
    };
  }, [createTerrain, createPlane, updateFlightPhysics]);

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

      {/* HUD - show when not loading and controls hint is dismissed */}
      {!isLoading && !showControls && (
      <div className="flight-hud">
        {/* Airspeed Indicator */}
        <div className="hud-panel airspeed-panel">
          <div className="panel-label">AIRSPEED</div>
          <div className="panel-value">{speed}</div>
          <div className="panel-unit">KTS</div>
        </div>

        {/* Altimeter */}
        <div className="hud-panel altitude-panel">
          <div className="panel-label">ALTITUDE</div>
          <div className="panel-value">{altitude.toLocaleString()}</div>
          <div className="panel-unit">FT</div>
        </div>

        {/* Heading */}
        <div className="hud-panel heading-panel">
          <div className="panel-label">HEADING</div>
          <div className="panel-value">{heading.toString().padStart(3, '0')}°</div>
        </div>

        {/* Throttle */}
        <div className="hud-panel throttle-panel">
          <div className="panel-label">THROTTLE</div>
          <div className="throttle-bar">
            <div className="throttle-fill" style={{ height: `${throttle}%` }}></div>
          </div>
          <div className="panel-value">{throttle}%</div>
        </div>

        {/* Pitch indicator */}
        <div className="hud-panel pitch-panel">
          <div className="panel-label">PITCH</div>
          <div className="panel-value">{pitch > 0 ? '+' : ''}{pitch}°</div>
        </div>

        {/* Stall warning */}
        {stalled && (
          <div className="stall-warning">
            <span>⚠️ STALL WARNING ⚠️</span>
          </div>
        )}
      </div>
      )}

      {/* Touch Controls - show when not loading and controls hint is dismissed */}
      {!isLoading && !showControls && isLandscape && (
      <div className="touch-controls">
        {/* Left side - Throttle control area */}
        <div
          className="throttle-area"
          onTouchStart={handleThrottleStart}
          onTouchMove={handleThrottleMove}
          onTouchEnd={handleThrottleEnd}
          onTouchCancel={handleThrottleEnd}
        >
          <div className="throttle-zone">
            <div className="throttle-indicator">
              <span className="throttle-icon">⬆</span>
              <span className="throttle-label">THROTTLE</span>
              <span className="throttle-icon">⬇</span>
            </div>
          </div>
        </div>

        {/* Right side - Flight stick area */}
        <div
          className="flight-stick-area"
          onTouchStart={handleFlightStickStart}
          onTouchMove={handleFlightStickMove}
          onTouchEnd={handleFlightStickEnd}
          onTouchCancel={handleFlightStickEnd}
        >
          <div className="flight-stick-zone">
            <div className="stick-crosshair">
              <div className="crosshair-v"></div>
              <div className="crosshair-h"></div>
              <div className="stick-center"></div>
            </div>
            <span className="stick-label">FLIGHT STICK</span>
          </div>
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
            <h3>✈️ Flight Controls</h3>
            <button
              className="takeoff-btn"
              onClick={(e) => { e.stopPropagation(); gameRunningRef.current = true; setShowControls(false); }}
              onTouchStart={(e) => { e.stopPropagation(); }}
              onTouchEnd={(e) => { e.preventDefault(); e.stopPropagation(); gameRunningRef.current = true; setShowControls(false); }}
              onPointerDown={(e) => { e.stopPropagation(); }}
              onPointerUp={(e) => { e.preventDefault(); e.stopPropagation(); gameRunningRef.current = true; setShowControls(false); }}
            >
              Ready for Takeoff!
            </button>
            <div className="controls-grid">
              <div className="control-item">
                <span className="key">📱 LEFT</span>
                <span>Swipe up/down for throttle</span>
              </div>
              <div className="control-item">
                <span className="key">📱 RIGHT</span>
                <span>Swipe to steer (flight stick)</span>
              </div>
              <div className="control-item">
                <span className="key">W/S ↑↓</span>
                <span>Pitch (Dive/Climb)</span>
              </div>
              <div className="control-item">
                <span className="key">A/D ←→</span>
                <span>Roll Left/Right</span>
              </div>
            </div>
            <p className="hint-tip">💡 Use the right side to steer like a joystick. Bank (roll) to turn smoothly!</p>
          </div>
        </div>
      )}
    </div>
  );
};

export default FlyGame;
