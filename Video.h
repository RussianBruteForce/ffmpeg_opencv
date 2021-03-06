
#pragma once
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#pragma clang diagnostic ignored "-Wweak-vtables"
#pragma clang diagnostic ignored "-Wpadded"

extern "C" {
#include <libavformat/avformat.h>
}

#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

/*
 * Define, if you use av_malloc allocated buffer.
 * It will be free'd automatically.
 */
//#define VIDEO_AVBUFFER

/*
 * Define, if you want to customize TAG in runtime
 */
//#define VIDEO_CUSTOM_TAG

/*
 * Define, if you want to dump format to stdout
 */
//#define VIDEO_DUMP_FORMAT

class Video
{
      public:
	using byte = uint8_t;
	using string = std::string;

      private:
#ifndef VIDEO_AVBUFFER
	class mem_ctx
	{
	      public:
		mem_ctx() = delete;
		mem_ctx(const Video::byte *data_, size_t size);

		static int read(void *opaque, uint8_t *buf, int size);
		static int64_t seek(void *opaque, int64_t pos, int whence);

	      private:
		const size_t data_size{0};
		std::unique_ptr<FILE, decltype(&std::fclose)> f;
	};
#endif

      public:
#ifdef VIDEO_CUSTOM_TAG
	static string TAG;
#else
	static constexpr auto TAG{"Video"};
#endif

	class VideoError final : public std::runtime_error
	{
	      public:
		explicit VideoError(const string &what_arg)
		    : std::runtime_error(what_arg)
		{
		}
		explicit VideoError(const char *what_arg)
		    : std::runtime_error(what_arg)
		{
		}
	};

	Video();
	~Video();
	Video(void *data_, size_t size_);
	void set(void *data_, size_t size);
	void process(std::function<void(unsigned char *, int, int, int)> f_);

      private:
	static constexpr auto refcount{false};

	static constexpr AVPixelFormat output_pix_format{AV_PIX_FMT_GRAY8};

	byte *data_ptr{nullptr};
	size_t data_size{0};

#ifndef VIDEO_AVBUFFER
	static constexpr auto BUFFER_SIZE{16 * 1024};
	std::unique_ptr<mem_ctx> data;
	byte *buffer;
#endif

	template <class T>
	using ptr_ = std::unique_ptr<T, decltype(&::av_free)>;

	template <class T> ptr_<T> mk_ptr_(T *ptr)
	{
		return ptr_<T>{ptr, &::av_free};
	}

	ptr_<AVFormatContext> fmt_ctx;
	ptr_<AVFrame> frame;
	ptr_<AVFrame> frame_converted{nullptr, &::av_free};

	ptr_<AVIOContext> avio_ctx{nullptr, &::av_free};

	ptr_<AVPacket> pkt;

	int video_stream_idx{-1};
	ptr_<AVCodecContext> dec_ctx{nullptr, &::av_free};

	void frame_converted_alloc();
};

#pragma clang diagnostic pop
