# X64.31 first normal launch (candidate x64-31-normal-launch)

Single developer run, 2026-10-14. Process exit code 0, no timeout, about 35 s of wall time, 204 checkpoints
in `generals64.log` (normal_launch ... engine_init, TheAudio.init ~2.2 s, terrain_bridges_load, engine_cleanup,
display_detach, app_window_release, minidumper_shutdown, WinMain exit). Clean shutdown, no dumps.

The wrapper then failed its post-run stage integrity check (`Runtime changed: Added: generals64.log`) because
the log is created by design next to the exe. This was a workflow defect, not a game failure. Fixed in the
follow-up commit: the log is an explicitly allow-listed, preserved-then-removed artifact
(`Restore-NativeRuntimeGeneratedFiles`); every other mutation still fails (`Test-NativeNormalLaunchWorkflow.ps1`).
The log only records startup/shutdown phases, so it does not show what was played.
