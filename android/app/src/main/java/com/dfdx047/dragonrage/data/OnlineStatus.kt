package com.dfdx047.dragonrage.data

import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.json.JSONObject

/**
 * The state of online play as the matchmaking service sees this build (the PC port's service: port/matchmaking): asked
 * when the launcher opens, with the two requests that change nothing there. "hello" says which PC release the
 * service lets in (MIN_RELEASE) and the newest one; "queues" says how many players search now, and is refused with
 * "build key" when the service only lets in signed (official PC) releases.
 */
sealed interface OnlineState {
    data object Checking : OnlineState
    data object Unreachable : OnlineState
    data class Outdated(val mine: Int, val min: Int) : OnlineState
    data class OfficialOnly(val mine: Int) : OnlineState
    data class Working(val mine: Int, val searching: Int, val latest: Int, val dragon: Boolean = false) : OnlineState
    /** Dragon Net's service (Android + Switch) does not take this build's netcode yet (it is updated by its keeper). */
    data class DragonVersion(val mine: Int) : OnlineState
    /** The service answered HTTP 429: over its limit (a free Worker's daily requests, Cloudflare's error 1027). */
    data object Limited : OnlineState
}

object OnlineStatus {
    private const val NET_VERSION = 6 // the online protocol's version (port/src/gs/net.c NET_VERSION)
    private const val DRNET_SIM = 1   // Dragon Net's simulation version (port/src/gs/net_dragon.h): v = NET_VERSION * 1000 + DRNET_SIM
    /** Dragon Net (Android + Switch): the official service of both (the Tenkaichi3DecompAndroidOnline fork's Worker,
     *  running port/dragonnet); another one in dragonnet_url.txt. */
    const val DRAGON_URL = "https://mitigo-rooms.minetivacraft.workers.dev"
    const val MODE = "net_mode" // the engine's setting: 0 Dragon Net (Android + Switch), 1 against PC

    fun dragonUrl(paths: GamePaths): String = runCatching { File(paths.root, "dragonnet_url.txt").readText().trim() }.getOrNull()
        ?.takeIf { it.startsWith("https://") }?.trimEnd('/') ?: DRAGON_URL
    const val DEFAULT_URL = "https://odd-surf-0af7.bt3decomp.workers.dev"

    /** The service in use: match_url.txt in the game folder (the engine reads it too, net_match.c), else the official. */
    fun current(paths: GamePaths): String = runCatching { File(paths.root, "match_url.txt").readText().trim() }.getOrNull()
        ?.takeIf { it.startsWith("https://") }?.trimEnd('/') ?: DEFAULT_URL

    fun choose(paths: GamePaths, url: String) {
        val f = File(paths.root, "match_url.txt")
        if (url == DEFAULT_URL) runCatching { f.delete() } else runCatching { f.writeText(url + "\n") }
    }

    /** Why the last check could not reach the service (the exception or the HTTP answer), for the app's diary. */
    @Volatile var lastFailure: String? = null

    private fun post(url: String, body: String): Pair<Int, String> {
        val c = (URL(url).openConnection() as HttpURLConnection).apply {
            requestMethod = "POST"
            doOutput = true
            connectTimeout = 8_000
            readTimeout = 10_000
            setRequestProperty("Content-Type", "application/json")
            setRequestProperty("User-Agent", "DragonRage-Android")
        }
        return try {
            c.outputStream.use { it.write(body.toByteArray()) }
            val code = c.responseCode
            val text = (if (code in 200..299) c.inputStream else c.errorStream)?.bufferedReader()?.use { it.readText() } ?: ""
            code to text
        } finally {
            c.disconnect()
        }
    }

    private fun failed(url: String, e: Throwable): Pair<Int, String>? {
        lastFailure = "${url.substringAfter("://").substringBefore('/')}: ${e.javaClass.simpleName}: ${e.message}"
        return null
    }

    private fun unreachable(code: Int, text: String): OnlineState {
        lastFailure = "HTTP $code ${text.take(120).replace('\n', ' ')}"
        return OnlineState.Unreachable
    }

    suspend fun check(paths: GamePaths, mine: Int, mode: Int = 0): OnlineState = withContext(Dispatchers.IO) {
        lastFailure = null
        if (mode == 0) return@withContext checkDragon(paths, mine)
        val base = current(paths)
        val hello = runCatching { post("$base/v1/hello", """{"rel":$mine}""") }.getOrElse { failed(base, it) }
            ?: return@withContext OnlineState.Unreachable
        if (hello.first == 429) return@withContext OnlineState.Limited
        if (hello.first !in 200..299) return@withContext unreachable(hello.first, hello.second)
        val h = runCatching { JSONObject(hello.second) }.getOrNull() ?: return@withContext OnlineState.Unreachable
        val min = h.optInt("min", 0)
        val latest = h.optInt("latest", 0)
        if (min > mine) return@withContext OnlineState.Outdated(mine, min)
        val q = runCatching { post("$base/v1/queues", """{"v":$NET_VERSION,"rel":$mine}""") }.getOrNull()
        if (q != null && q.first == 403 && q.second.contains("build key")) return@withContext OnlineState.OfficialOnly(mine)
        var searching = 0
        if (q != null && q.first in 200..299) {
            runCatching {
                val o = JSONObject(q.second).getJSONObject("waiting")
                o.keys().forEach { k -> searching += o.optInt(k, 0).coerceAtLeast(0) }
            }
        }
        OnlineState.Working(mine, searching, latest)
    }

    /** Dragon Net: no release gate (only the netcode: v), so it does not wait for the PC port's releases. */
    private fun checkDragon(paths: GamePaths, mine: Int): OnlineState {
        val url = dragonUrl(paths)
        val q = runCatching { post("$url/v1/queues", """{"v":${NET_VERSION * 1000 + DRNET_SIM},"rel":$mine}""") }.getOrElse { failed(url, it) }
            ?: return OnlineState.Unreachable
        if (q.first == 409 && q.second.contains("version")) return OnlineState.DragonVersion(mine)
        if (q.first == 429) return OnlineState.Limited
        if (q.first !in 200..299) return unreachable(q.first, q.second)
        var searching = 0
        runCatching {
            val o = JSONObject(q.second).getJSONObject("waiting")
            o.keys().forEach { k -> searching += o.optInt(k, 0).coerceAtLeast(0) }
        }
        return OnlineState.Working(mine, searching, 0, dragon = true)
    }

    fun release(n: Int): String = "0.${n / 100}.${n % 100}"
}
