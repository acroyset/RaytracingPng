//
// Created by Andreas Royset on 11/3/24.
//

#ifndef COLLISION_H
#define COLLISION_H
#include "float3.h"
#include "Material.h"

class HitInfo {
    private:
    bool hit;
    float t;
    float3 normal;
    Material *material;

    public:
    HitInfo() {
        hit = false;
        t = float(pow(10,10));
        normal = float3();
        material = new Material();

    }
    explicit HitInfo(Material *material) {
        this->hit = false;
        this->t = 0;
        this->normal = {0,0,0};
        this->material = material;
    }

    void updateData(const bool hit, const float t = 0, const float3 normal = {0,0,1}) {
        this->hit = hit;
        this->t = t;
        this->normal = normal;
    }

    [[nodiscard]] bool getHit() const {return hit;}
    [[nodiscard]] float getT() const {return t;}
    [[nodiscard]] Material* getMaterial() const {return material;}
    [[nodiscard]] float3 getNormal() const {return normal;}
};

#endif //COLLISION_H
