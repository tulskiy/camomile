#include <string.h>
#include <jni.h>
#include "src/mpg123.h"
#include <stdio.h>
#include <android/log.h>

#define LOG(x...) __android_log_print(ANDROID_LOG_DEBUG, "camomile", x)
#define LOGE(x...) __android_log_print(ANDROID_LOG_ERROR, "camomile", x)

#define JNI_FUNCTION(name) Java_com_tulskiy_camomile_audio_formats_mp3_MP3Decoder_ ## name

jint JNI_FUNCTION(open) (JNIEnv* env, jobject obj, jstring file, jintArray formatArray) {
    mpg123_init();

    mpg123_handle* mh = mpg123_new(NULL, NULL);

    const char* fileName = (*env)->GetStringUTFChars(env, file, NULL);
    int ret = mpg123_open(mh, fileName);
    LOG("Opening file %s. Result: %s", fileName, mpg123_plain_strerror(ret));
    (*env)->ReleaseStringUTFChars(env, file, fileName);


    if (ret == MPG123_OK) {
        // decode a bit to get new format
        off_t framenum;
        unsigned char audio[1];
        size_t bytes;
        ret = mpg123_decode(mh, NULL, 0, audio, 1, &bytes);

        if(ret == MPG123_NEW_FORMAT) {
            long rate;
            int channels, encoding;
            mpg123_getformat(mh, &rate, &channels, &encoding);

            jint* format = (jint*)(*env)->GetIntArrayElements(env, formatArray, 0);
            format[0] = (int) rate;
            format[1] = channels;
            format[2] = encoding;
            (*env)->ReleaseIntArrayElements(env, formatArray, format, 0);

            LOG("New format. rate: %d, channels: %d, format: %d", rate, channels, encoding);

            if (encoding != MPG123_ENC_SIGNED_16 && encoding != MPG123_ENC_SIGNED_8) {
                LOGE("Unsupported format!!!");
                return 0;
            }
        }
        return (jint)mh;
    } else {
        return 0;
    }
}

void JNI_FUNCTION(seek) (JNIEnv* env, jobject obj, jint handle, jint offset) {
    mpg123_handle* mh = (mpg123_handle*) handle;
    int ret = mpg123_seek(mh, offset, SEEK_SET);

    if (ret < 0) {
        LOGE("Error during seek to sample %d: %s", offset, mpg123_plain_strerror(ret));
    } else if (ret != offset) {
        LOGE("mpg123_seek returned invalid offset, expected %d, got %d", offset, ret);
    }
}

jint JNI_FUNCTION(decode) (JNIEnv* env, jobject obj, jint handle, jbyteArray buffer, jint size) {
    mpg123_handle* mh = (mpg123_handle*) handle;
    jbyte* target = (jbyte*)(*env)->GetByteArrayElements(env, buffer, 0);

    size_t done;
    int ret = mpg123_decode(mh, NULL, 0, target, (size_t)size, &done);
    //LOG("Decoded %d bytes, ret: %d, size: %d", done, ret, size);

    (*env)->ReleaseByteArrayElements(env, buffer, target, 0);
    if (ret != MPG123_OK && ret != MPG123_DONE) {
        LOGE("mpg123 error: %s", mpg123_plain_strerror(ret));
        return 0;
    } else {
        return done;
    }
}

void JNI_FUNCTION(close) (JNIEnv* env, jobject obj, jint handle) {
    mpg123_handle* mh = (mpg123_handle*) handle;

    mpg123_close(mh);
}
