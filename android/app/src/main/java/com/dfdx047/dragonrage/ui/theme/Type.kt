package com.dfdx047.dragonrage.ui.theme

import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Shapes
import androidx.compose.material3.Typography
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.FontStyle
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp

private val base = Typography()

// Heavy, slanted display type: the energy of the series' logos without shipping a licensed font.
val DragonTypography = Typography(
    displaySmall = base.displaySmall.copy(fontWeight = FontWeight.Black, fontStyle = FontStyle.Italic, letterSpacing = 1.5.sp),
    headlineLarge = base.headlineLarge.copy(fontWeight = FontWeight.Black, fontStyle = FontStyle.Italic, letterSpacing = 1.sp),
    headlineMedium = base.headlineMedium.copy(fontWeight = FontWeight.ExtraBold, fontStyle = FontStyle.Italic),
    headlineSmall = base.headlineSmall.copy(fontWeight = FontWeight.ExtraBold),
    titleLarge = base.titleLarge.copy(fontWeight = FontWeight.Bold),
    titleMedium = base.titleMedium.copy(fontWeight = FontWeight.Bold),
    labelLarge = base.labelLarge.copy(fontWeight = FontWeight.Bold, letterSpacing = 0.6.sp),
)

val TitleStyle = TextStyle(fontSize = 40.sp, fontWeight = FontWeight.Black, fontStyle = FontStyle.Italic, letterSpacing = 2.sp)

val DragonShapes = Shapes(
    extraSmall = RoundedCornerShape(8.dp),
    small = RoundedCornerShape(12.dp),
    medium = RoundedCornerShape(18.dp),
    large = RoundedCornerShape(26.dp),
    extraLarge = RoundedCornerShape(32.dp),
)
