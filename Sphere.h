//
// Created by Andreas Royset on 11/2/24.
//

#ifndef SPHERE_H
#define SPHERE_H
#include <cmath>
#include "HitInfo.h"
#include "float3.h"
#include "Object.h"
#include "Material.h"

class Sphere : public Object {
    private:
        float radius;
        float3 pos{};
        Material* material{};
    public:
        Sphere(const float radius,
            const float3 pos,
            Material* material) {

            this->pos = pos;
            this->radius = radius;
            this->material = material;

        }

    [[nodiscard]] HitInfo* hit(const bool hit = false, const float t = 0, const float3 normal = {0,0,0}) const {
            auto* hit_info = new HitInfo(material);
            hit_info->updateData(hit, t, normal);
            return hit_info;
        }

    [[nodiscard]] HitInfo* checkCollision(const float3& pos, const float3& dir, const float3& inv_dir) const override {
        const float3 ray_pos = pos-this->pos;
        const float d = ray_pos.dot(dir);

        const auto discriminant = float(d*d - ray_pos.x*ray_pos.x - ray_pos.y*ray_pos.y - ray_pos.z*ray_pos.z + this->radius*this->radius);

        if (discriminant < 0) {
            return hit();
        } if (discriminant == 0) {
            const float t = -d;
            const float3 newPos = pos-dir*-t;
            if (t > 0.01) {
                return hit(true, t, (newPos-this->pos).normalize());
            }
            return hit(false);
        }
        float t = -d - float(sqrt(discriminant));
        if (t > 0.01) {
            const float3 newPos = pos-dir*-t;
            return hit(true, t, (newPos-this->pos).normalize());
        }
        t = -d + float(sqrt(discriminant));
        if (t > 0.01) {
            const float3 newPos = pos-dir*-t;
            return hit(true, t, (newPos-this->pos).normalize());
        }
            return hit();
    }


};

#endif //SPHERE_H
