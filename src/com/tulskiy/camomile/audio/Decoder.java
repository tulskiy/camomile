package com.tulskiy.camomile.audio;

import java.io.File;

/**
 * Author: Denis_Tulskiy
 * Date: 1/10/12
 */
public interface Decoder {

    boolean open(File file);

    AudioFormat getAudioFormat();

    int decode(byte[] buffer);

    void close();

    void seek(int sample);
}
