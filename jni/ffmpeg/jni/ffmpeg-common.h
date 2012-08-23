#ifndef FFMPEG_JNI_COMMON_H
#define FFMPEG_JNI_COMMON_H

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavformat/isom.h"
#include <string.h>
#include <jni.h>
#include <stdio.h>
#include <android/log.h>
#include <stdbool.h>

#define LOG(x...) __android_log_print(ANDROID_LOG_DEBUG, "camomile", x)
#define LOGE(x...) __android_log_print(ANDROID_LOG_ERROR, "camomile", x)

static bool initialized = false;
#define AUDIO_INBUF_SIZE 20480

typedef struct Context {
    AVFormatContext *f_ctx;
    AVCodecContext *c_ctx;
    AVCodec *codec;
    int stream_id;
    AVPacket pkt;
    uint8_t inbuf[AUDIO_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
    int bytesleft;
    char *out_buf;
    int skip_bytes;
    int bps;
} Context;

static void log_error(const char * method, int code) {
    char error[256];
    av_strerror(code, error, sizeof(error));
    LOGE("%s returned %d, %s", method, code, error);
}

Context* av_open(JNIEnv* env, jstring file, jintArray formatArray) {
	if (!initialized) {
		av_register_all();
		initialized = true;
	}
	Context* ctx = malloc(sizeof(Context));

	const char* fileName = (*env)->GetStringUTFChars(env, file, NULL);
	LOG("opening file %s", fileName);
	int ret = av_open_input_file(&ctx->f_ctx, fileName, NULL, 0, NULL);

	if (ret != 0) {
	    log_error("av_open_input_file", ret);
        return 0;
    }
	(*env)->ReleaseStringUTFChars(env, file, fileName);

    av_find_stream_info(ctx->f_ctx);
    ctx->stream_id = -1;
    for (int i = 0; i < ctx->f_ctx->nb_streams; i++) {
        if (ctx->f_ctx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO) {
            ctx->stream_id = i;
            break;
        }
    }

    if (ctx->stream_id == -1) {
        LOGE("could not find audio stream");
        return 0;
    }

    ctx->c_ctx = ctx->f_ctx->streams[ctx->stream_id]->codec;

	jint* format = (jint*)(*env)->GetIntArrayElements(env, formatArray, 0);
	format[0] = ctx->c_ctx->sample_rate;
    format[1] = ctx->c_ctx->channels;
    format[2] = av_get_bits_per_sample_format(ctx->c_ctx->sample_fmt);
    ctx->bps = format[1] * format[2] / 8;
    (*env)->ReleaseIntArrayElements(env, formatArray, format, 0);

    ctx->codec = avcodec_find_decoder(ctx->c_ctx->codec_id);
    if(!ctx->codec) {
      LOGE("unsupported codec");
      return 0;
    }
    if (avcodec_open(ctx->c_ctx, ctx->codec) < 0) {
        LOGE("failed to open codec");
    }

    LOG("enc_delay: %d, enc_padding: %d", ctx->f_ctx->enc_delay, ctx->f_ctx->enc_padding);
    LOG("total samples: %d", ctx->f_ctx->duration * ctx->c_ctx->sample_rate / AV_TIME_BASE);

    ctx->bytesleft = 0;
    ctx->skip_bytes = 0;
    ctx->out_buf = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

	return ctx;
}

jint av_decode(Context* ctx) {
	if (ctx->bytesleft == 0) {
//	    LOG("reading next frame");
        int ret = av_read_frame (ctx->f_ctx, &ctx->pkt);
//        LOG("%d", ctx->pkt.data);
        if (ret < 0) {
            log_error("av_read_frame", ret);
            return -1;
        }
        ctx->bytesleft = ctx->pkt.size;
//        LOG("got a frame. size: %d", ctx->pkt.size);
    }
    if (ctx->f_ctx->streams[ctx->pkt.stream_index]->codec->codec_type != AVMEDIA_TYPE_AUDIO) {
        LOG("found non-audio packet");
        return -1;
    }
    int out_size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
    int consumed = avcodec_decode_audio3(ctx->c_ctx, (short *)ctx->out_buf, &out_size, &ctx->pkt);
//    LOG("consumed %d bytes. output %d bytes", consumed, out_size);
    if (consumed < 0) {
        return -1;
    } else {
        ctx->bytesleft -= consumed;
    }

    if (ctx->bytesleft == 0)
        av_free_packet(&ctx->pkt);
		
	return out_size;
}

int av_seek(Context* ctx, int offset) {
    int pos = (((double) offset) / ctx->c_ctx->sample_rate) * AV_TIME_BASE;
    LOG("seek to sample %d, samplerate %d, position: %d", offset, ctx->c_ctx->sample_rate, pos);
    int ret = av_seek_frame(ctx->f_ctx, -1, pos, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(ctx->c_ctx);
    ctx->bytesleft = 0;
    if (ret < 0) {
        log_error("av_seek_frame", ret);
    }
}

void av_close(Context* ctx) {
	
}

#endif