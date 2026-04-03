#pragma once

#include <glad/gl.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Shader;

class SsaoPass {
public:
    SsaoPass();
    ~SsaoPass();

    SsaoPass(const SsaoPass&) = delete;
    SsaoPass& operator=(const SsaoPass&) = delete;

    void init();
    void resize(int width, int height);
    void render(GLuint sceneDepthTex,
                GLuint sceneGeomNormalTex,
                const glm::mat4& projection,
                const glm::mat4& view,
                float aoRadius,
                float aoBias,
                float aoStrength,
                float aoFadeStart,
                float aoFadeEnd);
    GLuint aoTexture() const;

private:
    void generateKernel();
    void generateNoiseTexture();
    void createFbos(int width, int height);
    void destroyFbos();
    void destroy();

    std::unique_ptr<Shader> ssaoShader_;
    std::unique_ptr<Shader> blurShader_;
    std::vector<glm::vec3> kernel_;
    GLuint noiseTex_ = 0;
    GLuint ssaoFbo_ = 0;
    GLuint ssaoColorTex_ = 0;
    GLuint blurFbo_ = 0;
    GLuint blurColorTex_ = 0;
    GLuint quadVAO_ = 0;
    GLuint quadVBO_ = 0;
    int width_ = 0;
    int height_ = 0;
};
