package com.tulskiy.camomile.audio.model;

import com.j256.ormlite.field.DatabaseField;

/**
 * Author: Denis_Tulskiy
 * Date: 5/4/12
 */
public class Track {
    @DatabaseField(generatedId = true)
    public int _id;

    @DatabaseField
    public String path;
    @DatabaseField
    public long totalSamples;
    @DatabaseField
    public int startPosition;
    @DatabaseField
    public int subIndex;
    @DatabaseField
    public int sampleRate;
    @DatabaseField
    public int channels;
    @DatabaseField
    public long lastModified;
    @DatabaseField
    public String codec;
    @DatabaseField
    public int bitrate;

    @DatabaseField
    public String title;
    @DatabaseField(index = true)
    public String artist;
    @DatabaseField(index = true)
    public String album;
    @DatabaseField
    public String genre;
    @DatabaseField
    public String year;
    @DatabaseField
    public String trackNo;
    @DatabaseField
    public String diskNo;

    public Track() {
    }
}
