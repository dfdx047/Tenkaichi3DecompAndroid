package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.Game
import com.dfdx047.dragonrage.ui.components.DragonBall
import com.dfdx047.dragonrage.ui.theme.Dbz

/**
 * The first screen: which game to open. Both are in the same app with the same pages; each has its own folder,
 * disc image, saves and settings, its own engine and its own colours (Game).
 */
@Composable
fun WelcomeScreen(last: Game, onPick: (Game) -> Unit) {
    val wide = LocalConfiguration.current.screenWidthDp >= 600
    Box(
        Modifier.fillMaxSize().background(Dbz.Night).windowInsetsPadding(WindowInsets.safeDrawing),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier.widthIn(max = 900.dp).verticalScroll(rememberScrollState()).padding(20.dp),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(18.dp),
        ) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                DragonBall(stars = 4, size = 64.dp)
                Spacer(Modifier.size(10.dp))
                Column {
                    Text("DRAGON RAGE", style = MaterialTheme.typography.headlineMedium, fontWeight = FontWeight.Black, color = Dbz.SaiyanGold)
                    Text(stringResource(R.string.welcome_pick), style = MaterialTheme.typography.bodyLarge, color = Color(0xFFC4C3D6))
                }
            }
            val cards: @Composable (Modifier) -> Unit = { m ->
                GameCard(
                    Game.BT3, "Budokai Tenkaichi 3", stringResource(R.string.welcome_bt3), "SLUS-21678",
                    listOf(Color(0xFFFF8A1F), Color(0xFFB8420A)), Dbz.SaiyanGold, last == Game.BT3, m, onPick,
                )
                GameCard(
                    Game.BT4, "Budokai Tenkaichi 4", stringResource(R.string.welcome_bt4), "SLUS-219.78 · Beta 14 Rev2",
                    listOf(Color(0xFF2FB8F0), Color(0xFF5A2A8C)), Color(0xFFD3A6FF), last == Game.BT4, m, onPick,
                )
                GameCard(
                    Game.BT2, "Budokai Tenkaichi 2", stringResource(R.string.welcome_bt2), "SLUS-21441",
                    listOf(Color(0xFF2FB85A), Color(0xFF0B5A33)), Color(0xFFA6E86B), last == Game.BT2, m, onPick,
                )
            }
            if (wide) {
                Row(horizontalArrangement = Arrangement.spacedBy(16.dp), modifier = Modifier.fillMaxWidth()) {
                    cards(Modifier.weight(1f))
                }
            } else {
                Column(verticalArrangement = Arrangement.spacedBy(16.dp), modifier = Modifier.fillMaxWidth()) {
                    cards(Modifier.fillMaxWidth())
                }
            }
            Text(
                stringResource(R.string.welcome_note),
                style = MaterialTheme.typography.bodySmall, color = Color(0xFF9A99B0),
            )
        }
    }
}

@Composable
private fun GameCard(
    game: Game, title: String, text: String, disc: String, colors: List<Color>, accent: Color, last: Boolean,
    modifier: Modifier, onPick: (Game) -> Unit,
) {
    val shape = RoundedCornerShape(22.dp)
    Column(
        modifier
            .clip(shape)
            .background(Brush.linearGradient(colors))
            .border(if (last) 3.dp else 1.dp, if (last) accent else Color.White.copy(alpha = 0.25f), shape)
            .clickable { onPick(game) }
            .padding(20.dp),
        verticalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        Text(title.uppercase(), style = MaterialTheme.typography.titleLarge, fontWeight = FontWeight.Black, color = Color.White)
        Text(disc, style = MaterialTheme.typography.labelMedium, color = Color.White.copy(alpha = 0.8f))
        Spacer(Modifier.height(2.dp))
        Text(text, style = MaterialTheme.typography.bodyMedium, color = Color.White)
        if (last) {
            Text(stringResource(R.string.welcome_last), style = MaterialTheme.typography.labelLarge, color = accent, fontWeight = FontWeight.Bold)
        }
    }
}
