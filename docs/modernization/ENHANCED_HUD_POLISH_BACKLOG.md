# Enhanced HUD - cross-faction polish backlog (deferred)

Status: **deferred by decision.** Do NOT start before USA, China and GLA are all implemented and natively accepted; the final visual
richness of the three factions is compared only then.

Native review (X64.36, China v1): both USA and China "could eventually use some additional visual detail / polish". Candidates for
the later pass (presentation only, through `EnhancedHudTheme` and the single renderer; Shared Framework v1 stays frozen):

* additional material detail and subtle panel depth;
* faction-specific micro-detail (scratches, wear, stamped markings, weld/rivet variation) with restraint;
* improved visual richness consistent across USA / China / GLA;
* a cross-faction consistency review of weights, lamps, state colours and typography accents.

Constraints for that pass: no change to geometry, hit regions, state semantics, Original behaviour or gameplay; readable at 800x600
and UI scales; contract tests keep the renderer faction-blind.

## Renderer backlog

### Helicopter shadow culling (deferred; recorded at the X64.37 closeout, 2026-10-16)

Symptom: a helicopter's projected shadow can disappear when the helicopter itself leaves the visible camera / frustum even though the
shadow footprint on the terrain should still be visible. Likely cause class: render / culling / shadow-ownership (the shadow is submitted
only while its owning drawable passes the visibility test). Related earlier observation: `X64_35_PERFORMANCE_OPTIMIZATION.md` section 22
(helicopter shadows pop in/out, suspected pre-existing in the Win32/DX8 renderer as well).

Not fixed in X64.38 (CPU scalability) unless it blocks profiling. A future fix needs the shadow footprint (not the casting object's bounds)
to drive visibility, with Original behaviour kept as the reference.
