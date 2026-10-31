plugins {
    alias(libs.plugins.android.application)
    alias(libs.plugins.kotlin.android)
    alias(libs.plugins.kotlin.compose)
}

android {
    namespace = "com.dfdx047.dragonrage"
    compileSdk = 35

    defaultConfig {
        applicationId = "com.dfdx047.dragonrage"
        minSdk = 29
        targetSdk = 35
        // CI passes the tag (android-v0.1.0 -> 0.1.0) and the run number
        // Without CI (a local build) the name is 0.9.0 and the code follows it (0.9.0 -> 900), so a local build of
        // a newer version always installs over the one before it; the same version again installs with `adb install -r`.
        val vName = System.getenv("DR_VERSION_NAME") ?: "0.9.0"
        val vParts = vName.substringBefore('-').split('.').map { it.toIntOrNull() ?: 0 }
        versionCode = System.getenv("DR_VERSION_CODE")?.toInt()
            ?: maxOf(1, vParts.getOrElse(0) { 0 } * 10000 + vParts.getOrElse(1) { 0 } * 100 + vParts.getOrElse(2) { 0 })
        versionName = vName
        // the PC release the engine is (port/src/port_release.h): the launcher's online status compares it with what
        // the matchmaking service asks for
        val portRelease = rootProject.file("../port/src/port_release.h").takeIf { it.exists() }?.readLines()
            ?.firstOrNull { it.startsWith("#define PORT_RELEASE ") }?.substringAfter("#define PORT_RELEASE ")?.trim()?.toIntOrNull() ?: 0
        buildConfigField("int", "PORT_RELEASE", portRelease.toString())
        // the BT2 engine packages this app imports (bt2/runtime/android/engine_abi.h; data/Bt2Engine.kt)
        val bt2EngineAbi = rootProject.file("../bt2/runtime/android/engine_abi.h").takeIf { it.exists() }?.readLines()
            ?.firstOrNull { it.startsWith("#define BT2_ENGINE_ABI ") }?.substringAfter("#define BT2_ENGINE_ABI ")?.trim()?.toIntOrNull() ?: 0
        buildConfigField("int", "BT2_ENGINE_ABI", bt2EngineAbi.toString())

        // The game engine will be arm64 only (see docs: 32-bit pointers lowered for AArch64).
        ndk { abiFilters += listOf("arm64-v8a") }
    }

    // Release signing: a keystore given by the environment (CI secrets), else the debug key.
    val ks = System.getenv("DR_KEYSTORE")
    signingConfigs {
        // Test builds (debug, and the nightly): one fixed key kept in the repository, so a newer build installs over
        // the old one and keeps the game's data. Not for publishing.
        getByName("debug") {
            storeFile = file("dragonrage-test.jks")
            storePassword = "dragonrage"
            keyAlias = "dragonrage-test"
            keyPassword = "dragonrage"
        }
        if (ks != null && file(ks).exists()) {
            create("release") {
                storeFile = file(ks)
                storePassword = System.getenv("DR_KEYSTORE_PASSWORD")
                keyAlias = System.getenv("DR_KEY_ALIAS")
                keyPassword = System.getenv("DR_KEY_PASSWORD")
            }
        }
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(getDefaultProguardFile("proguard-android-optimize.txt"), "proguard-rules.pro")
            signingConfig = signingConfigs.findByName("release") ?: signingConfigs.getByName("debug")
        }
        // A test build installed beside Dragon Rage instead of over it (./gradlew assembleDebug -PsideBySide): its
        // own package and the name "Dragon Rage (teste)" (src/sideBySide), so the player's app and its data stay.
        if (project.hasProperty("sideBySide")) {
            getByName("debug") { applicationIdSuffix = ".teste" }
        }
    }
    if (project.hasProperty("sideBySide")) {
        sourceSets.getByName("debug").res.srcDir("src/sideBySide/res")
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }
    buildFeatures {
        compose = true
        buildConfig = true
    }
    packaging {
        resources.excludes += "/META-INF/{AL2.0,LGPL2.1}"
        // the engine is loaded from a file at a fixed address (src/main/cpp/loader.c): installed as files, not stripped
        jniLibs {
            useLegacyPackaging = true
            keepDebugSymbols += "**/libbt3.so"
        }
    }
}

dependencies {
    implementation(libs.androidx.core.ktx)
    implementation(libs.androidx.lifecycle.runtime.ktx)
    implementation(libs.androidx.lifecycle.runtime.compose)
    implementation(libs.androidx.lifecycle.viewmodel.compose)
    implementation(libs.androidx.activity.compose)
    implementation(libs.androidx.navigation.compose)
    implementation(platform(libs.androidx.compose.bom))
    implementation(libs.androidx.ui)
    implementation(libs.androidx.ui.graphics)
    implementation(libs.androidx.ui.tooling.preview)
    implementation(libs.androidx.material3)
    implementation(libs.androidx.material.icons.extended)
    implementation(libs.commons.compress) // texture packs and mods: zips java.util.zip cannot read (data/FileUtils.kt)
    debugImplementation(libs.androidx.ui.tooling)
}
