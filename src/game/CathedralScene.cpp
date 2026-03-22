#include "CathedralScene.h"

#include <glm/gtc/matrix_transform.hpp>

void CathedralScene::build() {
    meshes_.clear();
    objects_.clear();
    lights_.clear();

    // ------------------------------------------------------------------
    // Floor: flat plane at y=0
    // ------------------------------------------------------------------
    auto floorMesh = std::make_unique<Mesh>(Mesh::createPlane(20.0f));
    objects_.push_back({floorMesh.get(), glm::mat4(1.0f)});
    meshes_.push_back(std::move(floorMesh));

    // ------------------------------------------------------------------
    // Ceiling: flat plane at y=4.5, normal pointing down (flip via scale)
    // ------------------------------------------------------------------
    auto ceilMesh = std::make_unique<Mesh>(Mesh::createPlane(20.0f));
    glm::mat4 ceilModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 4.5f, 0.0f));
    ceilModel = glm::scale(ceilModel, glm::vec3(1.0f, -1.0f, 1.0f));  // flip normals down
    objects_.push_back({ceilMesh.get(), ceilModel});
    meshes_.push_back(std::move(ceilMesh));

    // ------------------------------------------------------------------
    // Back wall: large cube scaled thin, at z=-8
    // ------------------------------------------------------------------
    auto wallMesh = std::make_unique<Mesh>(Mesh::createCube(1.0f));
    glm::mat4 wallModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.5f, -8.0f));
    wallModel = glm::scale(wallModel, glm::vec3(8.0f, 5.0f, 0.3f));
    objects_.push_back({wallMesh.get(), wallModel});
    meshes_.push_back(std::move(wallMesh));

    // ------------------------------------------------------------------
    // 6 Pillars: 3 pairs along z-axis at x = -3 and x = 3
    // Using createCube(1.0f) then scale to (0.6, 4.0, 0.6)
    // z positions: -2, -5, -8 (pairs at x=-3 and x=3)
    // ------------------------------------------------------------------
    float pillarX[] = {-3.0f, 3.0f};
    float pillarZ[] = {-2.0f, -5.0f, -8.0f};

    for (float pz : pillarZ) {
        for (float px : pillarX) {
            auto pillarMesh = std::make_unique<Mesh>(Mesh::createCube(1.0f));
            glm::mat4 pillarModel = glm::translate(glm::mat4(1.0f), glm::vec3(px, 2.0f, pz));
            pillarModel = glm::scale(pillarModel, glm::vec3(0.6f, 4.0f, 0.6f));
            objects_.push_back({pillarMesh.get(), pillarModel});
            meshes_.push_back(std::move(pillarMesh));
        }
    }

    // ------------------------------------------------------------------
    // 2 Arch crossbeams: connect pillar pairs at y=4.0
    // Scale to (6.0, 0.4, 0.6) bridging x=-3 to x=3
    // ------------------------------------------------------------------
    float archZ[] = {-2.0f, -5.0f};

    for (float az : archZ) {
        auto archMesh = std::make_unique<Mesh>(Mesh::createCube(1.0f));
        glm::mat4 archModel = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 4.0f, az));
        archModel = glm::scale(archModel, glm::vec3(6.0f, 0.4f, 0.6f));
        objects_.push_back({archMesh.get(), archModel});
        meshes_.push_back(std::move(archMesh));
    }

    // ------------------------------------------------------------------
    // Point lights: 5 torches at varying distances (D-09)
    // ------------------------------------------------------------------
    PointLight light0;
    light0.position  = glm::vec3(-2.0f, 3.0f, -2.0f);
    light0.color     = glm::vec3(1.0f, 1.0f, 1.0f);
    light0.radius    = 8.0f;
    light0.intensity = 1.0f;
    lights_.push_back(light0);

    PointLight light1;
    light1.position  = glm::vec3(2.0f, 3.0f, -2.0f);
    light1.color     = glm::vec3(1.0f, 1.0f, 1.0f);
    light1.radius    = 8.0f;
    light1.intensity = 1.0f;
    lights_.push_back(light1);

    PointLight light2;
    light2.position  = glm::vec3(0.0f, 3.5f, 2.0f);
    light2.color     = glm::vec3(1.0f, 1.0f, 1.0f);
    light2.radius    = 10.0f;
    light2.intensity = 0.8f;
    lights_.push_back(light2);

    PointLight light3;
    light3.position  = glm::vec3(-3.0f, 2.0f, -6.0f);
    light3.color     = glm::vec3(1.0f, 1.0f, 1.0f);
    light3.radius    = 6.0f;
    light3.intensity = 0.6f;
    lights_.push_back(light3);

    PointLight light4;
    light4.position  = glm::vec3(3.0f, 2.0f, -6.0f);
    light4.color     = glm::vec3(1.0f, 1.0f, 1.0f);
    light4.radius    = 6.0f;
    light4.intensity = 0.6f;
    lights_.push_back(light4);
}
