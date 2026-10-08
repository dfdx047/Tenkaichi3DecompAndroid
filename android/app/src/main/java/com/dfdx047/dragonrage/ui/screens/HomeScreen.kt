package com.dfdx047.dragonrage.ui.screens

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Album
import androidx.compose.material.icons.rounded.CheckCircle
import androidx.compose.material.icons.rounded.Close
import androidx.compose.material.icons.rounded.Memory
import androidx.compose.material.icons.rounded.PlayArrow
import androidx.compose.material.icons.rounded.Warning
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import com.dfdx047.dragonrage.R
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.CompositingStrategy
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.data.InstallState
import com.dfdx047.dragonrage.data.formatBytes
import com.dfdx047.dragonrage.ui.components.DragonBall
import com.dfdx047.dragonrage.ui.components.SectionCard
import com.dfdx047.dragonrage.ui.components.StatPill
import com.dfdx047.dragonrage.ui.theme.Dbz
import com.dfdx047.dragonrage.ui.theme.TitleStyle

@Composable
fun HomeScreen(vm: AppViewModel) {
    val install by vm.install.collectAsStateWithLifecycle()
    val packs by vm.texturePacks.collectAsStateWithLifecycle()
    val mods by vm.fileMods.collectAsStateWithLifecycle()
    val stages by vm.stages.collectAsStateWithLifecycle()
    val songs by vm.songs.collectAsStateWithLifecycle()
    val pickIso = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) vm.installFromIso(uri)
    }
    var confirmRemove by remember { mutableStateOf(false) }

    Column(
        Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp).padding(bottom = 24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Hero()

        val installed = install is InstallState.Installed
        Button(
            onClick = vm::play,
            enabled = installed,
            modifier = Modifier.fillMaxWidth().height(64.dp),
            shape = MaterialTheme.shapes.large,
            colors = ButtonDefaults.buttonColors(containerColor = MaterialTheme.colorScheme.primary),
        ) {
            Icon(Icons.Rounded.PlayArrow, null, Modifier.size(30.dp))
            Spacer(Modifier.size(8.dp))
            Text(stringResource(R.string.play), style = MaterialTheme.typography.headlineSmall)
        }

        SectionCard(stringResource(R.string.game_data), Icons.Rounded.Album) {
            when (val s = install) {
                InstallState.NotInstalled -> {
                    Text(
                        stringResource(R.string.game_data_intro),
                        style = MaterialTheme.typography.bodyMedium,
                        color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                    FilledTonalButton(onClick = { pickIso.launch(arrayOf("*/*")) }) { Text(stringResource(R.string.pick_iso)) }
                }
                is InstallState.Working -> {
                    Text(s.step, style = MaterialTheme.typography.bodyLarge)
                    LinearProgressIndicator(
                        progress = { s.progress.coerceIn(0f, 1f) },
                        modifier = Modifier.fillMaxWidth(),
                        color = MaterialTheme.colorScheme.tertiary,
                    )
                    Text(s.detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    OutlinedButton(onClick = vm::cancelInstall) {
                        Icon(Icons.Rounded.Close, null, Modifier.size(18.dp)); Spacer(Modifier.size(6.dp)); Text(stringResource(R.string.cancel))
                    }
                }
                is InstallState.Installed -> {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(Icons.Rounded.CheckCircle, null, tint = Dbz.ShenronGreen)
                        Spacer(Modifier.size(8.dp))
                        Text(stringResource(R.string.game_data_ok, s.files, formatBytes(s.bytes)), style = MaterialTheme.typography.bodyMedium)
                    }
                    TextButton(onClick = { confirmRemove = true }) { Text(stringResource(R.string.remove_game_data)) }
                }
                is InstallState.Failed -> {
                    Row(verticalAlignment = Alignment.Top) {
                        Icon(Icons.Rounded.Warning, null, tint = MaterialTheme.colorScheme.error)
                        Spacer(Modifier.size(8.dp))
                        Text(s.message, style = MaterialTheme.typography.bodyMedium)
                    }
                    Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                        FilledTonalButton(onClick = { pickIso.launch(arrayOf("*/*")) }) { Text(stringResource(R.string.pick_other_iso)) }
                        TextButton(onClick = vm::dismissInstallError) { Text(stringResource(R.string.close)) }
                    }
                }
            }
        }

        SectionCard(stringResource(R.string.engine), Icons.Rounded.Memory, accent = MaterialTheme.colorScheme.secondary) {
            if (vm.engineAvailable) {
                Text(stringResource(R.string.engine_included), style = MaterialTheme.typography.bodyMedium)
            } else {
                Text(
                    stringResource(R.string.engine_missing),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
            StatPill(stringResource(R.string.stat_textures), "${packs.count { it.enabled }}", Dbz.SaiyanGold, Modifier.weight(1f))
            StatPill(stringResource(R.string.stat_mods), "${mods.count { it.enabled }}", MaterialTheme.colorScheme.primary, Modifier.weight(1f))
            StatPill(stringResource(R.string.stat_stages), "${stages.size}", Dbz.KiBlue, Modifier.weight(1f))
            StatPill(stringResource(R.string.stat_songs), "${songs.size}", Dbz.ShenronGreen, Modifier.weight(1f))
        }
    }

    if (confirmRemove) {
        AlertDialog(
            onDismissRequest = { confirmRemove = false },
            title = { Text(stringResource(R.string.remove_game_data_q)) },
            text = { Text(stringResource(R.string.remove_game_data_text)) },
            confirmButton = { TextButton(onClick = { confirmRemove = false; vm.removeGameData() }) { Text(stringResource(R.string.remove)) } },
            dismissButton = { TextButton(onClick = { confirmRemove = false }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}

@Composable
private fun Hero() {
    Box(Modifier.fillMaxWidth().padding(top = 12.dp), contentAlignment = Alignment.CenterStart) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            DragonBall(stars = 4, size = 120.dp)
            Column(Modifier.padding(start = 4.dp)) {
                val gold = Brush.verticalGradient(listOf(Dbz.BallLight, Dbz.SaiyanGold, Dbz.GiOrange))
                Text(
                    "DRAGON\nRAGE",
                    style = TitleStyle.copy(lineHeight = TitleStyle.fontSize * 0.95f),
                    modifier = Modifier
                        .graphicsLayer(compositingStrategy = CompositingStrategy.Offscreen)
                        .drawWithContent {
                            drawContent()
                            drawRect(gold, blendMode = BlendMode.SrcIn)
                        },
                )
                Text(
                    stringResource(R.string.tagline),
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}
