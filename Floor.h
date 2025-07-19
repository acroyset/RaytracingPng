//
// Created by Andreas Royset on 4/26/25.
//

#ifndef FLOOR_H
#define FLOOR_H

#include "float3.h"
#include "Material.h"

class Floor{
  public:
    bool active;
    float height;
    Material* material1;
    Material* material2;
    float checkerboard_size;

    explicit Floor(const bool active = true,
            const float height = -500,
            Material* material1 = new Material(),
            Material* material2 = nullptr,
            const float checkerboard_size = 1000) {
        this->active = active;
        this->height = height;
        this->material1 = material1;
        if (material2 == nullptr) {this->material2 = material1;}
        else {this->material2 = material2;}
        this->checkerboard_size = checkerboard_size;
    }
};

#endif //FLOOR_H
