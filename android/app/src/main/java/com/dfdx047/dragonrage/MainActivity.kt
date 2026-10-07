package com.dfdx047.dragonrage

import android.os.Bundle
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.activity.enableEdgeToEdge
import androidx.activity.viewModels
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
import androidx.compose.material.icons.rounded.Extension
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.LockOpen
import androidx.compose.material.icons.rounded.Settings
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
import com.dfdx047.dragonrage.ui.screens.HomeScreen
import com.dfdx047.dragonrage.ui.screens.ModsScreen
import com.dfdx047.dragonrage.ui.screens.SettingsScreen
import com.dfdx047.dragonrage.ui.screens.TexturesScreen
import com.dfdx047.dragonrage.ui.theme.DragonRageTheme

class MainActivity : ComponentActivity() {
    private val vm: AppViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContent {
            val ui by vm.uiState.collectAsStateWithLifecycle()
            DragonRageTheme(mode = ui.theme, dynamicColor = ui.dynamicColor) {
                DragonRageApp(vm)
            }
        }
    }
}

private enum class Dest(val route: String, val label: String, val icon: ImageVector) {
    HOME("home", "Início", Icons.Rounded.Home),
    TEXTURES("textures", "Texturas", Icons.Rounded.Texture),
    MODS("mods", "Mods", Icons.Rounded.Extension),
    CHEATS("cheats", "Cheats", Icons.Rounded.LockOpen),
    SETTINGS("settings", "Ajustes", Icons.Rounded.Settings),
}

@Composable
private fun DragonRageApp(vm: AppViewModel) {
    val nav = rememberNavController()
    val entry by nav.currentBackStackEntryAsState()
    val current = entry?.destination?.route
    val snackbar = remember { SnackbarHostState() }
    val busy by vm.busy.collectAsStateWithLifecycle()
    val wide = LocalConfiguration.current.screenWidthDp >= 600

    LaunchedEffect(Unit) { vm.messages.collect { snackbar.showSnackbar(it) } }
    // files may have been changed from outside (file manager, adb) while the app was in the background
    LifecycleEventEffect(Lifecycle.Event.ON_RESUME) { vm.refreshAll() }

    fun go(d: Dest) = nav.navigate(d.route) {
        popUpTo(nav.graph.findStartDestination().id) { saveState = true }
        launchSingleTop = true
        restoreState = true
    }

    val colors = MaterialTheme.colorScheme
    Row(Modifier.fillMaxSize().background(colors.background).kiBackground(colors.primary, colors.secondary)) {
        if (wide) {
            NavigationRail(containerColor = Color.Transparent, modifier = Modifier.windowInsetsPadding(WindowInsets.safeDrawing)) {
                Dest.entries.forEach { d ->
                    NavigationRailItem(
                        selected = current == d.route, onClick = { go(d) },
                        icon = { Icon(d.icon, null) }, label = { Text(d.label) },
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
                        NavigationBar(containerColor = colors.surfaceContainer.copy(alpha = 0.92f)) {
                            Dest.entries.forEach { d ->
                                NavigationBarItem(
                                    selected = current == d.route, onClick = { go(d) },
                                    icon = { Icon(d.icon, null) }, label = { Text(d.label, maxLines = 1) },
                                )
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
                    composable(Dest.CHEATS.route) { CheatsScreen(vm) }
                    composable(Dest.SETTINGS.route) { SettingsScreen(vm) }
                }
            }
        }
    }
}
