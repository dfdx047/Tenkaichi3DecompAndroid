package com.dfdx047.dragonrage.data

import org.json.JSONArray
import org.json.JSONObject

const val REPO = "dfdx047/Tenkaichi3DecompAndroid"
const val REPO_URL = "https://github.com/$REPO"

data class UpdateInfo(val version: String, val notes: String, val apkUrl: String, val size: Long, val nightly: Boolean)

/** The update dialog's state. */
sealed interface UpdateState {
    object Idle : UpdateState
    object Checking : UpdateState
    data class Available(val info: UpdateInfo) : UpdateState
    data class Downloading(val info: UpdateInfo, val done: Long, val total: Long) : UpdateState
    /** Downloaded, but Android has not yet let this app install packages. */
    data class NeedPermission(val info: UpdateInfo) : UpdateState
}

/**
 * Looks at the repository's GitHub releases. A test build (0.0.0-dev.N, version code N = the CI run) follows the
 * "android-nightly" pre-release, whose text carries the run number; a normal build follows the highest
 * `android-vX.Y.Z` release.
 */
object AppUpdates {
    suspend fun check(currentName: String, currentCode: Int): UpdateInfo? {
        val releases = JSONArray(Net.getText("https://api.github.com/repos/$REPO/releases?per_page=30"))
        val dev = currentName.contains("-dev")
        var best: UpdateInfo? = null
        var bestV: List<Int>? = null
        for (i in 0 until releases.length()) {
            val r = releases.getJSONObject(i)
            if (r.optBoolean("draft")) continue
            val tag = r.optString("tag_name")
            val (apkUrl, size) = apkAsset(r) ?: continue
            if (dev) {
                if (tag != "android-nightly") continue
                val run = Regex("""dev\.(\d+)""").find(r.optString("body"))?.groupValues?.get(1)?.toIntOrNull() ?: return null
                return if (run > currentCode) UpdateInfo("0.0.0-dev.$run", "", apkUrl, size, true) else null
            }
            if (!tag.startsWith("android-v") || r.optBoolean("prerelease")) continue
            val v = parse(tag.removePrefix("android-v")) ?: continue
            if (bestV == null || compare(v, bestV) > 0) {
                bestV = v
                best = UpdateInfo(tag.removePrefix("android-v"), r.optString("body").trim().take(700), apkUrl, size, false)
            }
        }
        val mine = parse(currentName) ?: listOf(0)
        return best?.takeIf { compare(bestV!!, mine) > 0 }
    }

    private fun apkAsset(r: JSONObject): Pair<String, Long>? {
        val assets = r.optJSONArray("assets") ?: return null
        for (i in 0 until assets.length()) {
            val a = assets.getJSONObject(i)
            if (a.optString("name").endsWith(".apk")) return a.getString("browser_download_url") to a.optLong("size", -1)
        }
        return null
    }

    private fun parse(v: String): List<Int>? =
        v.substringBefore('-').split('.').map { it.toIntOrNull() ?: return null }.takeIf { it.isNotEmpty() }

    private fun compare(a: List<Int>, b: List<Int>): Int {
        for (i in 0 until maxOf(a.size, b.size)) {
            val d = a.getOrElse(i) { 0 } - b.getOrElse(i) { 0 }
            if (d != 0) return d
        }
        return 0
    }
}

data class CatalogPack(val name: String, val description: String, val url: String, val size: Long)

sealed interface CatalogState {
    object Loading : CatalogState
    object Failed : CatalogState
    data class Loaded(val packs: List<CatalogPack>) : CatalogState
}

/**
 * The list of texture packs the app offers to download: android/texture-packs.json in the repository, read from
 * `main`, so packs can be added or fixed without a new app version.
 * Format: {"packs":[{"name":"…","description":"…","url":"https://…/pack.zip","size":123456}]}
 */
object TextureCatalog {
    const val URL = "https://raw.githubusercontent.com/$REPO/main/android/texture-packs.json"

    suspend fun load(): List<CatalogPack> {
        val arr = JSONObject(Net.getText(URL)).optJSONArray("packs") ?: return emptyList()
        return (0 until arr.length()).mapNotNull {
            val o = arr.getJSONObject(it)
            val url = o.optString("url")
            if (!url.startsWith("https://")) null
            else CatalogPack(o.optString("name", "Texture pack"), o.optString("description"), url, o.optLong("size", -1))
        }
    }
}
