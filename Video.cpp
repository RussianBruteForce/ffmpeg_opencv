#include "Video.h"

extern "C" {
#include <libswscale/swscale.h>
}

#include <iostream>
#include <stdexcept>

Video::Video(void *data_ptr, size_t data_size)
    : bd{static_cast<uint8_t *>(data_ptr), data_size}
{
	av_register_all();
	avcodec_register_all();

	init_stream();
	init_frame_converted();
	init_codec();

	frame = av_frame_alloc();
	if (!frame)
		throw std::runtime_error{"video: could not allocate frame"};

	av_init_packet(&pkt);
	pkt.data = nullptr;
	pkt.size = 0;
}

Video::~Video()
{
	avformat_close_input(&ctx);
	if (avio_ctx)
		av_freep(&avio_ctx->buffer);
	av_freep(&avio_ctx);

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
}

void Video::process(
    std::function<void(unsigned char *buf, int wrap, int xsize, int ysize)> f_)
{
	int frame_count{0};
	while (av_read_frame(ctx, &pkt) >= 0) {
		int got_frame;
		auto len =
		    avcodec_decode_video2(video_ctx, frame, &got_frame, &pkt);
		if (len < 0)
			throw std::runtime_error{
			    "video: error while decoding frame " +
			    std::to_string(frame_count)};

		if (got_frame) {
			auto w = frame->width;
			auto h = frame->height;
			auto gray_convert_ctx = sws_getContext(
			    w, h, input_pix_format, w, h, output_pix_format,
			    SWS_POINT, nullptr, nullptr, nullptr);

			sws_scale(gray_convert_ctx, frame->data,
				  frame->linesize, 0, h, frame_converted->data,
				  frame_converted->linesize);

			f_(frame_converted->data[0],
			   frame_converted->linesize[0], w, h);
			++frame_count;
			sws_freeContext(gray_convert_ctx);
		}
		if (pkt.data) {
			pkt.size -= len;
			pkt.data += len;
		}
	}
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

void Video::init_stream()
{
	ctx = avformat_alloc_context();
	if (!ctx)
		throw std::runtime_error{
		    "video: could not allocate format contex"};

	avio_ctx_buffer =
	    static_cast<uint8_t *>(av_malloc(avio_ctx_buffer_size));
	if (!avio_ctx_buffer)
		throw std::runtime_error{"video: could not allocate io buffer"};

	avio_ctx = avio_alloc_context(avio_ctx_buffer, avio_ctx_buffer_size, 0,
				      &bd, &read_packet, nullptr, nullptr);
	if (!avio_ctx)
		throw std::runtime_error{"video: could not allocate io contex"};
	ctx->pb = avio_ctx;

	auto status = avformat_open_input(&ctx, nullptr, nullptr, nullptr);
	if (status < 0)
		throw std::runtime_error{"video: could not open input"};

	status = avformat_find_stream_info(ctx, nullptr);
	if (status < 0)
		throw std::runtime_error{
		    "video: could not find stream information"};

	video_stream = ctx->streams[av_find_default_stream_index(ctx)];

	width = video_stream->codec->width;
	heigh = video_stream->codec->height;

	input_pix_format = video_stream->codec->pix_fmt;
}

void Video::init_codec()
{
	auto codec = avcodec_find_decoder(video_stream->codec->codec_id);
	if (!codec)
		throw std::runtime_error{"video: codec not found"};

	video_ctx = avcodec_alloc_context3(codec);
	if (!video_ctx)
		throw std::runtime_error{
		    "video: could not allocate video codec contex"};

	if (codec->capabilities & AV_CODEC_CAP_TRUNCATED)
		video_ctx->flags |=
		    AV_CODEC_FLAG_TRUNCATED; // we do not send complete frames

	if (avcodec_open2(video_ctx, codec, nullptr) < 0)
		throw std::runtime_error{"video: could not open codec"};
}

void Video::init_frame_converted()
{
	frame_converted = av_frame_alloc();
	if (!frame_converted)
		throw std::runtime_error{"video: could not allocate frame"};

	int frame_converted_buffer_size =
	    avpicture_get_size(output_pix_format, width, heigh);

	frame_converted_buffer =
	    static_cast<uint8_t *>(av_malloc(frame_converted_buffer_size));
	if (!frame_converted_buffer)
		throw std::runtime_error{
		    "video: could not allocate picture buffer"};

	avpicture_fill(reinterpret_cast<AVPicture *>(frame_converted),
		       frame_converted_buffer, output_pix_format, width, heigh);
}
