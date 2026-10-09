package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.LinearProgressIndicator
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.BuildConfig
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.UpdateState
import com.dfdx047.dragonrage.data.formatBytes

/** The pop-up for a new version: offer, download with progress, and the "install unknown apps" permission step. */
@Composable
fun UpdateDialog(vm: AppViewModel) {
    val st by vm.update.collectAsStateWithLifecycle()
    when (val s = st) {
        is UpdateState.Available -> AlertDialog(
            onDismissRequest = vm::dismissUpdate,
            title = { Text(stringResource(R.string.update_available)) },
            text = {
                Column(Modifier.verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(stringResource(R.string.update_versions, s.info.version, BuildConfig.VERSION_NAME))
                    if (s.info.size > 0) Text(formatBytes(s.info.size), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    if (s.info.nightly) Text(stringResource(R.string.update_nightly), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    if (s.info.notes.isNotBlank()) Text(s.info.notes, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    Text(stringResource(R.string.update_keeps), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
            },
            confirmButton = { Button(onClick = vm::startUpdate) { Text(stringResource(R.string.update_now)) } },
            dismissButton = { TextButton(onClick = vm::dismissUpdate) { Text(stringResource(R.string.update_later)) } },
        )

        is UpdateState.Downloading -> AlertDialog(
            onDismissRequest = {},
            title = { Text(stringResource(R.string.update_downloading)) },
            text = {
                Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                    if (s.total > 0) LinearProgressIndicator(progress = { (s.done.toFloat() / s.total).coerceIn(0f, 1f) }, modifier = Modifier.fillMaxWidth())
                    else LinearProgressIndicator(Modifier.fillMaxWidth())
                    Text(
                        formatBytes(s.done) + if (s.total > 0) " / ${formatBytes(s.total)}" else "",
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
            },
            confirmButton = {},
            dismissButton = { TextButton(onClick = vm::cancelUpdate) { Text(stringResource(R.string.cancel)) } },
        )

        is UpdateState.NeedPermission -> AlertDialog(
            onDismissRequest = vm::dismissUpdate,
            title = { Text(stringResource(R.string.update_permission_title)) },
            text = { Text(stringResource(R.string.update_permission_text)) },
            confirmButton = { Button(onClick = vm::openInstallPermission) { Text(stringResource(R.string.update_permission_open)) } },
            dismissButton = { TextButton(onClick = vm::dismissUpdate) { Text(stringResource(R.string.cancel)) } },
        )

        else -> {}
    }
}
