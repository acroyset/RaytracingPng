//
// Created by Andreas Royset on 4/26/25.
//

#ifndef BOX_H
#define BOX_H

#include "Object.h"
#include "Material.h"
#include "HitInfo.h"
#include <complex>

class Box : public Object {
private:
    float3 min_corner;
    float3 max_corner;
    Material* material;
    HitInfo* collision;

public:
    Box(const float3& min_corner, const float3& max_corner, Material* material)
        : min_corner(min_corner), max_corner(max_corner), material(material) {
        collision = new HitInfo(material);
    }

    [[nodiscard]] HitInfo* checkCollision(const float3& pos, const float3& dir, const float3& inv_dir) const override {
        const float tx1 = (min_corner.x - pos.x) * inv_dir.x;
        const float tx2 = (max_corner.x - pos.x) * inv_dir.x;
        const float tx_min = std::min(tx1, tx2);
        const float tx_max = std::max(tx1, tx2);

        const float ty1 = (min_corner.y - pos.y) * inv_dir.y;
        const float ty2 = (max_corner.y - pos.y) * inv_dir.y;
        const float ty_min = std::min(ty1, ty2);
        const float ty_max = std::max(ty1, ty2);

        const float tz1 = (min_corner.z - pos.z) * inv_dir.z;
        const float tz2 = (max_corner.z - pos.z) * inv_dir.z;
        const float tz_min = std::min(tz1, tz2);
        const float tz_max = std::max(tz1, tz2);

        const float tmin = std::max(std::max(tx_min, ty_min), tz_min);
        const float tmax = std::min(std::min(tx_max, ty_max), tz_max);

        if (tmax < 0.0f || tmin > tmax) {
            collision->updateData(false);
            return collision;
        }

        const float t_hit = (tmin > 0.01f) ? tmin : tmax;

        if (t_hit < 0.01f) {
            collision->updateData(false);
            return collision;
        }

        float3 normal = {0, 0, 0};

        constexpr  float eps = 1e-4f;
        if (std::abs(tmin - tx_min) < eps) normal = (inv_dir.x < 0.0f) ? float3(1, 0, 0) : float3(-1, 0, 0);
        else if (std::abs(tmin - ty_min) < eps) normal = (inv_dir.y < 0.0f) ? float3(0, 1, 0) : float3(0, -1, 0);
        else if (std::abs(tmin - tz_min) < eps) normal = (inv_dir.z < 0.0f) ? float3(0, 0, 1) : float3(0, 0, -1);

        collision->updateData(true, t_hit, normal);
        return collision;
    }
};

#endif //BOX_H
