#ifndef FFMPEG_MANAGER
#define FFMPEG_MANAGER

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
}

class FFMPEGManager
{
private:
    AVFormatContext *fmt_ctx;
    AVCodecContext *dec_ctx;
    AVFilterContext *buffersink_ctx;
    AVFilterContext *buffersrc_ctx;
    AVFilterGraph *filter_graph;

    AVFrame *frame;
    AVFrame *filt_frame;

    int video_stream_index;
    int64_t last_pts;

    int init_fmt_context(const char *filename);
    int init_dec_context(AVPixelFormat pix_fmt);
    int open_input_file(const char *filename, AVPixelFormat pix_fmt);
    int init_filters(const char *filters_descr);

    int read_frame_to_packet(AVPacket* packet);
    int receive_frame();
    int get_filter_frame();
    int loop_internal();
public:
    FFMPEGManager();
    ~FFMPEGManager();

    int Init(const char* filename, AVPixelFormat pix_fmt, int width, int height);
    void Free();
    int Close(int ret);

    int Loop();
};

FFMPEGManager::FFMPEGManager()
{
    fmt_ctx = NULL;
    dec_ctx = NULL;
    buffersink_ctx = NULL;
    buffersrc_ctx = NULL;
    filter_graph = NULL;

    frame = NULL;
    filt_frame = NULL;

    video_stream_index = -1;
    last_pts = AV_NOPTS_VALUE;
}

FFMPEGManager::~FFMPEGManager()
{
}

int FFMPEGManager::init_fmt_context(const char *filename) {
    int ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open input file\n");
        return ret;
    }

    if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
        return ret;
    }

    return 0;
}

int FFMPEGManager::init_dec_context(AVPixelFormat pix_fmt) {
    AVCodec *dec;
    /* select the video stream */
    int ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &dec, 0);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
        return ret;
    }
    video_stream_index = ret;

    /* create decoding context */
    dec_ctx = avcodec_alloc_context3(dec);
    if (!dec_ctx)
        return AVERROR(ENOMEM);
    avcodec_parameters_to_context(dec_ctx, fmt_ctx->streams[video_stream_index]->codecpar);

    /* init the video decoder */
    if ((ret = avcodec_open2(dec_ctx, dec, NULL)) < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot open video decoder\n");
        return ret;
    }

    dec_ctx->pix_fmt = pix_fmt;
    return 0;
}

int FFMPEGManager::open_input_file(const char *filename, AVPixelFormat pix_fmt) {
    int ret = init_fmt_context(filename);
    return (ret < 0)? ret:init_dec_context(pix_fmt);
}

int FFMPEGManager::init_filters(const char *filters_descr)
{
    char args[512];
    int ret = 0;
    const AVFilter *buffersrc  = avfilter_get_by_name("buffer");
    const AVFilter *buffersink = avfilter_get_by_name("buffersink");
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs  = avfilter_inout_alloc();
    AVRational time_base = fmt_ctx->streams[video_stream_index]->time_base;
    enum AVPixelFormat pix_fmts[] = { dec_ctx->pix_fmt, AV_PIX_FMT_NONE };

    filter_graph = avfilter_graph_alloc();
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        goto end;
    }

    /* buffer video source: the decoded frames from the decoder will be inserted here. */
    snprintf(args, sizeof(args),
            "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
            dec_ctx->width, dec_ctx->height, dec_ctx->pix_fmt,
            time_base.num, time_base.den,
            dec_ctx->sample_aspect_ratio.num, dec_ctx->sample_aspect_ratio.den);

    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer source\n");
        goto end;
    }

    /* buffer video sink: to terminate the filter chain. */
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot create buffer sink\n");
        goto end;
    }

    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        av_log(NULL, AV_LOG_ERROR, "Cannot set output pixel format\n");
        goto end;
    }

    /*
     * Set the endpoints for the filter graph. The filter_graph will
     * be linked to the graph described by filters_descr.
     */

    /*
     * The buffer source output must be connected to the input pad of
     * the first filter described by filters_descr; since the first
     * filter input label is not specified, it is set to "in" by
     * default.
     */
    outputs->name       = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx    = 0;
    outputs->next       = NULL;

    /*
     * The buffer sink input must be connected to the output pad of
     * the last filter described by filters_descr; since the last
     * filter output label is not specified, it is set to "out" by
     * default.
     */
    inputs->name       = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx    = 0;
    inputs->next       = NULL;

    if ((ret = avfilter_graph_parse_ptr(filter_graph, filters_descr,
                                    &inputs, &outputs, NULL)) < 0)
        goto end;

    if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
        goto end;

end:
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);

    return ret;
}

int FFMPEGManager::Init(const char* filename, AVPixelFormat pix_fmt, int width, int height) {
    int ret;

    frame = av_frame_alloc();
    filt_frame = av_frame_alloc();


    if ((ret = open_input_file(filename, pix_fmt)) >= 0) {
        char filter_descr[16];
        sprintf(filter_descr, "scale=%d:%d", width, height);
        ret = init_filters(filter_descr);
    }

    return Close(ret);
}

void FFMPEGManager::Free() {
    avfilter_graph_free(&filter_graph);
    if (&dec_ctx) {
        avcodec_free_context(&dec_ctx);
    }
    avformat_close_input(&fmt_ctx);
    if (&frame) {
        av_frame_free(&frame);
    }
    if (&filt_frame) {
        av_frame_free(&filt_frame);
    }
}

int FFMPEGManager::Close(int ret) {
    Free();
    if (ret < 0 && ret != AVERROR_EOF) {
        fprintf(stderr, "Error occurred: %s\n", av_err2str(ret));
        exit(1);
    }
    return 0;
}

int FFMPEGManager::read_frame_to_packet(AVPacket* packet) {
    av_packet_unref(packet);
    return av_read_frame(fmt_ctx, packet);
}

int FFMPEGManager::receive_frame() {
    av_frame_unref(frame);
    return avcodec_receive_frame(dec_ctx, frame);
}

int FFMPEGManager::get_filter_frame() {
    av_frame_unref(filt_frame);
    return av_buffersink_get_frame(buffersink_ctx, filt_frame);
}

int FFMPEGManager::loop_internal() {
    AVPacket packet;
    /* read all packets */
    int ret = av_read_frame(fmt_ctx, &packet);
    if (ret < 0) {
        av_packet_unref(&packet);
    }
    
    for(; ret >= 0; ret = read_frame_to_packet(&packet)) {
        if (packet.stream_index != video_stream_index) {
            continue;
        }

        ret = avcodec_send_packet(dec_ctx, &packet);
        if (ret < 0) {
            av_log(NULL, AV_LOG_ERROR, "Error while sending a packet to the decoder\n");
            break;
        }

        for(ret = avcodec_receive_frame(dec_ctx, frame); 
            ret >= 0 && !(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF); 
            ret = receive_frame()) {
            if (ret < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while receiving a frame from the decoder\n");
                return ret;
            }

            frame->pts = frame->best_effort_timestamp;

            /* push the decoded frame into the filtergraph */
            if (av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF) < 0) {
                av_log(NULL, AV_LOG_ERROR, "Error while feeding the filtergraph\n");
                break;
            }

            /* pull filtered frames from the filtergraph */
            for(ret = av_buffersink_get_frame(buffersink_ctx, filt_frame); 
                ret >= 0 && !(ret == AVERROR(EAGAIN) || ret == AVERROR_EOF); 
                ret = get_filter_frame()) {
                if (ret < 0)
                    return ret;
                //display_frame(filt_frame, buffersink_ctx->inputs[0]->time_base);
            }
        }
    }
    return 0;
}

int FFMPEGManager::Loop() {
    int ret = loop_internal();

    return Close(ret);
}

#endif

