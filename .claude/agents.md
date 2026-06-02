# Blu Engine - Agent Decision Rules

## Before Adding Features

1. Search for an existing subsystem, abstraction, or prototype before adding code.
2. Keep GPU API calls inside `Platform/`; shared renderers use backend-neutral interfaces.
3. Keep serializable ECS components as pure data and update scene serialization for persistent fields.
4. Extend `GameFramework` for native gameplay behavior. Do not introduce another scripting stack.
5. Regenerate Visual Studio projects after changing source layout or Premake configuration.

## Native Gameplay

- Extend `UObject`, `AActor`, `APawn`, `ACharacter`, `AGameMode`, and the native class registry.
- Runtime actor instances are owned by scene systems and destroyed through a deferred queue.
- Actor class identifiers are stable namespace-qualified strings.
- Reflection-lite property metadata may expose editable values without code generation.

## Dependency Boundaries

```
Core
  <- Events
  <- Rendering abstractions <- Platform implementations
  <- Scene <- runtime systems
  <- GameFramework
  <- Editor
```

- Rendering abstractions must not include Scene headers.
- Components must not include rendering implementations.
- Platform implementations must not include each other.
- Panels inspect or mutate scene data; render passes belong to the scene or renderer.

## Code Style

- Use `Shared<T>` and `Unique<T>` consistently; avoid unmanaged ownership.
- Log through `BLU_CORE_*` macros.
- Match the existing PCH and naming patterns.
- Add comments only where a design reason is not obvious from the code.
