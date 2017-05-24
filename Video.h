#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include <functional>
#include <memory>

class Video
{
	using byte = uint8_t;
	class mem_ctx {
	public:
		mem_ctx() = delete;
		mem_ctx(const Video::byte* data, size_t size);
//		~mem_ctx();

		static int read(void *opaque, uint8_t *buf, int size);
		static int64_t seek(void *opaque, int64_t pos, int whence);

	private:
		const size_t data_size{0};
		std::unique_ptr<FILE, decltype(&std::fclose)> f;
	};
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

	bool video_ctx_opened{false};
	AVCodecContext *video_ctx{nullptr};
	AVStream *video_stream{nullptr};
	int video_stream_idx{-1};

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

	int decode_packet(int *got_frame, int cached, int& video_frame_count);

	std::unique_ptr<mem_ctx> data;
};
