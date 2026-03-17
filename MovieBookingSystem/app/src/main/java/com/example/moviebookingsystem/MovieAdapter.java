package com.example.moviebookingsystem;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.ImageButton;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import java.util.List;

public class MovieAdapter extends ArrayAdapter<Movie> {

    public interface OnDeleteClickListener {
        void onDelete(int position);
    }

    private OnDeleteClickListener deleteListener;

    public MovieAdapter(@NonNull Context context, @NonNull List<Movie> movies, OnDeleteClickListener listener) {
        super(context, 0, movies);
        this.deleteListener = listener;
    }

    @NonNull
    @Override
    public View getView(int position, @Nullable View convertView, @NonNull ViewGroup parent) {
        if (convertView == null) {
            convertView = LayoutInflater.from(getContext()).inflate(R.layout.item_movie, parent, false);
        }

        Movie movie = getItem(position);

        TextView tvTitle = convertView.findViewById(R.id.tvMovieTitle);
        TextView tvGenre = convertView.findViewById(R.id.tvMovieGenre);
        TextView tvAvailable = convertView.findViewById(R.id.tvAvailableTickets);
        ImageButton btnRemove = convertView.findViewById(R.id.btnRemoveMovie);

        if (movie != null) {
            tvTitle.setText(movie.getTitle());
            tvGenre.setText(movie.getGenre());
            tvAvailable.setText("Tickets: " + movie.getAvailableTickets() + " Left");
        }

        btnRemove.setOnClickListener(v -> {
            if (deleteListener != null) {
                deleteListener.onDelete(position);
            }
        });

        return convertView;
    }
}
