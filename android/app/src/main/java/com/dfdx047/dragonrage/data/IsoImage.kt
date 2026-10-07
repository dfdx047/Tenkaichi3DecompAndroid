package com.dfdx047.dragonrage.data

import java.io.Closeable
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.nio.channels.FileChannel

/**
 * A read-only ISO 9660 image (the layout of port/setup/native.cpp): the primary volume descriptor at sector 16,
 * then the directory records from the root. Paths are kept as on the disc ("DATA/PZS3US1.AFS"), without ";1".
 */
class IsoImage(private val channel: FileChannel) : Closeable {
    data class Entry(val path: String, val offset: Long, var size: Long)

    val files = mutableListOf<Entry>()

    init {
        val pvd = read(16L * SECTOR, SECTOR)
        if (pvd[0].toInt() != 1 || String(pvd, 1, 5, Charsets.US_ASCII) != "CD001") {
            throw IOException("Não é uma imagem ISO 9660")
        }
        val root = ByteBuffer.wrap(pvd).order(ByteOrder.LITTLE_ENDIAN)
        val lba = root.getInt(156 + 2).toLong() and 0xFFFFFFFFL
        val size = root.getInt(156 + 10).toLong() and 0xFFFFFFFFL
        walk(lba, size, "", 0)
    }

    fun find(path: String): Entry? = files.firstOrNull { it.path.equals(path, ignoreCase = true) }

    fun read(offset: Long, length: Int): ByteArray {
        val out = ByteArray(length)
        readInto(offset, ByteBuffer.wrap(out))
        return out
    }

    fun readInto(offset: Long, buf: ByteBuffer) {
        var pos = offset
        while (buf.hasRemaining()) {
            val n = channel.read(buf, pos)
            if (n < 0) throw IOException("A imagem terminou antes do esperado (arquivo incompleto?)")
            pos += n
        }
    }

    private fun walk(lba: Long, size: Long, dir: String, depth: Int) {
        if (depth > 8 || size > 16L * 1024 * 1024) return
        val d = read(lba * SECTOR, size.toInt())
        val bb = ByteBuffer.wrap(d).order(ByteOrder.LITTLE_ENDIAN)
        var pos = 0
        while (pos + 34 <= d.size) {
            val len = d[pos].toInt() and 0xFF
            if (len == 0) { // records do not cross sectors: the rest of this one is padding
                pos = (pos / SECTOR + 1) * SECTOR
                continue
            }
            if (pos + len > d.size) break
            val extent = bb.getInt(pos + 2).toLong() and 0xFFFFFFFFL
            val bytes = bb.getInt(pos + 10).toLong() and 0xFFFFFFFFL
            val flags = d[pos + 25].toInt()
            val nameLen = d[pos + 32].toInt() and 0xFF
            val rawName = String(d, pos + 33, minOf(nameLen, len - 33).coerceAtLeast(0), Charsets.ISO_8859_1)
            pos += len
            if (nameLen == 1 && (rawName[0] == '\u0000' || rawName[0] == '\u0001')) continue // "." and ".."
            var name = rawName.substringBefore(';')
            if (name.endsWith('.') && flags and 2 == 0) name = name.dropLast(1)
            val path = if (dir.isEmpty()) name else "$dir/$name"
            if (flags and 2 != 0) {
                walk(extent, bytes, path, depth + 1)
            } else {
                val last = files.lastOrNull()
                if (last != null && last.path == path) last.size += bytes // a file in several extents
                else files += Entry(path, extent * SECTOR, bytes)
            }
        }
    }

    override fun close() = channel.close()

    companion object {
        const val SECTOR = 2048
    }
}
