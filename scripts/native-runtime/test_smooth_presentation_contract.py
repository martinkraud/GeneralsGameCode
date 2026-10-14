"""X64.34 source contracts: Smooth presentation stays presentation-only and instrumented."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def read(path):
    return (ROOT / path).read_bytes().decode("latin1").replace("\r\n", "\n")


DRAWABLES = ("Generals/Code/GameEngine/Source/GameClient/Drawable.cpp",
             "GeneralsMD/Code/GameEngine/Source/GameClient/Drawable.cpp")


class SmoothPresentationContracts(unittest.TestCase):
    def test_airborne_policy_is_client_side_in_both_titles(self):
        for path in DRAWABLES:
            text = read(path)
            self.assertIn("obj->isKindOf(KINDOF_AIRCRAFT) || obj->isKindOf(KINDOF_PROJECTILE)", text)
            self.assertIn("MaxAircraftSampleDistance", text)
            self.assertIn("MaxProjectileSampleDistance", text)
            body = text[text.index("Bool Drawable::isGroundPresentationEligible() const"):text.index("void Drawable::draw()")]
            # eligibility/capture never call an authoritative setter
            for setter in ("setPosition(", "setOrientation(", "setTransformMatrix(", "setTurretTargetAngle"):
                self.assertNotIn(setter, body)

    def test_turret_presentation_is_a_render_only_override(self):
        text = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/Drawable/Draw/W3DModelDraw.cpp")
        body = text[text.index("void W3DModelDraw::handleClientTurretPositioning()"):]
        body = body[:body.index("//void W3DModelDraw::handleClientFlagPositioning()")]
        self.assertIn("GroundTranslation::isEnabled()", body)
        self.assertIn("m_turretPresentation[tslot]", body)
        self.assertIn("history.present(timing, turretAngle, turretPitch", body)
        # the canonical AI angles are only read; nothing in the function writes them back
        self.assertNotIn("setTurret", body)
        self.assertNotIn("->setCurrentRotation", body)

    def test_history_is_never_serialized(self):
        for path in DRAWABLES:
            text = read(path)
            xfer = text[text.index("void Drawable::xfer(") : text.index("void Drawable::loadPostProcess")] \
                if "void Drawable::xfer(" in text and "void Drawable::loadPostProcess" in text else ""
            self.assertNotIn("m_groundTranslation", xfer)
        header = read("Generals/Code/GameEngine/Include/GameClient/Drawable.h")
        self.assertIn("deliberately absent from xfer/CRC", header)

    def test_scheduler_and_tick_rate_are_untouched(self):
        pacer = read("Core/GameEngine/Source/Common/FramePacer.cpp")
        self.assertIn("LOGICFRAMES_PER_SECOND", read("Core/GameEngine/Include/Common/GameCommon.h"))
        self.assertNotIn("AnglePairHistory", pacer)
        self.assertIn("LOGICFRAMES_PER_SECOND = WWSyncPerSecond", read("Core/GameEngine/Include/Common/GameCommon.h"))
        self.assertIn("WWSyncPerSecond = 30,", read("Core/Libraries/Source/WWVegas/WWLib/WWCommon.h"))   # 30 TPS contract

    def test_receipt_counters_and_sampling(self):
        prof = read("Core/GameEngine/Include/Common/PerformanceProfile.h")
        for name in ("DrawCalls", "TexturesCreated", "BuffersCreated", "DrawablesDrawn", "ParticlesLive", "ObjectsLive"):
            self.assertIn(name, prof)
        host = read("Core/GameEngineDevice/Source/W3DDevice/GameClient/W3DDisplayOrdinaryHost.cpp")
        body = host[host.index("void NativeSampleRenderCounters()"):]
        self.assertIn("if (!PerformanceProfile::activeRecorder)", body[:200])     # free when not capturing
        self.assertIn("=== Generals Performance Receipt ===", read("Core/GameEngine/Source/Common/PerformanceProfile.cpp"))


if __name__ == "__main__":
    unittest.main()
