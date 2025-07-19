//
// Created by Andreas Royset on 4/26/25.
//

#ifndef SKY_H
#define SKY_H

inline int clamp(const int x, const int min, const int max) {
    return std::max(min, std::min(max, x));
}

class Sky {
    public:
    bool active;
    float3 color1;
    float3 color2;

    float3 sun_dir;
    float3 sun_color;

    //sf::Image skyTexture;
    bool loaded;

    explicit Sky(const bool active = true,
            const float3 sun_dir = float3(0.4, 0.4, 0.9).normalize(),
            const float3 sun_color = float3(1.0, 0.95, 0.9) * 5.0,
            const float3 color1 = float3(0.6, 0.75, 0.9),
            const float3 color2 = float3(0.30, 0.55, 0.8)
            ) {
        this->active = active;
        this->color1 = color1;
        this->color2 = color2;

        this->sun_dir = sun_dir;
        this->sun_color = sun_color;

        //loaded = skyTexture.loadFromFile("sky.png");
        loaded = false;
        if (!loaded) {
            //std::cerr << "Failed to load sky.png\n";
        }
    }
    [[nodiscard]] float3 getSkyColor(const float3& dir) const {
        if (loaded) {
            const float u = 0.5f + atan2(dir.z, dir.x) / (2.0f * float(M_PI));
            const float v = 0.5f - asin(dir.y) / float(M_PI);

            //const unsigned int x = clamp(static_cast<int>(u * float(skyTexture.getSize().x)), 0, int(skyTexture.getSize().x) - 1);
            //const unsigned int y = clamp(static_cast<int>(v * float(skyTexture.getSize().y)), 0, int(skyTexture.getSize().y) - 1);

            //const sf::Color color = skyTexture.getPixel(x, y);
            //Vec3f ev_color = {float(color.r), float(color.g), float(color.b)};
            //ev_color /= 255.0f; // Normalize to 0..1

            //const float sun_strength = float(pow(std::max(0.0f, dir.dot(this->sun_dir)), 1024)); // sharp sun disc
            //ev_color += this->sun_color * sun_strength;

            //return ev_color;
        }
        float3 ev_color = this->color1.lerp(this->color2, abs(dir.y));

        const float sun_strength = float(pow(std::max(0.0f, dir.dot(this->sun_dir)), 1024)); // sharp sun disc
        ev_color += this->sun_color * sun_strength;

        return ev_color;
    }
};

#endif //SKY_H
