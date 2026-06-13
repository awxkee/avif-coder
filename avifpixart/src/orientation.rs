/*
 * Copyright (c) Radzivon Bartoshyk 2026/6. All rights reserved.
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
use hpvcd::Orientation;

/// Generic pixel-buffer rotation; works for both `u8` and `u16` samples.
pub(crate) fn apply_orientation<T: Copy + Default>(
    rgba: &[T],
    w: usize,
    h: usize,
    o: Orientation,
) -> (Vec<T>, usize, usize) {
    let (nw, nh) = match o {
        Orientation::Rotate90
        | Orientation::Rotate270
        | Orientation::Transpose
        | Orientation::Transverse => (h, w),
        _ => (w, h),
    };
    (rotate_buf(w, h, rgba, o), nw, nh)
}

fn rotate_buf<T: Copy + Default>(w: usize, h: usize, px: &[T], o: Orientation) -> Vec<T> {
    let pixels = px.as_chunks::<3>().0;

    match o {
        Orientation::Normal => px.to_vec(),

        // Reverse every pixel
        Orientation::Rotate180 => pixels
            .iter()
            .rev()
            .flat_map(|p| p.iter().copied())
            .collect(),

        // Mirror each row left↔right
        Orientation::FlipH => pixels
            .chunks(w)
            .flat_map(|row| row.iter().rev())
            .flat_map(|p| p.iter().copied())
            .collect(),

        // Reverse row order top↔bottom
        Orientation::FlipV => pixels
            .chunks(w)
            .rev()
            .flat_map(|row| row.iter())
            .flat_map(|p| p.iter().copied())
            .collect(),

        // 90° CW:  dest(dr, dc) = source(h-1-dc, dr);  dims: w×h → h×w
        Orientation::Rotate90 => (0..w)
            .flat_map(|dr| (0..h).map(move |dc| (h - 1 - dc) * w + dr))
            .flat_map(|si| [px[si * 3], px[si * 3 + 1], px[si * 3 + 2]])
            .collect(),

        // 90° CCW: dest(dr, dc) = source(dc, w-1-dr);  dims: w×h → h×w
        Orientation::Rotate270 => (0..w)
            .flat_map(|dr| (0..h).map(move |dc| dc * w + (w - 1 - dr)))
            .flat_map(|si| [px[si * 3], px[si * 3 + 1], px[si * 3 + 2]])
            .collect(),

        // Main-diagonal reflection: dest(dr, dc) = source(dc, dr);  dims: w×h → h×w
        Orientation::Transpose => (0..w)
            .flat_map(|dr| (0..h).map(move |dc| dc * w + dr))
            .flat_map(|si| [px[si * 3], px[si * 3 + 1], px[si * 3 + 2]])
            .collect(),

        // Anti-diagonal reflection: dest(dr, dc) = source(h-1-dc, w-1-dr);  dims: w×h → h×w
        Orientation::Transverse => (0..w)
            .flat_map(|dr| (0..h).map(move |dc| (h - 1 - dc) * w + (w - 1 - dr)))
            .flat_map(|si| [px[si * 3], px[si * 3 + 1], px[si * 3 + 2]])
            .collect(),
    }
}
