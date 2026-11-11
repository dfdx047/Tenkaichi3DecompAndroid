package com.dfdx047.dragonrage.data

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import com.dfdx047.dragonrage.R
import java.io.File
import java.io.FileInputStream
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest
import java.util.zip.CRC32
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Budokai Tenkaichi 4 (BT4decomp): what its PC setup does (port/setup/native.cpp, step_data), for
 * Android. Team BT4's Beta 14 Rev2 image, English or Spanish (told apart by BIN/DBZ4.BIN's SHA-1, as the setup
 * does), is unpacked into bt4/gamedata in BT4decomp's layout:
 *   pzs4usN/NNNNN.bin  every entry of DATA/PZS4USN.AFS, cut by the sector table DBZ4.BIN holds for it
 *   disc/...           the loose files (SLUS_219.78, BIN/DBZ4.BIN, BIN/DBZP.BIN, the ADX and PSS of DATA/)
 * Each file is checked against the size and CRC-32 of plat_verify_tab[_eng].h (assets/bt4dec/verify_LANG.bin, made
 * from those headers), so a damaged image stops the install instead of a game that breaks later. It goes into
 * gamedata.part first and becomes gamedata/ only once all of it is there; .bt4-language tells the engine its
 * language (game_profile.c, base_language). The image is not needed afterwards.
 */
class Bt4Installer(
    private val context: Context,
    private val paths: GamePaths,
    private val resolver: ContentResolver,
) {
    fun currentState(): InstallState {
        if (!paths.installedMarker.exists()) return InstallState.NotInstalled
        val lines = paths.installedMarker.readText().lines()
        // An update from 1.0.x: the old BT4 engine left its own marker here (files=, bytes=, game=bt4: its runtime/data
        // and the image's address), with none of BT4decomp's files. Not installed for BT4decomp: the player imports the
        // image again (the install replaces gamedata/; the saves are elsewhere and stay).
        if (lines.none { it.startsWith("language=") } || !File(paths.gameData, "pzs4us0").isDirectory) {
            return InstallState.NotInstalled
        }
        val files = lines.firstOrNull { it.startsWith("files=") }?.substringAfter('=')?.toIntOrNull() ?: 0
        val bytes = lines.firstOrNull { it.startsWith("bytes=") }?.substringAfter('=')?.toLongOrNull() ?: 0L
        return InstallState.Installed(files, bytes)
    }

    private class Table(val counts: IntArray, val size: IntArray, val crc: IntArray)

    private fun table(language: String): Table {
        val raw = context.assets.open("bt4dec/verify_$language.bin").use { it.readBytes() }
        val b = ByteBuffer.wrap(raw).order(ByteOrder.LITTLE_ENDIAN)
        check(b.int == 0x56345442) { "verify table" } // "BT4V"
        val counts = IntArray(4) { b.int }
        val n = (raw.size - 20) / 8
        val size = IntArray(n)
        val crc = IntArray(n)
        for (i in 0 until n) { size[i] = b.int; crc[i] = b.int }
        return Table(counts, size, crc)
    }

    suspend fun install(uri: Uri, report: (InstallState.Working) -> Unit): InstallState.Installed = withContext(Dispatchers.IO) {
        report(InstallState.Working(context.getString(R.string.step_checking), 0f, context.getString(R.string.detail_finding)))
        val pfd = resolver.openFileDescriptor(uri, "r") ?: throw GameDataInstaller.Stop(context.getString(R.string.err_open_file))
        val part = File(paths.root, "gamedata.part")
        var files = 0
        var bytes = 0L
        var language = ""
        pfd.use {
            val channel = FileInputStream(pfd.fileDescriptor).channel
            ActivityLog.info("iso", "bt4: image of ${runCatching { channel.size() }.getOrDefault(-1L) shr 20} MB")
            val iso = try {
                IsoImage(channel)
            } catch (e: IOException) {
                throw GameDataInstaller.Stop(GameDataInstaller.notIsoMessage(context, channel))
            }
            iso.use {
                val index = iso.find("BIN/DBZ4.BIN")
                if (iso.find(ELF) == null || index == null || index.size > (16L shl 20)) {
                    ActivityLog.warn("iso", "bt4: no $ELF or BIN/DBZ4.BIN; top of the image: " + iso.files.map { it.path.substringBefore('/') }.distinct().take(20))
                    throw GameDataInstaller.Stop(context.getString(R.string.bt4_err_not_bt4, GameDataInstaller.foundInImage(context, iso)))
                }
                val db = iso.read(index.offset, index.size.toInt())
                val sha1 = MessageDigest.getInstance("SHA-1").digest(db).joinToString("") { "%02x".format(it) }
                language = when (sha1) {
                    INDEX_SHA1_EN -> "en"
                    INDEX_SHA1_ES -> "es"
                    else -> {
                        ActivityLog.warn("iso", "bt4: BIN/DBZ4.BIN SHA-1 $sha1: not Beta 14 Rev2 EN/ES")
                        throw GameDataInstaller.Stop(context.getString(R.string.bt4dec_err_version, sha1.take(8)))
                    }
                }
                ActivityLog.info("iso", "bt4: Beta 14 Rev2, language $language (DBZ4.BIN SHA-1 $sha1)")
                val t = table(language)
                // where each file of the table comes from: the AFS entries by DBZ4.BIN's sector tables, then the loose files
                val from = ArrayList<Pair<Long, Long>>(t.size.size)
                val to = ArrayList<String>(t.size.size)
                for (n in 0 until 4) {
                    val afs = iso.find("DATA/PZS4US$n.AFS") ?: throw GameDataInstaller.Stop(context.getString(R.string.bt4dec_err_file, "DATA/PZS4US$n.AFS"))
                    val key = "pzs4us$n.afs\u0000".toByteArray()
                    val at = indexOf(db, key) - 16
                    val h = ByteBuffer.wrap(db, at, 16).order(ByteOrder.LITTLE_ENDIAN)
                    val zero = h.int; val hsize = h.int; val count = h.int; val dup = h.int
                    if (at < 0 || zero != 0 || count != t.counts[n] || dup != count || hsize != 0x11A + count * 2 || at + hsize > db.size) {
                        throw GameDataInstaller.Stop(context.getString(R.string.bt4dec_err_file, "BIN/DBZ4.BIN"))
                    }
                    var pos = 0L
                    for (i in 0 until count) {
                        val sectors = ((db[at + 0x11A + i * 2].toInt() and 0xFF) or ((db[at + 0x11B + i * 2].toInt() and 0xFF) shl 8)).toLong()
                        var len = sectors * SECTOR
                        if (pos + len > afs.size) len = afs.size - pos
                        from += (afs.offset + pos) to len
                        to += "pzs4us$n/%05d.bin".format(i)
                        pos += sectors * SECTOR
                    }
                }
                for (name in LOOSE) {
                    val e = iso.find(name.removePrefix("disc/")) ?: throw GameDataInstaller.Stop(context.getString(R.string.bt4dec_err_file, name))
                    from += e.offset to e.size
                    to += name
                }
                if (from.size != t.size.size) throw GameDataInstaller.Stop(context.getString(R.string.bt4dec_err_file, "BIN/DBZ4.BIN"))
                val total = from.sumOf { it.second }
                val need = (total shr 20) + 256
                val free = paths.root.usableSpace shr 20
                if (free < need) throw GameDataInstaller.Stop(context.getString(R.string.bt4dec_err_space, need.toInt(), free.toInt()))

                part.deleteRecursively()
                part.mkdirs()
                val buf = ByteBuffer.allocate(1 shl 20)
                val crc = CRC32()
                val made = HashSet<String>()
                var lastReport = 0L
                for (k in from.indices) {
                    val (off, len) = from[k]
                    val name = to[k]
                    val out = File(part, name)
                    val dir = name.substringBeforeLast('/', "")
                    if (made.add(dir)) out.parentFile?.mkdirs()
                    crc.reset()
                    java.io.FileOutputStream(out).use { os ->
                        var at = 0L
                        while (at < len) {
                            buf.clear()
                            buf.limit(minOf(buf.capacity().toLong(), len - at).toInt())
                            iso.readInto(off + at, buf)
                            crc.update(buf.array(), 0, buf.limit())
                            os.write(buf.array(), 0, buf.limit())
                            at += buf.limit()
                        }
                    }
                    if (len != (t.size[k].toLong() and 0xFFFFFFFFL) || crc.value != (t.crc[k].toLong() and 0xFFFFFFFFL)) {
                        ActivityLog.warn("iso", "bt4: $name: $len bytes CRC %08X, the table has %d bytes CRC %08X".format(crc.value, t.size[k], t.crc[k]))
                        part.deleteRecursively()
                        throw GameDataInstaller.Stop(context.getString(R.string.bt4dec_err_file, name))
                    }
                    files++
                    bytes += len
                    val now = System.nanoTime()
                    if (now - lastReport > 200_000_000L) {
                        lastReport = now
                        report(InstallState.Working(context.getString(R.string.step_extracting), bytes.toFloat() / total, "$files / ${from.size} · ${bytes shr 20} / ${total shr 20} MB"))
                    }
                }
            }
        }
        File(part, ".bt4-language").writeText(language + "\n")
        File(part, ".installed").writeText("files=$files\nbytes=$bytes\ngame=bt4\nlanguage=$language\n")
        paths.gameData.deleteRecursively()
        if (!part.renameTo(paths.gameData)) throw GameDataInstaller.Stop(context.getString(R.string.bt4dec_err_file, "gamedata"))
        paths.runtimeData.deleteRecursively() // (what the old BT4 engine of 1.0.x copied out of the image: not used any more)
        paths.isoUri.delete()
        ActivityLog.info("iso", "bt4: $files files, ${bytes shr 20} MB in ${paths.gameData} ($language); the image is no longer needed")
        InstallState.Installed(files, bytes)
    }

    fun uninstall() {
        File(paths.root, "gamedata.part").deleteRecursively()
        paths.gameData.deleteRecursively()
        paths.runtimeData.deleteRecursively() // (the old BT4 engine's)
        paths.isoUri.delete()
    }

    private fun indexOf(hay: ByteArray, needle: ByteArray): Int {
        outer@ for (i in 0..hay.size - needle.size) {
            for (j in needle.indices) if (hay[i + j] != needle[j]) continue@outer
            return i
        }
        return -1
    }

    companion object {
        const val ELF = "SLUS_219.78"
        private const val SECTOR = 2048L
        /** BIN/DBZ4.BIN of Beta 14 Rev2 (port/setup/native.cpp, step_data). */
        const val INDEX_SHA1_EN = "145837274421c39ae7eedc3fa1b697d4212541c2"
        const val INDEX_SHA1_ES = "989ee93cbd31508717bc245793c1e89c50109e0f"
        /** plat_verify_tab.h, kVerifyLoose: after the AFS entries, in this order. */
        val LOOSE = listOf(
            "disc/BIN/DBZ4.BIN", "disc/BIN/DBZP.BIN", "disc/DATA/PZS4US.DIR", "disc/DATA/ZS4USED.ADX", "disc/DATA/ZS4USED.PSS",
            "disc/DATA/ZS4USOP.ADX", "disc/DATA/ZS4USOP.PSS", "disc/SLUS_219.78",
        )
    }
}
