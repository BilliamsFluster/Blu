#include "Blupch.h"
#include "Mesh.h"

namespace Blu
{
    Mesh::Mesh(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices)
    {
        m_IndexCount = (uint32_t)indices.size();

        m_VertexArray  = VertexArray::Create();
        m_VertexBuffer = VertexBuffer::Create((uint32_t)(vertices.size() * sizeof(Vertex3D)));
        m_VertexBuffer->SetData(vertices.data(), (uint32_t)(vertices.size() * sizeof(Vertex3D)));

        BufferLayout layout = {
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal"   },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float3, "a_Tangent"  },
        };
        m_VertexBuffer->SetLayout(layout);
        m_VertexArray->AddVertexBuffer(m_VertexBuffer);

        Shared<IndexBuffer> ib = IndexBuffer::Create(
            const_cast<uint32_t*>(indices.data()), m_IndexCount);
        m_VertexArray->AddIndexBuffer(ib);
    }

    Shared<Mesh> Mesh::CreateCube()
    {
        std::vector<Vertex3D> vertices = {
            // Front  (Z+)  tangent = +X (u increases rightward)
            { {-0.5f,-0.5f, 0.5f}, {0,0,1},  {0,0}, {1,0,0} },
            { { 0.5f,-0.5f, 0.5f}, {0,0,1},  {1,0}, {1,0,0} },
            { { 0.5f, 0.5f, 0.5f}, {0,0,1},  {1,1}, {1,0,0} },
            { {-0.5f, 0.5f, 0.5f}, {0,0,1},  {0,1}, {1,0,0} },
            // Back   (Z-)  tangent = -X (u=0 at +X side, u=1 at -X side)
            { { 0.5f,-0.5f,-0.5f}, {0,0,-1}, {0,0}, {-1,0,0} },
            { {-0.5f,-0.5f,-0.5f}, {0,0,-1}, {1,0}, {-1,0,0} },
            { {-0.5f, 0.5f,-0.5f}, {0,0,-1}, {1,1}, {-1,0,0} },
            { { 0.5f, 0.5f,-0.5f}, {0,0,-1}, {0,1}, {-1,0,0} },
            // Left   (X-)  tangent = +Z (u=0 at -Z, u=1 at +Z)
            { {-0.5f,-0.5f,-0.5f}, {-1,0,0}, {0,0}, {0,0,1} },
            { {-0.5f,-0.5f, 0.5f}, {-1,0,0}, {1,0}, {0,0,1} },
            { {-0.5f, 0.5f, 0.5f}, {-1,0,0}, {1,1}, {0,0,1} },
            { {-0.5f, 0.5f,-0.5f}, {-1,0,0}, {0,1}, {0,0,1} },
            // Right  (X+)  tangent = -Z (u=0 at +Z, u=1 at -Z)
            { { 0.5f,-0.5f, 0.5f}, {1,0,0},  {0,0}, {0,0,-1} },
            { { 0.5f,-0.5f,-0.5f}, {1,0,0},  {1,0}, {0,0,-1} },
            { { 0.5f, 0.5f,-0.5f}, {1,0,0},  {1,1}, {0,0,-1} },
            { { 0.5f, 0.5f, 0.5f}, {1,0,0},  {0,1}, {0,0,-1} },
            // Top    (Y+)  tangent = +X
            { {-0.5f, 0.5f, 0.5f}, {0,1,0},  {0,0}, {1,0,0} },
            { { 0.5f, 0.5f, 0.5f}, {0,1,0},  {1,0}, {1,0,0} },
            { { 0.5f, 0.5f,-0.5f}, {0,1,0},  {1,1}, {1,0,0} },
            { {-0.5f, 0.5f,-0.5f}, {0,1,0},  {0,1}, {1,0,0} },
            // Bottom (Y-)  tangent = +X
            { {-0.5f,-0.5f,-0.5f}, {0,-1,0}, {0,0}, {1,0,0} },
            { { 0.5f,-0.5f,-0.5f}, {0,-1,0}, {1,0}, {1,0,0} },
            { { 0.5f,-0.5f, 0.5f}, {0,-1,0}, {1,1}, {1,0,0} },
            { {-0.5f,-0.5f, 0.5f}, {0,-1,0}, {0,1}, {1,0,0} },
        };

        std::vector<uint32_t> indices = {
             0, 1, 2,  2, 3, 0,   // Front
             4, 5, 6,  6, 7, 4,   // Back
             8, 9,10, 10,11, 8,   // Left
            12,13,14, 14,15,12,   // Right
            16,17,18, 18,19,16,   // Top
            20,21,22, 22,23,20,   // Bottom
        };

        return std::make_shared<Mesh>(vertices, indices);
    }

    Shared<Mesh> Mesh::CreateQuad()
    {
        std::vector<Vertex3D> vertices = {
            { {-0.5f,-0.5f, 0.0f}, {0,0,1}, {0,0}, {1,0,0} },
            { { 0.5f,-0.5f, 0.0f}, {0,0,1}, {1,0}, {1,0,0} },
            { { 0.5f, 0.5f, 0.0f}, {0,0,1}, {1,1}, {1,0,0} },
            { {-0.5f, 0.5f, 0.0f}, {0,0,1}, {0,1}, {1,0,0} },
        };
        std::vector<uint32_t> indices = { 0,1,2, 2,3,0 };
        return std::make_shared<Mesh>(vertices, indices);
    }
}
