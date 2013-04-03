from XRootD import client
 
def test_copy():
  c = client.CopyProcess()
  c.add_job( source='root://localhost//tmp/spam', 
             target='root://localhost//tmp/eggs' )
  c.prepare()
  c.run()