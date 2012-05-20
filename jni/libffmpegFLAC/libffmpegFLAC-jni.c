#include <string.h>
#include <jni.h>
#include <stdio.h>
#include <android/log.h>
#include "src/decoder.h"

#define LOG(x...) __android_log_print(ANDROID_LOG_DEBUG, "camomile", x)
#define LOGE(x...) __android_log_print(ANDROID_LOG_ERROR, "camomile", x)

#define JNI_FUNCTION(name) Java_com_tulskiy_camomile_audio_formats_flac_FLACDecoder_ ## name

int parceFLACmetadata(FILE* input, FLACContext* context);

jint JNI_FUNCTION(open) (JNIEnv* env, jobject obj, jstring file, jintArray formatArray) {
    FLACContext* context = malloc(sizeof(FLACContext));

    const char* fileName = (*env)->GetStringUTFChars(env, file, NULL);
    FILE* input = fopen(fileName, "r");
    (*env)->ReleaseStringUTFChars(env, file, fileName);

    if (!input) {
        LOGE("could not open file");
        return 0;
    }

    parceFLACmetadata(input, context);

    jint* format = (jint*)(*env)->GetIntArrayElements(env, formatArray, 0);
    format[0] = context->samplerate;
    format[1] = context->channels;
    format[2] = context->bps;
    (*env)->ReleaseIntArrayElements(env, formatArray, format, 0);

    return (jint) context;
}

void JNI_FUNCTION(seek) (JNIEnv* env, jobject obj, jint handle, jint offset) {
    FLACContext* context = (FLACContext*) handle;
}

jint JNI_FUNCTION(decode) (JNIEnv* env, jobject obj, jint handle, jbyteArray buffer, jint size) {
    FLACContext* context = (FLACContext*) handle;
    jbyte* target = (jbyte*)(*env)->GetByteArrayElements(env, buffer, 0);



    (*env)->ReleaseByteArrayElements(env, buffer, target, 0);
    return 0;
}

void JNI_FUNCTION(close) (JNIEnv* env, jobject obj, jint handle) {
    FLACContext* context = (FLACContext*) handle;
    free(context);
}

int parceFLACmetadata(FILE* FLACfile, FLACContext* context)
{
	uint s1 = 0;
	int metaDataFlag = 1;
	char metaDataChunk[128];
	unsigned long metaDataBlockLength = 0;
	char* tagContents;

	s1 = fread(metaDataChunk, 1, 4, FLACfile);

	if(s1 != 4)
	{
		printf("Read failure\n");
		fclose(FLACfile);
		return 1;
	}

	if(memcmp(metaDataChunk, "fLaC", 4) != 0)
	{
		printf("Not a FLAC file\n");
		fclose(FLACfile);
		return 1;
	}

	// Now we are at the stream block
	// Each block has metadata header of 4 bytes
	do
	{
		s1 = fread(metaDataChunk, 1, 4, FLACfile);

		if(s1 != 4)
		{
			printf("Read failure\n");
			fclose(FLACfile);
			return 1;
		}

		//Check if last chunk
		if(metaDataChunk[0] & 0x80) metaDataFlag = 0;

		metaDataBlockLength = (metaDataChunk[1] << 16) | (metaDataChunk[2] << 8) | metaDataChunk[3];

		//STREAMINFO block
		if((metaDataChunk[0] & 0x7F) == 0)
		{

			if(metaDataBlockLength > 128)
			{
				printf("Metadata buffer too small\n");
				fclose(FLACfile);
				return 1;
			}

			s1 = fread(metaDataChunk, 1, metaDataBlockLength, FLACfile);

			if(s1 != metaDataBlockLength)
			{
				printf("Read failure\n");
				fclose(FLACfile);
				return 1;
			}
			/*
			<bits> Field in STEAMINFO
			<16> min block size (samples)
			<16> max block size (samples)
			<24> min frams size (bytes)
			<24> max frams size (bytes)
			<20> Sample rate (Hz)
			<3> (number of channels)-1
			<5> (bits per sample)-1.
			<36> Total samples in stream.
			<128> MD5 signature of the unencoded audio data.
			*/

			context->min_blocksize = (metaDataChunk[0] << 8) | metaDataChunk[1];
			context->max_blocksize = (metaDataChunk[2] << 8) | metaDataChunk[3];
			context->min_framesize = (metaDataChunk[4] << 16) | (metaDataChunk[5] << 8) | metaDataChunk[6];
			context->max_framesize = (metaDataChunk[7] << 16) | (metaDataChunk[8] << 8) | metaDataChunk[9];
			context->samplerate = (metaDataChunk[10] << 12) | (metaDataChunk[11] << 4) | ((metaDataChunk[12] & 0xf0) >> 4);
			context->channels = ((metaDataChunk[12] & 0x0e) >> 1) + 1;
			context->bps = (((metaDataChunk[12] & 0x01) << 4) | ((metaDataChunk[13] & 0xf0)>>4) ) + 1;

			//This field in FLAC context is limited to 32-bits
			context->totalsamples = (metaDataChunk[14] << 24) | (metaDataChunk[15] << 16) | (metaDataChunk[16] << 8) | metaDataChunk[17];
		} else {
          			if(!fseek(FLACfile, SEEK_CUR, metaDataBlockLength))
          			{
          				printf("File Seek Failed\n");
          				fclose(FLACfile);
          				return 1;
          			}
          		}
	} while(metaDataFlag);


	// track length in ms
	context->length = (context->totalsamples / context->samplerate) * 1000;
	// file size in bytes
	context->filesize = 1000;
	// current offset is end of metadata in bytes
	context->metadatalength = ftell(FLACfile);
	// bitrate of file
	context->bitrate = ((context->filesize - context->metadatalength) * 8) / context->length;

	fclose(FLACfile);
	return 0;

}



//Just a dummy function for the flac_decode_frame
void yield()
{
	//Do nothing
}