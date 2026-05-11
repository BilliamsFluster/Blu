#include "SceneHierarchyPanel.h"
#include "imgui.h"
#include "Blu/Scene/Component.h"
#include <glm/gtc/type_ptr.hpp>
#include "Blu/Rendering/Texture.h"
#include "Blu/Rendering/Material.h"
#include <imgui_internal.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>
#include "Blu/Scripting/ScriptEngine.h"
#include "Blu/LightSystem/LightManager.h"

namespace Blu
{
	SceneHierarchyPanel::SceneHierarchyPanel(const Shared<Scene>& scene)
	{
		SetContext(scene);
	}
	void SceneHierarchyPanel::SetContext(const Shared<Scene>& scene)
	{
		m_Context = scene;
		m_SelectedEntity = {};
	}
	void SceneHierarchyPanel::SetSelectedEntity(Entity entity)
	{
		m_SelectedEntity = entity;

	}
	void SceneHierarchyPanel::OnImGuiRender(bool* pShowOutliner, bool* pShowDetails)
	{
		m_EntityHovered = false;

		// ------------------------------------------------------------------
		// Outliner (formerly "Scene Hierarchy")
		// ------------------------------------------------------------------
		if (ImGui::Begin("Outliner", pShowOutliner))
		{
			// ---- Search bar ----
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##search", "Search...", m_SearchBuffer, sizeof(m_SearchBuffer));
			ImGui::Separator();

			// ---- Entity list ----
			m_Context->m_Registry.each([&](auto entityID)
			{
				Entity entity{ entityID, m_Context.get() };

				// Filter by search string (case-insensitive substring match).
				if (m_SearchBuffer[0] != '\0')
				{
					const auto& tag = entity.GetComponent<TagComponent>().Tag;
					// Simple case-insensitive search using std::search + tolower lambda.
					auto it = std::search(
						tag.begin(), tag.end(),
						m_SearchBuffer, m_SearchBuffer + strlen(m_SearchBuffer),
						[](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
					if (it == tag.end())
						return; // skip non-matching entities
				}

				DrawEntityNode(entity);
			});

			if (!m_EntityHovered)
			{
				if (ImGui::BeginPopupContextWindow())
				{
					if (ImGui::BeginMenu("Create Entity"))
					{
						if (ImGui::MenuItem("Camera Entity"))
						{
							auto Entity = m_Context->CreateEntity("Camera");
							Entity.AddComponent<CameraComponent>();
						}
						if (ImGui::MenuItem("Mesh Entity"))
						{
							auto Entity = m_Context->CreateEntity("Mesh");
							auto& mc = Entity.AddComponent<MeshComponent>();
							mc.MeshData = Mesh::CreateCube();
							mc.MaterialInstance = Material::Create();
						}
						if (ImGui::MenuItem("Sprite Entity"))
						{
							auto Entity = m_Context->CreateEntity("Sprite");
							Entity.AddComponent<SpriteRendererComponent>();
						}
						ImGui::Separator();
						if (ImGui::MenuItem("Point Light"))
						{
							auto Entity = m_Context->CreateEntity("PointLight");
							Entity.AddComponent<PointLightComponent>();
						}
						if (ImGui::MenuItem("Directional Light"))
						{
							auto Entity = m_Context->CreateEntity("DirectionalLight");
							Entity.AddComponent<DirectionalLightComponent>();
						}
						if (ImGui::MenuItem("Spot Light"))
						{
							auto Entity = m_Context->CreateEntity("SpotLight");
							Entity.AddComponent<SpotLightComponent>();
						}
						ImGui::Separator();
						if (ImGui::MenuItem("Empty Entity"))
						{
							m_Context->CreateEntity("Empty");
						}
						ImGui::EndMenu();
					}
					ImGui::EndPopup();
				}
			}
		}
		ImGui::End(); // Outliner

		// ------------------------------------------------------------------
		// Details (formerly "Properties")
		// ------------------------------------------------------------------
		if (ImGui::Begin("Details", pShowDetails))
		{
			if (m_SelectedEntity)
				DrawEntityComponents(m_SelectedEntity);
		}
		ImGui::End(); // Details
	}
	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		ImGuiTreeNodeFlags flags = ((m_SelectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0)|ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());
		if (ImGui::IsItemClicked())
		{
			m_SelectedEntity = entity;

		}
		m_EntityHovered |= ImGui::IsItemHovered();

		bool entityDeleted = false;
		
		
		if (ImGui::BeginPopupContextItem())
		{
			std::string selectedEntityName = std::format("Delete {}", m_SelectedEntity.GetComponent<TagComponent>().Tag.c_str());
			if (ImGui::MenuItem(selectedEntityName.c_str()))
			{
				entityDeleted = true;
			}
			ImGui::EndPopup();
		}
		if (opened)
		{
			ImGui::TreePop();
		}
		if (entityDeleted)
		{
			m_Context->DestroyEntity(entity);
			if (m_SelectedEntity == entity)
			{
				m_SelectedEntity = {};
			}


		}
	}
	const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
	template<typename ComponentType, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, UIFunction function)
	{
		if (entity.HasComponent<ComponentType>())
		{
			auto& entityComponent = entity.GetComponent<ComponentType>();
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;
			ImGui::Separator();
			bool open = ImGui::TreeNodeEx((void*)typeid(ComponentType).hash_code(), treeNodeFlags, name.c_str());
			ImGui::PopStyleVar();
			ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f - 13.0f);
			if (ImGui::Button("...", ImVec2{ lineHeight, lineHeight }))
			{
				ImGui::OpenPopup("Component Settings");
			}
			bool removeComponent = false;
			if (ImGui::BeginPopup("Component Settings"))
			{
				if (ImGui::MenuItem("Remove Component"))
				{
					removeComponent = true;
				}
				ImGui::EndPopup();
			}
			if (open)
			{
				if (!removeComponent)
				{
					function(entityComponent);
				}

				else
				{
					entity.RemoveComponent<ComponentType>();

				}
				ImGui::TreePop();
			}

		}
	}
	static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f)
	{
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];
		ImGui::PushID(label.c_str());
		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();
		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });
		float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };
		
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
		{
			values.x = resetValue;
		}
		ImGui::PopStyleColor(3);
		ImGui::SameLine();
		ImGui::DragFloat("##X", &values.x, 0.1f);
		ImGui::PopItemWidth();
		ImGui::PopFont();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.3f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
		{
			values.y = resetValue;
		}
		ImGui::SameLine();
		ImGui::DragFloat("##Y", &values.y, 0.1f);
		ImGui::PopItemWidth();
		ImGui::SameLine();
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
		{
			values.z = resetValue;
		}
		ImGui::SameLine();
		ImGui::DragFloat("##Z", &values.z, 0.1f);
		ImGui::PopItemWidth();
		ImGui::Columns(1);
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::PopStyleVar();
		ImGui::PopID();
	}
	static void DrawParticleSystemPanel(ParticleSystemComponent& component)
	{
		
		DrawVec3Control("Position", component.ParticleSystemProps.Position);
		DrawVec3Control("Velocity", component.ParticleSystemProps.Velocity);

		ImGui::Text("Color Begin");
		ImGui::ColorEdit4("Color Begin", glm::value_ptr(component.ParticleSystemProps.ColorBegin));

		ImGui::Text("Color End");
		ImGui::ColorEdit4("Color End", glm::value_ptr(component.ParticleSystemProps.ColorEnd));
		DrawVec3Control("Rotation", component.ParticleAttributes.Rotation);

		ImGui::Text("Size Begin");
		ImGui::SliderFloat("Size Begin", &component.ParticleSystemProps.SizeBegin, 0.0f, 5.0f);
		ImGui::Text("Size End");
		ImGui::SliderFloat("Size End", &component.ParticleSystemProps.SizeEnd, 0.0f, 5.0f);
		ImGui::Text("Size Variation");
		ImGui::SliderFloat("Size Variation", &component.ParticleSystemProps.SizeVariation, 0.0f, 1.0f);
		ImGui::Text("Life Time");
		ImGui::SliderFloat("Life Time", &component.ParticleSystemProps.LifeTime, 0.0f, 10.0f);
		ImGui::Text("Max Particles Per Emit");
		ImGui::InputInt("Max Particles Per Emit", &component.ParticleSystemProps.MaxParticlesPerEmit);

		ImGui::Text("Particle Count");
		ImGui::InputInt("Particle Count", &component.ParticleSystemProps.ParticleCount);

		ImGui::Text("Looping");
		ImGui::Checkbox("Looping", &component.ParticleSystemProps.Looping);

		ImGui::Text("Max Loop Count");
		ImGui::InputInt("Max Loop Count", &component.ParticleSystemProps.MaxLoopCount);

		ImGui::Text("Start Simulation");
		//ImGui::Checkbox("Loop Count", &component.ParticleSystemProps.LoopCount);
			

		
	}
	
	
	//static std::map<int, ParticleSystemComponent> ParticleSystems;
	static std::vector<ParticleSystemComponent> ParticleSystems(10);
	void SceneHierarchyPanel::DrawEntityComponents(Entity entity)
	{
		ImGui::BeginChild("Properties Window");
		if (entity.HasComponent<TagComponent>())
		{
			auto& tag = entity.GetComponent<TagComponent>().Tag;
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), tag.c_str());
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer)))
			{
				tag = std::string(buffer);
			}

		}
		ImGui::SameLine();
		ImGui::PushItemWidth(-1);
		if (ImGui::Button("Add Component"))
		{
			ImGui::OpenPopup("AddComponent");
		}
		if (ImGui::BeginPopup("AddComponent"))
		{
			if (!m_SelectedEntity.HasComponent<CameraComponent>())
			{
				if (ImGui::MenuItem("Camera"))
				{
					m_SelectedEntity.AddComponent<CameraComponent>();
					ImGui::CloseCurrentPopup();
				}

			}
			if (!m_SelectedEntity.HasComponent<ScriptComponent>())
			{
				ScriptEngine::RemoveEntityInstance(&m_SelectedEntity);
				if (ImGui::MenuItem("Script"))
				{
					m_SelectedEntity.AddComponent<ScriptComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			
			if (!m_SelectedEntity.HasComponent<SpriteRendererComponent>())
			{
				if (ImGui::MenuItem("Sprite Renderer"))
				{
					m_SelectedEntity.AddComponent<SpriteRendererComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<PointLightComponent>())
			{
				if (ImGui::MenuItem("Point Light"))
				{
					m_SelectedEntity.AddComponent<PointLightComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<DirectionalLightComponent>())
			{
				if (ImGui::MenuItem("Directional Light"))
				{
					m_SelectedEntity.AddComponent<DirectionalLightComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<SpotLightComponent>())
			{
				if (ImGui::MenuItem("Spot Light"))
				{
					m_SelectedEntity.AddComponent<SpotLightComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<CircleRendererComponent>())
			{
				if (ImGui::MenuItem("Circle Renderer"))
				{
					m_SelectedEntity.AddComponent<CircleRendererComponent>();
					ImGui::CloseCurrentPopup();
				}

			}


			if (!m_SelectedEntity.HasComponent<ParticleSystemComponent>())
			{
				if (ImGui::MenuItem("Particle System"))
				{
					m_SelectedEntity.AddComponent<ParticleSystemComponent>();
					ImGui::CloseCurrentPopup();
				}

			}

			if (!m_SelectedEntity.HasComponent<Rigidbody2DComponent>())
			{
				if (ImGui::MenuItem("Rigidbody 2D"))
				{
					m_SelectedEntity.AddComponent<Rigidbody2DComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<BoxCollider2DComponent>())
			{
				if (ImGui::MenuItem("Box Collider 2D"))
				{
					m_SelectedEntity.AddComponent<BoxCollider2DComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<CircleCollider2DComponent>())
			{
				if (ImGui::MenuItem("Circle Collider 2D"))
				{
					m_SelectedEntity.AddComponent<CircleCollider2DComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<MeshComponent>())
			{
				if (ImGui::MenuItem("Mesh Renderer"))
				{
					auto& mc = m_SelectedEntity.AddComponent<MeshComponent>();
					mc.MeshData = Mesh::CreateCube();
					mc.MaterialInstance = Material::Create();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}

		DrawComponent<TransformComponent>("Transform", entity, [](auto& component)
			{

				DrawVec3Control("Translation", component.Translation);
				glm::vec3 rotation = glm::degrees(component.Rotation);
				ImGui::Spacing();
				DrawVec3Control("Rotation", rotation);
				component.Rotation = glm::radians(rotation);
				ImGui::Spacing();
				DrawVec3Control("Scale", component.Scale, 1.0f);
			});
		// ── Point Light ─────────────────────────────────────────────────────────
		DrawComponent<PointLightComponent>("Point Light", entity, [](auto& L)
			{
				// Two-column layout: label (fixed 80 px) | control (fill)
				constexpr float kLabelW = 80.0f;
				ImGui::Columns(2, "PLCols", false);
				ImGui::SetColumnWidth(0, kLabelW);

				ImGui::AlignTextToFramePadding(); ImGui::Text("Ambient");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::ColorEdit3("##PL_Ambient",  glm::value_ptr(L.Ambient));  ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Diffuse");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::ColorEdit3("##PL_Diffuse",  glm::value_ptr(L.Diffuse));  ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Specular");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::ColorEdit3("##PL_Specular", glm::value_ptr(L.Specular)); ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Intensity");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##PL_Intensity", &L.Intensity, 0.05f, 0.0f, 20.0f, "%.2f"); ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Range");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##PL_Range",     &L.Range,     0.5f,  0.1f, 500.0f, "%.1f"); ImGui::NextColumn();

				ImGui::Columns(1);

				ImGui::Spacing();
				ImGui::TextDisabled("Attenuation  (1 / (c + l*d + q*d\xc2\xb2))");

				ImGui::Columns(2, "PLAttCols", false);
				ImGui::SetColumnWidth(0, kLabelW);

				ImGui::AlignTextToFramePadding(); ImGui::Text("Constant");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##PL_Const",  &L.AttConstant,  0.001f,  0.0f, 5.0f, "%.4f"); ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Linear");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##PL_Lin",    &L.AttLinear,    0.001f,  0.0f, 5.0f, "%.4f"); ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Quadratic");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##PL_Quad",   &L.AttQuadratic, 0.0001f, 0.0f, 5.0f, "%.5f"); ImGui::NextColumn();

				ImGui::Columns(1);

				ImGui::Spacing();
				ImGui::TextDisabled("Range Presets");
				if (ImGui::Button("Range 7##PL"))   { L.Range=7;   L.AttConstant=1.0f; L.AttLinear=0.70f;  L.AttQuadratic=1.80f;   }
				ImGui::SameLine();
				if (ImGui::Button("Range 20##PL"))  { L.Range=20;  L.AttConstant=1.0f; L.AttLinear=0.22f;  L.AttQuadratic=0.20f;   }
				ImGui::SameLine();
				if (ImGui::Button("Range 50##PL"))  { L.Range=50;  L.AttConstant=1.0f; L.AttLinear=0.09f;  L.AttQuadratic=0.032f;  }
				ImGui::SameLine();
				if (ImGui::Button("Range 100##PL")) { L.Range=100; L.AttConstant=1.0f; L.AttLinear=0.045f; L.AttQuadratic=0.0075f; }
			});

		// ── Directional Light ─────────────────────────────────────────────────
		DrawComponent<DirectionalLightComponent>("Directional Light", entity, [](auto& L)
			{
				DrawVec3Control("Direction", L.Direction);
				if (glm::length(L.Direction) > 0.001f)
					L.Direction = glm::normalize(L.Direction);

				ImGui::Spacing();

				constexpr float kLabelW = 80.0f;
				ImGui::Columns(2, "DLCols", false);
				ImGui::SetColumnWidth(0, kLabelW);

				ImGui::AlignTextToFramePadding(); ImGui::Text("Ambient");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::ColorEdit3("##DL_Ambient",  glm::value_ptr(L.Ambient));  ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Diffuse");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::ColorEdit3("##DL_Diffuse",  glm::value_ptr(L.Diffuse));  ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Specular");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::ColorEdit3("##DL_Specular", glm::value_ptr(L.Specular)); ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Intensity");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##DL_Intensity", &L.Intensity, 0.05f, 0.0f, 20.0f, "%.2f"); ImGui::NextColumn();

				ImGui::Columns(1);
			});

		// ── Spot Light ────────────────────────────────────────────────────────
		DrawComponent<SpotLightComponent>("Spot Light", entity, [](auto& L)
			{
				DrawVec3Control("Direction", L.Direction);
				if (glm::length(L.Direction) > 0.001f)
					L.Direction = glm::normalize(L.Direction);

				ImGui::Spacing();

				constexpr float kLabelW = 80.0f;
				ImGui::Columns(2, "SLCols", false);
				ImGui::SetColumnWidth(0, kLabelW);

				ImGui::AlignTextToFramePadding(); ImGui::Text("Ambient");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::ColorEdit3("##SL_Ambient",  glm::value_ptr(L.Ambient));  ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Diffuse");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::ColorEdit3("##SL_Diffuse",  glm::value_ptr(L.Diffuse));  ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Specular");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::ColorEdit3("##SL_Specular", glm::value_ptr(L.Specular)); ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Intensity");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##SL_Intensity", &L.Intensity, 0.05f, 0.0f, 20.0f, "%.2f"); ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Range");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##SL_Range",     &L.Range,     0.5f,  0.1f, 500.0f, "%.1f"); ImGui::NextColumn();

				ImGui::Columns(1);

				ImGui::Spacing();
				ImGui::TextDisabled("Cone Angles");

				ImGui::Columns(2, "SLConeCols", false);
				ImGui::SetColumnWidth(0, kLabelW);

				ImGui::AlignTextToFramePadding(); ImGui::Text("Inner Cone");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##SL_Inner", &L.InnerConeAngle, 0.5f, 0.5f, 89.0f, "%.1f\xc2\xb0"); ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Outer Cone");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##SL_Outer", &L.OuterConeAngle, 0.5f, 1.0f, 90.0f, "%.1f\xc2\xb0"); ImGui::NextColumn();

				ImGui::Columns(1);
				if (L.OuterConeAngle < L.InnerConeAngle + 0.5f)
					L.OuterConeAngle = L.InnerConeAngle + 0.5f;

				ImGui::Spacing();
				ImGui::TextDisabled("Attenuation  (1 / (c + l*d + q*d\xc2\xb2))");

				ImGui::Columns(2, "SLAttCols", false);
				ImGui::SetColumnWidth(0, kLabelW);

				ImGui::AlignTextToFramePadding(); ImGui::Text("Constant");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##SL_Const",  &L.AttConstant,  0.001f,  0.0f, 5.0f, "%.4f"); ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Linear");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##SL_Lin",    &L.AttLinear,    0.001f,  0.0f, 5.0f, "%.4f"); ImGui::NextColumn();

				ImGui::AlignTextToFramePadding(); ImGui::Text("Quadratic");
				ImGui::NextColumn(); ImGui::SetNextItemWidth(-1.0f);
				ImGui::DragFloat("##SL_Quad",   &L.AttQuadratic, 0.0001f, 0.0f, 5.0f, "%.5f"); ImGui::NextColumn();

				ImGui::Columns(1);
			});




		DrawComponent<ScriptComponent>("Script", entity, [entity](auto& component) mutable
			{
				bool scriptExists = ScriptEngine::EntityClassExists(component.Name);
				const auto& entities = ScriptEngine::GetEntities();
				static std::unordered_map<std::string, Shared<ScriptClass>> searchResults;
				Shared<ScriptClass> scriptClass = ScriptEngine::GetEntityScriptClass(component.Name);


				Shared<ScriptInstance> scriptInstance = ScriptEngine::GetEntityScriptInstance(entity.GetUUID());
				
				if (scriptClass)
				{
					if (scriptInstance) 
					{
						auto& fields = scriptClass->GetScriptFields();
						for (const auto& [name, field] : fields)
						{
							if (field.Type == ScriptFieldType::Float)
							{
								float data = scriptInstance->GetFieldValue<float>(name);

								ImGui::Text("%s", name.c_str());
								ImGui::SameLine();
								ImGui::PushID(name.c_str()); // Add an ID to prevent conflicts with other ImGui widgets
								ImGui::PushItemWidth(100); // Set the width of the next widget to 100
								
								

								if (ImGui::DragFloat("##data", &data))
								{
									scriptInstance->SetFieldValue<float>(name, data);
									
								}
								ImGui::PopItemWidth(); // Reset the width for subsequent widgets
								ImGui::PopID();
								
							}
							// Handle other field types...
						}
					}
					
				}
				


				std::string buttonLabel = component.Name.empty() ? "Add Script" : component.Name;
				if (ImGui::Button(buttonLabel.c_str()))
				{
					ImGui::OpenPopup("AddScriptPopup");
				}

				ImGui::SameLine(); // Set next control on the same line

				// Add the clear button
				if (!component.Name.empty() && ImGui::Button("Clear Script"))
				{
					component.Name.clear();
				}

				ImGui::SetNextWindowSize(ImVec2(200, 100));
				// The pop-up dialog
				if (ImGui::BeginPopup("AddScriptPopup"))
				{
					// This is your search field
					static char searchBuffer[64] = "";
					static char lastSearchBuffer[64] = "";
					ImGui::SetNextItemWidth(-1); // Set width to occupy all available space
					if (ImGui::InputText("##Search", searchBuffer, sizeof(searchBuffer), ImGuiInputTextFlags_CharsNoBlank))
					{
						if (strcmp(searchBuffer, lastSearchBuffer) != 0) // Check if the search text has changed
						{
							// Clear the previous search results
							searchResults.clear();

							// Go through each entity and check if its name contains the search buffer
							for (const auto& e : entities)
							{
								if (e.first.find(searchBuffer) != std::string::npos)
								{
									// If the entity's name contains the search buffer, add it to the search results
									searchResults.insert(e);
								}
							}

							strcpy(lastSearchBuffer, searchBuffer); // Remember the last search text
						}
					}

					ImGui::Separator(); // Add a separator for visual clarity

					ImGui::BeginChild("EntityList", ImVec2(-1, -ImGui::GetFrameHeightWithSpacing())); // Auto-resizing child window

					if (strlen(searchBuffer) == 0)
					{
						// If searchBuffer is empty, loop over the entities map
						for (const auto& e : entities)
						{
							std::string entityName = e.first;

							// Check if this entity is currently selected
							bool isSelected = entityName == component.Name;

							if (ImGui::Selectable(entityName.c_str(), isSelected))
							{
								component.Name = entityName;
								ImGui::CloseCurrentPopup(); // Close the popup when an entity is selected
								UUID uuid = entity.GetUUID();
								ScriptEngine::OnCreateEntity(&entity); // creates the scriptInstance


							}

							// If this entity is selected, make it default focused
							if (isSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
					}
					else
					{
						// If searchBuffer is not empty, loop over the searchResults map
						for (const auto& e : searchResults)
						{
							std::string entityName = e.first;

							// Check if this entity is currently selected
							bool isSelected = entityName == component.Name;

							if (ImGui::Selectable(entityName.c_str(), isSelected))
							{
								component.Name = entityName;
								ImGui::CloseCurrentPopup(); // Close the popup when an entity is selected
								ScriptEngine::OnCreateEntity(&entity); // creates the scriptInstance


							}

							// If this entity is selected, make it default focused
							if (isSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
					}

					ImGui::EndChild();
					ImGui::EndPopup();
				}


			});

		DrawComponent<ParticleSystemComponent>("Particle System", entity, [&](auto& component)
			{
				static const char* particleSystems[] = { "Default", "Fountain", "Explosion", "RainFall" }; // Add more as needed
				static int currentParticleSystem = 0;
				static int lastParticleSystem = -1;
				float itemWidth = 2.0f; // Adjust this value as needed
				ImGui::PushItemWidth(ImGui::GetWindowWidth() / itemWidth);

				if (ImGui::Combo("Particle System", &currentParticleSystem, particleSystems, IM_ARRAYSIZE(particleSystems)))
				{
					// The user has selected a different particle system type.
					if (lastParticleSystem != -1)
					{
						// Save the current state of the particle system component to the previous particle system type.
						ParticleSystems[lastParticleSystem] = component;
					}

					// Load the previous state of the new particle system type into the component.
					component = ParticleSystems[currentParticleSystem];

					lastParticleSystem = currentParticleSystem;
				}


				switch (currentParticleSystem)
				{
				case 0: // Default - Call the approate Particle function in the Particle Component OnUpdateFunction
					component.CurrentParticleSystem = [&]() { component.PSystem.Emit(component.ParticleSystemProps); };
					break;
				case 1: // Fountain - Call the approate Particle function in the Particle Component OnUpdateFunction
					component.CurrentParticleSystem = [&]() { component.PSystem.EmitFountain(component.ParticleSystemProps); };
					break;
				case 2: // Explosion - Call the approate Particle function in the Particle Component OnUpdateFunction
					component.CurrentParticleSystem = [&]() { component.PSystem.EmitExplosion(component.ParticleSystemProps); };
					break;
				case 3: // RainFall - Call the approate Particle function in the Particle Component OnUpdateFunction
					component.CurrentParticleSystem = [&]() { component.PSystem.EmitRainFall(component.ParticleSystemProps); };
					break;
				}

				DrawParticleSystemPanel(component);

			});





		DrawComponent<CameraComponent>("Camera", entity, [](auto& component)
			{
				float itemWidth = 2.0f; // Adjust this value as needed
				ImGui::PushItemWidth(ImGui::GetWindowWidth() / itemWidth);
				auto& camera = component.Camera;
				ImGui::Checkbox("Primary", &component.Primary);
				ImGui::Checkbox("Fixed AspectRatio", &component.FixedAspectRatio);

				const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
				const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];
				if (ImGui::BeginCombo("Projection", currentProjectionTypeString))
				{

					for (int i = 0; i < 2; i++)
					{
						bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
						if (ImGui::Selectable(projectionTypeStrings[i], isSelected))
						{
							currentProjectionTypeString = projectionTypeStrings[i];
							camera.SetProjectionType((SceneCamera::ProjectionType)i);
						}
						if (isSelected)
						{
							ImGui::SetItemDefaultFocus();
						}
					}
					ImGui::EndCombo();
				}
				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective)
				{
					float fov = camera.GetPerspectiveFOV();
					float pNear = camera.GetPerspectiveNear();
					float pFar = camera.GetPerspectiveFar();

					ImGui::DragFloat("Perspective FOV", &fov);
					camera.SetPerspectiveFOV(fov);

					ImGui::DragFloat("Perspective Near", &pNear);
					camera.SetPerspectiveNear(pNear);

					ImGui::DragFloat("Perspective Far", &pFar);
					camera.SetPerspectiveFar(pFar);
				}
				if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic)
				{
					float orthoSize = camera.GetOrthographicSize();
					float orthoNear = camera.GetOrthographicNearClip();
					float orthoFar = camera.GetOrthographicFarClip();

					ImGui::DragFloat("Ortho Size", &orthoSize);
					camera.SetOrthographicSize(orthoSize);

					ImGui::DragFloat("Ortho Near", &orthoNear);
					camera.SetOrthographicNearClip(orthoNear);

					ImGui::DragFloat("Ortho Far", &orthoFar);
					camera.SetOrthographicFarClip(orthoFar);

				}




			});
		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](auto& component)
			{
				float itemWidth = 2.0f; // Adjust this value as needed
				ImGui::PushItemWidth(ImGui::GetWindowWidth() / itemWidth);
				
				// New section for material
				if (ImGui::CollapsingHeader("Material"))
				{
					if (!component.MaterialInstance) return;

					// Helper lambda to handle texture UI
					auto HandleTextureUI = [&](const char* label, Shared<Texture2D>& texture)
						{
							if (texture)
							{
								ImGui::Text(label);
								if (ImGui::IsItemHovered())
								{
									std::string tooltip = "Texture Path: " + std::string(texture->GetTexturePath());
									ImGui::SetTooltip(tooltip.c_str());
								}
								ImGui::SameLine();
								std::string buttonLabel = "X##" + std::string(label);
								if (ImGui::Button(buttonLabel.c_str()))
								{
									texture = nullptr;
								}
								if (ImGui::IsItemHovered())
								{
									std::string tooltip = "Clear " + std::string(label);
									ImGui::SetTooltip(tooltip.c_str());
								}
							}
							else
							{
								ImGui::Button(label, ImVec2(50.0f, 50.0f));
								if (ImGui::BeginDragDropTarget())
								{
									if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
									{
										std::filesystem::path payloadPath = std::string(reinterpret_cast<const char*>(payload->Data));
										texture = Texture2D::Create(payloadPath.string());
										texture->GetTexturePath();
									}
									ImGui::EndDragDropTarget();
								}
							}
						};

					// Handle each texture type
					HandleTextureUI("Albedo Map", component.MaterialInstance->AlbedoMap);
					// Albedo Color
					ImGui::ColorEdit4("Albedo Color: ", glm::value_ptr(component.Color));

					
					
					

					HandleTextureUI("Diffuse Map", component.MaterialInstance->DiffuseMap);
					ImGui::ColorEdit4("Diffuse Color: ", glm::value_ptr(component.MaterialInstance->GetDiffuseColor()));

					HandleTextureUI("Specular Map", component.MaterialInstance->SpecularMap);
					ImGui::ColorEdit4("Specular Color: ", glm::value_ptr(component.MaterialInstance->GetSpecularColor()));
					ImGui::ColorEdit4("Ambient Color: ", glm::value_ptr(component.MaterialInstance->GetAmbientColor()));

					HandleTextureUI("Normal Map", component.MaterialInstance->NormalMap);

				}
			});

		DrawComponent<CircleRendererComponent>("Circle Renderer", entity, [](auto& component)
			{
				float itemWidth = 2.0f; // Adjust this value as needed
				ImGui::PushItemWidth(ImGui::GetWindowWidth() / itemWidth);
				ImGui::ColorEdit4("Color: ", glm::value_ptr(component.Color));
				ImGui::DragFloat("Thickness", &component.Thickness, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Fade", &component.Fade, 0.01f, 0.0f, 1.0f);
			});
		DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, [](auto& component)
			{

				float itemWidth = 2.0f;
				float itemHeight = 10.0f;
				ImGui::PushItemWidth(ImGui::GetWindowWidth() / itemWidth);


				ImGui::Text("Offset");
				ImGui::DragFloat2("##Offset", glm::value_ptr(component.Offset), 0.1f);
				ImGui::Text("Size");
				ImGui::DragFloat2("##Size", glm::value_ptr(component.Size), 0.1f);

				ImGui::Text("Density");
				ImGui::DragFloat("##Density", &component.Density, 0.01f, 0.0f, 10.0f);
				ImGui::Text("Friction");
				ImGui::DragFloat("##Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
				ImGui::Text("Restitution");
				ImGui::DragFloat("##Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
				ImGui::Text("Restitution Threshold");
				ImGui::DragFloat("##RestitutionThreshold", &component.RestitutionThreshold, 0.1f, 0.0f, 10.0f);
				ImGui::Checkbox("Show Collision", &component.ShowCollision);


			});

		DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, [](auto& component)
			{

				float itemWidth = 2.0f;
				float itemHeight = 10.0f;
				ImGui::PushItemWidth(ImGui::GetWindowWidth() / itemWidth);


				ImGui::Text("Offset");
				ImGui::DragFloat2("##Offset", glm::value_ptr(component.Offset), 0.1f);
				ImGui::Text("Density");
				ImGui::DragFloat("##Density", &component.Density, 0.01f, 0.0f, 10.0f);
				ImGui::Text("Friction");
				ImGui::DragFloat("##Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
				ImGui::Text("Restitution");
				ImGui::DragFloat("##Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
				ImGui::Text("Restitution Threshold");
				ImGui::DragFloat("##RestitutionThreshold", &component.RestitutionThreshold, 0.1f, 0.0f, 10.0f);


			});
		DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, [](auto& component)
			{
				const char* bodyTypes[] = { "Static", "Dynamic", "Kinematic" };
				const char* currentBodyType = bodyTypes[(int)component.Type];
				if (ImGui::BeginCombo("Body Type", currentBodyType)) // The second parameter is the label previewed before opening the combo.
				{
					for (int i = 0; i < IM_ARRAYSIZE(bodyTypes); i++)
					{
						bool isSelected = (currentBodyType == bodyTypes[i]); // You can store your selection however you want.
						if (ImGui::Selectable(bodyTypes[i], isSelected))
							component.Type = (Rigidbody2DComponent::BodyType)i;

						if (isSelected)
							ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + keyboard navigation focus).
					}
					ImGui::EndCombo();
				}
				ImGui::Checkbox("Fixed Rotation", &component.FixedRotation);
			});

		DrawComponent<MeshComponent>("Mesh Renderer", entity, [](auto& component)
			{
				static const char* meshTypes[] = { "Cube", "Quad" };
				static int currentMeshType = 0;
				if (ImGui::Combo("Mesh Type", &currentMeshType, meshTypes, IM_ARRAYSIZE(meshTypes)))
				{
					if (currentMeshType == 0)
						component.MeshData = Mesh::CreateCube();
					else
						component.MeshData = Mesh::CreateQuad();
				}

				if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
				{
					if (!component.MaterialInstance)
						component.MaterialInstance = Material::Create();

					ImGui::ColorEdit3("Ambient##Mesh",  glm::value_ptr(component.MaterialInstance->GetAmbientColor()));
					ImGui::ColorEdit3("Diffuse##Mesh",  glm::value_ptr(component.MaterialInstance->GetDiffuseColor()));
					ImGui::ColorEdit3("Specular##Mesh", glm::value_ptr(component.MaterialInstance->GetSpecularColor()));
					ImGui::DragFloat("Shininess##Mesh", &component.MaterialInstance->GetShininess(), 0.5f, 1.0f, 256.0f);
				}
			});

		float extraSpace = 200.0f;  // Extra space at the end in pixels
		ImGui::Dummy(ImVec2(0.0f, extraSpace));

		ImGui::EndChild();
		
			
		
	}
}