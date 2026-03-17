package com.example.booking_pattern;

import android.content.Context;
import android.content.SharedPreferences;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;

import java.util.ArrayList;

public class BookingDataManager {
    private static final String PREF_NAME = "BookingPrefs";
    private static final String KEY_RECORDS = "records";
    private static BookingDataManager instance;
    private ArrayList<BookingRecord> records;

    private BookingDataManager(Context context) {
        records = loadRecords(context);
    }

    public static synchronized BookingDataManager getInstance(Context context) {
        if (instance == null) {
            instance = new BookingDataManager(context.getApplicationContext());
        }
        return instance;
    }

    public void addRecord(Context context, BookingRecord record) {
        records.add(record);
        saveRecords(context);
    }

    public ArrayList<BookingRecord> getRecords() {
        return records;
    }

    private void saveRecords(Context context) {
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        SharedPreferences.Editor editor = prefs.edit();
        JSONArray jsonArray = new JSONArray();
        for (BookingRecord record : records) {
            JSONObject obj = new JSONObject();
            try {
                obj.put("name", record.getName());
                obj.put("age", record.getAge());
                obj.put("email", record.getEmail());
                obj.put("category", record.getCategory());
                obj.put("date", record.getDate());
                obj.put("time", record.getTime());
                obj.put("type", record.getType());
                obj.put("notification", record.getNotification());
                jsonArray.put(obj);
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }
        editor.putString(KEY_RECORDS, jsonArray.toString());
        editor.apply();
    }

    private ArrayList<BookingRecord> loadRecords(Context context) {
        ArrayList<BookingRecord> list = new ArrayList<>();
        SharedPreferences prefs = context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
        String json = prefs.getString(KEY_RECORDS, null);
        if (json != null) {
            try {
                JSONArray jsonArray = new JSONArray(json);
                for (int i = 0; i < jsonArray.length(); i++) {
                    JSONObject obj = jsonArray.getJSONObject(i);
                    list.add(new BookingRecord(
                            obj.getString("name"),
                            obj.getInt("age"),
                            obj.getString("email"),
                            obj.getString("category"),
                            obj.getString("date"),
                            obj.getString("time"),
                            obj.getString("type"),
                            obj.getString("notification")
                    ));
                }
            } catch (JSONException e) {
                e.printStackTrace();
            }
        }
        return list;
    }
}
