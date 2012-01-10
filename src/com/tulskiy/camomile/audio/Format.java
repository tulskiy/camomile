package com.tulskiy.camomile.audio;

/**
 * Author: Denis_Tulskiy
 * Date: 1/10/12
 */
public class Format {
    private int rate;
    private int channels;
    private int encoding;

    public Format(int rate, int channels, int encoding) {
        this.rate = rate;
        this.channels = channels;
        this.encoding = encoding;
    }

    public int getRate() {
        return rate;
    }

    public void setRate(int rate) {
        this.rate = rate;
    }

    public int getChannels() {
        return channels;
    }

    public void setChannels(int channels) {
        this.channels = channels;
    }

    public int getEncoding() {
        return encoding;
    }

    public void setEncoding(int encoding) {
        this.encoding = encoding;
    }
}
