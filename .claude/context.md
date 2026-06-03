# Blu Engine - Architecture Context

## Project Layout

```
Blu/                           # Core engine static library
  engine/src/Blu/
    Core/                      # Application, Window, Input, LayerStack
    Events/                    # Event dispatch
    Rendering/                 # Backend-neutral rendering APIs
    Scene/                     # entt ECS, components, serializer
    GameFramework/             # UObject, AActor, APawn, ACharacter, AGameMode
    Physics/                   # Box2D and Jolt integration
    Audio/                     # AudioEngine
    UI/                        # Runtime HUD documents
    Platform/
      DirectX11/               # Reference renderer backend
      OpenGL/                  # Forward-compatible backend
Blu-Editor/                    # Editor application
Azure/                         # Standalone gameplay application
premake5.lua                   # Workspace and project generation
```

## Core Direction

- Gameplay scripting is native C++. Extend the existing `GameFramework` classes instead of adding a parallel scripting runtime.
- Components are pure serialized data. Runtime actor instances belong to scene-owned systems, not ECS components.
- DX11 is the reference backend for advanced rendering. Preserve OpenGL compilation and its existing forward path.
- Prefer modular services and factories over subsystem-specific shortcuts.

## Rendering Backend Abstraction

Rendering abstractions live under `Blu/Rendering/`; platform API calls stay under `Blu/Platform/`.
Resources are created through backend-dispatch factories based on `RendererAPI::GetAPI()`.

## Scene / ECS Architecture

`Scene` owns the entt registry and runtime systems. `Entity` is a thin handle paired with its owning `Scene*`.
Persist entity references as UUIDs and asset references as stable handles or canonical virtual paths.

## Native Gameplay Architecture

`UObject`, `AActor`, `APawn`, `ACharacter`, and `AGameMode` are the native gameplay foundation.
Actor classes register stable IDs and factories with the native registry. A scene-owned actor system creates
instances, injects entity context, runs lifecycle callbacks, and destroys instances at safe frame boundaries.

## Build System

- Premake5 generates VS2022 projects: `GlobalExternalDependencies/bin/premake/premake5.exe vs2022`.
- C++20 is used across Blu, Blu-Editor, and Azure.
- `Blupch.h` is the engine PCH.
- DirectX shader assets live under `Blu-Editor/assets/shaders/DX11/`.
