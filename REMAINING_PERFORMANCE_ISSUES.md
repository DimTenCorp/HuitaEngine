# Remaining performance issues after recent optimizations

Date: 2026-04-15

## 1) No visibility culling in world draw loops (high impact)
Both renderers still iterate and submit almost all world draw calls/faces every frame without frustum/PVS filtering in the render path.

- `Renderer::geometryPass` draws all selected draw calls in a tight loop.
- `LightmappedRenderer::renderWorld` traverses `faceDrawCalls` and submits all non-transparent/non-sky faces.

### Why it matters
CPU draw submission cost still scales with total scene complexity, not visible complexity.

---

## 2) Transparent sorting is still full O(n log n) every frame (medium/high impact)
We now reuse sort buffers, but still recompute distances for all transparent draw calls and run full sort each frame.

### Why it matters
For maps with many transparent surfaces this remains a hot CPU path.

---

## 3) Door rendering still streams full geometry each frame (medium impact)
Persistent buffers removed `glGen/glDelete` churn, but each door still uploads full vertex+index arrays every frame via `glBufferSubData`.

### Why it matters
If door geometry is static in local space, best cost is one-time upload + per-door transform only.

---

## 4) Dynamic door collider rebuild still global when any door changes (medium impact)
Current logic skips rebuild if nothing moved, which is good. But once any door changes, it rebuilds triangles for all non-passable doors.

### Why it matters
Could be improved by per-door dirty ranges / partial updates.

---

## 5) Potentially unsafe GL state cache assumptions (correctness/perf risk)
Renderer-side state caches (`depthTestEnabled`, `blendEnabled`, etc.) assume these booleans always match real GL state. If external code changes GL state directly, cache can desync.

### Why it matters
Can cause redundant work at best or incorrect state at worst.

---

## 6) Door-face filtering in lightmapped mesh build is O(Faces * DoorModels) (load-time impact)
During mesh build, each face is checked by scanning all door model indices to determine whether the face belongs to a door model.

### Why it matters
Increases map load time on large maps.

---

## 7) No per-pass GPU timing/profiling instrumentation (observability gap)
There is still no built-in timing around opaque/transparent/doors passes.

### Why it matters
Hard to verify optimization ROI and guard against regressions.

---

## Recommended next fixes
1. Add frustum/PVS culling before draw submission.
2. Use incremental transparent sorting (or binning) and squared-distance keys.
3. Keep door geometry resident (one-time upload per door), draw with transforms only.
4. Move door collider to per-door dirty updates (partial dynamic triangle refresh).
5. Harden GL state cache (explicit invalidation on external state changes).
6. Precompute face->isDoor lookup (bitset/hash) during BSP preprocessing.
7. Add CPU and GPU timers per render stage.
