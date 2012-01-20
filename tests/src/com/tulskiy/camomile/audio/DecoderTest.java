package com.tulskiy.camomile.audio;

import android.content.res.AssetManager;
import android.test.AndroidTestCase;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;


/**
 * Author: Denis_Tulskiy
 * Date: 1/20/12
 */
public class DecoderTest extends AndroidTestCase {
    public void testMP3() throws IOException {
        AssetManager am = getContext().getAssets();

        InputStream is = am.open("testfiles/mp3/sample.mp3");

        assertNotNull(is);
    }
}
