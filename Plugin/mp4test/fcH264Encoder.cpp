﻿#include "openh264/codec_api.h"
#include "fcH264Encoder.h"


#define fcWindows

#ifdef fcWindows

    #include <windows.h>
    #if defined(_M_AMD64)
        #define OpenH264DLL "openh264-1.4.0-win64msvc.dll"
    #elif defined(_M_IX86)
        #define OpenH264DLL "openh264-1.4.0-win32msvc.dll"
    #endif

#else 
    #include <dlfcn.h>
#endif


typedef int  (*WelsCreateSVCEncoderT)(ISVCEncoder** ppEncoder);
typedef void (*WelsDestroySVCEncoderT)(ISVCEncoder* pEncoder);

WelsCreateSVCEncoderT g_WelsCreateSVCEncoder = nullptr;
WelsDestroySVCEncoderT g_WelsDestroySVCEncoder = nullptr;

#ifdef fcWindows
HMODULE g_h264_dll;

static void LoadOpenH264Module()
{
    if (g_h264_dll != nullptr) { return; }
    g_h264_dll = ::LoadLibraryA(OpenH264DLL);
    if (g_h264_dll != nullptr) {
        g_WelsCreateSVCEncoder = (WelsCreateSVCEncoderT)::GetProcAddress(g_h264_dll, "WelsCreateSVCEncoder");
        g_WelsDestroySVCEncoder = (WelsDestroySVCEncoderT)::GetProcAddress(g_h264_dll, "WelsDestroySVCEncoder");
    }
}

#else

void *g_h264_dll;

static void LoadOpenH264Module()
{
    if (g_h264_dll != nullptr) { return; }
    g_h264_dll = ::dlopen(OpenH264DLL, RTLD_GLOBAL);
    if (g_h264_dll != nullptr) {
        g_WelsCreateSVCEncoder = (WelsCreateSVCEncoderT)::dlsym(g_h264_dll, "WelsCreateSVCEncoder");
        g_WelsDestroySVCEncoder = (WelsDestroySVCEncoderT)::dlsym(g_h264_dll, "WelsDestroySVCEncoder");
    }
}

#endif // fcWindows


fcH264Encoder::fcH264Encoder(int width, int height, float frame_rate, int target_bitrate)
    : m_encoder(nullptr)
    , m_width(width)
    , m_height(height)
{
    LoadOpenH264Module();
    if (g_WelsCreateSVCEncoder) {
        g_WelsCreateSVCEncoder(&m_encoder);

        SEncParamBase param;
        memset(&param, 0, sizeof(SEncParamBase));
        param.iUsageType = CAMERA_VIDEO_REAL_TIME;
        param.fMaxFrameRate = frame_rate;
        param.iPicWidth = m_width;
        param.iPicHeight = m_height;
        param.iTargetBitrate = target_bitrate;
        param.iRCMode = RC_BITRATE_MODE;
        int ret = m_encoder->Initialize(&param);
    }
}

fcH264Encoder::~fcH264Encoder()
{
    if (g_WelsDestroySVCEncoder)
    {
        g_WelsDestroySVCEncoder(m_encoder);
    }
}

fcH264Encoder::operator bool() const
{
    return m_encoder != nullptr;
}

fcH264Encoder::result fcH264Encoder::encodeRGBA(const bRGBA *src)
{
    result r = {nullptr, 0};
    if (!m_encoder) { return r; }

    m_buf.resize(m_width * m_height * 3 / 2);
    uint8_t *pic_y = (uint8_t*)&m_buf[0];
    uint8_t *pic_u = pic_y + (m_width * m_height);
    uint8_t *pic_v = pic_u + ((m_width * m_height) >> 2);
    RGBA_to_I420(pic_y, pic_u, pic_v, src, m_width, m_height);
    return encodeI420(pic_y, pic_u, pic_v);
}

fcH264Encoder::result fcH264Encoder::encodeI420(const void *src_y, const void *src_u, const void *src_v)
{
    result r = { nullptr, 0 };
    if (!m_encoder) { return r; }

    SSourcePicture src;
    memset(&src, 0, sizeof(src));
    src.iPicWidth = m_width;
    src.iPicHeight = m_height;
    src.iColorFormat = videoFormatI420;
    src.pData[0] = (unsigned char*)src_y;
    src.pData[1] = (unsigned char*)src_u;
    src.pData[2] = (unsigned char*)src_v;
    src.iStride[0] = m_width;
    src.iStride[1] = m_width >> 1;
    src.iStride[2] = m_width >> 1;

    SFrameBSInfo dst;
    memset(&dst, 0, sizeof(dst));

    int ret = m_encoder->EncodeFrame(&src, &dst);
    if (ret == 0) {
        if (dst.eFrameType != videoFrameTypeSkip) {
            r.data = dst.sLayerInfo[0].pBsBuf;
            r.data_size = dst.iFrameSizeInBytes;
        }
    }
    return r;
}