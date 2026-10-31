package com.dfdx047.dragonrage

import android.app.Application
import android.app.LocaleManager
import android.os.Build
import android.os.LocaleList
import android.content.Intent
import android.net.Uri
import android.provider.Settings
import androidx.core.content.FileProvider
import java.io.File
import java.io.IOException
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.dfdx047.dragonrage.data.ActivityLog
import com.dfdx047.dragonrage.data.AppUpdates
import com.dfdx047.dragonrage.data.CatalogState
import com.dfdx047.dragonrage.data.EngineSettings
import com.dfdx047.dragonrage.data.Renderer
import com.dfdx047.dragonrage.data.Net
import com.dfdx047.dragonrage.data.TextureCatalog
import com.dfdx047.dragonrage.data.UpdateState
import com.dfdx047.dragonrage.data.formatBytes
import com.dfdx047.dragonrage.data.zipUnpackedSize
import com.dfdx047.dragonrage.data.ExtraItem
import com.dfdx047.dragonrage.data.ExtraKind
import com.dfdx047.dragonrage.data.FileMod
import com.dfdx047.dragonrage.data.Bt4Installer
import com.dfdx047.dragonrage.data.Game
import com.dfdx047.dragonrage.data.GameDataInstaller
import com.dfdx047.dragonrage.data.GamePaths
import com.dfdx047.dragonrage.data.GpuDriver
import com.dfdx047.dragonrage.data.GpuDrivers
import com.dfdx047.dragonrage.data.GpuProfiles
import com.dfdx047.dragonrage.data.DriverAsset
import com.dfdx047.dragonrage.data.DriverRelease
import com.dfdx047.dragonrage.data.DriverSource
import com.dfdx047.dragonrage.data.DriverSources
import com.dfdx047.dragonrage.data.PerfReport
import com.dfdx047.dragonrage.data.LastRun
import com.dfdx047.dragonrage.data.OnlineState
import com.dfdx047.dragonrage.data.OnlineStatus
import com.dfdx047.dragonrage.data.LogExport
import com.dfdx047.dragonrage.data.ImportResult
import com.dfdx047.dragonrage.data.InstallState
import com.dfdx047.dragonrage.data.Mods
import com.dfdx047.dragonrage.data.TexturePack
import com.dfdx047.dragonrage.data.TexturePacks
import com.dfdx047.dragonrage.data.UiPrefs
import com.dfdx047.dragonrage.engine.EngineBridge
import com.dfdx047.dragonrage.touch.TouchConfig
import com.dfdx047.dragonrage.touch.TouchStore
import com.dfdx047.dragonrage.ui.theme.ThemeMode
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

/** A long operation in progress (shown as a bar with its text). */
data class Busy(val title: String, val detail: String = "")

class AppViewModel @JvmOverloads constructor(app: Application, val game: Game = Game.BT3) : AndroidViewModel(app) {
    val paths = GamePaths(app, game).also {
        it.ensure()
        ActivityLog.init(it)
        ActivityLog.info("app", "opened (${game.id}): ${BuildConfig.VERSION_NAME} (${BuildConfig.VERSION_CODE}), PC release ${BuildConfig.PORT_RELEASE}, Android ${Build.VERSION.RELEASE}, ${Build.MANUFACTURER} ${Build.MODEL}")
    }
    private val resolver = app.contentResolver
    private val installer = GameDataInstaller(app, paths, resolver)
    /** Budokai Tenkaichi 4's set-up (its own: the image is checked and read in place); null for Budokai Tenkaichi 3. */
    private val bt4 = if (game == Game.BT4) Bt4Installer(app, paths, resolver) else null
    private val packs = TexturePacks(app, paths, resolver)
    private val mods = Mods(app, paths, resolver)
    private val settings = EngineSettings(paths.settings)
    private val ui = UiPrefs.get(app)

    val uiState = ui.state
    val engineAvailable = EngineBridge.isAvailable(app, game.engine)

    private val _install = MutableStateFlow(bt4?.currentState() ?: installer.currentState())
    val install: StateFlow<InstallState> = _install.asStateFlow()

    private val _packs = MutableStateFlow<List<TexturePack>>(emptyList())
    val texturePacks: StateFlow<List<TexturePack>> = _packs.asStateFlow()

    private val _mods = MutableStateFlow<List<FileMod>>(emptyList())
    val fileMods: StateFlow<List<FileMod>> = _mods.asStateFlow()

    private val _stages = MutableStateFlow<List<ExtraItem>>(emptyList())
    val stages: StateFlow<List<ExtraItem>> = _stages.asStateFlow()

    private val _songs = MutableStateFlow<List<ExtraItem>>(emptyList())
    val songs: StateFlow<List<ExtraItem>> = _songs.asStateFlow()

    private val _settings = MutableStateFlow(settings.snapshot())
    val engineSettings: StateFlow<Map<String, Int>> = _settings.asStateFlow()

    private val touchStore = TouchStore(paths.root)
    private val _touch = MutableStateFlow(touchStore.load())
    /** The on-screen controller (touch_controls.json, read by the game's process at start). */
    val touch: StateFlow<TouchConfig> = _touch.asStateFlow()

    fun updateTouch(c: TouchConfig) {
        _touch.value = c
        viewModelScope.launch(Dispatchers.IO) { touchStore.save(c) }
    }

    // ---- GPU drivers (adrenotools) and bug reports

    private val drivers = GpuDrivers(app, resolver)
    private val _drivers = MutableStateFlow(drivers.list())
    val gpuDrivers: StateFlow<List<GpuDriver>> = _drivers.asStateFlow()
    private val _driver = MutableStateFlow(drivers.selected()?.id)
    /** The chosen driver's id; null = the system's. */
    val gpuDriver: StateFlow<String?> = _driver.asStateFlow()

    fun importDriver(uri: Uri) = viewModelScope.launch(Dispatchers.IO) {
        ActivityLog.info("driver", "import: ${ActivityLog.describe(resolver, uri)}")
        val r = runCatching { drivers.import(uri) }
        val d = r.getOrNull()
        _drivers.value = drivers.list()
        if (d == null) {
            ActivityLog.fail("driver", "import failed (not a driver .zip?)", r.exceptionOrNull())
            failed(R.string.fail_driver_title, str(R.string.driver_bad_zip))
        } else {
            ActivityLog.info("driver", "imported: ${d.name} (${d.id})")
            drivers.select(d.id)
            _driver.value = d.id
            say(str(R.string.driver_imported, d.name))
        }
    }

    fun selectDriver(id: String?) = viewModelScope.launch(Dispatchers.IO) {
        ActivityLog.info("driver", "chosen: ${id ?: "the system's"}")
        drivers.select(id)
        _driver.value = drivers.selected()?.id
    }

    fun deleteDriver(d: GpuDriver) = viewModelScope.launch(Dispatchers.IO) {
        ActivityLog.info("driver", "removed: ${d.name}")
        drivers.delete(d)
        _drivers.value = drivers.list()
        _driver.value = drivers.selected()?.id
    }

    private val _logShare = MutableSharedFlow<Uri>(extraBufferCapacity = 1)
    /** A report saved in Downloads, to offer for sharing. */
    val logShare: SharedFlow<Uri> = _logShare.asSharedFlow()

    fun exportLog() = viewModelScope.launch(Dispatchers.IO) {
        runCatching { LogExport(getApplication(), paths).saveToDownloads() }
            .onSuccess { (uri, name) ->
                say(str(R.string.log_saved, name))
                _logShare.tryEmit(uri)
            }
            .onFailure {
                ActivityLog.fail("log", "export failed", it)
                say(str(R.string.log_failed, it.message ?: ""))
            }
    }

    /** Saves a report and opens the share sheet at once (pages without the Debug page's log card, Textures). */
    fun exportLogAndShare() = viewModelScope.launch(Dispatchers.IO) {
        val log = autoLog()
        if (log == null) say(str(R.string.log_auto_failed)) else _texFail.value = TexFail("", log)
    }

    private val _busy = MutableStateFlow<Busy?>(null)
    val busy: StateFlow<Busy?> = _busy.asStateFlow()

    private val _messages = MutableSharedFlow<String>(extraBufferCapacity = 8)
    val messages: SharedFlow<String> = _messages.asSharedFlow()

    private var installJob: Job? = null

    init {
        refreshAll()
    }

    // ---- renderer: Vulkan, or OpenGL ES (chosen by itself for Mali GPUs)

    private val _gpuName = MutableStateFlow("")
    /** The GPU's name as its OpenGL ES driver gives it ("" until known). */
    val gpuName: StateFlow<String> = _gpuName.asStateFlow()

    // ---- the GPU's tier and its settings (offered once; again from the Debug page)

    private val _gpuOffer = MutableStateFlow<GpuProfiles.Profile?>(null)
    /** Recommended settings for this GPU waiting for the player's yes or no (a dialog). */
    val gpuOffer: StateFlow<GpuProfiles.Profile?> = _gpuOffer.asStateFlow()

    fun gpuProfile(): GpuProfiles.Profile? = GpuProfiles.profile(_gpuName.value)

    fun offerGpuProfile() {
        val p = gpuProfile()
        if (p == null) say(str(R.string.gpu_unknown_tier, _gpuName.value.ifEmpty { "?" })) else _gpuOffer.value = p
    }

    fun acceptGpuOffer() = viewModelScope.launch(Dispatchers.IO) {
        val p = _gpuOffer.value ?: return@launch
        _gpuOffer.value = null
        GpuProfiles.apply(p, settings)
        _settings.value = settings.snapshot()
        say(str(R.string.gpu_applied))
    }

    fun declineGpuOffer() = viewModelScope.launch(Dispatchers.IO) {
        val p = _gpuOffer.value ?: return@launch
        _gpuOffer.value = null
        if (settings.get(EngineSettings.GPU_TIER, -1) == -1) settings.set(EngineSettings.GPU_TIER, -2 - p.tier) // (asked: not again)
        _settings.value = settings.snapshot()
    }

    // ---- the last run's numbers (Debug page)

    private val _perf = MutableStateFlow<PerfReport?>(null)
    val perf: StateFlow<PerfReport?> = _perf.asStateFlow()

    private val _online = MutableStateFlow<OnlineState>(OnlineState.Checking)
    /** How online play stands for this build (the matchmaking service's answer), for the banner on Home. */
    val online: StateFlow<OnlineState> = _online.asStateFlow()

    private val _matchServer = MutableStateFlow(OnlineStatus.current(paths))
    /** The matchmaking service the game uses (official unless the player chose another, Settings). */
    val matchServer: StateFlow<String> = _matchServer.asStateFlow()

    init {
        // (the Community server of the 0.7.3 test builds is Dragon Net's own now: against PC, the PC port's service)
        runCatching {
            val f = File(paths.root, "match_url.txt")
            if (f.exists() && f.readText().contains("mitigo-rooms")) {
                f.delete()
                ActivityLog.info("online", "match_url.txt (mitigo-rooms) removed: that server is Dragon Net's now")
            }
        }
    }

    fun setMatchServer(url: String) {
        ActivityLog.info("online", "PC server: $url")
        OnlineStatus.choose(paths, url)
        _matchServer.value = OnlineStatus.current(paths)
        checkOnline()
    }

    fun checkOnline() {
        _online.value = OnlineState.Checking
        val mode = _settings.value[OnlineStatus.MODE] ?: 0
        viewModelScope.launch {
            val r = runCatching { OnlineStatus.check(paths, BuildConfig.PORT_RELEASE, mode) }
            _online.value = r.getOrDefault(OnlineState.Unreachable)
            ActivityLog.info("online", "${if (mode == 0) "Dragon Net" else "PC"}: ${_online.value}" + (r.exceptionOrNull()?.let { " (${it.javaClass.simpleName}: ${it.message})" }
                ?: OnlineStatus.lastFailure?.takeIf { _online.value == OnlineState.Unreachable }?.let { " ($it)" } ?: ""))
        }
    }

    /** Whom online plays against: 0 Dragon Net (Android + Switch), 1 the PC (also chosen in the game's online screen). */
    fun setNetMode(mode: Int) {
        viewModelScope.launch(Dispatchers.IO) {
            ActivityLog.info("online", "play against: ${if (mode == 0) "Android + Switch (Dragon Net)" else "PC"}")
            settings.set(OnlineStatus.MODE, mode)
            _settings.value = settings.snapshot()
            withContext(Dispatchers.Main) { checkOnline() }
        }
    }

    /** A report the app saved in Downloads by itself (a problem it saw), to name and to share. */
    data class SavedLog(val uri: Uri, val name: String)

    private fun autoLog(): SavedLog? = runCatching { LogExport(getApplication(), paths).saveToDownloads() }
        .getOrNull()?.let { (uri, name) -> SavedLog(uri, name) }

    private val _lastRun = MutableStateFlow<LastRun.Report?>(null)
    /** Why the game's last run stopped, when its log says it did not end well (shown once, as a dialog). */
    val lastRun: StateFlow<LastRun.Report?> = _lastRun.asStateFlow()
    private val _lastRunLog = MutableStateFlow<SavedLog?>(null)
    /** The report saved for it in Downloads (made at once: players rarely export one themselves). */
    val lastRunLog: StateFlow<SavedLog?> = _lastRunLog.asStateFlow()

    fun checkLastRun() = viewModelScope.launch(Dispatchers.IO) {
        runCatching { LastRun.check(paths) }.getOrNull()?.let {
            ActivityLog.fail("game", "the last run ended badly: ${it.problem}: ${it.detail}")
            _lastRunLog.value = autoLog()
            _lastRun.value = it
        }
    }

    fun dismissLastRun() {
        _lastRun.value = null
    }

    /** A texture pack that could not be installed: why, and the report saved for it in Downloads. */
    data class TexFail(val reason: String, val log: SavedLog?, val title: Int = R.string.texfail_title)

    private val _texFail = MutableStateFlow<TexFail?>(null)
    val texFail: StateFlow<TexFail?> = _texFail.asStateFlow()

    fun dismissTexFail() {
        _texFail.value = null
    }

    private fun texFailed(e: Exception) {
        ActivityLog.fail("textures", "install failed", e)
        _texFail.value = TexFail(e.message ?: e.javaClass.simpleName, autoLog())
    }

    /** Anything else that failed (the ISO, a driver, a mod, an update): logged, the report saved in Downloads at once,
     *  and the dialog with "Send log". */
    private fun failed(title: Int, reason: String) {
        _texFail.value = TexFail(reason, autoLog(), title)
    }

    fun refreshPerf() = viewModelScope.launch(Dispatchers.IO) { _perf.value = runCatching { PerfReport.read(paths) }.getOrNull() }

    // ---- drivers from their authors' GitHub releases (Drivers page)

    sealed interface DriverList {
        data object Loading : DriverList
        data object Failed : DriverList
        data class Loaded(val releases: List<DriverRelease>) : DriverList
    }

    private val _driverLists = MutableStateFlow<Map<DriverSource, DriverList>>(emptyMap())
    val driverLists: StateFlow<Map<DriverSource, DriverList>> = _driverLists.asStateFlow()

    fun loadDrivers(source: DriverSource, again: Boolean = false) {
        val now = _driverLists.value[source]
        if (!again && (now is DriverList.Loaded || now is DriverList.Loading)) return
        _driverLists.value = _driverLists.value + (source to DriverList.Loading)
        viewModelScope.launch {
            val r = runCatching { DriverSources.releases(source) }
            _driverLists.value = _driverLists.value + (source to (r.getOrNull()?.let { DriverList.Loaded(it) } ?: DriverList.Failed))
        }
    }

    fun downloadDriver(asset: DriverAsset) = work(str(R.string.busy_driver_download), R.string.fail_driver_title) {
        ActivityLog.info("driver", "download: ${asset.url}")
        paths.temp.mkdirs()
        val file = java.io.File(paths.temp, "driver-${System.currentTimeMillis()}.zip")
        try {
            Net.download(asset.url, file) { done, total ->
                setDetail(str(R.string.progress_downloading, formatBytes(done)) + if (total > 0) " / ${formatBytes(total)}" else "")
            }
            if (!Net.looksLikeZip(file)) throw IOException(str(R.string.err_download_not_zip))
            val d = drivers.importFile(file) ?: throw IOException(str(R.string.driver_bad_zip))
            drivers.select(d.id)
            _drivers.value = drivers.list()
            _driver.value = d.id
            ActivityLog.info("driver", "downloaded and imported: ${d.name}")
            say(str(R.string.driver_imported, d.name))
        } finally {
            file.delete()
        }
    }

    init {
        viewModelScope.launch(Dispatchers.IO) {
            _gpuName.value = runCatching { Renderer.gpuName(paths) }.getOrDefault("")
            runCatching { Renderer.apply(settings, paths) }
            _settings.value = settings.snapshot()
            // the GPU's recommended settings, offered once (GpuProfiles)
            val p = GpuProfiles.profile(_gpuName.value)
            if (p != null && settings.get(EngineSettings.GPU_TIER, -1) == -1) _gpuOffer.value = p
            _perf.value = runCatching { PerfReport.read(paths) }.getOrNull()
        }
        checkOnline()
    }

    fun setRenderer(choice: Int) = viewModelScope.launch(Dispatchers.IO) {
        ActivityLog.info("settings", "renderer: $choice (0 automatic, 1 Vulkan, 2 OpenGL ES), GPU ${_gpuName.value}")
        settings.set(EngineSettings.RENDERER, choice)
        runCatching { Renderer.apply(settings, paths) }
        _settings.value = settings.snapshot()
    }

    fun refreshAll() {
        viewModelScope.launch(Dispatchers.IO) {
            _packs.value = packs.list()
            _mods.value = mods.listMods()
            _stages.value = mods.listExtras(ExtraKind.STAGE)
            _songs.value = mods.listExtras(ExtraKind.SONG)
            settings.load()
            _settings.value = settings.snapshot()
            _touch.value = touchStore.load() // (the game's own quick menu may have changed it)
        }
    }

    // ---- game data

    fun installFromIso(uri: Uri) {
        if (installJob?.isActive == true) return
        installJob = viewModelScope.launch {
            val started = System.currentTimeMillis()
            withContext(Dispatchers.IO) {
                ActivityLog.info("iso", "install from ${ActivityLog.describe(resolver, uri)}")
                ActivityLog.info("iso", "free space: ${paths.root.usableSpace / (1024 * 1024)} MB in ${paths.root}")
            }
            var step = ""
            try {
                val report: (InstallState.Working) -> Unit = {
                    _install.value = it
                    if (it.step != step) {
                        step = it.step
                        ActivityLog.info("iso", "step: ${it.step} (${it.detail})")
                    }
                }
                val done = if (bt4 != null) bt4.install(uri, report) else installer.install(uri, report = report)
                if (done.game == Game.BT2) { // a BT2 image picked here goes to the BT2 tab (Bt2ViewModel)
                    _install.value = installer.currentState()
                    say(str(R.string.msg_bt2_installed_tab))
                } else {
                    _install.value = done
                }
                ActivityLog.info("iso", "installed ${done.game}: ${done.files} files, ${done.bytes} bytes, modified image: ${done.modified}, " +
                    "${(System.currentTimeMillis() - started) / 1000} s")
                say(str(R.string.msg_data_ready))
            } catch (e: CancellationException) {
                ActivityLog.warn("iso", "cancelled at: $step")
                // (BT3's data is only cleared when its own unpacking had begun: a cancelled BT2 copy leaves it as it was)
                withContext(Dispatchers.IO) { if (bt4 != null) bt4.uninstall() else if (!paths.installedMarker.exists()) installer.uninstall() }
                _install.value = bt4?.currentState() ?: installer.currentState()
                throw e
            } catch (e: GameDataInstaller.Stop) {
                val why = e.message ?: str(R.string.msg_error)
                ActivityLog.fail("iso", "refused at \"$step\": $why")
                _install.value = InstallState.Failed(why)
                withContext(Dispatchers.IO) { failed(R.string.fail_iso_title, why) }
            } catch (e: Exception) {
                val why = str(R.string.msg_iso_read_error, e.message ?: e.javaClass.simpleName)
                ActivityLog.fail("iso", "failed at \"$step\"", e)
                _install.value = InstallState.Failed(why)
                withContext(Dispatchers.IO) { failed(R.string.fail_iso_title, why) }
            }
        }
    }

    fun cancelInstall() {
        installJob?.cancel()
    }

    fun removeGameData() = viewModelScope.launch(Dispatchers.IO) {
        ActivityLog.info("iso", "game data removed by the player")
        if (bt4 != null) bt4.uninstall() else installer.uninstall()
        _install.value = InstallState.NotInstalled
    }

    fun dismissInstallError() {
        if (_install.value is InstallState.Failed) _install.value = bt4?.currentState() ?: installer.currentState()
    }

    // ---- texture packs

    fun importTexturePack(uri: Uri) = work(str(R.string.busy_import_pack)) {
        try {
            ActivityLog.info("textures", "import: ${ActivityLog.describe(resolver, uri)}")
            val p = packs.import(uri) { setDetail(it) }
            _packs.value = packs.list()
            ActivityLog.info("textures", "installed: ${p.name}, ${p.textures} textures")
            say(str(R.string.msg_pack_installed, p.name, p.textures))
        } catch (e: CancellationException) {
            throw e
        } catch (e: Exception) {
            texFailed(e)
        }
    }

    fun setPackEnabled(pack: TexturePack, on: Boolean) = work(str(if (on) R.string.busy_pack_on else R.string.busy_pack_off)) {
        ActivityLog.info("textures", "${pack.name}: ${if (on) "on" else "off"}")
        packs.setEnabled(pack, on)
        _packs.value = packs.list()
    }

    fun deletePack(pack: TexturePack) = work(str(R.string.busy_pack_remove)) {
        ActivityLog.info("textures", "removed: ${pack.name}")
        packs.delete(pack)
        _packs.value = packs.list()
        say(str(R.string.msg_pack_removed, pack.name))
    }

    private val _catalog = MutableStateFlow<CatalogState>(CatalogState.Loading)
    val catalog: StateFlow<CatalogState> = _catalog.asStateFlow()

    fun loadCatalog() {
        _catalog.value = CatalogState.Loading
        viewModelScope.launch {
            _catalog.value = runCatching { CatalogState.Loaded(TextureCatalog.load()) }.getOrDefault(CatalogState.Failed)
        }
    }

    /** Downloads a texture pack (.zip, https) and installs it like one picked from the files. */
    fun downloadTexturePack(url: String, name: String?) {
        val u = url.trim()
        if (!u.startsWith("https://")) {
            say(str(R.string.err_https_only))
            return
        }
        work(str(R.string.busy_download_pack)) {
            paths.temp.mkdirs()
            val file = File(paths.temp, "download-${System.currentTimeMillis()}.zip")
            try {
              try {
                Net.download(u, file) { done, total ->
                    setDetail(str(R.string.progress_downloading, formatBytes(done)) + if (total > 0) " / ${formatBytes(total)}" else "")
                }
                ActivityLog.info("textures", "downloaded ${file.length()} bytes from $u")
                if (!Net.looksLikeZip(file)) throw IOException(str(R.string.err_download_not_zip))
                val label = name ?: u.substringBefore('?').substringAfterLast('/').removeSuffix(".zip").ifBlank { "Texture pack" }
                packs.checkRoom(zipUnpackedSize { java.io.RandomAccessFile(file, "r").channel })
                val p = packs.importFrom(label, { file.inputStream() }) { d -> setDetail(d) }
                _packs.value = packs.list()
                ActivityLog.info("textures", "installed: ${p.name}, ${p.textures} textures")
                say(str(R.string.msg_pack_installed, p.name, p.textures))
              } catch (e: CancellationException) {
                throw e
              } catch (e: Exception) {
                texFailed(e)
              }
            } finally {
                file.delete()
            }
        }
    }

    // ---- app updates (GitHub releases)

    private val _update = MutableStateFlow<UpdateState>(UpdateState.Idle)
    val update: StateFlow<UpdateState> = _update.asStateFlow()
    private var updateFile: File? = null
    private var updateJob: Job? = null

    // (after the state above: properties are set up in order, and this reads _update)
    init {
        checkForUpdate(manual = false)
    }

    /** Looks for a newer version. At start it only speaks up when there is one; from About it also says "up to date". */
    fun checkForUpdate(manual: Boolean) {
        if (_update.value !is UpdateState.Idle) return
        _update.value = UpdateState.Checking
        viewModelScope.launch {
            val r = runCatching { AppUpdates.check(BuildConfig.VERSION_NAME, BuildConfig.VERSION_CODE) }
            val info = r.getOrNull()
            r.exceptionOrNull()?.let { ActivityLog.warn("update", "check failed: ${it.javaClass.simpleName}: ${it.message}") }
            if (info != null) ActivityLog.info("update", "available: ${info}")
            _update.value = if (info != null) UpdateState.Available(info) else UpdateState.Idle
            if (manual && info == null) say(str(if (r.isSuccess) R.string.update_none else R.string.update_check_failed))
        }
    }

    fun dismissUpdate() {
        if (_update.value is UpdateState.Available || _update.value is UpdateState.NeedPermission) _update.value = UpdateState.Idle
    }

    fun cancelUpdate() {
        updateJob?.cancel()
        _update.value = UpdateState.Idle
    }

    fun startUpdate() {
        val info = (_update.value as? UpdateState.Available)?.info ?: return
        val app = getApplication<Application>()
        updateJob = viewModelScope.launch {
            _update.value = UpdateState.Downloading(info, 0, info.size)
            ActivityLog.info("update", "download: ${info.apkUrl}")
            try {
                val dir = File(app.cacheDir, "updates").apply { deleteRecursively(); mkdirs() }
                val f = File(dir, "DragonRage-update.apk")
                Net.download(info.apkUrl, f) { d, t -> _update.value = UpdateState.Downloading(info, d, if (t > 0) t else info.size) }
                val pkg = app.packageManager.getPackageArchiveInfo(f.path, 0)?.packageName
                if (pkg != app.packageName) throw IOException(str(R.string.update_bad_apk))
                updateFile = f
                ActivityLog.info("update", "downloaded ${f.length()} bytes")
                installUpdate(info)
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                _update.value = UpdateState.Idle
                ActivityLog.fail("update", "failed", e)
                withContext(Dispatchers.IO) { failed(R.string.fail_update_title, str(R.string.update_failed, e.message ?: e.javaClass.simpleName)) }
            }
        }
    }

    /** Hands the APK to the system installer, or asks first for the "install unknown apps" permission. */
    private fun installUpdate(info: com.dfdx047.dragonrage.data.UpdateInfo) {
        val app = getApplication<Application>()
        val f = updateFile ?: return
        if (!app.packageManager.canRequestPackageInstalls()) {
            _update.value = UpdateState.NeedPermission(info)
            return
        }
        val uri = FileProvider.getUriForFile(app, "${app.packageName}.files", f)
        app.startActivity(
            Intent(Intent.ACTION_VIEW).setDataAndType(uri, "application/vnd.android.package-archive")
                .addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION or Intent.FLAG_ACTIVITY_NEW_TASK),
        )
        _update.value = UpdateState.Idle
    }

    fun openInstallPermission() {
        val app = getApplication<Application>()
        app.startActivity(
            Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:${app.packageName}")).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
        )
    }

    /** Back from the settings screen: if the permission is on now, the install goes ahead. */
    fun resumeUpdate() {
        val s = _update.value
        if (s is UpdateState.NeedPermission && getApplication<Application>().packageManager.canRequestPackageInstalls()) installUpdate(s.info)
    }

    // ---- mods, stages, songs

    fun importMod(uris: List<Uri>) = work(str(R.string.busy_import_mods), R.string.fail_mod_title) {
        var mods0 = 0
        var stages0 = 0
        var songs0 = 0
        for (uri in uris) {
            ActivityLog.info("mods", "import: ${ActivityLog.describe(resolver, uri)}")
            when (val r = mods.import(uri) { setDetail(it) }) {
                is ImportResult.Mod -> mods0++
                is ImportResult.Extras -> if (r.kind == ExtraKind.STAGE) stages0 += r.count else songs0 += r.count
            }
        }
        refreshMods()
        ActivityLog.info("mods", "added: $mods0 mods, $stages0 stages, $songs0 songs")
        say(str(R.string.msg_added, listOfNotNull(
            mods0.takeIf { it > 0 }?.let { str(R.string.count_mods, it) },
            stages0.takeIf { it > 0 }?.let { str(R.string.count_stages, it) },
            songs0.takeIf { it > 0 }?.let { str(R.string.count_songs, it) },
        ).joinToString(", ")))
    }

    fun setModEnabled(mod: FileMod, on: Boolean) = work(str(R.string.busy_apply_mods), R.string.fail_mod_title) {
        ActivityLog.info("mods", "${mod.name}: ${if (on) "on" else "off"}")
        mods.setEnabled(mod, on) { setDetail(it) }
        refreshMods()
    }

    fun moveMod(mod: FileMod, delta: Int) = work(str(R.string.busy_reorder_mods)) {
        mods.move(mod, delta) { setDetail(it) }
        refreshMods()
    }

    fun deleteMod(mod: FileMod) = work(str(R.string.busy_remove_mod)) {
        ActivityLog.info("mods", "removed: ${mod.name}")
        mods.delete(mod) { setDetail(it) }
        refreshMods()
        say(str(R.string.msg_mod_removed, mod.name))
    }

    fun renameExtra(kind: ExtraKind, item: ExtraItem, name: String) = work(str(R.string.busy_rename)) {
        mods.rename(kind, item, name)
        refreshMods()
    }

    fun deleteExtra(kind: ExtraKind, item: ExtraItem) = work(str(R.string.busy_remove)) {
        mods.deleteExtra(kind, item)
        refreshMods()
    }

    private fun refreshMods() {
        _mods.value = mods.listMods()
        _stages.value = mods.listExtras(ExtraKind.STAGE)
        _songs.value = mods.listExtras(ExtraKind.SONG)
    }

    // ---- settings

    /** Lite mode: on, it also sets the glow and the glare to 0 (the chains at 0 are left out); off, it leaves them. */
    fun setLite(on: Boolean) = viewModelScope.launch(Dispatchers.IO) {
        settings.set(EngineSettings.LITE, if (on) 1 else 0)
        if (on) {
            settings.set(EngineSettings.GLOW, 0)
            settings.set(EngineSettings.GLARE, 0)
        }
        _settings.value = settings.snapshot()
    }

    fun setting(name: String, value: Int) {
        viewModelScope.launch(Dispatchers.IO) {
            settings.set(name, value)
            _settings.value = settings.snapshot()
        }
    }

    fun setTheme(mode: ThemeMode) = ui.setTheme(mode)

    /** The app's language ("" = the system's), through Android's per-app language (13 and newer). */
    val canPickLanguage = Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU

    fun language(): String =
        if (canPickLanguage) getApplication<Application>().getSystemService(LocaleManager::class.java).applicationLocales.toLanguageTags()
        else ""

    fun setLanguage(tag: String) {
        if (canPickLanguage) {
            getApplication<Application>().getSystemService(LocaleManager::class.java).applicationLocales = LocaleList.forLanguageTags(tag)
        }
    }
    fun setDynamicColor(on: Boolean) = ui.setDynamic(on)

    // ---- launch

    fun play() {
        if (game == Game.BT4 && _install.value is InstallState.Installed && GpuProfiles.weakChip(_gpuName.value)) {
            android.widget.Toast.makeText(getApplication(), str(R.string.bt4_weak_cpu), android.widget.Toast.LENGTH_LONG).show()
        }
        when {
            _install.value !is InstallState.Installed -> say(str(R.string.msg_install_first))
            !engineAvailable -> say(str(R.string.msg_no_engine))
            else -> ActivityLog.info("game", "started (renderer ${_settings.value[EngineSettings.RENDERER] ?: 0}" + (if (game == Game.BT4) ", BT4 renderer ${if ((_settings.value[EngineSettings.BT4_RENDERER] ?: 1) == 1) "ARMSX2" else "Dragon Rage"}" else "") + ", online ${if ((_settings.value[OnlineStatus.MODE] ?: 0) == 0) "Dragon Net" else "PC"})").let { getApplication<android.app.Application>().startActivity(
                android.content.Intent(getApplication(), if (game == Game.BT4) com.dfdx047.dragonrage.engine.Bt4Activity::class.java else com.dfdx047.dragonrage.engine.GameActivity::class.java)
                    .addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK),
            ) }
        }
    }

    /** A "Play" icon for the home screen: it starts the game without opening the launcher (PlayShortcutActivity). */
    fun createPlayShortcut() {
        val app = getApplication<Application>()
        if (!androidx.core.content.pm.ShortcutManagerCompat.isRequestPinShortcutSupported(app)) {
            say(str(R.string.shortcut_unsupported))
            return
        }
        val info = androidx.core.content.pm.ShortcutInfoCompat.Builder(app, "play-pinned")
            .setShortLabel(str(R.string.app_name))
            .setLongLabel(str(R.string.shortcut_play_long))
            .setIcon(androidx.core.graphics.drawable.IconCompat.createWithResource(app, R.mipmap.ic_launcher))
            .setIntent(
                android.content.Intent(app, com.dfdx047.dragonrage.PlayShortcutActivity::class.java)
                    .setAction(android.content.Intent.ACTION_VIEW),
            )
            .build()
        if (androidx.core.content.pm.ShortcutManagerCompat.requestPinShortcut(app, info, null)) say(str(R.string.shortcut_asked))
    }

    // ---- helpers

    private fun str(id: Int, vararg args: Any): String = getApplication<Application>().getString(id, *args)

    private fun say(text: String) {
        _messages.tryEmit(text)
    }

    private fun setDetail(text: String) {
        _busy.value = _busy.value?.copy(detail = text)
    }

    private fun work(title: String, failTitle: Int = 0, block: suspend () -> Unit) {
        if (_busy.value != null) {
            say(str(R.string.msg_wait))
            return
        }
        viewModelScope.launch(Dispatchers.IO) {
            _busy.value = Busy(title)
            try {
                block()
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                ActivityLog.fail("work", "\"$title\" failed", e)
                val why = e.message ?: "${str(R.string.msg_error)}: ${e.javaClass.simpleName}"
                if (failTitle != 0) failed(failTitle, why) else say(why)
            } finally {
                _busy.value = null
            }
        }
    }

    companion object {
        /** The view model of one game (MainActivity keeps one per game: switching does not mix their folders). */
        fun factory(app: Application, game: Game) = object : androidx.lifecycle.ViewModelProvider.Factory {
            @Suppress("UNCHECKED_CAST")
            override fun <T : androidx.lifecycle.ViewModel> create(modelClass: Class<T>): T = AppViewModel(app, game) as T
        }
    }
}
