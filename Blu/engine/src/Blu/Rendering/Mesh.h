#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include "Blu/Core/Core.h"
#include "Buffer.h"
#include "VertexArray.h"

namespace Blu
{
    struct Vertex3D
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
    };

    struct MeshVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
    };

    struct SubMesh
    {
        std::vector<MeshVertex>  Vertices;
        std::vector<uint32_t>    Indices;
        int                      MaterialIndex = -1;
        Shared<VertexArray>      VAO;
    };

    struct Model
    {
        std::vector<SubMesh>     Meshes;
        std::vector<Shared<class Material>> Materials;
        std::string              FilePath;
    };

    class Mesh
    {
    public:
        Mesh(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices);

        const Shared<VertexArray>& GetVertexArray() const { return m_VertexArray; }
        uint32_t GetIndexCount() const { return m_IndexCount; }

        static Shared<Mesh> CreateCube();
        static Shared<Mesh> CreateQuad();

    private:
        Shared<VertexArray>  m_VertexArray;
        Shared<VertexBuffer> m_VertexBuffer;
        uint32_t             m_IndexCount = 0;
    };
}
