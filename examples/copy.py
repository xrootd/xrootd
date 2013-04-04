from XRootD import client

process = client.CopyProcess()
process.add_job( 'root://localhost//tmp/spam', 'root://localhost//tmp/eggs' )
process.prepare()
process.run()