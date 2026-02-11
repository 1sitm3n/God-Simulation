#pragma once

#include "core/util/Types.h"
#include <functional>
#include <string>

namespace godsim {

// ─── Simulation Layer Identifiers ───
enum class LayerID : u8 {
    Cosmological = 0,
    Planetary    = 1,
    Biological   = 2,
    Civilisation = 3,
    Divine       = 4,
    COUNT        = 5
};

inline std::string layer_name(LayerID layer) {
    switch (layer) {
        case LayerID::Cosmological: return "Cosmological";
        case LayerID::Planetary:    return "Planetary";
        case LayerID::Biological:   return "Biological";
        case LayerID::Civilisation: return "Civilisation";
        case LayerID::Divine:       return "Divine";
        default:                    return "Unknown";
    }
}

// ─── Entity ID ───
// Layout: [8-bit layer] [56-bit unique ID]
// This gives us 72 quadrillion unique entities per layer — more than enough.
struct EntityID {
    u64 value = 0;

    // Extract the layer this entity belongs to
    LayerID layer() const {
        return static_cast<LayerID>(value >> 56);
    }

    // Extract the unique ID within the layer
    u64 id() const {
        return value & 0x00FFFFFFFFFFFFFF;
    }

    // Create an EntityID from layer + unique ID
    static EntityID create(LayerID layer, u64 unique_id) {
        return {(static_cast<u64>(layer) << 56) | (unique_id & 0x00FFFFFFFFFFFFFF)};
    }

    // Null/invalid entity
    static EntityID null() { return {0}; }
    bool is_valid() const { return value != 0; }

    bool operator==(const EntityID& other) const { return value == other.value; }
    bool operator!=(const EntityID& other) const { return value != other.value; }
    bool operator<(const EntityID& other) const { return value < other.value; }
};

} // namespace godsim

// Hash support for use in containers
template<>
struct std::hash<godsim::EntityID> {
    std::size_t operator()(const godsim::EntityID& eid) const noexcept {
        return std::hash<godsim::u64>{}(eid.value);
    }
};
