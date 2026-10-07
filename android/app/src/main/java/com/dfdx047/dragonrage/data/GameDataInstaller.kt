package com.dfdx047.dragonrage.data

import android.content.ContentResolver
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
    data class Installed(val files: Int, val bytes: Long) : InstallState
    data class Failed(val message: String) : InstallState
}

/**
 * The PC port's setup (port/setup/native.cpp, steps 1 and 2) for Android: checks that the image is the
 * unmodified USA release and unpacks the game's data in the layout of port/tools/extract_disc.py:
 *
 *   gamedata/disc/SLUS_216.78, gamedata/disc/BIN/..., gamedata/disc/DATA/<non-AFS files>
 *   gamedata/<afs name in lower case>/00000.bin ...   one file per entry of each DATA/<name>.AFS archive
 *
 * The image is read where it is (through the document the user picked); nothing of it is kept except the
 * unpacked files.
 */
class GameDataInstaller(
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
        return InstallState.Installed(files, bytes)
    }

    suspend fun install(uri: Uri, report: (InstallState.Working) -> Unit): InstallState.Installed = withContext(Dispatchers.IO) {
        val pfd = resolver.openFileDescriptor(uri, "r") ?: throw Stop("Não foi possível abrir o arquivo escolhido.")
        pfd.use {
            val channel = FileInputStream(pfd.fileDescriptor).channel
            val iso = try {
                IsoImage(channel)
            } catch (e: IOException) {
                throw Stop("O arquivo não é uma imagem de disco ISO válida. Use a ISO do jogo (não CSO, CHD ou ZIP).")
            }
            iso.use {
                report(InstallState.Working("Verificando o disco", 0f, "procurando o jogo na imagem"))
                verify(iso, report)
                extract(iso, report)
            }
        }
    }

    private suspend fun verify(iso: IsoImage, report: (InstallState.Working) -> Unit) {
        val slus = iso.find("SLUS_216.78")
        val dbzp = iso.find("BIN/DBZP.BIN")
        if (slus == null || dbzp == null) {
            throw Stop(
                "Esta não é a imagem da versão americana (USA) de Budokai Tenkaichi 3: não há SLUS_216.78 nela.\n" +
                    "Outras regiões e a versão de Wii têm outros programas e não são suportadas.",
            )
        }
        report(InstallState.Working("Verificando o disco", 0.5f, "conferindo as somas de verificação"))
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
        if (checkSums && (sha1(rom) != ROM_SHA1 || sha1(menu) != DBZP_SHA1)) {
            throw Stop(
                "Os programas do jogo nesta imagem não têm as somas esperadas.\n" +
                    "É preciso a versão USA (SLUS-21678) sem modificações; uma imagem com patch ou danificada não funciona.",
            )
        }
    }

    private suspend fun extract(iso: IsoImage, report: (InstallState.Working) -> Unit): InstallState.Installed {
        val take = iso.files.filter {
            val u = it.path.uppercase()
            u == "SLUS_216.78" || u.startsWith("BIN/") || u.startsWith("DATA/")
        }
        val total = take.sumOf { it.size }
        val free = StatFs(paths.root.path).availableBytes
        if (free < total + 256L * 1024 * 1024) {
            throw Stop("Espaço insuficiente: são precisos ${formatBytes(total)} e há ${formatBytes(free)} livres.")
        }
        // A fresh start: whatever an earlier, interrupted run left is replaced (mods stay).
        paths.installedMarker.delete()
        paths.gameData.listFiles()?.filter { it.name != "mods" }?.forEach { it.deleteRecursively() }
        paths.gameData.mkdirs()

        val buf = ByteBuffer.allocateDirect(1 shl 20)
        var done = 0L
        var files = 0
        var lastPercent = -1
        fun progress(n: Long) {
            done += n
            val percent = if (total > 0) (done * 100 / total).toInt() else 0
            if (percent != lastPercent) {
                lastPercent = percent
                report(InstallState.Working("Extraindo os dados do jogo", done.toFloat() / total, "$percent% · $files arquivos"))
            }
        }

        for (f in take) {
            coroutineContext.ensureActive()
            val u = f.path.uppercase()
            val isAfs = u.startsWith("DATA/") && u.endsWith(".AFS")
            if (!isAfs) {
                val out = File(paths.gameData, "disc/$u") // upper case, as the game asks for them
                out.parentFile?.mkdirs()
                copyOut(iso, f.offset, f.size, out, buf)
                files++
                progress(f.size)
                continue
            }
            val dir = File(paths.gameData, u.substring(5, u.length - 4).lowercase())
            dir.mkdirs()
            val head = ByteBuffer.wrap(iso.read(f.offset, 8)).order(ByteOrder.LITTLE_ENDIAN)
            val count = head.getInt(4).toLong() and 0xFFFFFFFFL
            if (head.getInt(0) != 0x00534641 || count * 8 + 8 > f.size) throw Stop("Um arquivo do disco (${f.path}) está danificado.")
            val table = ByteBuffer.wrap(iso.read(f.offset + 8, (count * 8).toInt())).order(ByteOrder.LITTLE_ENDIAN)
            var inArchive = 0L
            for (i in 0 until count.toInt()) {
                coroutineContext.ensureActive()
                val off = table.getInt(i * 8).toLong() and 0xFFFFFFFFL
                val size = table.getInt(i * 8 + 4).toLong() and 0xFFFFFFFFL
                if (off + size > f.size) throw Stop("Um arquivo do disco (${f.path}) está danificado.")
                copyOut(iso, f.offset + off, size, File(dir, "%05d.bin".format(i)), buf)
                files++
                inArchive += size
                progress(size)
            }
            progress((f.size - inArchive).coerceAtLeast(0)) // the archive's table and padding
        }
        val bytes = paths.gameData.sizeDeep()
        paths.installedMarker.writeText("files=$files\nbytes=$bytes\n")
        return InstallState.Installed(files, bytes)
    }

    private fun copyOut(iso: IsoImage, offset: Long, size: Long, out: File, buf: ByteBuffer) {
        out.outputStream().channel.use { oc ->
            var pos = offset
            var left = size
            while (left > 0) {
                buf.clear()
                buf.limit(minOf(left, buf.capacity().toLong()).toInt())
                iso.readInto(pos, buf)
                buf.flip()
                while (buf.hasRemaining()) oc.write(buf)
                pos += buf.limit()
                left -= buf.limit()
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
