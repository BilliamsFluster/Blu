#include "AssetPreviewService.h"
#include "Blu/Debug/Instrumentor.h"
#include "Blu/Rendering/Material.h"
#include "Blu/Rendering/ModelLoader.h"
#include "Blu/Rendering/RenderCommand.h"
#include "Blu/Rendering/Renderer3D.h"
#include "Blu/Scene/SceneSerializer.h"
#include "Blu/Utils/AssetPath.h"
#include <algorithm>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace Blu
{
	namespace
	{
		static bool IsPreviewableModel(const std::filesystem::path& path)
		{
			std::string ext = AssetPath::ToLower(path.extension().string());
			return ext == ".fbx" || ext == ".obj" || ext == ".gltf" ||
			       ext == ".glb" || ext == ".dae" || ext == ".ply" ||
			       ext == ".3ds" || ext == ".blend" || ext == ".bluprefab";
		}

		static void ComputeModelBounds(const Shared<Model>& model, glm::vec3& center, float& radius)
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

		static void ComputeEntityBounds(Entity entity, glm::vec3& center, float& radius)
		{
			center = glm::vec3(0.0f);
			radius = 1.0f;
			if (!entity)
				return;

			if (entity.HasComponent<MeshComponent>() && entity.GetComponent<MeshComponent>().ModelAsset)
				ComputeModelBounds(entity.GetComponent<MeshComponent>().ModelAsset, center, radius);
			else if (entity.HasComponent<TransformComponent>())
			{
				auto& tc = entity.GetComponent<TransformComponent>();
				radius = std::max({ tc.Scale.x, tc.Scale.y, tc.Scale.z, 0.5f });
			}
		}
	}

	AssetPreviewService& AssetPreviewService::Get()
	{
		static AssetPreviewService service;
		return service;
	}

	Shared<Texture2D> AssetPreviewService::GetImageThumbnail(const std::filesystem::path& path)
	{
		if (path.empty() || !std::filesystem::exists(path))
			return nullptr;

		std::string key = path.lexically_normal().generic_string();
		auto lastWrite = std::filesystem::last_write_time(path);
		auto it = m_ImageCache.find(key);
		if (it != m_ImageCache.end() && it->second.LastWriteTime == lastWrite)
			return it->second.Texture;

		Shared<Texture2D> texture = Texture2D::Create(path.string());
		if (texture)
			m_ImageCache[key] = { texture, lastWrite };
		return texture;
	}

	uint64_t AssetPreviewService::GetAssetThumbnail(const std::filesystem::path& path, bool& failed)
	{
		failed = false;
		if (path.empty() || !std::filesystem::exists(path) || !IsPreviewableModel(path))
		{
			failed = true;
			return 0;
		}

		std::string key = path.lexically_normal().generic_string();
		auto lastWrite = std::filesystem::last_write_time(path);
		auto& entry = m_RenderedCache[key];
		if (entry.Framebuffer && entry.LastWriteTime == lastWrite && !entry.Failed)
			return entry.Framebuffer->GetColorAttachmentID();
		if (entry.Failed && entry.LastWriteTime == lastWrite)
		{
			failed = true;
			return 0;
		}
		if (m_RenderedThisFrame >= 1)
			return entry.Framebuffer ? entry.Framebuffer->GetColorAttachmentID() : 0;

		entry.LastWriteTime = lastWrite;
		entry.Failed = !RenderModelLikeThumbnail(path, entry);
		m_RenderedThisFrame++;
		failed = entry.Failed;
		return entry.Framebuffer && !entry.Failed ? entry.Framebuffer->GetColorAttachmentID() : 0;
	}

	void AssetPreviewService::RenderEntityPreview(Entity entity, const Shared<FrameBuffer>& framebuffer, EditorCamera& camera, glm::vec2 size, bool resetCamera, UUID& lastEntityID)
	{
		RenderEntityToFramebuffer(entity, framebuffer, camera, size, resetCamera, lastEntityID);
	}

	void AssetPreviewService::Invalidate(const std::filesystem::path& path)
	{
		std::string key = path.lexically_normal().generic_string();
		m_ImageCache.erase(key);
		m_RenderedCache.erase(key);
	}

	bool AssetPreviewService::RenderModelLikeThumbnail(const std::filesystem::path& path, RenderedCacheEntry& entry)
	{
		std::string ext = AssetPath::ToLower(path.extension().string());
		if (ext == ".bluprefab")
		{
			Shared<Scene> scene = std::make_shared<Scene>();
			SceneSerializer serializer(scene);
			if (!serializer.Deserialize(path.string()))
				return false;
			auto view = scene->GetAllEntitiesWith<IDComponent>();
			if (view.begin() == view.end())
				return false;
			Entity entity{ *view.begin(), scene.get() };
			if (!entry.Framebuffer)
			{
				FrameBufferSpecifications spec;
				spec.Width = 192;
				spec.Height = 192;
				spec.Attachments = { FrameBufferTextureFormat::RGBA8, FrameBufferTextureFormat::Depth };
				entry.Framebuffer = FrameBuffer::Create(spec);
			}
			UUID last = 0;
			return RenderEntityToFramebuffer(entity, entry.Framebuffer, m_ThumbnailCamera, { 192.0f, 192.0f }, true, last);
		}

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

		glm::vec3 center;
		float radius;
		ComputeModelBounds(model, center, radius);

		m_ThumbnailCamera.SetViewportSize(192.0f, 192.0f);
		m_ThumbnailCamera.SetDistance(radius * 2.6f);
		m_ThumbnailCamera.SetPitchYaw(glm::radians(18.0f), glm::radians(-35.0f));
		m_ThumbnailCamera.SetFocalPoint(glm::vec3(0.0f, radius * 0.15f, 0.0f));

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
		Renderer3D::BeginScene(m_ThumbnailCamera);
		Renderer3D::DrawMesh(glm::translate(glm::mat4(1.0f), -center), mesh);
		Renderer3D::FlushDrawCalls();
		Renderer3D::EndScene();
		entry.Framebuffer->UnBind();
		return true;
	}

	bool AssetPreviewService::RenderEntityToFramebuffer(Entity entity, const Shared<FrameBuffer>& framebuffer, EditorCamera& camera, glm::vec2 size, bool resetCamera, UUID& lastEntityID)
	{
		if (!entity || !framebuffer)
			return false;

		uint32_t width = std::max(64u, (uint32_t)size.x);
		uint32_t height = std::max(64u, (uint32_t)size.y);
		auto spec = framebuffer->GetSpecification();
		if (spec.Width != width || spec.Height != height)
			framebuffer->Resize(width, height);

		glm::vec3 center;
		float radius;
		ComputeEntityBounds(entity, center, radius);
		UUID id = entity.GetUUID();
		camera.SetViewportSize((float)width, (float)height);
		if (resetCamera || lastEntityID != id)
		{
			camera.SetDistance(radius * 2.8f);
			camera.SetPitchYaw(glm::radians(18.0f), glm::radians(-35.0f));
			camera.SetFocalPoint(glm::vec3(0.0f, radius * 0.2f, 0.0f));
			lastEntityID = id;
		}

		framebuffer->Bind();
		RenderCommand::SetClearColor({ 0.07f, 0.075f, 0.08f, 1.0f });
		RenderCommand::Clear();
		Renderer3D::SetLights(
			{ DirLightData{ glm::normalize(glm::vec3(-0.35f, -0.8f, -0.45f)), {0.14f, 0.14f, 0.16f}, {1.0f, 0.96f, 0.88f}, {0.45f, 0.45f, 0.45f}, 1.6f } },
			{},
			{});
		Renderer3D::BeginScene(camera);
		if (entity.HasComponent<MeshComponent>())
		{
			auto& mesh = entity.GetComponent<MeshComponent>();
			if (!mesh.MeshData && !mesh.ModelAsset)
				mesh.MeshData = mesh.Primitive == MeshComponent::PrimitiveType::Quad ? Mesh::CreateQuad() : Mesh::CreateCube();

			glm::mat4 transform = glm::translate(glm::mat4(1.0f), -center);
			if (entity.HasComponent<VisualOffsetComponent>())
				transform *= entity.GetComponent<VisualOffsetComponent>().GetTransform();
			else if (entity.HasComponent<TransformComponent>() && !mesh.ModelAsset)
				transform *= glm::scale(glm::mat4(1.0f), entity.GetComponent<TransformComponent>().Scale);
			Renderer3D::DrawMesh(transform, mesh);
			Renderer3D::FlushDrawCalls();
		}
		Renderer3D::EndScene();
		framebuffer->UnBind();
		return true;
	}
}
