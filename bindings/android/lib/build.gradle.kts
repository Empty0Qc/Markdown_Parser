plugins {
    id("com.android.library") version "8.4.0"
    id("org.jetbrains.kotlin.android") version "2.0.0"
    id("maven-publish")
}

group   = "com.mkparser"
version = "0.1.1"

android {
    namespace  = "com.mkparser"
    compileSdk = 35

    defaultConfig {
        minSdk = 24

        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }

        externalNativeBuild {
            cmake { cppFlags("") }
        }
    }

    externalNativeBuild {
        cmake {
            path    = file("CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }
    kotlinOptions {
        jvmTarget = "17"
    }

    // Kotlin sources live entirely in src/main/kotlin.
    // MkParser.kt is kept in sync with bindings/android/MkParser.kt.
    sourceSets {
        getByName("main") {
            kotlin.srcDirs("src/main/kotlin")
        }
    }
}

dependencies {
    implementation("androidx.recyclerview:recyclerview:1.3.2")
}

afterEvaluate {
    publishing {
        publications {
            create<MavenPublication>("release") {
                artifact(tasks.named("bundleReleaseAar"))
                groupId    = "com.mkparser"
                artifactId = "mk-parser"
                version    = "0.1.1"
                pom {
                    packaging = "aar"
                    withXml {
                        val deps = asNode().appendNode("dependencies")
                        configurations["releaseRuntimeClasspath"].resolvedConfiguration
                            .firstLevelModuleDependencies.forEach { dep ->
                                val d = deps.appendNode("dependency")
                                d.appendNode("groupId", dep.moduleGroup)
                                d.appendNode("artifactId", dep.moduleName)
                                d.appendNode("version", dep.moduleVersion)
                                d.appendNode("scope", "runtime")
                            }
                    }
                }
            }
        }
    }
}
