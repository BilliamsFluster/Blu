#include "Blupch.h"
#include "ModelLoader.h"
#include "Blu/Rendering/RendererAPI.h"
#include "Blu/Rendering/Material.h"
#include "Blu/Rendering/Texture.h"
#include "Blu/Core/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>

namespace Blu
{
    static void ProcessMesh(aiMesh* mesh, SubMesh& out)
    {
        out.Vertices.reserve(mesh->mNumVertices);
        for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
        {
            MeshVertex v;
            v.Position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            v.Normal = mesh->HasNormals()
                ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z)
                : glm::vec3(0, 1, 0);
            v.TexCoord = mesh->mTextureCoords[0]
                ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                : glm::vec2(0);
            out.Vertices.push_back(v);
        }

        out.Indices.reserve(mesh->mNumFaces * 3);
        for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
        {
            const aiFace& face = mesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; ++j)
                out.Indices.push_back(face.mIndices[j]);
        }
        out.MaterialIndex = mesh->mMaterialIndex;
    }

    static void ProcessMaterials(const aiScene* scene, Model& model, const std::string& modelDir)
    {
        if (!scene->HasMaterials()) return;

        model.Materials.resize(scene->mNumMaterials);
        for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
        {
            aiMaterial* aiMat = scene->mMaterials[i];
            auto material = Material::Create();

            // Colors
            aiColor3D ambient(0.2f, 0.2f, 0.2f);
            aiMat->Get(AI_MATKEY_COLOR_AMBIENT, ambient);
            aiColor3D diffuse(0.8f, 0.8f, 0.8f);
            aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
            aiColor3D specular(0.5f, 0.5f, 0.5f);
            aiMat->Get(AI_MATKEY_COLOR_SPECULAR, specular);
            float shininess = 32.0f;
            aiMat->Get(AI_MATKEY_SHININESS, shininess);

            glm::vec3 amb(ambient.r, ambient.g, ambient.b);
            glm::vec3 dif(diffuse.r, diffuse.g, diffuse.b);
            glm::vec3 spe(specular.r, specular.g, specular.b);

            material->SetAmbientColor(amb);
            material->SetDiffuseColor(dif);
            material->SetSpecularColor(spe);
            material->SetShininess(shininess);

            // Diffuse texture
            aiString texPath;
            if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS)
            {
                std::string texStr(texPath.C_Str());
                // Resolve relative path against model directory
                if (!std::filesystem::path(texStr).is_absolute())
                    texStr = (std::filesystem::path(modelDir) / texStr).string();
                if (std::filesystem::exists(texStr))
                {
                    material->AlbedoMap = Texture2D::Create(texStr);
                    material->DiffuseMap = material->AlbedoMap;
                }
                else
                {
                    BLU_CORE_WARN("ModelLoader: texture not found: {0}", texStr);
                }
            }

            aiString specTexPath;
            if (aiMat->GetTexture(aiTextureType_SPECULAR, 0, &specTexPath) == AI_SUCCESS)
            {
                std::string texStr(specTexPath.C_Str());
                if (!std::filesystem::path(texStr).is_absolute())
                    texStr = (std::filesystem::path(modelDir) / texStr).string();
                if (std::filesystem::exists(texStr))
                    material->SpecularMap = Texture2D::Create(texStr);
            }

            aiString normTexPath;
            if (aiMat->GetTexture(aiTextureType_NORMALS, 0, &normTexPath) == AI_SUCCESS)
            {
                std::string texStr(normTexPath.C_Str());
                if (!std::filesystem::path(texStr).is_absolute())
                    texStr = (std::filesystem::path(modelDir) / texStr).string();
                if (std::filesystem::exists(texStr))
                    material->NormalMap = Texture2D::Create(texStr);
            }

            model.Materials[i] = material;
        }
    }

    static void ProcessNode(aiNode* node, const aiScene* scene, Model& model)
    {
        for (uint32_t i = 0; i < node->mNumMeshes; ++i)
        {
            SubMesh sub;
            ProcessMesh(scene->mMeshes[node->mMeshes[i]], sub);
            model.Meshes.push_back(std::move(sub));
        }
        for (uint32_t i = 0; i < node->mNumChildren; ++i)
            ProcessNode(node->mChildren[i], scene, model);
    }

    Shared<Model> ModelLoader::Load(const std::string& path)
    {
        Assimp::Importer importer;
        uint32_t flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals
                       | aiProcess_PreTransformVertices
                       | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace;
        if (RendererAPI::GetAPI() != RendererAPI::API::Direct3D)
            flags |= aiProcess_FlipUVs;

        const aiScene* scene = importer.ReadFile(path, flags);
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            BLU_CORE_ERROR("Assimp: {0}", importer.GetErrorString());
            return nullptr;
        }

        auto model = std::make_shared<Model>();
        model->FilePath = path;
        ProcessNode(scene->mRootNode, scene, *model);

        // Extract materials
        std::string modelDir = std::filesystem::path(path).parent_path().string();
        ProcessMaterials(scene, *model, modelDir);

        return model;
    }
}
