package com.dfdx047.dragonrage

import android.app.Application
import android.app.LocaleManager
import android.os.Build
import android.os.LocaleList
import android.net.Uri
import androidx.lifecycle.AndroidViewModel
import androidx.lifecycle.viewModelScope
import com.dfdx047.dragonrage.data.EngineSettings
import com.dfdx047.dragonrage.data.ExtraItem
import com.dfdx047.dragonrage.data.ExtraKind
import com.dfdx047.dragonrage.data.FileMod
import com.dfdx047.dragonrage.data.GameDataInstaller
import com.dfdx047.dragonrage.data.GamePaths
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
