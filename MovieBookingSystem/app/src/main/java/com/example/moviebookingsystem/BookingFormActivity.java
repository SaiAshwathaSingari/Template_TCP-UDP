package com.example.moviebookingsystem;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.PopupMenu;
import androidx.appcompat.widget.Toolbar;

import com.google.android.material.textfield.TextInputEditText;

public class BookingFormActivity extends AppCompatActivity {

    private String movieName;
    private int movieIndex;
    private String seatPreference = "No Preference";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_booking_form);

        Toolbar toolbar = findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle("Booking Details");
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        movieName = getIntent().getStringExtra("movie_name");
        movieIndex = getIntent().getIntExtra("movie_index", -1);
        TextView tvMovieName = findViewById(R.id.tvMovieName);
        tvMovieName.setText(movieName);

        TextInputEditText etName = findViewById(R.id.etName);
        TextInputEditText etTickets = findViewById(R.id.etTickets);
        RadioGroup rgSeatType = findViewById(R.id.rgSeatType);
        Button btnSeatPreference = findViewById(R.id.btnSeatPreference);
        Button btnNext = findViewById(R.id.btnNext);

        btnSeatPreference.setOnClickListener(v -> {
            PopupMenu popup = new PopupMenu(BookingFormActivity.this, btnSeatPreference);
            popup.getMenu().add("Window Seat");
            popup.getMenu().add("Aisle Seat");
            popup.getMenu().add("Middle Seat");
            popup.setOnMenuItemClickListener(item -> {
                seatPreference = item.getTitle().toString();
                btnSeatPreference.setText("Preference: " + seatPreference);
                return true;
            });
            popup.show();
        });

        btnNext.setOnClickListener(v -> {
            String name = etName.getText().toString().trim();
            String ticketsStr = etTickets.getText().toString().trim();
            int selectedSeatId = rgSeatType.getCheckedRadioButtonId();

            if (name.isEmpty()) {
                etName.setError("Name is required");
                return;
            }
            if (ticketsStr.isEmpty()) {
                etTickets.setError("Enter number of tickets");
                return;
            }

            try {
                int numRequested = Integer.parseInt(ticketsStr);
                Movie movie = DataRepository.getMovies().get(movieIndex);
                if (numRequested > movie.getAvailableTickets()) {
                    Toast.makeText(this, "Only " + movie.getAvailableTickets() + " tickets left!", Toast.LENGTH_SHORT).show();
                    return;
                }

                if (selectedSeatId == -1) {
                    Toast.makeText(this, "Select a seat category", Toast.LENGTH_SHORT).show();
                    return;
                }

                RadioButton rbSelectedSeat = findViewById(selectedSeatId);
                String seatType = rbSelectedSeat.getText().toString();

                Intent intent = new Intent(BookingFormActivity.this, ShowDetailsActivity.class);
                intent.putExtra("movie_index", movieIndex);
                intent.putExtra("movie_name", movieName);
                intent.putExtra("user_name", name);
                intent.putExtra("tickets", ticketsStr);
                intent.putExtra("seat_type", seatType);
                intent.putExtra("seat_preference", seatPreference);
                startActivity(intent);
            } catch (NumberFormatException e) {
                etTickets.setError("Invalid number");
            }
        });
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }
}
