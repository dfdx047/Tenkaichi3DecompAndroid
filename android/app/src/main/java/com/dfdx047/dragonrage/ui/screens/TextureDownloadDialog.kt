package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalUriHandler
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.CatalogState
import com.dfdx047.dragonrage.data.formatBytes

private const val SEARCH_URL = "https://www.google.com/search?q=Dragon+Ball+Z+Budokai+Tenkaichi+3+texture+pack+PCSX2+dds"

/** Packs from the catalog (android/texture-packs.json in the repository), a field for any direct .zip link, and a web search. */
@Composable
fun TextureDownloadDialog(vm: AppViewModel, onDismiss: () -> Unit) {
    val catalog by vm.catalog.collectAsStateWithLifecycle()
    var link by remember { mutableStateOf("") }
    val uri = LocalUriHandler.current
    LaunchedEffect(Unit) { vm.loadCatalog() }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.tex_dl_title)) },
        text = {
            Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(12.dp)) {
                when (val c = catalog) {
                    CatalogState.Loading -> Row(verticalAlignment = Alignment.CenterVertically) {
                        CircularProgressIndicator(Modifier.size(20.dp), strokeWidth = 2.dp)
                        Text(stringResource(R.string.tex_dl_loading), Modifier.padding(start = 12.dp))
                    }
                    CatalogState.Failed -> Text(stringResource(R.string.tex_dl_failed), color = MaterialTheme.colorScheme.error)
                    is CatalogState.Loaded -> if (c.packs.isEmpty()) {
                        Text(stringResource(R.string.tex_dl_empty), color = MaterialTheme.colorScheme.onSurfaceVariant)
                    } else c.packs.forEach { p ->
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Column(Modifier.weight(1f).padding(end = 8.dp)) {
                                Text(p.name, style = MaterialTheme.typography.titleSmall)
                                val sub = listOfNotNull(p.description.takeIf { it.isNotBlank() }, p.size.takeIf { it > 0 }?.let { formatBytes(it) }).joinToString(" · ")
                                if (sub.isNotEmpty()) Text(sub, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                            }
                            Button(onClick = { vm.downloadTexturePack(p.url, p.name); onDismiss() }) { Text(stringResource(R.string.tex_dl_get)) }
                        }
                    }
                }
                HorizontalDivider()
                Text(stringResource(R.string.tex_dl_link), style = MaterialTheme.typography.titleSmall)
                OutlinedTextField(
                    value = link, onValueChange = { link = it }, singleLine = true,
                    placeholder = { Text("https://…/pack.zip") }, modifier = Modifier.fillMaxWidth(),
                )
                Text(stringResource(R.string.tex_dl_link_hint), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                Button(
                    enabled = link.trim().startsWith("https://"),
                    onClick = { vm.downloadTexturePack(link, null); onDismiss() },
                ) { Text(stringResource(R.string.tex_dl_get)) }
                TextButton(onClick = { runCatching { uri.openUri(SEARCH_URL) } }) { Text(stringResource(R.string.tex_dl_search)) }
            }
        },
        confirmButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.close)) } },
    )
}
