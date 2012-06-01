package com.tulskiy.camomile.audio.formats.ffmpeg;

import com.tulskiy.camomile.audio.AudioFormat;
import com.tulskiy.camomile.audio.Decoder;

import java.io.File;

/**
 * Author: Denis Tulskiy
 * Date: 5/20/12
 */
public class FFMPEGDecoder implements Decoder {
    private int handle;
    private AudioFormat audioFormat;
    private File file;

    private int encDelay;
    private int encPadding;

    public boolean open(File file) {
        this.file = file;
        int[] format = new int[5];
        handle = open(file.getAbsolutePath(), format);
        audioFormat = new AudioFormat(format[0], format[1], format[2]);
        encDelay = format[3];
        encPadding = format[4];
        return handle != 0;
    }

    public AudioFormat getAudioFormat() {
        return audioFormat;
    }

    public int decode(byte[] buffer) {
        return decode(handle, buffer, buffer.length);
    }

    public void close() {
        close(handle);
    }

    public void seek(int sample) {
        seek(handle, sample);
    }

    public int getEncDelay() {
        return encDelay;
    }

    public int getEncPadding() {
        return encPadding;
    }

    private native int open(String fileName, int[] format);

    private native int seek(int handle, int offset);

    private native int decode(int handle, byte[] buffer, int size);

    private native int close(int handle);

    static {
        System.loadLibrary("ffmpeg-jni");
    }
}
