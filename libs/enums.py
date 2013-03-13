
def enum(**enums):
  """Build the equivalent of a C++ enum"""
  reverse = dict((value, key) for key, value in enums.iteritems())
  enums['reverse_mapping'] = reverse
  return type('Enum', (), enums)
  
OpenFlags = enum(
#  COMPRESS  = 1,
   DELETE    = 2,
   FORCE     = 4,
   NEW       = 8,
   READ      = 16,
   UPDATE    = 32,
#  ASYNC     = 64,
   REFRESH   = 128,
   MAKEPATH  = 256,
   APPEND    = 512,
#  RETSTAT   = 1024,
   REPLICA   = 2048,
   POSC      = 4096,
   NOWAIT    = 8192,
   SEQIO     = 16384
)