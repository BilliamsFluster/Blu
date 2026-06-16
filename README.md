# Blu

**Blu is a from-scratch game engine written in modern C++ (C++20), built around a DirectX 11
renderer.** It started as a way for me to really understand how game engines work under the
hood — not by reading about render graphs and ECS and asset pipelines, but by building them —
and it has grown into something I can actually make a game with. The proof is shipped
alongside it: **Azure**, a playable first-person, wave-based zombie shooter that runs on the
engine and doubles as the reference for how to build a game on top of Blu.

![Blu](https://github.com/BilliamsFluster/Blu/assets/50932749/783be712-06f2-4594-85a3-c47c17b4fd0b)

---

## What Blu is (and what it's for)

Blu is a Windows-first, editor-driven engine in the spirit of the engines you already know:
you open the editor, compose a scene out of entities and components, import meshes and
textures, tweak lighting, hit **Play**, and iterate. Under that familiar surface, everything
is hand-rolled C++ — the renderer, the entity-component system, the asset database, the
physics and audio integration, the animation system, and the editor itself.

The goal has always been twofold:

1. **Learn by building.** Every subsystem here exists because I wanted to understand it
   deeply — deferred shading, cascaded shadow maps, image-based lighting, GPU skinning,
   UUID-based asset handles, scene serialization, a native gameplay framework. If something
   is in Blu, it's because I built it, not because I pulled in a library to hide it.
2. **Actually make games.** Blu is past the "spinning cube" stage. It runs a real game with
   first-person controls, projectiles, enemy AI, a HUD, audio, and a main-menu-to-level flow.
   Azure is the worked example; the engine is general enough to build other things.

If you're here to read code and see how a small engine is wired together end to end, start
with [the engine source](Blu/engine/src/Blu) and the [editor](Blu-Editor/src). If you want to
build a game, read [Building your own game](#building-your-own-game) below.

---

## Highlights

**Rendering (DirectX 11)**
- Forward **and** deferred render paths, switchable per scene.
- Physically based shading (PBR) with **image-based lighting** — irradiance, prefiltered
  environment, and a split-sum BRDF LUT computed from an HDR environment map.
- **Cascaded shadow maps** (3 cascades via a `Texture2DArray`) for the sun.
- A material system with blend modes (Opaque/Masked/Transparent/Additive), two-sided
  surfaces, an Unlit model, and a node-based material graph; materials persist as `.blumat`
  assets.
- Post-processing stack: **bloom, SSAO, FXAA, ACES tonemapping**, plus exponential height fog.
- A **time-of-day** system driving a procedural Preetham sky with analytical clouds.
- Many dynamic lights (directional, point, spot) assembled per frame, plus transient lights
  for effects like muzzle flashes.
- A batched **2D renderer** (sprites, circles, lines, text) for UI and overlays.
- GPU-instanced foliage and a heightfield **terrain** system with wind deformation.

**World, ECS, and serialization**
- An [entt](https://github.com/skypjack/entt)-based entity-component system.
- Human-readable **YAML scene files (`.blu`)** with a versioned format and back-compat
  handling, written and read by [`SceneSerializer`](Blu/engine/src/Blu/Scene/SceneSerializer.cpp).
- A native gameplay framework with an Unreal-flavored hierarchy:
  `UObject → AActor → APawn → ACharacter`, plus `AGameMode`, `ActorSystem`, and a
  `NativeClassRegistry` so C++ classes are spawnable and serializable by name.

**Physics, audio, animation**
- **3D physics** via [Jolt](https://github.com/jrouwe/JoltPhysics) — box/sphere/capsule/mesh
  shapes, a `CharacterVirtual` controller, and raycasts (used for ballistics).
- **2D physics** via Box2D.
- **3D spatial audio** via [miniaudio](https://github.com/mackron/miniaudio) — positional
  sources with attenuation, looping, pitch/volume control.
- **Skeletal animation** with GPU skinning (up to 128 bones), driven by clips imported
  alongside meshes.

**Asset pipeline**
- A UUID **handle-based asset database** ([`AssetManager`](Blu/engine/src/Blu/Rendering/AssetManager.h))
  with `.meta` sidecar files, so asset references survive renames, moves, and reimports.
- Model import through [Assimp](https://github.com/assimp/assimp) (`.fbx`, `.gltf`, etc.),
  with import settings (scale, LOD flags) stored per asset.
- A virtual file system with mounts (`project://`, `engine://`, `editor://`, `cache://`).

**Editor**
- A dockable [Dear ImGui](https://github.com/ocornut/imgui) interface: Viewport (with
  [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) transform gizmos), Outliner,
  Inspector/Details, Content Browser, Output Log, and Rendering/Diagnostics panels.
- Drag-and-drop model import, in-editor Play/Stop, prefab authoring (`.bluprefab`), and a
  procedural-thumbnail content browser.

---

## Getting started

### Prerequisites

- **Windows 10 or 11**
- **Visual Studio 2022** with the *Desktop development with C++* workload
- The **Windows 10 SDK** (10.0 or newer) — included with the C++ workload
- **Git** (you'll need submodules)

DirectX 11 ships with Windows, so there's nothing to install for it. You don't need Python or
CMake — `premake5` is vendored in the repo.

### Clone (with submodules)

Blu depends on several third-party libraries pulled in as git submodules (glm, Dear ImGui,
yaml-cpp, ImGuizmo, Box2D, Assimp, and Jolt). Clone recursively so they come along:

```sh
git clone --recursive https://github.com/BilliamsFluster/Blu.git
```

Already cloned without `--recursive`? Pull them in after the fact:

```sh
git submodule update --init --recursive
```

### Generate the Visual Studio solution

From the repo root, run:

```sh
GenerateProjects.bat
```

This invokes the vendored `premake5` and produces **`Blu.sln`** plus the per-project files for
Visual Studio 2022.

### Build and run

1. Open **`Blu.sln`** in Visual Studio 2022.
2. **Blu-Editor** is already set as the startup project.
3. Pick a configuration: **Debug**, **Release**, or **Dist** — all **x64**.
4. Press **F5** (or Ctrl+F5) to build and launch.

> **Working directory note:** the editor loads its fonts, shaders, and `assets/` folder from
> paths relative to the working directory. Launching from Visual Studio sets the working
> directory to the `Blu-Editor/` project folder automatically, which is where those live. If
> you run `Blu-Editor.exe` directly from `bin/…`, run it **from the `Blu-Editor/` directory**
> (or copy `assets/` next to the exe).

**Configurations at a glance**
- **Debug** — full symbols, asserts on, Jolt debug renderer and assertions enabled.
- **Release** — optimized, asserts off, Jolt profiling enabled.
- **Dist** — optimized shipping build.

---

## Using the editor

A typical loop in the Blu editor:

1. **Make or open a scene.** *File → New* gives you a starter scene (sun, ground, camera);
   *File → Open* loads a `.blu`. Scenes live in [`Blu-Editor/assets/scenes`](Blu-Editor/assets/scenes).
2. **Populate it.** Create entities from the **Outliner**, and add components in the
   **Details/Inspector** panel (mesh, transform, lights, colliders, audio, scripts/actors…).
   Components are grouped into collapsible categories.
3. **Import assets.** Drag a model from the **Content Browser** into the viewport to spawn it;
   skeletal meshes get an animator automatically. Imports get a `.meta` sidecar with a stable
   UUID. Drag textures/materials to assign them.
4. **Transform with gizmos.** `W` = translate, `E` = rotate, `R` = scale (`Q` returns to
   select). Snapping toggles live in the viewport toolbar.
5. **Light and dress the scene.** The **Rendering** panel controls the sun, time-of-day,
   shadows, fog, post-processing, and the skybox.
6. **Play.** Hit **Play** to copy the scene and run physics + gameplay live (mouse is
   captured; the runtime HUD shows). **Stop** restores your edit scene untouched.
7. **Save** with *File → Save* — back out to YAML.

Asset/file types you'll see: **`.blu`** (scene), **`.blumat`** (material), **`.bluprefab`**
(prefab), **`.bluui`** (runtime UI document), and **`.meta`** (asset sidecar, hidden in the
browser).

---

## The Azure sample game

[`Azure/`](Azure/src) is a complete first-person, wave-based zombie shooter built entirely on
Blu — the best place to see the engine used in anger:

- **First-person** camera and movement (WASD, mouse-look, sprint, jump) on a Jolt character
  controller, with view-model arms and a weapon.
- A **projectile weapon** with a magazine, reserve ammo, reload, fire cooldown, and damage,
  fired down the view ray.
- **Zombies** with health that chase the player, take damage, and die; a wave-based
  [`ZombieGameMode`](Azure/src/GameModes/ZombieGameMode.h) escalates enemy counts each wave,
  with win/lose conditions.
- A **HUD** (ammo readout, reticle, hitmarker, player stats) bound through the runtime UI.
- **Scene flow** — a clickable main menu transitions to the level and back via `SceneManager`.

You can play it two ways:
- **Inside the editor** — open a gameplay scene (e.g. the zombie demo) and press Play.
- **Standalone** — build the **`Azure`** project (a console/windowed app) and run it. It
  resolves its startup scene from [`Azure/Game.config`](Azure) (`StartupScene: <path>`), which
  a command-line `.blu` argument can override.

---

## Projects (`--project`)

Blu has the beginnings of a project system. A **project** is a folder with its own assets and
scenes plus a small **`.bluproj`** manifest at its root:

```yaml
Project:
  Name: MyGame
  AssetsDirectory: assets
  StartupScene: assets/scenes/Main.blu
```

Launch the editor against one with:

```sh
Blu-Editor.exe --project path/to/MyGame
```

(you can pass the folder or the `.bluproj` file directly). On launch the editor re-points its
`project://` and `cache://` virtual mounts at that folder — so asset resolution and the asset
registry become project-scoped — and opens the project's `StartupScene`. Without `--project`,
the editor behaves exactly as it always has.

> This is the foundation for a future launcher/hub. Two things are still bundled rather than
> project-scoped today: the Content Browser still shows the editor's built-in `assets/` folder,
> and gameplay modules are compiled into the editor (see [Building your own game](#building-your-own-game)).

---

## Building your own game

You don't edit the engine to make a game — you build a small gameplay module against it, the
same way Azure does. The pattern (mirrored from the `Azure-Game` project in
[`premake5.lua`](premake5.lua)):

1. **Create a gameplay static library** with your `AActor` / `APawn` / `ACharacter` and
   `AGameMode` subclasses (see [`AActor`](Blu/engine/src/Blu/GameFramework) for the lifecycle:
   `BeginPlay` / `Tick` / `EndPlay`).
2. **Register your classes** in a `RegisterMyGameModule()` function using the
   `NativeClassRegistry` — that makes them spawnable and serializable by name from scenes.
3. **Author scenes** in the editor, dropping your actors in via their registered class IDs.
4. **Ship a runtime** (optional): an Azure-style console/windowed app that pushes a game layer,
   registers your module, and reads a `Game.config` for the startup scene. Or just keep
   iterating inside the editor's Play mode.

Because the engine (`Blu`), the gameplay code (`Azure-Game`), and the runtime (`Azure`) are
already separate projects, your game is a sibling of `Azure-Game`, not a fork of the engine.

---

## Repository layout

| Path | What's there |
| --- | --- |
| [`Blu/engine/src/Blu`](Blu/engine/src/Blu) | The engine: Core, Rendering, Scene/ECS, GameFramework, Physics, Audio, UI, Platform/DirectX11 |
| [`Blu/engine/ExternalDependencies`](Blu/engine/ExternalDependencies) | Vendored/submodule third-party libs (Assimp, Box2D, ImGui, Jolt, yaml-cpp, GLFW, …) |
| [`Blu-Editor`](Blu-Editor) | The editor app, its panels, and `assets/` (scenes, textures, shaders, fonts, HDRs, prefabs) |
| [`Azure`](Azure) | The standalone game runtime (`GameLayer`, entry point, `Game.config`) |
| [`Azure-Game`](Azure-Game) | The Azure gameplay module — actors, game modes, registration (built as a static lib) |
| [`Blu-Tests`](Blu-Tests) | Headless console test suite (no GPU required) |
| [`premake5.lua`](premake5.lua) / [`GenerateProjects.bat`](GenerateProjects.bat) | Project generation |

---

## Headless screenshot harness

The editor can render a single offscreen frame of any scene to a PNG and exit — handy for
quick visual checks, regression captures, and CI:

```sh
Blu-Editor.exe --screenshot <scene.blu> <out.png> [width height] [--play] [--deferred]
```

`--play` runs the gameplay path (first-person camera, HUD); `--deferred` forces the deferred
render path. Run it from the `Blu-Editor/` directory so assets resolve.

---

## Tests

[`Blu-Tests`](Blu-Tests) is a console executable that runs headless (no GPU). It covers the
parts of the engine that are easy to get subtly wrong: actor lifecycle and deferred
destruction, **scene serialization round-trips** (serialize → deserialize → serialize is
byte-stable), scene versioning / legacy-scene upgrades, and **stable asset handles** across
registry loss. Build and run the `Blu-Tests` project to execute the suite.

---

## Project status & roadmap

Blu is under active development and honest about where it is:

- **DirectX 11 is the supported backend.** An OpenGL platform layer exists but is dormant and
  not on the maintained path.
- **C# scripting has been removed** — gameplay is pure C++ via the native class registry.
- **In progress / deferred:** screen-space god rays, deferred decals (bullet holes / blood),
  a GPU particle system, mesh LOD generation, and routing all material rendering through the
  handle-based `.blumat` path. These are scaffolded in places but not finished.

A natural next step is an **Unreal-style project hub/launcher** — pick or create a project, each
opening the editor against its own assets and gameplay module. The groundwork is already here
(the engine / gameplay-lib / runtime split, the `FileSystemService` mounts, and `Game.config`),
so it's an extension rather than a rewrite. The first piece exists today: see
[Projects (`--project`)](#projects---project). Still to come: a project-scoped Content Browser
root and a visual launcher window.

---

## License

See [`LICENSE`](LICENSE).
