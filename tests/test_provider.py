from pathlib import Path
import hashlib, json, subprocess, unittest
ROOT=Path(__file__).resolve().parents[1]
class ProviderTests(unittest.TestCase):
    def test_verify(self):
        result=subprocess.run([str(ROOT/'framebufferctl'),'verify'],text=True,capture_output=True)
        self.assertEqual(result.returncode,0,result.stdout+result.stderr)
        self.assertIn('TREEFORGE_BOOTSTRAP_FRAMEBUFFER_VERIFY=PASS',result.stdout)
    def test_component_identity(self):
        data=json.loads((ROOT/'provider.json').read_text())
        self.assertEqual(data['version'],'v1.0b1')
        self.assertEqual(data['abi'],1)
        self.assertEqual(data['ownership']['repository'],'TreeForgeDEV/treeforge_bootstrap_framebuffer')
    def test_no_boot_policy(self):
        text='\n'.join((ROOT/p).read_text() for p in ('userspace/consumer-primitives-v1.inc','userspace/consumer-transport-v1.inc'))
        for forbidden in ('boot_android_slot_a','boot_alternate_os','pixel_partitioner','TFB_ACTION_'):
            self.assertNotIn(forbidden,text)
if __name__=='__main__': unittest.main()
