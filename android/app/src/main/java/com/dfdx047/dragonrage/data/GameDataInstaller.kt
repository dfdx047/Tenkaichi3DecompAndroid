package com.dfdx047.dragonrage.data

import android.content.ContentResolver
import android.content.Context
import com.dfdx047.dragonrage.R
import android.net.Uri
import android.os.StatFs
import java.io.File
import java.io.FileInputStream
import java.io.IOException
import java.nio.ByteBuffer
import java.nio.ByteOrder
import java.security.MessageDigest
import kotlin.coroutines.coroutineContext
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.withContext

sealed interface InstallState {
    data object NotInstalled : InstallState
    data class Working(val step: String, val progress: Float, val detail: String) : InstallState
    data class Installed(val files: Int, val bytes: Long, val modified: Boolean = false) : InstallState
    data class Failed(val message: String) : InstallState
}

/**
 * The PC port's setup (port/setup/native.cpp, steps 1 and 2) for Android: checks that the image is the
 * unmodified USA release and unpacks the game's data in the layout of port/tools/extract_disc.py:
 *
 *   gamedata/disc/SLUS_216.78, gamedata/disc/BIN/..., gamedata/disc/DATA/... (the AFS archives whole)
 *
 * (The PC setup also splits each archive into one file per entry, gamedata/<name>/00000.bin ...; the engine reads
 * either. Loose files and mods/ still win over the archive.)
 *
 * The image is read where it is (through the document the user picked); nothing of it is kept except the
 * unpacked files.
 */
class GameDataInstaller(
    private val context: Context,
    private val paths: GamePaths,
    private val resolver: ContentResolver,
    private val checkSums: Boolean = true, // (off only in tests with a synthetic image)
) {

    class Stop(message: String) : Exception(message)

    fun currentState(): InstallState {
        if (!paths.installedMarker.exists()) return InstallState.NotInstalled
        val lines = paths.installedMarker.readText().lines()
        val files = lines.firstOrNull { it.startsWith("files=") }?.substringAfter('=')?.toIntOrNull() ?: 0
        val bytes = lines.firstOrNull { it.startsWith("bytes=") }?.substringAfter('=')?.toLongOrNull() ?: 0L
        val modified = lines.any { it == "modified=1" }
        return InstallState.Installed(files, bytes, modified)
    }

    suspend fun install(uri: Uri, report: (InstallState.Working) -> Unit): InstallState.Installed = withContext(Dispatchers.IO) {
        val pfd = resolver.openFileDescriptor(uri, "r") ?: throw Stop(context.getString(R.string.err_open_file))
        pfd.use {
            val channel = FileInputStream(pfd.fileDescriptor).channel
            val iso = try {
                IsoImage(channel)
            } catch (e: IOException) {
                throw Stop(context.getString(R.string.err_not_iso))
            }
            iso.use {
                report(InstallState.Working(context.getString(R.string.step_checking), 0f, context.getString(R.string.detail_finding)))
                val original = verify(iso, report)
                extract(iso, report, modified = !original)
            }
        }
    }

    /** True for the unmodified USA release. A modified image (a translation, a mod such as "Tenkaichi 4") is
     *  installed too: its files are used as they are, but the engine runs the original program, so changes a mod made
     *  to the game's code (not to its data) do not apply. */
    private suspend fun verify(iso: IsoImage, report: (InstallState.Working) -> Unit): Boolean {
        val slus = iso.find("SLUS_216.78")
        val dbzp = iso.find("BIN/DBZP.BIN")
        if (slus == null || dbzp == null) {
            throw Stop(context.getString(R.string.err_not_usa))
        }
        report(InstallState.Working(context.getString(R.string.step_checking), 0.5f, context.getString(R.string.detail_checksums)))
        coroutineContext.ensureActive()
        val elf = iso.read(slus.offset, slus.size.toInt())
        val rom = ByteArray((ROM_END - ROM_BASE).toInt())
        if (elf.size > 0x34 && elf[0] == 0x7F.toByte() && elf[1] == 'E'.code.toByte()) {
            val b = ByteBuffer.wrap(elf).order(ByteOrder.LITTLE_ENDIAN)
            val shoff = b.getInt(0x20).toLong() and 0xFFFFFFFFL
            val shentsize = b.getShort(0x2E).toInt() and 0xFFFF
            val shnum = b.getShort(0x30).toInt() and 0xFFFF
            for (i in 0 until shnum) {
                val at = shoff + i.toLong() * shentsize
                if (at + 24 > elf.size) break
                val p = at.toInt()
                val type = b.getInt(p + 4).toLong() and 0xFFFFFFFFL
                val flags = b.getInt(p + 8).toLong() and 0xFFFFFFFFL
                val addr = b.getInt(p + 12).toLong() and 0xFFFFFFFFL
                val off = b.getInt(p + 16).toLong() and 0xFFFFFFFFL
                val size = b.getInt(p + 20).toLong() and 0xFFFFFFFFL
                // allocated, not NOBITS, inside the flat image, inside the file
                if (flags and 2L != 0L && type != 8L && size != 0L && addr >= ROM_BASE && addr + size <= ROM_END && off + size <= elf.size) {
                    System.arraycopy(elf, off.toInt(), rom, (addr - ROM_BASE).toInt(), size.toInt())
                }
            }
        }
        val menu = iso.read(dbzp.offset, dbzp.size.toInt())
        return !checkSums || (sha1(rom) == ROM_SHA1 && sha1(menu) == DBZP_SHA1)
    }

    private suspend fun extract(iso: IsoImage, report: (InstallState.Working) -> Unit, modified: Boolean): InstallState.Installed {
        val take = iso.files.filter {
            val u = it.path.uppercase()
            u == "SLUS_216.78" || u.startsWith("BIN/") || u.startsWith("DATA/")
        }
        val total = take.sumOf { it.size }
        val free = StatFs(paths.root.path).availableBytes
        if (free < total + 256L * 1024 * 1024) {
            throw Stop(context.getString(R.string.err_space, formatBytes(total), formatBytes(free)))
        }
        // A fresh start: whatever an earlier, interrupted run left is replaced (mods stay).
        paths.installedMarker.delete()
        paths.gameData.listFiles()?.filter { it.name != "mods" }?.forEach { it.deleteRecursively() }
        paths.gameData.mkdirs()

        val buf = ByteBuffer.allocateDirect(8 shl 20)
        var done = 0L
        var files = 0
        var lastPercent = -1
        fun progress(n: Long) {
            done += n
            val percent = if (total > 0) (done * 100 / total).toInt() else 0
            if (percent != lastPercent) {
                lastPercent = percent
                report(InstallState.Working(context.getString(R.string.step_extracting), done.toFloat() / total, context.getString(R.string.detail_extract, percent, files)))
            }
        }

        // Every file is copied as it is, the AFS archives too: the engine reads their entries through the
        // archive's table (port/src/plat_fcache.c). Unpacking them into ~20 000 small files, as the PC setup does,
        // is what made the install take so long on Android's shared storage.
        for (f in take) {
            coroutineContext.ensureActive()
            val u = f.path.uppercase()
            val out = File(paths.gameData, "disc/$u") // upper case, as the game asks for them
            out.parentFile?.mkdirs()
            copyOut(iso, f.offset, f.size, out, buf) { progress(it) }
            files++
        }
        val bytes = paths.gameData.sizeDeep()
        paths.installedMarker.writeText("files=$files\nbytes=$bytes\nlayout=afs\n" + (if (modified) "modified=1\n" else ""))
        return InstallState.Installed(files, bytes, modified)
    }

    private suspend fun copyOut(iso: IsoImage, offset: Long, size: Long, out: File, buf: ByteBuffer, onBytes: (Long) -> Unit = {}) {
        out.outputStream().channel.use { oc ->
            var pos = offset
            var left = size
            while (left > 0) {
                coroutineContext.ensureActive()
                buf.clear()
                buf.limit(minOf(left, buf.capacity().toLong()).toInt())
                iso.readInto(pos, buf)
                buf.flip()
                val n = buf.limit().toLong()
                while (buf.hasRemaining()) oc.write(buf)
                pos += n
                left -= n
                onBytes(n)
            }
        }
    }

    fun uninstall() {
        paths.installedMarker.delete()
        paths.gameData.listFiles()?.filter { it.name != "mods" }?.forEach { it.deleteRecursively() }
    }

    private fun sha1(data: ByteArray): String =
        MessageDigest.getInstance("SHA-1").digest(data).joinToString("") { "%02x".format(it) }

    companion object {
        // config/SLUS_216.78.yaml and config/DBZP.yaml of the decompilation
        const val ROM_SHA1 = "caee6c2269bba89fc51eea9e7beac3adbec9dc52"
        const val DBZP_SHA1 = "4f910969e05d9b25c83af7642b60da9949a4348b"
        const val ROM_BASE = 0x100000L
        const val ROM_END = 0x2FF180L
    }
}
