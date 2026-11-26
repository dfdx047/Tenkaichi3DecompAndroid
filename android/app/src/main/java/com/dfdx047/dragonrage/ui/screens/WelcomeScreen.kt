package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.pager.HorizontalPager
import androidx.compose.foundation.pager.rememberPagerState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Button
import androidx.compose.material3.ButtonDefaults
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.remember
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.layout.ContentScale
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.foundation.border
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.dfdx047.dragonrage.data.Game
import com.dfdx047.dragonrage.R
import android.graphics.BitmapFactory

private val KanitDisplay = FontFamily(Font(R.font.kanit_extrabold_italic))

private data class GameSlide(
    val game: Game,
    val title: String,
    val subtitle: String,
    val description: Int,
    val serial: String,
    val colors: List<Color>,
    val accent: Color,
    val number: String,
    val portraitCover: String,
    val landscapeCover: String?,
    val logo: String,
)

@Composable
fun WelcomeScreen(last: Game, onPick: (Game) -> Unit) {
    // A very large pager range makes the three entries behave like a circular carousel.
    // The initial page is aligned to BT2, then adjusted to the last selected game.
    val slides = remember {
        listOf(
            GameSlide(
                Game.BT2, "BUDOKAI TENKAICHI 2", "THE NEXT LEVEL OF TENKAICHI",
                R.string.welcome_bt2,
                "SLUS-21441", listOf(Color(0xFF092F27), Color(0xFF16865A), Color(0xFF071713)),
                Color(0xFF9BFFB8), "02", "bt2.png", "bt2land.png", "bt2logo.png",
            ),
            GameSlide(
                Game.BT3, "BUDOKAI TENKAICHI 3", "THE ULTIMATE BATTLE",
                R.string.welcome_bt3,
                "SLUS-21678", listOf(Color(0xFF431B0B), Color(0xFFE36A16), Color(0xFF160A08)),
                Color(0xFFFFC56B), "03", "bt3.png", "bt3land.png", "bt3logo.png",
            ),
            GameSlide(
                Game.BT4, "BUDOKAI TENKAICHI 4", "BETA 14 · REV 2",
                R.string.welcome_bt4,
                "SLUS-219.78", listOf(Color(0xFF111B42), Color(0xFF5141A0), Color(0xFF100D20)),
                Color(0xFFD4C1FF), "04", "bt4.jpg", "bt4land.png", "bt4logo.png",
            ),
        )
    }
    val initialIndex = slides.indexOfFirst { it.game == last }.coerceAtLeast(0)
    val pagerState = rememberPagerState(initialPage = 1_000_002 + initialIndex) { Int.MAX_VALUE }

    HorizontalPager(
        state = pagerState,
        modifier = Modifier.fillMaxSize(),
        contentPadding = PaddingValues(0.dp),
        pageSpacing = 0.dp,
        key = { it },
    ) { page ->
        val slide = slides[page % slides.size]
        Box(
            modifier = Modifier
                .fillMaxSize()
                .background(Brush.verticalGradient(slide.colors)),
        ) {
            // Keep the right half free of foreground decorations for character artwork.
            Box(
                Modifier
                    .fillMaxSize()
                    .background(
                        Brush.verticalGradient(
                            0f to Color.Black.copy(alpha = 0.12f),
                            0.42f to Color.Transparent,
                            0.68f to Color.Black.copy(alpha = 0.28f),
                            1f to Color.Black.copy(alpha = 0.88f),
                        ),
                    ),
            )

            BoxWithConstraints(Modifier.fillMaxSize()) {
                val isLandscape = maxWidth > maxHeight
                CoverArtwork(if (isLandscape) slide.landscapeCover ?: slide.portraitCover else slide.portraitCover)
                Box(
                    Modifier
                        .fillMaxSize()
                        .background(
                            Brush.verticalGradient(
                                0f to Color.Black.copy(alpha = 0.12f),
                                0.42f to Color.Transparent,
                                0.68f to Color.Black.copy(alpha = 0.28f),
                                1f to Color.Black.copy(alpha = 0.88f),
                            ),
                        ),
                )
                if (isLandscape) {
                    // Game logo occupies the intentionally open left-center area.
                    LogoArtwork(
                        assetName = slide.logo,
                        modifier = Modifier
                            .align(Alignment.CenterStart)
                            .fillMaxWidth(0.34f)
                            .fillMaxHeight(0.34f)
                            .padding(start = 22.dp),
                    )
                    // Landscape: identity and game index stay at the upper-left.
                    Column(
                        modifier = Modifier
                            .align(Alignment.TopStart)
                            .fillMaxWidth(0.48f)
                            .padding(start = 22.dp, top = 10.dp),
                        verticalArrangement = Arrangement.spacedBy(1.dp),
                    ) {
                        Text(
                            "DRAGON RAGE",
                            color = Color.White.copy(alpha = 0.78f),
                            style = MaterialTheme.typography.labelSmall,
                            fontWeight = FontWeight.Black,
                            letterSpacing = MaterialTheme.typography.labelSmall.letterSpacing,
                        )
                        Text(
                            "SELECT YOUR FIGHT",
                            color = slide.accent,
                            style = MaterialTheme.typography.labelSmall,
                            fontWeight = FontWeight.Bold,
                        )
                        Text(
                            slide.title,
                            color = Color.White,
                            style = MaterialTheme.typography.titleMedium.copy(
                                fontFamily = KanitDisplay,
                                fontWeight = FontWeight.ExtraBold,
                                fontStyle = androidx.compose.ui.text.font.FontStyle.Italic,
                            ),
                            maxLines = 1,
                        )
                    }

                    // Compact controls sit at the lower-left edge; the center remains open.
                    Column(
                        modifier = Modifier
                            .align(Alignment.BottomStart)
                            .fillMaxWidth(0.48f)
                            .navigationBarsPadding()
                            .padding(start = 22.dp, bottom = 3.dp),
                        horizontalAlignment = Alignment.Start,
                        verticalArrangement = Arrangement.spacedBy(5.dp),
                    ) {
                        Box(
                            modifier = Modifier
                                .fillMaxWidth()
                                .clip(RoundedCornerShape(11.dp))
                                .background(Color.White.copy(alpha = 0.10f))
                                .border(1.dp, Color.White.copy(alpha = 0.20f), RoundedCornerShape(11.dp))
                                .padding(horizontal = 10.dp, vertical = 6.dp),
                        ) {
                            Text(
                                stringResource(slide.description),
                                color = Color.White.copy(alpha = 0.94f),
                                style = MaterialTheme.typography.labelSmall.copy(
                                    fontFamily = KanitDisplay,
                                    fontWeight = FontWeight.ExtraBold,
                                    fontStyle = androidx.compose.ui.text.font.FontStyle.Italic,
                                ),
                                maxLines = 3,
                            )
                        }
                        Button(
                            onClick = { onPick(slide.game) },
                            modifier = Modifier.size(width = 164.dp, height = 40.dp),
                            shape = RoundedCornerShape(13.dp),
                            colors = ButtonDefaults.buttonColors(
                                containerColor = slide.accent,
                                contentColor = Color(0xFF101018),
                            ),
                            contentPadding = PaddingValues(horizontal = 18.dp, vertical = 6.dp),
                        ) {
                            Text(
                                stringResource(R.string.play),
                                style = MaterialTheme.typography.labelLarge.copy(
                                    fontFamily = KanitDisplay,
                                    fontWeight = FontWeight.ExtraBold,
                                    fontStyle = androidx.compose.ui.text.font.FontStyle.Italic,
                                ),
                            )
                        }
                        androidx.compose.foundation.layout.Row(
                            verticalAlignment = Alignment.CenterVertically,
                            horizontalArrangement = Arrangement.spacedBy(9.dp),
                        ) {
                            RowDots(active = page % slides.size, accent = slide.accent)
                            Text(
                                stringResource(R.string.welcome_swipe),
                                color = Color.White.copy(alpha = 0.58f),
                                style = MaterialTheme.typography.labelSmall,
                            )
                        }
                    }
                } else {
                    // Portrait layout is intentionally preserved.
                    Column(
                        modifier = Modifier
                            .align(Alignment.TopStart)
                            .fillMaxWidth()
                            .padding(horizontal = 28.dp, vertical = 34.dp),
                        verticalArrangement = Arrangement.spacedBy(8.dp),
                    ) {
                        Text(
                            "DRAGON RAGE",
                            color = Color.White.copy(alpha = 0.82f),
                            style = MaterialTheme.typography.labelLarge,
                            fontWeight = FontWeight.Black,
                            letterSpacing = MaterialTheme.typography.labelLarge.letterSpacing,
                        )
                        Text(
                            "SELECT YOUR FIGHT",
                            color = slide.accent,
                            style = MaterialTheme.typography.labelMedium,
                            fontWeight = FontWeight.Bold,
                        )
                    }
                    Column(
                        modifier = Modifier
                            .fillMaxSize()
                            .padding(horizontal = 28.dp),
                        horizontalAlignment = Alignment.CenterHorizontally,
                        verticalArrangement = Arrangement.SpaceBetween,
                    ) {
                        Spacer(Modifier.weight(1f))
                        Column(
                            modifier = Modifier
                                .fillMaxWidth()
                                .navigationBarsPadding()
                                .padding(bottom = 24.dp),
                            horizontalAlignment = Alignment.CenterHorizontally,
                            verticalArrangement = Arrangement.spacedBy(12.dp),
                        ) {
                            Box(
                                modifier = Modifier
                                    .fillMaxWidth()
                                    .padding(horizontal = 8.dp)
                                    .clip(RoundedCornerShape(16.dp))
                                    .background(Color.White.copy(alpha = 0.10f))
                                    .border(1.dp, Color.White.copy(alpha = 0.22f), RoundedCornerShape(16.dp))
                                    .padding(horizontal = 16.dp, vertical = 13.dp),
                                contentAlignment = Alignment.Center,
                            ) {
                                Text(
                                    stringResource(slide.description),
                                    color = Color.White.copy(alpha = 0.94f),
                                    style = MaterialTheme.typography.bodySmall.copy(fontFamily = KanitDisplay, fontWeight = FontWeight.ExtraBold, fontStyle = androidx.compose.ui.text.font.FontStyle.Italic),
                                    textAlign = TextAlign.Center,
                                )
                            }
                            Button(
                                onClick = { onPick(slide.game) },
                                modifier = Modifier.size(width = 216.dp, height = 52.dp),
                                shape = RoundedCornerShape(18.dp),
                                colors = ButtonDefaults.buttonColors(
                                    containerColor = slide.accent,
                                    contentColor = Color(0xFF101018),
                                ),
                                contentPadding = PaddingValues(horizontal = 28.dp, vertical = 12.dp),
                            ) {
                                Text(
                                    stringResource(R.string.play),
                                    style = MaterialTheme.typography.titleMedium.copy(fontFamily = KanitDisplay, fontWeight = FontWeight.ExtraBold, fontStyle = androidx.compose.ui.text.font.FontStyle.Italic),
                                    fontWeight = FontWeight.ExtraBold,
                                    letterSpacing = MaterialTheme.typography.titleMedium.letterSpacing,
                                )
                            }
                            RowDots(active = page % slides.size, accent = slide.accent)
                            Text(
                                stringResource(R.string.welcome_swipe),
                                color = Color.White.copy(alpha = 0.58f),
                                style = MaterialTheme.typography.labelSmall,
                                fontWeight = FontWeight.Medium,
                            )
                        }
                    }
                }
            }
        }
    }
}

@Composable
private fun RowDots(active: Int, accent: Color) {
    androidx.compose.foundation.layout.Row(
        horizontalArrangement = Arrangement.spacedBy(9.dp),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        repeat(3) { index ->
            Box(
                Modifier
                    .size(if (index == active) 9.dp else 6.dp)
                    .clip(CircleShape)
                    .background(if (index == active) accent else Color.White.copy(alpha = 0.42f)),
            )
        }
    }
}

 
@Composable
private fun CoverArtwork(assetName: String) {
    val context = LocalContext.current
    val bitmap = remember(assetName) {
        runCatching {
            context.assets.open("covers/$assetName").use { BitmapFactory.decodeStream(it)?.asImageBitmap() }
        }.getOrNull()
    }
    if (bitmap != null) {
        Image(
            bitmap = bitmap,
            contentDescription = null,
            modifier = Modifier.fillMaxSize(),
            contentScale = ContentScale.Crop,
        )
    }
}


@Composable
private fun LogoArtwork(assetName: String, modifier: Modifier = Modifier) {
    val context = LocalContext.current
    val bitmap = remember(assetName) {
        runCatching {
            context.assets.open("covers/$assetName").use { BitmapFactory.decodeStream(it)?.asImageBitmap() }
        }.getOrNull()
    }
    if (bitmap != null) {
        Image(
            bitmap = bitmap,
            contentDescription = null,
            modifier = modifier,
            contentScale = ContentScale.Fit,
        )
    }
}
