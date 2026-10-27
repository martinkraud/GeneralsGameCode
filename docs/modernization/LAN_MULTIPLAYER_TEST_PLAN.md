# LAN multiplayer test plan (two PCs) - evidence first, no new multiplayer features

Status: **PREPARED, NOT RUN.** Build under test: `a885d798f` (UI.3 distribution; HEAD of `origin/dev/modern-engine` at preparation time is a documentation / tooling commit on top of it, the executables are unchanged).
Rule of this test: nothing in the game is changed to make the test pass. Every failure is recorded with evidence and classified (section 8); fixes are a later decision.

## 1. What must be true before a result counts (build identity)

Both PCs must run **byte-identical executables** and **compatible game data**. The validated executables (identical inside the ZIPs and inside the staged `dist-a885d798f-*` candidates, verified on the development PC):

| Title | Executable | Size (B) | SHA-256 |
|---|---|---:|---|
| Generals | `Generals.exe` (ZIP / Steam overlay) = `generalsv.exe` (dev stage) | 8 799 744 | `C182DF6F0F2705EADF00297D4B36521014BEB2A01A01DB15D4B17A0F7429B07C` |
| Zero Hour | `Generals.exe` (ZIP / Steam overlay) = `generalszh.exe` (dev stage) | 9 156 096 | `ECBDB6D4044485E8ED98E3AD577BAE17CE0E7FDCDAAC66774DE44CF4D923BB42` |

* Desktop: Steam installation + the ZIP (`MKRaudstein-<Title>-x64-Preview-a885d798f.zip`) extracted over it (the executable is then `Generals.exe`).
* Laptop: the staged dev-runtime `build\dev-runtimes\native\<Title>\dist-a885d798f-<gen|zh>\game\` (executable `generalsv.exe` / `generalszh.exe`) - **or** the same ZIP. Any other stage (`x64-34-...`, `x64-35-...`, `x64-20-fix1`, a stage built from a newer commit) is a different build and invalidates the test.
* **Proof**: run `scripts\native-runtime\Get-LanTestFingerprint.ps1` on each PC (section 5, step A1). It prints the executable SHA-256 with a MATCHES / DIFFERS verdict, the x64 check, the overlay-file hashes (the Generals overlay replaces `CommandButton.ini`, `CommandSet.ini`, `PlayerTemplate.ini` and three `GenPowersShortcutBar*.wnd`: they enter the Generals INI CRC), every `.big` size (game data version) and the network / firewall state. The two reports must agree line for line except machine names and IPs.
* Game data: both PCs must have the same Steam release (this test assumes the current English Steam builds; the development PC's `.big` sizes are in its fingerprint, e.g. `INIZH.big 18 764 687`). Loose INI / map / script files in the game folder change the INI / map CRC: the fingerprint lists them (`Data\Scripts` holds 3 files in the dev stage; the Steam installation should show the same).

## 2. How LAN works today (code audit, read-only)

| Topic | Finding (source) |
|---|---|
| Entry | Modern main menu Multiplayer > **Network** pushes the retail `Menus/LanLobbyMenu.wnd` (`MainMenu.cpp`, `Action::Network`). The LAN lobby, host / join / direct-connect and the LAN game options screens are the **retail screens** (not migrated to the Modern look). Multiplayer > Online is the dead GameSpy path (not part of this test). |
| Transport | Plain UDP (`Transport.cpp`, `udp.cpp`, `LANAPI.cpp`). **Port 8086** = lobby / discovery (`lobbyPort`), **port 8088** = game traffic (`NETWORK_BASE_PORT_NUMBER`). Discovery is a **limited broadcast** (`255.255.255.255`) from the interface chosen in Options. |
| Which interface | Options > **Network** (retail block inside the Modern Options "Network" category) > *LAN IP* combo; stored in `Options.ini` as `IPAddress`. Enumerated with `gethostbyname` (`IPEnumeration.cpp`): VPN / Hyper-V / WSL / virtual adapters appear too - pick the real LAN one on both PCs. |
| Lobby protocol | `LANMessage` (packed, fixed-size UDP datagrams): game list, request join, join accept / deny, slot / option changes (`GameOptions` string), chat, map status (map CRC + size per slot), accept (ready) flags, game start countdown. |
| Direct connect | `Menus/NetworkDirectConnect.wnd`: shows the local IP, host / join by typing the remote IP (no broadcast needed). The documented fallback when discovery fails. |
| In-match sync | Deterministic lockstep (`Network.cpp`, `ConnectionManager.cpp`, `FrameData*.cpp`): only commands travel; every client simulates. The game-logic CRC is exchanged and compared every **100 frames** (`NET_CRC_INTERVAL = 100` in Release; about every 3.3 s at 30 logic frames per second). On a difference every client gets `Menus/CRCMismatch.wnd` and the game ends (`Network::setSawCRCMismatch`, `GameLogic::update`); the replay header records the desync flag (`RecorderClass::logCRCMismatch`). |
| Disconnect | `DisconnectManager.cpp`: a silent player is shown in the disconnect menu with a countdown (`m_networkPlayerTimeoutTime` = **60 000 ms**); the others may vote the player out; a router / packet-router timeout is handled the same way. There is **no reconnect** in retail. |
| Recording | Every match (also LAN) is recorded to `<user data>\Replays\Last Replay.rep` and **overwritten by the next match**. The headless simulator can re-run it. |
| Logging in Release | The a885d798f executables are Release builds: **`DEBUG_LOG` is compiled out**, so there is no network log. The only text logs are `generals64.log` (next to the executable: launch / display / audio / shutdown checkpoints, no network events) and crash dumps (`<user data>\CrashDumps\*.dmp`). Network evidence therefore comes from: replays, screenshots of the dialogs, Windows network state, optional packet captures. This is a known limit of the test, not something to fix now. |
| Debug switches that exist (not used by default) | `-NetCRCInterval <n>` (check every n frames; 1 = every frame), `-DebugCRCFromFrame 0 -LogObjectCRCs -VerifyClientCRC -SaveDebugCRCPerFrame <dir>` (CRC logs) - these are `DEBUG_CRC` / debug-build features; check whether the command line parser accepts them in this Release build before relying on them (they are listed under `#if` in `CommandLine.cpp`). Not part of the standard procedure. |

## 3. Version / compatibility checks that exist (exact behaviour)

1. **Join check (Generals only, active in Release)**: `LANAPI::handleRequestJoin` compares the joiner's `exeCRC` and `iniCRC` with the host's. `exeCRC` = CRC of the **running executable file** (`GlobalData::generateExeCRC`, `GetModuleFileName`); `iniCRC` = CRC of the loaded INI data. A difference -> `MSG_JOIN_DENY` reason `RET_CRC_MISMATCH` (dialog text key `LAN:ErrorCRCMismatch`). Consequences: Generals clients must run the same executable **and** the same INI data (the Generals overlay INI files count); a retail 32-bit client cannot join a Modern Engine host and vice versa.
2. **Join check in Zero Hour: NOT enforced.** The same block is compiled out with `#if !RTS_ZEROHOUR` and a `TheSuperHackers @todo Enable CRC checks!` comment. A Zero Hour client with a different executable or INI data **can join**; the mismatch would then show up as a desync (item 4) - or silently, if the logic still agrees.
3. **Map check**: each client reports map CRC + size (`MapStatus`); a client without the map is flagged and the host cannot start (map is selected by the host from his list).
4. **In-match CRC check** (both titles): section 2, every 100 frames.
5. **Other join denials**: game full, game already started, duplicate player name, game gone, busy, timeout (`LANAPI::ReturnType`); the duplicate-serial check is disabled (`#if 0`).
6. **There is no Modern Engine version / protocol version check.** Nothing compares commit, build number or feature set; two different Modern builds are accepted by Zero Hour and, in Generals, only rejected if the executable or INI CRC differs. Documented only, not to be implemented now.
7. **Lobby options that travel**: the host's `GameOptions` string carries slots, map, Starting Cash, the retail "limit superweapons" flag and - when set - the Modern `AI Behaviour` and `Superweapon Limit` keys (`GameInfoToAsciiString`). **The LAN game options screen has no AI Behaviour or Superweapon Limit control** (only the retail Starting Cash combo and the Limit Superweapons check box), so a LAN match runs with the defaults Original / Unlimited. Expected result for those two test lines: *not offered*, defaults in force. Anything else is a finding.

## 4. Known risks to watch (hypotheses, not facts)
* Two different CPUs run the same deterministic simulation: the build is MSVC x64 SSE2 (no x87), which should agree; a floating-point difference between the Intel desktop and the laptop CPU would appear as an early CRC mismatch.
* Modern presentation features (frame-rate limit, interpolation, UI scale, camera, Modern Options) are local-only by design; they must **not** change the result. Use the same frame-rate limit on both PCs for the baseline, then change it on one PC in a separate case (D6).
* Windows firewall / "Public" network profile blocking UDP 8086 / 8088; several network adapters; VPN.
* The LAN screens are untouched retail code compiled for x64 for the first time in a real two-machine test (padding / pointer-size bugs would show as garbled lobby data or crashes).

## 5. Procedure

Use a shared note (paper / phone) with the **wall-clock time** (to the second) of every event. Test **Zero Hour first** (cases A-E, about 60 min), then **Generals** with the shorter set (A, B, C, D1-D4, E1, E7).
HOST = the PC that creates the game (desktop first; repeat the lobby cases B1-B3 with the laptop hosting once). CLIENT = the other PC.
`<game>` = the game folder (Steam folder with the overlay, or the dev stage `...\game`).

### A. Preparation (both PCs, ~15 min)
* **A0 Hygiene.** Close the game. In Steam, do not let it auto-update during the test (offline mode or pause updates). Back up `Documents\Command and Conquer Generals Zero Hour Data` (and `... Generals Data`) by copying the folder - the test does not change it, the backup is for safety. Do not delete anything under `Replays` / `CrashDumps`.
* **A1 Fingerprint.** Copy `scripts\native-runtime\Get-LanTestFingerprint.ps1` and `Collect-LanTestEvidence.ps1` to `C:\lan-test\` on both PCs (USB / network share). Run on each PC (Windows PowerShell is enough):
  `powershell -ExecutionPolicy Bypass -File C:\lan-test\Get-LanTestFingerprint.ps1 -GameDir "<game>" -Role Host -OutDir C:\lan-test` (use `-Role Client` on the other). Both reports must say `exe verdict : MATCHES`, `PE machine : x64`, identical `.big` sizes. **If either says DIFFERS stop: the PCs do not run the validated build and no multiplayer result is valid.** Keep both reports.
* **A2 Network.** Both PCs on the same router / subnet (e.g. 192.168.0.x), Wi-Fi client isolation off, no VPN. Set the network profile to **Private**. First game start: when Windows asks, **allow Generals.exe / generalszh.exe on Private networks**. Sync clocks: `w32tm /resync` (admin) or note the offset.
* **A3 LAN IP.** Start the game on each PC (Zero Hour), Options > **Network** tab > *LAN IP*: select the real LAN address (the one shown in the fingerprint, not a virtual adapter), **Apply**. Both PCs. (This writes `Options.ini`; it is the only settings change.)
* **A4 Solo sanity.** On each PC: Skirmish, any map, 2 minutes, exit. Proves the install works before blaming the network.
* **A5 Optional packet capture** (recommended for the first run; skip if it is a hassle): Wireshark on the LAN interface, capture filter `udp port 8086 or udp port 8088`, start before B1, save as `host-T00.pcapng` / `client-T00.pcapng`. Without Wireshark (admin PowerShell): `pktmon filter add -p 8086` ; `pktmon filter add -p 8088` ; `pktmon start --capture --pkt-size 0 -f C:\lan-test\<role>.etl` ... `pktmon stop` ; `pktmon etl2pcap C:\lan-test\<role>.etl`.

### B. Lobby (Zero Hour)
| # | Step | Expected | Record |
|---|---|---|---|
| B1 | HOST: main menu > Multiplayer > Network. Enter a player name (e.g. `HostPC`), press **Host**, name the game. CLIENT: Multiplayer > Network, name `LaptopPC`. | the client sees the host's game in the games list within ~10 s (LAN discovery); players list shows both once joined | time until the game appears; if never: T-DISCOVERY (section 7); then try **Direct Connect** (type the host IP) and record whether that works |
| B2 | CLIENT selects the game, **Join**. | joins; host sees the client in slot 1; no error dialog | any dialog text (e.g. CRC mismatch, game full, duplicate name, timeout) |
| B3 | Names | both names appear identical on both screens. Negative: a third join attempt using the **same name** as an existing player (or rename the client to the host's name before joining) -> expected denial "duplicate name" | what was shown |
| B4 | Chat: type lines both ways; include non-ASCII (`æøå`). | each line appears on both screens with the right sender | latency, garbled characters |
| B5 | HOST: **Select Map** (pick a 2-4 player map the Steam install has, e.g. a standard Skirmish map). | client sees the map name / preview change within seconds; no "map missing" for the client | what the client shows |
| B6 | Slots: HOST sets slot 2 and 3 to AI (Easy / Hard), changes teams; each human sets **own** faction / colour / team; try to change the other human's slot (should be refused). | every change visible on both screens within ~1 s; both screens end identical (compare slot by slot) | screenshot of both screens |
| B7 | HOST changes **Starting Cash**. | client sees the new value | value on each side |
| B8 | HOST toggles the retail **Limit Superweapons** check box (if present). **AI Behaviour** and **Superweapon Limit** dropdowns of Skirmish: not expected in this screen. | check box mirrors on the client; the two dropdowns are absent (document: not offered in LAN) | what is / is not offered |
| B9 | **Ready state**: CLIENT presses **Accept** (and un-accepts); HOST watches the accept marks; HOST changes a setting after the client accepted. | host cannot start until every human accepted; a changed setting clears the client's accept | behaviour of the accept marks |
| B10 | CLIENT **Leave** before the start, then join again; HOST re-fills the slot. | clean leave, game list updates, rejoin works | any stuck slot / ghost player |

### C. Launch and initial synchronisation
| # | Step | Expected | Record |
|---|---|---|---|
| C1 | Both players accepted, HOST **Start Game**. | countdown / loading screens on both, both enter the match | time to reach the map on each PC; any dialog |
| C2 | In the first 60 s compare: start positions, factions, colours, starting cash, initial units and base on both screens. | identical | screenshots side by side |
| C3 | Watch for the first 2-3 CRC checks (about 10 s): no "mismatch" screen. | none | time if it appears |

### D. Gameplay (10-20 min, one full match)
Setup for D: HOST + CLIENT on one team against 2 AI (Hard); a second match later HOST vs CLIENT (+ AI) if time allows.
| # | Step | Expected | Record |
|---|---|---|---|
| D1 | Both build base, power, production; queue units; place several buildings. | every building / unit appears for the other player; costs / money match | money on both screens at minutes 2, 5, 10 |
| D2 | Combat: attack the AI; use a general's power / superweapon if available. | same outcomes on both screens (who dies, when) | notes |
| D3 | AI behaviour: Easy / Hard AI builds and attacks normally in LAN (Original AI; Enhanced AI is not selectable here). | normal AI | anything odd (idle AI, no production) |
| D4 | In-game chat both ways; Esc menu; (LAN has no pause). | works | |
| D5 | At minutes 5, 10, 15 compare unit counts / money / minimap on both screens. | equal | numbers |
| D6 | (separate short case) change a **local-only** setting on one PC during a match (frame-rate limit in Options is not reachable in-match: use window focus loss / alt-tab for 5 s instead; and move the camera quickly). | no desync, no lag spike on the other PC | |
| D7 | Any `CRCMismatch` screen, "player disconnected", stutter, long freeze (> 3 s), sound / visual corruption. | none | exact time + what was on screen |

### E. End, disconnect, return, rematch, exit
| # | Step | Expected | Record |
|---|---|---|---|
| E1 | Finish the match (win / lose) on both. | victory / defeat dialogs on both; return to the LAN lobby or main menu | where each PC ends up |
| E2 | **Immediately** run `Collect-LanTestEvidence.ps1 -Role <Host|Client> -Tag E1` on **each PC** (section 6) - the next match overwrites `Last Replay.rep`. | | |
| E3 | Rematch: HOST creates / restarts a game, CLIENT joins again, start a 3-minute match. | works without restarting the game | stale state in the lobby |
| E4 | Mid-match leave: CLIENT uses the in-game **Exit / Quit**; HOST continues vs AI for 2 min. | host not frozen; AI continues | |
| E5 | Mid-match host exit; and, in another run, **disable Wi-Fi on the CLIENT for 15 s**, enable again. | the survivor sees the disconnect UI with a countdown (up to 60 s); no hard freeze; no reconnect expected | what the UI shows, time to resolve |
| E6 | After an E4 / E5 case return to the lobby and start a fresh game. | works | |
| E7 | **Clean exit** on both: Exit Game from the Modern main menu. Task Manager: the process is gone within ~5 s; `generals64.log` ends with the shutdown checkpoints. | no crash dialog, no `.dmp` | any hang on exit |

### F. Optional negative tests (only if the main run is clean; they change the setup, do them last)
* **F1 (Generals only)**: one PC with the original retail 32-bit `Generals.exe` (Steam "Verify integrity" restores it) joins a Modern host -> expected **CRC mismatch** denial. Documents that the check exists.
* **F2 (Zero Hour)**: same with the retail exe -> by the code audit the join is **not** denied; record what happens (expected: joins, then desync / mismatch screen, or crash). Documents that Zero Hour has no version enforcement.
* **F3**: same-name join (B3) and "game already started" (join after C1).

## 6. Evidence to keep

**Always, both PCs, after every completed or failed case** (one command each): `Collect-LanTestEvidence.ps1 -GameDir "<game>" -Role Host|Client -Tag <case id> -OutDir C:\lan-test` -> one folder + ZIP per case containing: `generals64.log`, the replays of the last 3 hours (`Last Replay.rep` renamed with the time), crash dumps of the last 3 hours, a copy of `Options.ini`, `ipconfig /all`, routes, ARP, UDP endpoints on 8086 / 8088, network profile, firewall state and rules, clock status, Application-log crash / hang entries, and the PC fingerprint. The collector only **copies**; it never deletes or changes anything.
Plus by hand: **photos / screenshots of every dialog** (CRC mismatch, join denied text, "connection lost", crash dialog), the written timeline (wall-clock times), and - if captured - the `.pcapng`.

| Failure | HOST keeps | CLIENT keeps | Why |
|---|---|---|---|
| **T-DISCOVERY** client does not find the host | collector ZIP (esp. `system\udp-8086-8088.txt`, `firewall-*.txt`, `net-profiles.txt`, `ipconfig-all.txt`, `settings\Options.ini` -> `IPAddress`), `.pcapng` | same set | shows chosen LAN IP, whether 8086 is bound, firewall / profile, whether broadcasts arrive; also record whether **Direct Connect** works |
| **T-JOIN** join fails | the lobby screen + the host's game name / slot state, collector ZIP, fingerprint | **exact dialog text**, collector ZIP, fingerprint | the denial reason (CRC mismatch / duplicate name / game full / started / timeout) decides the class; fingerprints prove build + data |
| **T-LAUNCH** match launch fails or hangs | collector ZIP, screenshots of host lobby / loading screen, `generals64.log` | same | where the loading stopped; map status per slot |
| **T-DESYNC** CRC mismatch screen / different game state | **both `Last Replay.rep`** (host and client copy; the desync flag is in the header), both `generals64.log`, both fingerprints, **side-by-side screenshots + times**, unit / money numbers | same | the two replays should hold the same command stream; the project's headless replay simulator can compare them later. Do **not** start another match before copying the replays |
| **T-CRASH** a client crashes | the survivor's collector ZIP and a note of what it showed | the crashed PC: **every new `.dmp`** in `<user data>\CrashDumps`, `generals64.log`, the Application-log extract (in the ZIP), crash dialog photo | the dump + the PDB of the exact build (`MKRaudstein-<Title>-x64-Preview-a885d798f-Symbols.zip` on the desktop) gives the stack |
| **T-DISCONNECT** network drop / disconnect UI | collector ZIP, screenshot of the disconnect menu, time of the drop | same, plus Wi-Fi / adapter state at that moment | tells transport loss from game logic |
| **T-STATE** different game state without a mismatch screen | both replays, screenshots with times, unit / money numbers at the same wall-clock time, fingerprints | same | a silent divergence is the most serious finding |

Where the files are: game log = `<game>\generals64.log` (written next to the executable you started); user data = `Documents\Command and Conquer Generals Zero Hour Data` (Zero Hour) / `Documents\Command and Conquer Generals Data` (Generals): `Replays\`, `CrashDumps\`, `Options.ini`.

## 7. Result sheet (copy per title)

| Case | Host PC result | Client PC result | Evidence ZIP | Class (section 8) | Notes |
|---|---|---|---|---|---|
| A1 fingerprints equal | | | | | |
| B1 discovery | | | | | |
| B2 join | | | | | |
| ... one row per case ... | | | | | |

## 8. How findings are classified
**PASS** / **FAIL-ENV** (firewall, adapter, clocks, wrong stage - fix the setup and repeat) / **FAIL-BUILD** (fingerprints differ) / **FAIL-LAN** (retail LAN code behaves wrongly on x64: lobby, join, protocol) / **FAIL-SYNC** (CRC mismatch or silent divergence) / **FAIL-CRASH** / **LIMIT** (documented: no AI Behaviour / Superweapon Limit in the LAN screen; no version enforcement in Zero Hour; no network log in Release; no reconnect).
The decisions after the test (what to fix first) belong to the owner; nothing is implemented as part of this preparation.
