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
        ScreenTitle("Cheats", "Os desejos do Shenlong")

        SectionCard("Desbloquear tudo", Icons.Rounded.LockOpen, accent = Dbz.SaiyanGold) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                DragonBall(stars = 7, size = 64.dp)
                Spacer(Modifier.size(8.dp))
                Text(
                    "Todos os personagens, estágios, músicas e itens, e o máximo de Zeni. É a função de debug que os " +
                        "desenvolvedores deixaram no jogo, a mesma do port de PC.",
                    style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.onSurfaceVariant,
                    modifier = Modifier.weight(1f),
                )
            }
            Row(verticalAlignment = Alignment.CenterVertically) {
                Text(
                    if (pending) "Será aplicado na próxima vez que o jogo abrir" else "Aplicar ao abrir o jogo",
                    style = MaterialTheme.typography.titleSmall,
                    modifier = Modifier.weight(1f),
                )
                Switch(
                    checked = pending,
                    onCheckedChange = { on -> if (on) confirm = true else vm.setting(EngineSettings.UNLOCK_ALL, 0) },
                )
            }
        }

        SectionCard("Como funciona", Icons.Rounded.Info, accent = MaterialTheme.colorScheme.secondary) {
            Text(
                "O desbloqueio é gravado no save do jogo (cartão de memória) e não pode ser desfeito pelo app. Ele é " +
                    "aplicado uma vez, quando o jogo abre, e o interruptor volta a ficar desligado.\n\n" +
                    "Cheats do PCSX2 (.pnch) não se aplicam aqui: o Dragon Rage roda o código do jogo nativamente, não um emulador.",
                style = MaterialTheme.typography.bodyMedium,
                color = MaterialTheme.colorScheme.onSurfaceVariant,
            )
        }
    }

    if (confirm) {
        AlertDialog(
            onDismissRequest = { confirm = false },
            icon = { Icon(Icons.Rounded.LockOpen, null) },
            title = { Text("Desbloquear tudo?") },
            text = { Text("O save do jogo será alterado na próxima vez que ele abrir. Faça uma cópia da pasta saves se quiser guardar o progresso atual.") },
            confirmButton = { TextButton(onClick = { confirm = false; vm.setting(EngineSettings.UNLOCK_ALL, 1) }) { Text("Desbloquear") } },
            dismissButton = { TextButton(onClick = { confirm = false }) { Text("Cancelar") } },
        )
    }
}
