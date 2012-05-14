package com.tulskiy.camomile.audio.formats.mp3;

import com.tulskiy.camomile.audio.model.AudioFileReader;
import com.tulskiy.camomile.audio.model.Track;
import org.jaudiotagger.audio.mp3.MP3File;
import org.jaudiotagger.tag.FieldKey;
import org.jaudiotagger.tag.id3.AbstractID3v2Tag;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;

/**
 * Author: Denis_Tulskiy
 * Date: 5/14/12
 */
public class MP3FileReader extends AudioFileReader {
    private static final Logger log = LoggerFactory.getLogger("camomile");

    @Override
    protected Track read(Track track, File file) {
        try {
            MP3File mp3File = new MP3File(file, MP3File.LOAD_ALL, true);

            copyHeaderFields(mp3File.getMP3AudioHeader(), track);
            copyTagFields(mp3File.getTag(), track);
            return track;
        } catch (Exception e) {
            log.error("could not read tags for file: " + track.path, e);
        }
        return null;
    }
}
