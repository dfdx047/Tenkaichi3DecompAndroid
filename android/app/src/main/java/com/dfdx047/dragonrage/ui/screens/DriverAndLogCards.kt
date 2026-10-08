package com.dfdx047.dragonrage.ui.screens

import android.content.Intent
import androidx.activity.compose.rememberLauncherForActivityResult
import androidx.activity.result.contract.ActivityResultContracts
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.size
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.BugReport
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.DeveloperBoard
import androidx.compose.material.icons.rounded.Download
import androidx.compose.material.icons.rounded.Upload
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.RadioButton
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.ui.components.SectionCard
import com.dfdx047.dragonrage.ui.theme.Dbz

/** Custom Vulkan drivers for Adreno GPUs (Turnip, newer Qualcomm drivers), through adrenotools. */
@Composable
fun GpuDriverCard(vm: AppViewModel) {
    val list by vm.gpuDrivers.collectAsStateWithLifecycle()
    val chosen by vm.gpuDriver.collectAsStateWithLifecycle()
    val pick = rememberLauncherForActivityResult(ActivityResultContracts.OpenDocument()) { uri -> uri?.let { vm.importDriver(it) } }

    SectionCard(stringResource(R.string.gpu_driver), Icons.Rounded.DeveloperBoard, accent = Dbz.KiBlue) {
        Text(stringResource(R.string.gpu_driver_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        Row(verticalAlignment = Alignment.CenterVertically) {
            RadioButton(selected = chosen == null, onClick = { vm.selectDriver(null) })
            Text(stringResource(R.string.gpu_driver_system), style = MaterialTheme.typography.bodyLarge, modifier = Modifier.weight(1f))
        }
        list.forEach { d ->
            Row(verticalAlignment = Alignment.CenterVertically) {
                RadioButton(selected = chosen == d.id, onClick = { vm.selectDriver(d.id) })
                Text(
                    if (d.version.isNotEmpty()) "${d.name} · ${d.version}" else d.name,
                    style = MaterialTheme.typography.bodyLarge, modifier = Modifier.weight(1f),
                )
                IconButton(onClick = { vm.deleteDriver(d) }) { Icon(Icons.Rounded.Delete, null) }
            }
        }
        FilledTonalButton(onClick = { pick.launch(arrayOf("application/zip", "application/x-zip-compressed", "application/octet-stream")) }) {
            Icon(Icons.Rounded.Upload, null, Modifier.size(18.dp)); Spacer(Modifier.size(6.dp)); Text(stringResource(R.string.gpu_driver_import))
        }
    }
}

/** A bug report in Downloads (device, the game's last run, the app's system log), and the share sheet for it. */
@Composable
fun LogCard(vm: AppViewModel) {
    val context = LocalContext.current
    val share = stringResource(R.string.log_share)
    LaunchedEffect(Unit) {
        vm.logShare.collect { uri ->
            val send = Intent(Intent.ACTION_SEND).apply {
                type = "text/plain"
                putExtra(Intent.EXTRA_STREAM, uri)
                addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            }
            runCatching { context.startActivity(Intent.createChooser(send, share)) }
        }
    }
    SectionCard(stringResource(R.string.log_title), Icons.Rounded.BugReport, accent = Dbz.SaiyanGold) {
        Text(stringResource(R.string.log_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        FilledTonalButton(onClick = { vm.exportLog() }) {
            Icon(Icons.Rounded.Download, null, Modifier.size(18.dp)); Spacer(Modifier.size(6.dp)); Text(stringResource(R.string.log_export))
        }
    }
}
