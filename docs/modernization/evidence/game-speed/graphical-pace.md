# Graphical Game Speed pace (seeded replay, normal rendering client, 1.5 minute light scenario unless "heavy")

| run | logic ticks | wall s | ticks/s | render fps | tick p99 ms | ticks >33.3 ms | fingerprint vs headless |
|---|---:|---:|---:|---:|---:|---:|---|
| zh-gs100 | 2700 | 91.2 | 29.60 | 119.6 | 3.48 | 0 | 90 checkpoints, first divergence -1 |
| zh-gs125 | 2700 | 73.2 | 36.90 | 119.6 | 3.43 | 0 | 90 checkpoints, first divergence -1 |
| zh-gs150 | 2700 | 61.1 | 44.16 | 119.6 | 3.56 | 0 | 90 checkpoints, first divergence -1 |
| zh-gs75 | 2700 | 125.6 | 21.50 | 115.3 | 3.72 | 2 | 90 checkpoints, first divergence -1 |
| zh-gs150-f60 | 2700 | 61.1 | 44.18 | 59.9 | 3.67 | 0 | 90 checkpoints, first divergence -1 |
| zh-gs150-f240 | 2700 | 61.2 | 44.15 | 197.5 | 3.73 | 0 | 90 checkpoints, first divergence -1 |
| zh-gs100-f60 | 2700 | 91.1 | 29.63 | 59.9 | 3.60 | 0 | 90 checkpoints, first divergence -1 |
| gen-gs100 | 2700 | 91.2 | 29.62 | 119.7 | 5.16 | 0 | 90 checkpoints, first divergence -1 |
| gen-gs150 | 2700 | 61.2 | 44.14 | 119.6 | 5.24 | 0 | 90 checkpoints, first divergence -1 |
| heavy-gs150 | 14400 | 322.2 | 44.69 | 114.4 | 18.09 | 3 | 480 checkpoints, first divergence -1 |
| heavy-gs125 | 14400 | 386.3 | 37.28 | 114.1 | 19.77 | 14 | 480 checkpoints, first divergence -1 |
| heavy-gs100 | 14400 | 482.4 | 29.85 | 113.0 | 21.84 | 19 | 480 checkpoints, first divergence -1 |

Labels: zh-/gen- = Zero Hour / Generals, gsNNN = Game Speed percent, fNN = render cap written into the replay header (60 / 240 FPS; 120 otherwise), heavy = 8 minutes with the 4x unit multiplier (GENERALS_SIM_CLONE=3@8000).
