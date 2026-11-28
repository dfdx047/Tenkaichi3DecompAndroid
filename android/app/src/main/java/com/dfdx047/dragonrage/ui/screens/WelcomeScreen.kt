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
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.safeDrawing
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.BiasAlignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shadow
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.painterResource
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.text.style.TextGeometricTransform
import androidx.compose.ui.unit.Dp
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
private val CodeGray = Color(0xFFB8BCC8)
private val DescGray = Color(0xFFC3C7D3)
private val Bt3Red = Color(0xFFF0281E)

/**
 * The first screen: which game to open. Each game keeps its own folder, disc image, saves, settings and engine.
 */
@Composable
fun WelcomeScreen(last: Game, onPick: (Game) -> Unit) {
    val wide = LocalConfiguration.current.screenWidthDp >= 600
    val scrollState = rememberScrollState()
    val outerPad = 8.dp
    val gap = if (wide) 16.dp else 10.dp
    val headerH = if (wide) 44.dp else 54.dp
    val footerH = 20.dp

    Box(
        Modifier
            .fillMaxSize()
            .background(DeepNavy)
            .drawBehind {
                val band = 26.dp.toPx()
                for (i in -2..4) {
                    val x = size.width * i / 4f
                    drawLine(
                        color = Color(0xFF0D1B36).copy(alpha = 0.65f),
                        start = Offset(x, 0f),
                        end = Offset(x + size.height * 0.32f, size.height),
                        strokeWidth = band,
                    )
                }
            }
            .windowInsetsPadding(WindowInsets.safeDrawing),
        contentAlignment = Alignment.TopCenter,
    ) {
        BoxWithConstraints(Modifier.fillMaxSize()) {
            val contentW = (maxWidth - outerPad * 2f).coerceAtLeast(220.dp)
            val gapCount = if (wide) 5 else 4
            val cardSpaceH = (maxHeight - outerPad * 2f - headerH - footerH - gap * gapCount).coerceAtLeast(120.dp)

            val cardW: Dp
            val cardH: Dp
            if (wide) {
                val maxCardW = ((contentW - gap * 2f) / 3f).coerceAtLeast(80.dp)
                val baseH = cardSpaceH.coerceAtMost(maxCardW / 0.85f)
                cardH = baseH.coerceAtLeast(150.dp)
                cardW = (cardH * 0.85f).coerceAtMost(maxCardW)
            } else {
                cardW = contentW
                cardH = cardW / 2f
            }
            val groupW = if (wide) cardW * 3f + gap * 2f else cardW
            val headerOffset = if (wide) ((contentW - groupW) / 2f).coerceAtLeast(0.dp) else 0.dp
            val cardRowsH = if (wide) cardH else cardH * 3f
            val fits = cardRowsH + gap * gapCount + headerH + footerH + outerPad * 2f <= maxHeight
            val footerSpacer = if (wide) {
                (maxHeight - outerPad * 2f - headerH - footerH - gap * gapCount - cardRowsH).coerceAtLeast(0.dp)
            } else {
                0.dp
            }

            Column(
                Modifier
                    .fillMaxSize()
                    .verticalScroll(scrollState)
                    .padding(horizontal = outerPad, vertical = outerPad),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(gap, if (fits) Alignment.CenterVertically else Alignment.Top),
            ) {
                WelcomeHeader(
                    wide = wide,
                    modifier = if (wide) Modifier.align(Alignment.Start).padding(start = headerOffset) else Modifier,
                )

                val cards: @Composable (Modifier) -> Unit = { m ->
                    GameCard(
                        game = Game.BT3,
                        number = "3",
                        tag = R.string.welcome_tag_original,
                        text = stringResource(R.string.welcome_short_bt3),
                        disc = "SLUS-21678",
                        accent = Dbz.SaiyanGold,
                        numberColor = Bt3Red,
                        deep = Dbz.GiOrangeDeep,
                        art = R.drawable.welcome_art_bt3,
                        artAlignment = BiasAlignment(0f, -0.30f),
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
                        numberColor = Dbz.KiBlue,
                        deep = KiBlueDeep,
                        art = R.drawable.welcome_art_bt4,
                        artAlignment = BiasAlignment(-0.3f, -0.30f),
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
                        numberColor = Dbz.ShenronGreen,
                        deep = ShenronGreenDeep,
                        art = R.drawable.welcome_art_bt2,
                        artAlignment = BiasAlignment(-1f, -0.30f),
                        last = last == Game.BT2,
                        wide = wide,
                        modifier = m,
                        onPick = onPick,
                    )
                }

                if (wide) {
                    Row(
                        Modifier.fillMaxWidth(),
                        horizontalArrangement = Arrangement.spacedBy(gap, Alignment.CenterHorizontally),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        cards(Modifier.width(cardW).heightIn(min = cardH))
                    }
                } else {
                    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(gap)) {
                        cards(Modifier.width(cardW).heightIn(min = cardH))
                    }
                }

                if (wide) Spacer(Modifier.height(footerSpacer))
                WelcomeFooter(modifier = Modifier.width(groupW))
            }
        }
    }
}

@Composable
private fun WelcomeHeader(wide: Boolean, modifier: Modifier = Modifier) {
    val ball = if (wide) 34.dp else 46.dp
    val nameSp = if (wide) 20.sp else 26.sp
    val subSp = if (wide) 8.sp else 10.sp
    val nameStyle = (if (wide) MaterialTheme.typography.headlineSmall else MaterialTheme.typography.headlineMedium).copy(
        fontSize = nameSp,
        lineHeight = nameSp * 1.05f,
        fontWeight = FontWeight.Black,
        fontStyle = FontStyle.Italic,
    )
    Row(
        modifier,
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        DragonBall(stars = 4, size = ball)
        Box(
            Modifier.width(1.dp).height(ball * 0.82f)
                .background(CodeGray.copy(alpha = 0.5f)),
        )
        Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Row(horizontalArrangement = Arrangement.spacedBy(if (wide) 4.dp else 6.dp)) {
                Text("DRAGON", style = nameStyle, color = Color.White, maxLines = 1)
                Text("RAGE", style = nameStyle, color = Dbz.SaiyanGold, maxLines = 1)
            }
            Canvas(Modifier.fillMaxWidth().height(if (wide) 4.dp else 5.dp)) {
                drawLine(
                    color = Dbz.SaiyanGold,
                    start = Offset(0f, size.height * 0.85f),
                    end = Offset(size.width * 0.72f, size.height * 0.2f),
                    strokeWidth = size.height * 0.45f,
                )
            }
            Text(
                stringResource(R.string.welcome_short_subtitle),
                fontSize = subSp,
                fontWeight = FontWeight.Medium,
                letterSpacing = if (wide) 1.sp else 1.6.sp,
                color = Color(0xFFC4C3D6),
                maxLines = 1,
            )
        }
    }
}

@Composable
private fun WelcomeFooter(modifier: Modifier = Modifier) {
    Row(
        modifier,
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Box(Modifier.weight(1f).height(1.dp).background(CodeGray.copy(alpha = 0.5f)))
        InfoIcon(tint = CodeGray, modifier = Modifier.size(14.dp))
        Text(
            stringResource(R.string.welcome_short_footer),
            fontSize = 9.sp,
            fontStyle = FontStyle.Italic,
            color = CodeGray,
            maxLines = 1,
        )
        Box(Modifier.weight(1f).height(1.dp).background(CodeGray.copy(alpha = 0.5f)))
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
    numberColor: Color,
    deep: Color,
    @DrawableRes art: Int?,
    artAlignment: Alignment = Alignment.Center,
    last: Boolean,
    wide: Boolean,
    modifier: Modifier,
    onPick: (Game) -> Unit,
) {
    val shape = RoundedCornerShape(18.dp)
    val border = if (last) 3.dp else 2.dp
    val faceAlignment = when (game) {
        Game.BT3 -> BiasAlignment(0f, -0.34f)
        Game.BT4 -> BiasAlignment(-0.3f, -0.42f)
        Game.BT2 -> BiasAlignment(-1f, -0.34f)
    }
    Box(
        modifier
            .drawBehind {
                val radius = 18.dp.toPx()
                drawRoundRect(
                    color = accent.copy(alpha = 0.16f),
                    cornerRadius = CornerRadius(radius),
                    style = Stroke(width = 14.dp.toPx()),
                )
                drawRoundRect(
                    color = accent.copy(alpha = 0.30f),
                    cornerRadius = CornerRadius(radius),
                    style = Stroke(width = 7.dp.toPx()),
                )
            }
            .clip(shape)
            .background(CardDark)
            .background(accent.copy(alpha = 0.06f))
            .border(border, accent, shape)
            .clickable { onPick(game) },
    ) {
        if (wide) {
            CardArt(
                art = art,
                accent = accent,
                deep = deep,
                alignment = faceAlignment,
                portrait = false,
                modifier = Modifier.matchParentSize(),
            )
            Box(
                Modifier.fillMaxSize().background(
                    Brush.verticalGradient(
                        0.34f to Color.Transparent,
                        0.58f to CardDark.copy(alpha = 0.94f),
                        1f to CardDark.copy(alpha = 0.98f),
                    ),
                ),
            )
            Column(
                Modifier.fillMaxSize().padding(10.dp),
                verticalArrangement = Arrangement.spacedBy(4.dp, Alignment.Bottom),
            ) {
                CardText(tag, disc, number, text, accent, numberColor, twoLines = false)
                SelectButton(accent)
            }
        } else {
            Box(Modifier.fillMaxWidth()) {
                CardArt(
                    art = art,
                    accent = accent,
                    deep = deep,
                    alignment = artAlignment,
                    portrait = true,
                    modifier = Modifier.matchParentSize(),
                )
                Row(Modifier.fillMaxWidth()) {
                    Box(Modifier.weight(0.46f))
                    Column(
                        Modifier.weight(0.54f).padding(horizontal = 9.dp, vertical = 8.dp),
                        verticalArrangement = Arrangement.spacedBy(4.dp),
                    ) {
                        CardText(tag, disc, number, text, accent, numberColor, twoLines = true)
                        SelectButton(accent)
                    }
                }
            }
        }
        if (last) {
            Box(
                Modifier.align(Alignment.TopStart)
                    .padding(6.dp)
                    .clip(RoundedCornerShape(6.dp))
                    .background(accent.copy(alpha = 0.92f))
                    .padding(horizontal = 8.dp, vertical = 2.dp),
            ) {
                Text(
                    stringResource(R.string.welcome_last),
                    fontSize = 9.sp,
                    fontWeight = FontWeight.Black,
                    fontStyle = FontStyle.Italic,
                    color = DeepNavy,
                    maxLines = 1,
                )
            }
        }
    }
}

@Composable
private fun BadgeAndCode(tag: Int, disc: String, accent: Color) {
    val density = LocalDensity.current
    BoxWithConstraints {
        val badgeText = stringResource(tag)
        val badgeW = (badgeText.length * 9f * 0.62f + 18f) * density.fontScale
        val codeW = (disc.length * 10f * 0.60f + 4f) * density.fontScale
        val badge: @Composable () -> Unit = {
            Box(
                Modifier.clip(RoundedCornerShape(5.dp))
                    .background(accent)
                    .padding(horizontal = 8.dp, vertical = 2.dp),
            ) {
                Text(
                    badgeText,
                    fontSize = 9.sp,
                    fontWeight = FontWeight.Black,
                    fontStyle = FontStyle.Italic,
                    color = DeepNavy,
                    maxLines = 1,
                )
            }
        }
        val code: @Composable () -> Unit = {
            Text(
                disc,
                fontSize = 10.sp,
                fontWeight = FontWeight.Medium,
                fontStyle = FontStyle.Italic,
                color = CodeGray,
                maxLines = 1,
            )
        }
        if (badgeW + codeW + 12f <= maxWidth.value) {
            Row(
                verticalAlignment = Alignment.CenterVertically,
                horizontalArrangement = Arrangement.spacedBy(6.dp),
            ) {
                badge()
                Box(Modifier.width(1.dp).height(10.dp).background(CodeGray.copy(alpha = 0.55f)))
                code()
            }
        } else {
            Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
                badge()
                code()
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
    numberColor: Color,
    twoLines: Boolean,
) {
    val density = LocalDensity.current
    Column(verticalArrangement = Arrangement.spacedBy(if (twoLines) 4.dp else 3.dp)) {
        BadgeAndCode(tag, disc, accent)
        BoxWithConstraints {
            val baseSp = if (twoLines) 19f else 17f
            val chars = if (twoLines) 9 else 17
            val numberSp = if (twoLines) 46f else 38f
            val numberBox = (numberSp * 0.68f * density.fontScale).dp
            val titleAvail = (maxWidth - numberBox - 4.dp).coerceAtLeast(48.dp)
            val titleSp = ((titleAvail.value / (chars * 0.58f * density.fontScale)).coerceIn(10f, baseSp)).sp
            val titleStyle = (if (twoLines) MaterialTheme.typography.titleLarge else MaterialTheme.typography.titleMedium).copy(
                fontSize = titleSp,
                lineHeight = titleSp * 1.04f,
                fontWeight = FontWeight.Black,
                fontStyle = FontStyle.Italic,
                textGeometricTransform = TextGeometricTransform(scaleX = 0.9f),
                brush = Brush.verticalGradient(listOf(Color.White, Color(0xFFC9D2E0))),
                shadow = Shadow(color = Color.Black.copy(alpha = 0.55f), offset = Offset(0f, 1f), blurRadius = 3f),
            )
            Row(verticalAlignment = Alignment.Bottom) {
                if (twoLines) {
                    Column(Modifier.weight(1f)) {
                        Text("BUDOKAI", style = titleStyle, color = Color.Unspecified, maxLines = 1)
                        Text("TENKAICHI", style = titleStyle, color = Color.Unspecified, maxLines = 1)
                    }
                } else {
                    Text(
                        "BUDOKAI TENKAICHI",
                        modifier = Modifier.weight(1f),
                        style = titleStyle,
                        color = Color.Unspecified,
                        maxLines = 1,
                    )
                }
                Text(
                    number,
                    modifier = Modifier.width(numberBox),
                    style = MaterialTheme.typography.headlineLarge.copy(
                        fontSize = numberSp.sp,
                        fontWeight = FontWeight.Black,
                        fontStyle = FontStyle.Italic,
                        textGeometricTransform = TextGeometricTransform(scaleX = 0.9f),
                        shadow = Shadow(color = Color.Black.copy(alpha = 0.6f), offset = Offset(0f, 1f), blurRadius = 3f),
                    ),
                    color = numberColor,
                    textAlign = TextAlign.End,
                    maxLines = 1,
                )
            }
        }
        Text(
            text,
            fontSize = 10.5.sp,
            lineHeight = 13.5.sp,
            fontWeight = FontWeight.Normal,
            color = DescGray,
        )
    }
}

@Composable
private fun SelectButton(accent: Color) {
    Box(
        Modifier
            .fillMaxWidth()
            .heightIn(min = 32.dp)
            .clip(RoundedCornerShape(16.dp))
            .background(Brush.verticalGradient(listOf(accent.copy(alpha = 0.95f), accent.copy(alpha = 0.72f)))),
        contentAlignment = Alignment.Center,
    ) {
        Row(
            verticalAlignment = Alignment.CenterVertically,
            horizontalArrangement = Arrangement.spacedBy(6.dp),
        ) {
            Text(
                stringResource(R.string.welcome_select),
                fontSize = 12.sp,
                fontWeight = FontWeight.Black,
                fontStyle = FontStyle.Italic,
                color = DeepNavy,
                maxLines = 1,
            )
            Canvas(Modifier.size(10.dp)) {
                val chevron = 1.6.dp.toPx()
                drawLine(DeepNavy, Offset(0f, 0f), Offset(size.width * 0.6f, size.height / 2f), strokeWidth = chevron)
                drawLine(DeepNavy, Offset(size.width * 0.6f, size.height / 2f), Offset(0f, size.height), strokeWidth = chevron)
            }
        }
    }
}

@Composable
private fun CardArt(
    @DrawableRes art: Int?,
    accent: Color,
    deep: Color,
    alignment: Alignment,
    portrait: Boolean,
    modifier: Modifier = Modifier,
) {
    Box(modifier, contentAlignment = Alignment.TopStart) {
        if (art == null) {
            Box(Modifier.fillMaxSize().background(Brush.linearGradient(listOf(accent, deep))))
        } else {
            Box(if (portrait) Modifier.fillMaxHeight().fillMaxWidth(0.46f) else Modifier.fillMaxSize()) {
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
                            .width(56.dp)
                            .fillMaxHeight()
                            .background(
                                Brush.horizontalGradient(0.35f to Color.Transparent, 1f to CardDark),
                            ),
                    )
                }
            }
        }
    }
}
