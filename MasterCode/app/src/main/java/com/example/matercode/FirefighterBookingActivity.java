package com.example.matercode;

import android.app.DatePickerDialog;
import android.content.Intent;
import android.os.Bundle;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.util.Calendar;

public class FirefighterBookingActivity extends AppCompatActivity {

    private String selectedDate = "";
    private Calendar calendar;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_firefighter_booking);

        EditText etLocation = findViewById(R.id.etLocation);
        RadioGroup rgEmergency = findViewById(R.id.rgEmergency);
        Spinner spPriority = findViewById(R.id.spPriority);
        Button btnPickDate = findViewById(R.id.btnPickDate);
        TextView tvSelectedDate = findViewById(R.id.tvSelectedDate);
        Button btnSubmit = findViewById(R.id.btnSubmit);

        // Spinner Setup (Lab 5)
        String[] priorities = {"Low", "Medium", "High", "Critical"};
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, priorities);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spPriority.setAdapter(adapter);

        calendar = Calendar.getInstance();

        // DatePicker Setup (Lab 5)
        btnPickDate.setOnClickListener(v -> {
            int year = calendar.get(Calendar.YEAR);
            int month = calendar.get(Calendar.MONTH);
            int day = calendar.get(Calendar.DAY_OF_MONTH);

            DatePickerDialog datePickerDialog = new DatePickerDialog(this, (view, year1, month1, dayOfMonth) -> {
                Calendar chosenDate = Calendar.getInstance();
                chosenDate.set(year1, month1, dayOfMonth);

                // Validation: Within 20 days
                long diff = chosenDate.getTimeInMillis() - calendar.getTimeInMillis();
                long days = diff / (24 * 60 * 60 * 1000);

                if (days >= 0 && days <= 20) {
                    selectedDate = dayOfMonth + "/" + (month1 + 1) + "/" + year1;
                    tvSelectedDate.setText("Selected: " + selectedDate);
                } else {
                    Toast.makeText(this, "Select a date within 20 days!", Toast.LENGTH_SHORT).show();
                }
            }, year, month, day);

            datePickerDialog.show();
        });

        btnSubmit.setOnClickListener(v -> {
            String location = etLocation.getText().toString();
            int selectedId = rgEmergency.getCheckedRadioButtonId();
            String priority = spPriority.getSelectedItem().toString();

            if (location.isEmpty() || selectedId == -1 || selectedDate.isEmpty()) {
                Toast.makeText(this, "Please fill all fields", Toast.LENGTH_SHORT).show();
                return;
            }

            String emergencyType = "";
            if (selectedId == R.id.rbSmall) emergencyType = "Small Fire";
            else if (selectedId == R.id.rbLarge) emergencyType = "Large Fire";
            else if (selectedId == R.id.rbRescue) emergencyType = "Rescue Operation";

            // Intent with Extras (Lab 4)
            Intent intent = new Intent(this, BookingSummaryActivity.class);
            intent.putExtra("service", "Firefighter");
            intent.putExtra("location", location);
            intent.putExtra("type", emergencyType);
            intent.putExtra("priority", priority);
            intent.putExtra("date", selectedDate);
            startActivity(intent);
        });
    }
}
