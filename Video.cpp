#include "Video.h"

extern "C" {
#include <libswscale/swscale.h>
}

#include <iostream>
#include <stdexcept>

namespace
{
using str_t = decltype(Video::TAG);

static str_t averr(int code)
{
	static thread_local std::array<char, AV_ERROR_MAX_STRING_SIZE> buf;
	av_make_error_string(buf.data(), buf.size(), code);
	return str_t(buf.data(), buf.size());
}

static str_t errstr(int err) { return Video::TAG + ": " + averr(err); }

static str_t errstr(const char *err) { return Video::TAG + ": " + err; }

static void errthrow(str_t err) { throw std::runtime_error{std::move(err)}; }

static void errcheck(int val)
{
	if (val < 0)
		errthrow(errstr(val));
}

template <class T> static void errcheck(T *ptr, const char *errmsg)
{
	if (!ptr)
		errthrow(errstr(errmsg));
}

static int read_packet(void *opaque, uint8_t *buf, int buf_size)
{
	struct _bd {
		uint8_t *ptr;
		size_t size;
	};
	_bd *bd = static_cast<_bd *>(opaque);

	buf_size = FFMIN(buf_size, bd->size);

	memcpy(buf, bd->ptr, buf_size);
	bd->ptr += buf_size;
	bd->size -= buf_size;

	return buf_size;
}
}

std::string Video::TAG = "AV";

Video::Video()
{
	av_register_all();
	avcodec_register_all();

	frame = av_frame_alloc();
	errcheck(frame, "Could not allocate frame");

	pkt = static_cast<AVPacket *>(av_malloc(sizeof(AVPacket)));
	errcheck(pkt, "Could not allocate packet");
	av_init_packet(pkt);
}

Video::Video(void *data_ptr, size_t data_size) : Video()
{
	set(data_ptr, data_size);
}

Video::~Video()
{
	avformat_close_input(&ctx);
	if (avio_ctx) {
		av_freep(&avio_ctx->buffer);
		av_freep(&avio_ctx);
	}

	if (video_ctx) {
		avcodec_close(video_ctx);
		av_free(video_ctx);
	}
	if (frame)
		av_frame_free(&frame);
	if (frame_converted_buffer)
		av_freep(&frame_converted_buffer);
	if (frame_converted)
		av_frame_free(&frame_converted);
	if (pkt) {
		av_free_packet(pkt);
		av_free(pkt);
	}
}

void Video::set(void *data_ptr, size_t data_size)
{
	bd.ptr = static_cast<uint8_t *>(data_ptr);
	bd.size = data_size;

	init_stream();
	init_frame_converted();
	init_codec();
	pkt->data = nullptr;
	pkt->size = 0;
}

void Video::process(
    std::function<void(unsigned char *buf, int wrap, int xsize, int ysize)> f_)
{
	int frame_count{0};
	int status = -1;
	while ((status = av_read_frame(ctx, pkt)) >= 0) {
		int got_frame;
		auto len =
		    avcodec_decode_video2(video_ctx, frame, &got_frame, pkt);
		errcheck(len);

		if (got_frame == 0)
			errthrow("No frame could be decompressed");

		auto w = frame->width;
		auto h = frame->height;
		auto gray_convert_ctx = sws_getContext(
		    w, h, input_pix_format, w, h, output_pix_format, SWS_POINT,
		    nullptr, nullptr, nullptr);

		sws_scale(gray_convert_ctx, frame->data, frame->linesize, 0, h,
			  frame_converted->data, frame_converted->linesize);

		f_(frame_converted->data[0], frame_converted->linesize[0], w,
		   h);
		++frame_count;
		sws_freeContext(gray_convert_ctx);

		if (pkt->data) {
			pkt->size -= len;
			pkt->data += len;
		}
	}
	errcheck(status);
}

void Video::init_stream()
{
	ctx = avformat_alloc_context();
	errcheck(ctx, "Could not allocate format context");

	avio_ctx_buffer =
	    static_cast<uint8_t *>(av_malloc(avio_ctx_buffer_size));
	errcheck(avio_ctx_buffer, "Could not allocate io buffer");

	avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0,
				      &bd, &read_packet, nullptr, nullptr);
	errcheck(avio_ctx, "Could not allocate io context");
	ctx->pb = avio_ctx;

	auto status = avformat_open_input(&ctx, nullptr, nullptr, nullptr);
	errcheck(status);

	status = avformat_find_stream_info(ctx, nullptr);
	errcheck(status);

	video_stream = ctx->streams[av_find_default_stream_index(ctx)];

	width = video_stream->codec->width;
	heigh = video_stream->codec->height;

	input_pix_format = video_stream->codec->pix_fmt;
}

void Video::init_codec()
{
	auto codec = avcodec_find_decoder(video_stream->codec->codec_id);
	errcheck(codec, "Codec not found");

	video_ctx = avcodec_alloc_context3(codec);
	errcheck(video_ctx, "Could not allocate video codec context");

	if (codec->capabilities & AV_CODEC_CAP_TRUNCATED)
		video_ctx->flags |=
		    AV_CODEC_FLAG_TRUNCATED; // we do not send complete frames

	auto status = avcodec_open2(video_ctx, codec, nullptr);
	errcheck(status);
}

void Video::init_frame_converted()
{
	frame_converted = av_frame_alloc();
	errcheck(frame_converted, "Could not allocate frame");

	int frame_converted_buffer_size =
	    avpicture_get_size(output_pix_format, width, heigh);
	errcheck(frame_converted_buffer_size);

	frame_converted_buffer =
	    static_cast<uint8_t *>(av_malloc(frame_converted_buffer_size));
	errcheck(frame_converted_buffer, "Could not allocate picture buffer");

	auto status = avpicture_fill(
	    reinterpret_cast<AVPicture *>(frame_converted),
	    frame_converted_buffer, output_pix_format, width, heigh);
	errcheck(status);
}
