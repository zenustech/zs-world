#include "SimpleGeom.hpp"

#include "glm/gtc/type_ptr.hpp"

namespace zs {
    using Node = typename TriMesh::Node;
    using Elem = typename TriMesh::Elem;
    using Norm = typename TriMesh::Norm;
    using UV = typename TriMesh::UV;
    using vec3f = vec<float, 3>;
    using vec2f = vec<float, 2>;

    const static float PI = 3.1415926535f;

#define SIMPLE_GEOM_HEAD TriMesh mesh;\
    auto& verts = mesh.nodes;\
    auto& faces = mesh.elems;\
    auto& norms = mesh.norms;\
    auto& uvs = mesh.uvs;\
    Node vert;\
    Norm norm;\
    UV uv;\
    Elem face;

    static inline glm::mat3 getAxisMatrix(char axis) {
        glm::mat3 axisMat(0.0f);
        if (axis == 'X') {
            axisMat[1][0] = axisMat[2][2] = 1.0f;
            axisMat[0][1] = -1.0f;
        } else if (axis == 'Z') {
            axisMat[0][0] = axisMat[1][2] = 1.0f;
            axisMat[2][1] = -1.0f;
        } else { // we usd Y as default axis, no need to switch axis
            axisMat[0][0] = axisMat[1][1] = axisMat[2][2] = 1.0f;
        }
        return axisMat;
    }

    static inline void applyMatrix(std::vector<std::array<float, 3>>& vecs, const glm::mat3& mat) {
        for (auto& _ : vecs) {
            glm::vec3 v = { _[0], _[1], _[2] };
            v = mat * v;
            _ = { v[0], v[1], v[2] };
        }
    }

    TriMesh createSphere(float radius, unsigned rows, unsigned cols, bool hasNormal, bool hasUV, bool isFlipFace) {
        SIMPLE_GEOM_HEAD;

        rows = std::max(rows, (unsigned)2);
        cols = std::max(cols, (unsigned)3);

        // up
        vert = { 0.0f, radius, 0.0f };
        verts.emplace_back(vert);
        if (hasNormal) {
            norm = { 0.0f, 1.0f, 0.0f };
            norms.emplace_back(norm);
        }
        if (hasUV) {
            uv = { 0.0f, 1.0f };
            uvs.emplace_back(uv);
        }

        // body
        for (unsigned row = 1; row < rows; row++) {
            float theta = PI * row / rows;
            for (unsigned col = 0; col < cols; col++) {
                float phi = PI * 2.0f * col / cols;
                vert = { sin(theta) * cos(phi) * radius , cos(theta) * radius, -sin(theta) * sin(phi) * radius };
                verts.emplace_back(vert);

                if (hasNormal) {
                    glm::vec3 n = { vert[0], vert[1], vert[2] };
                    n = glm::normalize(n);
                    norm = { n[0], n[1],n[2] };
                    norms.emplace_back(norm);
                }

                if (hasUV) {
                    uv = { 1.0f * col / cols, 1.0f - 1.0f * row / rows };
                    uvs.emplace_back(uv);
                }
            }
        }

        // bottom
        vert = { 0.0f, -radius, 0.0f };
        verts.emplace_back(vert);
        if (hasNormal) {
            norm = { 0.0f, -1.0f, 0.0f };
            norms.emplace_back(norm);
        }
        if (hasUV) {
            uv = { 0.0f, 0.0f };
            uvs.emplace_back(uv);
        }

        // setup face indices
        {
            // head
            for (unsigned col = 0; col < cols; col++) {
                if (col == cols - 1) {
                    face = { 0, cols, 1 };
                } else {
                    face = { 0, col + 1, col + 2 };
                }
                faces.emplace_back(face);
            }
            // body
            for (unsigned row = 1; row < rows - 1; row++) {
                for (unsigned col = 0; col < cols; col++) {
                    if (col == cols - 1) {
                        face[0] = (row - 1) * cols + 1;
                        face[1] = (row - 1) * cols + cols;
                        face[2] = row * cols + cols;
                        faces.emplace_back(face);

                        face[0] = (row - 1) * cols + 1;
                        face[1] = row * cols + cols;
                        face[2] = row * cols + 1;
                        faces.emplace_back(face);
                    } else {
                        face[0] = (row - 1) * cols + col + 2;
                        face[1] = (row - 1) * cols + col + 1;
                        face[2] = row * cols + col + 1;
                        faces.emplace_back(face);

                        face[0] = (row - 1) * cols + col + 2;
                        face[1] = row * cols + col + 1;
                        face[2] = row * cols + col + 2;
                        faces.emplace_back(face);
                    }
                }
            }
            // tail
            for (unsigned col = 0; col < cols; col++) {
                if (col == cols - 1) {
                    face[0] = (rows - 2) * cols + 1;
                    face[1] = (rows - 2) * cols + col + 1;
                    face[2] = (rows - 1) * cols + 1;
                } else {
                    face[0] = (rows - 2) * cols + col + 2;
                    face[1] = (rows - 2) * cols + col + 1;
                    face[2] = (rows - 1) * cols + 1;
                }
                faces.emplace_back(face);
            }
        }

        if (isFlipFace) {
            for (auto& f : faces) {
                std::swap(f[1], f[2]);
            }
            for (auto& n : norms) {
                for (int i = 0; i < 3; ++i) {
                    n[i] = -n[i];
                }
            }
        }
        
        return mesh;
    }

    TriMesh createCapsule(float radius, float height, char axis, unsigned halfRows, unsigned cols, bool hasNormal, bool hasUV) {
        SIMPLE_GEOM_HEAD;

        // axis rotation
        glm::mat3 axisMat = getAxisMatrix(axis);

        // top point
        vert = { 0.0f, 0.5f * height + radius, 0.0f };
        verts.emplace_back(vert); // top point
        if (hasNormal) {
            norm = { 0.0f, 1.0f, 0.0f };
            norms.emplace_back(norm);
        }
        if (hasUV) {
            uv = { 0.0f, 1.0f };
            uvs.emplace_back(uv);
        }

        // top half-sphere
        for (unsigned row = 1; row < halfRows; row++) {
            float v = 0.5f * row / halfRows;
            float theta = PI * v;
            for (unsigned column = 0; column < cols; column++) {
                float phi = PI * 2.0f * column / cols;
                vert[0] = radius * sin(theta) * cos(phi);
                vert[1] = radius * cos(theta) + 0.5f * height;
                vert[2] = radius * -sin(theta) * sin(phi);
                verts.emplace_back(vert);

                if (hasNormal) {
                    glm::vec3 n = { vert[0], vert[1], vert[2] };
                    n = glm::normalize(n);
                    norm = { n[0],n[1],n[2] };
                    norms.emplace_back(norm);
                }
                if (hasUV) {
                    uv = { 1.0f * column / cols, 1.0f - 1.0f * row / (halfRows * 2) };
                    uvs.emplace_back(uv);
                }
            }
        }
        // down half-sphere
        for (unsigned row = halfRows; row < halfRows * 2; row++) {
            float theta = PI * 0.5f * row / halfRows;
            for (unsigned column = 0; column < cols; column++) {
                float phi = PI * 2.0f * column / cols;
                vert[0] = radius * sin(theta) * cos(phi);
                vert[1] = radius * cos(theta) - 0.5f * height;
                vert[2] = radius * -sin(theta) * sin(phi);
                verts.emplace_back(vert);

                if (hasNormal) {
                    glm::vec3 n = { vert[0], vert[1], vert[2] };
                    n = glm::normalize(n);
                    norm = { n[0],n[1],n[2] };
                    norms.emplace_back(norm);
                }
                if (hasUV) {
                    uv = { 1.0f * column / cols, 1.0f - 1.0f * row / (halfRows * 2) };
                    uvs.emplace_back(uv);
                }
            }
        }
        // bottom point
        vert = { 0.0f, -0.5f * height - radius, 0.0f };
        verts.emplace_back(vert);
        applyMatrix(verts, axisMat);
        if (hasNormal) {
            norm = { 0.0f, -1.0f, 0.0f };
            norms.emplace_back(norm);
            applyMatrix(norms, axisMat);
        }
        if (hasUV) {
            uv = { 0.0f, 0.0f };
            uvs.emplace_back(uv);
        }

        // setup capsule poly indices
        {
            //head
            for (unsigned column = 0; column < cols; column++) {
                if (column == cols - 1) {
                    face = { 0, cols, 1 };
                } else {
                    face = { 0, column + 1, column + 2 };
                }
                faces.emplace_back(face);
            }
            //body
            for (auto row = 1; row < halfRows * 2 - 1; row++) {
                for (auto column = 0; column < cols; column++) {
                    if (column == cols - 1) {
                        face[0] = (row - 1) * cols + 1;
                        face[1] = (row - 1) * cols + cols;
                        face[2] = row * cols + cols;
                        faces.emplace_back(face);

                        face[0] = (row - 1) * cols + 1;
                        face[1] = row * cols + cols;
                        face[2] = row * cols + 1;
                        faces.emplace_back(face);
                    } else {
                        face[0] = (row - 1) * cols + column + 2;
                        face[1] = (row - 1) * cols + column + 1;
                        face[2] = row * cols + column + 1;
                        faces.emplace_back(face);

                        face[0] = (row - 1) * cols + column + 2;
                        face[1] = row * cols + column + 1;
                        face[2] = row * cols + column + 2;
                        faces.emplace_back(face);
                    }
                }
            }
            //tail
            for (auto col = 0; col < cols; col++) {
                if (col == cols - 1) {
                    face[0] = (halfRows * 2 - 2) * cols + 1;
                    face[1] = (halfRows * 2 - 2) * cols + col + 1;
                    face[2] = (halfRows * 2 - 1) * cols + 1;
                    faces.emplace_back(face);
                } else {
                    face[0] = (halfRows * 2 - 2) * cols + col + 2;
                    face[1] = (halfRows * 2 - 2) * cols + col + 1;
                    face[2] = (halfRows * 2 - 1) * cols + 1;
                    faces.emplace_back(face);
                }
            }
        }

        return mesh;
    }

    TriMesh createCube(float size, unsigned divX, unsigned divY, unsigned divZ, bool hasNormal, bool hasUV) {
        SIMPLE_GEOM_HEAD;

        divX = std::max(divX, (unsigned)2);
        divY = std::max(divY, (unsigned)2);
        divZ = std::max(divZ, (unsigned)2);

        float sw = 1.0f / (divX - 1);
        float sh = 1.0f / (divY - 1);
        float sd = 1.0f / (divZ - 1);

        // vertices
        for (int i = 0; i < divX; i++) {
            for (int j = 0; j < divY; j++) {
                for (int k = 0; k < divZ; k++) {
                    if (j == 0 || j == divY - 1 || i == 0 || i == divX - 1 || k == 0 || k == divZ - 1) {
                        vert = { 0.5f - i * sw, -0.5f + j * sh, 0.5f - k * sd };
                        verts.emplace_back(vert);

                        if (hasNormal) {
                            glm::vec3 n(vert[0], vert[1], vert[2]);
                            n = glm::normalize(n);
                            norm = { n[0], n[1], n[2] };
                            norms.emplace_back(norm);
                        }

                        if (hasUV) {
                            ; // TODO
                        }
                    }
                }
            }
        }

        int le = divY * divZ;
        int rls = verts.size() - divZ;
        int pp = (divY - 1) * divZ;
        int pcircle = le - (divY - 2) * (divZ - 2);
        int inc = verts.size() - divY * divZ;

        // left and right
        for (int j = 0; j < divY - 1; j++) {
            for (int k = 0; k < divZ - 1; k++) {
                unsigned i1 = k + j * (divZ);
                unsigned i2 = i1 + 1;
                unsigned i3 = i2 + (divZ - 1);
                unsigned i4 = i3 + 1;

                // Left
                face = { i1, i2, i3 };
                faces.emplace_back(face);
                face = { i4, i3, i2 };
                faces.emplace_back(face);

                // Right
                face = { i1 + inc, i3 + inc, i2 + inc };
                faces.emplace_back(face);
                face = { i4 + inc, i2 + inc, i3 + inc };
                faces.emplace_back(face);
            }
        }

        // top and bottom
        for (int j = 0; j < divX - 1; j++) {
            for (int i = 0; i < divZ - 1; i++) {
                unsigned i1 = i + le + (j - 1) * pcircle;
                unsigned i2 = i1 + pcircle;
                if (j == 0) i1 = i;
                unsigned i3 = i1 + 1;
                unsigned i4 = i2 + 1;

                // Bottom
                face = { i1, i2, i3 };
                faces.emplace_back(face);
                face = { i4, i3, i2 };
                faces.emplace_back(face);

                unsigned i1_ = pp + i + j * pcircle;
                unsigned i2_ = (j == divX - 2) ? (rls + i) : (i1_ + pcircle);
                unsigned i3_ = i1_ + 1;
                unsigned i4_ = i2_ + 1;

                // Top
                face = { i1_, i3_, i2_ };
                faces.emplace_back(face);
                face = { i4_, i2_, i3_ };
                faces.emplace_back(face);
            }
        }

        // front and back
        for (int j = 0; j < divX - 1; j++) {
            for (int i = 0; i < divY - 1; i++) {
                unsigned i1, i2, i3, i4, i1_, i2_, i3_, i4_;

                int tc = le + divZ + j * pcircle;
                int ci1 = tc + i * 2; // front and back point increase

                if (j == divX - 2 && i != 0) {
                    i1 = tc + i * divZ;  // depth increase
                    i2 = i1 - divZ;
                    i3 = ci1 - pcircle;
                } else {
                    i1 = ci1;
                    i2 = i1 - 2;
                    i3 = i1 - pcircle;
                }
                if (j == 0) {
                    i3 = divZ * (i + 1);
                    i4 = i3 - divZ;
                } else {
                    i4 = i3 - 2;
                }

                if (i == 0) {
                    i2 = i1 - divZ;
                    i4 = i3 - divZ;
                }

                i1_ = i1 + 1;
                i2_ = i2 + 1;
                i3_ = i3 + 1;
                i4_ = i4 + 1;
                if (i == divY - 2 || j == divX - 2 && i != divY - 2)
                    i1_ = i1 + (divZ - 1);
                if (i == 0 || j == divX - 2)
                    i2_ = i2 + (divZ - 1);
                if (j != 0 && i == divY - 2)
                    i3_ = i3 + (divZ - 1);
                if (i == 0 || j == 0)
                    i4_ = i4 + (divZ - 1);
                if (j == 0)
                    i3_ = i3 + (divZ - 1);

                // Back
                face = { i1, i2, i3 };
                faces.emplace_back(face);
                face = { i4, i3, i2 };
                faces.emplace_back(face);

                // Front
                face = { i1_, i3_, i2_ };
                faces.emplace_back(face);
                face = { i4_, i2_, i3_ };
                faces.emplace_back(face);
            }
        }

        return mesh;
    }

    TriMesh createPlane(float width, float height, char axis, unsigned rows, unsigned cols, bool hasNormal, bool hasUV) {
        SIMPLE_GEOM_HEAD;

        glm::mat3 axisMat = getAxisMatrix(axis);

        rows = std::max(rows, (unsigned)1);
        cols = std::max(cols, (unsigned)1);

        auto start_point = vec3f(-0.5f, 0.0f, -0.5f);
        auto scale = vec3f(width, 1.0f, height);
        vec3f normal(0.0f);
        float rm = 1.0f / rows;
        float cm = 1.0f / cols;
        int fi = 0;

        // Vertices & UV
        for (int i = 0; i <= rows; i++) {
            auto rp = start_point + vec3f(i * rm, 0.0f, 0.0f);

            for (int j = 0; j <= cols; j++) {
                auto p = rp + vec3f(0, 0, j * cm);
                p = p * scale;
                vert = { p[0], p[1],p[2] };
                verts.emplace_back(vert);
                if (hasUV) {
                    uv = { i * rm, cm * j };
                    uvs.emplace_back(uv);
                }
            }
        }
        applyMatrix(verts, axisMat);

        // Indices
        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                unsigned i1 = fi;
                unsigned i2 = i1 + 1;
                unsigned i3 = fi + (cols + 1);
                unsigned i4 = i3 + 1;

                face = { i1, i2, i4 };
                faces.emplace_back(face);
                face = { i3, i1, i4 };
                faces.emplace_back(face);
                fi += 1;
            }
            fi += 1;
        }

        // Assign normal
        if (hasNormal){
            glm::vec3 n = { 0.0f, 1.0f, 0.0f };
            n = axisMat * n;
            norm = { n[0], n[1], n[2] };
            norms.assign(verts.size(), norm);
        }

        return mesh;
    }

    TriMesh createDisk(float radius, unsigned divisions, bool hasNormal, bool hasUV) {
        SIMPLE_GEOM_HEAD;

        divisions = std::max(divisions, (unsigned)3);

        vert = { 0.0f, 0.0f, 0.0f };
        verts.emplace_back(vert);
        if (hasNormal) {
            norm = { 0.0f, 1.0f, 0.0f };
            norms.emplace_back(norm);
        }
        if (hasUV) {
            uv = { 0.5f, 0.5f };
            uvs.emplace_back(uv);
        }

        for (unsigned i = 0; i < divisions; i++) {
            float rad = 2.0f * PI * i / divisions;
            vert = { cos(rad) * radius , 0.0f, -sin(rad) * radius };
            verts.emplace_back(vert);

            face = { i + 1, i + 2, 0 };
            faces.emplace_back(face);

            if (hasNormal) {
                norms.emplace_back(norm); // norm is always (0, 1, 0)
            }

            if (hasUV) {
                uv = { vert[0] / 2.0f + 0.5f, vert[2] / 2.0f + 0.5f };
                uvs.emplace_back(uv);
            }
        }

        // Update last
        faces[faces.size() - 1] = { divisions, 1, 0 };

        return mesh;
    }

    TriMesh createTube(float topRadius, float bottomRadius, float height, unsigned rows, unsigned cols, bool hasNormal, bool hasUV) {
        SIMPLE_GEOM_HEAD;

        rows = std::max(rows, (unsigned)3);
        cols = std::max(cols, (unsigned)3);

        float hs = height * 0.5f;
        float phi = (bottomRadius - topRadius) / hs * 0.5f;

        // top, Position, Normal and indices
        vert = { 0.0f, hs, 0.0f };
        verts.emplace_back(vert);
        if (hasNormal) {
            norm = { 0.0f, 1.0f, 0.0f };
            norms.emplace_back(norm);
        }
        if (hasUV) {
            ; // ????
        }
        for (unsigned i = 1; i <= cols; ++i) {
            float theta = 2.0f * PI * (i - 1) / cols;
            vert = { cos(theta) * topRadius, hs, sin(theta) * topRadius };
            verts.emplace_back(vert);

            if (hasNormal) {
                glm::vec3 n = { vert[0], sin(phi) + 1.0f, vert[2] }; // body normal interpolates with up normal
                n = glm::normalize(n);
                norm = { n[0],n[1],n[2] };
                norms.emplace_back(norm);
            }
            if (hasUV) {
                ;
            }

            // indices
            if (i == cols) {
                face = { 0, 1, cols };
            }else{
                face = { 0, i + 1, i };
            }
            faces.emplace_back(face);
        }

        // middle
        unsigned baseIndex;
        for (unsigned row = 1; row < rows; ++row) {
            float mix = 1.0f * row / (rows - 1);
            float r = (1.0f - mix) * topRadius + mix * bottomRadius;
            baseIndex = verts.size();
            for (unsigned i = 0; i < cols; ++i) {
                float theta = 2.0f * PI * i / cols;
                vert = { cos(theta) * r, hs - mix * height, sin(theta) * r };
                verts.emplace_back(vert);

                if (hasNormal) {
                    glm::vec3 normal = {
                        vert[0] / r,
                        sin(phi),
                        vert[2] / r
                    };
                    normal = glm::normalize(normal);
                    norm = { normal[0], normal[1], normal[2] };
                    norms.emplace_back(norm);
                }

                if (hasUV) {
                    ; // TODO
                }

                if (i == cols - 1) {
                    face = { baseIndex + i, baseIndex + i - cols, baseIndex - cols };
                    faces.emplace_back(face);
                    face = { baseIndex + i , baseIndex - cols, baseIndex };
                    faces.emplace_back(face);
                } else {
                    face = { baseIndex + i, baseIndex + i - cols, baseIndex + i - cols + 1 };
                    faces.emplace_back(face);
                    face = { baseIndex + i , baseIndex + i - cols + 1, baseIndex + i + 1 };
                    faces.emplace_back(face);
                }
            }
        }

        // bottom
        baseIndex = verts.size();
        vert = { 0.0f, -hs, 0.0f };
        verts.emplace_back(vert);
        if (hasNormal) {
            norm = { 0.0f, -1.0f, 0.0f };
            norms.emplace_back(norm);
        }
        if (hasUV) {
            ; // TODO
        }
        for (int i = 1; i <= cols; ++i) {
            if (hasNormal) {
                // normal interpolation
                auto& n = norms[baseIndex - cols - 1 + i];
                glm::vec3 v = { n[0], n[1] - 1.0f, n[2] };
                v = glm::normalize(v);
                n = { v[0], v[1], v[2] };
            }

            // indices
            if (i == cols) {
                face = { baseIndex, baseIndex - 1, baseIndex - cols};
            } else {
                face = { baseIndex, baseIndex - cols - 1 + i, baseIndex - cols + i };
            }
            faces.emplace_back(face);
        }

        return mesh;
    }

    TriMesh createTorus(float outRadius, float inRadius, unsigned outSegments, unsigned inSegments, bool hasNormal, bool hasUV) {
        SIMPLE_GEOM_HEAD;

        outSegments = std::max(outSegments, (unsigned)3);
        inSegments = std::max(inSegments, (unsigned)3);

        verts.resize(outSegments * inSegments);
        if (hasNormal) {
            norms.resize(verts.size());
        }
        for (auto j = 0; j < inSegments; j++) {
            float theta = PI * 2.0 * j / inSegments - PI;
            float y = sin(theta) * inRadius;
            auto radius = outRadius + cos(theta) * inRadius;
            for (auto i = 0; i < outSegments; i++) {
                int index = j * outSegments + i;
                float phi = PI * 2.0f * i / outSegments;
                glm::vec3 pos = { cos(phi) * radius, y, sin(phi) * radius };
                glm::vec3 refCenter = { cos(phi) * outRadius, 0, sin(phi) * outRadius };
                verts[index] = { pos[0], pos[1], pos[2] };
                if (hasNormal) {
                    pos = normalize(pos - refCenter);
                    norms[index] = { pos[0], pos[1], pos[2] };
                }
            }
        }

        for (auto j = 0; j < inSegments; j++) {
            for (auto i = 0; i < outSegments; i++) {
                int index = j * outSegments + i;
                unsigned a = j * outSegments + i;
                unsigned b = ((j + 1) * outSegments + i) % (inSegments * outSegments);
                unsigned c = ((j + 1) * outSegments + (i + 1) % outSegments) % (inSegments * outSegments);
                unsigned d = j * outSegments + (i + 1) % outSegments;
                face = { a, b, c };
                faces.emplace_back(face);
                face = { a, c, d };
                faces.emplace_back(face);
            }
        }

        return mesh;
    }

    TriMesh createCone(float radius, float height, char axis, unsigned cols, bool hasNormal, bool hasUV) {
        SIMPLE_GEOM_HEAD;

        glm::mat3 axisMat = getAxisMatrix(axis);

        for (size_t i = 0; i < cols; i++) {
            float rad = 2.0f * PI * i / cols;
            vert = { cos(rad) * radius, -0.5f * height, -sin(rad) * radius };
            verts.emplace_back(vert);

            if (hasNormal) {
                glm::vec3 n(cos(rad) * height, radius, -sin(rad) * height);
                n = normalize(n) + glm::vec3(0.0f, -1.0f, 0.0f);
                n = glm::normalize(n);
                norm = { n[0], n[1], n[2] };
                norms.emplace_back(norm);
            }

            if (hasUV) {
                ; // TODO
            }
        }

        // the top one
        vert = { 0.0f, 0.5f * height, 0.0f };
        verts.emplace_back(vert);
        // the bottom one
        vert = { 0.0f, -0.5f * height, 0.0f };
        verts.emplace_back(vert);
        if (hasNormal) {
            norm = { 0.0f, 1.0f, 0.0f };
            norms.emplace_back(norm);
            norm = { 0.0f, -1.0f, 0.0f };
            norms.emplace_back(norm);
        }
        if (hasUV) {
            ; // TODO
        }

        applyMatrix(verts, axisMat);
        applyMatrix(norms, axisMat);

        for (unsigned i = 0; i < cols; i++) {
            face = { cols, i, (i + 1) % cols };
            faces.emplace_back(face);
            face = { i, cols + 1, (i + 1) % cols };
            faces.emplace_back(face);
        }

        return mesh;
    }

    TriMesh createCylinder(float radius, float height, char axis, unsigned cols, bool hasNormal, bool hasUV) {
        SIMPLE_GEOM_HEAD;

        glm::mat3 axisMat = getAxisMatrix(axis);

        // vertices of top circle
        for (size_t i = 0; i < cols; i++) {
            float rad = 2.0f * PI * i / cols;
            vert[0] = cos(rad) * radius;
            vert[1] = 0.5f * height;
            vert[2] = -sin(rad) * radius;
            verts.emplace_back(vert);
            if (hasNormal) {
                glm::vec3 n(cos(rad), 1.0f, -sin(rad));
                n = glm::normalize(n);
                norm = { n[0], n[1], n[2] };
                norms.emplace_back(norm);
            }
            if (hasUV) {
                ; // TODO
            }
        }
        // vertices of bottom circle
        for (size_t i = 0; i < cols; i++) {
            float rad = 2.0f * PI * i / cols;
            vert[0] = cos(rad) * radius;
            vert[1] = -0.5f * height;
            vert[2] = -sin(rad) * radius;
            verts.emplace_back(vert);
            if (hasNormal) {
                glm::vec3 n(cos(rad), -1.0f, -sin(rad));
                n = glm::normalize(n);
                norm = { n[0], n[1], n[2] };
                norms.emplace_back(norm);
            }
            if (hasUV) {
                ; // TODO
            }
        }
        vert[0] = 0.0f; vert[1] = 0.5f * height; vert[2] = 0.0f;
        verts.emplace_back(vert);
        vert[0] = 0.0f; vert[1] = -0.5f * height; vert[2] = 0.0f;
        verts.emplace_back(vert);
        if (hasNormal) {
            norm = { 0.0f, 1.0f, 0.0f };
            norms.emplace_back(norm);
            norm = { 0.0f, -1.0f, 0.0f };
            norms.emplace_back(norm);
        }
        if (hasUV) {
            ; // TODO
        }

        applyMatrix(verts, axisMat);
        applyMatrix(norms, axisMat);

        for (size_t i = 0; i < cols; i++) {
            face[0] = cols * 2;
            face[1] = i;
            face[2] = (i + 1) % cols;
            faces.emplace_back(face);
        }
        // Bottom
        for (size_t i = 0; i < cols; i++) {
            face[0] = i + cols;
            face[1] = cols * 2 + 1;
            face[2] = (i + 1) % cols + cols;
            faces.emplace_back(face);
        }
        // Side
        for (size_t i = 0; i < cols; i++) {
            face[0] = (i + 1) % cols; face[1] = i; face[2] = (i + 1) % cols + cols;
            faces.emplace_back(face);
            face[0] = (i + 1) % cols + cols; face[1] = i; face[2] = i + cols;
            faces.emplace_back(face);
        }

        return mesh;
    }

#undef SIMPLE_GEOM_HEAD
}
