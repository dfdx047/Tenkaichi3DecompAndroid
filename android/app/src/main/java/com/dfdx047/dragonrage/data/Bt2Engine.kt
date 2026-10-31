package com.dfdx047.dragonrage.data

import android.content.ContentResolver
import android.content.Context
import android.net.Uri
import android.util.Base64
import com.dfdx047.dragonrage.BuildConfig
import java.io.File
import java.io.InputStream
import java.security.KeyFactory
import java.security.MessageDigest
import java.security.Signature
import java.security.spec.X509EncodedKeySpec
import java.util.zip.ZipInputStream
import org.json.JSONObject

/**
 * Budokai Tenkaichi 2's engine (libbt2.so) for the APKs that leave it out (the public ones, built by GitHub): it is
 * made from the game's program, so it never goes into the repository or its releases. The players get it as a motor
 * package shared outside GitHub (bt2/android/build.py package: libbt2.so, manifest.json, manifest.sig) and import it
 * on the BT2 tab; it is kept in the app's own folder (files/engines/bt2), the only place Android loads code from
 * besides the APK, and Bt2GameActivity loads it from there.
 *
 * A package is taken only when its manifest is signed by the motor key ([PUBLIC_KEY]; its private half stays with
 * whoever makes the packages), its libbt2.so is the one the manifest names (size and SHA-256) and its ABI is this
 * app's (BuildConfig.BT2_ENGINE_ABI, bt2/runtime/android/engine_abi.h): native code from anyone else is never run.
 */
object Bt2Engine {
    /** The motor key's public half (X.509, P-256): `python3 bt2/android/build.py key` prints it. */
    private const val PUBLIC_KEY =
        "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEYaUKdhhhS4usxKJ9VXraJ+OArJUMuFOEeWoM4KNRvi8vW54GnQsyOLP5Lrc2DoPz04Wulxz21SADJ9fQ9u4JQA=="
    private const val LIBRARY = "libbt2.so"
    private const val MAX_LIBRARY = 400L shl 20

    /** An imported engine: its version, and whether this app can start it (an app update can need a newer one). */
    data class Info(val version: String, val abi: Int, val library: File) {
        val compatible get() = abi == BuildConfig.BT2_ENGINE_ABI
    }

    /** Why a package was refused. */
    enum class Refusal { NOT_A_PACKAGE, BAD_SIGNATURE, DAMAGED, OTHER_APP_VERSION }

    class Refused(val why: Refusal, val version: String = "") : Exception(why.name)

    private fun folder(context: Context) = File(context.filesDir, "engines/bt2")

    /** The imported engine, if there is one. */
    fun installed(context: Context): Info? {
        val dir = folder(context)
        val lib = File(dir, LIBRARY).takeIf { it.isFile } ?: return null
        val m = runCatching { JSONObject(File(dir, "manifest.json").readText()) }.getOrNull() ?: return null
        if (lib.length() != m.optLong("size", -1)) return null
        return Info(m.optString("version"), m.optInt("abi", -1), lib)
    }

    /** The engine to start: the imported one when this app can start it. */
    fun usable(context: Context): File? = installed(context)?.takeIf { it.compatible }?.library

    fun import(context: Context, resolver: ContentResolver, uri: Uri): Info =
        importFrom(context) { resolver.openInputStream(uri) }

    /** Reads a motor package, checks it and puts it in place of the engine imported before (kept when it fails). */
    private fun importFrom(context: Context, open: () -> InputStream?): Info {
        val dir = folder(context)
        val tmp = File(dir.parentFile, ".bt2-import").also { it.deleteRecursively(); it.mkdirs() }
        try {
            var manifest: ByteArray? = null
            var signature: ByteArray? = null
            var hash: String? = null
            (open() ?: throw Refused(Refusal.NOT_A_PACKAGE)).use { input ->
                ZipInputStream(input.buffered()).use { zip ->
                    while (true) {
                        val e = zip.nextEntry ?: break
                        when (e.name) {
                            "manifest.json" -> manifest = zip.readBytes()
                            "manifest.sig" -> signature = zip.readBytes()
                            LIBRARY -> hash = copyHashing(zip, File(tmp, LIBRARY))
                        }
                    }
                }
            }
            val text = manifest ?: throw Refused(Refusal.NOT_A_PACKAGE)
            val sig = signature ?: throw Refused(Refusal.BAD_SIGNATURE)
            if (!verify(text, sig)) throw Refused(Refusal.BAD_SIGNATURE)
            val m = runCatching { JSONObject(String(text)) }.getOrNull() ?: throw Refused(Refusal.NOT_A_PACKAGE)
            if (m.optString("game") != Game.BT2.serial || m.optString("library") != LIBRARY) throw Refused(Refusal.NOT_A_PACKAGE)
            val lib = File(tmp, LIBRARY)
            if (hash == null || hash != m.optString("sha256") || lib.length() != m.optLong("size", -1)) throw Refused(Refusal.DAMAGED)
            val info = Info(m.optString("version"), m.optInt("abi", -1), lib)
            if (!info.compatible) throw Refused(Refusal.OTHER_APP_VERSION, info.version)
            File(tmp, "manifest.json").writeBytes(text)
            dir.deleteRecursively()
            if (!tmp.renameTo(dir)) throw Refused(Refusal.DAMAGED)
            return installed(context) ?: throw Refused(Refusal.DAMAGED)
        } finally {
            tmp.deleteRecursively()
        }
    }

    fun remove(context: Context) { folder(context).deleteRecursively() }

    private fun copyHashing(input: InputStream, out: File): String {
        val digest = MessageDigest.getInstance("SHA-256")
        val buf = ByteArray(1 shl 16)
        var total = 0L
        out.outputStream().use { o ->
            while (true) {
                val n = input.read(buf)
                if (n < 0) break
                total += n
                if (total > MAX_LIBRARY) throw Refused(Refusal.DAMAGED)
                digest.update(buf, 0, n)
                o.write(buf, 0, n)
            }
        }
        return digest.digest().joinToString("") { "%02x".format(it) }
    }

    private fun verify(manifest: ByteArray, signature: ByteArray): Boolean = runCatching {
        val key = KeyFactory.getInstance("EC").generatePublic(X509EncodedKeySpec(Base64.decode(PUBLIC_KEY, Base64.DEFAULT)))
        Signature.getInstance("SHA256withECDSA").run {
            initVerify(key)
            update(manifest)
            verify(signature)
        }
    }.getOrDefault(false)
}
