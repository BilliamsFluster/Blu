# Blu Engine - Known Decisions and Technical Debt

## Critical Decisions

### Native Gameplay
Mono and C# support are retired. Extend the existing native `GameFramework` stack. Do not add a second scripting
runtime. Serializable actor components must remain pure data; runtime actor ownership belongs to `ActorSystem`.

### Renderer Backends
DX11 is the reference implementation for deferred rendering and advanced effects. Keep platform API calls inside
their platform directories and preserve the existing OpenGL forward path behind the renderer abstraction.

### D3D11 Constant Buffers
`D3D11Shader` writes uniforms to CPU shadow storage and uploads dirty buffers during `Bind()`. Shader reflection
provides layout data. Bind the shader before constructing D3D11 vertex-array input layouts.

### Scene Serialization
Scene files are YAML with a `.blu` extension. Stable entity UUIDs survive round trips. Every persistent component
field requires corresponding serializer and deserializer support.

### Components
`Component.h` is intentionally centralized. Components are pure data. Do not store owning smart pointers or
long-lived runtime instances in serializable components. Resolve handles in runtime systems.

## Lifetime Rules

- Use `Unique<T>` for exclusive ownership and `Shared<T>` only for real shared ownership.
- Use deferred destruction for scene-owned runtime actors and other frame-visible objects.
- Use typed generational handles for registries where stale-reference detection matters.
- Do not add tracing garbage collection for native objects.

## Known Technical Debt

- The current native actor lifecycle still lives partly inside `Scene` and must move into `ActorSystem`.
- `AssetManager` is incomplete and path handling still needs a mounted filesystem service.
- Runtime materials need asset-backed templates and instances before deferred rendering expands.
- Advanced OpenGL feature parity is deferred; its existing forward path must remain buildable.
