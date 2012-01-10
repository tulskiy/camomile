package com.tulskiy.camomile.audio.formats.mp3;

import android.media.AudioFormat;
import com.tulskiy.camomile.audio.Decoder;
import com.tulskiy.camomile.audio.Format;

import java.io.File;

/**
 * Author: Denis_Tulskiy
 * Date: 1/10/12
 */
public class MP3Decoder implements Decoder {
    private int handle;
    private Format audioFormat;
    private static final int MPG123_ENCODING_SIGNED_16 = 0xD0;

    public boolean open(File file) {
        int[] format = new int[3];
        handle = open(file.getAbsolutePath(), format);
        audioFormat = new Format(
                format[0], // rate
                format[1] == 2 ? AudioFormat.CHANNEL_CONFIGURATION_STEREO : AudioFormat.CHANNEL_CONFIGURATION_MONO,
                format[2] == MPG123_ENCODING_SIGNED_16 ? AudioFormat.ENCODING_PCM_16BIT : AudioFormat.ENCODING_PCM_8BIT
        );
        return handle != 0;
    }

    public Format getAudioFormat() {
        return audioFormat;
    }

    public int decode(byte[] buffer) {
        int length = decode(handle, buffer, buffer.length);
        return length > 0 ? length : -1; 
    }
    
    public void close() {
        close(handle);
    }
    
    public native int open(String fileName, int[] format);
    
    public native int decode(int handle, byte[] buffer, int size);
    
    public native int close(int handle);
    
    static {
        System.loadLibrary("mpg123-jni");
    }
}
