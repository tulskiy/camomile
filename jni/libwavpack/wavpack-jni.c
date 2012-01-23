#include <string.h>
#include <jni.h>
#include <stdio.h>
#include <android/log.h>
#include "src/wavpack.h"

#define LOG(x...) __android_log_print(ANDROID_LOG_DEBUG, "camomile", x)
#define LOGE(x...) __android_log_print(ANDROID_LOG_ERROR, "camomile", x)

#define JNI_FUNCTION(name) Java_com_tulskiy_camomile_audio_formats_wavpack_WavPackDecoder_ ## name

typedef struct {
    WavpackContext *wpc;
    int32_t *temp_buffer;
    int channels;
    int bps;
} Context;

static unsigned char *format_samples (int bps, unsigned char *dst, int32_t *src, uint32_t samcnt) {
    int32_t temp;
    switch (bps) {
        case 1:
            while (samcnt--)
                *dst++ = *src++ + 128;
            break;
        case 2:
            while (samcnt--) {
                *dst++ = (unsigned char) (temp = *src++);
                *dst++ = (unsigned char) (temp >> 8);
            }
            break;
    }
    return dst;
}

jint JNI_FUNCTION(open) (JNIEnv* env, jobject obj, jstring file, jintArray formatArray) {
    const char* fileName = (*env)->GetStringUTFChars(env, file, NULL);
    char error[80];
    int flags = OPEN_WVC | OPEN_2CH_MAX;
    Context* ctx = malloc(sizeof(Context));
    ctx->wpc = WavpackOpenFileInput(fileName, error, flags, 0);
    (*env)->ReleaseStringUTFChars(env, file, fileName);

    if (!ctx->wpc) {
        LOGE("Error opening wavpack file: %s", error);
        return 0;
    }

    ctx->channels = WavpackGetReducedChannels(ctx->wpc);
    ctx->bps = WavpackGetBytesPerSample(ctx->wpc);
    ctx->temp_buffer = malloc(4096L * ctx->channels * 4);

    jint* format = (jint*)(*env)->GetIntArrayElements(env, formatArray, 0);
    format[0] = WavpackGetSampleRate(ctx->wpc);
    format[1] = ctx->channels;
    format[2] = ctx->bps * 8;
    (*env)->ReleaseIntArrayElements(env, formatArray, format, 0);

    return (jint) ctx;
}

void JNI_FUNCTION(seek) (JNIEnv* env, jobject obj, jint handle, jint offset) {
	Context* ctx = (Context*) handle;
	if (!WavpackSeekSample(ctx->wpc, offset)) {
		LOGE("Could not seek to sample: %d, total samples %d", offset, WavpackGetNumSamples(ctx->wpc));
	}
}

jint JNI_FUNCTION(decode) (JNIEnv* env, jobject obj, jint handle, jbyteArray buffer, jint size) {
    Context* ctx = (Context*) handle;
    jbyte* target = (jbyte*)(*env)->GetByteArrayElements(env, buffer, 0);

    int samples_unpacked = WavpackUnpackSamples(ctx->wpc, ctx->temp_buffer, 4096);
	LOG("unpacked %d", samples_unpacked);
    if (samples_unpacked) {
        format_samples(ctx->bps, target, ctx->temp_buffer, samples_unpacked * ctx->channels);
    }
    (*env)->ReleaseByteArrayElements(env, buffer, target, 0);
	
	if (!samples_unpacked)
		return -1;
	else 
		return samples_unpacked * ctx->channels * ctx->bps;
}

void JNI_FUNCTION(close) (JNIEnv* env, jobject obj, jint handle) {
	Context* ctx = (Context*) handle;
	WavpackCloseFile(ctx->wpc);
	free(ctx->temp_buffer);
	free(ctx);
}