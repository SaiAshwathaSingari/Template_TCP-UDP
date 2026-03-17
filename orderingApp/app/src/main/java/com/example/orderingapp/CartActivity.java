package com.example.orderingapp;

import android.content.Intent;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.Toolbar;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

public class CartActivity extends AppCompatActivity {
    private RecyclerView recyclerView;
    private CartAdapter adapter;
    private TextView tvCartTotal;
    private Button btnCheckout;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_cart);

        // Explicitly setup toolbar if present to prevent issues with M3
        Toolbar toolbar = findViewById(R.id.toolbar);
        if (toolbar != null) {
            setSupportActionBar(toolbar);
            if (getSupportActionBar() != null) {
                getSupportActionBar().setDisplayHomeAsUpEnabled(true);
                getSupportActionBar().setTitle("Your Cart");
            }
        }

        recyclerView = findViewById(R.id.cartRecyclerView);
        tvCartTotal = findViewById(R.id.tvCartTotal);
        btnCheckout = findViewById(R.id.btnCheckout);

        if (recyclerView == null || tvCartTotal == null || btnCheckout == null) {
            Toast.makeText(this, "UI Error in Cart", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }

        updateTotal();

        adapter = new CartAdapter(this, this::updateTotal);
        recyclerView.setLayoutManager(new LinearLayoutManager(this));
        recyclerView.setAdapter(adapter);

        btnCheckout.setOnClickListener(v -> {
            if (CartManager.getInstance().getCartItems().isEmpty()) {
                Toast.makeText(this, "Cart is empty", Toast.LENGTH_SHORT).show();
            } else {
                startActivity(new Intent(CartActivity.this, CheckoutActivity.class));
            }
        });
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish();
        return true;
    }

    private void updateTotal() {
        if (tvCartTotal != null) {
            tvCartTotal.setText(String.format("₹%.2f", CartManager.getInstance().getTotalCost()));
        }
    }
}