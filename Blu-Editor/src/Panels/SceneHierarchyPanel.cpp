#include "SceneHierarchyPanel.h"
#include "imgui.h"
#include "Blu/Scene/Component.h"
#include "Blu/GameFramework/GameFramework.h"
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>
#include "Blu/Rendering/Texture.h"
#include "Blu/Rendering/Material.h"
#include "Blu/Rendering/ModelLoader.h"
#include "Blu/Audio/AudioEngine.h"
#include "Blu/Utils/PlatformUtils.h"
#include <imgui_internal.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <random>
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
			if (!m_SelectedEntity.HasComponent<Rigidbody3DComponent>())
			{
				if (ImGui::MenuItem("Rigidbody 3D"))
				{
					m_SelectedEntity.AddComponent<Rigidbody3DComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<BoxCollider3DComponent>())
			{
				if (ImGui::MenuItem("Box Collider 3D"))
				{
					m_SelectedEntity.AddComponent<BoxCollider3DComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<SphereCollider3DComponent>())
			{
				if (ImGui::MenuItem("Sphere Collider 3D"))
				{
					m_SelectedEntity.AddComponent<SphereCollider3DComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<CapsuleCollider3DComponent>())
			{
				if (ImGui::MenuItem("Capsule Collider 3D"))
				{
					m_SelectedEntity.AddComponent<CapsuleCollider3DComponent>();
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
			if (!m_SelectedEntity.HasComponent<MeshLODComponent>())
			{
				if (ImGui::MenuItem("Mesh LOD"))
				{
					m_SelectedEntity.AddComponent<MeshLODComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<SpringArmComponent>())
			{
				if (ImGui::MenuItem("Spring Arm"))
				{
					m_SelectedEntity.AddComponent<SpringArmComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<AudioSourceComponent>())
			{
				if (ImGui::MenuItem("Audio Source"))
				{
					m_SelectedEntity.AddComponent<AudioSourceComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<FoliageComponent>())
			{
				if (ImGui::MenuItem("Foliage (GPU Instanced)"))
				{
					m_SelectedEntity.AddComponent<FoliageComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<AnimatorComponent>())
			{
				if (ImGui::MenuItem("Animator"))
				{
					m_SelectedEntity.AddComponent<AnimatorComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<NativeScriptComponent>())
			{
				if (ImGui::MenuItem("Native Script"))
				{
					m_SelectedEntity.AddComponent<NativeScriptComponent>();
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

					auto& mi = *component.MaterialInstance;

					ImGui::ColorEdit4("Albedo Color: ", glm::value_ptr(component.Color));
					ImGui::DragFloat("Metallic",         &mi.Metallic,         0.01f, 0.0f, 1.0f);
					ImGui::DragFloat("Roughness",        &mi.Roughness,        0.01f, 0.0f, 1.0f);
					ImGui::DragFloat("AO",               &mi.AO,               0.01f, 0.0f, 1.0f);
					ImGui::ColorEdit3("Emissive Color",  glm::value_ptr(mi.EmissiveColor));
					ImGui::DragFloat("Emissive Strength",&mi.EmissiveStrength,  0.1f,  0.0f, 100.0f);

					auto TexUI = [&](const char* label, Shared<Texture2D>& tex)
					{
						if (tex)
						{
							ImGui::Text(label);
							if (ImGui::IsItemHovered())
								ImGui::SetTooltip("Path: %s", tex->GetTexturePath().c_str());
							ImGui::SameLine();
							if (ImGui::Button(("X##" + std::string(label)).c_str()))
								tex = nullptr;
						}
						else
						{
							ImGui::Button(label, ImVec2(50.0f, 50.0f));
							if (ImGui::BeginDragDropTarget())
							{
								if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
								{
									std::filesystem::path p = std::string((const char*)payload->Data);
									tex = Texture2D::Create(p.string());
								}
								ImGui::EndDragDropTarget();
							}
						}
					};

					TexUI("Albedo",  component.MaterialInstance->AlbedoMap);
					TexUI("Normal",  component.MaterialInstance->NormalMap);
					TexUI("MetallicRoughness", component.MaterialInstance->MetallicRoughnessMap);
					TexUI("AO",      component.MaterialInstance->AOMap);
					TexUI("Emissive", component.MaterialInstance->EmissiveMap);
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

		DrawComponent<Rigidbody3DComponent>("Rigidbody 3D", entity, [](auto& component)
			{
				const char* bodyTypes[] = { "Static", "Dynamic", "Kinematic" };
				const char* current = bodyTypes[(int)component.Type];
				if (ImGui::BeginCombo("Body Type##3D", current))
				{
					for (int i = 0; i < IM_ARRAYSIZE(bodyTypes); i++)
					{
						bool sel = (current == bodyTypes[i]);
						if (ImGui::Selectable(bodyTypes[i], sel)) component.Type = (Rigidbody3DComponent::BodyType)i;
						if (sel) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				ImGui::DragFloat("Gravity Scale",   &component.GravityScale,   0.01f, 0.0f, 10.0f);
				ImGui::DragFloat("Linear Damping",  &component.LinearDamping,  0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Angular Damping", &component.AngularDamping, 0.01f, 0.0f, 1.0f);
				ImGui::Checkbox("Lock X", &component.FixedRotationX); ImGui::SameLine();
				ImGui::Checkbox("Lock Y", &component.FixedRotationY); ImGui::SameLine();
				ImGui::Checkbox("Lock Z", &component.FixedRotationZ);
			});

		DrawComponent<BoxCollider3DComponent>("Box Collider 3D", entity, [](auto& component)
			{
				ImGui::DragFloat3("Half Extents", glm::value_ptr(component.HalfExtents), 0.01f, 0.01f, 100.0f);
				ImGui::DragFloat3("Offset",       glm::value_ptr(component.Offset),      0.01f);
				ImGui::DragFloat("Friction",      &component.Friction,    0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Restitution",   &component.Restitution, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Density",       &component.Density,     1.0f,  0.01f, 10000.0f);
			});

		DrawComponent<SphereCollider3DComponent>("Sphere Collider 3D", entity, [](auto& component)
			{
				ImGui::DragFloat("Radius",       &component.Radius,      0.01f, 0.01f, 100.0f);
				ImGui::DragFloat3("Offset",       glm::value_ptr(component.Offset), 0.01f);
				ImGui::DragFloat("Friction",      &component.Friction,    0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Restitution",   &component.Restitution, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Density",       &component.Density,     1.0f,  0.01f, 10000.0f);
			});

		DrawComponent<CapsuleCollider3DComponent>("Capsule Collider 3D", entity, [](auto& component)
			{
				ImGui::DragFloat("Radius",       &component.Radius,      0.01f, 0.01f, 100.0f);
				ImGui::DragFloat("Half Height",  &component.HalfHeight,  0.01f, 0.01f, 100.0f);
				ImGui::DragFloat3("Offset",       glm::value_ptr(component.Offset), 0.01f);
				ImGui::DragFloat("Friction",      &component.Friction,    0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Restitution",   &component.Restitution, 0.01f, 0.0f, 1.0f);
				ImGui::DragFloat("Density",       &component.Density,     1.0f,  0.01f, 10000.0f);
			});

		DrawComponent<MeshComponent>("Mesh Renderer", entity, [](auto& component)
			{
				// ── Texture slot — thumbnail + drop target + browse button ─────
				// Shows a 48×48 thumbnail when a texture is assigned, or a drop
				// target + file-browse button when it's empty.  Works for both
				// drag-from-content-browser and native file-dialog browsing.
				auto TexSlot = [](const char* uid, const char* label, Shared<Texture2D>& tex)
				{
					constexpr float kThumb  = 48.0f;
					constexpr float kLabelW = 72.0f;

					ImGui::PushID(uid);

					// Label column
					ImGui::BeginGroup();
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted(label);
					ImGui::EndGroup();

					ImGui::SameLine(kLabelW);

					if (tex)
					{
						// Show thumbnail; hovering reveals full path
						ImGui::Image(
							reinterpret_cast<ImTextureID>(tex->GetImTextureID()),
							ImVec2(kThumb, kThumb));
						if (ImGui::IsItemHovered())
						{
							ImGui::BeginTooltip();
							ImGui::Image(
								reinterpret_cast<ImTextureID>(tex->GetImTextureID()),
								ImVec2(128.0f, 128.0f));
							ImGui::Text("%s", tex->GetTexturePath().c_str());
							ImGui::EndTooltip();
						}
						// Drop new texture on top of thumbnail
						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
								tex = Texture2D::Create(std::string((const char*)p->Data));
							ImGui::EndDragDropTarget();
						}
						ImGui::SameLine();
						if (ImGui::SmallButton("x"))
							tex = nullptr;
					}
					else
					{
						// Empty drop-target box
						ImGui::Button(("Drop " + std::string(label)).c_str(), ImVec2(kThumb * 2.0f, kThumb));
						if (ImGui::BeginDragDropTarget())
						{
							if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
								tex = Texture2D::Create(std::string((const char*)p->Data));
							ImGui::EndDragDropTarget();
						}
						// Browse button — opens a native file dialog
						ImGui::SameLine();
						if (ImGui::SmallButton("..."))
						{
							std::string path = FileDialogs::OpenFile(
								"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.hdr\0All Files\0*.*\0");
							if (!path.empty())
								tex = Texture2D::Create(path);
						}
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("Browse for %s texture", label);
					}

					ImGui::PopID();
				};

				// ── Imported model ────────────────────────────────────────
				if (component.ModelAsset)
				{
					ImGui::TextDisabled("%s", component.FilePath.c_str());
					ImGui::Text("SubMeshes: %zu", component.ModelAsset->Meshes.size());

					auto& mats = component.ModelAsset->Materials;
					if (!mats.empty() && ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
					{
						for (int i = 0; i < (int)mats.size(); ++i)
						{
							auto& mat = mats[i];
							if (!mat) continue;

							// Use the FBX material name if available, otherwise "Material N"
							const std::string& nodeName = mat->Name.empty()
								? ("Material " + std::to_string(i))
								: mat->Name;

							ImGui::PushID(i);
							bool open = ImGui::TreeNodeEx(nodeName.c_str(), 0);
							if (open)
							{
								ImGui::ColorEdit4("Albedo",   glm::value_ptr(mat->AlbedoColor));
								ImGui::DragFloat("Metallic",  &mat->Metallic,         0.01f, 0.0f, 1.0f);
								ImGui::DragFloat("Roughness", &mat->Roughness,        0.01f, 0.0f, 1.0f);
								ImGui::DragFloat("AO",        &mat->AO,               0.01f, 0.0f, 1.0f);
								ImGui::ColorEdit3("Emissive", glm::value_ptr(mat->EmissiveColor));
								ImGui::DragFloat("Emit Str",  &mat->EmissiveStrength, 0.1f,  0.0f, 100.0f);
								ImGui::Separator();
								// Blend mode selector
								static const char* blendModeNames[] = { "Opaque", "Masked", "Transparent", "Additive" };
								int blendIdx = static_cast<int>(mat->Blend);
								if (ImGui::Combo("Blend Mode", &blendIdx, blendModeNames, IM_ARRAYSIZE(blendModeNames)))
									mat->Blend = static_cast<BlendMode>(blendIdx);
								if (mat->Blend == BlendMode::Masked)
									ImGui::DragFloat("Alpha Cutoff", &mat->AlphaCutoff, 0.01f, 0.0f, 1.0f);
								ImGui::Checkbox("Two Sided", &mat->TwoSided);
								// Shading model selector
								static const char* shadingNames[] = { "PBR", "Unlit" };
								int shadingIdx = static_cast<int>(mat->Shading);
								if (ImGui::Combo("Shading", &shadingIdx, shadingNames, IM_ARRAYSIZE(shadingNames)))
									mat->Shading = static_cast<ShadingModel>(shadingIdx);
								ImGui::Separator();
								TexSlot("alb",  "Albedo",   mat->AlbedoMap);
								TexSlot("nrm",  "Normal",   mat->NormalMap);
								TexSlot("mr",   "Metallic", mat->MetallicRoughnessMap);
								TexSlot("ao",   "AO",       mat->AOMap);
								TexSlot("emis", "Emissive", mat->EmissiveMap);
								ImGui::TreePop();
							}
							ImGui::PopID();
						}
					}
				}
				// ── Primitive mesh ────────────────────────────────────────
				else
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

						auto& mi = *component.MaterialInstance;
						ImGui::ColorEdit4("Albedo##Mesh",         glm::value_ptr(mi.AlbedoColor));
						ImGui::DragFloat("Metallic##Mesh",        &mi.Metallic,         0.01f, 0.0f, 1.0f);
						ImGui::DragFloat("Roughness##Mesh",       &mi.Roughness,        0.01f, 0.0f, 1.0f);
						ImGui::DragFloat("AO##Mesh",              &mi.AO,               0.01f, 0.0f, 1.0f);
						ImGui::ColorEdit3("Emissive##Mesh",       glm::value_ptr(mi.EmissiveColor));
						ImGui::DragFloat("Emissive Str##Mesh",    &mi.EmissiveStrength, 0.1f,  0.0f, 100.0f);
						ImGui::Separator();
						// Blend mode selector
						static const char* blendModeNames[] = { "Opaque", "Masked", "Transparent", "Additive" };
						int blendIdx = static_cast<int>(mi.Blend);
						if (ImGui::Combo("Blend Mode##Mesh", &blendIdx, blendModeNames, IM_ARRAYSIZE(blendModeNames)))
							mi.Blend = static_cast<BlendMode>(blendIdx);
						if (mi.Blend == BlendMode::Masked)
							ImGui::DragFloat("Alpha Cutoff##Mesh", &mi.AlphaCutoff, 0.01f, 0.0f, 1.0f);
						ImGui::Checkbox("Two Sided##Mesh", &mi.TwoSided);
						// Shading model selector
						static const char* shadingNames[] = { "PBR", "Unlit" };
						int shadingIdx = static_cast<int>(mi.Shading);
						if (ImGui::Combo("Shading##Mesh", &shadingIdx, shadingNames, IM_ARRAYSIZE(shadingNames)))
							mi.Shading = static_cast<ShadingModel>(shadingIdx);
						ImGui::Separator();
						TexSlot("alb_m",  "Albedo",   component.MaterialInstance->AlbedoMap);
						TexSlot("nrm_m",  "Normal",   component.MaterialInstance->NormalMap);
						TexSlot("mr_m",   "Metallic", component.MaterialInstance->MetallicRoughnessMap);
						TexSlot("ao_m",   "AO",       component.MaterialInstance->AOMap);
						TexSlot("emis_m", "Emissive", component.MaterialInstance->EmissiveMap);
					}
				}
			});

		// ── Mesh LOD ──────────────────────────────────────────────────────────────
		DrawComponent<MeshLODComponent>("Mesh LOD", entity, [](auto& lod)
		{
			ImGui::Checkbox("Active", &lod.Active);
			ImGui::Text("LOD Levels (%d)", (int)lod.Levels.size());
			ImGui::Separator();
			for (int i = 0; i < (int)lod.Levels.size(); ++i)
			{
				auto& lv = lod.Levels[i];
				ImGui::PushID(i);
				ImGui::Text("LOD %d", i);
				ImGui::SameLine();
				ImGui::DragFloat("Max Distance", &lv.MaxDistance, 1.0f, 0.1f, 5000.0f);
				ImGui::Text("  Model: %s", lv.FilePath.empty() ? "(none)" : lv.FilePath.c_str());
				if (ImGui::Button("Remove")) { lod.Levels.erase(lod.Levels.begin() + i); ImGui::PopID(); break; }
				ImGui::Separator();
				ImGui::PopID();
			}
			if (ImGui::Button("Add LOD Level"))
				lod.Levels.push_back({});
		});

		// ── Spring-Arm ────────────────────────────────────────────────────────────
		DrawComponent<SpringArmComponent>("Spring Arm", entity, [](auto& arm)
		{
			ImGui::DragFloat("Arm Length",      &arm.ArmLength,        0.1f, 0.1f, 50.0f);
			ImGui::DragFloat3("Socket Offset",  glm::value_ptr(arm.SocketOffset), 0.05f);
			ImGui::DragFloat3("Pivot Offset",   glm::value_ptr(arm.PivotOffset),  0.05f);
			ImGui::DragFloat("Pitch (deg)",     &arm.Pitch,            0.5f, -89.0f, 89.0f);
			ImGui::DragFloat("Yaw Offset",      &arm.Yaw,              0.5f, -180.0f, 180.0f);
			ImGui::Separator();
			ImGui::Checkbox("Enable Lag",       &arm.EnableLag);
			if (arm.EnableLag)
				ImGui::DragFloat("Lag Speed",   &arm.PositionLagSpeed, 0.5f, 0.5f, 30.0f);
			ImGui::Separator();
			ImGui::Text("Camera UUID: %llu", (unsigned long long)arm.TargetCameraUUID);
		});

		// ── Animator Component ────────────────────────────────────────────────────
		DrawComponent<AnimatorComponent>("Animator", entity, [&](auto& anim)
		{
			// Populate SkelData from MeshComponent if not already set
			if (!anim.SkelData && entity.HasComponent<MeshComponent>())
			{
				auto& mc = entity.GetComponent<MeshComponent>();
				if (mc.ModelAsset && mc.ModelAsset->SkelData)
					anim.SkelData = mc.ModelAsset->SkelData;
			}

			if (!anim.SkelData)
			{
				ImGui::TextColored(ImVec4(1,1,0,1), "No skeleton found on Model.");
				ImGui::TextWrapped("Load an FBX/glTF with bones in the Mesh Component first.");
				return;
			}

			// Clip selector
			auto& clips = anim.SkelData->Clips;
			if (!clips.empty())
			{
				const char* preview = clips[anim.CurrentClipIndex].Name.c_str();
				if (ImGui::BeginCombo("Clip", preview))
				{
					for (int i = 0; i < (int)clips.size(); ++i)
					{
						bool selected = (i == anim.CurrentClipIndex);
						if (ImGui::Selectable(clips[i].Name.c_str(), selected))
						{
							anim.CurrentClipIndex = i;
							anim.CurrentTime      = 0.0f;
						}
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}
				const auto& clip = clips[anim.CurrentClipIndex];
				float durationSec = clip.DurationSeconds();
				float currentSec  = clip.TicksPerSec > 0.0f ? anim.CurrentTime / clip.TicksPerSec : 0.0f;
				ImGui::ProgressBar(durationSec > 0.0f ? currentSec / durationSec : 0.0f,
				                   ImVec2(-1, 0),
				                   (std::to_string((int)currentSec) + "s / " +
				                    std::to_string((int)durationSec) + "s").c_str());
			}
			else
			{
				ImGui::TextColored(ImVec4(1,0.5f,0,1), "No animation clips found in file.");
			}

			ImGui::Separator();
			ImGui::Checkbox("Playing", &anim.Playing);
			ImGui::SameLine();
			ImGui::Checkbox("Loop",    &anim.Loop);
			ImGui::DragFloat("Speed",  &anim.SpeedScale, 0.01f, 0.0f, 10.0f);
			if (ImGui::Button("Rewind##anim"))
				anim.CurrentTime = 0.0f;

			ImGui::Separator();
			ImGui::Text("Skeleton: %d bones", anim.SkelData->Skel ? anim.SkelData->Skel->NumBones : 0);
		});

		// ── Foliage Component ─────────────────────────────────────────────────────
		DrawComponent<FoliageComponent>("Foliage (GPU Instanced)", entity, [](auto& fc)
		{
			// Model file
			char fbuf[512] = {};
			std::strncpy(fbuf, fc.FilePath.c_str(), sizeof(fbuf) - 1);
			if (ImGui::InputText("Model File##fc", fbuf, sizeof(fbuf)))
				fc.FilePath = fbuf;
			ImGui::SameLine();
			if (ImGui::Button("...##fc"))
			{
				std::string r = Blu::FileDialogs::OpenFile("Model Files\0*.fbx;*.obj;*.gltf;*.glb\0All\0*.*\0");
				if (!r.empty())
				{
					fc.FilePath  = r;
					fc.ModelAsset = ModelLoader::Load(r);
				}
			}
			if (ImGui::Button("Reload Model##fc") && !fc.FilePath.empty())
				fc.ModelAsset = ModelLoader::Load(fc.FilePath);

			ImGui::Text("Instances: %d", (int)fc.Transforms.size());

			// Scatter tool
			ImGui::Separator();
			ImGui::Text("Scatter");
			static int   scatterCount    = 50;
			static float scatterRadius   = 20.0f;
			static float scatterMinScale = 0.8f;
			static float scatterMaxScale = 1.2f;
			ImGui::DragInt  ("Count##scatter",    &scatterCount,    1,    1,    10000);
			ImGui::DragFloat("Radius##scatter",   &scatterRadius,   0.5f, 1.0f, 500.0f);
			ImGui::DragFloat("Min Scale##scatter", &scatterMinScale, 0.01f, 0.01f, 10.0f);
			ImGui::DragFloat("Max Scale##scatter", &scatterMaxScale, 0.01f, 0.01f, 10.0f);
			if (ImGui::Button("Scatter##do"))
			{
				fc.Transforms.clear();
				fc.Transforms.reserve(scatterCount);
				std::mt19937 rng(42);
				std::uniform_real_distribution<float> uniR(-scatterRadius, scatterRadius);
				std::uniform_real_distribution<float> uniRot(0.0f, glm::two_pi<float>());
				std::uniform_real_distribution<float> uniSc(scatterMinScale, scatterMaxScale);
				for (int i = 0; i < scatterCount; ++i)
				{
					float x = uniR(rng), z = uniR(rng);
					float scale = uniSc(rng);
					float rot   = uniRot(rng);
					glm::mat4 t = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.0f, z))
					            * glm::rotate(glm::mat4(1.0f), rot, glm::vec3(0, 1, 0))
					            * glm::scale(glm::mat4(1.0f), glm::vec3(scale));
					fc.Transforms.push_back(t);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear##fc"))
				fc.Transforms.clear();

			// Wind
			ImGui::Separator();
			ImGui::Checkbox("Wind Enabled", &fc.WindEnabled);
			if (fc.WindEnabled)
			{
				ImGui::DragFloat("Wind Strength",   &fc.WindStrength,   0.001f, 0.0f, 1.0f);
				ImGui::DragFloat("Wind Frequency",  &fc.WindFrequency,  0.05f,  0.1f, 20.0f);
				ImGui::DragFloat3("Wind Direction", glm::value_ptr(fc.WindDirection), 0.01f);
			}
		});

		// ── Audio Source ──────────────────────────────────────────────────────────
		DrawComponent<AudioSourceComponent>("Audio Source", entity, [](auto& asc)
		{
			// File path
			char buf[512] = {};
			std::strncpy(buf, asc.FilePath.c_str(), sizeof(buf) - 1);
			if (ImGui::InputText("File", buf, sizeof(buf)))
				asc.FilePath = buf;
			ImGui::SameLine();
			if (ImGui::Button("...##audio"))
			{
				// Simple open-file via platform dialog if available
				std::string result = Blu::FileDialogs::OpenFile("Audio Files\0*.wav;*.mp3;*.ogg;*.flac\0All Files\0*.*\0");
				if (!result.empty())
					asc.FilePath = result;
			}

			ImGui::SliderFloat("Volume",      &asc.Volume,      0.0f, 1.0f);
			ImGui::SliderFloat("Pitch",       &asc.Pitch,       0.1f, 4.0f);
			ImGui::Checkbox   ("Loop",        &asc.Loop);
			ImGui::Checkbox   ("Play On Start", &asc.PlayOnStart);
			ImGui::Separator();
			ImGui::Checkbox   ("Spatial 3D",  &asc.Spatial);
			if (asc.Spatial)
			{
				ImGui::DragFloat("Min Distance", &asc.MinDistance, 0.1f, 0.01f, 1000.0f);
				ImGui::DragFloat("Max Distance", &asc.MaxDistance, 1.0f, asc.MinDistance, 2000.0f);
			}
			// Runtime controls (only meaningful during Play)
			if (asc._RuntimeHandle != kInvalidSound)
			{
				ImGui::Separator();
				ImGui::Text("Runtime handle: %u", asc._RuntimeHandle);
				bool playing = AudioEngine::Get().IsPlaying(asc._RuntimeHandle);
				ImGui::Text("State: %s", playing ? "Playing" : "Stopped/Paused");
				if (ImGui::Button("Play##asc"))  AudioEngine::Get().Play (asc._RuntimeHandle);
				ImGui::SameLine();
				if (ImGui::Button("Pause##asc")) AudioEngine::Get().Pause(asc._RuntimeHandle);
				ImGui::SameLine();
				if (ImGui::Button("Stop##asc"))  AudioEngine::Get().Stop (asc._RuntimeHandle);
			}
		});

		// ── Native Script ─────────────────────────────────────────────────────────
		DrawComponent<NativeScriptComponent>("Native Script", entity, [](auto& nsc)
		{
			const auto& registry = ActorRegistry::Get().GetAll();

			const char* preview = nsc.ClassName.empty() ? "(None)" : nsc.ClassName.c_str();
			if (ImGui::BeginCombo("Class##nsc", preview))
			{
				if (ImGui::Selectable("(None)", nsc.ClassName.empty()))
					nsc.ClassName.clear();
				for (auto& [name, _] : registry)
				{
					bool selected = (nsc.ClassName == name);
					if (ImGui::Selectable(name.c_str(), selected))
						nsc.ClassName = name;
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::Spacing();
			if (nsc.Instance)
				ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Running");
			else if (!nsc.ClassName.empty())
				ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Will bind \"%s\" on Play", nsc.ClassName.c_str());
			else
				ImGui::TextDisabled("No class selected.");
		});

		float extraSpace = 200.0f;  // Extra space at the end in pixels
		ImGui::Dummy(ImVec2(0.0f, extraSpace));

		ImGui::EndChild();
		
			
		
	}
}