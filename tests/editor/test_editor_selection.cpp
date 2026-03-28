#include "editor/scene/EditorSelectionSystem.h"

#include <cassert>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

int main() {
    {
        EditorSelectionHandle box;
        box.objectId = 101;
        box.shape = EditorSelectionShape::WorldAabb;
        box.placementSurface = true;
        box.worldMin = glm::vec3(-1.0f, -1.0f, -1.0f);
        box.worldMax = glm::vec3(1.0f, 1.0f, 1.0f);

        const EditorRay ray{
            glm::vec3(0.0f, 0.0f, 5.0f),
            glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)),
        };
        const auto hit = pickEditorObject({box}, ray);
        assert(hit.has_value());
        assert(hit->objectId == 101);
        assert(hit->distance > 3.9f && hit->distance < 4.1f);
    }

    {
        EditorSelectionHandle sphere;
        sphere.objectId = 202;
        sphere.shape = EditorSelectionShape::Sphere;
        sphere.anchor = glm::vec3(2.0f, 0.0f, 0.0f);
        sphere.radius = 0.5f;

        const EditorRay missRay{
            glm::vec3(0.0f, 0.0f, 5.0f),
            glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)),
        };
        assert(!pickEditorObject({sphere}, missRay).has_value());
    }

    {
        EditorSelectionHandle collider;
        collider.objectId = 303;
        collider.objectKind = EditorSceneObjectKind::CylinderCollider;
        collider.shape = EditorSelectionShape::Cylinder;
        collider.placementSurface = true;
        collider.localToWorld = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f));
        collider.radius = 1.0f;
        collider.halfHeight = 1.5f;

        const EditorRay ray{
            glm::vec3(0.25f, 0.0f, 4.0f),
            glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)),
        };
        const auto hit = pickEditorPlacementSurface({collider}, ray);
        assert(hit.has_value());
        assert(hit->objectId == 303);
    }

    {
        EditorSelectionHandle collider;
        collider.objectId = 401;
        collider.objectKind = EditorSceneObjectKind::BoxCollider;
        collider.shape = EditorSelectionShape::WorldAabb;
        collider.worldMin = glm::vec3(-1.0f, -1.0f, -1.0f);
        collider.worldMax = glm::vec3(1.0f, 1.0f, 1.0f);

        EditorSelectionHandle mesh;
        mesh.objectId = 402;
        mesh.objectKind = EditorSceneObjectKind::Mesh;
        mesh.shape = EditorSelectionShape::WorldAabb;
        mesh.worldMin = glm::vec3(-1.0f, -1.0f, -1.05f);
        mesh.worldMax = glm::vec3(1.0f, 1.0f, 0.95f);

        const EditorRay ray{
            glm::vec3(0.0f, 0.0f, 4.0f),
            glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)),
        };
        const auto hits = pickEditorObjects({collider, mesh}, ray);
        assert(hits.size() == 2);
        assert(hits.front().objectId == 402);
        assert(hits.back().objectId == 401);
    }

    return 0;
}
