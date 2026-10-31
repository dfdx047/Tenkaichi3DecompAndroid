package com.dfdx047.dragonrage.data

import java.io.File

/**
 * How the game's last run ended, from its log (bt3_log.txt): the engine and its loader write why they stop. Read when
 * the launcher comes back; each log is looked at once (its time is kept in .lastrun_seen), so a problem is told once.
 */
object LastRun {
    enum class Problem { DATA, MEMORY, ENGINE, CRASH, TEXTURES }

    data class Report(val problem: Problem, val detail: String)

    fun check(paths: GamePaths): Report? {
        val log = File(paths.root, "bt3_log.txt")
        val seen = File(paths.root, ".lastrun_seen")
        if (!log.exists()) return null
        val stamp = log.lastModified().toString()
        if (runCatching { seen.readText() }.getOrNull() == stamp) return null
        runCatching { seen.writeText(stamp) }
        val lines = runCatching { log.readLines() }.getOrNull() ?: return null
        return diagnose(lines)
    }

    /** What the lines of a log say of how the run ended (apart from the files it is read from, so it can be tried on its own). */
    internal fun diagnose(lines: List<String>): Report? {
        fun find(vararg keys: String) = lines.lastOrNull { l -> keys.any { l.contains(it) } }
        find("The game's data comes from your own disc image", "not the unmodified USA release", "damaged file", "was not found")?.let {
            val i = lines.indexOf(it)
            val reason = (if (it.contains("comes from your own disc image") && i > 0) lines[i - 1] else it).substringAfter("bt3: ").trim()
            return Report(Problem.DATA, reason)
        }
        find("no room below 4 GB", "no free place below 4 GB", "no memory below 4 GB", "cannot map ")?.let { return Report(Problem.MEMORY, it.substringAfter("bt3: ").trim()) }
        find("the engine did not load", "cannot read the engine", "the engine has no entry point")?.let { return Report(Problem.ENGINE, it.trim()) }
        // (last: a crash that follows one of the causes above, the process ending under Android's own threads, says less)
        find("CRASH: signal")?.let { return Report(Problem.CRASH, it.substringAfter("CRASH: ").trim()) }
        // A texture pack the game did not take: no file with a PCSX2 name in it, or a fight played without one of its
        // textures being used (made for another release or region of the game, or not a PCSX2 pack)
        // That line is also printed on every start by whoever has no pack at all (the switch is on by default and the folder is empty):
        // only a folder with files in it, which the engine did not take, is a pack that does not work.
        find("texture pack: no replacement textures")?.let { line ->
            val folder = line.substringAfter(" found in ", "").trim()
            if (folder.isNotEmpty() && runCatching { File(folder).walkTopDown().any { it.isFile } }.getOrDefault(false)) {
                return Report(Problem.TEXTURES, line.substringAfter("bt3: ").trim())
            }
        }
        val packLine = find("replacement textures in ")
        val replaced = find("textures replaced so far")
        if (packLine != null && replaced != null && lines.any { it.contains(", fight)") } &&
            Regex("""(\d+) textures replaced so far""").find(replaced)?.groupValues?.get(1) == "0") {
            return Report(Problem.TEXTURES, packLine.substringAfter("bt3: ").trim() + "; 0 used")
        }
        return null
    }
}
