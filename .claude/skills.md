# Blu Engine — Developer Skill Recipes

Common tasks and the exact files to touch, in order. Follow these to avoid missing steps.

---

## Add a New ECS Component

1. **`Blu/engine/src/Blu/Scene/Component.h`** — add the struct. Keep it pure data (no logic, no virtual methods).
2. **`Blu/engine/src/Blu/Scene/SceneSerializer.cpp`** — add `SerializeComponent<YourComponent>` and `DeserializeComponent<YourComponent>` blocks.
3. **`Blu-Editor/src/Panels/SceneHierarchyPanel.cpp`** — add UI in `DrawComponents()` to display/edit the new component.
4. **`Blu/engine/src/Blu/Scene/Scene.cpp`** — if the component drives runtime behavior, wire it into `OnUpdateRuntime()` or `OnRuntimeStart()`.
5. If it involves rendering: feed data into `Renderer2D` or `Renderer3D` from `Scene::OnUpdateEditor()`.

---

## Add a New Renderer2D Draw Primitive

1. **`Blu/engine/src/Blu/Rendering/Renderer2D.h`** — declare the static `Draw*()` method.
2. **`Blu/engine/src/Blu/Rendering/Renderer2D.cpp`** — implement it. Follow the existing pattern:
   - Flush if batch limit reached.
   - Write vertices into `s_Data.YourVertexBufferBase`.
   - Increment `s_Data.YourIndexCount`.
3. Add the vertex buffer, index buffer, vertex array, and shader to `Renderer2DStorage` in `Renderer2D.cpp`.
4. Initialize them in `Renderer2D::Init()`.
5. Flush them in `Renderer2D::FlushAndReset()`.

---

## Add a New Shader Uniform (Both Backends)

### OpenGL (GLSL)
1. Add the `uniform` declaration to the `.glsl` file in `Blu-Editor/assets/shaders/`.
2. In `OpenGLShader.cpp`, add the `SetUniform*()` call — it uses `glUniform*` directly via `glGetUniformLocation`.

### DX11 (HLSL)
1. Add the field to the correct `cbuffer` in the `.hlsl` file in `Blu-Editor/assets/shaders/DX11/`.
2. No C++ changes needed — `D3D11Shader` reflects the cbuffer layout automatically at compile time.
3. Call `shader->SetUniform*(name, value)` from the renderer; the CPU shadow + dirty-upload mechanism handles the rest.

---

## Add a New Rendering Backend Resource (Both Backends)

New type (e.g., `ComputeShader`):

1. Create abstract base: `Blu/engine/src/Blu/Rendering/ComputeShader.h`
   - Pure virtual interface matching the pattern of `Shader.h`.
   - Static `Create(...)` factory.
2. Create `Blu/engine/src/Blu/Rendering/ComputeShader.cpp`
   - Implement `Create()` with switch on `RendererAPI::GetAPI()`.
3. Create `Platform/OpenGL/OpenGLComputeShader.h/cpp`
4. Create `Platform/DirectX11/D3D11ComputeShader.h/cpp`
5. Add files to `Blu.vcxproj` and `Blu.vcxproj.filters` (or regenerate via premake).
6. Include the new type in `Blu/engine/src/Blu.h` (the master include).

---

## Add a New Panel to the Editor

1. Create `Blu-Editor/src/Panels/YourPanel.h/cpp`.
2. Inherit from nothing (panels are plain classes, not Layers).
3. Declare `void OnGuiDraw()` — all ImGui calls go here.
4. Add a `YourPanel m_YourPanel` member to `BluEditorLayer`.
5. Call `m_YourPanel.OnGuiDraw()` inside `BluEditorLayer::OnGuiDraw()`.

---

## Extend Scene Serialization

Scenes are YAML via yaml-cpp. `SceneSerializer.cpp` serializes per entity.

```cpp
// Serialize a new component
SerializeComponent<YourComponent>(out, entity, [](YAML::Emitter& out, YourComponent& c) {
    out << YAML::Key << "YourField" << YAML::Value << c.YourField;
});

// Deserialize
DeserializeComponent<YourComponent>(entityNode, entity, [](YAML::Node& node, Entity entity) {
    auto& c = entity.AddComponent<YourComponent>();
    c.YourField = node["YourField"].as<Type>();
});
```

---

## Add a New C# Script API Call (Internal Call)

Internal calls let C# scripts call C++ engine functions.

1. Implement the C++ function in `Blu/engine/src/Blu/Scripting/ScriptJoiner.cpp`.
   - Use `Entity` and `Scene` APIs to do the work.
   - Signature must use Mono-compatible types (primitives, `MonoString*`, structs by pointer).
2. Register it at the end of `ScriptEngine::Init()`:
   ```cpp
   mono_add_internal_call("Blu.InternalCalls::YourMethodName", YourCppFunction);
   ```
3. Declare the matching `[MethodImpl(MethodImplOptions.InternalCall)] extern static` in the C# `InternalCalls` class in `Blu-ScriptCore`.

---

## Add a New Event Type

1. Create the event class in the appropriate header under `Blu/engine/src/Blu/Events/`.
2. Inherit from `Event`.
3. Implement `GetType()`, `GetName()`, `Accept(EventHandler&)`.
4. Add the enum value to `EventType` in `Event.h`.
5. Dispatch via `EventDispatcher::Get().Dispatch(event)` from the source (usually `WindowsWindow` callbacks).

---

## Regenerate Project Files

After any change to `premake5.lua` or adding/removing source files:

```powershell
# From repo root
./vendor/premake/premake5.exe vs2022
```

Then reload the solution in Visual Studio.

---

## Switch Active Rendering Backend

In `RendererAPI.cpp`, change the default:
```cpp
RendererAPI::API RendererAPI::s_API = RendererAPI::API::DirectX11;
// or
RendererAPI::API RendererAPI::s_API = RendererAPI::API::OpenGL;
```

The `GraphicsContext`, `RendererAPI`, and all resource factories will automatically instantiate the matching backend.
