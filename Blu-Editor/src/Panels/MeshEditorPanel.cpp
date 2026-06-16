#include "MeshEditorPanel.h"
#include "../AssetPreviewService.h"
#include "imgui.h"
#include "Blu/Rendering/AssetManager.h"
#include "Blu/Rendering/AssetMeta.h"
#include "Blu/Rendering/ModelLoader.h"
#include "Blu/Rendering/RendererAPI.h"
#include "Blu/Utils/FileSystemService.h"
#include <string>

namespace Blu
{
	void MeshEditorPanel::Open(const std::filesystem::path& assetPath)
	{
		m_AssetPath = assetPath;
		m_Loaded = false;
		LoadFromMeta();
	}

	void MeshEditorPanel::LoadFromMeta()
	{
		m_Scale = 1.0f; m_GenerateLODs = false; m_LODCount = 3; m_ImportMaterials = true;
		m_MaterialSlots = -1;
		m_Loaded = true;
		if (m_AssetPath.empty())
			return;

		auto& fileSystem = FileSystemService::Get();
		const std::string virtualPath = fileSystem.ToVirtualPath(m_AssetPath);
		if (virtualPath.empty())
			return;

		AssetMeta meta;
		if (AssetMetaIO::Read(virtualPath, meta))
		{
			m_Scale           = meta.Mesh.Scale;
			m_GenerateLODs    = meta.Mesh.GenerateLODs;
			m_LODCount        = meta.Mesh.LODCount;
			m_ImportMaterials = meta.Mesh.ImportMaterials;
		}

		const AssetHandle handle = AssetManager::Get().FindHandleForPath(virtualPath);
		if ((uint64_t)handle != 0)
		{
			if (auto model = AssetManager::Get().LoadModel(handle))
				m_MaterialSlots = (int)model->Materials.size();
		}
	}

	void MeshEditorPanel::OnImGuiRender(bool* open)
	{
		if (open && !*open)
			return;
		if (!ImGui::Begin("Static Mesh", open))
		{
			ImGui::End();
			return;
		}
		if (m_AssetPath.empty())
		{
			ImGui::TextDisabled("No mesh selected.");
			ImGui::TextWrapped("Double-click a model in the Content Browser, or right-click it and choose \"Mesh Properties\".");
			ImGui::End();
			return;
		}
		if (!m_Loaded)
			LoadFromMeta();

		ImGui::TextUnformatted(m_AssetPath.filename().string().c_str());
		ImGui::TextDisabled("%s", m_AssetPath.generic_string().c_str());
		ImGui::Separator();

		// ── Live preview (offscreen-rendered thumbnail of the model) ────────────
		bool failed = false;
		const uint64_t thumbId = AssetPreviewService::Get().GetAssetThumbnail(m_AssetPath, failed);
		if (thumbId != 0 && !failed)
		{
			const bool isDX11 = RendererAPI::GetAPI() == RendererAPI::API::Direct3D;
			const ImVec2 uv0 = isDX11 ? ImVec2(0, 0) : ImVec2(0, 1);
			const ImVec2 uv1 = isDX11 ? ImVec2(1, 1) : ImVec2(1, 0);
			ImGui::Image(reinterpret_cast<ImTextureID>(thumbId), ImVec2(240, 240), uv0, uv1);
		}
		else
		{
			ImGui::TextDisabled("(preview unavailable)");
		}

		ImGui::Separator();
		if (m_MaterialSlots >= 0)
			ImGui::Text("Material slots: %d", m_MaterialSlots);
		else
			ImGui::TextDisabled("Material slots: (load the model to inspect)");

		ImGui::Separator();
		ImGui::TextUnformatted("Import Settings (.meta)");
		ImGui::DragFloat("Scale", &m_Scale, 0.01f, 0.0001f, 1000.0f, "%.3f");
		ImGui::Checkbox("Import materials", &m_ImportMaterials);
		ImGui::Checkbox("Generate LODs", &m_GenerateLODs);
		if (m_GenerateLODs)
		{
			ImGui::SameLine();
			ImGui::SetNextItemWidth(90.0f);
			ImGui::SliderInt("Levels", &m_LODCount, 1, 5);
		}

		ImGui::Separator();
		// Reimport writes the edited settings back to the .meta (preserving the handle/type) and
		// re-cooks the source, then refreshes the preview.
		if (ImGui::Button("Reimport", ImVec2(120, 0)))
		{
			auto& fileSystem = FileSystemService::Get();
			const std::string virtualPath = fileSystem.ToVirtualPath(m_AssetPath);
			const AssetHandle handle = virtualPath.empty()
				? AssetHandle(0) : AssetManager::Get().FindHandleForPath(virtualPath);
			if ((uint64_t)handle != 0)
			{
				AssetMeta meta;
				AssetMetaIO::Read(virtualPath, meta); // keep the stable handle/type
				meta.Mesh.Scale           = m_Scale;
				meta.Mesh.GenerateLODs    = m_GenerateLODs;
				meta.Mesh.LODCount        = m_LODCount;
				meta.Mesh.ImportMaterials = m_ImportMaterials;
				AssetMetaIO::Write(virtualPath, meta);
				AssetManager::Get().Reimport(handle);
				AssetPreviewService::Get().Invalidate(m_AssetPath);
				LoadFromMeta();
			}
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset", ImVec2(120, 0)))
			LoadFromMeta();

		ImGui::End();
	}
}
