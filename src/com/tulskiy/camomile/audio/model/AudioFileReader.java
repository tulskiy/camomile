package com.tulskiy.camomile.audio.model;

import org.jaudiotagger.audio.generic.GenericAudioHeader;
import org.jaudiotagger.tag.FieldKey;
import org.jaudiotagger.tag.Tag;

import java.io.File;

/**
 * Author: Denis_Tulskiy
 * Date: 5/14/12
 */
public abstract class AudioFileReader {
    public Track read(File file) {
        Track track = new Track();
        track.path = file.getAbsolutePath();
        track.lastModified = file.lastModified();
        return read(track, file);
    }

    protected abstract Track read(Track track, File file);

    protected void copyHeaderFields(GenericAudioHeader header, Track track) {
        track.channels = header.getChannelNumber();
        track.totalSamples = header.getTotalSamples();

        track.sampleRate = header.getSampleRateAsNumber();
        track.codec = header.getFormat();
        track.bitrate = (int) header.getBitRateAsNumber();
    }

    protected void copyTagFields(Tag tag, Track track) {
        if (tag != null) {
            track.album = tag.getFirst(FieldKey.ALBUM);
            track.artist = tag.getFirst(FieldKey.ARTIST);
            track.genre = tag.getFirst(FieldKey.GENRE);
            track.title = tag.getFirst(FieldKey.TITLE);
            track.year = tag.getFirst(FieldKey.YEAR);
            track.trackNo = tag.getFirst(FieldKey.TRACK);
            track.diskNo = tag.getFirst(FieldKey.DISC_NO);
        }
    }
}
