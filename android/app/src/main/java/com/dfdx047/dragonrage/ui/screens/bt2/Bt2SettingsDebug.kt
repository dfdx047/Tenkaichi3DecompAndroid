package com.dfdx047.dragonrage.ui.screens.bt2

import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.rememberScrollState
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.AutoAwesome
import androidx.compose.material.icons.rounded.BugReport
import androidx.compose.material.icons.rounded.History
import androidx.compose.material.icons.rounded.HighQuality
import androidx.compose.material.icons.rounded.Speed
import androidx.compose.material.icons.rounded.VolumeUp
import androidx.compose.material.icons.rounded.Save
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.Bt2ViewModel
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.Bt2Settings
import com.dfdx047.dragonrage.ui.components.SectionCard

/** BT2's settings (Bt2Settings): picture, sound and the GPU's speed-for-accuracy options. */
@Composable
fun Bt2SettingsScreen(vm: Bt2ViewModel) {
    val s by vm.settings.collectAsStateWithLifecycle()
    Bt2Page(stringResource(R.string.nav_settings), stringResource(R.string.bt2_next_start)) {
        SectionCard(stringResource(R.string.bt2_set_picture), Icons.Rounded.HighQuality) {
            Text(stringResource(R.string.bt2_set_ss), style = MaterialTheme.typography.bodyLarge)
            // paraLLEl-GS's steps (samples per pixel; 4 and more draw at twice the resolution): no fractions.
            val steps = listOf(1 to R.string.bt2_ss_1, 2 to R.string.bt2_ss_2, 4 to R.string.bt2_ss_4, 8 to R.string.bt2_ss_8, 16 to R.string.bt2_ss_16)
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                steps.forEach { (n, label) ->
                    FilterChip(selected = s.superSampling == n, onClick = { vm.update { it.copy(superSampling = n) } }, label = { Text(stringResource(label)) })
                }
            }
            Text(stringResource(R.string.bt2_set_ss_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(stringResource(R.string.bt2_set_output), style = MaterialTheme.typography.bodyLarge)
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                Bt2Settings.OUTPUT_SCALES.forEach { n ->
                    FilterChip(selected = s.outputScale == n, onClick = { vm.update { it.copy(outputScale = n) } }, label = { Text("$n%") })
                }
            }
            Text(stringResource(R.string.bt2_set_output_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            SwitchRow(stringResource(R.string.bt2_patch_widescreen), null, s.widescreen) { on -> vm.update { it.copy(widescreen = on) } }
            SwitchRow(stringResource(R.string.bt2_set_integer), stringResource(R.string.bt2_set_integer_text), s.integerScaling) { on -> vm.update { it.copy(integerScaling = on) } }
            SwitchRow(stringResource(R.string.bt2_set_fps), stringResource(R.string.bt2_set_fps_text), s.showFrameRate) { on -> vm.update { it.copy(showFrameRate = on) } }
        }
        SectionCard(stringResource(R.string.bt2_set_speed), Icons.Rounded.Speed, accent = MaterialTheme.colorScheme.tertiary) {
            SwitchRow(stringResource(R.string.bt2_set_frameskip), stringResource(R.string.bt2_set_frameskip_text), s.frameSkip) { on -> vm.update { it.copy(frameSkip = on) } }
            Text(stringResource(R.string.bt2_set_present), style = MaterialTheme.typography.bodyLarge)
            Row(Modifier.horizontalScroll(rememberScrollState()), horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                listOf(stringResource(R.string.bt2_present_auto), "FIFO (VSync)", "Mailbox", "Immediate").forEachIndexed { n, label ->
                    FilterChip(selected = s.presentMode == n, onClick = { vm.update { it.copy(presentMode = n) } }, label = { Text(label) })
                }
            }
            Text(stringResource(R.string.bt2_set_present_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            SwitchRow(stringResource(R.string.bt2_set_light), stringResource(R.string.bt2_set_light_text), s.blendDemote) { on -> vm.update { it.copy(blendDemote = on) } }
            SwitchRow(stringResource(R.string.bt2_set_nomip), stringResource(R.string.bt2_set_nomip_text), s.noMipmaps) { on -> vm.update { it.copy(noMipmaps = on) } }
            SwitchRow(stringResource(R.string.bt2_set_unsynced), stringResource(R.string.bt2_set_unsynced_text), s.unsyncedReadbacks) { on -> vm.update { it.copy(unsyncedReadbacks = on) } }
            SwitchRow(stringResource(R.string.bt2_set_promote), stringResource(R.string.bt2_set_promote_text), s.backbufferPromotion) { on -> vm.update { it.copy(backbufferPromotion = on) } }
            Text(stringResource(R.string.bt2_set_live), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            SwitchRow(stringResource(R.string.bt2_patch_60), stringResource(R.string.bt2_patch_60_text), s.sixtyFps) { on -> vm.update { it.copy(sixtyFps = on) } }
        }
        SectionCard(stringResource(R.string.bt2_fx_title), Icons.Rounded.AutoAwesome, accent = MaterialTheme.colorScheme.primary) {
            SwitchRow(stringResource(R.string.bt2_fx_screen), stringResource(R.string.bt2_fx_screen_text), s.screenEffects) { on -> vm.update { it.copy(screenEffects = on) } }
            SwitchRow(stringResource(R.string.bt2_fx_fog), stringResource(R.string.bt2_fx_fog_text), s.fog) { on -> vm.update { it.copy(fog = on) } }
            Text(stringResource(R.string.bt2_fx_note), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        SectionCard(stringResource(R.string.bt2_set_sound), Icons.Rounded.VolumeUp, accent = MaterialTheme.colorScheme.secondary) {
            Volume(stringResource(R.string.bt2_set_master), s.masterVolume) { v -> vm.update { it.copy(masterVolume = v) } }
            Volume(stringResource(R.string.bt2_set_music), s.musicVolume) { v -> vm.update { it.copy(musicVolume = v) } }
            Volume(stringResource(R.string.bt2_set_effects), s.effectsVolume) { v -> vm.update { it.copy(effectsVolume = v) } }
        }
    }
}

@Composable
private fun Volume(label: String, value: Int, onChange: (Int) -> Unit) {
    Column {
        Text("$label: $value%", style = MaterialTheme.typography.bodyMedium)
        Slider(value = value.toFloat(), onValueChange = { onChange(it.toInt()) }, valueRange = 0f..100f, steps = 19, modifier = Modifier.fillMaxWidth())
    }
}

/** The logs of BT2's runs: the last one's end, the kept ones, and a report for sending. */
@Composable
fun Bt2DebugScreen(vm: Bt2ViewModel) {
    val last by vm.lastRun.collectAsStateWithLifecycle()
    val runs by vm.runHistory.collectAsStateWithLifecycle()
    Bt2Page(stringResource(R.string.nav_debug), stringResource(R.string.bt2_debug_sub)) {
        FilledTonalButton(onClick = { vm.saveReport() }) {
            Icon(Icons.Rounded.Save, null); Text("  " + stringResource(R.string.bt2_debug_save))
        }
        SectionCard(stringResource(R.string.bt2_debug_last_run), Icons.Rounded.BugReport) {
            if (last.isEmpty()) {
                Text(stringResource(R.string.bt2_debug_no_run), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                Text(
                    last.joinToString("\n"), fontFamily = FontFamily.Monospace, fontSize = 11.sp, lineHeight = 14.sp,
                    modifier = Modifier.horizontalScroll(rememberScrollState()),
                )
            }
        }
        SectionCard(stringResource(R.string.bt2_debug_runs, runs.size), Icons.Rounded.History, accent = MaterialTheme.colorScheme.secondary) {
            runs.forEach { Text(it.name, style = MaterialTheme.typography.bodySmall) }
            Text(stringResource(R.string.bt2_debug_runs_where), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}
