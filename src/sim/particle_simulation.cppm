module;

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

export module sim.particle_simulation;

export namespace sim {

// an undirected edge between two particle indices
using Edge = std::pair<std::uint32_t, std::uint32_t>;

struct SimulationConfig {
    std::size_t particle_count = 55;
    float bounds_width = 1280.0f * 2;
    float bounds_height = 720.0f * 2;

    // unified 1/r^2 pairwise force
    // positive = attractive (gravity)
    // negative = repulsive (coulomb)
    float force_strength = -50000.0f;

    // prevents excessive forces at close distances
    float softening = 1000.0f;

    // spring attraction along graph edges
    float spring_strength = 0.9f;
    float spring_rest_length = 0.0f;

    float damping = 0.97f;
    float max_speed = 900.0f;

    // for spatial grid: cell_size >= interaction_radius, so a 3x3 search covers all neighbors
    float cell_size = 500.0f;
};

struct ParticleView {
    std::span<float> x;
    std::span<float> y;
    std::span<float> vx;
    std::span<float> vy;
    std::span<float> mass;
    std::span<float> radius;
};

struct ConstParticleView {
    std::span<const float> x;
    std::span<const float> y;
    std::span<const float> vx;
    std::span<const float> vy;
    std::span<const float> mass;
    std::span<const float> radius;
};

class ParticleStorage {
   private:
    std::vector<float> x_;
    std::vector<float> y_;
    std::vector<float> vx_;
    std::vector<float> vy_;
    std::vector<float> mass_;
    std::vector<float> radius_;

   public:
    explicit ParticleStorage(std::size_t particle_count = 0);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    void resize(std::size_t particle_count);

    [[nodiscard]] ParticleView view() noexcept;
    [[nodiscard]] ConstParticleView view() const noexcept;
};

// uniform grid for O(1) neighbor lookup
// bounds divided into cells of cell_size * cell_size
class SpatialGrid {
   private:
    float cell_size_{};
    int grid_w_{};
    int grid_h_{};

    std::vector<std::uint32_t> cell_counts_;
    std::vector<std::uint32_t> cell_starts_;
    std::vector<std::uint32_t> sorted_indices_;
    std::vector<std::uint32_t> particle_cells_;

   public:
    void build(
        std::span<const float> x,
        std::span<const float> y,
        float bounds_width,
        float bounds_height,
        float cell_size) noexcept;

    [[nodiscard]] std::span<const std::uint32_t> cell_particles(int cx, int cy) const noexcept;

    [[nodiscard]] int grid_w() const noexcept;

    [[nodiscard]] int grid_h() const noexcept;

    [[nodiscard]] float cell_size() const noexcept;
};

class ParticleSimulation {
   private:
    void initialize_particles() noexcept;

    ParticleStorage storage_;
    SimulationConfig config_;
    SpatialGrid grid_;  // persistent across steps to reuse vector allocations

    // force accumulators: written in force pass, read in integration pass
    // kept here to reuse allocations across frames
    std::vector<float> ax_;
    std::vector<float> ay_;

    std::vector<Edge> edges_;

   public:
    explicit ParticleSimulation(SimulationConfig config = {});

    void resize_bounds(float width, float height) noexcept;
    void step(float dt) noexcept;

    [[nodiscard]] const SimulationConfig& config() const noexcept;
    [[nodiscard]] ConstParticleView particles() const noexcept;
    [[nodiscard]] std::span<const Edge> edges() const noexcept;
};

}  // namespace sim
