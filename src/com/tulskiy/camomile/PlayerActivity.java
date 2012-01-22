package com.tulskiy.camomile;

import android.app.Activity;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.view.View;
import com.tulskiy.camomile.audio.Decoder;
import com.tulskiy.camomile.audio.AudioFormat;
import com.tulskiy.camomile.audio.formats.mp3.MP3Decoder;
import com.tulskiy.camomile.audio.formats.wavpack.WavPackDecoder;

import java.io.File;

public class PlayerActivity extends Activity {
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
    }

    private static class PlayerThread implements Runnable {
        public void run() {
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
        }
    }
}
