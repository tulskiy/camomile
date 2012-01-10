package com.tulskiy.camomile;

import android.app.Activity;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.view.View;
import com.tulskiy.camomile.audio.Decoder;
import com.tulskiy.camomile.audio.Format;
import com.tulskiy.camomile.audio.formats.mp3.MP3Decoder;

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
                    Thread t = new Thread(new Runnable() {
                        public void run() {
                            Decoder decoder = new MP3Decoder();
                            if (decoder.open(new File("/sdcard/Music/06. Alice.mp3"))) {
                                Format audioFormat = decoder.getAudioFormat();
                                int minSize = 4 * AudioTrack.getMinBufferSize(audioFormat.getRate(), audioFormat.getChannels(), audioFormat.getEncoding());
                                AudioTrack track = new AudioTrack(AudioManager.STREAM_MUSIC, audioFormat.getRate(),
                                        audioFormat.getChannels(), audioFormat.getEncoding(),
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
                    });

                    t.start();
                } catch (Exception e) {
                    e.printStackTrace();
                }
            }
        });
    }

}
