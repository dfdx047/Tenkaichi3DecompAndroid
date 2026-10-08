package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ExperimentalLayoutApi
import androidx.compose.foundation.layout.FlowRow
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Delete
import androidx.compose.material.icons.rounded.Edit
import androidx.compose.material.icons.rounded.Gamepad
import androidx.compose.material.icons.rounded.SportsEsports
import androidx.compose.material.icons.rounded.Bolt
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.FilterChip
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Slider
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.viewinterop.AndroidView
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.touch.Macro
import com.dfdx047.dragonrage.touch.MacroStep
import com.dfdx047.dragonrage.touch.PadSkin
import com.dfdx047.dragonrage.touch.Ps2
import com.dfdx047.dragonrage.touch.TouchConfig
import com.dfdx047.dragonrage.touch.TouchMode
import com.dfdx047.dragonrage.touch.TouchPadView
import com.dfdx047.dragonrage.ui.components.ScreenTitle
import com.dfdx047.dragonrage.ui.components.SectionCard
import com.dfdx047.dragonrage.ui.theme.Dbz
import kotlin.math.roundToInt

/** The on-screen controller: when it shows, its look, its layout, and macros. */
@Composable
fun ControlsScreen(vm: AppViewModel) {
    val c by vm.touch.collectAsStateWithLifecycle()
    var editing by remember { mutableStateOf(false) }
    var editMacro by remember { mutableStateOf<Macro?>(null) }

    Column(
        Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp).padding(bottom = 24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        ScreenTitle(stringResource(R.string.nav_controls), stringResource(R.string.controls_subtitle))

        // a small picture of the layout, in the chosen look
        Box(
            Modifier.fillMaxWidth().aspectRatio(16f / 9f).clip(MaterialTheme.shapes.large)
                .background(Color(0xFF0B1020)),
        ) {
            AndroidView(
                factory = { ctx -> TouchPadView(ctx, c.copy(mode = TouchMode.ALWAYS)) },
                update = { it.config = c.copy(mode = TouchMode.ALWAYS) },
                modifier = Modifier.fillMaxSize(),
            )
        }
        FilledTonalButton(onClick = { editing = true }, modifier = Modifier.fillMaxWidth()) {
            Icon(Icons.Rounded.Edit, null, Modifier.size(18.dp)); Spacer(Modifier.size(8.dp)); Text(stringResource(R.string.touch_edit_layout))
        }

        SectionCard(stringResource(R.string.touch_controls), Icons.Rounded.SportsEsports, accent = Dbz.KiBlue) {
            ChipRow(
                options = listOf(stringResource(R.string.touch_off) to 0, stringResource(R.string.touch_auto) to 1, stringResource(R.string.touch_always) to 2),
                selected = c.mode.ordinal,
                onSelect = { vm.updateTouch(c.copy(mode = TouchMode.entries[it])) },
            )
            Text(stringResource(R.string.touch_hint), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            Text(stringResource(R.string.touch_skin), style = MaterialTheme.typography.titleSmall)
            ChipRow(
                options = listOf(
                    stringResource(R.string.skin_playstation) to PadSkin.PLAYSTATION.ordinal,
                    stringResource(R.string.skin_xbox) to PadSkin.XBOX.ordinal,
                    stringResource(R.string.skin_nintendo) to PadSkin.NINTENDO.ordinal,
                ),
                selected = c.skin.ordinal,
                onSelect = { vm.updateTouch(c.copy(skin = PadSkin.entries[it])) },
            )
            StepSlider(stringResource(R.string.opacity), { "${it * 10}%" }, c.opacity / 10, 1..10) { vm.updateTouch(c.copy(opacity = it * 10)) }
            StepSlider(stringResource(R.string.touch_size), { "${it * 10}%" }, (c.scale * 10).roundToInt(), 5..20) { vm.updateTouch(c.copy(scale = it / 10f)) }
            ToggleRow(stringResource(R.string.touch_haptics), stringResource(R.string.touch_haptics_text), c.haptics) { vm.updateTouch(c.copy(haptics = it)) }
            ToggleRow(stringResource(R.string.touch_float), stringResource(R.string.touch_float_text), c.sticksFloat) { vm.updateTouch(c.copy(sticksFloat = it)) }
        }

        SectionCard(stringResource(R.string.macros), Icons.Rounded.Bolt, accent = Dbz.SaiyanGold) {
            Text(stringResource(R.string.macros_intro), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            c.macros.forEach { m ->
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(m.name, style = MaterialTheme.typography.bodyLarge)
                        Text(
                            m.steps.joinToString("  ›  ") { stepText(c.skin, it) } + if (m.repeat) "  ⟳" else "",
                            style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 2,
                        )
                    }
                    IconButton(onClick = { editMacro = m }) { Icon(Icons.Rounded.Edit, null) }
                    IconButton(onClick = { vm.updateTouch(c.copy(macros = c.macros.filter { it.id != m.id })) }) { Icon(Icons.Rounded.Delete, null) }
                }
            }
            FilledTonalButton(onClick = {
                editMacro = Macro(id = "m" + System.currentTimeMillis().toString(36), name = "M${c.macros.size + 1}", steps = listOf(MacroStep(Ps2.SQUARE, 100)))
            }) {
                Icon(Icons.Rounded.Add, null, Modifier.size(18.dp)); Spacer(Modifier.size(6.dp)); Text(stringResource(R.string.macro_add))
            }
        }
    }

    if (editing) {
        LayoutEditor(c, onChange = { vm.updateTouch(it) }, onDone = { editing = false })
    }
    editMacro?.let { m ->
        MacroDialog(c.skin, m, onDismiss = { editMacro = null }) { saved ->
            val list = if (c.macros.any { it.id == saved.id }) c.macros.map { if (it.id == saved.id) saved else it } else c.macros + saved
            vm.updateTouch(c.copy(macros = list))
            editMacro = null
        }
    }
}

/** The whole screen as the game's: the controller in edit mode, with its own toolbar. */
@Composable
private fun LayoutEditor(c: TouchConfig, onChange: (TouchConfig) -> Unit, onDone: () -> Unit) {
    Dialog(onDismissRequest = onDone, properties = DialogProperties(usePlatformDefaultWidth = false, decorFitsSystemWindows = false)) {
        Box(Modifier.fillMaxSize().background(Color(0xFF0B1020))) {
            Text(
                stringResource(R.string.touch_edit_hint),
                style = MaterialTheme.typography.bodyMedium, color = Color.White.copy(alpha = 0.6f),
                modifier = Modifier.align(Alignment.Center).padding(32.dp),
            )
            AndroidView(
                factory = { ctx ->
                    TouchPadView(ctx, c.copy(mode = TouchMode.ALWAYS), editing = true).apply {
                        listener = object : TouchPadView.Listener {
                            override fun onConfigChanged(config: TouchConfig) = onChange(config.copy(mode = c.mode))
                            override fun onEditDone() = onDone()
                        }
                    }
                },
                modifier = Modifier.fillMaxSize(),
            )
        }
    }
}

@OptIn(ExperimentalLayoutApi::class)
@Composable
private fun MacroDialog(skin: PadSkin, initial: Macro, onDismiss: () -> Unit, onSave: (Macro) -> Unit) {
    var name by remember { mutableStateOf(initial.name) }
    var repeat by remember { mutableStateOf(initial.repeat) }
    val steps = remember { mutableStateListOf<MacroStep>().apply { addAll(initial.steps) } }

    AlertDialog(
        onDismissRequest = onDismiss,
        title = { Text(stringResource(R.string.macro_edit)) },
        text = {
            Column(Modifier.heightIn(max = 520.dp).verticalScroll(rememberScrollState()), verticalArrangement = Arrangement.spacedBy(10.dp)) {
                OutlinedTextField(value = name, onValueChange = { name = it.take(12) }, label = { Text(stringResource(R.string.macro_name)) }, singleLine = true)
                ToggleRow(stringResource(R.string.macro_repeat), stringResource(R.string.macro_repeat_text), repeat) { repeat = it }
                steps.forEachIndexed { i, st ->
                    Column(
                        Modifier.fillMaxWidth().clip(MaterialTheme.shapes.medium)
                            .background(MaterialTheme.colorScheme.surfaceContainerHigh).padding(10.dp),
                    ) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Text(stringResource(R.string.macro_step_n, i + 1), style = MaterialTheme.typography.labelLarge, modifier = Modifier.weight(1f))
                            Text("${st.ms} ms", style = MaterialTheme.typography.labelMedium, color = MaterialTheme.colorScheme.primary)
                            IconButton(onClick = { steps.removeAt(i) }) { Icon(Icons.Rounded.Delete, null) }
                        }
                        FlowRow(horizontalArrangement = Arrangement.spacedBy(6.dp), verticalArrangement = Arrangement.spacedBy(2.dp)) {
                            Ps2.MACRO_BUTTONS.forEach { b ->
                                FilterChip(
                                    selected = st.buttons and b != 0,
                                    onClick = { steps[i] = st.copy(buttons = st.buttons xor b) },
                                    label = { Text(buttonName(skin, b)) },
                                )
                            }
                        }
                        Slider(
                            value = st.ms.toFloat(),
                            onValueChange = { steps[i] = st.copy(ms = (it / 16f).roundToInt() * 16) },
                            valueRange = 16f..1000f,
                        )
                    }
                }
                TextButton(onClick = { steps += MacroStep(0, 100) }) {
                    Icon(Icons.Rounded.Add, null, Modifier.size(18.dp)); Spacer(Modifier.size(6.dp)); Text(stringResource(R.string.macro_add_step))
                }
            }
        },
        confirmButton = {
            TextButton(
                onClick = { onSave(initial.copy(name = name.ifBlank { initial.name }, repeat = repeat, steps = steps.toList())) },
                enabled = steps.isNotEmpty(),
            ) { Text(stringResource(R.string.save)) }
        },
        dismissButton = { TextButton(onClick = onDismiss) { Text(stringResource(R.string.cancel)) } },
    )
}

private fun stepText(skin: PadSkin, s: MacroStep): String {
    val names = Ps2.MACRO_BUTTONS.filter { s.buttons and it != 0 }.map { buttonName(skin, it) }
    return (if (names.isEmpty()) "…" else names.joinToString("+")) + " ${s.ms}ms"
}

/** A PS2 button's name in the chosen console's look. */
private fun buttonName(skin: PadSkin, bit: Int): String {
    val ctl = com.dfdx047.dragonrage.touch.Ctl.entries.firstOrNull { it.bit == bit }
    return when (bit) {
        Ps2.UP -> "↑"
        Ps2.DOWN -> "↓"
        Ps2.LEFT -> "←"
        Ps2.RIGHT -> "→"
        else -> ctl?.let { TouchPadView.faceLabel(skin, it) } ?: "?"
    }
}
