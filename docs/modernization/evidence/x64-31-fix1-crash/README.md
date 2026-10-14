# X64.31-fix1 native review crash (run 20261014T0706407673524Z)

Exit code 1. Dumps: CrashFG (780 MB, kept in the stage under normal-launch/crash-*; not committed) and
CrashMG (committed here). Both are taken at the same point, from the same exception, by MiniDumper::TriggerMiniDump
inside the catch(...) of GameEngine::execute ("Uncaught Exception in GameEngine::update"); MG and FG are two sizes
of one crash, not two crash paths.

Root cause (full dump, symbolized against the exact exe/PDB): a std::runtime_error
"D3D11.2 requires an ordinary single-mip file texture without native storage" thrown by the D3D11 texture bridge
(D3D11TextureBridge.cpp prepare lambda) during GameLogic::update:
AssistedTargetingUpdate::makeFeedbackLaser -> ThingFactory::newObject -> Drawable::Drawable ->
W3DLaserDraw::W3DLaserDraw -> WW3DAssetManager::Get_Texture -> TextureClass::Init -> Prepare_File_Texture.
W3DLaserDraw asks for the laser texture with the default full mip chain; the bridge only accepted MIP_LEVELS_1.
Any gameplay texture requested with mips would have crashed the same way, and an absent/unsupported file threw too.

State before the crash: normal launch, skirmish map loaded (terrain_first_frame, world_objects_first_frame with 184
objects registered), roughly 7 minutes of play (process uptime 7:22).
