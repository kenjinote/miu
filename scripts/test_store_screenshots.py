"""Verify destructive boundaries and resumability without touching App Store Connect."""
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

import store_screenshots as s


class SyncSafetyTests(unittest.TestCase):
    def run_sync(self, fail_processing=False, matching=False):
        with tempfile.TemporaryDirectory() as temp:
            args=SimpleNamespace(out_dir=Path(temp),version='1.0.44')
            desired=[{'file':f'{i}.png','md5':f'new{i}','sha256':f'sha{i}'} for i in range(5)]
            current=[{'id':f'old{i}','attributes':{'sourceFileChecksum':f'old{i}',
                     'assetDeliveryState':{'state':'COMPLETE'}}} for i in range(5)]
            if matching:
                current=[{'id':f'new{i}','attributes':{'sourceFileChecksum':f'new{i}',
                         'assetDeliveryState':{'state':'COMPLETE'}}} for i in range(5)]
            target={'locale':'ja','set_id':'set','desired':desired,'current':list(current)}
            events=[]
            def api(method,path,payload=None):
                if method=='DELETE':
                    events.append(('delete',path.split('/')[-1]))
                    current[:]=[v for v in current if v['id']!=path.split('/')[-1]]
                elif method=='PATCH':
                    order=[r['id'] for r in payload['data']]
                    current.sort(key=lambda r:order.index(r['id']))
                return {}
            def upload(sid,path):
                name='new'+Path(path).stem
                events.append(('upload',name))
                current.append({'id':name,'attributes':{'sourceFileChecksum':name,
                                'assetDeliveryState':{'state':'COMPLETE'}}})
                return True,name
            def wait(ids):
                events.append(('wait',len(ids)))
                if fail_processing: raise RuntimeError('Processing failed')
            with patch.object(s,'listing',side_effect=lambda p:list(current)), \
                 patch.object(s,'api',side_effect=api), \
                 patch.object(s.legacy,'upload_single_screenshot',side_effect=upload), \
                 patch.object(s,'wait_complete',side_effect=wait):
                if fail_processing:
                    with self.assertRaises(RuntimeError): s.sync_target(args,target)
                else:
                    s.sync_target(args,target)
                    self.assertEqual([v['id'] for v in current],[f'new{i}' for i in range(5)])
            return events

    def test_uploads_finish_before_old_assets_are_deleted(self):
        events=self.run_sync()
        self.assertEqual([e[0] for e in events],['upload']*5+['wait']+['delete']*5)

    def test_processing_failure_retains_originals(self):
        events=self.run_sync(fail_processing=True)
        self.assertNotIn('delete',[e[0] for e in events])

    def test_completed_assets_are_reused(self):
        events=self.run_sync(matching=True)
        self.assertEqual(events,[('wait',5)])

    def test_wrong_version_cannot_select_another_editable_version(self):
        responses=[[{'id':'app'}],[{'id':'wrong','attributes':{'versionString':'1.0.45',
                    'platform':'MAC_OS','appStoreState':'PREPARE_FOR_SUBMISSION'}}]]
        with patch.object(s,'listing',side_effect=responses):
            with self.assertRaisesRegex(RuntimeError,'exactly one'):
                s.make_plan(SimpleNamespace(version='1.0.44'),{})

    def test_noneditable_version_stops_before_localization_changes(self):
        responses=[[{'id':'app'}],[{'id':'version','attributes':{'versionString':'1.0.44',
                    'platform':'MAC_OS','appStoreState':'READY_FOR_SALE'}}]]
        with patch.object(s,'listing',side_effect=responses):
            with self.assertRaisesRegex(RuntimeError,'not editable'):
                s.make_plan(SimpleNamespace(version='1.0.44'),{})

    def test_backup_failure_prevents_sync(self):
        with patch.object(s,'make_plan',return_value={}), \
             patch.object(s,'backup',side_effect=RuntimeError('Backup failed')), \
             patch.object(s,'sync_target') as mutation:
            with self.assertRaises(RuntimeError): s.sync(SimpleNamespace(),{})
            mutation.assert_not_called()


if __name__=='__main__': unittest.main()
