package com.example.booking_pattern;

import java.io.Serializable;

public class BookingRecord implements Serializable {
    private String name;
    private int age;
    private String email;
    private String category;
    private String date;
    private String time;
    private String type;
    private String notification;

    public BookingRecord(String name, int age, String email, String category, String date, String time, String type, String notification) {
        this.name = name;
        this.age = age;
        this.email = email;
        this.category = category;
        this.date = date;
        this.time = time;
        this.type = type;
        this.notification = notification;
    }

    @Override
    public String toString() {
        return name + " (" + type + ")\n" + date + " at " + time + " [" + category + "]";
    }

    public String getName() { return name; }
    public int getAge() { return age; }
    public String getEmail() { return email; }
    public String getCategory() { return category; }
    public String getDate() { return date; }
    public String getTime() { return time; }
    public String getType() { return type; }
    public String getNotification() { return notification; }
}
