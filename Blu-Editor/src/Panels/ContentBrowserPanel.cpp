#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#ifdef SetCurrentDirectory
#undef SetCurrentDirectory
#endif
#include "ContentBrowserPanel.h"
#include "../AssetPreviewService.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <fstream>
#include "Blu/Core/MouseCodes.h"
#include "Blu/Core/Input.h"
#include "Blu/Platform/OpenGL/OpenGLTexture.h"
#include "Blu/Rendering/RendererAPI.h"
#include "Blu/Utils/PlatformUtils.h"
#include "Blu/Utils/AssetPath.h"
#include "Blu/Rendering/ModelLoader.h"
#include "Blu/Rendering/Renderer3D.h"
#include "Blu/Rendering/RenderCommand.h"
#include "Blu/Scene/Component.h"
#include <algorithm>
#include <cctype>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#undef SetCurrentDirectory
#undef min
#undef max
#endif

namespace Blu
{
    constexpr std::string_view s_AssetsDirectory = "assets";

    // Extension → { tint color, badge label (uppercase, ≤4 chars) }
    struct ExtInfo { ImVec4 Tint; const char* Badge; };
    static bool IsImageExtension(const std::string& ext)
    {
        return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
               ext == ".bmp" || ext == ".tga" || ext == ".hdr";
    }
    static ExtInfo GetExtensionInfo(const std::string& ext)
    {
        // Textures — green
        if (IsImageExtension(ext))
            return { ImVec4(0.40f, 0.90f, 0.55f, 1.f), "IMG" };
        // Meshes — orange
        if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" ||
            ext == ".glb" || ext == ".dae" || ext == ".3ds" || ext == ".ply")
            return { ImVec4(1.00f, 0.60f, 0.20f, 1.f), "MESH" };
        // Scenes / YAML — blue
        if (ext == ".yaml" || ext == ".yml" || ext == ".blu" || ext == ".scene" || ext == ".bluui")
            return { ImVec4(0.30f, 0.65f, 1.00f, 1.f), "SCN" };
        if (ext == ".bluprefab")
            return { ImVec4(0.25f, 0.70f, 1.00f, 1.f), "PFB" };
        // Shaders — purple
        if (ext == ".hlsl" || ext == ".glsl" || ext == ".vert" ||
            ext == ".frag" || ext == ".comp" || ext == ".vs" || ext == ".fs")
            return { ImVec4(0.75f, 0.40f, 1.00f, 1.f), "SHD" };
        // Scripts — yellow
        if (ext == ".cs" || ext == ".lua" || ext == ".py")
            return { ImVec4(1.00f, 0.85f, 0.20f, 1.f), "SRC" };
        // Audio — red
        if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
            return { ImVec4(1.00f, 0.35f, 0.35f, 1.f), "AUD" };
        return { ImVec4(0.78f, 0.78f, 0.78f, 1.f), "" };
    }

    static bool IsModelExtension(const std::string& ext)
    {
        return ext == ".fbx" || ext == ".obj" || ext == ".gltf" ||
               ext == ".glb" || ext == ".dae" || ext == ".3ds" ||
               ext == ".blend" || ext == ".ply" || ext == ".bluprefab";
    }

    static std::string ToProjectRelative(const std::filesystem::path& path)
    {
        try
        {
            return std::filesystem::relative(path, std::filesystem::current_path()).generic_string();
        }
        catch (...)
        {
            return path.generic_string();
        }
    }

    static void RevealInExplorer(const std::filesystem::path& path)
    {
#ifdef _WIN32
        std::wstring args = L"/select,\"" + std::filesystem::absolute(path).wstring() + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
#else
        (void)path;
#endif
    }

    static void OpenInDefaultEditor(const std::filesystem::path& path)
    {
#ifdef _WIN32
        ShellExecuteW(nullptr, L"open", std::filesystem::absolute(path).wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
#else
        (void)path;
#endif
    }

    static void DrawExtensionBadge(ImDrawList* dl, ImVec2 iconMin, ImVec2 iconMax, const ExtInfo& info)
    {
        if (!info.Badge || info.Badge[0] == '\0') return;

        const ImVec2 textSz   = ImGui::CalcTextSize(info.Badge);
        const float  padX     = 4.f;
        const float  padY     = 2.f;
        const float  badgeH   = textSz.y + padY * 2.f;
        const float  badgeW   = textSz.x + padX * 2.f;
        ImVec2 badgeMin = { iconMax.x - badgeW - 2.f, iconMax.y - badgeH - 2.f };
        ImVec2 badgeMax = { iconMax.x - 2.f,          iconMax.y - 2.f };

        // Badge bg (slightly transparent tint)
        ImU32 bgCol = IM_COL32(
            (int)(info.Tint.x * 180),
            (int)(info.Tint.y * 180),
            (int)(info.Tint.z * 180), 220);
        dl->AddRectFilled(badgeMin, badgeMax, bgCol, 2.f);
        ImVec2 textPos = { badgeMin.x + padX, badgeMin.y + padY };
        dl->AddText(textPos, IM_COL32(255, 255, 255, 255), info.Badge);
    }

    static bool IsAudioExtension(const std::string& ext)
    {
        return ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac";
    }
    static bool IsShaderExtension(const std::string& ext)
    {
        return ext == ".hlsl" || ext == ".glsl" || ext == ".vert" ||
               ext == ".frag" || ext == ".comp" || ext == ".vs" || ext == ".fs";
    }
    static bool IsFontExtension(const std::string& ext)
    {
        return ext == ".ttf" || ext == ".otf" || ext == ".ttc";
    }

    // ── Procedural thumbnails (ImDrawList vector art — no PNG assets needed) ──────
    // Audio: a red waveform on a dark plate, the way Unreal/Unity show sound assets.
    static void DrawSoundThumbnail(ImDrawList* dl, ImVec2 mn, ImVec2 mx)
    {
        dl->AddRectFilled(mn, mx, IM_COL32(30, 24, 26, 255), 6.f);
        const float w = mx.x - mn.x, h = mx.y - mn.y;
        const float cy = (mn.y + mx.y) * 0.5f;
        const ImU32 col = IM_COL32(255, 95, 95, 255);
        static const float amp[] = { 0.15f,0.42f,0.7f,0.55f,0.88f,0.5f,0.96f,0.62f,
                                     0.78f,0.34f,0.6f,0.26f,0.5f,0.32f,0.18f };
        const int n = (int)(sizeof(amp) / sizeof(amp[0]));
        const float pad = w * 0.12f;
        const float usableW = w - pad * 2.f;
        const float step = usableW / n;
        const float barW = step * 0.55f;
        for (int i = 0; i < n; ++i)
        {
            const float x = mn.x + pad + step * (i + 0.5f);
            const float a = amp[i] * (h * 0.40f);
            dl->AddRectFilled(ImVec2(x - barW * 0.5f, cy - a),
                              ImVec2(x + barW * 0.5f, cy + a), col, barW * 0.5f);
        }
    }
    // Shader: a purple "material ball" with a specular highlight.
    static void DrawShaderThumbnail(ImDrawList* dl, ImVec2 mn, ImVec2 mx)
    {
        dl->AddRectFilled(mn, mx, IM_COL32(24, 20, 32, 255), 6.f);
        const ImVec2 c = { (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f };
        const float side = (mx.x - mn.x) < (mx.y - mn.y) ? (mx.x - mn.x) : (mx.y - mn.y);
        const float r = side * 0.30f;
        dl->AddCircleFilled({ c.x, c.y + r * 0.18f }, r, IM_COL32(95, 55, 150, 255), 40);     // shaded base
        dl->AddCircleFilled({ c.x, c.y - r * 0.05f }, r * 0.92f, IM_COL32(155, 95, 225, 255), 40); // lit body
        dl->AddCircleFilled({ c.x - r * 0.34f, c.y - r * 0.36f }, r * 0.24f, IM_COL32(240, 225, 255, 235), 24); // highlight
    }
    // Font: a large "Aa" specimen on a neutral plate.
    static void DrawFontThumbnail(ImDrawList* dl, ImVec2 mn, ImVec2 mx)
    {
        dl->AddRectFilled(mn, mx, IM_COL32(32, 32, 36, 255), 6.f);
        const char* txt = "Aa";
        ImFont* font = ImGui::GetFont();
        const float fontSz = (mx.y - mn.y) * 0.5f;
        const ImVec2 ts = font->CalcTextSizeA(fontSz, FLT_MAX, 0.f, txt);
        const ImVec2 pos = { (mn.x + mx.x) * 0.5f - ts.x * 0.5f, (mn.y + mx.y) * 0.5f - ts.y * 0.5f };
        dl->AddText(font, fontSz, pos, IM_COL32(232, 232, 238, 255), txt);
    }

    static bool IsSceneExtension(const std::string& ext)
    {
        return ext == ".blu" || ext == ".scene" || ext == ".bluui";
    }
    static bool IsPrefabExtension(const std::string& ext)
    {
        return ext == ".bluprefab";
    }

    // Generic document: a page with a folded corner and a few text lines tinted by the
    // file's category accent. Used for text/yaml/script/config and any unknown type, so
    // nothing falls back to the flat "page" PNG.
    static void DrawDocThumbnail(ImDrawList* dl, ImVec2 mn, ImVec2 mx, ImU32 accent)
    {
        const float w = mx.x - mn.x, h = mx.y - mn.y;
        const float pad = w * 0.24f;
        const ImVec2 a = { mn.x + pad, mn.y + h * 0.12f };
        const ImVec2 b = { mx.x - pad, mx.y - h * 0.12f };
        const float fold = (b.x - a.x) * 0.34f;
        dl->AddRectFilled(a, ImVec2(b.x, b.y), IM_COL32(236, 237, 242, 255), 3.f);
        dl->AddTriangleFilled(ImVec2(b.x - fold, a.y), ImVec2(b.x, a.y + fold),
                              ImVec2(b.x, a.y), IM_COL32(198, 201, 210, 255)); // folded corner
        const float lx0 = a.x + (b.x - a.x) * 0.16f, lx1 = b.x - (b.x - a.x) * 0.16f;
        const float ly = a.y + h * 0.34f, dy = h * 0.13f;
        for (int i = 0; i < 4; ++i)
        {
            const float yy = ly + dy * i;
            const float ex = (i == 3) ? lx0 + (lx1 - lx0) * 0.55f : lx1;
            dl->AddLine(ImVec2(lx0, yy), ImVec2(ex, yy), accent, 2.0f);
        }
    }
    // Scene/level: sky + ground + a little object — reads as a "level".
    static void DrawSceneThumbnail(ImDrawList* dl, ImVec2 mn, ImVec2 mx)
    {
        dl->AddRectFilled(mn, mx, IM_COL32(28, 34, 48, 255), 6.f);
        const float w = mx.x - mn.x, h = mx.y - mn.y;
        const float gy = mn.y + h * 0.64f;
        dl->AddRectFilled(ImVec2(mn.x, gy), ImVec2(mx.x, mx.y), IM_COL32(52, 80, 112, 255), 0.f);
        dl->AddCircleFilled(ImVec2(mn.x + w * 0.74f, mn.y + h * 0.30f), w * 0.10f, IM_COL32(125, 175, 235, 255), 20);
        const float cs = w * 0.18f, cx = mn.x + w * 0.34f;
        dl->AddRectFilled(ImVec2(cx - cs * 0.5f, gy - cs), ImVec2(cx + cs * 0.5f, gy), IM_COL32(95, 135, 185, 255), 2.f);
    }
    // Prefab: a cyan "blueprint" wireframe cube.
    static void DrawPrefabThumbnail(ImDrawList* dl, ImVec2 mn, ImVec2 mx)
    {
        dl->AddRectFilled(mn, mx, IM_COL32(20, 34, 40, 255), 6.f);
        const ImVec2 c = { (mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f };
        const float s = ((mx.x - mn.x) < (mx.y - mn.y) ? (mx.x - mn.x) : (mx.y - mn.y)) * 0.28f;
        const ImU32 col = IM_COL32(85, 205, 235, 255);
        const ImVec2 top = { c.x, c.y - s }, right = { c.x + s, c.y - s * 0.4f },
                     bot = { c.x, c.y + s * 0.2f }, left = { c.x - s, c.y - s * 0.4f };
        dl->AddQuad(top, right, bot, left, col, 2.0f);                       // top face
        dl->AddLine(left, ImVec2(left.x, left.y + s), col, 2.0f);
        dl->AddLine(right, ImVec2(right.x, right.y + s), col, 2.0f);
        dl->AddLine(bot, ImVec2(bot.x, bot.y + s), col, 2.0f);
        dl->AddLine(ImVec2(left.x, left.y + s), ImVec2(bot.x, bot.y + s), col, 2.0f);
        dl->AddLine(ImVec2(right.x, right.y + s), ImVec2(bot.x, bot.y + s), col, 2.0f);
    }
    // Polished manila folder (tab + back panel + lighter front flap).
    static void DrawFolderThumbnail(ImDrawList* dl, ImVec2 mn, ImVec2 mx)
    {
        const float w = mx.x - mn.x, h = mx.y - mn.y;
        const float pad = w * 0.12f;
        const ImVec2 a = { mn.x + pad, mn.y + h * 0.32f };
        const ImVec2 b = { mx.x - pad, mx.y - h * 0.16f };
        const float tabW = (b.x - a.x) * 0.44f;
        dl->AddRectFilled(ImVec2(a.x, a.y - h * 0.12f), ImVec2(a.x + tabW, a.y + h * 0.05f),
                          IM_COL32(206, 162, 78, 255), 3.f);       // tab
        dl->AddRectFilled(a, b, IM_COL32(222, 178, 92, 255), 4.f); // back panel
        dl->AddRectFilled(ImVec2(a.x + w * 0.03f, a.y + h * 0.12f), b,
                          IM_COL32(246, 206, 122, 255), 4.f);      // front flap (lighter)
    }

    static bool IsScriptExtension(const std::string& ext)
    {
        return ext == ".cs" || ext == ".lua" || ext == ".py";
    }

    // Type-filter dropdown order: 0=All, then the categories below. Folders always pass
    // (kept visible for navigation); only files are filtered.
    static const char* const s_TypeFilterItems[] = {
        "All Types", "Images", "Models", "Audio", "Shaders", "Scripts", "Scenes", "Prefabs", "Fonts"
    };
    static bool MatchesTypeFilter(int filter, const std::string& ext, bool isDir)
    {
        if (filter == 0 || isDir) return true;
        switch (filter)
        {
            case 1: return IsImageExtension(ext);
            case 2: return IsModelExtension(ext);
            case 3: return IsAudioExtension(ext);
            case 4: return IsShaderExtension(ext);
            case 5: return IsScriptExtension(ext);
            case 6: return IsSceneExtension(ext);
            case 7: return IsPrefabExtension(ext);
            case 8: return IsFontExtension(ext);
        }
        return true;
    }

    ContentBrowserPanel::ContentBrowserPanel()
        : m_CurrentDirectory(s_AssetsDirectory)
    {
        m_FileIcons[".png"] = Blu::Texture2D::Create("assets/textures/FileIcon.png");
        m_FolderOpenIcon    = Blu::Texture2D::Create("assets/textures/FolderOpen.png");
        m_FolderClosedIcon  = std::filesystem::exists("assets/textures/Folder.png")
            ? Blu::Texture2D::Create("assets/textures/Folder.png")
            : m_FolderOpenIcon;
        m_NavigationHistory = GetDirectoryPath(m_CurrentDirectory);
        LoadFavorites();
    }

    void ContentBrowserPanel::LoadFavorites()
    {
        m_FavoritePaths.clear();
        std::ifstream f("config/cb_favorites.txt");
        if (!f.is_open()) return;
        std::string line;
        while (std::getline(f, line))
            if (!line.empty() && std::filesystem::exists(line))
                m_FavoritePaths.emplace_back(line);
    }
    void ContentBrowserPanel::SaveFavorites() const
    {
        std::error_code ec;
        std::filesystem::create_directories("config", ec);
        std::ofstream f("config/cb_favorites.txt", std::ios::trunc);
        if (!f.is_open()) return;
        for (const auto& p : m_FavoritePaths)
            f << p.generic_string() << "\n";
    }
    bool ContentBrowserPanel::IsFavorite(const std::filesystem::path& p) const
    {
        for (const auto& f : m_FavoritePaths)
            if (f == p) return true;
        return false;
    }
    void ContentBrowserPanel::ToggleFavorite(const std::filesystem::path& p)
    {
        for (auto it = m_FavoritePaths.begin(); it != m_FavoritePaths.end(); ++it)
            if (*it == p) { m_FavoritePaths.erase(it); SaveFavorites(); return; }
        m_FavoritePaths.push_back(p);
        SaveFavorites();
    }

    void ContentBrowserPanel::SetBrowserDirectory(const std::filesystem::path& directory)
    {
        if (directory.empty() || !std::filesystem::exists(directory) || !std::filesystem::is_directory(directory))
            return;

        std::filesystem::path normalized = directory.lexically_normal();
        std::string generic = normalized.generic_string();
        if (generic.find(s_AssetsDirectory.data()) != 0)
            return;

        m_CurrentDirectory = normalized;
        m_NavigationHistory = GetDirectoryPath(m_CurrentDirectory);
    }

    Shared<Texture2D> ContentBrowserPanel::GetImageThumbnail(const std::filesystem::path& path)
    {
        if (path.empty() || !std::filesystem::exists(path))
            return nullptr;

        const std::string key = path.lexically_normal().generic_string();
        const auto lastWriteTime = std::filesystem::last_write_time(path);

        auto it = m_ImageThumbnailCache.find(key);
        if (it != m_ImageThumbnailCache.end() && it->second.LastWriteTime == lastWriteTime)
            return it->second.Texture;

        Shared<Texture2D> texture = Texture2D::Create(path.string());
        if (!texture)
            return nullptr;

        m_ImageThumbnailCache[key] = { texture, lastWriteTime };
        return texture;
    }

    static void ComputeModelPreviewBounds(const Shared<Model>& model, glm::vec3& center, float& radius)
    {
        center = glm::vec3(0.0f);
        radius = 1.0f;
        if (!model)
            return;

        uint32_t count = 0;
        for (const auto& submesh : model->Meshes)
        {
            glm::vec3 subCenter = glm::vec3(submesh.LocalTransform * glm::vec4(submesh.BoundingCenter, 1.0f));
            center += subCenter;
            count++;
        }
        if (count > 0)
            center /= (float)count;

        float maxRadius = 0.5f;
        for (const auto& submesh : model->Meshes)
        {
            glm::vec3 subCenter = glm::vec3(submesh.LocalTransform * glm::vec4(submesh.BoundingCenter, 1.0f));
            float scale = std::max({
                glm::length(glm::vec3(submesh.LocalTransform[0])),
                glm::length(glm::vec3(submesh.LocalTransform[1])),
                glm::length(glm::vec3(submesh.LocalTransform[2])),
                0.001f });
            maxRadius = std::max(maxRadius, glm::length(subCenter - center) + submesh.BoundingRadius * scale);
        }
        radius = std::max(maxRadius, 0.5f);
    }

    bool ContentBrowserPanel::RenderModelThumbnail(const std::filesystem::path& path, ModelThumbnailCacheEntry& entry)
    {
        Shared<Model> model = ModelLoader::Load(path.string());
        if (!model || (model->Meshes.empty() && model->SkinnedMeshes.empty()))
            return false;

        if (!entry.Framebuffer)
        {
            FrameBufferSpecifications spec;
            spec.Width = 192;
            spec.Height = 192;
            spec.Attachments = { FrameBufferTextureFormat::RGBA8, FrameBufferTextureFormat::Depth };
            entry.Framebuffer = FrameBuffer::Create(spec);
        }

        glm::vec3 center(0.0f);
        float radius = 1.0f;
        ComputeModelPreviewBounds(model, center, radius);

        m_ModelThumbnailCamera.SetViewportSize(192.0f, 192.0f);
        m_ModelThumbnailCamera.SetDistance(radius * 2.6f);
        m_ModelThumbnailCamera.SetPitchYaw(glm::radians(18.0f), glm::radians(-35.0f));
        m_ModelThumbnailCamera.SetFocalPoint(glm::vec3(0.0f, radius * 0.15f, 0.0f));

        MeshComponent mesh;
        mesh.FilePath = AssetPath::ToProjectRelative(path);
        mesh.ModelAsset = model;
        mesh.MaterialInstance = Material::Create();

        entry.Framebuffer->Bind();
        RenderCommand::SetClearColor({ 0.08f, 0.085f, 0.09f, 1.0f });
        RenderCommand::Clear();
        Renderer3D::SetLights(
            { DirLightData{ glm::normalize(glm::vec3(-0.35f, -0.8f, -0.45f)), {0.12f, 0.12f, 0.14f}, {1.0f, 0.96f, 0.88f}, {0.4f, 0.4f, 0.4f}, 1.5f } },
            {},
            {});
        Renderer3D::BeginScene(m_ModelThumbnailCamera);
        Renderer3D::DrawMesh(glm::translate(glm::mat4(1.0f), -center), mesh);
        Renderer3D::FlushDrawCalls();
        Renderer3D::EndScene();
        entry.Framebuffer->UnBind();
        return true;
    }

    uint64_t ContentBrowserPanel::GetModelThumbnail(const std::filesystem::path& path, bool& failed)
    {
        failed = false;
        if (path.empty() || !std::filesystem::exists(path))
        {
            failed = true;
            return 0;
        }

        const std::string key = path.lexically_normal().generic_string();
        const auto lastWriteTime = std::filesystem::last_write_time(path);
        auto& entry = m_ModelThumbnailCache[key];
        if (entry.Framebuffer && entry.LastWriteTime == lastWriteTime && !entry.Failed)
            return entry.Framebuffer->GetColorAttachmentID();
        if (entry.Failed && entry.LastWriteTime == lastWriteTime)
        {
            failed = true;
            return 0;
        }

        if (m_ModelThumbnailsRenderedThisFrame >= 1)
            return entry.Framebuffer ? entry.Framebuffer->GetColorAttachmentID() : 0;

        entry.LastWriteTime = lastWriteTime;
        entry.Failed = !RenderModelThumbnail(path, entry);
        m_ModelThumbnailsRenderedThisFrame++;
        failed = entry.Failed;
        return entry.Framebuffer && !entry.Failed ? entry.Framebuffer->GetColorAttachmentID() : 0;
    }

    static std::filesystem::path s_RightClickedItemPath;

    void ContentBrowserPanel::OnImGuiRender()
    {
        static std::filesystem::path s_RenamingPath;
        static std::filesystem::path s_PendingDeletePath; // deferred until the confirm modal
        static char s_Filter[128] = "";
        m_ModelThumbnailsRenderedThisFrame = 0;
        AssetPreviewService::Get().ResetFrameBudget();

        const bool isDX11 = RendererAPI::GetAPI() == RendererAPI::API::Direct3D;
        ImVec2 uv0 = isDX11 ? ImVec2(0, 0) : ImVec2(0, 1);
        ImVec2 uv1 = isDX11 ? ImVec2(1, 1) : ImVec2(1, 0);

        ImGui::Begin("Content Browser");

        // ── Left pane: directory tree ────────────────────────────────────────────
        ImGui::BeginChild("##cbLeft", ImVec2(ImGui::GetWindowWidth() * 0.22f, 0), true);

        ImGui::TextDisabled("Favorites");
        if (ImGui::Selectable("assets", m_CurrentDirectory == s_AssetsDirectory))
        {
            m_CurrentDirectory = s_AssetsDirectory;
            m_NavigationHistory = GetDirectoryPath(m_CurrentDirectory);
        }
        // Pinned folders (right-click a folder in the grid → Add to Favorites).
        std::filesystem::path favToRemove;
        for (size_t i = 0; i < m_FavoritePaths.size(); ++i)
        {
            const auto& fav = m_FavoritePaths[i];
            ImGui::PushID((int)i);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.82f, 0.30f, 1.0f));
            const std::string label = "* " + fav.filename().string();
            const bool sel = ImGui::Selectable(label.c_str(), m_CurrentDirectory == fav);
            ImGui::PopStyleColor();
            if (sel)
            {
                m_CurrentDirectory  = fav;
                m_NavigationHistory = GetDirectoryPath(m_CurrentDirectory);
            }
            if (ImGui::BeginPopupContextItem("##favctx"))
            {
                if (ImGui::MenuItem("Remove from Favorites")) favToRemove = fav;
                ImGui::EndPopup();
            }
            ImGui::PopID();
        }
        if (!favToRemove.empty()) ToggleFavorite(favToRemove);
        ImGui::Separator();

        if (ImGui::Button("+ New"))
            ImGui::OpenPopup("##CBNew");

        if (ImGui::BeginPopup("##CBNew"))
        {
            if (ImGui::MenuItem("New Folder"))
            {
                std::filesystem::path p = m_CurrentDirectory / "NewFolder";
                for (int i = 0; std::filesystem::exists(p); ++i)
                    p = m_CurrentDirectory / ("NewFolder" + std::to_string(i));
                std::filesystem::create_directory(p);
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("New File"))
            {
                std::filesystem::path p = m_CurrentDirectory / "NewFile.txt";
                for (int i = 0; std::filesystem::exists(p); ++i)
                    p = m_CurrentDirectory / ("NewFile" + std::to_string(i) + ".txt");
                { std::ofstream f(p); }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Separator();
        ShowDirectoryNodes(s_AssetsDirectory);
        ImGui::EndChild();

        ImGui::SameLine();

        // ── Right pane: content view ─────────────────────────────────────────────
        ImGui::BeginChild("##cbRight", ImVec2(0, 0), false);

        {
            ImGui::BeginChild("##cbAssetToolbar", ImVec2(0, 30.f), false, ImGuiWindowFlags_NoScrollbar);
            if (ImGui::Button("+ Add"))
                ImGui::OpenPopup("##CBNew");
            ImGui::SameLine();
            if (ImGui::Button("Import"))
            {
                std::string filepath = FileDialogs::OpenFile("FBX (*.fbx)\0*.fbx\0OBJ (*.obj)\0*.obj\0GLTF (*.gltf)\0*.gltf\0GLB (*.glb)\0*.glb\0All Models\0*.fbx;*.obj;*.gltf;*.glb;*.dae;*.blend;*.ply\0");
                if (!filepath.empty() && m_ImportModelCallback)
                    m_ImportModelCallback(filepath);
            }
            ImGui::SameLine();
            if (ImGui::Button("Save All") && m_SaveAllCallback)
                m_SaveAllCallback();
            ImGui::SameLine();
            if (ImGui::Button("Save Prefab") && m_SaveSelectedAsPrefabCallback)
                m_SaveSelectedAsPrefabCallback();
            ImGui::SameLine();
            ImGui::TextDisabled("%s", ToProjectRelative(m_CurrentDirectory).c_str());
            ImGui::EndChild();
        }

        // -- Breadcrumb bar -------------------------------------------------------
        {
            ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.f));
            ImGui::BeginChild("##cbBread", ImVec2(0, 22.f), false, ImGuiWindowFlags_NoScrollbar);
            bool first = true;
            for (auto it = m_NavigationHistory.begin(); it != m_NavigationHistory.end(); ++it)
            {
                if (!first) { ImGui::SameLine(0, 2); ImGui::TextDisabled("/"); ImGui::SameLine(0, 2); }
                first = false;
                std::string name = it->filename().string();
                if (name.empty()) name = it->string(); // root
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0,0,0,0));
                if (ImGui::SmallButton(name.c_str()))
                {
                    m_CurrentDirectory = *it;
                    m_NavigationHistory.erase(std::next(it), m_NavigationHistory.end());
                    break;
                }
                ImGui::PopStyleColor();
            }
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }

        // -- Toolbar: back button + search + thumbnail size -----------------------
        {
            ImGui::BeginChild("##cbToolbar", ImVec2(0, 30.f), false, ImGuiWindowFlags_NoScrollbar);

            if (m_CurrentDirectory != s_AssetsDirectory)
            {
                if (ImGui::ArrowButton("##cbBack", ImGuiDir_Left))
                {
                    std::filesystem::path parent = m_CurrentDirectory.parent_path();
                    if (parent.string().find(s_AssetsDirectory.data()) != std::string::npos)
                    {
                        m_CurrentDirectory = parent;
                        m_NavigationHistory = GetDirectoryPath(m_CurrentDirectory);
                    }
                }
                ImGui::SameLine(0, 4);
            }

            // Search box
            ImGui::PushItemWidth(180.f);
            ImGui::InputTextWithHint("##cbSearch", "Search...", s_Filter, IM_ARRAYSIZE(s_Filter));
            ImGui::PopItemWidth();
            ImGui::SameLine();
            if (ImGui::Button("Sort"))
                ImGui::OpenPopup("##cbSort");
            if (ImGui::BeginPopup("##cbSort"))
            {
                if (ImGui::MenuItem("Name", nullptr, m_SortMode == 0)) m_SortMode = 0;
                if (ImGui::MenuItem("Type", nullptr, m_SortMode == 1)) m_SortMode = 1;
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.f);
            ImGui::Combo("##cbType", &m_TypeFilter, s_TypeFilterItems, IM_ARRAYSIZE(s_TypeFilterItems));
            ImGui::SameLine();

            // Thumbnail size slider (right-aligned)
            float sliderW = 90.f;
            float rightX  = ImGui::GetWindowWidth() - sliderW - 4.f;
            ImGui::SameLine(rightX);
            ImGui::PushItemWidth(sliderW);
            ImGui::SliderFloat("##cbThumb", &m_ThumbnailSize, 40.f, 160.f, "%.0f px");
            ImGui::PopItemWidth();

            ImGui::EndChild();
        }

        ImGui::Separator();

        // -- File grid ------------------------------------------------------------
        const float iconSzF   = m_ThumbnailSize;
        const float padding   = 8.f;
        const float cellSize  = iconSzF + padding;
        const ImVec2 iconSize = { iconSzF, iconSzF };

        float panelWidth   = ImGui::GetContentRegionAvail().x;
        int   columnCount  = std::max(1, (int)(panelWidth / (cellSize + 4.f)));

        m_ObjectClicked = false;
        ImGui::Columns(columnCount, nullptr, false);

        std::vector<std::filesystem::directory_entry> entries;
        if (std::filesystem::exists(m_CurrentDirectory))
        {
            if (m_TypeFilter != 0)
            {
                // A type filter is active: gather ALL matching files under this folder
                // recursively (Unreal-style), so assets in sub-folders are still shown.
                std::error_code ec;
                for (std::filesystem::recursive_directory_iterator it(m_CurrentDirectory, ec), end;
                     it != end; it.increment(ec))
                {
                    if (ec) break;
                    if (it->is_directory()) continue;
                    std::string e = it->path().extension().string();
                    for (char& ch : e) ch = (char)std::tolower((unsigned char)ch);
                    if (MatchesTypeFilter(m_TypeFilter, e, false))
                        entries.push_back(*it);
                }
            }
            else
            {
                for (auto& entry : std::filesystem::directory_iterator(m_CurrentDirectory))
                    entries.push_back(entry);
            }
        }
        SortEntries(entries, m_SortMode);

        uint32_t displayedItems = 0;
        for (auto& entry : entries)
        {
            const auto& path         = entry.path();
            std::string filenameStr  = path.filename().string();
            std::string extStr       = path.extension().string();

            // Lowercase extension for matching
            std::string extLower = extStr;
            for (char& c : extLower) c = (char)std::tolower((unsigned char)c);

            // Search filter
            if (s_Filter[0] != '\0')
            {
                std::string nameLower = filenameStr;
                for (char& c : nameLower) c = (char)std::tolower((unsigned char)c);
                if (nameLower.find(s_Filter) == std::string::npos) { ImGui::NextColumn(); continue; }
            }


            bool isSelected = (m_SelectedFilename == filenameStr);
            ImGui::PushID(path.string().c_str());
            ImGui::BeginGroup();

            // Icon
            void* iconID;
            ImVec4 tint = { 1.f, 1.f, 1.f, 1.f };
            ExtInfo extInfo = { {1,1,1,1}, "" };
            bool drawBadge = false;
            int  procIcon  = 0; // 0=none, 1=sound, 2=shader, 3=font (drawn with ImDrawList)

            iconID = reinterpret_cast<void*>(static_cast<intptr_t>(m_FileIcons[".png"]->GetImTextureID()));
            if (entry.is_directory())
            {
                procIcon = 7; // polished procedural folder (replaces the folder PNG)
            }
            else
            {
                extInfo = GetExtensionInfo(extLower);
                tint    = extInfo.Tint;

                if (IsImageExtension(extLower))
                {
                    drawBadge = true;
                    if (auto thumbnail = AssetPreviewService::Get().GetImageThumbnail(path))
                    {
                        iconID = reinterpret_cast<void*>(static_cast<intptr_t>(thumbnail->GetImTextureID()));
                        tint = { 1.f, 1.f, 1.f, 1.f };
                        drawBadge = false;
                    }
                }
                else if (IsModelExtension(extLower))
                {
                    drawBadge = true;
                    bool thumbnailFailed = false;
                    uint64_t thumbnailID = AssetPreviewService::Get().GetAssetThumbnail(path, thumbnailFailed);
                    if (thumbnailID != 0)
                    {
                        iconID = reinterpret_cast<void*>(thumbnailID);
                        tint = { 1.f, 1.f, 1.f, 1.f };
                        drawBadge = false;
                    }
                    else if (thumbnailFailed)
                    {
                        extInfo = { ImVec4(1.00f, 0.45f, 0.25f, 1.f), "MISS" };
                        tint = extInfo.Tint;
                    }
                }
                else if (IsAudioExtension(extLower))  { procIcon = 1; }
                else if (IsShaderExtension(extLower)) { procIcon = 2; }
                else if (IsFontExtension(extLower))   { procIcon = 3; }
                else if (IsSceneExtension(extLower))  { procIcon = 5; }
                else if (IsPrefabExtension(extLower)) { procIcon = 6; }
                else { procIcon = 4; drawBadge = true; } // generic document for any other type
            }

            // Reserve the cell with a Dummy for procedural icons (so the hit-rect is set),
            // otherwise draw the icon/preview texture.
            if (procIcon != 0)
                ImGui::Dummy(iconSize);
            else
                ImGui::Image(iconID, iconSize, uv0, uv1, tint);
            ImVec2 iconMin = ImGui::GetItemRectMin();
            ImVec2 iconMax = ImGui::GetItemRectMax();
            ImDrawList* cellDL = ImGui::GetWindowDrawList();
            if      (procIcon == 1) DrawSoundThumbnail(cellDL, iconMin, iconMax);
            else if (procIcon == 2) DrawShaderThumbnail(cellDL, iconMin, iconMax);
            else if (procIcon == 3) DrawFontThumbnail(cellDL, iconMin, iconMax);
            else if (procIcon == 4) DrawDocThumbnail(cellDL, iconMin, iconMax, ImGui::ColorConvertFloat4ToU32(extInfo.Tint));
            else if (procIcon == 5) DrawSceneThumbnail(cellDL, iconMin, iconMax);
            else if (procIcon == 6) DrawPrefabThumbnail(cellDL, iconMin, iconMax);
            else if (procIcon == 7) DrawFolderThumbnail(cellDL, iconMin, iconMax);

            // Badge overlay for files
            if (drawBadge)
                DrawExtensionBadge(cellDL, iconMin, iconMax, extInfo);

            // Selection highlight / invisible button overlay
            ImGui::SetCursorPos({ ImGui::GetCursorPos().x, ImGui::GetCursorPos().y - iconSize.y });
            if (ImGui::InvisibleButton("##icon", iconSize))
                m_SelectedFilename = filenameStr;

            if (isSelected)
            {
                ImDrawList* dl = ImGui::GetWindowDrawList();
                ImVec2 mn = ImGui::GetItemRectMin(), mx = ImGui::GetItemRectMax();
                dl->AddRect(mn, mx, IM_COL32(30, 151, 201, 255), 2.f, 0, 2.f);
            }

            // Interactions
            if (ImGui::IsItemHovered())
            {
                if (entry.is_directory() && ImGui::IsMouseDoubleClicked(0))
                {
                    m_CurrentDirectory    = path;
                    m_NavigationHistory   = GetDirectoryPath(m_CurrentDirectory);
                    m_SelectedFilename    = "";
                }
                else if (!entry.is_directory() && ImGui::IsMouseDoubleClicked(0))
                {
                    // Double-click a sound file → open the Sound Preview window.
                    if (IsAudioExtension(extLower) && m_OpenSoundEditorCallback)
                        m_OpenSoundEditorCallback(path);
                }
                if (ImGui::IsMouseClicked(1))
                {
                    s_RightClickedItemPath = path;
                    m_ObjectClicked        = true;
                }
            }
            else if (ImGui::IsMouseClicked(0) && !ImGui::IsAnyItemHovered())
            {
                m_SelectedFilename = "";
            }

            // Drag source
            if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                std::string payloadStr = path.string();
                ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", payloadStr.c_str(), payloadStr.size() + 1);
                ImGui::Image(iconID, { 40.f, 40.f }, uv0, uv1, tint);
                ImGui::SameLine();
                ImGui::TextUnformatted(filenameStr.c_str());
                ImGui::EndDragDropSource();
            }

            // Drop target (folders only: move file into folder)
            if (entry.is_directory() && ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
                {
                    std::filesystem::path src = std::string(reinterpret_cast<const char*>(p->Data));
                    if (src != path)
                    {
                        try { std::filesystem::rename(src, path / src.filename()); }
                        catch (...) {}
                    }
                }
                ImGui::EndDragDropTarget();
            }

            // Label — clamp to column width
            float colW = ImGui::GetColumnWidth() - 4.f;
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + colW);
            ImGui::TextUnformatted(filenameStr.c_str());
            ImGui::PopTextWrapPos();

            if (!entry.is_directory())
            {
                if (path.generic_string().find("assets/imports/") != std::string::npos)
                    ImGui::TextColored(ImVec4(0.40f, 0.80f, 1.0f, 1.0f), "Imported");
                else if (IsModelExtension(extLower))
                    ImGui::TextDisabled("Model");
                else if (IsImageExtension(extLower))
                    ImGui::TextDisabled("Image");
            }

            ImGui::EndGroup();

            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", path.string().c_str());

            ImGui::NextColumn();
            ImGui::PopID();
            displayedItems++;
        }

        ImGui::Columns(1);
        ImGui::Separator();
        ImGui::TextDisabled("%u item%s", displayedItems, displayedItems == 1 ? "" : "s");

        // ── Right-click context menu ─────────────────────────────────────────────
        if (ImGui::IsMouseClicked(1) && !m_ObjectClicked)
            s_RightClickedItemPath.clear();

        if (!s_RightClickedItemPath.empty() && ImGui::BeginPopupContextWindow("##cbCtxItem"))
        {
            if (ImGui::MenuItem("Reveal in Explorer"))
                RevealInExplorer(s_RightClickedItemPath);
            if (ImGui::MenuItem("Go to Folder"))
            {
                // Navigate the browser to the folder this asset lives in and select it
                // (useful from the recursive type-filter view). Clearing the filter shows
                // the item in its folder context.
                std::filesystem::path parent = s_RightClickedItemPath.parent_path();
                if (!parent.empty() && std::filesystem::exists(parent))
                {
                    m_CurrentDirectory  = parent;
                    m_NavigationHistory = GetDirectoryPath(m_CurrentDirectory);
                    m_SelectedFilename  = s_RightClickedItemPath.filename().string();
                    m_TypeFilter        = 0;
                }
            }
            if (ImGui::MenuItem("Copy Project Relative Path"))
                ImGui::SetClipboardText(ToProjectRelative(s_RightClickedItemPath).c_str());
            ImGui::Separator();

            std::string extLower = s_RightClickedItemPath.extension().string();
            for (char& c : extLower) c = (char)std::tolower((unsigned char)c);
            if (IsModelExtension(extLower))
            {
                if (extLower == ".bluprefab")
                {
                    if (ImGui::MenuItem("Instantiate Prefab"))
                    {
                        if (m_InstantiatePrefabCallback)
                            m_InstantiatePrefabCallback(s_RightClickedItemPath);
                    }
                    if (ImGui::MenuItem("Open Prefab Source"))
                        OpenInDefaultEditor(s_RightClickedItemPath);
                    if (ImGui::MenuItem("Refresh Prefab Thumbnail"))
                        AssetPreviewService::Get().Invalidate(s_RightClickedItemPath);
                    ImGui::Separator();
                }
                if (ImGui::MenuItem("Reimport / Reload"))
                {
                    if (m_ImportModelCallback)
                        m_ImportModelCallback(s_RightClickedItemPath);
                }
                if (ImGui::MenuItem("Generate Static Collision on Selected Actor"))
                {
                    if (m_GenerateStaticCollisionCallback)
                        m_GenerateStaticCollisionCallback(s_RightClickedItemPath);
                }
                ImGui::Separator();
            }

            if (std::filesystem::is_directory(s_RightClickedItemPath))
            {
                const bool fav = IsFavorite(s_RightClickedItemPath);
                if (ImGui::MenuItem(fav ? "Remove from Favorites" : "Add to Favorites"))
                {
                    ToggleFavorite(s_RightClickedItemPath);
                    ImGui::CloseCurrentPopup();
                }
                ImGui::Separator();
            }

            if (ImGui::MenuItem("Rename"))
            {
                s_RenamingPath = s_RightClickedItemPath;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::MenuItem("Delete"))
            {
                // Defer to a confirmation modal — remove_all on a folder is irreversible.
                s_PendingDeletePath = s_RightClickedItemPath;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        else if (s_RightClickedItemPath.empty() && ImGui::BeginPopupContextWindow("##cbCtxEmpty"))
        {
            if (ImGui::MenuItem("New Folder"))
            {
                std::filesystem::path p = m_CurrentDirectory / "NewFolder";
                for (int i = 0; std::filesystem::exists(p); ++i)
                    p = m_CurrentDirectory / ("NewFolder" + std::to_string(i));
                std::filesystem::create_directory(p);
            }
            if (ImGui::MenuItem("New File"))
            {
                std::filesystem::path p = m_CurrentDirectory / "NewFile.txt";
                for (int i = 0; std::filesystem::exists(p); ++i)
                    p = m_CurrentDirectory / ("NewFile" + std::to_string(i) + ".txt");
                { std::ofstream f(p); }
            }
            ImGui::EndPopup();
        }

        // ── Rename modal ─────────────────────────────────────────────────────────
        if (!s_RenamingPath.empty())
            ImGui::OpenPopup("##cbRename");

        static char s_RenameBuffer[128] = "";
        if (ImGui::BeginPopupModal("##cbRename", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Rename: %s", s_RenamingPath.filename().string().c_str());
            if (ImGui::IsWindowAppearing())
            {
                auto fn = s_RenamingPath.filename().string();
                std::copy(fn.begin(), fn.end(), s_RenameBuffer);
                s_RenameBuffer[fn.size()] = '\0';
            }
            ImGui::SetNextItemWidth(260.f);
            ImGui::InputText("##cbRenInput", s_RenameBuffer, IM_ARRAYSIZE(s_RenameBuffer));
            ImGui::Spacing();
            if (ImGui::Button("Rename", { 80.f, 0.f }))
            {
                std::filesystem::path newPath = s_RenamingPath.parent_path() / s_RenameBuffer;
                if (!std::filesystem::exists(newPath))
                    std::filesystem::rename(s_RenamingPath, newPath);
                s_RenamingPath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", { 80.f, 0.f }))
            {
                s_RenamingPath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // ---- Delete confirmation (mirrors the rename modal) ----
        if (!s_PendingDeletePath.empty())
            ImGui::OpenPopup("##cbDelete");
        if (ImGui::BeginPopupModal("##cbDelete", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            const bool isDir = std::filesystem::is_directory(s_PendingDeletePath);
            ImGui::Text("Delete this %s?", isDir ? "folder" : "file");
            ImGui::TextWrapped("%s", s_PendingDeletePath.filename().string().c_str());
            if (isDir)
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f),
                                   "This deletes the folder and ALL its contents.");
            ImGui::Spacing();
            if (ImGui::Button("Delete", { 80.f, 0.f }))
            {
                std::error_code ec;
                if (std::filesystem::exists(s_PendingDeletePath))
                    std::filesystem::remove_all(s_PendingDeletePath, ec);
                s_PendingDeletePath.clear();
                s_RightClickedItemPath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", { 80.f, 0.f }))
            {
                s_PendingDeletePath.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::EndChild(); // cbRight
        ImGui::End();
    }

    void ContentBrowserPanel::SortEntries(std::vector<std::filesystem::directory_entry>& entries, int sort_option)
    {
        if (sort_option == 0)
        {
            std::sort(entries.begin(), entries.end(),
                [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b)
                {
                    if (a.is_directory() != b.is_directory())
                        return a.is_directory() > b.is_directory();
                    return a.path().filename().string() < b.path().filename().string();
                });
        }
        else
        {
            std::sort(entries.begin(), entries.end(),
                [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b)
                {
                    if (a.is_directory() != b.is_directory())
                        return a.is_directory() > b.is_directory();
                    if (a.path().extension() != b.path().extension())
                        return a.path().extension().string() < b.path().extension().string();
                    return a.path().filename().string() < b.path().filename().string();
                });
        }
    }

    void ContentBrowserPanel::ShowDirectoryNodes(const std::filesystem::path& directoryPath)
    {
        std::string dirName = directoryPath.filename().string();

        auto it      = m_DirectoryExpandedState.find(directoryPath.string());
        bool expanded = (it != m_DirectoryExpandedState.end()) ? it->second : false;

        // Procedural folder icon (matches the grid; replaces the flat folder PNG).
        ImGui::Dummy(ImVec2(14.f, 14.f));
        DrawFolderThumbnail(ImGui::GetWindowDrawList(), ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
        ImGui::SameLine(0, 4);

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick;
        if (expanded) flags |= ImGuiTreeNodeFlags_DefaultOpen;

        bool nodeOpen = ImGui::TreeNodeEx(dirName.c_str(), flags);
        if (ImGui::IsItemClicked())
        {
            m_DirectoryExpandedState[directoryPath.string()] = !expanded;
            if (!expanded)
            {
                m_CurrentDirectory  = directoryPath;
                m_NavigationHistory = GetDirectoryPath(m_CurrentDirectory);
            }
        }

        if (nodeOpen)
        {
            for (auto& p : std::filesystem::directory_iterator(directoryPath))
            {
                if (p.is_directory())
                    ShowDirectoryNodes(p.path());
            }
            ImGui::TreePop();
        }
    }

    std::deque<std::filesystem::path> ContentBrowserPanel::GetDirectoryPath(const std::filesystem::path& directory)
    {
        std::deque<std::filesystem::path> result;
        for (std::filesystem::path p = directory; p != s_AssetsDirectory; p = p.parent_path())
            result.push_front(p);
        result.push_front(s_AssetsDirectory);
        return result;
    }

    void ContentBrowserPanel::CreateNewFile(const std::filesystem::path& directory, const std::string& baseName)
    {
        std::filesystem::path p = directory / baseName;
        for (int i = 0; std::filesystem::exists(p); ++i)
            p = directory / (baseName + std::to_string(i) + ".txt");
        { std::ofstream f(p); }
    }

    void ContentBrowserPanel::CreateNewFolder(const std::filesystem::path& directory, const std::string& baseName)
    {
        std::filesystem::path p = directory / baseName;
        for (int i = 0; std::filesystem::exists(p); ++i)
            p = directory / (baseName + std::to_string(i));
        std::filesystem::create_directory(p);
    }
}
