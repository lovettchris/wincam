#include "pch.h"
#include "FFmpegEncoder.h"
#include "Timer.h"
#include "FpsThrottle.h"
#include "d3d11.h"
extern "C" {
#include "libavdevice/avdevice.h"
#include "libavcodec/avcodec.h"
#include "libavcodec/codec.h"
#include "libswscale/swscale.h"
#include "libavutil/imgutils.h"
#include "libavutil/mem.h"
}
#undef min
#undef max

int custom_write_buffer(void* opaque, const uint8_t* buf, int buf_size) {
    winrt::Windows::Storage::Streams::IRandomAccessStream* stream = (winrt::Windows::Storage::Streams::IRandomAccessStream*)opaque;
    auto buffer = winrt::Windows::Storage::Streams::Buffer(buf_size);
    ::memcpy(buffer.data(), buf, buf_size);
    stream->WriteAsync(buffer).get();
    return buf_size;
}

std::string GetErrorString(int hr) {
    switch (hr) {
        case AVERROR_BSF_NOT_FOUND       :///< Bitstream filter not found
            return "Bitstream filter not found";
        case AVERROR_BUG                 :///< Internal bug, also see AVERROR_BUG2
            return "Internal bug";
        case AVERROR_BUFFER_TOO_SMALL    :///< Buffer too small
            return "Buffer too small";
        case AVERROR_DECODER_NOT_FOUND   :///< Decoder not found
            return "Decoder not found";
        case AVERROR_DEMUXER_NOT_FOUND   :///< Demuxer not found
            return "Demuxer not found";
        case AVERROR_ENCODER_NOT_FOUND   :///< Encoder not found
            return "Encoder not found";
        case AVERROR_EOF                 :///< End of file
            return "End of file";
        case AVERROR_EXIT                :///< Immediate exit was requested; the called function should not be restarted
            return "Immediate exit was requested; the called function should not be restarted";
        case AVERROR_EXTERNAL            :///< Generic error in an external library
            return "Generic error in an external library";
        case AVERROR_FILTER_NOT_FOUND    :///< Filter not found
            return "Filter not found";
        case AVERROR_INVALIDDATA         :///< Invalid data found when processing input
            return "Invalid data found when processing input";
        case AVERROR_MUXER_NOT_FOUND     :///< Muxer not found
            return "Muxer not found";
        case AVERROR_OPTION_NOT_FOUND    :///< Option not found
            return "Option not found";
        case AVERROR_PATCHWELCOME        :///< Not yet implemented in FFmpeg, patches welcome
            return "Not yet implemented in FFmpeg, patches welcome";
        case AVERROR_PROTOCOL_NOT_FOUND  :///< Protocol not found
            return "Protocol not found";
        case AVERROR_STREAM_NOT_FOUND    :///< Stream not found
            return "Stream not found";
        case AVERROR_BUG2                :
            return "Internal bug 2";
        case AVERROR_UNKNOWN             :///< Unknown error, typically from an external library
            return "Unknown error, typically from an external library";
        case AVERROR_EXPERIMENTAL        :///< Requested feature is flagged experimental. Set strict_std_compliance if you really want to use it.
            return "Requested feature is flagged experimental. Set strict_std_compliance if you really want to use it";
        case AVERROR_INPUT_CHANGED       :///< Input changed between calls. Reconfiguration is required. (can be OR-ed with AVERROR_OUTPUT_CHANGED)
            return "Input changed between calls. Reconfiguration is required";
        case AVERROR_OUTPUT_CHANGED      :///< Output changed between calls. Reconfiguration is required. (can be OR-ed with AVERROR_INPUT_CHANGED)
            return "Output changed between calls. Reconfiguration is required";
        case AVERROR_HTTP_BAD_REQUEST    :
            return "HTTP bad request";
        case AVERROR_HTTP_UNAUTHORIZED   :
            return "HTTP unauthorized";
        case AVERROR_HTTP_FORBIDDEN      :
            return "HTTP forbidden";
        case AVERROR_HTTP_NOT_FOUND      :
            return "HTTP not found";
        case AVERROR_HTTP_TOO_MANY_REQUESTS:
            return "HTTP too many requests";
        case AVERROR_HTTP_OTHER_4XX:
            return "HTTP other 4xx error";
        case AVERROR_HTTP_SERVER_ERROR:
            return "HTTP server error";
    }
    return "Unknown error";
}

class FFmpegEncoderImpl : public VideoEncoderImpl
{
    std::vector<double> _ticks;
    double _maxDuration = 0; // seconds
    bool _running = false;
    util::Timer _sampleTimer;
    std::string _errorString;

public:
    FFmpegEncoderImpl() {
        avdevice_register_all();
    }

    ~FFmpegEncoderImpl() {

    }

    winrt::Windows::Foundation::IAsyncOperation<int> EncodeAsync(
        std::shared_ptr<SimpleCapture> capture,
        VideoEncoderProperties* properties,
        winrt::Windows::Storage::Streams::IRandomAccessStream stream) override
    {
        if (!capture->WaitForNextFrame(10000)) {
            _running = false;
            throw std::exception("frames are not arriving");
        }
        auto bounds = capture->GetTextureBounds();
        auto width = bounds.right - bounds.left;
        auto height = bounds.bottom - bounds.top;
        auto bitrateInBps = properties->bitrateInBps;
        auto frameRate = properties->frameRate;
        auto _maxDuration = properties->seconds;
        
        AVFormatContext* formatContext = avformat_alloc_context();
        const AVOutputFormat* outputFormat = av_guess_format(NULL, "output.mp4", NULL);
        formatContext->oformat = outputFormat;

        int io_buffer_size = 65536;
        uint8_t* io_buffer = (uint8_t*)av_malloc(io_buffer_size);
        AVIOContext* avioContext = avio_alloc_context(
            io_buffer, io_buffer_size, 1, (void*)&stream, nullptr, custom_write_buffer, NULL);

        // Find the "libx264" encoder
        const AVCodec* codec_test = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!codec_test) {
            fprintf(stderr, "Codec 'libx264' not found\n");            
        }

        const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        AVCodecContext* codecContext = avcodec_alloc_context3(codec);
        AVRational time_base = { 1, (int)frameRate };
        AVRational av_framerate = { (int)frameRate, 1 };
        codecContext->bit_rate = bitrateInBps;
        codecContext->width = width;
        codecContext->height = height;
        codecContext->time_base = time_base;
        codecContext->framerate = av_framerate;
        codecContext->gop_size = 10;
        codecContext->max_b_frames = 1;
        codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
        codecContext->sw_pix_fmt = AV_PIX_FMT_YUV420P;
        
        int error = 0;

        int hr = avcodec_open2(codecContext, codec, NULL);
        if (hr < 0) {
            _errorString = GetErrorString(hr);
            error = 3;
        }

        // setup converter for frame format from DXGI_FORMAT_B8G8R8A8_UNORM to YUV420P
        SwsContext* swsCtx = sws_getContext(width, height,
            AV_PIX_FMT_BGRA, codecContext->width,
            height, AV_PIX_FMT_YUV420P,
            SWS_BILINEAR, NULL, NULL, NULL);

        // Allocate destination frame
        AVFrame* dstFrame = av_frame_alloc();
        dstFrame->format = AV_PIX_FMT_YUV420P;
        dstFrame->width = width;
        dstFrame->height = height;
        av_image_alloc(dstFrame->data, dstFrame->linesize, width, height, AV_PIX_FMT_YUV420P, 1);

        // Create AVFrame to hold our AV_PIX_FMT_BGRA data.
        AVFrame* frame = av_frame_alloc();
        frame->format = AV_PIX_FMT_BGRA;
        frame->width = width;
        frame->height = height;
        av_frame_get_buffer(frame, 32); // should we 64 bit align them to match directX ?

        util::Timer timer;
        timer.Start();
        util::FpsThrottle throttle(frameRate);
        _running = true;
        uint8_t* buffer = nullptr;
        unsigned int buffer_size = 0;
        int rowPitch = 0;
        AVPacket* packet = av_packet_alloc();

        while (_running && error == 0)
        {
            throttle.Step(); // give it time to capture a frame.
            winrt::com_ptr<ID3D11Texture2D> texture;
            double frame_time = capture->ReadNextTexture(10, texture);

            if (frame_time == 0) {
                _running = false;
                _errorString = "ReadNextTexture failed";
                error = 1;
                break;
            }
            if (timer.Seconds() > _maxDuration) {
                break;
            }

            _ticks.push_back(frame_time);
            if (buffer == nullptr) {
                auto rect = capture->GetCaptureBounds();
                rowPitch = (rect.right - rect.left) * 4;
                buffer_size = (rect.right - rect.left) * (rect.bottom - rect.top) * 4;
                buffer = new uint8_t[buffer_size];
            }

            try {
                capture->ReadPixels(texture.get(), (char*)buffer, buffer_size);
            }
            catch (...) {
                _running = false;
                _errorString = "ReadPixels failed";
                error = 2;
                break;
            }

            // Copy texture data to frame (gross, can this be a block copy?)
            for (int y = 0; y < codecContext->height; y++) {
                memcpy(frame->data[0] + y * frame->linesize[0],
                    buffer + y * rowPitch,
                    width * 4);
            }

            // Convert frame to YUV420P
            sws_scale(swsCtx, frame->data, frame->linesize, 0, codecContext->height,
                frame->data, frame->linesize);

            // Send the frame to the encoder and receive the encoded packets.
            avcodec_send_frame(codecContext, frame);

            while (avcodec_receive_packet(codecContext, packet) == 0) {
                av_interleaved_write_frame(formatContext, packet);
                av_packet_unref(packet);
            }
        }

        // finish up the video format.
        if (error == 0) {
            av_write_trailer(formatContext);
        }

        // cleanup
        av_packet_free(&packet);
        avformat_free_context(formatContext);
        avcodec_free_context(&codecContext);
        avio_context_free(&avioContext);
        av_free(io_buffer);

        av_frame_free(&frame);
        av_freep(&dstFrame->data[0]);
        av_frame_free(&dstFrame);

        co_return error;
    }

    const char* GetErrorMessage(int hr) override
    {
        return _errorString.c_str();
    }

    void Stop() override
    {
        _running = false;
    }

    bool IsRunning() override {
        return _running;
    }

    unsigned int GetSampleTimes(double* buffer, unsigned int size) override
    {
        auto available = (unsigned int)_ticks.size();
        if (buffer != nullptr && available > 0) {
            auto count = std::min(available, size);
            ::memcpy(buffer, _ticks.data(), count * sizeof(double));
        }
        return available;
    }

};

std::unique_ptr<VideoEncoderImpl> CreateFFmpegEncoder()
{
    return std::make_unique<FFmpegEncoderImpl>();
}