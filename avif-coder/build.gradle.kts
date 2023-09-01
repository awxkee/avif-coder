@file:Suppress("UnstableApiUsage")

plugins {
    id("com.android.library")
    id("org.jetbrains.kotlin.android")
    id("maven-publish")
}

task("androidSourcesJar", Jar::class) {
    archiveClassifier.set("sources")
    from(android.sourceSets.getByName("main").java.srcDirs)
}

afterEvaluate {
    publishing {
        publications {
            create<MavenPublication>("mavenJava") {
                groupId = "com.github.awxkee"
                artifactId = "avif-coder"
                version = "1.0.23"
                from(components["release"])
//                artifact("androidSourcesJar")
            }
        }
    }
}

android {
    publishing {
        singleVariant("release") {
            withSourcesJar()
            withJavadocJar()
        }
    }

    namespace = "com.github.awxkee.avifcoder"
    compileSdk = 34

    defaultConfig {
        minSdk = 24
        targetSdk = 34

        externalNativeBuild {
            cmake {
                ndkVersion = "25.2.9519653"
                cppFlags(
                    "-Wl,--build-id=none",
                    "-Wl,--gc-sections",
                    "-ffunction-sections",
                    "-fdata-sections",
                    "-fvisibility=hidden"
                )
                cFlags(
                    "-ffunction-sections",
                    "-fdata-sections",
                    "-fvisibility=hidden",
                    "-Wl,--build-id=none",
                    "-Wl,--gc-sections",
                )
                abiFilters += setOf("armeabi-v7a", "arm64-v8a", "x86_64")
            }
        }

    }

    buildTypes {
        release {
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro"
            )
            consumerProguardFiles("consumer-rules.pro")
        }
    }
    externalNativeBuild {
        cmake {
            version = "3.22.1"
            path("src/main/cpp/CMakeLists.txt")
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = JavaVersion.VERSION_17.toString()
    }
}
dependencies {
    implementation("androidx.annotation:annotation-jvm:1.6.0")
}
