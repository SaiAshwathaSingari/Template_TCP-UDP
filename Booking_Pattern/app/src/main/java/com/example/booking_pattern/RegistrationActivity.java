package com.example.booking_pattern;

import android.content.Intent;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.MenuItem;
import android.widget.Button;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.textfield.TextInputEditText;

import java.util.Objects;

public class RegistrationActivity extends AppCompatActivity {

    private TextInputEditText etName, etAge, etEmail;
    private RadioGroup rgCategory;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_registration);

        // Show back button in action bar
        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        etName = findViewById(R.id.etName);
        etAge = findViewById(R.id.etAge);
        etEmail = findViewById(R.id.etEmail);
        rgCategory = findViewById(R.id.rgCategory);
        Button btnNext = findViewById(R.id.btnNextToBooking);

        btnNext.setOnClickListener(v -> validateAndProceed());
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        if (item.getItemId() == android.R.id.home) {
            onBackPressed();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    private void validateAndProceed() {
        String name = etName.getText() != null ? etName.getText().toString().trim() : "";
        String ageStr = etAge.getText() != null ? etAge.getText().toString().trim() : "";
        String email = etEmail.getText() != null ? etEmail.getText().toString().trim() : "";
        int selectedId = rgCategory.getCheckedRadioButtonId();

        if (TextUtils.isEmpty(name)) {
            etName.setError("Name is required");
            etName.requestFocus();
            return;
        }

        if (TextUtils.isEmpty(ageStr)) {
            etAge.setError("Age is required");
            etAge.requestFocus();
            return;
        }

        int age;
        try {
            age = Integer.parseInt(ageStr);
        } catch (NumberFormatException e) {
            etAge.setError("Enter a valid number");
            etAge.requestFocus();
            return;
        }

        if (age < 1 || age > 120) {
            etAge.setError("Enter a valid age (1-120)");
            etAge.requestFocus();
            return;
        }

        if (TextUtils.isEmpty(email) || !android.util.Patterns.EMAIL_ADDRESS.matcher(email).matches()) {
            etEmail.setError("Valid email is required");
            etEmail.requestFocus();
            return;
        }

        if (selectedId == -1) {
            Toast.makeText(this, "Please select a category", Toast.LENGTH_SHORT).show();
            return;
        }

        RadioButton rbSelected = findViewById(selectedId);
        String category = rbSelected.getText().toString();

        Intent intent = new Intent(RegistrationActivity.this, BookingActivity.class);
        intent.putExtra("name", name);
        intent.putExtra("age", age);
        intent.putExtra("email", email);
        intent.putExtra("category", category);
        startActivity(intent);
    }
}
