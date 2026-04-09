module;

#include <cstddef>
#include <span>
#include <vector>

export module sim.particle_simulation;

export namespace sim {

struct SimulationConfig {
    std::size_t particle_count = 1'000;
    float bounds_width = 1280.0f * 2;
    float bounds_height = 720.0f * 2;
    float attraction_strength = 180.0f;
    float softening = 50.0f;
    float damping = 0.997f;
    float max_speed = 400.0f;
};

struct ParticleView {
    std::span<float> x;
    std::span<float> y;
    std::span<float> vx;
    std::span<float> vy;
    std::span<float> mass;
};

struct ConstParticleView {
    std::span<const float> x;
    std::span<const float> y;
    std::span<const float> vx;
    std::span<const float> vy;
    std::span<const float> mass;
};

class ParticleStorage {
   private:
    std::vector<float> x_;
    std::vector<float> y_;
    std::vector<float> vx_;
    std::vector<float> vy_;
    std::vector<float> mass_;

   public:
    explicit ParticleStorage(std::size_t particle_count = 0);

    [[nodiscard]] std::size_t size() const noexcept;
    void resize(std::size_t particle_count);

    [[nodiscard]] ParticleView view() noexcept;
    [[nodiscard]] ConstParticleView view() const noexcept;
};

class ParticleSimulation {
   private:
    void initialize_particles() noexcept;

    ParticleStorage storage_;
    SimulationConfig config_;

   public:
    explicit ParticleSimulation(SimulationConfig config = {});

    void resize_bounds(float width, float height) noexcept;
    void step(float dt) noexcept;

    [[nodiscard]] const SimulationConfig& config() const noexcept;
    [[nodiscard]] ConstParticleView particles() const noexcept;
};

}  // namespace sim
