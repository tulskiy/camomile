package com.tulskiy.camomile;

import android.test.ActivityInstrumentationTestCase2;
import com.tulskiy.camomile.audio.formats.mp3.MP3Decoder;

import java.io.File;

/**
 * This is a simple framework for a test of an Application.  See
 * {@link android.test.ApplicationTestCase ApplicationTestCase} for more information on
 * how to write and extend Application tests.
 * <p/>
 * To run this test, you can type:
 * adb shell am instrument -w \
 * -e class com.tulskiy.camomile.PlayerActivityTest \
 * com.tulskiy.camomile.tests/android.test.InstrumentationTestRunner
 */
public class PlayerActivityTest extends ActivityInstrumentationTestCase2<PlayerActivity> {

    public PlayerActivityTest() {
        super("com.tulskiy.camomile", PlayerActivity.class);
    }

    public void test1() {
        assertTrue(new MP3Decoder().open(new File("/sdcard/test/01.mp3")));
    }

}
