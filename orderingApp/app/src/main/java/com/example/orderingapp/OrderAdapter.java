package com.example.orderingapp;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;
import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Map;

public class OrderAdapter extends RecyclerView.Adapter<OrderAdapter.ViewHolder> {
    private List<Order> orderList;

    public OrderAdapter(List<Order> orderList) {
        this.orderList = orderList;
    }

    @NonNull
    @Override
    public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
        View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.order_item_row, parent, false);
        return new ViewHolder(view);
    }

    @Override
    public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
        Order order = orderList.get(position);
        
        SimpleDateFormat sdf = new SimpleDateFormat("dd MMM yyyy, HH:mm", Locale.getDefault());
        holder.tvOrderDate.setText("Date: " + sdf.format(new Date(order.getTimestamp())));
        holder.tvOrderId.setText("#" + order.getOrderId());
        holder.tvCustomerName.setText("Customer: " + order.getCustomerName());
        holder.tvCustomerAddress.setText("Address: " + order.getCustomerAddress());
        holder.tvOrderTotal.setText("Total: ₹" + String.format("%.2f", order.getTotalAmount()));

        StringBuilder summary = new StringBuilder("Items ordered:\n");
        for (Map.Entry<Item, Integer> entry : order.getItems().entrySet()) {
            summary.append("• ")
                   .append(entry.getKey().getName())
                   .append(" x ")
                   .append(entry.getValue())
                   .append(" (₹")
                   .append(String.format("%.2f", entry.getKey().getPrice() * entry.getValue()))
                   .append(")\n");
        }
        holder.tvOrderSummaryText.setText(summary.toString().trim());
    }

    @Override
    public int getItemCount() {
        return orderList.size();
    }

    public static class ViewHolder extends RecyclerView.ViewHolder {
        TextView tvOrderDate, tvOrderId, tvCustomerName, tvCustomerAddress, tvOrderSummaryText, tvOrderTotal;

        public ViewHolder(@NonNull View itemView) {
            super(itemView);
            tvOrderDate = itemView.findViewById(R.id.tvOrderDate);
            tvOrderId = itemView.findViewById(R.id.tvOrderId);
            tvCustomerName = itemView.findViewById(R.id.tvCustomerName);
            tvCustomerAddress = itemView.findViewById(R.id.tvCustomerAddress);
            tvOrderSummaryText = itemView.findViewById(R.id.tvOrderSummaryText);
            tvOrderTotal = itemView.findViewById(R.id.tvOrderTotal);
        }
    }
}