package com.tulskiy.camomile.audio;

import android.content.res.AssetManager;
import android.test.AndroidTestCase;
import android.test.InstrumentationTestCase;
import android.util.Log;
import com.tulskiy.camomile.audio.formats.mp3.MP3Decoder;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;


/**
 * Author: Denis_Tulskiy
 * Date: 1/20/12
 */
public class DecoderTest extends InstrumentationTestCase {
    public void testMP3() throws IOException {
        new Tester(new MP3Decoder(), "testfiles/mp3/sample.mp3", 29400).start();
    }

    class Tester {
        private Decoder decoder;
        private int totalSamples;
        private File input;
        private ByteBuffer reference;
        private AudioFormat fmt;

        public Tester(Decoder decoder, String fileName, int totalSamples) {
            this.decoder = decoder;
            this.totalSamples = totalSamples;
            input = copyInputData(decoder, fileName);
            assertNotNull("Could not copy input to temp file", input);
        }

        public void start() {
            assertTrue("Decoder returned an error", decoder.open(input));
            fmt = decoder.getAudioFormat();

            reference = decode(0);

            int[] testOffsets = new int[10];
            testOffsets[0] = 0;
            testOffsets[1] = 1;
            testOffsets[2] = totalSamples;
            testOffsets[3] = totalSamples - 1;

            for (int i = 4; i < testOffsets.length; i++) {
                testOffsets[i] = (int) (Math.random() * totalSamples);
            }

            for (int offset : testOffsets) {
                ByteBuffer buffer = decode(offset);

                reference.rewind();
                reference.position(offset * fmt.getFrameSize());

                while (reference.hasRemaining()) {
                    assertEquals(reference.get(), buffer.get());
                }
            }
        }

        private ByteBuffer decode(int offset) {
            decoder.seek(offset);

            ByteBuffer output = ByteBuffer.allocate(fmt.getFrameSize() * totalSamples + 10000);

            byte[] buf = new byte[65536];

            while (true) {
                int len = decoder.decode(buf);
                if (len == -1) {
                    break;
                }
                output.put(buf, 0, len);
            }
            assertEquals((totalSamples - offset) * fmt.getFrameSize(), output.position());
            return (ByteBuffer) output.rewind();
        }

        private File copyInputData(Decoder decoder, String fileName) {
            try {
                AssetManager am = getInstrumentation().getContext().getAssets();
                InputStream is = am.open(fileName);

                File file = File.createTempFile(decoder.getClass().getCanonicalName(), "input.dat");

                FileOutputStream fos = new FileOutputStream(file);

                byte[] buf = new byte[1024];
                while (is.available() > 0) {
                    int len = is.read(buf);
                    fos.write(buf, 0, len);
                }
                
                is.close();
                fos.close();
                return file;
            } catch (IOException e) {
                fail(e.getMessage());
            }
            return null;
        }
    }
}
