#include <string.h>
#include <jni.h>
#include <stdio.h>
#include <android/log.h>
#include "src/ivorbisfile.h"
#include "src/ivorbiscodec.h"

#define LOG(x...) __android_log_print(ANDROID_LOG_DEBUG, "camomile", x)
#define LOGE(x...) __android_log_print(ANDROID_LOG_ERROR, "camomile", x)

#define JNI_FUNCTION(name) Java_com_tulskiy_camomile_audio_formats_ogg_VorbisDecoder_ ## name

jint JNI_FUNCTION(open) (JNIEnv* env, jobject obj, jstring file, jintArray formatArray) {
    OggVorbis_File* vf = malloc(sizeof(OggVorbis_File));

    const char* fileName = (*env)->GetStringUTFChars(env, file, NULL);
    FILE* input = fopen(fileName, "r");
    (*env)->ReleaseStringUTFChars(env, file, fileName);

    if (!input) {
        LOGE("could not open file");
        return 0;
    }

    int ret = ov_open(input, vf, NULL, 0);

    if (ret != 0) {
        LOGE("libvorbis ov_open returned an error: %d", ret);
        return 0;
    }

    vorbis_info* vi = ov_info(vf, -1);
    LOG("vorbis file channels: %d, sample rate: %d", vi->channels, vi->rate);
    jint* format = (jint*)(*env)->GetIntArrayElements(env, formatArray, 0);
    format[0] = vi->rate;
    format[1] = vi->channels;
    format[2] = 16;
    (*env)->ReleaseIntArrayElements(env, formatArray, format, 0);

    return (jint) vf;
}

void JNI_FUNCTION(seek) (JNIEnv* env, jobject obj, jint handle, jint offset) {
    OggVorbis_File* vf = (OggVorbis_File*) handle;
    int ret = ov_pcm_seek(vf, offset);

    if (ret != 0) {
        LOGE("Error during seek to sample %d: %d", offset, ret);
    }
}

jint JNI_FUNCTION(decode) (JNIEnv* env, jobject obj, jint handle, jbyteArray buffer, jint size) {
    OggVorbis_File* vf = (OggVorbis_File*) handle;
    jbyte* target = (jbyte*)(*env)->GetByteArrayElements(env, buffer, 0);

    int bitstream;
    int ret = ov_read(vf, target, size, &bitstream);

    (*env)->ReleaseByteArrayElements(env, buffer, target, 0);
    if (ret <= 0) {
        LOGE("libvobis returned an error (possibly EOF): %s", ret);
        return -1;
    } else {
        return ret;
    }
}

void JNI_FUNCTION(close) (JNIEnv* env, jobject obj, jint handle) {
    OggVorbis_File* vf = (OggVorbis_File*) handle;

    ov_clear(vf);
}