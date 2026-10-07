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

@Composable
fun DragonRageTheme(
    mode: ThemeMode = ThemeMode.SYSTEM,
    dynamicColor: Boolean = false,
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
        dark -> DragonDark
        else -> DragonLight
    }
    MaterialTheme(colorScheme = scheme, typography = DragonTypography, shapes = DragonShapes, content = content)
}
