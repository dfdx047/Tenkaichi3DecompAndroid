package com.dfdx047.dragonrage.ui.screens

import android.app.ActivityManager
import android.content.Context
import android.os.Build
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.AutoFixHigh
import androidx.compose.material.icons.rounded.Insights
import androidx.compose.material.icons.rounded.PhoneAndroid
import androidx.compose.material.icons.rounded.Refresh
import androidx.compose.material3.FilledTonalButton
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedButton
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.BuildConfig
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.EngineSettings
import com.dfdx047.dragonrage.data.GpuProfiles
import com.dfdx047.dragonrage.data.Game
import com.dfdx047.dragonrage.ui.components.ScreenTitle
import com.dfdx047.dragonrage.ui.components.SectionCard
import com.dfdx047.dragonrage.ui.theme.Dbz

/** The device, the GPU's tier and its recommended settings, the last run's speed, and the log for a report. */
@Composable
fun DebugScreen(vm: AppViewModel) {
    val gpu by vm.gpuName.collectAsStateWithLifecycle()
    val s by vm.engineSettings.collectAsStateWithLifecycle()
    val perf by vm.perf.collectAsStateWithLifecycle()
    val rankedDebugUrl by vm.rankedDebugUrl.collectAsStateWithLifecycle()
    var rankedDebugDraft by remember(rankedDebugUrl) { mutableStateOf(rankedDebugUrl) }
    var rankedDebugError by remember { mutableStateOf(false) }
    val context = LocalContext.current
    LaunchedEffect(Unit) { vm.refreshPerf() }

    Column(
        Modifier.verticalScroll(rememberScrollState()).padding(horizontal = 16.dp).padding(bottom = 24.dp),
        verticalArrangement = Arrangement.spacedBy(16.dp),
    ) {
        ScreenTitle(stringResource(R.string.nav_debug), stringResource(R.string.debug_sub))

        SectionCard(stringResource(R.string.debug_device), Icons.Rounded.PhoneAndroid, accent = Dbz.KiBlue) {
            val ram = remember {
                val mem = ActivityManager.MemoryInfo()
                (context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager).getMemoryInfo(mem)
                mem.totalMem / (1024 * 1024)
            }
            val tier = GpuProfiles.tierOf(gpu)
            InfoLine(stringResource(R.string.debug_model), "${Build.MANUFACTURER} ${Build.MODEL}")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) InfoLine("SoC", "${Build.SOC_MANUFACTURER} ${Build.SOC_MODEL}")
            InfoLine("Android", "${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
            InfoLine("RAM", "$ram MB")
            InfoLine("GPU", gpu.ifEmpty { "?" })
            InfoLine(stringResource(R.string.debug_tier), tierName(tier))
            InfoLine(stringResource(R.string.renderer), if (s[EngineSettings.GPU_API] == 1) "OpenGL ES" else "Vulkan")
        }

        if (BuildConfig.DEBUG && vm.game == Game.BT3) {
            SectionCard(stringResource(R.string.ranked_debug_title), Icons.Rounded.Insights, accent = Dbz.SaiyanGold) {
                Text(stringResource(R.string.ranked_debug_help), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                OutlinedTextField(
                    value = rankedDebugDraft,
                    onValueChange = { rankedDebugDraft = it; rankedDebugError = false },
                    modifier = Modifier.fillMaxWidth(),
                    singleLine = true,
                    label = { Text(stringResource(R.string.ranked_debug_url)) },
                    placeholder = { Text("http://127.0.0.1:8787") },
                )
                if (rankedDebugError) Text(stringResource(R.string.ranked_debug_invalid), color = MaterialTheme.colorScheme.error)
                Row(horizontalArrangement = Arrangement.spacedBy(8.dp)) {
                    OutlinedButton(onClick = { rankedDebugError = !vm.setRankedDebugUrl(rankedDebugDraft) }) {
                        Text(stringResource(R.string.ranked_debug_apply))
                    }
                    OutlinedButton(onClick = {
                        rankedDebugDraft = ""
                        rankedDebugError = !vm.setRankedDebugUrl("")
                    }) { Text(stringResource(R.string.ranked_debug_disable)) }
                }
            }
        }

        SectionCard(stringResource(R.string.debug_gpu_opt), Icons.Rounded.AutoFixHigh, accent = Dbz.SaiyanGold) {
            Text(stringResource(R.string.debug_gpu_opt_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            FilledTonalButton(onClick = { vm.offerGpuProfile() }) { Text(stringResource(R.string.debug_gpu_opt_button)) }
        }

        SectionCard(stringResource(R.string.debug_perf), Icons.Rounded.Insights, accent = Dbz.ShenronGreen) {
            ToggleRow(stringResource(R.string.debug_perf_log), stringResource(R.string.debug_perf_log_text), (s["perf_log"] ?: 1) != 0) {
                vm.setting("perf_log", if (it) 1 else 0)
            }
            val p = perf
            if (p == null || (p.fightSamples == 0 && p.lines.isEmpty())) {
                Text(stringResource(R.string.debug_perf_none), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
            } else {
                if (p.fightSamples > 0) {
                    val color = when {
                        p.fightMin >= 95 -> Dbz.ShenronGreen
                        p.fightAvg >= 85 -> Dbz.SaiyanGold
                        else -> MaterialTheme.colorScheme.error
                    }
                    Text(stringResource(R.string.debug_perf_fight, p.fightAvg, p.fightMin, p.fightFps), style = MaterialTheme.typography.titleMedium, color = color)
                    Text(
                        stringResource(if (p.fightMin >= 95) R.string.debug_perf_ok else R.string.debug_perf_slow),
                        style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant,
                    )
                }
                if (p.menuAvg > 0) Text(stringResource(R.string.debug_perf_menus, p.menuAvg), style = MaterialTheme.typography.bodyMedium)
                if (p.gpu.isNotEmpty() || p.resolution.isNotEmpty()) {
                    Text("${p.gpu} · ${p.renderer} · ${p.resolution}", style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
                }
                Column(Modifier.fillMaxWidth()) {
                    p.lines.takeLast(6).forEach {
                        Text(it, style = MaterialTheme.typography.labelSmall.copy(fontFamily = FontFamily.Monospace), color = MaterialTheme.colorScheme.onSurfaceVariant, maxLines = 2)
                    }
                }
            }
            OutlinedButton(onClick = { vm.refreshPerf() }) {
                Icon(Icons.Rounded.Refresh, null, Modifier.size(18.dp)); Spacer(Modifier.size(6.dp)); Text(stringResource(R.string.drivers_refresh))
            }
        }

        LogCard(vm)
    }
}

@Composable
private fun InfoLine(label: String, value: String) {
    Row(Modifier.fillMaxWidth()) {
        Text(label, style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant, modifier = Modifier.weight(0.4f))
        Text(value, style = MaterialTheme.typography.bodyMedium, modifier = Modifier.weight(0.6f))
    }
}

@Composable
internal fun tierName(tier: Int): String = stringResource(
    when (tier) {
        0 -> R.string.tier_0
        1 -> R.string.tier_1
        2 -> R.string.tier_2
        3 -> R.string.tier_3
        4 -> R.string.tier_4
        else -> R.string.tier_unknown
    },
)
