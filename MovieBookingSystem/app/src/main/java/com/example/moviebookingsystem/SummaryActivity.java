package com.example.moviebookingsystem;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

public class SummaryActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_summary);

        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle("Confirm Booking");
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        TextView tvSummary = findViewById(R.id.tvSummary);
        TextView tvTotalAmount = findViewById(R.id.tvTotalAmount);
        Button btnConfirm = findViewById(R.id.btnConfirm);
        Button btnBack = findViewById(R.id.btnBack);

        Bundle extras = getIntent().getExtras();
        if (extras != null) {
            int movieIndex = extras.getInt("movie_index");
            String movie = extras.getString("movie_name");
            String user = extras.getString("user_name");
            String ticketsStr = extras.getString("tickets");
            String seatType = extras.getString("seat_type");
            String pref = extras.getString("seat_preference");
            String theater = extras.getString("theater");
            String date = extras.getString("date");
            String time = extras.getString("time");

            int numTickets = Integer.parseInt(ticketsStr);
            int pricePerTicket = seatType.contains("Gold") ? 300 : 150;
            int total = numTickets * pricePerTicket;

            String summaryText = "Movie: " + movie + "\n\n" +
                               "Name: " + user + "\n" +
                               "Tickets: " + numTickets + "\n" +
                               "Category: " + seatType + "\n" +
                               "Preference: " + pref + "\n\n" +
                               "Cinema: " + theater + "\n" +
                               "Date: " + date + "\n" +
                               "Time: " + time;

            tvSummary.setText(summaryText);
            tvTotalAmount.setText("Total Payable: ₹" + total);

            btnConfirm.setOnClickListener(v -> {
                // Deduct tickets
                Movie m = DataRepository.getMovies().get(movieIndex);
                m.setAvailableTickets(m.getAvailableTickets() - numTickets);
                
                // Add to bookings list
                String bookingEntry = movie + " (" + numTickets + " tix) - " + date + " " + time;
                DataRepository.addBooking(bookingEntry);

                Toast.makeText(this, "Ticket Booked Successfully!", Toast.LENGTH_LONG).show();
                Intent intent = new Intent(SummaryActivity.this, MainActivity.class);
                intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
                startActivity(intent);
            });
        }

        btnBack.setOnClickListener(v -> finish());
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }
}
