package com.example.moviebookingsystem;

import android.content.Intent;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.ListView;
import android.widget.Toast;

import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;

import com.google.android.material.textfield.TextInputEditText;

import java.util.List;

public class MovieListActivity extends AppCompatActivity {

    private MovieAdapter adapter;
    private List<Movie> movieList;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_movie_list);

        if (getSupportActionBar() != null) {
            getSupportActionBar().setTitle("Select Movie");
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
        }

        ListView movieListView = findViewById(R.id.movieListView);
        Button btnAddMovie = findViewById(R.id.btnAddMovie);

        movieList = DataRepository.getMovies();
        
        // Initialize adapter with delete listener
        adapter = new MovieAdapter(this, movieList, position -> {
            new AlertDialog.Builder(MovieListActivity.this)
                    .setTitle("Remove Movie")
                    .setMessage("Are you sure you want to remove '" + movieList.get(position).getTitle() + "'?")
                    .setPositiveButton("Remove", (dialog, which) -> {
                        DataRepository.removeMovie(position);
                        adapter.notifyDataSetChanged();
                        Toast.makeText(this, "Movie removed", Toast.LENGTH_SHORT).show();
                    })
                    .setNegativeButton("Cancel", null)
                    .show();
        });
        
        movieListView.setAdapter(adapter);

        btnAddMovie.setOnClickListener(v -> showAddMovieDialog());

        movieListView.setOnItemClickListener((parent, view, position, id) -> {
            Movie selectedMovie = movieList.get(position);
            if (selectedMovie.getAvailableTickets() <= 0) {
                Toast.makeText(this, "Housefull!", Toast.LENGTH_SHORT).show();
                return;
            }
            Intent intent = new Intent(MovieListActivity.this, BookingFormActivity.class);
            intent.putExtra("movie_index", position);
            intent.putExtra("movie_name", selectedMovie.getTitle());
            startActivity(intent);
        });
    }

    private void showAddMovieDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        View dialogView = getLayoutInflater().inflate(R.layout.dialog_add_movie, null);
        builder.setView(dialogView);
        
        TextInputEditText etTitle = dialogView.findViewById(R.id.etAddTitle);
        TextInputEditText etGenre = dialogView.findViewById(R.id.etAddGenre);
        TextInputEditText etTickets = dialogView.findViewById(R.id.etAddTickets);

        builder.setTitle("Add Movie Details")
                .setPositiveButton("Add", (dialog, which) -> {
                    String title = etTitle.getText().toString().trim();
                    String genre = etGenre.getText().toString().trim();
                    String ticketsStr = etTickets.getText().toString().trim();

                    if (title.isEmpty() || genre.isEmpty() || ticketsStr.isEmpty()) {
                        Toast.makeText(this, "All fields are required", Toast.LENGTH_SHORT).show();
                    } else {
                        int tickets = Integer.parseInt(ticketsStr);
                        DataRepository.addMovie(new Movie(title, genre, tickets));
                        adapter.notifyDataSetChanged();
                        Toast.makeText(this, "Movie added successfully", Toast.LENGTH_SHORT).show();
                    }
                })
                .setNegativeButton("Cancel", null)
                .show();
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }

    @Override
    protected void onResume() {
        super.onResume();
        if (adapter != null) {
            adapter.notifyDataSetChanged();
        }
    }
}
