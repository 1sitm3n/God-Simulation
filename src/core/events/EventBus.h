#pragma once

#include "core/ecs/EntityID.h"
#include "core/time/SimTime.h"
#include "core/util/Types.h"
#include "core/util/Log.h"

#include <variant>
#include <vector>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <algorithm>
#include <string>
#include <mutex>

namespace godsim {

// ─── Layer Mask (bitmask for targeting layers) ───
using LayerMask = u8;
constexpr LayerMask LAYER_BIT(LayerID layer) { return 1 << static_cast<u8>(layer); }
constexpr LayerMask ALL_LAYERS = 0x1F; // bits 0-4

enum class Propagation {
    Up,        // Child → parent layer
    Down,      // Parent → child layer
    Broadcast, // All layers
    Targeted   // Specific layers via mask
};

// ─── Event Payload Types ───
// These grow as layers are implemented. Phase 0 only needs debug/lifecycle events.

struct DebugEvent {
    std::string message;
};

struct LayerTickedEvent {
    LayerID layer;
    SimTime time;
    SimTime delta;
};

struct EntityCreatedEvent {
    EntityID entity;
    LayerID  layer;
};

struct EntityDestroyedEvent {
    EntityID entity;
    LayerID  layer;
};

// The variant of all possible event payloads.
// New event types are added here as layers are built.
using EventPayload = std::variant<
    DebugEvent,
    LayerTickedEvent,
    EntityCreatedEvent,
    EntityDestroyedEvent
>;

// ─── Event ───
struct Event {
    u64          id         = 0;
    SimTime      timestamp  = {};
    EntityID     source     = EntityID::null();
    LayerMask    target     = ALL_LAYERS;
    Propagation  propagation = Propagation::Broadcast;
    EventPayload payload;
};

// ─── Event Log ───
// Append-only log of all events for replay and history.
class EventLog {
public:
    void append(const Event& event) {
        events_.push_back(event);
    }

    std::vector<const Event*> query(SimTime from, SimTime to) const {
        std::vector<const Event*> result;
        for (const auto& e : events_) {
            if (e.timestamp >= from && e.timestamp <= to) {
                result.push_back(&e);
            }
        }
        return result;
    }

    void truncate_after(SimTime time) {
        events_.erase(
            std::remove_if(events_.begin(), events_.end(),
                           [time](const Event& e) { return e.timestamp > time; }),
            events_.end());
    }

    size_t size() const { return events_.size(); }
    const std::vector<Event>& events() const { return events_; }
    void clear() { events_.clear(); }

private:
    std::vector<Event> events_;
};

// ─── Event Bus ───
// Routes events from emitters to layer-specific handlers.
class EventBus {
public:
    /// Emit an event. It will be delivered on the next dispatch() call.
    void emit(Event event) {
        event.id = next_event_id_++;
        pending_.push_back(std::move(event));
    }

    /// Convenience: emit with just a payload, timestamp, and target.
    template<typename T>
    void emit(T payload, SimTime timestamp, LayerMask target = ALL_LAYERS,
              Propagation prop = Propagation::Broadcast, EntityID source = EntityID::null()) {
        Event e;
        e.id = next_event_id_++;
        e.timestamp = timestamp;
        e.source = source;
        e.target = target;
        e.propagation = prop;
        e.payload = std::move(payload);
        pending_.push_back(std::move(e));
    }

    /// Subscribe a handler for a specific event payload type.
    /// The handler is called only if the event targets the given layer.
    template<typename T>
    void subscribe(LayerID layer, std::function<void(const Event&, const T&)> handler) {
        auto type_idx = std::type_index(typeid(T));
        handlers_[type_idx].push_back({
            layer,
            [handler = std::move(handler)](const Event& event) {
                handler(event, std::get<T>(event.payload));
            }
        });
    }

    /// Deliver all pending events to their subscribed handlers
    /// and log them to the event log.
    void dispatch() {
        // Sort pending events by timestamp for ordering guarantee
        std::sort(pending_.begin(), pending_.end(),
                  [](const Event& a, const Event& b) { return a.timestamp < b.timestamp; });

        for (const auto& event : pending_) {
            // Log the event
            log_.append(event);

            // Find the type index of the payload
            std::type_index type_idx = std::visit(
                [](const auto& payload) { return std::type_index(typeid(payload)); },
                event.payload);

            // Dispatch to matching handlers
            auto it = handlers_.find(type_idx);
            if (it != handlers_.end()) {
                for (const auto& entry : it->second) {
                    // Check if this handler's layer is in the event's target mask
                    if (event.target & LAYER_BIT(entry.layer)) {
                        entry.handler(event);
                    }
                }
            }
        }
        pending_.clear();
    }

    /// Get the event log.
    const EventLog& log() const { return log_; }
    EventLog& log() { return log_; }

    /// Number of pending (undispatched) events.
    size_t pending_count() const { return pending_.size(); }

    /// Reset everything.
    void clear() {
        pending_.clear();
        log_.clear();
        handlers_.clear();
        next_event_id_ = 0;
    }

private:
    struct HandlerEntry {
        LayerID layer;
        std::function<void(const Event&)> handler;
    };

    std::vector<Event> pending_;
    EventLog log_;
    std::unordered_map<std::type_index, std::vector<HandlerEntry>> handlers_;
    u64 next_event_id_ = 0;
};

} // namespace godsim
