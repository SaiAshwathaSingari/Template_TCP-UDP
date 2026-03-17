package com.example.productlist;

import android.os.Bundle;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

public class DetailActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_detail);

        // Enable action bar back button
        if (getSupportActionBar() != null) {
            getSupportActionBar().setDisplayHomeAsUpEnabled(true);
            getSupportActionBar().setTitle("Product Details");
        }

        Product product = (Product) getIntent().getSerializableExtra("product");

        if (product != null) {
            ImageView img = findViewById(R.id.detailImage);
            TextView name = findViewById(R.id.detailName);
            TextView category = findViewById(R.id.detailCategory);
            TextView desc = findViewById(R.id.detailDescription);
            Button favBtn = findViewById(R.id.addToFavoritesBtn);
            Button backBtn = findViewById(R.id.backBtn);

            img.setImageResource(product.getImageResource());
            name.setText(product.getName());
            category.setText("Category: " + product.getCategory());
            desc.setText(product.getDescription());

            favBtn.setOnClickListener(v -> 
                Toast.makeText(this, product.getName() + " added to favorites!", Toast.LENGTH_SHORT).show()
            );

            // Explicit UI Back Button
            backBtn.setOnClickListener(v -> finish());
        }
    }

    @Override
    public boolean onSupportNavigateUp() {
        finish(); // Go back to MainActivity
        return true;
    }
}
