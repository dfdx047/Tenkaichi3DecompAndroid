package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
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
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import com.dfdx047.dragonrage.data.Game

private data class GameSlide(
    val game: Game,
    val title: String,
    val subtitle: String,
    val serial: String,
    val colors: List<Color>,
    val accent: Color,
    val number: String,
)

@Composable
fun WelcomeScreen(last: Game, onPick: (Game) -> Unit) {
    // A very large pager range makes the three entries behave like a circular carousel.
    // The initial page is aligned to BT2, then adjusted to the last selected game.
    val slides = remember {
        listOf(
            GameSlide(
                Game.BT2, "BUDOKAI TENKAICHI 2", "THE NEXT LEVEL OF TENKAICHI",
                "SLUS-21441", listOf(Color(0xFF092F27), Color(0xFF16865A), Color(0xFF071713)),
                Color(0xFF9BFFB8), "02",
            ),
            GameSlide(
                Game.BT3, "BUDOKAI TENKAICHI 3", "THE ULTIMATE BATTLE",
                "SLUS-21678", listOf(Color(0xFF431B0B), Color(0xFFE36A16), Color(0xFF160A08)),
                Color(0xFFFFC56B), "03",
            ),
            GameSlide(
                Game.BT4, "BUDOKAI TENKAICHI 4", "BETA 14 · REV 2",
                "SLUS-219.78", listOf(Color(0xFF111B42), Color(0xFF5141A0), Color(0xFF100D20)),
                Color(0xFFD4C1FF), "04",
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
            // Graphic layers create a dramatic full-screen backdrop without remote assets.
            Box(
                Modifier
                    .align(Alignment.TopEnd)
                    .padding(top = 110.dp, end = 18.dp)
                    .size(250.dp)
                    .clip(CircleShape)
                    .background(
                        Brush.radialGradient(
                            listOf(slide.accent.copy(alpha = 0.38f), slide.accent.copy(alpha = 0.06f), Color.Transparent),
                        ),
                    ),
            )
            Box(
                Modifier
                    .align(Alignment.Center)
                    .padding(start = 36.dp)
                    .size(190.dp)
                    .clip(CircleShape)
                    .background(slide.accent.copy(alpha = 0.07f)),
            )
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
                    .align(Alignment.BottomCenter)
                    .fillMaxWidth()
                    .padding(horizontal = 28.dp)
                    .padding(bottom = 38.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.spacedBy(14.dp),
            ) {
                Text(
                    slide.number,
                    color = slide.accent.copy(alpha = 0.8f),
                    style = MaterialTheme.typography.labelLarge,
                    fontWeight = FontWeight.Black,
                )
                Text(
                    slide.title,
                    color = Color.White,
                    style = MaterialTheme.typography.headlineMedium,
                    fontWeight = FontWeight.Black,
                    textAlign = TextAlign.Center,
                )
                Text(
                    slide.subtitle,
                    color = slide.accent,
                    style = MaterialTheme.typography.titleSmall,
                    fontWeight = FontWeight.Bold,
                    textAlign = TextAlign.Center,
                )
                Text(
                    slide.serial,
                    color = Color.White.copy(alpha = 0.68f),
                    style = MaterialTheme.typography.labelMedium,
                )

                RowDots(
                    active = page % slides.size,
                    accent = slide.accent,
                )

                Spacer(Modifier.height(4.dp))
                Button(
                    onClick = { onPick(slide.game) },
                    modifier = Modifier
                        .size(width = 216.dp, height = 52.dp),
                    shape = RoundedCornerShape(18.dp),
                    colors = ButtonDefaults.buttonColors(
                        containerColor = slide.accent,
                        contentColor = Color(0xFF101018),
                    ),
                    contentPadding = PaddingValues(horizontal = 28.dp, vertical = 12.dp),
                ) {
                    Text(
                        "JUGAR",
                        style = MaterialTheme.typography.titleMedium,
                        fontWeight = FontWeight.Black,
                        letterSpacing = MaterialTheme.typography.titleMedium.letterSpacing,
                    )
                }
                Text(
                    "DESLIZA PARA CAMBIAR DE JUEGO",
                    color = Color.White.copy(alpha = 0.58f),
                    style = MaterialTheme.typography.labelSmall,
                    fontWeight = FontWeight.Medium,
                )
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
