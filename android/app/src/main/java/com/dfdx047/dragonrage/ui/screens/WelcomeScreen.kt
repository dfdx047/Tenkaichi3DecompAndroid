package com.dfdx047.dragonrage.ui.screens

import androidx.annotation.DrawableRes
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.IntrinsicSize
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.matchParentSize
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
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
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.data.Game
import com.dfdx047.dragonrage.ui.components.DragonBall
import com.dfdx047.dragonrage.ui.theme.Dbz

private val DeepNavy = Color(0xFF050914)
private val CardDark = Color(0xFF0A0E1B)
private val KiBlueDeep = Color(0xFF10466B)
private val ShenronGreenDeep = Color(0xFF0B5A33)

/**
 * The first screen: which game to open. Each game keeps its own folder, disc image, saves, settings and engine.
 */
@Composable
fun WelcomeScreen(last: Game, onPick: (Game) -> Unit) {
    val wide = LocalConfiguration.current.screenWidthDp >= 600
    val density = LocalDensity.current
    val insets = WindowInsets.safeDrawing.get(density, LayoutDirection.Ltr)
    val availableHeight = (LocalConfiguration.current.screenHeightDp.dp - insets.top - insets.bottom).coerceAtLeast(320.dp)
    val compact = wide && density.fontScale <= 1.15f && availableHeight >= 400.dp

    val outerPadding = if (compact) 12.dp else 20.dp
    val gap = if (compact) 10.dp else 18.dp
    val headerReserve = if (wide) 56.dp else 92.dp
    val footerReserve = 30.dp
    val cardHeight = (availableHeight - outerPadding * 2 - headerReserve - footerReserve - gap * 2)
        .coerceIn(150.dp, 340.dp)
    val scrollState = rememberScrollState()

    Box(
        Modifier.fillMaxSize().background(DeepNavy).windowInsetsPadding(WindowInsets.safeDrawing),
        contentAlignment = Alignment.Center,
    ) {
        Column(
            Modifier
                .widthIn(max = if (wide) 1100.dp else 900.dp)
                .then(if (compact) Modifier else Modifier.verticalScroll(scrollState))
                .padding(outerPadding),
            horizontalAlignment = Alignment.CenterHorizontally,
            verticalArrangement = Arrangement.spacedBy(gap),
        ) {
            WelcomeHeader(wide = wide)

            val cards: @Composable (Modifier) -> Unit = { m ->
                GameCard(
                    game = Game.BT3,
                    number = "3",
                    tag = R.string.welcome_tag_original,
                    text = stringResource(R.string.welcome_short_bt3),
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
                    text = stringResource(R.string.welcome_short_bt4),
                    disc = "BETA 14 REV2",
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
                    text = stringResource(R.string.welcome_short_bt2),
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
                    horizontalArrangement = Arrangement.spacedBy(if (compact) 10.dp else 16.dp),
                    modifier = Modifier.fillMaxWidth(),
                ) {
                    cards(Modifier.weight(1f).then(if (compact) Modifier.height(cardHeight) else Modifier))
                }
            } else {
                Column(verticalArrangement = Arrangement.spacedBy(gap), modifier = Modifier.fillMaxWidth()) {
                    cards(Modifier.fillMaxWidth())
                }
            }

            WelcomeFooter()
        }
    }
}

@Composable
private fun WelcomeHeader(wide: Boolean) {
    val titleStyle = (if (wide) MaterialTheme.typography.headlineSmall else MaterialTheme.typography.headlineMedium)
        .copy(fontWeight = FontWeight.Black, fontStyle = FontStyle.Italic)
    val subtitleStyle = (if (wide) MaterialTheme.typography.labelSmall else MaterialTheme.typography.labelMedium)
        .copy(letterSpacing = if (wide) 1.5.sp else 2.sp)
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(if (wide) 8.dp else 10.dp),
    ) {
        DragonBall(stars = 4, size = if (wide) 42.dp else 64.dp)
        Column {
            Row(horizontalArrangement = Arrangement.spacedBy(if (wide) 5.dp else 7.dp)) {
                Text("DRAGON", style = titleStyle, color = Color.White)
                Text("RAGE", style = titleStyle, color = Dbz.SaiyanGold)
            }
            Text(stringResource(R.string.welcome_short_subtitle), style = subtitleStyle, color = Color(0xFFC4C3D6))
        }
    }
}

@Composable
private fun WelcomeFooter() {
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        InfoIcon(tint = Color(0xFF9A99B0), modifier = Modifier.size(16.dp))
        Text(
            stringResource(R.string.welcome_short_footer),
            style = MaterialTheme.typography.bodySmall,
            color = Color(0xFF9A99B0),
        )
    }
}

@Composable
private fun InfoIcon(tint: Color, modifier: Modifier = Modifier) {
    Canvas(modifier) {
        val r = size.minDimension / 2f
        drawCircle(color = tint.copy(alpha = 0.18f), radius = r)
        drawCircle(color = tint, radius = r, style = Stroke(width = size.minDimension * 0.08f))
        drawCircle(color = tint, radius = size.minDimension * 0.07f, center = Offset(center.x, center.y - r * 0.5f))
        drawLine(
            color = tint,
            start = Offset(center.x, center.y - r * 0.16f),
            end = Offset(center.x, center.y + r * 0.55f),
            strokeWidth = size.minDimension * 0.12f,
        )
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
    val faceAlignment = when (game) {
        Game.BT3 -> Alignment(0f, -0.36f)
        Game.BT4 -> Alignment(0f, -0.52f)
        Game.BT2 -> Alignment(-1f, -0.40f)
    }
    Box(
        modifier
            .drawBehind {
                val radius = 24.dp.toPx()
                drawRoundRect(
                    color = accent.copy(alpha = 0.10f),
                    cornerRadius = CornerRadius(radius),
                    style = Stroke(width = 12.dp.toPx()),
                )
                drawRoundRect(
                    color = accent.copy(alpha = 0.18f),
                    cornerRadius = CornerRadius(radius),
                    style = Stroke(width = 6.dp.toPx()),
                )
            }
            .clip(shape)
            .background(CardDark)
            .border(if (last) 3.dp else 2.dp, accent, shape)
            .clickable { onPick(game) },
    ) {
        if (wide) {
            CardArt(
                art = art,
                accent = accent,
                deep = deep,
                alignment = faceAlignment,
                portrait = false,
                game = game,
                modifier = Modifier.matchParentSize(),
            )
            Column(
                Modifier.fillMaxWidth().padding(14.dp),
                verticalArrangement = Arrangement.spacedBy(8.dp),
            ) {
                CardText(tag = tag, disc = disc, number = number, text = text, accent = accent, twoLines = false)
                SelectButton(accent = accent)
            }
        } else {
            Row(
                Modifier.fillMaxWidth().height(IntrinsicSize.Min),
                verticalAlignment = Alignment.Top,
            ) {
                CardArt(
                    art = art,
                    accent = accent,
                    deep = deep,
                    alignment = artAlignment,
                    portrait = true,
                    game = game,
                    modifier = Modifier.weight(0.45f).fillMaxHeight(),
                )
                Column(
                    Modifier.weight(0.55f).padding(horizontal = 16.dp, vertical = 14.dp),
                    verticalArrangement = Arrangement.spacedBy(8.dp),
                ) {
                    CardText(tag = tag, disc = disc, number = number, text = text, accent = accent, twoLines = true)
                    SelectButton(accent = accent)
                }
            }
        }
        if (last) {
            Box(
                Modifier.align(Alignment.TopStart)
                    .padding(10.dp)
                    .clip(RoundedCornerShape(10.dp))
                    .background(accent.copy(alpha = 0.88f))
                    .padding(horizontal = 10.dp, vertical = 4.dp),
            ) {
                Text(
                    stringResource(R.string.welcome_last),
                    style = MaterialTheme.typography.labelSmall,
                    color = DeepNavy,
                    fontWeight = FontWeight.Bold,
                )
            }
        }
    }
}

@Composable
private fun CardText(
    tag: Int,
    disc: String,
    number: String,
    text: String,
    accent: Color,
    twoLines: Boolean,
) {
    val density = LocalDensity.current
    Column(verticalArrangement = Arrangement.spacedBy(if (twoLines) 6.dp else 4.dp)) {
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
                    color = DeepNavy,
                    fontWeight = FontWeight.Bold,
                )
            }
            Text(
                disc,
                style = MaterialTheme.typography.labelMedium,
                color = Color.White.copy(alpha = 0.84f),
            )
        }
        BoxWithConstraints {
            val numberBox = if (twoLines) 52.dp else 46.dp
            val titleWidth = (maxWidth - numberBox - 6.dp).coerceAtLeast(60.dp)
            val chars = if (twoLines) 9 else 17
            val titleMax = if (twoLines) 30.sp else 24.sp
            val titleSp = (titleWidth.value / (chars * 0.62f) / density.fontScale).sp.coerceIn(13.sp, titleMax)
            val numberSp = (numberBox.value * 0.82f / density.fontScale).sp
            val titleStyle = (if (twoLines) MaterialTheme.typography.titleLarge else MaterialTheme.typography.titleMedium)
                .copy(fontWeight = FontWeight.Black, fontStyle = FontStyle.Italic)
            Row(
                verticalAlignment = Alignment.Bottom,
                horizontalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                if (twoLines) {
                    Column(Modifier.weight(1f)) {
                        Text("BUDOKAI", style = titleStyle, fontSize = titleSp, color = Color.White, maxLines = 1)
                        Text("TENKAICHI", style = titleStyle, fontSize = titleSp, color = Color.White, maxLines = 1)
                    }
                } else {
                    Text(
                        "BUDOKAI TENKAICHI",
                        modifier = Modifier.weight(1f),
                        style = titleStyle,
                        fontSize = titleSp,
                        color = Color.White,
                        maxLines = 1,
                    )
                }
                Text(
                    number,
                    modifier = Modifier.width(numberBox),
                    style = MaterialTheme.typography.headlineLarge.copy(
                        fontWeight = FontWeight.Black,
                        fontStyle = FontStyle.Italic,
                    ),
                    fontSize = numberSp,
                    textAlign = TextAlign.End,
                    color = accent,
                    maxLines = 1,
                )
            }
        }
        Text(text, style = MaterialTheme.typography.bodyMedium, color = Color.White)
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
            color = DeepNavy,
            fontWeight = FontWeight.Bold,
        )
    }
}

@Composable
private fun CardArt(
    @DrawableRes art: Int?,
    accent: Color,
    deep: Color,
    alignment: Alignment,
    portrait: Boolean,
    game: Game,
    modifier: Modifier = Modifier,
) {
    Box(modifier) {
        if (art == null) {
            Box(Modifier.fillMaxSize().background(Brush.linearGradient(listOf(accent, deep))))
        } else {
            Image(
                painter = painterResource(art),
                contentDescription = null,
                modifier = Modifier.fillMaxSize(),
                alignment = alignment,
                contentScale = ContentScale.Crop,
            )
            if (portrait) {
                Box(
                    Modifier.align(Alignment.CenterEnd)
                        .fillMaxHeight()
                        .width(28.dp)
                        .background(Brush.horizontalGradient(listOf(Color.Transparent, CardDark))),
                )
            } else {
                Box(
                    Modifier.fillMaxSize().background(
                        Brush.verticalGradient(
                            0.34f to Color.Transparent,
                            1f to CardDark.copy(alpha = if (game == Game.BT2) 0.72f else 0.88f),
                        ),
                    ),
                )
            }
        }
    }
}
