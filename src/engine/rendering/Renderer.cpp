#include "Renderer.h"
#include "Shader.h"
#include "Mesh.h"

#include <algorithm>

Renderer::Renderer(Shader* shader)
    : shader_(shader)
{}

void Renderer::drawScene(const std::vector<RenderObject>& objects,
                         const std::vector<PointLight>& lights,
                         const glm::mat4& view,
                         const glm::mat4& projection,
                         const glm::vec3& cameraPos)
{
    shader_->use();

    shader_->setMat4("uView", view);
    shader_->setMat4("uProjection", projection);
    shader_->setVec3("uCameraPos", cameraPos);

    int numLights = static_cast<int>(std::min(lights.size(), static_cast<size_t>(32)));
    shader_->setInt("uNumLights", numLights);

    for (int i = 0; i < numLights; ++i) {
        std::string base = "uPointLights[" + std::to_string(i) + "].";
        shader_->setVec3(base + "position", lights[i].position);
        shader_->setVec3(base + "color",    lights[i].color);
        shader_->setFloat(base + "radius",    lights[i].radius);
        shader_->setFloat(base + "intensity", lights[i].intensity);
    }

    for (const auto& obj : objects) {
        shader_->setMat4("uModel", obj.modelMatrix);
        obj.mesh->draw();
    }
}
