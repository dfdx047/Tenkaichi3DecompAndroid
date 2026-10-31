package com.dfdx047.dragonrage.data

import android.content.ContentResolver
import android.content.Context
import android.content.Intent
import android.net.Uri
import android.os.ParcelFileDescriptor
import com.dfdx047.dragonrage.R
import java.io.File
import java.io.FileInputStream
import java.io.IOException
import java.security.MessageDigest
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/**
 * Budokai Tenkaichi 4 (Team BT4's mod, Beta 14 Rev2): what the Windows launcher of dbz-bt4-recomp does
 * (windows/launcher/DBZ_BT4.cs, Prepare), for Android. The game's code was recompiled from exactly one ELF, so the
 * image must hold that one (its SHA-256); the ELF, SYSTEM.CNF and the small IRX/ and BIN/ files are copied to
 * runtime/data, and the big AFS archives stay in the image, which the engine reads where it is (a file descriptor
 * of the document the player picked: nothing of 4 GB is copied).
 */
class Bt4Installer(
    private val context: Context,
    private val paths: GamePaths,
    private val resolver: ContentResolver,
) {
    fun currentState(): InstallState {
        if (!paths.installedMarker.exists() || imageUri() == null) return InstallState.NotInstalled
        val lines = paths.installedMarker.readText().lines()
        val files = lines.firstOrNull { it.startsWith("files=") }?.substringAfter('=')?.toIntOrNull() ?: 0
        val bytes = lines.firstOrNull { it.startsWith("bytes=") }?.substringAfter('=')?.toLongOrNull() ?: 0L
        return InstallState.Installed(files, bytes)
    }

    /** The image's address, if one was set up (and its permission is still held). */
    fun imageUri(): Uri? = runCatching { Uri.parse(paths.isoUri.readText().trim()) }.getOrNull()
        ?.takeIf { u -> resolver.persistedUriPermissions.any { it.uri == u && it.isReadPermission } }

    /** The image, open for the engine (it reads it as /proc/self/fd/<n>); null when it is gone. */
    fun openImage(): ParcelFileDescriptor? = imageUri()?.let { runCatching { resolver.openFileDescriptor(it, "r") }.getOrNull() }

    suspend fun install(uri: Uri, report: (InstallState.Working) -> Unit): InstallState.Installed = withContext(Dispatchers.IO) {
        report(InstallState.Working(context.getString(R.string.step_checking), 0f, context.getString(R.string.detail_finding)))
        // the right to open the image again is asked first: without it the game could not read the disc later, and a
        // refusal after minutes of copying looked like the import had broken
        val persisted = runCatching { resolver.takePersistableUriPermission(uri, Intent.FLAG_GRANT_READ_URI_PERMISSION) }
        persisted.exceptionOrNull()?.let { ActivityLog.warn("iso", "bt4: no lasting permission for the image: $it") }
        if (persisted.isFailure && resolver.persistedUriPermissions.none { it.uri == uri && it.isReadPermission }) {
            throw GameDataInstaller.Stop(context.getString(R.string.bt4_err_permission))
        }
        val pfd = resolver.openFileDescriptor(uri, "r") ?: throw GameDataInstaller.Stop(context.getString(R.string.err_open_file))
        var files = 0
        var bytes = 0L
        pfd.use {
            val channel = FileInputStream(pfd.fileDescriptor).channel
            ActivityLog.info("iso", "bt4: image of ${runCatching { channel.size() }.getOrDefault(-1L) shr 20} MB")
            val iso = try {
                IsoImage(channel)
            } catch (e: IOException) {
                throw GameDataInstaller.Stop(GameDataInstaller.notIsoMessage(context, channel))
            }
            iso.use {
                val elf = iso.find(ELF)
                if (elf == null) {
                    ActivityLog.warn("iso", "bt4: no $ELF; top of the image: " + iso.files.map { it.path.substringBefore('/') }.distinct().take(20))
                    throw GameDataInstaller.Stop(context.getString(R.string.bt4_err_not_bt4))
                }
                val data = iso.read(elf.offset, elf.size.toInt())
                val hash = sha256(data)
                ActivityLog.info("iso", "bt4: $ELF ${data.size} bytes, SHA-256 $hash")
                if (hash != ELF_SHA256) throw GameDataInstaller.Stop(context.getString(R.string.bt4_err_version, data.size, hash.take(8)))
                report(InstallState.Working(context.getString(R.string.step_extracting), 0.1f, ELF))
                paths.runtimeData.deleteRecursively()
                paths.runtimeData.mkdirs()
                // (the runner is started with SLUS_216.78, the name BT3-Recomp's tables know; both names, as on Windows)
                File(paths.runtimeData, "SLUS_216.78").writeBytes(data)
                File(paths.runtimeData, ELF).writeBytes(data)
                files += 2
                bytes += 2L * data.size
                val keep = iso.files.filter { e ->
                    val top = e.path.substringBefore('/')
                    e.path.equals("SYSTEM.CNF", ignoreCase = true) ||
                        ((top.equals("IRX", true) || top.equals("BIN", true)) && e.path.contains('/') && e.size < (64L shl 20))
                }
                val total = keep.sumOf { it.size }.coerceAtLeast(1L)
                var done = 0L
                val buf = java.nio.ByteBuffer.allocate(1 shl 20)
                for (e in keep) {
                    report(InstallState.Working(context.getString(R.string.step_extracting), 0.1f + 0.9f * done / total, e.path))
                    val out = File(paths.runtimeData, e.path)
                    out.parentFile?.mkdirs()
                    // in pieces of 1 MB: a whole 60 MB file in memory at once failed on phones with a small app heap
                    java.io.FileOutputStream(out).use { os ->
                        var at = 0L
                        while (at < e.size) {
                            buf.clear()
                            buf.limit(minOf(buf.capacity().toLong(), e.size - at).toInt())
                            iso.readInto(e.offset + at, buf)
                            os.write(buf.array(), 0, buf.limit())
                            at += buf.limit()
                        }
                    }
                    files++
                    bytes += e.size
                    done += e.size
                }
                if (!File(paths.runtimeData, "BIN/DBZP.BIN").exists() && !File(paths.runtimeData, "bin/DBZP.BIN").exists()) {
                    ActivityLog.warn("iso", "bt4: BIN/DBZP.BIN was not copied (" + keep.map { it.path } + ")")
                }
            }
        }
        paths.isoUri.parentFile?.mkdirs()
        paths.isoUri.writeText(uri.toString() + "\n")
        paths.installedMarker.parentFile?.mkdirs()
        paths.installedMarker.writeText("files=$files\nbytes=$bytes\ngame=bt4\n")
        ActivityLog.info("iso", "bt4: $files files copied, the image stays at $uri")
        InstallState.Installed(files, bytes)
    }

    fun uninstall() {
        imageUri()?.let { u -> runCatching { resolver.releasePersistableUriPermission(u, Intent.FLAG_GRANT_READ_URI_PERMISSION) } }
        paths.runtimeData.deleteRecursively()
        paths.isoUri.delete()
        paths.installedMarker.delete()
    }

    companion object {
        const val ELF = "SLUS_219.78"
        /** SHA-256 of SLUS_219.78 of the Beta 14 Rev2 (dbz-bt4-recomp, windows/launcher/DBZ_BT4.cs). */
        const val ELF_SHA256 = "224525C5639C00E01C2604C4685D307BC739C041000DCF8C69A4F884F465E8A4"

        fun sha256(data: ByteArray): String = MessageDigest.getInstance("SHA-256").digest(data).joinToString("") { "%02X".format(it) }
    }
}
