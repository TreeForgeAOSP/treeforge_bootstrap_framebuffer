"""Release archive checks must not interpret internal checksums as loose assets."""
import hashlib
import importlib.util
import io
from pathlib import Path
import tarfile
from tempfile import TemporaryDirectory
import unittest


SCRIPT = Path(__file__).resolve().parents[1] / 'scripts/treeforge_release_workflow.py'
spec = importlib.util.spec_from_file_location('framebuffer_release', SCRIPT)
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


def digest(data):
    return hashlib.sha256(data).hexdigest()


class FramebufferReleaseAssetsTest(unittest.TestCase):
    def make_assets(self, folder):
        tag = 'v1.0b1'
        name = f'treeforge-bootstrap-framebuffer-{tag}-provider.tar.xz'
        assets = Path(folder) / 'assets'
        assets.mkdir()
        members = {'LICENSE': b'Apache-2.0\n', 'provider.json': b'{"provider":"treeforge_bootstrap_framebuffer"}\n'}
        checksums = ''.join(f'{digest(raw)}  {path}\n' for path, raw in sorted(members.items())).encode()
        members['SHA256SUMS'] = checksums
        (assets / 'SHA256SUMS').write_bytes(checksums)
        (assets / 'provider.json').write_bytes(members['provider.json'])
        archive = assets / name
        with tarfile.open(archive, 'w:xz') as bundle:
            for path, raw in sorted(members.items()):
                entry = tarfile.TarInfo(f'treeforge_bootstrap_framebuffer/{path}')
                entry.size = len(raw)
                bundle.addfile(entry, io.BytesIO(raw))
        (assets / (name + '.sha256')).write_text(f'{release.sha(archive)}  {name}\n')
        return assets, tag, name

    def test_internal_checksum_manifest_is_accepted(self):
        with TemporaryDirectory() as folder:
            assets, tag, _ = self.make_assets(folder)
            release.verify_framebuffer_assets(assets, tag)

    def test_archive_tampering_rejected(self):
        with TemporaryDirectory() as folder:
            assets, tag, name = self.make_assets(folder)
            with (assets / name).open('ab') as output:
                output.write(b'tampered')
            with self.assertRaises(release.Refusal):
                release.verify_framebuffer_assets(assets, tag)

    def test_provider_sidecar_divergence_rejected(self):
        with TemporaryDirectory() as folder:
            assets, tag, _ = self.make_assets(folder)
            (assets / 'provider.json').write_text('changed')
            with self.assertRaises(release.Refusal):
                release.verify_framebuffer_assets(assets, tag)


if __name__ == '__main__':
    unittest.main()
