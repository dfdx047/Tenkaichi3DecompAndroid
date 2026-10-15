#!/usr/bin/env python3
"""The game's side of the build key, checked against Python's own hmac and hashlib.

    python3 port/matchmaking/test_sign.py port/build/bt3_64            a program built without a key
    BT3_BUILD_KEY=<the dummy key it was built with> python3 port/matchmaking/test_sign.py port/build/bt3_64

What it does (nothing leaves this computer: the program is only ever pointed at 127.0.0.1):
  1. runs the program with BT3_SIGN_TEST=1, which prints the text and the signature of one fixed request and ends,
     and compares them with what Python computes (the same request is in test.mjs, for the Worker);
  2. listens on 127.0.0.1 and has the program send one request each way it can (BT3_SIGN_TEST=post: the curl
     library, then with BT3_MATCH_CURL=1 the `curl` program; on Windows the first is WinHTTP), and looks at the
     headers that arrived: none of the three without a key; with a key, a signature that verify() below accepts;
  3. with a key: answers the first request as the Worker answers one whose time is off, with a clock 100000 seconds
     ahead, and expects the request again, signed for that clock.
Use a made-up key here, never a release's: the script does not print it, but it is in your shell's history.
A prefix command for the program (wine, for bt3.exe) can be given after it."""
import hashlib, hmac, http.server, json, os, subprocess, sys, threading, time

DUMMY = b"dummy-test-key-0123456789-not-a-secret"  # the fixed test key of net_sign.c: no secret
BODY = b'{"v":4,"rel":134,"name":"Test","addrs":["203.0.113.5:7000"],"queue":"S","who":"abcdefghijklmnop"}'
TOLERANCE = 300


def canonical(method, path, release, when, body):
    return "\n".join(["BT3-SIGN-1", method, path, str(release), str(when), hashlib.sha256(body).hexdigest()])


def sign(key, method, path, release, when, body):
    return hmac.new(key, canonical(method, path, release, when, body).encode(), hashlib.sha256).hexdigest()


def verify(keys, method, path, headers, body, now):
    """What the Worker's keyCheck does: None if the request is rightly signed, else why not."""
    release, when, sig = headers.get("X-BT3-Release", ""), headers.get("X-BT3-Time", ""), headers.get("X-BT3-Sign", "")
    if sig == "":
        return "none"
    if release not in keys:
        return "release"
    if not when.isdigit() or not hmac.compare_digest(sig, sign(keys[release], method, path, release, when, body)):
        return "sign"
    return "time" if abs(now - int(when)) > TOLERANCE else None


fails = 0


def check(name, ok):
    global fails
    print(("ok   " if ok else "FAIL ") + name)
    fails += 0 if ok else 1


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    program, prefix = os.path.abspath(sys.argv[1]), sys.argv[2:]
    key = os.environ.get("BT3_BUILD_KEY", "").strip().encode()
    env = {k: v for k, v in os.environ.items() if k not in ("BT3_BUILD_KEY", "BT3_MATCH_CURL")}

    def run(more):
        r = subprocess.run(prefix + [program], env={**env, **more}, capture_output=True, text=True, timeout=60)
        return {l.split(" ", 2)[1]: l.split(" ", 2)[2].strip() for l in r.stdout.splitlines() if l.startswith("sign: ") and l.count(" ") >= 2}

    # 1. the fixed request
    out = run({"BT3_SIGN_TEST": "1"})
    check("SHA-256 of 'abc'", out.get("sha256(abc)") == hashlib.sha256(b"abc").hexdigest())
    check("the text that is signed", out.get("text") == canonical("POST", "/v1/quick", 134, 1791900000, BODY).replace("\n", "|"))
    check("its signature with the dummy key", out.get("dummy") == "key " + sign(DUMMY, "POST", "/v1/quick", 134, 1791900000, BODY))
    if key:
        check("its signature with the key the program was built with", out.get("build") == "key yes " + sign(key, "POST", "/v1/quick", 134, 1791900000, BODY))
        data = open(program, "rb").read()
        check("the key is not in the program as it is written (it is scrambled)", key not in data and key.decode().encode("utf-16-le") not in data)
    else:
        check("the program says it has no key", out.get("build") == "key none")

    # 2. and 3. what goes out
    seen, plan = [], {"skew": 0}

    class Handler(http.server.BaseHTTPRequestHandler):
        def do_POST(self):
            body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
            seen.append((self.path, {k: v for k, v in self.headers.items()}, body))
            now = int(time.time()) + plan["skew"]
            why = verify(plan.get("keys", {}), "POST", self.path, self.headers, body, now) if plan.get("keys") else None
            if why is not None:
                answer, status = {"error": "build key", "why": why, "now": now}, 403
            else:
                answer, status = {"min": 0, "latest": 0, "now": now}, 200
            text = json.dumps(answer, separators=(",", ":")).encode()
            self.send_response(status)
            self.send_header("Content-Type", "application/json")
            self.send_header("Content-Length", str(len(text)))
            self.end_headers()
            self.wfile.write(text)

        def log_message(self, *a):
            pass

    server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    url = f"http://127.0.0.1:{server.server_address[1]}"
    ours = ("x-bt3-release", "x-bt3-time", "x-bt3-sign")
    for way, more in (("the system's own HTTP", {}), ("the curl program", {"BT3_MATCH_CURL": "1"})):
        seen.clear()
        plan.clear()
        plan["skew"] = 0
        out = run({"BT3_SIGN_TEST": "post", "BT3_MATCH_URL": url, **more})
        if len(seen) == 0:
            check(f"{way}: a request arrived", False)
            continue
        path, headers, body = seen[0]
        sent = {k: v for k, v in headers.items() if k.lower() in ours}
        release = json.loads(body).get("rel")
        check(f"{way}: one request, to /v1/hello, and its answer read", len(seen) == 1 and path == "/v1/hello" and '"min":0' in out.get("post", ""))
        if not key:
            check(f"{way}: without a key none of the three headers is sent", sent == {})
            continue
        check(f"{way}: the three headers are sent, the release as in the body", len(sent) == 3 and headers.get("X-BT3-Release") == str(release))
        check(f"{way}: the signature is the key's for this request, the time is now", verify({str(release): key}, "POST", path, headers, body, int(time.time())) is None)
        check(f"{way}: another key does not verify it", verify({str(release): DUMMY}, "POST", path, headers, body, int(time.time())) == "sign")
        check(f"{way}: nor does it verify another body", verify({str(release): key}, "POST", path, headers, body + b" ", int(time.time())) == "sign")
        # 3. a service whose clock is 100000 seconds ahead
        seen.clear()
        plan["keys"], plan["skew"] = {str(release): key}, 100000
        out = run({"BT3_SIGN_TEST": "post", "BT3_MATCH_URL": url, **more})
        check(f"{way}: refused for its time, the request is sent once more by the service's clock and accepted",
              len(seen) == 2 and abs(int(seen[1][1].get("X-BT3-Time", "0")) - int(time.time()) - 100000) <= 5 and '"min":0' in out.get("post", ""))
    server.shutdown()
    print("all passed" if fails == 0 else f"{fails} failed")
    sys.exit(0 if fails == 0 else 1)


if __name__ == "__main__":
    main()
