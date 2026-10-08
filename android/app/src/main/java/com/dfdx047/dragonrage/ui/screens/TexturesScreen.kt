package com.dfdx047.dragonrage.ui.screens

import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Texture
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ExtendedFloatingActionButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Switch
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
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.data.EngineSettings
import com.dfdx047.dragonrage.data.TexturePack
import com.dfdx047.dragonrage.data.formatBytes
import com.dfdx047.dragonrage.ui.components.EmptyState
import com.dfdx047.dragonrage.ui.components.ScreenTitle
import com.dfdx047.dragonrage.ui.components.SectionCard

@Composable
fun TexturesScreen(vm: AppViewModel) {
    val packs by vm.texturePacks.collectAsStateWithLifecycle()
    val settings by vm.engineSettings.collectAsStateWithLifecycle()
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri ->
        if (uri != null) vm.importTexturePack(uri)
    }
    var toDelete by remember { mutableStateOf<TexturePack?>(null) }

    Box(Modifier.fillMaxSize()) {
        LazyColumn(
            contentPadding = PaddingValues(start = 16.dp, end = 16.dp, bottom = 96.dp),
            verticalArrangement = Arrangement.spacedBy(12.dp),
        ) {
            item { ScreenTitle(stringResource(R.string.textures_title), stringResource(R.string.textures_sub)) }
            item {
                SectionCard(stringResource(R.string.textures_use), Icons.Rounded.Texture) {
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Text(
                            stringResource(R.string.textures_use_text),
                            style = MaterialTheme.typography.bodyMedium,
                            color = MaterialTheme.colorScheme.onSurfaceVariant,
                            modifier = Modifier.weight(1f),
                        )
                        Switch(
                            checked = (settings[EngineSettings.TEXTURE_PACK] ?: 1) != 0,
                            onCheckedChange = { vm.setting(EngineSettings.TEXTURE_PACK, if (it) 1 else 0) },
                        )
                    }
                }
            }
            if (packs.isEmpty()) {
                item {
                    EmptyState(
                        Icons.Rounded.Texture,
                        stringResource(R.string.textures_empty),
                        stringResource(R.string.textures_empty_text),
                    )
                }
            }
            items(packs, key = { it.dir.path }) { p ->
                PackCard(p, onToggle = { vm.setPackEnabled(p, it) }, onDelete = { toDelete = p })
            }
        }
        ExtendedFloatingActionButton(
            onClick = { pick.launch(arrayOf("application/zip", "application/x-zip-compressed", "application/octet-stream")) },
            icon = { Icon(Icons.Rounded.Add, null) },
            text = { Text(stringResource(R.string.add_pack)) },
            modifier = Modifier.align(Alignment.BottomEnd).padding(16.dp),
        )
    }

    toDelete?.let { p ->
        AlertDialog(
            onDismissRequest = { toDelete = null },
            title = { Text(stringResource(R.string.delete_q, p.name)) },
            text = { Text(stringResource(R.string.delete_pack_text, formatBytes(p.bytes))) },
            confirmButton = { TextButton(onClick = { toDelete = null; vm.deletePack(p) }) { Text(stringResource(R.string.delete)) } },
            dismissButton = { TextButton(onClick = { toDelete = null }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}

@Composable
private fun PackCard(p: TexturePack, onToggle: (Boolean) -> Unit, onDelete: () -> Unit) {
    Card(
        shape = MaterialTheme.shapes.large,
        colors = CardDefaults.cardColors(
            containerColor = if (p.enabled) MaterialTheme.colorScheme.surfaceContainerHigh else MaterialTheme.colorScheme.surfaceContainerLow,
        ),
    ) {
        Row(Modifier.fillMaxWidth().padding(start = 18.dp, end = 8.dp, top = 12.dp, bottom = 12.dp), verticalAlignment = Alignment.CenterVertically) {
            Column(Modifier.weight(1f)) {
                Text(p.name, style = MaterialTheme.typography.titleMedium, maxLines = 2, overflow = TextOverflow.Ellipsis)
                Text(
                    stringResource(R.string.pack_info, p.textures, formatBytes(p.bytes)) + if (p.enabled) "" else " · " + stringResource(R.string.pack_off),
                    style = MaterialTheme.typography.bodySmall,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                )
            }
            Switch(checked = p.enabled, onCheckedChange = onToggle)
            Spacer(Modifier.size(4.dp))
            IconButton(onClick = onDelete) { Icon(Icons.Rounded.Delete, stringResource(R.string.delete)) }
        }
    }
}
