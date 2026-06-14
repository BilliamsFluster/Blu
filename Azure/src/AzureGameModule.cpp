#include "AzureGameModule.h"
#include "Actors/PlayerCharacter.h"
#include "Actors/ZombieCharacter.h"
#include "Actors/ZombieTestActor.h"
#include "Actors/EnvironmentActor.h"
#include "GameModes/ZombieGameMode.h"
#include "Blu/GameFramework/NativeClassRegistry.h"

namespace Azure
{
	void RegisterAzureGameModule()
	{
		auto& registry = Blu::NativeClassRegistry::Get();
		registry.RegisterActor<PlayerCharacter>(
			"Azure::PlayerCharacter", "Player Character", "Azure", { "PlayerCharacter" });
		registry.RegisterActor<ZombieCharacter>(
			"Azure::ZombieCharacter",
			"Zombie Character",
			"Azure",
			{ "ZombieCharacter" },
			{
				Blu::MakeNativeProperty<ZombieCharacter>("DetectRange", &ZombieCharacter::DetectRange),
				Blu::MakeNativeProperty<ZombieCharacter>("AttackRange", &ZombieCharacter::AttackRange),
				Blu::MakeNativeProperty<ZombieCharacter>("AttackDamage", &ZombieCharacter::AttackDamage)
			});
		registry.RegisterActor<ZombieTestActor>(
			"Azure::ZombieTestActor",
			"Zombie Test Actor",
			"Azure",
			{ "ZombieTestActor" },
			{
				Blu::MakeNativeProperty<ZombieTestActor>("DetectRange", &ZombieTestActor::DetectRange),
				Blu::MakeNativeProperty<ZombieTestActor>("AttackRange", &ZombieTestActor::AttackRange),
				Blu::MakeNativeProperty<ZombieTestActor>("AttackDamage", &ZombieTestActor::AttackDamage),
				Blu::MakeNativeProperty<ZombieTestActor>("AttackCooldownSeconds", &ZombieTestActor::AttackCooldownSeconds)
			});
		registry.RegisterActor<EnvironmentActor>(
			"Azure::EnvironmentActor", "Environment Actor", "Azure", { "EnvironmentActor" });
		registry.RegisterGameMode<ZombieGameMode>(
			"Azure::ZombieGameMode", "Zombie Game Mode", "Azure", { "ZombieGameMode" });
	}
}
