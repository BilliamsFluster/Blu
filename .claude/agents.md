# Blu Engine — Agent Decision Rules

Rules for making good decisions quickly when working on this codebase.

---

## Before Adding Any New Feature

1. **Check for existing abstractions first.** Every GPU resource type already has an abstract base in `Blu/Rendering/`. Never bypass the abstraction by calling OpenGL or D3D11 API directly from renderer-level code.
2. **Check `Component.h` before adding new data to the scene.** If the data belongs to an entity, it goes in a component — not as a field on `Scene`.
3. **Search `Renderer2D.cpp` before writing new draw logic.** Quads, circles, and lines are already batched there. Extending them is cheaper than adding a new draw path.
4. **Check `SceneSerializer.cpp` before adding component fields.** Any component field that must survive save/load needs a serialization entry.
5. **Confirm which backend is active** before assuming a renderer fix is complete. A fix in `OpenGLShader` is not automatically present in `D3D11Shader`.

---

## When Touching Rendering Code

| Task | Files to Touch |
|------|---------------|
| Change a shader uniform | Both `.glsl` + `.hlsl` counterparts |
| Add a draw call | `Renderer2D.h`, `Renderer2D.cpp` (BeginScene/EndScene/FlushAndReset) |
| Add a GPU resource type | Abstract base + `OpenGL*` + `D3D11*` + factory in abstract `.cpp` |
| Change framebuffer format | `OpenGLFrameBuffer.h/cpp` **and** `D3D11FrameBuffer.h/cpp` |
| Add a render pass | Wire into `Scene::OnUpdateEditor()` or `Scene::OnUpdateRuntime()`, not inside panels |

Never call draw functions from panel code (SceneHierarchyPanel, ContentBrowserPanel). Panels inspect and mutate data; rendering is driven by `Scene::OnUpdate*()` and `BluEditorLayer`.

---

## When Touching the Scene / ECS

- Components are **pure data**. No virtual methods, no logic, no includes of engine systems.
- Do not store `Shared<T>` inside a component if it will be serialized — use paths/UUIDs and resolve at runtime.
- `entt::registry` owns component data; `Entity` is just a typed handle. Never cache component references across frames — `entt` may move them after structural changes.
- Physics (`b2Body*`) pointers stored in runtime components (`Rigidbody2DComponent::RuntimeBody`) are **only valid between `OnRuntimeStart()` and `OnRuntimeStop()`**.

---

## When Touching the Scripting System

- `ScriptEngine` is initialized **once** per app run. Do not call `Init()` again at runtime.
- Adding a new internal call requires changes in **both** `ScriptJoiner.cpp` (C++) and `Blu-ScriptCore` (C#). Forgetting either side causes a `mono_get_exception_missing_method` crash.
- Script instances are created in `Scene::OnRuntimeStart()` via `ScriptEngine::InstantiateClass()`. Modifying script instance fields after instantiation requires going through `ScriptInstance::SetFieldValue()`.

---

## Dependency Rules (What May Include What)

```
Core (Application, Window, Input, Layer)
  ← Events
  ← Rendering abstractions (no platform headers)
       ← Platform implementations (may include <d3d11.h>, <glad/glad.h>)
  ← Scene (may include Rendering abstractions + Components)
       ← Scripting (may include Scene, not Rendering directly)
  ← LightSystem (may include Rendering abstractions + Components)
  ← Editor (BluEditorLayer, Panels — may include everything)
```

**Illegal directions:**
- Rendering abstractions must NOT include Scene or Scripting headers.
- Components must NOT include Rendering abstractions (use forward declarations; resolve in Scene/Renderer).
- Platform implementations must NOT include each other (no OpenGL inside D3D11 files).
- Events must NOT include Core/Application.

---

## Avoiding Duplicate Functionality

| If you want to... | Use this, not a new impl |
|-------------------|--------------------------|
| Load a texture | `Texture2D::Create(path)` |
| Compile a shader | `Shader::Create(path)` or `ShaderLibrary::Load()` |
| Draw a colored quad | `Renderer2D::DrawQuad()` |
| Draw a sprite | `Renderer2D::DrawSprite()` |
| Read a pixel from framebuffer | `FrameBuffer::ReadPixel()` |
| Get the active camera VP matrix | `EditorCamera::GetViewProjection()` or `SceneCamera` via `CameraComponent` |
| Dispatch a game event | `EventDispatcher::Get().Dispatch()` |
| Serialize a YAML value | `YAML::Emitter` via existing `SceneSerializer` helpers |
| Create an entity | `Scene::CreateEntity(name)` — never construct `entt::entity` directly |

---

## Circular Dependency Watchlist

These pairs are currently safe but fragile:

| Pair | How it stays safe | Risk if broken |
|------|-------------------|----------------|
| `Renderer2D` ↔ `LightManager` | Friendship, no mutual includes | Medium — LightManager writes to Renderer2D internals |
| `Scene` ↔ `ScriptEngine` | Forward declarations only | High — full include in either direction causes a cycle |
| `Component.h` ↔ `Rendering/` | Components use forward-declared types | Medium — adding a Shared<Texture> member to a component pulls in Texture.h |
| `Entity` ↔ `Scene` | Entity holds `Scene*`, Scene stores entt registry | Low — already tightly coupled by design |

When you add an `#include` to any of these files, trace both directions before committing.

---

## Code Style Rules

- Use `Shared<T>` / `Unique<T>` — never raw `new`/`delete` for engine objects.
- Use `BLU_CORE_ASSERT(cond, msg)` for internal invariants; `BLU_ASSERT` for client-side assertions.
- Log via `BLU_CORE_*` macros (spdlog under the hood): `BLU_CORE_INFO`, `BLU_CORE_WARN`, `BLU_CORE_ERROR`.
- Match existing file: if a `.cpp` uses a PCH (`#include "Blupch.h"` first), keep it.
- No comments explaining what the code does — only why, when non-obvious.
- C++20 features are available (concepts, ranges, designated initializers) but prefer patterns already established in the codebase.

---

## Before Reporting a Task Complete

- [ ] Did the change affect both rendering backends (if rendering-related)?
- [ ] Did the change add a component field that needs serialization?
- [ ] Did the change add a script API call that needs C# declaration?
- [ ] Did the change add a source file that needs to be in `premake5.lua` (or `.vcxproj`)?
- [ ] Does the code introduce any new include in the direction of a known circular dependency risk?
