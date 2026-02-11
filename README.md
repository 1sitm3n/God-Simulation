# God Simulation

A god simulation where you shape a planet from barren rock to thriving civilisation. Create terrain, define climates, seed life, and watch evolution unfold across millions of years.

**Phase 0: Foundation** — Core engine systems (ECS, Event Bus, Tick Scheduler, RNG, Serialisation).

## Build

### Requirements
- CMake 3.20+
- C++20 compiler (GCC 12+, Clang 15+, or MSVC 2022+)
- Git (for dependency fetching)

### Build Commands

```bash
# Configure (dependencies are fetched automatically)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# Build
cmake --build build -j$(nproc)

# Run
./build/godsim

# Run tests
cd build && ctest --output-on-failure
```

### Build Types
- `Debug` — Sanitisers enabled (AddressSanitizer + UBSan), no optimisation
- `Release` — Full optimisation, no sanitisers
- `RelWithDebInfo` — Optimised with debug symbols

## Project Structure

```
src/
├── core/           # Engine foundation
│   ├── ecs/        # Entity-Component-System (entt wrapper)
│   ├── events/     # Event bus and logging
│   ├── time/       # Tick scheduler and SimTime
│   ├── rng/        # Deterministic random number generation
│   ├── serialise/  # Binary serialisation for snapshots
│   └── util/       # Logging, types, assertions
├── layers/         # Simulation layers
│   ├── cosmological/
│   ├── planetary/
│   ├── biological/
│   ├── civilisation/
│   └── divine/
└── simulation/     # Top-level simulation orchestrator
```

## Architecture

See `docs/` for the full architecture reference and game design document.

## Phase Roadmap

| Phase | Focus | Status |
|-------|-------|--------|
| 0 | Foundation (ECS, Events, Ticks, RNG) | **In Progress** |
| 1 | The Living Planet (Terrain, Climate, Rendering) | Planned |
| 2 | Life Finds a Way (Evolution, Ecosystems) | Planned |
| 3 | Civilisation Dawns (Societies, Culture, Conflict) | Planned |
| 4 | Divine Power (Player Interactions) | Planned |
| 5 | Time Mastery (Snapshots, Rewind, Branches) | Planned |
| 6 | Polish & Vertical Slice | Planned |
