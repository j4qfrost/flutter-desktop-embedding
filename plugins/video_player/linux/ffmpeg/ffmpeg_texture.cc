#ifndef FFMPEG_TEXTURE
#define FFMPEG_TEXTURE

#include <flutter/texture_registrar.h>

class FFMPEGTexture : public Texture 
{
private:
    /* data */
public:
    FFMPEGTexture(/* args */);
    virtual ~FFMPEGTexture();

    virtual const PixelBuffer* CopyPixelBuffer(size_t width, size_t height);
};

FFMPEGTexture::FFMPEGTexture(/* args */)
{
}

FFMPEGTexture::~FFMPEGTexture()
{
}

const PixelBuffer* FFMPEGTexture::CopyPixelBuffer(size_t width, size_t height) {
    PixelBuffer pb;
    
    return &pb;
}

#endif