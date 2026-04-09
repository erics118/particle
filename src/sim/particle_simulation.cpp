module;

#include <algorithm>
#include <cmath>
#include <random>

module sim.particle_simulation;

namespace sim {

namespace {

constexpr float kTau = 6.28318530717958647692f;

// seed for deterministic RNG
constexpr std::uint32_t kRandomSeed = 0xBEEFEDu;

// initialize the state of the particles
void initialize_particle_state(ParticleView particles, const SimulationConfig& config) noexcept {
    const float center_x = config.bounds_width * 0.5f;
    const float center_y = config.bounds_height * 0.5f;

    const float cloud_radius = std::min(config.bounds_width, config.bounds_height) * 0.3f;

    const float initial_speed = 100.0f;

    std::mt19937 rng(kRandomSeed);

    std::uniform_real_distribution<float> angle_distribution(0.0f, kTau);
    std::uniform_real_distribution<float> radius_distribution(0.0f, 1.0f);
    std::uniform_real_distribution<float> speed_distribution(-initial_speed, initial_speed);
    std::uniform_real_distribution<float> mass_distribution(0.5f, 5.0f);

    // generate a random position, velocity, and mass for each particle
    for (std::size_t index = 0; index < particles.x.size(); ++index) {
        const float angle = angle_distribution(rng);

        // sqrt maps uniform [0,1] to uniform circle distribution
        const float radius = std::sqrt(radius_distribution(rng)) * cloud_radius;
        const float offset_x = std::cos(angle) * radius;
        const float offset_y = std::sin(angle) * radius;

        particles.x[index] = center_x + offset_x;
        particles.y[index] = center_y + offset_y;

        particles.vx[index] = speed_distribution(rng);
        particles.vy[index] = speed_distribution(rng);

        particles.mass[index] = mass_distribution(rng);
    }
}

// run once per simulation step to update particle positions and velocities
void step_particles(ParticleView particles, const SimulationConfig& config, float dt) noexcept {
    const float center_x = config.bounds_width * 0.5f;
    const float center_y = config.bounds_height * 0.5f;

    // prevent division by zero, and create infinite force at zero distance
    const float inverse_softening = 1.0f / config.softening;

    // prevent sqrt in hot paths
    const float max_speed_squared = config.max_speed * config.max_speed;

    for (std::size_t index = 0; index < particles.x.size(); ++index) {
        const float dx = center_x - particles.x[index];
        const float dy = center_y - particles.y[index];

        const float distance_squared = dx * dx + dy * dy + config.softening;
        const float inverse_distance = 1.0f / std::sqrt(distance_squared);
        const float inverse_distance_cubed = inverse_distance * inverse_distance * inverse_distance;

        // strength of acceleration, inverse_softening to prevent infinite acceleration
        const float gravity = config.attraction_strength * inverse_softening;

        // force weakens with distance
        const float falloff = inverse_distance_cubed;

        const float acceleration_scale = gravity * particles.mass[index] * falloff;

        // set velocity to current velocity plus acceleration towards the center
        // with damping to prevent infinite acceleration
        particles.vx[index] = (particles.vx[index] + dx * acceleration_scale * dt) * config.damping;
        particles.vy[index] = (particles.vy[index] + dy * acceleration_scale * dt) * config.damping;

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

        // if the particle goes out of bounds, reflect it back and reverse its velocity
        if (particles.x[index] < 0.0f) {
            particles.x[index] = 0.0f;
            particles.vx[index] = -particles.vx[index];
        } else if (particles.x[index] > config.bounds_width) {
            particles.x[index] = config.bounds_width;
            particles.vx[index] = -particles.vx[index];
        }

        if (particles.y[index] < 0.0f) {
            particles.y[index] = 0.0f;
            particles.vy[index] = -particles.vy[index];
        } else if (particles.y[index] > config.bounds_height) {
            particles.y[index] = config.bounds_height;
            particles.vy[index] = -particles.vy[index];
        }
    }
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
}

ParticleView ParticleStorage::view() noexcept {
    return ParticleView{
        .x = x_,
        .y = y_,
        .vx = vx_,
        .vy = vy_,
        .mass = mass_,
    };
}

ConstParticleView ParticleStorage::view() const noexcept {
    return ConstParticleView{
        .x = x_,
        .y = y_,
        .vx = vx_,
        .vy = vy_,
        .mass = mass_,
    };
}

ParticleSimulation::ParticleSimulation(SimulationConfig config)
    : storage_(config.particle_count),
      config_(config) {
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

    step_particles(storage_.view(), config_, dt);
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
