#include "Blupch.h"
#include "ModelLoader.h"
#include "Animation.h"
#include "Blu/Rendering/RendererAPI.h"
#include "Blu/Rendering/Material.h"
#include "Blu/Rendering/Texture.h"
#include "Blu/Core/Log.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>

namespace Blu
{
    namespace fs = std::filesystem;

    // Returns the "assets/textures" directory, creating it if needed.
    static fs::path GetAssetsTextureDir()
    {
        fs::path texDir = fs::path("assets") / "textures";
        if (!fs::exists(texDir))
            fs::create_directories(texDir);
        return texDir;
    }

    // Returns the destination path in assets/textures/<modelName>/ for a texture.
    static fs::path GetTextureAssetPath(const std::string& modelName,
                                         const std::string& filename)
    {
        fs::path outDir = GetAssetsTextureDir() / modelName;
        if (!fs::exists(outDir))
            fs::create_directories(outDir);
        return outDir / filename;
    }

    // Write texture data into assets/textures/<modelName>/<filename>.
    static fs::path WriteTextureAsset(const std::string& modelName,
                                       const std::string& filename,
                                       const void* data, size_t dataSize)
    {
        fs::path outPath = GetTextureAssetPath(modelName, filename);
        std::ofstream f(outPath, std::ios::binary);
        if (f.is_open())
        {
            f.write(static_cast<const char*>(data), dataSize);
            f.close();
        }
        return outPath;
    }

    static void ProcessMesh(aiMesh* mesh, SubMesh& out)
    {
        const bool hasTangents = mesh->HasTangentsAndBitangents();
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
            v.Tangent = hasTangents
                ? glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z)
                : glm::vec3(1, 0, 0);
            out.Vertices.push_back(v);
        }

        out.Indices.reserve(mesh->mNumFaces * 3);
        for (uint32_t i = 0; i < mesh->mNumFaces; ++i)
        {
            const aiFace& face = mesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; ++j)
                out.Indices.push_back(face.mIndices[j]);
        }
        out.IndexCount    = (uint32_t)out.Indices.size();
        out.MaterialIndex = mesh->mMaterialIndex;
    }

    // ─── Detect image format from magic bytes ────────────────────────────────
    static std::string DetectImageExtension(const void* data, size_t dataSize)
    {
        if (!data || dataSize < 2) return ".png";
        const uint8_t* b = static_cast<const uint8_t*>(data);
        // PNG: 89 50 4E 47
        if (dataSize >= 4 && b[0] == 0x89 && b[1] == 0x50 && b[2] == 0x4E && b[3] == 0x47)
            return ".png";
        // JPEG: FF D8 FF
        if (dataSize >= 3 && b[0] == 0xFF && b[1] == 0xD8 && b[2] == 0xFF)
            return ".jpg";
        // DDS: 44 44 53 20 ("DDS ")
        if (dataSize >= 4 && b[0] == 0x44 && b[1] == 0x44 && b[2] == 0x53 && b[3] == 0x20)
            return ".dds";
        // BMP: 42 4D
        if (b[0] == 0x42 && b[1] == 0x4D)
            return ".bmp";
        // GIF: 47 49 46
        if (dataSize >= 3 && b[0] == 0x47 && b[1] == 0x49 && b[2] == 0x46)
            return ".gif";
        // TIFF little-endian: 49 49 / big-endian: 4D 4D
        if ((b[0] == 0x49 && b[1] == 0x49) || (b[0] == 0x4D && b[1] == 0x4D))
            return ".tiff";
        return ".png"; // fallback
    }

    // ─── Helper: load a texture from an assimp path, copying to assets if needed ─
    static Shared<Texture2D> LoadTexture(const std::string& texPath,
                                          const std::string& modelName,
                                          const std::string& modelDir)
    {
        // Blender exports FBX with "//" as a relative-path prefix (relative to
        // the source .blend / export directory). Strip it before resolving.
        std::string stripped = texPath;
        if (stripped.size() >= 2 && stripped[0] == '/' && stripped[1] == '/')
            stripped = stripped.substr(2);

        std::string resolved(stripped);
        if (!fs::path(resolved).is_absolute())
            resolved = (fs::path(modelDir) / resolved).string();

        std::string foundPath;
        if (fs::exists(resolved))
            foundPath = resolved;
        else
        {
            // Fallback 1: texture alongside the FBX (absolute path from another machine)
            fs::path sameDir = fs::path(modelDir) / fs::path(stripped).filename();
            if (fs::exists(sameDir))
                foundPath = sameDir.string();
        }

        std::string filename = fs::path(stripped).filename().string();

        if (foundPath.empty() && !filename.empty())
        {
            // Fallback 2: subdirectories of modelDir (e.g. "texture/", "Textures/", "tex/")
            std::error_code ec;
            for (auto& entry : fs::directory_iterator(modelDir, ec))
            {
                if (!entry.is_directory(ec)) continue;
                fs::path candidate = entry.path() / filename;
                if (fs::exists(candidate, ec)) { foundPath = candidate.string(); break; }
            }
        }

        if (foundPath.empty() && !filename.empty())
        {
            // Fallback 3: sibling directories of modelDir (parent's children).
            // Handles the common Sketchfab layout where the FBX lives in
            // "ModelName/" and the textures in a sibling "tex/" or "textures/" folder.
            std::error_code ec;
            fs::path parentDir = fs::path(modelDir).parent_path();
            for (auto& entry : fs::directory_iterator(parentDir, ec))
            {
                if (!entry.is_directory(ec)) continue;
                if (entry.path() == fs::path(modelDir)) continue; // skip self
                fs::path candidate = entry.path() / filename;
                if (fs::exists(candidate, ec)) { foundPath = candidate.string(); break; }
            }
        }

        if (foundPath.empty())
        {
            // Fallback 4: assets/textures/<modelName>/<filename>
            fs::path assetFallback = GetTextureAssetPath(modelName, filename);
            if (fs::exists(assetFallback))
                foundPath = assetFallback.string();
            else
            {
                // Fallback 5: assets/textures/<filename>
                fs::path globalFallback = GetAssetsTextureDir() / filename;
                if (fs::exists(globalFallback))
                    foundPath = globalFallback.string();
            }
        }

        if (!foundPath.empty())
        {
            // Copy to assets/textures/<modelName>/ so scene serialisation uses a stable path
            fs::path assetPath = GetTextureAssetPath(modelName, fs::path(stripped).filename().string());
            if (fs::path(foundPath) != assetPath && !fs::exists(assetPath))
            {
                std::ifstream src(foundPath, std::ios::binary);
                std::ofstream dst(assetPath, std::ios::binary);
                if (src.is_open() && dst.is_open())
                    dst << src.rdbuf();
            }
            return Texture2D::Create(assetPath.string());
        }

        BLU_CORE_WARN("ModelLoader: texture not found: {0}", resolved);
        return nullptr;
    }

    // ─── Helper: load a texture from an embedded assimp texture ──────────────
    static Shared<Texture2D> LoadEmbeddedTexture(const aiTexture* aiTex,
                                                  const std::string& name,
                                                  const std::string& modelName)
    {
        if (!aiTex) return nullptr;

        // Save embedded texture as an asset in assets/textures/<modelName>/
        if (aiTex->mHeight == 0 && aiTex->pcData)
        {
            // Compressed embedded texture — write raw bytes via the asset helper
            fs::path assetPath = WriteTextureAsset(modelName, name,
                                                    aiTex->pcData, aiTex->mWidth);
            return Texture2D::Create(assetPath.string());
        }
        else if (aiTex->mHeight > 0 && aiTex->pcData)
        {
            BLU_CORE_WARN("ModelLoader: uncompressed embedded texture not yet supported: {0}", name);
        }

        return nullptr;
    }

    // ─── Helper: resolve path and load if not embedded ───────────────────────
    static Shared<Texture2D> ResolveTexture(const aiMaterial* aiMat, aiTextureType type,
                                             uint32_t index, const aiScene* scene,
                                             const std::string& modelName,
                                             const std::string& modelDir)
    {
        aiString texPath;
        if (aiMat->GetTexture(type, index, &texPath) != AI_SUCCESS)
            return nullptr;

        std::string pathStr(texPath.C_Str());

        // Check for embedded texture reference (*0, *1, etc.)
        if (pathStr[0] == '*')
        {
            int embeddedIndex = std::stoi(pathStr.substr(1));
            if (embeddedIndex >= 0 && embeddedIndex < (int)scene->mNumTextures)
            {
                const aiTexture* aiTex = scene->mTextures[embeddedIndex];
                // mFilename may be an absolute path from the artist's machine — use only the leaf name.
                std::string texName = fs::path(aiTex->mFilename.C_Str()).filename().string();
                if (texName.empty())
                {
                    // No original filename — detect from data header
                    if (aiTex->mHeight == 0 && aiTex->pcData)
                        texName = "embedded_" + std::to_string(embeddedIndex)
                                + DetectImageExtension(aiTex->pcData, aiTex->mWidth);
                    else
                        texName = "embedded_" + std::to_string(embeddedIndex) + ".png";
                }
                return LoadEmbeddedTexture(aiTex, texName, modelName);
            }
            return nullptr;
        }

        // Embedded texture key match (some FBX exporters embed by name, not index)
        for (uint32_t t = 0; t < scene->mNumTextures; ++t)
        {
            if (scene->mTextures[t]->mFilename.C_Str() == pathStr)
            {
                std::string texName = fs::path(scene->mTextures[t]->mFilename.C_Str()).filename().string();
                if (texName.empty()) texName = "embedded_" + std::to_string(t) + ".png";
                return LoadEmbeddedTexture(scene->mTextures[t], texName, modelName);
            }
        }

        // Regular file path
        return LoadTexture(pathStr, modelName, modelDir);
    }

    static void ProcessMaterials(const aiScene* scene, Model& model,
                                  const std::string& modelName,
                                  const std::string& modelDir)
    {
        if (!scene->HasMaterials()) return;

        model.Materials.resize(scene->mNumMaterials);
        for (uint32_t i = 0; i < scene->mNumMaterials; ++i)
        {
            aiMaterial* aiMat = scene->mMaterials[i];
            auto material = Material::Create();

            aiString matName;
            if (aiMat->Get(AI_MATKEY_NAME, matName) == AI_SUCCESS && matName.length > 0)
                material->Name = matName.C_Str();
            else
                material->Name = "Material " + std::to_string(i);

            // ─── PBR scalar properties ───────────────────────────────────────
            aiColor3D diffuse(0.8f, 0.8f, 0.8f);
            aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, diffuse);
            material->AlbedoColor = glm::vec4(diffuse.r, diffuse.g, diffuse.b, 1.0f);

            aiColor3D emissive(0.0f, 0.0f, 0.0f);
            aiMat->Get(AI_MATKEY_COLOR_EMISSIVE, emissive);
            material->EmissiveColor = glm::vec3(emissive.r, emissive.g, emissive.b);

            // assimp pre-5.2 doesn't expose PBR scalar keys for FBX — use safe defaults
            material->Metallic  = 0.0f;
            material->Roughness = 0.5f;

            // ─── PBR textures ───────────────────────────────────────────────
            // Albedo: DIFFUSE is the reliable slot in assimp pre-5.2 FBX
            material->AlbedoMap = ResolveTexture(aiMat, aiTextureType_DIFFUSE, 0, scene, modelName, modelDir);

            // Normal: NORMALS first, HEIGHT as fallback (some exporters use HEIGHT for normal maps)
            {
                auto tex = ResolveTexture(aiMat, aiTextureType_NORMALS, 0, scene, modelName, modelDir);
                if (!tex)
                    tex = ResolveTexture(aiMat, aiTextureType_HEIGHT, 0, scene, modelName, modelDir);
                material->NormalMap = tex;
            }

            // Metallic/roughness: UNKNOWN is the only reliable packed ORM slot in assimp pre-5.2
            material->MetallicRoughnessMap = ResolveTexture(aiMat, aiTextureType_UNKNOWN, 0, scene, modelName, modelDir);

            // AO: LIGHTMAP is the conventional baked-AO slot
            material->AOMap = ResolveTexture(aiMat, aiTextureType_LIGHTMAP, 0, scene, modelName, modelDir);

            // Emissive
            material->EmissiveMap = ResolveTexture(aiMat, aiTextureType_EMISSIVE, 0, scene, modelName, modelDir);

            model.Materials[i] = material;
        }
    }

    // ─── Merge all submeshes that share the same material into one draw call ───
    // Bakes each submesh's LocalTransform into its vertex positions/normals so the
    // merged submesh can use an identity LocalTransform. Reduces draw calls from
    // N_submeshes down to N_unique_materials.
    static void MergeSubmeshesByMaterial(Model& model)
    {
        if (model.Meshes.empty()) return;

        std::vector<int>                   insertionOrder;
        std::unordered_map<int, SubMesh>   groups;

        for (auto& src : model.Meshes)
        {
            int key = src.MaterialIndex;
            if (groups.find(key) == groups.end())
            {
                insertionOrder.push_back(key);
                groups[key].MaterialIndex  = key;
                groups[key].LocalTransform = glm::mat4(1.0f); // transforms baked in below
            }

            SubMesh& dst        = groups[key];
            uint32_t indexBase  = (uint32_t)dst.Vertices.size();

            // Transform positions, normals, and tangents into a common (root-relative) space.
            glm::mat3 normalMat  = glm::transpose(glm::inverse(glm::mat3(src.LocalTransform)));
            glm::mat3 linearMat  = glm::mat3(src.LocalTransform);
            for (const auto& v : src.Vertices)
            {
                MeshVertex xv;
                xv.Position = glm::vec3(src.LocalTransform * glm::vec4(v.Position, 1.0f));
                xv.Normal   = glm::normalize(normalMat * v.Normal);
                xv.TexCoord = v.TexCoord;
                xv.Tangent  = glm::normalize(linearMat * v.Tangent);
                dst.Vertices.push_back(xv);
            }

            for (uint32_t idx : src.Indices)
                dst.Indices.push_back(idx + indexBase);
        }

        model.Meshes.clear();
        for (int key : insertionOrder)
        {
            auto& sub      = groups[key];
            sub.IndexCount = (uint32_t)sub.Indices.size();
            model.Meshes.push_back(std::move(sub));
        }
    }

    // Convert Assimp row-major matrix to GLM column-major.
    static glm::mat4 AiToGlm(const aiMatrix4x4& m)
    {
        return glm::mat4(
            m.a1, m.b1, m.c1, m.d1,
            m.a2, m.b2, m.c2, m.d2,
            m.a3, m.b3, m.c3, m.d3,
            m.a4, m.b4, m.c4, m.d4);
    }

    static void ProcessNode(aiNode* node, const aiScene* scene, Model& model,
                            const glm::mat4& parentTransform)
    {
        glm::mat4 nodeTransform = parentTransform * AiToGlm(node->mTransformation);
        for (uint32_t i = 0; i < node->mNumMeshes; ++i)
        {
            SubMesh sub;
            ProcessMesh(scene->mMeshes[node->mMeshes[i]], sub);
            sub.LocalTransform = nodeTransform;
            model.Meshes.push_back(std::move(sub));
        }
        for (uint32_t i = 0; i < node->mNumChildren; ++i)
            ProcessNode(node->mChildren[i], scene, model, nodeTransform);
    }

    // ─── Upload submesh vertex/index data to GPU ─────────────────────────────
    static void UploadSubMeshGPU(SubMesh& submesh)
    {
        if (submesh.VAO) return;

        submesh.VAO = VertexArray::Create();
        Shared<VertexBuffer> vb = VertexBuffer::Create((uint32_t)(submesh.Vertices.size() * sizeof(MeshVertex)));
        vb->SetData(submesh.Vertices.data(), (uint32_t)(submesh.Vertices.size() * sizeof(MeshVertex)));
        BufferLayout layout = {
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float3, "a_Tangent" },
        };
        vb->SetLayout(layout);
        submesh.VAO->AddVertexBuffer(vb);
        Shared<IndexBuffer> ib = IndexBuffer::Create(submesh.Indices.data(), (uint32_t)submesh.Indices.size());
        submesh.VAO->AddIndexBuffer(ib);
    }

    static void UploadSkinnedSubMeshGPU(SkinnedSubMesh& submesh)
    {
        if (submesh.VAO) return;

        submesh.VAO = VertexArray::Create();
        auto vb = VertexBuffer::Create((uint32_t)(submesh.Vertices.size() * sizeof(SkinnedMeshVertex)));
        vb->SetData(submesh.Vertices.data(), (uint32_t)(submesh.Vertices.size() * sizeof(SkinnedMeshVertex)));
        BufferLayout layout = {
            { ShaderDataType::Float3, "a_Position" },
            { ShaderDataType::Float3, "a_Normal" },
            { ShaderDataType::Float2, "a_TexCoord" },
            { ShaderDataType::Float3, "a_Tangent" },
            { ShaderDataType::Int4,   "a_BoneIDs" },
            { ShaderDataType::Float4, "a_BoneWeights" },
        };
        vb->SetLayout(layout);
        submesh.VAO->AddVertexBuffer(vb);
        auto ib = IndexBuffer::Create(submesh.Indices.data(), (uint32_t)submesh.Indices.size());
        submesh.VAO->AddIndexBuffer(ib);
    }

    // ─── Bone extraction ──────────────────────────────────────────────────────

    // Extract skeleton data (bone info map + hierarchy) and animation clips
    // from an aiScene.  Returns null when no bones are present.
    static Shared<SkeletonData> ExtractSkeletonData(const aiScene* scene,
                                                     const glm::mat4& rootTransform)
    {
        // Quick check: does any mesh have bones?
        bool hasBones = false;
        for (uint32_t m = 0; m < scene->mNumMeshes && !hasBones; ++m)
            hasBones = scene->mMeshes[m]->mNumBones > 0;
        if (!hasBones) return nullptr;

        auto skelData   = std::make_shared<SkeletonData>();
        skelData->Skel  = std::make_shared<Skeleton>();
        Skeleton& skel  = *skelData->Skel;

        // Global inverse transform (root-level correction for coordinate system)
        skel.GlobalInverseTransform = glm::inverse(rootTransform);

        // Build bone info map from all meshes
        for (uint32_t m = 0; m < scene->mNumMeshes; ++m)
        {
            aiMesh* mesh = scene->mMeshes[m];
            for (uint32_t b = 0; b < mesh->mNumBones; ++b)
            {
                aiBone* bone = mesh->mBones[b];
                std::string boneName(bone->mName.C_Str());

                if (skel.BoneInfoMap.find(boneName) == skel.BoneInfoMap.end())
                {
                    BoneInfo bi;
                    bi.ID           = skel.NumBones++;
                    bi.OffsetMatrix = AiToGlm(bone->mOffsetMatrix);
                    skel.BoneInfoMap[boneName] = bi;
                }
            }
        }

        // Recursively build node hierarchy (stores default local transforms)
        std::function<void(const aiNode*, BoneNode&)> BuildNode =
            [&](const aiNode* ainode, BoneNode& out)
            {
                out.Name           = ainode->mName.C_Str();
                out.LocalTransform = AiToGlm(ainode->mTransformation);
                out.Children.resize(ainode->mNumChildren);
                for (uint32_t c = 0; c < ainode->mNumChildren; ++c)
                    BuildNode(ainode->mChildren[c], out.Children[c]);
            };
        BuildNode(scene->mRootNode, skel.RootNode);

        // Extract animation clips
        skelData->Clips.reserve(scene->mNumAnimations);
        for (uint32_t a = 0; a < scene->mNumAnimations; ++a)
        {
            aiAnimation* aiAnim = scene->mAnimations[a];
            AnimationClip clip;
            clip.Name        = aiAnim->mName.C_Str();
            if (clip.Name.empty()) clip.Name = "Anim_" + std::to_string(a);
            clip.Duration    = (float)aiAnim->mDuration;
            clip.TicksPerSec = (float)(aiAnim->mTicksPerSecond > 0.0 ? aiAnim->mTicksPerSecond : 25.0);

            clip.Channels.reserve(aiAnim->mNumChannels);
            for (uint32_t c = 0; c < aiAnim->mNumChannels; ++c)
            {
                aiNodeAnim* chan = aiAnim->mChannels[c];
                BoneChannel bc;
                bc.Name = chan->mNodeName.C_Str();

                auto boneIt = skel.BoneInfoMap.find(bc.Name);
                bc.BoneID = (boneIt != skel.BoneInfoMap.end()) ? boneIt->second.ID : -1;

                bc.Positions.reserve(chan->mNumPositionKeys);
                for (uint32_t k = 0; k < chan->mNumPositionKeys; ++k)
                {
                    const auto& key = chan->mPositionKeys[k];
                    bc.Positions.push_back({ glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z),
                                             (float)key.mTime });
                }

                bc.Rotations.reserve(chan->mNumRotationKeys);
                for (uint32_t k = 0; k < chan->mNumRotationKeys; ++k)
                {
                    const auto& key = chan->mRotationKeys[k];
                    bc.Rotations.push_back({ glm::quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z),
                                             (float)key.mTime });
                }

                bc.Scales.reserve(chan->mNumScalingKeys);
                for (uint32_t k = 0; k < chan->mNumScalingKeys; ++k)
                {
                    const auto& key = chan->mScalingKeys[k];
                    bc.Scales.push_back({ glm::vec3(key.mValue.x, key.mValue.y, key.mValue.z),
                                          (float)key.mTime });
                }

                clip.Channels.push_back(std::move(bc));
            }
            clip.BuildChannelMap();
            skelData->Clips.push_back(std::move(clip));
        }

        return skelData;
    }

    // Build a SkinnedSubMesh from an aiMesh, populating bone indices/weights.
    static void ProcessSkinnedMesh(aiMesh* mesh, SkinnedSubMesh& out,
                                   const Skeleton& skel)
    {
        const bool hasTangents = mesh->HasTangentsAndBitangents();
        out.Vertices.resize(mesh->mNumVertices);

        for (uint32_t i = 0; i < mesh->mNumVertices; ++i)
        {
            auto& v = out.Vertices[i];
            v.Position  = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
            v.Normal    = mesh->HasNormals()
                ? glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z)
                : glm::vec3(0, 1, 0);
            v.TexCoord  = mesh->mTextureCoords[0]
                ? glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y)
                : glm::vec2(0);
            v.Tangent   = hasTangents
                ? glm::vec3(mesh->mTangents[i].x, mesh->mTangents[i].y, mesh->mTangents[i].z)
                : glm::vec3(1, 0, 0);
            v.BoneIDs     = glm::ivec4(0);
            v.BoneWeights = glm::vec4(0.0f);
        }

        // Distribute bone weights across vertex slots (up to 4 per vertex)
        for (uint32_t b = 0; b < mesh->mNumBones; ++b)
        {
            aiBone* bone     = mesh->mBones[b];
            std::string name = bone->mName.C_Str();
            auto it          = skel.BoneInfoMap.find(name);
            if (it == skel.BoneInfoMap.end()) continue;
            int boneID = it->second.ID;

            for (uint32_t w = 0; w < bone->mNumWeights; ++w)
            {
                uint32_t vIdx  = bone->mWeights[w].mVertexId;
                float    weight = bone->mWeights[w].mWeight;
                if (vIdx >= out.Vertices.size()) continue;

                auto& v = out.Vertices[vIdx];
                for (int slot = 0; slot < kMaxBonesPerVertex; ++slot)
                {
                    if (v.BoneWeights[slot] == 0.0f)
                    {
                        v.BoneIDs[slot]     = boneID;
                        v.BoneWeights[slot] = weight;
                        break;
                    }
                }
            }
        }

        out.Indices.reserve(mesh->mNumFaces * 3);
        for (uint32_t f = 0; f < mesh->mNumFaces; ++f)
        {
            const aiFace& face = mesh->mFaces[f];
            for (uint32_t j = 0; j < face.mNumIndices; ++j)
                out.Indices.push_back(face.mIndices[j]);
        }
        out.IndexCount    = (uint32_t)out.Indices.size();
        out.MaterialIndex = (int)mesh->mMaterialIndex;
    }

    // Traverse nodes and build SkinnedSubMesh list for models that have bones.
    static void ProcessSkinnedNode(aiNode* node, const aiScene* scene,
                                   Model& model, const Skeleton& skel,
                                   const glm::mat4& parentTransform)
    {
        glm::mat4 nodeTransform = parentTransform * AiToGlm(node->mTransformation);
        for (uint32_t i = 0; i < node->mNumMeshes; ++i)
        {
            SkinnedSubMesh sub;
            ProcessSkinnedMesh(scene->mMeshes[node->mMeshes[i]], sub, skel);
            sub.LocalTransform = nodeTransform;
            model.SkinnedMeshes.push_back(std::move(sub));
        }
        for (uint32_t i = 0; i < node->mNumChildren; ++i)
            ProcessSkinnedNode(node->mChildren[i], scene, model, skel, nodeTransform);
    }

    Shared<Model> ModelLoader::Load(const std::string& path)
    {
        Assimp::Importer importer;
        // Note: aiProcess_PreTransformVertices is intentionally omitted.
        // Assimp 4.x writes no UpAxis metadata and applies no coordinate conversion,
        // so PreTransformVertices would bake Z-up positions without any correction.
        // Instead we traverse the node hierarchy and accumulate LocalTransform per
        // submesh; the FBX root-node rotation (if present) is preserved and applied
        // at render time via entityTransform * submesh.LocalTransform.
        uint32_t flags = aiProcess_Triangulate | aiProcess_GenSmoothNormals
                       | aiProcess_JoinIdenticalVertices | aiProcess_CalcTangentSpace;
        if (RendererAPI::GetAPI() == RendererAPI::API::Direct3D)
            flags |= aiProcess_FlipUVs;

        const aiScene* scene = importer.ReadFile(path, flags);
        if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
        {
            BLU_CORE_ERROR("Assimp: {0}", importer.GetErrorString());
            return nullptr;
        }

        auto model = std::make_shared<Model>();
        model->FilePath = path;

        // Assimp 4.x writes the FBX GlobalSettings UnitScaleFactor to metadata.
        // UnitScaleFactor = centimetres per scene unit (1 = cm, 100 = metres).
        // Engine units are metres, so the root scale = UnitScaleFactor / 100.
        double unitScaleFactor = 1.0;
        if (scene->mMetaData)
            scene->mMetaData->Get("UnitScaleFactor", unitScaleFactor);
        float rootScale = (unitScaleFactor > 0.0) ? (float)(unitScaleFactor / 100.0) : 1.0f;
        glm::mat4 rootTransform = glm::scale(glm::mat4(1.0f), glm::vec3(rootScale));

        // ─── Skeleton / animation extraction (before node traversal) ────────
        model->SkelData = ExtractSkeletonData(scene, rootTransform);
        const bool hasSkeleton = model->SkelData != nullptr;

        if (hasSkeleton)
        {
            // For skinned models, traverse to collect SkinnedSubMesh (preserves bone data).
            ProcessSkinnedNode(scene->mRootNode, scene, *model,
                               *model->SkelData->Skel, rootTransform);
        }
        else
        {
            ProcessNode(scene->mRootNode, scene, *model, rootTransform);
        }

        // Extract materials
        std::string modelDir  = fs::path(path).parent_path().string();
        std::string modelName = fs::path(path).stem().string();
        ProcessMaterials(scene, *model, modelName, modelDir);

        if (!hasSkeleton)
        {
            // Merge submeshes that share a material into a single draw call.
            // Skip for skinned meshes (bone indices are relative to submesh).
            MergeSubmeshesByMaterial(*model);
        }

        if (hasSkeleton)
        {
            BLU_CORE_INFO("ModelLoader: {0} - {1} skinned submesh(es), {2} animation(s)",
                modelName, model->SkinnedMeshes.size(),
                model->SkelData ? model->SkelData->Clips.size() : 0);
        }
        else
        {
            BLU_CORE_INFO("ModelLoader: {0} - {1} submesh(es) after merge", modelName, model->Meshes.size());
        }

        // ─── Compute bounding spheres for frustum culling ────────────────
        for (auto& submesh : model->Meshes)
        {
            glm::vec3 center(0.0f);
            if (!submesh.Vertices.empty())
            {
                for (const auto& v : submesh.Vertices)
                    center += v.Position;
                center /= (float)submesh.Vertices.size();
                float maxDistSq = 0.0f;
                for (const auto& v : submesh.Vertices)
                {
                    glm::vec3 diff = v.Position - center;
                    float d2 = glm::dot(diff, diff);
                    if (d2 > maxDistSq) maxDistSq = d2;
                }
                submesh.BoundingCenter = center;
                submesh.BoundingRadius = glm::sqrt(maxDistSq);
            }
        }

        // Upload all submesh vertex data to GPU at load time, then free CPU copies.
        for (auto& submesh : model->Meshes)
        {
            UploadSubMeshGPU(submesh);
            submesh.Vertices.clear();
            submesh.Vertices.shrink_to_fit();
            submesh.Indices.clear();
            submesh.Indices.shrink_to_fit();
        }
        for (auto& sub : model->SkinnedMeshes)
        {
            UploadSkinnedSubMeshGPU(sub);
            sub.Vertices.clear();
            sub.Vertices.shrink_to_fit();
            sub.Indices.clear();
            sub.Indices.shrink_to_fit();
        }

        return model;
    }
}
