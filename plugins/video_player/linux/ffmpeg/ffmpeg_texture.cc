#ifndef FFMPEG_TEXTURE
#define FFMPEG_TEXTURE

#include <flutter/texture_registrar.h>
#include "ffmpeg_manager.cc"

using flutter::Texture;

class FFMPEGTexture : public Texture 
{
private:
    FFMPEGManager* source;
public:
    FFMPEGTexture(FFMPEGManager* man);
    virtual ~FFMPEGTexture();

    virtual const PixelBuffer* CopyPixelBuffer(size_t width, size_t height);
};

FFMPEGTexture::FFMPEGTexture(FFMPEGManager* man)
{
    source = man;
}

FFMPEGTexture::~FFMPEGTexture()
{
    delete source;
    source = NULL;
}

const PixelBuffer* FFMPEGTexture::CopyPixelBuffer(size_t width, size_t height) {
    PixelBuffer* pb = new PixelBuffer();
    pb->width = source->Width();
    pb->height = source->Height();

    uint8_t buf[source->Width() * source->Height() * 4];
    source->Data(buf);
    pb->buffer = buf;
    return pb;
}

#endif