#include "../ffmpeg/ffmpeg_manager.cc";

int main() {
    FFMPEGManager* fm = new FFMPEGManager();
    fm->Init("SampleVideo_1280x720_1mb.mp4", AV_PIX_FMT_RGB24, 1280, 720);
    fm->Loop();
    return 0;
}