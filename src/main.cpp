#include <numeric>
#include <algorithm>
#include <cmath>
#include <ranges>

#include <imgui.h>
#include <imgui-SFML.h>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/RectangleShape.hpp>
#include <SFML/Window/Event.hpp>
//#include <SFML/System/Clock.hpp>
//#include <SFML/Graphics/CircleShape.hpp>

#include <spdlog/spdlog.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace renderer {

constexpr auto length(const glm::dvec3 &vec) noexcept -> double
{
    return std::sqrt(std::pow(vec.x, 2) + std::pow(vec.y, 2) + std::pow(vec.z, 2));
}

constexpr auto normalize(const glm::dvec3 &vec) -> glm::dvec3
{
    const auto len = renderer::length(vec);
    return len != 0.0 ? glm::dvec3{vec.x / len, vec.y / len, vec.z / len} : vec;
}

glm::dvec3 toRight(const glm::dvec3 &vec)
{
    const auto tmp = glm::dquat{vec.x, vec.y, vec.z, 1.0f};
    const auto res = glm::cross(glm::cross(tmp, glm::dquat{1.0, 0.0, 0.0, 1.0}), glm::conjugate(tmp));
    return {res.x, res.y, res.z};
}

glm::dvec3 toUp(const glm::dvec3 &vec)
{
    const auto tmp = glm::dquat{vec.x, vec.y, vec.z, 1.0f};
    const auto res = glm::cross(glm::cross(tmp, glm::dquat{0.0, 1.0, 0.0, 1.0}), glm::conjugate(tmp));
    return {res.x, res.y, res.z};
}

glm::dvec3 toForward(const glm::dvec3 &vec)
{
    const auto tmp = glm::dquat{vec.x, vec.y, vec.z, 1.0f};
    const auto res = glm::cross(glm::cross(tmp, glm::dquat{0.0, 0.0, 1.0, 1.0}), glm::conjugate(tmp));
    return {res.x, res.y, res.z};
}


struct Sphere {
    glm::dvec3 center;
    double radius;

    constexpr auto distance(const glm::dvec3 &point) const -> double
    {
        return std::max(0.0, renderer::length(point - center) - radius);
    }
};

struct Ray {
    glm::dvec3 origin;
    glm::dvec3 direction;
};

struct Camera {
    glm::dvec3 position;
    glm::dvec3 orientation;
    double fov = 90;

    constexpr auto toRay(glm::dvec2 &&pixel, const glm::dvec2 &image_size) const -> Ray
    {
        const auto angle = std::tan(glm::radians(fov / 2.0));

        assert(image_size.x > image_size.y);
        const auto ar = image_size.x / image_size.y; // aspect ratio
        const auto x = (2 * (pixel.x * image_size.x + 0.5) / image_size.x - 1) * angle * ar;
        const auto y = (1 - 2 * (pixel.y * image_size.y + 0.5) / image_size.y) * angle;
        return {position, renderer::normalize(glm::dvec3{x, y, -1.0} - position)};
    }

    auto handleEvent(const std::array<bool, sf::Keyboard::Key::KeyCount> &keys, const sf::Time &dt)
    {
        bool updated = false;

        if (keys[sf::Keyboard::Key::Escape]) {
            *this = renderer::Camera{{0, 0, 20}, {0, 0, -1}};
            updated = true;
        }

        if (keys[sf::Keyboard::Key::Z]) {
            position += toForward(orientation) * static_cast<double>(dt.asMilliseconds()) / 1000.0;
            updated = true;
        }
        if (keys[sf::Keyboard::Key::S]) {
            updated = true;
            position += toForward(orientation) * -1.0 * static_cast<double>(dt.asMilliseconds()) / 1000.0;
        }
        if (keys[sf::Keyboard::Key::Q]) {
            updated = true;
            position += toRight(orientation) * -1.0 * static_cast<double>(dt.asMilliseconds()) / 1000.0;
        }
        if (keys[sf::Keyboard::Key::D]) {
            updated = true;
            position += toRight(orientation) * static_cast<double>(dt.asMilliseconds()) / 1000.0;
        }
        if (keys[sf::Keyboard::Key::A]) {
            updated = true;
            position += toUp(orientation) * -1.0 * static_cast<double>(dt.asMilliseconds()) / 1000.0;
        }
        if (keys[sf::Keyboard::Key::E]) {
            updated = true;
            position += toUp(orientation) * static_cast<double>(dt.asMilliseconds()) / 1000.0;
        }

        return updated;

        //        if (keys[sf::Keyboard::Key::O]) {
        //            scene_updated = true;
        //            camera.orientation += glm::normalize(glm::euler(toRight(camera.orientation), dt.asMilliseconds()));
        //        }
        //        if (keys[sf::Keyboard::Key::L]) {
        //            scene_updated = true;
        //            camera.orientation +=
        //                glm::normalize(glm::euler(toRight(camera.orientation) * -1.0, dt.asMilliseconds()));
        //        }
        //        if (keys[sf::Keyboard::Key::K]) {
        //            scene_updated = true;
        //            camera.orientation +=
        //                glm::normalize(glm::euler(toUp(camera.orientation) * -1.0, dt.asMilliseconds()));
        //        }
        //        if (keys[sf::Keyboard::Key::M]) {
        //            scene_updated = true;
        //            camera.orientation += glm::normalize(glm::euler(toUp(camera.orientation), dt.asMilliseconds()));
        //        }
        //        if (keys[sf::Keyboard::Key::I]) {
        //            scene_updated = true;
        //            camera.orientation += glm::normalize(glm::euler(toForward(camera.orientation), dt.asMilliseconds()));
        //        }
        //        if (keys[sf::Keyboard::Key::P]) {
        //            scene_updated = true;
        //            camera.orientation +=
        //                glm::normalize(glm::euler(toForward(camera.orientation) * -1.0, dt.asMilliseconds()));
        //        }
    }
};

struct Scene {
    auto distance_estimator(glm::dvec3 &&point) const -> double
    {
        auto closest = std::numeric_limits<double>::max();
        for (const auto &i : spheres) { closest = std::min(closest, i.distance(point)); }
        return closest;
    }

    std::vector<Sphere> spheres;
};

struct RenderTarget : public sf::Drawable {
    RenderTarget(sf::Vector2i &&new_size)
    {
        shape = sf::RectangleShape({static_cast<float>(new_size.x), static_cast<float>(new_size.y)});
        setSize(std::move(new_size));
    }

    void setSize(sf::Vector2i &&new_size)
    {
        size = std::move(new_size);
        image.create(static_cast<std::uint32_t>(size.x), static_cast<std::uint32_t>(size.y));
        texture.loadFromImage(image);
        shape.setTexture(&texture, true);
    }

    void draw(sf::RenderTarget &target, sf::RenderStates states) const final { target.draw(shape, states); }

    using tracer = std::function<std::array<std::uint8_t, 4>(const Scene &scene, const Ray &)>;

    void each(const Scene &scene, const Camera &camera, const tracer &callback)
    {
        for (auto y = 0; y != size.y; y++) {
            for (auto x = 0; x != size.x; x++) {
                const auto color = callback(
                    scene,
                    camera.toRay(
                        {x / static_cast<double>(size.x), y / static_cast<double>(size.y)}, {size.x, size.y}));
                image.setPixel(
                    static_cast<std::uint32_t>(x),
                    static_cast<std::uint32_t>(y),
                    sf::Color(color[0], color[1], color[2], color[3]));
            }
        }

        texture.loadFromImage(image);
        shape.setTexture(&texture, true);
    }

    sf::Vector2i size;
    sf::Image image;
    sf::Texture texture;
    sf::RectangleShape shape;
};

struct Raymarcher {
    int max_ray_step = 100;
    double min_distance = 1e-6;

    constexpr auto render(const Scene &scene, const Ray &ray) const -> std::array<std::uint8_t, 4>
    {
        auto has_hit = false;
        auto total_distance = 0.0;
        auto step = 0;
        auto smallest_distance = std::numeric_limits<double>::max();
        for (; step != max_ray_step; step++) {
            const auto distance = scene.distance_estimator(ray.origin + total_distance * ray.direction);
            if (distance < smallest_distance) smallest_distance = distance;
            if (distance <= min_distance) {
                has_hit = true;
                break;
            }
            total_distance += distance;
        }

        if (has_hit) {
            const auto color =
                static_cast<std::uint8_t>((1.0 - step / static_cast<double>(max_ray_step)) * 255.0);
            return {color, color, color, 255};
        } else {
            const auto color = static_cast<std::uint8_t>(
                (1.0 - std::min(smallest_distance, min_distance * 1e5) / (min_distance * 1e5)) * 255.0);
            return {color, 0, 0, 255};
        }
    }
};

} // namespace renderer

using namespace renderer;

int main()
{
    std::srand(static_cast<std::uint32_t>(std::time(nullptr)));

    auto width = 800;
    auto height = 600;

    sf::RenderWindow window{
        sf::VideoMode{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)},
        "___",
        sf::Style::Resize | sf::Style::Close,
        sf::ContextSettings{}};
    ImGui::SFML::Init(window);

    renderer::RenderTarget target{{width, height}};
    target.setSize({10, 10});

    renderer::Camera camera{{0, 0, 20}, {0, 0, -1}};

    renderer::Raymarcher raymarcher{};

    renderer::Scene scene;
    scene.spheres.emplace_back(glm::dvec3{0, 0, 0}, 1.0);
    for (int i = 0; i != 10; i++) {
        scene.spheres.emplace_back(
            glm::dvec3{std::rand() % 100 - 50, std::rand() % 100 - 50, std::rand() % 100 - 50}, std::rand() % 5);

        // scene.spheres.emplace_back(glm::dvec3{10, 0, 15}, 4);
        // scene.spheres.emplace_back(glm::dvec3{1, 0, 10}, 3);
    }

    bool scene_updated = true;
    bool auto_scale = false;

    std::array<bool, sf::Keyboard::Key::KeyCount> keys;
    std::fill(keys.begin(), keys.end(), false);

    sf::Clock clock;
    while (window.isOpen()) {
        const auto dt = clock.restart();
        sf::Event event{};
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);
            if (event.type == sf::Event::Closed) {
                window.close();
            } else if (event.type == sf::Event::KeyPressed) {
                keys[event.key.code] = true;
            } else if (event.type == sf::Event::KeyReleased) {
                keys[event.key.code] = false;
            }
        }

        scene_updated |= camera.handleEvent(keys, dt);

        // const auto str = std::accumulate(
        //    keys.begin(), keys.end(), std::string{}, [](auto s, bool b) { return s + (b ? "1" : "0"); });
        // spdlog::warn("{}", str);

        ImGui::SFML::Update(window, dt);

        ImGui::Begin("Setting");
        auto size = std::to_array({target.size.x, target.size.y});
        if (ImGui::InputInt2("Target size = ", size.data())) {
            target.setSize({size[0], size[1]});
            width = size[0];
            height = size[1];
            scene_updated = true;
        }
        if (ImGui::Checkbox("Auto Scale", &auto_scale)) { scene_updated = true; }
        glm::vec3 tmp_pos = camera.position;
        if (ImGui::InputFloat3("Camera Position", &tmp_pos.x)) {
            scene_updated = true;
            camera.position = tmp_pos;
            target.setSize({10, 10});
        }
        glm::vec3 tmp_ori = camera.orientation;
        if (ImGui::InputFloat3("Camera Orientation", &tmp_ori.x)) {
            scene_updated = true;
            camera.orientation = tmp_ori;
            target.setSize({10, 10});
        }
        ImGui::Separator();
        float tmp = static_cast<float>(raymarcher.min_distance);
        if (ImGui::InputFloat("Min distance", &tmp, 0.0f, 0.0f, "%.10f")) {
            scene_updated = true;
            raymarcher.min_distance = tmp;
        }
        int step = raymarcher.max_ray_step;
        if (ImGui::InputInt("Max step", &step)) {
            scene_updated = true;
            raymarcher.max_ray_step = step;
        }
        ImGui::End();

        if (scene_updated) {
            const auto window_size = sf::Vector2i{window.getSize()};
            if (auto_scale && dt.asMilliseconds() <= 500
                && (target.size.y < window_size.y || target.size.x < window_size.x)) {
                const auto new_size = target.size + target.size / 10 + sf::Vector2i{1, 1};
                target.setSize(sf::Vector2i{
                    std::clamp(new_size.x * (window_size.x / window_size.y), 0, window_size.x),
                    std::clamp(new_size.y, 0, window_size.y)});
                scene_updated = true;
                width = target.size.x;
                height = target.size.y;
            } else {
                scene_updated = false;
            }
            target.each(
                scene, camera, [&raymarcher](const Scene &s, const Ray &r) { return raymarcher.render(s, r); });
        }

        window.clear(sf::Color::Black);
        window.draw(target);
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();

    return 0;
}
