// A stand-in for the Tag Team Online Worker's room relay (the Durable Object TagRelay) over UDP on this computer, for
// harness.c with TAGLINK_TEST_RELAY=<port>: the owner's messages start with the member's number (removed here), a
// member's reach the owner with its number in front. A client's first message says who it is: "Rh" or "Rm<number>".
import dgram from "node:dgram";
const port = Number(process.argv[2] || 8790);
const sock = dgram.createSocket("udp4");
const who = new Map(); // "h" / "m<n>" -> {address, port}
// LATE=<ms>: nothing passes until that long after a member came (the members connect directly first, as on phones)
const late = Number(process.env.LATE || 0);
// RELAY_MS=<ms>: each message held that long (the real relay: 100-300 ms each way from Brazil)
const hold = Number(process.env.RELAY_MS || 0);
const out = (b, port, address) => hold ? setTimeout(() => sock.send(b, port, address), hold) : sock.send(b, port, address);
let memberAt = 0;
const count = { hm: 0, mh: 0, lost: 0 }; // passed owner -> member, member -> owner, for nobody
setInterval(() => { if (count.hm || count.mh || count.lost) console.log("relay: passed owner->member", count.hm, "member->owner", count.mh, "nobody", count.lost); }, 1000).unref();
const name = (r) => [...who.entries()].find(([, v]) => v.address === r.address && v.port === r.port)?.[0];
sock.on("message", (msg, r) => {
    if (msg.length >= 2 && msg[0] === 0x52 /* R */ && /^R(h|m\d+)$/.test(msg.toString())) {
        who.set(msg.toString().slice(1), { address: r.address, port: r.port });
        if (msg[1] === 0x6d && !memberAt) memberAt = Date.now();
        console.log("relay:", msg.toString().slice(1), "at", r.port);
        return;
    }
    const tag = name(r);
    if (late && (!memberAt || Date.now() - memberAt < late)) return;
    if (tag === "h") {
        const to = who.get("m" + msg[0]);
        if (to) { out(Buffer.from(msg.subarray(1)), to.port, to.address); count.hm++; } else count.lost++;
    } else if (tag && tag.startsWith("m")) {
        const h = who.get("h");
        if (h) { out(Buffer.concat([Buffer.from([Number(tag.slice(1)) & 255]), msg]), h.port, h.address); count.mh++; } else count.lost++;
    }
});
sock.bind(port, "127.0.0.1", () => console.log("relay up on", port));
