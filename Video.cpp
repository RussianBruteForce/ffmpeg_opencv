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

static int64_t seek(void *opaque, int64_t offset, int whence) {
	struct _bd {
		uint8_t *ptr;
		size_t size;
	};
	_bd *bd = static_cast<_bd *>(opaque);

    if (AVSEEK_SIZE == whence)
	return -1;//bd->size;

    bd->ptr += offset;
    bd->size -= offset;

    return offset + whence;
}

static int refcount = 0;

static int open_codec_context(int *stream_idx,
                              AVCodecContext **dec_ctx, AVFormatContext *fmt_ctx, enum AVMediaType type)
{
	int ret, stream_index;
	AVStream *st;
	AVCodec *dec = nullptr;
	AVDictionary *opts = nullptr;

	ret = av_find_best_stream(fmt_ctx, type, -1, -1, nullptr, 0);
	if (ret < 0) {
		fprintf(stderr, "Could not find %s stream in input file\n",
		        av_get_media_type_string(type));
		return ret;
	} else {
		stream_index = ret;
		st = fmt_ctx->streams[stream_index];

		/* find decoder for the stream */
		dec = avcodec_find_decoder(st->codecpar->codec_id);
		if (!dec) {
			fprintf(stderr, "Failed to find %s codec\n",
			        av_get_media_type_string(type));
			return AVERROR(EINVAL);
		}

		/* Allocate a codec context for the decoder */
		*dec_ctx = avcodec_alloc_context3(dec);
		if (!*dec_ctx) {
			fprintf(stderr, "Failed to allocate the %s codec context\n",
			        av_get_media_type_string(type));
			return AVERROR(ENOMEM);
		}

		/* Copy codec parameters from input stream to output codec context */
		if ((ret = avcodec_parameters_to_context(*dec_ctx, st->codecpar)) < 0) {
			fprintf(stderr, "Failed to copy %s codec parameters to decoder context\n",
			        av_get_media_type_string(type));
			return ret;
		}

		/* Init the decoders, with or without reference counting */
		av_dict_set(&opts, "refcounted_frames", refcount ? "1" : "0", 0);
		if ((ret = avcodec_open2(*dec_ctx, dec, &opts)) < 0) {
			fprintf(stderr, "Failed to open %s codec\n",
			        av_get_media_type_string(type));
			return ret;
		}
		*stream_idx = stream_index;
	}

	return 0;
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
	int got_frame;
	while (av_read_frame(ctx, pkt) >= 0) {
		AVPacket orig_pkt = *pkt;
	    do {
		auto ret = decode_packet(&got_frame, 0, frame_count);
		if (ret < 0)
			break;
		pkt->data += ret;
		pkt->size -= ret;
	    } while (pkt->size > 0);
	    av_packet_unref(&orig_pkt);
	}
//	int status = -1;
//	while ((status = av_read_frame(ctx, pkt)) >= 0) {
//		if(pkt->stream_index != video_stream_idx)
//			continue;


//		do {
//			auto len =
//			                avcodec_decode_video2(video_ctx, frame, &got_frame, pkt);
//			len = FFMIN(len, pkt->size);
////			errcheck(len);

//			if (!got_frame)
//				break;
////				errthrow("No frame could be decompressed");

//			auto w = frame->width;
//			auto h = frame->height;
//			auto gray_convert_ctx = sws_getContext(
//			                                w, h, input_pix_format, w, h, output_pix_format, SWS_POINT,
//			                                nullptr, nullptr, nullptr);

//			sws_scale(gray_convert_ctx, frame->data, frame->linesize, 0, h,
//			          frame_converted->data, frame_converted->linesize);

//			f_(frame_converted->data[0], frame_converted->linesize[0], w,
//			                h);
//			++frame_count;
//			sws_freeContext(gray_convert_ctx);

//			if (pkt->data) {
//				pkt->size -= len;
//				pkt->data += len;
//			}
//		} while(pkt->size > 0);
//        }
//        if (status != AVERROR_EOF)
//                errcheck(status);
}

void Video::init_stream()
{
	ctx = avformat_alloc_context();
	errcheck(ctx, "Could not allocate format context");

	avio_ctx_buffer =
	                static_cast<uint8_t *>(av_malloc(avio_ctx_buffer_size));
	errcheck(avio_ctx_buffer, "Could not allocate io buffer");

	data = std::make_unique<mem_ctx>(bd.ptr, bd.size);
	avio_ctx = avio_alloc_context(bd.ptr, bd.size, 0,
	                              data.get(), &mem_ctx::read, nullptr, &mem_ctx::seek);
	errcheck(avio_ctx, "Could not allocate io context");

	ctx->pb = avio_ctx;

	auto status = avformat_open_input(&ctx, nullptr, nullptr, nullptr);
	errcheck(status);

	status = avformat_find_stream_info(ctx, nullptr);
	errcheck(status);

	status = open_codec_context(&video_stream_idx, &video_ctx, ctx, AVMEDIA_TYPE_VIDEO);
	errcheck(status);

	video_stream = ctx->streams[video_stream_idx];
	errcheck(video_stream, "Could not find valid video stream");

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

int Video::decode_packet(int* got_frame, int cached, int &video_frame_count)
{
	int ret = 0;
	int decoded = pkt->size;

	*got_frame = 0;

	if (pkt->stream_index == video_stream_idx) {
		/* decode video frame */
		ret = avcodec_decode_video2(video_ctx, frame, got_frame, pkt);
	    errcheck(ret);

	    if (*got_frame) {
		printf("video_frame%s n:%d coded_n:%d\n",
		       cached ? "(cached)" : "",
		       video_frame_count++, frame->coded_picture_number);
	    }
	}

	/* If we use frame reference counting, we own the data and need
	 * to de-reference it when we don't use it anymore */
	if (*got_frame && refcount)
		av_frame_unref(frame);

	return decoded;
}


Video::mem_ctx::mem_ctx(const Video::byte* data, size_t size):
        data_size{size},
        f{::fmemopen(const_cast<Video::byte*>(data), data_size, "r"),
          &std::fclose}
{
	if (!f)
		throw std::runtime_error{"Can't open buffer"};
}

int Video::mem_ctx::read(void* opaque, uint8_t* buf, int size) {
	auto ctx = static_cast<mem_ctx*>(opaque);
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

int64_t Video::mem_ctx::seek(void* opaque, int64_t pos, int whence) {
	if (pos < 0)
		return -1;
	auto *ctx = static_cast<mem_ctx*>(opaque);
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
		break;
	default:
		break;
	}

	auto status = std::fseek(f, pos, whence);
	if (status == 0)
		return pos;
	else
		return -1;
}

