package com.tulskiy.camomile.db;

import android.content.Context;
import android.database.sqlite.SQLiteDatabase;
import com.j256.ormlite.android.apptools.OrmLiteSqliteOpenHelper;
import com.j256.ormlite.support.ConnectionSource;
import com.j256.ormlite.table.TableUtils;
import com.tulskiy.camomile.audio.model.Track;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.sql.SQLException;

/**
 * Author: Denis_Tulskiy
 * Date: 5/14/12
 */
public class DatabaseHelper extends OrmLiteSqliteOpenHelper {
    private static final Logger log = LoggerFactory.getLogger("camomile");

    private static final String DATABASE_NAME = "camomile.db";
    private static final int DATABASE_VERSION = 1;

    private Context context;

    public DatabaseHelper(Context context) {
        super(context, DATABASE_NAME, null, DATABASE_VERSION);
    }

    @Override
    public void onCreate(SQLiteDatabase database, ConnectionSource connectionSource) {
        try {
            log.info("creating database schema");
            TableUtils.createTable(connectionSource, Track.class);
        } catch (SQLException e) {
            log.error("couldn't create database", e);
            throw new RuntimeException(e);
        }
    }

    @Override
    public void onUpgrade(SQLiteDatabase database, ConnectionSource connectionSource, int oldVersion, int newVersion) {
        try {
            log.info("updating database from version {} to version {}", oldVersion, newVersion);
            TableUtils.dropTable(connectionSource, Track.class, true);
        } catch (SQLException e) {
            log.error("couldn't delete database", e);
            throw new RuntimeException(e);
        }
    }
}
