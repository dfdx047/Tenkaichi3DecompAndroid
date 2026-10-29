package com.dfdx047.dragonrage.engine

import android.app.AlertDialog
import android.content.Intent
import android.hardware.input.InputManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.Process
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.SeekBar
import android.widget.TextView
import com.dfdx047.dragonrage.MainActivity
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.EngineSettings
import com.dfdx047.dragonrage.data.GamePaths
import com.dfdx047.dragonrage.data.Renderer
import com.dfdx047.dragonrage.data.GpuDrivers
import com.dfdx047.dragonrage.touch.PadSkin
import com.dfdx047.dragonrage.touch.TouchConfig
import com.dfdx047.dragonrage.touch.TouchMode
import com.dfdx047.dragonrage.touch.TouchListener
import com.dfdx047.dragonrage.touch.TouchSurface
import com.dfdx047.dragonrage.touch.createTouchSurface
import com.dfdx047.dragonrage.touch.view
import com.dfdx047.dragonrage.touch.TouchStore
import java.io.File
import org.libsdl.app.SDLActivity

/**
 * The game itself: SDL's activity, which loads libSDL3.so and libmain.so (the engine's loader,
 * src/main/cpp/loader.c) and calls SDL_main with the arguments below. It runs in a process of its own (":game" in
 * the manifest): the engine needs fixed addresses below 4 GB, which a fresh process has free, and when the game ends
 * its process goes with it and the launcher stays as it was.
 *
 * Over the game's surface: the on-screen controller ([TouchSurface]: the game-specific layout or the classic one), which feeds player 1's pad through
 * [TouchBridge] and steps aside while the engine's own window (settings, online) is open.
 */
open class GameActivity : SDLActivity() {
    protected lateinit var paths: GamePaths
    private lateinit var store: TouchStore
    private var pad: TouchSurface? = null
    private val handler = Handler(Looper.getMainLooper())
    private var menu: GameMenu? = null
    private var comboHeld = 0 // START (1) and SELECT (2) held on a controller

    /** Which game this screen runs (the menu's look and pages). */
    protected open val game: com.dfdx047.dragonrage.data.Game get() = com.dfdx047.dragonrage.data.Game.BT3

    private val watchMenu = object : Runnable {
        override fun run() {
            pad?.suppressed = TouchBridge.menuOpen() || menu?.isShown == true
            handler.postDelayed(this, 100)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        paths = gamePaths().also { it.ensure() }
        // the renderer for this start (Vulkan, or OpenGL ES for Mali when automatic): written before the engine reads it
        runCatching {
            val es = EngineSettings(paths.settings)
            Renderer.apply(es, paths)
            // the in-game menu's language: the launcher's (0 English, 1 Portuguese, 2 Spanish)
            val lang = when (resources.configuration.locales[0].language) { "pt" -> 1; "es" -> 2; else -> 0 }
            if (es.get(EngineSettings.LANG, -1) != lang) es.set(EngineSettings.LANG, lang)
            orientation = es.get(EngineSettings.ORIENTATION, 0)
            sustained = es.get(EngineSettings.SUSTAINED, 0) != 0
        }
        requestedOrientation = orientationFlag()
        store = TouchStore(paths.root)
        installDataList()
        super.onCreate(savedInstanceState)
        hideBars()
        addTouchPad()
        if (sustained) { // (Settings: steady clocks the phone can hold, where it supports them)
            runCatching {
                val pm = getSystemService(POWER_SERVICE) as android.os.PowerManager
                if (pm.isSustainedPerformanceModeSupported) window.setSustainedPerformanceMode(true)
                android.util.Log.i("bt3", "sustained performance mode: ${pm.isSustainedPerformanceModeSupported}")
            }
        }
    }

    /**
     * The system's bars go before the game's surface is made. SDL shows them at the start and hides them only when
     * the engine makes its window, by when the surface exists without the room of the navigation bar (the RP5:
     * 1920 x 972); in some starts the surface stayed at that size for the whole run and the system stretched the
     * picture to the screen (16:9 with bars at the sides, everything 11% taller). The same flags as SDL's own
     * fullscreen, so its state (it hides them again after a swipe) agrees.
     */
    @Suppress("DEPRECATION")
    private fun hideBars() {
        window.decorView.systemUiVisibility = android.view.View.SYSTEM_UI_FLAG_FULLSCREEN or
            android.view.View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
            android.view.View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY or
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
            android.view.View.SYSTEM_UI_FLAG_LAYOUT_STABLE
        window.addFlags(android.view.WindowManager.LayoutParams.FLAG_FULLSCREEN)
        window.clearFlags(android.view.WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN)
        setWindowStyle(true)
    }

    /** The player's choice (launcher, Settings): 0 landscape, 1 portrait, 2 as the device is held. */
    private var orientation = 0
    private var sustained = false

    private fun orientationFlag(): Int = when (orientation) {
        1 -> android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
        2 -> android.content.pm.ActivityInfo.SCREEN_ORIENTATION_FULL_USER
        else -> android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
    }

    /** SDL picks an orientation from the window's size when it is made; the player's choice wins over it. */
    override fun setOrientationBis(w: Int, h: Int, resizable: Boolean, hint: String?) {
        requestedOrientation = orientationFlag()
    }

    private fun addTouchPad() {
        val layout = SDLActivity.getContentView() as? ViewGroup ?: return
        val view = createTouchSurface(this, store.load())
        view.listener = object : TouchListener {
            override fun onPad(buttons: Int, lx: Int, ly: Int, rx: Int, ry: Int) = TouchBridge.setPad(buttons, lx, ly, rx, ry)

            override fun onMenu() = toggleMenu()

            override fun onOptions() = showOptions()

            override fun onConfigChanged(config: TouchConfig) = store.save(config)

            override fun onEditDone() {
                view.editing = false
            }
        }
        layout.addView(view.view, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        pad = view
        if (stickPadConnected()) padUsed() // (a handheld's own controller, or one paired before the game: no touch pad to start with)
        menu = GameMenu(this, game, { EngineSettings(paths.settings) }, object : GameMenu.Actions {
            override fun resumed() {
                pad?.suppressed = TouchBridge.menuOpen()
            }

            override fun quit() = quitToApp()

            override fun editTouchLayout() {
                pad?.editing = true
            }

            override fun touchOptions() = showOptions()

            override fun padButtons() = MenuBridge.action(MenuBridge.ACTION_PAD_BUTTONS)
        })
    }

    /**
     * The back key, the controller's MENU button and the engine's request: the touch menu (GameMenu) opens or closes.
     * While the engine's own window is up (the controller's menu, START + SELECT, or the online window), the key goes to
     * the engine and closes that window, as before; and when the engine cannot answer, the key opens its window.
     */
    private fun toggleMenu() {
        val m = menu
        when {
            pad?.editing == true -> pad?.editing = false
            m?.isShown == true -> m.close()
            TouchBridge.menuOpen() -> backToEngine()
            else -> {
                TouchBridge.setPad(0, 128, 128, 128, 128)
                val layout = SDLActivity.getContentView() as? ViewGroup
                if (m == null || layout == null || !m.show(layout)) backToEngine() else pad?.suppressed = true
            }
        }
    }

    private fun backToEngine() {
        SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_BACK)
        SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_BACK)
    }

    /** The engine asks for the menu (Budokai Tenkaichi 4: START + SELECT; from its own thread). */
    @Suppress("unused")
    fun openGameMenu() {
        runOnUiThread { if (menu?.isShown != true) toggleMenu() }
    }

    override fun onResume() {
        super.onResume()
        pad?.let { it.config = store.load() }
        handler.post(watchMenu)
        runCatching { (getSystemService(INPUT_SERVICE) as InputManager).registerInputDeviceListener(devices, handler) }
    }

    override fun onPause() {
        runCatching { (getSystemService(INPUT_SERVICE) as InputManager).unregisterInputDeviceListener(devices) }
        handler.removeCallbacks(watchMenu)
        TouchBridge.setPad(0, 128, 128, 128, 128)
        super.onPause()
    }

    /** The controller's own quick menu: look, opacity, layout editing, shown or not. */
    private fun showOptions() {
        val view = pad ?: return
        var c = view.config
        val dp = resources.displayMetrics.density
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding((20 * dp).toInt(), (8 * dp).toInt(), (20 * dp).toInt(), 0)
        }
        box.addView(TextView(this).apply { setText(R.string.touch_skin) })
        val skins = RadioGroup(this).apply { orientation = RadioGroup.HORIZONTAL }
        PadSkin.entries.forEachIndexed { i, s ->
            skins.addView(RadioButton(this).apply {
                id = 1000 + i
                setText(skinName(s))
                isChecked = c.skin == s
            })
        }
        skins.setOnCheckedChangeListener { _, id ->
            c = c.copy(skin = PadSkin.entries[id - 1000])
            view.config = c
            store.save(c)
        }
        box.addView(skins)
        val opacityLabel = TextView(this).apply { text = getString(R.string.touch_opacity_n, c.opacity) }
        box.addView(opacityLabel)
        box.addView(SeekBar(this).apply {
            max = 90
            progress = c.opacity - 10
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar, p: Int, fromUser: Boolean) {
                    c = c.copy(opacity = p + 10)
                    opacityLabel.text = getString(R.string.touch_opacity_n, c.opacity)
                    view.config = c
                }

                override fun onStartTrackingTouch(sb: SeekBar) {}
                override fun onStopTrackingTouch(sb: SeekBar) = store.save(c)
            })
        })
        AlertDialog.Builder(this)
            .setTitle(R.string.touch_controls)
            .setView(box)
            .setPositiveButton(R.string.touch_edit_layout) { _, _ -> view.editing = true }
            .setNeutralButton(if (c.mode == TouchMode.OFF) R.string.touch_show_controls else R.string.touch_hide_controls) { _, _ ->
                c = c.copy(mode = if (c.mode == TouchMode.OFF) TouchMode.AUTO else TouchMode.OFF)
                view.config = c
                store.save(c)
            }
            .setNegativeButton(R.string.close, null)
            .show()
    }

    private fun skinName(s: PadSkin): Int = when (s) {
        PadSkin.PLAYSTATION -> R.string.skin_playstation
        PadSkin.XBOX -> R.string.skin_xbox
        PadSkin.NINTENDO -> R.string.skin_nintendo
    }

    /** A physical controller in use: the on-screen one hides (automatic mode) until the screen is touched again. */
    private fun padUsed() {
        val v = pad ?: return
        if (v.config.mode == TouchMode.AUTO && !v.editing) v.hiddenForPad = true
    }

    /** A controller that is there: not a virtual device (some phones send their own sensors' or the system's key
     *  events as a "gamepad", which hid the on-screen controls again and again). */
    private fun realDevice(d: InputDevice?): Boolean =
        d != null && !d.isVirtual && (d.sources and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            d.sources and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK)

    /** A real controller with sticks is connected (not only keys that call themselves a gamepad). */
    private fun stickPadConnected(): Boolean = InputDevice.getDeviceIds().any { id ->
        val d = InputDevice.getDevice(id)
        realDevice(d) && d!!.getMotionRange(MotionEvent.AXIS_X) != null && d.getMotionRange(MotionEvent.AXIS_Y) != null
    }

    /** A controller connected while the game runs hides the touch pad; the last one gone brings it back. */
    private val devices = object : InputManager.InputDeviceListener {
        override fun onInputDeviceAdded(id: Int) {
            if (stickPadConnected()) padUsed()
        }

        override fun onInputDeviceRemoved(id: Int) {
            if (!stickPadConnected()) pad?.hiddenForPad = false
        }

        override fun onInputDeviceChanged(id: Int) {}
    }

    private fun fromController(source: Int): Boolean =
        source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK

    override fun dispatchGenericMotionEvent(ev: MotionEvent): Boolean {
        if (fromController(ev.source) && realDevice(ev.device)) padUsed()
        return super.dispatchGenericMotionEvent(ev)
    }

    /** The Tag Team rooms' relay (RelaySocket; port/src/gs/tag_link.c, through JNI on the engine's threads). */
    @Volatile private var relay: RelaySocket? = null

    @Suppress("unused")
    fun relayOpen(url: String): Boolean {
        relay?.close()
        relay = RelaySocket.connect(url)
        return relay != null
    }

    @Suppress("unused")
    fun relaySend(data: ByteArray): Boolean = relay?.send(data) ?: false

    /** The next message in, or null; an empty array: the relay is closed. */
    @Suppress("unused")
    fun relayRecv(): ByteArray? {
        val r = relay ?: return ByteArray(0)
        return r.receive() ?: if (r.open) null else ByteArray(0)
    }

    @Suppress("unused")
    fun relayClose() {
        relay?.close()
        relay = null
    }

    /**
     * HTTPS for the engine's online play (port/src/gs/net_match.c, through JNI on the engine's own thread): a POST of
     * [body] (JSON) to [url], the answer's text, or "" when there was none. Only the matchmaking service is asked this
     * way (a room code and the addresses the two games can reach each other at; no game data).
     */
    @Suppress("unused")
    fun netPost(url: String, body: String): String = netRequest(url, body, "")

    /**
     * The engine's requests to the matchmaking service (port/src/gs/net_match.c, through JNI on the engine's thread):
     * a POST of [body], or a GET when it is null; [headers] are extra "Name: value" lines (an official build signs
     * its requests with them). The answer's text, "" when there is none.
     */
    @Suppress("unused")
    fun netRequest(url: String, body: String?, headers: String?): String = runCatching {
        val c = (java.net.URL(url).openConnection() as java.net.HttpURLConnection).apply {
            requestMethod = if (body != null) "POST" else "GET"
            doOutput = body != null
            connectTimeout = 8_000
            readTimeout = 12_000
            if (body != null) setRequestProperty("Content-Type", "application/json")
            setRequestProperty("User-Agent", "DragonRage-Android")
            headers.orEmpty().lineSequence().forEach { line ->
                val k = line.substringBefore(':', "").trim()
                if (k.isNotEmpty()) setRequestProperty(k, line.substringAfter(':').trim())
            }
        }
        try {
            if (body != null) c.outputStream.use { it.write(body.toByteArray()) }
            val code = c.responseCode
            val stream = if (code in 200..299) c.inputStream else c.errorStream
            val text = stream?.bufferedReader()?.use { it.readText() } ?: ""
            // (in the log for a report about online play: the matchmaking's refusals, limits and outages)
            if (code !in 200..299) android.util.Log.w("bt3", "net: HTTP $code from ${url.substringAfterLast('/')}: ${text.take(200)}")
            // A 429 is said in the answer, which the engine reads (net_match.c: it then asks nothing for a while). A
            // Worker's own is JSON; Cloudflare's (error 1027, the daily requests) is not.
            if (code == 429) """{"error":"${if (text.trimStart().startsWith("{") && text.contains("busy")) "busy" else "limit"}","http":429}""" else text
        } finally {
            c.disconnect()
        }
    }.getOrElse {
        android.util.Log.w("bt3", "net: ${url.substringAfterLast('/')} failed: $it")
        ""
    }

    /**
     * "Quit to launcher" of the in-game window (port/src/gs/ui.cpp, through JNI on the engine's thread): the launcher
     * comes forward and the game's own process ends. A game cannot be started again in the same process (the engine
     * needs the fixed addresses only a fresh one has), so the process goes, as it does when the game is closed.
     */
    @Suppress("unused")
    fun quitToApp() {
        runOnUiThread {
            TouchBridge.setPad(0, 128, 128, 128, 128)
            startActivity(
                Intent(this, MainActivity::class.java)
                    .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TOP or Intent.FLAG_ACTIVITY_SINGLE_TOP),
            )
        }
        // a moment for the launcher to be started, then this process goes (the engine's saves are closed as they are written)
        Handler(Looper.getMainLooper()).postDelayed({ Process.killProcess(Process.myPid()) }, 450)
    }

    /** The list of where the game's data tables come from (made with the engine, android.py) next to the game's files. */
    /** The game's folder: Budokai Tenkaichi 3's (Bt4Activity: Budokai Tenkaichi 4's). */
    protected open fun gamePaths(): GamePaths = GamePaths(this)

    protected open fun installDataList() {
        // the list of the game's data tables (libbt3.so.dat) and of the engine's own addresses (libbt3.so.rel, for
        // loading it elsewhere than its usual place), both made with the engine (android.py), next to the game's files
        // and of the game's own variables (libbt3.so.mem: what a saved state holds, for online play)
        for (name in listOf("libbt3.so.dat", "libbt3.so.rel", "libbt3.so.mem")) {
            val out = File(paths.root, name)
            runCatching {
                assets.open(name).use { input ->
                    val bytes = input.readBytes()
                    if (!out.exists() || out.length() != bytes.size.toLong() || !out.readBytes().contentEquals(bytes)) {
                        out.writeBytes(bytes)
                    }
                }
            }
        }
        // the files the engine reads beside itself (android.py: assets/engine): fonts/ and tbo/ui/, copied when the
        // app's version changed (or they are missing)
        val stamp = File(paths.root, ".engine-files")
        val version = runCatching { packageManager.getPackageInfo(packageName, 0).lastUpdateTime.toString() }.getOrDefault("0")
        if (!stamp.exists() || stamp.readText() != version) {
            runCatching {
                copyAssetTree("engine", paths.root)
                stamp.writeText(version)
            }.onFailure { android.util.Log.w("bt3", "engine files: $it") }
        }
    }

    private fun copyAssetTree(path: String, into: File) {
        val names = assets.list(path).orEmpty()
        if (names.isEmpty()) { // a file
            val rel = path.substringAfter('/', "")
            if (rel.isEmpty()) return
            val out = File(into, rel)
            out.parentFile?.mkdirs()
            assets.open(path).use { input -> out.outputStream().use { input.copyTo(it) } }
            return
        }
        names.forEach { copyAssetTree("$path/$it", into) }
    }

    /**
     * The back key opens and closes the in-game menu, wherever it comes from. On handhelds such as the AYN Odin the
     * back button belongs to the built-in controller, and SDL would hand it to the game as that controller's
     * Back/Select button; here it always reaches the engine as the back key (gs_gpu.c: SDLK_AC_BACK).
     */
    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.keyCode == KeyEvent.KEYCODE_BACK) {
            if (pad?.editing == true) {
                if (event.action == KeyEvent.ACTION_UP) pad?.editing = false
                return true
            }
            if (event.action == KeyEvent.ACTION_UP) toggleMenu() // (the whole press: down and up are this one key)
            return true
        }
        if (event.keyCode == KeyEvent.KEYCODE_BUTTON_START || event.keyCode == KeyEvent.KEYCODE_BUTTON_SELECT) {
            // START + SELECT closes the touch menu as it opened it (Budokai Tenkaichi 4: openGameMenu). While the menu is
            // up their presses stay here, so the game does not get a START of its own when it resumes; their releases
            // go on (the engine saw the presses that opened the menu)
            val bit = if (event.keyCode == KeyEvent.KEYCODE_BUTTON_START) 1 else 2
            comboHeld = if (event.action == KeyEvent.ACTION_DOWN) comboHeld or bit else comboHeld and bit.inv()
            if (menu?.isShown == true && event.action == KeyEvent.ACTION_DOWN) {
                if (event.repeatCount == 0 && comboHeld == 3) menu?.close()
                return true
            }
        }
        if ((KeyEvent.isGamepadButton(event.keyCode) || fromController(event.source)) && realDevice(event.device)) padUsed()
        return super.dispatchKeyEvent(event)
    }

    override fun getLibraries(): Array<String> = arrayOf("SDL3", "main")

    /** The game folder, the app's native libraries (the engine, adrenotools' hooks) and the chosen GPU driver, if any. */
    override fun getArguments(): Array<String> {
        val driver = runCatching { GpuDrivers(this, contentResolver).selected() }.getOrNull()
        // Lite mode on a Mali: the render passes' areas cut to what they draw (loader.c, area_driver)
        val liteMali = game == com.dfdx047.dragonrage.data.Game.BT3 && runCatching {
            EngineSettings(paths.settings).get(EngineSettings.LITE, 0) != 0 && Renderer.isMali(Renderer.gpuName(paths))
        }.getOrDefault(false)
        return arrayOf(
            paths.root.path, applicationInfo.nativeLibraryDir, driver?.dir?.path ?: "", driver?.library ?: "",
            if (liteMali) "1" else "0",
        )
    }
}
