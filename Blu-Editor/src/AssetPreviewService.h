#pragma once

#include <filesystem>
#include <map>
#include "Blu/Core/Core.h"
#include "Blu/Rendering/EditorCamera.h"
#include "Blu/Rendering/FrameBuffer.h"
#include "Blu/Rendering/Texture.h"
#include "Blu/Scene/Entity.h"

namespace Blu
{
	class AssetPreviewService
	{
	public:
		static AssetPreviewService& Get();

		Shared<Texture2D> GetImageThumbnail(const std::filesystem::path& path);
		uint64_t GetAssetThumbnail(const std::filesystem::path& path, bool& failed);
		void RenderEntityPreview(Entity entity, const Shared<FrameBuffer>& framebuffer, EditorCamera& camera, glm::vec2 size, bool resetCamera, UUID& lastEntityID);
		void ResetFrameBudget() { m_RenderedThisFrame = 0; }
		void Invalidate(const std::filesystem::path& path);

	private:
		struct TextureCacheEntry
		{
			Shared<Texture2D> Texture;
			std::filesystem::file_time_type LastWriteTime{};
		};

		struct RenderedCacheEntry
		{
			Shared<FrameBuffer> Framebuffer;
			std::filesystem::file_time_type LastWriteTime{};
			bool Failed = false;
		};

		bool RenderModelLikeThumbnail(const std::filesystem::path& path, RenderedCacheEntry& entry);
		bool RenderEntityToFramebuffer(Entity entity, const Shared<FrameBuffer>& framebuffer, EditorCamera& camera, glm::vec2 size, bool resetCamera, UUID& lastEntityID);

		std::map<std::string, TextureCacheEntry> m_ImageCache;
		std::map<std::string, RenderedCacheEntry> m_RenderedCache;
		EditorCamera m_ThumbnailCamera = EditorCamera(35.0f, 1.0f, 0.1f, 5000.0f);
		uint32_t m_RenderedThisFrame = 0;
	};
}
