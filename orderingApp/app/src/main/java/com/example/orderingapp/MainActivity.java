package com.example.orderingapp;

import android.content.Intent;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Spinner;
import android.widget.Toast;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.appcompat.widget.SearchView;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import com.google.android.material.chip.Chip;
import com.google.android.material.chip.ChipGroup;
import com.google.android.material.floatingactionbutton.ExtendedFloatingActionButton;
import com.google.android.material.textfield.TextInputEditText;
import java.util.List;

public class MainActivity extends AppCompatActivity {
    private RecyclerView recyclerView;
    private ItemAdapter adapter;
    private List<Item> itemList;
    private SearchView searchView;
    private ChipGroup categoryChipGroup;
    private ExtendedFloatingActionButton btnViewCart;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        recyclerView = findViewById(R.id.recyclerView);
        searchView = findViewById(R.id.searchView);
        categoryChipGroup = findViewById(R.id.categoryChipGroup);
        btnViewCart = findViewById(R.id.btnViewCart);

        itemList = ItemRepository.getInstance().getAllItems();
        adapter = new ItemAdapter(this, itemList);
        recyclerView.setLayoutManager(new LinearLayoutManager(this));
        recyclerView.setAdapter(adapter);

        searchView.setOnQueryTextListener(new SearchView.OnQueryTextListener() {
            @Override
            public boolean onQueryTextSubmit(String query) {
                filterItems();
                return false;
            }
            @Override
            public boolean onQueryTextChange(String newText) {
                filterItems();
                return false;
            }
        });

        categoryChipGroup.setOnCheckedStateChangeListener((group, checkedIds) -> filterItems());

        btnViewCart.setOnClickListener(v -> startActivity(new Intent(MainActivity.this, CartActivity.class)));
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshList();
    }

    private void refreshList() {
        itemList.clear();
        itemList.addAll(ItemRepository.getInstance().getAllItems());
        adapter.updateList(itemList);
        filterItems();
    }

    private void filterItems() {
        String query = searchView.getQuery().toString();
        int checkedChipId = categoryChipGroup.getCheckedChipId();
        String category = "All";
        if (checkedChipId != View.NO_ID) {
            Chip chip = findViewById(checkedChipId);
            if (chip != null) category = chip.getText().toString();
        }
        adapter.filter(query, category);
    }

    private void showAddItemDialog() {
        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle("Add New Item to Shop");
        
        View view = LayoutInflater.from(this).inflate(R.layout.dialog_add_item, null);
        TextInputEditText etName = view.findViewById(R.id.etNewName);
        TextInputEditText etPrice = view.findViewById(R.id.etNewPrice);
        TextInputEditText etDesc = view.findViewById(R.id.etNewDesc);
        Spinner categorySpinner = view.findViewById(R.id.newCategorySpinner);

        String[] categories = {"Food", "Electronics", "Grocery"};
        ArrayAdapter<String> catAdapter = new ArrayAdapter<>(this, android.R.layout.simple_spinner_item, categories);
        catAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
        categorySpinner.setAdapter(catAdapter);

        builder.setView(view);
        builder.setPositiveButton("Add", (dialog, which) -> {
            String name = etName.getText().toString();
            String priceStr = etPrice.getText().toString();
            String desc = etDesc.getText().toString();
            String category = categorySpinner.getSelectedItem().toString();

            if (!name.isEmpty() && !priceStr.isEmpty()) {
                try {
                    double price = Double.parseDouble(priceStr);
                    Item newItem = new Item(name, desc, price, R.mipmap.ic_launcher, category);
                    ItemRepository.getInstance().addItem(newItem);
                    refreshList();
                    Toast.makeText(this, name + " added to shop!", Toast.LENGTH_SHORT).show();
                } catch (NumberFormatException e) {
                    Toast.makeText(this, "Invalid price", Toast.LENGTH_SHORT).show();
                }
            } else {
                Toast.makeText(this, "Please fill all fields", Toast.LENGTH_SHORT).show();
            }
        });
        builder.setNegativeButton("Cancel", null);
        builder.show();
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.action_clear_cart) {
            CartManager.getInstance().clearCart();
            Toast.makeText(this, "Cart cleared", Toast.LENGTH_SHORT).show();
            return true;
        } else if (id == R.id.action_add_item) {
            showAddItemDialog();
            return true;
        } else if (id == R.id.action_view_orders) {
            startActivity(new Intent(this, OrdersListActivity.class));
            return true;
        }
        return super.onOptionsItemSelected(item);
    }
}