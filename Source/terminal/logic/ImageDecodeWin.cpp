/**
 * @file ImageDecodeWin.cpp
 * @brief Windows native image decoding via WIC, with Linux fallback stub.
 *
 * @see ImageDecode.h
 */

#include "ImageDecode.h"

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

/**______________________________END OF NAMESPACE______________________________*/
} // namespace Terminal

#endif
