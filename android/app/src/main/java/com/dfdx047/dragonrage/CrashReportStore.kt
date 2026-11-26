package com.dfdx047.dragonrage

import android.content.ClipData
import android.content.ClipboardManager
import android.content.Context
import android.os.Build
import android.os.Process
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.getValue
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.unit.dp
import java.io.File
import java.io.PrintWriter
import java.io.StringWriter
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

/**
 * Persists the last uncaught Java/Kotlin exception in app-private storage.
 * The report is read on the next launch, so it remains available after a process crash.
 */
object CrashReportStore {
    private const val REPORT_FILE = "last_crash_report.txt"

    fun install(context: Context) {
        val current = Thread.getDefaultUncaughtExceptionHandler()
        if (current is CrashHandler) return
        Thread.setDefaultUncaughtExceptionHandler(
            CrashHandler(context.applicationContext, current),
        )
    }

    fun readLatest(context: Context): String? =
        runCatching {
            File(context.filesDir, REPORT_FILE).takeIf { it.isFile }?.readText()
        }.getOrNull()?.takeIf { it.isNotBlank() }

    private class CrashHandler(
        context: Context,
        private val delegate: Thread.UncaughtExceptionHandler?,
    ) : Thread.UncaughtExceptionHandler {
        private val appContext = context.applicationContext

        override fun uncaughtException(thread: Thread, throwable: Throwable) {
            runCatching {
                val stack = StringWriter().also { writer ->
                    PrintWriter(writer).use { throwable.printStackTrace(it) }
                }.toString()
                val timestamp = SimpleDateFormat(
                    "yyyy-MM-dd HH:mm:ss.SSS Z",
                    Locale.US,
                ).format(Date())
                val version = runCatching {
                    appContext.packageManager.getPackageInfo(appContext.packageName, 0).versionName
                }.getOrNull() ?: "unknown"
                val report = buildString {
                    appendLine("DRAGON RAGE - CRASH REPORT")
                    appendLine("Date: $timestamp")
                    appendLine("App version: $version")
                    appendLine("Device: ${Build.MANUFACTURER} ${Build.MODEL}")
                    appendLine("Android: ${Build.VERSION.RELEASE} (SDK ${Build.VERSION.SDK_INT})")
                    appendLine("Process: ${appContext.packageName}")
                    appendLine("Thread: ${thread.name}")
                    appendLine()
                    appendLine("EXCEPTION")
                    appendLine(stack)
                }

                val destination = File(appContext.filesDir, REPORT_FILE)
                val temporary = File(appContext.filesDir, "$REPORT_FILE.tmp")
                temporary.writeText(report)
                if (!temporary.renameTo(destination)) {
                    destination.writeText(report)
                    temporary.delete()
                }
            }

            // Preserve Android's normal crash handling and process termination.
            if (delegate != null) {
                delegate.uncaughtException(thread, throwable)
            } else {
                Process.killProcess(Process.myPid())
                kotlin.system.exitProcess(10)
            }
        }
    }
}

@Composable
fun CrashReportDialog(report: String?) {
    if (report == null) return
    var visible by remember(report) { mutableStateOf(true) }
    if (!visible) return

    val context = LocalContext.current
    AlertDialog(
        onDismissRequest = { visible = false },
        title = { Text("Se detectó un cierre inesperado") },
        text = {
            Column(Modifier.heightIn(max = 360.dp).verticalScroll(rememberScrollState())) {
                Text(
                    "Dragon Rage guardó el informe del último cierre. Puedes copiarlo y enviarlo para investigar el problema. El informe permanece guardado en el almacenamiento privado de la aplicación.",
                )
                Text(report)
            }
        },
        confirmButton = {
            TextButton(
                onClick = {
                    val clipboard = context.getSystemService(Context.CLIPBOARD_SERVICE) as ClipboardManager
                    clipboard.setPrimaryClip(ClipData.newPlainText("Dragon Rage crash report", report))
                    visible = false
                },
            ) {
                Text("Copiar informe")
            }
        },
        dismissButton = {
            TextButton(onClick = { visible = false }) {
                Text("Cerrar")
            }
        },
    )
}
