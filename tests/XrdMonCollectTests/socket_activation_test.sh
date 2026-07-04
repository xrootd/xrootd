#!/usr/bin/env bash

# Socket-activation test for xrdmoncollect. A pre-bound UDP socket is handed
# to the collector via the systemd LISTEN_FDS protocol (no systemd needed:
# the harness plays PID 1). The socket outlives the collector process, so the
# test asserts the zero-loss-restart property directly: the u-map, open, and
# close packets are all sent while NO collector is running, queue in the
# held socket's kernel receive buffer, and must come out as one fully
# correlated transfer document after the restart.

BIN=$1
test -x "${BIN}" || { echo "usage: $0 <xrdmoncollect>"; exit 1; }
command -v python3 >/dev/null 2>&1 || { echo "python3 not found"; exit 77; }

exec python3 - "${BIN}" <<'EOF'
import json, os, signal, socket, struct, subprocess, sys, tempfile, time

BIN = sys.argv[1]
STOD = 1700000000

def packet(code, payload):
    return struct.pack('>BBHI', ord(code), 0, 8 + len(payload), STOD) + payload

def rec(rtype, flag, body):
    return struct.pack('>BBH', rtype, flag, 4 + len(body)) + body

def tod(t, sid):
    return rec(2, 0, struct.pack('>IIIQ', 0, t, t, sid))

# The listening socket the harness owns for the whole test (dual-stack, any
# port), plus a fixed-source sender so all packets form one incarnation.
sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sock.setsockopt(socket.IPPROTO_IPV6, socket.IPV6_V6ONLY, 0)
sock.bind(('::', 0))
port = sock.getsockname()[1]
sender = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sender.bind(('127.0.0.1', 0))

tmp = tempfile.mkdtemp(prefix='xrdmon-sd-')
out = os.path.join(tmp, 'out.ndjson')

def start(logname):
    log = open(os.path.join(tmp, logname), 'w')
    def preexec():
        # The LISTEN_FDS protocol: the passed sockets start at fd 3 and
        # LISTEN_PID must name the exec'ed process.
        os.dup2(sock.fileno(), 3)
        os.environ['LISTEN_FDS'] = '1'
        os.environ['LISTEN_PID'] = str(os.getpid())
    return subprocess.Popen(
        [BIN, '-p', str(port), '-o', out,
         '--flush-secs', '1', '--flush-count', '1'],
        pass_fds=(3,), preexec_fn=preexec,
        stdout=log, stderr=log, stdin=subprocess.DEVNULL)

def fail(msg):
    for l in ('run1.log', 'run2.log'):
        p = os.path.join(tmp, l)
        if os.path.exists(p):
            sys.stderr.write('=== %s ===\n%s' % (l, open(p).read()))
    sys.stderr.write('FAIL: %s\n' % msg)
    sys.exit(1)

# Run 1: prove the collector adopts the inherited socket, then stop it.
p = start('run1.log')
time.sleep(1)
p.send_signal(signal.SIGTERM)
if p.wait(timeout=15) != 0:
    fail('collector (run 1) exited with %d' % p.returncode)
log1 = open(os.path.join(tmp, 'run1.log')).read()
if 'using systemd-provided UDP socket' not in log1:
    fail('run 1 did not adopt the inherited socket')

# With no collector running, send a whole transfer's packets: they queue in
# the held socket's kernel receive buffer instead of being lost.
def send(pkt): sender.sendto(pkt, ('127.0.0.1', port))
send(packet('u', struct.pack('>I', 7)
     + b'xroot/alice.123:4@wn.example.org\nrole=prod'))
send(packet('f', tod(STOD, 42)
     + rec(1, 0x03, struct.pack('>IQI', 100, 123456, 7)
                    + b'/store/data/file.root\x00')))
send(packet('f', tod(STOD + 82, 42)
     + rec(0, 0x02, struct.pack('>IQQQ', 100, 10485760, 0, 0)
                    + struct.pack('>IIIHHQIIIIII',
                                  320,0,0,0,0,0,4096,1048576,0,0,0,0))))

# Run 2: the restarted collector must drain the queued datagrams and emit the
# fully correlated transfer document.
p = start('run2.log')
doc = None
for _ in range(30):
    time.sleep(0.5)
    if os.path.exists(out):
        for line in open(out):
            d = json.loads(line)
            if d.get('attributes', {}).get('file.path') == '/store/data/file.root':
                doc = d
                break
    if doc: break
p.send_signal(signal.SIGTERM)
p.wait(timeout=15)

if not doc:
    fail('no transfer document for the packets sent during the restart window')
a = doc['attributes']
if a.get('user.name') != 'alice' or a.get('xrootd.transfer.open_seen') is not True:
    fail('document not fully correlated: %s' % json.dumps(a))

print('OK: zero-loss restart -- %d bytes read by alice, correlated across '
      'a collector restart on the held socket' % a['xrootd.transfer.read_bytes'])
EOF
