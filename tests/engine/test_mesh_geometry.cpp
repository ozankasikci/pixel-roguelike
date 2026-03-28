#include "engine/rendering/geometry/MeshGeometry.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cassert>
#include <cstdio>

int main() {
    // generateCube: 6 faces * 4 verts = 24 verts, 6 faces * 2 tris * 3 = 36 indices
    {
        auto cube = generateCube(1.0f);
        assert(cube.positions.size() == 24);
        assert(cube.normals.size() == 24);
        assert(cube.uvs.size() == 24);
        assert(cube.tangents.size() == 24);
        assert(cube.indices.size() == 36);
    }

    // generatePlane: 4 verts, 6 indices
    {
        auto plane = generatePlane(1.0f);
        assert(plane.positions.size() == 4);
        assert(plane.normals.size() == 4);
        assert(plane.uvs.size() == 4);
        assert(plane.tangents.size() == 4);
        assert(plane.indices.size() == 6);
    }

    // generateCylinder: side ring + duplicated cap rings + 2 cap centers
    {
        int seg = 12;
        auto cyl = generateCylinder(1.0f, 1.0f, seg);
        int expectedVerts = (seg + 1) * 4 + 2;
        assert(cyl.positions.size() == static_cast<size_t>(expectedVerts));
        assert(cyl.normals.size() == cyl.positions.size());
        assert(cyl.uvs.size() == cyl.positions.size());
        assert(cyl.tangents.size() == cyl.positions.size());
        // Side: seg*6, bottom cap: seg*3, top cap: seg*3
        int expectedIndices = seg * 6 + seg * 3 + seg * 3;
        assert(cyl.indices.size() == static_cast<size_t>(expectedIndices));
    }

    // mergeMeshes: two cubes merged
    {
        auto cube = generateCube(1.0f);
        auto merged = mergeMeshes({
            {cube, glm::mat4(1.0f)},
            {cube, glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f))}
        });
        assert(merged.positions.size() == 48);
        assert(merged.normals.size() == 48);
        assert(merged.uvs.size() == 48);
        assert(merged.tangents.size() == 48);
        assert(merged.indices.size() == 72);

        // Second cube's indices should be offset by 24
        assert(merged.indices[36] >= 24);
    }

    // mergeMeshes: transform applied correctly
    {
        auto plane = generatePlane(2.0f);
        glm::mat4 shift = glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f));
        auto merged = mergeMeshes({{plane, shift}});
        // All positions should have x >= 9.0 (shifted by 10, plane half-size is 1)
        for (const auto& p : merged.positions) {
            assert(p.x >= 9.0f);
        }
    }

    printf("All mesh geometry tests passed!\n");
    return 0;
}
