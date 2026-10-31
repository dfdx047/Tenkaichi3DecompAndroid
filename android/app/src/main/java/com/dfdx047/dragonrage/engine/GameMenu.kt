package com.dfdx047.dragonrage.engine

import android.app.Activity
import android.view.ViewGroup
import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.ExitToApp
import androidx.compose.material.icons.automirrored.rounded.VolumeUp
import androidx.compose.material.icons.rounded.AutoAwesome
import androidx.compose.material.icons.rounded.Dashboard
import androidx.compose.material.icons.rounded.Public
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Speed
import androidx.compose.material.icons.rounded.SportsEsports
import androidx.compose.material.icons.rounded.Star
import androidx.compose.material.icons.rounded.Tv
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilterChip
import androidx.compose.material3.FilterChipDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Slider
import androidx.compose.material3.Surface
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateMapOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.ComposeView
import androidx.compose.ui.platform.ViewCompositionStrategy
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.LifecycleRegistry
import androidx.lifecycle.ViewModelStore
import androidx.lifecycle.ViewModelStoreOwner
import androidx.lifecycle.setViewTreeLifecycleOwner
import androidx.lifecycle.setViewTreeViewModelStoreOwner
import androidx.savedstate.SavedStateRegistry
import androidx.savedstate.SavedStateRegistryController
import androidx.savedstate.SavedStateRegistryOwner
import androidx.savedstate.setViewTreeSavedStateRegistryOwner
import com.dfdx047.dragonrage.data.EngineSettings
import com.dfdx047.dragonrage.data.Game
import com.dfdx047.dragonrage.ui.components.kiBackground
import com.dfdx047.dragonrage.ui.theme.DragonRageTheme
import com.dfdx047.dragonrage.ui.theme.ThemeMode
import kotlinx.coroutines.delay
import kotlin.math.roundToInt

/**
 * The pause menu of the game, drawn by the app over the picture (Dragon Rage 0.9): the phone's own scrolling, switches,
 * sliders and buttons, in the launcher's look (Budokai Tenkaichi 4: its own colours). It replaces the engine's window
 * (ImGui) for touch; that window stays for controllers (START + SELECT) and for the controller's button bindings.
 *
 * The values are the engine's (MenuBridge: a copy made on its thread when the menu opens); a change is sent at once
 * and applied by the engine between two frames, as its own window applies it. The game waits while the menu is up,
 * except in an online match.
 */
class GameMenu(
    private val activity: Activity,
    private val game: Game,
    private val settings: () -> EngineSettings,
    private val actions: Actions,
) {
    interface Actions {
        fun resumed()
        fun quit()
        fun editTouchLayout()
        fun touchOptions()
        fun padButtons()
    }

    private var view: ComposeView? = null
    // one for the screen's whole life: Compose keeps its recomposer on SDL's layout, made with this lifecycle
    private val owner = OverlayOwner()

    val isShown: Boolean get() = view != null

    /** Shows the menu over [parent]; false when the engine did not answer (the old window is used then). */
    fun show(parent: ViewGroup): Boolean {
        if (view != null) return true
        if (!MenuBridge.open(true)) return false
        val o = owner
        // (on the window's root too: Compose looks for them from SDL's layout up, and SDL's activity has none)
        activity.window.decorView.apply {
            setViewTreeLifecycleOwner(o)
            setViewTreeSavedStateRegistryOwner(o)
            setViewTreeViewModelStoreOwner(o)
        }
        val v = ComposeView(activity).apply {
            setViewTreeLifecycleOwner(o)
            setViewTreeSavedStateRegistryOwner(o)
            setViewTreeViewModelStoreOwner(o)
            setViewCompositionStrategy(ViewCompositionStrategy.DisposeOnDetachedFromWindow)
            isClickable = true // (touches stop here: none reaches the game or the on-screen controller)
            isFocusable = true
            setContent {
                DragonRageTheme(mode = ThemeMode.DARK, game = game) {
                    MenuScreen(game, settings, onResume = { close(resume = true) }, actions = actions, closeMenu = { close(resume = false) })
                }
            }
        }
        parent.addView(v, ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT))
        v.requestFocus()
        view = v
        return true
    }

    /** Closes it; [resume]: back to the game (else another screen takes over: the editor, the bindings). */
    fun close(resume: Boolean = true) {
        val v = view ?: return
        view = null
        (v.parent as? ViewGroup)?.removeView(v)
        MenuBridge.open(false)
        if (resume) actions.resumed()
    }

    /** A plain activity (SDL's) has no lifecycle for Compose: this is the menu's own, resumed while it is shown. */
    private class OverlayOwner : LifecycleOwner, SavedStateRegistryOwner, ViewModelStoreOwner {
        private val registry = LifecycleRegistry(this)
        private val saved = SavedStateRegistryController.create(this)
        override val lifecycle: Lifecycle get() = registry
        override val savedStateRegistry: SavedStateRegistry get() = saved.savedStateRegistry
        override val viewModelStore = ViewModelStore()

        init {
            saved.performAttach()
            saved.performRestore(null)
            registry.currentState = Lifecycle.State.RESUMED
        }
    }
}

// ---- the text: the menu's own lines in the three languages of the app (as the engine's window has them)

private class L(lang: String) {
    private val i = when (lang) { "pt" -> 1; "es" -> 2; else -> 0 }
    fun t(en: String, pt: String, es: String) = when (i) { 1 -> pt; 2 -> es; else -> en }
}

private enum class Page { VIDEO, PERFORMANCE, EFFECTS, AUDIO, INTERFACE, CONTROLS, ONLINE, CHEATS }

/** The values shown: the engine's copy when it comes, the settings file until then. */
private class MenuValues(private val file: () -> EngineSettings) {
    val map = mutableStateMapOf<String, Int>()
    var texts by mutableStateOf(mapOf<String, String>())
    var ready by mutableStateOf(false)

    fun get(k: String, d: Int): Int = map[k] ?: d

    fun set(k: String, v: Int) {
        map[k] = v
        MenuBridge.set(k, v)
    }

    suspend fun load() {
        runCatching { map.putAll(file().snapshot()) }
        val start = MenuBridge.gen()
        var waited = 0
        while (MenuBridge.gen() == start && waited < 1500) {
            delay(30)
            waited += 30
        }
        if (MenuBridge.gen() != start) {
            for (k in KEYS) {
                val v = MenuBridge.get(k, Int.MIN_VALUE)
                if (v != Int.MIN_VALUE) map[k] = v
            }
            texts = TEXT_KEYS.associateWith { MenuBridge.text(it) }
        }
        ready = true
    }

    companion object {
        val KEYS = listOf(
            "scale4", "aspect_milli", "filter", "smooth_2d", "texture_pack", "texture_count", "gpu_api", "running_gl",
            "frame_skip", "frag_depth", "interp", "present", "meter", "fx_off", "glow", "music", "effects", "voices",
            "stage_sounds", "music_follows_speed", "hud_layout", "menus_43", "side_bars", "menu_font_id", "battle_font_id",
            "text_font_id", "clash_square", "clash_square_x", "chat", "game_speed", "skip_logos", "skip_intro",
            "skip_copyright", "unlock_done", "online", "fps", "net_spectators", "lobby_match", "lobby_owner", "spectating", "spec_view",
        )
        val TEXT_KEYS = listOf("gpu", "device", "fonts_menu", "fonts_battle", "fonts_text", "lobby_code", "lobby_server", "spec_names")
    }
}

@Composable
private fun MenuScreen(
    game: Game,
    settings: () -> EngineSettings,
    onResume: () -> Unit,
    actions: GameMenu.Actions,
    closeMenu: () -> Unit,
) {
    val context = androidx.compose.ui.platform.LocalContext.current
    val l = remember { L(context.resources.configuration.locales[0].language) }
    val values = remember { MenuValues(settings) }
    LaunchedEffect(Unit) { values.load() }
    val pages = remember(game) {
        if (game == Game.BT4) listOf(Page.VIDEO, Page.PERFORMANCE, Page.EFFECTS, Page.CONTROLS)
        else Page.entries.toList()
    }
    var page by remember { mutableStateOf(Page.VIDEO) }
    var askQuit by remember { mutableStateOf(false) }
    val scheme = MaterialTheme.colorScheme

    Box(
        Modifier.fillMaxSize().background(scheme.background.copy(alpha = 0.94f))
            .kiBackground(scheme.primary, scheme.secondary)
            .clickable(interactionSource = remember { MutableInteractionSource() }, indication = null) {},
    ) {
        androidx.compose.runtime.CompositionLocalProvider(androidx.compose.material3.LocalContentColor provides scheme.onBackground) {
        BoxWithConstraints(Modifier.fillMaxSize().windowInsetsPadding(WindowInsets.safeDrawing).padding(horizontal = 12.dp, vertical = 8.dp)) {
            val wide = maxWidth >= 560.dp
            Column(Modifier.fillMaxSize()) {
                TopBar(l, game, onResume = onResume, onQuit = { askQuit = true })
                Spacer(Modifier.height(8.dp))
                if (game != Game.BT4 && (values.get("lobby_match", 0) != 0 || values.get("spectating", 0) != 0)) {
                    NetBar(l, values, onDone = onResume)
                    Spacer(Modifier.height(8.dp))
                }
                if (wide) {
                    Row(Modifier.fillMaxSize()) {
                        Column(
                            Modifier.width(184.dp).fillMaxHeight().verticalScroll(rememberScrollState()),
                            verticalArrangement = Arrangement.spacedBy(6.dp),
                        ) {
                            pages.forEach { p -> PageButton(pageIcon(p), pageName(l, p), p == page) { page = p } }
                        }
                        Spacer(Modifier.width(12.dp))
                        PageContent(Modifier.weight(1f).fillMaxHeight(), l, game, page, values, actions, closeMenu)
                    }
                } else {
                    Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(6.dp)) {
                        pages.forEach { p ->
                            FilterChip(selected = p == page, onClick = { page = p }, label = { Text(pageName(l, p)) },
                                leadingIcon = { Icon(pageIcon(p), null, Modifier.size(18.dp)) })
                        }
                    }
                    Spacer(Modifier.height(8.dp))
                    PageContent(Modifier.fillMaxSize(), l, game, page, values, actions, closeMenu)
                }
            }
        }
        }
    }
    if (askQuit) {
        AlertDialog(
            onDismissRequest = { askQuit = false },
            title = { Text(l.t("Quit to the launcher?", "Sair para o app?", "¿Salir a la app?")) },
            text = {
                Text(l.t("The game closes and you go back to the app. Progress not saved in the game is lost.",
                    "O jogo fecha e você volta para o app. O progresso que não foi salvo no jogo é perdido.",
                    "El juego se cierra y vuelves a la app. Se pierde el progreso no guardado en el juego."))
            },
            confirmButton = { Button(onClick = { askQuit = false; actions.quit() }) { Text(l.t("Quit", "Sair", "Salir")) } },
            dismissButton = { TextButton(onClick = { askQuit = false }) { Text(l.t("Stay", "Ficar", "Quedarme")) } },
        )
    }
}

@Composable
private fun TopBar(l: L, game: Game, onResume: () -> Unit, onQuit: () -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(
                if (game == Game.BT4) "BUDOKAI TENKAICHI 4" else "DRAGON RAGE",
                style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Black, color = MaterialTheme.colorScheme.tertiary,
                maxLines = 1, overflow = TextOverflow.Ellipsis,
            )
            Text(l.t("PAUSED", "PAUSADO", "EN PAUSA"), style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        OutlinedButton(onClick = onQuit, modifier = Modifier.heightIn(min = 52.dp), contentPadding = PaddingValues(horizontal = 18.dp)) {
            Icon(Icons.AutoMirrored.Rounded.ExitToApp, null, Modifier.size(20.dp))
            Spacer(Modifier.width(8.dp))
            Text(l.t("Quit", "Sair", "Salir"))
        }
        Spacer(Modifier.width(10.dp))
        Button(onClick = onResume, modifier = Modifier.heightIn(min = 52.dp), contentPadding = PaddingValues(horizontal = 24.dp)) {
            Icon(Icons.Rounded.PlayArrow, null, Modifier.size(22.dp))
            Spacer(Modifier.width(8.dp))
            Text(l.t("Resume", "Continuar", "Continuar"), fontWeight = FontWeight.Bold)
        }
    }
}

/** A lobby's match or a watched match: the ways back and out, as the engine's window has them (the PC's at its top). */
@Composable
private fun NetBar(l: L, v: MenuValues, onDone: () -> Unit) {
    val lobby = v.get("lobby_match", 0) != 0
    val owner = v.get("lobby_owner", 0) != 0
    val watching = v.get("spectating", 0) != 0
    val names = v.texts["spec_names"].orEmpty().split('|')
    val code = v.texts["lobby_code"].orEmpty()
    val server = v.texts["lobby_server"].orEmpty()
    fun act(a: Int) { MenuBridge.action(a); onDone() }
    Surface(
        shape = RoundedCornerShape(20.dp),
        color = MaterialTheme.colorScheme.surfaceContainer.copy(alpha = 0.92f),
        contentColor = MaterialTheme.colorScheme.onSurface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f)),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(Modifier.padding(horizontal = 16.dp, vertical = 10.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            val title = if (watching) {
                val view = when (v.get("spec_view", 0)) {
                    0 -> names.getOrNull(0)?.ifEmpty { null } ?: l.t("player 1", "jogador 1", "jugador 1")
                    1 -> names.getOrNull(1)?.ifEmpty { null } ?: l.t("player 2", "jogador 2", "jugador 2")
                    else -> l.t("both (split screen)", "os dois (tela dividida)", "los dos (pantalla dividida)")
                }
                l.t("Watching %s vs %s · View: %s", "Assistindo %s x %s · Câmera: %s", "Viendo %s vs %s · Cámara: %s")
                    .format(names.getOrNull(0).orEmpty(), names.getOrNull(1).orEmpty(), view)
            } else {
                l.t("Lobby", "Lobby", "Lobby") + if (code.isNotEmpty()) "  $code · Server $server" else ""
            }
            Text(title, style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.tertiary, fontWeight = FontWeight.Bold)
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                if (watching) {
                    OutlinedButton(onClick = {
                        MenuBridge.action(MenuBridge.ACTION_SPEC_VIEW)
                        v.map["spec_view"] = (v.get("spec_view", 0) + 1) % 3
                    }) { Text(l.t("Change view", "Trocar câmera", "Cambiar cámara")) }
                }
                if (lobby && owner) {
                    OutlinedButton(onClick = { act(MenuBridge.ACTION_LOBBY_BACK) }) { Text(l.t("Back to the lobby", "Voltar ao lobby", "Volver al lobby")) }
                }
                if (lobby) {
                    OutlinedButton(onClick = { act(MenuBridge.ACTION_LOBBY_LEAVE) }) {
                        Text(if (owner) l.t("Close the lobby", "Fechar o lobby", "Cerrar el lobby") else l.t("Leave the lobby", "Sair do lobby", "Salir del lobby"))
                    }
                } else if (watching) {
                    OutlinedButton(onClick = { act(MenuBridge.ACTION_SPEC_STOP) }) { Text(l.t("Stop watching", "Parar de assistir", "Dejar de ver")) }
                }
            }
        }
    }
}

@Composable
private fun PageButton(icon: ImageVector, name: String, selected: Boolean, onClick: () -> Unit) {
    val scheme = MaterialTheme.colorScheme
    Surface(
        onClick = onClick,
        shape = RoundedCornerShape(16.dp),
        color = if (selected) scheme.primary else scheme.surfaceContainer.copy(alpha = 0.9f),
        contentColor = if (selected) scheme.onPrimary else scheme.onSurface,
        modifier = Modifier.fillMaxWidth().heightIn(min = 54.dp),
    ) {
        Row(Modifier.padding(horizontal = 14.dp, vertical = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            Icon(icon, null, Modifier.size(22.dp))
            Spacer(Modifier.width(12.dp))
            Text(name, style = MaterialTheme.typography.titleSmall, maxLines = 1, overflow = TextOverflow.Ellipsis)
        }
    }
}

private fun pageIcon(p: Page): ImageVector = when (p) {
    Page.VIDEO -> Icons.Rounded.Tv
    Page.PERFORMANCE -> Icons.Rounded.Speed
    Page.EFFECTS -> Icons.Rounded.AutoAwesome
    Page.AUDIO -> Icons.AutoMirrored.Rounded.VolumeUp
    Page.INTERFACE -> Icons.Rounded.Dashboard
    Page.CONTROLS -> Icons.Rounded.SportsEsports
    Page.ONLINE -> Icons.Rounded.Public
    Page.CHEATS -> Icons.Rounded.Star
}

private fun pageName(l: L, p: Page): String = when (p) {
    Page.VIDEO -> l.t("Video", "Vídeo", "Video")
    Page.PERFORMANCE -> l.t("Performance", "Desempenho", "Rendimiento")
    Page.EFFECTS -> l.t("Effects", "Efeitos", "Efectos")
    Page.AUDIO -> l.t("Audio", "Áudio", "Audio")
    Page.INTERFACE -> l.t("Interface", "Interface", "Interfaz")
    Page.CONTROLS -> l.t("Controls", "Controles", "Controles")
    Page.ONLINE -> l.t("Online", "Online", "Online")
    Page.CHEATS -> l.t("Cheats", "Trapaças", "Trucos")
}

// ---- the pages

@Composable
private fun PageContent(
    modifier: Modifier,
    l: L,
    game: Game,
    page: Page,
    v: MenuValues,
    actions: GameMenu.Actions,
    closeMenu: () -> Unit,
) {
    androidx.compose.runtime.key(page) { // (each page starts at its top)
    Column(
        modifier.verticalScroll(rememberScrollState()).padding(bottom = 24.dp),
        verticalArrangement = Arrangement.spacedBy(12.dp),
    ) {
        when (page) {
            Page.VIDEO -> VideoPage(l, game, v)
            Page.PERFORMANCE -> PerformancePage(l, game, v)
            Page.EFFECTS -> EffectsPage(l, v)
            Page.AUDIO -> AudioPage(l, v)
            Page.INTERFACE -> InterfacePage(l, v)
            Page.CONTROLS -> ControlsPage(l, game, v, actions, closeMenu)
            Page.ONLINE -> OnlinePage(l, v)
            Page.CHEATS -> CheatsPage(l, v)
        }
    }
    }
}

@Composable
private fun Card(title: String, content: @Composable ColumnScope.() -> Unit) {
    Surface(
        shape = RoundedCornerShape(20.dp),
        color = MaterialTheme.colorScheme.surfaceContainer.copy(alpha = 0.92f),
        // (a colour with its own alpha is not one of the scheme's: the text colour has to be said, or it is black)
        contentColor = MaterialTheme.colorScheme.onSurface,
        border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.5f)),
        modifier = Modifier.fillMaxWidth(),
    ) {
        Column(Modifier.padding(horizontal = 18.dp, vertical = 16.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text(title, style = MaterialTheme.typography.titleMedium, color = MaterialTheme.colorScheme.tertiary, fontWeight = FontWeight.Bold)
            Box(Modifier.size(width = 40.dp, height = 3.dp).clip(RoundedCornerShape(2.dp)).background(MaterialTheme.colorScheme.primary))
            content()
        }
    }
}

/** A line with a switch: the whole line answers a touch. */
@Composable
private fun SwitchRow(title: String, text: String?, checked: Boolean, onChange: (Boolean) -> Unit) {
    Row(
        Modifier.fillMaxWidth().clip(RoundedCornerShape(12.dp)).clickable { onChange(!checked) }.heightIn(min = 56.dp).padding(vertical = 4.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Column(Modifier.weight(1f).padding(end = 12.dp)) {
            Text(title, style = MaterialTheme.typography.bodyLarge)
            if (!text.isNullOrEmpty()) {
                Text(text, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        Switch(checked = checked, onCheckedChange = onChange)
    }
}

/** A title and big buttons, one per value; the nearest to the value is lit. */
@Composable
private fun Choice(title: String, text: String?, options: List<Pair<String, Int>>, value: Int, onSelect: (Int) -> Unit) {
    val best = options.minByOrNull { kotlin.math.abs(it.second - value) }?.second
    Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
        Text(title, style = MaterialTheme.typography.bodyLarge)
        Row(Modifier.fillMaxWidth().horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            options.forEach { (name, v) ->
                FilterChip(
                    selected = v == best, onClick = { onSelect(v) },
                    label = { Text(name, style = MaterialTheme.typography.labelLarge, modifier = Modifier.padding(vertical = 10.dp)) },
                    colors = FilterChipDefaults.filterChipColors(
                        selectedContainerColor = MaterialTheme.colorScheme.primary,
                        selectedLabelColor = MaterialTheme.colorScheme.onPrimary,
                    ),
                )
            }
        }
        if (!text.isNullOrEmpty()) {
            Text(text, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

/** A slider over whole steps with − and + at its ends; the value goes to the game when the finger lifts. */
@Composable
private fun StepRow(title: String, text: String?, value: Int, range: IntRange, step: Int, unit: String, onChange: (Int) -> Unit) {
    var drag by remember(value) { mutableFloatStateOf(value.toFloat()) }
    fun send(x: Int) = onChange(x.coerceIn(range.first, range.last))
    Column {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Text(title, style = MaterialTheme.typography.bodyLarge, modifier = Modifier.weight(1f))
            Text("${drag.roundToInt()}$unit", style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
        }
        Row(verticalAlignment = Alignment.CenterVertically) {
            TextButton(onClick = { send(value - step) }, modifier = Modifier.size(52.dp), contentPadding = PaddingValues(0.dp)) {
                Text("−", style = MaterialTheme.typography.titleLarge)
            }
            Slider(
                value = drag,
                onValueChange = { drag = it },
                onValueChangeFinished = { send((drag / step).roundToInt() * step) },
                valueRange = range.first.toFloat()..range.last.toFloat(),
                modifier = Modifier.weight(1f),
            )
            TextButton(onClick = { send(value + step) }, modifier = Modifier.size(52.dp), contentPadding = PaddingValues(0.dp)) {
                Text("+", style = MaterialTheme.typography.titleLarge)
            }
        }
        if (!text.isNullOrEmpty()) {
            Text(text, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun Note(text: String) {
    Text(text, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
}

@Composable
private fun BigButton(text: String, primary: Boolean = false, onClick: () -> Unit) {
    if (primary) {
        Button(onClick = onClick, modifier = Modifier.fillMaxWidth().heightIn(min = 54.dp)) { Text(text, fontWeight = FontWeight.Bold) }
    } else {
        OutlinedButton(onClick = onClick, modifier = Modifier.fillMaxWidth().heightIn(min = 54.dp)) { Text(text) }
    }
}

@Composable
private fun VideoPage(l: L, game: Game, v: MenuValues) {
    val gl = v.get("running_gl", 0) != 0
    Card(l.t("Picture", "Imagem", "Imagen")) {
        val scales = listOf(4, 5, 6, 7, 8, 10, 12, 16, 20, 24)
        val s = v.get("scale4", 8)
        Choice(
            l.t("Internal resolution", "Resolução interna", "Resolución interna"),
            l.t("${128 * s} x ${112 * s}. 1x is the PlayStation 2's own. Higher is sharper and heavier for the GPU.",
                "${128 * s} x ${112 * s}. 1x é a do PlayStation 2. Mais alta fica mais nítida e pesa mais na GPU.",
                "${128 * s} x ${112 * s}. 1x es la del PlayStation 2. Más alta se ve más nítida y pesa más en la GPU."),
            scales.map { q -> (if (q % 4 == 0) "${q / 4}x" else "${q / 4.0}x") to q }, s,
        ) { v.set("scale4", it) }
        if (game == Game.BT3) {
            Choice(
                l.t("Aspect ratio", "Proporção", "Relación de aspecto"),
                l.t("Screen: the shape of this device's screen, nothing squeezed. Wider than 4:3 shows more of the fight.",
                    "Tela: o formato da tela do aparelho, sem esticar. Mais largo que 4:3 mostra mais da luta.",
                    "Pantalla: la forma de la pantalla del equipo, sin estirar. Más ancho que 4:3 muestra más de la pelea."),
                listOf(l.t("Screen", "Tela", "Pantalla") to 0, "4:3" to 1333, "16:10" to 1600, "16:9" to 1778, "20:9" to 2222, "21:9" to 2389),
                v.get("aspect_milli", 0),
            ) { v.set("aspect_milli", it) }
        } else {
            Choice(
                l.t("Aspect ratio", "Proporção", "Relación de aspecto"),
                l.t("The mod draws 16:9: 16:9 shows it as made. 4:3 is the PlayStation 2's frame as it is (people look thinner). Screen stretches it to the whole screen.",
                    "O mod desenha em 16:9: 16:9 mostra como foi feito. 4:3 é o quadro do PlayStation 2 como ele é (os personagens ficam mais finos). Tela estica para a tela toda.",
                    "El mod dibuja en 16:9: 16:9 lo muestra como fue hecho. 4:3 es el cuadro del PlayStation 2 tal cual (los personajes se ven más delgados). Pantalla lo estira a toda la pantalla."),
                listOf("16:9" to 0, "4:3" to 1, l.t("Screen", "Tela", "Pantalla") to 2),
                v.get("bt4_aspect", 0),
            ) { v.set("bt4_aspect", it) }
        }
        if (!gl) {
            Choice(
                l.t("Screen filter", "Filtro de tela", "Filtro de pantalla"),
                l.t("How the picture is scaled to the screen. FSR and GSR sharpen; best below 2x.",
                    "Como a imagem é ampliada para a tela. FSR e GSR deixam mais nítido; melhor abaixo de 2x.",
                    "Cómo se amplía la imagen a la pantalla. FSR y GSR la hacen más nítida; mejor debajo de 2x."),
                listOf(l.t("Bilinear", "Bilinear", "Bilineal") to 0, l.t("Sharp", "Nítido", "Nítido") to 1, "FXAA" to 2, "FSR 1" to 3, "Snapdragon GSR" to 4),
                v.get("filter", 0),
            ) { v.set("filter", it) }
        }
        Choice(
            l.t("2D filtering", "Filtro 2D", "Filtro 2D"),
            l.t("The HUD, menus and text above 1x.", "HUD, menus e textos acima de 1x.", "HUD, menús y textos encima de 1x."),
            listOf(l.t("Sharp (console)", "Nítido (console)", "Nítido (consola)") to 0, l.t("Smooth", "Suave", "Suave") to 1),
            v.get("smooth_2d", 1),
        ) { v.set("smooth_2d", it) }
        val count = v.get("texture_count", 0)
        if (count > 0) {
            SwitchRow(
                l.t("Texture pack ($count textures)", "Pacote de texturas ($count texturas)", "Pack de texturas ($count texturas)"), null,
                v.get("texture_pack", 1) != 0,
            ) { v.set("texture_pack", if (it) 1 else 0) }
        }
    }
    Card(l.t("Renderer", "Renderizador", "Renderizador")) {
        Choice(
            l.t("Graphics API", "API gráfica", "API gráfica"),
            l.t("From the next start of the game. If it cannot start, the other one is tried.",
                "Vale a partir da próxima vez que o jogo abrir. Se não iniciar, o outro é tentado.",
                "Vale desde el próximo inicio del juego. Si no arranca, se prueba el otro."),
            listOf("Vulkan" to 0, "OpenGL ES" to 1), v.get("gpu_api", 0),
        ) { v.set("gpu_api", it) }
        if (game == Game.BT4 && com.dfdx047.dragonrage.data.EngineSettings.hasArmsx2Renderer(androidx.compose.ui.platform.LocalContext.current)) {
            Choice(
                l.t("Renderer", "Renderizador", "Renderizador"),
                l.t("ARMSX2 (default): the PlayStation 2 emulator's renderer, the most faithful picture. Dragon Rage (experimental): Budokai Tenkaichi 3's, with faults in some of BT4's scenery. From the next start.",
                    "ARMSX2 (padrão): o renderizador do emulador de PlayStation 2, a imagem mais fiel. Dragon Rage (experimental): o do Budokai Tenkaichi 3, com falhas em parte do cenário do BT4. Vale na próxima vez que o jogo abrir.",
                    "ARMSX2 (predeterminado): el renderizador del emulador de PlayStation 2, la imagen más fiel. Dragon Rage (experimental): el de Budokai Tenkaichi 3, con fallos en parte del escenario de BT4. Desde el próximo inicio."),
                listOf("ARMSX2" to 1, l.t("Dragon Rage (experimental)", "Dragon Rage (experimental)", "Dragon Rage (experimental)") to 0), v.get("bt4_renderer", 1),
            ) { v.set("bt4_renderer", it) }
        }
        val gpu = v.texts["gpu"].orEmpty()
        Note("${l.t("Running now", "Rodando agora", "Ahora")}: ${if (gl) "OpenGL ES" else "Vulkan"}${if (gpu.isNotEmpty()) "  ·  GPU: $gpu" else ""}")
    }
}

@Composable
private fun PerformancePage(l: L, game: Game, v: MenuValues) {
    val gl = v.get("running_gl", 0) != 0
    Card(l.t("Speed", "Velocidade", "Velocidad")) {
        if (game == Game.BT3) {
            SwitchRow(
                l.t("Skip pictures instead of slowing down", "Pular quadros em vez de ficar lento", "Saltar cuadros en vez de ir lento"),
                l.t("When the GPU is behind, the game keeps its speed and leaves a picture out.",
                    "Quando a GPU não dá conta, o jogo mantém a velocidade e pula um quadro.",
                    "Cuando la GPU no alcanza, el juego mantiene la velocidad y salta un cuadro."),
                v.get("frame_skip", 1) != 0,
            ) { v.set("frame_skip", if (it) 1 else 0) }
        }
        SwitchRow(
            l.t("Precise depth", "Profundidade precisa", "Profundidad precisa"),
            l.t("Exact overlap of surfaces in one plane (waterfalls, decals). Off is faster on weak GPUs; rare flicker.",
                "Sobreposição exata de superfícies no mesmo plano (cachoeiras, decalques). Desligado é mais rápido em GPUs fracas; pode piscar raramente.",
                "Superposición exacta de superficies en el mismo plano (cascadas, calcos). Apagado es más rápido en GPUs débiles; puede parpadear."),
            v.get("frag_depth", 0) != 0,
        ) { v.set("frag_depth", if (it) 1 else 0) }
        if (game == Game.BT3) {
            Choice(
                l.t("Smooth motion", "Movimento suave", "Movimiento suave"),
                l.t("Extra pictures between the game's 30. Costs GPU; the fight itself does not change.",
                    "Quadros extras entre os 30 do jogo. Pesa na GPU; a luta em si não muda.",
                    "Cuadros extra entre los 30 del juego. Pesa en la GPU; la pelea no cambia."),
                listOf(l.t("Off (30)", "Desligado (30)", "Apagado (30)") to 0, "60" to 1, "120" to 2), v.get("interp", 0),
            ) { v.set("interp", it) }
        } else {
            SwitchRow(
                l.t("60 fps in fights (experimental)", "60 fps nas lutas (experimental)", "60 fps en las peleas (experimental)"),
                l.t("The fight runs at 60 pictures a second (BT3-Recomp's mode, as in the mod's PC build). Heavy for the CPU: where the device cannot hold 60, it goes back to 30 by itself.",
                    "A luta roda a 60 quadros por segundo (o modo do BT3-Recomp, como na versão de PC do mod). Pesa muito na CPU: se o aparelho não aguentar 60, volta sozinho para 30.",
                    "La pelea va a 60 cuadros por segundo (el modo de BT3-Recomp, como en la versión de PC del mod). Pesa mucho en la CPU: si el equipo no aguanta 60, vuelve solo a 30.") +
                    if (v.get("bt4_fps60_dropped", 0) != 0)
                        "\n" + l.t("This device could not hold 60 fps in BT4's fights; back to 30.",
                            "Este aparelho não aguenta 60 fps nas lutas do BT4; voltando para 30.",
                            "Este equipo no aguanta 60 fps en las peleas de BT4; volviendo a 30.")
                    else "",
                v.get("bt4_fps60", 0) != 0,
            ) { v.set("bt4_fps60", if (it) 1 else 0) }
        }
        if (!gl) {
            Choice(
                l.t("Present mode", "Modo de apresentação", "Modo de presentación"),
                l.t("FIFO: steady, a little more latency. Mailbox: no tearing. Immediate: least latency.",
                    "FIFO: estável, um pouco mais de atraso. Mailbox: sem rasgos. Immediate: menor atraso.",
                    "FIFO: estable, un poco más de retraso. Mailbox: sin cortes. Immediate: menor retraso."),
                listOf(l.t("Automatic", "Automático", "Automático") to 0, "FIFO (VSync)" to 1, "Mailbox" to 2, "Immediate" to 3),
                v.get("present", 0),
            ) { v.set("present", it) }
        }
        if (game == Game.BT3) {
            SwitchRow(
                l.t("Show frame rate", "Mostrar FPS", "Mostrar FPS"),
                l.t("And the connection in an online match.", "E a conexão numa partida online.", "Y la conexión en una partida online."),
                v.get("meter", 0) != 0,
            ) { v.set("meter", if (it) 1 else 0) }
        }
    }
    Card(l.t("Diagnostics", "Diagnóstico", "Diagnóstico")) {
        val s = v.get("scale4", 8)
        Note("GPU: ${v.texts["gpu"].orEmpty()}")
        Note("${l.t("Renderer", "Renderizador", "Renderizador")}: ${if (gl) "OpenGL ES 3.2" else "Vulkan"}")
        Note("${l.t("Resolution", "Resolução", "Resolución")}: ${s / 4.0}x (${128 * s} x ${112 * s})")
        Note("${l.t("Device", "Aparelho", "Equipo")}: ${v.texts["device"].orEmpty().ifEmpty { android.os.Build.MODEL }}")
        Note(l.t("The full log is in the launcher's Debug page (Export log).", "O log completo fica na página Depuração do app (Exportar log).",
            "El log completo está en la página Depuración de la app (Exportar log)."))
    }
}

@Composable
private fun EffectsPage(l: L, v: MenuValues) {
    Card(l.t("PlayStation 2 effects", "Efeitos do PlayStation 2", "Efectos del PlayStation 2")) {
        val names = listOf(
            l.t("Outline", "Contorno", "Contorno"), l.t("See-through tint", "Tom através do cenário", "Tono a través del escenario"),
            l.t("Depth tint", "Névoa de distância", "Niebla de distancia"), l.t("Glare and glow", "Brilho e reflexo", "Brillo y resplandor"),
            l.t("Distance blur", "Desfoque de distância", "Desenfoque de distancia"), l.t("Movement blur", "Desfoque de movimento", "Desenfoque de movimiento"),
        )
        val off = v.get("fx_off", 0)
        names.forEachIndexed { i, name ->
            SwitchRow(name, null, off and (1 shl i) == 0) { on -> v.set("fx_off", if (on) off and (1 shl i).inv() else off or (1 shl i)) }
        }
        StepRow(
            l.t("Glow strength", "Intensidade do brilho", "Intensidad del brillo"),
            l.t("100% is the PlayStation 2's; 60% by default.", "100% é o do PlayStation 2; 60% por padrão.", "100% es el del PlayStation 2; 60% por defecto."),
            v.get("glow", 60), 0..200, 10, "%",
        ) { v.set("glow", it) }
    }
}

@Composable
private fun AudioPage(l: L, v: MenuValues) {
    Card(l.t("Volume", "Volume", "Volumen")) {
        listOf(
            "music" to l.t("Music", "Música", "Música"), "effects" to l.t("Sound effects", "Efeitos sonoros", "Efectos de sonido"),
            "voices" to l.t("Voices", "Vozes", "Voces"), "stage_sounds" to l.t("Stage sounds", "Sons do cenário", "Sonidos del escenario"),
        ).forEach { (k, name) -> StepRow(name, null, v.get(k, 100), 0..200, 10, "%") { v.set(k, it) } }
        SwitchRow(
            l.t("Music follows game speed", "Música acompanha a velocidade", "La música sigue la velocidad"),
            l.t("Outside 100% speed (online, Cheats page).", "Fora de 100% de velocidade (online, página Trapaças).", "Fuera de 100% de velocidad (online, página Trucos)."),
            v.get("music_follows_speed", 0) != 0,
        ) { v.set("music_follows_speed", if (it) 1 else 0) }
    }
}

@Composable
private fun InterfacePage(l: L, v: MenuValues) {
    Card(l.t("Screen layout", "Layout da tela", "Diseño de pantalla")) {
        Choice(
            l.t("HUD in a wide picture", "HUD na tela larga", "HUD en pantalla ancha"), null,
            listOf(l.t("Edges", "Bordas", "Bordes") to 0, l.t("Edges, long bars", "Bordas, barras longas", "Bordes, barras largas") to 1,
                l.t("Middle (4:3)", "Centro (4:3)", "Centro (4:3)") to 2),
            v.get("hud_layout", 0),
        ) { v.set("hud_layout", it) }
        SwitchRow(
            l.t("Menus in 4:3", "Menus em 4:3", "Menús en 4:3"),
            l.t("Menus at their own shape instead of stretched; fights stay wide.", "Menus no formato original em vez de esticados; lutas continuam largas.",
                "Menús en su forma original en vez de estirados; las peleas siguen anchas."),
            v.get("menus_43", 0) != 0,
        ) { v.set("menus_43", if (it) 1 else 0) }
        Choice(
            l.t("Side bars", "Barras laterais", "Barras laterales"),
            l.t("What fills the sides of a 4:3 picture.", "O que preenche as laterais de uma imagem 4:3.", "Lo que llena los lados de una imagen 4:3."),
            listOf(l.t("Black", "Preto", "Negro") to 0, l.t("Border", "Moldura", "Marco") to 1, l.t("Fighter panels", "Painéis dos lutadores", "Paneles de luchadores") to 2),
            v.get("side_bars", 0),
        ) { v.set("side_bars", it) }
    }
    Card(l.t("Fonts", "Fontes", "Fuentes")) {
        listOf(
            Triple("menu_font_id", "fonts_menu", l.t("Menu font", "Fonte dos menus", "Fuente de menús")),
            Triple("battle_font_id", "fonts_battle", l.t("Battle font", "Fonte da luta", "Fuente de pelea")),
            Triple("text_font_id", "fonts_text", l.t("Dialogue font", "Fonte dos diálogos", "Fuente de diálogos")),
        ).forEach { (key, list, title) ->
            val options = v.texts[list].orEmpty().split('|').mapNotNull { e ->
                val id = e.substringBefore(':').toIntOrNull() ?: return@mapNotNull null
                val name = e.substringAfter(':', "").ifEmpty { l.t("Original", "Original", "Original") }
                name to id
            }.ifEmpty { listOf(l.t("Original", "Original", "Original") to 0) }
            Choice(title, null, options, v.get(key, 0)) { v.set(key, it) }
        }
    }
}

@Composable
private fun ControlsPage(l: L, game: Game, v: MenuValues, actions: GameMenu.Actions, closeMenu: () -> Unit) {
    Card(l.t("On-screen controller", "Controle na tela", "Control en pantalla")) {
        Note(l.t("Move, resize and hide the buttons, or change their look and transparency.",
            "Mova, mude o tamanho e esconda os botões, ou troque o visual e a transparência.",
            "Mueve, cambia el tamaño y oculta los botones, o cambia su aspecto y transparencia."))
        BigButton(l.t("Edit the layout", "Editar o layout", "Editar el diseño"), primary = true) { closeMenu(); actions.editTouchLayout() }
        BigButton(l.t("Look, transparency, show or hide", "Visual, transparência, mostrar ou esconder", "Aspecto, transparencia, mostrar u ocultar")) {
            closeMenu(); actions.touchOptions()
        }
    }
    if (game == Game.BT3) {
        Card(l.t("Clashes", "Choques", "Choques")) {
            val cs = v.get("clash_square", 0) != 0
            SwitchRow(
                l.t("Punch button counts in clashes", "Botão de soco conta nos choques", "Botón de golpe cuenta en choques"),
                l.t("In a clash of beams or rushes each press of Square (X on Xbox) counts as a d-pad tap. Works online.",
                    "Num choque de rajadas ou de golpes, cada toque em Quadrado (X no Xbox) conta como um toque no direcional. Funciona online.",
                    "En un choque de rayos o golpes, cada toque de Cuadrado (X en Xbox) cuenta como un toque de la cruceta. Funciona online."),
                cs,
            ) { v.set("clash_square", if (it) 1 else 0) }
            if (cs) {
                Choice(l.t("Worth of a press", "Valor de cada toque", "Valor de cada toque"), null,
                    listOf("1" to 4, "1.5" to 6, "1.75" to 7, "2" to 8), v.get("clash_square_x", 4)) { v.set("clash_square_x", it) }
            }
        }
        Card(l.t("Physical controller", "Controle físico", "Control físico")) {
            Note(l.t("Any wireless or USB controller with the PS2 controller's buttons: cross at the bottom, circle right, square left, triangle top, L1 / R1, L2 / R2, L3 / R3, START, SELECT. A controller the game does not know gets the order of the PS2 adapters.",
                "Qualquer controle sem fio ou USB com os botões do controle de PS2: X embaixo, Círculo à direita, Quadrado à esquerda, Triângulo em cima, L1 / R1, L2 / R2, L3 / R3, START, SELECT. Um controle que o jogo não conhece recebe a ordem dos adaptadores de PS2.",
                "Cualquier control inalámbrico o USB con los botones del control de PS2: X abajo, Círculo a la derecha, Cuadrado a la izquierda, Triángulo arriba, L1 / R1, L2 / R2, L3 / R3, START, SELECT. Un control que el juego no conoce recibe el orden de los adaptadores de PS2."))
            BigButton(l.t("Automatic set-up (PS2 layout)", "Configuração automática (layout de PS2)", "Configuración automática (diseño de PS2)"), primary = true) {
                MenuBridge.action(MenuBridge.ACTION_PAD_AUTO)
            }
            Note(l.t("To change one button, use the game's own window (made for a controller).",
                "Para trocar um botão só, use a janela do próprio jogo (feita para controle).",
                "Para cambiar un solo botón, usa la ventana del propio juego (hecha para control)."))
            BigButton(l.t("Controller buttons", "Botões do controle", "Botones del control")) { closeMenu(); actions.padButtons() }
        }
    }
}

@Composable
private fun OnlinePage(l: L, v: MenuValues) {
    Card(l.t("How online works", "Como o online funciona", "Cómo funciona el online")) {
        Note(l.t(
            "1. In the game's main menu choose Dragon Net Battle (or Tenkaichi Budokai Online for the PC players).\n" +
                "2. Quick match finds an opponent by itself. Create room gives a code; your friend types it in Join.\n" +
                "3. Only button presses travel; both games compute the fight, with rollback for late presses.\n" +
                "4. Both players need the same release and the same game (the USA disc, SLUS-21678).",
            "1. No menu principal do jogo escolha Dragon Net Battle (ou Tenkaichi Budokai Online para jogar com o PC).\n" +
                "2. Partida rápida acha um adversário sozinha. Criar sala gera um código; seu amigo digita em Entrar.\n" +
                "3. Só os botões apertados viajam; os dois jogos calculam a luta, com rollback para toques atrasados.\n" +
                "4. Os dois precisam da mesma versão e do mesmo jogo (o disco americano, SLUS-21678).",
            "1. En el menú principal del juego elige Dragon Net Battle (o Tenkaichi Budokai Online para jugar con PC).\n" +
                "2. Partida rápida busca un rival sola. Crear sala da un código; tu amigo lo escribe en Unirse.\n" +
                "3. Solo viajan los botones pulsados; los dos juegos calculan la pelea, con rollback para toques atrasados.\n" +
                "4. Los dos necesitan la misma versión y el mismo juego (el disco americano, SLUS-21678).",
        ))
    }
    Card(l.t("If it does not connect", "Se não conectar", "Si no conecta")) {
        Note(l.t(
            "- \"Busy\" or \"too many rooms\": the free servers have limits; wait a few minutes.\n" +
                "- \"Different version\": update Dragon Rage.\n" +
                "- Stuck on connecting: try Wi-Fi instead of mobile data (or the other way round), turn off VPN and data saver.\n" +
                "- Still failing: export the log in the launcher (Debug page) and send it on our Discord.",
            "- \"Busy\" ou \"too many rooms\": os servidores gratuitos têm limite; espere alguns minutos.\n" +
                "- \"Different version\": atualize o Dragon Rage.\n" +
                "- Travado em conectando: troque Wi-Fi por dados móveis (ou o contrário), desligue VPN e economia de dados.\n" +
                "- Continua falhando: exporte o log no app (página Depuração) e mande no nosso Discord.",
            "- \"Busy\" o \"too many rooms\": los servidores gratuitos tienen límite; espera unos minutos.\n" +
                "- \"Different version\": actualiza Dragon Rage.\n" +
                "- Se queda conectando: cambia Wi-Fi por datos móviles (o al revés), apaga VPN y ahorro de datos.\n" +
                "- Sigue fallando: exporta el log en la app (página Depuración) y mándalo en nuestro Discord.",
        ))
    }
    Card(l.t("Chat with PC players", "Chat com jogadores de PC", "Chat con jugadores de PC")) {
        SwitchRow(
            l.t("Show the other player's messages", "Mostrar mensagens do outro jogador", "Mostrar mensajes del otro jugador"),
            l.t("Typing needs a keyboard (Bluetooth or USB).", "Para escrever precisa de teclado (Bluetooth ou USB).", "Para escribir hace falta teclado (Bluetooth o USB)."),
            v.get("chat", 1) != 0,
        ) { v.set("chat", if (it) 1 else 0) }
    }
    Card(l.t("Spectators and lobbies", "Espectadores e lobbies", "Espectadores y lobbies")) {
        SwitchRow(
            l.t("Let others watch my matches", "Deixar outros assistirem minhas partidas", "Dejar que otros vean mis partidas"),
            l.t("Up to six people can watch your match, by your room code or your address; they never change the fight.", "Até seis pessoas podem assistir à sua partida, pelo código da sala ou pelo seu endereço; elas nunca mudam a luta.", "Hasta seis personas pueden ver tu partida, por el código de la sala o tu dirección; nunca cambian la pelea."),
            v.get("net_spectators", 1) != 0,
        ) { v.set("net_spectators", if (it) 1 else 0) }
        Note(l.t(
            "- Watch a match: Dragon Net Battle > Join room, type the code and choose Watch the match. View: player 1, player 2 or split screen (R1).\n- Lobby: Dragon Net Battle > Create room > Open a lobby (or Tenkaichi Budokai Online > Lobby). Up to eight people: two fight, the rest watch, with rules, Ready and chat.\n- Quick match, Region row: Auto finds someone nearby first and everyone after 25 s; a region first, everyone after 40 s.\n- The pause menu (Back button) has Back to the lobby, Leave / Close the lobby, Change view and Stop watching when they apply.",
            "- Assistir a uma partida: Dragon Net Battle > Entrar em sala, digite o código e escolha Assistir à partida. Câmera: jogador 1, jogador 2 ou tela dividida (R1).\n- Lobby: Dragon Net Battle > Criar sala > Abrir um lobby (ou Tenkaichi Budokai Online > Lobby). Até oito pessoas: duas lutam e as outras assistem, com regras, Pronto e chat.\n- Partida rápida, linha Região: Auto procura alguém perto primeiro e qualquer um depois de 25 s; uma região primeiro, qualquer um depois de 40 s.\n- O menu de pausa (botão Voltar) tem Voltar ao lobby, Sair / Fechar o lobby, Trocar câmera e Parar de assistir quando valem.",
            "- Ver una partida: Dragon Net Battle > Entrar a sala, escribe el código y elige Ver la partida. Cámara: jugador 1, jugador 2 o pantalla dividida (R1).\n- Lobby: Dragon Net Battle > Crear sala > Abrir un lobby (o Tenkaichi Budokai Online > Lobby). Hasta ocho personas: dos pelean y el resto mira, con reglas, Listo y chat.\n- Partida rápida, fila Región: Auto busca a alguien cerca primero y a cualquiera tras 25 s; una región primero, cualquiera tras 40 s.\n- El menú de pausa (botón Atrás) tiene Volver al lobby, Salir / Cerrar el lobby, Cambiar cámara y Dejar de ver cuando aplican.",
        ))
    }
}

@Composable
private fun CheatsPage(l: L, v: MenuValues) {
    Card(l.t("Game speed", "Velocidade do jogo", "Velocidad del juego")) {
        StepRow(
            l.t("Speed", "Velocidade", "Velocidad"),
            l.t("Outside online play only. 100% is the game's own; 114% is what competitive play uses.",
                "Só fora do online. 100% é a do jogo; 114% é a usada no competitivo.",
                "Solo fuera del online. 100% es la del juego; 114% es la del competitivo."),
            v.get("game_speed", 100), 50..200, 1, "%",
        ) { v.set("game_speed", it) }
        Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            listOf(100, 114, 150).forEach { p ->
                OutlinedButton(onClick = { v.set("game_speed", p) }, modifier = Modifier.heightIn(min = 48.dp)) { Text("$p%") }
            }
        }
    }
    Card(l.t("Unlock everything", "Desbloquear tudo", "Desbloquear todo")) {
        Note(l.t("All characters, stages, music, items and the most Zenni: a debug function the developers left in. It empties your " +
            "records list and becomes permanent when the game saves. Use it from the main menu on.",
            "Todos os personagens, cenários, músicas, itens e o máximo de Zenni: uma função de debug que os desenvolvedores deixaram. " +
                "Ela apaga sua lista de recordes e fica permanente quando o jogo salvar. Use a partir do menu principal.",
            "Todos los personajes, escenarios, música, objetos y el máximo de Zenni: una función de depuración que dejaron los desarrolladores. " +
                "Vacía tu lista de récords y queda permanente cuando el juego guarda. Úsala desde el menú principal."))
        var sure by remember { mutableStateOf(false) }
        var done by remember { mutableIntStateOf(v.get("unlock_done", 0)) }
        if (done != 0) {
            Text(l.t("Done.", "Feito.", "Listo."), color = MaterialTheme.colorScheme.primary, style = MaterialTheme.typography.titleSmall)
        } else if (!sure) {
            BigButton(l.t("Unlock everything...", "Desbloquear tudo...", "Desbloquear todo...")) { sure = true }
        } else {
            Text(l.t("Sure? This cannot be undone once the game saves.", "Certeza? Não dá para desfazer depois que o jogo salvar.",
                "¿Seguro? No se puede deshacer cuando el juego guarde."), color = MaterialTheme.colorScheme.tertiary)
            Row(horizontalArrangement = Arrangement.spacedBy(10.dp)) {
                Button(onClick = { v.set("unlock_all", 1); sure = false; done = 1 }, modifier = Modifier.heightIn(min = 52.dp)) {
                    Text(l.t("Unlock", "Desbloquear", "Desbloquear"))
                }
                OutlinedButton(onClick = { sure = false }, modifier = Modifier.heightIn(min = 52.dp)) { Text(l.t("Cancel", "Cancelar", "Cancelar")) }
            }
        }
    }
    Card(l.t("Game start", "Início do jogo", "Inicio del juego")) {
        listOf(
            "skip_logos" to l.t("Skip logos", "Pular logos", "Saltar logos"),
            "skip_intro" to l.t("Skip intro video", "Pular vídeo de abertura", "Saltar video de apertura"),
            "skip_copyright" to l.t("Skip copyright page", "Pular tela de copyright", "Saltar pantalla de copyright"),
        ).forEach { (k, name) -> SwitchRow(name, null, v.get(k, 0) != 0) { v.set(k, if (it) 1 else 0) } }
        Note(l.t("From the next start of the game.", "Vale na próxima vez que o jogo abrir.", "Vale desde el próximo inicio del juego."))
    }
}

