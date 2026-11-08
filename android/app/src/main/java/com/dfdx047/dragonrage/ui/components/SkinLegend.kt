package com.dfdx047.dragonrage.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import com.dfdx047.dragonrage.touch.Ctl
import com.dfdx047.dragonrage.touch.PadSkin
import com.dfdx047.dragonrage.touch.TouchPadView

// Under the controller's preview on the Controls pages (BT3's and BT2's), so the chosen console's look is seen at once.

/** The chosen console's button symbols, large and in their colours: what the preview's small badges show. */
@Composable
fun SkinLegend(skin: PadSkin) {
    val face = listOf(Ctl.CROSS, Ctl.CIRCLE, Ctl.SQUARE, Ctl.TRIANGLE)
    val shoulders = listOf(Ctl.L1, Ctl.L2, Ctl.R1, Ctl.R2)
    Row(
        Modifier.fillMaxWidth().clip(MaterialTheme.shapes.medium).background(Color(0xFF0B1020)).padding(vertical = 10.dp),
        horizontalArrangement = Arrangement.SpaceEvenly, verticalAlignment = Alignment.CenterVertically,
    ) {
        face.forEach { b ->
            val col = if (skin == PadSkin.NINTENDO) Color.White else Color(TouchPadView.faceColor(skin, b))
            Text(TouchPadView.faceLabel(skin, b), color = col, fontSize = 22.sp, fontWeight = FontWeight.Bold)
        }
        shoulders.forEach { b ->
            Text(TouchPadView.faceLabel(skin, b), color = Color.White, fontSize = 15.sp, fontWeight = FontWeight.Bold)
        }
    }
}
