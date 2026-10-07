package com.dfdx047.dragonrage.data

import android.content.ContentResolver
import android.net.Uri
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

data class TexturePack(
    val name: String,
    val enabled: Boolean,
    val textures: Int,
    val bytes: Long,
    val dir: File,
)

/**
 * Texture packs made for the game under PCSX2. The engine reads every `.dds` / `.png` named
 * `<texture hash>-[<palette hash>-]<bits>` at any depth below `textures/` (port/src/gs/gs_texpack.c), so a
 * pack is simply a folder there; switched off, it moves to `textures_off/`.
 */
class TexturePacks(private val paths: GamePaths, private val resolver: ContentResolver) {

    fun list(): List<TexturePack> {
        fun scan(dir: File, enabled: Boolean) = dir.listFiles { f -> f.isDirectory }.orEmpty().map { d ->
            var count = 0
            var bytes = 0L
            d.walkTopDown().filter { it.isFile }.forEach {
                bytes += it.length()
                if (isReplacement(it.name)) count++
            }
            TexturePack(d.name, enabled, count, bytes, d)
        }
        return (scan(paths.textures, true) + scan(paths.texturesOff, false)).sortedBy { it.name.lowercase() }
    }

    /** Unpacks a zip as a new pack. Returns the pack, or throws with a message for the user. */
    suspend fun import(uri: Uri, onProgress: (String) -> Unit): TexturePack = withContext(Dispatchers.IO) {
        val fileName = resolver.displayName(uri)
        if (!fileName.endsWith(".zip", ignoreCase = true)) {
            throw IllegalArgumentException("Escolha um pack em .zip (arquivos .7z e .rar ainda não são aceitos).")
        }
        val name = safeName(fileName.substringBeforeLast('.'))
        val staging = uniqueChild(paths.temp, name)
        staging.mkdirs()
        try {
            var found = 0
            val files = resolver.openInputStream(uri)?.use { input ->
                unzip(input, staging, onBytes = { onProgress("Extraindo… ${formatBytes(it)}") }) { rel ->
                    if (isReplacement(rel.substringAfterLast('/'))) found++
                }
            } ?: throw IllegalStateException("Não foi possível abrir o arquivo.")
            if (files == 0) throw IllegalArgumentException("O zip está vazio.")
            if (found == 0) {
                throw IllegalArgumentException(
                    "Nenhuma textura no formato do PCSX2 foi encontrada (nomes como 1a2b3c4d5e6f7a8b-0f1e2d3c-00004a64.dds).",
                )
            }
            val dest = uniqueChild(paths.textures, name)
            if (!staging.renameTo(dest)) {
                staging.copyRecursively(dest, overwrite = true)
            }
            list().first { it.dir == dest }
        } finally {
            staging.deleteRecursively()
        }
    }

    suspend fun setEnabled(pack: TexturePack, enabled: Boolean) = withContext(Dispatchers.IO) {
        if (pack.enabled == enabled) return@withContext
        val dest = uniqueChild(if (enabled) paths.textures else paths.texturesOff, pack.name)
        if (!pack.dir.renameTo(dest)) {
            pack.dir.copyRecursively(dest, overwrite = true)
            pack.dir.deleteRecursively()
        }
    }

    suspend fun delete(pack: TexturePack) = withContext(Dispatchers.IO) { pack.dir.deleteRecursively() }

    companion object {
        private val NAME = Regex("^[0-9a-fA-F]+-([0-9a-fA-F]+-)?[0-9a-fA-F]+\\.(dds|png)$", RegexOption.IGNORE_CASE)
        fun isReplacement(fileName: String) = NAME.matches(fileName)
    }
}
