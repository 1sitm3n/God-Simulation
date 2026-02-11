#pragma once

#include "core/time/SimTime.h"
#include "core/ecs/EntityID.h"
#include "core/events/EventBus.h"
#include "core/util/Log.h"
#include "layers/Layer.h"

#include <vector>
#include <string>
#include <functional>

namespace godsim {

/// Defines a single tick level in the hierarchy.
struct TickLevel {
    std::string name;          // e.g. "cosmic", "geological"
    SimTime     duration;      // How many days one tick of this level represents
    LayerMask   active_layers; // Which layers tick at this level
};

/// The TickScheduler manages hierarchical simulation ticking.
/// Different layers tick at different temporal resolutions.
/// The player's time controls determine which tick level is active
/// and how fast the simulation runs.
class TickScheduler {
public:
    explicit TickScheduler(EventBus& bus) : event_bus_(bus) {}

    // ─── Configuration ───

    void add_level(TickLevel level) {
        levels_.push_back(std::move(level));
    }

    void register_layer(Layer* layer) {
        layers_.push_back(layer);
    }

    /// Set up the default tick hierarchy for the god simulation.
    void configure_defaults() {
        // Level 0: Detail    - 1 day         - all layers
        add_level({"detail",    SimTime::from_days(1),          ALL_LAYERS});
        // Level 1: History   - 1 year        - all except divine (divine reacts to events)
        add_level({"history",   SimTime::from_years(1),         ALL_LAYERS});
        // Level 2: Evolution - 100 years      - cosmological, planetary, biological
        add_level({"evolution", SimTime::from_years(100),       0x07}); // bits 0,1,2
        // Level 3: Geological - 10,000 years  - cosmological, planetary
        add_level({"geological",SimTime::from_kiloyears(10),    0x03}); // bits 0,1
        // Level 4: Cosmic    - 1,000,000 years - cosmological only
        add_level({"cosmic",    SimTime::from_megayears(1),     0x01}); // bit 0
    }

    // ─── Execution ───

    /// Advance the simulation by one tick at the active level.
    /// Returns the new simulation time.
    SimTime step() {
        if (paused_ || levels_.empty()) return current_time_;

        const auto& level = levels_[active_level_];
        SimTime delta = level.duration;

        // Tick each registered layer if it's active at this level
        for (auto* layer : layers_) {
            u8 layer_bit = LAYER_BIT(layer->id());
            if (level.active_layers & layer_bit) {
                layer->tick(current_time_, delta);
            }
        }

        // Advance time
        current_time_ += delta;

        // Emit a tick event for the event log
        event_bus_.emit(
            LayerTickedEvent{LayerID::COUNT, current_time_, delta},
            current_time_
        );

        // Dispatch all events generated during this tick
        event_bus_.dispatch();

        return current_time_;
    }

    // ─── Time Control ───

    void pause()  { paused_ = true; }
    void resume() { paused_ = false; }
    bool is_paused() const { return paused_; }

    void set_speed(f32 multiplier) { speed_ = multiplier; }
    f32  speed() const { return speed_; }

    SimTime current_time() const { return current_time_; }
    void    set_time(SimTime t)  { current_time_ = t; }

    void   set_active_level(size_t idx) {
        if (idx < levels_.size()) active_level_ = idx;
    }
    size_t active_level() const { return active_level_; }

    const std::vector<TickLevel>& levels() const { return levels_; }

    /// Run N ticks at the active level.
    SimTime run(size_t num_ticks) {
        bool was_paused = paused_;
        paused_ = false;
        for (size_t i = 0; i < num_ticks; i++) {
            step();
        }
        paused_ = was_paused;
        return current_time_;
    }

private:
    EventBus& event_bus_;
    std::vector<TickLevel> levels_;
    std::vector<Layer*> layers_;
    SimTime current_time_ = {};
    size_t active_level_ = 0;
    f32 speed_ = 1.0f;
    bool paused_ = true;
};

} // namespace godsim
