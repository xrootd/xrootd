from XRootD import client
import pytest, sys
from env import *

def test_creation():
  u = client.FileSystem(SERVER_URL).url
  assert u is not None

def test_deletion():
  u = client.FileSystem(SERVER_URL).url
  del u

  if sys.hexversion > 0x03000000:
    pytest.raises(UnboundLocalError, 'assert u')
  else:
    pytest.raises(NameError, 'assert u')

def test_valid():
  u = client.FileSystem(SERVER_URL).url
  assert u.is_valid()

def test_invalid():
  u = client.FileSystem('root://').url
  assert u.is_valid() == False

def test_getters():
  u = client.FileSystem("root://user1:passwd1@host1:123//path?param1=val1&param2=val2").url
  assert u.is_valid()
  assert u.hostid == 'user1:passwd1@host1:123'
  assert u.protocol == 'root'
  assert u.username == 'user1'
  assert u.password == 'passwd1'
  assert u.hostname == 'host1'
  assert u.port == 123
  assert u.path == '/path'
  assert u.path_with_params == '/path?param1=val1&param2=val2'

def test_setters():
  u = client.FileSystem(SERVER_URL).url
  u.protocol = 'root'
  assert u.protocol == 'root'
  u.username = 'user1'
  assert u.username == 'user1'
  u.password = 'passwd1'
  assert u.password == 'passwd1'
  u.hostname = 'host1'
  assert u.hostname == 'host1'
  u.port = 123
  assert u.port == 123
  u.path = '/path'
  assert u.path == '/path'
  u.clear()
  assert str(u) == ''
