import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import { fileURLToPath } from "node:url";
import { createDatabase } from "./db.js";

const here = new URL(".", import.meta.url);
const workerPath = new URL("../matchmaking/worker.js", here);
const workerSource = await readFile(workerPath, "utf8");
// Import from source text so the parent repository does not need a package.json or module-mode change.
const workerUrl = `data:text/javascript;base64,${Buffer.from(workerSource).toString("base64")}`;
const worker = (await import(workerUrl)).default;
const DB = createDatabase(process.env.DB_PATH || "./matchmaking.sqlite");
const env = {
    DB,
    BUILD_KEYS: process.env.BUILD_KEYS,
    REQUIRE_KEY: process.env.REQUIRE_KEY,
    MIN_RELEASE: process.env.MIN_RELEASE,
    LATEST_RELEASE: process.env.LATEST_RELEASE,
    ADMIN_KEY: process.env.ADMIN_KEY,
};

function loopback(address) {
    return address === "127.0.0.1" || address === "::1" || address === "::ffff:127.0.0.1";
}

async function requestBody(req) {
    const parts = [];
    for await (const part of req) parts.push(part);
    return Buffer.concat(parts);
}

const server = createServer(async (req, res) => {
    try {
        const socketIp = req.socket.remoteAddress || "";
        const headers = new Headers();
        for (const [name, value] of Object.entries(req.headers)) {
            if (Array.isArray(value)) value.forEach((v) => headers.append(name, v));
            else if (value !== undefined) headers.set(name, value);
        }
        const forwarded = loopback(socketIp) ? headers.get("x-forwarded-for")?.split(",")[0]?.trim() : "";
        headers.set("cf-connecting-ip", forwarded || socketIp);

        const body = await requestBody(req);
        const init = { method: req.method, headers };
        if (req.method !== "GET" && req.method !== "HEAD") {
            init.body = body;
            init.duplex = "half";
        }
        const request = new Request(new URL(req.url || "/", `http://${req.headers.host || "127.0.0.1:8787"}`), init);
        // Node Request has no Cloudflare-specific `cf` property.
        const ctx = { waitUntil(p) { Promise.resolve(p).catch(console.error); } };
        const response = await worker.fetch(request, env, ctx);
        res.writeHead(response.status, Object.fromEntries(response.headers.entries()));
        res.end(Buffer.from(await response.arrayBuffer()));
    } catch (error) {
        console.error(error);
        if (!res.headersSent) res.writeHead(500, { "content-type": "application/json" });
        res.end('{"error":"internal error"}');
    }
});

server.listen(8787, "127.0.0.1", () => {
    console.log("matchmaking listening on http://127.0.0.1:8787");
});

function shutdown() {
    server.close(() => {
        DB.close();
        process.exit(0);
    });
}
process.on("SIGINT", shutdown);
process.on("SIGTERM", shutdown);
