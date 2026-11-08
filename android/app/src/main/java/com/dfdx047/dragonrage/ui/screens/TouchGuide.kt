package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.widthIn
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Close
import androidx.compose.material3.Card
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.Icon
import androidx.compose.material3.IconButton
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.PathEffect
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.dp
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogProperties
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.ui.theme.Dbz

/*
 * The guide of "Toque Pro": six cards, each a drawing made here (thumbs, rings, arrows: nothing of the game) with a
 * title and a few lines. port/tools/touch_guide_images.py draws the same six for the changelog (docs/images/touch-pro).
 */

private val Bg = Color(0xFF0D0F1C)
private val Gold = Dbz.SaiyanGold
private val Ki = Dbz.KiBlue
private val Dim = Color(0xFF3A4060)
private val Soft = Color(0xFFB8C0E0)

@Composable
fun TouchGuide(onDismiss: () -> Unit) {
    Dialog(onDismissRequest = onDismiss, properties = DialogProperties(usePlatformDefaultWidth = false)) {
        Box(Modifier.fillMaxSize().background(MaterialTheme.colorScheme.background)) {
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(16.dp),
                verticalArrangement = Arrangement.spacedBy(14.dp),
                horizontalAlignment = Alignment.CenterHorizontally,
            ) {
                Row(Modifier.widthIn(max = 720.dp).fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
                    Column(Modifier.weight(1f)) {
                        Text(stringResource(R.string.tpg_title), style = MaterialTheme.typography.headlineSmall, color = MaterialTheme.colorScheme.onBackground)
                        Text(stringResource(R.string.tpg_intro), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
                    }
                    IconButton(onClick = onDismiss) { Icon(Icons.Rounded.Close, stringResource(R.string.close)) }
                }
                GuideCard(R.string.tpg_dynamic, R.string.tpg_dynamic_text) { drawDynamic() }
                GuideCard(R.string.tpg_dead, R.string.tpg_dead_text) { drawDeadZone() }
                GuideCard(R.string.tpg_hit, R.string.tpg_hit_text) { drawHitArea() }
                GuideCard(R.string.tpg_slide, R.string.tpg_slide_text) { drawSlide() }
                GuideCard(R.string.tpg_toggle, R.string.tpg_toggle_text) { drawToggle() }
                GuideCard(R.string.tpg_profiles, R.string.tpg_profiles_text) { drawProfiles() }
                Text(
                    stringResource(R.string.tpg_online), style = MaterialTheme.typography.bodyMedium,
                    color = MaterialTheme.colorScheme.primary, modifier = Modifier.widthIn(max = 720.dp).fillMaxWidth(),
                )
            }
        }
    }
}

@Composable
private fun GuideCard(title: Int, text: Int, draw: DrawScope.() -> Unit) {
    Card(
        Modifier.widthIn(max = 720.dp).fillMaxWidth(), shape = RoundedCornerShape(16.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceContainer),
    ) {
        Canvas(Modifier.fillMaxWidth().aspectRatio(16f / 9f).clip(RoundedCornerShape(topStart = 16.dp, topEnd = 16.dp)).background(Bg), onDraw = draw)
        Column(Modifier.padding(14.dp), verticalArrangement = Arrangement.spacedBy(4.dp)) {
            Text(stringResource(title), style = MaterialTheme.typography.titleMedium)
            Text(stringResource(text), style = MaterialTheme.typography.bodyMedium, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

// ---- drawing helpers (fractions of the card, so the drawings scale)

private fun DrawScope.u(f: Float) = size.height * f

/** A thumb seen from above: a rounded tip with its nail. */
private fun DrawScope.thumb(at: Offset, r: Float, color: Color = Color(0xFFE8C4A0)) {
    drawOval(color.copy(alpha = 0.92f), Offset(at.x - r * 0.8f, at.y - r), Size(r * 1.6f, r * 2.6f))
    drawOval(Color.White.copy(alpha = 0.35f), Offset(at.x - r * 0.45f, at.y - r * 0.8f), Size(r * 0.9f, r * 0.9f))
}

private fun DrawScope.arrow(from: Offset, to: Offset, color: Color, w: Float) {
    drawLine(color, from, to, w, StrokeCap.Round)
    val dx = to.x - from.x
    val dy = to.y - from.y
    val len = kotlin.math.hypot(dx, dy).coerceAtLeast(1f)
    val ux = dx / len
    val uy = dy / len
    val h = w * 3.2f
    val p = Path().apply {
        moveTo(to.x + ux * h * 0.6f, to.y + uy * h * 0.6f)
        lineTo(to.x - ux * h + -uy * h * 0.7f, to.y - uy * h + ux * h * 0.7f)
        lineTo(to.x - ux * h - -uy * h * 0.7f, to.y - uy * h - ux * h * 0.7f)
        close()
    }
    drawPath(p, color)
}

private fun DrawScope.stickBase(c: Offset, r: Float, color: Color, alpha: Float = 1f) {
    drawCircle(color.copy(alpha = 0.18f * alpha), r, c)
    drawCircle(color.copy(alpha = 0.8f * alpha), r, c, style = Stroke(u(0.012f)))
}

private fun DrawScope.button(c: Offset, r: Float, fill: Color, pressed: Boolean = false) {
    drawCircle(if (pressed) fill else Color(0xFF1A1F35), r, c)
    drawCircle(fill, r, c, style = Stroke(u(0.014f)))
}

/** The base follows the thumb past the edge: the old place dimmed, the new one bright. */
private fun DrawScope.drawDynamic() {
    val r = u(0.20f)
    val a = Offset(size.width * 0.30f, u(0.55f))
    val b = Offset(size.width * 0.58f, u(0.45f))
    stickBase(a, r, Dim, 0.9f)
    arrow(Offset(a.x + r * 0.2f, a.y), Offset(b.x - r * 0.25f, b.y + u(0.02f)), Gold, u(0.014f))
    stickBase(b, r, Ki)
    val thumbAt = Offset(b.x + r * 0.95f, b.y - r * 0.2f)
    drawCircle(Ki, r * 0.42f, Offset(b.x + r * 0.6f, b.y - r * 0.12f))
    thumb(thumbAt, u(0.07f))
    drawCircle(Gold, u(0.02f), Offset(size.width * 0.12f, u(0.85f)))
}

/** Rings: the dead zone (nothing), the ramp (walk to run) and the full tilt. */
private fun DrawScope.drawDeadZone() {
    val c = Offset(size.width * 0.5f, u(0.5f))
    val r = u(0.40f)
    drawCircle(Ki.copy(alpha = 0.10f), r, c)
    drawCircle(Ki.copy(alpha = 0.55f), r, c, style = Stroke(u(0.012f), pathEffect = PathEffect.dashPathEffect(floatArrayOf(u(0.03f), u(0.02f)))))
    drawCircle(Gold.copy(alpha = 0.25f), r * 0.75f, c)
    drawCircle(Gold, r * 0.75f, c, style = Stroke(u(0.012f)))
    drawCircle(Color(0xFF2A2F48), r * 0.14f, c)
    drawCircle(Soft, r * 0.14f, c, style = Stroke(u(0.01f)))
    // the tilt bar beside it: grey, then the ramp, then full
    val x0 = size.width * 0.78f
    val y0 = u(0.15f)
    val h = u(0.70f)
    drawRect(Color(0xFF2A2F48), Offset(x0, y0 + h * 0.86f), Size(u(0.06f), h * 0.14f))
    drawRect(Gold.copy(alpha = 0.7f), Offset(x0, y0 + h * 0.25f), Size(u(0.06f), h * 0.61f))
    drawRect(Ki, Offset(x0, y0), Size(u(0.06f), h * 0.25f))
}

/** A button with the larger ring a touch counts on. */
private fun DrawScope.drawHitArea() {
    val c = Offset(size.width * 0.5f, u(0.5f))
    val r = u(0.20f)
    drawCircle(Gold.copy(alpha = 0.12f), r * 1.35f, c)
    drawCircle(Gold, r * 1.35f, c, style = Stroke(u(0.012f), pathEffect = PathEffect.dashPathEffect(floatArrayOf(u(0.03f), u(0.02f)))))
    button(c, r, Ki)
    thumb(Offset(c.x + r * 1.15f, c.y + r * 0.75f), u(0.07f))
}

/** A finger's path across three buttons, each pressed in turn. */
private fun DrawScope.drawSlide() {
    val ys = u(0.5f)
    val xs = listOf(0.25f, 0.5f, 0.75f).map { size.width * it }
    val cols = listOf(Ki, Gold, Dbz.StarRed)
    xs.forEachIndexed { i, x -> button(Offset(x, ys), u(0.13f), cols[i], pressed = i == 2) }
    val p = Path().apply {
        moveTo(xs[0], ys + u(0.02f))
        cubicTo(xs[0] + u(0.2f), ys - u(0.25f), xs[1] - u(0.2f), ys + u(0.25f), xs[1], ys)
        cubicTo(xs[1] + u(0.2f), ys - u(0.25f), xs[2] - u(0.2f), ys + u(0.25f), xs[2], ys)
    }
    drawPath(p, Color.White.copy(alpha = 0.8f), style = Stroke(u(0.014f), pathEffect = PathEffect.dashPathEffect(floatArrayOf(u(0.025f), u(0.02f)))))
    thumb(Offset(xs[2] + u(0.03f), ys + u(0.05f)), u(0.07f))
}

/** Tap: held (lit); tap again: let go (dim). */
private fun DrawScope.drawToggle() {
    val y = u(0.5f)
    val a = Offset(size.width * 0.28f, y)
    val b = Offset(size.width * 0.72f, y)
    button(a, u(0.17f), Gold, pressed = true)
    drawCircle(Gold.copy(alpha = 0.25f), u(0.26f), a, style = Stroke(u(0.02f)))
    button(b, u(0.17f), Dim)
    arrow(Offset(a.x + u(0.24f), y), Offset(b.x - u(0.26f), y), Soft, u(0.012f))
    thumb(Offset(a.x + u(0.05f), a.y + u(0.12f)), u(0.06f))
    thumb(Offset(b.x + u(0.05f), b.y + u(0.12f)), u(0.06f))
}

/** Two screens: the stick on the left, then mirrored for the left hand. */
private fun DrawScope.drawProfiles() {
    val w = size.width * 0.40f
    val h = u(0.62f)
    for (side in 0..1) {
        val x0 = if (side == 0) size.width * 0.06f else size.width * 0.54f
        val y0 = u(0.19f)
        drawRoundRect(Color(0xFF161A2E), Offset(x0, y0), Size(w, h), androidx.compose.ui.geometry.CornerRadius(u(0.04f)))
        drawRoundRect(Soft.copy(alpha = 0.5f), Offset(x0, y0), Size(w, h), androidx.compose.ui.geometry.CornerRadius(u(0.04f)), style = Stroke(u(0.008f)))
        val stickX = if (side == 0) x0 + w * 0.22f else x0 + w * 0.78f
        val btnX = if (side == 0) x0 + w * 0.76f else x0 + w * 0.24f
        stickBase(Offset(stickX, y0 + h * 0.62f), h * 0.2f, Ki)
        listOf(-1f to 0f, 1f to 0f, 0f to -1f, 0f to 1f).forEach { (dx, dy) ->
            button(Offset(btnX + dx * h * 0.13f, y0 + h * 0.55f + dy * h * 0.13f), h * 0.07f, Gold)
        }
    }
    arrow(Offset(size.width * 0.47f, u(0.5f)), Offset(size.width * 0.53f, u(0.5f)), Gold, u(0.014f))
}
