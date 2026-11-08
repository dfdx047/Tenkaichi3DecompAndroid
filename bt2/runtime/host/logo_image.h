// The game's logo on the launcher's first screen (docs/plans/launcher/LAUNCHER_PLAN.md, Style): a PNG
// with transparency in the assets folder next to the programs (assets/logo.png, copied there by the
// build from assets/launcher/logo.png). Read with the Windows imaging component and scaled with its
// high-quality filter to the size a screen needs; without the file the screen has no logo.
#pragma once
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wincodec.h>
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <vector>
#pragma comment(lib,"windowscodecs.lib")

struct LogoImage {
    unsigned width=0,height=0;
    std::vector<std::uint32_t> bgra; // Premultiplied alpha.
    bool empty() const {return bgra.empty();}

    // The image in `path` scaled to fit `max_width` x `max_height` (keeping its shape); empty when
    // the file is missing or cannot be read.
    static LogoImage load(const std::filesystem::path& path,unsigned max_width,unsigned max_height) {
        LogoImage image;
        std::error_code error;
        if(!max_width || !max_height || !std::filesystem::is_regular_file(path,error))return image;
        const HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
        IWICImagingFactory* factory=nullptr;IWICBitmapDecoder* decoder=nullptr;IWICBitmapFrameDecode* frame=nullptr;
        IWICBitmapScaler* scaler=nullptr;IWICFormatConverter* converter=nullptr;
        UINT w=0,h=0;
        if(SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)))
           && SUCCEEDED(factory->CreateDecoderFromFilename(path.wstring().c_str(),nullptr,GENERIC_READ,WICDecodeMetadataCacheOnDemand,&decoder))
           && SUCCEEDED(decoder->GetFrame(0,&frame)) && SUCCEEDED(frame->GetSize(&w,&h)) && w && h) {
            const double scale=std::min(double(max_width)/w,double(max_height)/h);
            const UINT sw=std::max(1u,UINT(w*scale+0.5)),sh=std::max(1u,UINT(h*scale+0.5));
            if(SUCCEEDED(factory->CreateBitmapScaler(&scaler)) && SUCCEEDED(scaler->Initialize(frame,sw,sh,WICBitmapInterpolationModeHighQualityCubic))
               && SUCCEEDED(factory->CreateFormatConverter(&converter))
               && SUCCEEDED(converter->Initialize(scaler,GUID_WICPixelFormat32bppPBGRA,WICBitmapDitherTypeNone,nullptr,0.0,WICBitmapPaletteTypeCustom))) {
                image.bgra.resize(std::size_t(sw)*sh);
                if(SUCCEEDED(converter->CopyPixels(nullptr,sw*4,UINT(image.bgra.size()*4),reinterpret_cast<BYTE*>(image.bgra.data())))) {image.width=sw;image.height=sh;}
                else image.bgra.clear();
            }
        }
        for(IUnknown* object:{static_cast<IUnknown*>(converter),static_cast<IUnknown*>(scaler),static_cast<IUnknown*>(frame),static_cast<IUnknown*>(decoder),static_cast<IUnknown*>(factory)})
            if(object)object->Release();
        if(SUCCEEDED(com))CoUninitialize();
        return image;
    }
};
#endif
