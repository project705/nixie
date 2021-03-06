/*
 * MIT License
 * 
 * Copyright (c) 2021 project705
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

package com.epieye.nixie;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.os.Build;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.os.StrictMode;
import android.util.DisplayMetrics;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Toast;

import kankan.wheel.widget.OnWheelChangedListener;
import kankan.wheel.widget.WheelView;
import kankan.wheel.widget.adapters.NumericWheelAdapter;

import android.app.Activity;
import android.content.Intent;


import java.io.IOException;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;

public class MainActivity extends AppCompatActivity {
    static final int numDigits = 12;

    @TargetApi(Build.VERSION_CODES.M)
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        StrictMode.ThreadPolicy policy = new StrictMode.ThreadPolicy.Builder().permitAll().build();
        StrictMode.setThreadPolicy(policy);

        setContentView(R.layout.activity_main);

        final char[] currWheelState = {'0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0', '0'};

        LinearLayout bpLayout = (LinearLayout) findViewById(R.id.llBackplate);
        assert bpLayout != null;
        bpLayout.setOnTouchListener(new OnSwipeTouchListener(this) {
            @Override
            public void onSwipeLeft() {
                Toast currToast = Toast.makeText(MainActivity.this, R.string.prev_toast, Toast.LENGTH_SHORT);
                currToast.setGravity(Gravity.TOP, 0, 0);
                currToast.show();

                String dataStr = new String(currWheelState);
                sendUDP("0," + dataStr);
            }

            @Override
            public void onSwipeRight() {
                Toast currToast = Toast.makeText(MainActivity.this, R.string.next_toast, Toast.LENGTH_SHORT);
                currToast.setGravity(Gravity.TOP, 0, 0);
                currToast.show();

                String dataStr = new String(currWheelState);
                sendUDP("1," + dataStr);
            }
        });

        LinearLayout ilayout = (LinearLayout) findViewById(R.id.llWheels);

        DisplayMetrics dm = getResources().getDisplayMetrics();
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(LinearLayout.LayoutParams.WRAP_CONTENT, LinearLayout.LayoutParams.WRAP_CONTENT);
        params.width = dm.widthPixels / numDigits;
        params.gravity = Gravity.CENTER_VERTICAL;

        WheelAdapter[] wa = new WheelAdapter[numDigits];

        for (int i = 0; i < numDigits; i++) {
            kankan.wheel.widget.WheelView wheel = new kankan.wheel.widget.WheelView(this);
            wheel.setLayoutParams(params);
            final WheelAdapter wheelAdapter = new WheelAdapter(this, i);
            wa[i] = wheelAdapter;
            wheel.setId(wheel.generateViewId());
            wheel.setViewAdapter(wheelAdapter);
            ilayout.addView(wheel);

            wheel.addChangingListener(new OnWheelChangedListener() {
                @Override
                public void onChanged(WheelView wheel, int oldValue, int newValue) {
                    final WheelAdapter wa = (WheelAdapter) wheel.getViewAdapter();

                    currWheelState[wa.getCol()] = (char)(newValue+48);
                    String dataStr = new String(currWheelState);
                    String toastStr = String.format("%d: %d-->%d ==> %s DONE", wa.getCol(), oldValue, newValue, dataStr);

                    sendUDP("2," + dataStr);
                }
            });
        }
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        getMenuInflater().inflate(R.menu.menu_nixie, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        int id = item.getItemId();
        if (id == R.id.action_settings) {
            Intent i = new Intent(this, ConfigurationActivity.class);
            startActivityForResult(i, 0);

            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (data == null) {
            return;
        }
        switch(requestCode) {
            case 0:
                Bundle extras = data.getExtras();
                String IP = extras.getString("IP");
                int port = extras.getInt("port");

                Toast.makeText(this, "Network Settings: " + IP + ":" + port, Toast.LENGTH_SHORT).show();
                break;
            default:
                return;
        }
    }

    public void sendUDP(String str) {
        final SharedPreferences settings = getSharedPreferences("nixie", 0);
        final String ipAddress = settings.getString("ipAddress", "169.254.10.10");
        final int port = settings.getInt("port", 5445);

        try {
            final DatagramSocket sock = new DatagramSocket();
            InetAddress ipAddr = InetAddress.getByName(ipAddress);

            byte[] buf = new byte[str.length()];
            buf = str.getBytes();

            DatagramPacket send_packet = new DatagramPacket(buf, buf.length, ipAddr, port);
            sock.send(send_packet);
            sock.close();
        } catch (IOException e) {
            System.err.println("Exception: " + e.getMessage());
        }
    }

    public class WheelAdapter extends NumericWheelAdapter {
        private int col;

        public WheelAdapter(Context context, int col) {
            super(context, 0, 9);
            this.col = col;

            setItemResource(R.layout.wheel_text_item);
            setItemTextResource(R.id.text);
        }

        public int getCol() {
            return this.col;
        }

        @Override
        public CharSequence getItemText(int index) {
            if (index >= 0 && index < getItemsCount()) {
                return Integer.toString(index);
            }
            return null;
        }
    }
}
