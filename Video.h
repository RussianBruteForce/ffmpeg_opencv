#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <functional>

class Video
{
      public:
	static std::string TAG;

	Video();
	Video(void *data_ptr, size_t data_size);
	~Video();

	void set(void *data_ptr, size_t data_size);
	void process(std::function<void(unsigned char *, int, int, int)> f_);

      private:
	static constexpr AVPixelFormat output_pix_format{AV_PIX_FMT_GRAY8};

	struct {
		uint8_t *ptr{nullptr};
		size_t size;
	} bd;

	AVCodecContext *video_ctx;
	AVStream *video_stream;

	size_t width;
	size_t heigh;

	AVPixelFormat input_pix_format;

	size_t avio_ctx_buffer_size = 32 * 1024; // 32 KiB
	uint8_t *avio_ctx_buffer{nullptr};

	AVFormatContext *ctx{nullptr};
	AVIOContext *avio_ctx{nullptr};

	uint8_t *frame_converted_buffer{nullptr};
	AVFrame *frame_converted{nullptr};

	AVFrame *frame{nullptr};
	AVPacket *pkt{nullptr};

	void init_stream();
	void init_codec();
	void init_frame_converted();
};
