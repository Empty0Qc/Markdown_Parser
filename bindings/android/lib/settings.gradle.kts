pluginManagement {
    repositories {
        maven { url = uri("https://artifactory.intra.xiaojukeji.com/artifactory/public/") }
        maven { url = uri("https://maven.aliyun.com/repository/google") }
        maven { url = uri("https://maven.aliyun.com/nexus/content/repositories/jcenter/") }
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        maven { url = uri("https://artifactory.intra.xiaojukeji.com/artifactory/public/") }
        maven { url = uri("https://maven.aliyun.com/repository/google") }
        maven { url = uri("https://maven.aliyun.com/nexus/content/repositories/jcenter/") }
        google()
        mavenCentral()
    }
}

rootProject.name = "mk-parser"
