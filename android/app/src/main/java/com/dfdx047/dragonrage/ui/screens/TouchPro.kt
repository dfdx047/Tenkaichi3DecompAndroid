package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.size
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.automirrored.rounded.HelpOutline
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Save
import androidx.compose.material.icons.rounded.TouchApp
import androidx.compose.material.icons.rounded.Upload
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
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
import androidx.compose.ui.unit.dp
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.touch.Ctl
import com.dfdx047.dragonrage.touch.Haptic
import com.dfdx047.dragonrage.touch.StickMode
import com.dfdx047.dragonrage.touch.TOGGLE_CANDIDATES
import com.dfdx047.dragonrage.touch.TouchConfig
import com.dfdx047.dragonrage.touch.TouchPreset
import com.dfdx047.dragonrage.touch.TouchProfile
import com.dfdx047.dragonrage.ui.components.SectionCard
import com.dfdx047.dragonrage.ui.theme.Dbz

/*
 * "Toque Pro" (1.0): the on-screen controller's tuning, for players without a physical controller: ready-made
 * profiles, left-handed mirror, the stick (mode, dead zone, range, zone, 8 directions), the buttons (touch area, sliding,
 * toggles), vibration and the player's own saved profiles, with the guide (TouchGuide). The same section in BT3's and
 * BT2's Controls pages; the values are TouchConfig's, read by TouchPadView / ClassicPadView through TouchTuning.
 */

/** A held button's friendly name (the action it has in a fight). */
@Composable
fun toggleName(c: Ctl): String = stringResource(
    when (c) {
        Ctl.L2 -> R.string.tp_act_charge
        Ctl.CIRCLE -> R.string.tp_act_guard
        Ctl.R1 -> R.string.tp_act_up
        Ctl.R2 -> R.string.tp_act_down
        Ctl.L1 -> R.string.tp_act_lock
        else -> R.string.tp_act_other
    },
)

@OptIn(ExperimentalLayoutApi::class)
@Composable
fun TouchProSection(c: TouchConfig, onChange: (TouchConfig) -> Unit) {
    var guide by remember { mutableStateOf(false) }
    var saving by remember { mutableStateOf(false) }

    SectionCard(stringResource(R.string.tp_title), Icons.Rounded.TouchApp, accent = Dbz.SaiyanGold) {
        Text(stringResource(R.string.tp_intro), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        OutlinedButton(onClick = { guide = true }, modifier = Modifier.fillMaxWidth()) {
            Icon(Icons.AutoMirrored.Rounded.HelpOutline, null, Modifier.size(18.dp)); Spacer(Modifier.size(8.dp)); Text(stringResource(R.string.tp_guide))
        }

        // (a) ready-made profiles and the left-handed mirror
        Text(stringResource(R.string.tp_presets), style = MaterialTheme.typography.titleSmall)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TouchPreset.entries.forEach { p ->
                FilterChip(selected = c.withPreset(p).sameTuning(c), onClick = { onChange(c.withPreset(p)) }, label = { Text(presetName(p)) })
            }
        }
        Text(presetText(TouchPreset.entries.firstOrNull { c.withPreset(it).sameTuning(c) }), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        ToggleRow(stringResource(R.string.tp_left), stringResource(R.string.tp_left_text), c.leftHanded) { onChange(c.mirrored()) }

        HorizontalDivider()
        // (b) the stick
        Text(stringResource(R.string.tp_stick), style = MaterialTheme.typography.titleSmall)
        ChipRow(
            options = listOf(
                stringResource(R.string.tp_stick_fixed) to StickMode.FIXED.ordinal,
                stringResource(R.string.tp_stick_floating) to StickMode.FLOATING.ordinal,
                stringResource(R.string.tp_stick_dynamic) to StickMode.DYNAMIC.ordinal,
            ),
            selected = c.stickMode.ordinal,
            onSelect = { onChange(c.copy(stickMode = StickMode.entries[it], sticksFloat = it != StickMode.FIXED.ordinal)) },
        )
        Text(
            stringResource(
                when (c.stickMode) {
                    StickMode.FIXED -> R.string.tp_stick_fixed_text
                    StickMode.FLOATING -> R.string.tp_stick_floating_text
                    StickMode.DYNAMIC -> R.string.tp_stick_dynamic_text
                },
            ),
            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
        )
        StepSlider(stringResource(R.string.tp_dead), { "$it%" }, c.deadZone, 0..40) { onChange(c.copy(deadZone = it)) }
        Text(stringResource(R.string.tp_dead_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        StepSlider(stringResource(R.string.tp_range), { "${it * 5}%" }, c.stickRange / 5, 8..20) { onChange(c.copy(stickRange = it * 5)) }
        Text(stringResource(R.string.tp_range_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        if (c.stickMode != StickMode.FIXED) {
            StepSlider(stringResource(R.string.tp_zone), { "${it * 5}%" }, c.stickZone / 5, 5..12) { onChange(c.copy(stickZone = it * 5)) }
            Text(stringResource(R.string.tp_zone_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        ToggleRow(stringResource(R.string.tp_snap8), stringResource(R.string.tp_snap8_text), c.snap8) { onChange(c.copy(snap8 = it)) }

        HorizontalDivider()
        // (c) the buttons
        Text(stringResource(R.string.tp_buttons), style = MaterialTheme.typography.titleSmall)
        StepSlider(stringResource(R.string.tp_hit), { "${it * 5}%" }, c.hitArea / 5, 18..32) { onChange(c.copy(hitArea = it * 5)) }
        Text(stringResource(R.string.tp_hit_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        ToggleRow(stringResource(R.string.tp_slide), stringResource(R.string.tp_slide_text), c.slideAll) { onChange(c.copy(slideAll = it)) }
        Text(stringResource(R.string.tp_toggle), style = MaterialTheme.typography.bodyLarge)
        Text(stringResource(R.string.tp_toggle_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        FlowRow(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
            TOGGLE_CANDIDATES.forEach { b ->
                val on = b in c.toggles
                FilterChip(selected = on, onClick = { onChange(c.copy(toggles = if (on) c.toggles - b else c.toggles + b)) }, label = { Text(toggleName(b)) })
            }
        }

        HorizontalDivider()
        // (d) vibration
        Text(stringResource(R.string.tp_haptic), style = MaterialTheme.typography.titleSmall)
        ChipRow(
            options = listOf(
                stringResource(R.string.tp_haptic_off) to Haptic.OFF.ordinal,
                stringResource(R.string.tp_haptic_light) to Haptic.LIGHT.ordinal,
                stringResource(R.string.tp_haptic_strong) to Haptic.STRONG.ordinal,
            ),
            selected = c.haptic.ordinal,
            onSelect = { onChange(c.copy(haptic = Haptic.entries[it], haptics = it != Haptic.OFF.ordinal)) },
        )

        HorizontalDivider()
        // (e) the player's own profiles
        Text(stringResource(R.string.tp_mine), style = MaterialTheme.typography.titleSmall)
        if (c.profiles.isEmpty()) {
            Text(stringResource(R.string.tp_mine_empty), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        c.profiles.forEachIndexed { i, p ->
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(p.name, style = MaterialTheme.typography.bodyLarge, modifier = Modifier.weight(1f))
                TextButton(onClick = { onChange(p.apply(c)) }) {
                    Icon(Icons.Rounded.Upload, null, Modifier.size(18.dp)); Spacer(Modifier.size(4.dp)); Text(stringResource(R.string.tp_load))
                }
                IconButton(onClick = { onChange(c.copy(profiles = c.profiles.filterIndexed { j, _ -> j != i })) }) {
                    Icon(Icons.Rounded.Delete, stringResource(R.string.tp_delete))
                }
            }
        }
        FilledTonalButton(onClick = { saving = true }) {
            Icon(Icons.Rounded.Save, null, Modifier.size(18.dp)); Spacer(Modifier.size(6.dp)); Text(stringResource(R.string.tp_save))
        }
    }

    if (guide) TouchGuide(onDismiss = { guide = false })
    if (saving) {
        var name by remember { mutableStateOf("") }
        AlertDialog(
            onDismissRequest = { saving = false },
            title = { Text(stringResource(R.string.tp_save)) },
            text = {
                Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
                    Text(stringResource(R.string.tp_save_text), style = MaterialTheme.typography.bodySmall)
                    OutlinedTextField(value = name, onValueChange = { name = it.take(24) }, singleLine = true, label = { Text(stringResource(R.string.tp_name)) })
                }
            },
            confirmButton = {
                TextButton(enabled = name.isNotBlank(), onClick = {
                    val n = name.trim()
                    // (the same name again replaces that profile)
                    onChange(c.copy(profiles = c.profiles.filter { it.name != n } + TouchProfile.of(n, c)))
                    saving = false
                }) { Text(stringResource(R.string.tp_save_ok)) }
            },
            dismissButton = { TextButton(onClick = { saving = false }) { Text(stringResource(R.string.cancel)) } },
        )
    }
}

/** Whether two configurations have the same tuning (what a ready-made profile sets). */
private fun TouchConfig.sameTuning(o: TouchConfig) = stickMode == o.stickMode && deadZone == o.deadZone && stickRange == o.stickRange &&
    stickZone == o.stickZone && snap8 == o.snap8 && hitArea == o.hitArea && slideAll == o.slideAll && toggles == o.toggles && haptic == o.haptic

@Composable
private fun presetName(p: TouchPreset): String = stringResource(
    when (p) {
        TouchPreset.STANDARD -> R.string.tp_preset_standard
        TouchPreset.COMPETITIVE -> R.string.tp_preset_competitive
        TouchPreset.RELAXED -> R.string.tp_preset_relaxed
        TouchPreset.CLASSIC -> R.string.tp_preset_classic
    },
)

@Composable
private fun presetText(p: TouchPreset?): String = stringResource(
    when (p) {
        TouchPreset.STANDARD -> R.string.tp_preset_standard_text
        TouchPreset.COMPETITIVE -> R.string.tp_preset_competitive_text
        TouchPreset.RELAXED -> R.string.tp_preset_relaxed_text
        TouchPreset.CLASSIC -> R.string.tp_preset_classic_text
        null -> R.string.tp_preset_custom_text
    },
)
