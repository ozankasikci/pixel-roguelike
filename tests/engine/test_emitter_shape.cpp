#include "engine/particles/EmitterShape.h"
#include <cassert>
#include <cmath>

int main() {
    using namespace particles;
    std::mt19937 rng(42);

    // PointShape
    { PointShape shape; for(int i=0;i<100;++i){auto p=shape.samplePosition(rng);assert(p.x==0&&p.y==0&&p.z==0);} for(int i=0;i<100;++i){auto d=shape.sampleDirection(rng);assert(std::fabs(glm::length(d)-1.0f)<0.01f);} }

    // SphereShape volume
    { SphereShape shape(2.0f,false); for(int i=0;i<200;++i){assert(glm::length(shape.samplePosition(rng))<=2.01f);} }

    // SphereShape surface
    { SphereShape shape(3.0f,true); for(int i=0;i<200;++i){assert(std::fabs(glm::length(shape.samplePosition(rng))-3.0f)<0.01f);} }

    // ConeShape base disc
    { ConeShape shape(30.0f,0.5f); for(int i=0;i<200;++i){auto p=shape.samplePosition(rng);assert(p.y==0.0f);assert(std::sqrt(p.x*p.x+p.z*p.z)<=0.51f);} }

    // ConeShape direction within angle
    { ConeShape shape(45.0f,0.1f); float cosLimit=std::cos(glm::radians(45.0f)); for(int i=0;i<200;++i){auto d=shape.sampleDirection(rng);assert(std::fabs(glm::length(d)-1.0f)<0.01f);assert(glm::dot(d,glm::vec3(0,1,0))>=cosLimit-0.01f);} }

    return 0;
}
