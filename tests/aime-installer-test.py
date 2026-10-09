"""Run the installer in a disposable fixture; never touch the live game."""
from pathlib import Path
import subprocess
import tempfile
import unittest


class InstallerTest(unittest.TestCase):
    def test_diagnostic_update_preserves_manual_mode(self):
        installer = Path(__file__).resolve().parents[1] / 'install-aime.ps1'
        with tempfile.TemporaryDirectory(prefix='aime installer ') as temp:
            root = Path(temp)
            (root / 'chusanhook_x64.dll').write_bytes(b'fixture, never loaded')
            (root / 'segatools.ini').write_text('[aimeio]\npath=chuni-aime.dll\n')
            config = '; keep my choice\n[aime]\nenabled=0\n'
            (root / 'chuni-aime.ini').write_text(config)
            # Isolate the fixture from unrelated live processes. The production
            # installer's real process guard is verified separately.
            quote = lambda path: "'" + str(path).replace("'", "''") + "'"
            runner = root / 'test.ps1'
            runner.write_text('function Get-Process {}\n& ' + quote(installer)
                + ' -GameBin ' + quote(root) + ' -PreserveMode\n')
            result = subprocess.run(['powershell.exe', '-NoProfile',
                '-ExecutionPolicy', 'Bypass', '-File', str(runner)],
                capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual((root / 'chuni-aime.ini').read_text(), config)
            self.assertTrue((root / 'chuni-aime.dll').is_file())


if __name__ == '__main__':
    unittest.main(verbosity=2)
