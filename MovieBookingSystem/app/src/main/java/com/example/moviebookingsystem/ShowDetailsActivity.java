package com.example.moviebookingsystem;

import android.content.Intent;
import android.os.Bundle;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.DatePicker;
import android.widget.Spinner;
import android.widget.TimePicker;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;

import java.util.Calendar;

public class ShowDetailsActivity extends AppCompatActivity {

    private String movieName, userName, tickets, seatType, seatPreference;
    private int movieIndex;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_show_details);

        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle("Show Selection");
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        Bundle extras = getIntent().getExtras();
        if (extras != null) {
            movieIndex = extras.getInt("movie_index");
            movieName = extras.getString("movie_name");
            userName = extras.getString("user_name");
            tickets = extras.getString("tickets");
            seatType = extras.getString("seat_type");
            seatPreference = extras.getString("seat_preference");
        }

        DatePicker datePicker = findViewById(R.id.datePicker);
        TimePicker timePicker = findViewById(R.id.timePicker);
        Spinner spinnerTheater = findViewById(R.id.spinnerTheater);
        Button btnReviewBooking = findViewById(R.id.btnReviewBooking);

        String[] theaters = {"PVR Cinemas: Forum Mall", "Cinepolis: Nexus", "INOX: Mantri Square", "Carnival: Rockline"};
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, theaters);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerTheater.setAdapter(adapter);

        Calendar calendar = Calendar.getInstance();
        datePicker.setMinDate(calendar.getTimeInMillis());

        btnReviewBooking.setOnClickListener(v -> {
            String date = datePicker.getDayOfMonth() + "/" + (datePicker.getMonth() + 1) + "/" + datePicker.getYear();
            int hour, minute;
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.M) {
                hour = timePicker.getHour();
                minute = timePicker.getMinute();
            } else {
                hour = timePicker.getCurrentHour();
                minute = timePicker.getCurrentMinute();
            }
            String am_pm = (hour >= 12) ? "PM" : "AM";
            int displayHour = (hour > 12) ? hour - 12 : (hour == 0 ? 12 : hour);
            String time = String.format("%02d:%02d %s", displayHour, minute, am_pm);
            String theater = spinnerTheater.getSelectedItem().toString();

            Intent intent = new Intent(ShowDetailsActivity.this, SummaryActivity.class);
            intent.putExtra("movie_index", movieIndex);
            intent.putExtra("movie_name", movieName);
            intent.putExtra("user_name", userName);
            intent.putExtra("tickets", tickets);
            intent.putExtra("seat_type", seatType);
            intent.putExtra("seat_preference", seatPreference);
            intent.putExtra("date", date);
            intent.putExtra("time", time);
            intent.putExtra("theater", theater);
            startActivity(intent);
        });
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }
}
