package com.example.orderingapp;

import android.content.Intent;
import android.os.Bundle;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import java.util.HashMap;
import java.util.Map;
import java.util.UUID;

public class CheckoutActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_checkout);

        TextView tvOrderSummary = findViewById(R.id.tvOrderSummary);
        TextView tvFinalTotal = findViewById(R.id.tvFinalTotal);
        EditText etName = findViewById(R.id.etName);
        EditText etAddress = findViewById(R.id.etAddress);
        Spinner paymentSpinner = findViewById(R.id.paymentSpinner);
        Button btnConfirmOrder = findViewById(R.id.btnConfirmOrder);

        Map<Item, Integer> cartItems = new HashMap<>(CartManager.getInstance().getCartItems());
        double totalCost = CartManager.getInstance().getTotalCost();

        StringBuilder summary = new StringBuilder();
        for (Map.Entry<Item, Integer> entry : cartItems.entrySet()) {
            summary.append(entry.getKey().getName())
                    .append(" x ")
                    .append(entry.getValue())
                    .append(" (₹")
                    .append(String.format("%.2f", entry.getKey().getPrice() * entry.getValue()))
                    .append(")\n");
        }
        tvOrderSummary.setText(summary.toString());
        tvFinalTotal.setText(String.format("Total: ₹%.2f", totalCost));

        String[] methods = {"Credit Card", "UPI / GPay", "Cash on Delivery"};
        ArrayAdapter<String> adapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, methods);
        adapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        paymentSpinner.setAdapter(adapter);

        btnConfirmOrder.setOnClickListener(v -> {
            String name = etName.getText().toString().trim();
            String address = etAddress.getText().toString().trim();

            if (name.isEmpty() || address.isEmpty()) {
                Toast.makeText(this, "Please enter name and address", Toast.LENGTH_SHORT).show();
            } else {
                // Save the order
                String orderId = UUID.randomUUID().toString().substring(0, 8).toUpperCase();
                Order newOrder = new Order(orderId, cartItems, totalCost, name, address);
                OrderManager.getInstance().addOrder(newOrder);

                Toast.makeText(this, "Order #" + orderId + " placed successfully!", Toast.LENGTH_LONG).show();
                CartManager.getInstance().clearCart();
                
                finishAffinity();
                startActivity(new Intent(this, MainActivity.class));
            }
        });
    }
}