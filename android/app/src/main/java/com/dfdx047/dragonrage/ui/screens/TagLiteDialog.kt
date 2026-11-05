package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Speed
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import kotlinx.coroutines.delay
import androidx.compose.ui.Modifier
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.platform.LocalInputModeManager
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.ui.theme.Dbz

/** On a weak GPU (GpuProfiles tiers 0 and 1), once: "turn on the Tag Team's light graphics?", with "Turn on" chosen
 *  (focused, so a gamepad's A takes it) and "Not now". Shown after the GPU settings offer, never with it. */
@Composable
fun TagLiteDialog(vm: AppViewModel) {
    val ask by vm.tagLiteOffer.collectAsStateWithLifecycle()
    val gpuOffer by vm.gpuOffer.collectAsStateWithLifecycle()
    if (!ask || gpuOffer != null) return
    val on = remember { FocusRequester() }
    AlertDialog(
        onDismissRequest = { vm.answerTagLiteOffer(false) },
        icon = { Icon(Icons.Rounded.Speed, null, tint = Dbz.SaiyanGold) },
        title = { Text(stringResource(R.string.tag_lite)) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(6.dp)) {
                Text(stringResource(R.string.tag_lite_offer), style = MaterialTheme.typography.bodyMedium)
                Text(stringResource(R.string.tag_lite_offer_later), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        },
        confirmButton = {
            // (in the dialog's own window, and again when a gamepad or keyboard key leaves touch mode: else the first key
            // puts the focus on "Not now", the first button)
            val mode = LocalInputModeManager.current.inputMode
            LaunchedEffect(mode) { delay(80); runCatching { on.requestFocus() } }
            Button(onClick = { vm.answerTagLiteOffer(true) }, modifier = Modifier.focusRequester(on)) { Text(stringResource(R.string.tag_lite_offer_yes)) }
        },
        dismissButton = { TextButton(onClick = { vm.answerTagLiteOffer(false) }) { Text(stringResource(R.string.tag_lite_offer_no)) } },
    )
}
