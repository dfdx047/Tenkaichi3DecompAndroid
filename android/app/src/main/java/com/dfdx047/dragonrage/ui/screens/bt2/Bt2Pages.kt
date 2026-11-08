package com.dfdx047.dragonrage.ui.screens.bt2

import androidx.compose.runtime.setValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.mutableStateOf
import androidx.compose.material3.TextButton
import androidx.compose.material3.AlertDialog
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.DeveloperBoard
import androidx.compose.material.icons.rounded.Extension
import androidx.compose.material.icons.rounded.Folder
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.LockOpen
import androidx.compose.material.icons.rounded.Texture
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.RadioButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.Bt2ViewModel
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.ui.components.ScreenTitle
import com.dfdx047.dragonrage.ui.components.SectionCard

// The BT2 tab's pages besides home, debug and settings: each is BT3's page for what BT2's engine can do.

/** A page that scrolls, with its title. */
@Composable
internal fun Bt2Page(title: String, subtitle: String?, content: @Composable ColumnScope.() -> Unit) {
    Column(
        Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp).padding(bottom = 24.dp),
        verticalArrangement = Arrangement.spacedBy(14.dp),
    ) {
        ScreenTitle(title, subtitle)
        content()
    }
}

/** A setting with its explanation and a switch. */
@Composable
internal fun SwitchRow(title: String, text: String?, checked: Boolean, onChange: (Boolean) -> Unit) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f)) {
            Text(title, style = MaterialTheme.typography.bodyLarge)
            if (text != null) Text(text, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        Spacer(Modifier.size(12.dp))
        Switch(checked = checked, onCheckedChange = onChange)
    }
}

/** HD texture packs in PCSX2's layout (bt2/data/textures), which BT2's renderer replaces textures with. */
@Composable
fun Bt2TexturesScreen(vm: Bt2ViewModel) {
    val packs by vm.texturePacks.collectAsStateWithLifecycle()
    val s by vm.settings.collectAsStateWithLifecycle()
    Bt2Page(stringResource(R.string.bt2_tex_title), stringResource(R.string.bt2_tex_sub)) {
        SectionCard(stringResource(R.string.bt2_tex_title), Icons.Rounded.Texture) {
            SwitchRow(stringResource(R.string.bt2_tex_hd), stringResource(R.string.bt2_next_start), s.hdTextures) { on -> vm.update { it.copy(hdTextures = on) } }
            if (packs.isEmpty()) {
                Text(stringResource(R.string.bt2_tex_none), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                Text(stringResource(R.string.bt2_tex_found, packs.first()), style = MaterialTheme.typography.bodyMedium)
                if (packs.size > 1) Text(packs.drop(1).joinToString(", "), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        SectionCard(stringResource(R.string.bt2_tex_where), Icons.Rounded.Folder, accent = MaterialTheme.colorScheme.secondary) {
            Text(stringResource(R.string.bt2_tex_how), style = MaterialTheme.typography.bodySmall)
            Text(vm.texturesFolder.path, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

/** BT2's engine reads the disc image itself and has no file overrides: what works instead. */
@Composable
fun Bt2ModsScreen() {
    Bt2Page(stringResource(R.string.nav_mods), null) {
        SectionCard(stringResource(R.string.bt2_mods_title), Icons.Rounded.Extension) {
            Text(stringResource(R.string.bt2_mods_text), style = MaterialTheme.typography.bodyMedium)
        }
    }
}

/** The game's own patches BT2 has (ee/game_patches.h) and its 60 fps fights; no cheat codes yet. */
@Composable
fun Bt2CheatsScreen(vm: Bt2ViewModel) {
    val s by vm.settings.collectAsStateWithLifecycle()
    val unlocked by vm.unlocked.collectAsStateWithLifecycle()
    var confirm by remember { mutableStateOf(false) }
    Bt2Page(stringResource(R.string.bt2_cheats_title), stringResource(R.string.bt2_next_start)) {
        // Unlock everything: a 100% save on a card of its own (Bt2ViewModel.setUnlock), as BT3's page offers it.
        SectionCard(stringResource(R.string.bt2_unlock_title), Icons.Rounded.LockOpen) {
            if (vm.unlockAvailable) {
                SwitchRow(stringResource(R.string.bt2_unlock_title), stringResource(R.string.bt2_unlock_text), unlocked) { on ->
                    if (on) confirm = true else vm.setUnlock(false)
                }
            } else {
                Text(stringResource(R.string.bt2_unlock_missing), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
        SectionCard(stringResource(R.string.bt2_cheats_title), Icons.Rounded.LockOpen) {
            SwitchRow(stringResource(R.string.bt2_patch_widescreen), stringResource(R.string.bt2_patch_widescreen_text), s.widescreen) { on -> vm.update { it.copy(widescreen = on) } }
            SwitchRow(stringResource(R.string.bt2_patch_60), stringResource(R.string.bt2_patch_60_text), s.sixtyFps) { on -> vm.update { it.copy(sixtyFps = on) } }
            Text(stringResource(R.string.bt2_cheats_none), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
    if (confirm) {
        AlertDialog(
            onDismissRequest = { confirm = false },
            title = { Text(stringResource(R.string.bt2_unlock_q)) },
            text = { Text(stringResource(R.string.bt2_unlock_q_text)) },
            confirmButton = { TextButton(onClick = { confirm = false; vm.setUnlock(true) }) { Text(stringResource(R.string.unlock)) } },
            dismissButton = { TextButton(onClick = { confirm = false }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}

/** BT2's own GPU driver choice (Bt2Drivers): the app's installed drivers, of which BT3's choice is unaffected. */
@Composable
fun Bt2DriversScreen(vm: Bt2ViewModel) {
    val list by vm.gpuDrivers.collectAsStateWithLifecycle()
    val chosen by vm.gpuDriver.collectAsStateWithLifecycle()
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> uri?.let { vm.importDriver(it) } }
    Bt2Page(stringResource(R.string.nav_drivers), stringResource(R.string.bt2_drivers_note)) {
        SectionCard(stringResource(R.string.bt2_drivers_title), Icons.Rounded.DeveloperBoard) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                RadioButton(selected = chosen == null, onClick = { vm.selectDriver(null) })
                Text(stringResource(R.string.bt2_drivers_system), style = MaterialTheme.typography.bodyLarge)
            }
            list.forEach { d ->
                Row(verticalAlignment = Alignment.CenterVertically) {
                    RadioButton(selected = chosen == d.id, onClick = { vm.selectDriver(d.id) })
                    Column {
                        Text(d.name, style = MaterialTheme.typography.bodyLarge)
                        if (d.version.isNotEmpty()) Text(d.version, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                }
            }
            FilledTonalButton(onClick = { pick.launch(arrayOf("*/*")) }) { Text(stringResource(R.string.bt2_drivers_import)) }
        }
    }
}
