package com.dfdx047.dragonrage

import android.app.Application
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
    private val installer = GameDataInstaller(paths, resolver)
    private val packs = TexturePacks(paths, resolver)
    private val mods = Mods(paths, resolver)
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
        }
    }

    // ---- game data

    fun installFromIso(uri: Uri) {
        if (installJob?.isActive == true) return
        installJob = viewModelScope.launch {
            try {
                _install.value = installer.install(uri) { _install.value = it }
                say("Dados do jogo prontos!")
            } catch (e: CancellationException) {
                withContext(Dispatchers.IO) { installer.uninstall() }
                _install.value = InstallState.NotInstalled
                throw e
            } catch (e: GameDataInstaller.Stop) {
                _install.value = InstallState.Failed(e.message ?: "Erro")
            } catch (e: Exception) {
                _install.value = InstallState.Failed("Erro ao ler a imagem: ${e.message ?: e.javaClass.simpleName}")
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

    fun importTexturePack(uri: Uri) = work("Importando pack de texturas") {
        val p = packs.import(uri) { setDetail(it) }
        _packs.value = packs.list()
        say("Pack \"${p.name}\" instalado: ${p.textures} texturas")
    }

    fun setPackEnabled(pack: TexturePack, on: Boolean) = work(if (on) "Ativando pack" else "Desativando pack") {
        packs.setEnabled(pack, on)
        _packs.value = packs.list()
    }

    fun deletePack(pack: TexturePack) = work("Removendo pack") {
        packs.delete(pack)
        _packs.value = packs.list()
        say("Pack \"${pack.name}\" removido")
    }

    // ---- mods, stages, songs

    fun importMod(uris: List<Uri>) = work("Importando mods") {
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
        say(listOfNotNull(
            mods0.takeIf { it > 0 }?.let { "$it mod(s)" },
            stages0.takeIf { it > 0 }?.let { "$it estágio(s)" },
            songs0.takeIf { it > 0 }?.let { "$it música(s)" },
        ).joinToString(", ", prefix = "Adicionado: "))
    }

    fun setModEnabled(mod: FileMod, on: Boolean) = work("Aplicando mods") {
        mods.setEnabled(mod, on) { setDetail(it) }
        refreshMods()
    }

    fun moveMod(mod: FileMod, delta: Int) = work("Reordenando mods") {
        mods.move(mod, delta) { setDetail(it) }
        refreshMods()
    }

    fun deleteMod(mod: FileMod) = work("Removendo mod") {
        mods.delete(mod) { setDetail(it) }
        refreshMods()
        say("Mod \"${mod.name}\" removido")
    }

    fun renameExtra(kind: ExtraKind, item: ExtraItem, name: String) = work("Renomeando") {
        mods.rename(kind, item, name)
        refreshMods()
    }

    fun deleteExtra(kind: ExtraKind, item: ExtraItem) = work("Removendo") {
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
    fun setDynamicColor(on: Boolean) = ui.setDynamic(on)

    // ---- launch

    fun play() {
        when {
            _install.value !is InstallState.Installed -> say("Instale os dados do jogo primeiro (a ISO do BT3 USA).")
            !engineAvailable -> say("O motor do jogo ainda não está incluído nesta versão do Dragon Rage.")
            else -> say("Iniciando…") // the SDL activity of the engine is started here once it exists
        }
    }

    // ---- helpers

    private fun say(text: String) {
        _messages.tryEmit(text)
    }

    private fun setDetail(text: String) {
        _busy.value = _busy.value?.copy(detail = text)
    }

    private fun work(title: String, block: suspend () -> Unit) {
        if (_busy.value != null) {
            say("Aguarde a operação atual terminar.")
            return
        }
        viewModelScope.launch(Dispatchers.IO) {
            _busy.value = Busy(title)
            try {
                block()
            } catch (e: CancellationException) {
                throw e
            } catch (e: Exception) {
                say(e.message ?: "Erro: ${e.javaClass.simpleName}")
            } finally {
                _busy.value = null
            }
        }
    }
}
