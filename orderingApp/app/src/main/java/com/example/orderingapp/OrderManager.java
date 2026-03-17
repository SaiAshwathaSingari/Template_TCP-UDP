package com.example.orderingapp;

import java.util.ArrayList;
import java.util.List;

public class OrderManager {
    private static OrderManager instance;
    private List<Order> orders;

    private OrderManager() {
        orders = new ArrayList<>();
    }

    public static synchronized OrderManager getInstance() {
        if (instance == null) {
            instance = new OrderManager();
        }
        return instance;
    }

    public void addOrder(Order order) {
        orders.add(0, order);
    }

    public List<Order> getOrders() {
        return orders;
    }
}