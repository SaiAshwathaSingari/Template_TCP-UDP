package com.example.myapplication;

import android.content.Intent;
import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // Redirect to the correct package
        Intent intent = new Intent(this, com.example.matercode.MainActivity.class);
        startActivity(intent);
        finish();
    }
}
