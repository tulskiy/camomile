#include <string.h>
#include <jni.h>
#include "mpg123.h"
#include <android/log.h>

#define LOG(x...) __android_log_print(ANDROID_LOG_DEBUG, "sloth", x)

#define JNI_FUNCTION(name) Java_com_tulskiy_sloth_Mpg123Decoder_ ## name

jint JNI_FUNCTION(open) (JNIEnv* env, jobject obj, jstring file) {
    mpg123_init();

    mpg123_handle* mh = mpg123_new(NULL, NULL);

    const char* fileName = (*env)->GetStringUTFChars(env, file, NULL);
    int ret = mpg123_open(mh, fileName);
    (*env)->ReleaseStringUTFChars(env, file, fileName);

    LOG("Opening file %s. Result: %d", fileName, ret);

    // decode a bit to get new format
    off_t framenum;
    unsigned char audio[1];
    size_t bytes;
    ret = mpg123_decode(mh, NULL, 0, audio, 1, &bytes);

    if(ret == MPG123_NEW_FORMAT) {
	    long rate;
		int channels, format;
		mpg123_getformat(mh, &rate, &channels, &format);

		LOG("%d, %d, %d", rate, channels, format);
	}

    return (jint)mh;
}

jint JNI_FUNCTION(decode) (JNIEnv* env, jobject obj, jint handle, jbyteArray buffer, jint size) {
    mpg123_handle* mh = (mpg123_handle*) handle;
    jbyte* target = (jbyte*)(*env)->GetByteArrayElements(env, buffer, 0);

    size_t done;
    int ret = mpg123_decode(mh, NULL, 0, target, (size_t)size, &done);
    //LOGE("Decoded %d bytes, ret: %d, size: %d", done, ret, size);

    (*env)->ReleaseByteArrayElements(env, buffer, target, 0);
    if (ret != MPG123_OK && ret != MPG123_DONE)
        return 0;
    else
        return done;
}
