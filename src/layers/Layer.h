#pragma once

#include "core/ecs/EntityID.h"
#include "core/ecs/Registry.h"
#include "core/events/EventBus.h"
#include "core/time/SimTime.h"
#include "core/rng/RNG.h"
#include "core/serialise/BinaryStream.h"

#include <string>

namespace godsim {

/// Abstract base class for all simulation layers.
/// Each layer owns its simulation logic and communicates
/// with other layers exclusively through the event bus.
class Layer {
public:
    virtual ~Layer() = default;

    // ─── Identity ───
    virtual LayerID     id()   const = 0;
    virtual std::string name() const = 0;

    // ─── Lifecycle ───
    /// Called once at startup. Store references to shared systems.
    virtual void initialise(Registry& registry, EventBus& bus, RNG& rng) = 0;

    /// Called once at shutdown.
    virtual void shutdown() = 0;

    // ─── Simulation ───
    /// Called by the tick scheduler at this layer's temporal resolution.
    virtual void tick(SimTime current_time, SimTime delta_time) = 0;

    // ─── Serialisation ───
    virtual void serialise(BinaryWriter& writer) const = 0;
    virtual void deserialise(BinaryReader& reader) = 0;

    // ─── Statistics ───
    u64 tick_count() const { return tick_count_; }

protected:
    /// Layers call this at the start of their tick() to track count.
    void increment_tick() { tick_count_++; }

    u64 tick_count_ = 0;
};

} // namespace godsim
