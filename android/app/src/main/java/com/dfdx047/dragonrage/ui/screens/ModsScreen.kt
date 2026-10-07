package com.dfdx047.dragonrage.ui.screens

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.itemsIndexed
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material.icons.rounded.Extension
import androidx.compose.material.icons.rounded.KeyboardArrowDown
import androidx.compose.material.icons.rounded.KeyboardArrowUp
import androidx.compose.material.icons.rounded.Landscape
import androidx.compose.material.icons.rounded.MusicNote
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Switch
import androidx.compose.material3.Tab
import androidx.compose.material3.TabRow
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.data.ExtraItem
import com.dfdx047.dragonrage.data.ExtraKind
import com.dfdx047.dragonrage.data.FileMod
import com.dfdx047.dragonrage.data.formatBytes
import com.dfdx047.dragonrage.ui.components.EmptyState
import com.dfdx047.dragonrage.ui.components.ScreenTitle
import com.dfdx047.dragonrage.ui.theme.Dbz

@Composable
fun ModsScreen(vm: AppViewModel) {
    val mods by vm.fileMods.collectAsStateWithLifecycle()
    val stages by vm.stages.collectAsStateWithLifecycle()
    val songs by vm.songs.collectAsStateWithLifecycle()
    var tab by rememberSaveable { mutableIntStateOf(0) }
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { uris ->
        if (uris.isNotEmpty()) vm.importMod(uris)
    }
    var deleteMod by remember { mutableStateOf<FileMod?>(null) }
    var deleteExtra by remember { mutableStateOf<Pair<ExtraKind, ExtraItem>?>(null) }
    var rename by remember { mutableStateOf<Pair<ExtraKind, ExtraItem>?>(null) }

    Box(Modifier.fillMaxSize()) {
        Column {
            ScreenTitle("Mods", "Personagens, estágios e músicas novas", Modifier.padding(horizontal = 16.dp))
            TabRow(selectedTabIndex = tab, containerColor = Color.Transparent) {
                Tab(tab == 0, { tab = 0 }, text = { Text("Arquivos (${mods.size})") })
                Tab(tab == 1, { tab = 1 }, text = { Text("Estágios (${stages.size})") })
                Tab(tab == 2, { tab = 2 }, text = { Text("Músicas (${songs.size})") })
            }
            LazyColumn(
                contentPadding = PaddingValues(start = 16.dp, end = 16.dp, top = 12.dp, bottom = 96.dp),
                verticalArrangement = Arrangement.spacedBy(10.dp),
            ) {
                when (tab) {
                    0 -> {
                        if (mods.isEmpty()) item {
                            EmptyState(
                                Icons.Rounded.Extension, "Nenhum mod",
                                "Mods em .zip que substituem arquivos do jogo, com as pastas como em gamedata " +
                                    "(pzs3us0, pzs3us1, pzs3us2, disc). Se dois mods mexem no mesmo arquivo, vale o de baixo.",
                            )
                        }
                        itemsIndexed(mods, key = { _, m -> m.id }) { i, m ->
                            ModCard(
                                m, first = i == 0, last = i == mods.lastIndex,
                                onToggle = { vm.setModEnabled(m, it) },
                                onUp = { vm.moveMod(m, -1) }, onDown = { vm.moveMod(m, 1) },
                                onDelete = { deleteMod = m },
                            )
                        }
                    }
                    else -> {
                        val kind = if (tab == 1) ExtraKind.STAGE else ExtraKind.SONG
                        val list = if (tab == 1) stages else songs
                        if (list.isEmpty()) item {
                            if (kind == ExtraKind.STAGE) {
                                EmptyState(
                                    Icons.Rounded.Landscape, "Nenhum estágio extra",
                                    "Adicione mapas .unk (os mesmos usados no PCSX2). Eles aparecem como novas opções na seleção de estágio.",
                                )
                            } else {
                                EmptyState(
                                    Icons.Rounded.MusicNote, "Nenhuma música extra",
                                    "Adicione músicas .adx. Elas entram na lista de músicas (BGM) com o nome que você escolher.",
                                )
                            }
                        }
                        itemsIndexed(list, key = { _, it -> it.file.path }) { _, item ->
                            ExtraCard(
                                item, kind,
                                onRename = { rename = kind to item },
                                onDelete = { deleteExtra = kind to item },
                            )
                        }
                    }
                }
            }
        }
        ExtendedFloatingActionButton(
            onClick = { pick.launch(arrayOf("*/*")) },
            icon = { Icon(Icons.Rounded.Add, null) },
            text = { Text("Adicionar") },
            modifier = Modifier.align(Alignment.BottomEnd).padding(16.dp),
        )
    }

    deleteMod?.let { m ->
        ConfirmDelete("Apagar o mod \"${m.name}\"?", onConfirm = { vm.deleteMod(m) }, onDismiss = { deleteMod = null })
    }
    deleteExtra?.let { (kind, item) ->
        ConfirmDelete("Apagar \"${item.displayName}\"?", onConfirm = { vm.deleteExtra(kind, item) }, onDismiss = { deleteExtra = null })
    }
    rename?.let { (kind, item) ->
        var text by remember(item) { mutableStateOf(item.displayName) }
        AlertDialog(
            onDismissRequest = { rename = null },
            title = { Text("Nome no jogo") },
            text = {
                OutlinedTextField(
                    value = text, onValueChange = { text = it.take(40) }, singleLine = true,
                    supportingText = { Text("Escrito com as letras do próprio jogo na seleção") },
                )
            },
            confirmButton = { TextButton(onClick = { rename = null; vm.renameExtra(kind, item, text) }) { Text("Salvar") } },
            dismissButton = { TextButton(onClick = { rename = null }) { Text("Cancelar") } },
        )
    }
}

@Composable
private fun ConfirmDelete(title: String, onConfirm: () -> Unit, onDismiss: () -> Unit) {
    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(title) },
        confirmButton = { TextButton(onClick = { onDismiss(); onConfirm() }) { Text("Apagar") } },
        dismissButton = { TextButton(onClick = onDismiss) { Text("Cancelar") } },
    )
}

@Composable
private fun ModCard(
    m: FileMod, first: Boolean, last: Boolean,
    onToggle: (Boolean) -> Unit, onUp: () -> Unit, onDown: () -> Unit, onDelete: () -> Unit,
) {
    Card(
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = if (m.enabled) MaterialTheme.colorScheme.surfaceContainerHigh else MaterialTheme.colorScheme.surfaceContainerLow,
        ),
    ) {
        Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 4.dp, top = 10.dp, bottom = 10.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(m.name, style = MaterialTheme.typography.titleMedium, maxLines = 2, overflow = TextOverflow.Ellipsis)
                Text("${m.files} arquivos · ${formatBytes(m.bytes)}", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
            Column {
                IconButton(onClick = onUp, enabled = !first) { Icon(Icons.Rounded.KeyboardArrowUp, "Subir") }
                IconButton(onClick = onDown, enabled = !last) { Icon(Icons.Rounded.KeyboardArrowDown, "Descer") }
            }
            Switch(checked = m.enabled, onCheckedChange = onToggle)
            IconButton(onClick = onDelete) { Icon(Icons.Rounded.Delete, "Apagar") }
        }
    }
}

@Composable
private fun ExtraCard(item: ExtraItem, kind: ExtraKind, onRename: () -> Unit, onDelete: () -> Unit) {
    Card(shape = MaterialTheme.shapes.large, colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainerHigh)) {
        Row(Modifier.fillMaxWidth().padding(start = 16.dp, end = 4.dp, top = 8.dp, bottom = 8.dp), verticalAlignment = Alignment.CenterVertically) {
            Icon(
                if (kind == ExtraKind.STAGE) Icons.Rounded.Landscape else Icons.Rounded.MusicNote, null,
                tint = if (kind == ExtraKind.STAGE) Dbz.KiBlue else Dbz.ShenronGreen,
            )
            Column(Modifier.weight(1f).padding(start = 12.dp)) {
                Text(item.displayName, style = MaterialTheme.typography.titleMedium, maxLines = 1, overflow = TextOverflow.Ellipsis)
                Text("${item.file.name} · ${formatBytes(item.bytes)}", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 1, overflow = TextOverflow.Ellipsis)
            }
            IconButton(onClick = onRename) { Icon(Icons.Rounded.Edit, "Renomear") }
            IconButton(onClick = onDelete) { Icon(Icons.Rounded.Delete, "Apagar") }
        }
    }
}
