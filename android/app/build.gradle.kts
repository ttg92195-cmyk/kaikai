plugins {
    id("com.android.application")
}

android {
    namespace = "com.kaikai.game"
    compileSdk = 34

    defaultConfig {
        applicationId = "com.kaikai.game"
        minSdk = 21
        targetSdk = 34
        versionCode = 1
        versionName = "1.0.0"
    }

    buildTypes {
        release {
            isMinifyEnabled = false
            isShrinkResources = false
        }
        debug {
            isMinifyEnabled = false
            isDebuggable = true
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }

    // Native libraries are pre-built with CMake and placed in jniLibs/
    // No externalNativeBuild block needed - Gradle just packages the .so files
    sourceSets {
        getByName("main") {
            jniLibs.srcDirs("src/main/jniLibs")
        }
    }

    // Only package the ABIs we built
    packaging {
        jniLibs {
            // Exclude unused ABIs
            excludes += setOf()
        }
    }
}

dependencies {
    // No dependencies - pure native C++ game
}
