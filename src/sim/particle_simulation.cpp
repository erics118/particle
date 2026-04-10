module;

#include <tbb/blocked_range.h>
#include <tbb/parallel_for.h>

#include <algorithm>
#include <cmath>
#include <mdspan>
#include <random>

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

constexpr float kTau = 6.28318530717958647692f;

// seed for deterministic RNG
constexpr std::uint32_t kRandomSeed = 0xBEEFEDu;

// initialize the state of the particles
void initialize_particle_state(ParticleView particles, const SimulationConfig& config) noexcept {
    const float center_x = config.bounds_width * 0.5f;
    const float center_y = config.bounds_height * 0.5f;

    const float cloud_radius = std::min(config.bounds_width, config.bounds_height) * 0.3f;

    const float initial_speed = 10.0f;

    std::mt19937 rng(kRandomSeed);

    std::uniform_real_distribution<float> angle_distribution(0.0f, kTau);
    std::uniform_real_distribution<float> radius_distribution(0.0f, 1.0f);
    std::uniform_real_distribution<float> speed_distribution(-initial_speed, initial_speed);
    std::uniform_real_distribution<float> mass_distribution(0.5f, 3.0f);

    // generate a random position, velocity, mass, and radius for each particle
    for (std::size_t index = 0; index < particles.x.size(); ++index) {
        const float angle = angle_distribution(rng);

        // sqrt maps uniform [0,1] to uniform circle distribution
        const float placement_radius = std::sqrt(radius_distribution(rng)) * cloud_radius;
        const float offset_x = std::cos(angle) * placement_radius;
        const float offset_y = std::sin(angle) * placement_radius;

        particles.x[index] = center_x + offset_x;
        particles.y[index] = center_y + offset_y;

        particles.vx[index] = speed_distribution(rng);
        particles.vy[index] = speed_distribution(rng);

        particles.mass[index] = mass_distribution(rng);

        // radius directly proportional to mass
        particles.radius[index] = mass_distribution(rng) * 2.0f;
    }
}

// run once per simulation step to update particle positions and velocities
void step_particles(
    ParticleView particles,
    const SimulationConfig& config,
    float dt,
    SpatialGrid& grid,
    std::span<float> ax,
    std::span<float> ay) noexcept {
    // build grid from current positions before computing forces
    grid.build(particles.x, particles.y, config.bounds_width, config.bounds_height, config.cell_size);

    const float center_x = config.bounds_width * 0.5f;
    const float center_y = config.bounds_height * 0.5f;

    // prevent division by zero, and create infinite force at zero distance
    const float inverse_softening = 1.0f / config.softening;

    // compare squared distances to avoid sqrt in the neighbor loop
    const float interaction_radius_sq = config.interaction_radius * config.interaction_radius;

    const std::size_t n = particles.x.size();

    // first pass: only compute forces for all particles in parallel
    // reads only from positions, writes only to ax/ay
    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, n), [&](const tbb::blocked_range<std::size_t>& range) {
        for (std::size_t index = range.begin(); index != range.end(); ++index) {
            // global gravity: inverse cube falloff from the center
            const float dx = center_x - particles.x[index];
            const float dy = center_y - particles.y[index];
            const float distance_squared = dx * dx + dy * dy + config.softening;
            const float inverse_distance = 1.0f / std::sqrt(distance_squared);
            const float falloff = inverse_distance * inverse_distance * inverse_distance;

            const float gravity = config.attraction_strength * inverse_softening;
            const float acceleration_scale = gravity * particles.mass[index] * falloff;

            float fax = dx * acceleration_scale;
            float fay = dy * acceleration_scale;

            // local repulsion: linear falloff from neighbors within interaction_radius
            // prevents particles from being too close to each other
            const int cx = std::clamp(static_cast<int>(particles.x[index] / config.cell_size), 0, grid.grid_w() - 1);
            const int cy = std::clamp(static_cast<int>(particles.y[index] / config.cell_size), 0, grid.grid_h() - 1);

            // loop over the 3x3 neighborhood of cells
            for (int ny = cy - 1; ny <= cy + 1; ++ny) {
                for (int nx = cx - 1; nx <= cx + 1; ++nx) {
                    for (const auto j : grid.cell_particles(nx, ny)) {
                        // if is the current particle, skip
                        if (j == index) {
                            continue;
                        }

                        const float rdx = particles.x[index] - particles.x[j];
                        const float rdy = particles.y[index] - particles.y[j];
                        const float dist_sq = rdx * rdx + rdy * rdy;

                        // same position or too far away, so no force
                        if (dist_sq == 0.0f || dist_sq >= interaction_radius_sq) {
                            continue;
                        }

                        const float dist = std::sqrt(dist_sq);
                        const float contact = particles.radius[index] + particles.radius[j];

                        float force{};
                        if (dist < contact) {
                            // overlap: strong force proportional to penetration depth
                            const float penetration = contact - dist;
                            force = config.repulsion_strength * (1.0f + penetration / contact) / dist;
                        } else {
                            // soft repulsion: linear falloff from contact surface to interaction_radius
                            const float t = (dist - contact) / (config.interaction_radius - contact);
                            force = config.repulsion_strength * (1.0f - t) / dist;
                        }

                        // apply the repulsion force away from the neighbor
                        fax += rdx * force;
                        fay += rdy * force;
                    }
                }
            }

            ax[index] = fax;
            ay[index] = fay;
        }
    });

    // second pass: integrate velocities and positions in parallel
    // read from ax/ay, write to vx/vy/x/y per-particle
    const float max_speed_squared = config.max_speed * config.max_speed;

    tbb::parallel_for(tbb::blocked_range<std::size_t>(0, n), [&](const tbb::blocked_range<std::size_t>& range) {
        for (std::size_t index = range.begin(); index != range.end(); ++index) {
            // set velocity to current velocity plus acceleration towards the center
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

    step_particles(storage_.view(), config_, dt, grid_, ax_, ay_);
}

const SimulationConfig& ParticleSimulation::config() const noexcept {
    return config_;
}

ConstParticleView ParticleSimulation::particles() const noexcept {
    return storage_.view();
}

void ParticleSimulation::initialize_particles() noexcept {
    initialize_particle_state(storage_.view(), config_);
}

}  // namespace sim
