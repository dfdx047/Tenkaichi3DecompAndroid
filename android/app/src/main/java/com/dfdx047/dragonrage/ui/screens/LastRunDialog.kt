package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ReportProblem
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.LastRun

/** "The game closed because …" with what to do, when its last run did not end well (LastRun). */
@Composable
fun LastRunDialog(vm: AppViewModel) {
    val r by vm.lastRun.collectAsStateWithLifecycle()
    val report = r ?: return
    val log by vm.lastRunLog.collectAsStateWithLifecycle()
    val context = LocalContext.current
    val (title, text) = when (report.problem) {
        LastRun.Problem.DATA -> R.string.lastrun_data_title to R.string.lastrun_data
        LastRun.Problem.MEMORY -> R.string.lastrun_memory_title to R.string.lastrun_memory
        LastRun.Problem.ENGINE -> R.string.lastrun_engine_title to R.string.lastrun_engine
        LastRun.Problem.CRASH -> R.string.lastrun_crash_title to R.string.lastrun_crash
        LastRun.Problem.TEXTURES -> R.string.lastrun_textures_title to R.string.lastrun_textures
        LastRun.Problem.DRIVER -> R.string.lastrun_driver_title to R.string.lastrun_driver
    }
    AlertDialog(
        onDismissRequest = { vm.dismissLastRun() },
        icon = { Icon(Icons.Rounded.ReportProblem, null, tint = MaterialTheme.colorScheme.error) },
        title = { Text(stringResource(title)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                Text(stringResource(text), style = MaterialTheme.typography.bodyMedium)
                Text(report.detail, style = MaterialTheme.typography.labelSmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                SavedLogNote(log)
            }
        },
        confirmButton = {
            TextButton(onClick = { log?.let { shareLog(context, it.uri) } ?: vm.exportLog(); vm.dismissLastRun() }) { Text(stringResource(R.string.log_send)) }
        },
        dismissButton = { TextButton(onClick = { vm.dismissLastRun() }) { Text(stringResource(R.string.close)) } },
    )
}
