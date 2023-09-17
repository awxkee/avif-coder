package com.radzivon.bartoshyk.avif.coder

enum class ScaleMode(internal val value: Int) {
    FIT(1),
    FILL(2),

    // Just resize, not aspect into provided dimensions
    RESIZE(3)
}