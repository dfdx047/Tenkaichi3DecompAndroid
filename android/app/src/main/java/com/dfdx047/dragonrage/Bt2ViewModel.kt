package com.dfdx047.dragonrage

import android.app.Application
import android.content.ContentValues
import android.content.Intent
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.dfdx047.dragonrage.data.ActivityLog
import com.dfdx047.dragonrage.data.Bt2Drivers
import com.dfdx047.dragonrage.data.Bt2Renderer
import com.dfdx047.dragonrage.data.Bt2Settings
import com.dfdx047.dragonrage.data.Bt2Unlock
import com.dfdx047.dragonrage.data.Game
import com.dfdx047.dragonrage.data.GameDataInstaller
import com.dfdx047.dragonrage.data.GamePaths
import com.dfdx047.dragonrage.data.GpuDriver
import com.dfdx047.dragonrage.data.GpuDrivers
import com.dfdx047.dragonrage.data.InstallState
import com.dfdx047.dragonrage.data.Renderer
import com.dfdx047.dragonrage.engine.Bt2GameActivity
import com.dfdx047.dragonrage.engine.EngineBridge
import com.dfdx047.dragonrage.touch.TouchConfig
import com.dfdx047.dragonrage.touch.TouchStore
import java.io.File
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.flow.MutableSharedFlow
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.SharedFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asSharedFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlin.coroutines.cancellation.CancellationException

/**
 * The Budokai Tenkaichi 2 tab (ui/screens/bt2): its disc image, its engine (libbt2.so, Bt2GameActivity), its
 * settings (Bt2Settings), its texture packs and the logs of its runs. Apart from AppViewModel, which serves BT3's
 * pages as before; what the two games share (the GPU driver, the on-screen controller) stays there.
 *
 * Everything set on the BT2 tab is BT2's alone and never touches BT3: its settings, its on-screen controller layout
 * (<root>/bt2/touch_controls.json), its GPU driver choice (Bt2Drivers), its texture packs. Its log is its own too
 * (<root>/bt2/user/activity.log, never the app's activity log BT3's pages show): when a run starts, the device, the
 * GPU and the settings; when the app comes back, the engine's log of the run (bt2/user/last-run.log) is kept in
 * bt2/user/runs (the last ten) and its outcome is noted.
 */
class Bt2ViewModel(app: Application) : AndroidViewModel(app) {
    val paths = GamePaths(app).also { it.ensure() }
    private val installer = GameDataInstaller(app, paths, app.contentResolver)
    private val settingsFile = Bt2Settings(paths)
    private val runLog = File(paths.bt2Root, "user/last-run.log")
    private val runs = File(paths.bt2Root, "user/runs")
    private val imported = File(paths.bt2Root, "user/.last-run-kept")
    private val activity = File(paths.bt2Root, "user/activity.log")
    private val touchStore = TouchStore(paths.bt2Root)
    private val drivers = GpuDrivers(app, app.contentResolver)

    /** libbt2.so in this APK (the releases, built where the game's code was generated; not the public CI's). */
    val engineAvailable = EngineBridge.isPackaged(app, Game.BT2)

    /** A motor package imported before 1.0 is deleted at the first start with the engine in the APK; the player is told once. */
    private val _engineReplaced = MutableStateFlow(false)
    val engineReplaced: StateFlow<Boolean> = _engineReplaced.asStateFlow()
    fun dismissEngineReplaced() { _engineReplaced.value = false }

    init {
        viewModelScope.launch(Dispatchers.IO) {
            val old = File(app.filesDir, "engines/bt2")
            if (engineAvailable && old.exists()) {
                note("I", "the motor package imported earlier deleted: the engine is in the APK")
                old.deleteRecursively()
                _engineReplaced.value = true
            }
        }
    }

    private val _install = MutableStateFlow(installer.currentStateBt2())
    val install: StateFlow<InstallState> = _install.asStateFlow()
    private var installJob: Job? = null

    private val _settings = MutableStateFlow(settingsFile.load())
    val settings: StateFlow<Bt2Settings.Values> = _settings.asStateFlow()

    /** The HD texture packs in bt2/data/textures (PCSX2's layout: a folder holding "replacements"). */
    private val _packs = MutableStateFlow<List<String>>(emptyList())
    val texturePacks: StateFlow<List<String>> = _packs.asStateFlow()

    /** The end of the last run's log, and the kept logs of earlier runs (newest first). */
    private val _lastRun = MutableStateFlow<List<String>>(emptyList())
    val lastRun: StateFlow<List<String>> = _lastRun.asStateFlow()
    private val _runs = MutableStateFlow<List<File>>(emptyList())
    val runHistory: StateFlow<List<File>> = _runs.asStateFlow()

    /** BT2's on-screen controller layout (apart from BT3's). */
    private val _touch = MutableStateFlow(touchStore.load())
    val touch: StateFlow<TouchConfig> = _touch.asStateFlow()

    /** The app's installed GPU drivers, and the one BT2 uses (null: the system's). */
    private val _drivers = MutableStateFlow<List<GpuDriver>>(emptyList())
    val gpuDrivers: StateFlow<List<GpuDriver>> = _drivers.asStateFlow()
    private val _driver = MutableStateFlow(Bt2Drivers.selectedId(paths))
    val gpuDriver: StateFlow<String?> = _driver.asStateFlow()

    /** The unlock-everything save is in this APK (assets/bt2/unlock: a 100% save as a card folder). */
    val unlockAvailable: Boolean = Bt2Unlock.available(app)
    /** BT2 plays from the unlock-everything card. */
    private val _unlocked = MutableStateFlow(Bt2Unlock.isOn(paths))
    val unlocked: StateFlow<Boolean> = _unlocked.asStateFlow()

    private val _messages = MutableSharedFlow<String>(extraBufferCapacity = 4)
    val messages: SharedFlow<String> = _messages.asSharedFlow()

    val texturesFolder: File get() = File(paths.bt2Data, "textures")

    private fun str(id: Int, vararg args: Any) = getApplication<Application>().getString(id, *args)
    private fun say(text: String) { _messages.tryEmit(text) }

    /** BT2's own log: one dated line per event, the newest 2000 kept. */
    private fun note(level: String, text: String) {
        runCatching {
            activity.parentFile?.mkdirs()
            activity.appendText(SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.ROOT).format(Date()) + " $level $text\n")
            if (activity.length() > 400_000) activity.writeText(activity.readLines().takeLast(2000).joinToString("\n", postfix = "\n"))
        }
    }

    /** On the app's return: what changed outside (files, a finished run). */
    fun refresh() = viewModelScope.launch(Dispatchers.IO) {
        if (installJob?.isActive != true) _install.value = installer.currentStateBt2()
        _settings.value = settingsFile.load()
        _packs.value = findPacks()
        _touch.value = touchStore.load() // (the game's quick menu may have changed it)
        _drivers.value = drivers.list()
        _driver.value = Bt2Drivers.selectedId(paths)
        _unlocked.value = Bt2Unlock.isOn(paths)
        keepFinishedRun()
        _lastRun.value = runCatching { runLog.readLines().takeLast(40) }.getOrDefault(emptyList())
        _runs.value = runs.listFiles()?.sortedByDescending { it.name }.orEmpty()
    }

    // ---- the disc image

    fun installFromIso(uri: Uri) {
        if (installJob?.isActive == true) return
        installJob = viewModelScope.launch {
            note("I", "image from ${withContext(Dispatchers.IO) { ActivityLog.describe(getApplication<Application>().contentResolver, uri) }}")
            try {
                val done = installer.install(uri, only = Game.BT2) { _install.value = it }
                _install.value = done
                note("I", "image in place: ${done.bytes} bytes")
                say(str(R.string.msg_data_ready))
            } catch (e: CancellationException) {
                _install.value = withContext(Dispatchers.IO) { installer.currentStateBt2() }
                throw e
            } catch (e: GameDataInstaller.Stop) {
                note("E", "image refused: ${e.message}")
                _install.value = InstallState.Failed(e.message ?: str(R.string.msg_error))
            } catch (e: Exception) {
                note("E", "image failed: $e")
                _install.value = InstallState.Failed(str(R.string.msg_iso_read_error, e.message ?: e.javaClass.simpleName))
            }
        }
    }

    fun cancelInstall() { installJob?.cancel() }

    fun dismissInstallError() { if (_install.value is InstallState.Failed) _install.value = installer.currentStateBt2() }

    fun removeImage() = viewModelScope.launch(Dispatchers.IO) {
        note("I", "image removed by the player")
        installer.uninstallBt2()
        _install.value = InstallState.NotInstalled
    }

    // ---- playing

    fun play() {
        when {
            _install.value !is InstallState.Installed -> say(str(R.string.msg_install_bt2_first))
            !engineAvailable -> say(str(R.string.msg_no_engine_bt2))
            else -> viewModelScope.launch {
                withContext(Dispatchers.IO) {
                    settingsFile.applyPhoneDefaults()
                    Bt2Renderer(paths).applyPhoneDefaults()
                    // (a match that ended badly must not make this start one: the engine takes online_session for a match's)
                    for (name in listOf("online_session", "online_restart", "online_match")) runCatching { File(paths.bt2Root, "user/$name").delete() }
                    val s = _settings.value
                    note("I", "run started: ${Build.MANUFACTURER} ${Build.MODEL}, Android ${Build.VERSION.RELEASE}, GPU ${Renderer.gpuName(paths)}")
                    note("I", "settings: super-sampling ${s.superSampling}x, widescreen ${s.widescreen}, 60 fps ${s.sixtyFps}, " +
                        "HD textures ${s.hdTextures}, light blending ${s.blendDemote}, no mipmaps ${s.noMipmaps}")
                }
                getApplication<Application>().startActivity(
                    Intent(getApplication(), Bt2GameActivity::class.java).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
                )
            }
        }
    }

    // ---- the on-screen controller and the GPU driver (BT2's own)

    fun updateTouch(c: TouchConfig) {
        _touch.value = c
        viewModelScope.launch(Dispatchers.IO) { touchStore.save(c) }
    }

    fun selectDriver(id: String?) = viewModelScope.launch(Dispatchers.IO) {
        note("I", "GPU driver chosen: ${id ?: "the system's"}")
        Bt2Drivers.select(paths, id)
        _driver.value = id
    }

    /** A driver .zip added to the app's drivers and chosen for BT2 only (BT3's choice stays). */
    fun importDriver(uri: Uri) = viewModelScope.launch(Dispatchers.IO) {
        val d = runCatching { drivers.import(uri) }.getOrNull()
        _drivers.value = drivers.list()
        if (d == null) { note("E", "driver import failed"); say(str(R.string.driver_bad_zip)); return@launch }
        note("I", "driver imported: ${d.name} (${d.id})")
        Bt2Drivers.select(paths, d.id)
        _driver.value = d.id
        say(str(R.string.driver_imported, d.name))
    }

    // ---- unlock everything

    /**
     * On: BT2 plays from a card of its own holding a 100% save (<root>/bt2/saves/"Tudo desbloqueado", made from the
     * APK's assets/bt2/unlock once and kept); the card in use before is remembered. Off: back to that card. The
     * player's own cards are never changed or deleted.
     */
    fun setUnlock(on: Boolean) = viewModelScope.launch(Dispatchers.IO) {
        val before = settingsFile.memoryCard()
        if (!Bt2Unlock.set(getApplication(), paths, on)) {
            note("E", "unlock card not made"); say(str(R.string.msg_error)); return@launch
        }
        note("I", if (on) "unlock everything: on (card \"${Bt2Unlock.CARD}\", previous card ${before ?: "(none)"})"
            else "unlock everything: off (back to ${settingsFile.memoryCard() ?: "the engine's choice"})")
        _unlocked.value = on
    }

    // ---- settings and textures

    fun update(change: (Bt2Settings.Values) -> Bt2Settings.Values) {
        val next = change(_settings.value)
        _settings.value = next
        viewModelScope.launch(Dispatchers.IO) { settingsFile.save(next) }
    }

    private fun findPacks(): List<String> {
        val folder = texturesFolder.also { it.mkdirs() }
        if (File(folder, "replacements").isDirectory) return listOf(".")
        return folder.listFiles()?.filter { File(it, "replacements").isDirectory }?.map { it.name }?.sorted().orEmpty()
    }

    // ---- logs

    /** A run's log the app has not kept yet: copied into bt2/user/runs and summed up in the activity log. */
    private fun keepFinishedRun() {
        if (!runLog.isFile) return
        val stamp = runLog.lastModified().toString()
        if (runCatching { imported.readText() }.getOrNull() == stamp) return
        runs.mkdirs()
        val name = "run-" + SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT).format(Date(runLog.lastModified())) + ".log"
        runCatching { runLog.copyTo(File(runs, name), overwrite = true) }
        runs.listFiles()?.sortedByDescending { it.name }?.drop(10)?.forEach { it.delete() }
        val lines = runCatching { runLog.readLines() }.getOrDefault(emptyList())
        val ended = lines.lastOrNull { it.startsWith("The game ended") || it.contains("stopped") } ?: lines.lastOrNull().orEmpty()
        note("I", "run log kept as $name (${lines.size} lines): ${ended.take(160)}")
        lines.filter { it.contains("error", true) || it.contains("lacks") || it.contains("Kernel baseline word read") }.take(20)
            .forEach { note("W", it.take(200)) }
        imported.writeText(stamp)
    }

    /** A report for sending: the device, the BT2 settings, the last run's log and the app's activity log, in Downloads. */
    fun saveReport() = viewModelScope.launch(Dispatchers.IO) {
        runCatching {
            val text = buildString {
                appendLine("Dragon Rage ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE}) - Budokai Tenkaichi 2")
                appendLine("${Build.MANUFACTURER} ${Build.MODEL}, Android ${Build.VERSION.RELEASE}, GPU ${Renderer.gpuName(paths)}")
                appendLine("engine in the APK: $engineAvailable, image: ${_install.value}")
                appendLine("\n== bt2/user/settings"); appendLine(runCatching { File(paths.bt2Root, "user/settings").readText() }.getOrDefault("(none)"))
                appendLine("== bt2/user/android"); appendLine(runCatching { File(paths.bt2Root, "user/android").readText() }.getOrDefault("(none)"))
                appendLine("== last run (bt2/user/last-run.log)"); appendLine(runCatching { runLog.readText() }.getOrDefault("(no run yet)"))
                appendLine("== BT2 activity log (bt2/user/activity.log)"); runCatching { activity.readLines().takeLast(300) }.getOrDefault(emptyList()).forEach { appendLine(it) }
            }
            val name = "DragonRage-BT2-log-" + SimpleDateFormat("yyyyMMdd-HHmmss", Locale.ROOT).format(Date()) + ".txt"
            val values = ContentValues().apply {
                put(MediaStore.Downloads.DISPLAY_NAME, name)
                put(MediaStore.Downloads.MIME_TYPE, "text/plain")
                put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS)
            }
            val resolver = getApplication<Application>().contentResolver
            val uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values) ?: error("Downloads is not available")
            resolver.openOutputStream(uri)?.use { it.write(text.toByteArray()) }
            name
        }.onSuccess { say(str(R.string.bt2_debug_saved, it)) }
            .onFailure { say(str(R.string.msg_error)) }
    }
}
