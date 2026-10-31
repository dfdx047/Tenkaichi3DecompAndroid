package com.dfdx047.dragonrage

import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import com.dfdx047.dragonrage.data.Game
import com.dfdx047.dragonrage.ui.screens.bt2.Bt2HomeScreen
import com.dfdx047.dragonrage.ui.screens.bt2.Bt2TexturesScreen
import com.dfdx047.dragonrage.ui.screens.bt2.Bt2ModsScreen
import com.dfdx047.dragonrage.ui.screens.bt2.Bt2ControlsScreen
import com.dfdx047.dragonrage.ui.screens.bt2.Bt2CheatsScreen
import com.dfdx047.dragonrage.ui.screens.bt2.Bt2DriversScreen
import com.dfdx047.dragonrage.ui.screens.bt2.Bt2DebugScreen
import com.dfdx047.dragonrage.ui.screens.bt2.Bt2SettingsScreen
import android.os.Bundle
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.verticalScroll
import androidx.lifecycle.ViewModelProvider
import com.dfdx047.dragonrage.data.UiPrefs
import com.dfdx047.dragonrage.ui.screens.WelcomeScreen
import androidx.compose.material.icons.rounded.SwapHoriz
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.BugReport
import androidx.compose.material.icons.rounded.DeveloperBoard
import androidx.compose.material.icons.rounded.Extension
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.LockOpen
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material.icons.rounded.SportsEsports
import androidx.compose.material.icons.rounded.Texture
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.NavigationRail
import androidx.compose.material3.NavigationRailItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.SnackbarHost
import androidx.compose.material3.SnackbarHostState
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import com.dfdx047.dragonrage.R
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.unit.dp
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.compose.LifecycleEventEffect
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.navigation.NavGraph.Companion.findStartDestination
import androidx.navigation.compose.NavHost
import androidx.navigation.compose.composable
import androidx.navigation.compose.currentBackStackEntryAsState
import androidx.navigation.compose.rememberNavController
import com.dfdx047.dragonrage.ui.components.BusyBar
import com.dfdx047.dragonrage.ui.components.kiBackground
import com.dfdx047.dragonrage.ui.screens.CheatsScreen
import com.dfdx047.dragonrage.ui.screens.ControlsScreen
import com.dfdx047.dragonrage.ui.screens.HomeScreen
import com.dfdx047.dragonrage.ui.screens.ModsScreen
import com.dfdx047.dragonrage.ui.screens.SettingsScreen
import com.dfdx047.dragonrage.ui.screens.TexturesScreen
import com.dfdx047.dragonrage.ui.screens.UpdateDialog
import com.dfdx047.dragonrage.ui.screens.DebugScreen
import com.dfdx047.dragonrage.ui.screens.DriversScreen
import com.dfdx047.dragonrage.ui.screens.GpuOfferDialog
import com.dfdx047.dragonrage.ui.screens.LastRunDialog
import com.dfdx047.dragonrage.ui.screens.TexFailDialog
import androidx.compose.foundation.clickable
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBars
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Surface
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.style.TextOverflow
import com.dfdx047.dragonrage.ui.theme.DragonRageTheme

class MainActivity : ComponentActivity() {
    private val bt2: Bt2ViewModel by viewModels() // (Budokai Tenkaichi 2's own: its folder, settings and pages)

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        val prefs = UiPrefs.get(this)
        setContent {
            // the welcome screen first (which game), then that game's pages with its own view model, folder and colours
            var chosen by rememberSaveable { mutableStateOf<String?>(null) }
            val ui by prefs.state.collectAsStateWithLifecycle()
            val game = chosen?.let { Game.of(it) }
            DragonRageTheme(mode = ui.theme, dynamicColor = ui.dynamicColor, game = game ?: Game.BT3) {
                if (game == null) {
                    WelcomeScreen(last = prefs.game) { g -> prefs.setGame(g); chosen = g.id }
                } else if (game == Game.BT2) {
                    // BT2 has no AppViewModel: its pages, view model and files (bt2/) are apart from BT3's and BT4's
                    BackHandler { chosen = null }
                    Bt2App(bt2, onSwitchGame = { chosen = null })
                } else {
                    val vm = remember(game) {
                        ViewModelProvider(this@MainActivity, AppViewModel.factory(application, game))["game-${game.id}", AppViewModel::class.java]
                    }
                    BackHandler { chosen = null } // (back: to the welcome screen)
                    DragonRageApp(vm, onSwitchGame = { chosen = null })
                }
            }
        }
    }

    // A controller (Bluetooth or USB) is the game's only, as a PS2 pad: the launcher ignores it (touch drives it).
    private fun fromController(source: Int): Boolean =
        source and InputDevice.SOURCE_GAMEPAD == InputDevice.SOURCE_GAMEPAD ||
            source and InputDevice.SOURCE_JOYSTICK == InputDevice.SOURCE_JOYSTICK

    override fun dispatchKeyEvent(event: KeyEvent): Boolean =
        if (KeyEvent.isGamepadButton(event.keyCode) || fromController(event.source)) true else super.dispatchKeyEvent(event)

    override fun dispatchGenericMotionEvent(ev: MotionEvent): Boolean =
        if (fromController(ev.source)) true else super.dispatchGenericMotionEvent(ev)
}

private enum class Dest(val route: String, val label: Int, val icon: ImageVector, val bt4: Boolean = true) {
    HOME("home", R.string.nav_home, Icons.Rounded.Home),
    TEXTURES("textures", R.string.nav_textures, Icons.Rounded.Texture),
    MODS("mods", R.string.nav_mods, Icons.Rounded.Extension, bt4 = false),
    CONTROLS("controls", R.string.nav_controls, Icons.Rounded.SportsEsports),
    CHEATS("cheats", R.string.nav_cheats, Icons.Rounded.LockOpen, bt4 = false),
    DRIVERS("drivers", R.string.nav_drivers, Icons.Rounded.DeveloperBoard, bt4 = false),
    DEBUG("debug", R.string.nav_debug, Icons.Rounded.BugReport),
    SETTINGS("settings", R.string.nav_settings, Icons.Rounded.Settings),
}

@Composable
private fun DragonRageApp(vm: AppViewModel, onSwitchGame: () -> Unit) {
    // Budokai Tenkaichi 4 has no mods, cheats or drivers pages (yet): those are the BT3 engine's
    val pages = Dest.entries.filter { vm.game == Game.BT3 || it.bt4 }
    val nav = rememberNavController()
    val entry by nav.currentBackStackEntryAsState()
    val current = entry?.destination?.route
    val snackbar = remember { SnackbarHostState() }
    val busy by vm.busy.collectAsStateWithLifecycle()
    val wide = LocalConfiguration.current.screenWidthDp >= 600

    LaunchedEffect(Unit) { vm.messages.collect { snackbar.showSnackbar(it) } }
    // files may have been changed from outside (file manager, adb) while the app was in the background
    LifecycleEventEffect(Lifecycle.Event.ON_RESUME) { vm.refreshAll(); vm.resumeUpdate(); vm.checkLastRun() }
    UpdateDialog(vm)
    GpuOfferDialog(vm)
    LastRunDialog(vm)
    TexFailDialog(vm)

    fun go(d: Dest) = nav.navigate(d.route) {
        popUpTo(nav.graph.findStartDestination().id) { saveState = true }
        launchSingleTop = true
        restoreState = true
    }

    val colors = MaterialTheme.colorScheme
    Row(Modifier.fillMaxSize().background(colors.background).kiBackground(colors.primary, colors.secondary)) {
        if (wide) {
            NavigationRail(containerColor = Color.Transparent, modifier = Modifier.windowInsetsPadding(WindowInsets.safeDrawing)) {
                // nine pages (BT3) are taller than a 1080-pixel landscape screen: the rail scrolls
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    pages.forEach { d ->
                        NavigationRailItem(
                            selected = current == d.route, onClick = { go(d) },
                            icon = { Icon(d.icon, null) }, label = { Text(stringResource(d.label)) },
                        )
                    }
                    NavigationRailItem(
                        selected = false, onClick = onSwitchGame,
                        icon = { Icon(Icons.Rounded.SwapHoriz, null) }, label = { Text(stringResource(R.string.nav_switch_game)) },
                    )
                }
            }
        }
        Scaffold(
            containerColor = Color.Transparent,
            snackbarHost = { SnackbarHost(snackbar) },
            bottomBar = {
                Column {
                    BusyBar(busy)
                    if (!wide) {
                        // eight pages: a bar that scrolls sideways when the screen is too narrow for all of them
                        Surface(color = colors.surfaceContainer.copy(alpha = 0.94f)) {
                            Row(
                                Modifier.fillMaxWidth().horizontalScroll(rememberScrollState())
                                    .windowInsetsPadding(WindowInsets.navigationBars).padding(horizontal = 4.dp, vertical = 6.dp),
                                horizontalArrangement = Arrangement.SpaceEvenly,
                            ) {
                                pages.forEach { d -> BarItem(d.icon, d.label, current == d.route) { go(d) } }
                                BarItem(Icons.Rounded.SwapHoriz, R.string.nav_switch_game, false, onSwitchGame)
                            }
                        }
                    }
                }
            },
        ) { pad ->
            Box(Modifier.fillMaxSize().padding(pad), contentAlignment = Alignment.TopCenter) {
                NavHost(
                    nav, startDestination = Dest.HOME.route,
                    modifier = Modifier.widthIn(max = 760.dp).fillMaxSize(),
                ) {
                    composable(Dest.HOME.route) { HomeScreen(vm) }
                    composable(Dest.TEXTURES.route) { TexturesScreen(vm) }
                    composable(Dest.MODS.route) { ModsScreen(vm) }
                    composable(Dest.CONTROLS.route) { ControlsScreen(vm) }
                    composable(Dest.CHEATS.route) { CheatsScreen(vm) }
                    composable(Dest.DRIVERS.route) { DriversScreen(vm) }
                    composable(Dest.DEBUG.route) { DebugScreen(vm) }
                    composable(Dest.SETTINGS.route) { SettingsScreen(vm) }
                }
            }
        }
    }
}

/**
 * Budokai Tenkaichi 2's pages (ui/screens/bt2), over their own Bt2ViewModel. Nothing here reads or writes BT3's or
 * BT4's folders or settings: what is set on these pages is BT2's alone (<root>/bt2/).
 */
@Composable
private fun Bt2App(bt2: Bt2ViewModel, onSwitchGame: () -> Unit) {
    val nav = rememberNavController()
    val entry by nav.currentBackStackEntryAsState()
    val current = entry?.destination?.route
    val snackbar = remember { SnackbarHostState() }
    val wide = LocalConfiguration.current.screenWidthDp >= 600

    LaunchedEffect(Unit) { bt2.messages.collect { snackbar.showSnackbar(it) } }
    // files may have been changed from outside (file manager, adb) while the app was in the background
    LifecycleEventEffect(Lifecycle.Event.ON_RESUME) { bt2.refresh() }

    fun go(d: Dest) = nav.navigate(d.route) {
        popUpTo(nav.graph.findStartDestination().id) { saveState = true }
        launchSingleTop = true
        restoreState = true
    }

    val colors = MaterialTheme.colorScheme
    Row(Modifier.fillMaxSize().background(colors.background).kiBackground(colors.primary, colors.secondary)) {
        if (wide) {
            NavigationRail(containerColor = Color.Transparent, modifier = Modifier.windowInsetsPadding(WindowInsets.safeDrawing)) {
                Column(Modifier.verticalScroll(rememberScrollState())) {
                    Dest.entries.forEach { d ->
                        NavigationRailItem(
                            selected = current == d.route, onClick = { go(d) },
                            icon = { Icon(d.icon, null) }, label = { Text(stringResource(d.label)) },
                        )
                    }
                    NavigationRailItem(
                        selected = false, onClick = onSwitchGame,
                        icon = { Icon(Icons.Rounded.SwapHoriz, null) }, label = { Text(stringResource(R.string.nav_switch_game)) },
                    )
                }
            }
        }
        Scaffold(
            containerColor = Color.Transparent,
            snackbarHost = { SnackbarHost(snackbar) },
            bottomBar = {
                Column {
                    if (!wide) {
                        Surface(color = colors.surfaceContainer.copy(alpha = 0.94f)) {
                            Row(
                                Modifier.fillMaxWidth().horizontalScroll(rememberScrollState())
                                    .windowInsetsPadding(WindowInsets.navigationBars).padding(horizontal = 4.dp, vertical = 6.dp),
                                horizontalArrangement = Arrangement.SpaceEvenly,
                            ) {
                                Dest.entries.forEach { d -> BarItem(d.icon, d.label, current == d.route) { go(d) } }
                                BarItem(Icons.Rounded.SwapHoriz, R.string.nav_switch_game, false, onSwitchGame)
                            }
                        }
                    }
                }
            },
        ) { pad ->
            NavHost(
                nav, startDestination = Dest.HOME.route,
                modifier = Modifier.padding(pad).widthIn(max = 760.dp).fillMaxSize(),
            ) {
                composable(Dest.HOME.route) { Bt2HomeScreen(bt2) }
                composable(Dest.TEXTURES.route) { Bt2TexturesScreen(bt2) }
                composable(Dest.MODS.route) { Bt2ModsScreen() }
                composable(Dest.CONTROLS.route) { Bt2ControlsScreen(bt2) }
                composable(Dest.CHEATS.route) { Bt2CheatsScreen(bt2) }
                composable(Dest.DRIVERS.route) { Bt2DriversScreen(bt2) }
                composable(Dest.DEBUG.route) { Bt2DebugScreen(bt2) }
                composable(Dest.SETTINGS.route) { Bt2SettingsScreen(bt2) }
            }
        }
    }
}

/** One page of the bottom bar: its icon in a pill that lights when it is the page shown, and its name. */
@Composable
private fun BarItem(icon: ImageVector, label: Int, selected: Boolean, onClick: () -> Unit) {
    val colors = MaterialTheme.colorScheme
    Column(
        Modifier.width(76.dp).clip(RoundedCornerShape(14.dp)).clickable(onClick = onClick).padding(vertical = 4.dp),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Box(
            Modifier.clip(RoundedCornerShape(16.dp))
                .background(if (selected) colors.primary.copy(alpha = 0.22f) else Color.Transparent)
                .padding(horizontal = 16.dp, vertical = 4.dp),
        ) {
            Icon(icon, null, tint = if (selected) colors.primary else colors.onSurfaceVariant, modifier = Modifier.size(24.dp))
        }
        Text(
            stringResource(label), maxLines = 1, overflow = TextOverflow.Ellipsis,
            style = MaterialTheme.typography.labelSmall, color = if (selected) colors.primary else colors.onSurfaceVariant,
        )
    }
}
