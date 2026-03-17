package com.example.productlist;

import java.io.Serializable;

public class Product implements Serializable {
    private String name;
    private String description;
    private String category;
    private int imageResource;

    public Product(String name, String description, String category, int imageResource) {
        this.name = name;
        this.description = description;
        this.category = category;
        this.imageResource = imageResource;
    }

    public String getName() { return name; }
    public String getDescription() { return description; }
    public String getCategory() { return category; }
    public int getImageResource() { return imageResource; }
}
