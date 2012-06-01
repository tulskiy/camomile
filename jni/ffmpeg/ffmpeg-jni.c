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

#define JNI_FUNCTION(name) Java_com_tulskiy_camomile_audio_formats_ffmpeg_FFMPEGDecoder_ ## name

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
} Context;

jint JNI_FUNCTION(open) (JNIEnv* env, jobject obj, jstring file, jintArray formatArray) {
	if (!initialized) {
		av_register_all();
		initialized = true;
	}
	Context* ctx = malloc(sizeof(Context));

	const char* fileName = (*env)->GetStringUTFChars(env, file, NULL);
	int ret = av_open_input_file(&ctx->f_ctx, fileName, NULL, 0, NULL);
	if (ret != 0) {
	    char error[256];
        av_strerror(ret, error, sizeof(error));
	    LOGE("could not open input file: %s, reason: %s", fileName, error);
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
    format[3] = ctx->f_ctx->enc_delay;
    format[4] = ctx->f_ctx->enc_padding;
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
    ctx->out_buf = av_malloc(AVCODEC_MAX_AUDIO_FRAME_SIZE);

	return (jint) ctx;
}

void JNI_FUNCTION(seek) (JNIEnv* env, jobject obj, jint handle, jint offset) {
    Context* ctx = (Context*) handle;

    int pos = (((double) offset) / ctx->c_ctx->sample_rate) * AV_TIME_BASE;
    LOG("seek to sample %d, samplerate %d, position: %d", offset, ctx->c_ctx->sample_rate, pos);
    int ret = av_seek_frame(ctx->f_ctx, -1, pos, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(ctx->c_ctx);
    ctx->bytesleft = 0;
    if (ret < 0) {
        char error[256];
        av_strerror(ret, error, sizeof(error));
        LOGE("av_seek_frame returned %d, %s", ret, error);
    }

    AVStream* stream = ctx->f_ctx->streams[ctx->stream_id];
    MOVStreamContext *sc = stream->priv_data;
    LOG("current sample %d, cttc sample: %d, samples per frame %d, sample count %d", sc->current_sample, sc->ctts_sample, sc->sample_size, sc->sample_count);
}

jint JNI_FUNCTION(decode) (JNIEnv* env, jobject obj, jint handle, jbyteArray buffer, jint size) {
    Context* ctx = (Context*) handle;
	jbyte* target = (jbyte*)(*env)->GetByteArrayElements(env, buffer, 0);
	if (ctx->bytesleft == 0) {
//	    LOG("reading next frame");
        int ret = av_read_frame (ctx->f_ctx, &ctx->pkt);
//        LOG("%d", ctx->pkt.data);
        if (ret < 0) {
            char error[256];
            av_strerror(ret, error, sizeof(error));
            LOGE("av_read_frame returned %d, %s", ret, error);
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
    memcpy(target, ctx->out_buf, out_size);
	(*env)->ReleaseByteArrayElements(env, buffer, (jbyte*)target, 0);
	return out_size;
}

void JNI_FUNCTION(close) (JNIEnv* env, jobject obj, jint handle) {

}