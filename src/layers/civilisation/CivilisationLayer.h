#pragma once

#include "layers/Layer.h"

namespace godsim {

class CivilisationLayer : public Layer {
public:
    LayerID     id()   const override { return LayerID::Civilisation; }
    std::string name() const override { return "Civilisation"; }

    void initialise(Registry& registry, EventBus& bus, RNG& rng) override {
        registry_ = &registry;
        bus_ = &bus;
        rng_ = &rng;
        LOG_INFO("CivilisationLayer initialised");
    }

    void shutdown() override {
        LOG_INFO("CivilisationLayer shutdown (ticked {} times)", tick_count_);
    }

    void tick(SimTime current_time, SimTime delta_time) override {
        increment_tick();
        bus_->emit(
            LayerTickedEvent{LayerID::Civilisation, current_time, delta_time},
            current_time, ALL_LAYERS
        );
    }

    void serialise(BinaryWriter& writer) const override {
        writer.write_u64(tick_count_);
    }

    void deserialise(BinaryReader& reader) override {
        tick_count_ = reader.read_u64();
    }

private:
    Registry* registry_ = nullptr;
    EventBus* bus_ = nullptr;
    RNG* rng_ = nullptr;
};

} // namespace godsim
