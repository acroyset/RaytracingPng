//
// Created by Andreas Royset on 4/26/25.
//

#ifndef MATERIAL_H
#define MATERIAL_H

#include "float3.h"

class Material {
    public:
    float3 color{};
    float smoothness;
    float specular_probability;
    float3 specular_color{};
    float transparency;
    float index_of_refraction;
    float3 emission_color{};

    Material() {
        color = float3{};
        smoothness = 0;
        specular_probability = 0.0f;
        specular_color = float3{};
        transparency = 0.0f;
        index_of_refraction = 1.0f;
        emission_color = float3{};
    }

    explicit Material(const float3 color, const float smoothness, const float specular_probability = 1, const float3 specular_color = float3(-1), const float transparency = 0, const float index_of_refraction = 1, const float3 emission_color = float3(0, 0, 0)) {
        this->color = color;
        this->smoothness = smoothness;
        this->specular_probability = specular_probability;
        if (specular_color == float3{-1}) this->specular_color = color;
        else this->specular_color = specular_color;
        this->transparency = transparency;
        this->index_of_refraction = index_of_refraction;
        this->emission_color = emission_color;
    }

    Material* avg(const Material* other) const {
        auto result = new Material();
        result->color = (color+other->color)/2;
        result->smoothness = (smoothness+other->smoothness)/2;
        result->specular_probability = (specular_probability+other->specular_probability)/2;
        result->specular_color = (specular_color+other->specular_color)/2;
        result->transparency = (transparency+other->transparency)/2;
        result->index_of_refraction = (index_of_refraction + other->index_of_refraction)/2;
        result->emission_color = (emission_color+other->emission_color)/2;
        return result;
    }
};

#endif //MATERIAL_H
