# Blu Engine — Known Gotchas, Decisions & Technical Debt

## Critical Architecture Decisions

### Dual Backend (OpenGL + DX11)
Both backends must stay in sync. Every new GPU resource type requires:
1. Abstract base class in `Blu/Rendering/`
2. `OpenGL*` implementation in `Platform/OpenGL/`
3. `D3D11*` implementation in `Platform/DirectX11/`
4. `Type::Create(...)` factory case in the abstract `.cpp`

**Never add OpenGL-specific code paths to shared renderer logic.** If a feature only exists in one backend, gate it behind `RendererAPI::GetAPI()`.

### D3D11 Constant Buffers (Uniforms)
`D3D11Shader` does **not** upload uniforms immediately on `SetUniform*()`. It writes to a CPU shadow buffer and marks the cbuffer slot dirty. Upload happens in `Bind()` via `UploadDirtyCBuffers()`. When adding uniforms to HLSL shaders, shader reflection extracts the layout automatically — no manual registration needed.

HLSL cbuffer slot assignment (`b0`, `b1`, ...):
- `b0` — per-object data (transform, color)
- `b1` — per-scene data (camera ViewProjection)
- Higher slots — material/light data

### D3D11VertexArray and VS Bytecode
`D3D11VertexArray` requires the compiled vertex shader bytecode to build the `InputLayout`. This bytecode is passed via `D3D11Context::SetCurrentVSBytecode()` before `AddVertexBuffer()` is called. **Order matters** — bind shader first, then create vertex array or add vertex buffers.

### Renderer2D Batch Flushing
`Renderer2D` accumulates draw calls into CPU vertex buffers and flushes when:
- The quad/circle/line count exceeds the per-batch maximum
- `EndScene()` / `Flush()` is called explicitly

**Always call `BeginScene()` before any draw calls and `EndScene()` after.** Forgetting `EndScene()` leaves geometry in the CPU buffer and it will appear in the next frame's draw.

### LightManager — Tight Coupling Warning
`LightManager` is a `friend` of `Renderer2D` and writes directly into `Renderer2D`'s internal storage. It is updated per-scene before `Renderer2D::EndScene()`. Do not restructure Renderer2D's internal data layout without updating `LightManager` accordingly.

### Component.h Is a God Header
All component types live in a single `Component.h`. This is intentional (entt pattern) but means any file that touches components recompiles when any component changes. Avoid including `Component.h` in frequently-included headers. Forward-declare where possible; only include in `.cpp` files or scene-layer headers.

### ScriptEngine Lifecycle
Mono runtime is initialized once per application lifetime. `ScriptEngine::Init()` must be called before `Scene::OnRuntimeStart()`. `ScriptEngine::Shutdown()` must be called on application exit. Reloading assemblies during runtime is possible but requires re-calling `OnCreate` on all script components.

### Scene Serialization Format
Scenes are YAML files with `.blu` extension, serialized via `SceneSerializer` (yaml-cpp). Entity UUID is stable across serialize/deserialize. When adding new components, add corresponding `SerializeComponent` / `DeserializeComponent` blocks in `SceneSerializer.cpp`.

### Physics (Box2D) — Runtime Only
`b2World` is created in `Scene::OnRuntimeStart()` and destroyed in `Scene::OnRuntimeStop()`. There is no physics simulation in editor mode. Rigidbody and collider components are just data in editor mode.

### Event System — Not Thread-Safe
`EventDispatcher` is single-threaded. Events dispatched from background threads must be queued to the main thread. Currently all events originate from GLFW callbacks on the main thread, so this is safe — but do not call `Dispatch()` from worker threads.

## Known Limitations / Technical Debt

- **Renderer3D** is stubbed / incomplete. Mesh rendering is minimal and not batched.
- **No asset manager / asset UUID system.** Textures and shaders are loaded by path string at runtime. Refactoring this requires adding a registry before touching the content browser.
- **No hot-reload for GLSL shaders** in release builds. HLSL shaders are compiled at runtime by `d3dcompiler`.
- **ImGui docking** requires the dockspace to be set up in `BluEditorLayer::OnGuiDraw()` before any panels draw. Moving panels to their own `Layer` objects will break the docking layout.
- **OpenGLFrameBuffer** does not support MSAA. `FrameBufferSpecifications::Samples` field exists but is only honored by DX11 path.
- **EditorCamera** pan/orbit/zoom are hardcoded to Alt+LMB/MMB/RMB. Key bindings are not configurable.
- **No undo/redo system.** Any destructive editor action (delete entity, move component) is permanent.
- **ScriptJoiner** bridges C++ ↔ C# internal call registration. Adding new engine API calls to scripts requires registering them in `ScriptJoiner.cpp` via `mono_add_internal_call`.

## Anti-Patterns to Avoid

- Do not use `new`/`delete` for engine objects — use `Shared<T>` / `Unique<T>`.
- Do not include platform headers (`<d3d11.h>`, `<GL/gl.h>`) outside of `Platform/` directories.
- Do not store raw `entt::entity` handles outside of `Entity` wrapper — always pair with the owning `Scene*`.
- Do not call `Renderer2D` draw functions outside `BeginScene()`/`EndScene()` pair.
- Do not add fields to `Component.h` structs without updating the serializer.
- Do not modify `premake5.lua` project structure without regenerating VS solution files.
