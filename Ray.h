//
// Created by Andreas Royset on 11/2/24.
//

#ifndef RAY_H
#define RAY_H

#include "Sphere.h"
#include "HitInfo.h"
#include <iostream>
#include "float3.h"
#include "Material.h"
#include "Floor.h"
#include "Sky.h"

inline float schlick(const float cos_theta, const float n1, const float n2) {
    if (fabs(n1 - n2) < 1e-4f) return 0.0f;

    const float r0 = pow((n1 - n2) / (n1 + n2), 2.0f);
    return r0 + (1.0f - r0) * pow(1.0f - cos_theta, 5.0f);
}

inline float randomValue(uint32_t& state){
    state = state * 747796405u + 2891336453u;
    uint32_t result = ((state >> ((state >> 28) + 4u)) ^ state) * 277803737u;
    result = (result >> 22) ^ result;
    return float(result) / 4294967295.0;
}

class Ray {
    private:
        float3 pos{};
        float3 dir{};
        float3 inv_dir{};
        int bounce;
        float3 color{};
        std::vector<float> ior;
        bool mirror = true;

    public:
        Ray() {
            this->pos = {0, 0, 0};
            this->dir = {0, 0, 1};
            this->bounce = 0;
            this->color = {1, 1, 1};
            this->ior.push_back(1.0f);
        }

        std::pair<float3, bool> trace(const float3& pos, const float3& dir, std::vector<Object*> const &bodies, const Floor* floor_data, const Sky* sky_data, const int bounceLim, uint32_t& state) {
            updateStart(pos,dir);

            for (int i = 0; i < bounceLim; i++) {
                if (!updatePos(bodies, floor_data, sky_data, false, state)) {
                    break;
                }
            }

            if (this->bounce == bounceLim) {
                this->color.clear();
            }

            return {this->color, (this->bounce == 0 or mirror)};
        }

        void updateStart(const float3& pos, const float3& dir) {
            this->pos = pos;
            this->dir = dir;
            this->inv_dir = dir.invert();
            this->bounce = 0;
            this->mirror = true;
            this->color = {1, 1, 1};
            this->ior.clear();
            this->ior.push_back(1.0f);
        }

        bool updateColor(const Material* material, bool isSpecular) {
            if (material->emission_color.mag() > 0) {
                this->color *= material->emission_color;;
                return true;
            }
            if (isSpecular) {
                this->color *= material->specular_color;
            } else {
                this->color *= material->color;
            }
            return false;
        }

        [[nodiscard]] float3 reflect(const float3 normal, const float reflection, uint32_t& state) const {
            float3 reflect = this->dir-normal*2*this->dir.dot(normal);
            const float3 random = (randPoint(state) + normal).normalize();
            if (reflect.dot(normal) < 0) {
                reflect = -reflect;
            }
            return random.lerp(reflect, reflection);
        }

        [[nodiscard]] float3 handleCol(const float3  normal, const Material* material, bool isSpecular, uint32_t& state) {
            if (randomValue(state) < material->transparency) {

                bool entering;
                float m1;
                float m2;
                float3 n = normal;

                if (this->dir.dot(normal) > 0.0f) {
                    entering = false;
                    m1 = material->index_of_refraction;
                    m2 = this->ior[this->ior.size()-2];
                    n = -normal;
                } else {
                    entering = true;
                    m1 = this->ior[this->ior.size()-1];
                    m2 = material->index_of_refraction;
                }

                const float eta = m1 / m2;

                const float cos_theta = -this->dir.dot(n);

                const float reflect_prob = schlick(cos_theta, m1, m2);
                if (randomValue(state) < reflect_prob) {
                    return reflect(normal, material->smoothness, state);
                }

                const float3 r_out_perp = (this->dir + n * cos_theta) * eta;

                float k = 1.0f - r_out_perp.mag2();
                if (fabs(k) < 1e-6f) {
                    k = 0.0f;
                }
                if (k < 0.0f) {
                    // TIF
                    return reflect(normal, material->smoothness, state);
                }

                const float3 r_out_parallel = -n * std::sqrt(k);

                float3 refracted = r_out_perp + r_out_parallel;

                if (entering) {
                    this->ior.push_back(material->index_of_refraction);
                } else {
                    this->ior.pop_back();
                }
                const float3 random = (randPoint(state)+normal).normalize();
                refracted = random.lerp(refracted, material->specular_probability);

                return refracted;

            }
            else {
                return reflect(normal, material->smoothness * float(isSpecular), state);
            }
        }

        [[nodiscard]] HitInfo* closest_collision(std::vector<Object*> const &bodies) {
            HitInfo* best = nullptr;
            auto best_t = float(pow(10,10));
            for (const Object *obj : bodies) {
                HitInfo* col = obj->checkCollision(this->pos, this->dir, this->inv_dir);
                if (col->getHit()) {
                    if (col->getT() < best_t) {
                        delete best;
                        best = col;
                        best_t = col->getT();
                    }
                }
            }
            return best;
        }

        bool updatePos(std::vector<Object*> const &bodies, const Floor* floor_data, const Sky* sky_data, bool simple, uint32_t& state){
            if (terminate(state)) {
                mirror = false;
                this->color.clear();
                return false;
            }

            const HitInfo* best = closest_collision(bodies);

            if (best != nullptr) {
                if (simple) {
                    this->pos += this->dir*best->getT();
                    this->dir = sky_data->sun_dir;
                    this->inv_dir = this->dir.invert();

                    float light = best->getNormal().dot(sky_data->sun_dir);
                    if (closest_collision(bodies) != nullptr) {
                        light = 0.0f;
                    }
                    this->color = float3(light);
                    delete best;
                    return false;
                }

                const bool isSpecular = best->getMaterial()->specular_probability > randomValue(state);

                if (updateColor(best->getMaterial(), isSpecular)) {
                    delete best;
                    return false;
                }

                if (best->getMaterial()->smoothness != 1 or best->getMaterial()->specular_probability != 1 or best->getMaterial()->transparency != 0) {
                    mirror = false;
                }

                this->bounce++;

                this->pos += this->dir*best->getT();
                this->dir = handleCol(best->getNormal(), best->getMaterial(), isSpecular, state);
                this->inv_dir = this->dir.invert();

                delete best;
                return true;
            }
            if (floor_data->active and this->dir.y < 0) {
                const float t = (floor_data->height-this->pos.y)/this->dir.y;

                bool color1;

                if (0.01f < t && t < 1000000) {
                    this->pos += this->dir*t;

                    float3 normal = {0,1,0};

                    float x = pos.x;
                    float z = pos.z;
                    if (x < 0) {x = abs(x)+floor_data->checkerboard_size/2;}
                    if (z < 0) {z = abs(z)+floor_data->checkerboard_size/2;}
                    if (float(int(x) % int(floor_data->checkerboard_size)) - floor_data->checkerboard_size/2 < 0 xor
                        float(int(z) % int(floor_data->checkerboard_size)) - floor_data->checkerboard_size/2 < 0) {

                        color1 = false;
                    } else {
                        color1 = true;
                    }

                    Material* material;
                    if (color1) material = floor_data->material1;
                    else material = floor_data->material2;

                    const bool isSpecular = material->specular_probability > randomValue(state);
                    if (updateColor(material, isSpecular)) {
                        delete best;
                        return false;
                    }

                    if (material->smoothness != 1 or material->specular_probability != 1 or material->transparency != 0) {
                        mirror = false;
                    }

                    this->dir = reflect(normal, material->smoothness*float(isSpecular), state);
                    this->inv_dir = this->dir.invert();

                    this->bounce ++;

                    delete best;
                    return true;
                }
            }
            if (sky_data->active) {
                const float3 ev_color = sky_data->getSkyColor(this->dir);
                this->color *= ev_color;
            }
            else {
                this->color.clear();
            }
            delete best;
            return false;
        }

        bool terminate(uint32_t& state) {
            if (this->bounce > 5) {
                const float continue_prob = std::min(1.0f, this->color.sum()); // brightness-based
                if (randomValue(state) > continue_prob) {
                    //this->color = {1, 0, 0};
                    return true;
                }
                this->color *= (1.0f / continue_prob); // compensate
            }
            return false;
        }

        static float3 randPoint(uint32_t& state) {
            for (int i = 0; i < 10; i++) {
                const float x = 2*randomValue(state)-1;
                const float y = 2*randomValue(state)-1;
                const float z = 2*randomValue(state)-1;
                const float magnitude = float3(x, y, z).mag();
                if (magnitude <= 1 and magnitude != 0) {return float3(x,y,z).normalize();}
            }
            return float3(1, 0, 0).normalize();
        }
};

#endif //RAY_H
