#include "Blupch.h"
#include "RuntimeUI.h"
#include "Blu/Core/Log.h"
#include "Blu/Rendering/OrthographicCamera.h"
#include "Blu/Rendering/PipelineState.h"
#include "Blu/Rendering/Renderer2D.h"
#include "Blu/Scene/Entity.h"
#include "Blu/Utils/AssetPath.h"
#include "yaml-cpp/yaml.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <unordered_map>

namespace Blu
{
	namespace
	{
		struct CachedDocument
		{
			UIDocument Document;
			std::filesystem::file_time_type LastWriteTime{};
			bool Valid = false;
			bool Failed = false;
		};

		std::unordered_map<std::string, CachedDocument> s_DocumentCache;
		std::unordered_map<std::string, Shared<Texture2D>> s_TextureCache;

		static glm::vec2 ReadVec2(const YAML::Node& node, glm::vec2 fallback)
		{
			if (!node || !node.IsSequence() || node.size() < 2)
				return fallback;
			return { node[0].as<float>(), node[1].as<float>() };
		}

		static glm::vec4 ReadColor(const YAML::Node& node, glm::vec4 fallback)
		{
			if (!node || !node.IsSequence() || node.size() < 4)
				return fallback;
			return { node[0].as<float>(), node[1].as<float>(), node[2].as<float>(), node[3].as<float>() };
		}

		static std::string Lower(std::string value)
		{
			for (char& c : value)
				c = (char)std::tolower((unsigned char)c);
			return value;
		}

		static UIWidgetType ParseWidgetType(const std::string& raw)
		{
			std::string type = Lower(raw);
			if (type == "text" || type == "label") return UIWidgetType::Text;
			if (type == "image") return UIWidgetType::Image;
			if (type == "progressbar" || type == "progress") return UIWidgetType::ProgressBar;
			if (type == "row") return UIWidgetType::Row;
			if (type == "column") return UIWidgetType::Column;
			if (type == "button") return UIWidgetType::Button;
			return UIWidgetType::Panel;
		}

		static UIBinding ParseBinding(const std::string& raw)
		{
			std::string binding = Lower(raw);
			if (binding == "pawnname") return UIBinding::PawnName;
			if (binding == "health") return UIBinding::Health;
			if (binding == "stamina") return UIBinding::Stamina;
			if (binding == "healthtext") return UIBinding::HealthText;
			if (binding == "staminatext") return UIBinding::StaminaText;
			if (binding == "zombiecount") return UIBinding::ZombieCount;
			if (binding == "interactprompt") return UIBinding::InteractPrompt;
			if (binding == "ammotext") return UIBinding::AmmoText;
			if (binding == "reticle") return UIBinding::Reticle;
			if (binding == "hitmarker") return UIBinding::Hitmarker;
			return UIBinding::None;
		}

		static UIWidget ParseWidget(const YAML::Node& node)
		{
			UIWidget widget;
			if (node["Type"]) widget.Type = ParseWidgetType(node["Type"].as<std::string>());
			if (node["Name"]) widget.Name = node["Name"].as<std::string>();
			if (node["Text"]) widget.Text = node["Text"].as<std::string>();
			if (node["Image"]) widget.ImagePath = AssetPath::ToProjectRelative(node["Image"].as<std::string>());
			if (node["Binding"]) widget.Binding = ParseBinding(node["Binding"].as<std::string>());
			if (node["Position"]) widget.Position = ReadVec2(node["Position"], widget.Position);
			if (node["Size"]) widget.Size = ReadVec2(node["Size"], widget.Size);
			if (node["Padding"]) widget.Padding = ReadVec2(node["Padding"], widget.Padding);
			if (node["Gap"]) widget.Gap = node["Gap"].as<float>();
			if (node["FontSize"]) widget.FontSize = node["FontSize"].as<float>();
			if (node["Color"]) widget.Color = ReadColor(node["Color"], widget.Color);
			if (node["ForegroundColor"]) widget.ForegroundColor = ReadColor(node["ForegroundColor"], widget.ForegroundColor);
			if (node["FillColor"]) widget.FillColor = ReadColor(node["FillColor"], widget.FillColor);
			if (node["Visible"]) widget.Visible = node["Visible"].as<bool>();
			if (node["Children"])
			{
				for (auto child : node["Children"])
					widget.Children.push_back(ParseWidget(child));
			}
			return widget;
		}

		static std::string BindingText(UIBinding binding, const SceneDiagnostics& diagnostics)
		{
			switch (binding)
			{
			case UIBinding::PawnName:
				return diagnostics.PossessedPawnName.empty() ? "Pawn: <none>" : "Pawn: " + diagnostics.PossessedPawnName;
			case UIBinding::HealthText:
				return "Health " + std::to_string((int)diagnostics.PossessedPawnHealth) + " / " + std::to_string((int)diagnostics.PossessedPawnMaxHealth);
			case UIBinding::StaminaText:
				return "Stamina " + std::to_string((int)diagnostics.PossessedPawnStamina) + " / " + std::to_string((int)diagnostics.PossessedPawnMaxStamina);
			case UIBinding::ZombieCount:
				return "Zombies: " + std::to_string(diagnostics.ActiveZombieCount);
			case UIBinding::AmmoText:
				if (!diagnostics.PossessedPawnHasAmmo)
					return "";
				if (diagnostics.PossessedPawnReloading)
					return "RELOADING";
				return std::to_string(diagnostics.PossessedPawnAmmoInMag) + " / " + std::to_string(diagnostics.PossessedPawnAmmoReserve);
			case UIBinding::InteractPrompt:
				return diagnostics.NearbyInteractableName.empty() ? "" : "E: " + diagnostics.NearbyInteractableName;
			default:
				return {};
			}
		}

		static float BindingProgress(UIBinding binding, const SceneDiagnostics& diagnostics)
		{
			if (binding == UIBinding::Health)
				return diagnostics.PossessedPawnMaxHealth > 0.0f ? diagnostics.PossessedPawnHealth / diagnostics.PossessedPawnMaxHealth : 0.0f;
			if (binding == UIBinding::Stamina)
				return diagnostics.PossessedPawnMaxStamina > 0.0f ? diagnostics.PossessedPawnStamina / diagnostics.PossessedPawnMaxStamina : 0.0f;
			return 0.0f;
		}

		static Shared<Texture2D> GetTexture(const std::string& rawPath)
		{
			if (rawPath.empty())
				return nullptr;
			std::string path = AssetPath::ToProjectRelative(rawPath);
			auto it = s_TextureCache.find(path);
			if (it != s_TextureCache.end())
				return it->second;
			auto resolved = AssetPath::ResolvePath(path);
			if (!std::filesystem::exists(resolved))
				return nullptr;
			Shared<Texture2D> texture = Texture2D::Create(resolved.string());
			if (texture)
				s_TextureCache[path] = texture;
			return texture;
		}

		static uint32_t CountWidgets(const UIWidget& widget)
		{
			uint32_t count = widget.Visible ? 1u : 0u;
			for (const auto& child : widget.Children)
				count += CountWidgets(child);
			return count;
		}

		static uint32_t CountWidgets(const UIDocument& document)
		{
			uint32_t count = 0;
			for (const auto& widget : document.Widgets)
				count += CountWidgets(widget);
			return count;
		}

		static std::array<uint8_t, 7> Glyph(char c)
		{
			switch ((char)std::toupper((unsigned char)c))
			{
			case 'A': return { 0x0E,0x11,0x11,0x1F,0x11,0x11,0x11 };
			case 'B': return { 0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E };
			case 'C': return { 0x0E,0x11,0x10,0x10,0x10,0x11,0x0E };
			case 'D': return { 0x1E,0x11,0x11,0x11,0x11,0x11,0x1E };
			case 'E': return { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F };
			case 'F': return { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x10 };
			case 'G': return { 0x0E,0x11,0x10,0x17,0x11,0x11,0x0F };
			case 'H': return { 0x11,0x11,0x11,0x1F,0x11,0x11,0x11 };
			case 'I': return { 0x1F,0x04,0x04,0x04,0x04,0x04,0x1F };
			case 'J': return { 0x01,0x01,0x01,0x01,0x11,0x11,0x0E };
			case 'K': return { 0x11,0x12,0x14,0x18,0x14,0x12,0x11 };
			case 'L': return { 0x10,0x10,0x10,0x10,0x10,0x10,0x1F };
			case 'M': return { 0x11,0x1B,0x15,0x15,0x11,0x11,0x11 };
			case 'N': return { 0x11,0x19,0x15,0x13,0x11,0x11,0x11 };
			case 'O': return { 0x0E,0x11,0x11,0x11,0x11,0x11,0x0E };
			case 'P': return { 0x1E,0x11,0x11,0x1E,0x10,0x10,0x10 };
			case 'Q': return { 0x0E,0x11,0x11,0x11,0x15,0x12,0x0D };
			case 'R': return { 0x1E,0x11,0x11,0x1E,0x14,0x12,0x11 };
			case 'S': return { 0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E };
			case 'T': return { 0x1F,0x04,0x04,0x04,0x04,0x04,0x04 };
			case 'U': return { 0x11,0x11,0x11,0x11,0x11,0x11,0x0E };
			case 'V': return { 0x11,0x11,0x11,0x11,0x11,0x0A,0x04 };
			case 'W': return { 0x11,0x11,0x11,0x15,0x15,0x15,0x0A };
			case 'X': return { 0x11,0x11,0x0A,0x04,0x0A,0x11,0x11 };
			case 'Y': return { 0x11,0x11,0x0A,0x04,0x04,0x04,0x04 };
			case 'Z': return { 0x1F,0x01,0x02,0x04,0x08,0x10,0x1F };
			case '0': return { 0x0E,0x11,0x13,0x15,0x19,0x11,0x0E };
			case '1': return { 0x04,0x0C,0x04,0x04,0x04,0x04,0x0E };
			case '2': return { 0x0E,0x11,0x01,0x02,0x04,0x08,0x1F };
			case '3': return { 0x1E,0x01,0x01,0x0E,0x01,0x01,0x1E };
			case '4': return { 0x02,0x06,0x0A,0x12,0x1F,0x02,0x02 };
			case '5': return { 0x1F,0x10,0x10,0x1E,0x01,0x01,0x1E };
			case '6': return { 0x0E,0x10,0x10,0x1E,0x11,0x11,0x0E };
			case '7': return { 0x1F,0x01,0x02,0x04,0x08,0x08,0x08 };
			case '8': return { 0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E };
			case '9': return { 0x0E,0x11,0x11,0x0F,0x01,0x01,0x0E };
			case ':': return { 0x00,0x04,0x04,0x00,0x04,0x04,0x00 };
			case '/': return { 0x01,0x02,0x02,0x04,0x08,0x08,0x10 };
			case '<': return { 0x02,0x04,0x08,0x10,0x08,0x04,0x02 };
			case '>': return { 0x08,0x04,0x02,0x01,0x02,0x04,0x08 };
			case '-': return { 0x00,0x00,0x00,0x1F,0x00,0x00,0x00 };
			default: return { 0,0,0,0,0,0,0 };
			}
		}

		static void DrawRuntimeText(const std::string& text, glm::vec2 position, float size, const glm::vec4& color)
		{
			const float pixel = std::max(1.0f, size / 9.0f);
			float cursorX = position.x;
			float cursorY = position.y;
			for (char c : text)
			{
				if (c == '\n')
				{
					cursorX = position.x;
					cursorY += pixel * 9.0f;
					continue;
				}
				if (c == ' ')
				{
					cursorX += pixel * 4.0f;
					continue;
				}
				auto glyph = Glyph(c);
				for (int row = 0; row < 7; ++row)
				{
					for (int col = 0; col < 5; ++col)
					{
						if ((glyph[row] >> (4 - col)) & 1)
						{
							glm::vec2 center = { cursorX + col * pixel + pixel * 0.5f, cursorY + row * pixel + pixel * 0.5f };
							Renderer2D::DrawQuad(center, { pixel, pixel }, color);
						}
					}
				}
				cursorX += pixel * 6.0f;
			}
		}

		static void DrawReticle(glm::vec2 center, const glm::vec4& color, float scale)
		{
			const float gap = 5.0f * scale, len = 9.0f * scale, thick = std::max(1.0f, 2.0f * scale);
			Renderer2D::DrawQuad({ center.x, center.y - gap - len * 0.5f }, { thick, len }, color); // up
			Renderer2D::DrawQuad({ center.x, center.y + gap + len * 0.5f }, { thick, len }, color); // down
			Renderer2D::DrawQuad({ center.x - gap - len * 0.5f, center.y }, { len, thick }, color); // left
			Renderer2D::DrawQuad({ center.x + gap + len * 0.5f, center.y }, { len, thick }, color); // right
			Renderer2D::DrawQuad(center, { thick, thick }, color);                                  // centre dot
		}

		static void DrawHitmarker(glm::vec2 center, const glm::vec4& color, float scale)
		{
			const float size = 26.0f * scale;
			const float pixel = std::max(1.0f, size / 9.0f);
			// "X" glyph is 5px wide x 7px tall in the runtime font; offset to centre it.
			DrawRuntimeText("X", { center.x - 2.5f * pixel, center.y - 3.5f * pixel }, size, color);
		}

		static void DrawWidget(const UIWidget& widget, glm::vec2 parentPosition, glm::vec2 viewport, const SceneDiagnostics& diagnostics, float scale)
		{
			if (!widget.Visible)
				return;

			// Crosshair + hitmarker are anchored to the viewport centre, ignoring authored
			// Position/Size so they stay centred at any resolution.
			if (widget.Binding == UIBinding::Reticle)
			{
				DrawReticle(viewport * 0.5f, widget.ForegroundColor, scale);
				return;
			}
			if (widget.Binding == UIBinding::Hitmarker)
			{
				if (diagnostics.HitmarkerTimer > 0.0f)
					DrawHitmarker(viewport * 0.5f, widget.ForegroundColor, scale);
				return;
			}

			glm::vec2 position = parentPosition + widget.Position * scale;
			glm::vec2 size = widget.Size * scale;
			glm::vec2 center = position + size * 0.5f;

			if (widget.Type == UIWidgetType::Panel || widget.Type == UIWidgetType::Button ||
			    widget.Type == UIWidgetType::Column || widget.Type == UIWidgetType::Row)
				Renderer2D::DrawQuad(center, size, widget.Color);
			else if (widget.Type == UIWidgetType::Image)
			{
				if (auto texture = GetTexture(widget.ImagePath))
					Renderer2D::DrawQuad(center, size, texture);
				else
					Renderer2D::DrawQuad(center, size, widget.Color);
			}
			else if (widget.Type == UIWidgetType::ProgressBar)
			{
				Renderer2D::DrawQuad(center, size, widget.Color);
				float fraction = std::clamp(BindingProgress(widget.Binding, diagnostics), 0.0f, 1.0f);
				glm::vec2 fillSize = { std::max(0.0f, size.x * fraction), size.y };
				glm::vec2 fillCenter = { position.x + fillSize.x * 0.5f, center.y };
				if (fillSize.x > 0.5f)
					Renderer2D::DrawQuad(fillCenter, fillSize, widget.FillColor);
			}

			if (widget.Type == UIWidgetType::Text || widget.Type == UIWidgetType::Button)
			{
				std::string text = widget.Binding == UIBinding::None ? widget.Text : BindingText(widget.Binding, diagnostics);
				if (!text.empty())
					DrawRuntimeText(text, position, widget.FontSize * scale, widget.ForegroundColor);
			}

			if (widget.Type == UIWidgetType::Column || widget.Type == UIWidgetType::Row || !widget.Children.empty())
			{
				glm::vec2 childCursor = position + widget.Padding * scale;
				for (const auto& child : widget.Children)
				{
					UIWidget childWidget = child;
					childWidget.Position += childCursor - position;
					DrawWidget(childWidget, position, viewport, diagnostics, scale);
					if (widget.Type == UIWidgetType::Row)
						childCursor.x += child.Size.x * scale + widget.Gap * scale;
					else if (widget.Type == UIWidgetType::Column)
						childCursor.y += child.Size.y * scale + widget.Gap * scale;
				}
			}
		}
	}

	bool RuntimeUI::LoadDocument(const std::filesystem::path& path, UIDocument& outDocument)
	{
		std::filesystem::path resolved = AssetPath::ResolvePath(path.string());
		if (!std::filesystem::exists(resolved))
			return false;

		try
		{
			YAML::Node data = YAML::LoadFile(resolved.string());
			if (data["UI"])
				data = data["UI"];
			UIDocument document;
			if (data["Name"]) document.Name = data["Name"].as<std::string>();
			if (data["Widgets"])
			{
				for (auto widget : data["Widgets"])
					document.Widgets.push_back(ParseWidget(widget));
			}
			outDocument = std::move(document);
			return true;
		}
		catch (const std::exception& e)
		{
			BLU_CORE_WARN("RuntimeUI: failed to load {0}: {1}", resolved.string(), e.what());
			return false;
		}
	}

	RuntimeUIRenderResult RuntimeUI::RenderDocument(const std::string& documentPath, const SceneDiagnostics& diagnostics, float viewportWidth, float viewportHeight, float scale)
	{
		RuntimeUIRenderResult result;
		result.DocumentPath = AssetPath::ToProjectRelative(documentPath);
		result.ViewportWidth = viewportWidth;
		result.ViewportHeight = viewportHeight;

		if (viewportWidth < 1.0f || viewportHeight < 1.0f)
			return result;

		std::string key = result.DocumentPath;
		auto resolved = AssetPath::ResolvePath(key);
		CachedDocument& entry = s_DocumentCache[key];
		if (std::filesystem::exists(resolved))
		{
			auto lastWrite = std::filesystem::last_write_time(resolved);
			if (!entry.Valid || entry.LastWriteTime != lastWrite)
			{
				entry.Valid = LoadDocument(resolved, entry.Document);
				entry.Failed = !entry.Valid;
				entry.LastWriteTime = lastWrite;
			}
		}
		else if (!entry.Valid && !entry.Failed)
		{
			entry.Document = CreateFallbackHUD();
			entry.Valid = true;
			result.MissingDocument = true;
			result.UsedFallback = true;
			BLU_CORE_WARN("RuntimeUI: missing UI document {0}, using fallback HUD", key);
		}

		UIDocument fallback;
		const UIDocument* document = nullptr;
		if (entry.Valid)
		{
			document = &entry.Document;
		}
		else
		{
			fallback = CreateFallbackHUD();
			document = &fallback;
			result.UsedFallback = true;
		}

		result.WidgetCount = CountWidgets(*document);
		result.Rendered = result.WidgetCount > 0;

		OrthographicCamera camera({ 0.0f, viewportWidth, viewportHeight, 0.0f });
		PipelineStateCache::GetNoDepth()->Bind();
		Renderer2D::BeginScene(camera);
		for (const auto& widget : document->Widgets)
			DrawWidget(widget, { 0.0f, 0.0f }, { viewportWidth, viewportHeight }, diagnostics, scale);
		Renderer2D::EndScene();
		PipelineStateCache::GetOpaque()->Bind();

		return result;
	}

	RuntimeUIRenderResult RuntimeUI::RenderGameplayHUD(Scene& scene, float viewportWidth, float viewportHeight)
	{
		SceneDiagnostics diagnostics = scene.GetDiagnostics();
		uint32_t rootCount = 0;
		auto view = scene.GetAllEntitiesWith<UIRootComponent>();
		for (auto e : view)
		{
			rootCount++;
			Entity entity{ e, &scene };
			auto& root = entity.GetComponent<UIRootComponent>();
			if (!root.Visible)
				continue;
			auto result = RenderDocument(root.DocumentPath, diagnostics, viewportWidth, viewportHeight, root.Scale);
			result.UIRootCount = rootCount;
			return result;
		}

		auto fallbackResult = RenderDocument("assets/ui/GameplayHUD.bluui", diagnostics, viewportWidth, viewportHeight, 1.0f);
		fallbackResult.UIRootCount = rootCount;
		return fallbackResult;
	}

	void RuntimeUI::Invalidate(const std::string& documentPath)
	{
		s_DocumentCache.erase(AssetPath::ToProjectRelative(documentPath));
	}

	UIDocument RuntimeUI::CreateFallbackHUD()
	{
		UIDocument document;
		document.Name = "Fallback Gameplay HUD";
		UIWidget panel;
		panel.Type = UIWidgetType::Column;
		panel.Position = { 18.0f, 18.0f };
		panel.Size = { 300.0f, 140.0f };
		panel.Padding = { 10.0f, 10.0f };
		panel.Gap = 8.0f;
		panel.Color = { 0.03f, 0.035f, 0.04f, 0.72f };

		UIWidget pawn;
		pawn.Type = UIWidgetType::Text;
		pawn.Size = { 260.0f, 18.0f };
		pawn.Binding = UIBinding::PawnName;
		pawn.FontSize = 14.0f;
		panel.Children.push_back(pawn);

		UIWidget healthText = pawn;
		healthText.Binding = UIBinding::HealthText;
		panel.Children.push_back(healthText);
		UIWidget healthBar;
		healthBar.Type = UIWidgetType::ProgressBar;
		healthBar.Size = { 240.0f, 14.0f };
		healthBar.Binding = UIBinding::Health;
		healthBar.Color = { 0.18f, 0.08f, 0.08f, 0.9f };
		healthBar.FillColor = { 0.85f, 0.18f, 0.18f, 1.0f };
		panel.Children.push_back(healthBar);

		UIWidget staminaText = pawn;
		staminaText.Binding = UIBinding::StaminaText;
		panel.Children.push_back(staminaText);
		UIWidget staminaBar = healthBar;
		staminaBar.Binding = UIBinding::Stamina;
		staminaBar.Color = { 0.08f, 0.12f, 0.08f, 0.9f };
		staminaBar.FillColor = { 0.25f, 0.80f, 0.25f, 1.0f };
		panel.Children.push_back(staminaBar);

		UIWidget zombies = pawn;
		zombies.Binding = UIBinding::ZombieCount;
		panel.Children.push_back(zombies);
		UIWidget prompt = pawn;
		prompt.Binding = UIBinding::InteractPrompt;
		prompt.ForegroundColor = { 1.0f, 0.82f, 0.30f, 1.0f };
		panel.Children.push_back(prompt);

		document.Widgets.push_back(panel);
		return document;
	}
}
