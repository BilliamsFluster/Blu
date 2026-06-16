#include "ShaderEditorPanel.h"
#include "imgui.h"
#include "Blu/Rendering/Shader.h"
#include <fstream>
#include <sstream>
#include <string>

namespace Blu
{
	// Grow the std::string backing store as the user types (ImGui resizable-buffer idiom).
	static int ShaderTextResize(ImGuiInputTextCallbackData* data)
	{
		if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
		{
			auto* str = static_cast<std::string*>(data->UserData);
			str->resize(data->BufTextLen);
			data->Buf = str->data();
		}
		return 0;
	}

	void ShaderEditorPanel::Open(const std::filesystem::path& path)
	{
		m_Path = path;
		m_Loaded = false;
		m_Status.clear();
		Load();
	}

	void ShaderEditorPanel::Load()
	{
		m_Source.clear();
		m_Loaded = true;
		if (m_Path.empty())
			return;
		std::ifstream file(m_Path, std::ios::binary);
		if (!file)
		{
			m_Status = "could not open file";
			return;
		}
		std::stringstream ss;
		ss << file.rdbuf();
		m_Source = ss.str();
		m_Source.reserve(m_Source.size() + 4096); // headroom so early edits don't reallocate
	}

	void ShaderEditorPanel::OnImGuiRender(bool* open)
	{
		if (open && !*open)
			return;
		if (!ImGui::Begin("Shader Editor", open))
		{
			ImGui::End();
			return;
		}
		if (m_Path.empty())
		{
			ImGui::TextDisabled("No shader open.");
			ImGui::TextWrapped("Double-click a .hlsl file in the Content Browser to edit it here.");
			ImGui::End();
			return;
		}
		if (!m_Loaded)
			Load();

		ImGui::TextUnformatted(m_Path.filename().string().c_str());
		ImGui::SameLine();
		if (ImGui::SmallButton("Reload from disk"))
			Load();

		bool doSave = false, doRecompile = false;
		if (ImGui::Button("Save"))                       doSave = true;
		ImGui::SameLine();
		if (ImGui::Button("Save & Recompile")) { doSave = true; doRecompile = true; }
		if (!m_Status.empty())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("(%s)", m_Status.c_str());
		}

		ImGui::Separator();
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		ImGui::InputTextMultiline("##shadersrc", m_Source.data(), m_Source.capacity() + 1, avail,
			ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize, ShaderTextResize, &m_Source);

		if (doSave)
		{
			std::ofstream out(m_Path, std::ios::binary | std::ios::trunc);
			if (out) { out << m_Source; m_Status = "saved"; }
			else     { m_Status = "save failed"; }
		}
		if (doRecompile)
		{
			const int n = Shader::ReloadFile(m_Path.string());
			m_Status = "recompiled " + std::to_string(n) + " shader(s)";
		}

		ImGui::End();
	}
}
