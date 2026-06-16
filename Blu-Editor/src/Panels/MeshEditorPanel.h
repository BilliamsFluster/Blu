#pragma once
#include <filesystem>

namespace Blu
{
	// "Static Mesh" asset editor — opened from the Content Browser (double-click or context menu on a
	// model file). Shows a live preview, the model's material-slot count, and the .meta import settings
	// (scale / material import / LOD generation) with a Reimport action that re-cooks with the edited
	// settings. The properties window for imported meshes (Unreal-style).
	class MeshEditorPanel
	{
	public:
		void Open(const std::filesystem::path& assetPath); // set target + (re)read its .meta
		void OnImGuiRender(bool* open);
		bool HasTarget() const { return !m_AssetPath.empty(); }

	private:
		void LoadFromMeta();

		std::filesystem::path m_AssetPath;
		// Editable .meta import settings (mirror MeshImportSettings).
		float m_Scale           = 1.0f;
		bool  m_GenerateLODs    = false;
		int   m_LODCount        = 3;
		bool  m_ImportMaterials = true;
		// Cached read-only info.
		int   m_MaterialSlots   = -1; // -1 = unknown / not loaded
		bool  m_Loaded          = false;
	};
}
