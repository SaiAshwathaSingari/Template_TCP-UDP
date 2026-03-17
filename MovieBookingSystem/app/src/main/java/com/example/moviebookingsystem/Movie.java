package com.example.moviebookingsystem;

public class Movie {
    private String title;
    private String genre;
    private int availableTickets;

    public Movie(String title, String genre, int availableTickets) {
        this.title = title;
        this.genre = genre;
        this.availableTickets = availableTickets;
    }

    public String getTitle() { return title; }
    public String getGenre() { return genre; }
    public int getAvailableTickets() { return availableTickets; }
    public void setAvailableTickets(int availableTickets) { this.availableTickets = availableTickets; }
}
