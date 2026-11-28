package com.dfdx047.dragonrage.ui.screens

import androidx.annotation.DrawableRes
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
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
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.Game
import com.dfdx047.dragonrage.ui.components.DragonBall
import com.dfdx047.dragonrage.ui.theme.Dbz

// Cores que faltam na paleta Dbz: o azul profundo do BT4 e o verde profundo do BT2.
private val KiBlueDeep = Color(0xFF5A2A8C)
private val ShenronGreenDeep = Color(0xFF0B5A33)

/**
 * The first screen: which game to open. Each game is in the same app with the same pages; each has its own folder,
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
                    game = Game.BT3,
                    number = "3",
                    tag = R.string.welcome_tag_original,
                    text = stringResource(R.string.welcome_bt3),
                    disc = "SLUS-21678",
                    accent = Dbz.SaiyanGold,
                    deep = Dbz.GiOrangeDeep,
                    art = R.drawable.welcome_art_bt3,
                    last = last == Game.BT3,
                    wide = wide,
                    modifier = m,
                    onPick = onPick,
                )
                GameCard(
                    game = Game.BT4,
                    number = "4",
                    tag = R.string.welcome_tag_modified,
                    text = stringResource(R.string.welcome_bt4),
                    disc = "SLUS-219.78 · Beta 14 Rev2",
                    accent = Dbz.KiBlue,
                    deep = KiBlueDeep,
                    art = R.drawable.welcome_art_bt4,
                    last = last == Game.BT4,
                    wide = wide,
                    modifier = m,
                    onPick = onPick,
                )
                GameCard(
                    game = Game.BT2,
                    number = "2",
                    tag = R.string.welcome_tag_classic,
                    text = stringResource(R.string.welcome_bt2),
                    disc = "SLUS-21441",
                    accent = Dbz.ShenronGreen,
                    deep = ShenronGreenDeep,
                    art = R.drawable.welcome_art_bt2,
                    artAlignment = Alignment.CenterStart,
                    last = last == Game.BT2,
                    wide = wide,
                    modifier = m,
                    onPick = onPick,
                )
            }
            if (wide) {
                Row(
                    horizontalArrangement = Arrangement.spacedBy(16.dp),
                    modifier = Modifier.fillMaxWidth().height(IntrinsicSize.Max),
                ) {
                    cards(Modifier.weight(1f).fillMaxHeight())
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
    game: Game,
    number: String,
    tag: Int,
    text: String,
    disc: String,
    accent: Color,
    deep: Color,
    @DrawableRes art: Int?,
    artAlignment: Alignment = Alignment.Center,
    last: Boolean,
    wide: Boolean,
    modifier: Modifier,
    onPick: (Game) -> Unit,
) {
    val shape = RoundedCornerShape(24.dp)
    val cardBackground = Brush.linearGradient(
        listOf(Dbz.NightHigh, accent.copy(alpha = 0.22f), deep.copy(alpha = 0.72f), Dbz.Night),
    )
    Box(
        modifier
            .clip(shape)
            .background(cardBackground)
            .border(if (last) 3.dp else 2.dp, accent, shape)
            .clickable { onPick(game) },
    ) {
        if (wide) {
            Column(
                Modifier.fillMaxSize().padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(12.dp),
            ) {
                CardArt(
                    art = art,
                    accent = accent,
                    deep = deep,
                    artAlignment = artAlignment,
                    modifier = Modifier.fillMaxWidth().weight(0.55f),
                )
                Column(
                    Modifier.fillMaxWidth().weight(0.45f),
                    verticalArrangement = Arrangement.SpaceBetween,
                ) {
                    CardText(tag = tag, disc = disc, number = number, text = text, accent = accent)
                    SelectButton(accent = accent)
                }
            }
        } else {
            Row(
                Modifier.fillMaxWidth().padding(16.dp),
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(14.dp),
            ) {
                CardArt(
                    art = art,
                    accent = accent,
                    deep = deep,
                    artAlignment = artAlignment,
                    modifier = Modifier.weight(0.4f).aspectRatio(1f),
                )
                Column(
                    Modifier.weight(0.6f),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    CardText(tag = tag, disc = disc, number = number, text = text, accent = accent)
                    SelectButton(accent = accent)
                }
            }
        }
        if (last) {
            Box(
                Modifier.align(Alignment.TopEnd)
                    .padding(10.dp)
                    .clip(RoundedCornerShape(10.dp))
                    .background(accent.copy(alpha = 0.92f))
                    .padding(horizontal = 10.dp, vertical = 4.dp),
            ) {
                Text(
                    stringResource(R.string.welcome_last),
                    style = MaterialTheme.typography.labelSmall,
                    color = Dbz.Night,
                    fontWeight = FontWeight.Bold,
                )
            }
        }
    }
}

@Composable
private fun CardText(tag: Int, disc: String, number: String, text: String, accent: Color) {
    Column(verticalArrangement = Arrangement.spacedBy(8.dp)) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(8.dp),
        ) {
            Box(
                Modifier.clip(RoundedCornerShape(8.dp))
                    .background(accent.copy(alpha = 0.9f))
                    .padding(horizontal = 8.dp, vertical = 3.dp),
            ) {
                Text(
                    stringResource(tag),
                    style = MaterialTheme.typography.labelSmall,
                    color = Dbz.Night,
                    fontWeight = FontWeight.Bold,
                )
            }
            Text(
                disc,
                style = MaterialTheme.typography.labelMedium,
                color = Color.White.copy(alpha = 0.82f),
            )
        }
        Row(
            verticalAlignment = Alignment.Bottom,
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Text(
                "BUDOKAI TENKAICHI",
                style = MaterialTheme.typography.titleLarge.copy(
                    fontWeight = FontWeight.Black,
                    fontStyle = FontStyle.Italic,
                ),
                color = Color.White,
            )
            Text(
                number,
                style = MaterialTheme.typography.displaySmall.copy(
                    fontWeight = FontWeight.Black,
                    fontStyle = FontStyle.Italic,
                ),
                color = accent,
            )
        }
        Text(
            text,
            style = MaterialTheme.typography.bodyMedium,
            color = Color.White,
        )
    }
}

@Composable
private fun SelectButton(accent: Color) {
    Box(
        Modifier.clip(RoundedCornerShape(14.dp))
            .background(accent.copy(alpha = 0.92f))
            .padding(horizontal = 14.dp, vertical = 8.dp),
    ) {
        Text(
            stringResource(R.string.welcome_select) + " >",
            style = MaterialTheme.typography.labelLarge,
            color = Dbz.Night,
            fontWeight = FontWeight.Bold,
        )
    }
}

@Composable
private fun CardArt(
    @DrawableRes art: Int?,
    accent: Color,
    deep: Color,
    artAlignment: Alignment = Alignment.Center,
    modifier: Modifier = Modifier,
) {
    Box(modifier.clip(RoundedCornerShape(18.dp))) {
        if (art == null) {
            Box(Modifier.fillMaxSize().background(Brush.linearGradient(listOf(accent, deep))))
        } else {
            Image(
                painter = painterResource(art),
                contentDescription = null,
                modifier = Modifier.fillMaxSize(),
                alignment = artAlignment,
                contentScale = ContentScale.Crop,
            )
            Box(
                Modifier.fillMaxSize().background(
                    Brush.verticalGradient(listOf(Color.Transparent, Dbz.Night.copy(alpha = 0.88f))),
                ),
            )
        }
    }
}
