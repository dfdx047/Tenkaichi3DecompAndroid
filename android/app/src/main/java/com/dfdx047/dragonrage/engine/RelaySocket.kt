package com.dfdx047.dragonrage.engine

import android.util.Log
import java.io.BufferedInputStream
import java.io.InputStream
import java.io.OutputStream
import java.net.URI
import java.security.SecureRandom
import java.util.Base64
import java.util.concurrent.ConcurrentLinkedQueue
import javax.net.ssl.SSLSocket
import javax.net.ssl.SSLSocketFactory

/**
 * The Tag Team rooms' relay (port/src/gs/tag_link.c, port/tagnet-worker): one WebSocket to the room's relay at the
 * Tag Team Online service, for players who cannot reach each other directly. Binary messages only: what the engine
 * sends goes out as one message, what comes in waits in [inbox] until the engine reads it ([GameActivity.relayRecv]).
 * A small client of its own (RFC 6455: the handshake, masked frames out, ping answered), as Android has none built in.
 */
class RelaySocket private constructor(private val socket: SSLSocket, private val input: InputStream, private val output: OutputStream) {
    private val inbox = ConcurrentLinkedQueue<ByteArray>()
    private val random = SecureRandom()
    @Volatile var open = true
        private set

    private val reader = Thread({ readLoop() }, "tag-relay").apply { isDaemon = true; start() }

    fun send(data: ByteArray): Boolean = frame(0x2, data)

    fun receive(): ByteArray? = inbox.poll()

    fun close() {
        if (!open) return
        frame(0x8, ByteArray(0))
        open = false
        runCatching { socket.close() }
    }

    @Synchronized
    private fun frame(opcode: Int, data: ByteArray): Boolean {
        if (!open) return false
        return try {
            val head = ByteArray(14)
            var n = 0
            head[n++] = (0x80 or opcode).toByte()
            when {
                data.size < 126 -> head[n++] = (0x80 or data.size).toByte()
                data.size < 65536 -> {
                    head[n++] = (0x80 or 126).toByte(); head[n++] = (data.size shr 8).toByte(); head[n++] = data.size.toByte()
                }
                else -> return false
            }
            val mask = ByteArray(4).also { random.nextBytes(it) }
            System.arraycopy(mask, 0, head, n, 4); n += 4
            val body = ByteArray(data.size) { (data[it].toInt() xor mask[it and 3].toInt()).toByte() }
            output.write(head, 0, n)
            output.write(body)
            output.flush()
            true
        } catch (e: Exception) {
            open = false
            false
        }
    }

    private fun readFully(buf: ByteArray, len: Int) {
        var at = 0
        while (at < len) {
            val r = input.read(buf, at, len - at)
            if (r < 0) throw java.io.EOFException()
            at += r
        }
    }

    private fun readLoop() {
        val head = ByteArray(8)
        try {
            while (open) {
                readFully(head, 2)
                val opcode = head[0].toInt() and 0x0F
                var len = (head[1].toInt() and 0x7F).toLong()
                val masked = head[1].toInt() and 0x80 != 0
                if (len == 126L) { readFully(head, 2); len = ((head[0].toInt() and 0xFF) shl 8 or (head[1].toInt() and 0xFF)).toLong() }
                else if (len == 127L) { readFully(head, 8); len = 0; for (i in 0 until 8) len = len shl 8 or (head[i].toLong() and 0xFF) }
                if (len > 1 shl 20) throw java.io.IOException("message too large")
                val mask = ByteArray(4)
                if (masked) readFully(mask, 4)
                val data = ByteArray(len.toInt())
                readFully(data, data.size)
                if (masked) for (i in data.indices) data[i] = (data[i].toInt() xor mask[i and 3].toInt()).toByte()
                when (opcode) {
                    0x2 -> if (inbox.size < 4096) inbox.add(data)
                    0x9 -> frame(0xA, data) // ping: pong
                    0x8 -> { open = false }
                }
            }
        } catch (e: Exception) {
            if (open) Log.w("bt3", "tag relay: closed: $e")
        }
        open = false
        runCatching { socket.close() }
    }

    companion object {
        /** Opens the WebSocket at [url] ("https://host/path?query"); null when it could not. Blocks for the handshake. */
        fun connect(url: String): RelaySocket? = try {
            val u = URI(url)
            val port = if (u.port > 0) u.port else 443
            val s = SSLSocketFactory.getDefault().createSocket(u.host, port) as SSLSocket
            s.soTimeout = 0
            s.tcpNoDelay = true
            s.startHandshake()
            val out = s.outputStream
            val input = BufferedInputStream(s.inputStream)
            val key = Base64.getEncoder().encodeToString(ByteArray(16).also { SecureRandom().nextBytes(it) })
            val path = (u.rawPath ?: "/") + (u.rawQuery?.let { "?$it" } ?: "")
            out.write(("GET $path HTTP/1.1\r\nHost: ${u.host}\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n" +
                "Sec-WebSocket-Key: $key\r\nSec-WebSocket-Version: 13\r\nUser-Agent: DragonRage-Android\r\n\r\n").toByteArray())
            out.flush()
            val status = StringBuilder()
            var last4 = 0
            while (true) { // the answer's header, up to its empty line
                val c = input.read()
                if (c < 0) break
                if (status.length < 4096) status.append(c.toChar())
                last4 = (last4 shl 8) or c
                if (last4 == 0x0D0A0D0A) break
            }
            if (!status.startsWith("HTTP/1.1 101")) {
                Log.w("bt3", "tag relay: refused: ${status.lineSequence().firstOrNull()}")
                runCatching { s.close() }
                null
            } else RelaySocket(s, input, out)
        } catch (e: Exception) {
            Log.w("bt3", "tag relay: could not connect: $e")
            null
        }
    }
}
