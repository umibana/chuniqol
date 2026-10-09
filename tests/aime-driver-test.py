"""Exercise the real x64 AimeIO DLL with isolated synthetic card fixtures."""
import ctypes
import os
from pathlib import Path
import shutil
import tempfile
import unittest

DLL = Path(__file__).resolve().parents[1] / 'build' / 'chuni-aime.dll'
CARD = '01234567890123456789'
CARD_BYTES = bytes.fromhex(CARD)
INVALID_ARGUMENT = -2147024809


class ArtifactTest(unittest.TestCase):
    def test_driver_has_been_built(self):
        self.assertTrue(DLL.is_file(), 'Automatic AimeIO driver has not been implemented/built')


class DriverTest(unittest.TestCase):
    def setUp(self):
        if not DLL.is_file():
            self.skipTest('Driver not built yet')
        self.temp = tempfile.TemporaryDirectory(prefix='aime test ')
        self.root = Path(self.temp.name)
        self.root.joinpath('DEVICE').mkdir()
        self.root.joinpath('DEVICE/aime.txt').write_text(CARD, encoding='ascii')
        self.root.joinpath('segatools.ini').write_text(
            '[aime]\naimePath=DEVICE\\aime.txt\nscan=0\n', encoding='ascii')
        self.config(1)
        self.saved_env = os.environ.pop('SEGATOOLS_CONFIG_PATH', None)
        self.module_path = self.root / 'chuni-aime.dll'
        shutil.copy2(DLL, self.module_path)
        self.driver = ctypes.CDLL(str(self.module_path))
        self.version = self.driver.aime_io_get_api_version
        self.version.restype = ctypes.c_uint16
        self.init = self.driver.aime_io_init
        self.init.restype = ctypes.c_long
        self.poll = self.driver.aime_io_nfc_poll
        self.poll.argtypes = [ctypes.c_uint8]
        self.poll.restype = ctypes.c_long
        self.get = self.driver.aime_io_nfc_get_aime_id
        self.get.argtypes = [ctypes.c_uint8, ctypes.POINTER(ctypes.c_uint8), ctypes.c_size_t]
        self.get.restype = ctypes.c_long
        self.felica = self.driver.aime_io_nfc_get_felica_id
        self.felica.argtypes = [ctypes.c_uint8, ctypes.POINTER(ctypes.c_uint64)]
        self.felica.restype = ctypes.c_long

    def config(self, enabled):
        self.root.joinpath('chuni-aime.ini').write_text(
            f'[aime]\nenabled={enabled}\n', encoding='ascii')
        # Notify the Windows INI cache of the external update.
        ctypes.windll.kernel32.WritePrivateProfileStringW(
            None, None, None, str(self.root / 'chuni-aime.ini'))

    def tearDown(self):
        if hasattr(self, 'driver'):
            ctypes.windll.kernel32.FreeLibrary.argtypes = [ctypes.c_void_p]
            ctypes.windll.kernel32.FreeLibrary(self.driver._handle)
            self.driver._handle = 0
        if hasattr(self, 'saved_env'):
            if self.saved_env is not None:
                os.environ['SEGATOOLS_CONFIG_PATH'] = self.saved_env
            else:
                os.environ.pop('SEGATOOLS_CONFIG_PATH', None)
        if hasattr(self, 'temp'):
            self.temp.cleanup()

    def read(self, unit=0):
        out = (ctypes.c_uint8 * 10)()
        return self.get(unit, out, 10), bytes(out)

    def test_api_and_no_card_before_poll(self):
        self.assertEqual(self.version(), 0x0100)
        self.assertEqual(self.init(), 0)
        self.assertEqual(self.read()[0], 1)

    def test_automatic_read_without_keyboard_and_same_identity(self):
        self.assertEqual(self.init(), 0)
        for _ in range(3):
            self.assertEqual(self.poll(0), 0)
            self.assertEqual(self.read(), (0, CARD_BYTES))
        self.assertEqual(self.root.joinpath('DEVICE/aime.txt').read_text(), CARD)

    def test_disable_returns_to_manual_and_live_reenable(self):
        self.assertEqual(self.init(), 0)
        self.assertEqual(self.poll(0), 0)
        self.assertEqual(self.read()[0], 0)
        self.config(0)
        self.assertEqual(self.poll(0), 0)
        self.assertEqual(self.read()[0], 1)  # scan=0: no global keyboard side effects.
        self.config(1)
        self.assertEqual(self.poll(0), 0)
        self.assertEqual(self.read(), (0, CARD_BYTES))

    def test_unknown_reader_and_invalid_buffers(self):
        self.assertEqual(self.init(), 0)
        self.assertEqual(self.poll(1), 0)
        self.assertEqual(self.read(1)[0], 1)
        self.assertEqual(self.get(0, None, 10), INVALID_ARGUMENT)
        out = (ctypes.c_uint8 * 9)()
        self.assertEqual(self.get(0, out, 9), INVALID_ARGUMENT)

    def test_does_not_generate_another_card(self):
        self.assertEqual(self.init(), 0)
        self.assertEqual(self.poll(0), 0)
        out = ctypes.c_uint64(0x1234)
        self.assertEqual(self.felica(0, ctypes.byref(out)), 1)
        self.assertEqual(out.value, 0x1234)
        self.assertFalse(self.root.joinpath('DEVICE/felica.txt').exists())

    def test_invalid_or_missing_card_fails_initialization(self):
        for invalid in ['', CARD[:-1], CARD + '0', 'A' + CARD[1:], CARD + 'junk']:
            self.root.joinpath('DEVICE/aime.txt').write_text(invalid, encoding='ascii')
            self.assertLess(self.init(), 0, invalid)
        self.root.joinpath('DEVICE/aime.txt').unlink()
        self.assertLess(self.init(), 0)

    def test_whitespace_and_utf8_bom(self):
        self.root.joinpath('DEVICE/aime.txt').write_bytes(b'\xef\xbb\xbf' + CARD.encode() + b'\r\n')
        self.assertEqual(self.init(), 0)
        self.assertEqual(self.poll(0), 0)
        self.assertEqual(self.read(), (0, CARD_BYTES))

    def test_removed_card_never_reuses_stale_identity(self):
        self.assertEqual(self.init(), 0)
        self.assertEqual(self.poll(0), 0)
        self.assertEqual(self.read()[0], 0)
        self.root.joinpath('DEVICE/aime.txt').unlink()
        self.assertLess(self.poll(0), 0)
        self.assertEqual(self.read()[0], 1)

    def test_configured_card_path_and_log_privacy(self):
        self.root.joinpath('tarjeta actual.txt').write_text(CARD, encoding='ascii')
        self.root.joinpath('segatools.ini').write_text(
            '[aime]\naimePath=tarjeta actual.txt\nscan=0\n', encoding='ascii')
        self.assertEqual(self.init(), 0)
        self.assertEqual(self.poll(0), 0)
        self.assertEqual(self.read(), (0, CARD_BYTES))
        log = self.root.joinpath('chuni-aime.log').read_text()
        self.assertIn('AUTO', log)
        self.assertNotIn(CARD, log)

    def test_diagnostics_show_paths_and_reader_handoff_without_identity(self):
        self.assertEqual(self.init(), 0)
        self.assertEqual(self.read()[0], 1)
        self.assertEqual(self.poll(0), 0)
        self.assertEqual(self.read(), (0, CARD_BYTES))
        log = self.root.joinpath('chuni-aime.log').read_text(encoding='utf-8')
        self.assertIn(str(self.root / 'segatools.ini'), log)
        self.assertIn(str(self.root / 'DEVICE/aime.txt'), log)
        self.assertIn('GET AIME: unit=0 size=10 result=00000001', log)
        self.assertIn('GET AIME: unit=0 size=10 result=00000000', log)
        self.assertNotIn(CARD, log)

    def test_recovers_when_current_card_is_restored(self):
        self.assertEqual(self.init(), 0)
        self.assertEqual(self.poll(0), 0)
        self.root.joinpath('DEVICE/aime.txt').unlink()
        self.assertLess(self.poll(0), 0)
        self.assertEqual(self.read()[0], 1)
        self.root.joinpath('DEVICE/aime.txt').write_text(CARD, encoding='ascii')
        self.assertEqual(self.poll(0), 0)
        self.assertEqual(self.read(), (0, CARD_BYTES))
        self.assertIn('RECOVERED', self.root.joinpath('chuni-aime.log').read_text())

    def test_segatoools_environment_override_uses_config_directory(self):
        external = self.root / 'external config'
        external.mkdir()
        external.joinpath('current.txt').write_text(CARD, encoding='ascii')
        external.joinpath('sega.ini').write_text(
            '[aime]\naimePath=current.txt\nscan=0\n', encoding='ascii')
        os.environ['SEGATOOLS_CONFIG_PATH'] = str(external / 'sega.ini')
        self.root.joinpath('DEVICE/aime.txt').unlink()
        self.assertEqual(self.init(), 0)
        self.assertEqual(self.poll(0), 0)
        self.assertEqual(self.read(), (0, CARD_BYTES))


if __name__ == '__main__':
    unittest.main(verbosity=2)
