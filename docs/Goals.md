# LightWeightGameFramework Goals

## 1. Product Goal

LightWeightGameFramework is a personal C++ framework for building small 2D games on Windows with low development and runtime overhead.

The goal is not to compete with general-purpose engines such as Unity or Unreal Engine.

Instead, the framework is intended to provide a lightweight alternative for projects where a full editor-driven engine is unnecessary or too costly for the target development environment.

The framework should reduce repeated low-level work such as window management, input handling, rendering setup, resource lifetime management, and other common game infrastructure so that game code can stay focused on game-specific behavior.

---

## 2. Problem Statement

General-purpose game engines provide strong productivity, but their editor and runtime environments may introduce more CPU, memory, storage, and workflow overhead than a small 2D project actually needs.

This project explores how small a practical 2D game development environment can remain while still providing enough structure and convenience to ship a real game.

The framework is therefore a complementary tool rather than a replacement for existing engines.

Use a general-purpose engine when its editor, tooling, ecosystem, or platform support provides meaningful value.

Use LightWeightGameFramework when the project is small, 2D, code-driven, and benefits from a smaller runtime and development footprint.

---

## 3. Target Use Case

The initial target is:

- Single-developer projects
- Windows PC
- Small 2D games
- Code-first development
- Low-spec development and runtime environments
- Standalone executable builds

The first version is not intended to be a general public game engine API.

---

## 4. Core Requirements

The framework should eventually allow a game to:

- Start and shut down an application cleanly
- Create and manage a window
- Run a game loop
- Obtain frame timing information
- Read keyboard and mouse input
- Render basic 2D graphics
- Load and use texture resources
- Play basic audio
- Manage game states or scenes when required
- Produce a Release build that runs without an editor

These requirements are not a checklist that must be implemented in advance.

A feature should be implemented when an actual game or executable milestone requires it.

---

## 5. Non-Goals

The initial framework will not include:

- A custom editor
- A 3D engine
- Visual scripting
- A general-purpose ECS
- A custom physics engine
- A networking framework
- Multiplayer support
- Android support
- iOS support
- Linux support
- A general-purpose UI editor
- A plugin system
- A custom programming language
- A custom audio engine
- Custom asset-authoring tools

These items may only be reconsidered after a real project demonstrates a clear need for them.

---

## 6. Design Principles

### Game First

The framework exists to reduce game development work.

Framework development should not block real game development for long periods.

A preferred workflow is:

```text
Need appears in a game
        ↓
Implement the smallest useful solution
        ↓
Use it in practice
        ↓
Observe repeated or shared problems
        ↓
Generalize only when justified
        ↓
Promote it into the framework
```

### Minimal by Default

Do not add a feature because a game engine is expected to have it.

Before adding a feature, ask whether it:

- Reduces development time
- Reduces repeated work
- Reduces mistakes
- Improves game-side readability or usability
- Is likely to be used in a real project
- Provides enough value to justify the added framework complexity

If the answer is unclear, defer the feature.

### Measure Instead of Assume

The project targets low-spec environments, but optimization decisions must be based on measurement rather than intuition.

When practical, record:

- Idle RAM usage
- Runtime RAM usage
- Idle CPU usage
- Runtime CPU usage
- GPU usage
- Frame time
- FPS
- Startup time
- Executable and required-file size

Performance should be compared before and after meaningful framework changes.

### Avoid Premature Architecture

Do not introduce large architectural systems before a real problem requires them.

In particular, avoid introducing the following by default:

- ECS
- Service Locator
- Dependency Injection frameworks
- Global Event Bus designs
- Excessive interface hierarchies
- Singleton-based system ownership
- Complex multithreading
- Job systems

### Explicit Ownership

Use C++ ownership tools according to actual lifetime and ownership relationships.

Prefer clear ownership, RAII, and predictable resource cleanup.

Avoid shared ownership merely for convenience.

### Simple Game-Side API

Framework internals are secondary to game-side usability.

An abstraction is useful only when it provides one or more of the following:

- Simplifies a complex operation
- Isolates platform-specific behavior
- Removes repeated code
- Reduces error-prone resource handling
- Improves readability of game code

A wrapper that only renames an external API should be questioned.

---

## 7. Initial Technical Direction

The initial technical direction is:

| Area | Choice |
| --- | --- |
| Language | C++20 |
| Platform | Windows |
| Build System | CMake |
| Window / Platform Layer | GLFW |
| Graphics API | OpenGL |
| OpenGL Loader | GLAD family |

Additional libraries should be introduced only when the corresponding feature is actually required.

For example, texture-loading or audio dependencies should not be added before texture loading or audio is needed by the current milestone.

---

## 8. Architecture Boundary

The intended dependency direction is:

```text
Game
 │
 ▼
LightWeightGameFramework
 │
 ├─ Core
 ├─ Graphics
 ├─ Input
 ├─ Resource
 └─ Audio
 │
 ▼
GLFW / OpenGL / Third-Party Libraries
 │
 ▼
Operating System
```

Game code should not repeatedly depend on GLFW, OpenGL, or operating-system details for common framework-level tasks.

This boundary should still remain thin enough to avoid unnecessary abstraction.

---

## 9. Development and Release Policy

Development work is integrated through `develop`.

```text
feature/*
    ↓ Pull Request
 develop
    ↓ Release Pull Request
   main
    ↓
Tag / GitHub Release
```

Branch roles:

- `feature/*`: individual implementation or documentation work
- `develop`: latest integrated development state
- `main`: stable release state

`main` should only be updated when a version is considered ready to release.

The purpose of this separation is to keep experimental or incomplete development work away from the stable release branch.

---

## 10. First Milestone

The first executable milestone is intentionally small.

```text
Program Start
    ↓
Initialize Framework
    ↓
Create Window
    ↓
Create OpenGL Context
    ↓
Run Game Loop
    ↓
Process OS Events
    ↓
Render Frame
    ↓
Exit Cleanly
```

Completion criteria:

- The program starts successfully
- A window is created successfully
- The game loop continues to run
- The application can close through the window close action or an explicit input
- Resources are released cleanly on shutdown
- Debug and Release builds can both be generated
- Baseline RAM and CPU usage are recorded

No texture, audio, scene, ECS, or other higher-level system is required for this milestone.

---

## 11. First Major Completion Criterion

The framework is not considered complete because it reaches a specific feature count.

The first major success criterion is:

> Build a small 2D game using LightWeightGameFramework and run it successfully as a Release build.

The project should then be evaluated against its original purpose:

- Did it reduce repeated low-level work?
- Did it reduce development time?
- Did it reduce mistakes?
- Did game code remain readable?
- Did the framework remain lightweight enough for its target environment?
- Was the framework actually useful while making a real game?

If the answer to these questions is not satisfactory, the framework design should be reconsidered rather than expanded with more features.
