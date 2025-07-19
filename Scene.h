//
// Created by Andreas Royset on 4/27/25.
//

#ifndef SCENE_H
#define SCENE_H

#include <utility>
#include <vector>
#include "float3.h"
#include "Sky.h"
#include "Floor.h"
#include "Object.h"
#include "Camera.h"
#include "Image.h"

class Scene {
public:
    int width, height;
    Camera camera;
    int antialiasing;
    std::vector<Object*> bodies;
    Floor* floor_data;
    Sky* sky_data;
    int tileSize;
    Image colorBuffer{};
    std::vector<int> sampleCount;
    std::vector<float> prob;
    int iterations;
    int bounceLim;

    Scene(
          const int width,
          const int height,
          Camera  camera,
          const int antialiasing,
          const std::vector<Object*>& bodies,
          Floor* floor_data = new Floor(),
          Sky* sky_data = new Sky(),
          const int bounceLim = 15,
          const int tileSize = 128)
        : width(width),
          height(height),
          camera(std::move(camera)),
          antialiasing(antialiasing),
          bodies(bodies),
          floor_data(floor_data),
          sky_data(sky_data),
          tileSize(tileSize),
          sampleCount(width * height, 0),
          prob(width * height, 1),
          bounceLim(bounceLim) {
        colorBuffer.clear({width, height}
    );
        iterations = 0;
    }

    Scene(
          const int height,
          const float aspect_ratio,
          const Camera& camera,
          const int antialiasing,
          const std::vector<Object*>& bodies,
          Floor* floor_data = new Floor(),
          Sky* sky_data = new Sky(),
          const int bounceLim = 15,
          const int tileSize = 128)
        : width(int(float(height)*aspect_ratio)),
          height(height),
          camera(camera),
          antialiasing(antialiasing),
          bodies(bodies),
          floor_data(floor_data),
          sky_data(sky_data),
          tileSize(tileSize),
          sampleCount(width * height, 0),
          prob(width * height, 1),
          bounceLim(bounceLim)
    {
        colorBuffer.clear({width,height});
        iterations = 0;
    }

    void reset() {
        colorBuffer.clear();
        sampleCount = std::vector<int>(width * height, 0);
        prob = std::vector<float>(width * height, 1);
        iterations = 0;
    }
};

#endif //SCENE_H
