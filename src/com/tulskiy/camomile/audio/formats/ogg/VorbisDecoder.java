package com.tulskiy.camomile.audio.formats.ogg;

import com.tulskiy.camomile.audio.AudioFormat;
import com.tulskiy.camomile.audio.Decoder;

import java.io.File;

/**
 * Author: Denis Tulskiy
 * Date: 1/22/12
 */
public class VorbisDecoder implements Decoder {

    private AudioFormat audioFormat;
    private int handle;

    public boolean open(File file) {
        int[] format = new int[3];
        handle = open(file.getAbsolutePath(), format);
        audioFormat = new AudioFormat(format[0], format[1], format[2]);

        return handle != 0;
    }

    public AudioFormat getAudioFormat() {
        return audioFormat;
    }

    public int decode(byte[] buffer) {
        return decode(handle, buffer, buffer.length);
    }

    public void close() {

    }

    public void seek(int sample) {
        seek(handle, sample);
    }

    private native int open(String fileName, int[] format);

    private native int seek(int handle, int offset);

    private native int decode(int handle, byte[] buffer, int size);

    private native int close(int handle);

    static {
        System.loadLibrary("tremolo-jni");
    }
}
