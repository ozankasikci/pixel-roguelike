#include "engine/particles/ParticlePool.h"
#include <cassert>
#include <cmath>

int main() {
    using namespace particles;

    // Basic spawn and count
    {
        ParticlePool pool(64);
        assert(pool.aliveCount() == 0 && pool.capacity() == 64);
        pool.spawn(glm::vec3(1,2,3), glm::vec3(0,1,0), glm::vec4(1), 0.5f, 2.0f, 0.0f);
        assert(pool.aliveCount() == 1);
        assert(pool.positions()[0] == glm::vec3(1,2,3));
        assert(std::fabs(pool.lifetimes()[0] - 2.0f) < 1e-5f);
    }

    // Kill via swap-and-pop
    {
        ParticlePool pool(64);
        pool.spawn(glm::vec3(1,0,0), {}, glm::vec4(1), 0.1f, 1.0f, 0.0f);
        pool.spawn(glm::vec3(2,0,0), {}, glm::vec4(1), 0.1f, 1.0f, 0.0f);
        pool.spawn(glm::vec3(3,0,0), {}, glm::vec4(1), 0.1f, 1.0f, 0.0f);
        assert(pool.aliveCount() == 3);
        pool.kill(0);
        assert(pool.aliveCount() == 2);
        assert(pool.positions()[0] == glm::vec3(3,0,0));
    }

    // Capacity enforcement
    {
        ParticlePool pool(2);
        pool.spawn({}, {}, glm::vec4(1), 0.1f, 1.0f, 0.0f);
        pool.spawn({}, {}, glm::vec4(1), 0.1f, 1.0f, 0.0f);
        assert(pool.aliveCount() == 2);
        pool.spawn({}, {}, glm::vec4(1), 0.1f, 1.0f, 0.0f);
        assert(pool.aliveCount() == 2);
    }

    // Instance buffer packing
    {
        ParticlePool pool(64);
        pool.spawn(glm::vec3(1,0,0), {}, glm::vec4(1,0,0,1), 0.5f, 5.0f, 0.0f);
        pool.ages()[0] = 2.5f;
        std::vector<ParticleInstance> inst(1);
        pool.packInstances(inst.data(), 1);
        assert(inst[0].position == glm::vec3(1,0,0));
        assert(std::fabs(inst[0].size - 0.5f) < 1e-5f);
        assert(std::fabs(inst[0].normalizedAge - 0.5f) < 1e-5f);
    }

    // Sorted packing
    {
        ParticlePool pool(64);
        pool.spawn(glm::vec3(0,0,-5), {}, glm::vec4(1), 0.1f, 1.0f, 0.0f);
        pool.spawn(glm::vec3(0,0,-1), {}, glm::vec4(1), 0.1f, 1.0f, 0.0f);
        pool.spawn(glm::vec3(0,0,-3), {}, glm::vec4(1), 0.1f, 1.0f, 0.0f);
        std::vector<uint32_t> indices(3);
        pool.sortByDistance(glm::vec3(0), indices.data());
        assert(pool.positions()[indices[0]].z == -5.0f);
        assert(pool.positions()[indices[1]].z == -3.0f);
        assert(pool.positions()[indices[2]].z == -1.0f);
        std::vector<ParticleInstance> inst(3);
        pool.packInstancesSorted(inst.data(), indices.data(), 3);
        assert(inst[0].position.z == -5.0f);
        assert(inst[2].position.z == -1.0f);
    }

    return 0;
}
