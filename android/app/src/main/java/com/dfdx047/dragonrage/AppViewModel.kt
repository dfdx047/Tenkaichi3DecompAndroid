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
import com.dfdx047.dragonrage.data.AppUpdates
import com.dfdx047.dragonrage.data.CatalogState
import com.dfdx047.dragonrage.data.EngineSettings
import com.dfdx047.dragonrage.data.Net
import com.dfdx047.dragonrage.data.TextureCatalog
import com.dfdx047.dragonrage.data.UpdateState
import com.dfdx047.dragonrage.data.formatBytes
import com.dfdx047.dragonrage.data.ExtraItem
import com.dfdx047.dragonrage.data.ExtraKind
import com.dfdx047.dragonrage.data.FileMod
import com.dfdx047.dragonrage.data.GameDataInstaller
import com.dfdx047.dragonrage.data.GamePaths
import com.dfdx047.dragonrage.data.GpuDriver
import com.dfdx047.dragonrage.data.GpuDrivers
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

class AppViewModel(app: Application) : AndroidViewModel(app) {
    val paths = GamePaths(app).also { it.ensure() }
    private val resolver = app.contentResolver
    private val installer = GameDataInstaller(app, paths, resolver)
    private val packs = TexturePacks(app, paths, resolver)
    private val mods = Mods(app, paths, resolver)
    private val settings = EngineSettings(paths.settings)
    private val ui = UiPrefs(app)

    val uiState = ui.state
    val engineAvailable = EngineBridge.isAvailable(app)

    private val _install = MutableStateFlow(installer.currentState())
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
        val d = runCatching { drivers.import(uri) }.getOrNull()
        _drivers.value = drivers.list()
        if (d == null) say(str(R.string.driver_bad_zip)) else {
            drivers.select(d.id)
            _driver.value = d.id
            say(str(R.string.driver_imported, d.name))
        }
    }

    fun selectDriver(id: String?) = viewModelScope.launch(Dispatchers.IO) {
        drivers.select(id)
        _driver.value = drivers.selected()?.id
    }

    fun deleteDriver(d: GpuDriver) = viewModelScope.launch(Dispatchers.IO) {
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
            .onFailure { say(str(R.string.log_failed, it.message ?: "")) }
    }

    private val _busy = MutableStateFlow<Busy?>(null)
    val busy: StateFlow<Busy?> = _busy.asStateFlow()

    private val _messages = MutableSharedFlow<String>(extraBufferCapacity = 8)
    val messages: SharedFlow<String> = _messages.asSharedFlow()

    private var installJob: Job? = null

    init {
        refreshAll()
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
            try {
                _install.value = installer.install(uri) { _install.value = it }
                say(str(R.string.msg_data_ready))
            } catch (e: CancellationException) {
                withContext(Dispatchers.IO) { installer.uninstall() }
                _install.value = InstallState.NotInstalled
                throw e
            } catch (e: GameDataInstaller.Stop) {
                _install.value = InstallState.Failed(e.message ?: str(R.string.msg_error))
            } catch (e: Exception) {
                _install.value = InstallState.Failed(str(R.string.msg_iso_read_error, e.message ?: e.javaClass.simpleName))
            }
        }
    }

    fun cancelInstall() {
        installJob?.cancel()
    }

    fun removeGameData() = viewModelScope.launch(Dispatchers.IO) {
        installer.uninstall()
        _install.value = InstallState.NotInstalled
    }

    fun dismissInstallError() {
        if (_install.value is InstallState.Failed) _install.value = installer.currentState()
    }

    // ---- texture packs

    fun importTexturePack(uri: Uri) = work(str(R.string.busy_import_pack)) {
        val p = packs.import(uri) { setDetail(it) }
        _packs.value = packs.list()
        say(str(R.string.msg_pack_installed, p.name, p.textures))
    }

    fun setPackEnabled(pack: TexturePack, on: Boolean) = work(str(if (on) R.string.busy_pack_on else R.string.busy_pack_off)) {
        packs.setEnabled(pack, on)
        _packs.value = packs.list()
    }

    fun deletePack(pack: TexturePack) = work(str(R.string.busy_pack_remove)) {
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
                Net.download(u, file) { done, total ->
                    setDetail(str(R.string.progress_downloading, formatBytes(done)) + if (total > 0) " / ${formatBytes(total)}" else "")
                }
                if (!Net.looksLikeZip(file)) throw IOException(str(R.string.err_download_not_zip))
                val label = name ?: u.substringBefore('?').substringAfterLast('/').removeSuffix(".zip").ifBlank { "Texture pack" }
                val p = file.inputStream().use { packs.importStream(label, it) { d -> setDetail(d) } }
                _packs.value = packs.list()
                say(str(R.string.msg_pack_installed, p.name, p.textures))
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
            try {
                val dir = File(app.cacheDir, "updates").apply { deleteRecursively(); mkdirs() }
                val f = File(dir, "DragonRage-update.apk")
                Net.download(info.apkUrl, f) { d, t -> _update.value = UpdateState.Downloading(info, d, if (t > 0) t else info.size) }
                val pkg = app.packageManager.getPackageArchiveInfo(f.path, 0)?.packageName
                if (pkg != app.packageName) throw IOException(str(R.string.update_bad_apk))
                updateFile = f
                installUpdate(info)
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                _update.value = UpdateState.Idle
                say(str(R.string.update_failed, e.message ?: e.javaClass.simpleName))
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

    fun importMod(uris: List<Uri>) = work(str(R.string.busy_import_mods)) {
        var mods0 = 0
        var stages0 = 0
        var songs0 = 0
        for (uri in uris) {
            when (val r = mods.import(uri) { setDetail(it) }) {
                is ImportResult.Mod -> mods0++
                is ImportResult.Extras -> if (r.kind == ExtraKind.STAGE) stages0 += r.count else songs0 += r.count
            }
        }
        refreshMods()
        say(str(R.string.msg_added, listOfNotNull(
            mods0.takeIf { it > 0 }?.let { str(R.string.count_mods, it) },
            stages0.takeIf { it > 0 }?.let { str(R.string.count_stages, it) },
            songs0.takeIf { it > 0 }?.let { str(R.string.count_songs, it) },
        ).joinToString(", ")))
    }

    fun setModEnabled(mod: FileMod, on: Boolean) = work(str(R.string.busy_apply_mods)) {
        mods.setEnabled(mod, on) { setDetail(it) }
        refreshMods()
    }

    fun moveMod(mod: FileMod, delta: Int) = work(str(R.string.busy_reorder_mods)) {
        mods.move(mod, delta) { setDetail(it) }
        refreshMods()
    }

    fun deleteMod(mod: FileMod) = work(str(R.string.busy_remove_mod)) {
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
        when {
            _install.value !is InstallState.Installed -> say(str(R.string.msg_install_first))
            !engineAvailable -> say(str(R.string.msg_no_engine))
            else -> getApplication<android.app.Application>().startActivity(
                android.content.Intent(getApplication(), com.dfdx047.dragonrage.engine.GameActivity::class.java)
                    .addFlags(android.content.Intent.FLAG_ACTIVITY_NEW_TASK),
            )
        }
    }

    // ---- helpers

    private fun str(id: Int, vararg args: Any): String = getApplication<Application>().getString(id, *args)

    private fun say(text: String) {
        _messages.tryEmit(text)
    }

    private fun setDetail(text: String) {
        _busy.value = _busy.value?.copy(detail = text)
    }

    private fun work(title: String, block: suspend () -> Unit) {
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
                say(e.message ?: "${str(R.string.msg_error)}: ${e.javaClass.simpleName}")
            } finally {
                _busy.value = null
            }
        }
    }
}
