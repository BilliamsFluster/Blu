#pragma once
#include "Blu/GameFramework/ACharacter.h"
#include "Blu/Core/InputMap.h"
#include "Blu/Core/UUID.h"

namespace Blu { class Entity; }

namespace Azure
{
	// The player-controlled character.
	// Reads WASD + mouse from InputMap and drives ACharacter locomotion.
	class PlayerCharacter : public Blu::ACharacter
	{
	public:
		void BeginPlay() override;
		void Tick(float dt) override;
		void SetupPlayerInput(Blu::InputMap& input) override;
		void OnPossessed() override;
		void OnUnPossessed() override;

	private:
		void ResetMouseLookState();
		void UpdateStats(float dt, bool wantsSprint, bool isMoving, float& outSpeedScale);
		void UpdateFirstPersonCamera();   // drives the primary camera from yaw/pitch at eye height
		glm::vec3 LookForward() const;    // unit view direction from yaw/pitch
		void UpdateWeapon(float dt);      // fire/reload input + cooldowns
		void FireWeapon();                // spawn one projectile down the view ray
		void UpdateProjectiles(float dt); // step live projectiles, resolve zombie hits
		void TryInteract();
		bool TryPickupOverlap();
		bool ApplyPickup(Blu::Entity pickupEntity);

		float m_Yaw        = 0.0f;   // degrees, look yaw (body follows)
		float m_Pitch      = 0.0f;   // degrees, look pitch (camera only)
		float m_EyeHeight  = 0.75f;  // metres above the pawn (capsule-centre) origin
		Blu::UUID m_CameraUUID = Blu::UUID(0); // primary camera the pawn drives
		float m_PrevMouseX = 0.0f;
		float m_PrevMouseY = 0.0f;
		bool  m_FirstMouse = true;

		// ── Weapon (projectile) ──────────────────────────────────────────────
		int   m_MagSize       = 12;
		int   m_AmmoInMag      = 12;
		int   m_AmmoReserve    = 60;
		float m_FireCooldown   = 0.0f;
		float m_FireInterval   = 0.14f;  // seconds between shots
		float m_ReloadTimer    = 0.0f;
		float m_ReloadDuration = 1.2f;
		bool  m_Reloading      = false;
		float m_ProjectileSpeed = 60.0f;
		float m_WeaponDamage    = 34.0f;
	};
}
