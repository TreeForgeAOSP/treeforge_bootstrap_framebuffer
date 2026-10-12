from pathlib import Path
import json
import re
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[1]
VERSION_RE = re.compile(r"v1\.0b[0-9]+")


class ProviderTests(unittest.TestCase):
    def test_verify(self):
        result = subprocess.run(
            [str(ROOT / "framebufferctl"), "verify"],
            text=True,
            capture_output=True,
            check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("TREEFORGE_BOOTSTRAP_FRAMEBUFFER_VERIFY=PASS", result.stdout)

    def test_component_identity(self):
        version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
        data = json.loads((ROOT / "provider.json").read_text(encoding="utf-8"))
        self.assertEqual(data["version"], version)
        self.assertEqual(data["abi"], 1)
        self.assertEqual(
            data["ownership"]["repository"],
            "TreeForgeDEV/treeforge_bootstrap_framebuffer",
        )

    def test_readme_single_version(self):
        version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
        text = (ROOT / "README.md").read_text(encoding="utf-8")
        self.assertEqual(
            text.splitlines()[0],
            f"# TreeForge Bootstrap Framebuffer — {version}",
        )
        self.assertEqual(VERSION_RE.findall(text), [version])

    def test_no_boot_policy(self):
        text = "\n".join(
            (ROOT / path).read_text(encoding="utf-8")
            for path in (
                "userspace/consumer-primitives-v1.inc",
                "userspace/consumer-transport-v1.inc",
            )
        )
        for forbidden in (
            "boot_android_slot_a",
            "boot_alternate_os",
            "pixel_partitioner",
            "TFB_ACTION_",
        ):
            self.assertNotIn(forbidden, text)

    def test_buffered_present_contract(self):
        data = json.loads((ROOT / "provider.json").read_text(encoding="utf-8"))
        self.assertEqual(
            data["userspace"]["contract"]["presentation"],
            "buffered-region-v1",
        )
        primitives = (ROOT / "userspace/consumer-primitives-v1.inc").read_text(
            encoding="utf-8"
        )
        transport = (ROOT / "userspace/consumer-transport-v1.inc").read_text(
            encoding="utf-8"
        )
        self.assertIn("TFB_FB_BUFFERED_PRESENT_V1", primitives)
        self.assertIn("static int tfb_fb_buffer_begin_region(", primitives)
        self.assertIn("static void tfb_fb_buffer_present_region(void)", primitives)
        self.assertIn("/dev/treeforge-bootstrap-fb-buffer-ready", transport)


if __name__ == "__main__":
    unittest.main()
