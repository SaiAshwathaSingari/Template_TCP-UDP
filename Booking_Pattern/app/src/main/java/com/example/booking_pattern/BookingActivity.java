package com.example.booking_pattern;

import android.content.Intent;
import android.os.Bundle;
import android.view.MenuItem;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.DatePicker;
import android.widget.PopupMenu;
import android.widget.Spinner;
import android.widget.TimePicker;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import java.util.Calendar;

public class BookingActivity extends AppCompatActivity {

    private DatePicker datePicker;
    private TimePicker timePicker;
    private Spinner spinnerType;
    private String name, email, category;
    private int age;
    private String additionalOption = "None";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_booking);

        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        Intent intent = getIntent();
        name = intent.getStringExtra("name");
        email = intent.getStringExtra("email");
        category = intent.getStringExtra("category");
        age = intent.getIntExtra("age", 0);

        datePicker = findViewById(R.id.datePicker);
        timePicker = findViewById(R.id.timePicker);
        spinnerType = findViewById(R.id.spinnerType);
        Button btnShowPopup = findViewById(R.id.btnShowPopup);
        Button btnNext = findViewById(R.id.btnNextToSummary);

        String[] bookingTypes = {"General", "Priority", "VIP", "Emergency"};
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, bookingTypes);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinnerType.setAdapter(adapter);

        btnShowPopup.setOnClickListener(v -> {
            PopupMenu popup = new PopupMenu(BookingActivity.this, v);
            popup.getMenu().add("Send Confirmation SMS");
            popup.getMenu().add("Send Confirmation Email");
            popup.getMenu().add("No Notification");

            popup.setOnMenuItemClickListener(item -> {
                additionalOption = item.getTitle().toString();
                Toast.makeText(BookingActivity.this, "Selected: " + additionalOption, Toast.LENGTH_SHORT).show();
                return true;
            });
            popup.show();
        });

        Calendar today = Calendar.getInstance();
        datePicker.setMinDate(today.getTimeInMillis());

        btnNext.setOnClickListener(v -> proceedToSummary());
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            onBackPressed();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void proceedToSummary() {
        int day = datePicker.getDayOfMonth();
        int month = datePicker.getMonth() + 1;
        int year = datePicker.getYear();
        String date = day + "/" + month + "/" + year;

        int hour = timePicker.getHour();
        int minute = timePicker.getMinute();
        String time = String.format("%02d:%02d", hour, minute);

        String type = spinnerType.getSelectedItem().toString();

        Intent intent = new Intent(BookingActivity.this, SummaryActivity.class);
        intent.putExtra("name", name);
        intent.putExtra("age", age);
        intent.putExtra("email", email);
        intent.putExtra("category", category);
        intent.putExtra("date", date);
        intent.putExtra("time", time);
        intent.putExtra("type", type);
        intent.putExtra("additionalOption", additionalOption);
        startActivity(intent);
    }
}
