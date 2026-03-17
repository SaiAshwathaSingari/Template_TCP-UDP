package com.example.orderingapp;

import java.io.Serializable;
import java.util.Objects;

public class Item implements Serializable {
    private String name;
    private String description;
    private double price;
    private int imageResource;
    private String category;

    public Item(String name, String description, double price, int imageResource, String category) {
        this.name = name;
        this.description = description;
        this.price = price;
        this.imageResource = imageResource;
        this.category = category;
    }

    public String getName() { return name; }
    public String getDescription() { return description; }
    public double getPrice() { return price; }
    public int getImageResource() { return imageResource; }
    public String getCategory() { return category; }

    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;
        Item item = (Item) o;
        return Double.compare(item.price, price) == 0 &&
                name.equals(item.name) &&
                category.equals(item.category);
    }

    @Override
    public int hashCode() {
        return Objects.hash(name, price, category);
    }
}