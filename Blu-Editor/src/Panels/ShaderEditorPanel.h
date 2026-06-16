#pragma once
#include <filesystem>
#include <string>

namespace Blu
{
	// In-editor HLSL editor — open a .hlsl from the Content Browser (double-click), edit the source,
	// Save to disk, and Save & Recompile to hot-reload every live shader using that file
	// (Shader::ReloadFile). A bad edit keeps the previously-working shader, so iteration is safe.
	class ShaderEditorPanel
	{
	public:
		void Open(const std::filesystem::path& path);
		void OnImGuiRender(bool* open);

	private:
		void Load();

		std::filesystem::path m_Path;
		std::string m_Source;
		std::string m_Status;
		bool m_Loaded = false;
	};
}
