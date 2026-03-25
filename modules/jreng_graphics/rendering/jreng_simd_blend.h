#pragma once

#include <cstdint>

#if defined(__arm64__) || defined(__aarch64__)
    #define JRENG_SIMD_NEON 1
    #include <arm_neon.h>
#elif defined(__x86_64__) || defined(_M_X64)
    #define JRENG_SIMD_SSE2 1
    #include <emmintrin.h>
#endif

namespace jreng::simd
{

static constexpr uint32_t ALPHA_FULL  { 255u };
static constexpr uint32_t ALPHA_SHIFT { 24u  };
static constexpr uint32_t RED_SHIFT   { 16u  };
static constexpr uint32_t GREEN_SHIFT { 8u   };
static constexpr uint32_t BYTE_MASK   { 0xFFu };

// ---------------------------------------------------------------------------
// blendSrcOver4
// Premultiplied src-over blend of 4 packed ARGB (0xAARRGGBB) pixels.
// Formula: dest = src + dest * (255 - srcAlpha) / 255
// ---------------------------------------------------------------------------

inline void blendSrcOver4 (uint32_t* dest, const uint32_t* src) noexcept
{
#if JRENG_SIMD_SSE2

    // Load 4 src and 4 dest pixels as 16-byte vectors.
    __m128i vSrc  { _mm_loadu_si128 (reinterpret_cast<const __m128i*> (src))  };
    __m128i vDest { _mm_loadu_si128 (reinterpret_cast<const __m128i*> (dest)) };
    __m128i vZero { _mm_setzero_si128() };

    // Unpack lower/upper 2 pixels to 16-bit lanes so we can do 16-bit maths.
    __m128i srcLo  { _mm_unpacklo_epi8 (vSrc,  vZero) };
    __m128i srcHi  { _mm_unpackhi_epi8 (vSrc,  vZero) };
    __m128i destLo { _mm_unpacklo_epi8 (vDest, vZero) };
    __m128i destHi { _mm_unpackhi_epi8 (vDest, vZero) };

    // Extract alpha from each src pixel and broadcast it to all 4 byte lanes
    // of that pixel.  Alpha sits in byte 3 of each 32-bit word (0xAARRGGBB).
    // After unpack to 16-bit, pixel 0 occupies lanes [0..3], pixel 1 [4..7].
    // _mm_shufflelo_epi16 / _mm_shufflehi_epi16 operate on 16-bit granules:
    // alpha is at lane index 3 (lo half) and 7 (hi half) within each group.
    // Broadcast lane 3 → all 4 lanes of the lo 64-bit half, etc.
    __m128i alphaLo { _mm_shufflelo_epi16 (_mm_shufflehi_epi16 (srcLo, 0xFF), 0xFF) };
    __m128i alphaHi { _mm_shufflelo_epi16 (_mm_shufflehi_epi16 (srcHi, 0xFF), 0xFF) };

    // invAlpha = 255 - srcAlpha, broadcast same way.
    __m128i vAlphaFull  { _mm_set1_epi16 (static_cast<short> (ALPHA_FULL)) };
    __m128i invAlphaLo  { _mm_sub_epi16 (vAlphaFull, alphaLo) };
    __m128i invAlphaHi  { _mm_sub_epi16 (vAlphaFull, alphaHi) };

    // dest * invAlpha  (16-bit multiply, keep low 16 bits — safe for 8-bit * 8-bit).
    __m128i scaledLo { _mm_mullo_epi16 (destLo, invAlphaLo) };
    __m128i scaledHi { _mm_mullo_epi16 (destHi, invAlphaHi) };

    // Approximate division by 255 as (x + 128) >> 8 — error < 1 LSB for 8-bit inputs.
    __m128i vBias { _mm_set1_epi16 (128) };
    scaledLo = _mm_srli_epi16 (_mm_add_epi16 (scaledLo, vBias), 8);
    scaledHi = _mm_srli_epi16 (_mm_add_epi16 (scaledHi, vBias), 8);

    // Repack 16-bit lanes back to 8-bit, then saturated-add src on top.
    __m128i scaled  { _mm_packus_epi16 (scaledLo, scaledHi) };
    __m128i result  { _mm_adds_epu8 (vSrc, scaled) };

    _mm_storeu_si128 (reinterpret_cast<__m128i*> (dest), result);

#elif JRENG_SIMD_NEON

    uint8x16_t vSrc  { vld1q_u8 (reinterpret_cast<const uint8_t*> (src))  };
    uint8x16_t vDest { vld1q_u8 (reinterpret_cast<const uint8_t*> (dest)) };

    // Split into low (pixels 0-1) and high (pixels 2-3) halves.
    uint8x8_t srcLo  { vget_low_u8 (vSrc)  };
    uint8x8_t srcHi  { vget_high_u8 (vSrc) };
    uint8x8_t destLo { vget_low_u8 (vDest) };
    uint8x8_t destHi { vget_high_u8 (vDest) };

    // Broadcast alpha byte to all 4 channel positions per pixel.
    // Memory layout per pixel: [B, G, R, A] — alpha at byte 3 and 7.
    uint8x8_t alphaIdx { vcreate_u8 (0x0707070703030303ULL) };
    uint8x8_t alphaLo  { vtbl1_u8 (srcLo, alphaIdx) };
    uint8x8_t alphaHi  { vtbl1_u8 (srcHi, alphaIdx) };

    uint8x8_t vFull      { vdup_n_u8 (static_cast<uint8_t> (ALPHA_FULL)) };
    uint8x8_t invAlphaLo { vsub_u8 (vFull, alphaLo) };
    uint8x8_t invAlphaHi { vsub_u8 (vFull, alphaHi) };

    // dest * invAlpha: widen to 16-bit, multiply, round-shift-narrow back to 8-bit.
    uint8x8_t scaledLo { vrshrn_n_u16 (vmull_u8 (destLo, invAlphaLo), 8) };
    uint8x8_t scaledHi { vrshrn_n_u16 (vmull_u8 (destHi, invAlphaHi), 8) };

    // result = src + scaled (saturated add).
    uint8x8_t resultLo { vqadd_u8 (srcLo, scaledLo) };
    uint8x8_t resultHi { vqadd_u8 (srcHi, scaledHi) };

    vst1q_u8 (reinterpret_cast<uint8_t*> (dest), vcombine_u8 (resultLo, resultHi));

#else

    for (int i { 0 }; i < 4; ++i)
    {
        const uint32_t s         { src[i]  };
        const uint32_t d         { dest[i] };
        const uint32_t srcAlpha  { (s >> ALPHA_SHIFT) & BYTE_MASK };
        const uint32_t invAlpha  { ALPHA_FULL - srcAlpha };

        const uint32_t rD { (d >> RED_SHIFT)   & BYTE_MASK };
        const uint32_t gD { (d >> GREEN_SHIFT) & BYTE_MASK };
        const uint32_t bD {  d                 & BYTE_MASK };
        const uint32_t aD { (d >> ALPHA_SHIFT) & BYTE_MASK };

        // (x + 128) >> 8 approximates x / 255 with < 1 LSB error.
        const uint32_t rOut { ((s >> RED_SHIFT)   & BYTE_MASK) + ((rD * invAlpha + 128u) >> 8u) };
        const uint32_t gOut { ((s >> GREEN_SHIFT) & BYTE_MASK) + ((gD * invAlpha + 128u) >> 8u) };
        const uint32_t bOut { ( s                 & BYTE_MASK) + ((bD * invAlpha + 128u) >> 8u) };
        const uint32_t aOut { ((s >> ALPHA_SHIFT) & BYTE_MASK) + ((aD * invAlpha + 128u) >> 8u) };

        dest[i] = ((aOut > ALPHA_FULL ? ALPHA_FULL : aOut) << ALPHA_SHIFT)
                | ((rOut > ALPHA_FULL ? ALPHA_FULL : rOut) << RED_SHIFT)
                | ((gOut > ALPHA_FULL ? ALPHA_FULL : gOut) << GREEN_SHIFT)
                |  (bOut > ALPHA_FULL ? ALPHA_FULL : bOut);
    }

#endif
}

// ---------------------------------------------------------------------------
// blendMonoTinted4
// Composite 4 pixels from a mono glyph atlas onto dest.
// `alpha`       — 4 coverage bytes from the atlas (one per pixel).
// `premulFgColor` — packed 0xAARRGGBB where R,G,B are NOT premultiplied
//                   (they are scaled by the atlas alpha here before blending).
// ---------------------------------------------------------------------------

inline void blendMonoTinted4 (uint32_t*       dest,
                               const uint8_t*  alpha,
                               uint32_t        premulFgColor) noexcept
{
    // Unpack the foreground colour channels once; used for every pixel.
    const uint32_t fgR { (premulFgColor >> RED_SHIFT)   & BYTE_MASK };
    const uint32_t fgG { (premulFgColor >> GREEN_SHIFT) & BYTE_MASK };
    const uint32_t fgB {  premulFgColor                 & BYTE_MASK };

#if JRENG_SIMD_SSE2

    // Build 4 src pixels from atlas alpha and fg colour, then delegate to
    // blendSrcOver4.  The SIMD path below does both steps in one pass
    // to avoid a redundant store + reload cycle.

    // Pack fg channels into a 16-bit vector replicated across pixel slots.
    // We compute (fgCh * a + 128) >> 8 per channel, per pixel, in 16-bit.

    __m128i vZero  { _mm_setzero_si128() };
    __m128i vBias  { _mm_set1_epi16 (128) };

    // Broadcast fg channels into separate 16-bit vectors (same value in all 8 lanes).
    __m128i vFgR { _mm_set1_epi16 (static_cast<short> (fgR)) };
    __m128i vFgG { _mm_set1_epi16 (static_cast<short> (fgG)) };
    __m128i vFgB { _mm_set1_epi16 (static_cast<short> (fgB)) };

    // Load 4 atlas alpha bytes and zero-extend to 16-bit in a 128-bit register.
    // We place them at pixel positions [0..3] by loading the 4 bytes as a 32-bit
    // scalar and unpacking.
    uint32_t alphaPacked { static_cast<uint32_t> (alpha[0])
                         | (static_cast<uint32_t> (alpha[1]) << 8u)
                         | (static_cast<uint32_t> (alpha[2]) << 16u)
                         | (static_cast<uint32_t> (alpha[3]) << 24u) };
    __m128i vAlpha4 { _mm_cvtsi32_si128 (static_cast<int> (alphaPacked)) };
    // Unpack to 16-bit: [a0, a1, a2, a3, 0, 0, 0, 0]
    __m128i vA16 { _mm_unpacklo_epi8 (vAlpha4, vZero) };

    // Shuffle so each 16-bit alpha value is in the correct channel slot.
    // After building src pixels in ARGB 32-bit order (A, R, G, B as 16-bit pairs):
    // We need per-pixel vectors.  It is simpler to compute scalar here since
    // the interleave into 0xAARRGGBB words is awkward in SSE2 without SSSE3.
    // Fall through to scalar pixel build, then use blendSrcOver4.
    (void) vFgR; (void) vFgG; (void) vFgB; (void) vA16; (void) vBias;

    uint32_t srcPixels[4];
    for (int i { 0 }; i < 4; ++i)
    {
        const uint32_t a   { alpha[i] };
        const uint32_t sR  { (fgR * a + 128u) >> 8u };
        const uint32_t sG  { (fgG * a + 128u) >> 8u };
        const uint32_t sB  { (fgB * a + 128u) >> 8u };
        srcPixels[i] = (a << ALPHA_SHIFT) | (sR << RED_SHIFT) | (sG << GREEN_SHIFT) | sB;
    }
    blendSrcOver4 (dest, srcPixels);

#elif JRENG_SIMD_NEON

    uint32_t srcPixels[4];
    for (int i { 0 }; i < 4; ++i)
    {
        const uint32_t a  { alpha[i] };
        const uint32_t sR { (fgR * a + 128u) >> 8u };
        const uint32_t sG { (fgG * a + 128u) >> 8u };
        const uint32_t sB { (fgB * a + 128u) >> 8u };
        srcPixels[i] = (a << ALPHA_SHIFT) | (sR << RED_SHIFT) | (sG << GREEN_SHIFT) | sB;
    }
    blendSrcOver4 (dest, srcPixels);

#else

    for (int i { 0 }; i < 4; ++i)
    {
        const uint32_t a  { alpha[i] };
        const uint32_t sR { (fgR * a + 128u) >> 8u };
        const uint32_t sG { (fgG * a + 128u) >> 8u };
        const uint32_t sB { (fgB * a + 128u) >> 8u };

        const uint32_t srcPixel  { (a << ALPHA_SHIFT) | (sR << RED_SHIFT) | (sG << GREEN_SHIFT) | sB };
        const uint32_t d         { dest[i] };
        const uint32_t invAlpha  { ALPHA_FULL - a };

        const uint32_t rD { (d >> RED_SHIFT)   & BYTE_MASK };
        const uint32_t gD { (d >> GREEN_SHIFT) & BYTE_MASK };
        const uint32_t bD {  d                 & BYTE_MASK };
        const uint32_t aD { (d >> ALPHA_SHIFT) & BYTE_MASK };

        const uint32_t rOut { sR + ((rD * invAlpha + 128u) >> 8u) };
        const uint32_t gOut { sG + ((gD * invAlpha + 128u) >> 8u) };
        const uint32_t bOut { sB + ((bD * invAlpha + 128u) >> 8u) };
        const uint32_t aOut {  a + ((aD * invAlpha + 128u) >> 8u) };

        (void) srcPixel;

        dest[i] = ((aOut > ALPHA_FULL ? ALPHA_FULL : aOut) << ALPHA_SHIFT)
                | ((rOut > ALPHA_FULL ? ALPHA_FULL : rOut) << RED_SHIFT)
                | ((gOut > ALPHA_FULL ? ALPHA_FULL : gOut) << GREEN_SHIFT)
                |  (bOut > ALPHA_FULL ? ALPHA_FULL : bOut);
    }

#endif
}

// ---------------------------------------------------------------------------
// fillOpaque4
// Write 4 identical opaque ARGB pixels.  No blending — pure store.
// ---------------------------------------------------------------------------

inline void fillOpaque4 (uint32_t* dest, uint32_t color) noexcept
{
#if JRENG_SIMD_SSE2

    _mm_storeu_si128 (reinterpret_cast<__m128i*> (dest),
                      _mm_set1_epi32 (static_cast<int> (color)));

#elif JRENG_SIMD_NEON

    vst1q_u32 (dest, vdupq_n_u32 (color));

#else

    dest[0] = color;
    dest[1] = color;
    dest[2] = color;
    dest[3] = color;

#endif
}

} // namespace jreng::simd
