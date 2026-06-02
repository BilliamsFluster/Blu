# Blu Engine - Common Change Recipes

## Add an ECS Component

1. Add the pure-data struct in `Blu/engine/src/Blu/Scene/Component.h`.
2. Add YAML read and write support in `SceneSerializer.cpp`.
3. Add editor controls in `SceneHierarchyPanel.cpp` when the component is user-authored.
4. Resolve runtime behavior in the owning scene system rather than inside the component.

## Add Native Gameplay Behavior

1. Reuse the appropriate `GameFramework` base class.
2. Register a stable namespace-qualified class ID and factory with the native registry.
3. Declare reflection-lite editable properties only when editor overrides are required.
4. Store only the class ID and serialized overrides in the ECS actor component.
5. Let `ActorSystem` own lifecycle and deferred destruction.

## Add a Rendering Resource

1. Add or extend the backend-neutral interface under `Blu/Rendering/`.
2. Implement the resource under `Platform/DirectX11/`.
3. Preserve the OpenGL factory boundary and existing forward compatibility path.
4. Route resource creation through the shared factory.

## Extend Scene Serialization

Persist UUIDs, virtual paths, and asset handles. Do not serialize runtime pointers or smart pointers.

## Regenerate Projects

```powershell
./GlobalExternalDependencies/bin/premake/premake5.exe vs2022
```
