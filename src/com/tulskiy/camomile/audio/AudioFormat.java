package com.tulskiy.camomile.audio;

/**
 * Author: Denis_Tulskiy
 * Date: 1/10/12
 */
public class AudioFormat {
    private int sampleRate;
    private int channels;
    private int sampleSizeInBits;
    private int frameSize;

    private int encoding;
    private int channelConfig;

    public AudioFormat(int sampleRate, int channels, int sampleSizeInBits) {
        this.sampleRate = sampleRate;
        this.channels = channels;
        this.sampleSizeInBits = sampleSizeInBits;
        frameSize = ((sampleSizeInBits + 7) / 8) * channels;

        if (sampleSizeInBits == 8) {
            encoding = android.media.AudioFormat.ENCODING_PCM_8BIT;
        } else if (sampleSizeInBits == 16) {
            encoding = android.media.AudioFormat.ENCODING_PCM_16BIT;
        } else {
            encoding = android.media.AudioFormat.ENCODING_INVALID;
        }

        if (channels == 1) {
            channelConfig = android.media.AudioFormat.CHANNEL_CONFIGURATION_MONO;
        } else if (channels == 2) {
            channelConfig = android.media.AudioFormat.CHANNEL_CONFIGURATION_STEREO;
        } else {
            channelConfig = android.media.AudioFormat.CHANNEL_CONFIGURATION_INVALID;
        }
    }

    public int getSampleRate() {
        return sampleRate;
    }

    public int getChannels() {
        return channels;
    }

    public int getEncoding() {
        return encoding;
    }

    public int getSampleSizeInBits() {
        return sampleSizeInBits;
    }

    public int getChannelConfig() {
        return channelConfig;
    }

    public int getFrameSize() {
        return frameSize;
    }
}
