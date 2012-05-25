package com.tulskiy.camomile;

import android.app.Activity;
import android.content.Intent;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import com.j256.ormlite.android.apptools.OpenHelperManager;
import com.j256.ormlite.dao.RuntimeExceptionDao;
import com.j256.ormlite.table.TableUtils;
import com.tulskiy.camomile.audio.AudioFormat;
import com.tulskiy.camomile.audio.Decoder;
import com.tulskiy.camomile.audio.formats.ffmpeg.FFMPEGDecoder;
import com.tulskiy.camomile.audio.formats.flac.FLACDecoder;
import com.tulskiy.camomile.audio.formats.mp3.MP3Decoder;
import com.tulskiy.camomile.audio.formats.mp3.MP3FileReader;
import com.tulskiy.camomile.audio.formats.ogg.VorbisDecoder;
import com.tulskiy.camomile.audio.formats.wavpack.WavPackDecoder;
import com.tulskiy.camomile.audio.model.Track;
import com.tulskiy.camomile.db.DatabaseHelper;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.FileOutputStream;
import java.util.Arrays;
import java.util.LinkedList;
import java.util.concurrent.Callable;

public class PlayerActivity extends Activity {
    public static final Logger log = LoggerFactory.getLogger("camomile");


    private DatabaseHelper databaseHelper = null;

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (databaseHelper != null) {
            OpenHelperManager.releaseHelper();
            databaseHelper = null;
        }
    }

    private DatabaseHelper getDBHelper() {
        if (databaseHelper == null) {
            databaseHelper =
                    OpenHelperManager.getHelper(this, DatabaseHelper.class);
        }
        return databaseHelper;
    }

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
                        Decoder decoder = new FFMPEGDecoder();
//                        FileOutputStream fos = new FileOutputStream("/sdcard/output.wav");
//                        fos.write(new byte[44]);
                        long time = System.currentTimeMillis();
                        int count = 0;
                        if (decoder.open(new File("/sdcard/test/Rolling In The Deep.flac"))) {
                            byte[] buffer = new byte[65536];
                            while (true) {
                                int length = decoder.decode(buffer);
                                if (length < 0) {
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
                    final LinkedList<Track> datas = new LinkedList<Track>();
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
                    time = System.currentTimeMillis();
                    TableUtils.clearTable(getDBHelper().getConnectionSource(), Track.class);

                    final RuntimeExceptionDao<Track, Integer> dao = getDBHelper().getRuntimeExceptionDao(Track.class);
                    dao.callBatchTasks(new Callable<Object>() {
                        public Object call() throws Exception {
                            for (Track data : datas) {
                                dao.create(data);
                            }
                            return null;
                        }
                    });

                    log.info("time to insert: {}. count: {}", System.currentTimeMillis() - time, dao.countOf());
                } catch (Exception e) {
                    log.error("Error", e);
                }
            }
        });

        View list = findViewById(R.id.list);
        list.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                Intent intent = new Intent(PlayerActivity.this, TrackListActivity.class);
                startActivity(intent);
            }
        });
    }

    private static class PlayerThread implements Runnable {
        public void run() {
            try {
                Decoder decoder = new FFMPEGDecoder();
                if (decoder.open(new File("/sdcard/test/Rolling In The Deep.flac"))) {
                    AudioFormat audioFormat = decoder.getAudioFormat();
                    int bufferSize = 192000;
                    AudioTrack track = new AudioTrack(
                            AudioManager.STREAM_MUSIC,
                            audioFormat.getSampleRate(),
                            audioFormat.getChannelConfig(),
                            audioFormat.getEncoding(),
                            bufferSize, AudioTrack.MODE_STREAM);
                    track.play();
                    byte[] buffer = new byte[192000];
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
