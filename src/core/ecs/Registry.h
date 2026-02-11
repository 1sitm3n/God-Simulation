#pragma once

#include "EntityID.h"
#include "core/util/Log.h"
#include "core/util/Assert.h"

#include <entt/entt.hpp>
#include <unordered_map>
#include <vector>

namespace godsim {

/// Registry wraps entt::registry with layer-aware entity IDs.
/// Each entity gets a stable EntityID that encodes its layer,
/// mapped to entt's internal recycling entity handles.
class Registry {
public:
    Registry() = default;

    // ─── Entity Lifecycle ───

    EntityID create_entity(LayerID layer) {
        auto entt_handle = registry_.create();
        u64 unique = next_id_++;
        EntityID eid = EntityID::create(layer, unique);

        id_to_entt_[eid] = entt_handle;
        entt_to_id_[entt_handle] = eid;

        return eid;
    }

    void destroy_entity(EntityID eid) {
        auto it = id_to_entt_.find(eid);
        if (it == id_to_entt_.end()) {
            LOG_WARN("Attempted to destroy non-existent entity {}", eid.value);
            return;
        }
        entt_to_id_.erase(it->second);
        registry_.destroy(it->second);
        id_to_entt_.erase(it);
    }

    bool is_alive(EntityID eid) const {
        return id_to_entt_.find(eid) != id_to_entt_.end();
    }

    // ─── Component Management ───

    template<typename T, typename... Args>
    T& add_component(EntityID eid, Args&&... args) {
        return registry_.emplace<T>(resolve(eid), std::forward<Args>(args)...);
    }

    template<typename T>
    T& get_component(EntityID eid) {
        return registry_.get<T>(resolve(eid));
    }

    template<typename T>
    const T& get_component(EntityID eid) const {
        return registry_.get<T>(resolve_const(eid));
    }

    template<typename T>
    bool has_component(EntityID eid) const {
        auto it = id_to_entt_.find(eid);
        if (it == id_to_entt_.end()) return false;
        return registry_.all_of<T>(it->second);
    }

    template<typename T>
    void remove_component(EntityID eid) {
        registry_.remove<T>(resolve(eid));
    }

    // ─── Iteration ───

    template<typename... Components, typename Func>
    void each(Func&& func) {
        auto view = registry_.view<Components...>();
        for (auto entt_handle : view) {
            auto it = entt_to_id_.find(entt_handle);
            if (it != entt_to_id_.end()) {
                func(it->second, view.template get<Components>(entt_handle)...);
            }
        }
    }

    template<typename... Components, typename Func>
    void each_in_layer(LayerID layer, Func&& func) {
        auto view = registry_.view<Components...>();
        for (auto entt_handle : view) {
            auto it = entt_to_id_.find(entt_handle);
            if (it != entt_to_id_.end() && it->second.layer() == layer) {
                func(it->second, view.template get<Components>(entt_handle)...);
            }
        }
    }

    std::vector<EntityID> entities_in_layer(LayerID layer) const {
        std::vector<EntityID> result;
        for (const auto& [eid, _] : id_to_entt_) {
            if (eid.layer() == layer) result.push_back(eid);
        }
        return result;
    }

    // ─── Statistics ───

    size_t entity_count() const { return id_to_entt_.size(); }

    size_t entity_count_in_layer(LayerID layer) const {
        size_t count = 0;
        for (const auto& [eid, _] : id_to_entt_) {
            if (eid.layer() == layer) count++;
        }
        return count;
    }

    entt::registry& raw() { return registry_; }
    const entt::registry& raw() const { return registry_; }

private:
    entt::entity resolve(EntityID eid) {
        auto it = id_to_entt_.find(eid);
        GODSIM_ASSERT(it != id_to_entt_.end(), "Entity {} does not exist", eid.value);
        return it->second;
    }

    entt::entity resolve_const(EntityID eid) const {
        auto it = id_to_entt_.find(eid);
        GODSIM_ASSERT(it != id_to_entt_.end(), "Entity {} does not exist", eid.value);
        return it->second;
    }

    entt::registry registry_;
    std::unordered_map<EntityID, entt::entity> id_to_entt_;
    std::unordered_map<entt::entity, EntityID> entt_to_id_;
    u64 next_id_ = 1;
};

} // namespace godsim
