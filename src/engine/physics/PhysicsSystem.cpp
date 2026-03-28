// Jolt.h must be included first
#include <Jolt/Jolt.h>

#include "engine/physics/PhysicsSystem.h"
#include "engine/core/Application.h"
#include "game/components/StaticColliderComponent.h"
#include "game/components/CharacterControllerComponent.h"
#include "game/components/TransformComponent.h"

#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyActivationListener.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include <glm/trigonometric.hpp>
#include <spdlog/spdlog.h>
#include <entt/entt.hpp>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

// --- Jolt type conversions ---
static inline JPH::Vec3 toJolt(const glm::vec3& v) { return JPH::Vec3(v.x, v.y, v.z); }
static inline glm::vec3 toGlm(const JPH::Vec3& v) { return glm::vec3(v.GetX(), v.GetY(), v.GetZ()); }

#ifdef JPH_DOUBLE_PRECISION
static inline JPH::RVec3 toJoltR(const glm::vec3& v) { return JPH::RVec3(v.x, v.y, v.z); }
static inline glm::vec3 toGlm(const JPH::RVec3& v) { return glm::vec3(float(v.GetX()), float(v.GetY()), float(v.GetZ())); }
#else
static inline JPH::RVec3 toJoltR(const glm::vec3& v) { return toJolt(v); }
#endif

static inline JPH::Quat toJoltQuat(const glm::vec3& eulerDegrees) {
    return JPH::Quat::sEulerAngles(toJolt(glm::radians(eulerDegrees)));
}

// --- Jolt layer definitions ---
namespace Layers {
    static constexpr JPH::ObjectLayer NON_MOVING = 0;
    static constexpr JPH::ObjectLayer MOVING = 1;
    static constexpr JPH::uint NUM_LAYERS = 2;
}

namespace BroadPhaseLayers {
    static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
    static constexpr JPH::BroadPhaseLayer MOVING(1);
    static constexpr unsigned int NUM_LAYERS = 2;
}

// --- BroadPhaseLayerInterface ---
class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM_LAYERS; }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override {
        switch (inLayer) {
            case Layers::NON_MOVING: return BroadPhaseLayers::NON_MOVING;
            case Layers::MOVING:     return BroadPhaseLayers::MOVING;
            default:                 return BroadPhaseLayers::NON_MOVING;
        }
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override {
        switch ((JPH::BroadPhaseLayer::Type)inLayer) {
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING: return "NON_MOVING";
            case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:     return "MOVING";
            default: return "UNKNOWN";
        }
    }
#endif
};

// --- ObjectVsBroadPhaseLayerFilter ---
class OVBPLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == BroadPhaseLayers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

// --- ObjectLayerPairFilter ---
class OLPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::ObjectLayer inLayer2) const override {
        switch (inLayer1) {
            case Layers::NON_MOVING:
                return inLayer2 == Layers::MOVING;
            case Layers::MOVING:
                return true;
            default:
                return false;
        }
    }
};

// --- PhysicsSystem::Impl ---
struct PhysicsSystem::Impl {
    std::unique_ptr<JPH::TempAllocatorImpl> tempAllocator;
    std::unique_ptr<JPH::JobSystemThreadPool> jobSystem;

    BPLayerInterfaceImpl bpLayerInterface;
    OVBPLayerFilterImpl objectVsBPFilter;
    OLPairFilterImpl objectLayerPairFilter;

    std::unique_ptr<JPH::PhysicsSystem> physicsSystem;

    // Character controller
    struct EntityHash {
        std::size_t operator()(entt::entity entity) const noexcept {
            return static_cast<std::size_t>(static_cast<std::uint32_t>(entity));
        }
    };

    struct StaticBodyBinding {
        JPH::BodyID bodyId;
    };

    struct CharacterBinding {
        JPH::Ref<JPH::CharacterVirtual> character;
        float eyeOffset = 0.7f;
    };

    std::unordered_map<entt::entity, StaticBodyBinding, EntityHash> staticBodies;
    std::unordered_map<entt::entity, CharacterBinding, EntityHash> characters;

    // Fixed timestep accumulator
    float accumulator = 0.0f;
    static constexpr float fixedTimeStep = 1.0f / 60.0f;
};

// --- PhysicsSystem implementation ---

PhysicsSystem::PhysicsSystem() = default;
PhysicsSystem::~PhysicsSystem() = default;

namespace {

JPH::ShapeRefC makeColliderShape(const StaticColliderComponent& collider) {
    if (collider.shape == ColliderShape::Box) {
        return new JPH::BoxShape(toJolt(collider.halfExtents));
    }
    if (collider.shape == ColliderShape::Cylinder) {
        return new JPH::CylinderShape(collider.halfHeight, collider.radius);
    }
    return nullptr;
}

} // namespace

void PhysicsSystem::init(Application& app) {
    init(app.registry());
}

void PhysicsSystem::init(entt::registry& registry) {
    // 1. Initialize Jolt runtime
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    // 2. Create implementation
    impl_ = std::make_unique<Impl>();
    impl_->tempAllocator = std::make_unique<JPH::TempAllocatorImpl>(10 * 1024 * 1024); // 10 MB
    impl_->jobSystem = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, 1); // 1 worker thread

    // 3. Create Jolt PhysicsSystem
    impl_->physicsSystem = std::make_unique<JPH::PhysicsSystem>();
    impl_->physicsSystem->Init(
        1024,   // max bodies
        0,      // num body mutexes (0 = auto)
        1024,   // max body pairs
        1024,   // max contact constraints
        impl_->bpLayerInterface,
        impl_->objectVsBPFilter,
        impl_->objectLayerPairFilter
    );

    update(registry, 0.0f);

    // 4. Optimize broad phase after all bodies added
    impl_->physicsSystem->OptimizeBroadPhase();

    spdlog::info("PhysicsSystem initialized");
}

void PhysicsSystem::update(Application& app, float deltaTime) {
    update(app.registry(), deltaTime);
}

void PhysicsSystem::update(entt::registry& registry, float deltaTime) {
    if (!impl_ || !impl_->physicsSystem) return;

    auto& bodyInterface = impl_->physicsSystem->GetBodyInterface();

    auto staticIt = impl_->staticBodies.begin();
    while (staticIt != impl_->staticBodies.end()) {
        if (!registry.valid(staticIt->first) || !registry.all_of<StaticColliderComponent>(staticIt->first)) {
            bodyInterface.RemoveBody(staticIt->second.bodyId);
            bodyInterface.DestroyBody(staticIt->second.bodyId);
            staticIt = impl_->staticBodies.erase(staticIt);
            continue;
        }
        ++staticIt;
    }

    auto colliderView = registry.view<StaticColliderComponent>();
    for (auto [entity, collider] : colliderView.each()) {
        auto found = impl_->staticBodies.find(entity);
        if (found == impl_->staticBodies.end()) {
            JPH::ShapeRefC shape = makeColliderShape(collider);
            if (!shape) {
                continue;
            }
            JPH::BodyCreationSettings bodySettings(
                shape,
                toJoltR(collider.position),
                toJoltQuat(collider.rotation),
                JPH::EMotionType::Static,
                Layers::NON_MOVING
            );
            JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);
            impl_->staticBodies.emplace(entity, Impl::StaticBodyBinding{bodyId});
            found = impl_->staticBodies.find(entity);
        }

        bodyInterface.SetPositionAndRotation(
            found->second.bodyId,
            toJoltR(collider.position),
            toJoltQuat(collider.rotation),
            JPH::EActivation::DontActivate
        );
    }

    auto characterIt = impl_->characters.begin();
    while (characterIt != impl_->characters.end()) {
        if (!registry.valid(characterIt->first) || !registry.all_of<CharacterControllerComponent, TransformComponent>(characterIt->first)) {
            characterIt = impl_->characters.erase(characterIt);
            continue;
        }
        ++characterIt;
    }

    auto ccView = registry.view<CharacterControllerComponent, TransformComponent>();
    for (auto [entity, cc, transform] : ccView.each()) {
        if (impl_->characters.contains(entity)) {
            continue;
        }

        const float eyeOff = cc.eyeOffset();
        const glm::vec3 capsuleCenter = transform.position - glm::vec3(0.0f, eyeOff, 0.0f);
        JPH::RefConst<JPH::Shape> capsuleShape = new JPH::CapsuleShape(cc.capsuleHalfHeight, cc.capsuleRadius);

        JPH::CharacterVirtualSettings settings;
        settings.mShape = capsuleShape;
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
        settings.mMaxStrength = 100.0f;
        settings.mMass = 80.0f;
        settings.mUp = JPH::Vec3::sAxisY();
        settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cc.capsuleRadius);

        Impl::CharacterBinding binding;
        binding.character = new JPH::CharacterVirtual(
            &settings,
            toJoltR(capsuleCenter),
            JPH::Quat::sIdentity(),
            0,
            impl_->physicsSystem.get()
        );
        binding.eyeOffset = eyeOff;
        impl_->characters.emplace(entity, std::move(binding));
    }

    // Fixed timestep stepping (for future dynamic bodies)
    impl_->accumulator += deltaTime;

    // Cap accumulator to prevent spiral of death on hitches
    if (impl_->accumulator > Impl::fixedTimeStep * 5.0f) {
        impl_->accumulator = Impl::fixedTimeStep * 5.0f;
    }

    while (impl_->accumulator >= Impl::fixedTimeStep) {
        impl_->physicsSystem->Update(
            Impl::fixedTimeStep,
            1, // collision steps
            impl_->tempAllocator.get(),
            impl_->jobSystem.get()
        );
        impl_->accumulator -= Impl::fixedTimeStep;
    }
}

void PhysicsSystem::shutdown() {
    shutdownRuntime();
}

void PhysicsSystem::shutdownRuntime() {
    if (!impl_) return;

    // Remove static bodies
    if (impl_->physicsSystem) {
        auto& bodyInterface = impl_->physicsSystem->GetBodyInterface();
        for (const auto& [entity, binding] : impl_->staticBodies) {
            (void)entity;
            bodyInterface.RemoveBody(binding.bodyId);
            bodyInterface.DestroyBody(binding.bodyId);
        }
        impl_->staticBodies.clear();
        impl_->characters.clear();
    }

    impl_.reset();

    // Cleanup Jolt runtime
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    spdlog::info("PhysicsSystem shutdown");
}

// --- Character controller API ---

void PhysicsSystem::setCharacterVelocity(entt::entity entity, const glm::vec3& velocity) {
    if (impl_ == nullptr) {
        return;
    }
    auto it = impl_->characters.find(entity);
    if (it != impl_->characters.end()) {
        it->second.character->SetLinearVelocity(toJolt(velocity));
    }
}

void PhysicsSystem::updateCharacter(entt::entity entity, float deltaTime, const glm::vec3& gravity) {
    if (!impl_) return;
    auto it = impl_->characters.find(entity);
    if (it == impl_->characters.end()) return;

    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mStickToFloorStepDown = JPH::Vec3(0, -0.5f, 0);
    updateSettings.mWalkStairsStepUp = JPH::Vec3(0, 0.3f, 0);

    it->second.character->ExtendedUpdate(
        deltaTime,
        toJolt(gravity),
        updateSettings,
        impl_->physicsSystem->GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
        impl_->physicsSystem->GetDefaultLayerFilter(Layers::MOVING),
        {},  // BodyFilter (accept all)
        {},  // ShapeFilter (accept all)
        *impl_->tempAllocator
    );
}

glm::vec3 PhysicsSystem::getCharacterPosition(entt::entity entity) const {
    if (impl_) {
        auto it = impl_->characters.find(entity);
        if (it != impl_->characters.end()) {
            return toGlm(it->second.character->GetPosition());
        }
    }
    return glm::vec3(0.0f);
}

void PhysicsSystem::setCharacterPosition(entt::entity entity, const glm::vec3& position) {
    if (impl_) {
        auto it = impl_->characters.find(entity);
        if (it != impl_->characters.end()) {
            it->second.character->SetPosition(toJoltR(position));
        }
    }
}

GroundState PhysicsSystem::getCharacterGroundState(entt::entity entity) const {
    if (!impl_) return GroundState::InAir;
    auto it = impl_->characters.find(entity);
    if (it == impl_->characters.end()) return GroundState::InAir;

    switch (it->second.character->GetGroundState()) {
        case JPH::CharacterBase::EGroundState::OnGround:
            return GroundState::OnGround;
        case JPH::CharacterBase::EGroundState::OnSteepGround:
            return GroundState::OnSteepGround;
        default:
            return GroundState::InAir;
    }
}
