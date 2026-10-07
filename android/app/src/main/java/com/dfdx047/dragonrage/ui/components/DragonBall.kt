package com.dfdx047.dragonrage.ui.components

import androidx.compose.animation.core.LinearEasing
import androidx.compose.animation.core.RepeatMode
import androidx.compose.animation.core.animateFloat
import androidx.compose.animation.core.infiniteRepeatable
import androidx.compose.animation.core.rememberInfiniteTransition
import androidx.compose.animation.core.tween
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.size
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.rotate
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import com.dfdx047.dragonrage.ui.theme.Dbz
import kotlin.math.PI
import kotlin.math.cos
import kotlin.math.min
import kotlin.math.sin

/** Positions of the stars inside a ball of radius 1, for 1 to 7 stars (as the balls are drawn in the series). */
private val starLayouts: List<List<Offset>> = listOf(
    listOf(Offset(0f, 0f)),
    listOf(Offset(-0.28f, -0.1f), Offset(0.28f, 0.1f)),
    listOf(Offset(0f, -0.3f), Offset(-0.3f, 0.2f), Offset(0.3f, 0.2f)),
    listOf(Offset(-0.27f, -0.27f), Offset(0.27f, -0.27f), Offset(-0.27f, 0.27f), Offset(0.27f, 0.27f)),
    listOf(Offset(0f, -0.38f), Offset(-0.36f, -0.1f), Offset(0.36f, -0.1f), Offset(-0.22f, 0.32f), Offset(0.22f, 0.32f)),
    listOf(Offset(0f, -0.4f), Offset(-0.35f, -0.15f), Offset(0.35f, -0.15f), Offset(-0.35f, 0.22f), Offset(0.35f, 0.22f), Offset(0f, 0.42f)),
    listOf(Offset(0f, 0f), Offset(0f, -0.42f), Offset(-0.38f, -0.2f), Offset(0.38f, -0.2f), Offset(-0.38f, 0.22f), Offset(0.38f, 0.22f), Offset(0f, 0.44f)),
)

private fun starPath(center: Offset, outer: Float): Path = Path().apply {
    val inner = outer * 0.42f
    for (i in 0 until 10) {
        val a = -PI / 2 + i * PI / 5
        val r = if (i % 2 == 0) outer else inner
        val x = center.x + (r * cos(a)).toFloat()
        val y = center.y + (r * sin(a)).toFloat()
        if (i == 0) moveTo(x, y) else lineTo(x, y)
    }
    close()
}

fun DrawScope.drawDragonBall(stars: Int, center: Offset, radius: Float, glow: Float = 1f) {
    // ki aura
    drawCircle(
        brush = Brush.radialGradient(
            0.55f to Dbz.SaiyanGold.copy(alpha = 0.45f * glow),
            1f to Color.Transparent,
            center = center,
            radius = radius * 1.7f,
        ),
        radius = radius * 1.7f,
        center = center,
    )
    // the ball
    drawCircle(
        brush = Brush.radialGradient(
            0f to Dbz.BallLight,
            0.45f to Dbz.BallAmber,
            1f to Dbz.GiOrangeDeep,
            center = center + Offset(-radius * 0.3f, -radius * 0.35f),
            radius = radius * 1.35f,
        ),
        radius = radius,
        center = center,
    )
    val layout = starLayouts[(stars - 1).coerceIn(0, 6)]
    val starSize = radius * if (stars == 1) 0.42f else if (stars <= 4) 0.26f else 0.2f
    layout.forEach { o -> drawPath(starPath(center + o * radius, starSize), Dbz.StarRed) }
    // shine
    drawOval(
        color = Color.White.copy(alpha = 0.7f),
        topLeft = center + Offset(-radius * 0.62f, -radius * 0.62f),
        size = Size(radius * 0.42f, radius * 0.26f),
    )
}

/** A dragon ball with a slow turn and a breathing aura. */
@Composable
fun DragonBall(stars: Int = 4, size: Dp = 96.dp, animated: Boolean = true, modifier: Modifier = Modifier) {
    val t = rememberInfiniteTransition(label = "ball")
    val pulse by t.animateFloat(
        initialValue = 0.7f, targetValue = 1f,
        animationSpec = infiniteRepeatable(tween(1400), RepeatMode.Reverse), label = "pulse",
    )
    val tilt by t.animateFloat(
        initialValue = -8f, targetValue = 8f,
        animationSpec = infiniteRepeatable(tween(3200, easing = LinearEasing), RepeatMode.Reverse), label = "tilt",
    )
    Canvas(modifier.size(size)) {
        val r = min(this.size.width, this.size.height) / 2f / 1.7f
        rotate(if (animated) tilt else 0f) {
            drawDragonBall(stars, center, r, if (animated) pulse else 1f)
        }
    }
}
