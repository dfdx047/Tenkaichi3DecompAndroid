package com.dfdx047.dragonrage.data

import android.content.ContentResolver
import android.net.Uri
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/** A mod that replaces game files: its files mirror gamedata/ (for example pzs3us1/00123.bin). */
data class FileMod(val id: String, val name: String, val enabled: Boolean, val files: Int, val bytes: Long, val dir: File)

/** An extra stage (.unk) or song (.adx), with the name shown in the game's menu. */
data class ExtraItem(val file: File, val displayName: String, val bytes: Long)

enum class ExtraKind(val ext: String, val manifest: String) { STAGE("unk", "maps.txt"), SONG("adx", "songs.txt") }

sealed interface ImportResult {
    data class Mod(val mod: FileMod) : ImportResult
    data class Extras(val kind: ExtraKind, val count: Int) : ImportResult
}

/**
 * Mods in the PC port's three places:
 *  - file overrides: the engine uses gamedata/mods/<path> instead of gamedata/<path> (port/src/plat_file.c).
 *    The app keeps each mod in modlib/<mod>/ and rebuilds gamedata/mods/ from the ones switched on, in their
 *    order (a later mod wins over an earlier one for the same file);
 *  - stages/ (.unk) and songs/ (.adx), named by maps.txt / songs.txt (`file|Name In The Menu`).
 */
class Mods(private val paths: GamePaths, private val resolver: ContentResolver) {

    private val stateFile get() = File(paths.modLibrary, "mods.txt") // order and switches: "1|folder" per line

    // ---- file overrides

    fun listMods(): List<FileMod> {
        val dirs = paths.modLibrary.listFiles { f -> f.isDirectory }.orEmpty().associateBy { it.name }
        val state = readState()
        val ordered = state.map { it.first }.filter { it in dirs } + dirs.keys.filter { k -> state.none { it.first == k } }.sorted()
        return ordered.map { id ->
            val d = dirs.getValue(id)
            FileMod(id, id, state.firstOrNull { it.first == id }?.second ?: true, d.countFiles(), d.sizeDeep(), d)
        }
    }

    private fun readState(): List<Pair<String, Boolean>> =
        if (!stateFile.exists()) emptyList()
        else stateFile.readLines().mapNotNull { l ->
            val bar = l.indexOf('|')
            if (bar < 1) null else l.substring(bar + 1) to (l.substring(0, bar) == "1")
        }

    private fun writeState(mods: List<FileMod>) {
        stateFile.writeText(mods.joinToString("\n", postfix = "\n") { "${if (it.enabled) 1 else 0}|${it.id}" })
    }

    suspend fun setEnabled(mod: FileMod, enabled: Boolean, onProgress: (String) -> Unit) = withContext(Dispatchers.IO) {
        writeState(listMods().map { if (it.id == mod.id) it.copy(enabled = enabled) else it })
        apply(onProgress)
    }

    suspend fun move(mod: FileMod, delta: Int, onProgress: (String) -> Unit) = withContext(Dispatchers.IO) {
        val list = listMods().toMutableList()
        val i = list.indexOfFirst { it.id == mod.id }
        val j = (i + delta).coerceIn(0, list.lastIndex)
        if (i < 0 || i == j) return@withContext
        list.add(j, list.removeAt(i))
        writeState(list)
        apply(onProgress)
    }

    suspend fun delete(mod: FileMod, onProgress: (String) -> Unit) = withContext(Dispatchers.IO) {
        mod.dir.deleteRecursively()
        writeState(listMods())
        apply(onProgress)
    }

    /** Rebuilds gamedata/mods/ from the mods switched on. */
    suspend fun apply(onProgress: (String) -> Unit) = withContext(Dispatchers.IO) {
        paths.mods.deleteRecursively()
        paths.mods.mkdirs()
        val on = listMods().filter { it.enabled }
        on.forEachIndexed { i, m ->
            onProgress("Aplicando mods ${i + 1}/${on.size}: ${m.name}")
            m.dir.copyRecursively(paths.mods, overwrite = true)
        }
    }

    // ---- import

    /**
     * A .unk or .adx goes to stages/ or songs/. A zip holding only .unk or only .adx files is unpacked there;
     * any other zip becomes a file-override mod.
     */
    suspend fun import(uri: Uri, onProgress: (String) -> Unit): ImportResult = withContext(Dispatchers.IO) {
        val fileName = resolver.displayName(uri)
        val ext = fileName.substringAfterLast('.', "").lowercase()
        ExtraKind.entries.firstOrNull { it.ext == ext }?.let { kind ->
            val dest = uniqueFile(dirOf(kind), safeName(fileName))
            resolver.openInputStream(uri)?.use { input -> dest.outputStream().use { input.copyTo(it) } }
                ?: throw IllegalStateException("Não foi possível abrir o arquivo.")
            return@withContext ImportResult.Extras(kind, 1)
        }
        if (ext != "zip") {
            throw IllegalArgumentException("Formato não suportado: escolha um .zip (mod), .unk (estágio) ou .adx (música).")
        }
        val name = safeName(fileName.substringBeforeLast('.'))
        val staging = uniqueChild(paths.temp, name)
        try {
            val files = resolver.openInputStream(uri)?.use { input ->
                unzip(input, staging, onBytes = { onProgress("Extraindo… ${formatBytes(it)}") })
            } ?: throw IllegalStateException("Não foi possível abrir o arquivo.")
            if (files == 0) throw IllegalArgumentException("O zip está vazio.")
            val all = staging.walkTopDown().filter { it.isFile }.toList()
            val kind = ExtraKind.entries.firstOrNull { k -> all.all { it.extension.equals(k.ext, true) || it.name.equals(k.manifest, true) } }
            if (kind != null) {
                var n = 0
                all.filter { it.extension.equals(kind.ext, true) }.forEach { f ->
                    f.copyTo(uniqueFile(dirOf(kind), f.name)); n++
                }
                return@withContext ImportResult.Extras(kind, n)
            }
            // A file-override mod. Zips often wrap everything in one folder: strip it when the inside looks like
            // gamedata (pzs3us0/1/2, disc/).
            var root = staging
            while (true) {
                val kids = root.listFiles().orEmpty()
                if (kids.size == 1 && kids[0].isDirectory && !looksLikeGameData(kids[0].name)) root = kids[0] else break
            }
            if (root.listFiles().orEmpty().none { looksLikeGameData(it.name) }) {
                throw IllegalArgumentException(
                    "Este zip não parece um mod do port: a estrutura deve espelhar gamedata/ (pastas pzs3us0, pzs3us1, pzs3us2 ou disc).",
                )
            }
            val dest = uniqueChild(paths.modLibrary, name)
            if (!root.renameTo(dest)) root.copyRecursively(dest, overwrite = true)
            writeState(listMods())
            apply(onProgress)
            ImportResult.Mod(listMods().first { it.dir == dest })
        } finally {
            staging.deleteRecursively()
        }
    }

    private fun looksLikeGameData(name: String) = name.lowercase().let { it.startsWith("pzs3") || it == "disc" }

    // ---- stages and songs

    private fun dirOf(kind: ExtraKind) = if (kind == ExtraKind.STAGE) paths.stages else paths.songs

    fun listExtras(kind: ExtraKind): List<ExtraItem> {
        val dir = dirOf(kind)
        val names = readManifest(File(dir, kind.manifest))
        val files = dir.listFiles { f -> f.isFile && f.extension.equals(kind.ext, true) }.orEmpty()
        // the engine's order: the manifest's first, then the rest by name without regard to case
        val inManifest = names.keys.mapNotNull { k -> files.firstOrNull { it.name == k } }
        val rest = files.filter { f -> f !in inManifest }.sortedBy { it.name.lowercase() }
        return (inManifest + rest).map { f -> ExtraItem(f, names[f.name] ?: defaultName(f), f.length()) }
    }

    fun rename(kind: ExtraKind, item: ExtraItem, newName: String) {
        val manifest = File(dirOf(kind), kind.manifest)
        val names = LinkedHashMap(readManifest(manifest))
        val clean = newName.replace('|', ' ').replace('\n', ' ').trim()
        if (clean.isEmpty() || clean == defaultName(item.file)) names.remove(item.file.name) else names[item.file.name] = clean
        writeManifest(manifest, names)
    }

    fun deleteExtra(kind: ExtraKind, item: ExtraItem) {
        item.file.delete()
        val manifest = File(dirOf(kind), kind.manifest)
        val names = LinkedHashMap(readManifest(manifest))
        if (names.remove(item.file.name) != null) writeManifest(manifest, names)
    }

    private fun readManifest(f: File): Map<String, String> =
        if (!f.exists()) emptyMap()
        else f.readLines().mapNotNull { l ->
            val bar = l.indexOf('|')
            if (bar < 1) null else l.substring(0, bar).trim() to l.substring(bar + 1).trim()
        }.toMap(LinkedHashMap())

    private fun writeManifest(f: File, names: Map<String, String>) {
        if (names.isEmpty()) f.delete() else f.writeText(names.entries.joinToString("\n", postfix = "\n") { "${it.key}|${it.value}" })
    }

    private fun uniqueFile(dir: File, name: String): File {
        dir.mkdirs()
        var f = File(dir, name)
        var n = 2
        while (f.exists()) f = File(dir, "${name.substringBeforeLast('.')}_${n++}.${name.substringAfterLast('.')}")
        return f
    }

    companion object {
        /** `My_Song.adx` is shown as "My Song" (docs/port/adding_songs.md). */
        fun defaultName(f: File) = f.nameWithoutExtension.replace('_', ' ')
    }
}
