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
#include "Blu/Utils/AssetPath.h"
#include <imgui_internal.h>
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <random>
#include "Blu/LightSystem/LightManager.h"

namespace Blu
{
	static std::string GetAssetOwnerName(Entity entity)
	{
		if (entity && entity.HasComponent<TagComponent>())
			return entity.GetComponent<TagComponent>().Tag;

		return "Entity";
	}

	static Shared<Texture2D> LoadImportedTexture(const std::filesystem::path& path, const std::string& ownerName)
	{
		if (path.empty())
			return nullptr;

		std::string importedPath = AssetPath::ImportTexturePath(path, ownerName);
		return Texture2D::Create(AssetPath::ResolvePath(importedPath).string());
	}

	static void DrawNativePropertyOverride(ActorComponent& actorComponent, const NativePropertyDescriptor& property)
	{
		auto override = actorComponent.Overrides.find(property.Name);
		bool enabled = override != actorComponent.Overrides.end();
		if (ImGui::Checkbox(("##override_" + property.Name).c_str(), &enabled))
		{
			if (enabled)
				actorComponent.Overrides[property.Name] = property.DefaultValue;
			else
				actorComponent.Overrides.erase(property.Name);
		}
		ImGui::SameLine();
		ImGui::TextUnformatted(property.Name.c_str());
		if (!enabled)
			return;

		ImGui::Indent();
		NativePropertyValue& value = actorComponent.Overrides[property.Name];
		const std::string controlID = "##value_" + property.Name;
		switch (property.Type)
		{
			case NativePropertyType::Bool:
				ImGui::Checkbox(controlID.c_str(), &std::get<bool>(value));
				break;
			case NativePropertyType::Integer:
				ImGui::InputScalar(controlID.c_str(), ImGuiDataType_S64, &std::get<int64_t>(value));
				break;
			case NativePropertyType::Float:
				ImGui::DragFloat(controlID.c_str(), &std::get<float>(value), 0.1f);
				break;
			case NativePropertyType::String:
			{
				char buffer[256] = {};
				strncpy_s(buffer, std::get<std::string>(value).c_str(), sizeof(buffer) - 1);
				if (ImGui::InputText(controlID.c_str(), buffer, sizeof(buffer)))
					std::get<std::string>(value) = buffer;
				break;
			}
			case NativePropertyType::Vec2:
				ImGui::DragFloat2(controlID.c_str(), glm::value_ptr(std::get<glm::vec2>(value)), 0.1f);
				break;
			case NativePropertyType::Vec3:
				ImGui::DragFloat3(controlID.c_str(), glm::value_ptr(std::get<glm::vec3>(value)), 0.1f);
				break;
			case NativePropertyType::Vec4:
				ImGui::DragFloat4(controlID.c_str(), glm::value_ptr(std::get<glm::vec4>(value)), 0.1f);
				break;
			case NativePropertyType::AssetReference:
			{
				char buffer[256] = {};
				strncpy_s(buffer, std::get<NativeAssetReference>(value).Path.c_str(), sizeof(buffer) - 1);
				if (ImGui::InputText(controlID.c_str(), buffer, sizeof(buffer)))
					std::get<NativeAssetReference>(value).Path = buffer;
				break;
			}
			case NativePropertyType::EntityReference:
			{
				auto& entityReference = std::get<NativeEntityReference>(value);
				ImGui::InputScalar(controlID.c_str(), ImGuiDataType_U64, &entityReference.UUID);
				break;
			}
		}
		ImGui::Unindent();
	}

	static uint32_t CountModelCollisionTriangles(const Shared<Model>& model)
	{
		if (!model)
			return 0;

		uint32_t triangleCount = 0;
		for (const auto& submesh : model->Meshes)
			triangleCount += (uint32_t)(submesh.Indices.size() / 3);
		return triangleCount;
	}

	static bool EntityHasLight(Entity entity)
	{
		return entity.HasComponent<PointLightComponent>() ||
		       entity.HasComponent<DirectionalLightComponent>() ||
		       entity.HasComponent<SpotLightComponent>();
	}

	static bool EntityHasPhysics(Entity entity)
	{
		return entity.HasComponent<Rigidbody3DComponent>() ||
		       entity.HasComponent<BoxCollider3DComponent>() ||
		       entity.HasComponent<SphereCollider3DComponent>() ||
		       entity.HasComponent<CapsuleCollider3DComponent>() ||
		       entity.HasComponent<MeshCollider3DComponent>() ||
		       entity.HasComponent<Rigidbody2DComponent>() ||
		       entity.HasComponent<BoxCollider2DComponent>() ||
		       entity.HasComponent<CircleCollider2DComponent>();
	}

	static bool EntityHasAuthoringWarning(Entity entity)
	{
		if (entity.HasComponent<MeshComponent>())
		{
			auto& mesh = entity.GetComponent<MeshComponent>();
			if (!mesh.FilePath.empty() && !mesh.ModelAsset)
				return true;
		}

		if (entity.HasComponent<Rigidbody3DComponent>() &&
		    !entity.HasComponent<BoxCollider3DComponent>() &&
		    !entity.HasComponent<SphereCollider3DComponent>() &&
		    !entity.HasComponent<CapsuleCollider3DComponent>() &&
		    !entity.HasComponent<MeshCollider3DComponent>())
			return true;

		if (entity.HasComponent<MeshCollider3DComponent>())
		{
			auto& collider = entity.GetComponent<MeshCollider3DComponent>();
			if (!entity.HasComponent<MeshComponent>() ||
			    !entity.GetComponent<MeshComponent>().ModelAsset ||
			    (entity.HasComponent<Rigidbody3DComponent>() &&
			     entity.GetComponent<Rigidbody3DComponent>().Type != Rigidbody3DComponent::BodyType::Static) ||
			    !collider.RuntimeStatus.empty())
				return true;
		}

		if (entity.HasComponent<CharacterControllerComponent>() &&
		    !entity.HasComponent<CapsuleCollider3DComponent>())
			return true;

		return false;
	}

	static const char* EntityIcon(Entity entity)
	{
		if (entity.HasComponent<CharacterControllerComponent>() || entity.HasComponent<ActorComponent>())
			return "[P]";
		if (entity.HasComponent<CameraComponent>())
			return "[C]";
		if (EntityHasLight(entity))
			return "[L]";
		if (entity.HasComponent<MeshComponent>() || entity.HasComponent<SpriteRendererComponent>() || entity.HasComponent<CircleRendererComponent>())
			return "[M]";
		if (EntityHasPhysics(entity))
			return "[X]";
		return "[E]";
	}

	static void DrawBadge(const char* label, const ImVec4& color)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, color);
		ImGui::TextUnformatted(label);
		ImGui::PopStyleColor();
	}

	SceneHierarchyPanel::SceneHierarchyPanel(const Shared<Scene>& scene)
	{
		SetContext(scene);
	}
	void SceneHierarchyPanel::SetContext(const Shared<Scene>& scene)
	{
		m_Context = scene;
		m_SelectedEntity = {};
		m_LastEntityCount = (size_t)-1; // re-baseline so a scene swap doesn't read as a dirty edit
		m_OutlinerFolders.clear();      // per-scene; seed from the scene's saved folders (incl. empty ones)
		if (scene)
			m_OutlinerFolders = scene->m_EditorFolders;
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

			uint32_t actorCount = 0;
			uint32_t visibleCount = 0;
			uint32_t warningCount = 0;
			m_Context->m_Registry.each([&](auto entityID)
			{
				Entity entity{ entityID, m_Context.get() };
				actorCount++;
				if (PassesActiveFilters(entity))
					visibleCount++;
				if (EntityHasAuthoringWarning(entity))
					warningCount++;
			});
			// Entity count changed since last frame (create/delete from any source) → scene dirty.
			// m_LastEntityCount == -1 means "just (re)baselined" — record without flagging.
			if (m_LastEntityCount != (size_t)-1 && (size_t)actorCount != m_LastEntityCount)
				NotifySceneModified();
			m_LastEntityCount = actorCount;

			ImGui::TextDisabled("%u actors | %u visible | %s", actorCount, visibleCount,
			                    m_SelectedEntity ? "1 selected" : "0 selected");
			if (warningCount > 0)
			{
				ImGui::SameLine();
				DrawBadge(("WARN " + std::to_string(warningCount)).c_str(), ImVec4(1.0f, 0.65f, 0.2f, 1.0f));
			}
			ImGui::Separator();

			// ---- Entity list (grouped into Outliner folders) ----
			std::map<std::string, std::vector<Entity>> outlinerFoldered;
			std::vector<Entity> outlinerRoots;
			m_Context->m_Registry.each([&](auto entityID)
			{
				Entity entity{ entityID, m_Context.get() };
				if (!PassesActiveFilters(entity))
					return;

				// Filter by search string (case-insensitive substring match).
				if (m_SearchBuffer[0] != '\0')
				{
					const auto& tag = entity.GetComponent<TagComponent>().Tag;
					auto it = std::search(
						tag.begin(), tag.end(),
						m_SearchBuffer, m_SearchBuffer + strlen(m_SearchBuffer),
						[](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); });
					if (it == tag.end())
						return; // skip non-matching entities
				}

				std::string folder;
				if (entity.HasComponent<FolderComponent>())
					folder = entity.GetComponent<FolderComponent>().Path;
				if (folder.empty())
					outlinerRoots.push_back(entity);
				else
				{
					outlinerFoldered[folder].push_back(entity);
					m_OutlinerFolders.insert(folder);
				}
			});

			// Re-file a dropped entity into a folder ("" = move to the Outliner root).
			auto acceptEntityDrop = [&](const std::string& folder)
			{
				if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("OUTLINER_ENTITY"))
				{
					UUID id = *static_cast<const UUID*>(p->Data);
					Entity dropped = m_Context->GetEntityByUUID(id);
					if (dropped)
					{
						if (folder.empty())
						{
							if (dropped.HasComponent<FolderComponent>())
								dropped.RemoveComponent<FolderComponent>();
						}
						else
						{
							if (!dropped.HasComponent<FolderComponent>())
								dropped.AddComponent<FolderComponent>();
							dropped.GetComponent<FolderComponent>().Path = folder;
						}
						NotifySceneModified();
					}
				}
			};

			// Folder nodes (alphabetical) — collapsible, drag-drop targets, gold-tinted.
			std::string folderToDelete;
			for (const auto& folderName : m_OutlinerFolders)
			{
				ImGui::PushID(folderName.c_str());
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.82f, 0.35f, 1.0f));
				const bool folderOpen = ImGui::TreeNodeEx(folderName.c_str(),
					ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen,
					"%s/", folderName.c_str());
				ImGui::PopStyleColor();
				if (ImGui::BeginDragDropTarget()) { acceptEntityDrop(folderName); ImGui::EndDragDropTarget(); }
				if (ImGui::BeginPopupContextItem("##folderctx"))
				{
					if (ImGui::MenuItem("Delete Folder (move entities to root)")) folderToDelete = folderName;
					ImGui::EndPopup();
				}
				if (folderOpen)
				{
					auto it = outlinerFoldered.find(folderName);
					if (it != outlinerFoldered.end())
						for (auto& e : it->second) DrawEntityNode(e);
					ImGui::TreePop();
				}
				ImGui::PopID();
			}

			// Root-level entities.
			for (auto& e : outlinerRoots)
				DrawEntityNode(e);

			// Drop-to-root zone: the empty space below accepts entity drops to un-folder them.
			{
				const float remaining = ImGui::GetContentRegionAvail().y;
				ImGui::Dummy(ImVec2(-1.0f, remaining > 12.0f ? remaining : 12.0f));
				if (ImGui::BeginDragDropTarget()) { acceptEntityDrop(""); ImGui::EndDragDropTarget(); }
			}

			if (!folderToDelete.empty())
			{
				auto it = outlinerFoldered.find(folderToDelete);
				if (it != outlinerFoldered.end())
					for (auto& e : it->second)
						if (e.HasComponent<FolderComponent>()) e.RemoveComponent<FolderComponent>();
				m_OutlinerFolders.erase(folderToDelete);
				NotifySceneModified();
			}

			// Mirror the folder set onto the scene so it persists (incl. empty folders).
			if (m_Context)
				m_Context->m_EditorFolders = m_OutlinerFolders;

			if (!m_EntityHovered)
			{
				if (ImGui::BeginPopupContextWindow())
				{
					if (ImGui::MenuItem("New Folder"))
					{
						std::string name = "Folder";
						for (int n = 1; m_OutlinerFolders.count(name); ++n)
							name = "Folder" + std::to_string(n);
						m_OutlinerFolders.insert(name);
					}
					ImGui::Separator();
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
							mc.Primitive = MeshComponent::PrimitiveType::Cube;
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

	bool SceneHierarchyPanel::PassesActiveFilters(Entity entity) const
	{
		const bool isCharacter = entity.HasComponent<CharacterControllerComponent>();
		const bool isScript = entity.HasComponent<ActorComponent>();
		const bool isCamera = entity.HasComponent<CameraComponent>();
		const bool isLight = EntityHasLight(entity);
		const bool isMesh = entity.HasComponent<MeshComponent>() || entity.HasComponent<SpriteRendererComponent>() || entity.HasComponent<CircleRendererComponent>();
		const bool isPhysics = EntityHasPhysics(entity);

		if (isCharacter && m_FilterCharacters) return true;
		if (isScript && m_FilterScripts) return true;
		if (isCamera && m_FilterCameras) return true;
		if (isLight && m_FilterLights) return true;
		if (isMesh && m_FilterMeshes) return true;
		if (isPhysics && m_FilterPhysics) return true;
		if (!isCharacter && !isScript && !isCamera && !isLight && !isMesh && !isPhysics)
			return m_FilterOther;
		return false;
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity)
	{
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		// In-place rename (F2-style): replace the tree node with an InputText while this
		// entity is being renamed from the Outliner context menu.
		if (m_RenamingEntity == entity)
		{
			char buffer[256];
			strcpy_s(buffer, sizeof(buffer), tag.c_str());
			if (m_RenameRequestFocus)
			{
				ImGui::SetKeyboardFocusHere();
				m_RenameRequestFocus = false;
			}
			ImGui::SetNextItemWidth(-1.0f);
			const bool committed = ImGui::InputText("##RenameEntity", buffer, sizeof(buffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
			// Commit on Enter or when focus leaves the field (click away); ignore empty names.
			if (committed || ImGui::IsItemDeactivated())
			{
				if (buffer[0] != '\0' && tag != buffer)
				{
					tag = buffer;
					NotifySceneModified();
				}
				m_RenamingEntity = {};
			}
			return;
		}

		ImGuiTreeNodeFlags flags = ((m_SelectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0)|ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;
		std::string label = std::string(EntityIcon(entity)) + " " + tag;
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", label.c_str());
		// Stable per-entity popup id, evaluated against the tree-node row right here.
		// This decouples the right-click trigger from the *last* submitted item: an
		// authoring WARN badge (drawn below) is ID-less, and BeginPopupContextItem()
		// with no str_id would fall back to that 0 id and trip IM_ASSERT(id != 0)
		// (imgui.cpp:11221) — the crash seen when generating mesh collision.
		std::string entityCtxId = "##entityctx" + std::to_string((uint32_t)entity);
		ImGui::OpenPopupOnItemClick(entityCtxId.c_str(), ImGuiPopupFlags_MouseButtonRight);
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
		{
			UUID dragId = entity.GetUUID();
			ImGui::SetDragDropPayload("OUTLINER_ENTITY", &dragId, sizeof(UUID));
			ImGui::TextUnformatted(tag.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::IsItemClicked())
		{
			m_SelectedEntity = entity;

		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0) && m_OpenActorEditorCallback)
			m_OpenActorEditorCallback(entity);
		m_EntityHovered |= ImGui::IsItemHovered();
		if (EntityHasAuthoringWarning(entity))
		{
			ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 44.0f);
			DrawBadge("WARN", ImVec4(1.0f, 0.65f, 0.2f, 1.0f));
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("Entity has an authoring/runtime setup warning");
		}

		bool entityDeleted = false;

		if (ImGui::BeginPopup(entityCtxId.c_str()))
		{
			// Target the right-clicked entity (not whatever was previously selected) so
			// Rename/Delete act on the row under the cursor.
			m_SelectedEntity = entity;
			if (ImGui::MenuItem("Open Actor Editor"))
			{
				if (m_OpenActorEditorCallback)
					m_OpenActorEditorCallback(entity);
			}
			if (ImGui::MenuItem("Rename"))
			{
				m_RenamingEntity = entity;
				m_RenameRequestFocus = true;
			}
			ImGui::Separator();
			std::string selectedEntityName = std::format("Delete {}", entity.GetComponent<TagComponent>().Tag.c_str());
			if (ImGui::MenuItem(selectedEntityName.c_str()))
			{
				// Prefer the host's confirmation modal; fall back to immediate delete
				// if no callback is wired (e.g. panel used standalone).
				if (m_RequestDeleteCallback)
					m_RequestDeleteCallback(entity);
				else
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
	static char s_ComponentSearchBuffer[128] = {};

	static bool FilterMatches(const char* text, const char* filter)
	{
		if (!filter || filter[0] == '\0')
			return true;
		std::string haystack = text ? text : "";
		std::string needle = filter;
		for (char& c : haystack) c = (char)std::tolower((unsigned char)c);
		for (char& c : needle) c = (char)std::tolower((unsigned char)c);
		return haystack.find(needle) != std::string::npos;
	}

	// Set by the inspector property helpers when a value actually changes (before/after
	// diff). DrawEntityComponents consumes it once per frame to flag the scene dirty.
	static bool s_InspectorEdited = false;

	template<typename ComponentType, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, UIFunction function)
	{
		if (entity.HasComponent<ComponentType>())
		{
			if (!FilterMatches(name.c_str(), s_ComponentSearchBuffer))
				return;
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
		const glm::vec3 before = values; // dirty-detect: any drag or reset-button change
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
		if (values != before) s_InspectorEdited = true;
	}

	static void BeginPropertyGrid(const char* id, float labelWidth = 116.0f)
	{
		ImGui::PushID(id);
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, labelWidth);
	}

	static void EndPropertyGrid()
	{
		ImGui::Columns(1);
		ImGui::PopID();
	}

	static void PropertyLabel(const char* label)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::NextColumn();
		ImGui::SetNextItemWidth(-FLT_MIN);
	}

	static void DrawFloatProperty(const char* label, float& value, float speed = 0.01f, float min = 0.0f, float max = 0.0f, const char* fmt = "%.3f")
	{
		ImGui::PushID(label);
		PropertyLabel(label);
		if (ImGui::DragFloat("##value", &value, speed, min, max, fmt)) s_InspectorEdited = true;
		ImGui::NextColumn();
		ImGui::PopID();
	}

	static void DrawIntProperty(const char* label, int& value, int min = 0, int max = 0)
	{
		ImGui::PushID(label);
		PropertyLabel(label);
		const bool changed = (min != max)
			? ImGui::SliderInt("##value", &value, min, max)
			: ImGui::InputInt("##value", &value);
		if (changed) s_InspectorEdited = true;
		ImGui::NextColumn();
		ImGui::PopID();
	}

	static void DrawBoolProperty(const char* label, bool& value)
	{
		ImGui::PushID(label);
		PropertyLabel(label);
		if (ImGui::Checkbox("##value", &value)) s_InspectorEdited = true;
		ImGui::NextColumn();
		ImGui::PopID();
	}

	static bool DrawTextProperty(const char* label, char* buffer, size_t bufferSize)
	{
		ImGui::PushID(label);
		PropertyLabel(label);
		ImGui::SetNextItemWidth(-1.0f);
		bool changed = ImGui::InputText("##value", buffer, bufferSize);
		ImGui::NextColumn();
		ImGui::PopID();
		return changed;
	}

	static void DrawVec2Property(const char* label, glm::vec2& value, float speed = 0.01f, float min = 0.0f, float max = 0.0f)
	{
		ImGui::PushID(label);
		PropertyLabel(label);
		if (ImGui::DragFloat2("##value", glm::value_ptr(value), speed, min, max, "%.3f")) s_InspectorEdited = true;
		ImGui::NextColumn();
		ImGui::PopID();
	}

	static void DrawVec3Property(const char* label, glm::vec3& value, float speed = 0.01f, float min = 0.0f, float max = 0.0f)
	{
		ImGui::PushID(label);
		PropertyLabel(label);
		if (ImGui::DragFloat3("##value", glm::value_ptr(value), speed, min, max, "%.3f")) s_InspectorEdited = true;
		ImGui::NextColumn();
		ImGui::PopID();
	}

	static void DrawColor4Property(const char* label, glm::vec4& value)
	{
		ImGui::PushID(label);
		PropertyLabel(label);
		if (ImGui::ColorEdit4("##value", glm::value_ptr(value), ImGuiColorEditFlags_NoInputs)) s_InspectorEdited = true;
		ImGui::NextColumn();
		ImGui::PopID();
	}

	static bool DrawComboProperty(const char* label, int& value, const char* const* items, int itemCount)
	{
		ImGui::PushID(label);
		PropertyLabel(label);
		bool changed = ImGui::Combo("##value", &value, items, itemCount);
		ImGui::NextColumn();
		ImGui::PopID();
		return changed;
	}

	static void DrawReadOnlyProperty(const char* label, const char* value)
	{
		ImGui::PushID(label);
		PropertyLabel(label);
		ImGui::TextDisabled("%s", value);
		ImGui::NextColumn();
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

	// Set by AddComponentSearchResult when a component is added; DrawEntityComponents
	// consumes it to flag the scene dirty (entity count is unchanged on add).
	static bool s_ComponentMenuModified = false;

	// Colour-code the add-component menu by category so it's faster to scan.
	static ImVec4 ComponentCategoryColor(const char* category)
	{
		std::string c = category ? category : "";
		if (c.find("Light") != std::string::npos || c.find("Camera") != std::string::npos)
			return ImVec4(0.95f, 0.80f, 0.30f, 1.0f); // amber  — camera/lighting
		if (c.find("Render") != std::string::npos || c.find("Mesh") != std::string::npos)
			return ImVec4(0.40f, 0.75f, 1.00f, 1.0f); // blue   — rendering
		if (c.find("Phys") != std::string::npos || c.find("Collid") != std::string::npos)
			return ImVec4(0.45f, 0.85f, 0.45f, 1.0f); // green  — physics
		if (c.find("Audio") != std::string::npos)
			return ImVec4(0.85f, 0.55f, 0.95f, 1.0f); // purple — audio
		if (c.find("Game") != std::string::npos || c.find("Script") != std::string::npos || c.find("Actor") != std::string::npos)
			return ImVec4(1.00f, 0.55f, 0.45f, 1.0f); // coral  — gameplay
		return ImVec4(0.60f, 0.60f, 0.60f, 1.0f);     // grey   — other
	}

	template <typename T, typename SetupFn>
	static void AddComponentSearchResult(Entity entity, const char* category, const char* label, const char* warning, SetupFn setup)
	{
		if (entity.HasComponent<T>() || !FilterMatches(label, s_ComponentSearchBuffer))
			return;
		// Rendered under a collapsible category header, so show just an indented label
		// tinted by category colour (the header names the category).
		ImGui::Indent(10.0f);
		ImGui::PushStyleColor(ImGuiCol_Text, ComponentCategoryColor(category));
		const bool clicked = ImGui::Selectable(label);
		ImGui::PopStyleColor();
		if (clicked)
		{
			auto& component = entity.AddComponent<T>();
			setup(component);
			s_ComponentMenuModified = true;
			ImGui::CloseCurrentPopup();
		}
		if (warning && warning[0] && ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", warning);
		ImGui::Unindent(10.0f);
	}

	void SceneHierarchyPanel::DrawEntityComponents(Entity entity)
	{
		m_SelectedEntity = entity;
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
		ImGui::PopItemWidth();
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputTextWithHint("##ComponentFilter", "Search components/properties...", s_ComponentSearchBuffer, IM_ARRAYSIZE(s_ComponentSearchBuffer));
		if (ImGui::BeginPopup("AddComponent"))
		{
			ImGui::InputTextWithHint("##ComponentSearch", "Search components...", s_ComponentSearchBuffer, IM_ARRAYSIZE(s_ComponentSearchBuffer));
			ImGui::SeparatorText("Palette");
			// When a search filter is active, show matching rows flat (no headers); otherwise
			// group under collapsible category headers. AddComponentSearchResult self-hides rows
			// the entity already has or that don't match the filter.
			const bool cmpFiltering = s_ComponentSearchBuffer[0] != '\0';
			auto cmpCategory = [&](const char* name) {
				return cmpFiltering || ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen);
			};
			if (cmpCategory("Camera / Lighting"))
			{
				AddComponentSearchResult<CameraComponent>(entity, "Camera/Lighting", "Camera", "", [](auto&) {});
				AddComponentSearchResult<PointLightComponent>(entity, "Camera/Lighting", "Point Light", "", [](auto&) {});
				AddComponentSearchResult<DirectionalLightComponent>(entity, "Camera/Lighting", "Directional Light", "", [](auto&) {});
				AddComponentSearchResult<SpotLightComponent>(entity, "Camera/Lighting", "Spot Light", "", [](auto&) {});
				AddComponentSearchResult<FogVolumeComponent>(entity, "Camera/Lighting", "Fog Volume", "", [](auto&) {});
			}
			if (cmpCategory("Rendering"))
			{
				AddComponentSearchResult<SpriteRendererComponent>(entity, "Rendering", "Sprite Renderer", "", [](auto&) {});
				AddComponentSearchResult<CircleRendererComponent>(entity, "Rendering", "Circle Renderer", "", [](auto&) {});
				AddComponentSearchResult<MeshComponent>(entity, "Rendering", "Mesh Renderer", "", [](auto& mc) { mc.MeshData = Mesh::CreateCube(); mc.Primitive = MeshComponent::PrimitiveType::Cube; mc.MaterialInstance = Material::Create(); });
				AddComponentSearchResult<TerrainComponent>(entity, "Rendering", "Terrain", "Use Rebuild Terrain after editing the descriptor.", [](auto&) {});
				AddComponentSearchResult<MeshLODComponent>(entity, "Rendering", "Mesh LOD", "", [](auto&) {});
			}
			if (cmpCategory("Physics"))
			{
				AddComponentSearchResult<Rigidbody3DComponent>(entity, "Physics", "Rigidbody 3D", "Requires a Box/Sphere/Capsule/Mesh collider to create a runtime body.", [](auto&) {});
				AddComponentSearchResult<BoxCollider3DComponent>(entity, "Physics", "Box Collider 3D", "", [](auto&) {});
				AddComponentSearchResult<SphereCollider3DComponent>(entity, "Physics", "Sphere Collider 3D", "", [](auto&) {});
				AddComponentSearchResult<CapsuleCollider3DComponent>(entity, "Physics", "Capsule Collider 3D", "", [](auto&) {});
				AddComponentSearchResult<MeshCollider3DComponent>(entity, "Physics", "Mesh Collider 3D", "Static Rigidbody 3D only in this milestone.", [](auto&) {});
			}
			if (cmpCategory("Character / Gameplay"))
			{
				AddComponentSearchResult<CharacterControllerComponent>(entity, "Character", "Character Controller", "Requires Capsule Collider 3D for correct runtime creation.", [](auto&) {});
				AddComponentSearchResult<PlayerStatsComponent>(entity, "Gameplay", "Player Stats", "", [](auto&) {});
				AddComponentSearchResult<InteractableComponent>(entity, "Gameplay", "Interactable", "", [](auto&) {});
				AddComponentSearchResult<PickupComponent>(entity, "Gameplay", "Pickup", "Pickup auto-adds Interactable in the legacy menu path.", [](auto&) {});
				AddComponentSearchResult<ActorComponent>(entity, "Gameplay", "Native Actor", "", [](auto&) {});
			}
			if (cmpCategory("Audio / Animation / FX"))
			{
				AddComponentSearchResult<AudioSourceComponent>(entity, "Audio", "Audio Source", "", [](auto&) {});
				AddComponentSearchResult<AnimatorComponent>(entity, "Animation", "Animator", "", [](auto&) {});
				AddComponentSearchResult<FoliageComponent>(entity, "Foliage", "Foliage (GPU Instanced)", "", [](auto&) {});
			}
			if (cmpCategory("UI"))
			{
				AddComponentSearchResult<UIRootComponent>(entity, "UI", "UI Root", "", [](auto&) {});
			}
			ImGui::SeparatorText("Legacy List");
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
			if (!m_SelectedEntity.HasComponent<MeshCollider3DComponent>())
			{
				if (ImGui::MenuItem("Mesh Collider 3D"))
				{
					m_SelectedEntity.AddComponent<MeshCollider3DComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<CharacterControllerComponent>())
			{
				if (ImGui::MenuItem("Character Controller"))
				{
					m_SelectedEntity.AddComponent<CharacterControllerComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<PlayerStatsComponent>())
			{
				if (ImGui::MenuItem("Player Stats"))
				{
					m_SelectedEntity.AddComponent<PlayerStatsComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<InteractableComponent>())
			{
				if (ImGui::MenuItem("Interactable"))
				{
					m_SelectedEntity.AddComponent<InteractableComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<PickupComponent>())
			{
				if (ImGui::MenuItem("Pickup"))
				{
					m_SelectedEntity.AddComponent<PickupComponent>();
					if (!m_SelectedEntity.HasComponent<InteractableComponent>())
					{
						auto& interactable = m_SelectedEntity.AddComponent<InteractableComponent>();
						interactable.Type = InteractableComponent::InteractionType::Pickup;
					}
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<VisualOffsetComponent>())
			{
				if (ImGui::MenuItem("Visual Offset"))
				{
					m_SelectedEntity.AddComponent<VisualOffsetComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			if (!m_SelectedEntity.HasComponent<MeshComponent>())
			{
				if (ImGui::MenuItem("Mesh Renderer"))
				{
					auto& mc = m_SelectedEntity.AddComponent<MeshComponent>();
					mc.MeshData = Mesh::CreateCube();
					mc.Primitive = MeshComponent::PrimitiveType::Cube;
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
			if (!m_SelectedEntity.HasComponent<ActorComponent>())
			{
				if (ImGui::MenuItem("Native Actor"))
				{
					m_SelectedEntity.AddComponent<ActorComponent>();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndPopup();
		}

		// A component was added from the palette, or an inspector property was edited
		// (set by the Draw*Property/DrawVec3Control helpers) → flag the scene dirty.
		// One-frame lag is fine for an unsaved-changes marker.
		if (s_ComponentMenuModified || s_InspectorEdited)
		{
			s_ComponentMenuModified = false;
			s_InspectorEdited = false;
			NotifySceneModified();
		}

		ImGui::Separator();
		if (EntityHasAuthoringWarning(entity))
		{
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.65f, 0.2f, 1.0f));
			ImGui::TextWrapped("This actor has setup warnings. Check mesh, physics, collider, or runtime status fields below.");
			ImGui::PopStyleColor();
		}
		ImGui::TextDisabled("Quick Actions");
		if (ImGui::SmallButton("Open Actor Editor") && m_OpenActorEditorCallback)
			m_OpenActorEditorCallback(entity);
		ImGui::SameLine();
		if (ImGui::SmallButton("Focus Viewport Camera"))
			BLU_CORE_INFO("SceneHierarchy: Focus Viewport Camera requested for {0}", GetAssetOwnerName(entity));
		if (entity.HasComponent<MeshComponent>())
		{
			ImGui::SameLine();
			if (ImGui::SmallButton("Reload Model"))
			{
				auto& mesh = entity.GetComponent<MeshComponent>();
				if (!mesh.FilePath.empty())
					mesh.ModelAsset = ModelLoader::Load(AssetPath::ResolvePath(mesh.FilePath).string());
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Generate Static Collision"))
			{
				std::string message;
				if (m_Context && m_Context->GenerateStaticMeshCollision(entity, &message))
					BLU_CORE_INFO("SceneHierarchy: {0}", message);
				else
					BLU_CORE_WARN("SceneHierarchy: {0}", message);
			}
		}
		if (entity.HasComponent<CharacterControllerComponent>())
		{
			if (entity.HasComponent<MeshComponent>())
				ImGui::SameLine();
			if (ImGui::SmallButton("Fit Visual To Capsule"))
			{
				std::string message;
				if (m_Context && m_Context->FitCharacterVisualToCapsule(entity, &message))
					BLU_CORE_INFO("SceneHierarchy: {0}", message);
				else
					BLU_CORE_WARN("SceneHierarchy: {0}", message);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton("Snap Feet To Ground"))
			{
				std::string message;
				if (m_Context && m_Context->SnapCharacterFeetToGround(entity, &message))
					BLU_CORE_INFO("SceneHierarchy: {0}", message);
				else
					BLU_CORE_WARN("SceneHierarchy: {0}", message);
			}
		}
		ImGui::Separator();

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
		DrawComponent<VisualOffsetComponent>("Visual Offset", entity, [&](auto& component)
			{
				ImGui::TextDisabled("Render-only offset from gameplay transform");
				DrawVec3Control("Translation", component.Translation);
				glm::vec3 rotation = glm::degrees(component.Rotation);
				ImGui::Spacing();
				DrawVec3Control("Rotation", rotation);
				component.Rotation = glm::radians(rotation);
				ImGui::Spacing();
				DrawVec3Control("Scale", component.Scale, 1.0f);

				ImGui::Spacing();
				if (ImGui::Button("Reset Visual Offset"))
				{
					std::string message;
					if (m_Context && m_Context->ResetVisualOffset(entity, &message))
						BLU_CORE_INFO("SceneHierarchy: {0}", message);
					else
						BLU_CORE_WARN("SceneHierarchy: {0}", message);
				}
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
		DrawComponent<FogVolumeComponent>("Fog Volume", entity, [](auto& F)
			{
				const char* shapes[] = { "Box", "Sphere" };
				int shape = (int)F.VolumeShape;
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::Combo("Shape", &shape, shapes, 2))
					F.VolumeShape = (FogVolumeComponent::Shape)shape;

				if (F.VolumeShape == FogVolumeComponent::Shape::Box)
					DrawVec3Control("Extents", F.Extents);
				else
					ImGui::DragFloat("Radius", &F.Radius, 0.1f, 0.1f, 500.0f, "%.2f");

				ImGui::ColorEdit3("Color",   glm::value_ptr(F.Color));
				ImGui::DragFloat("Density", &F.Density, 0.005f, 0.0f, 5.0f, "%.3f");
				ImGui::DragFloat("Falloff", &F.Falloff, 0.01f, 0.0f, 1.0f, "%.2f");
			});

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
		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [&](auto& component)
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
						const std::string ownerName = GetAssetOwnerName(entity);
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
									tex = LoadImportedTexture(p, ownerName);
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
				BeginPropertyGrid("Rigidbody3DGrid");
				const char* bodyTypes[] = { "Static", "Dynamic", "Kinematic" };
				int bodyType = (int)component.Type;
				if (DrawComboProperty("Body Type", bodyType, bodyTypes, IM_ARRAYSIZE(bodyTypes)))
					component.Type = (Rigidbody3DComponent::BodyType)bodyType;
				DrawFloatProperty("Gravity Scale", component.GravityScale, 0.01f, 0.0f, 10.0f, "%.3f");
				DrawFloatProperty("Linear Damping", component.LinearDamping, 0.01f, 0.0f, 1.0f, "%.3f");
				DrawFloatProperty("Angular Damping", component.AngularDamping, 0.01f, 0.0f, 1.0f, "%.3f");
				EndPropertyGrid();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted("Rotation Locks");
				ImGui::SameLine(116.0f);
				ImGui::Checkbox("X", &component.FixedRotationX); ImGui::SameLine();
				ImGui::Checkbox("Y", &component.FixedRotationY); ImGui::SameLine();
				ImGui::Checkbox("Z", &component.FixedRotationZ);
			});

		DrawComponent<BoxCollider3DComponent>("Box Collider 3D", entity, [](auto& component)
			{
				BeginPropertyGrid("BoxCollider3DGrid");
				DrawVec3Property("Half Extents", component.HalfExtents, 0.01f, 0.01f, 100.0f);
				DrawVec3Property("Offset", component.Offset, 0.01f);
				DrawFloatProperty("Friction", component.Friction, 0.01f, 0.0f, 1.0f, "%.3f");
				DrawFloatProperty("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f, "%.3f");
				DrawFloatProperty("Density", component.Density, 1.0f, 0.01f, 10000.0f, "%.3f");
				EndPropertyGrid();
			});

		DrawComponent<SphereCollider3DComponent>("Sphere Collider 3D", entity, [](auto& component)
			{
				BeginPropertyGrid("SphereCollider3DGrid");
				DrawFloatProperty("Radius", component.Radius, 0.01f, 0.01f, 100.0f, "%.3f");
				DrawVec3Property("Offset", component.Offset, 0.01f);
				DrawFloatProperty("Friction", component.Friction, 0.01f, 0.0f, 1.0f, "%.3f");
				DrawFloatProperty("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f, "%.3f");
				DrawFloatProperty("Density", component.Density, 1.0f, 0.01f, 10000.0f, "%.3f");
				EndPropertyGrid();
			});

		DrawComponent<CapsuleCollider3DComponent>("Capsule Collider 3D", entity, [](auto& component)
			{
				BeginPropertyGrid("CapsuleCollider3DGrid");
				DrawFloatProperty("Radius", component.Radius, 0.01f, 0.01f, 100.0f, "%.3f");
				DrawFloatProperty("Half Height", component.HalfHeight, 0.01f, 0.01f, 100.0f, "%.3f");
				DrawVec3Property("Offset", component.Offset, 0.01f);
				DrawFloatProperty("Friction", component.Friction, 0.01f, 0.0f, 1.0f, "%.3f");
				DrawFloatProperty("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f, "%.3f");
				DrawFloatProperty("Density", component.Density, 1.0f, 0.01f, 10000.0f, "%.3f");
				EndPropertyGrid();
			});

		DrawComponent<CharacterControllerComponent>("Character Controller", entity, [&](auto& component)
			{
				BeginPropertyGrid("CharacterControllerGrid");
				DrawFloatProperty("Move Speed", component.MoveSpeed, 0.05f, 0.0f, 50.0f, "%.2f");
				DrawFloatProperty("Jump Impulse", component.JumpImpulse, 0.05f, 0.0f, 50.0f, "%.2f");
				DrawFloatProperty("Step Height", component.StepHeight, 0.01f, 0.0f, 2.0f, "%.2f");
				DrawFloatProperty("Slope Limit", component.SlopeLimit, 0.5f, 0.0f, 89.0f, "%.1f");
				EndPropertyGrid();

				if (!entity.HasComponent<CapsuleCollider3DComponent>())
					ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Requires Capsule Collider 3D");
				else
				{
					auto& capsule = entity.GetComponent<CapsuleCollider3DComponent>();
					const float fullHeight = (capsule.HalfHeight + capsule.Radius) * 2.0f;
					ImGui::TextDisabled("Capsule %.2f radius / %.2f height", capsule.Radius, fullHeight);
				}

				if (!entity.HasComponent<VisualOffsetComponent>())
					ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "No Visual Offset; centered meshes may clip through the ground");

				if (ImGui::Button("Fit Visual To Capsule"))
				{
					std::string message;
					if (m_Context && m_Context->FitCharacterVisualToCapsule(entity, &message))
						BLU_CORE_INFO("SceneHierarchy: {0}", message);
					else
						BLU_CORE_WARN("SceneHierarchy: {0}", message);
				}
				ImGui::SameLine();
				if (ImGui::Button("Snap Feet To Ground"))
				{
					std::string message;
					if (m_Context && m_Context->SnapCharacterFeetToGround(entity, &message))
						BLU_CORE_INFO("SceneHierarchy: {0}", message);
					else
						BLU_CORE_WARN("SceneHierarchy: {0}", message);
				}

				if (ImGui::TreeNodeEx("Runtime", ImGuiTreeNodeFlags_Framed))
				{
					BeginPropertyGrid("CharacterRuntimeGrid");
					DrawReadOnlyProperty("Runtime", component._RuntimeCharacter ? "Active" : "Inactive");
					DrawReadOnlyProperty("Grounded", component.IsGrounded ? "Yes" : "No");
					ImGui::PushID("Velocity");
					PropertyLabel("Velocity");
					ImGui::TextDisabled("%.2f, %.2f, %.2f", component.Velocity.x, component.Velocity.y, component.Velocity.z);
					ImGui::NextColumn();
					ImGui::PopID();
					EndPropertyGrid();
					ImGui::TreePop();
				}
			});

		DrawComponent<PlayerStatsComponent>("Player Stats", entity, [](auto& stats)
			{
				BeginPropertyGrid("PlayerStatsGrid");
				DrawFloatProperty("Health", stats.Health, 0.5f, 0.0f, stats.MaxHealth, "%.1f");
				DrawFloatProperty("Max Health", stats.MaxHealth, 0.5f, 1.0f, 10000.0f, "%.1f");
				DrawFloatProperty("Stamina", stats.Stamina, 0.5f, 0.0f, stats.MaxStamina, "%.1f");
				DrawFloatProperty("Max Stamina", stats.MaxStamina, 0.5f, 1.0f, 10000.0f, "%.1f");
				DrawFloatProperty("Stamina Regen", stats.StaminaRegenRate, 0.25f, 0.0f, 1000.0f, "%.1f");
				DrawFloatProperty("Sprint Drain", stats.SprintStaminaDrain, 0.25f, 0.0f, 1000.0f, "%.1f");
				EndPropertyGrid();
				stats.MaxHealth = std::max(stats.MaxHealth, 1.0f);
				stats.MaxStamina = std::max(stats.MaxStamina, 1.0f);
				stats.Health = std::clamp(stats.Health, 0.0f, stats.MaxHealth);
				stats.Stamina = std::clamp(stats.Stamina, 0.0f, stats.MaxStamina);
			});

		DrawComponent<InteractableComponent>("Interactable", entity, [](auto& interactable)
			{
				char nameBuffer[128] = {};
				strncpy_s(nameBuffer, interactable.DisplayName.c_str(), sizeof(nameBuffer) - 1);
				if (ImGui::InputText("Display Name", nameBuffer, sizeof(nameBuffer)))
					interactable.DisplayName = nameBuffer;

				BeginPropertyGrid("InteractableGrid");
				DrawBoolProperty("Enabled", interactable.Enabled);
				DrawFloatProperty("Radius", interactable.InteractionRadius, 0.05f, 0.0f, 1000.0f, "%.2f");
				const char* types[] = { "Pickup", "Trigger", "Usable" };
				int type = (int)interactable.Type;
				if (DrawComboProperty("Type", type, types, IM_ARRAYSIZE(types)))
					interactable.Type = (InteractableComponent::InteractionType)type;
				EndPropertyGrid();
			});

		DrawComponent<PickupComponent>("Pickup", entity, [](auto& pickup)
			{
				BeginPropertyGrid("PickupGrid");
				const char* types[] = { "Health", "Stamina", "Generic Item" };
				int type = (int)pickup.Type;
				if (DrawComboProperty("Type", type, types, IM_ARRAYSIZE(types)))
					pickup.Type = (PickupComponent::PickupType)type;
				DrawFloatProperty("Amount", pickup.Amount, 0.5f, 0.0f, 10000.0f, "%.1f");
				DrawIntProperty("Count", pickup.Count, 0, 999);
				DrawBoolProperty("Consume", pickup.ConsumeOnPickup);
				EndPropertyGrid();
			});

		DrawComponent<UIRootComponent>("UI Root", entity, [](auto& ui)
			{
				BeginPropertyGrid("UIRootGrid");
				char path[512] = {};
				std::strncpy(path, ui.DocumentPath.c_str(), sizeof(path) - 1);
				if (DrawTextProperty("Document", path, sizeof(path)))
					ui.DocumentPath = AssetPath::ToProjectRelative(path);
				DrawBoolProperty("Visible", ui.Visible);
				DrawFloatProperty("Scale", ui.Scale, 0.01f, 0.1f, 4.0f, "%.2f");
				EndPropertyGrid();
				if (!ui.DocumentPath.empty() && !std::filesystem::exists(AssetPath::ResolvePath(ui.DocumentPath)))
					ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.25f, 1.0f), "UI document is missing");
			});

		DrawComponent<MeshCollider3DComponent>("Mesh Collider 3D", entity, [&](auto& component)
			{
				BeginPropertyGrid("MeshCollider3DGrid");
				DrawBoolProperty("Enabled", component.Enabled);
				DrawBoolProperty("Double Sided", component.DoubleSided);
				DrawFloatProperty("Friction", component.Friction, 0.01f, 0.0f, 1.0f, "%.3f");
				DrawFloatProperty("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f, "%.3f");
				EndPropertyGrid();

				uint32_t triangleCount = component.RuntimeTriangleCount;
				if (entity.HasComponent<MeshComponent>())
					triangleCount = std::max(triangleCount, CountModelCollisionTriangles(entity.GetComponent<MeshComponent>().ModelAsset));
				if (ImGui::TreeNodeEx("Runtime", ImGuiTreeNodeFlags_Framed))
				{
					BeginPropertyGrid("MeshColliderRuntimeGrid");
					char tris[32];
					snprintf(tris, sizeof(tris), "%u", triangleCount);
					DrawReadOnlyProperty("Triangles", tris);
					DrawReadOnlyProperty("Runtime", component.RuntimeBodyCreated ? "Active" : "Inactive");
					EndPropertyGrid();
					ImGui::TreePop();
				}
				if (!component.RuntimeStatus.empty())
					ImGui::TextWrapped("%s", component.RuntimeStatus.c_str());

				if (!entity.HasComponent<MeshComponent>())
					ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Requires Mesh Renderer");
				else if (!entity.GetComponent<MeshComponent>().ModelAsset)
					ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Mesh Renderer has no loaded model");
				else if (triangleCount == 0)
					ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "No CPU mesh triangles available; reload model");

				if (!entity.HasComponent<Rigidbody3DComponent>())
					ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.2f, 1.0f), "No Rigidbody 3D; no runtime body will be created");
				else if (entity.GetComponent<Rigidbody3DComponent>().Type != Rigidbody3DComponent::BodyType::Static)
					ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "Mesh Collider supports Static Rigidbody 3D only");
			});

		DrawComponent<MeshComponent>("Mesh Renderer", entity, [&](auto& component)
			{
				// ── Texture slot — thumbnail + drop target + browse button ─────
				// Shows a 48×48 thumbnail when a texture is assigned, or a drop
				// target + file-browse button when it's empty.  Works for both
				// drag-from-content-browser and native file-dialog browsing.
				auto TexSlot = [&](const char* uid, const char* label, Shared<Texture2D>& tex)
				{
					const std::string ownerName = GetAssetOwnerName(entity);
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
								tex = LoadImportedTexture(std::string((const char*)p->Data), ownerName);
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
								tex = LoadImportedTexture(std::string((const char*)p->Data), ownerName);
							ImGui::EndDragDropTarget();
						}
						// Browse button — opens a native file dialog
						ImGui::SameLine();
						if (ImGui::SmallButton("..."))
						{
							std::string path = FileDialogs::OpenFile(
								"Images\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.hdr\0All Files\0*.*\0");
							if (!path.empty())
								tex = LoadImportedTexture(path, ownerName);
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
					if (ImGui::Button("Reload Model##mesh"))
						component.ModelAsset = ModelLoader::Load(AssetPath::ResolvePath(component.FilePath).string());
					ImGui::Text("SubMeshes: %zu", component.ModelAsset->Meshes.size());
					ImGui::Text("Collision Triangles: %u", CountModelCollisionTriangles(component.ModelAsset));
					if (ImGui::Button("Generate Static Collision##mesh"))
					{
						std::string message;
						if (m_Context && m_Context->GenerateStaticMeshCollision(entity, &message))
							BLU_CORE_INFO("SceneHierarchy: {0}", message);
						else
							BLU_CORE_WARN("SceneHierarchy: {0}", message);
					}
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip("Adds/updates Static Rigidbody3D + MeshCollider3D from this model");

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
								BeginPropertyGrid("ImportedMaterialGrid");
								DrawColor4Property("Albedo", mat->AlbedoColor);
								DrawFloatProperty("Metallic", mat->Metallic, 0.01f, 0.0f, 1.0f, "%.3f");
								DrawFloatProperty("Roughness", mat->Roughness, 0.01f, 0.0f, 1.0f, "%.3f");
								DrawFloatProperty("AO", mat->AO, 0.01f, 0.0f, 1.0f, "%.3f");
								DrawVec3Property("Emissive", mat->EmissiveColor, 0.01f, 0.0f, 20.0f);
								DrawFloatProperty("Emit Str", mat->EmissiveStrength, 0.1f, 0.0f, 100.0f, "%.2f");
								static const char* blendModeNames[] = { "Opaque", "Masked", "Transparent", "Additive" };
								int blendIdx = static_cast<int>(mat->Blend);
								if (DrawComboProperty("Blend Mode", blendIdx, blendModeNames, IM_ARRAYSIZE(blendModeNames)))
									mat->Blend = static_cast<BlendMode>(blendIdx);
								if (mat->Blend == BlendMode::Masked)
									DrawFloatProperty("Alpha Cutoff", mat->AlphaCutoff, 0.01f, 0.0f, 1.0f, "%.3f");
								DrawBoolProperty("Two Sided", mat->TwoSided);
								static const char* shadingNames[] = { "PBR", "Unlit" };
								int shadingIdx = static_cast<int>(mat->Shading);
								if (DrawComboProperty("Shading", shadingIdx, shadingNames, IM_ARRAYSIZE(shadingNames)))
									mat->Shading = static_cast<ShadingModel>(shadingIdx);
								EndPropertyGrid();
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
					int currentMeshType = component.Primitive == MeshComponent::PrimitiveType::Quad ? 1 : 0;
					if (ImGui::Combo("Mesh Type", &currentMeshType, meshTypes, IM_ARRAYSIZE(meshTypes)))
					{
						if (currentMeshType == 0)
						{
							component.MeshData = Mesh::CreateCube();
							component.Primitive = MeshComponent::PrimitiveType::Cube;
						}
						else
						{
							component.MeshData = Mesh::CreateQuad();
							component.Primitive = MeshComponent::PrimitiveType::Quad;
						}
					}

					if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
					{
						if (!component.MaterialInstance)
							component.MaterialInstance = Material::Create();

						auto& mi = *component.MaterialInstance;
						BeginPropertyGrid("PrimitiveMaterialGrid");
						DrawColor4Property("Albedo", mi.AlbedoColor);
						DrawFloatProperty("Metallic", mi.Metallic, 0.01f, 0.0f, 1.0f, "%.3f");
						DrawFloatProperty("Roughness", mi.Roughness, 0.01f, 0.0f, 1.0f, "%.3f");
						DrawFloatProperty("AO", mi.AO, 0.01f, 0.0f, 1.0f, "%.3f");
						DrawVec3Property("Emissive", mi.EmissiveColor, 0.01f, 0.0f, 20.0f);
						DrawFloatProperty("Emissive Str", mi.EmissiveStrength, 0.1f, 0.0f, 100.0f, "%.2f");
						static const char* blendModeNames[] = { "Opaque", "Masked", "Transparent", "Additive" };
						int blendIdx = static_cast<int>(mi.Blend);
						if (DrawComboProperty("Blend Mode", blendIdx, blendModeNames, IM_ARRAYSIZE(blendModeNames)))
							mi.Blend = static_cast<BlendMode>(blendIdx);
						if (mi.Blend == BlendMode::Masked)
							DrawFloatProperty("Alpha Cutoff", mi.AlphaCutoff, 0.01f, 0.0f, 1.0f, "%.3f");
						DrawBoolProperty("Two Sided", mi.TwoSided);
						static const char* shadingNames[] = { "PBR", "Unlit" };
						int shadingIdx = static_cast<int>(mi.Shading);
						if (DrawComboProperty("Shading", shadingIdx, shadingNames, IM_ARRAYSIZE(shadingNames)))
							mi.Shading = static_cast<ShadingModel>(shadingIdx);
						EndPropertyGrid();
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
			ImGui::Text("Arm Length");
			ImGui::DragFloat("##SA_ArmLen", &arm.ArmLength, 0.1f, 0.1f, 50.0f, "%.2f");

			ImGui::Text("Socket Offset");
			ImGui::DragFloat3("##SA_Socket", glm::value_ptr(arm.SocketOffset), 0.05f, 0.0f, 0.0f, "%.2f");

			ImGui::Text("Pivot Offset");
			ImGui::DragFloat3("##SA_Pivot", glm::value_ptr(arm.PivotOffset), 0.05f, 0.0f, 0.0f, "%.2f");

			ImGui::Text("Pitch (deg)");
			ImGui::DragFloat("##SA_Pitch", &arm.Pitch, 0.5f, -89.0f, 89.0f, "%.1f");

			ImGui::Text("Yaw Offset");
			ImGui::DragFloat("##SA_Yaw", &arm.Yaw, 0.5f, -180.0f, 180.0f, "%.1f");

			ImGui::Checkbox("Inherit Target Yaw", &arm.InheritYaw);

			ImGui::Separator();
			ImGui::Checkbox("Enable Lag", &arm.EnableLag);
			if (arm.EnableLag)
			{
				ImGui::Text("Lag Speed");
				ImGui::DragFloat("##SA_LagSpd", &arm.PositionLagSpeed, 0.5f, 0.5f, 30.0f, "%.1f");
			}
			ImGui::Separator();
			ImGui::Text("Camera UUID: %llu", (unsigned long long)arm.TargetCameraUUID);
		});

		// ── Animator Component ────────────────────────────────────────────────────
		DrawComponent<TerrainComponent>("Terrain", entity, [&](auto& terrain)
		{
			terrain.Spec = SanitizeTerrainSpec(terrain.Spec);
			ImGui::DragInt("Width (quads)", &terrain.Spec.GridWidth, 1.0f, 1, 1024);
			ImGui::DragInt("Height (quads)", &terrain.Spec.GridHeight, 1.0f, 1, 1024);
			ImGui::DragFloat("Cell Size", &terrain.Spec.CellSize, 0.1f, 0.001f, 100.0f);
			ImGui::DragFloat("Height Scale", &terrain.Spec.HeightScale, 0.5f, 0.0f, 2000.0f);

			char heightmapPath[512] = {};
			std::strncpy(heightmapPath, terrain.Spec.HeightmapPath.c_str(), sizeof(heightmapPath) - 1);
			if (ImGui::InputText("Heightmap", heightmapPath, sizeof(heightmapPath)))
				terrain.Spec.HeightmapPath = AssetPath::ToProjectRelative(heightmapPath);
			ImGui::SameLine();
			if (ImGui::Button("...##terrain"))
			{
				std::string path = FileDialogs::OpenFile(
					"Image (*.png;*.jpg;*.bmp;*.tga)\0*.png;*.jpg;*.jpeg;*.bmp;*.tga\0All\0*.*\0");
				if (!path.empty())
					terrain.Spec.HeightmapPath = AssetPath::ImportTexturePath(path, GetAssetOwnerName(entity));
			}

			if (ImGui::Button("Rebuild Terrain") && m_Context)
			{
				std::string message;
				if (!m_Context->RebuildTerrain(entity, &message))
					BLU_CORE_WARN("Terrain rebuild failed: {}", message);
			}
		});

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

			// Clip selector — picking a clip CROSSFADES to it over the Blend time via PlayClip
			// (Phase 10a). The editor update ticks the animator, so the blend plays live in the
			// viewport. Set Blend to 0 for an instant cut.
			auto& clips = anim.SkelData->Clips;
			if (!clips.empty())
			{
				anim.CurrentClipIndex = std::clamp(anim.CurrentClipIndex, 0, (int)clips.size() - 1);
				int shownClip = (anim.NewClipIndex >= 0) ? anim.NewClipIndex : anim.CurrentClipIndex;
				shownClip = std::clamp(shownClip, 0, (int)clips.size() - 1);
				if (ImGui::BeginCombo("Clip", clips[shownClip].Name.c_str()))
				{
					for (int i = 0; i < (int)clips.size(); ++i)
					{
						bool selected = (i == shownClip);
						if (ImGui::Selectable(clips[i].Name.c_str(), selected))
							anim.PlayClip(i, anim.BlendDuration); // crossfade (instant if Blend <= 0)
						if (selected) ImGui::SetItemDefaultFocus();
					}
					ImGui::EndCombo();
				}

				ImGui::DragFloat("Blend (s)", &anim.BlendDuration, 0.01f, 0.0f, 2.0f, "%.2f");

				// Live crossfade indicator while a transition is in flight.
				if (anim.NewClipIndex >= 0 && anim.NewClipIndex < (int)clips.size())
				{
					float t = anim.BlendDuration > 0.0f ? std::clamp(anim.BlendElapsed / anim.BlendDuration, 0.0f, 1.0f) : 1.0f;
					ImGui::TextColored(ImVec4(0.55f, 0.8f, 1.0f, 1.0f), "Blending %s -> %s",
						clips[anim.CurrentClipIndex].Name.c_str(), clips[anim.NewClipIndex].Name.c_str());
					ImGui::ProgressBar(t, ImVec2(-1, 0), "crossfade");
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
		DrawComponent<FoliageComponent>("Foliage (GPU Instanced)", entity, [&](auto& fc)
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
					fc.ModelAsset = ModelLoader::Load(r);
					fc.FilePath  = AssetPath::ImportModelPath(r);
				}
			}
			if (ImGui::Button("Reload Model##fc") && !fc.FilePath.empty())
				fc.ModelAsset = ModelLoader::Load(AssetPath::ResolvePath(fc.FilePath).string());

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
		DrawComponent<AudioSourceComponent>("Audio Source", entity, [&](auto& asc)
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
					asc.FilePath = AssetPath::CopyExternalAssetToProject(result, "audio", GetAssetOwnerName(entity));
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

		// ── Native Actor ──────────────────────────────────────────────────────────
		DrawComponent<ActorComponent>("Native Actor", entity, [entity](auto& actorComponent) mutable
		{
			const auto classes = NativeClassRegistry::Get().GetClasses(NativeClassKind::Actor);
			const NativeClassDescriptor* selectedDescriptor = NativeClassRegistry::Get().FindDescriptor(actorComponent.ClassID);

			const char* preview = actorComponent.ClassID.empty()
				? "(None)"
				: (selectedDescriptor ? selectedDescriptor->DisplayName.c_str() : actorComponent.ClassID.c_str());
			if (ImGui::BeginCombo("Class##nsc", preview))
			{
				if (ImGui::Selectable("(None)", actorComponent.ClassID.empty()))
				{
					actorComponent.ClassID.clear();
					actorComponent.Overrides.clear();
				}
				for (const NativeClassDescriptor* descriptor : classes)
				{
					bool selected = actorComponent.ClassID == descriptor->ID;
					if (ImGui::Selectable(descriptor->DisplayName.c_str(), selected))
					{
						actorComponent.ClassID = descriptor->ID;
						actorComponent.Overrides.clear();
					}
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			// Manual text entry — type the class name when the dropdown is empty
			char buf[128] = {};
			if (!actorComponent.ClassID.empty())
				strncpy_s(buf, actorComponent.ClassID.c_str(), sizeof(buf) - 1);
			ImGui::SetNextItemWidth(-1.0f);
			if (ImGui::InputText("##nsc_manual", buf, sizeof(buf)))
				actorComponent.ClassID = buf;

			ImGui::Spacing();
			if (entity.GetScene() && entity.GetScene()->FindActor(entity.GetUUID()))
				ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "Running");
			else if (!actorComponent.ClassID.empty())
				ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Will create \"%s\" on Play", actorComponent.ClassID.c_str());
			else
				ImGui::TextDisabled("No class selected.");

			selectedDescriptor = NativeClassRegistry::Get().FindDescriptor(actorComponent.ClassID);
			if (selectedDescriptor && !selectedDescriptor->Properties.empty())
			{
				ImGui::Separator();
				ImGui::TextDisabled("Property Overrides");
				for (const NativePropertyDescriptor& property : selectedDescriptor->Properties)
					DrawNativePropertyOverride(actorComponent, property);
			}
		});

		float extraSpace = 200.0f;  // Extra space at the end in pixels
		ImGui::Dummy(ImVec2(0.0f, extraSpace));

		ImGui::EndChild();
		
			
		
	}
}
