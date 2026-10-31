package com.dfdx047.dragonrage.ui.screens.bt2

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.background
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
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
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
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
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
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawWithContent
import androidx.compose.ui.graphics.BlendMode
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.CompositingStrategy
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.Bt2ViewModel
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.InstallState
import com.dfdx047.dragonrage.data.formatBytes
import com.dfdx047.dragonrage.ui.components.DragonBall
import com.dfdx047.dragonrage.ui.components.SectionCard
import com.dfdx047.dragonrage.ui.theme.Dbz
import com.dfdx047.dragonrage.ui.theme.TitleStyle

/** The BT2 tab's home: the game's title, its disc image, playing it and its engine (BT3's HomeScreen, for BT2). */
@Composable
fun Bt2HomeScreen(vm: Bt2ViewModel) {
    val install by vm.install.collectAsStateWithLifecycle()
    val pickIso = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> if (uri != null) vm.installFromIso(uri) }
    val pickEngine = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> if (uri != null) vm.importEngine(uri) }
    val engine by vm.engine.collectAsStateWithLifecycle()
    val engineBusy by vm.engineBusy.collectAsStateWithLifecycle()
    var confirmRemove by remember { mutableStateOf(false) }
    var confirmRemoveEngine by remember { mutableStateOf(false) }
    val installed = install is InstallState.Installed
    val engineReady = vm.enginePackaged || engine?.compatible == true
    val links = LocalUriHandler.current

    Column(
        Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp).padding(bottom = 24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        Bt2Hero()
        Bt2OnlineCard()

        Button(
            onClick = vm::play, enabled = installed && engineReady,
            modifier = Modifier.fillMaxWidth().height(64.dp), shape = RoundedCornerShape(22.dp),
            colors = ButtonDefaults.buttonColors(containerColor = Dbz.GiOrange, contentColor = Dbz.Night),
        ) {
            Icon(Icons.Rounded.PlayArrow, null, Modifier.size(28.dp))
            Spacer(Modifier.size(8.dp))
            Text(stringResource(R.string.bt2_play), fontSize = 20.sp, fontWeight = FontWeight.Black)
        }
        if (installed) {
            Text(stringResource(R.string.bt2_back_hint), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }

        SectionCard(stringResource(R.string.game_data), Icons.Rounded.Album) {
            when (val s = install) {
                InstallState.NotInstalled -> {
                    Text(stringResource(R.string.bt2_intro), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    FilledTonalButton(onClick = { pickIso.launch(arrayOf("*/*")) }) { Text(stringResource(R.string.pick_iso)) }
                }
                is InstallState.Working -> {
                    Text(s.step, style = MaterialTheme.typography.bodyLarge)
                    LinearProgressIndicator(progress = { s.progress.coerceIn(0f, 1f) }, modifier = Modifier.fillMaxWidth(), color = MaterialTheme.colorScheme.tertiary)
                    Text(s.detail, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    OutlinedButton(onClick = vm::cancelInstall) {
                        Icon(Icons.Rounded.Close, null, Modifier.size(18.dp)); Spacer(Modifier.size(6.dp)); Text(stringResource(R.string.cancel))
                    }
                }
                is InstallState.Installed -> {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Icon(Icons.Rounded.CheckCircle, null, tint = Dbz.ShenronGreen)
                        Spacer(Modifier.size(8.dp))
                        Text(stringResource(R.string.bt2_ready, formatBytes(s.bytes)), style = MaterialTheme.typography.bodyMedium)
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

        SectionCard(stringResource(R.string.bt2_engine_title), Icons.Rounded.Memory, accent = MaterialTheme.colorScheme.secondary) {
            // the APK's own (a private build), else the motor package the player imports (Bt2Engine)
            val e = engine
            when {
                engineBusy -> {
                    Text(stringResource(R.string.bt2_engine_importing), style = MaterialTheme.typography.bodyMedium)
                    LinearProgressIndicator(Modifier.fillMaxWidth(), color = MaterialTheme.colorScheme.tertiary)
                }
                e != null && e.compatible -> Row(verticalAlignment = Alignment.CenterVertically) {
                    Icon(Icons.Rounded.CheckCircle, null, tint = Dbz.ShenronGreen)
                    Spacer(Modifier.size(8.dp))
                    Text(stringResource(R.string.bt2_engine_installed, e.version), style = MaterialTheme.typography.bodyMedium)
                }
                e != null -> Row(verticalAlignment = Alignment.Top) {
                    Icon(Icons.Rounded.Warning, null, tint = MaterialTheme.colorScheme.error)
                    Spacer(Modifier.size(8.dp))
                    Text(stringResource(R.string.bt2_engine_outdated, e.version), style = MaterialTheme.typography.bodyMedium)
                }
                vm.enginePackaged -> Text(stringResource(R.string.bt2_engine_included), style = MaterialTheme.typography.bodyMedium)
                else -> Text(stringResource(R.string.bt2_engine_get), style = MaterialTheme.typography.bodyMedium)
            }
            if (!engineBusy && !(vm.enginePackaged && e == null)) {
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp), verticalAlignment = Alignment.CenterVertically) {
                    FilledTonalButton(onClick = { pickEngine.launch(arrayOf("application/zip", "application/octet-stream", "*/*")) }) {
                        Text(stringResource(if (e == null) R.string.bt2_engine_import else R.string.bt2_engine_import_other))
                    }
                    if (e == null) TextButton(onClick = { links.openUri(DISCORD) }) { Text("Discord") }
                    else TextButton(onClick = { confirmRemoveEngine = true }) { Text(stringResource(R.string.remove)) }
                }
            }
            Text(stringResource(R.string.bt2_gpu_note), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }

    if (confirmRemoveEngine) {
        AlertDialog(
            onDismissRequest = { confirmRemoveEngine = false },
            title = { Text(stringResource(R.string.bt2_engine_remove_q)) },
            text = { Text(stringResource(R.string.bt2_engine_remove_text)) },
            confirmButton = { TextButton(onClick = { confirmRemoveEngine = false; vm.removeEngine() }) { Text(stringResource(R.string.remove)) } },
            dismissButton = { TextButton(onClick = { confirmRemoveEngine = false }) { Text(stringResource(R.string.cancel)) } },
        )
    }

    if (confirmRemove) {
        AlertDialog(
            onDismissRequest = { confirmRemove = false },
            title = { Text(stringResource(R.string.bt2_remove_q)) },
            text = { Text(stringResource(R.string.bt2_remove_text)) },
            confirmButton = { TextButton(onClick = { confirmRemove = false; vm.removeImage() }) { Text(stringResource(R.string.remove)) } },
            dismissButton = { TextButton(onClick = { confirmRemove = false }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}

/** Where the motor packages are shared (HomeScreen's Discord link). */
private const val DISCORD = "https://discord.gg/6XXxT547Qx"

/**
 * Online play, as BT3's Home shows it (HomeScreen's OnlineBanner), for BT2: Dragon Net 2 is not there yet, so the
 * card says so and the button to play online is switched off.
 */
@Composable
private fun Bt2OnlineCard() {
    val color = MaterialTheme.colorScheme.outline
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(containerColor = color.copy(alpha = 0.13f)),
        border = androidx.compose.foundation.BorderStroke(1.5.dp, color.copy(alpha = 0.6f)),
    ) {
        Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.size(12.dp).clip(CircleShape).background(color))
                Spacer(Modifier.size(10.dp))
                Text(stringResource(R.string.bt2_online_banner_title).uppercase(), style = MaterialTheme.typography.labelLarge, color = color)
            }
            Text(stringResource(R.string.bt2_online_name), style = MaterialTheme.typography.titleLarge)
            Text(stringResource(R.string.bt2_online_soon), style = MaterialTheme.typography.titleMedium, color = Dbz.GiOrange)
            Text(stringResource(R.string.bt2_online_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            FilledTonalButton(onClick = {}, enabled = false) { Text(stringResource(R.string.bt2_play_online)) }
        }
    }
}

@Composable
private fun Bt2Hero() {
    Box(Modifier.fillMaxWidth().padding(top = 12.dp), contentAlignment = Alignment.CenterStart) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            DragonBall(stars = 2, size = 120.dp)
            Column(Modifier.padding(start = 4.dp)) {
                val gold = Brush.verticalGradient(listOf(Dbz.BallLight, Dbz.SaiyanGold, Dbz.GiOrange))
                Text(
                    "DRAGON\nRAGE",
                    style = TitleStyle.copy(lineHeight = TitleStyle.fontSize * 0.95f),
                    modifier = Modifier
                        .graphicsLayer(compositingStrategy = CompositingStrategy.Offscreen)
                        .drawWithContent { drawContent(); drawRect(gold, blendMode = BlendMode.SrcIn) },
                )
                Text(stringResource(R.string.bt2_tagline), style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}
