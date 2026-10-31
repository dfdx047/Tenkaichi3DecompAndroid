#!/usr/bin/env python3
"""The other player of an online match, on this computer: for developing BT2's online play with one phone.

It does what the app and the game's library (bt2/plugin/online/link.cpp) do on a phone: asks the matchmaking service for a room (or
joins one, or searches), opens a UDP socket, and says hello to the other player's addresses until he answers; then pings him.

    python3 bt2/tools/dev_server/peer.py host                       a room: prints its code and waits for the phone to join
    python3 bt2/tools/dev_server/peer.py join <CODE>                joins the phone's room
    python3 bt2/tools/dev_server/peer.py quick                      quick match
    options: --server http://127.0.0.1:8787  --name Rival  --address <this computer's LAN address>  --seconds 60

The service: bt2/tools/dev_server/serve.mjs (adb reverse tcp:8787 tcp:8787 for the phone). The phone must reach this computer's
LAN address (the same Wi-Fi). The packet format is link.h's: 20 bytes, "BT2N", version 1, type, session, number, clock.
"""
import argparse
import json
import socket
import struct
import sys
import time
import urllib.request

MAGIC, VERSION = 0x4E325442, 1
HELLO, HELLO_ACK, PING, PONG = 1, 2, 3, 4
NETCODE = 6001


def fnv(text):
    h = 2166136261
    for c in text.encode():
        h = ((h ^ c) * 16777619) & 0xFFFFFFFF
    return h


def post(server, path, body):
    request = urllib.request.Request(server + path, data=json.dumps(body).encode(), headers={"Content-Type": "application/json", "User-Agent": "dragonrage-peer/1"}, method="POST")
    try:
        with urllib.request.urlopen(request, timeout=10) as r:
            return json.load(r)
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", "replace")
        try:
            return json.loads(body)
        except ValueError:
            return {"error": f"HTTP {e.code}: {body[:120]}"}


def lan_address():
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        s.connect(("192.168.1.1", 9))
        return s.getsockname()[0]
    except OSError:
        return "127.0.0.1"
    finally:
        s.close()


def stun_address(sock, server=("stun.l.google.com", 19302)):
    """The address the router shows to the internet for this socket (a STUN Binding request), or None."""
    import os
    transaction = os.urandom(12)
    request = struct.pack(">HHI", 1, 0, 0x2112A442) + transaction
    try:
        target = (socket.gethostbyname(server[0]), server[1])
    except OSError:
        return None
    end = time.time() + 3
    while time.time() < end:
        sock.sendto(request, target)
        time.sleep(0.3)
        try:
            data, _ = sock.recvfrom(512)
        except BlockingIOError:
            continue
        if len(data) < 20 or data[8:20] != transaction or struct.unpack(">H", data[:2])[0] != 0x0101: continue
        at = 20
        while at + 4 <= len(data):
            kind, length = struct.unpack(">HH", data[at:at + 4])
            if kind == 0x0020 and length >= 8:
                port = struct.unpack(">H", data[at + 6:at + 8])[0] ^ 0x2112
                ip = bytes(b ^ m for b, m in zip(data[at + 8:at + 12], b"\x21\x12\xa4\x42"))
                return ".".join(map(str, ip)) + ":" + str(port)
            at += 4 + ((length + 3) & ~3)
    return None


def packet(kind, session, number, echo):
    return struct.pack("<IBBHIII", MAGIC, VERSION, kind, 0, session, number, echo)


def millis():
    return int(time.monotonic() * 1000) & 0xFFFFFFFF


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("how", choices=["host", "join", "quick"])
    ap.add_argument("code", nargs="?")
    ap.add_argument("--server", default="http://127.0.0.1:8787")
    ap.add_argument("--name", default="Rival")
    ap.add_argument("--address", default=None)
    ap.add_argument("--stun", action="store_true", help="publish the address the router shows to the internet (for a test across networks)")
    ap.add_argument("--seconds", type=int, default=60)
    ap.add_argument("--relay-only", action="store_true", help="send only to the host's relay= address (to test the TURN relay)")
    args = ap.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind(("0.0.0.0", 0))
    sock.setblocking(False)
    mine = f"{args.address or lan_address()}:{sock.getsockname()[1]}"
    if args.stun:
        public = stun_address(sock)
        print("public address (STUN):", public)
        if public: mine = public
    print("my address:", mine)

    if args.how == "host":
        room = post(args.server, "/v1/host", {"v": NETCODE, "name": args.name, "addrs": [mine]})
        if "error" in room: sys.exit(f"service: {room['error']}")
        code, key, peer = room["code"], room["key"], None
        print("ROOM CODE:", code, flush=True)
    elif args.how == "join":
        if not args.code: sys.exit("join needs the room's code")
        room = post(args.server, "/v1/join", {"v": NETCODE, "code": args.code.upper(), "name": args.name, "addrs": [mine]})
        if "error" in room: sys.exit(f"service: {room['error']}")
        code, key, peer = args.code.upper(), room["key"], room["peer"]
    else:
        room = post(args.server, "/v1/quick", {"v": NETCODE, "name": args.name, "addrs": [mine], "queue": "S", "who": "peerpy" + str(int(time.time()))})
        if "error" in room: sys.exit(f"service: {room['error']}")
        code, key = room["code"], room["key"]
        peer = room.get("peer") if room["role"] == "join" else None
        print("quick match:", room["role"], "room", code, flush=True)

    deadline = time.time() + args.seconds
    while peer is None and time.time() < deadline:   # the host waits for the other player to join
        time.sleep(1)
        peer = post(args.server, "/v1/poll", {"code": code, "key": key}).get("peer")
    if peer is None: sys.exit("nobody came")
    print("paired with", peer["name"], peer["addrs"], flush=True)

    session = fnv(code)
    targets = []
    for a in peer["addrs"]:
        if a.startswith("seen="): continue
        if args.relay_only and not a.startswith("relay="): continue
        host, port = a.removeprefix("relay=").rsplit(":", 1)
        targets.append((host, int(port)))
    remote, up, number, last_hello, last_ping, rtts = None, False, 0, 0, 0, []
    while time.time() < deadline:
        now = millis()
        if not up and now - last_hello >= 100:
            last_hello = now
            for t in targets:
                number += 1
                sock.sendto(packet(HELLO, session, number, now), t)
        if up and now - last_ping >= 500:
            last_ping = now
            number += 1
            sock.sendto(packet(PING, session, number, now), remote)
        try:
            while True:
                data, source = sock.recvfrom(64)
                if len(data) < 20: continue
                magic, version, kind, _, sess, num, echo = struct.unpack("<IBBHIII", data[:20])
                if magic != MAGIC or version != VERSION or sess != session: continue
                if kind == HELLO:
                    sock.sendto(packet(HELLO_ACK, session, num, echo), source)
                if kind in (HELLO, HELLO_ACK, PING, PONG) and not up:
                    up, remote = True, source
                    print("LINE UP with", source, flush=True)
                if kind == PING:
                    sock.sendto(packet(PONG, session, num, echo), source)
                if kind == PONG:
                    rtts.append((millis() - echo) & 0xFFFFFFFF)
                    print("round trip", rtts[-1], "ms", flush=True)
        except BlockingIOError:
            pass
        time.sleep(0.01)
    print("up:", up, "round trips:", rtts[-5:])
    post(args.server, "/v1/leave", {"code": code, "key": key})


if __name__ == "__main__":
    main()
