package com.example.productlist;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.ImageView;
import android.widget.TextView;
import java.util.List;

public class ProductAdapter extends BaseAdapter {
    private Context context;
    private List<Product> products;

    public ProductAdapter(Context context, List<Product> products) {
        this.context = context;
        this.products = products;
    }

    @Override
    public int getCount() { return products.size(); }
    @Override
    public Object getItem(int position) { return products.get(position); }
    @Override
    public long getItemId(int position) { return position; }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (convertView == null) {
            convertView = LayoutInflater.from(context).inflate(R.layout.product_item, parent, false);
        }
        Product product = products.get(position);
        ImageView img = convertView.findViewById(R.id.productImage);
        TextView name = convertView.findViewById(R.id.productName);
        TextView desc = convertView.findViewById(R.id.productShortDesc);

        img.setImageResource(product.getImageResource());
        name.setText(product.getName());
        desc.setText(product.getDescription());
        return convertView;
    }
}
