package com.example.booking_pattern;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;

import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Button btnRegister = findViewById(R.id.btnRegister);
        Button btnViewRecords = findViewById(R.id.btnViewRecords);

        btnRegister.setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, RegistrationActivity.class);
            startActivity(intent);
        });

        btnViewRecords.setOnClickListener(v -> {
            Intent intent = new Intent(MainActivity.this, RecordsActivity.class);
            startActivity(intent);
        });
    }
}
