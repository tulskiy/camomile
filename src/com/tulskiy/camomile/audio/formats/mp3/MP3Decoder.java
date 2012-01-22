package com.tulskiy.camomile.audio.formats.mp3;

import com.tulskiy.camomile.audio.Decoder;
import com.tulskiy.camomile.audio.AudioFormat;

import java.io.File;

/**
 * Author: Denis_Tulskiy
 * Date: 1/10/12
 */
public class MP3Decoder implements Decoder {
    private int handle;
    private AudioFormat audioFormat;
    private static final int MPG123_ENCODING_SIGNED_16 = 0xD0;

    public boolean open(File file) {
        int[] format = new int[3];
        handle = open(file.getAbsolutePath(), format);
        audioFormat = new AudioFormat(
                format[0], // sample rate
                format[1], // channels
                format[2] == MPG123_ENCODING_SIGNED_16 ? 16 : 8
        );
        return handle != 0;
    }

    public AudioFormat getAudioFormat() {
        return audioFormat;
    }

    public int decode(byte[] buffer) {
        int length = decode(handle, buffer, buffer.length);
        return length > 0 ? length : -1; 
    }

    public void seek(int sample) {
        seek(handle, sample);
    }
    
    public void close() {
        close(handle);
    }
    
    private native int open(String fileName, int[] format);
    
    private native int decode(int handle, byte[] buffer, int size);
    
    private native int seek(int handle, int offset);
    
    private native int close(int handle);
    
    static {
        System.loadLibrary("mpg123-jni");
    }
}
