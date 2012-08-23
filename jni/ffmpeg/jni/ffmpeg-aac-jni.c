#include "ffmpeg-common.h"

#define JNI_FUNCTION(name) Java_com_tulskiy_camomile_audio_formats_aac_MP4Decoder_ ## name

#define AAC_FRAME_SIZE 1024
#define SEEK_PREHEAT 2

void JNI_FUNCTION(seek) (JNIEnv* env, jobject obj, jint handle, jint offset) {
    Context* ctx = (Context*) handle;
    offset += ctx->f_ctx->enc_delay;

    av_seek(ctx, offset);

    AVStream* stream = ctx->f_ctx->streams[ctx->stream_id];
    MOVStreamContext *sc = stream->priv_data;

    sc->current_sample -= SEEK_PREHEAT;
    for (int i = 0; i < SEEK_PREHEAT; i++) {
        av_decode(ctx);
    }

    ctx->skip_bytes = ctx->bps * (offset - (sc->current_sample * AAC_FRAME_SIZE));
    LOG("current sample: %d, skip_bytes: %d, sample count: %d", sc->current_sample, ctx->skip_bytes, sc->sample_count);
}

jint JNI_FUNCTION(open) (JNIEnv* env, jobject obj, jstring file, jintArray formatArray) {
    Context* ctx = av_open(env, file, formatArray);

    JNI_FUNCTION(seek)(env, obj, (jint) ctx, 0);

	return (jint) ctx;
}

jint JNI_FUNCTION(decode) (JNIEnv* env, jobject obj, jint handle, jbyteArray buffer, jint size) {
    Context* ctx = (Context*) handle;

    jbyte* target = (jbyte*)(*env)->GetByteArrayElements(env, buffer, 0);
    int ret = av_decode(ctx);

    if (ret > 0) {
        ret -= ctx->skip_bytes;
        memcpy(target, ctx->out_buf + ctx->skip_bytes, ret);
        ctx->skip_bytes = 0;

        AVStream* stream = ctx->f_ctx->streams[ctx->stream_id];
        MOVStreamContext *sc = stream->priv_data;

        if (sc->current_sample == sc->sample_count)
            ret -= ctx->f_ctx->enc_padding * ctx->bps;
    }

    (*env)->ReleaseByteArrayElements(env, buffer, (jbyte*)target, 0);
    
    return ret;
}

void JNI_FUNCTION(close) (JNIEnv* env, jobject obj, jint handle) {

}