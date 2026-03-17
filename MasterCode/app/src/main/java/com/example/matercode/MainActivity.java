package com.example.matercode;

import android.content.Intent;
import android.os.Bundle;
import android.view.ContextMenu;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.BaseAdapter;
import android.widget.GridView;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

public class MainActivity extends AppCompatActivity {

    String[] services = {"Firefighter", "Ambulance", "Parking", "Complaint"};

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        GridView gvServices = findViewById(R.id.gvServices);
        ServiceAdapter adapter = new ServiceAdapter();
        gvServices.setAdapter(adapter);

        // Register for Context Menu (Lab 7)
        registerForContextMenu(gvServices);

        gvServices.setOnItemClickListener((parent, view, position, id) -> {
            if (position == 0) { // Firefighter
                Intent intent = new Intent(MainActivity.this, FirefighterBookingActivity.class);
                startActivity(intent);
            } else {
                Toast.makeText(MainActivity.this, services[position] + " service coming soon!", Toast.LENGTH_SHORT).show();
            }
        });
    }

    // Options Menu (Lab 6)
    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.main_menu, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.menu_settings) {
            Toast.makeText(this, "Settings clicked", Toast.LENGTH_SHORT).show();
            return true;
        } else if (id == R.id.menu_help) {
            Toast.makeText(this, "Help clicked", Toast.LENGTH_SHORT).show();
            return true;
        } else if (id == R.id.menu_logout) {
            finish();
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    // Context Menu (Lab 7)
    @Override
    public void onCreateContextMenu(ContextMenu menu, View v, ContextMenu.ContextMenuInfo menuInfo) {
        super.onCreateContextMenu(menu, v, menuInfo);
        getMenuInflater().inflate(R.menu.main_menu, menu); // Reusing menu for simplicity
        menu.setHeaderTitle("Select Action");
    }

    @Override
    public boolean onContextItemSelected(@NonNull MenuItem item) {
        AdapterView.AdapterContextMenuInfo info = (AdapterView.AdapterContextMenuInfo) item.getMenuInfo();
        if (info != null) {
            String service = services[info.position];
            Toast.makeText(this, item.getTitle() + " for " + service, Toast.LENGTH_SHORT).show();
        }
        return super.onContextItemSelected(item);
    }

    class ServiceAdapter extends BaseAdapter {
        @Override
        public int getCount() {
            return services.length;
        }

        @Override
        public Object getItem(int position) {
            return services[position];
        }

        @Override
        public long getItemId(int position) {
            return position;
        }

        @Override
        public View getView(int position, View convertView, ViewGroup parent) {
            if (convertView == null) {
                convertView = getLayoutInflater().inflate(android.R.layout.simple_list_item_1, parent, false);
            }
            TextView tv = convertView.findViewById(android.R.id.text1);
            tv.setText(services[position]);
            tv.setGravity(android.view.Gravity.CENTER);
            tv.setPadding(0, 80, 0, 80);
            tv.setBackgroundResource(android.R.drawable.btn_default);
            return convertView;
        }
    }
}
