package com.example.booking_pattern;

import android.os.Bundle;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.util.ArrayList;

public class RecordsActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_records);

        ListView listView = findViewById(R.id.listViewRecords);

        // Fetch data from Singleton using context
        ArrayList<BookingRecord> records = BookingDataManager.getInstance(this).getRecords();

        if (records.isEmpty()) {
            Toast.makeText(this, "No records found!", Toast.LENGTH_SHORT).show();
        }

        ArrayAdapter<BookingRecord> adapter = new ArrayAdapter<>(
                this, 
                android.R.layout.simple_list_item_1, 
                records
        );
        
        listView.setAdapter(adapter);

        listView.setOnItemClickListener((parent, view, position, id) -> {
            BookingRecord selected = records.get(position);
            Toast.makeText(this, "Details: " + selected.getName() + ", " + selected.getType(), Toast.LENGTH_SHORT).show();
        });
    }
}
