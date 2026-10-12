from pathlib import Path
import subprocess,unittest,re
ROOT=Path(__file__).resolve().parents[2]
BASE='c8aaccbee77e32f7ad21dbaa54682b7ea6537d3e'
def read(p):return (ROOT/p).read_bytes().decode('latin1').replace('\r\n','\n')
def without_native(s):
 lines=s.splitlines(keepends=True);out=[];depth=0
 for line in lines:
  if not depth and line.strip()=='#if defined(_WIN64)':depth=1
  elif depth:
   if re.match(r'\s*#if',line):depth+=1
   elif re.match(r'\s*#endif',line):depth-=1
   elif re.match(r'\s*#else',line) and depth==1:raise AssertionError('Unexpected else in native-only addition')
  else:out.append(line)
 assert depth==0
 return ''.join(out)
class DisplayContracts(unittest.TestCase):
 def test_legacy_ww3d_and_display_tokens_unchanged(self):
  for p in ['Core/Libraries/Source/WWVegas/WW3D2/ww3d.cpp','Core/Libraries/Source/WWVegas/WW3D2/ww3d.h','Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplay.cpp','Core/GameEngineDevice/Include/W3DDevice/GameClient/W3DDisplay.h','Core/GameEngine/Include/GameClient/Display.h']:
   old=subprocess.check_output(['git','show',f'{BASE}:{p}'],cwd=ROOT).decode('latin1').replace('\r\n','\n')
   self.assertEqual('\n'.join(x for x in without_native(read(p)).splitlines() if x.strip()),'\n'.join(x for x in without_native(old).splitlines() if x.strip()),p)
 def test_real_owner_and_frame_route_without_ordinary_draw(self):
  s=read('Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayPresentation.cpp').split('int W3DDisplay::runNativePresentationSmoke()',1)[1]
  for term in ['WW3D::Init_Native_Presentation','WW3D::Begin_Render','WW3D::End_Render','WW3D::Shutdown_Native_Presentation','display.Display::setWidth','display.Display::setHeight']:self.assertIn(term,s)
  for term in ['->execute(', '->update(', '->draw(', 'm_headless =', 'showShell(']:self.assertNotIn(term,s)
  for title in ['Generals','GeneralsMD']:
   s=read(f'{title}/Code/GameEngine/Source/Common/GameMain.cpp')
   self.assertLess(s.index('TheGameEngine->init();'),s.index('runNativePresentationSmoke()'))
   self.assertLess(s.index('runNativePresentationSmoke()'),s.index('TheGameEngine->execute();'))
 def test_unsupported_world_calls_fail_before_legacy_noop(self):
  s=read('Core/Libraries/Source/WWVegas/WW3D2/ww3d.cpp')
  self.assertEqual(s.count('World rendering unsupported by native presentation lifecycle'),4)
  self.assertIn('Cannot mix native presentation and legacy WW3D lifecycles',s)
  self.assertIn('config.software=false;',s)
  self.assertLess(s.index('RenderBackend=nullptr;',s.index('void WW3D::Shutdown_Native_Presentation')),s.index('NativePresentation.reset();'))
 def test_old_graphics_and_init_flags_preserved_new_explicit_selector(self):
  s=read('scripts/native-runtime/Invoke-NativeGraphicsSmoke.ps1')
  self.assertIn("if($DisplayLifecycle){'-nativeDisplaySmoke'}else{'-nativeGraphicsSmoke'}",s)
  self.assertIn('-DisplayLifecycle:$DisplayLifecycle',s)
  for term in ['$exit -eq 0','$shutdownBytes -eq 0','$dumps.Count -eq 0','$evidence.Valid']:self.assertIn(term,s)
 def test_image_gate_scoped_ownership_and_reference_route(self):
  s=read('Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayPresentation.cpp')
  for term in ['StartupDiagnostics::uiImageSmoke()', 'findImageByName("GeneralsLogo")', 'MIP_LEVELS_1', 'display.drawImage(image,504,16,791,110,0xffffffff,Display::DRAW_IMAGE_ALPHA)', 'Render2DClass::Set_Screen_Resolution(previousScreen)']:
   self.assertIn(term,s)
  cleanup=s.split('void Shutdown() noexcept override',1)[1]
  self.assertLess(cleanup.index('delete display.m_2DRender'),cleanup.index('texture->Release_Ref()'))
  self.assertLess(cleanup.index('texture->Release_Ref()'),cleanup.index('WW3D::Shutdown_Native_Presentation()'))
  for term in ['Get_Texture(', 'showShell(', '->update(', '->execute(', 'm_headless =']:
   self.assertNotIn(term,s)
 def test_sentence_real_producer_private_lifetime_and_bias_restore(self):
  s=read('Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayPresentation.cpp')
  for term in ['TheGameText->fetch("GUI:SelectDifficulty", &exists)','Initialize_GDI_Font("Arial",14,false,window)','sentence->Build_Sentence(text.str(),nullptr,nullptr)','sentence->Render()','WW3D::Set_Screen_UV_Bias(previousBias)']:
   self.assertIn(term,s)
  cleanup=s.split('void Shutdown() noexcept override',1)[1]
  self.assertLess(cleanup.index('delete sentence'),cleanup.index('font->Release_Ref()'))
  self.assertLess(cleanup.index('font->Release_Ref()'),cleanup.index('WW3D::Shutdown_Native_Presentation()'))
 def test_panel_uses_real_inherited_primitives_without_second_manager_ownership(self):
  s=read('Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayPresentation.cpp')
  for term in ['TheWindowManager->GameWindowManager::winOpenRect(0xffffbb00,1,532,108,756,360)', 'TheWindowManager->GameWindowManager::winFillRect(0x7e000000,1,533,109,755,359)']:self.assertIn(term,s)
  self.assertLess(s.index('winOpenRect('),s.index('winFillRect('))
  self.assertLess(s.index('winFillRect('),s.index('display.drawImage('))
  for term in ['W3DGameWindowManager panel','winCreateFromScript(', 'winSetDrawFunc(', 'new WindowLayout', 'MainMenuInit(']:self.assertNotIn(term,s)
  # Preserve the manager, original default USER draw, and primitive forwarding.
  for p in ['Core/GameEngine/Source/GameClient/GUI/GameWindowManager.cpp', 'Core/GameEngine/Source/GameClient/GUI/GameWindowGlobal.cpp', 'Core/GameEngineDevice/Source/W3DDevice/GameClient/GUI/W3DGameWindow.cpp']:
   old=subprocess.check_output(['git','show',f'34fdd323f68621fdfea3cc1cf0d27fd62a7a34a2:{p}'],cwd=ROOT)
   self.assertEqual((ROOT/p).read_bytes().replace(b'\r\n',b'\n'),old.replace(b'\r\n',b'\n'))
if __name__=='__main__':unittest.main()
