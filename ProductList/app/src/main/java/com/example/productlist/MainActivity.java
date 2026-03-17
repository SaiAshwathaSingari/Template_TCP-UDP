package com.example.productlist;

import android.content.Intent;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.ListView;
import android.widget.Spinner;
import android.widget.Toast;

import androidx.appcompat.app.AppCompatActivity;

import java.util.ArrayList;
import java.util.List;

public class MainActivity extends AppCompatActivity {

    private Spinner spinner;
    private ProductAdapter adapter;
    private List<Product> allProducts;
    private List<Product> displayedProducts;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        ListView listView = findViewById(R.id.productListView);
        spinner = findViewById(R.id.categorySpinner);

        loadProducts();
        setupSpinner();

        adapter = new ProductAdapter(this, displayedProducts);
        listView.setAdapter(adapter);

        listView.setOnItemClickListener((parent, view, position, id) -> {
            Product product = displayedProducts.get(position);
            Intent intent = new Intent(MainActivity.this, DetailActivity.class);
            intent.putExtra("product", product);
            startActivity(intent);
        });

        registerForContextMenu(listView);
    }

    private void loadProducts() {
        allProducts = new ArrayList<>();
        // ELECTRONICS
        allProducts.add(new Product("Laptop Pro", "16-inch high performance workstation", "Electronics", android.R.drawable.ic_menu_gallery));
        allProducts.add(new Product("Smartphone Z", "Latest 5G flagship phone", "Electronics", android.R.drawable.ic_menu_call));
        allProducts.add(new Product("Tablet Ultra", "12-inch OLED creative tablet", "Electronics", android.R.drawable.ic_menu_edit));
        allProducts.add(new Product("Smart Watch", "Health and fitness tracker", "Electronics", android.R.drawable.ic_lock_idle_alarm));
        allProducts.add(new Product("Headphones", "Noise canceling over-ear", "Electronics", android.R.drawable.ic_lock_silent_mode_off));
        
        // CLOTHING
        allProducts.add(new Product("Cotton T-Shirt", "100% organic cotton tee", "Clothing", android.R.drawable.ic_menu_info_details));
        allProducts.add(new Product("Slim Fit Jeans", "Classic denim slim fit", "Clothing", android.R.drawable.ic_menu_info_details));
        allProducts.add(new Product("Winter Jacket", "Heavy duty warm puffer", "Clothing", android.R.drawable.ic_menu_info_details));
        allProducts.add(new Product("Running Shoes", "Lightweight athletic footwear", "Clothing", android.R.drawable.ic_menu_directions));
        allProducts.add(new Product("Baseball Cap", "Adjustable sports cap", "Clothing", android.R.drawable.ic_menu_info_details));

        // FOOD
        allProducts.add(new Product("Margherita Pizza", "Classic tomato and mozzarella", "Food", android.R.drawable.ic_menu_view));
        allProducts.add(new Product("Beef Burger", "Gourmet burger with fries", "Food", android.R.drawable.ic_menu_view));
        allProducts.add(new Product("Garden Salad", "Fresh organic vegetables", "Food", android.R.drawable.ic_menu_view));
        allProducts.add(new Product("Chocolate Cake", "Rich dark chocolate dessert", "Food", android.R.drawable.ic_menu_view));
        allProducts.add(new Product("Pasta Alfredo", "Creamy white sauce pasta", "Food", android.R.drawable.ic_menu_view));
        allProducts.add(new Product("Iced Coffee", "Cold brewed with milk", "Food", android.R.drawable.ic_menu_view));
        
        displayedProducts = new ArrayList<>(allProducts);
    }

    private void setupSpinner() {
        String[] categories = {"All", "Electronics", "Clothing", "Food"};
        ArrayAdapter<String> spinnerAdapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, categories);
        spinnerAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        spinner.setAdapter(spinnerAdapter);

        spinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
            @Override
            public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
                filterProducts(categories[position]);
            }

            @Override
            public void onNothingSelected(AdapterView<?> parent) {}
        });
    }

    private void filterProducts(String category) {
        displayedProducts.clear();
        if (category.equals("All")) {
            displayedProducts.addAll(allProducts);
        } else {
            for (Product p : allProducts) {
                if (p.getCategory().equalsIgnoreCase(category)) {
                    displayedProducts.add(p);
                }
            }
        }
        adapter.notifyDataSetChanged();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        if (item.getItemId() == R.id.action_refresh) {
            Toast.makeText(this, "Refreshing list...", Toast.LENGTH_SHORT).show();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        getMenuInflater().inflate(R.menu.context_menu, menu);
    }

    @Override
    public boolean onContextItemSelected(MenuItem item) {
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        if (info != null) {
            if (item.getItemId() == R.id.action_edit) {
                Toast.makeText(this, "Edit: " + displayedProducts.get(info.position).getName(), Toast.LENGTH_SHORT).show();
                return true;
            } else if (item.getItemId() == R.id.action_delete) {
                Toast.makeText(this, "Delete: " + displayedProducts.get(info.position).getName(), Toast.LENGTH_SHORT).show();
                return true;
            }
        }
        return super.onContextItemSelected(item);
    }
}
