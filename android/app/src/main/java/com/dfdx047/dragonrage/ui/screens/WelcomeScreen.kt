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
import androidx.compose.ui.graphics.TransformOrigin
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.graphics.graphicsLayer
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
private val ScrimDark = Color(0xFF080C16)
private val TextShadow = Shadow(
    color = Color.Black.copy(alpha = 0.65f),
    offset = Offset(0f, 1f),
    blurRadius = 2f,
)

/**
 * The first screen: which game to open. Each game keeps its own folder, disc image, saves, settings and engine.
 */
@Composable
fun WelcomeScreen(last: Game, onPick: (Game) -> Unit) {
    val wide = LocalConfiguration.current.screenWidthDp >= 600
    val fontScale = LocalDensity.current.fontScale
    val scrollState = rememberScrollState()
    val outerPad = 8.dp
    val gap = if (wide) 8.dp else 10.dp
    val topPad = if (wide) 0.dp else 16.dp
    val bottomPad = if (wide) 8.dp else outerPad
    val headerH = if (wide) 56.dp else 54.dp
    val footerH = if (wide) 24.dp else 20.dp
    val headerGap = if (wide) 8.dp else gap
    val footerGap = if (wide) 8.dp else gap

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

            val cardW: Dp
            val cardH: Dp
            if (wide) {
                val maxCardW = ((contentW - gap * 2f) / 3f).coerceAtLeast(80.dp)
                val availableH = (
                    maxHeight - topPad - headerH - headerGap - footerH - footerGap - bottomPad
                    ).coerceAtLeast(120.dp)
                val idealH = availableH.coerceAtMost(maxCardW / 0.85f)
                val readableH = (200f * fontScale + 24f).dp
                cardH = maxOf(idealH, readableH)
                cardW = minOf(cardH * 0.85f, maxCardW)
            } else {
                cardW = contentW
                cardH = cardW / 2f
            }
            val groupW = if (wide) cardW * 3f + gap * 2f else cardW
            val headerOffset = if (wide) ((contentW - groupW) / 2f).coerceAtLeast(0.dp) else 0.dp
            val footerSpacer = if (wide) {
                (maxHeight - topPad - headerH - headerGap - cardH - footerH - bottomPad).coerceAtLeast(0.dp)
            } else {
                0.dp
            }

            Column(
                Modifier
                    .fillMaxSize()
                    .verticalScroll(scrollState)
                    .padding(start = outerPad, top = topPad, end = outerPad, bottom = bottomPad),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                WelcomeHeader(
                    wide = wide,
                    modifier = if (wide) Modifier.align(Alignment.Start).padding(start = headerOffset) else Modifier,
                )

                Spacer(Modifier.height(headerGap))

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
                        artPortrait = R.drawable.welcome_art_bt3,
                        artLandscape = R.drawable.welcome_art_bt3_full,
                        artAlignment = Alignment.Center,
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
                        artPortrait = R.drawable.welcome_art_bt4,
                        artLandscape = R.drawable.welcome_art_bt4_full,
                        artAlignment = Alignment.Center,
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
                        artPortrait = R.drawable.welcome_art_bt2,
                        artLandscape = R.drawable.welcome_art_bt2,
                        artAlignment = Alignment.CenterStart,
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
                        verticalAlignment = Alignment.Top,
                    ) {
                        cards(Modifier.width(cardW).height(cardH))
                    }
                } else {
                    Column(Modifier.fillMaxWidth(), verticalArrangement = Arrangement.spacedBy(gap)) {
                        cards(Modifier.width(cardW).heightIn(min = cardH))
                    }
                }

                Spacer(Modifier.height(if (wide) footerSpacer else footerGap))
                WelcomeFooter(modifier = Modifier.width(groupW), wide = wide)
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
        modifier.then(if (wide) Modifier.height(56.dp) else Modifier),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.dp),
    ) {
        DragonBall(stars = 4, size = ball)
        Box(
            Modifier.width(1.dp).height(ball * 0.82f)
                .background(CodeGray.copy(alpha = 0.5f)),
        )
        Column(verticalArrangement = Arrangement.spacedBy(2.dp)) {
            Row(
                Modifier.drawBehind {
                    val y = size.height - 1.dp.toPx()
                    drawLine(
                        color = Dbz.SaiyanGold,
                        start = Offset(0f, y),
                        end = Offset(size.width, y),
                        strokeWidth = 1.5.dp.toPx(),
                    )
                },
                horizontalArrangement = Arrangement.spacedBy(if (wide) 4.dp else 6.dp),
            ) {
                Text("DRAGON", style = nameStyle, color = Color.White, maxLines = 1)
                Text("RAGE", style = nameStyle, color = Dbz.SaiyanGold, maxLines = 1)
            }
            Text(
                stringResource(R.string.welcome_short_subtitle),
                fontSize = subSp,
                lineHeight = if (wide) 10.sp else 12.sp,
                fontWeight = FontWeight.Medium,
                letterSpacing = if (wide) 1.sp else 1.6.sp,
                color = Color(0xFFC4C3D6),
                maxLines = 1,
            )
        }
    }
}

@Composable
private fun WelcomeFooter(modifier: Modifier = Modifier, wide: Boolean) {
    Row(
        modifier.then(if (wide) Modifier.height(24.dp) else Modifier),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Box(Modifier.weight(1f).height(1.dp).background(CodeGray.copy(alpha = 0.5f)))
        InfoIcon(tint = CodeGray, modifier = Modifier.size(14.dp))
        Text(
            stringResource(R.string.welcome_short_footer),
            fontSize = 9.sp,
            lineHeight = 11.sp,
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
    @DrawableRes artPortrait: Int?,
    @DrawableRes artLandscape: Int? = null,
    artAlignment: Alignment = Alignment.Center,
    last: Boolean,
    wide: Boolean,
    modifier: Modifier,
    onPick: (Game) -> Unit,
) {
    val shape = RoundedCornerShape(18.dp)
    val border = if (last) 3.dp else 2.dp
    val portraitAlignment = when (game) {
        Game.BT3 -> BiasAlignment(0f, -0.30f)
        Game.BT4 -> BiasAlignment(-0.3f, -0.30f)
        Game.BT2 -> BiasAlignment(-1f, -0.30f)
    }
    val landscapeArt = artLandscape ?: artPortrait
    val shiftRatio = when (game) {
        Game.BT3 -> 0f
        Game.BT4 -> -0.17f
        Game.BT2 -> -0.23f
    }
    val artZoom = if (wide && game == Game.BT3) 1.3f else 1f
    val artOrigin = if (game == Game.BT3) TransformOrigin(0.45f, 0.31f) else TransformOrigin.Center
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
            .then(if (wide) Modifier else Modifier.background(accent.copy(alpha = 0.06f)))
            .border(border, accent, shape)
            .clickable { onPick(game) },
    ) {
        if (wide) {
            CardArt(
                art = landscapeArt,
                accent = accent,
                deep = deep,
                alignment = artAlignment,
                portrait = false,
                shiftRatio = shiftRatio,
                zoom = artZoom,
                origin = artOrigin,
                modifier = Modifier.matchParentSize(),
            )
            Box(
                Modifier.matchParentSize().background(
                    Brush.verticalGradient(
                        0.40f to Color.Transparent,
                        0.60f to ScrimDark.copy(alpha = 0.75f),
                        0.75f to ScrimDark.copy(alpha = 0.90f),
                        1f to ScrimDark.copy(alpha = 0.90f),
                    ),
                ),
            )
            Column(
                Modifier.matchParentSize().padding(horizontal = 8.dp, vertical = 6.dp),
                verticalArrangement = Arrangement.spacedBy(2.dp, Alignment.Bottom),
            ) {
                CardText(tag, disc, number, text, accent, numberColor, twoLines = false)
                SelectButton(accent, compact = true)
            }
        } else {
            CardArt(
                art = artPortrait,
                accent = accent,
                deep = deep,
                alignment = portraitAlignment,
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
                    SelectButton(accent, compact = false)
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
                    fontSize = if (wide) 8.sp else 9.sp,
                    lineHeight = 10.sp,
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
    Row(
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(6.dp),
    ) {
        Box(
            Modifier.clip(RoundedCornerShape(5.dp))
                .background(accent)
                .padding(horizontal = 8.dp, vertical = 2.dp),
        ) {
            Text(
                stringResource(tag),
                style = MaterialTheme.typography.labelSmall.copy(shadow = TextShadow),
                fontSize = 9.sp,
                lineHeight = 11.sp,
                fontWeight = FontWeight.Black,
                fontStyle = FontStyle.Italic,
                color = DeepNavy,
                maxLines = 1,
            )
        }
        Box(Modifier.width(1.dp).height(10.dp).background(CodeGray.copy(alpha = 0.55f)))
        Text(
            disc,
            modifier = Modifier.weight(1f, fill = false),
            style = MaterialTheme.typography.labelSmall.copy(shadow = TextShadow),
            fontSize = 10.sp,
            lineHeight = 12.sp,
            fontWeight = FontWeight.Medium,
            fontStyle = FontStyle.Italic,
            color = CodeGray,
            maxLines = 1,
            softWrap = false,
        )
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
    Column(verticalArrangement = Arrangement.spacedBy(if (twoLines) 4.dp else 2.dp)) {
        BadgeAndCode(tag, disc, accent)
        BoxWithConstraints {
            val numberSp = if (twoLines) 46f else 30f
            val numberBox = (numberSp * 0.68f * density.fontScale).dp
            val titleAvail = (maxWidth - numberBox - 6.dp).coerceAtLeast(40.dp)
            val chars = if (twoLines) 9 else 17
            val baseSp = if (twoLines) 19f else 17f
            val titleSp = ((titleAvail.value / (chars * 0.58f * density.fontScale)).coerceIn(9f, baseSp))
            val titleSpUnit = titleSp.sp
            val lineSp = if (twoLines) {
                titleSp * 1.04f
            } else {
                maxOf(numberSp * 1.05f, titleSp * 1.04f)
            }
            val titleStyle = (if (twoLines) MaterialTheme.typography.titleLarge else MaterialTheme.typography.titleMedium).copy(
                fontSize = titleSpUnit,
                lineHeight = lineSp.sp,
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
                Spacer(Modifier.width(6.dp))
                Text(
                    number,
                    modifier = Modifier.width(numberBox),
                    style = MaterialTheme.typography.headlineLarge.copy(
                        fontSize = numberSp.sp,
                        lineHeight = (if (twoLines) numberSp * 1.05f else lineSp).sp,
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
            style = MaterialTheme.typography.bodySmall.copy(shadow = TextShadow),
            fontSize = if (twoLines) 10.5.sp else 10.sp,
            lineHeight = if (twoLines) 13.5.sp else 12.5.sp,
            fontWeight = FontWeight.Normal,
            color = DescGray,
            minLines = 3,
        )
    }
}

@Composable
private fun SelectButton(accent: Color, compact: Boolean) {
    Box(
        Modifier
            .fillMaxWidth()
            .heightIn(min = if (compact) 30.dp else 32.dp)
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
                fontSize = if (compact) 11.sp else 12.sp,
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
    shiftRatio: Float = 0f,
    zoom: Float = 1f,
    origin: TransformOrigin = TransformOrigin.Center,
    modifier: Modifier = Modifier,
) {
    Box(modifier, contentAlignment = Alignment.TopStart) {
        Box(if (portrait) Modifier.fillMaxHeight().fillMaxWidth(0.46f) else Modifier.fillMaxSize()) {
            if (art != null) {
                if (portrait) {
                    Image(
                        painter = painterResource(art),
                        contentDescription = null,
                        modifier = Modifier.fillMaxSize(),
                        alignment = alignment,
                        contentScale = ContentScale.Crop,
                    )
                } else {
                    BoxWithConstraints(Modifier.fillMaxSize()) {
                        val shift = maxHeight * shiftRatio
                        Image(
                            painter = painterResource(art),
                            contentDescription = null,
                            modifier = Modifier
                                .fillMaxSize()
                                .graphicsLayer {
                                    translationY = shift.toPx()
                                    scaleX = zoom
                                    scaleY = zoom
                                    transformOrigin = origin
                                },
                            alignment = alignment,
                            contentScale = ContentScale.Crop,
                        )
                    }
                }
            }
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
