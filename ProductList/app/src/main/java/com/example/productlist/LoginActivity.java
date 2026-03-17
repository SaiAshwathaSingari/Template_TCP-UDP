package com.example.productlist;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class LoginActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_login);

        EditText username = findViewById(R.id.username);
        EditText password = findViewById(R.id.password);
        Button loginBtn = findViewById(R.id.loginBtn);

        loginBtn.setOnClickListener(v -> {
            // Trim whitespace to avoid errors
            String user = username.getText().toString().trim();
            String pass = password.getText().toString().trim();

            // Check if empty
            if (user.isEmpty() || pass.isEmpty()) {
                Toast.makeText(this, "Please fill all fields", Toast.LENGTH_SHORT).show();
                return;
            }

            // Simple demo validation (admin/1234)
            // Using equalsIgnoreCase for user for easier typing
            if (user.equalsIgnoreCase("admin") && pass.equals("1234")) {
                startActivity(new Intent(LoginActivity.this, MainActivity.class));
                finish();
            } else {
                Toast.makeText(this, "Invalid Credentials (Use admin/1234)", Toast.LENGTH_SHORT).show();
            }
        });
    }
}
