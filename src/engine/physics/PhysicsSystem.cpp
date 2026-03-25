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
    JPH::Ref<JPH::CharacterVirtual> character;
    float characterEyeOffset = 0.7f;

    struct StaticBodyBinding {
        entt::entity entity = entt::null;
        JPH::BodyID bodyId;
    };

    // Tracked bodies for syncing and cleanup
    std::vector<StaticBodyBinding> staticBodies;

    // Fixed timestep accumulator
    float accumulator = 0.0f;
    static constexpr float fixedTimeStep = 1.0f / 60.0f;
};

// --- PhysicsSystem implementation ---

PhysicsSystem::PhysicsSystem() = default;
PhysicsSystem::~PhysicsSystem() = default;

void PhysicsSystem::init(Application& app) {
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

    auto& bodyInterface = impl_->physicsSystem->GetBodyInterface();
    auto& registry = app.registry();

    // 4. Create static bodies from StaticColliderComponent entities
    auto colliderView = registry.view<StaticColliderComponent>();
    for (auto [entity, collider] : colliderView.each()) {
        JPH::ShapeRefC shape;

        if (collider.shape == ColliderShape::Box) {
            shape = new JPH::BoxShape(toJolt(collider.halfExtents));
        } else if (collider.shape == ColliderShape::Cylinder) {
            shape = new JPH::CylinderShape(collider.halfHeight, collider.radius);
        }

        if (shape) {
            JPH::BodyCreationSettings bodySettings(
                shape,
                toJoltR(collider.position),
                toJoltQuat(collider.rotation),
                JPH::EMotionType::Static,
                Layers::NON_MOVING
            );

            JPH::BodyID bodyId = bodyInterface.CreateAndAddBody(bodySettings, JPH::EActivation::DontActivate);
            impl_->staticBodies.push_back({entity, bodyId});
        }
    }

    spdlog::info("PhysicsSystem: created {} static bodies", impl_->staticBodies.size());

    // 5. Create CharacterVirtual from CharacterControllerComponent
    auto ccView = registry.view<CharacterControllerComponent, TransformComponent>();
    for (auto [entity, cc, transform] : ccView.each()) {
        // Derive capsule center from eye position
        float eyeOff = cc.eyeOffset();
        glm::vec3 capsuleCenter = transform.position - glm::vec3(0.0f, eyeOff, 0.0f);

        // Create capsule shape (standing upright)
        JPH::RefConst<JPH::Shape> capsuleShape = new JPH::CapsuleShape(cc.capsuleHalfHeight, cc.capsuleRadius);

        JPH::CharacterVirtualSettings settings;
        settings.mShape = capsuleShape;
        settings.mMaxSlopeAngle = JPH::DegreesToRadians(45.0f);
        settings.mMaxStrength = 100.0f;
        settings.mMass = 80.0f;
        settings.mUp = JPH::Vec3::sAxisY();
        settings.mSupportingVolume = JPH::Plane(JPH::Vec3::sAxisY(), -cc.capsuleRadius);

        impl_->character = new JPH::CharacterVirtual(
            &settings,
            toJoltR(capsuleCenter),
            JPH::Quat::sIdentity(),
            0,  // user data
            impl_->physicsSystem.get()
        );

        impl_->characterEyeOffset = eyeOff;

        spdlog::info("PhysicsSystem: created CharacterVirtual at ({:.1f}, {:.1f}, {:.1f})",
            capsuleCenter.x, capsuleCenter.y, capsuleCenter.z);
        break; // only one player
    }

    // 6. Optimize broad phase after all bodies added
    impl_->physicsSystem->OptimizeBroadPhase();

    spdlog::info("PhysicsSystem initialized");
}

void PhysicsSystem::update(Application& app, float deltaTime) {
    if (!impl_ || !impl_->physicsSystem) return;

    auto& bodyInterface = impl_->physicsSystem->GetBodyInterface();
    auto& registry = app.registry();
    for (const auto& binding : impl_->staticBodies) {
        if (!registry.valid(binding.entity)) continue;
        const auto* collider = registry.try_get<StaticColliderComponent>(binding.entity);
        if (!collider) continue;

        bodyInterface.SetPositionAndRotation(
            binding.bodyId,
            toJoltR(collider->position),
            toJoltQuat(collider->rotation),
            JPH::EActivation::DontActivate
        );
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
    if (!impl_) return;

    // Remove static bodies
    if (impl_->physicsSystem) {
        auto& bodyInterface = impl_->physicsSystem->GetBodyInterface();
        for (const auto& binding : impl_->staticBodies) {
            bodyInterface.RemoveBody(binding.bodyId);
            bodyInterface.DestroyBody(binding.bodyId);
        }
        impl_->staticBodies.clear();
    }

    impl_.reset();

    // Cleanup Jolt runtime
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    spdlog::info("PhysicsSystem shutdown");
}

// --- Character controller API ---

void PhysicsSystem::setCharacterVelocity(const glm::vec3& velocity) {
    if (impl_ && impl_->character) {
        impl_->character->SetLinearVelocity(toJolt(velocity));
    }
}

void PhysicsSystem::updateCharacter(float deltaTime, const glm::vec3& gravity) {
    if (!impl_ || !impl_->character) return;

    JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings;
    updateSettings.mStickToFloorStepDown = JPH::Vec3(0, -0.5f, 0);
    updateSettings.mWalkStairsStepUp = JPH::Vec3(0, 0.3f, 0);

    impl_->character->ExtendedUpdate(
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

glm::vec3 PhysicsSystem::getCharacterPosition() const {
    if (impl_ && impl_->character) {
        return toGlm(impl_->character->GetPosition());
    }
    return glm::vec3(0.0f);
}

void PhysicsSystem::setCharacterPosition(const glm::vec3& position) {
    if (impl_ && impl_->character) {
        impl_->character->SetPosition(toJoltR(position));
    }
}

GroundState PhysicsSystem::getCharacterGroundState() const {
    if (!impl_ || !impl_->character) return GroundState::InAir;

    switch (impl_->character->GetGroundState()) {
        case JPH::CharacterBase::EGroundState::OnGround:
            return GroundState::OnGround;
        case JPH::CharacterBase::EGroundState::OnSteepGround:
            return GroundState::OnSteepGround;
        default:
            return GroundState::InAir;
    }
}
