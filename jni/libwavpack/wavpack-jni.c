#include <string.h>
#include <jni.h>
#include <stdio.h>
#include <android/log.h>
#include "src/wavpack.h"

#define LOG(x...) __android_log_print(ANDROID_LOG_DEBUG, "camomile", x)
#define LOGE(x...) __android_log_print(ANDROID_LOG_ERROR, "camomile", x)

#define JNI_FUNCTION(name) Java_com_tulskiy_camomile_audio_formats_wavpack_WavPackDecoder_ ## name

jint JNI_FUNCTION(open) (JNIEnv* env, jobject obj, jstring file, jintArray formatArray) {
    const char* fileName = (*env)->GetStringUTFChars(env, file, NULL);
    char error[80];
    int flags = OPEN_WVC | OPEN_2CH_MAX;
    WavpackContext *wpc = WavpackOpenFileInput(fileName, error, flags, 0);
    (*env)->ReleaseStringUTFChars(env, file, fileName);

    if (!wpc) {
        LOGE("Error opening wavpack file: %s", error);
        return 0;
    }

    jint* format = (jint*)(*env)->GetIntArrayElements(env, formatArray, 0);
    format[0] = WavpackGetSampleRate(wpc);
    format[1] = WavpackGetReducedChannels(wpc);
    format[2] = WavpackGetBitsPerSample(wpc);
    (*env)->ReleaseIntArrayElements(env, formatArray, format, 0);

    return (jint) wpc;
}

void JNI_FUNCTION(seek) (JNIEnv* env, jobject obj, jint handle, jint offset) {
//    OggVorbis_File* vf = (OggVorbis_File*) handle;
//    int ret = ov_pcm_seek(vf, offset);
//
//    if (ret != 0) {
//        LOGE("Error during seek to sample %d: %d", offset, ret);
//    }
}

jint JNI_FUNCTION(decode) (JNIEnv* env, jobject obj, jint handle, jbyteArray buffer, jint size) {
    WavpackContext* wpc = (WavpackContext*) handle;
    jbyte* target = (jbyte*)(*env)->GetByteArrayElements(env, buffer, 0);

    int ret = WavpackUnpackSamples(wpc, (int32_t*)target, size);
    (*env)->ReleaseByteArrayElements(env, buffer, target, 0);
    if (ret <= 0) {
        LOGE("libvobis returned an error (possibly EOF): %s", ret);
        return -1;
    } else {
        return ret;
    }
}

void JNI_FUNCTION(close) (JNIEnv* env, jobject obj, jint handle) {
//    OggVorbis_File* vf = (OggVorbis_File*) handle;
//
//    ov_clear(vf);
}