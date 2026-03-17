package com.example.orderingapp;

import java.io.Serializable;
import java.util.Map;

public class Order implements Serializable {
    private String orderId;
    private Map<Item, Integer> items;
    private double totalAmount;
    private String customerName;
    private String customerAddress;
    private long timestamp;

    public Order(String orderId, Map<Item, Integer> items, double totalAmount, String customerName, String customerAddress) {
        this.orderId = orderId;
        this.items = items;
        this.totalAmount = totalAmount;
        this.customerName = customerName;
        this.customerAddress = customerAddress;
        this.timestamp = System.currentTimeMillis();
    }

    public String getOrderId() { return orderId; }
    public Map<Item, Integer> getItems() { return items; }
    public double getTotalAmount() { return totalAmount; }
    public String getCustomerName() { return customerName; }
    public String getCustomerAddress() { return customerAddress; }
    public long getTimestamp() { return timestamp; }
}