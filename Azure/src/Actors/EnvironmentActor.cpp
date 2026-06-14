#include "EnvironmentActor.h"
#include "Blu/Core/Log.h"
#include "Blu/Scene/Scene.h"
#include "Blu/Scene/Entity.h"
#include "Blu/Scene/Component.h"
#include "Blu/Rendering/Mesh.h"
#include "Blu/Rendering/Material.h"
#include "Blu/Rendering/Terrain.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <vector>
#include <cmath>

namespace Azure
{
	float EnvironmentActor::Rand01()
	{
		m_Seed = m_Seed * 1664525u + 1013904223u;
		return (float)((m_Seed >> 8) & 0xFFFFFF) / (float)0x1000000;
	}

	static Blu::Shared<Blu::Material> MakeMat(const glm::vec3& albedo, float rough)
	{
		auto m = Blu::Material::Create();
		m->AlbedoColor      = glm::vec4(albedo, 1.0f);
		m->Metallic         = 0.0f;
		m->Roughness        = rough;
		m->EmissiveColor    = albedo * 0.05f;  // faint, so foliage never reads as pure black
		m->EmissiveStrength = 1.0f;
		return m;
	}

	// Single unit-cube model with one material. The instanced foliage path applies only the
	// per-instance transform (it ignores per-submesh LocalTransform), so each foliage layer
	// must be a single cube shaped entirely by its instance matrices (non-uniform scale).
	static Blu::Shared<Blu::Model> BuildBoxModel(const glm::vec3& albedo, float rough)
	{
		auto cube  = Blu::Mesh::CreateCube();
		auto model = std::make_shared<Blu::Model>();
		model->Materials = { MakeMat(albedo, rough) };
		Blu::SubMesh sm;
		sm.VAO            = cube->GetVertexArray();
		sm.IndexCount     = cube->GetIndexCount();
		sm.MaterialIndex  = 0;
		sm.LocalTransform = glm::mat4(1.0f);
		sm.BoundingCenter = cube->GetBoundingCenter();
		sm.BoundingRadius = cube->GetBoundingRadius();
		model->Meshes.push_back(std::move(sm));
		return model;
	}

	void EnvironmentActor::BeginPlay()
	{
		Blu::Scene* scene = GetScene();
		if (!scene)
			return;

		// Grab the terrain spec so foliage conforms to the rolling surface.
		Blu::TerrainSpec terrainSpec;
		bool haveTerrain = false;
		{
			auto view = scene->GetAllEntitiesWith<Blu::TerrainComponent>();
			for (auto e : view)
			{
				terrainSpec = view.get<Blu::TerrainComponent>(e).Spec;
				haveTerrain = true;
				break;
			}
		}
		auto heightAt = [&](float x, float z) -> float
		{
			return haveTerrain ? Blu::TerrainProceduralHeight(x, z, terrainSpec) : 0.0f;
		};

		const float halfExtent = 19.0f; // scatter within the ~40x40 ground

		// ── Grass blades: thin tall boxes scattered everywhere (incl. the flat arena) ──
		std::vector<glm::mat4> grass;
		grass.reserve(900);
		for (int i = 0; i < 900; ++i)
		{
			float x   = (Rand01() * 2.0f - 1.0f) * halfExtent;
			float z   = (Rand01() * 2.0f - 1.0f) * halfExtent;
			float y   = heightAt(x, z);
			float yaw = Rand01() * glm::two_pi<float>();
			float w   = 0.06f + Rand01() * 0.05f;          // blade thickness
			float h   = 0.40f + Rand01() * 0.45f;          // blade height
			glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(x, y + h * 0.5f, z));
			m = glm::rotate(m, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
			m = glm::scale(m, glm::vec3(w, h, w));
			grass.push_back(m);
		}

		// ── Trees: trunk + crown layers sharing positions, ringed around the arena ────
		std::vector<glm::mat4> trunks, crowns;
		for (int i = 0; i < 40; ++i)
		{
			float ang = Rand01() * glm::two_pi<float>();
			float rad = 15.0f + Rand01() * 9.0f; // 15..24 from centre → on the hills
			float x   = std::cos(ang) * rad;
			float z   = std::sin(ang) * rad;
			if (std::abs(x) > halfExtent || std::abs(z) > halfExtent)
				continue;
			float y   = heightAt(x, z);
			float yaw = Rand01() * glm::two_pi<float>();
			float s   = 0.85f + Rand01() * 0.6f;
			float th  = 1.6f * s;  // trunk height
			float ch  = 1.7f * s;  // crown height

			glm::mat4 trunk = glm::translate(glm::mat4(1.0f), glm::vec3(x, y + th * 0.5f, z));
			trunk = glm::rotate(trunk, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
			trunk = glm::scale(trunk, glm::vec3(0.22f * s, th, 0.22f * s));
			trunks.push_back(trunk);

			glm::mat4 crown = glm::translate(glm::mat4(1.0f), glm::vec3(x, y + th + ch * 0.35f, z));
			crown = glm::rotate(crown, yaw, glm::vec3(0.0f, 1.0f, 0.0f));
			crown = glm::scale(crown, glm::vec3(1.5f * s, ch, 1.5f * s));
			crowns.push_back(crown);
		}

		auto spawnFoliage = [&](const char* name, Blu::Shared<Blu::Model> model,
		                        std::vector<glm::mat4>&& xforms, float windStrength)
		{
			if (xforms.empty())
				return;
			Blu::Entity e = scene->CreateEntity(name);
			if (!e.HasComponent<Blu::TransformComponent>())
				e.AddComponent<Blu::TransformComponent>(); // identity; instances are world-space
			auto& fc = e.AddComponent<Blu::FoliageComponent>();
			fc.ModelAsset    = std::move(model);
			fc.Transforms    = std::move(xforms);
			fc.WindEnabled   = true;
			fc.WindStrength  = windStrength;
			fc.WindFrequency = 1.5f;
			fc.WindDirection = glm::vec3(1.0f, 0.0f, 0.35f);
		};

		const int grassCount = (int)grass.size();
		const int treeCount  = (int)trunks.size();
		spawnFoliage("GrassFoliage", BuildBoxModel(glm::vec3(0.20f, 0.44f, 0.13f), 0.95f), std::move(grass),  0.16f);
		spawnFoliage("TreeTrunks",   BuildBoxModel(glm::vec3(0.30f, 0.20f, 0.11f), 0.90f), std::move(trunks), 0.03f);
		spawnFoliage("TreeCrowns",   BuildBoxModel(glm::vec3(0.15f, 0.35f, 0.12f), 0.95f), std::move(crowns), 0.05f);

		BLU_CORE_INFO("EnvironmentActor: scattered {0} grass blades + {1} trees", grassCount, treeCount);
	}
}
