package com.example.orderingapp;

import android.os.Bundle;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;
import androidx.appcompat.app.AppCompatActivity;

public class ItemDetailActivity extends AppCompatActivity {
    private Item selectedItem;
    private int quantity = 1;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_item_detail);

        selectedItem = (Item) getIntent().getSerializableExtra("selected_item");

        if (selectedItem == null) {
            Toast.makeText(this, "Error loading item", Toast.LENGTH_SHORT).show();
            finish();
            return;
        }

        ImageView detailImage = findViewById(R.id.detailImage);
        TextView detailName = findViewById(R.id.detailName);
        TextView detailPrice = findViewById(R.id.detailPrice);
        TextView detailDescription = findViewById(R.id.detailDescription);
        TextView tvQuantity = findViewById(R.id.tvQuantity);
        Button btnIncrease = findViewById(R.id.btnIncrease);
        Button btnDecrease = findViewById(R.id.btnDecrease);
        TextView tvTotalPrice = findViewById(R.id.tvTotalPrice);
        Button btnAddToCart = findViewById(R.id.btnAddToCart);

        detailImage.setImageResource(selectedItem.getImageResource());
        detailName.setText(selectedItem.getName());
        detailPrice.setText(String.format("₹%.2f", selectedItem.getPrice()));
        detailDescription.setText(selectedItem.getDescription());
        updateTotalPrice(tvTotalPrice);

        btnIncrease.setOnClickListener(v -> {
            quantity++;
            tvQuantity.setText(String.valueOf(quantity));
            updateTotalPrice(tvTotalPrice);
        });

        btnDecrease.setOnClickListener(v -> {
            if (quantity > 1) {
                quantity--;
                tvQuantity.setText(String.valueOf(quantity));
                updateTotalPrice(tvTotalPrice);
            }
        });

        btnAddToCart.setOnClickListener(v -> {
            CartManager.getInstance().addToCart(selectedItem, quantity);
            Toast.makeText(this, "Added to cart", Toast.LENGTH_SHORT).show();
            finish();
        });
    }

    private void updateTotalPrice(TextView tvTotalPrice) {
        double total = selectedItem.getPrice() * quantity;
        tvTotalPrice.setText(String.format("Total: ₹%.2f", total));
    }
}