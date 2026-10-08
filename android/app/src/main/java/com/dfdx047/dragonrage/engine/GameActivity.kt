package com.dfdx047.dragonrage.engine

import android.app.AlertDialog
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.ViewGroup
import android.widget.LinearLayout
import android.widget.RadioButton
import android.widget.RadioGroup
import android.widget.SeekBar
import android.widget.TextView
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.GamePaths
import com.dfdx047.dragonrage.data.GpuDrivers
import com.dfdx047.dragonrage.touch.PadSkin
import com.dfdx047.dragonrage.touch.TouchConfig
import com.dfdx047.dragonrage.touch.TouchMode
import com.dfdx047.dragonrage.touch.TouchPadView
import com.dfdx047.dragonrage.touch.TouchStore
import java.io.File
import org.libsdl.app.SDLActivity

/**
 * The game itself: SDL's activity, which loads libSDL3.so and libmain.so (the engine's loader,
 * src/main/cpp/loader.c) and calls SDL_main with the arguments below. It runs in a process of its own (":game" in
 * the manifest): the engine needs fixed addresses below 4 GB, which a fresh process has free, and when the game ends
 * its process goes with it and the launcher stays as it was.
 *
 * Over the game's surface: the on-screen controller ([TouchPadView]), which feeds player 1's pad through
 * [TouchBridge] and steps aside while the engine's own window (settings, online) is open.
 */
class GameActivity : SDLActivity() {
    private lateinit var paths: GamePaths
    private lateinit var store: TouchStore
    private var pad: TouchPadView? = null
    private val handler = Handler(Looper.getMainLooper())

    private val watchMenu = object : Runnable {
        override fun run() {
            pad?.suppressed = TouchBridge.menuOpen()
            handler.postDelayed(this, 100)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        paths = GamePaths(this).also { it.ensure() }
        store = TouchStore(paths.root)
        installDataList()
        super.onCreate(savedInstanceState)
        addTouchPad()
    }

    private fun addTouchPad() {
        val layout = SDLActivity.getContentView() as? ViewGroup ?: return
        val view = TouchPadView(this, store.load())
        view.listener = object : TouchPadView.Listener {
            override fun onPad(buttons: Int, lx: Int, ly: Int, rx: Int, ry: Int) = TouchBridge.setPad(buttons, lx, ly, rx, ry)

            override fun onMenu() {
                SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_BACK)
                SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_BACK)
            }

            override fun onOptions() = showOptions()

            override fun onConfigChanged(config: TouchConfig) = store.save(config)

            override fun onEditDone() {
                view.editing = false
            }
        }
        layout.addView(view, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        pad = view
    }

    override fun onResume() {
        super.onResume()
        pad?.let { it.config = store.load() }
        handler.post(watchMenu)
    }

    override fun onPause() {
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

    private fun fromController(source: Int): Boolean =
        source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK

    override fun dispatchGenericMotionEvent(ev: MotionEvent): Boolean {
        if (fromController(ev.source)) padUsed()
        return super.dispatchGenericMotionEvent(ev)
    }

    /** The list of where the game's data tables come from (made with the engine, android.py) next to the game's files. */
    private fun installDataList() {
        // the list of the game's data tables (libbt3.so.dat) and of the engine's own addresses (libbt3.so.rel, for
        // loading it elsewhere than its usual place), both made with the engine (android.py), next to the game's files
        for (name in listOf("libbt3.so.dat", "libbt3.so.rel")) {
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
            when (event.action) {
                KeyEvent.ACTION_DOWN -> if (event.repeatCount == 0) SDLActivity.onNativeKeyDown(KeyEvent.KEYCODE_BACK)
                KeyEvent.ACTION_UP -> SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_BACK)
            }
            return true
        }
        if (KeyEvent.isGamepadButton(event.keyCode) || fromController(event.source)) padUsed()
        return super.dispatchKeyEvent(event)
    }

    override fun getLibraries(): Array<String> = arrayOf("SDL3", "main")

    /** The game folder, the app's native libraries (the engine, adrenotools' hooks) and the chosen GPU driver, if any. */
    override fun getArguments(): Array<String> {
        val driver = runCatching { GpuDrivers(this, contentResolver).selected() }.getOrNull()
        return arrayOf(paths.root.path, applicationInfo.nativeLibraryDir, driver?.dir?.path ?: "", driver?.library ?: "")
    }
}
