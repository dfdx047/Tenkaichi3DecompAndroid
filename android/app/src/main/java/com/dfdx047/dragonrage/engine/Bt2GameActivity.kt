package com.dfdx047.dragonrage.engine

import android.view.KeyEvent
import android.view.ViewGroup
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.dfdx047.dragonrage.data.Bt2Drivers
import com.dfdx047.dragonrage.data.Bt2Engine
import com.dfdx047.dragonrage.data.Bt2MenuItem
import com.dfdx047.dragonrage.data.Bt2Settings
import com.dfdx047.dragonrage.data.Bt2Unlock
import com.dfdx047.dragonrage.data.Game
import com.dfdx047.dragonrage.data.GpuDriver
import com.dfdx047.dragonrage.data.Renderer
import android.content.Intent
import java.io.File
import org.libsdl.app.SDLActivity

/**
 * Budokai Tenkaichi 2: BT2-Recomp's runtime built for arm64 as libbt2.so, started by SDL's activity like BT3 is, with
 * the same on-screen controller, in-game menu key and "quit to launcher" ([GameActivity]).
 *
 * It runs in a process of its own (":game_bt2" in the manifest) and never loads libmain.so: BT3's loader reserves
 * the low addresses BT3's engine needs as soon as it is loaded, and SDL keeps its state in statics. BT2's runtime
 * keeps the PS2's memory in an array of its own, so libbt2.so is an ordinary shared library that SDL loads itself
 * and whose SDL_main it calls.
 *
 * Its menu (the controller's menu button and the back key, [showMenu]) is the activity's own: the screen's
 * resolution and the renderer's speed options, applied while the game runs, the controller's options and leaving.
 *
 * What libbt2.so provides (bt2/runtime/android/android_main.cpp, built by bt2/android/build.py):
 *   SDL_main(argc, argv)    argv[1] the game's folder (<root>/bt2: data/BT2.iso, saves/, startup/, user/), argv[2]
 *                           the app's native library folder, argv[3] and argv[4] the chosen GPU driver's folder and
 *                           file ("" for none)
 *   Java_com_dfdx047_dragonrage_engine_TouchBridge_nativeSetPad(buttons, lx, ly, rx, ry)   player 1's pad (PS2 bits)
 *   Java_com_dfdx047_dragonrage_engine_TouchBridge_nativeMenuOpen()                        its own menu is open
 *   Java_com_dfdx047_dragonrage_engine_TouchBridge_nativePlayerTeamSize()                  (-1: unknown)
 * and it calls [quitToApp] when the player leaves the game (back twice).
 */
class Bt2GameActivity : GameActivity() {
    /** BT2's runtime reads its disc image and settings itself and makes its startup inputs from the image
     *  (bt2/runtime/game/startup_inputs.h): nothing of BT3's to put in place. */
    override fun prepareEngine() {
        // the GPU's name, found once and kept (Renderer.gpuName): the in-game menu offers OpenGL ES for Mali GPUs
        runCatching { Renderer.gpuName(paths) }
        Bt2MenuItem.install(this, paths) // the pictures of the Online Mode item (engine reads <root>/bt2/user/menu-item)
    }

    /** BT2 has the menu of its own below (Bt2Menu), on BT2's settings: GameMenu is BT3's and BT4's. */
    override fun usesGameMenu(): Boolean = false

    override fun getLibraries(): Array<String> = arrayOf("SDL3", Game.BT2.library)

    /** The engine the player imported (Bt2Engine: files/engines/bt2), else the APK's own (getLibraries). */
    private val imported by lazy { Bt2Engine.usable(this) }

    override fun loadLibraries() {
        val engine = imported ?: return super.loadLibraries()
        System.loadLibrary("SDL3")
        System.load(engine.path)
    }

    override fun getMainSharedObject(): String = imported?.path ?: super.getMainSharedObject()

    /** BT2's own controller layout (<root>/bt2/touch_controls.json): what the BT2 tab sets, apart from BT3's. */
    override fun touchFolder(): File = paths.bt2Root

    /** BT2's own GPU driver choice (Bt2ViewModel: <root>/bt2/user/driver), apart from BT3's. */
    override fun gpuDriver(): GpuDriver? = Bt2Drivers.selected(this, paths)

    /**
     * The game wrote gpu_preset = 1 in its renderer settings: it found a weak GPU and turned the effects off for the first run. Here the
     * screen's picture goes to 75% of its size, once (the settings can change it back; this does not come again).
     */
    private fun lowEndPreset() {
        val done = File(paths.bt2Root, "user/gpu_preset_done")
        if (done.exists()) return
        val marked = runCatching { File(paths.bt2Root, "user/renderer").readLines().any { it.trim() == "gpu_preset = 1" } }.getOrDefault(false)
        if (!marked) return
        runCatching {
            val settings = Bt2Settings(paths)
            val v = settings.load()
            if (v.outputScale == 100) settings.save(v.copy(outputScale = 75))
            done.writeText("75\n")
        }
    }

    override fun onResume() {
        super.onResume()
        lowEndPreset()
        applyOutputScale(Bt2Settings(paths).load().outputScale)
        refreshPerfOverlay()
        (SDLActivity.getContentView() as? ViewGroup)?.let { online.start(it) }
        holdWifi()
        // The whole screen for the game: the status and navigation bars come back with a swipe.
        WindowCompat.getInsetsController(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.systemBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    override fun onPadMenu() = showMenu()

    override fun onBackKey(event: KeyEvent): Boolean {
        if (event.action == KeyEvent.ACTION_UP) showMenu()
        return true
    }

    /** The screen's picture at [percent] of the surface's size: the engine draws into a smaller swapchain, which the
     *  display enlarges (its swapchain follows the window's size). */
    fun applyOutputScale(percent: Int) {
        val surface = mSurface ?: return
        surface.post {
            val w = surface.width
            val h = surface.height
            if (percent >= 100 || w <= 0 || h <= 0) surface.holder.setSizeFromLayout()
            else surface.holder.setFixedSize(w * percent / 100, h * percent / 100)
        }
    }

    override fun onPause() {
        perf.hide()
        online.stop()
        wifiLock?.let { runCatching { if (it.isHeld) it.release() } }
        super.onPause()
    }

    /** Wi-Fi in low-latency mode while the game is in front: on 2.4 GHz especially (and on 5 GHz too) the radio's power saving holds packets
     *  for tens of milliseconds, which is what makes an online fight stutter; the mode keeps it awake. Without a Wi-Fi connection it does nothing. */
    private var wifiLock: android.net.wifi.WifiManager.WifiLock? = null

    private fun holdWifi() {
        runCatching {
            val manager = applicationContext.getSystemService(WIFI_SERVICE) as android.net.wifi.WifiManager
            val lock = wifiLock ?: manager.createWifiLock(android.net.wifi.WifiManager.WIFI_MODE_FULL_LOW_LATENCY, "dragonrage:bt2-online").also {
                it.setReferenceCounted(false)
                wifiLock = it
            }
            if (!lock.isHeld) lock.acquire()
        }
    }

    /** What the app puts over the Online Mode screens: the player's online name (Bt2OnlineOverlay). */
    private val online by lazy { Bt2OnlineOverlay(this, paths) }

    /**
     * The engine again in a new process (Bt2RelaunchActivity): for an online match (its own card first, so both players start from the same one) or,
     * when the match is over, as it normally is. The game's own process ends, as it does when the player quits (GameActivity.quitToApp).
     */
    fun restartEngine(forMatch: Boolean) {
        if (forMatch) {
            if (!Bt2Unlock.makeMatchCard(applicationContext, paths)) {
                runCatching { File(paths.bt2Root, "user/online_session").delete() }
            }
        } else {
            runCatching { File(paths.bt2Root, "user/online_session").delete() }
        }
        runCatching { File(paths.bt2Root, "user/online_match").delete() }
        runOnUiThread {
            startActivity(Intent(this, Bt2RelaunchActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
        }
        android.os.Handler(android.os.Looper.getMainLooper()).postDelayed({ android.os.Process.killProcess(android.os.Process.myPid()) }, 400)
    }

    /** The performance panel (Bt2PerfOverlay) as the setting has it: the BT2 tab's or the in-game menu's switch. */
    private val perf by lazy { Bt2PerfOverlay(this) }

    fun refreshPerfOverlay() {
        val layout = SDLActivity.getContentView() as? ViewGroup ?: return
        if (Bt2Settings(paths).load().showFrameRate) perf.show(layout) else perf.hide()
    }

    /** BT2's menu over the game (Bt2Menu, laid out as BT3's), which stands still meanwhile. */
    private val menu by lazy { Bt2Menu(this, paths, onControls = { showOptions() }, onClose = { overlayOpen = false }) }

    private fun showMenu() {
        val layout = SDLActivity.getContentView() as? ViewGroup ?: return
        if (menu.isShown) { menu.close(); return }
        overlayOpen = true
        TouchBridge.setPad(0, 128, 128, 128, 128)
        menu.show(layout)
    }

    override fun getArguments(): Array<String> {
        val driver = gpuDriver()
        return arrayOf(paths.bt2Root.path, applicationInfo.nativeLibraryDir, driver?.dir?.path ?: "", driver?.library ?: "")
    }
}
