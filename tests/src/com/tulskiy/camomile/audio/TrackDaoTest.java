package com.tulskiy.camomile.audio;

import android.test.AndroidTestCase;
import android.test.RenamingDelegatingContext;
import com.j256.ormlite.dao.RuntimeExceptionDao;
import com.tulskiy.camomile.db.DatabaseHelper;
import com.tulskiy.camomile.audio.model.Track;

/**
 * Author: Denis_Tulskiy
 * Date: 5/14/12
 */
public class TrackDaoTest extends AndroidTestCase {

    private DatabaseHelper helper;

    @Override
    protected void setUp() throws Exception {
        RenamingDelegatingContext context
                = new RenamingDelegatingContext(getContext(), "test_");
        helper = new DatabaseHelper(context);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();

        helper.close();
    }

    public void test() {
        RuntimeExceptionDao<Track, Integer> trackDao = helper.getRuntimeExceptionDao(Track.class);

        Track track = new Track();
        track.path = "/sdcard/Music/05 Shadow Of The Day.wv";
        track.totalSamples = 124235;
        track.sampleRate = 44100;
        track.album = "album1";
        track.bitrate = 12;
        track.artist = "Linkin Park";
        track.codec = "WavPack";

        assertEquals(1, trackDao.create(track));
        assertEquals(1, trackDao.create(track));
        assertEquals(1, trackDao.create(track));

        assertEquals(3, trackDao.countOf());
    }
}
