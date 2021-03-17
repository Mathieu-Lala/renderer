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

namespace renderer {

constexpr auto length(glm::dvec3 &&vec) -> double
{
    return std::sqrt(std::pow(vec.x, 2) + std::pow(vec.y, 2) + std::pow(vec.z, 2));
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

    constexpr auto toRay() const -> Ray { return {position, {0, 0, 1}}; }
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

    using tracer = std::function<std::array<std::uint8_t, 4>(glm::dvec2 &&, const Scene &scene, const Ray &)>;

    void each(const Scene &scene, const Camera &camera, const tracer &callback)
    {
        const auto ray = camera.toRay();
        spdlog::info("{} {} {}", ray.origin.x, ray.origin.y, ray.origin.z);

        for (auto y = 0; y != size.y; y++) {
            for (auto x = 0; x != size.x; x++) {
                const auto color =
                    callback({x / static_cast<double>(size.x), y / static_cast<double>(size.y)}, scene, ray);
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

constexpr auto raymarching(glm::dvec2 &&pixel, const Scene &scene, const Ray &ray) -> std::array<std::uint8_t, 4>
{
    constexpr auto max_ray_step = 100.0;
    constexpr auto min_distance = std::numeric_limits<double>::epsilon() * 10;

    const auto from = ray.origin;
    // todo : apply ray.direction
    const auto direction = glm::dvec3({pixel.x, pixel.y, 1});

    auto total_distance = 0.0;

    auto step = 0.0;
    for (; step != max_ray_step; step++) {
        auto point = from + total_distance * direction;
        const auto distance = scene.distance_estimator(std::move(point));
        total_distance += distance;
        if (distance < min_distance) break;
    }

    const auto color = static_cast<std::uint8_t>((1.0 - step / max_ray_step) * 255.0);
    return {color, color, color, 255};
}

} // namespace renderer


int main()
{
    auto width = 800;
    auto height = 800;

    sf::RenderWindow window{
        sf::VideoMode{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)},
        "",
        sf::Style::Resize | sf::Style::Close,
        sf::ContextSettings{}};
    ImGui::SFML::Init(window);

    renderer::RenderTarget target{{width, height}};
    target.setSize({10, 10});

    renderer::Camera camera{{0, 0, 0}};

    renderer::Scene scene;
    scene.spheres.emplace_back(glm::dvec3{10, 0, 15}, 4);
    scene.spheres.emplace_back(glm::dvec3{1, 0, 10}, 3);

    bool scene_updated = true;
    bool auto_scale = true;

    sf::Clock clock;
    while (window.isOpen()) {
        const auto dt = clock.restart();
        sf::Event event{};
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);
            if (event.type == sf::Event::Closed) { window.close(); }
        }

        ImGui::SFML::Update(window, dt);

        ImGui::Begin("Setting");
        auto size = std::to_array({width, height});
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
        ImGui::End();

        if (scene_updated) {
            const auto window_size = sf::Vector2i{window.getSize()};
            if (auto_scale && dt.asMilliseconds() <= 500
                && (target.size.y < window_size.y || target.size.x < window_size.x)) {
                const auto new_size = target.size + target.size / 10 + sf::Vector2i{1, 1};
                target.setSize(sf::Vector2i{
                    std::clamp(new_size.x, 0, window_size.x), std::clamp(new_size.y, 0, window_size.y)});
                scene_updated = true;
                width = target.size.x;
                height = target.size.y;
            } else {
                scene_updated = false;
            }
            target.each(scene, camera, renderer::raymarching);
        }

        window.clear(sf::Color::Black);
        window.draw(target);
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();

    return 0;
}
