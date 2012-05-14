package com.tulskiy.camomile;

import android.app.Activity;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import com.tulskiy.camomile.audio.AudioFormat;
import com.tulskiy.camomile.audio.Decoder;
import com.tulskiy.camomile.audio.formats.mp3.MP3FileReader;
import com.tulskiy.camomile.audio.formats.wavpack.WavPackDecoder;
import com.tulskiy.camomile.audio.model.Track;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.util.Arrays;
import java.util.LinkedList;

public class PlayerActivity extends Activity {
    public static final Logger log = LoggerFactory.getLogger("camomile");

    /**
     * Called when the activity is first created.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

        View playButton = findViewById(R.id.play);
        playButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                try {
                    Thread t = new Thread(new PlayerThread());

                    t.start();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });

        View decodeButton = findViewById(R.id.decode);
        decodeButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                try {
                    long totalTime = 0;
                    int trials = 5;
                    for (int i = 0; i < trials; i++) {
                        Decoder decoder = new WavPackDecoder();
//                        FileOutputStream fos = new FileOutputStream("/sdcard/output.wav");
//                        fos.write(new byte[44]);
                        long time = System.currentTimeMillis();
                        if (decoder.open(new File("/sdcard/Music/05 Shadow Of The Day.wv"))) {
                            byte[] buffer = new byte[65536];
                            while (true) {
                                int length = decoder.decode(buffer);
                                if (length == -1) {
                                    break;
                                }
//                                fos.write(buffer, 0, length);
                            }
                            decoder.close();
//                            fos.close();
                        }
                        long result = System.currentTimeMillis() - time;
                        totalTime += result;
                        Log.d("camomile", "time to decode: " + result);
                    }

                    Log.d("camomile", "average decode time: " + totalTime / trials);
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });

        View testButton = findViewById(R.id.scan);
        testButton.setOnClickListener(new View.OnClickListener() {
            public void onClick(View view) {
                try {
                    File root = new File("/sdcard/Music");
                    LinkedList<File> queue = new LinkedList<File>();
                    LinkedList<Track> datas = new LinkedList<Track>();
                    MP3FileReader reader = new MP3FileReader();
                    queue.push(root);
                    long time = System.currentTimeMillis();
                    while (!queue.isEmpty()) {
                        File file = queue.pop();

                        if (file.isDirectory() && !new File(file, ".nomedia").exists()) {
                            queue.addAll(Arrays.asList(file.listFiles()));
                        } else if (file.getName().endsWith(".mp3")) {
                            datas.push(reader.read(file));
                        }
                    }
                    log.info("time to scan: {}", System.currentTimeMillis() - time);
                } catch (Exception e) {
                    log.error("Error", e);
                }
            }
        });
    }

    private static class PlayerThread implements Runnable {
        public void run() {
            try {
                Decoder decoder = new WavPackDecoder();
                if (decoder.open(new File("/sdcard/Music/05 Shadow Of The Day.wv"))) {
                    AudioFormat audioFormat = decoder.getAudioFormat();
                    int minSize = 4 * AudioTrack.getMinBufferSize(
                            audioFormat.getSampleRate(),
                            audioFormat.getChannelConfig(),
                            audioFormat.getEncoding());
                    AudioTrack track = new AudioTrack(
                            AudioManager.STREAM_MUSIC,
                            audioFormat.getSampleRate(),
                            audioFormat.getChannelConfig(),
                            audioFormat.getEncoding(),
                            minSize, AudioTrack.MODE_STREAM);
                    track.play();
                    byte[] buffer = new byte[minSize];
                    int i = 0;
                    while (true) {
                        int length = decoder.decode(buffer);
                        if (length == -1) {
                            break;
                        }
                        track.write(buffer, 0, length);
                    }
                    decoder.close();
                    track.stop();
                }
            } catch (Throwable e) {
                Log.e("camomile", "eruru", e);
            }
        }
    }
}
