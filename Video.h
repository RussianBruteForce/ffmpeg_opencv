#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <functional>

class Video
{
      public:
	Video(void *data_ptr, size_t data_size);
	~Video();

	void process(std::function<void(unsigned char *, int, int, int)> f_);

      private:
	static constexpr AVPixelFormat output_pix_format{AV_PIX_FMT_GRAY8};

	struct {
		uint8_t *ptr;
		size_t size;
	} bd;

	AVCodecContext *video_ctx;
	AVStream *video_stream;

	size_t width;
	size_t heigh;

	AVPixelFormat input_pix_format;

	size_t avio_ctx_buffer_size = 32 * 1024; // 32 KiB
	uint8_t *avio_ctx_buffer;

	AVFormatContext *ctx;
	AVIOContext *avio_ctx;

	uint8_t *frame_converted_buffer;
	AVFrame *frame_converted;

	AVFrame *frame;
	AVPacket pkt;

	void init_stream();
	void init_codec();
	void init_frame_converted();
};
