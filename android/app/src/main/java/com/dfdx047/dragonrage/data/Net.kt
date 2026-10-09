package com.dfdx047.dragonrage.data

import java.io.File
import java.io.IOException
import java.net.HttpURLConnection
import java.net.URL
import kotlin.coroutines.coroutineContext
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.ensureActive
import kotlinx.coroutines.withContext

/** Plain HTTPS downloads (no libraries): GitHub's API, the texture catalog, packs, app updates. */
object Net {
    private fun open(url: String): HttpURLConnection {
        if (!url.startsWith("https://")) throw IOException("Only https links are allowed")
        return (URL(url).openConnection() as HttpURLConnection).apply {
            connectTimeout = 15_000
            readTimeout = 30_000
            instanceFollowRedirects = true
            setRequestProperty("User-Agent", "DragonRage-Android")
            setRequestProperty("Accept", "application/vnd.github+json, application/json, */*")
        }
    }

    suspend fun getText(url: String): String = withContext(Dispatchers.IO) {
        val c = open(url)
        try {
            if (c.responseCode !in 200..299) throw IOException("HTTP ${c.responseCode}")
            c.inputStream.bufferedReader().use { it.readText() }
        } finally {
            c.disconnect()
        }
    }

    /** Downloads [url] to [dest]; [onProgress] gets (bytes so far, total or -1) a few times a second. */
    suspend fun download(url: String, dest: File, onProgress: (Long, Long) -> Unit) = withContext(Dispatchers.IO) {
        val c = open(url)
        try {
            if (c.responseCode !in 200..299) throw IOException("HTTP ${c.responseCode}")
            val total = c.contentLengthLong
            dest.parentFile?.mkdirs()
            var done = 0L
            var last = 0L
            c.inputStream.use { input ->
                dest.outputStream().use { out ->
                    val buf = ByteArray(1 shl 16)
                    while (true) {
                        coroutineContext.ensureActive()
                        val n = input.read(buf)
                        if (n < 0) break
                        out.write(buf, 0, n)
                        done += n
                        val now = System.currentTimeMillis()
                        if (now - last > 200) {
                            last = now
                            onProgress(done, total)
                        }
                    }
                }
            }
            onProgress(done, if (total > 0) total else done)
        } catch (e: Throwable) {
            dest.delete()
            throw e
        } finally {
            c.disconnect()
        }
    }

    /** True if the file starts with the zip signature ("PK"): catches HTML pages served instead of the file. */
    fun looksLikeZip(f: File): Boolean = f.inputStream().use { it.read() == 'P'.code && it.read() == 'K'.code }
}
