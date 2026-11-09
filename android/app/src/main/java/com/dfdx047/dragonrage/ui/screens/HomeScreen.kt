package com.dfdx047.dragonrage.ui.screens

import com.dfdx047.dragonrage.data.Game
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
import androidx.compose.material.icons.rounded.AddToHomeScreen
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
import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.rounded.Code
import androidx.compose.material.icons.rounded.Forum
import androidx.compose.material.icons.rounded.LocalCafe
import androidx.compose.material.icons.rounded.Public
import androidx.compose.material.icons.rounded.SmartDisplay
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.text.font.FontWeight
import com.dfdx047.dragonrage.data.REPO_URL
import com.dfdx047.dragonrage.data.OnlineState
import com.dfdx047.dragonrage.data.OnlineStatus
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.IconButton
import androidx.compose.material.icons.rounded.Refresh

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
        val bt4 = vm.game == Game.BT4
        Hero(bt4)
        if (!bt4) OnlineBanner(vm) // (online is the BT3 engine's: Dragon Net and the PC)

        val installed = install is InstallState.Installed
        PlayButton(enabled = installed, onClick = vm::play)
        if (installed && !bt4) {
            OutlinedButton(onClick = vm::createPlayShortcut, modifier = Modifier.fillMaxWidth()) {
                Icon(Icons.Rounded.AddToHomeScreen, null, Modifier.size(20.dp))
                Spacer(Modifier.size(8.dp))
                Text(stringResource(R.string.shortcut_add))
            }
            Text(
                stringResource(R.string.shortcut_add_text),
                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }

        SectionCard(stringResource(R.string.game_data), Icons.Rounded.Album) {
            when (val s = install) {
                InstallState.NotInstalled -> {
                    Text(
                        stringResource(if (bt4) R.string.bt4_game_data_intro else R.string.game_data_intro),
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
                        Text(stringResource(if (s.modified) R.string.game_data_ok_mod else R.string.game_data_ok, s.files, formatBytes(s.bytes)), style = MaterialTheme.typography.bodyMedium)
                    }
                    if (s.modified) {
                        Row(verticalAlignment = Alignment.Top) {
                            Icon(Icons.Rounded.Warning, null, tint = Dbz.SaiyanGold)
                            Spacer(Modifier.size(8.dp))
                            Text(
                                stringResource(R.string.game_data_modified),
                                style = MaterialTheme.typography.bodySmall,
                                color = MaterialTheme.colorScheme.onSurfaceVariant,
                            )
                        }
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
                Text(stringResource(if (bt4) R.string.bt4_engine_included else R.string.engine_included), style = MaterialTheme.typography.bodyMedium)
            } else {
                Text(
                    stringResource(if (bt4) R.string.bt4_engine_missing else R.string.engine_missing),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }

        Links()

        Row(horizontalArrangement = Arrangement.spacedBy(10.dp), modifier = Modifier.fillMaxWidth()) {
            StatPill(stringResource(R.string.stat_textures), "${packs.count { it.enabled }}", Dbz.SaiyanGold, Modifier.weight(1f))
            if (!bt4) {
                StatPill(stringResource(R.string.stat_mods), "${mods.count { it.enabled }}", MaterialTheme.colorScheme.primary, Modifier.weight(1f))
                StatPill(stringResource(R.string.stat_stages), "${stages.size}", Dbz.KiBlue, Modifier.weight(1f))
                StatPill(stringResource(R.string.stat_songs), "${songs.size}", Dbz.ShenronGreen, Modifier.weight(1f))
            }
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
private fun Hero(bt4: Boolean = false) {
    Box(Modifier.fillMaxWidth().padding(top = 12.dp), contentAlignment = Alignment.CenterStart) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            DragonBall(stars = 4, size = 120.dp)
            Column(Modifier.padding(start = 4.dp)) {
                val gold = if (bt4) Brush.verticalGradient(listOf(Color(0xFFC4ECFF), Color(0xFF5CD6FF), Color(0xFFB27CF0)))
                else Brush.verticalGradient(listOf(Dbz.BallLight, Dbz.SaiyanGold, Dbz.GiOrange))
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
                    stringResource(if (bt4) R.string.bt4_tagline else R.string.tagline),
                    style = MaterialTheme.typography.labelLarge,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
        }
    }
}

/** The Play button: gold to orange, a slow ki aura breathing around it and a gleam passing over it. */
@Composable
private fun PlayButton(enabled: Boolean, onClick: () -> Unit) {
    val t = rememberInfiniteTransition(label = "play")
    val breath by t.animateFloat(0f, 1f, infiniteRepeatable(tween(1600), RepeatMode.Reverse), label = "aura")
    val gleam by t.animateFloat(-0.4f, 1.4f, infiniteRepeatable(tween(2600, easing = LinearEasing)), label = "gleam")
    val shape = RoundedCornerShape(22.dp)
    Box(
        Modifier.fillMaxWidth().height(84.dp).drawBehind {
            if (!enabled) return@drawBehind
            // the aura: soft layers of orange and gold, larger as it breathes
            for (i in 3 downTo 1) {
                val grow = (6f + 7f * i * (0.6f + 0.4f * breath)).dp.toPx()
                drawRoundRect(
                    color = (if (i == 1) Dbz.SaiyanGold else Dbz.GiOrange).copy(alpha = 0.09f + 0.05f * breath / i),
                    topLeft = Offset(-grow, -grow * 0.6f),
                    size = androidx.compose.ui.geometry.Size(size.width + grow * 2, size.height + grow * 1.2f),
                    cornerRadius = CornerRadius(30.dp.toPx() + grow),
                )
            }
        },
        contentAlignment = Alignment.Center,
    ) {
        Box(
            Modifier.fillMaxSize().clip(shape)
                .background(
                    if (enabled) Brush.horizontalGradient(listOf(Dbz.GiOrangeDeep, Dbz.GiOrange, Dbz.BallAmber))
                    else Brush.horizontalGradient(listOf(Color(0xFF3A3D52), Color(0xFF2E3144))),
                )
                .border(2.dp, if (enabled) Dbz.BallLight.copy(alpha = 0.55f) else Color(0x33FFFFFF), shape)
                .drawBehind {
                    if (!enabled) return@drawBehind
                    val x = size.width * gleam
                    drawRect(
                        Brush.linearGradient(
                            0f to Color.Transparent, 0.5f to Color.White.copy(alpha = 0.28f), 1f to Color.Transparent,
                            start = Offset(x - size.height, 0f), end = Offset(x + size.height, size.height),
                        ),
                    )
                }
                .clickable(enabled = enabled, onClick = onClick),
            contentAlignment = Alignment.Center,
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(
                    Modifier.size(46.dp).clip(CircleShape).background(Color(0x33000000)),
                    contentAlignment = Alignment.Center,
                ) { Icon(Icons.Rounded.PlayArrow, null, Modifier.size(34.dp), tint = Color.White) }
                Spacer(Modifier.width(14.dp))
                Text(
                    stringResource(R.string.play).uppercase(),
                    style = MaterialTheme.typography.headlineMedium.copy(fontWeight = FontWeight.Black),
                    color = if (enabled) Color(0xFF2A1300) else Color(0xFF9A9CB0),
                )
            }
        }
    }
}

/**
 * The state of online play, large, at the top of Home: what the matchmaking service says of this build right now
 * (working, needs the next update, official PC releases only, no answer), how online works, and the help dialog.
 */
@Composable
private fun OnlineBanner(vm: AppViewModel) {
    val state by vm.online.collectAsStateWithLifecycle()
    var open by remember { mutableStateOf(false) }
    val (color, title, text) = when (val st = state) {
        OnlineState.Checking -> Triple(MaterialTheme.colorScheme.outline, stringResource(R.string.online_checking), "")
        OnlineState.Unreachable -> Triple(MaterialTheme.colorScheme.outline, stringResource(R.string.online_unreachable_title), stringResource(R.string.online_unreachable))
        OnlineState.Limited -> Triple(Dbz.GiOrange, stringResource(R.string.online_limited_title), stringResource(R.string.online_limited))
        is OnlineState.Outdated -> Triple(MaterialTheme.colorScheme.error, stringResource(R.string.online_outdated_title),
            stringResource(R.string.online_outdated, OnlineStatus.release(st.min), OnlineStatus.release(st.mine)))
        is OnlineState.OfficialOnly -> Triple(Dbz.GiOrange, stringResource(R.string.online_official_title), stringResource(R.string.online_official))
        is OnlineState.DragonVersion -> Triple(Dbz.GiOrange, stringResource(R.string.online_dragon_version_title), stringResource(R.string.online_dragon_version))
        is OnlineState.Working -> Triple(Dbz.ShenronGreen, stringResource(if (st.dragon) R.string.online_dragon_ok_title else R.string.online_ok_title),
            (if (st.dragon) stringResource(R.string.online_dragon_ok) else stringResource(R.string.online_ok, OnlineStatus.release(st.mine))) +
                (if (st.searching > 0) " " + stringResource(R.string.online_searching, st.searching) else ""))
    }
    Card(
        modifier = Modifier.fillMaxWidth(),
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(containerColor = color.copy(alpha = 0.13f), contentColor = MaterialTheme.colorScheme.onSurface),
        border = androidx.compose.foundation.BorderStroke(1.5.dp, color.copy(alpha = 0.6f)),
    ) {
        Column(Modifier.padding(18.dp), verticalArrangement = Arrangement.spacedBy(8.dp)) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.size(12.dp).clip(CircleShape).background(color))
                Spacer(Modifier.width(10.dp))
                Text(stringResource(R.string.online_banner_title).uppercase(), style = MaterialTheme.typography.labelLarge, color = color, modifier = Modifier.weight(1f))
                IconButton(onClick = { vm.checkOnline() }) { Icon(Icons.Rounded.Refresh, stringResource(R.string.drivers_refresh)) }
            }
            Text(title, style = MaterialTheme.typography.titleLarge)
            if (text.isNotEmpty()) Text(text, style = MaterialTheme.typography.bodyMedium)
            Text(stringResource(R.string.online_explain), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(stringResource(R.string.online_iso_notice), style = MaterialTheme.typography.bodySmall, color = Dbz.GiOrange)
            FilledTonalButton(onClick = { open = true }) { Text(stringResource(R.string.online_help_button)) }
        }
    }
    if (open) {
        AlertDialog(
            onDismissRequest = { open = false },
            title = { Text(stringResource(R.string.online_title)) },
            text = {
                Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    // the guide: the two kinds of online, how to play, Ranked, the chat, what each message means, the log
                    for ((t, b) in listOf(
                        R.string.online_modes_title to R.string.online_modes,
                        R.string.online_how_title to R.string.online_how,
                        R.string.online_ranked_title to R.string.online_ranked,
                        R.string.online_chat_title to R.string.online_chat,
                        R.string.online_fail_title to R.string.online_fail,
                        R.string.online_log_title to R.string.online_log,
                    )) {
                        Text(stringResource(t), style = MaterialTheme.typography.titleSmall, color = MaterialTheme.colorScheme.primary)
                        Text(stringResource(b), style = MaterialTheme.typography.bodyMedium)
                    }
                    FilledTonalButton(onClick = { open = false; vm.exportLogAndShare() }) { Text(stringResource(R.string.log_send)) }
                }
            },
            confirmButton = { TextButton(onClick = { open = false }) { Text(stringResource(R.string.close)) } },
        )
    }
}

private data class LinkItem(val label: String, val url: String, val icon: ImageVector, val color: Color)

/** Small round links: the channel, the community, support, and the code. */
@Composable
private fun Links() {
    val uri = LocalUriHandler.current
    val links = listOf(
        LinkItem("YouTube", "https://www.youtube.com/@DFDX47", Icons.Rounded.SmartDisplay, Color(0xFFFF3B30)),
        LinkItem("Discord", "https://discord.gg/6XXxT547Qx", Icons.Rounded.Forum, Color(0xFF7289FF)),
        LinkItem("Ko-fi", "https://ko-fi.com/dfdx047", Icons.Rounded.LocalCafe, Color(0xFF29ABE0)),
        LinkItem("GitHub", REPO_URL, Icons.Rounded.Code, Color(0xFFE6E8F0)),
    )
    Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.SpaceEvenly) {
        links.forEach { l ->
            Column(
                Modifier.clip(RoundedCornerShape(14.dp)).clickable { runCatching { uri.openUri(l.url) } }.padding(horizontal = 10.dp, vertical = 6.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Box(
                    Modifier.size(44.dp).clip(CircleShape).background(l.color.copy(alpha = 0.14f)).border(1.dp, l.color.copy(alpha = 0.45f), CircleShape),
                    contentAlignment = Alignment.Center,
                ) { Icon(l.icon, l.label, tint = l.color, modifier = Modifier.size(22.dp)) }
                Spacer(Modifier.height(4.dp))
                Text(l.label, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        }
    }
}
