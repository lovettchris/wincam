#include "pch.h"
#include "FFmpegEncoder.h"
#include "Timer.h"
#include "FpsThrottle.h"
#include "UnicodeFile.h"
#include <sstream>
#include <iomanip>
#define D3D11_NO_HELPERS
#include <d3d11.h>
extern "C" {
#include <libavdevice/avdevice.h>
#include <libavcodec/avcodec.h>
#include <libavcodec/codec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libavutil/avutil.h>
#include <libavutil/hwcontext.h>
#include <libavutil/hwcontext_d3d11va.h>
}
#undef min
#undef max

int custom_write_buffer(void* opaque, const uint8_t* buf, int buf_size) {
    auto file = static_cast<UnicodeFile*>(opaque);
    file->WriteBytes(buf, buf_size);
    return buf_size;
}

int64_t custom_seek_buffer(void* opaque, int64_t offset, int whence) {
    auto file = static_cast<UnicodeFile*>(opaque);
    int origin = whence & (~AVSEEK_FORCE);
    return file->Seek(offset, origin);
}

void check_ffmpeg_error(int hr, const char* msg) {
    if (hr < 0) {
        const int MAX_ERROR_STRING = 8192;
        std::string message;
        message.reserve(MAX_ERROR_STRING);
        av_make_error_string(&message[0], MAX_ERROR_STRING, hr);
        std::string combined = msg;
        combined.append(message);
        throw std::exception(combined.c_str());
    }
}

void check_windows_error(int hr, const char* msg) {
    if (hr < 0) {
        LPSTR messageBuffer = nullptr;

        //The parameters we pass in, tell Win32 to create the buffer that holds the message for us (because we don't yet know how long the message string will be).
        size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, hr, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        // Copy the error message into a std::string.
        std::string message = msg;
        message += std::string(messageBuffer, size);

        //Free the Win32's string's buffer.
        LocalFree(messageBuffer);
        throw std::exception(message.c_str());
    }
}

class FFmpegEncoderImpl : public VideoEncoderImpl
{
    std::vector<double> _ticks;
    bool _running = false;
    std::string _errorString;

public:
    FFmpegEncoderImpl() {
        avdevice_register_all();
    }

    ~FFmpegEncoderImpl() {

    }

    winrt::Windows::Foundation::IAsyncOperation<int> EncodeAsync(
        std::shared_ptr<ScreenCapture> capture,
        VideoEncoderProperties* properties,
        std::wstring filePath) override
    {
        int error = 0;
        AVFormatContext* formatContext = nullptr;
        AVIOContext* avioContext = nullptr;
        const AVCodec* codec = nullptr;
        AVCodecContext* codecContext = nullptr;
        AVPacket* packet = nullptr;
        uint8_t* io_buffer = nullptr;
        SwsContext* swsCtx = nullptr;
        AVFrame* dstFrame = nullptr;
        AVFrame* frame = nullptr;
        AVStream* out_stream = nullptr;
        bool debug_file_io = false;
        this->_ticks.clear();

        try {
            if (!capture->WaitForNextFrame(10000)) {
                throw std::exception("frames are not arriving");
            }
            auto bounds = capture->GetTextureBounds();
            auto width = bounds.right - bounds.left;
            auto height = bounds.bottom - bounds.top;
            auto bitrateInBps = properties->bitrateInBps;
            auto frameRate = properties->frameRate;
            auto maxDuration = properties->seconds;
            if (bitrateInBps == 0) {
                bitrateInBps = GetBestBitRate(frameRate, properties->quality);
            }
            /* resolution must be a multiple of two */
            if (width % 2 == 1) {
                width--;
            }
            if (height % 2 == 1) {
                height--;
            }
            if (width == 0 || height == 0) {
                throw std::exception("Resolution too small");
            }

            // Open the file
            UnicodeFile file;
            int hr = file.OpenFile(filePath);
            check_windows_error(hr, "OpenFile: ");

            // Find the "libx264" encoder
            codec = avcodec_find_encoder(AV_CODEC_ID_H264);
            if (!codec) {
                throw std::exception("H264 codec not found");
            }

            hr = avformat_alloc_output_context2(&formatContext, nullptr, nullptr, "output.mp4");
            check_ffmpeg_error(hr, "avformat_alloc_output_context2: ");

            out_stream = avformat_new_stream(formatContext, codec);

            if (debug_file_io)
            {
                hr = avio_open(&formatContext->pb, "video.mp4", AVIO_FLAG_WRITE);
                check_ffmpeg_error(hr, "avio_open: ");
            }
            else {
                int io_buffer_size = 65536;
                io_buffer = (uint8_t*)av_malloc(io_buffer_size);
                avioContext = avio_alloc_context(
                    io_buffer, io_buffer_size, 1, (void*)&file, nullptr, custom_write_buffer, custom_seek_buffer);
                formatContext->pb = avioContext; // hook up our custom IO context.
            }

            codecContext = avcodec_alloc_context3(codec);        
            AVRational time_base = { 1, (int)frameRate * 1000}; // in milliseconds.
            AVRational av_framerate = { (int)frameRate, 1 };
            codecContext->bit_rate = bitrateInBps;        
            codecContext->width = width;
            codecContext->height = height;
            codecContext->time_base = time_base;
            codecContext->pkt_timebase = time_base;
            codecContext->framerate = av_framerate;
            codecContext->gop_size = 10; // emit one intra frame every 10 frames at most.
            codecContext->max_b_frames = 1;
            codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
            codecContext->qmin = 3;
            out_stream->time_base = codecContext->time_base;
            out_stream->avg_frame_rate = av_framerate;
            out_stream->r_frame_rate = av_framerate;
            if (formatContext->oformat->flags & AVFMT_GLOBALHEADER)
            {
                codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
            }
            // duration comes out at 1000, or 1 millisecond which means 1000/30000 seconds or 1/30th of a second if we are running at 30 fps
            // The reason we set time_base to 1/30000 instead of 1/30 is to give the system more floating point precision.
            int64_t avp_duration = (codecContext->time_base.den / codecContext->time_base.num) / av_framerate.num * av_framerate.den;

            if (!formatContext->nb_streams)
            {
                throw std::exception("Output file dose not contain any stream");
            }

            if (codec->id == AV_CODEC_ID_H264) {
                av_opt_set(codecContext->priv_data, "preset", "fast", 0);
                av_opt_set(codecContext->priv_data, "crf", "20", 0);
            }
            hr = avcodec_open2(codecContext, codec, NULL);
            check_ffmpeg_error(hr, "avcodec_open2: ");

            hr = avcodec_parameters_from_context(out_stream->codecpar, codecContext);
            check_ffmpeg_error(hr, "avcodec_parameters_from_context: ");

            hr = avformat_write_header(formatContext, nullptr);
            check_ffmpeg_error(hr, "avformat_write_header: ");

            // setup converter for frame format from DXGI_FORMAT_B8G8R8A8_UNORM to YUV420P
            swsCtx = sws_getContext(width, height,
                AV_PIX_FMT_BGRA, codecContext->width,
                height, AV_PIX_FMT_YUV420P,
                SWS_BILINEAR, NULL, NULL, NULL);

            // Allocate destination frame
            dstFrame = av_frame_alloc();
            dstFrame->format = AV_PIX_FMT_YUV420P;
            dstFrame->width = width;
            dstFrame->height = height;
            av_image_alloc(dstFrame->data, dstFrame->linesize, width, height, AV_PIX_FMT_YUV420P, 1);

            // Create AVFrame to hold our AV_PIX_FMT_BGRA data.
            frame = av_frame_alloc();
            frame->format = AV_PIX_FMT_BGRA;
            frame->width = width;
            frame->height = height;
            av_frame_get_buffer(frame, 32); // should we 64 bit align them to match directX ?

            util::Timer timer;
            uint64_t frameCount = 0;
            util::FpsThrottle throttle(frameRate);
            _running = true;
            packet = av_packet_alloc();

            auto rect = capture->GetCaptureBounds();
            int rowPitch = (rect.right - rect.left) * 4;
            unsigned int buffer_size = (rect.right - rect.left) * (rect.bottom - rect.top) * 4;
            uint8_t* buffer = new uint8_t[buffer_size];
            double first_time = -1;
            double frame_time = 0;
            int64_t pts = 0;
            timer.Start();

            while (_running && error == 0)
            {
                throttle.Step(); // give it time to capture a frame.
                winrt::com_ptr<ID3D11Texture2D> texture;
                frame_time = capture->ReadNextTexture(10000, texture);
                if (frame_time < 0 || !texture) {
                    throw std::exception("ReadNextTexture failed");
                }
                if (first_time == -1) {
                    first_time = frame_time;
                    // we cannot use the time returned from window because it generates this error:
                    // Application provided invalid, non monotonically increasing dts to muxer in stream.
					// Instead we ensure a monotonically increasing time by using our own timer.
                    timer.Start();
                }
                frame_time = timer.Seconds();

                if (maxDuration > 0 && frame_time > maxDuration) {
                    break;
                }
                frameCount++;

                _ticks.push_back(frame_time);

                try {
                    capture->ReadPixels(texture.get(), (char*)buffer, buffer_size);
                }
                catch (...) {
                    throw std::exception("ReadPixels failed");
                }

                /* Make sure the frame data is writable.
                   On the first round, the frame is fresh from av_frame_get_buffer()
                   and therefore we know it is writable.
                   But on the next rounds, encode() will have called
                   avcodec_send_frame(), and the codec may have kept a reference to
                   the frame in its internal structures, that makes the frame
                   unwritable.
                   av_frame_make_writable() checks that and allocates a new buffer
                   for the frame only if necessary.
                 */
                hr = av_frame_make_writable(dstFrame);
                check_ffmpeg_error(hr, "av_frame_make_writable: ");

                // Copy texture data to frame (needed to handle even width/height difference)
                for (int y = 0; y < codecContext->height; y++) {
                    memcpy(frame->data[0] + y * frame->linesize[0],
                        buffer + y * rowPitch,
                        width * 4);
                }

                // Convert frame to YUV420P
                sws_scale(swsCtx, frame->data, frame->linesize, 0, codecContext->height,
                    dstFrame->data, dstFrame->linesize);

                // Sync presentation time to real time frame times we get from windows!
                dstFrame->pts = static_cast<int64_t>(frame_time * 1000 * frameRate); // in time_base units.
                dstFrame->duration = avp_duration;

                // Send the frame to the encoder and receive the encoded packets.
                hr = avcodec_send_frame(codecContext, dstFrame);
                check_ffmpeg_error(hr, "avcodec_send_frame: ");

                while (error == 0) {
                    hr = avcodec_receive_packet(codecContext, packet);
                    if (hr == AVERROR(EAGAIN) || hr == AVERROR_EOF)
                    {
                        break;
                    }
                    else if (hr == 0) {
                        packet->stream_index = 0;                        
                        hr = av_interleaved_write_frame(formatContext, packet);
                    }
                    av_packet_unref(packet);
                    check_ffmpeg_error(hr, "av_interleaved_write_frame: ");
                }
            }

            // finish up the video format.
            if (error == 0) {
                hr = av_write_trailer(formatContext);
                check_ffmpeg_error(hr, "av_write_trailer: ");
            }

            DebugFrameRate(timer, frameCount, frame_time);
        }
        catch (const std::exception& e)
        {
            _errorString = e.what();
            _running = false;
            error = 3;
        }

        // cleanup
        if (packet) {
            av_packet_free(&packet);
        }
        if (formatContext) {
            avformat_free_context(formatContext);
        }
        if (codecContext) {
            avcodec_free_context(&codecContext);
        }
        if (avioContext) {
            avio_context_free(&avioContext);
        }
        if (io_buffer) {
            av_free(io_buffer);
        }
        if (swsCtx) {
            sws_freeContext(swsCtx);
        }
        if (frame) {
            av_frame_free(&frame);
        }
        if (dstFrame) {
            av_frame_free(&dstFrame);
        }

        co_return error;
    }

    void DebugFrameRate(util::Timer& timer, uint64_t frameCount, double duration)
    {
        double seconds = timer.Seconds();
        double rate = frameCount / seconds;
        std::wostringstream wostringstream;
        wostringstream << L"written " << frameCount << L" frames in " << seconds << L" seconds which is " << rate << " fps and duration " << duration << " seconds.";
        std::wstring wideMessage = wostringstream.str();
        LPCTSTR wideChars = wideMessage.c_str();
        OutputDebugString(wideChars);
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