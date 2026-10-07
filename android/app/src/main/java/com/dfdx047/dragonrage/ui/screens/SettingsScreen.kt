package com.dfdx047.dragonrage.ui.screens

import android.os.Build
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.AutoAwesome
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.Palette
import androidx.compose.material.icons.rounded.Speed
import androidx.compose.material.icons.rounded.SportsEsports
import androidx.compose.material.icons.rounded.Tv
import androidx.compose.material.icons.automirrored.rounded.VolumeUp
import androidx.compose.material3.FilterChip
import androidx.compose.material3.HorizontalDivider
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Slider
import androidx.compose.material3.Switch
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableFloatStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.BuildConfig
import com.dfdx047.dragonrage.data.EngineSettings
import com.dfdx047.dragonrage.ui.components.ScreenTitle
import com.dfdx047.dragonrage.ui.components.SectionCard
import com.dfdx047.dragonrage.ui.theme.Dbz
import com.dfdx047.dragonrage.ui.theme.ThemeMode
import kotlin.math.roundToInt

@Composable
fun SettingsScreen(vm: AppViewModel) {
    val s by vm.engineSettings.collectAsStateWithLifecycle()
    val ui by vm.uiState.collectAsStateWithLifecycle()
    fun get(k: String, d: Int) = s[k] ?: d

    Column(
        Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp).padding(bottom = 24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        ScreenTitle("Ajustes", "Valem na próxima vez que o jogo abrir")

        SectionCard("Vídeo", Icons.Rounded.Tv) {
            val scale = get(EngineSettings.SCALE, EngineSettings.SCALE_DEFAULT)
            StepSlider(
                label = "Resolução interna",
                format = { "${it}x · ${512 * it}×${448 * it}" },
                value = scale, range = 1..8,
                onChange = { vm.setting(EngineSettings.SCALE, it) },
            )
            Text(
                "1x é a resolução do PS2. Em celulares e portáteis, 2x ou 3x costuma ser o ponto ideal.",
                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            Text("Proporção da tela", style = MaterialTheme.typography.titleSmall)
            ChipRow(
                options = listOf("4:3" to 1333, "16:10" to 1600, "16:9" to 1778, "21:9" to 2389, "32:9" to 3556),
                selected = get(EngineSettings.ASPECT, EngineSettings.ASPECT_DEFAULT),
                onSelect = { vm.setting(EngineSettings.ASPECT, it) },
            )
            ToggleRow("Mostrar FPS", "Contador de quadros por segundo durante o jogo", get(EngineSettings.METER, 0) != 0) {
                vm.setting(EngineSettings.METER, if (it) 1 else 0)
            }
        }

        SectionCard("Efeitos do PS2", Icons.Rounded.AutoAwesome, accent = Dbz.SaiyanGold) {
            val fxOff = get(EngineSettings.FX_OFF, 0)
            listOf(
                Triple(1, "Contorno", "A linha preta em volta dos lutadores"),
                Triple(2, "Silhueta atrás do cenário", "Mostra o lutador escondido atrás de objetos"),
                Triple(4, "Névoa de profundidade", "A cor que tinge o que está longe"),
                Triple(8, "Brilho e reflexo", "O bloom em volta de coisas brilhantes e o reflexo do céu"),
                Triple(16, "Desfoque à distância", "O foco suave no cenário distante"),
            ).forEach { (bit, title, sub) ->
                ToggleRow(title, sub, fxOff and bit == 0) { on ->
                    vm.setting(EngineSettings.FX_OFF, if (on) fxOff and bit.inv() else fxOff or bit)
                }
            }
            val glow = get(EngineSettings.GLOW, EngineSettings.GLOW_DEFAULT)
            StepSlider("Intensidade do brilho", { "${it * 10}%" }, glow / 10, 0..20) { vm.setting(EngineSettings.GLOW, it * 10) }
        }

        SectionCard("Áudio", Icons.AutoMirrored.Rounded.VolumeUp, accent = Dbz.ShenronGreen) {
            val music = get(EngineSettings.MUSIC, 100)
            StepSlider("Música", { "${it * 10}%" }, music / 10, 0..20) { vm.setting(EngineSettings.MUSIC, it * 10) }
            val fx = get(EngineSettings.EFFECTS, 100)
            StepSlider("Efeitos e vozes", { "${it * 10}%" }, fx / 10, 0..20) { vm.setting(EngineSettings.EFFECTS, it * 10) }
        }

        SectionCard("Controles", Icons.Rounded.SportsEsports, accent = Dbz.KiBlue) {
            Text("Controles na tela", style = MaterialTheme.typography.titleSmall)
            ChipRow(
                options = listOf("Desligado" to 0, "Automático" to 1, "Sempre" to 2),
                selected = get(EngineSettings.TOUCH, 1),
                onSelect = { vm.setting(EngineSettings.TOUCH, it) },
            )
            Text(
                "Automático: aparecem só quando não há controle conectado (no Odin e em portáteis, ficam escondidos).",
                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            val alpha = get(EngineSettings.TOUCH_OPACITY, 60)
            StepSlider("Opacidade", { "${it * 10}%" }, alpha / 10, 1..10) { vm.setting(EngineSettings.TOUCH_OPACITY, it * 10) }
        }

        SectionCard("Desempenho", Icons.Rounded.Speed, accent = MaterialTheme.colorScheme.secondary) {
            ToggleRow("Modo 60 FPS", "Em breve: o jogo roda a lógica a 30 quadros por segundo", false, enabled = false) {}
        }

        SectionCard("Aparência do app", Icons.Rounded.Palette, accent = MaterialTheme.colorScheme.tertiary) {
            Text("Tema", style = MaterialTheme.typography.titleSmall)
            ChipRow(
                options = listOf("Escuro" to ThemeMode.DARK.ordinal, "Claro" to ThemeMode.LIGHT.ordinal, "Sistema" to ThemeMode.SYSTEM.ordinal),
                selected = ui.theme.ordinal,
                onSelect = { vm.setTheme(ThemeMode.entries[it]) },
            )
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                ToggleRow("Cores do Material You", "Usa as cores do seu papel de parede no lugar das cores do Goku", ui.dynamicColor) {
                    vm.setDynamicColor(it)
                }
            }
        }

        SectionCard("Sobre", Icons.Rounded.Info, accent = MaterialTheme.colorScheme.outline) {
            Text("Dragon Rage ${BuildConfig.VERSION_NAME}", style = MaterialTheme.typography.titleSmall)
            Text(
                "Port para Android do Tenkaichi3Decomp, o port nativo de PC de Dragon Ball Z: Budokai Tenkaichi 3 " +
                    "baseado na decompilação BT3-Decompiled. Não é afiliado aos desenvolvedores, distribuidoras ou " +
                    "detentores dos direitos do jogo, e não contém nenhum dado do jogo: é preciso a sua própria cópia.",
                style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
            HorizontalDivider()
            Text("Pasta do jogo", style = MaterialTheme.typography.titleSmall)
            Text(vm.paths.root.path, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

@Composable
private fun ToggleRow(title: String, subtitle: String, checked: Boolean, enabled: Boolean = true, onChange: (Boolean) -> Unit) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        Column(Modifier.weight(1f).padding(end = 12.dp)) {
            Text(title, style = MaterialTheme.typography.bodyLarge)
            Text(subtitle, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
        Switch(checked = checked, onCheckedChange = onChange, enabled = enabled)
    }
}

/** A slider over whole steps that writes only when the finger lifts. */
@Composable
private fun StepSlider(label: String, format: (Int) -> String, value: Int, range: IntRange, onChange: (Int) -> Unit) {
    var drag by remember(value) { mutableFloatStateOf(value.toFloat()) }
    Column {
        Row {
            Text(label, style = MaterialTheme.typography.bodyLarge, modifier = Modifier.weight(1f))
            Text(
                format(drag.roundToInt()),
                style = MaterialTheme.typography.labelLarge, color = MaterialTheme.colorScheme.primary,
            )
        }
        Slider(
            value = drag,
            onValueChange = { drag = it },
            onValueChangeFinished = { onChange(drag.roundToInt()) },
            valueRange = range.first.toFloat()..range.last.toFloat(),
            steps = (range.last - range.first - 1).coerceAtLeast(0),
        )
    }
}

@Composable
private fun ChipRow(options: List<Pair<String, Int>>, selected: Int, onSelect: (Int) -> Unit) {
    // the nearest option counts as selected (an aspect written by the engine may be in between)
    val best = options.minByOrNull { kotlin.math.abs(it.second - selected) }?.second
    Row(horizontalArrangement = Arrangement.spacedBy(8.dp), modifier = Modifier.fillMaxWidth().horizontalScroll(rememberScrollState())) {
        options.forEach { (label, v) ->
            FilterChip(selected = v == best, onClick = { onSelect(v) }, label = { Text(label) })
        }
    }
}
