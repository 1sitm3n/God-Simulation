#pragma once

#include "core/util/Types.h"
#include <string>
#include <cmath>

namespace godsim {

/// SimTime represents simulation time as an integer number of days.
/// This gives us a unified timeline across all layers, with helpers
/// to convert to coarser units (years, kiloyears, megayears).
struct SimTime {
    i64 ticks = 0; // 1 tick = 1 day

    // ─── Unit Conversions ───
    double days()      const { return static_cast<double>(ticks); }
    double years()     const { return ticks / 365.25; }
    double kiloyears() const { return years() / 1000.0; }
    double megayears() const { return years() / 1e6; }

    // ─── Construction from Units ───
    static SimTime from_days(i64 d)          { return {d}; }
    static SimTime from_years(double y)      { return {static_cast<i64>(y * 365.25)}; }
    static SimTime from_kiloyears(double ky) { return from_years(ky * 1000.0); }
    static SimTime from_megayears(double my) { return from_years(my * 1e6); }

    // ─── Arithmetic ───
    SimTime operator+(SimTime other) const { return {ticks + other.ticks}; }
    SimTime operator-(SimTime other) const { return {ticks - other.ticks}; }
    SimTime& operator+=(SimTime other) { ticks += other.ticks; return *this; }
    SimTime& operator-=(SimTime other) { ticks -= other.ticks; return *this; }

    // ─── Comparison ───
    bool operator==(SimTime other) const { return ticks == other.ticks; }
    bool operator!=(SimTime other) const { return ticks != other.ticks; }
    bool operator<(SimTime other)  const { return ticks < other.ticks; }
    bool operator<=(SimTime other) const { return ticks <= other.ticks; }
    bool operator>(SimTime other)  const { return ticks > other.ticks; }
    bool operator>=(SimTime other) const { return ticks >= other.ticks; }

    // ─── Display ───
    std::string to_string() const {
        double y = years();
        if (std::abs(y) >= 1e6) return std::to_string(megayears()) + " MY";
        if (std::abs(y) >= 1e3) return std::to_string(kiloyears()) + " KY";
        if (std::abs(y) >= 1.0) return std::to_string(y) + " years";
        return std::to_string(ticks) + " days";
    }
};

} // namespace godsim
