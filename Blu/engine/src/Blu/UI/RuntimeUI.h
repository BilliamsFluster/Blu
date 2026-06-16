#pragma once

#include "Blu/Core/Core.h"
#include "Blu/Rendering/Texture.h"
#include "Blu/Scene/Scene.h"
#include <filesystem>
#include <string>
#include <vector>

namespace Blu
{
	enum class UIWidgetType
	{
		Panel = 0,
		Text,
		Image,
		ProgressBar,
		Row,
		Column,
		Button
	};

	enum class UIBinding
	{
		None = 0,
		PawnName,
		Health,
		Stamina,
		HealthText,
		StaminaText,
		ZombieCount,
		InteractPrompt,
		AmmoText,      // "<mag> / <reserve>" or "RELOADING"
		Reticle,       // centered crosshair (drawn, no text)
		Hitmarker,     // centered hit "X", shown while HitmarkerTimer > 0
		Wave,          // "Wave <n> / <total>" — from the game mode (HasWaveHUD)
		Score,         // "Score: <kills>"
		Lives          // "Lives: <n>"
	};

	struct UIWidget
	{
		UIWidgetType Type = UIWidgetType::Panel;
		std::string Name;
		std::string Text;
		std::string ImagePath;
		std::string ActionId;          // on click: "load:<scene.blu>" loads a scene, else dispatched to the click handler
		UIBinding Binding = UIBinding::None;
		glm::vec2 Position = { 0.0f, 0.0f };
		glm::vec2 Size = { 100.0f, 24.0f };
		glm::vec2 Padding = { 8.0f, 6.0f };
		float Gap = 6.0f;
		float FontSize = 14.0f;
		glm::vec4 Color = { 0.08f, 0.08f, 0.08f, 0.70f };
		glm::vec4 ForegroundColor = { 0.92f, 0.94f, 0.96f, 1.0f };
		glm::vec4 FillColor = { 0.20f, 0.65f, 1.0f, 1.0f };
		bool Visible = true;
		std::vector<UIWidget> Children;
	};

	struct UIDocument
	{
		std::string Name = "UI";
		std::vector<UIWidget> Widgets;
	};

	struct RuntimeUIRenderResult
	{
		bool Rendered = false;
		bool UsedFallback = false;
		bool MissingDocument = false;
		std::string DocumentPath;
		float ViewportWidth = 0.0f;
		float ViewportHeight = 0.0f;
		uint32_t UIRootCount = 0;
		uint32_t WidgetCount = 0;
	};

	class RuntimeUI
	{
	public:
		static RuntimeUIRenderResult RenderDocument(const std::string& documentPath, const SceneDiagnostics& diagnostics, float viewportWidth, float viewportHeight, float scale = 1.0f);
		static RuntimeUIRenderResult RenderGameplayHUD(Scene& scene, float viewportWidth, float viewportHeight);
		static bool LoadDocument(const std::filesystem::path& path, UIDocument& outDocument);
		static void Invalidate(const std::string& documentPath);

		// Offset subtracted from the OS mouse position when hit-testing clickable widgets.
		// 0 for a fullscreen game / headless capture; the editor sets it to its viewport
		// panel origin so menu buttons are clickable inside the docked viewport.
		static void SetMouseViewportOffset(const glm::vec2& offset);

	private:
		static UIDocument CreateFallbackHUD();
	};
}
