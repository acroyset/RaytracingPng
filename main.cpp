#include <atomic>
#include <condition_variable>
#include <iostream>
#include <thread>
#include <vector>
#include "Ray.h"
#include "Sphere.h"
#include "float3.h"
#include "float2.h"
#include "Object.h"
#include "Box.h"
#include "Floor.h"
#include "Scene.h"
#include "Sky.h"
#include <valarray>
#include "int2.h"
#include <functional>
#include <mutex>
#include <chrono>

namespace fs = std::filesystem;

class Timer {
    std::chrono::steady_clock::time_point start;
    std::chrono::steady_clock::time_point pause;
    bool paused = false;

public:
    explicit Timer(const bool paused = false) {
        start = std::chrono::steady_clock::now();
        this->paused = paused;
        if (paused) {
            pause = std::chrono::steady_clock::now();
        }
    }

    int reset() {
        const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        const int t = int(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
        start = end;
        return t;
    }
    [[nodiscard]] int elapsed() const {
        const std::chrono::time_point<std::chrono::steady_clock>::duration offset = paused ? std::chrono::steady_clock::now() - pause : std::chrono::time_point<std::chrono::steady_clock>::duration(0);
        const std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        const int t = int(std::chrono::duration_cast<std::chrono::milliseconds>(end - start - offset).count());
        return t;
    }
    void start_stop() {
        if (paused) {
            paused = false;
            const std::chrono::time_point<std::chrono::steady_clock>::duration elapsed =
                    std::chrono::steady_clock::now() - pause;
            start += elapsed;
        } else {
            paused = true;
            pause = std::chrono::steady_clock::now();
        }
    }
};

class ThreadPool {
    std::vector<std::thread> workers;
    std::vector<std::function<void()>> tasks;
    std::mutex queue_mutex;
    std::condition_variable cv;
    std::condition_variable finished_cv;
    bool stop = false;
    size_t active_tasks = 0;  // Tracks how many tasks are currently running
    std::atomic<size_t> total_enqueued{0};
    std::atomic<size_t> total_completed{0};

public:
    explicit ThreadPool(const size_t threads) {
        for (size_t i = 0; i < threads; ++i) {
            workers.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->cv.wait(lock, [this] {
                            return this->stop || !this->tasks.empty();
                        });

                        if (this->stop && this->tasks.empty())
                            return;

                        task = std::move(this->tasks.back());
                        this->tasks.pop_back();
                        ++active_tasks;
                    }

                    task();

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        --active_tasks;
                        ++total_completed;
                        if (tasks.empty() && active_tasks == 0) {
                            finished_cv.notify_all();
                        }
                    }
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            stop = true;
        }
        cv.notify_all();
        for (auto &worker : workers)
            worker.join();
    }

    void enqueue(std::function<void()> task) {
        {
            std::unique_lock<std::mutex> lock(queue_mutex);
            tasks.push_back(std::move(task));
            ++total_enqueued;
        }
        cv.notify_one();
    }

    // Wait for all tasks to finish
    void wait_for_tasks() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        finished_cv.wait(lock, [this] {
            return tasks.empty() && active_tasks == 0;
        });
    }

    size_t tasks_left() {
        std::unique_lock<std::mutex> lock(queue_mutex);
        return total_enqueued - total_completed;
    }

};

inline void makeImage(const char* filename, const std::vector<float>& data, int2 size) {
    auto* newData = new unsigned char[size.x * size.y * 3];
    for (int x = 0; x < size.x; x++) {
        for (int y = 0; y < size.y; y++) {
            const int index = x + y * size.x;
            float color = data[(x) + (y) * int(size.x)];
            color *= 255;

            newData[3*index+0] = int(color);
            newData[3*index+1] = int(color);
            newData[3*index+2] = int(color);
        }
    }
    stbi_write_png(filename, size.x, size.y, 3, newData, size.x * 3);
    delete[] newData;
}
float3 makeRay(const float2& pos, const Scene& scene) {
    // Build camera basis

    // Screen space
    const float ndc_x = (pos.x) / float(scene.width);  // 0 - 1
    const float ndc_y = (pos.y) / float(scene.height);

    float screen_x = 1.0f - 2.0f * ndc_x; //-1 - 1
    float screen_y = 1.0f - 2.0f * ndc_y; // flip y //-1 - 1

    const float aspect_ratio = float(scene.width)/float(scene.height);
    constexpr float scale = 1.0f;//tanf((fov * 0.5f) * (float(M_PI) / 180.0f)); // fov to scale

    screen_x *= aspect_ratio * scale;
    screen_y *= scale;

    // Ray direction
    const float3 dir = (scene.camera.forward + scene.camera.right * screen_x + scene.camera.up * screen_y).normalize();

    return dir;
}
void renderTile(const int tileX, const int tileY, const int tileWidth, const int tileHeight, Scene& scene, uint32_t& state) {
    const int aa = scene.antialiasing;
    const int aaStep = scene.iterations%(aa*aa);

    Ray ray;

    for (int y = tileY; y < tileY+tileHeight; ++y) {
        for (int x = tileX; x < tileX+tileWidth; ++x) {
            if (x >= scene.width || y >= scene.height) continue;

            const int index = y * scene.width + x;

            //if ((x % scene.tileSize == 0) or (y % scene.tileSize == 0)) {
            //    colorBuffer[i] += Vec3f(1, 0, 0);
            //    sampleCount[i] += 1;
            //    continue;
            //}

            bool hitSky = false;

            if (int(scene.prob[index])) {
                const auto xi = float(aaStep % aa);
                const auto yi = float(aaStep) / float(aa);

                const float ox = (xi + 0.5f) / float(aa) - 0.5f; // Center of each subpixel grid cell
                const float oy = (yi + 0.5f) / float(aa) - 0.5f;

                float3 dir = makeRay({float(x) + ox, float(y) + oy}, scene);
                const std::pair<float3, bool> out = ray.trace(scene.camera.position, dir, scene.bodies, scene.floor_data, scene.sky_data, scene.bounceLim, state);

                const float3 color = out.first*255;
                const bool sky = out.second;

                if (sky) {
                    hitSky = true;
                }

                scene.colorBuffer.add(x,y,color);
                scene.sampleCount[index]++;
            }
            if (hitSky) {
                if (scene.iterations+1>=aa*aa) {
                    scene.prob[index] = 0;
                }
            }
        }
    }
}
std::string timeConversionnMS(int ms) {
    auto x = float(ms);
    std::string out;
    if (x < 10) {
        out += std::to_string(x);
        out = out.substr(0, 4);
        out += "ms";
        return out;
    } // 0-99ms
    if (x < 1000) {
        out += std::to_string(x);
        out = out.substr(0, 3);
        out += "ms";
        return out;
    } // 100-999ms
    x /= 1000;
    if (x < 10) {
        out += std::to_string(x);
        out = out.substr(0, 3);
        out += "s";
        return out;
    } // 1-9s
    if (x < 60) {
        out += std::to_string(x);
        out = out.substr(0, 2);
        out += "s";
        return out;
    } // 10-59s
    x /= 60;
    if (x < 10) {
        out += std::to_string(x);
        out = out.substr(0, 3);
        out += "m";
        return out;
    } // 1-9m
    if (x < 60) {
        out += std::to_string(x);
        out = out.substr(0, 2);
        out += "m";
        return out;
    } // 10-59m
    x /= 60;
    if (x < 10) {
        out += std::to_string(x);
        out = out.substr(0, 3);
        out += "h";
        return out;
    } // 1-9h
    out += std::to_string(x); // 10h+
    out = out.substr(0, 2);
    out += "h";
    return out;
}
std::vector<float3> path(const float t) {
    //float3 pos = {350, -300, -650};
    float3 tgt = {300, -350, -200};

    float3 pos = {800 * float(cos(t*M_PI/10))+200, -300, 800 * float(sin(t*M_PI/10))};

    return {pos, tgt};
}
void makeMp4(const std::string& path, const int2 size) {
    std::string command = "ffmpeg -loglevel quiet -y -framerate 30 -i ";
    command += path;
    command += "frame%03d.png -vf scale=";
    command += std::to_string(size.x) + ':' + std::to_string(size.y) + ' ';
    command += "-c:v libx264 -pix_fmt yuv420p output.mp4";

    int result = std::system(command.c_str());

    if (result != 0) {
        std::cerr << "FFmpeg command failed with code: " << result << std::endl;
    } else {
        std::cout << "Video created successfully.\n";
    }
}
void createFrame(const std::string& path, const Image& image, const int frameNum) {
    std::string filename = path + "frame";
    const int zeros = 3 - int(std::to_string(frameNum).length());
    for (int j = 0; j < zeros; ++j) filename += '0';
    filename += std::to_string(frameNum);
    filename += ".png";
    image.makePng(filename);
}
Image readFrame(const std::string& path, const int frameNum) {
    std::string filename = path + "frame";
    const int zeros = 3 - int(std::to_string(frameNum).length());
    for (int j = 0; j < zeros; ++j) filename += '0';
    filename += std::to_string(frameNum);
    filename += ".png";
    Image image = Image(filename);
    return image;
}
void deletePngs(const std::string& folderPath) {
    const std::string command = "rm -f " + folderPath + "/*.png";
    std::system(command.c_str());
}
float softThreshold(const float x, const float threshold) {
    const float knee = 510-2*threshold;
    if (x < threshold)
        return 0.0f;
    if (x < threshold + knee)
        return ((x - threshold) * (x - threshold)) / (2.0f * knee);
    return x - threshold - (knee / 2.0f);
}
void wait_with_progress(ThreadPool& pool, int interval_ms, int tasks) {
    Timer timer;
    int prevRem = tasks;
    int remaining = tasks;
    int avgRate = 0;
    int samples = 0;
    while (true) {
        prevRem = remaining;
        remaining = int(pool.tasks_left());
        const int delta = prevRem - remaining;
        avgRate *= samples;
        avgRate += delta;
        ++samples;
        avgRate /= samples;
        int t = timer.reset() / avgRate * remaining;
        std::cout << "\rTasks remaining: " << remaining << "  -  " << timeConversionnMS(t) << " left" << std::flush;

        if (remaining <= 0) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }
    std::cout << "\nAll Tasks Finished" << std::endl;
}

int main() {
    Timer timer;
    auto* white_diffuse = new Material({0.9, 0.9, 0.9}, 0);
    auto* red_diffuse = new Material({0.9, 0.2, 0.2}, 0);
    auto* green_diffuse = new Material({0.2, 0.9, 0.2}, 0);
    auto* blue_diffuse = new Material({0.2, 0.2, 0.9}, 0);
    auto* black_diffuse = new Material({0.7, 0.7, 0.7}, 0);
    auto* mirror = new Material({0.9, 0.9, 0.9}, 1);
    auto* light = new Material({0,0,0}, 0, 0, {0,0,0},0,1, {0.1,1,1.5});
    auto* light2 = new Material({0,0,0}, 0, 0, {0,0,0},0,1, {2, 1, 0.2});

    auto* l1 = new Material({0,0,0}, 0, 0, {0,0,0},0,1, {5, 0.5, 0.5});
    auto* l2 = new Material({0,0,0}, 0, 0, {0,0,0},0,1, {0.5, 5, 0.5});
    auto* l3 = new Material({0,0,0}, 0, 0, {0,0,0},0,1, {0.5, 0.5, 5});
    auto* l4 = new Material({0,0,0}, 0, 0, {0,0,0},0,1, {5, 5, 0.5});

    std::vector<Object*> bodies = {
        //new Box({-100000, -500, -100000}, {100000, -501, 100000}, white_diffuse),
        new Sphere(300, {0, -200, 50}, light),
        new Sphere(150, {700, -350, 150}, red_diffuse),
        new Sphere(100, {100, -400, -400}, green_diffuse),
        new Sphere(100, {300, -400, -100}, blue_diffuse),
        new Sphere(50, {750, -450, -150}, light2),
        new Sphere(125, {-350, -375, -350}, white_diffuse),
        new Sphere(60, {450, -440, -300}, mirror),
    };

    float s = 400;
    float w = 40;
    float f = 0.75f;
    float s2 = 30;

    //std::vector<Object*> bodies = {
        //new Box({s+w, -(s+w), -(s+w)}, {s-w, -(s-w), s+w}, black_diffuse),
        //new Box({-(s+w), -(s+w), -(s+w)}, {-(s-w), -(s-w), s+w}, black_diffuse),
        //new Box({s-w, -(s+w), -(s+w)}, {-(s-w), -(s-w), -(s-w)}, black_diffuse),
        //new Box({s-w, -(s+w), s+w}, {-(s-w), -(s-w), s-w}, black_diffuse),

        //new Box({s+w, (s+w), -(s+w)}, {s-w, (s-w), s+w}, black_diffuse),
        //new Box({-(s+w), (s+w), -(s+w)}, {-(s-w), (s-w), s+w}, black_diffuse),
        //new Box({s-w, (s+w), -(s+w)}, {-(s-w), (s-w), -(s-w)}, black_diffuse),
        //new Box({s-w, (s+w), s+w}, {-(s-w), (s-w), s-w}, black_diffuse),

        //new Box({s+w, -(s-w), -(s+w)}, {s-w, s-w, -(s-w)}, black_diffuse),
        //new Box({-(s+w), -(s-w), -(s+w)}, {-(s-w), s-w, -(s-w)}, black_diffuse),
        //new Box({s+w, -(s-w), (s+w)}, {s-w, s-w, (s-w)}, black_diffuse),
        //new Box({-(s+w), -(s-w), (s+w)}, {-(s-w), s-w, (s-w)}, black_diffuse),

        //new Box({f*s+s2, s2, f*s+s2}, {f*s-s2, -s2, f*s-s2}, l1),
        // Box({-f*s+s2, s2, f*s+s2}, {-f*s-s2, -s2, f*s-s2}, l2),
        //new Box({f*s+s2, s2, -f*s+s2}, {f*s-s2, -s2, -f*s-s2}, l3),
        //new Box({-f*s+s2, s2, -f*s+s2}, {-f*s-s2, -s2, -f*s-s2}, l4)
    //};


    Scene scene(
        1440, 16.0f/9.0f,
        Camera({400, -200, -800},{0, 0, 0}),
        4,
        bodies,
        new Floor(true, -500, black_diffuse, black_diffuse, 100),
        new Sky(false, float3(-1, -1, 0.5).normalize(), {50, 50, 40}, {0,0,0}, {0,0,0}),
        8,
        128
        );

    bool stats = scene.camera.frameRate == 1;
    //if (!stats) deletePngs("animation");
    std::cout << "Setup Complete  -  " << timeConversionnMS(timer.reset()) << std::endl;

    const int maxIterations = 100;
    constexpr bool multithreading = false;

    bool bloomActive = true;
    float falloff = 1.0f;
    const std::vector<float> kernel {
        0.00391f,  // -5
        0.01018f,  // -4
        0.02459f,  // -3
        0.04947f,  // -2
        0.08553f,  // -1
        0.12066f,  //  0 (center)
        0.08553f,  // +1
        0.04947f,  // +2
        0.02459f,  // +3
        0.01018f,  // +4
        0.00391f   // +5
    };

    uint32_t state = time(nullptr);

    int numThreads = int(std::thread::hardware_concurrency());

    //render animation
    while (!scene.camera.update()) {
        //render iterations
        scene.reset();

        if (multithreading) {
            int tasks = 0;
            ThreadPool pool(numThreads);
            while (scene.iterations < maxIterations) {
                for (int tileY = 0; tileY < scene.height; tileY += scene.tileSize) {
                    for (int tileX = 0; tileX < scene.width; tileX += scene.tileSize) {
                        pool.enqueue([&, tileX, tileY]() {
                            const int tileWidth = std::min(scene.tileSize, scene.width - tileX);
                            const int tileHeight = std::min(scene.tileSize, scene.height - tileY);
                            renderTile(tileX, tileY, tileWidth, tileHeight, scene, state);
                        });
                        tasks++;
                    }
                }
                scene.iterations ++;
            }
            std::cout << std::endl;
            std::cout << tasks << " Tasks  -  " << numThreads << " Threads" << std::endl;
            wait_with_progress(pool, 500, tasks);
        }
        else {
            Timer renderTimer;
            while (scene.iterations < maxIterations) {
                for (int tileY = 0; tileY < scene.height; tileY += scene.tileSize) {
                    for (int tileX = 0; tileX < scene.width; tileX += scene.tileSize) {
                        const int tileWidth = std::min(scene.tileSize, scene.width - tileX);
                        const int tileHeight = std::min(scene.tileSize, scene.height - tileY);
                        renderTile(tileX, tileY, tileWidth, tileHeight, scene, state);
                    }
                }

                scene.iterations ++;

                if (stats) {
                    int timeMS = renderTimer.reset();
                    std::cout << "\rIterations: " << scene.iterations << "/" << maxIterations <<
                    "  -  " << timeConversionnMS(timeMS) << "  -  " << int((100*scene.iterations)/maxIterations) << "%  -  " <<
                    timeConversionnMS(timeMS*(maxIterations-scene.iterations)) << std::flush;
                }
            }
        }

        if (stats) {
            std::cout << std::endl;
            std::cout << "Render Complete  -  " << timeConversionnMS(timer.reset()) << std::endl;
        }
        scene.colorBuffer.divide(scene.sampleCount);

        if (stats) scene.colorBuffer.makePng("noBloom.png");

        //bloom
        auto bloom = scene.colorBuffer;
        bloom.apply([](const float x){return softThreshold(x, 127.5f);});
        bloom.clamp(0, 8192);
        if (bloomActive) {
            bloom.downsample();
            const int numMipLevels = int(log2(float(bloom.getSize().y)))-1;

            //downsample
            std::vector<Image> mipLevels;
            mipLevels.push_back(bloom);
            //bloom.makePng("downsample0.png");
            for (int i = 0; i < numMipLevels-1; i++) {
                bloom.downsample();
                bloom.blur(kernel);
                mipLevels.push_back(bloom);
                Image bloom2 = bloom;
                //bloom2.makePng("downsample"+std::to_string(i+1)+".png");
            }

            //upsample
            float weight = 1;
            float total = 0;
            bloom *= float3(weight); // apply weight to lowest mip before any += happens
            weight *= falloff;
            total += weight;
            for (int i = 0; i < numMipLevels-1; i++) {
                bloom.upsample();
                const Image& currentLevel = mipLevels[numMipLevels-i-2];
                bloom.resize(currentLevel.getSize());
                bloom += currentLevel;
                currentLevel *= float3(weight);
                bloom += currentLevel;
                weight *= falloff;
                total += weight;
                //bloom.makePng("upsample"+std::to_string(i+1)+".png");
            }
            bloom *= float3(1/total);

            //tonemap
            bloom *= float3(0.5f);
            bloom.aces();
            bloom.upsample();
            bloom.clamp(0, 255);
            bloom.makePng("bloom.png");
        }
        if (stats) std::cout << "Bloom Complete  -  " << timeConversionnMS(timer.reset()) << std::endl;

        //make pixels
        scene.colorBuffer += bloom;
        scene.colorBuffer.linearize();
        if (stats) std::cout << "Pixels Complete  -  " << timeConversionnMS(timer.reset()) << std::endl;

        //make image
        createFrame("animation/", scene.colorBuffer, scene.camera.frameCount);
        if (stats) {
            bloom.makePng("bloom.png");
            makeImage("prob.png", scene.prob, {scene.width, scene.height});
        }
        if (stats) std::cout << "Image Complete  -  " << timeConversionnMS(timer.reset()) << std::endl;

        if (!stats) std::cout << "frame " << scene.camera.frameCount << "/" << scene.camera.duration*scene.camera.frameRate << "  -  " << timeConversionnMS(timer.reset()) << std::endl;
    }

    if (!stats) {
        makeMp4("animation/", {scene.width, scene.height});
    }

    return 0;
}
