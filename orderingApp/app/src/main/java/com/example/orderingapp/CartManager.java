package com.example.orderingapp;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class CartManager {
    private static CartManager instance;
    private Map<Item, Integer> cartItems;

    private CartManager() {
        cartItems = new HashMap<>();
    }

    public static synchronized CartManager getInstance() {
        if (instance == null) {
            instance = new CartManager();
        }
        return instance;
    }

    public void addToCart(Item item, int quantity) {
        int currentQty = cartItems.containsKey(item) ? cartItems.get(item) : 0;
        cartItems.put(item, currentQty + quantity);
    }

    public void updateQuantity(Item item, int quantity) {
        if (quantity <= 0) {
            cartItems.remove(item);
        } else {
            cartItems.put(item, quantity);
        }
    }

    public void removeItem(Item item) {
        cartItems.remove(item);
    }

    public Map<Item, Integer> getCartItems() {
        return cartItems;
    }

    public double getTotalCost() {
        double total = 0;
        for (Map.Entry<Item, Integer> entry : cartItems.entrySet()) {
            total += entry.getKey().getPrice() * entry.getValue();
        }
        return total;
    }

    public void clearCart() {
        cartItems.clear();
    }
}