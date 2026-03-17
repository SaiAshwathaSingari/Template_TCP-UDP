package com.example.matercode;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

public class BookingSummaryActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_booking_summary);

        TextView tvSummary = findViewById(R.id.tvSummary);
        Button btnBackHome = findViewById(R.id.btnBackHome);

        Intent intent = getIntent();
        String service = intent.getStringExtra("service");
        String location = intent.getStringExtra("location");
        String type = intent.getStringExtra("type");
        String date = intent.getStringExtra("date");

        String summary = "Service: " + service + "\n" +
                         "Location: " + location + "\n" +
                         "Type: " + type + "\n" +
                         "Date: " + date;

        tvSummary.setText(summary);

        btnBackHome.setOnClickListener(v -> {
            Intent homeIntent = new Intent(this, MainActivity.class);
            homeIntent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
            startActivity(homeIntent);
        });
    }
}
