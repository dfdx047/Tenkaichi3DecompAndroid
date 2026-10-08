package com.dfdx047.dragonrage.data

import android.content.ContentValues
import android.content.Context
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * One text file for a bug report, saved in Downloads: the device, the game's own log of its last run (bt3_log.txt,
 * with a crash report when it crashed) and the app's log from the system (logcat: the app may read its own lines,
 * the launcher's and the game's, which run as the same app; the crash buffer has the system's report of a crash).
 */
class LogExport(private val context: Context, private val paths: GamePaths) {

    fun build(): String = buildString {
        val pm = context.packageManager
        val version = runCatching { pm.getPackageInfo(context.packageName, 0).versionName }.getOrNull() ?: "?"
        appendLine("Dragon Rage log  ${SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.ROOT).format(Date())}")
        appendLine("app ${context.packageName} $version")
        appendLine("device ${Build.MANUFACTURER} ${Build.MODEL} (${Build.DEVICE}), Android ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) appendLine("SoC ${Build.SOC_MANUFACTURER} ${Build.SOC_MODEL}")
        appendLine("hardware ${Build.HARDWARE}, board ${Build.BOARD}, ABIs ${Build.SUPPORTED_ABIS.joinToString()}")
        val mem = android.app.ActivityManager.MemoryInfo()
        (context.getSystemService(Context.ACTIVITY_SERVICE) as android.app.ActivityManager).getMemoryInfo(mem)
        appendLine("memory ${mem.totalMem / (1024 * 1024)} MB")
        appendLine("game data: " + (runCatching { paths.installedMarker.readText().replace('\n', ' ') }.getOrNull() ?: "not installed"))
        appendLine()
        appendLine("===== bt3_log.txt (the game's last run) =====")
        val log = File(paths.root, "bt3_log.txt")
        if (log.exists()) {
            val lines = log.readLines()
            if (lines.size > 3000) appendLine("(${lines.size - 3000} earlier lines left out)")
            lines.takeLast(3000).forEach { appendLine(it) }
        } else {
            appendLine("(none: the game has not been started yet)")
        }
        appendLine()
        appendLine("===== logcat (this app) =====")
        append(logcat("main,system,crash", 4000))
    }

    private fun logcat(buffers: String, lines: Int): String = runCatching {
        val p = ProcessBuilder("logcat", "-d", "-v", "threadtime", "-b", buffers, "-t", lines.toString()).redirectErrorStream(true).start()
        val out = p.inputStream.bufferedReader().readText()
        p.waitFor()
        out
    }.getOrElse { "(logcat could not be read: ${it.message})\n" }

    /** Writes the report to Downloads; returns its address (for sharing) and its name. */
    fun saveToDownloads(): Pair<Uri, String> {
        val name = "DragonRage-log-" + SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT).format(Date()) + ".txt"
        val values = ContentValues().apply {
            put(MediaStore.Downloads.DISPLAY_NAME, name)
            put(MediaStore.Downloads.MIME_TYPE, "text/plain")
            put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS)
        }
        val uri = context.contentResolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
            ?: throw IllegalStateException("Downloads is not available")
        context.contentResolver.openOutputStream(uri)?.use { it.write(build().toByteArray()) }
        return uri to name
    }
}
