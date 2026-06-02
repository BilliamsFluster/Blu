#include "MaterialGraphPanel.h"
#include "Blu/Rendering/MaterialSystem.h"
#include "imgui.h"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

namespace Blu
{
	namespace
	{
		const char* InputName(MaterialGraphInput input)
		{
			switch (input)
			{
				case MaterialGraphInput::AlbedoColor: return "Albedo Color";
				case MaterialGraphInput::Metallic: return "Metallic";
				case MaterialGraphInput::Roughness: return "Roughness";
				case MaterialGraphInput::AO: return "AO";
				case MaterialGraphInput::EmissiveColor: return "Emissive Color";
				case MaterialGraphInput::EmissiveStrength: return "Emissive Strength";
				case MaterialGraphInput::AlphaCutoff: return "Alpha Cutoff";
				case MaterialGraphInput::AlbedoTexture: return "Albedo Texture";
				case MaterialGraphInput::NormalTexture: return "Normal Texture";
				case MaterialGraphInput::MetallicRoughnessTexture: return "Metallic Roughness Texture";
				case MaterialGraphInput::AOTexture: return "AO Texture";
				case MaterialGraphInput::EmissiveTexture: return "Emissive Texture";
			}
			return "Unknown";
		}
	}

	MaterialGraphPanel::MaterialGraphPanel()
		: m_TemplateHandle()
	{
		ResetGraph();
	}

	void MaterialGraphPanel::ResetGraph()
	{
		m_Graph = {};
		m_OutputNodes.clear();
		auto add = [&](MaterialGraphInput input, MaterialGraphNodeID nodeID, bool connected = true)
		{
			m_OutputNodes.emplace_back(input, nodeID);
			if (connected)
				m_Graph.Connect(input, nodeID);
		};

		add(MaterialGraphInput::AlbedoColor, m_Graph.AddVector4Parameter("Albedo Color", glm::vec4(1.0f)));
		add(MaterialGraphInput::Metallic, m_Graph.AddScalarParameter("Metallic", 0.0f));
		add(MaterialGraphInput::Roughness, m_Graph.AddScalarParameter("Roughness", 0.5f));
		add(MaterialGraphInput::AO, m_Graph.AddScalarParameter("AO", 1.0f));
		add(MaterialGraphInput::EmissiveColor, m_Graph.AddVector3Parameter("Emissive Color", glm::vec3(0.0f)));
		add(MaterialGraphInput::EmissiveStrength, m_Graph.AddScalarParameter("Emissive Strength", 0.0f));
		add(MaterialGraphInput::AlphaCutoff, m_Graph.AddScalarParameter("Alpha Cutoff", 0.5f));
		add(MaterialGraphInput::AlbedoTexture, m_Graph.AddTextureParameter("Albedo Texture", AssetHandle(0)), false);
		add(MaterialGraphInput::NormalTexture, m_Graph.AddTextureParameter("Normal Texture", AssetHandle(0)), false);
		add(MaterialGraphInput::MetallicRoughnessTexture, m_Graph.AddTextureParameter("Metallic Roughness Texture", AssetHandle(0)), false);
		add(MaterialGraphInput::AOTexture, m_Graph.AddTextureParameter("AO Texture", AssetHandle(0)), false);
		add(MaterialGraphInput::EmissiveTexture, m_Graph.AddTextureParameter("Emissive Texture", AssetHandle(0)), false);
		m_CompiledTemplate.reset();
		m_Diagnostics.clear();
	}

	void MaterialGraphPanel::OnImGuiRender(bool* open)
	{
		if (!open || !*open)
			return;
		if (!ImGui::Begin("Material Graph", open))
		{
			ImGui::End();
			return;
		}

		ImGui::TextDisabled("Authoring graph. Compile emits a runtime MaterialTemplate asset.");
		const char* blendModes[] = { "Opaque", "Masked", "Transparent", "Additive" };
		int blend = (int)m_Graph.Blend;
		if (ImGui::Combo("Blend Mode", &blend, blendModes, IM_ARRAYSIZE(blendModes)))
			m_Graph.Blend = (BlendMode)blend;
		ImGui::Checkbox("Two Sided", &m_Graph.TwoSided);

		ImGui::SeparatorText("Parameter Nodes");
		for (auto& [input, nodeID] : m_OutputNodes)
		{
			auto node = std::find_if(m_Graph.GetNodes().begin(), m_Graph.GetNodes().end(),
				[&](const MaterialGraphNode& candidate) { return candidate.ID == nodeID; });
			if (node == m_Graph.GetNodes().end())
				continue;

			ImGui::PushID((int)nodeID);
			bool connected = m_Graph.IsConnected(input);
			if (ImGui::Checkbox("##connected", &connected))
			{
				if (connected) m_Graph.Connect(input, nodeID);
				else m_Graph.Disconnect(input);
			}
			ImGui::SameLine();
			ImGui::TextUnformatted(InputName(input));
			ImGui::SameLine(190.0f);
			switch (node->Type)
			{
				case MaterialGraphValueType::Scalar:
					ImGui::DragFloat("##value", &std::get<float>(node->Value), 0.01f);
					break;
				case MaterialGraphValueType::Vector3:
					ImGui::ColorEdit3("##value", glm::value_ptr(std::get<glm::vec3>(node->Value)));
					break;
				case MaterialGraphValueType::Vector4:
					ImGui::ColorEdit4("##value", glm::value_ptr(std::get<glm::vec4>(node->Value)));
					break;
				case MaterialGraphValueType::Texture:
				{
					uint64_t handle = (uint64_t)std::get<AssetHandle>(node->Value);
					if (ImGui::InputScalar("##value", ImGuiDataType_U64, &handle))
						node->Value = AssetHandle(handle);
					break;
				}
			}
			ImGui::PopID();
		}

		ImGui::SeparatorText("Surface Output");
		for (const auto& [input, nodeID] : m_OutputNodes)
		{
			if (m_Graph.IsConnected(input))
				ImGui::BulletText("%s <- node %u", InputName(input), nodeID);
		}

		if (ImGui::Button("Compile Template"))
		{
			auto result = MaterialGraphCompiler::Compile(m_Graph, m_TemplateHandle);
			m_Diagnostics = std::move(result.Diagnostics);
			m_CompiledTemplate = std::move(result.Template);
			if (m_CompiledTemplate)
				MaterialResolver::Get().RegisterTemplate(m_CompiledTemplate);
		}
		ImGui::SameLine();
		if (ImGui::Button("Reset Graph"))
			ResetGraph();

		if (m_CompiledTemplate)
			ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "Compiled template: %llu", (uint64_t)m_TemplateHandle);
		for (const auto& diagnostic : m_Diagnostics)
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.3f, 1.0f), "%s", diagnostic.c_str());

		ImGui::End();
	}
}
