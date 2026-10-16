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
