#ifndef FFMPEG_TEXTURE
#define FFMPEG_TEXTURE

#include <flutter/texture_registrar.h>
#include "ffmpeg_manager.cc"

class FFMPEGTexture : public Texture 
{
private:
    FFMPEGManager* manager;
public:
    FFMPEGTexture(FFMPEGManager* man);
    virtual ~FFMPEGTexture();

    virtual const PixelBuffer* CopyPixelBuffer(size_t width, size_t height);
};

FFMPEGTexture::FFMPEGTexture(FFMPEGManager* man)
{
    manager = man;
}

FFMPEGTexture::~FFMPEGTexture()
{
    delete manager;
    manager = NULL;
}

const PixelBuffer* FFMPEGTexture::CopyPixelBuffer(size_t width, size_t height) {
    PixelBuffer pb;
    manager->Data(pb.buffer);
    pb.width = manager->Width();
    pb.height = manager->Height();
    return &pb;
}

#endif