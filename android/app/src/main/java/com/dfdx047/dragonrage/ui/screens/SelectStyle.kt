package com.dfdx047.dragonrage.ui.screens

import androidx.compose.foundation.BorderStroke
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.selection.selectable
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.CheckCircle
import androidx.compose.material.icons.rounded.People
import androidx.compose.material3.AlertDialog
import androidx.compose.material3.Button
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedCard
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.drawscope.DrawScope
import androidx.compose.ui.platform.LocalInputModeManager
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.semantics.Role
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import com.dfdx047.dragonrage.AppViewModel
import com.dfdx047.dragonrage.R
import com.dfdx047.dragonrage.ui.theme.Dbz
import kotlinx.coroutines.delay

/*
 * The character select's look, chosen with two cards: "New" (port_charsel.c: the roster grid, the 3D fighter and the
 * split screen, in blue and red) and "Classic" (the game's own screen). Each card has a drawing of its screen, made
 * here: never a picture of the game (no game data in the app or the repository). The setting is select_classic (0 the
 * new one, 1 the game's); the engine reads it when the game opens. BT3 and its Tag Team mode; not BT4 nor BT2.
 */

/** The two cards side by side; [classic] is the one marked. */
@Composable
fun SelectStylePicker(classic: Boolean, onPick: (classic: Boolean) -> Unit, modifier: Modifier = Modifier, focus: FocusRequester? = null) {
    Row(modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(10.dp)) {
        SelectStyleCard(
            title = stringResource(R.string.select_new), text = stringResource(R.string.select_new_text), chosen = !classic,
            onClick = { onPick(false) }, modifier = Modifier.weight(1f).let { if (focus != null) it.focusRequester(focus) else it },
        ) { drawNewSelect() }
        SelectStyleCard(
            title = stringResource(R.string.select_classic), text = stringResource(R.string.select_classic_text), chosen = classic,
            onClick = { onPick(true) }, modifier = Modifier.weight(1f),
        ) { drawClassicSelect() }
    }
}

@Composable
private fun SelectStyleCard(title: String, text: String, chosen: Boolean, onClick: () -> Unit, modifier: Modifier, draw: DrawScope.() -> Unit) {
    val accent = MaterialTheme.colorScheme.primary
    OutlinedCard(
        modifier = modifier.clip(RoundedCornerShape(14.dp)).selectable(selected = chosen, role = Role.RadioButton, onClick = onClick),
        shape = RoundedCornerShape(14.dp),
        border = BorderStroke(if (chosen) 3.dp else 1.dp, if (chosen) accent else MaterialTheme.colorScheme.outlineVariant),
    ) {
        Box {
            Canvas(Modifier.fillMaxWidth().aspectRatio(16f / 9f).clip(RoundedCornerShape(topStart = 12.dp, topEnd = 12.dp)), onDraw = draw)
            if (chosen) {
                Icon(Icons.Rounded.CheckCircle, null, tint = accent, modifier = Modifier.align(Alignment.TopEnd).padding(4.dp).size(22.dp))
            }
        }
        Column(Modifier.padding(horizontal = 10.dp, vertical = 8.dp)) {
            Text(title, style = MaterialTheme.typography.titleSmall, color = if (chosen) accent else MaterialTheme.colorScheme.onSurface)
            Text(text, style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
        }
    }
}

// ---- the drawings (fractions of the card's picture, so they scale with it)

private val NewBlue = Color(0xFF1F5FD6)
private val NewBlueDeep = Color(0xFF0A1E5C)
private val NewRed = Color(0xFFD63A2F)
private val NewRedDeep = Color(0xFF5C0E0A)

/** New: the screen split in two, 1P blue on the left and 2P red on the right; each half with its roster grid, the
 *  fighter on a pedestal and the details panel. */
private fun DrawScope.drawNewSelect() {
    val w = size.width
    val h = size.height
    drawRect(Brush.horizontalGradient(listOf(NewBlueDeep, NewBlue, Color(0xFF3A1640), NewRed, NewRedDeep)))
    for (side in 0..1) {
        val x0 = if (side == 0) 0f else w / 2f
        val light = if (side == 0) Color(0xFF9DC2FF) else Color(0xFFFFB0A8)
        val gridLeft = side == 0
        // the roster: a grid of chips, one marked
        val cols = 3
        val rows = 5
        val cell = h * 0.13f
        val gap = cell * 0.18f
        val gx = if (gridLeft) x0 + w * 0.025f else x0 + w / 2f - w * 0.025f - cols * cell - (cols - 1) * gap
        val gy = h * 0.14f
        for (r in 0 until rows) for (c in 0 until cols) {
            val marked = r == 1 && c == (if (gridLeft) 1 else 0)
            drawRoundRect(
                if (marked) Dbz.SaiyanGold else light.copy(alpha = 0.55f),
                Offset(gx + c * (cell + gap), gy + r * (cell + gap)), Size(cell, cell), CornerRadius(cell * 0.18f),
            )
        }
        // the fighter on its pedestal, between the grid and the middle of the screen
        val fx = if (gridLeft) x0 + w * 0.36f else x0 + w * 0.14f
        drawOval(Color.White.copy(alpha = 0.35f), Offset(fx - w * 0.07f, h * 0.80f), Size(w * 0.14f, h * 0.08f))
        drawFighter(Offset(fx, h * 0.84f), h * 0.58f, Color.White.copy(alpha = 0.92f))
        // the details: a few lines under the fighter's name
        val dx = if (gridLeft) x0 + w * 0.27f else x0 + w * 0.06f
        drawRoundRect(light, Offset(dx, h * 0.06f), Size(w * 0.17f, h * 0.06f), CornerRadius(h * 0.02f))
    }
    // the split
    drawRect(Color.White.copy(alpha = 0.85f), Offset(w / 2f - w * 0.004f, 0f), Size(w * 0.008f, h))
}

/** Classic: the game's own screen, in its warm colours: the two players' pictures on the sides and the list of
 *  fighters in the middle, one marked. */
private fun DrawScope.drawClassicSelect() {
    val w = size.width
    val h = size.height
    drawRect(Brush.verticalGradient(listOf(Color(0xFF3B1A05), Dbz.GiOrangeDeep, Dbz.BallAmber)))
    // 1P and 2P: a framed picture each, the fighter in it
    for (side in 0..1) {
        val px = if (side == 0) w * 0.04f else w * 0.72f
        drawRoundRect(Color.Black.copy(alpha = 0.35f), Offset(px, h * 0.12f), Size(w * 0.24f, h * 0.72f), CornerRadius(h * 0.03f))
        drawRoundRect(Dbz.BallLight, Offset(px, h * 0.12f), Size(w * 0.24f, h * 0.72f), CornerRadius(h * 0.03f), style = androidx.compose.ui.graphics.drawscope.Stroke(h * 0.012f))
        drawFighter(Offset(px + w * 0.12f, h * 0.80f), h * 0.56f, Dbz.BallLight.copy(alpha = 0.9f))
        drawRoundRect(if (side == 0) Dbz.GiBlue else Dbz.StarRed, Offset(px, h * 0.86f), Size(w * 0.24f, h * 0.07f), CornerRadius(h * 0.02f))
    }
    // the list in the middle: name bars, one marked
    val lx = w * 0.32f
    val lw = w * 0.36f
    val bar = h * 0.085f
    for (i in 0 until 8) {
        val y = h * 0.10f + i * bar * 1.25f
        val marked = i == 2
        drawRoundRect(
            if (marked) Dbz.SaiyanGold else Color.Black.copy(alpha = 0.30f),
            Offset(lx, y), Size(lw, bar), CornerRadius(bar * 0.3f),
        )
        drawRoundRect(
            if (marked) Dbz.GiOrangeDeep else Dbz.BallLight.copy(alpha = 0.7f),
            Offset(lx + bar * 0.25f, y + bar * 0.2f), Size(bar * 0.6f, bar * 0.6f), CornerRadius(bar * 0.15f),
        )
        drawRoundRect(
            (if (marked) Color(0xFF3B1A05) else Dbz.BallLight).copy(alpha = 0.75f),
            Offset(lx + bar * 1.1f, y + bar * 0.38f), Size(lw * (0.45f + (i * 37 % 30) / 100f), bar * 0.24f), CornerRadius(bar * 0.12f),
        )
    }
}

/** A fighter's outline standing on [feet], [tall] high: head, body and legs. */
private fun DrawScope.drawFighter(feet: Offset, tall: Float, color: Color) {
    val head = tall * 0.10f
    drawCircle(color, head, Offset(feet.x, feet.y - tall + head))
    val body = Path().apply {
        moveTo(feet.x - tall * 0.17f, feet.y - tall * 0.76f) // shoulders
        lineTo(feet.x + tall * 0.17f, feet.y - tall * 0.76f)
        lineTo(feet.x + tall * 0.10f, feet.y - tall * 0.40f) // waist
        lineTo(feet.x + tall * 0.14f, feet.y) // legs
        lineTo(feet.x + tall * 0.03f, feet.y)
        lineTo(feet.x, feet.y - tall * 0.28f)
        lineTo(feet.x - tall * 0.03f, feet.y)
        lineTo(feet.x - tall * 0.14f, feet.y)
        lineTo(feet.x - tall * 0.10f, feet.y - tall * 0.40f)
        close()
    }
    drawPath(body, color)
}

/**
 * Once, at the first opening of a version that has the new select (BT3's launcher, after the GPU and light graphics
 * offers, never with them): the two cards, the new one marked. "OK" keeps the one marked; going back keeps it too.
 */
@Composable
fun SelectStyleDialog(vm: AppViewModel) {
    val ask by vm.selectStyleOffer.collectAsStateWithLifecycle()
    val gpuOffer by vm.gpuOffer.collectAsStateWithLifecycle()
    val tagLite by vm.tagLiteOffer.collectAsStateWithLifecycle()
    if (!ask || gpuOffer != null || tagLite) return
    val now by vm.engineSettings.collectAsStateWithLifecycle()
    var classic by remember { mutableStateOf((now[com.dfdx047.dragonrage.data.EngineSettings.SELECT_CLASSIC] ?: 0) != 0) }
    val first = remember { FocusRequester() }
    AlertDialog(
        onDismissRequest = { vm.answerSelectStyle(classic) },
        icon = { Icon(Icons.Rounded.People, null, tint = Dbz.SaiyanGold) },
        title = { Text(stringResource(R.string.select_style_ask), textAlign = TextAlign.Center) },
        text = {
            Column(verticalArrangement = Arrangement.spacedBy(10.dp)) {
                // (focus on the first card, again when a gamepad key leaves touch mode, as TagLiteDialog does)
                val mode = LocalInputModeManager.current.inputMode
                LaunchedEffect(mode) { delay(80); runCatching { first.requestFocus() } }
                SelectStylePicker(classic, { classic = it }, focus = first)
                Text(stringResource(R.string.select_style_text), style = MaterialTheme.typography.bodySmall, color = MaterialTheme.colorScheme.onSurfaceVariant)
            }
        },
        confirmButton = { Button(onClick = { vm.answerSelectStyle(classic) }) { Text(stringResource(R.string.select_style_ok)) } },
    )
}
