"""
Add a number of copy jobs and run them in parallel
--------------------------------------------------
"""
from XRootD import client

process = client.CopyProcess()

# From local to local
process.add_job( '/tmp/spam', '/tmp/spam1' )
# From local to remote
process.add_job( '/tmp/spam', 'root://localhost//tmp/spam2' )
# From remote to local
process.add_job( 'root://localhost//tmp/spam', '/tmp/spam3' )
# From remote to remote
process.add_job( 'root://localhost//tmp/spam', 'root://localhost//tmp/spam4' )

process.prepare()
process.run()