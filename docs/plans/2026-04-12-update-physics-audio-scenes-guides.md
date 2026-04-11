# [Task 7] Physics, Audio, and Scenes Guides Expansion

> **For Gemini:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Expand and refine the documentation for Physics, Audio, and Scenes in the Helios engine book.

**Architecture:** Use information gathered from the codebase and existing documentation to provide a comprehensive guide on auxiliary systems. Ensure technical accuracy and provide clear C++20 code examples.

**Tech Stack:** C++20, Jolt Physics, SoLoud, YAML-cpp.

---

### Task 1: Expand Physics Guide

**Files:**
- Modify: `docs/book/physics.md`

**Step 1: Update Physics Guide content**

Replace the content of `docs/book/physics.md` with detailed sections on Jolt integration, RigidBody types, Colliders, FixedUpdate, ContactEvents, and Raycasting.

**Step 2: Commit**

```bash
git add docs/book/physics.md
git commit -m "docs: expand physics guide with Jolt integration and code examples"
```

### Task 2: Expand Audio Guide

**Files:**
- Modify: `docs/book/audio.md`

**Step 1: Update Audio Guide content**

Replace the content of `docs/book/audio.md` with detailed sections on SoLoud integration, AudioSource components, global/spatial playback, listener updates, and asset import.

**Step 2: Commit**

```bash
git add docs/book/audio.md
git commit -m "docs: expand audio guide with SoLoud integration and spatial audio"
```

### Task 3: Expand Scenes Guide

**Files:**
- Modify: `docs/book/scenes.md`

**Step 1: Update Scenes Guide content**

Replace the content of `docs/book/scenes.md` with detailed sections on YAML serialization, SceneRoot, SceneManager vs SceneSerializer, loading vs instantiating, and .hvescn structure.

**Step 2: Commit**

```bash
git add docs/book/scenes.md
git commit -m "docs: expand scenes guide with YAML serialization and instantiation"
```
