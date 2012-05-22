#include <string.h>
#include <jni.h>
#include <stdio.h>
#include <android/log.h>
#include <stdbool.h>
#include "src/decoder.h"

#define LOG(x...) __android_log_print(ANDROID_LOG_DEBUG, "camomile", x)
#define LOGE(x...) __android_log_print(ANDROID_LOG_ERROR, "camomile", x)

#define JNI_FUNCTION(name) Java_com_tulskiy_camomile_audio_formats_flac_FLACDecoder_ ## name

#define MAX_SUPPORTED_SEEKTABLE_SIZE 5000

typedef struct FLACseekpoints {
    uint32_t sample;
    uint32_t offset;
    uint16_t blocksize;
} FLACseekpoints;

typedef struct ContextHolder {
    FLACContext* fc;
    FLACseekpoints seekpoints[MAX_SUPPORTED_SEEKTABLE_SIZE];
    int nseekpoints;
    int32_t decoded0[MAX_BLOCKSIZE];
    int32_t decoded1[MAX_BLOCKSIZE];
    int8_t read_buf[MAX_FRAMESIZE];
    FILE* input;
    int bytesleft;
    int write_pos;
    int frame;
} ContextHolder;

bool flac_init(ContextHolder* context);

jint JNI_FUNCTION(open) (JNIEnv* env, jobject obj, jstring file, jintArray formatArray) {
    ContextHolder* context = malloc(sizeof(ContextHolder));
    memset(context, 0, sizeof(ContextHolder));

    FLACContext* fc = malloc(sizeof(FLACContext));
    context->fc = fc;

    const char* fileName = (*env)->GetStringUTFChars(env, file, NULL);
    context->input = fopen(fileName, "r");
    (*env)->ReleaseStringUTFChars(env, file, fileName);

    if (!context->input) {
        LOGE("could not open file");
        return 0;
    }

    if (!flac_init(context)) {
        LOGE("flac_init returned false");
        return 0;
    }

    jint* format = (jint*)(*env)->GetIntArrayElements(env, formatArray, 0);
    format[0] = fc->samplerate;
    format[1] = fc->channels;
    format[2] = fc->bps;
    (*env)->ReleaseIntArrayElements(env, formatArray, format, 0);

    return (jint) context;
}

void JNI_FUNCTION(seek) (JNIEnv* env, jobject obj, jint handle, jint offset) {
    ContextHolder* context = (ContextHolder*) handle;
}

void yield() {}

jint JNI_FUNCTION(decode) (JNIEnv* env, jobject obj, jint handle, jbyteArray buffer, jint size) {
    ContextHolder* context = (ContextHolder*) handle;
    jshort* target = (jshort*)(*env)->GetByteArrayElements(env, buffer, 0);

    int length = fread(&context->read_buf[context->bytesleft], 1, MAX_FRAMESIZE - context->bytesleft, context->input);
    context->bytesleft += length;
    LOG("read %d bytes", length);
    if (context->bytesleft <= 0)
        return -1;

    int ret = flac_decode_frame(context->fc, context->decoded0, context->decoded1, context->read_buf, context->bytesleft, yield);
    if (ret < 0) {
        LOG("flac decode returned %d", ret);
        return -1;
    }
    LOG("Frame: %d, blocksize %d", context->frame++, context->fc->blocksize);
//    int sampleShift = FLAC_OUTPUT_DEPTH-context->fc->bps;
    int pos = 0;
    if(context->fc->channels == 2) {
        for(int sample = context->fc->sample_skip; sample < context->fc->blocksize; sample++) {
            int32_t tmp = context->decoded0[sample];
    		target[pos++] = tmp & 0xFFFF;

    		tmp = context->decoded1[sample];
    		target[pos++] = tmp & 0xFFFF;
    	}
    } /*else if(channels == 1) {
    	for(sample = wide_sample = 0; wide_sample < wide_samples; wide_sample++)
    	    target[sample++] = (FLAC__uint16)(buffer[0][wide_sample] + 0x8000);
    }*/

    int consumed = context->fc->gb.index/8;
    LOG("consumed %d bytes", consumed);
    context->write_pos = consumed;
    context->bytesleft -= consumed;
    memmove(context->read_buf, &context->read_buf[consumed], context->bytesleft);

    context->fc->sample_skip = 0;

    (*env)->ReleaseByteArrayElements(env, buffer, (jbyte*)target, 0);
    return pos * 2;
}

void JNI_FUNCTION(close) (JNIEnv* env, jobject obj, jint handle) {
    ContextHolder* context = (ContextHolder*) handle;
    free(context->fc);
    free(context);
}

bool flac_init(ContextHolder* context) {
    unsigned char buf[255];
    bool found_streaminfo = false;
    uint32_t seekpoint_hi,seekpoint_lo;
    uint32_t offset_hi,offset_lo;
    uint16_t blocksize;
    int endofmetadata = 0;
    uint32_t blocklength;
    FLACContext* fc = context->fc;
    FILE* input = context->input;

    memset(fc, 0, sizeof(FLACContext));

    fc->sample_skip = 0;

    if (fread(buf, 1, 4, input) < 4) {
        return false;
    }

    if (memcmp(buf, "fLaC", 4) != 0) {
        LOGE("Not a valid FLAC file!");
        return false;
    }
    fc->metadatalength += 4;
    while (!endofmetadata) {
        if (fread(buf, 1, 4, input) < 4) {
            return false;
        }

        endofmetadata = (buf[0]&0x80);
        blocklength = (buf[1] << 16) | (buf[2] << 8) | buf[3];
        fc->metadatalength += blocklength+4;
        LOG("found metadata block size: %d", blocklength, endofmetadata);

        if ((buf[0] & 0x7f) == 0) { /* 0 is the STREAMINFO block */
            LOG("found streaminfo block");
            if (fread(buf, 1, blocklength, input) < blocklength) return false;

            fc->min_blocksize = (buf[0] << 8) | buf[1];
            int max_blocksize = (buf[2] << 8) | buf[3];
            if (max_blocksize > MAX_BLOCKSIZE)
            {
                LOGE("FLAC: Maximum blocksize is too large (%d > %d)\n",
                     max_blocksize, MAX_BLOCKSIZE);
                return false;
            }
            fc->max_blocksize = max_blocksize;
            fc->min_framesize = (buf[4] << 16) | (buf[5] << 8) | buf[6];
            fc->max_framesize = (buf[7] << 16) | (buf[8] << 8) | buf[9];
            fc->samplerate = (buf[10] << 12) | (buf[11] << 4)
                             | ((buf[12] & 0xf0) >> 4);
            fc->channels = ((buf[12]&0x0e)>>1) + 1;
            fc->bps = (((buf[12]&0x01) << 4) | ((buf[13]&0xf0)>>4) ) + 1;

            /* totalsamples is a 36-bit field, but we assume <= 32 bits are
               used */
            fc->totalsamples = (buf[14] << 24) | (buf[15] << 16)
                               | (buf[16] << 8) | buf[17];

            /* Calculate track length (in ms) and estimate the bitrate
               (in kbit/s) */
            fc->length = ((int64_t) fc->totalsamples * 1000) / fc->samplerate;

            found_streaminfo=true;
        } else if ((buf[0] & 0x7f) == 3) { /* 3 is the SEEKTABLE block */
            LOG("found SEEKTABLE block");

            while ((context->nseekpoints < MAX_SUPPORTED_SEEKTABLE_SIZE) &&
                   (blocklength >= 18)) {
                if (fread(buf, 1, 18, input) < 18) return false;
                blocklength -= 18;

                seekpoint_hi = (buf[0] << 24) | (buf[1] << 16) |
                               (buf[2] << 8) | buf[3];
                seekpoint_lo = (buf[4] << 24) | (buf[5] << 16) |
                               (buf[6] << 8) | buf[7];
                offset_hi = (buf[8] << 24) | (buf[9] << 16) |
                            (buf[10] << 8) | buf[11];
                offset_lo = (buf[12] << 24) | (buf[13] << 16) |
                            (buf[14] << 8) | buf[15];

                blocksize = (buf[16] << 8) | buf[17];

                /* Only store seekpoints where the high 32 bits are zero */
                if ((seekpoint_hi == 0) && (seekpoint_lo != 0xffffffff) &&
                    (offset_hi == 0)) {
                        context->seekpoints[context->nseekpoints].sample=seekpoint_lo;
                        context->seekpoints[context->nseekpoints].offset=offset_lo;
                        context->seekpoints[context->nseekpoints].blocksize=blocksize;
                        context->nseekpoints++;
                }
            }
            LOG("found %d seekpoints", context->nseekpoints);
            /* Skip any unread seekpoints */
            if (blocklength > 0)
                fseek(input, blocklength, SEEK_CUR);
        } else {
            /* Skip to next metadata block */
            fseek(input, blocklength, SEEK_CUR);
        }
    }

   if (found_streaminfo) {
//       fseek(input, 0, SEEK_END);
//       fc->filesize = ftell(input);
//       fseek(input, fc->metadatalength, SEEK_SET);

//       fc->bitrate = ((int64_t) (fc->filesize - fc->metadatalength) * 8)
//                     / fc->length;
       return true;
   } else {
       return false;
   }
}