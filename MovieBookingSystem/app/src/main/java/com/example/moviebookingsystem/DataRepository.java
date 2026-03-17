package com.example.moviebookingsystem;

import java.util.ArrayList;
import java.util.List;

public class DataRepository {
    private static List<Movie> movieList = new ArrayList<>();
    private static List<String> userBookings = new ArrayList<>();

    static {
        movieList.add(new Movie("Inception", "Sci-Fi", 50));
        movieList.add(new Movie("Interstellar", "Sci-Fi", 30));
        movieList.add(new Movie("The Dark Knight", "Action", 40));
    }

    public static List<Movie> getMovies() {
        return movieList;
    }

    public static void addMovie(Movie movie) {
        movieList.add(movie);
    }

    public static void removeMovie(int index) {
        if (index >= 0 && index < movieList.size()) {
            movieList.remove(index);
        }
    }

    public static List<String> getBookings() {
        return userBookings;
    }

    public static void addBooking(String booking) {
        userBookings.add(booking);
    }
}
