import json
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import online_authority as authority


class AdmissionFenceTests(unittest.TestCase):
    project = authority.PROJECTS[1]
    config = {'minimum': '0.1.39.5-R5', 'require_update': True}

    def test_auth_outage_can_trigger_failover_while_rest_responds(self):
        with patch.object(authority, 'health', return_value=(200, {'epoch': 4})), \
             patch.object(authority, 'request', return_value=(503, b'')):
            status, _ = authority.service_health(self.project)
            self.assertEqual(authority.choose({'active': 'secondary'}, {'primary': 200, 'secondary': status}), 'primary')

    def test_committed_close_during_public_rest_failure(self):
        receipt = json.dumps([{'epoch': 4, 'backend_id': 1, 'admission_open': False,
                               'admission_until': 500}]).encode()
        with patch.dict(authority.os.environ, {self.project['secret']: 'fixture'}), \
             patch.object(authority, 'request', return_value=(201, receipt)), \
             patch.object(authority, 'route_config_matches', return_value=False) as public_probe:
            self.assertTrue(authority.write_config(self.project, 4, False, 500, self.config))
            public_probe.assert_not_called()

    def test_unconfirmed_outage_keeps_standby_closed(self):
        with patch.object(authority, 'write_config', return_value=False):
            with self.assertRaisesRegex(authority.AuthorityError, 'not fenced'):
                authority.fence_previous(self.project, 4, 87000, self.config, 500)

    def test_expired_lease_needs_no_database_access(self):
        with patch.object(authority, 'write_config') as write:
            self.assertEqual(authority.fence_previous(self.project, 4, 400, self.config, 500), 400)
            write.assert_not_called()

    def test_partial_or_wrong_receipt_cannot_close_route(self):
        for rows in ([], [{'epoch': 3, 'backend_id': 1, 'admission_open': False,
                           'admission_until': 500}]):
            with patch.dict(authority.os.environ, {self.project['secret']: 'fixture'}), \
                 patch.object(authority, 'request', return_value=(200, json.dumps(rows).encode())), \
                 patch.object(authority, 'route_config_matches', return_value=False):
                self.assertFalse(authority.write_config(self.project, 4, False, 500, self.config))


if __name__ == '__main__':
    unittest.main()
