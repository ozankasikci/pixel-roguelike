#include "editor/scene/EditorSelectionSystem.h"

#include "editor/scene/EditorPreviewWorld.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace {

glm::vec3 safeNormalize(const glm::vec3& value, const glm::vec3& fallback) {
    if (glm::dot(value, value) <= 0.0001f) {
        return fallback;
    }
    return glm::normalize(value);
}

std::optional<float> intersectRayAabb(const EditorRay& ray,
                                      const glm::vec3& boundsMin,
                                      const glm::vec3& boundsMax) {
    float tMin = 0.0f;
    float tMax = std::numeric_limits<float>::max();
    for (int axis = 0; axis < 3; ++axis) {
        const float origin = ray.origin[axis];
        const float direction = ray.direction[axis];
        if (std::abs(direction) < 0.0001f) {
            if (origin < boundsMin[axis] || origin > boundsMax[axis]) {
                return std::nullopt;
            }
            continue;
        }

        const float inv = 1.0f / direction;
        float t0 = (boundsMin[axis] - origin) * inv;
        float t1 = (boundsMax[axis] - origin) * inv;
        if (t0 > t1) {
            std::swap(t0, t1);
        }
        tMin = std::max(tMin, t0);
        tMax = std::min(tMax, t1);
        if (tMin > tMax) {
            return std::nullopt;
        }
    }
    return tMin;
}

std::optional<float> intersectRaySphere(const EditorRay& ray,
                                        const glm::vec3& center,
                                        float radius) {
    const glm::vec3 offset = ray.origin - center;
    const float b = glm::dot(offset, ray.direction);
    const float c = glm::dot(offset, offset) - radius * radius;
    const float discriminant = b * b - c;
    if (discriminant < 0.0f) {
        return std::nullopt;
    }
    const float sqrtDisc = std::sqrt(discriminant);
    const float t0 = -b - sqrtDisc;
    const float t1 = -b + sqrtDisc;
    const float t = t0 >= 0.0f ? t0 : t1;
    if (t < 0.0f) {
        return std::nullopt;
    }
    return t;
}

EditorRay transformRay(const EditorRay& ray, const glm::mat4& inverseTransform) {
    const glm::vec3 origin = glm::vec3(inverseTransform * glm::vec4(ray.origin, 1.0f));
    const glm::vec3 target = glm::vec3(inverseTransform * glm::vec4(ray.origin + ray.direction, 1.0f));
    return EditorRay{origin, safeNormalize(target - origin, glm::vec3(0.0f, 0.0f, -1.0f))};
}

std::optional<float> intersectRayObb(const EditorRay& ray,
                                     const glm::mat4& localToWorld,
                                     const glm::vec3& halfExtents) {
    const EditorRay localRay = transformRay(ray, glm::affineInverse(localToWorld));
    return intersectRayAabb(localRay, -halfExtents, halfExtents);
}

std::optional<float> intersectRayCylinder(const EditorRay& ray,
                                          const glm::mat4& localToWorld,
                                          float radius,
                                          float halfHeight) {
    const EditorRay localRay = transformRay(ray, glm::affineInverse(localToWorld));
    const glm::vec3 p = localRay.origin;
    const glm::vec3 d = localRay.direction;

    float best = std::numeric_limits<float>::max();
    bool hit = false;

    const float a = d.x * d.x + d.z * d.z;
    const float b = 2.0f * (p.x * d.x + p.z * d.z);
    const float c = p.x * p.x + p.z * p.z - radius * radius;
    if (std::abs(a) > 0.0001f) {
        const float discriminant = b * b - 4.0f * a * c;
        if (discriminant >= 0.0f) {
            const float sqrtDisc = std::sqrt(discriminant);
            const float inv = 0.5f / a;
            const float tValues[2]{
                (-b - sqrtDisc) * inv,
                (-b + sqrtDisc) * inv,
            };
            for (float t : tValues) {
                if (t < 0.0f) {
                    continue;
                }
                const float y = p.y + d.y * t;
                if (y >= -halfHeight && y <= halfHeight) {
                    best = std::min(best, t);
                    hit = true;
                }
            }
        }
    }

    if (std::abs(d.y) > 0.0001f) {
        const float capY[2]{-halfHeight, halfHeight};
        for (float yPlane : capY) {
            const float t = (yPlane - p.y) / d.y;
            if (t < 0.0f) {
                continue;
            }
            const glm::vec2 xz = glm::vec2(p.x + d.x * t, p.z + d.z * t);
            if (glm::dot(xz, xz) <= radius * radius) {
                best = std::min(best, t);
                hit = true;
            }
        }
    }

    if (!hit) {
        return std::nullopt;
    }
    return best;
}

std::optional<float> intersectHandle(const EditorSelectionHandle& handle, const EditorRay& ray) {
    switch (handle.shape) {
    case EditorSelectionShape::WorldAabb:
        return intersectRayAabb(ray, handle.worldMin, handle.worldMax);
    case EditorSelectionShape::OrientedBox:
        return intersectRayObb(ray, handle.localToWorld, handle.halfExtents);
    case EditorSelectionShape::Cylinder:
        return intersectRayCylinder(ray, handle.localToWorld, handle.radius, handle.halfHeight);
    case EditorSelectionShape::Sphere:
        return intersectRaySphere(ray, handle.anchor, handle.radius);
    }
    return std::nullopt;
}

void inflateThinBounds(glm::vec3& boundsMin, glm::vec3& boundsMax) {
    constexpr float kMinimumPickThickness = 0.30f;
    const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    glm::vec3 extents = boundsMax - boundsMin;
    if (extents.x < kMinimumPickThickness) {
        boundsMin.x = center.x - kMinimumPickThickness * 0.5f;
        boundsMax.x = center.x + kMinimumPickThickness * 0.5f;
    }
    if (extents.y < kMinimumPickThickness) {
        boundsMin.y = center.y - kMinimumPickThickness * 0.5f;
        boundsMax.y = center.y + kMinimumPickThickness * 0.5f;
    }
    if (extents.z < kMinimumPickThickness) {
        boundsMin.z = center.z - kMinimumPickThickness * 0.5f;
        boundsMax.z = center.z + kMinimumPickThickness * 0.5f;
    }
}

int selectionPriority(const EditorSelectionHandle& handle) {
    switch (handle.objectKind) {
    case EditorSceneObjectKind::Mesh:
    case EditorSceneObjectKind::Archetype:
        return 0;
    case EditorSceneObjectKind::Light:
        return 1;
    case EditorSceneObjectKind::PlayerSpawn:
        return 2;
    case EditorSceneObjectKind::BoxCollider:
    case EditorSceneObjectKind::CylinderCollider:
        return 3;
    }
    return 4;
}

} // namespace

EditorRay buildEditorRay(const glm::mat4& inverseViewProjection,
                         const glm::vec2& viewportOrigin,
                         const glm::vec2& viewportSize,
                         const glm::vec2& screenPosition) {
    const glm::vec2 local = screenPosition - viewportOrigin;
    const float x = (local.x / std::max(viewportSize.x, 1.0f)) * 2.0f - 1.0f;
    const float y = 1.0f - (local.y / std::max(viewportSize.y, 1.0f)) * 2.0f;

    const glm::vec4 nearPoint = inverseViewProjection * glm::vec4(x, y, -1.0f, 1.0f);
    const glm::vec4 farPoint = inverseViewProjection * glm::vec4(x, y, 1.0f, 1.0f);
    const glm::vec3 origin = glm::vec3(nearPoint) / nearPoint.w;
    const glm::vec3 target = glm::vec3(farPoint) / farPoint.w;
    return EditorRay{origin, safeNormalize(target - origin, glm::vec3(0.0f, 0.0f, -1.0f))};
}

std::vector<EditorSelectionHandle> buildEditorSelectionHandles(const EditorSceneDocument& document,
                                                               const EditorPreviewWorld& previewWorld) {
    std::vector<EditorSelectionHandle> handles;
    handles.reserve(document.objects().size());

    const glm::vec3 sceneCenter = previewWorld.sceneBounds().valid
        ? previewWorld.sceneBounds().center()
        : glm::vec3(0.0f);

    for (const auto& object : document.objects()) {
        EditorSelectionHandle handle;
        handle.objectId = object.id;
        handle.objectKind = object.kind;
        handle.anchor = editorSceneObjectAnchor(object);

        switch (object.kind) {
        case EditorSceneObjectKind::Mesh:
        case EditorSceneObjectKind::Archetype:
            if (const auto* bounds = previewWorld.findObjectBounds(object.id)) {
                handle.shape = EditorSelectionShape::WorldAabb;
                handle.worldMin = bounds->min;
                handle.worldMax = bounds->max;
                inflateThinBounds(handle.worldMin, handle.worldMax);
                handle.anchor = bounds->center();
                handle.placementSurface = true;
            } else {
                handle.shape = EditorSelectionShape::Sphere;
                handle.radius = 0.5f;
                handle.placementSurface = true;
            }
            break;
        case EditorSceneObjectKind::Light: {
            const auto& light = std::get<LevelLightPlacement>(object.payload);
            if (light.type == LightType::Directional) {
                handle.anchor = sceneCenter - safeNormalize(light.direction, glm::vec3(-0.3f, -0.9f, -0.1f)) * 4.0f;
            } else {
                handle.anchor = light.position;
            }
            handle.shape = EditorSelectionShape::Sphere;
            handle.radius = light.type == LightType::Directional ? 0.4f : std::max(0.25f, light.radius * 0.08f);
            break;
        }
        case EditorSceneObjectKind::BoxCollider: {
            handle.shape = EditorSelectionShape::OrientedBox;
            handle.localToWorld = document.worldTransformMatrix(object.id);
            handle.halfExtents = glm::vec3(0.5f);
            handle.anchor = glm::vec3(handle.localToWorld[3]);
            handle.placementSurface = true;
            break;
        }
        case EditorSceneObjectKind::CylinderCollider: {
            handle.shape = EditorSelectionShape::Cylinder;
            handle.localToWorld = document.worldTransformMatrix(object.id);
            handle.radius = 0.5f;
            handle.halfHeight = 0.5f;
            handle.anchor = glm::vec3(handle.localToWorld[3]);
            handle.placementSurface = true;
            break;
        }
        case EditorSceneObjectKind::PlayerSpawn:
            handle.shape = EditorSelectionShape::Sphere;
            handle.radius = 0.45f;
            handle.anchor = glm::vec3(document.worldTransformMatrix(object.id)[3]);
            break;
        default:
            handle.shape = EditorSelectionShape::Sphere;
            handle.radius = 0.4f;
            break;
        }
        handles.push_back(handle);
    }

    return handles;
}

std::vector<EditorHitResult> pickEditorObjects(const std::vector<EditorSelectionHandle>& handles,
                                               const EditorRay& ray) {
    struct RankedHit {
        EditorHitResult hit;
        int priority = 0;
    };

    std::vector<RankedHit> rankedHits;
    rankedHits.reserve(handles.size());
    for (const auto& handle : handles) {
        const auto hitDistance = intersectHandle(handle, ray);
        if (!hitDistance.has_value()) {
            continue;
        }
        rankedHits.push_back(RankedHit{
            EditorHitResult{
                handle.objectId,
                handle.objectKind,
                *hitDistance,
                ray.origin + ray.direction * *hitDistance
            },
            selectionPriority(handle),
        });
    }

    std::sort(rankedHits.begin(), rankedHits.end(), [](const RankedHit& lhs, const RankedHit& rhs) {
        constexpr float kDistanceTieThreshold = 0.35f;
        const float distanceDelta = lhs.hit.distance - rhs.hit.distance;
        if (std::abs(distanceDelta) > kDistanceTieThreshold) {
            return lhs.hit.distance < rhs.hit.distance;
        }
        if (lhs.priority != rhs.priority) {
            return lhs.priority < rhs.priority;
        }
        if (std::abs(distanceDelta) > 0.0001f) {
            return lhs.hit.distance < rhs.hit.distance;
        }
        return lhs.hit.objectId < rhs.hit.objectId;
    });

    std::vector<EditorHitResult> hits;
    hits.reserve(rankedHits.size());
    for (const auto& rankedHit : rankedHits) {
        hits.push_back(rankedHit.hit);
    }
    return hits;
}

std::optional<EditorHitResult> pickEditorObject(const std::vector<EditorSelectionHandle>& handles,
                                                const EditorRay& ray) {
    std::vector<EditorHitResult> hits = pickEditorObjects(handles, ray);
    if (hits.empty()) {
        return std::nullopt;
    }
    return hits.front();
}

std::optional<EditorHitResult> pickEditorPlacementSurface(const std::vector<EditorSelectionHandle>& handles,
                                                          const EditorRay& ray) {
    std::optional<EditorHitResult> bestHit;
    float bestDistance = std::numeric_limits<float>::max();
    for (const auto& handle : handles) {
        if (!handle.placementSurface) {
            continue;
        }
        const auto hitDistance = intersectHandle(handle, ray);
        if (!hitDistance.has_value() || *hitDistance >= bestDistance) {
            continue;
        }
        bestDistance = *hitDistance;
        bestHit = EditorHitResult{
            handle.objectId,
            handle.objectKind,
            *hitDistance,
            ray.origin + ray.direction * *hitDistance
        };
    }
    return bestHit;
}
