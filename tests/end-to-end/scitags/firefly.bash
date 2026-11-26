# Checks on the UDP fireflies a server sends to a collector started with
# start_collector (see server.bash, which this file expects to be loaded).
#
# A datagram is one line: syslog "<134>1", app-name xrootd, msgid
# firefly-json, then one JSON object. The copy the server sends to the peer
# on port 10514 does not reach the collector.
#
# firefly_check polls until the expected start/end pairs are in the file,
# then checks each pair. Clients are driven at 127.0.0.1, so afi is ipv4 and
# both addresses are 127.0.0.1. The usage counter of the file's direction on
# end is greater than on start, and, unless --sum, at least the file size.
# Both comparisons are skipped when the counter is 0 on start and on end,
# which is what a host without TCP_INFO reports.

# The size of the collector file, so a later check reads only new datagrams.
firefly_offset() {
    wc -c < "$1" | tr -d '[:space:]'
}

# firefly_unchanged <records> <offset>
#
# Succeed when the collector file is still at <offset> after a second. The
# end datagram follows the close of the client, so the wait covers a late
# one. The size is read again after that wait: a datagram that arrives
# during it is growth, not silence.
firefly_unchanged() {
    local records=$1 offset=$2 deadline=$(( SECONDS + 1 )) now

    while true; do
        now=$(firefly_offset "$records")
        if [[ "$now" != "$offset" ]]; then
            echo "collector $records grew from $offset to $now bytes" >&2
            firefly_dump "$records" "$offset"
            return 1
        fi
        if (( SECONDS >= deadline )); then
            return 0
        fi
        sleep 0.2
    done
}

# Print the datagrams written after <offset>.
firefly_dump() {
    echo "--- $1 from byte $2 ---" >&2
    tail -c +$(( $2 + 1 )) "$1" >&2 || true
}

# The checker. gate waits; check asserts. See firefly_check.
firefly_py() {
    python3 - "$@" <<'PY'
import argparse
import json
import re
import sys

HEADER = re.compile(r"^<134>1 - \S+ xrootd - firefly-json - (\{.*)$")


class FireflyError(Exception):
    pass


def read_events(path, offset):
    with open(path, "rb") as handle:
        handle.seek(offset)
        data = handle.read()
    if not data.endswith(b"\n"):
        cut = data.rfind(b"\n")
        data = data[: cut + 1] if cut >= 0 else b""
    events = []
    for line in data.decode("utf-8", "replace").splitlines():
        if not line:
            continue
        match = HEADER.match(line)
        if not match:
            raise FireflyError("not a firefly datagram: " + line)
        try:
            events.append(json.loads(match.group(1)))
        except json.JSONDecodeError as exc:
            raise FireflyError(f"firefly JSON ({exc}): {line}") from exc
    return events


def parse(event):
    try:
        life = event["flow-lifecycle"]
        flow = event["flow-id"]
        ctx = event["context"]
        usage = event["usage"]
        state = life["state"]
        if state not in ("start", "end"):
            raise FireflyError(f"flow-lifecycle state {state!r}")
        return {
            "state": state,
            "exp": int(ctx["experiment-id"]),
            "act": int(ctx["activity-id"]),
            "app": ctx.get("application"),
            "afi": flow["afi"],
            "src": flow["src-ip"],
            "dst": flow["dst-ip"],
            "proto": flow["protocol"],
            "sport": int(flow["src-port"]),
            "dport": int(flow["dst-port"]),
            "start": life["start-time"],
            "end": life.get("end-time"),
            "received": int(usage["received"]),
            "sent": int(usage["sent"]),
        }
    except FireflyError:
        raise
    except (KeyError, TypeError, ValueError) as exc:
        raise FireflyError(f"firefly field ({exc}): {event}") from exc


def load(path, offset, exp, act, select=False):
    groups = {}
    order = []
    for event in read_events(path, offset):
        item = parse(event)
        if item["exp"] != exp or item["act"] != act:
            if select:
                continue
            raise FireflyError(
                f"unexpected tag experiment {item['exp']} activity {item['act']}"
            )
        key = (item["src"], item["dst"], item["sport"], item["dport"],
               item["start"], item["app"])
        slot = groups.get(key)
        if slot is None:
            slot = {}
            groups[key] = slot
            order.append(key)
        if item["state"] in slot:
            raise FireflyError(
                f"two {item['state']} datagrams for "
                f"{item['src']}:{item['sport']} -> {item['dst']}:{item['dport']}"
            )
        slot[item["state"]] = item
    complete = []
    incomplete = 0
    for key in order:
        slot = groups[key]
        if "start" in slot and "end" in slot:
            complete.append((slot["start"], slot["end"]))
        else:
            incomplete += 1
    return complete, incomplete


def where(item):
    return f"{item['src']}:{item['sport']} -> {item['dst']}:{item['dport']}"


def check(args):
    complete, incomplete = load(
        args.records, args.offset, args.exp, args.act, args.select
    )
    if len(complete) != args.pairs or incomplete:
        raise FireflyError(
            f"{len(complete)} complete pair(s), {incomplete} open, "
            f"expected {args.pairs}"
        )
    problems = []
    measured = []
    skipped = 0
    for start, end in complete:
        place = where(start)
        # The pairing key does not include protocol or address family, and
        # the two datagrams are separate messages. Both must describe the
        # same TCP flow.
        for item in (start, end):
            if item["proto"] != "tcp":
                problems.append(
                    f"{place} {item['state']}: protocol {item['proto']}"
                )
            if item["afi"] != args.afi:
                problems.append(
                    f"{place} {item['state']}: afi {item['afi']}, "
                    f"expected {args.afi}"
                )
        src_ip = args.src_ip if args.src_ip is not None else args.address
        dst_ip = args.dst_ip if args.dst_ip is not None else args.address
        if start["src"] != src_ip or start["dst"] != dst_ip:
            problems.append(
                f"{place}: address {start['src']} -> {start['dst']}, "
                f"expected {src_ip} -> {dst_ip}"
            )
        if start["sport"] <= 0 or start["dport"] <= 0 or start["sport"] == start["dport"]:
            problems.append(f"{place}: ports are not a connection")
        if args.src_port is not None and start["sport"] != args.src_port:
            problems.append(f"{place}: src-port {start['sport']}, expected {args.src_port}")
        if args.dst_port is not None and start["dport"] != args.dst_port:
            problems.append(f"{place}: dst-port {start['dport']}, expected {args.dst_port}")
        if args.application is not None and start["app"] != args.application:
            got = start["app"] if start["app"] is not None else "absent"
            problems.append(f"{place}: application {got}, expected {args.application}")
        if not end["end"]:
            problems.append(f"{place}: end datagram has no end-time")
        before = start[args.direction]
        after = end[args.direction]
        if before == 0 and after == 0:
            skipped += 1
            continue
        # One connection carries the file after the start datagram, so its
        # counter grows. A parallel stream may only complete its bind, so
        # --sum accepts an end counter that stayed put and checks the total.
        if after < before or (not args.sum and after == before):
            problems.append(
                f"{place}: usage.{args.direction} end {after} "
                f"is not greater than start {before}"
            )
        measured.append(after)
        if not args.sum and after < args.minimum:
            problems.append(
                f"{place}: usage.{args.direction} end {after} "
                f"is below the file size {args.minimum}"
            )
    if args.sum and skipped != len(complete):
        total = sum(measured)
        if total < args.minimum:
            problems.append(
                f"sum of usage.{args.direction} {total} "
                f"is below the file size {args.minimum}"
            )
    if problems:
        raise FireflyError("\n".join(problems))


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="mode")

    gate = sub.add_parser("gate")
    gate.add_argument("fatal")
    gate.add_argument("records")
    gate.add_argument("offset", type=int)
    gate.add_argument("exp", type=int)
    gate.add_argument("act", type=int)
    gate.add_argument("pairs", type=int)
    gate.add_argument("--select", action="store_true")

    check_p = sub.add_parser("check")
    check_p.add_argument("records")
    check_p.add_argument("offset", type=int)
    check_p.add_argument("exp", type=int)
    check_p.add_argument("act", type=int)
    check_p.add_argument("pairs", type=int)
    check_p.add_argument("direction", choices=("sent", "received"))
    check_p.add_argument("minimum", type=int)
    check_p.add_argument("--src-port", type=int)
    check_p.add_argument("--dst-port", type=int)
    check_p.add_argument("--application")
    check_p.add_argument("--afi", default="ipv4")
    check_p.add_argument("--address", default="127.0.0.1")
    check_p.add_argument("--src-ip")
    check_p.add_argument("--dst-ip")
    check_p.add_argument("--select", action="store_true")
    check_p.add_argument("--sum", action="store_true")

    args = parser.parse_args()
    if not args.mode:
        parser.error("a mode is required")
    if args.mode == "gate":
        try:
            complete, incomplete = load(
                args.records, args.offset, args.exp, args.act, args.select
            )
        except FireflyError as exc:
            with open(args.fatal, "w") as handle:
                print(exc, file=handle)
            return 0
        if len(complete) > args.pairs:
            with open(args.fatal, "w") as handle:
                print(
                    f"{len(complete)} firefly pairs, expected {args.pairs}",
                    file=handle,
                )
            return 0
        if len(complete) == args.pairs and incomplete == 0:
            return 0
        return 1

    try:
        check(args)
    except FireflyError as exc:
        print(exc, flush=True)
        return 1
    return 0


sys.exit(main())
PY
}

# firefly_gate <fatal> <records> <offset> <experiment> <activity> <pairs>
#
# Succeed when that many start/end pairs are in the collector, or when a
# datagram cannot match and the reason was written to <fatal>. Fail while
# the pairs are still arriving, so wait_for keeps polling.
firefly_gate() {
    local fatal=$1
    shift
    firefly_py gate "$fatal" "$@"
}

# firefly_check <records> <offset> <experiment> <activity> <pairs>
#               <sent|received> <bytes> [--src-port P] [--dst-port P]
#               [--application NAME] [--afi ipv4|ipv6] [--address IP]
#               [--src-ip IP] [--dst-ip IP] [--select] [--sum]
#
# <bytes> is the minimum of usage.<direction> on each end datagram. With
# --sum it is the minimum of the sum across the connections, and a
# connection whose counter did not grow is allowed. --afi and --address
# default to an IPv4 client at 127.0.0.1. --src-ip and --dst-ip override
# one side of that pair. --src-port and --dst-port are the known port of
# the flow. --select keeps this tag and ignores any other tag in the slice.
firefly_check() {
    local records=$1 offset=$2 exp=$3 act=$4 pairs=$5 direction=$6 bytes=$7
    shift 7
    local fatal="${BATS_TEST_TMPDIR}/firefly.fatal"
    local select=()

    [[ " $* " == *" --select "* ]] && select=(--select)
    rm -f "$fatal"
    if ! wait_for 10 firefly_gate "$fatal" "$records" "$offset" "$exp" "$act" "$pairs" ${select[@]+"${select[@]}"}; then
        echo "timed out waiting for ${pairs} firefly pair(s) for experiment ${exp} activity ${act}" >&2
        firefly_py check "$records" "$offset" "$exp" "$act" "$pairs" "$direction" "$bytes" "$@" >&2 || true
        firefly_dump "$records" "$offset"
        return 1
    fi
    if [[ -s "$fatal" ]]; then
        cat "$fatal" >&2
        firefly_dump "$records" "$offset"
        return 1
    fi
    if ! firefly_py check "$records" "$offset" "$exp" "$act" "$pairs" "$direction" "$bytes" "$@"; then
        firefly_dump "$records" "$offset"
        return 1
    fi
}
