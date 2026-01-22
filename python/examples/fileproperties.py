"""
Write a chunk of data to a file and test it's property system
-------------------------------------------------------------

Produces the following output::

spam\n
true\n
true\n
true\n
localhost:1094\n
root://localhost:1094//tmp/eggs\n
True\n
True\n
True\n
false\n
false\n
false\n

"""
from XRootD import client
from XRootD.client.flags import OpenFlags

with client.File() as f:
  f.open('root://localhost//tmp/eggs', OpenFlags.DELETE)

  data = 'spam\n'
  f.write(data)
  print f.read()[1]
  print f.get_property( "ReadRecovery" )
  print f.get_property( "WriteRecovery" )
  print f.get_property( "FollowRedirects" )
  print f.get_property( "DataServer" )
  print f.get_property( "LastURL" )
  print f.set_property( "ReadRecovery", "false" )
  print f.set_property( "WriteRecovery", "false" )
  print f.set_property( "FollowRedirects", "false" )
  print f.get_property( "ReadRecovery" )
  print f.get_property( "WriteRecovery" )
  print f.get_property( "FollowRedirects" )
