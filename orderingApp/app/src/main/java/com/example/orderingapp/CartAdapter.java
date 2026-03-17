package com.example.orderingapp;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class CartAdapter extends RecyclerView.Adapter<CartAdapter.ViewHolder> {
    private List<Item> cartItemList;
    private Map<Item, Integer> cartItemsMap;
    private Context context;
    private OnCartChangedListener listener;

    public interface OnCartChangedListener {
        void onCartChanged();
    }

    public CartAdapter(Context context, OnCartChangedListener listener) {
        this.context = context;
        this.listener = listener;
        updateData();
    }

    public void updateData() {
        this.cartItemsMap = CartManager.getInstance().getCartItems();
        this.cartItemList = new ArrayList<>(cartItemsMap.keySet());
        notifyDataSetChanged();
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.cart_item_row, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        if (position >= cartItemList.size()) return;
        
        Item item = cartItemList.get(position);
        if (item == null) return;

        Integer qtyObj = cartItemsMap.get(item);
        final int quantity = (qtyObj == null) ? 0 : qtyObj;

        holder.itemName.setText(item.getName());
        holder.itemPrice.setText(String.format("₹%.2f", item.getPrice() * quantity));
        holder.itemImage.setImageResource(item.getImageResource());
        holder.tvQuantity.setText(String.valueOf(quantity));

        holder.btnIncrease.setOnClickListener(v -> {
            CartManager.getInstance().updateQuantity(item, quantity + 1);
            updateData();
            if (listener != null) listener.onCartChanged();
        });

        holder.btnDecrease.setOnClickListener(v -> {
            if (quantity > 1) {
                CartManager.getInstance().updateQuantity(item, quantity - 1);
                updateData();
                if (listener != null) listener.onCartChanged();
            }
        });

        holder.btnRemove.setOnClickListener(v -> {
            CartManager.getInstance().removeItem(item);
            updateData();
            if (listener != null) listener.onCartChanged();
        });
    }

    @Override
    public int getItemCount() {
        return cartItemList.size();
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {
        ImageView itemImage;
        TextView itemName, itemPrice, tvQuantity;
        View btnIncrease, btnDecrease, btnRemove;

        public ViewHolder(@NonNull View itemView) {
            super(itemView);
            itemImage = itemView.findViewById(R.id.cartItemImage);
            itemName = itemView.findViewById(R.id.cartItemName);
            itemPrice = itemView.findViewById(R.id.cartItemPrice);
            tvQuantity = itemView.findViewById(R.id.tvCartQuantity);
            btnIncrease = itemView.findViewById(R.id.btnCartIncrease);
            btnDecrease = itemView.findViewById(R.id.btnCartDecrease);
            btnRemove = itemView.findViewById(R.id.btnCartRemove);
        }
    }
}