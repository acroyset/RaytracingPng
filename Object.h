//
// Created by Andreas Royset on 4/26/25.
//

#ifndef OBJECT_H
#define OBJECT_H

#include "HitInfo.h"

class Object {
public:
    virtual ~Object() = default;
    [[nodiscard]] virtual HitInfo* checkCollision(const float3& pos, const float3& dir, const float3& inv_dir) const = 0;
};

#endif //OBJECT_H
