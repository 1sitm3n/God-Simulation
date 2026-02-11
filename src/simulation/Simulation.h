#pragma once

#include "core/ecs/Registry.h"
#include "core/events/EventBus.h"
#include "core/time/TickScheduler.h"
#include "core/rng/RNG.h"
#include "core/serialise/BinaryStream.h"
#include "core/util/Log.h"
#include "layers/Layer.h"

#include <vector>
#include <memory>
#include <string>

namespace godsim {

/// The Simulation is the top-level orchestrator.
/// It owns the ECS registry, event bus, tick scheduler, RNG,
/// and all simulation layers.
class Simulation {
public:
    explicit Simulation(u64 seed = 42)
        : rng_(seed), event_bus_(), tick_scheduler_(event_bus_) {}

    // ─── Lifecycle ───

    /// Register a layer. Call before initialise().
    template<typename T, typename... Args>
    T* add_layer(Args&&... args) {
        auto layer = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = layer.get();
        layers_.push_back(std::move(layer));
        return ptr;
    }

    /// Initialise all layers and configure the tick scheduler.
    void initialise() {
        LOG_INFO("═══ God Simulation Initialising ═══");
        LOG_INFO("Seed: {}", rng_.seed());

        // Configure tick hierarchy
        tick_scheduler_.configure_defaults();

        // Initialise and register each layer
        for (auto& layer : layers_) {
            layer->initialise(registry_, event_bus_, rng_);
            tick_scheduler_.register_layer(layer.get());
            LOG_INFO("  Registered layer: {} (ID {})",
                     layer->name(), static_cast<int>(layer->id()));
        }

        LOG_INFO("Tick levels configured:");
        for (size_t i = 0; i < tick_scheduler_.levels().size(); i++) {
            const auto& level = tick_scheduler_.levels()[i];
            LOG_INFO("  [{}] {} = {} days (layer mask: 0x{:02X})",
                     i, level.name, level.duration.ticks, level.active_layers);
        }

        LOG_INFO("═══ Initialisation Complete ═══");
    }

    /// Shutdown all layers.
    void shutdown() {
        LOG_INFO("═══ Shutting Down ═══");
        for (auto& layer : layers_) {
            layer->shutdown();
        }
        LOG_INFO("Final time: {}", tick_scheduler_.current_time().to_string());
        LOG_INFO("Total events logged: {}", event_bus_.log().size());
        LOG_INFO("═══ Shutdown Complete ═══");
    }

    // ─── Simulation Control ───

    /// Advance by one tick at the active level.
    SimTime step() { return tick_scheduler_.step(); }

    /// Run N ticks at the active level.
    SimTime run(size_t num_ticks) { return tick_scheduler_.run(num_ticks); }

    // ─── Time Control ───
    void    pause()                  { tick_scheduler_.pause(); }
    void    resume()                 { tick_scheduler_.resume(); }
    bool    is_paused() const        { return tick_scheduler_.is_paused(); }
    void    set_speed(f32 mult)      { tick_scheduler_.set_speed(mult); }
    void    set_tick_level(size_t l) { tick_scheduler_.set_active_level(l); }
    SimTime current_time() const     { return tick_scheduler_.current_time(); }

    // ─── Snapshots ───

    void save_snapshot(const std::string& path) const {
        LOG_INFO("Saving snapshot to: {}", path);
        BinaryWriter writer;

        // Header
        writer.write_string("GODSIM");
        writer.write_u32(1); // version

        // Simulation state
        writer.write_i64(tick_scheduler_.current_time().ticks);
        writer.write_u64(rng_.seed());
        writer.write_u64(static_cast<u64>(tick_scheduler_.active_level()));

        // Layer states
        writer.write_u32(static_cast<u32>(layers_.size()));
        for (const auto& layer : layers_) {
            writer.write_u8(static_cast<u8>(layer->id()));
            layer->serialise(writer);
        }

        writer.save_to_file(path);
        LOG_INFO("Snapshot saved ({} bytes)", writer.buffer().size());
    }

    void load_snapshot(const std::string& path) {
        LOG_INFO("Loading snapshot from: {}", path);
        auto reader = BinaryReader::from_file(path);

        // Header
        auto magic = reader.read_string();
        GODSIM_ASSERT(magic == "GODSIM", "Invalid snapshot file");
        auto version = reader.read_u32();
        GODSIM_ASSERT(version == 1, "Unsupported snapshot version: {}", version);

        // Simulation state
        SimTime time = {reader.read_i64()};
        tick_scheduler_.set_time(time);
        // RNG seed (we keep the original — determinism from here)
        reader.read_u64();
        size_t active_level = static_cast<size_t>(reader.read_u64());
        tick_scheduler_.set_active_level(active_level);

        // Layer states
        u32 layer_count = reader.read_u32();
        for (u32 i = 0; i < layer_count; i++) {
            auto layer_id = static_cast<LayerID>(reader.read_u8());
            // Find the matching layer and deserialise
            for (auto& layer : layers_) {
                if (layer->id() == layer_id) {
                    layer->deserialise(reader);
                    break;
                }
            }
        }

        LOG_INFO("Snapshot loaded. Time: {}", time.to_string());
    }

    // ─── Access ───
    Registry&           registry()  { return registry_; }
    const EventBus&     event_bus() const { return event_bus_; }
    EventBus&           event_bus() { return event_bus_; }
    RNG&                rng()       { return rng_; }
    TickScheduler&      scheduler() { return tick_scheduler_; }

private:
    RNG            rng_;
    Registry       registry_;
    EventBus       event_bus_;
    TickScheduler  tick_scheduler_;

    std::vector<std::unique_ptr<Layer>> layers_;
};

} // namespace godsim
