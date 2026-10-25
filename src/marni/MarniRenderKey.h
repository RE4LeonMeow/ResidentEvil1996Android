// Stable identity for port-rendered geometry; no game/save layout changes.
#pragma once
#include <cstdint>
struct MarniTriangleKey {
    uint32_t object, geometry, triangle;
    bool operator<(const MarniTriangleKey& b) const {
        if(object!=b.object)return object<b.object;
        if(geometry!=b.geometry)return geometry<b.geometry;
        return triangle<b.triangle;
    }
};
