package com.dfdx047.dragonrage.engine

import android.app.Activity
import android.graphics.Color
import android.graphics.Typeface
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.system.Os
import android.system.OsConstants
import android.view.ViewGroup
import android.widget.TextView
import com.dfdx047.dragonrage.R
import java.io.File

/**
 * Budokai Tenkaichi 2's performance panel over the game (the setting "performance panel", Bt2Settings.showFrameRate,
 * in the BT2 tab and in the in-game menu): pictures per second, the game's speed, the game thread's CPU, the whole
 * app's CPU over all cores and the GPU's load. Twice a second, from the engine's running counts
 * (Bt2Bridge.nativeStats: pictures shown, fields run, the game thread's id) and from /proc and /sys.
 *
 * The GPU's load is what the kernel driver lets an app read: Adreno's kgsl "gpubusy" (busy and total time of its last
 * window), or one of Mali's load files; "GPU —" when none can be read (no number is made up).
 */
class Bt2PerfOverlay(private val activity: Activity) {
    private val handler = Handler(Looper.getMainLooper())
    private val density = activity.resources.displayMetrics.density
    private val view = TextView(activity).apply {
        typeface = Typeface.MONOSPACE
        textSize = 12f
        setTextColor(Color.WHITE)
        setShadowLayer(2f, 1f, 1f, Color.BLACK)
        setBackgroundColor(Color.argb(140, 0, 0, 0))
        val p = (6 * density).toInt()
        setPadding(p, p / 2, p, p / 2)
        isClickable = false
        isFocusable = false
    }
    private val ticks = Os.sysconf(OsConstants._SC_CLK_TCK).takeIf { it > 0 } ?: 100L
    private val cores = Runtime.getRuntime().availableProcessors().coerceAtLeast(1)
    private val gpuFile: File? = GPU_FILES.map(::File).firstOrNull { f -> runCatching { f.readText(); true }.getOrDefault(false) }

    private class Sample(val at: Long, val pictures: Long, val fields: Long, val gameCpu: Long, val appCpu: Long)
    private var last: Sample? = null

    val isShown get() = view.parent != null

    private val tick = object : Runnable {
        override fun run() {
            update()
            handler.postDelayed(this, 500)
        }
    }

    fun show(into: ViewGroup) {
        if (isShown) return
        val m = (8 * density).toInt()
        into.addView(view, ViewGroup.MarginLayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply {
            leftMargin = m
            topMargin = m
        })
        view.text = "FPS …"
        last = null
        handler.post(tick)
    }

    fun hide() {
        handler.removeCallbacks(tick)
        (view.parent as? ViewGroup)?.removeView(view)
    }

    private fun update() {
        val stats = runCatching { Bt2Bridge.nativeStats() }.getOrNull() ?: return
        val tid = stats[2]
        val now = Sample(SystemClock.elapsedRealtime(), stats[0], stats[1],
            if (tid > 0) cpuTicks("/proc/self/task/$tid/stat") else 0L, cpuTicks("/proc/self/stat"))
        val before = last
        last = now
        if (before == null || now.at <= before.at) return
        val seconds = (now.at - before.at) / 1000.0
        val fps = (now.pictures - before.pictures) / seconds
        val speed = (now.fields - before.fields) / seconds / FIELD_RATE * 100.0
        val game = (now.gameCpu - before.gameCpu).toDouble() / ticks / seconds * 100.0
        val app = (now.appCpu - before.appCpu).toDouble() / ticks / seconds * 100.0 / cores
        val gpu = gpuLoad()
        view.text = activity.getString(R.string.bt2_perf_line1, fps, speed.toInt()) + "\n" +
            activity.getString(R.string.bt2_perf_line2, game.toInt().coerceIn(0, 100), app.toInt().coerceIn(0, 100), cores) + "\n" +
            (if (gpu != null) activity.getString(R.string.bt2_perf_gpu, gpu) else activity.getString(R.string.bt2_perf_gpu_none))
        view.setTextColor(when {
            speed >= 97.0 -> Color.WHITE
            speed >= 85.0 -> Color.rgb(255, 214, 90)
            else -> Color.rgb(255, 110, 100)
        })
    }

    /** utime + stime of a /proc stat line, in clock ticks (the fields after the command's closing parenthesis). */
    private fun cpuTicks(path: String): Long = runCatching {
        val fields = File(path).readText().substringAfterLast(')').trim().split(' ')
        fields[11].toLong() + fields[12].toLong() // (14th and 15th of the whole line)
    }.getOrDefault(0L)

    /** The GPU's load in percent, or null when the driver lets nothing be read. */
    private fun gpuLoad(): Int? {
        val f = gpuFile ?: return null
        val numbers = runCatching { Regex("\\d+").findAll(f.readText()).map { it.value.toLong() }.toList() }.getOrNull() ?: return null
        return when {
            f.name == "gpubusy" && numbers.size >= 2 -> if (numbers[1] > 0) (numbers[0] * 100 / numbers[1]).toInt().coerceIn(0, 100) else 0
            numbers.isNotEmpty() -> numbers[0].toInt().coerceIn(0, 100)
            else -> null
        }
    }

    companion object {
        private const val FIELD_RATE = 60000.0 / 1001.0 // NTSC fields per second: 100% speed
        private val GPU_FILES = listOf(
            "/sys/class/kgsl/kgsl-3d0/gpubusy",                // Adreno: busy and total time of the last window
            "/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage",    // Adreno: percent
            "/sys/kernel/gpu/gpu_busy",                        // Mali (Samsung): percent
            "/sys/class/misc/mali0/device/utilization",        // Mali: percent
            "/sys/devices/platform/mali.0/utilization",        // Mali: percent
        )
    }
}
