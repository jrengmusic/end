/**
 * @file ImageDecodeWin.cpp
 * @brief Windows native image decoding via WIC, with Linux fallback stub.
 *
 * @see ImageDecode.h
 */

#include "ImageDecode.h"
#include "ImageDecodeGif.h"

#if JUCE_WINDOWS

#include <wincodec.h>
#include <combaseapi.h>

namespace Terminal
{ /*____________________________________________________________________________*/

juce::Image loadImageNative (const void* data, size_t dataSize) noexcept
{
    juce::Image result;

    IWICImagingFactory* factory { nullptr };
    HRESULT hr { CoCreateInstance (CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                    IID_PPV_ARGS (&factory)) };

    if (SUCCEEDED (hr) and factory != nullptr)
    {
        IStream* stream { SHCreateMemStream (static_cast<const BYTE*> (data),
                                              static_cast<UINT> (dataSize)) };

        if (stream != nullptr)
        {
            IWICBitmapDecoder* decoder { nullptr };
            hr = factory->CreateDecoderFromStream (stream, nullptr,
                                                    WICDecodeMetadataCacheOnDemand, &decoder);

            if (SUCCEEDED (hr) and decoder != nullptr)
            {
                IWICBitmapFrameDecode* frame { nullptr };
                hr = decoder->GetFrame (0, &frame);

                if (SUCCEEDED (hr) and frame != nullptr)
                {
                    UINT w { 0 };
                    UINT h { 0 };
                    frame->GetSize (&w, &h);

                    if (w > 0 and h > 0)
                    {
                        IWICFormatConverter* converter { nullptr };
                        hr = factory->CreateFormatConverter (&converter);

                        if (SUCCEEDED (hr) and converter != nullptr)
                        {
                            hr = converter->Initialize (frame, GUID_WICPixelFormat32bppBGRA,
                                                        WICBitmapDitherTypeNone, nullptr,
                                                        0.0, WICBitmapPaletteTypeCustom);

                            if (SUCCEEDED (hr))
                            {
                                result = juce::Image (juce::Image::ARGB, static_cast<int> (w),
                                                      static_cast<int> (h), true);
                                juce::Image::BitmapData bm (result, juce::Image::BitmapData::writeOnly);

                                converter->CopyPixels (nullptr,
                                                        static_cast<UINT> (bm.lineStride),
                                                        static_cast<UINT> (bm.lineStride) * h,
                                                        reinterpret_cast<BYTE*> (bm.getLinePointer (0)));
                            }

                            converter->Release();
                        }
                    }

                    frame->Release();
                }

                decoder->Release();
            }

            stream->Release();
        }

        factory->Release();
    }

    return result;
}

ImageSequence loadImageSequenceNative (const void* data, size_t dataSize) noexcept
{
    ImageSequence seq;

    const bool isGif { dataSize >= 6
                       and static_cast<const char*> (data)[0] == 'G'
                       and static_cast<const char*> (data)[1] == 'I'
                       and static_cast<const char*> (data)[2] == 'F' };

    if (not isGif)
    {
        // Non-GIF: single frame via existing path
        juce::Image img { loadImageNative (data, dataSize) };

        if (img.isValid())
        {
            seq.width      = img.getWidth();
            seq.height     = img.getHeight();
            seq.frameCount = 1;
            int w { 0 };
            int h { 0 };
            swizzleARGBToRGBA (img, seq.pixels, w, h);
        }

        return seq;
    }

    // GIF path: parse metadata then compose frames via WIC
    const GifMetadata meta { parseGifMetadata (static_cast<const uint8_t*> (data), dataSize) };

    if (meta.isValid and not meta.frames.empty())
    {
        IWICImagingFactory* factory { nullptr };
        HRESULT hr { CoCreateInstance (CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                        IID_PPV_ARGS (&factory)) };

        if (SUCCEEDED (hr) and factory != nullptr)
        {
            IStream* stream { SHCreateMemStream (static_cast<const BYTE*> (data),
                                                  static_cast<UINT> (dataSize)) };

            if (stream != nullptr)
            {
                IWICBitmapDecoder* decoder { nullptr };
                hr = factory->CreateDecoderFromStream (stream, nullptr,
                                                        WICDecodeMetadataCacheOnDemand, &decoder);

                if (SUCCEEDED (hr) and decoder != nullptr)
                {
                    UINT nativeFrameCount { 0u };
                    decoder->GetFrameCount (&nativeFrameCount);

                    const int frameCount { static_cast<int> (
                        juce::jmin (static_cast<size_t> (nativeFrameCount),
                                    meta.frames.size())) };

                    const int canvasW { meta.canvasWidth };
                    const int canvasH { meta.canvasHeight };
                    const size_t frameBytes { static_cast<size_t> (canvasW)
                                              * static_cast<size_t> (canvasH) * 4u };
                    const size_t totalBytes { frameBytes * static_cast<size_t> (frameCount) };

                    seq.pixels.allocate (totalBytes, true);
                    seq.delays.allocate (static_cast<size_t> (frameCount), false);

                    juce::HeapBlock<uint8_t> canvas (frameBytes, true);
                    juce::HeapBlock<uint8_t> previousCanvas (frameBytes, true);

                    for (int i { 0 }; i < frameCount; ++i)
                    {
                        const GifFrameInfo& prev { meta.frames[static_cast<size_t> (i > 0 ? i - 1 : 0)] };

                        if (i > 0)
                        {
                            if (prev.disposal == 2)
                            {
                                const int clearLeft   { juce::jlimit (0, canvasW, prev.left) };
                                const int clearTop    { juce::jlimit (0, canvasH, prev.top) };
                                const int clearRight  { juce::jlimit (0, canvasW, prev.left + prev.width) };
                                const int clearBottom { juce::jlimit (0, canvasH, prev.top + prev.height) };

                                for (int row { clearTop }; row < clearBottom; ++row)
                                {
                                    uint8_t* dst { canvas.get()
                                                   + static_cast<size_t> (row * canvasW + clearLeft) * 4u };
                                    std::memset (dst, 0,
                                                 static_cast<size_t> (clearRight - clearLeft) * 4u);
                                }
                            }
                            else if (prev.disposal == 3)
                            {
                                std::memcpy (canvas.get(), previousCanvas.get(), frameBytes);
                            }
                        }

                        const GifFrameInfo& fi { meta.frames[static_cast<size_t> (i)] };

                        if (fi.disposal == 3)
                            std::memcpy (previousCanvas.get(), canvas.get(), frameBytes);

                        IWICBitmapFrameDecode* wicFrame { nullptr };
                        hr = decoder->GetFrame (static_cast<UINT> (i), &wicFrame);

                        if (SUCCEEDED (hr) and wicFrame != nullptr)
                        {
                            UINT subW { 0u };
                            UINT subH { 0u };
                            wicFrame->GetSize (&subW, &subH);

                            if (subW > 0u and subH > 0u)
                            {
                                IWICFormatConverter* converter { nullptr };
                                hr = factory->CreateFormatConverter (&converter);

                                if (SUCCEEDED (hr) and converter != nullptr)
                                {
                                    hr = converter->Initialize (wicFrame,
                                                                 GUID_WICPixelFormat32bppBGRA,
                                                                 WICBitmapDitherTypeNone,
                                                                 nullptr,
                                                                 0.0,
                                                                 WICBitmapPaletteTypeCustom);

                                    if (SUCCEEDED (hr))
                                    {
                                        // Render sub-frame into a temporary JUCE image
                                        juce::Image subImg (juce::Image::ARGB,
                                                            static_cast<int> (subW),
                                                            static_cast<int> (subH),
                                                            true);
                                        juce::Image::BitmapData bm (subImg,
                                                                     juce::Image::BitmapData::writeOnly);

                                        converter->CopyPixels (
                                            nullptr,
                                            static_cast<UINT> (bm.lineStride),
                                            static_cast<UINT> (bm.lineStride) * subH,
                                            reinterpret_cast<BYTE*> (bm.getLinePointer (0)));

                                        // Swizzle sub-frame to straight RGBA
                                        juce::HeapBlock<uint8_t> subRgba;
                                        int swW { 0 };
                                        int swH { 0 };
                                        swizzleARGBToRGBA (subImg, subRgba, swW, swH);

                                        const int iSubW { static_cast<int> (subW) };
                                        const int iSubH { static_cast<int> (subH) };
                                        const int blitLeft   { juce::jlimit (0, canvasW, fi.left) };
                                        const int blitTop    { juce::jlimit (0, canvasH, fi.top) };
                                        const int blitRight  { juce::jlimit (0, canvasW, fi.left + iSubW) };
                                        const int blitBottom { juce::jlimit (0, canvasH, fi.top + iSubH) };

                                        for (int row { blitTop }; row < blitBottom; ++row)
                                        {
                                            const int srcRow { row - fi.top };
                                            const uint8_t* src { subRgba.get()
                                                                  + static_cast<size_t> (
                                                                      srcRow * iSubW
                                                                      + (blitLeft - fi.left)) * 4u };
                                            uint8_t* dst { canvas.get()
                                                           + static_cast<size_t> (
                                                               row * canvasW + blitLeft) * 4u };

                                            for (int col { blitLeft }; col < blitRight; ++col)
                                            {
                                                if (src[3] > 0)
                                                {
                                                    dst[0] = src[0];
                                                    dst[1] = src[1];
                                                    dst[2] = src[2];
                                                    dst[3] = src[3];
                                                }

                                                src += 4;
                                                dst += 4;
                                            }
                                        }
                                    }

                                    converter->Release();
                                }
                            }

                            wicFrame->Release();
                        }

                        std::memcpy (seq.pixels.get() + static_cast<size_t> (i) * frameBytes,
                                     canvas.get(),
                                     frameBytes);

                        seq.delays[i] = fi.delayMs;
                    }

                    seq.width      = canvasW;
                    seq.height     = canvasH;
                    seq.frameCount = frameCount;

                    decoder->Release();
                }

                stream->Release();
            }

            factory->Release();
        }
    }

    return seq;
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal

#elif !(JUCE_MAC || JUCE_IOS)

// Linux / other: no native image codec support.
namespace Terminal
{ /*____________________________________________________________________________*/

juce::Image loadImageNative (const void*, size_t) noexcept
{
    return {};
}

ImageSequence loadImageSequenceNative (const void*, size_t) noexcept
{
    return {};
}

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal

#endif
