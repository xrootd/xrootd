#!/usr/bin/env python3
# Copyright (c) 2026 by the XRootD Collaboration. LGPL-3.0-or-later.
import json
from pathlib import Path
import tempfile
import unittest
import uuid

from mock_tape import ARCHIVE_QUERY, Tape, atomic_json, parse_files


class MockTapeTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.tape = Tape(self.root)
        (self.tape.archive / 'file').write_text('archived data')
        self.id = str(uuid.uuid4())
        self.files = [{'path': '/file', 'diskLifetime': 'PT1H',
                       'targetedMetadata': {'test': {'activity': 'analysis'}}}]

    def stage(self, request=None):
        request = request or self.id
        self.tape.mutate(request, 'stage', self.files, request + ':1')
        return request

    def test_stage_and_restart(self):
        self.stage()
        result = Tape(self.root).query(self.id, self.files)
        self.assertTrue(result['known'])
        self.assertEqual(result['files'][0]['state'], 'COMPLETED')
        self.assertEqual(result['acknowledged'], [self.id + ':1'])
        self.assertEqual(self.tape.read(self.id)['files'][0]['diskLifetime'], 'PT1H')
        self.assertEqual((self.tape.disk / 'file').read_text(), 'archived data')

    def test_replay_after_lost_acknowledgement(self):
        atomic_json(self.root / 'control.json', {'failAfterAccept': True})
        with self.assertRaises(RuntimeError):
            self.stage()
        self.stage()  # same operation is accepted exactly once by this mock
        self.assertEqual(len(self.tape.read(self.id)['operations']), 1)
        with self.assertRaises(ValueError):
            self.tape.mutate(self.id, 'cancel', self.files, self.id + ':1')

    def test_subset_cancel_is_atomic_and_pending_only(self):
        atomic_json(self.root / 'control.json', {'hold': True})
        self.stage()
        with self.assertRaises(ValueError):
            self.tape.mutate(self.id, 'cancel', self.files + [{'path': '/other'}], self.id + ':2')
        self.assertEqual(self.tape.query(self.id, self.files)['files'][0]['state'], 'STARTED')
        self.tape.mutate(self.id, 'cancel', self.files, self.id + ':2')
        self.assertEqual(self.tape.query(self.id, self.files)['files'][0]['state'], 'CANCELLED')
        self.assertFalse((self.tape.disk / 'file').exists())

    def test_release_preserves_other_request_pins(self):
        self.stage()
        other = self.stage(str(uuid.uuid4()))
        self.tape.mutate(self.id, 'evict', self.files, self.id + ':2')
        self.assertTrue((self.tape.disk / 'file').exists())
        self.tape.mutate(other, 'evict', self.files, other + ':2')
        self.assertFalse((self.tape.disk / 'file').exists())
        self.assertTrue((self.tape.archive / 'file').exists())
        self.assertEqual(self.tape.query(self.id, self.files)['files'][0]['state'], 'COMPLETED')

    def test_missing_and_empty_archive_files_fail_individually(self):
        (self.tape.archive / 'empty').touch()
        files = [{'path': '/file'}, {'path': '/missing'}, {'path': '/empty'}]
        self.tape.mutate(self.id, 'stage', files, self.id + ':1')
        self.assertEqual([f['state'] for f in self.tape.query(self.id, files)['files']],
                         ['COMPLETED', 'FAILED', 'FAILED'])

    def test_locality_and_unknown(self):
        self.assertFalse(self.tape.query(self.id, self.files)['known'])
        self.assertEqual(self.tape.query(ARCHIVE_QUERY, self.files)['files'][0]['locality'], 'TAPE')
        self.stage()
        self.assertEqual(self.tape.query(ARCHIVE_QUERY, self.files)['files'][0]['locality'], 'DISK_AND_TAPE')

    def test_distinct_metadata_and_credential_boundary(self):
        meta = json.dumps({'diskLifetime': 'PT1H'}).encode().hex()
        files, op = parse_files(['/a?xrd.prepare.file=' + meta + '&xrd.prepare.operation=' + self.id + ':1',
                                 '/b?xrd.prepare.file=7b7d&xrd.prepare.operation=' + self.id + ':1'])
        self.assertEqual(files, [{'path': '/a', 'diskLifetime': 'PT1H'}, {'path': '/b'}])
        self.assertEqual(op, self.id + ':1')
        for bad in ['/a?authz=secret', '/../a', '/a\n/b']:
            with self.assertRaises(ValueError):
                parse_files([bad])


if __name__ == '__main__':
    unittest.main()
