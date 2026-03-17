package com.example.moviebookingsystem;

import android.os.Bundle;
import android.view.ContextMenu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;

import java.util.List;

public class ViewBookingsActivity extends AppCompatActivity {

    private List<String> bookings;
    private ArrayAdapter<String> adapter;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_view_bookings);

        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle("My Bookings");
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        ListView bookingsListView = findViewById(R.id.bookingsListView);

        bookings = DataRepository.getBookings();

        adapter = new ArrayAdapter<>(this, android.R.layout.simple_list_item_1, bookings);
        bookingsListView.setAdapter(adapter);

        registerForContextMenu(bookingsListView);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        getMenuInflater().inflate(R.menu.context_menu, menu);
        menu.setHeaderTitle("Select Action");
    }

    @Override
    public boolean onContextItemSelected(@NonNull MenuItem item) {
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        int position = info.position;

        int id = item.getItemId();
        if (id == R.id.action_edit) {
            Toast.makeText(this, "Edit: " + bookings.get(position), Toast.LENGTH_SHORT).show();
            return true;
        } else if (id == R.id.action_cancel) {
            Toast.makeText(this, "Booking Canceled", Toast.LENGTH_SHORT).show();
            bookings.remove(position);
            adapter.notifyDataSetChanged();
            return true;
        }
        return super.onContextItemSelected(item);
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }
}
