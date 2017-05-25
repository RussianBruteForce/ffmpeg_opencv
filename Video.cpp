#include "Video.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wc++98-compat-pedantic"
#pragma clang diagnostic ignored "-Wsign-conversion"
#pragma clang diagnostic ignored "-Wshorten-64-to-32"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

#include <array>
#include <cstdio>

namespace
{
static Video::string averr(int code)
{
	static thread_local std::array<char, AV_ERROR_MAX_STRING_SIZE> buf;
	av_make_error_string(buf.data(), buf.size(), code);
	return Video::string(buf.data(), buf.size());
}

static Video::string errstr(int err)
{
	return
#ifdef VIDEO_CUSTOM_TAG
	    Video::TAG
#else
	    Video::string
	{
		Video::TAG
	}
#endif
	    + ": " + averr(err);
}

static Video::string errstr(const char *err)
{
	return
#ifdef VIDEO_CUSTOM_TAG
	    Video::TAG
#else
	    Video::string
	{
		Video::TAG
	}
#endif
	    + ": " + err;
}

[[noreturn]] static void errthrow(const Video::string &err)
{
	throw Video::VideoError{err};
}

static void errcheck(int val)
{
	if (val < 0)
		errthrow(errstr(val));
}

template <class T> static void errcheck(const T &val, const char *errmsg)
{
	if (!static_cast<bool>(val))
		errthrow(errstr(errmsg));
}

template <class T> static void errcheck(T *ptr, const char *errmsg = nullptr)
{
	if (!ptr) {
		if (errmsg)
			errthrow(errstr(errmsg));
		else
			errthrow(errstr("unknown"));
	}
}
} // anonymous namespace

#ifdef VIDEO_CUSTOM_TAG
Video::string Video::TAG{"Video"};
#endif

Video::Video()
    :
#ifndef VIDEO_AVBUFFER
      buffer{static_cast<decltype(buffer)>(av_malloc(BUFFER_SIZE))},
#endif
      fmt_ctx{avformat_alloc_context(),
	      [](void *ptr_) {
		      auto ptr = static_cast<AVFormatContext *>(ptr_);
		      avformat_close_input(&ptr);
	      }},
      frame{av_frame_alloc(),
	    [](void *ptr_) {
		    auto ptr = static_cast<AVFrame *>(ptr_);
		    av_frame_free(&ptr);
	    }},
      pkt{static_cast<AVPacket *>(av_malloc(sizeof(AVPacket))), &::av_free}
{
	av_register_all();
	errcheck(fmt_ctx, "Could not allocate context");
	av_init_packet(pkt.get());
	pkt->data = nullptr;
	pkt->size = 0;
}

Video::~Video()
{
	if (avio_ctx) {
		av_freep(&avio_ctx->buffer);
	}
	// In case of error this will prevent memleak
	av_packet_unref(pkt.get());
}

Video::Video(void *data_, size_t size_) : Video() { set(data_, size_); }

void Video::set(void *data_, size_t size_)
{
	data_ptr = static_cast<byte *>(data_);
	data_size = size_;

	constexpr auto buffer_is_writable{false};

#ifndef VIDEO_AVBUFFER
	data = std::make_unique<mem_ctx>(data_ptr, data_size);

	avio_ctx = mk_ptr_(avio_alloc_context(
	    buffer, BUFFER_SIZE, buffer_is_writable, data.get(), &mem_ctx::read,
	    nullptr, &mem_ctx::seek));
#else
	avio_ctx =
	    mk_ptr_(avio_alloc_context(data_ptr, data_size, buffer_is_writable,
				       nullptr, nullptr, nullptr, nullptr));
#endif
	errcheck(avio_ctx, "Could not allocate context");

	fmt_ctx->pb = avio_ctx.get();

	auto fmt_ctx_ptr = fmt_ctx.get();
	auto status =
	    avformat_open_input(&fmt_ctx_ptr, nullptr, nullptr, nullptr);
	errcheck(status);

	status = avformat_find_stream_info(fmt_ctx_ptr, nullptr);
	errcheck(status);

	av_dump_format(fmt_ctx_ptr, 0, nullptr, 0);

	auto auto_{-1};
	auto flags{0};
	video_stream_idx = av_find_best_stream(fmt_ctx_ptr, AVMEDIA_TYPE_VIDEO,
					       auto_, auto_, nullptr, flags);
	errcheck(video_stream_idx);

	AVStream *st = fmt_ctx->streams[video_stream_idx];

	AVCodec *dec = avcodec_find_decoder(st->codecpar->codec_id);
	//	Drop old ffmpeg support
	//	AVCodec *dec = avcodec_find_decoder(st->codec->codec_id);

	dec_ctx = ptr_<AVCodecContext>{
	    avcodec_alloc_context3(dec), [](void *ptr_) {
		    auto ptr = static_cast<AVCodecContext *>(ptr_);
		    avcodec_free_context(&ptr);
	    }};
	errcheck(dec_ctx, "Could not allocate context");

	status = avcodec_parameters_to_context(dec_ctx.get(), st->codecpar);
	errcheck(status);

	AVDictionary *opts{nullptr};
	av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
	status = avcodec_open2(dec_ctx.get(), dec, &opts);
	errcheck(status);
}

void Video::process(std::function<void(unsigned char *, int, int, int)> f_)
{
	av_init_packet(pkt.get());
	pkt->data = nullptr;
	pkt->size = 0;

	while (av_read_frame(fmt_ctx.get(), pkt.get()) >= 0) {
		int status{0};
		if (pkt->stream_index != video_stream_idx) {
			av_packet_unref(pkt.get());
			continue;
		}

		status = avcodec_send_packet(dec_ctx.get(), pkt.get());

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wold-style-cast"

		if (status < 0 && status != AVERROR(EAGAIN) &&
		    status != AVERROR_EOF)
			errthrow(errstr(status));
		if (status >= 0)
			pkt->size = 0;

		status = avcodec_receive_frame(dec_ctx.get(), frame.get());

		if (status < 0 && status != AVERROR(EAGAIN) &&
		    status != AVERROR_EOF)
			errthrow(errstr(status));
#pragma clang diagnostic pop

		/*
		 * Reset packet and clear buffers
		 */
		av_packet_unref(pkt.get());

		auto w = frame->width;
		auto h = frame->height;
		if (!w || !h)
			continue;

		if (!frame_converted)
			frame_converted_alloc();

		auto gray_convert_ctx = sws_getContext(
		    w, h, dec_ctx->pix_fmt, w, h, output_pix_format, SWS_POINT,
		    nullptr, nullptr, nullptr);

		sws_scale(gray_convert_ctx, frame->data, frame->linesize, 0, h,
			  frame_converted->data, frame_converted->linesize);

		f_(frame_converted->data[0], frame_converted->linesize[0], w,
		   h);
		sws_freeContext(gray_convert_ctx);
	}
}

void Video::frame_converted_alloc()
{
	// TODO: alloc only required buffer
	AVFrame *new_frame = av_frame_clone(frame.get());
	frame_converted =
	    ptr_<AVFrame>{new_frame, [](void *ptr_) {
				  auto ptr = static_cast<AVFrame *>(ptr_);
				  av_frame_free(&ptr);
			  }};
}

#ifndef VIDEO_AVBUFFER
Video::mem_ctx::mem_ctx(const Video::byte *data_, size_t size_)
    : data_size{size_},
      f{::fmemopen(const_cast<Video::byte *>(data_), data_size, "r"),
	&std::fclose}
{
	if (!f)
		throw Video::VideoError{"Can't open buffer"};
}

int Video::mem_ctx::read(void *opaque, uint8_t *buf, int size)
{
	auto ctx = static_cast<mem_ctx *>(opaque);
	auto f = ctx->f.get();
	auto status = std::fread(buf, sizeof(uint8_t), size, f);

	if (status != static_cast<decltype(status)>(size)) {
		if (std::feof(f)) {
			return status;
		} else {
			return -1;
		}
	} else {
		return size;
	}
}

int64_t Video::mem_ctx::seek(void *opaque, int64_t pos, int whence)
{
	if (pos < 0)
		return -1;
	auto *ctx = static_cast<mem_ctx *>(opaque);
	auto f = ctx->f.get();

	switch (whence) {
	case SEEK_SET:
		break;
	case SEEK_CUR:
		pos += std::ftell(f);
		break;
	case SEEK_END:
		pos = ctx->data_size - pos;
		break;
	case AVSEEK_SIZE:
		return ctx->data_size;
	default:
		break;
	}

	auto status = std::fseek(f, pos, whence);
	if (status == 0)
		return pos;
	else
		return -1;
}
#endif

#pragma clang diagnostic pop
