# God Simulation — System Architecture

## 1. Architectural Overview

The game is structured as a **layered simulation stack** with an **event-driven message bus** connecting layers. Each layer runs its own simulation logic at its own temporal resolution, communicating changes upward and downward through events rather than direct coupling.

```
┌─────────────────────────────────────────────────┐
│              DIVINE INTERACTION LAYER            │  ← Player interface into all layers
│         (Avatar, Miracles, Prophecies)           │
├─────────────────────────────────────────────────┤
│                TIME CONTROL LAYER                │  ← Snapshot, rewind, branch, fast-forward
│          (Timeline Manager, State Store)          │
├─────────────────────────────────────────────────┤
│             CIVILISATION LAYER                   │  ← Societies, culture, religion, tech, war
│         (Agent-based, emergent history)           │
├─────────────────────────────────────────────────┤
│              BIOLOGICAL LAYER                    │  ← Species, evolution, ecosystems
│        (Trait system, population dynamics)        │
├─────────────────────────────────────────────────┤
│          PLANETARY / ENVIRONMENTAL LAYER         │  ← Terrain, climate, biomes, resources
│         (Voxel/heightmap, fluid sim lite)         │
├─────────────────────────────────────────────────┤
│             COSMOLOGICAL LAYER                   │  ← Stars, planets, physics laws, orbits
│           (N-body lite, rule engine)              │
├─────────────────────────────────────────────────┤
│              SIMULATION KERNEL                   │  ← Tick scheduler, ECS, event bus
│         (Core engine, memory management)          │
└─────────────────────────────────────────────────┘
```

### Core Design Principles

- **Layered Independence**: Each simulation layer owns its data and logic. Layers communicate through the event bus, never by reaching into another layer's internals.
- **Temporal Decoupling**: Layers tick at different rates. The cosmological layer might advance millions of years per tick during fast-forward, while the civilisation layer operates in seasons or years. A tick scheduler manages this.
- **Resolution on Demand**: The simulation only runs at full fidelity where the player is looking. Unobserved regions run simplified approximations. When the player zooms into a village, that region "hydrates" to full detail.
- **Deterministic Replay**: Given the same seed and inputs, the simulation produces identical results. This is essential for time rewind and branching. All RNG is seeded and tracked.

---

## 2. Simulation Kernel

The kernel is the bedrock — it owns the ECS, event bus, tick scheduler, and memory management. Everything else is built on top of it.

### 2.1 Entity-Component-System (ECS)

The ECS must handle entities that span wildly different scales: a galaxy, a planet, a mountain, a species, a civilisation, a single prophet. The key insight is that **components are the resolution mechanism** — an entity only has the components relevant to its current level of detail.

```
Entity: just an ID (u64)
  - High bits encode layer affinity (cosmological, planetary, biological, civilisation)
  - Low bits are unique within that layer
  - Allows O(1) layer filtering

Component Storage: Sparse-set archetype model
  - Components grouped by archetype for cache-friendly iteration
  - Each layer defines its own component types
  - Cross-layer references use EntityID, never pointers
```

#### Core Component Types by Layer

**Cosmological Components:**
```
PhysicsLaws        { gravity_constant, speed_of_light, element_table[], force_rules[] }
CelestialBody      { mass, radius, composition[], orbit_params }
StarProperties     { luminosity, spectral_class, lifecycle_stage, age }
OrbitalRelation    { parent_entity, semi_major_axis, eccentricity, inclination }
```

**Planetary Components:**
```
PlanetaryBody      { type(rocky/gas/ice), atmosphere_composition[], surface_temp_range }
TerrainData        { heightmap_ref, biome_map_ref, resource_map_ref }
ClimateSystem      { axial_tilt, ocean_coverage, greenhouse_factor, wind_patterns[] }
Hydrosphere        { ocean_level, river_network_ref, water_cycle_params }
```

**Biological Components:**
```
Species            { trait_genome[], population, habitat_biomes[], diet_type }
Ecosystem          { species_refs[], food_web, carrying_capacity, stability_index }
EvolutionState     { mutation_rate, selection_pressure, generation_count }
Organism           { species_ref, age, location, health }  // only when zoomed in
```

**Civilisation Components:**
```
Settlement         { population, location, buildings[], resources_stockpile }
Society            { government_type, laws[], cultural_values[], tech_level }
Religion           { beliefs[], sacred_sites[], prophets[], divine_laws[] }
Army               { size, equipment_level, morale, location, allegiance }
Character          { name, role, beliefs[], relationships[], influence }  // key figures only
```

**Divine Components (attached to player actions):**
```
DivineManifestation { type(avatar/miracle/prophet/voice), target_entity, duration }
Prophecy            { text, conditions_for_fulfillment[], known_by_entities[] }
DivineLaw           { commandment_text, enforcement_type, adoption_spread }
Blessing            { target, effect_type, magnitude, duration }
Curse               { target, effect_type, magnitude, duration }
```

### 2.2 Event Bus

The event bus is the nervous system of the simulation. Events flow between layers and are the primary mechanism for cross-layer communication.

```
Event {
    id:          u64
    timestamp:   SimTime        // simulation time when event occurred
    source:      EntityID       // who/what generated it
    target:      LayerMask      // which layers should receive it
    payload:     EventData      // variant/enum of all event types
    propagation: PropagationType // UP (child→parent layer), DOWN, BROADCAST, TARGETED
}
```

**Event Flow Examples:**

```
Star goes supernova (Cosmological)
  → EVENT: SupernovaOccurred { star_id, blast_radius, energy }
  → Planetary layer receives: destroys/reshapes nearby planets
  → Biological layer receives: mass extinction events on affected worlds
  → Civilisation layer receives: visible in sky, religious/cultural impact

Player creates a prophet (Divine)
  → EVENT: ProphetCreated { character_id, teachings[], powers[] }
  → Civilisation layer receives: new character with divine knowledge
  → Religion system updates: new religious movement begins spreading

Species develops intelligence (Biological)
  → EVENT: SapientSpeciesEmerged { species_id, planet_id, traits[] }
  → Civilisation layer receives: bootstraps a new primitive society
```

**Event Queue Architecture:**
```
Per-layer event queues (lock-free MPSC)
  - Each layer has an inbox
  - Events are routed by the kernel based on target LayerMask
  - Events are processed in timestamp order within each layer
  - Cross-layer events are buffered and delivered at sync points

Event Log (append-only):
  - Every event is logged for replay/rewind
  - Indexed by SimTime for efficient time-range queries
  - Compressed in chunks for long-running simulations
```

### 2.3 Tick Scheduler

Different layers need different temporal resolutions. The tick scheduler manages this via a hierarchical tick system:

```
Tick Hierarchy:
  COSMIC_TICK   = 1 million years  (star lifecycle, galaxy dynamics)
  GEOLOGICAL_TICK = 10,000 years   (terrain erosion, climate shifts)
  EVOLUTION_TICK  = 100 years      (species adaptation, speciation)
  HISTORY_TICK    = 1 year         (civilisation, politics, war)
  DETAIL_TICK     = 1 day          (individual characters, when zoomed in)

Fast-Forward Rates:
  The player controls a multiplier that determines how many ticks
  of each type execute per real-time second.

  "Watch evolution unfold" → run EVOLUTION_TICKs rapidly
  "Watch a civilization's golden age" → run HISTORY_TICKs
  "Follow the prophet's journey" → run DETAIL_TICKs
```

The scheduler runs **outer ticks first, inner ticks nested within**:
```
for each COSMIC_TICK:
    cosmological_layer.tick()
    for each GEOLOGICAL_TICK within this cosmic tick:
        planetary_layer.tick()
        for each EVOLUTION_TICK within this geological tick:
            biological_layer.tick()
            for each HISTORY_TICK within this evolution tick:
                civilisation_layer.tick()
                if player_is_zoomed_in:
                    for each DETAIL_TICK within this history tick:
                        detail_layer.tick()
    process_cross_layer_events()
```

---

## 3. Layer Architectures

### 3.1 Cosmological Layer

**Purpose:** Define and simulate the universe's structure and physical laws.

**Simulation Model:**
- Simplified N-body for star/planet orbital mechanics (not full gravity sim — use Keplerian orbits with perturbation corrections)
- Star lifecycle state machine: nebula → main sequence → red giant → white dwarf/neutron star/black hole
- Galaxy is procedurally generated from parameters (spiral arm count, density, age)
- Physical laws are **data-driven** — stored as components, not hardcoded. The player can tweak gravity, element properties, etc.

**Law Engine:**
```
PhysicsRuleSet {
    fundamental_forces: [
        Force { name: "gravity", strength: 6.674e-11, range: INFINITE, falloff: INVERSE_SQUARE },
        Force { name: "electromagnetism", ... },
        // Player can add custom forces
    ],
    element_table: [
        Element { atomic_number, stability, properties[], formation_conditions },
        // Player can create custom elements
    ],
    constants: HashMap<String, f64>,  // c, h, k_B, etc. All tweakable.
}
```

When the player modifies a law, the system propagates a `LawsChanged` event downward. Each layer has handlers that re-evaluate their state against the new rules. This is computationally expensive so it triggers a "reality cascade" — a progressive re-simulation from the point of change.

### 3.2 Planetary / Environmental Layer

**Purpose:** Simulate individual worlds — terrain, climate, weather, resources, and biomes.

**Terrain System:**
```
Multi-resolution heightmap with LOD:
  - Global: 512x512 grid per planet (coarse terrain, continent shapes)
  - Regional: 2048x2048 tiles that load when the player zooms in
  - Local: 8192x8192 for detailed areas (where civilisations exist)

Generation Pipeline:
  1. Tectonic plate simulation (simplified) → continent shapes
  2. Hydraulic erosion pass → rivers, valleys, coastlines
  3. Climate simulation → temperature/rainfall grids
  4. Biome assignment → based on temp, rainfall, altitude, latitude
  5. Resource placement → minerals, fertile land, fresh water
```

**Climate Model (simplified):**
```
Inputs: Star luminosity, orbital distance, axial tilt, atmosphere composition,
        ocean coverage, greenhouse gases, albedo

Simulation: Energy balance model per latitude band
  - Solar input varies by latitude and season
  - Greenhouse effect modifies re-radiation
  - Ocean currents redistribute heat (simplified conveyor model)
  - Output: temperature map, precipitation map, wind patterns

Runs every GEOLOGICAL_TICK for long-term climate
Interpolated for shorter timescales
```

**Geological Events:**
Volcanoes, earthquakes, meteor impacts — triggered stochastically based on tectonic activity and cosmic events. Each generates events consumed by biological and civilisation layers.

### 3.3 Biological Layer

**Purpose:** Simulate life — emergence, evolution, ecosystems, and extinction.

**Species as Trait Vectors:**

Rather than simulating individual organisms (computationally impossible at scale), species are modelled as **statistical populations with trait vectors.**

```
SpeciesGenome {
    // Each trait is a float [0.0, 1.0] representing a spectrum
    body_size: f32,          // tiny ↔ massive
    intelligence: f32,       // instinctual ↔ sapient
    speed: f32,              // sessile ↔ fast
    resilience: f32,         // fragile ↔ hardy
    reproduction_rate: f32,  // slow K-strategy ↔ fast r-strategy
    social_complexity: f32,  // solitary ↔ eusocial
    sensory_acuity: f32,     // blind ↔ keen senses
    diet_type: f32,          // herbivore ↔ carnivore (0.5 = omnivore)
    habitat_preference: BiomeSet,
    
    // Morphological traits (affect appearance when rendered)
    limb_count: u8,
    body_plan: BodyPlanType,  // vertebrate, arthropod, radial, amorphous, etc.
    surface_covering: SurfaceType,  // fur, scales, skin, chitin, etc.
    
    custom_traits: Vec<CustomTrait>,  // player-defined traits
}
```

**Evolution Mechanics:**
```
Each EVOLUTION_TICK:
  1. Calculate fitness of each species in its current environment
     fitness = f(traits, biome_conditions, predator/prey relationships, competition)
  
  2. Population dynamics (Lotka-Volterra inspired):
     - Species above carrying capacity decline
     - Well-adapted species grow
     - Predator-prey oscillations
  
  3. Mutation/Adaptation:
     - Small random perturbations to trait vectors
     - Magnitude proportional to environmental pressure
     - Isolated populations diverge faster (allopatric speciation)
  
  4. Speciation events:
     - If a population's trait variance exceeds threshold → split into two species
     - Geographic isolation accelerates this
  
  5. Extinction check:
     - Population below minimum viable → extinct
     - Environmental catastrophe → mass extinction event
  
  6. Intelligence threshold:
     - When intelligence + social_complexity exceed threshold → emit SapientSpeciesEmerged event
     - Civilisation layer bootstraps
```

**Ecosystem Web:**
```
EcosystemGraph {
    nodes: Vec<SpeciesID>,
    edges: Vec<Relationship>,  // predation, competition, symbiosis, parasitism
    energy_flow: f32,          // total energy throughput (from plants/producers)
    stability: f32,            // how resilient to perturbation
    biodiversity_index: f32,
}
```

### 3.4 Civilisation Layer

**Purpose:** Simulate societies, culture, technology, religion, politics, and conflict.

This is the densest layer. It uses an **agent-based model** where agents are settlements, states, and key characters — not individual citizens (except when the player zooms in to individual-level detail).

**Settlement Agent:**
```
Settlement {
    population: u32,
    location: WorldCoord,
    resources: ResourceInventory,
    buildings: Vec<BuildingType>,
    culture: CultureProfile,
    religion: Option<ReligionRef>,
    tech_level: TechTree,
    happiness: f32,
    military_strength: f32,
    trade_routes: Vec<TradeRoute>,
    governance: GovernanceType,
}
```

**Culture System:**
```
CultureProfile {
    values: HashMap<CulturalValue, f32>,  // e.g., militarism: 0.8, piety: 0.6
    traditions: Vec<Tradition>,
    language_family: LanguageGroup,
    art_style: ArtStyleParams,            // for procedural generation of cultural artifacts
    taboos: Vec<Taboo>,
    
    // Cultures evolve over time and spread through contact
    drift_rate: f32,          // how fast values shift
    isolation_factor: f32,    // geographic isolation amplifies drift
}
```

**Religion System (crucial for god gameplay):**
```
Religion {
    name: String,
    founder: Option<EntityID>,            // a prophet or divine avatar
    core_beliefs: Vec<Belief>,
    divine_laws: Vec<DivineLaw>,          // player-created commandments
    prophecies: Vec<Prophecy>,
    sacred_sites: Vec<WorldCoord>,
    scripture: Vec<ScriptureEntry>,       // accumulated teachings/events
    
    followers: u64,
    schisms: Vec<SchismEvent>,            // when beliefs diverge
    reformation_pressure: f32,
    
    // How the religion views the player-god
    deity_perception: DeityPerception {
        name: String,                     // what followers call the god
        benevolence_rating: f32,          // based on player's actions
        fear_rating: f32,
        attributes: Vec<DivineAttribute>, // inferred from player behaviour
    }
}
```

**Technology Tree:**

Not a fixed tree — a **directed graph** that emerges from conditions:
```
TechDiscovery {
    name: String,
    prerequisites: Vec<TechID>,
    resource_requirements: Vec<ResourceReq>,
    intelligence_threshold: f32,         // species must be smart enough
    cultural_prerequisites: Vec<CulturalValue>,  // e.g., "curiosity" value must be high
    environmental_triggers: Vec<EnvCondition>,   // e.g., bronze age needs tin + copper
    discovery_probability: f32,          // per HISTORY_TICK, given prerequisites met
}
```

**Conflict System:**
```
War {
    belligerents: [Faction; 2+],
    cause: WarCause,                     // territorial, religious, resource, ideological
    battles: Vec<BattleOutcome>,
    attrition: f32,
    war_weariness: HashMap<Faction, f32>,
    
    // Resolved via simplified force comparison with random elements
    // Terrain, tech level, morale, leadership all factor in
}
```

**History Recording:**
Every significant event is logged into a persistent `WorldHistory` structure:
```
HistoryEvent {
    time: SimTime,
    event_type: HistoryEventType,
    participants: Vec<EntityID>,
    location: WorldCoord,
    description: String,           // procedurally generated
    significance: f32,             // determines if it's remembered by civilisations
    divine_involvement: bool,      // was the player involved?
}
```

This history feeds back into civilisation behaviour — societies remember past wars, prophets, miracles, and catastrophes, and these memories shape their decisions.

### 3.5 Divine Interaction Layer

**Purpose:** The player's interface for interacting with the simulation at any scale.

**Interaction Modes:**

```
enum DivineAction {
    // Cosmological scale
    CreateStar { position, mass, composition },
    DestroyStar { target },
    ModifyPhysicsLaw { law_id, new_value },
    CreatePlanet { orbit_parent, params },
    
    // Planetary scale
    RaiseTerrain { region, amount },
    CreateOcean { region },
    TriggerVolcano { location },
    ShiftClimate { region, temperature_delta },
    PlaceResource { location, resource_type, quantity },
    
    // Biological scale
    CreateSpecies { genome, location },
    ModifySpecies { species_id, trait_changes },
    TriggerMassExtinction { region, severity },
    AccelerateEvolution { species_id, direction },
    
    // Civilisation scale — Overt
    Smite { target_settlement, severity },
    Bless { target_settlement, bonus_type },
    DeliverCommandment { text, target_religion },
    PerformMiracle { type, location, witnesses },
    CreateDivineArtifact { properties, location },
    
    // Civilisation scale — Subtle
    SpawnProphet { character_params, mission },
    InspireDream { target_character, content },
    ManipulateWeather { region, effect },     // appears natural
    InfluenceDecision { character, nudge },    // subtle persuasion
    PlantIdea { settlement, concept },         // accelerate a discovery
    
    // Avatar Mode
    ManifestAvatar { appearance, powers, location },
    AvatarSpeak { dialogue },
    AvatarAct { action },
    DismissAvatar {},
    
    // Time Control
    SetSimSpeed { ticks_per_second },
    Pause {},
    CreateSnapshot { label },
    RewindTo { sim_time },
    BranchTimeline { from_snapshot },
    SwitchTimeline { timeline_id },
    
    // Meta / Creation
    CreateCustomTrait { definition },
    CreateCustomElement { properties },
    CreateCustomForce { parameters },
    DefineNewBiome { conditions },
}
```

**Observation System:**

The player needs to observe the simulation at any scale. This requires a **semantic zoom** system:

```
ZoomLevel {
    UNIVERSE,      // see galaxies as points of light
    GALAXY,        // see star systems
    SYSTEM,        // see planets orbiting a star
    PLANET,        // see the globe, continents, weather patterns
    REGION,        // see terrain, cities as clusters, armies moving
    SETTLEMENT,    // see individual buildings, streets, crowds
    INDIVIDUAL,    // see and interact with single characters
}

At each zoom level, different data is presented:
  UNIVERSE  → galaxy positions, ages, sizes
  GALAXY    → star map, notable systems highlighted
  SYSTEM    → planets, moons, orbital paths, habitable zone
  PLANET    → terrain, biomes, civilisation borders, resource overlay
  REGION    → settlements, roads, armies, terrain detail
  SETTLEMENT → buildings, population activity, culture visible
  INDIVIDUAL → character stats, beliefs, relationships, dialogue
```

**Divine Presence System:**

The player's past actions create a "divine presence" that civilisations detect and react to:
```
DivinePresence {
    overt_intervention_count: u32,    // miracles, smites, etc.
    subtle_intervention_count: u32,    // dreams, weather, nudges
    consistency: f32,                  // do your actions follow a pattern?
    benevolence_score: f32,            // helpful vs harmful ratio
    activity_regions: Vec<Region>,     // where you've been active
    
    // Civilisations with higher piety notice more
    // Over-intervention breeds dependency or rebellion
    // Under-intervention leads to atheism or false religions
}
```

---

## 4. Time Control System

This is one of the most technically demanding systems. It needs to support pause, fast-forward at variable rates, snapshot, rewind, and timeline branching.

### 4.1 State Serialization

Every piece of simulation state must be serializable. The ECS architecture helps enormously here — component data is already in flat, cache-friendly arrays.

```
SimulationSnapshot {
    sim_time: SimTime,
    rng_state: RngState,              // for deterministic replay
    layer_states: [
        CosmologicalState,
        PlanetaryState,
        BiologicalState,
        CivilisationState,
    ],
    event_log_position: u64,          // index into the event log
    metadata: SnapshotMetadata,
}
```

### 4.2 Snapshot Strategy

Full snapshots are expensive. Use a **delta-snapshot** approach:

```
Snapshot Types:
  FULL    — complete simulation state (taken every N million sim-ticks, or manually)
  DELTA   — only changed components since last snapshot (taken frequently)
  
Rewind Process:
  1. Find nearest FULL snapshot before target time
  2. Load it
  3. Apply DELTAs forward to reach exact target time
  OR
  3. Re-simulate from the FULL snapshot (if deltas insufficient)
  
Storage:
  Snapshots compressed with LZ4 (fast decompression)
  Old deltas pruned when no longer needed
  Player-created snapshots are always FULL and never pruned
```

### 4.3 Timeline Branching

```
Timeline {
    id: TimelineID,
    parent: Option<TimelineID>,
    branch_point: SimTime,             // when this timeline diverged
    snapshot_at_branch: SnapshotRef,   // the state when it branched
    event_log: EventLog,              // this timeline's events from branch point onward
    current_time: SimTime,
}

TimelineManager {
    timelines: HashMap<TimelineID, Timeline>,
    active_timeline: TimelineID,
    
    fn branch(label: String) → TimelineID {
        // Take full snapshot
        // Create new timeline pointing to current state
        // Both timelines can now diverge independently
    }
    
    fn switch(target: TimelineID) {
        // Save current timeline state
        // Load target timeline's latest state
        // Resume simulation in that timeline
    }
}
```

The key constraint: **only one timeline is actively simulated at a time.** Inactive timelines are frozen at their last state. The player can switch between them, but parallel simulation of multiple timelines is not feasible.

---

## 5. Data Flow Architecture

### 5.1 Downward Cascades (Laws → Reality)

When the player modifies something at a higher layer, effects cascade downward:

```
Player changes gravity constant
  → Cosmological: orbital mechanics recalculated, some planets destabilized
    → Planetary: surface gravity changes, atmosphere retention changes
      → Biological: species under new selection pressures, body plans shift
        → Civilisation: architecture/infrastructure affected, myths about the ground shaking

This is a "reality cascade" and it's expensive.
Optimization: cascade only within the player's view + one ring of neighbors.
Far-away regions get a "deferred cascade" flag and update when observed.
```

### 5.2 Upward Emergence (Matter → Meaning)

Lower layers produce emergent phenomena that trigger higher-layer events:

```
Planetary layer: a river changes course after erosion
  → Biological: new habitat corridor opens, species migrate
    → Civilisation: trade route disrupted, border dispute triggered
      → History: "The Great Divergence" event logged

No layer explicitly plans this chain — it emerges from events bubbling upward.
```

### 5.3 The Observation Pipeline

Rendering at different zoom levels requires an observation pipeline that aggregates data:

```
Observation Pipeline:
  1. Player camera determines current ZoomLevel + focus area
  2. Relevant layers activate their detail systems for that area
  3. Data is aggregated into a ViewState for the renderer
  4. Overlays (political borders, resource heat maps, divine influence)
     are composited based on player's selected filters

Performance Rule:
  Only the focused area runs at full detail.
  Everything else runs at minimum viable fidelity.
  "Hydration" occurs when the player moves their focus.
```

---

## 6. Memory & Performance Architecture

### 6.1 Level of Detail for Simulation (Sim-LOD)

Not just visual LOD — the simulation itself runs at different fidelities:

```
SimLOD Levels:
  FULL      — individual agents, detailed ecosystems, character-level events
  REGIONAL  — settlement-level aggregates, species populations, regional climate
  PLANETARY — continental summaries, global species stats, empire-level politics
  DORMANT   — frozen state, only updated on observation or cascade events

Assignment:
  Player's focus area      → FULL
  Adjacent regions          → REGIONAL
  Same planet, far away    → PLANETARY
  Other planets            → DORMANT (unless something dramatic happens)
```

### 6.2 Memory Budget

```
Estimated memory per layer (active planet):
  Terrain heightmap (multi-res)    ~64 MB
  Climate data                     ~8 MB
  Ecosystem data (per biome)       ~2 MB × biome count
  Civilisation data (per society)  ~1 MB × society count
  History log                      ~grows over time, compressed chunks
  Snapshots                        ~varies, LZ4 compressed

Target: Keep active simulation under 2 GB
  Use streaming for terrain tiles
  Compress dormant planet data
  Prune low-significance history events
```

### 6.3 Parallelism Strategy

```
Thread Architecture:
  Main Thread:       Tick scheduler, event routing, player input
  Sim Thread Pool:   Layer tick execution (layers can run in parallel
                     between sync points)
  Render Thread:     Vulkan rendering (you know this well)
  IO Thread:         Snapshot save/load, streaming terrain data

Parallelism within layers:
  Planetary:  tile-based parallel terrain/climate updates (OpenMP)
  Biological: per-ecosystem parallel evolution ticks
  Civilisation: per-settlement parallel agent updates
  
Sync Points:
  After each tick level completes, cross-layer events are processed.
  This is the synchronization barrier.
```

---

## 7. Data Storage Schema

### 7.1 On-Disk Format

```
save_game/
├── meta.json                    # save metadata, game version, sim time
├── universe.bin                 # cosmological state (compressed)
├── timelines/
│   ├── main/
│   │   ├── event_log.bin        # append-only event history
│   │   └── snapshots/
│   │       ├── full_0001.snap   # full snapshots (LZ4)
│   │       ├── delta_0002.snap
│   │       └── ...
│   └── branch_001/
│       └── ...
├── planets/
│   ├── planet_001/
│   │   ├── terrain/             # tiled heightmaps
│   │   ├── climate.bin
│   │   ├── ecosystems.bin
│   │   └── civilisations/
│   │       ├── societies.bin
│   │       ├── religions.bin
│   │       └── history.bin
│   └── ...
└── divine/
    ├── actions_log.bin          # player's divine action history
    ├── prophecies.bin
    └── avatars.bin
```

### 7.2 Streaming Architecture

Planets and regions load on demand. The universe state is always in memory (it's small), but planetary detail streams in as the player navigates:

```
StreamingManager {
    loaded_planets: LruCache<PlanetID, PlanetState>,
    loaded_regions: LruCache<RegionID, RegionDetail>,
    prefetch_queue: PriorityQueue<LoadRequest>,
    
    // Prefetch based on player camera trajectory
    // Unload distant regions when memory budget exceeded
    // Always keep current focus + neighbors loaded
}
```

---

## 8. Extension Points & Modding Architecture

Given the player should be able to create "new beings and things without any rules," the system needs robust extensibility:

```
Custom Definitions (player-created, serialized as data):
  - Custom species traits
  - Custom physics laws / forces
  - Custom elements
  - Custom biomes
  - Custom building types
  - Custom tech discoveries
  - Custom cultural values
  - Custom divine powers

Registry Pattern:
  Each "type" of thing lives in a registry.
  Built-in types are loaded first, then player-created types are added.
  Everything references types by ID, so custom types integrate seamlessly.

  TypeRegistry<T> {
      builtin: HashMap<TypeID, T>,
      custom: HashMap<TypeID, T>,
      
      fn get(id: TypeID) -> &T  // checks custom first, then builtin
      fn register(definition: T) -> TypeID
  }
```

---

## 9. Suggested Technology Stack

Given your existing Vulkan/C++ expertise with Mythbreaker:

```
Core Engine:        C++ (you already have Mythbreaker's foundation)
ECS:                Custom or entt (header-only, very fast)
Serialization:      FlatBuffers (zero-copy, good for snapshots)
Compression:        LZ4 (snapshot compression, fast decompress)
Parallelism:        OpenMP + custom thread pool (you've done this)
Scripting:          Lua (for modding, event handlers, AI behaviours)
Terrain:            Custom heightmap system (leverage your procedural terrain work)
RNG:                PCG family (deterministic, splittable for parallel use)
Rendering:          Vulkan (Mythbreaker's renderer, adapted for multi-scale)
UI:                 Dear ImGui for debug/dev, custom UI for release
Audio:              FMOD or custom
Networking:         None initially (single-player)
```

---

## 10. Critical Risk Areas

| Risk | Impact | Mitigation |
|------|--------|------------|
| Simulation too slow at scale | Game-breaking | Aggressive Sim-LOD, dormant planets, approximate models |
| Time rewind causes desyncs | Corrupts saves | Deterministic RNG, snapshot validation, replay testing |
| Evolution produces boring results | Unengaging | Curated trait interactions, dramatic event injection, player nudges |
| Scope creep across layers | Never ships | Vertical slice approach — get one planet working end-to-end first |
| Memory explosion with history | OOM crashes | Significance-based pruning, compressed chunk storage, configurable history depth |
| Scale transitions feel jarring | Poor UX | Semantic zoom with smooth data transitions, consistent visual language |
