# Reference token guard baseline (scripts/rendering, scripts/performance)

Status: **conclusively baselined at X64.36 (CP10).** Nine reference-token guards fail on `dev/modern-engine`. None is caused
by CP10; each started failing at a named, accepted milestone commit that intentionally changed the pinned file.

The guards compare whole-file token streams of current sources against a pinned `BASE` commit (plus a declared set of
allowed seams). They were not re-pinned when later accepted milestones legitimately changed those files, so they report the
accepted history rather than a regression. `scripts/native-runtime` (the CP10 contracts and the native workflow tests) is
green; the guards below are the only failing Python suites.

| guard | first failing commit (bisected, step 1 inside the window) | cause |
|---|---|---|
| `test_texture2d_reference.test_shared_dynamic_ui_and_file_asset_producers_are_unchanged` | `66d23d7d1` X64.22 ordinary-lifetime presentation host | retained-UI texture seams added for the native host |
| `test_ui_texture_reference.test_actual_caller_and_entire_legacy_policy_still_frozen` | `66d23d7d1` X64.22 | same |
| `test_render2d_reference.test_dynamic_allocation_lock_and_wrapper_sources_are_unchanged` | `4dcee8c18` R1b/R2a CPU-backed legacy buffers | `dx8vertexbuffer`/`dx8indexbuffer` AMD64 backing |
| `test_texture2d_reference.test_only_render2d_branch_and_single_owner_release_hooks_changed` | `a8781fccd` X64.30 radar dynamic UI texture residency | texture residency hooks |
| `test_ui_texture_reference.test_exact_init_and_install_seams_only` | `a8781fccd` X64.30 | same |
| `test_d3d11_reference.test_reference_backend_contracts_and_producers_are_unchanged` | `10b4503e9` X64.35 attribution | measurement-only phase scopes / labels |
| `test_sentence_content_reference.test_all_sentence_and_real_glyph_tokens_preserved` | `10b4503e9` X64.35 attribution | `render2dsentence` texture labels |
| `test_texture2d_reference.test_sentence_copy_preserves_entire_producer_and_surface_lifetime` | `10b4503e9` | same |
| `test_stage4b1_reference.test_exact_inverse_preserves_entire_pathfinder_token_order` | `10b4503e9` | `PERFORMANCE_PROFILE_SCOPE` around `calculateZones` (measurement only) |

Evidence: the same suites were run at HEAD of X64.35 (`4a8641178`), at `acad721d5` (start of CP10) and at the CP10 working
tree with identical failure sets; all pass at `3b3361b4c`..`1c82677fb` (X64.21 and earlier). CP10 changes none of the files
these guards watch (asserted by `test_cp10_does_not_touch_reference_guarded_sources` in
`scripts/native-runtime/test_enhanced_hud_contract.py`).

Disposition: the guards stay as historical evidence and are **not** relaxed or silenced. Re-pinning each `BASE` to the
accepted state it should now describe is a separate, reviewed change (it must re-derive each guard's allowed-seam list from
the diffs above) and is explicitly out of scope for the HUD work; CP10 acceptance does not depend on it because CP10 is
proven not to touch the guarded files.
