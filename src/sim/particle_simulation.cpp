module;

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <mdspan>
#include <span>

module sim.particle_simulation;

namespace sim {

// bins particles into grid cells based on their position
void SpatialGrid::build(
    std::span<const float> x,
    std::span<const float> y,
    float bounds_width,
    float bounds_height,
    float cell_size) noexcept {
    cell_size_ = cell_size;
    grid_w_ = std::max(1, static_cast<int>(std::ceil(bounds_width / cell_size)));
    grid_h_ = std::max(1, static_cast<int>(std::ceil(bounds_height / cell_size)));

    const int total_cells = grid_w_ * grid_h_;
    const std::size_t n = x.size();

    cell_counts_.assign(total_cells, 0u);
    cell_starts_.resize(total_cells);
    sorted_indices_.resize(n);
    particle_cells_.resize(n);

    // assign each particle to a cell and count particles per cell
    for (std::size_t i = 0; i < n; ++i) {
        const int cx = std::clamp(static_cast<int>(x[i] / cell_size_), 0, grid_w_ - 1);
        const int cy = std::clamp(static_cast<int>(y[i] / cell_size_), 0, grid_h_ - 1);
        const auto cell = cy * grid_w_ + cx;
        particle_cells_[i] = cell;
        ++cell_counts_[cell];
    }

    // exclusive prefix sum to get the start offset of each cell in sorted_indices_
    std::uint32_t running = 0;

    for (int c = 0; c < total_cells; ++c) {
        cell_starts_[c] = running;
        running += cell_counts_[c];
    }

    // scatter particles into sorted order using a copy of starts as write cursors
    std::vector<std::uint32_t> write_at(cell_starts_);

    for (std::size_t i = 0; i < n; ++i) {
        sorted_indices_[write_at[particle_cells_[i]]++] = static_cast<std::uint32_t>(i);
    }
}

// returns the indices of particles in the cell at (cx, cy)
std::span<const std::uint32_t> SpatialGrid::cell_particles(int cx, int cy) const noexcept {
    if (cx < 0 || cx >= grid_w_ || cy < 0 || cy >= grid_h_) {
        return {};
    }

    // 2d view over the flat cell arrays for clean row,col indexing
    const auto starts = std::mdspan(cell_starts_.data(), grid_h_, grid_w_);
    const auto counts = std::mdspan(cell_counts_.data(), grid_h_, grid_w_);

    return std::span{sorted_indices_}.subspan(starts[cy, cx], counts[cy, cx]);
}

int SpatialGrid::grid_w() const noexcept { return grid_w_; }

int SpatialGrid::grid_h() const noexcept { return grid_h_; }

float SpatialGrid::cell_size() const noexcept { return cell_size_; }

namespace {

struct NodePos {
    float x, y;
};

constexpr std::array<NodePos, 55> kPositions = {{
    {1000, 650},
    {930, 600},
    {940, 715},
    {1060, 720},
    {1080, 600},
    {1050, 668},
    {968, 672},
    {1000, 582},
    {1042, 730},
    {1068, 558},
    {840, 525},
    {718, 443},
    {608, 368},
    {508, 303},
    {418, 248},
    {1178, 565},
    {1328, 530},
    {1488, 500},
    {1645, 476},
    {1795, 460},
    {1945, 448},
    {918, 800},
    {848, 933},
    {798, 1053},
    {768, 1163},
    {746, 1263},
    {733, 1348},
    {726, 1403},
    {898, 1073},
    {993, 1103},
    {553, 298},
    {633, 288},
    {673, 343},
    {643, 423},
    {1478, 420},
    {1548, 435},
    {1578, 503},
    {1543, 568},
    {1468, 573},
    {648, 1408},
    {633, 1373},
    {668, 1428},
    {728, 1428},
    {792, 1418},
    {818, 1391},
    {373, 1018},
    {303, 958},
    {288, 1038},
    {323, 1103},
    {393, 1118},
    {1660, 406},
    {1725, 416},
    {1745, 486},
    {1685, 526},
    {358, 203},
}};

constexpr std::array<Edge, 86> kEdges = {{
    {0, 1},
    {0, 2},
    {0, 3},
    {0, 4},
    {0, 5},
    {0, 6},
    {0, 7},
    {0, 8},
    {0, 9},
    {1, 2},
    {2, 3},
    {3, 4},
    {4, 5},
    {5, 6},
    {6, 7},
    {7, 8},
    {8, 9},
    {9, 1},
    {1, 6},
    {2, 5},
    {3, 9},
    {4, 8},
    {1, 10},
    {7, 10},
    {10, 11},
    {11, 12},
    {12, 13},
    {13, 14},
    {12, 30},
    {12, 31},
    {12, 32},
    {12, 33},
    {13, 30},
    {13, 31},
    {11, 33},
    {14, 54},
    {4, 15},
    {9, 15},
    {15, 16},
    {16, 17},
    {17, 18},
    {18, 19},
    {19, 20},
    {17, 34},
    {17, 35},
    {17, 36},
    {17, 37},
    {17, 38},
    {18, 36},
    {18, 37},
    {16, 35},
    {18, 50},
    {19, 50},
    {50, 51},
    {51, 52},
    {52, 53},
    {53, 50},
    {20, 52},
    {2, 21},
    {6, 21},
    {21, 22},
    {22, 23},
    {23, 24},
    {24, 25},
    {25, 26},
    {26, 27},
    {22, 28},
    {23, 28},
    {28, 29},
    {27, 39},
    {27, 40},
    {27, 41},
    {27, 42},
    {27, 43},
    {27, 44},
    {26, 44},
    {25, 39},
    {24, 45},
    {29, 45},
    {45, 46},
    {45, 47},
    {45, 48},
    {45, 49},
    {10, 21},
    {16, 22},
    {16, 23},
}};

// initialize the state of the particles
void initialize_particle_state(ParticleView particles) noexcept {
    const float mass = 1.0f;
    const float radius = 6.0f;

    for (std::size_t i = 0; i < std::min(particles.x.size(), kPositions.size()); ++i) {
        particles.x[i] = kPositions[i].x;
        particles.y[i] = kPositions[i].y;
        particles.vx[i] = 0.0f;
        particles.vy[i] = 0.0f;
        particles.mass[i] = mass;
        particles.radius[i] = radius;
    }
}

// run once per simulation step to update particle positions and velocities
void step_particles(
    ParticleView particles,
    const SimulationConfig& config,
    float dt,
    std::span<const Edge> edges,
    std::span<float> ax,
    std::span<float> ay) noexcept {
    const std::size_t n = particles.x.size();

    // first pass: only compute forces for all particles in parallel
    // reads only from positions, writes only to ax/ay
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, n), [&](const tbb::blocked_range<std::size_t>& range) {
        for (std::size_t i = range.begin(); i != range.end(); ++i) {
            float fax = 0.0f;
            float fay = 0.0f;

            // pairwise 1/r^2 force between all particles
            // positive = attractive (gravity)
            // negative = repulsive (coulomb)
            for (std::size_t j = 0; j < n; ++j) {
                if (j == i) {
                    continue;
                }

                // dx/dy point from j toward i (away from j)
                const float dx = particles.x[i] - particles.x[j];
                const float dy = particles.y[i] - particles.y[j];

                const float dist_sq_soft = dx * dx + dy * dy + config.softening;
                const float dist_soft = std::sqrt(dist_sq_soft);
                const float inv_dist_cubed = 1.0f / (dist_sq_soft * dist_soft);

                // negative force flips direction
                fax -= config.force_strength * dx * inv_dist_cubed;
                fay -= config.force_strength * dy * inv_dist_cubed;
            }

            // spring attraction along graph edges
            for (const auto& [a, b] : edges) {
                if (a != i && b != i) {
                    continue;
                }

                const std::size_t neighbor = (a == i) ? b : a;

                // dx/dy point from i toward neighbor
                const float dx = particles.x[neighbor] - particles.x[i];
                const float dy = particles.y[neighbor] - particles.y[i];
                const float dist_sq = dx * dx + dy * dy;

                if (dist_sq == 0.0f) {
                    continue;
                }

                const float dist = std::sqrt(dist_sq);

                // force = spring_strength * (dist - rest_length)
                const float force = config.spring_strength * (dist - config.spring_rest_length) / dist;
                fax += dx * force;
                fay += dy * force;
            }

            ax[i] = fax;
            ay[i] = fay;
        }
    });

    // second pass: integrate velocities and positions in parallel
    // read from ax/ay, write to vx/vy/x/y per-particle
    const float max_speed_squared = config.max_speed * config.max_speed;

    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, n), [&](const tbb::blocked_range<std::size_t>& range) {
        for (std::size_t index = range.begin(); index != range.end(); ++index) {
            // set velocity to current velocity plus acceleration
            // with damping to prevent infinite acceleration
            particles.vx[index] = (particles.vx[index] + ax[index] * dt) * config.damping;
            particles.vy[index] = (particles.vy[index] + ay[index] * dt) * config.damping;

            // we compare with speed_squared to avoid a sqrt, as sqrt is expensive
            const float speed_squared =
                particles.vx[index] * particles.vx[index] +
                particles.vy[index] * particles.vy[index];

            // if speed is above max, we scale it down
            if (speed_squared > max_speed_squared) {
                const float speed_scale = config.max_speed / std::sqrt(speed_squared);
                particles.vx[index] *= speed_scale;
                particles.vy[index] *= speed_scale;
            }

            // update the position based on the velocity
            particles.x[index] += particles.vx[index] * dt;
            particles.y[index] += particles.vy[index] * dt;

            // reflect off bounds using the particle edge, not the center
            const float r = particles.radius[index];

            if (particles.x[index] < r) {
                particles.x[index] = r;
                particles.vx[index] = -particles.vx[index];
            } else if (particles.x[index] > config.bounds_width - r) {
                particles.x[index] = config.bounds_width - r;
                particles.vx[index] = -particles.vx[index];
            }

            if (particles.y[index] < r) {
                particles.y[index] = r;
                particles.vy[index] = -particles.vy[index];
            } else if (particles.y[index] > config.bounds_height - r) {
                particles.y[index] = config.bounds_height - r;
                particles.vy[index] = -particles.vy[index];
            }
        }
    });
}

}  // namespace

ParticleStorage::ParticleStorage(std::size_t particle_count) {
    resize(particle_count);
}

std::size_t ParticleStorage::size() const noexcept {
    return x_.size();
}

bool ParticleStorage::empty() const noexcept {
    return x_.empty();
}

// resize the storage to the given number of particles
void ParticleStorage::resize(std::size_t particle_count) {
    x_.resize(particle_count);
    y_.resize(particle_count);
    vx_.resize(particle_count);
    vy_.resize(particle_count);
    mass_.resize(particle_count);
    radius_.resize(particle_count);
}

ParticleView ParticleStorage::view() noexcept {
    return ParticleView{
        .x = x_,
        .y = y_,
        .vx = vx_,
        .vy = vy_,
        .mass = mass_,
        .radius = radius_,
    };
}

ConstParticleView ParticleStorage::view() const noexcept {
    return ConstParticleView{
        .x = x_,
        .y = y_,
        .vx = vx_,
        .vy = vy_,
        .mass = mass_,
        .radius = radius_,
    };
}

ParticleSimulation::ParticleSimulation(SimulationConfig config)
    : storage_(config.particle_count),
      config_(config),
      ax_(config.particle_count, 0.0f),
      ay_(config.particle_count, 0.0f) {
    initialize_particles();
}

void ParticleSimulation::resize_bounds(float width, float height) noexcept {
    config_.bounds_width = std::max(width, 1.0f);
    config_.bounds_height = std::max(height, 1.0f);
}

void ParticleSimulation::step(float dt) noexcept {
    if (dt <= 0.0f || storage_.empty()) {
        return;
    }

    // rebuild grid every frame
    grid_.build(storage_.view().x, storage_.view().y, config_.bounds_width, config_.bounds_height, config_.cell_size);

    step_particles(storage_.view(), config_, dt, edges_, ax_, ay_);
}

const SimulationConfig& ParticleSimulation::config() const noexcept {
    return config_;
}

ConstParticleView ParticleSimulation::particles() const noexcept {
    return storage_.view();
}

std::span<const Edge> ParticleSimulation::edges() const noexcept {
    return edges_;
}

void ParticleSimulation::initialize_particles() noexcept {
    initialize_particle_state(storage_.view());
    edges_.assign(kEdges.begin(), kEdges.end());
}

}  // namespace sim
