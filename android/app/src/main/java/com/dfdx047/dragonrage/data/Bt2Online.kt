package com.dfdx047.dragonrage.data

import org.json.JSONArray
import org.json.JSONObject
import java.net.HttpURLConnection
import java.net.Inet4Address
import java.net.NetworkInterface
import java.net.URL

/**
 * BT2's line to the Dragon Net matchmaking service (port/dragonnet/worker.js: rooms with a code, and quick match):
 * the same requests Dragon Rage's BT3 makes, with a netcode version of its own, so the service never pairs a BT2 player
 * with a BT3 one. Nothing of the game passes through it: it only tells each side the other's name and addresses. The
 * connection between the two games, and the fight, are not here.
 *
 * The new_online routes use the Supabase function directly; a device-local URL override is intentionally ignored.
 */
object Bt2Online {
    /** BT2's netcode version on the service (the service's ALLOWED_VERSIONS must list it). BT3's is 5001. */
    const val VERSION = 6001
    /** Shared Amistoso matchmaking service. */
    const val SERVICE = "https://jeowmebvwxcqfbmcjqlf.supabase.co/functions/v1/bt3"
    private const val REGION = "us-east-2"
    private const val QUEUE = "S" // single fights
    private const val GAME_PORT = 7000 // (the port the game will listen on, when it connects: for now only what the addresses say)

    class Failure(val why: String, val httpStatus: Int = 0, val errorField: String = why) : Exception(why)
    data class Peer(val name: String, val addrs: List<String>)
    /** A room this player is in: its code, this player's key in it, and the other player when already there. */
    data class Room(val code: String, val key: String, val peer: Peer?, val hosting: Boolean)

    fun url(@Suppress("UNUSED_PARAMETER") paths: GamePaths): String = SERVICE

    /** The name as the service takes it: printable ASCII, 16 letters at most ("Player" when nothing is left). */
    fun cleanName(name: String): String = name.filter { it.code in 0x20..0x7e }.trim().take(16).ifEmpty { tr("Player", "Jogador", "Jugador") }

    fun host(base: String, name: String, mine: List<String>): Room {
        val r = post(base, "/v1/host", JSONObject().put("v", VERSION).put("name", cleanName(name)).put("addrs", addrs(mine)))
        return Room(requiredString(r, "code"), requiredString(r, "key"), null, true)
    }

    fun join(base: String, code: String, name: String, mine: List<String>): Room {
        val r = post(base, "/v1/join", JSONObject().put("v", VERSION).put("code", code.trim().uppercase()).put("name", cleanName(name)).put("addrs", addrs(mine)))
        val peerJson = r.optJSONObject("peer") ?: throw invalidResponse()
        return Room(code.trim().uppercase(), requiredString(r, "key"), peer(peerJson), false)
    }

    /** Quick match: put with whoever waits longest, or given a room of its own to wait in. */
    fun quick(base: String, name: String, who: String, mine: List<String>): Room {
        val r = post(base, "/v1/quick", JSONObject().put("v", VERSION).put("name", cleanName(name)).put("addrs", addrs(mine)).put("queue", QUEUE).put("who", who))
        val join = r.optString("role") == "join"
        val joinedPeer = if (join) r.optJSONObject("peer")?.let(::peer) ?: throw invalidResponse() else null
        return Room(requiredString(r, "code"), requiredString(r, "key"), joinedPeer, !join)
    }

    /** The one who joined the room this player hosts, once there. */
    fun poll(base: String, room: Room): Peer? =
        post(base, "/v1/poll", JSONObject().put("code", room.code).put("key", room.key).put("wait", 20), readTimeoutMs = 40_000)
            .optJSONObject("peer")?.let(::peer)

    fun leave(base: String, room: Room) {
        runCatching { post(base, "/v1/leave", JSONObject().put("code", room.code).put("key", room.key)) }
    }

    /** A line of these screens in the device's language (English where it is neither of the other two). */
    fun tr(en: String, pt: String, es: String): String = when (java.util.Locale.getDefault().language) {
        "pt" -> pt
        "es" -> es
        else -> en
    }

    /** What the service said, in words for the player. */
    fun explain(why: String): String = when (why) {
        "version" -> tr("The other player's game is another version.", "O jogo da outra pessoa é de outra versão.", "El juego de la otra persona es de otra versión.")
        "no room" -> tr("Room not found. Check the code.", "Sala não encontrada. Confira o código.", "Sala no encontrada. Comprueba el código.")
        "full" -> tr("The room is already full.", "A sala já está cheia.", "La sala ya está llena.")
        "busy" -> tr("Too many tries. Wait a little.", "Muitas tentativas. Espere um pouco.", "Demasiados intentos. Espera un poco.")
        "old" -> tr("This game is out of date.", "Este jogo está desatualizado.", "Este juego está desactualizado.")
        "build key" -> tr("The server only takes official versions of the game.", "O servidor só aceita versões oficiais do jogo.", "El servidor solo acepta versiones oficiales del juego.")
        "bad request" -> tr("The server did not take the request.", "O servidor não aceitou o pedido.", "El servidor no aceptó la petición.")
        "network" -> tr("No connection to the server.", "Sem conexão com o servidor.", "Sin conexión con el servidor.")
        "limit" -> tr("Server busy, try again later.", "Servidor ocupado, tente mais tarde.", "Servidor ocupado, inténtalo más tarde.")
        else -> tr("The server said: $why", "O servidor respondeu: $why", "El servidor respondió: $why")
    }

    private fun peer(o: JSONObject) = Peer(o.optString("name", tr("Player", "Jogador", "Jugador")), (0 until (o.optJSONArray("addrs")?.length() ?: 0)).map { o.getJSONArray("addrs").getString(it) })

    private fun requiredString(o: JSONObject, field: String): String =
        o.optString(field).takeIf { it.isNotEmpty() } ?: throw invalidResponse()

    private fun invalidResponse() = Failure("invalid_response", 200, "invalid_response")

    /** This phone's addresses as the service wants them ("ip:port", four at most): the ones the game's socket reported (the
     *  line to the other game is that socket's: bt2/plugin/online/link.h), else the phone's own with a port of no use. */
    private fun addrs(mine: List<String>): JSONArray {
        val good = mine.filter { Regex("""[0-9.]{7,15}:\d{1,5}""").matches(it) }
        if (good.isNotEmpty()) return JSONArray(good.take(4))
        val found = runCatching {
            NetworkInterface.getNetworkInterfaces().toList().filter { it.isUp && !it.isLoopback }
                .flatMap { it.inetAddresses.toList() }.filterIsInstance<Inet4Address>().mapNotNull { it.hostAddress }
        }.getOrDefault(emptyList())
        return JSONArray((found.ifEmpty { listOf("0.0.0.0") }).take(4).map { "$it:$GAME_PORT" })
    }

    private fun post(base: String, path: String, body: JSONObject, readTimeoutMs: Int = 12_000): JSONObject {
        val c = (URL(base + path).openConnection() as HttpURLConnection)
        try {
            c.requestMethod = "POST"
            c.connectTimeout = 6000
            c.readTimeout = readTimeoutMs
            c.doOutput = true
            c.setRequestProperty("Content-Type", "application/json")
            c.setRequestProperty("x-region", REGION)
            c.outputStream.use { it.write(body.toString().toByteArray()) }
            val status = c.responseCode
            val stream = if (status in 200..299) c.inputStream else c.errorStream
            val text = stream?.bufferedReader()?.use { it.readText() }.orEmpty()
            val json = runCatching { JSONObject(text) }.getOrElse {
                throw Failure("invalid_response", status, "invalid_response")
            }
            val error = json.optString("error").takeIf { it.isNotEmpty() }
            if (error != null) throw Failure(error, status, error)
            if (status !in 200..299) throw Failure("http", status, "")
            return json
        } catch (e: Failure) {
            throw e
        } catch (e: Exception) {
            throw Failure("network", errorField = "network")
        } finally {
            c.disconnect()
        }
    }
}
