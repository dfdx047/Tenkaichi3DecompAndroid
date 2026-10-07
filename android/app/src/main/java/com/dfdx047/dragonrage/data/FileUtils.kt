package com.dfdx047.dragonrage.data

import android.content.ContentResolver
import android.net.Uri
import android.provider.OpenableColumns
import java.io.File
import java.io.IOException
import java.io.InputStream
import java.util.zip.ZipInputStream
import kotlin.coroutines.coroutineContext
import kotlinx.coroutines.ensureActive

fun ContentResolver.displayName(uri: Uri): String {
    query(uri, arrayOf(OpenableColumns.DISPLAY_NAME), null, null, null)?.use { c ->
        if (c.moveToFirst()) {
            val i = c.getColumnIndex(OpenableColumns.DISPLAY_NAME)
            if (i >= 0) c.getString(i)?.let { return it }
        }
    }
    return uri.lastPathSegment?.substringAfterLast('/') ?: "arquivo"
}

fun ContentResolver.size(uri: Uri): Long {
    query(uri, arrayOf(OpenableColumns.SIZE), null, null, null)?.use { c ->
        if (c.moveToFirst()) {
            val i = c.getColumnIndex(OpenableColumns.SIZE)
            if (i >= 0 && !c.isNull(i)) return c.getLong(i)
        }
    }
    return -1L
}

/** A name that is safe as a single folder or file name. */
fun safeName(name: String): String =
    name.replace(Regex("[\\\\/:*?\"<>|\\u0000-\\u001F]"), "_").trim().trimStart('.').ifBlank { "item" }.take(80)

fun File.sizeDeep(): Long = if (isFile) length() else walkTopDown().filter { it.isFile }.sumOf { it.length() }

fun File.countFiles(predicate: (File) -> Boolean = { true }): Int =
    walkTopDown().count { it.isFile && predicate(it) }

/** A folder name that does not exist yet in [parent]: "Name", "Name (2)", ... */
fun uniqueChild(parent: File, wanted: String): File {
    var f = File(parent, wanted)
    var n = 2
    while (f.exists()) f = File(parent, "$wanted (${n++})")
    return f
}

/**
 * Unpacks a zip into [dest]. Entries that would land outside [dest] ("zip slip") are refused. [onEntry] sees
 * every file written, relative to [dest]; [onBytes] the number of compressed-stream bytes read so far is not
 * available from ZipInputStream, so progress is reported in uncompressed bytes written.
 */
suspend fun unzip(input: InputStream, dest: File, onBytes: (Long) -> Unit = {}, onEntry: (String) -> Unit = {}): Int {
    val canonicalDest = dest.canonicalFile
    var files = 0
    var written = 0L
    val buf = ByteArray(1 shl 16)
    ZipInputStream(input.buffered(1 shl 16)).use { zip ->
        while (true) {
            coroutineContext.ensureActive()
            val e = zip.nextEntry ?: break
            val name = e.name.replace('\\', '/')
            if (name.startsWith("__MACOSX/") || name.endsWith(".DS_Store")) continue
            val out = File(dest, name).canonicalFile
            if (!out.path.startsWith(canonicalDest.path + File.separator)) throw IOException("Entrada inválida no zip: $name")
            if (e.isDirectory) {
                out.mkdirs()
                continue
            }
            out.parentFile?.mkdirs()
            out.outputStream().use { os ->
                while (true) {
                    val n = zip.read(buf)
                    if (n < 0) break
                    os.write(buf, 0, n)
                    written += n
                    onBytes(written)
                }
            }
            files++
            onEntry(out.relativeTo(canonicalDest).path)
        }
    }
    return files
}

fun formatBytes(bytes: Long): String {
    if (bytes < 1024) return "$bytes B"
    val units = listOf("KB", "MB", "GB", "TB")
    var v = bytes / 1024.0
    var i = 0
    while (v >= 1024 && i < units.lastIndex) {
        v /= 1024
        i++
    }
    return if (v >= 100) "%.0f %s".format(v, units[i]) else "%.1f %s".format(v, units[i])
}
