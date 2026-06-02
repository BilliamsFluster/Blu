#pragma once
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include "Blu/Core/Core.h"
#include "Buffer.h"
#include "VertexArray.h"

// Forward-declared to avoid circular include; defined in Animation.h
namespace Blu { struct SkeletonData; }

namespace Blu
{
    struct Vertex3D
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
        glm::vec3 Tangent;
    };

    struct MeshVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 TexCoord;
        glm::vec3 Tangent;
    };

    static constexpr int kMaxBonesPerVertex = 4;

    // Extended vertex for skinned meshes.  Bone indices/weights are set by
    // ModelLoader when the mesh has skinning data; otherwise all zero/1.0.
    struct SkinnedMeshVertex
    {
        glm::vec3  Position;
        glm::vec3  Normal;
        glm::vec2  TexCoord;
        glm::vec3  Tangent;
        glm::ivec4 BoneIDs    = glm::ivec4(0);
        glm::vec4  BoneWeights = glm::vec4(0.0f);
    };

    struct SubMesh
    {
        std::vector<MeshVertex>  Vertices;
        std::vector<uint32_t>    Indices;
        uint32_t                 IndexCount = 0;   // cached before Indices is freed post-upload
        int                      MaterialIndex = -1;
        Shared<VertexArray>      VAO;
        glm::mat4                LocalTransform = glm::mat4(1.0f); // node-space → model root space
        glm::vec3                BoundingCenter = glm::vec3(0.0f); // in local mesh space
        float                    BoundingRadius = 0.0f;
    };

    // A submesh variant with skinning data; shares the same Material/index system.
    struct SkinnedSubMesh
    {
        std::vector<SkinnedMeshVertex> Vertices;
        std::vector<uint32_t>          Indices;
        uint32_t                       IndexCount    = 0;
        int                            MaterialIndex = -1;
        Shared<VertexArray>            VAO;
        glm::mat4                      LocalTransform = glm::mat4(1.0f);
    };

    struct Model
    {
        std::vector<SubMesh>                     Meshes;        // static (no skin) draw path
        std::vector<SkinnedSubMesh>              SkinnedMeshes; // skinned draw path
        std::vector<Shared<class Material>>      Materials;
        std::string                              FilePath;

        // Non-null when the model has skeletal animation data.
        // ModelLoader fills this when bones are present in the file.
        Shared<struct SkeletonData>              SkelData;

        bool HasSkeleton() const { return SkelData != nullptr && !SkinnedMeshes.empty(); }
    };

    class Mesh
    {
    public:
        Mesh(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices);

        const Shared<VertexArray>& GetVertexArray() const { return m_VertexArray; }
        uint32_t GetIndexCount() const { return m_IndexCount; }

        // Local-space bounding sphere, computed from the vertices at construction.
        // Used for frustum culling of primitive (non-Model) meshes in Renderer3D.
        const glm::vec3& GetBoundingCenter() const { return m_BoundingCenter; }
        float            GetBoundingRadius() const { return m_BoundingRadius; }

        static Shared<Mesh> CreateCube();
        static Shared<Mesh> CreateQuad();

    private:
        Shared<VertexArray>  m_VertexArray;
        Shared<VertexBuffer> m_VertexBuffer;
        uint32_t             m_IndexCount = 0;
        glm::vec3            m_BoundingCenter = glm::vec3(0.0f);
        float                m_BoundingRadius = 0.0f;
    };
}
