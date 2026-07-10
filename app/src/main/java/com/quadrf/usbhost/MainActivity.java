package com.quadrf.usbhost;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.hardware.usb.UsbDevice;
import android.hardware.usb.UsbDeviceConnection;
import android.hardware.usb.UsbManager;
import android.os.Bundle;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;
import android.widget.Toast;
import android.view.View;

import java.util.HashMap;

public class MainActivity extends Activity {
    // Replace with actual Quad RF vendor/product IDs
    private static final int QUADRF_VENDOR_ID = 0x1d50;
    private static final int QUADRF_PRODUCT_ID = 0x614e;

    private static final String ACTION_USB_PERMISSION =
            "com.quadrf.usbhost.USB_PERMISSION";
    private static final String ACTION_SPECTRUM_UPDATE =
            "com.quadrf.usbhost.SPECTRUM_UPDATE";
    private static final String EXTRA_SPECTRUM = "spectrum";
    private static final String EXTRA_SIZE = "size";

    private UsbManager usbManager;
    private UsbDevice targetDevice;
    private TextView statusText;
    private Button startButton;
    private SeekBar freqSeekBar, gainSeekBar;
    private TextView freqDisplay, gainDisplay;
    private boolean isServiceRunning = false;

    private final BroadcastReceiver usbPermissionReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (ACTION_USB_PERMISSION.equals(action)) {
                synchronized (this) {
                    UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                    if (intent.getBooleanExtra(UsbManager.EXTRA_PERMISSION_GRANTED, false)) {
                        if (device != null && device.equals(targetDevice)) {
                            openDeviceAndStart();
                        }
                    } else {
                        Toast.makeText(MainActivity.this,
                                "USB permission denied", Toast.LENGTH_LONG).show();
                    }
                }
            }
        }
    };

    private final BroadcastReceiver usbDetachReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (UsbManager.ACTION_USB_DEVICE_DETACHED.equals(action)) {
                UsbDevice device = intent.getParcelableExtra(UsbManager.EXTRA_DEVICE);
                if (device != null && device.equals(targetDevice)) {
                    Toast.makeText(MainActivity.this,
                            "Quad RF disconnected", Toast.LENGTH_SHORT).show();
                    stopSdrService();
                    statusText.setText("Device disconnected");
                    startButton.setEnabled(false);
                    targetDevice = null;
                }
            }
        }
    };

    // Broadcast receiver for spectrum updates from SdrService
    private final BroadcastReceiver spectrumReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            if (ACTION_SPECTRUM_UPDATE.equals(intent.getAction())) {
                float[] spectrum = intent.getFloatArrayExtra(EXTRA_SPECTRUM);
                int size = intent.getIntExtra(EXTRA_SIZE, 0);
                if (spectrum != null && size > 0) {
                    // Update the waterfall view (placeholder – you can pass to a custom View)
                    // For now, we just show a simple text.
                    // In a real app, you would update a WaterfallView with these values.
                    // statusText.setText("Spectrum update: " + size + " bins");
                }
            }
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        statusText = findViewById(R.id.statusText);
        startButton = findViewById(R.id.startButton);
        freqSeekBar = findViewById(R.id.freqSeekBar);
        gainSeekBar = findViewById(R.id.gainSeekBar);
        freqDisplay = findViewById(R.id.freqDisplay);
        gainDisplay = findViewById(R.id.gainDisplay);

        usbManager = (UsbManager) getSystemService(Context.USB_SERVICE);

        // Register receivers
        IntentFilter permFilter = new IntentFilter(ACTION_USB_PERMISSION);
        registerReceiver(usbPermissionReceiver, permFilter);

        IntentFilter detachFilter = new IntentFilter(UsbManager.ACTION_USB_DEVICE_DETACHED);
        registerReceiver(usbDetachReceiver, detachFilter);

        IntentFilter specFilter = new IntentFilter(ACTION_SPECTRUM_UPDATE);
        registerReceiver(spectrumReceiver, specFilter);

        findAndRequestDevice();

        // Frequency slider: range 0–1000 MHz (in kHz steps)
        freqSeekBar.setMax(1000000);
        freqSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    long freqHz = (long) progress * 1000; // kHz -> Hz
                    freqDisplay.setText(String.format("%.3f MHz", freqHz / 1e6));
                    if (isServiceRunning) {
                        Intent serviceIntent = new Intent(MainActivity.this, SdrService.class);
                        serviceIntent.putExtra("frequency", freqHz);
                        startService(serviceIntent);
                    }
                }
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        freqSeekBar.setProgress(100000); // 100 MHz default

        // Gain slider: 0–40 dB
        gainSeekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    gainDisplay.setText(progress + " dB");
                    if (isServiceRunning) {
                        Intent serviceIntent = new Intent(MainActivity.this, SdrService.class);
                        serviceIntent.putExtra("gain", progress);
                        startService(serviceIntent);
                    }
                }
            }
            @Override public void onStartTrackingTouch(SeekBar seekBar) {}
            @Override public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        gainSeekBar.setProgress(20); // default 20 dB

        startButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                if (targetDevice == null) {
                    findAndRequestDevice();
                } else {
                    openDeviceAndStart();
                }
            }
        });
    }

    private void findAndRequestDevice() {
        HashMap<String, UsbDevice> deviceList = usbManager.getDeviceList();
        targetDevice = null;
        for (UsbDevice device : deviceList.values()) {
            if (device.getVendorId() == QUADRF_VENDOR_ID &&
                    device.getProductId() == QUADRF_PRODUCT_ID) {
                targetDevice = device;
                break;
            }
        }
        if (targetDevice != null) {
            statusText.setText("Found Quad RF device: " + targetDevice.getDeviceName());
            if (!usbManager.hasPermission(targetDevice)) {
                PendingIntent permissionIntent = PendingIntent.getBroadcast(this, 0,
                        new Intent(ACTION_USB_PERMISSION), PendingIntent.FLAG_IMMUTABLE);
                usbManager.requestPermission(targetDevice, permissionIntent);
            } else {
                openDeviceAndStart();
            }
        } else {
            statusText.setText("Quad RF device not connected.");
            startButton.setEnabled(false);
        }
    }

    private void openDeviceAndStart() {
        if (targetDevice == null) return;
        UsbDeviceConnection connection = usbManager.openDevice(targetDevice);
        if (connection == null) {
            Toast.makeText(this, "Failed to open USB connection", Toast.LENGTH_LONG).show();
            return;
        }
        int fileDescriptor = connection.getFileDescriptor();
        if (fileDescriptor < 0) {
            Toast.makeText(this, "Invalid file descriptor", Toast.LENGTH_LONG).show();
            return;
        }
        statusText.setText("USB open. FD = " + fileDescriptor);
        startButton.setEnabled(true);
        isServiceRunning = true;

        Intent serviceIntent = new Intent(this, SdrService.class);
        serviceIntent.putExtra("fileDescriptor", fileDescriptor);
        // Send initial frequency and gain
        serviceIntent.putExtra("frequency", (long) freqSeekBar.getProgress() * 1000);
        serviceIntent.putExtra("gain", gainSeekBar.getProgress());
        startForegroundService(serviceIntent);
    }

    private void stopSdrService() {
        if (isServiceRunning) {
            Intent serviceIntent = new Intent(this, SdrService.class);
            stopService(serviceIntent);
            isServiceRunning = false;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        unregisterReceiver(usbPermissionReceiver);
        unregisterReceiver(usbDetachReceiver);
        unregisterReceiver(spectrumReceiver);
        stopSdrService();
    }
}
