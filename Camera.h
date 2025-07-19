//
// Created by Andreas Royset on 7/5/25.
//

#ifndef CAMERA_H
#define CAMERA_H

#include "float3.h"
#include <functional>

class Camera {
    public:
    float3 position;
    float3 target;
    float3 forward;
    float3 up;
    float3 right;
    std::function<std::vector<float3>(float)> path;
    int frameCount;
    int frameRate;
    int duration;

    Camera(const float3 position, const float3 target) {
        this->position = position;
        this->target = target;
        forward = (target - position).normalize();
        const float3 world_up(0, 1, 0);
        right = forward.cross(world_up).normalize();
        up = world_up;
        this->frameCount = 0;
        this->frameRate = 1;
        this->duration = 1;
    }
    Camera(const std::function<std::vector<float3>(float)>& path, const int duration, const int frameRate = 30) {
        this->path = path;
        this->duration = duration;
        this->frameRate = frameRate;
        this->frameCount = 0;
    }

    bool update() {
        frameCount++;
        if (frameCount > duration*frameRate) {
            return true;
        }

        if (frameRate != 1) {
            const std::vector<float3> pos_tgt = path(float(frameCount-1)/float(frameRate));

            position = pos_tgt[0];
            target = pos_tgt[1];
            forward = (target - position).normalize();
            const float3 world_up(0, 1, 0);
            right = forward.cross(world_up).normalize();
            up = world_up;
        }

        return false;
    }
};

#endif //CAMERA_H
