package com.example.booking_pattern;

import android.content.Intent;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

public class SummaryActivity extends AppCompatActivity {

    private TextView tvSummary;
    private BookingRecord currentRecord;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_summary);

        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        tvSummary = findViewById(R.id.tvSummary);
        Button btnBack = findViewById(R.id.btnBack);
        Button btnConfirm = findViewById(R.id.btnConfirm);

        registerForContextMenu(tvSummary);

        Intent intent = getIntent();
        String name = intent.getStringExtra("name");
        int age = intent.getIntExtra("age", 0);
        String email = intent.getStringExtra("email");
        String category = intent.getStringExtra("category");
        String date = intent.getStringExtra("date");
        String time = intent.getStringExtra("time");
        String type = intent.getStringExtra("type");
        String additionalOption = intent.getStringExtra("additionalOption");

        currentRecord = new BookingRecord(name, age, email, category, date, time, type, additionalOption);

        String summary = "Name: " + name + "\n" +
                "Age: " + age + "\n" +
                "Email: " + email + "\n" +
                "Category: " + category + "\n" +
                "Date: " + date + "\n" +
                "Time: " + time + "\n" +
                "Type: " + type + "\n" +
                "Notification: " + additionalOption;

        tvSummary.setText(summary);

        btnBack.setOnClickListener(v -> finish());

        btnConfirm.setOnClickListener(v -> {
            BookingDataManager.getInstance(this).addRecord(this, currentRecord);
            Toast.makeText(SummaryActivity.this, "Booking Saved to History!", Toast.LENGTH_LONG).show();
            Intent mainIntent = new Intent(SummaryActivity.this, MainActivity.class);
            mainIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
            startActivity(mainIntent);
        });
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        int id = item.getItemId();
        if (id == android.R.id.home) {
            onBackPressed();
            return true;
        } else if (id == 1) {
            Toast.makeText(this, "Sharing summary...", Toast.LENGTH_SHORT).show();
            return true;
        } else if (id == 2) {
            Toast.makeText(this, "Preparing for print...", Toast.LENGTH_SHORT).show();
            return true;
        } else if (id == 3) {
            Toast.makeText(this, "Opening settings...", Toast.LENGTH_SHORT).show();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        menu.add(0, 1, 0, "Share Summary");
        menu.add(0, 2, 1, "Print Summary");
        menu.add(0, 3, 2, "Settings");
        return true;
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        menu.setHeaderTitle("Text Options");
        menu.add(0, 1, 0, "Copy Summary");
        menu.add(0, 2, 1, "Translate");
    }

    @Override
    public boolean onContextItemSelected(@NonNull MenuItem item) {
        int id = item.getItemId();
        if (id == 1) {
            Toast.makeText(this, "Copied to clipboard", Toast.LENGTH_SHORT).show();
            return true;
        } else if (id == 2) {
            Toast.makeText(this, "Translation not available", Toast.LENGTH_SHORT).show();
            return true;
        }
        return super.onContextItemSelected(item);
    }
}
