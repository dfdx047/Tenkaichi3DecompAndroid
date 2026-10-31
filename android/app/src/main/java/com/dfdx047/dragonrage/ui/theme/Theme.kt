package com.dfdx047.dragonrage.ui.theme

import android.os.Build
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.darkColorScheme
import androidx.compose.material3.dynamicDarkColorScheme
import androidx.compose.material3.dynamicLightColorScheme
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.platform.LocalContext
import com.dfdx047.dragonrage.data.Game

enum class ThemeMode { SYSTEM, DARK, LIGHT }

private val DragonDark = darkColorScheme(
    primary = DarkPrimary,
    onPrimary = DarkOnPrimary,
    primaryContainer = DarkPrimaryContainer,
    onPrimaryContainer = DarkOnPrimaryContainer,
    secondary = DarkSecondary,
    onSecondary = DarkOnSecondary,
    secondaryContainer = DarkSecondaryContainer,
    onSecondaryContainer = DarkOnSecondaryContainer,
    tertiary = DarkTertiary,
    onTertiary = DarkOnTertiary,
    tertiaryContainer = DarkTertiaryContainer,
    onTertiaryContainer = DarkOnTertiaryContainer,
    background = Dbz.Night,
    onBackground = Color(0xFFE6E4F0),
    surface = Dbz.Night,
    onSurface = Color(0xFFE6E4F0),
    surfaceVariant = Color(0xFF2A2D42),
    onSurfaceVariant = Color(0xFFC4C3D6),
    surfaceContainerLowest = Color(0xFF080A14),
    surfaceContainerLow = Color(0xFF12152A),
    surfaceContainer = Dbz.NightHigh,
    surfaceContainerHigh = Color(0xFF1E2238),
    surfaceContainerHighest = Color(0xFF282C44),
    outline = Color(0xFF8E8DA3),
    outlineVariant = Color(0xFF3E4158),
    error = Color(0xFFFF8A80),
)

private val DragonLight = lightColorScheme(
    primary = LightPrimary,
    onPrimary = LightOnPrimary,
    primaryContainer = LightPrimaryContainer,
    onPrimaryContainer = LightOnPrimaryContainer,
    secondary = LightSecondary,
    onSecondary = LightOnSecondary,
    secondaryContainer = LightSecondaryContainer,
    onSecondaryContainer = LightOnSecondaryContainer,
    tertiary = LightTertiary,
    onTertiary = LightOnTertiary,
    tertiaryContainer = LightTertiaryContainer,
    onTertiaryContainer = LightOnTertiaryContainer,
    background = Color(0xFFFFF8F3),
    surface = Color(0xFFFFF8F3),
    surfaceContainerLow = Color(0xFFFFF1E7),
    surfaceContainer = Color(0xFFFCEBDF),
    surfaceContainerHigh = Color(0xFFF6E5D9),
    surfaceContainerHighest = Color(0xFFF0DFD3),
)

// Budokai Tenkaichi 4: the same look with the colours of its new forms, Super Saiyan Blue's cyan for Goku's orange
// and the violet of god ki for the blue, over a deeper night.
private val Bt4Dark = DragonDark.copy(
    primary = Color(0xFF5CD6FF),
    onPrimary = Color(0xFF00344A),
    primaryContainer = Color(0xFF004C6B),
    onPrimaryContainer = Color(0xFFC4ECFF),
    secondary = Color(0xFFD3A6FF),
    onSecondary = Color(0xFF3F0A6B),
    secondaryContainer = Color(0xFF5A2A8C),
    onSecondaryContainer = Color(0xFFF0DBFF),
    background = Color(0xFF090C1E),
    surface = Color(0xFF090C1E),
    surfaceContainer = Color(0xFF131833),
)

private val Bt4Light = DragonLight.copy(
    primary = Color(0xFF00658A),
    primaryContainer = Color(0xFFC4ECFF),
    onPrimaryContainer = Color(0xFF001E2C),
    secondary = Color(0xFF7438A8),
    secondaryContainer = Color(0xFFF0DBFF),
    onSecondaryContainer = Color(0xFF2C0050),
    background = Color(0xFFF6FAFF),
    surface = Color(0xFFF6FAFF),
    surfaceContainerLow = Color(0xFFEDF4FC),
    surfaceContainer = Color(0xFFE6EEF8),
    surfaceContainerHigh = Color(0xFFDEE8F4),
    surfaceContainerHighest = Color(0xFFD6E2F0),
)

// Budokai Tenkaichi 2: the same look in green (the launcher's square for it), over a night with a green cast.
private val Bt2Dark = DragonDark.copy(
    primary = Color(0xFF5FE08A),
    onPrimary = Color(0xFF00391C),
    primaryContainer = Color(0xFF00522B),
    onPrimaryContainer = Color(0xFFB4F5C8),
    secondary = Color(0xFFA6E86B),
    onSecondary = Color(0xFF1F3600),
    secondaryContainer = Color(0xFF2F5A12),
    onSecondaryContainer = Color(0xFFD2F7B0),
    background = Color(0xFF08140E),
    surface = Color(0xFF08140E),
    surfaceContainer = Color(0xFF12241A),
)

private val Bt2Light = DragonLight.copy(
    primary = Color(0xFF006D3A),
    primaryContainer = Color(0xFFB4F5C8),
    onPrimaryContainer = Color(0xFF00210F),
    secondary = Color(0xFF3F7A12),
    secondaryContainer = Color(0xFFD2F7B0),
    onSecondaryContainer = Color(0xFF0F2000),
    background = Color(0xFFF4FBF5),
    surface = Color(0xFFF4FBF5),
    surfaceContainerLow = Color(0xFFEAF5EC),
    surfaceContainer = Color(0xFFE2EFE5),
    surfaceContainerHigh = Color(0xFFDAE9DE),
    surfaceContainerHighest = Color(0xFFD2E3D7),
)

@Composable
fun DragonRageTheme(
    mode: ThemeMode = ThemeMode.SYSTEM,
    dynamicColor: Boolean = false,
    game: Game = Game.BT3,
    content: @Composable () -> Unit,
) {
    val dark = when (mode) {
        ThemeMode.SYSTEM -> isSystemInDarkTheme()
        ThemeMode.DARK -> true
        ThemeMode.LIGHT -> false
    }
    val context = LocalContext.current
    val scheme = when {
        dynamicColor && Build.VERSION.SDK_INT >= Build.VERSION_CODES.S ->
            if (dark) dynamicDarkColorScheme(context) else dynamicLightColorScheme(context)
        dark -> when (game) { Game.BT4 -> Bt4Dark; Game.BT2 -> Bt2Dark; else -> DragonDark }
        else -> when (game) { Game.BT4 -> Bt4Light; Game.BT2 -> Bt2Light; else -> DragonLight }
    }
    MaterialTheme(colorScheme = scheme, typography = DragonTypography, shapes = DragonShapes, content = content)
}
