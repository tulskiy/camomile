package com.tulskiy.camomile.audio.formats.aac;

import com.tulskiy.camomile.audio.AudioFormat;
import com.tulskiy.camomile.audio.Decoder;
import com.tulskiy.camomile.audio.formats.ffmpeg.FFMPEGDecoder;

import java.io.File;

/**
 * Author: Denis_Tulskiy
 * Date: 5/30/12
 */
public class MP4Decoder implements Decoder {
    /**
     * frame size in samples
     */
    private static final int FRAME_SIZE = 1024;

    private FFMPEGDecoder ffmpegDecoder = new FFMPEGDecoder();

    private int frame;
    private int skipSamples;

    public boolean open(File file) {
        boolean ret = ffmpegDecoder.open(file);

        init(0);

        return ret;
    }

    private void init(int sample) {
        sample += ffmpegDecoder.getEncDelay();
    }

    public AudioFormat getAudioFormat() {
        return ffmpegDecoder.getAudioFormat();
    }

    public int decode(byte[] buffer) {
        int length = ffmpegDecoder.decode(buffer);
        frame++;
        return length;
    }

    public void close() {
        ffmpegDecoder.close();
    }

    public void seek(int sample) {
        ffmpegDecoder.seek(sample);
    }
}
