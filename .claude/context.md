# Blu Engine — Architecture Context

## Project Layout

```
Blu/                           # Core engine (StaticLib)
  engine/src/Blu/
    Core/                      # Application, Window, Input, LayerStack, Core.h
    Events/                    # Event, EventDispatcher, Key/Mouse/WindowEvent
    Rendering/                 # All rendering abstractions + Renderer2D/3D
    Scene/                     # ECS (entt), Entity, Components, Serializer
    Scripting/                 # Mono/C# integration: ScriptEngine, ScriptJoiner
    Physics/                   # Physics2D (Box2D wrapper)
    LightSystem/               # LightManager
    ImGui/                     # ImGuiLayer
    Math/                      # GLM wrappers / utilities
    Debug/                     # Instrumentor / profiling
    Platform/
      DirectX11/               # D3D11Context, D3D11RendererAPI, D3D11Shader, etc.
      OpenGL/                  # OpenGLRendererAPI, OpenGLShader, etc.
      Windows/                 # WindowsWindow (GLFW-backed)
  ExternalDependencies/        # glfw, glad, imgui, entt, box2d, mono, glm, yaml, spdlog
Blu-Editor/                    # Editor application (ConsoleApp)
  src/
    BluEditorLayer.h/cpp       # Main editor layer
    Panels/
      SceneHierarchyPanel      # Entity tree + component inspector
      ContentBrowserPanel      # Asset file browser
  assets/
    shaders/                   # GLSL shaders (Renderer2D_Quad, etc.)
    shaders/DX11/              # HLSL shaders for DX11 backend
    scenes/                    # .blu YAML scene files
Blu-ScriptCore/                # C# project: base classes exposed to user scripts
premake5.lua                   # Single root premake file for all projects
```

## Rendering Backend Abstraction

The rendering layer is a **Strategy pattern** selected at startup via `RendererAPI::GetAPI()`.

```
RenderCommand (static facade)
  └─ RendererAPI* (singleton instance)
        ├─ D3D11RendererAPI      (Platform/DirectX11/)
        └─ OpenGLRendererAPI     (Platform/OpenGL/)
```

Every GPU resource type has an abstract base + two concrete implementations:

| Abstraction      | OpenGL impl           | DX11 impl               |
|------------------|-----------------------|-------------------------|
| `Shader`         | `OpenGLShader`        | `D3D11Shader`           |
| `Texture2D`      | `OpenGLTexture2D`     | `D3D11Texture2D`        |
| `VertexBuffer`   | `OpenGLVertexBuffer`  | `D3D11VertexBuffer`     |
| `IndexBuffer`    | `OpenGLIndexBuffer`   | `D3D11IndexBuffer`      |
| `VertexArray`    | `OpenGLVertexArray`   | `D3D11VertexArray`      |
| `FrameBuffer`    | `OpenGLFrameBuffer`   | `D3D11FrameBuffer`      |
| `GraphicsContext`| `OpenGLContext`       | `D3D11Context`          |
| `Material`       | `OpenGLMaterial`      | `D3D11Material`         |

All are created via `Type::Create(...)` factory functions that dispatch on `RendererAPI::GetAPI()`.

## Scene / ECS Architecture

```
Scene
  entt::registry                  # All entity data lives here
  b2World*                        # Box2D physics (null until runtime)
  LightManager                    # Owns point/directional light lists

Entity (thin wrapper)
  entt::entity  (handle)
  Scene*        (owning scene)

Component.h (single header — all component types)
  IDComponent           — UUID
  TagComponent          — name string
  TransformComponent    — position, rotation, scale (GLM)
  SpriteRendererComponent
  CircleRendererComponent
  CameraComponent       — wraps SceneCamera (ProjectionType enum)
  ScriptComponent       — maps to Mono class name
  NativeScriptComponent
  Rigidbody2DComponent
  BoxCollider2DComponent / CircleCollider2DComponent
  PointLightComponent / DirectionalLightComponent
  MeshComponent
```

## Data Flow — One Frame

```
Application::Run()
  ├─ for each Layer (reverse): Layer::OnUpdate(ts)
  │    BluEditorLayer::OnUpdate()
  │      Renderer2D::BeginScene(camera)
  │      Scene::OnUpdateEditor() or Scene::OnUpdateRuntime()
  │        for SpriteRendererComponent → Renderer2D::DrawSprite()
  │        for CircleRendererComponent → Renderer2D::DrawCircle()
  │        for PointLightComponent → LightManager (feeds Renderer2D uniforms)
  │      Renderer2D::EndScene()  ← flush batches
  ├─ ImGuiLayer::Begin()
  │    for each Layer: Layer::OnGuiDraw()  ← panels, dockspace
  │    ImGuiLayer::End()  ← ImGui::Render + platform submit
  └─ Window::OnUpdate()  ← SwapBuffers / GLFW poll
```

## Naming Conventions

| Item | Convention | Example |
|------|------------|---------|
| Classes | PascalCase | `SpriteRendererComponent` |
| Files | PascalCase matching class | `OpenGLShader.cpp` |
| Private members | `m_PascalCase` | `m_VertexArray` |
| Static members | `s_PascalCase` | `s_RendererData` |
| Macros | `BLU_ALL_CAPS` | `BLU_CORE_ASSERT` |
| Component types | suffix `Component` | `TagComponent` |
| Events | suffix `Event` | `KeyPressedEvent` |
| Layers | suffix `Layer` | `ImGuiLayer` |
| Platform impls | prefix `D3D11` or `OpenGL` | `D3D11Shader` |
| Smart pointers | `Shared<T>`, `Unique<T>` | `Shared<Shader>` |

## Namespace

All engine code lives in `namespace Blu`. Subsystems may add inner namespaces:
`Blu::Layers`, `Blu::Events`, `Blu::Utils`, `Blu::Platform`.

## Key Singletons

| Singleton | Access | Purpose |
|-----------|--------|---------|
| `Application` | `Application::Get()` | Main app, window, layer stack |
| `Input` | static methods | Keyboard/mouse polling |
| `EventDispatcher` | `EventDispatcher::Get()` | Global event routing |
| `D3D11Context` | `D3D11Context::Get()` | DX11 device/context/swap chain |
| `ScriptEngine` | static methods | Mono runtime & assembly |

## Build System

- **Premake5** (`premake5.lua`) generates VS2022 solutions.
- **C++20** standard across all projects.
- **PCH:** `Blupch.h` (includes STL, GLM, spdlog, entt, core macros).
- Run `vendor/premake/premake5.exe vs2022` from repo root to regenerate.
- DirectX libs linked: `d3d11`, `dxgi`, `d3dcompiler`, `dxguid`.
- DX11 backend files live in `Blu-Editor/assets/shaders/DX11/` (HLSL).
- OpenGL shaders in `Blu-Editor/assets/shaders/` (GLSL, `.glsl` extension).
