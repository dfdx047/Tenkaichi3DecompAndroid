package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.LockOpen
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Icon
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
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.data.EngineSettings
import com.dfdx047.dragonrage.ui.components.DragonBall
import com.dfdx047.dragonrage.ui.components.ScreenTitle
import com.dfdx047.dragonrage.ui.components.SectionCard
import com.dfdx047.dragonrage.ui.theme.Dbz

@Composable
fun CheatsScreen(vm: AppViewModel) {
    val settings by vm.engineSettings.collectAsStateWithLifecycle()
    val pending = (settings[EngineSettings.UNLOCK_ALL] ?: 0) != 0
    var confirm by remember { mutableStateOf(false) }

    Column(
        Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp).padding(bottom = 24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        ScreenTitle(stringResource(R.string.cheats_title), stringResource(R.string.cheats_sub))

        SectionCard(stringResource(R.string.unlock_all), Icons.Rounded.LockOpen, accent = Dbz.SaiyanGold) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                DragonBall(stars = 7, size = 64.dp)
                Spacer(Modifier.size(8.dp))
                Text(
                    stringResource(R.string.unlock_all_text),
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.weight(1f),
                )
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    stringResource(if (pending) R.string.unlock_pending else R.string.unlock_apply),
                    style = MaterialTheme.typography.titleSmall,
                    modifier = Modifier.weight(1f),
                )
                Switch(
                    checked = pending,
                    onCheckedChange = { on -> if (on) confirm = true else vm.setting(EngineSettings.UNLOCK_ALL, 0) },
                )
            }
        }

        SectionCard(stringResource(R.string.how_it_works), Icons.Rounded.Info, accent = MaterialTheme.colorScheme.secondary) {
            Text(
                stringResource(R.string.cheats_how),
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }

    if (confirm) {
        AlertDialog(
            onDismissRequest = { confirm = false },
            icon = { Icon(Icons.Rounded.LockOpen, null) },
            title = { Text(stringResource(R.string.unlock_q)) },
            text = { Text(stringResource(R.string.unlock_q_text)) },
            confirmButton = { TextButton(onClick = { confirm = false; vm.setting(EngineSettings.UNLOCK_ALL, 1) }) { Text(stringResource(R.string.unlock)) } },
            dismissButton = { TextButton(onClick = { confirm = false }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}
