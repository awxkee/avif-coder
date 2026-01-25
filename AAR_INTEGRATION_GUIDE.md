# AAR Integration Guide

## Overview

This guide explains all methods to include AAR (Android Archive) files in your Android project. AAR files are the standard way to distribute Android libraries, containing compiled code, resources, and native libraries.

### What are AAR Files?

An AAR file is similar to a JAR file but specifically for Android. It contains:
- Compiled Java/Kotlin classes
- Android resources (layouts, drawables, etc.)
- Native libraries (`.so` files) for different ABIs
- AndroidManifest.xml
- ProGuard rules
- Metadata

### Why Use AAR Files?

**Benefits:**
- Pre-compiled, faster build times
- Smaller repository size (no source code)
- Easier distribution
- Version management
- Native libraries included

**When to Use:**
- Distributing libraries to other projects
- Using pre-built binaries
- Reducing build complexity
- Sharing libraries across teams

### Available AAR Files

From this project, you have two AAR files:
- **avif-coder-release.aar** (~9.2 MB) - Main library with native libraries
- **avif-coder-coil-release.aar** (~8.8 KB) - Coil integration wrapper

**Note:** If using Coil integration, you need both AARs (avif-coder-coil depends on avif-coder).

---

## Method 1: Local AAR Files with flatDir (Recommended for Local Development)

This is the simplest method for local development and testing.

### Step 1: Create libs Directory

Create a `libs` directory in your project root (or in your app module):

```bash
mkdir -p libs
# or
mkdir -p app/libs
```

### Step 2: Copy AAR Files

Copy the AAR files to the `libs` directory:

```bash
cp avif-coder/build/outputs/aar/avif-coder-release.aar libs/
cp avif-coder-coil/build/outputs/aar/avif-coder-coil-release.aar libs/
```

### Step 3: Configure Repository

**Option A: In `settings.gradle` (Recommended for Gradle 7.0+)**

```gradle
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        maven { url 'https://jitpack.io' }
        
        // Add flatDir repository
        flatDir {
            dirs 'libs'  // or 'app/libs' if AARs are in app module
        }
    }
}
```

**Option B: In root `build.gradle` (Legacy projects)**

```gradle
allprojects {
    repositories {
        google()
        mavenCentral()
        
        flatDir {
            dirs 'libs'
        }
    }
}
```

**Option C: In module `build.gradle` (Module-specific)**

```gradle
android {
    // ... other config
}

repositories {
    flatDir {
        dirs 'libs'
    }
}
```

### Step 4: Add Dependencies

In your `app/build.gradle` (or module `build.gradle`):

```gradle
dependencies {
    // Main library
    implementation(name: 'avif-coder-release', ext: 'aar')
    
    // Coil integration (if needed)
    implementation(name: 'avif-coder-coil-release', ext: 'aar')
    
    // Coil library (required for avif-coder-coil)
    implementation 'io.coil-kt:coil:2.5.0'
    
    // ... other dependencies
}
```

### Complete Example

**Project Structure:**
```
MyProject/
├── libs/
│   ├── avif-coder-release.aar
│   └── avif-coder-coil-release.aar
├── app/
│   └── build.gradle
├── build.gradle
└── settings.gradle
```

**settings.gradle:**
```gradle
pluginManagement {
    repositories {
        gradlePluginPortal()
        google()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        flatDir {
            dirs 'libs'
        }
    }
}

rootProject.name = "MyProject"
include ':app'
```

**app/build.gradle:**
```gradle
dependencies {
    implementation(name: 'avif-coder-release', ext: 'aar')
    implementation(name: 'avif-coder-coil-release', ext: 'aar')
    implementation 'io.coil-kt:coil:2.5.0'
}
```

### Pros and Cons

**Pros:**
- Simple setup
- No publishing required
- Works immediately
- Good for local development

**Cons:**
- AARs must be manually copied
- No version management
- Not suitable for CI/CD without scripts
- Can't use transitive dependencies easily

---

## Method 2: Direct File Dependencies

The most straightforward method, but with limitations.

### Setup

**Step 1:** Place AAR files in your project (e.g., `app/libs/` or `libs/`)

**Step 2:** Add dependencies directly:

```gradle
dependencies {
    implementation files('libs/avif-coder-release.aar')
    implementation files('libs/avif-coder-coil-release.aar')
    
    // Coil is required for avif-coder-coil
    implementation 'io.coil-kt:coil:2.5.0'
}
```

### Alternative: Using fileTree

```gradle
dependencies {
    implementation fileTree(dir: 'libs', include: ['*.aar'])
    implementation 'io.coil-kt:coil:2.5.0'
}
```

### Complete Example

**app/build.gradle:**
```gradle
android {
    // ... configuration
}

dependencies {
    // Direct file dependencies
    implementation files('libs/avif-coder-release.aar')
    implementation files('libs/avif-coder-coil-release.aar')
    
    // Required dependencies
    implementation 'io.coil-kt:coil:2.5.0'
    implementation 'androidx.annotation:annotation:1.7.1'
    
    // ... other dependencies
}
```

### Pros and Cons

**Pros:**
- Simplest method
- No repository configuration needed
- Direct control over files

**Cons:**
- No transitive dependency resolution
- Manual file management
- Paths must be correct
- Not ideal for version management

---

## Method 3: Maven Local Repository

Publish AARs to your local Maven repository (`~/.m2/repository/`) and use them like regular Maven dependencies.

### Step 1: Publish to Maven Local

From the avif-coder project directory:

```bash
# Publish both AARs to mavenLocal
./gradlew :avif-coder:publishToMavenLocal :avif-coder-coil:publishToMavenLocal
```

This will install the AARs to:
- `~/.m2/repository/com/github/awxkee/avif-coder/0.0.10/`
- `~/.m2/repository/com/github/awxkee/avif-coder-coil/1.5.12/`

### Step 2: Configure Repository

In your project's `settings.gradle` or `build.gradle`:

```gradle
dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
        mavenLocal()  // Add this
    }
}
```

Or in root `build.gradle` (legacy):

```gradle
allprojects {
    repositories {
        google()
        mavenCentral()
        mavenLocal()
    }
}
```

### Step 3: Add Dependencies

In your `app/build.gradle`:

```gradle
dependencies {
    implementation 'com.github.awxkee:avif-coder:0.0.10'
    implementation 'com.github.awxkee:avif-coder-coil:1.5.12'
    implementation 'io.coil-kt:coil:2.5.0'
}
```

### Complete Example

**settings.gradle:**
```gradle
dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        mavenLocal()  // Local Maven repository
    }
}
```

**app/build.gradle:**
```gradle
dependencies {
    implementation 'com.github.awxkee:avif-coder:0.0.10'
    implementation 'com.github.awxkee:avif-coder-coil:1.5.12'
    implementation 'io.coil-kt:coil:2.5.0'
}
```

### Checking Installed Versions

To see what's installed in mavenLocal:

```bash
ls -la ~/.m2/repository/com/github/awxkee/avif-coder/
ls -la ~/.m2/repository/com/github/awxkee/avif-coder-coil/
```

### Pros and Cons

**Pros:**
- Works like regular Maven dependencies
- Version management
- Transitive dependencies work
- Standard Maven structure
- Good for local development and testing

**Cons:**
- Only available on local machine
- Must republish after changes
- Not suitable for team sharing
- Requires publishing step

---

## Method 4: Local Maven Repository (Custom Location)

Create a custom local Maven repository structure for team sharing or CI/CD.

### Step 1: Create Maven Repository Structure

Create the following directory structure:

```
local-maven-repo/
└── com/
    └── github/
        └── awxkee/
            ├── avif-coder/
            │   └── 0.0.10/
            │       ├── avif-coder-0.0.10.aar
            │       ├── avif-coder-0.0.10.pom
            │       └── avif-coder-0.0.10.aar.sha1
            └── avif-coder-coil/
                └── 1.5.12/
                    ├── avif-coder-coil-1.5.12.aar
                    ├── avif-coder-coil-1.5.12.pom
                    └── avif-coder-coil-1.5.12.aar.sha1
```

### Step 2: Copy AAR Files

```bash
mkdir -p local-maven-repo/com/github/awxkee/avif-coder/0.0.10
mkdir -p local-maven-repo/com/github/awxkee/avif-coder-coil/1.5.12

cp avif-coder/build/outputs/aar/avif-coder-release.aar \
   local-maven-repo/com/github/awxkee/avif-coder/0.0.10/avif-coder-0.0.10.aar

cp avif-coder-coil/build/outputs/aar/avif-coder-coil-release.aar \
   local-maven-repo/com/github/awxkee/avif-coder-coil/1.5.12/avif-coder-coil-1.5.12.aar
```

### Step 3: Create POM Files

Create `avif-coder-0.0.10.pom`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
    http://maven.apache.org/xsd/maven-4.0.0.xsd" 
    xmlns="http://maven.apache.org/POM/4.0.0"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <modelVersion>4.0.0</modelVersion>
  <groupId>com.github.awxkee</groupId>
  <artifactId>avif-coder</artifactId>
  <version>0.0.10</version>
  <packaging>aar</packaging>
  <dependencies>
    <dependency>
      <groupId>androidx.annotation</groupId>
      <artifactId>annotation-jvm</artifactId>
      <version>1.7.1</version>
    </dependency>
  </dependencies>
</project>
```

Create `avif-coder-coil-1.5.12.pom`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<project xsi:schemaLocation="http://maven.apache.org/POM/4.0.0 
    http://maven.apache.org/xsd/maven-4.0.0.xsd" 
    xmlns="http://maven.apache.org/POM/4.0.0"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance">
  <modelVersion>4.0.0</modelVersion>
  <groupId>com.github.awxkee</groupId>
  <artifactId>avif-coder-coil</artifactId>
  <version>1.5.12</version>
  <packaging>aar</packaging>
  <dependencies>
    <dependency>
      <groupId>com.github.awxkee</groupId>
      <artifactId>avif-coder</artifactId>
      <version>0.0.10</version>
    </dependency>
    <dependency>
      <groupId>io.coil-kt</groupId>
      <artifactId>coil</artifactId>
      <version>2.5.0</version>
    </dependency>
  </dependencies>
</project>
```

### Step 4: Configure Repository

In your project, add the local Maven repository:

**settings.gradle:**
```gradle
dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
        maven {
            url = uri("file://${projectDir}/../local-maven-repo")
            // Or use absolute path:
            // url = uri("file:///path/to/local-maven-repo")
        }
    }
}
```

### Step 5: Add Dependencies

```gradle
dependencies {
    implementation 'com.github.awxkee:avif-coder:0.0.10'
    implementation 'com.github.awxkee:avif-coder-coil:1.5.12'
    implementation 'io.coil-kt:coil:2.5.0'
}
```

### Pros and Cons

**Pros:**
- Can be shared via version control or network share
- Works with CI/CD
- Standard Maven structure
- Version management

**Cons:**
- More complex setup
- Requires POM file creation
- Manual maintenance

---

## Method 5: Project-Level libs Directory (Standard Android)

This is the standard Android project structure for local libraries.

### Setup

**Step 1:** Create `libs` directory in your project root or app module

**Step 2:** Copy AAR files to `libs/`

**Step 3:** Configure in root `build.gradle`:

```gradle
allprojects {
    repositories {
        google()
        mavenCentral()
        
        flatDir {
            dirs "$rootDir/libs"  // Project root libs
            // or
            // dirs "${project(':app').projectDir}/libs"  // App module libs
        }
    }
}
```

**Step 4:** Add dependencies in `app/build.gradle`:

```gradle
dependencies {
    implementation(name: 'avif-coder-release', ext: 'aar')
    implementation(name: 'avif-coder-coil-release', ext: 'aar')
    implementation 'io.coil-kt:coil:2.5.0'
}
```

### Complete Example

**Project Structure:**
```
MyProject/
├── libs/
│   ├── avif-coder-release.aar
│   └── avif-coder-coil-release.aar
├── app/
│   └── build.gradle
└── build.gradle
```

**build.gradle (root):**
```gradle
allprojects {
    repositories {
        google()
        mavenCentral()
        flatDir {
            dirs "$rootDir/libs"
        }
    }
}
```

**app/build.gradle:**
```gradle
dependencies {
    implementation(name: 'avif-coder-release', ext: 'aar')
    implementation(name: 'avif-coder-coil-release', ext: 'aar')
    implementation 'io.coil-kt:coil:2.5.0'
}
```

### Pros and Cons

**Pros:**
- Standard Android project structure
- Works across multiple modules
- Simple to understand

**Cons:**
- Requires root build.gradle configuration
- AARs committed to repository (if using VCS)

---

## Complete Working Examples

### Example 1: Using Both AARs with Coil Integration

**settings.gradle:**
```gradle
pluginManagement {
    repositories {
        gradlePluginPortal()
        google()
        mavenCentral()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.FAIL_ON_PROJECT_REPOS)
    repositories {
        google()
        mavenCentral()
        maven { url 'https://jitpack.io' }
        flatDir {
            dirs 'libs'
        }
    }
}

rootProject.name = "MyApp"
include ':app'
```

**app/build.gradle:**
```gradle
plugins {
    id 'com.android.application'
    id 'org.jetbrains.kotlin.android'
}

android {
    namespace 'com.example.myapp'
    compileSdk 34

    defaultConfig {
        applicationId "com.example.myapp"
        minSdk 24
        targetSdk 34
    }
    
    // ... other config
}

dependencies {
    // AVIF/HEIC decoder library
    implementation(name: 'avif-coder-release', ext: 'aar')
    
    // Coil integration (optional, for image loading)
    implementation(name: 'avif-coder-coil-release', ext: 'aar')
    implementation 'io.coil-kt:coil:2.5.0'
    
    // ... other dependencies
}
```

**Usage in Kotlin:**
```kotlin
// Direct usage
val coder = HeifCoder()
val bitmap = coder.decode(imageBytes)

// With Coil
val imageLoader = ImageLoader.Builder(context)
    .components {
        add(HeifDecoder.Factory())
    }
    .build()

imageView.load("https://example.com/image.avif", imageLoader)
```

### Example 2: Using Maven Local

**Step 1: Publish to mavenLocal**
```bash
cd /path/to/avif-coder
./gradlew :avif-coder:publishToMavenLocal :avif-coder-coil:publishToMavenLocal
```

**Step 2: Use in your project**

**settings.gradle:**
```gradle
dependencyResolutionManagement {
    repositories {
        google()
        mavenCentral()
        mavenLocal()  // Add this
    }
}
```

**app/build.gradle:**
```gradle
dependencies {
    implementation 'com.github.awxkee:avif-coder:0.0.10'
    implementation 'com.github.awxkee:avif-coder-coil:1.5.12'
    implementation 'io.coil-kt:coil:2.5.0'
}
```

---

## Troubleshooting

### Issue 1: "Could not find avif-coder-release.aar"

**Symptoms:**
```
Could not resolve: avif-coder-release
```

**Solutions:**
1. Verify AAR file exists in the specified directory
2. Check the path in `flatDir { dirs '...' }` matches actual location
3. Ensure file name matches exactly (case-sensitive)
4. Try using absolute path: `dirs '/absolute/path/to/libs'`

### Issue 2: Native Library Not Found

**Symptoms:**
```
java.lang.UnsatisfiedLinkError: dlopen failed: library "libcoder.so" not found
```

**Solutions:**
1. Verify AAR contains native libraries:
   ```bash
   unzip -l avif-coder-release.aar | grep "\.so$"
   ```
2. Ensure you're using the release AAR (not debug)
3. Check that your app's `minSdk` is 24 or higher
4. Verify ABI filters match your device

### Issue 3: Transitive Dependencies Not Resolved

**Symptoms:**
```
Could not resolve: androidx.annotation:annotation-jvm:1.7.1
```

**Solutions:**
1. Add missing dependencies explicitly:
   ```gradle
   implementation 'androidx.annotation:annotation-jvm:1.7.1'
   ```
2. For flatDir method, transitive dependencies don't work - add them manually
3. Use Maven Local method for automatic transitive dependency resolution

### Issue 4: Coil Integration Not Working

**Symptoms:**
```
HeifDecoder not found or not working
```

**Solutions:**
1. Ensure both AARs are included:
   ```gradle
   implementation(name: 'avif-coder-release', ext: 'aar')
   implementation(name: 'avif-coder-coil-release', ext: 'aar')
   ```
2. Verify Coil version compatibility:
   ```gradle
   implementation 'io.coil-kt:coil:2.5.0'
   ```
3. Check that you're registering the decoder:
   ```kotlin
   val imageLoader = ImageLoader.Builder(context)
       .components {
           add(HeifDecoder.Factory())
       }
       .build()
   ```

### Issue 5: Build Fails with "Repository Mode"

**Symptoms:**
```
RepositoryMode.FAIL_ON_PROJECT_REPOS is enabled
```

**Solutions:**
1. Add flatDir in `settings.gradle` under `dependencyResolutionManagement`:
   ```gradle
   dependencyResolutionManagement {
       repositories {
           flatDir {
               dirs 'libs'
           }
       }
   }
   ```
2. Or change to `PREFER_PROJECT` mode (not recommended)

### Issue 6: ProGuard/R8 Issues

**Symptoms:**
```
ClassNotFoundException or MethodNotFoundException at runtime
```

**Solutions:**
1. Add ProGuard rules to `app/proguard-rules.pro`:
   ```proguard
   -keep class com.radzivon.bartoshyk.avif.coder.** { *; }
   -keep class com.github.awxkee.avifcoil.** { *; }
   -keepclassmembers class * {
       native <methods>;
   }
   ```
2. The AAR should include consumer ProGuard rules, but verify they're being applied

### Issue 7: Version Conflicts

**Symptoms:**
```
Multiple versions of the same library
```

**Solutions:**
1. Use dependency resolution strategy:
   ```gradle
   configurations.all {
       resolutionStrategy {
           force 'androidx.annotation:annotation-jvm:1.7.1'
       }
   }
   ```
2. Check for conflicting dependencies:
   ```bash
   ./gradlew :app:dependencies
   ```

---

## Best Practices

### When to Use Each Method

**flatDir (Method 1):**
- Local development and testing
- Quick prototyping
- When you control the AAR files directly
- Small teams or single developer

**Direct File Dependencies (Method 2):**
- Simplest possible setup
- One-off integrations
- When you don't need transitive dependencies

**Maven Local (Method 3):**
- Local development across multiple projects
- Testing before publishing
- When you want Maven-like behavior locally

**Local Maven Repository (Method 4):**
- Team sharing via network/version control
- CI/CD pipelines
- When you need version management but can't use remote repos

**Project libs Directory (Method 5):**
- Standard Android project structure
- Multi-module projects
- When AARs are part of the project

### Version Management

1. **Naming Convention:**
   - Include version in filename: `avif-coder-0.0.10-release.aar`
   - Or use directory structure for versioning

2. **Version Control:**
   - Consider if AARs should be committed to Git
   - Large files may need Git LFS
   - Alternative: Use CI/CD to generate AARs

3. **Version Updates:**
   - Document version changes
   - Test compatibility before updating
   - Keep changelog

### Dependency Management

1. **Always Include Required Dependencies:**
   ```gradle
   // Required for avif-coder
   implementation 'androidx.annotation:annotation-jvm:1.7.1'
   
   // Required for avif-coder-coil
   implementation 'io.coil-kt:coil:2.5.0'
   ```

2. **Check Transitive Dependencies:**
   ```bash
   ./gradlew :app:dependencies --configuration releaseRuntimeClasspath
   ```

3. **Use Specific Versions:**
   - Avoid `+` version ranges for stability
   - Pin exact versions for reproducible builds

### Security Considerations

1. **Verify AAR Sources:**
   - Only use AARs from trusted sources
   - Verify checksums if available
   - Review included native libraries

2. **Native Library Security:**
   - Native libraries have full system access
   - Review what native code is included
   - Keep AARs updated for security patches

### Performance Tips

1. **ABI Filtering:**
   - If you only support certain architectures, filter ABIs:
   ```gradle
   android {
       defaultConfig {
           ndk {
               abiFilters 'arm64-v8a', 'armeabi-v7a'
           }
       }
   }
   ```

2. **ProGuard/R8:**
   - Enable code shrinking for release builds
   - Use provided ProGuard rules
   - Test thoroughly after enabling

---

## Special Considerations

### Native Libraries

The `avif-coder-release.aar` includes native libraries for:
- **arm64-v8a** (64-bit ARM)
- **armeabi-v7a** (32-bit ARM)
- **x86** (32-bit Intel)
- **x86_64** (64-bit Intel)

Total size: ~9.2 MB (includes all ABIs)

### Android 15+ Compatibility

The AAR files are built with 16 KB page size alignment for Android 15+ compatibility. No additional configuration needed.

### Minimum SDK

- **Minimum SDK:** 24 (Android 7.0)
- **Target SDK:** 34 (Android 14)
- **Compile SDK:** 34

### ProGuard Rules

The AAR includes consumer ProGuard rules. For custom rules, add to `app/proguard-rules.pro`:

```proguard
# Keep native methods
-keepclassmembers class * {
    native <methods>;
}

# Keep decoder classes
-keep class com.radzivon.bartoshyk.avif.coder.** { *; }
-keep class com.github.awxkee.avifcoil.** { *; }
```

### Using Both AARs Together

When using Coil integration, you need both AARs:

```gradle
dependencies {
    // Main library (required)
    implementation(name: 'avif-coder-release', ext: 'aar')
    
    // Coil integration (optional, but requires main library)
    implementation(name: 'avif-coder-coil-release', ext: 'aar')
    
    // Coil library (required for avif-coder-coil)
    implementation 'io.coil-kt:coil:2.5.0'
}
```

**Order matters:** `avif-coder` must be included before `avif-coder-coil`.

---

## Quick Reference

### Method Comparison

| Method | Setup Complexity | Version Management | Team Sharing | CI/CD Friendly |
|--------|-----------------|-------------------|--------------|----------------|
| flatDir | Low | No | Manual | Yes (with scripts) |
| Direct Files | Very Low | No | Manual | Yes (with scripts) |
| Maven Local | Medium | Yes | No | Limited |
| Local Maven Repo | High | Yes | Yes | Yes |
| Project libs | Low | No | Yes (VCS) | Yes |

### Recommended Methods by Use Case

- **Local Development:** flatDir or Maven Local
- **Team Sharing:** Local Maven Repository or Project libs
- **CI/CD:** Local Maven Repository
- **Quick Testing:** Direct File Dependencies
- **Production:** Maven Repository (remote) or Local Maven Repository

---

## Additional Resources

- [Android AAR Format](https://developer.android.com/studio/projects/android-library#aar-contents)
- [Gradle Dependency Management](https://docs.gradle.org/current/userguide/dependency_management.html)
- [Maven Local Repository](https://maven.apache.org/guides/mini/guide-installing-3rd-party-artifacts.html)

---

## Summary

All methods work, but choose based on your needs:

1. **For quick local testing:** Use Method 1 (flatDir) or Method 2 (Direct Files)
2. **For local development across projects:** Use Method 3 (Maven Local)
3. **For team sharing or CI/CD:** Use Method 4 (Local Maven Repository)
4. **For standard Android projects:** Use Method 5 (Project libs)

The AAR files are ready to use and include all necessary native libraries. Just choose the integration method that best fits your workflow!
