/*
  parts of this code are adapted from rockbox's flac.c decoder
*/

/***************************************************************************
 *             __________               __   ___.
 *   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
 *   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
 *   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
 *   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
 *                     \/            \/     \/    \/            \/
 * $Id$
 *
 * Copyright (C) 2005 Dave Chapman
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY
 * KIND, either express or implied.
 *
 ****************************************************************************/

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
    int frame;
} ContextHolder;

bool flac_init(ContextHolder* ctx);
void advance_buffer(ContextHolder* ctx, int amount);
void fill_buffer(ContextHolder* ctx);
bool flac_seek(ContextHolder* ctx, uint32_t target_sample);

jint JNI_FUNCTION(open) (JNIEnv* env, jobject obj, jstring file, jintArray formatArray) {
    ContextHolder* ctx = malloc(sizeof(ContextHolder));
    memset(ctx, 0, sizeof(ContextHolder));

    FLACContext* fc = malloc(sizeof(FLACContext));
    ctx->fc = fc;

    const char* fileName = (*env)->GetStringUTFChars(env, file, NULL);
    ctx->input = fopen(fileName, "r");
    (*env)->ReleaseStringUTFChars(env, file, fileName);

    if (!ctx->input) {
        LOGE("could not open file");
        return 0;
    }

    if (!flac_init(ctx)) {
        LOGE("flac_init returned false");
        return 0;
    }

    jint* format = (jint*)(*env)->GetIntArrayElements(env, formatArray, 0);
    format[0] = fc->samplerate;
    format[1] = fc->channels;
    format[2] = fc->bps;
    (*env)->ReleaseIntArrayElements(env, formatArray, format, 0);

    if (fc->channels > 2 || (fc->bps != 16 && fc->bps != 8)) {
        LOGE("unsupported format: channels: %d, bps: %d!", fc->channels, fc->bps);
        return 0;
    }

    return (jint) ctx;
}

void JNI_FUNCTION(seek) (JNIEnv* env, jobject obj, jint handle, jint offset) {
    ContextHolder* ctx = (ContextHolder*) handle;

    flac_seek(ctx, offset);
}

void yield() {}

jint JNI_FUNCTION(decode) (JNIEnv* env, jobject obj, jint handle, jbyteArray buffer, jint size) {
    ContextHolder* ctx = (ContextHolder*) handle;
    jbyte* target = (jbyte*)(*env)->GetByteArrayElements(env, buffer, 0);
    jshort* target_16b = (jshort*) target;

    fill_buffer(ctx);
    if (ctx->bytesleft <= 0)
        return -1;

    int ret = flac_decode_frame(ctx->fc, ctx->decoded0, ctx->decoded1, ctx->read_buf, ctx->bytesleft, yield);
    if (ret < 0) {
        LOGE("flac decode returned %d", ret);
        return -1;
    }
//    LOG("Frame: %d, blocksize %d", ctx->frame++, ctx->fc->blocksize);
//    LOG("sample: %d, totalsamples: %d", ctx->fc->samplenumber, ctx->fc->totalsamples);
    int pos = 0;
    if (ctx->fc->bps == 16) {
        for (int sample = ctx->fc->sample_skip; sample < ctx->fc->blocksize; sample++) {
            target_16b[pos++] = ctx->decoded0[sample] + 0x80;

            if (ctx->fc->channels == 2) {
                target_16b[pos++] = ctx->decoded1[sample] + 0x80;
            }
        }
        pos *= 2;
    } else if (ctx->fc->bps == 8) {
        for (int sample = ctx->fc->sample_skip; sample < ctx->fc->blocksize; sample++) {
            target[pos++] = ctx->decoded0[sample] + 0x80;

            if (ctx->fc->channels == 2) {
                target[pos++] = ctx->decoded1[sample] + 0x80;
            }
        }
    }

    int consumed = ctx->fc->gb.index/8;
//    LOG("consumed %d bytes", consumed);
    ctx->bytesleft -= consumed;
    advance_buffer(ctx, consumed);

    ctx->fc->sample_skip = 0;

    (*env)->ReleaseByteArrayElements(env, buffer, (jbyte*)target, 0);
    return pos;
}

void JNI_FUNCTION(close) (JNIEnv* env, jobject obj, jint handle) {
    ContextHolder* ctx = (ContextHolder*) handle;
    free(ctx->fc);
    free(ctx);
}

bool flac_init(ContextHolder* ctx) {
    unsigned char buf[255];
    bool found_streaminfo = false;
    uint32_t seekpoint_hi,seekpoint_lo;
    uint32_t offset_hi,offset_lo;
    uint16_t blocksize;
    int endofmetadata = 0;
    uint32_t blocklength;
    FLACContext* fc = ctx->fc;
    FILE* input = ctx->input;

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
//        LOG("found metadata block size: %d", blocklength, endofmetadata);

        if ((buf[0] & 0x7f) == 0) { /* 0 is the STREAMINFO block */
//            LOG("found streaminfo block");
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
//            LOG("found SEEKTABLE block");

            while ((ctx->nseekpoints < MAX_SUPPORTED_SEEKTABLE_SIZE) &&
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
                        ctx->seekpoints[ctx->nseekpoints].sample=seekpoint_lo;
                        ctx->seekpoints[ctx->nseekpoints].offset=offset_lo;
                        ctx->seekpoints[ctx->nseekpoints].blocksize=blocksize;
                        ctx->nseekpoints++;
                }
            }
//            LOG("found %d seekpoints", ctx->nseekpoints);
            /* Skip any unread seekpoints */
            if (blocklength > 0)
                fseek(input, blocklength, SEEK_CUR);
        } else {
            /* Skip to next metadata block */
            fseek(input, blocklength, SEEK_CUR);
        }
    }

   if (found_streaminfo) {
       fseek(input, 0, SEEK_END);
       fc->filesize = ftell(input);
       fseek(input, fc->metadatalength, SEEK_SET);

       fc->bitrate = ((int64_t) (fc->filesize - fc->metadatalength) * 8)
                     / fc->length;
       return true;
   } else {
       return false;
   }
}

void advance_buffer(ContextHolder* ctx, int amount) {
    memmove(ctx->read_buf, &ctx->read_buf[amount], ctx->bytesleft);
}

void fill_buffer(ContextHolder* ctx) {
    ctx->bytesleft += fread(&ctx->read_buf[ctx->bytesleft], 1, MAX_FRAMESIZE - ctx->bytesleft, ctx->input);
    //    LOG("read %d bytes, bytesleft: %d", length, ctx->bytesleft);
}

/* Synchronize to next frame in stream - adapted from libFLAC 1.1.3b2 */
bool frame_sync(ContextHolder* ctx) {
    FLACContext* fc = ctx->fc;
    unsigned int x = 0;
    bool cached = false;

    /* Make sure we're byte aligned. */
    align_get_bits(&fc->gb);

    while(1) {
        if(fc->gb.size_in_bits - get_bits_count(&fc->gb) < 8) {
            /* Error, end of bitstream, a valid stream should never reach here
             * since the buffer should contain at least one frame header.
             */
            return false;
        }

        if(cached)
            cached = false;
        else
            x = get_bits(&fc->gb, 8);

        if(x == 0xff) { /* MAGIC NUMBER for first 8 frame sync bits. */
            x = get_bits(&fc->gb, 8);
            /* We have to check if we just read two 0xff's in a row; the second
             * may actually be the beginning of the sync code.
             */
            if(x == 0xff) { /* MAGIC NUMBER for first 8 frame sync bits. */
                cached = true;
            }
            else if(x >> 2 == 0x3e) { /* MAGIC NUMBER for last 6 sync bits. */
                /* Succesfully synced. */
                break;
            }
        }
    }

    /* Advance and init bit buffer to the new frame. */
    advance_buffer(ctx, (get_bits_count(&fc->gb)-16)>>3); /* consumed bytes */
    fill_buffer(ctx);
    init_get_bits(&fc->gb, ctx->read_buf, ctx->bytesleft*8);

    /* Decode the frame to verify the frame crc and
     * fill fc with its metadata.
     */
    if(flac_decode_frame(fc, ctx->decoded0, ctx->decoded1,
       ctx->read_buf, ctx->bytesleft, yield) < 0) {
        return false;
    }

    return true;
}

/* Seek to sample - adapted from libFLAC 1.1.3b2+ */
bool flac_seek(ContextHolder* ctx, uint32_t target_sample) {
    FLACContext* fc = ctx->fc;
    off_t orig_pos = ftell(ctx->input);
    off_t pos = -1;
    unsigned long lower_bound, upper_bound;
    unsigned long lower_bound_sample, upper_bound_sample;
    int i;
    unsigned approx_bytes_per_frame;
    uint32_t this_frame_sample = fc->samplenumber;
    unsigned this_block_size = fc->blocksize;
    bool needs_seek = true, first_seek = true;

    /* We are just guessing here. */
    if(fc->max_framesize > 0)
        approx_bytes_per_frame = (fc->max_framesize + fc->min_framesize)/2 + 1;
    /* Check if it's a known fixed-blocksize stream. */
    else if(fc->min_blocksize == fc->max_blocksize && fc->min_blocksize > 0)
        approx_bytes_per_frame = fc->min_blocksize*fc->channels*fc->bps/8 + 64;
    else
        approx_bytes_per_frame = 4608 * fc->channels * fc->bps/8 + 64;

    /* Set an upper and lower bound on where in the stream we will search. */
    lower_bound = fc->metadatalength;
    lower_bound_sample = 0;
    upper_bound = fc->filesize;
    upper_bound_sample = fc->totalsamples>0 ? fc->totalsamples : target_sample;

    /* Refine the bounds if we have a seektable with suitable points. */
    if(ctx->nseekpoints > 0) {
        /* Find the closest seek point <= target_sample, if it exists. */
        for(i = ctx->nseekpoints-1; i >= 0; i--) {
            if(ctx->seekpoints[i].sample <= target_sample)
                break;
        }
        if(i >= 0) { /* i.e. we found a suitable seek point... */
            lower_bound = fc->metadatalength + ctx->seekpoints[i].offset;
            lower_bound_sample = ctx->seekpoints[i].sample;
        }

        /* Find the closest seek point > target_sample, if it exists. */
        for(i = 0; i < ctx->nseekpoints; i++) {
            if(ctx->seekpoints[i].sample > target_sample)
                break;
        }
        if(i < ctx->nseekpoints) { /* i.e. we found a suitable seek point... */
            upper_bound = fc->metadatalength + ctx->seekpoints[i].offset;
            upper_bound_sample = ctx->seekpoints[i].sample;
        }
    }

    while(1) {
        /* Check if bounds are still ok. */
        if(lower_bound_sample >= upper_bound_sample ||
           lower_bound > upper_bound) {
            return false;
        }

        /* Calculate new seek position */
        if(needs_seek) {
            pos = (off_t)(lower_bound +
              (((target_sample - lower_bound_sample) *
              (int64_t)(upper_bound - lower_bound)) /
              (upper_bound_sample - lower_bound_sample)) -
              approx_bytes_per_frame);

            if(pos >= (off_t)upper_bound)
                pos = (off_t)upper_bound-1;
            if(pos < (off_t)lower_bound)
                pos = (off_t)lower_bound;
        }

        if(fseek(ctx->input, pos, SEEK_SET))
            return false;

        fill_buffer(ctx);
        init_get_bits(&fc->gb, ctx->read_buf, ctx->bytesleft*8);

        /* Now we need to get a frame.  It is possible for our seek
         * to land in the middle of audio data that looks exactly like
         * a frame header from a future version of an encoder.  When
         * that happens, frame_sync() will return false.
         * But there is a remote possibility that it is properly
         * synced at such a "future-codec frame", so to make sure,
         * we wait to see several "unparseable" errors in a row before
         * bailing out.
         */
        {
            unsigned unparseable_count;
            bool got_a_frame = false;
            for(unparseable_count = 0; !got_a_frame
                && unparseable_count < 10; unparseable_count++) {
                if(frame_sync(ctx))
                    got_a_frame = true;
            }
            if(!got_a_frame) {
                fseek(ctx->input, orig_pos, SEEK_SET);
                return false;
            }
        }

        this_frame_sample = fc->samplenumber;
        this_block_size = fc->blocksize;

        if(target_sample >= this_frame_sample
           && target_sample < this_frame_sample+this_block_size) {
            /* Found the frame containing the target sample. */
            fc->sample_skip = target_sample - this_frame_sample;
            break;
        }

        if(this_frame_sample + this_block_size >= upper_bound_sample &&
           !first_seek) {
            if(pos == (off_t)lower_bound || !needs_seek) {
                fseek(ctx->input, orig_pos, SEEK_SET);
                return false;
            }
            /* Our last move backwards wasn't big enough, try again. */
            approx_bytes_per_frame *= 2;
            continue;
        }
        /* Allow one seek over upper bound,
         * required for streams with unknown total samples.
         */
        first_seek = false;

        /* Make sure we are not seeking in a corrupted stream */
        if(this_frame_sample < lower_bound_sample) {
            fseek(ctx->input, orig_pos, SEEK_SET);
            return false;
        }

        approx_bytes_per_frame = this_block_size*fc->channels*fc->bps/8 + 64;

        /* We need to narrow the search. */
        if(target_sample < this_frame_sample) {
            upper_bound_sample = this_frame_sample;
            upper_bound = ftell(ctx->input);
        }
        else { /* Target is beyond this frame. */
            /* We are close, continue in decoding next frames. */
            if(target_sample < this_frame_sample + 4*this_block_size) {
                pos = ftell(ctx->input) + fc->framesize;
                needs_seek = false;
            }

            lower_bound_sample = this_frame_sample + this_block_size;
            lower_bound = ftell(ctx->input) + fc->framesize;
        }
    }

    return true;
}
