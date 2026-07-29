/*
 * Copyright (c) Radzivon Bartoshyk. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1.  Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * 2.  Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * 3.  Neither the name of the copyright holder nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

use thiserror::Error;

#[derive(Debug, Error)]
pub enum WeaverError {
    #[error("Data is not heic file")]
    InvalidHeic,
    #[error("HEIC decoder failed with an errror {0}")]
    FailedToDecodeHeic(String),
    #[error("AVIF AV2 decoder failed with an errror {0}")]
    FailedToDecodeAv2(String),
    #[cfg(all(
        target_os = "android",
        any(target_arch = "aarch64", target_arch = "arm")
    ))]
    #[error("Unsupported matrix coefficients {0:?}")]
    UnsupportedMatrix(hpvcd::MatrixCoefficients),
    #[cfg(all(
        target_os = "android",
        any(target_arch = "aarch64", target_arch = "arm")
    ))]
    #[error("Unsupported AV2 matrix coefficients {0:?}")]
    UnsupportedMatrixAv2(tealdust::MatrixCoefficients),
    #[error("Depth signalled for encoded plane doesn't match the container")]
    MismatchedBitDepth,
    #[error("Failed to allocate memory with size {0}")]
    FailedToAllocateMemory(u64),
    #[error("YUV decoding failed with an error {0}")]
    YuvDecodingSignalledError(String),
    #[error("YUV decoding failed with an error {0}")]
    PixelFormatIsNotSupported(String),
    #[error("Monochrome in current path is not supported")]
    MonochromeIsNotSupported,
}
