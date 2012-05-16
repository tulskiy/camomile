package com.tulskiy.camomile;

import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.TextView;
import com.j256.ormlite.android.apptools.OrmLiteBaseListActivity;
import com.j256.ormlite.dao.RuntimeExceptionDao;
import com.tulskiy.camomile.audio.model.Track;
import com.tulskiy.camomile.db.DatabaseHelper;

import java.util.List;

/**
 * Author: Denis_Tulskiy
 * Date: 5/16/12
 */
public class TrackListActivity extends OrmLiteBaseListActivity<DatabaseHelper> {
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        final RuntimeExceptionDao<Track, Integer> dao = getHelper().getRuntimeExceptionDao(Track.class);

        final List<Track> tracks = dao.queryForAll();
        setListAdapter(new BaseAdapter() {
            public int getCount() {
                return tracks.size();
            }

            public Object getItem(int position) {
                return tracks.get(position);
            }

            public long getItemId(int position) {
                return tracks.get(position).id;
            }

            public View getView(int position, View convertView, ViewGroup parent) {
                LayoutInflater layoutInflater = getLayoutInflater();
                View view = layoutInflater.inflate(R.layout.track_view, parent, false);
                TextView title = (TextView) view.findViewById(R.id.title);
                Track track = tracks.get(position);
                title.setText(track.title);
                TextView artist = (TextView) view.findViewById(R.id.artist);
                artist.setText(track.artist);
                TextView time = (TextView) view.findViewById(R.id.length);
                time.setText(samplesToTime(track.totalSamples, track.sampleRate));
                TextView album = (TextView) view.findViewById(R.id.album);
                album.setText(track.album);

                return view;
            }
        });
    }

    public static String formatSeconds(double seconds) {
        int min = (int) ((Math.round(seconds)) / 60);
        int hrs = min / 60;
        if (min > 0) seconds -= min * 60;
        if (seconds < 0) seconds = 0;
        if (hrs > 0) min -= hrs * 60;
        int days = hrs / 24;
        if (days > 0) hrs -= days * 24;
        int weeks = days / 7;
        if (weeks > 0) days -= weeks * 7;

        StringBuilder builder = new StringBuilder();
        if (weeks > 0) builder.append(weeks).append("wk ");
        if (days > 0) builder.append(days).append("d ");
        if (hrs > 0) builder.append(hrs).append(":");
        if (hrs > 0 && min < 10) builder.append("0");
        builder.append(min).append(":");
        int sec = (int) seconds;
        if (sec < 10) builder.append("0");
        builder.append(Math.round(sec));
        return builder.toString();
    }

    public static String samplesToTime(long samples, int sampleRate) {
        if (samples <= 0)
            return "-:--";
        double seconds = samplesToMillis(samples, sampleRate) / 1000f;
        return formatSeconds(seconds);
    }

    public static double samplesToMillis(long samples, int sampleRate) {
        return (double) samples / sampleRate * 1000;
    }
}