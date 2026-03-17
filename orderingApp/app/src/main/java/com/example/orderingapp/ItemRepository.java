package com.example.orderingapp;

import java.util.ArrayList;
import java.util.List;

public class ItemRepository {
    private static ItemRepository instance;
    private List<Item> itemList;

    private ItemRepository() {
        itemList = new ArrayList<>();
        initDefaultItems();
    }

    public static synchronized ItemRepository getInstance() {
        if (instance == null) {
            instance = new ItemRepository();
        }
        return instance;
    }

    private void initDefaultItems() {
        itemList.add(new Item("Classic Burger", "Juicy prime beef with secret sauce", 12.99, R.mipmap.ic_launcher, "Food"));
        itemList.add(new Item("Truffle Pizza", "Wood-fired with fresh mushrooms", 18.50, R.mipmap.ic_launcher, "Food"));
        itemList.add(new Item("Pro Headphones", "Active noise cancellation, 40h battery", 249.00, R.mipmap.ic_launcher, "Electronics"));
        itemList.add(new Item("Ultra Smartphone", "Stunning 120Hz display & 108MP camera", 899.00, R.mipmap.ic_launcher, "Electronics"));
        itemList.add(new Item("Organic Milk", "Farm fresh, 1L grass-fed whole milk", 3.99, R.mipmap.ic_launcher, "Grocery"));
    }

    public List<Item> getAllItems() {
        return new ArrayList<>(itemList);
    }

    public void addItem(Item item) {
        itemList.add(0, item);
    }
}