package com.quadrf.usbhost;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioTrack;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;
import android.widget.Toast;

import androidx.core.app.NotificationCompat;

public class SdrService extends Service implements JniCallback {
    private static final String TAG = "SdrService";
    private static final String CHANNEL_ID = "SdrServiceChannel";
    private static final int NOTIFICATION_ID = 1;
    private static final String ACTION_SPECTRUM_UPDATE =
            "com.quadrf.usbhost.SPECTRUM_UPDATE";
    private static final String EXTRA_SPECTRUM = "spectrum";
    private static final String EXTRA_SIZE = "size";

    private AudioTrack audioTrack;
    private int sampleRate = 48000;
    private int audioBufferSize = 4096;

    private native void startSdrBackend(int usbFileDescriptor, JniCallback callback);
    private native void stopSdrBackend();
    private native void setFrequencyNative(long frequencyHz);
    private native void setGainNative(int gainDb);

    static {
        System.loadLibrary("quadrf_sdr_jni");
    }

    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();
        startForeground(NOTIFICATION_ID, createNotification());
        initAudioTrack();
        Log.i(TAG, "SdrService created");
    }

    private void initAudioTrack() {
        int minBufferSize = AudioTrack.getMinBufferSize(sampleRate,
                AudioFormat.CHANNEL_OUT_MONO,
                AudioFormat.ENCODING_PCM_16BIT);
        audioBufferSize = Math.max(minBufferSize * 2, 4096);
        AudioAttributes attrs = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                .build();
        AudioFormat format = new AudioFormat.Builder()
                .setEncoding(AudioFormat.ENCODING_PCM_16BIT)
                .setSampleRate(sampleRate)
                .setChannelMask(AudioFormat.CHANNEL_OUT_MONO)
                .build();
        audioTrack = new AudioTrack(attrs, format, audioBufferSize,
                AudioTrack.MODE_STREAM, AudioTrack.AUDIO_SESSION_ID_GENERATE);
        audioTrack.play();
        Log.i(TAG, "AudioTrack initialised");
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (intent != null) {
            // Handle control intents for frequency/gain changes
            if (intent.hasExtra("frequency")) {
                long freq = intent.getLongExtra("frequency", 100000000L);
                setFrequencyNative(freq);
                return START_STICKY;
            }
            if (intent.hasExtra("gain")) {
                int gain = intent.getIntExtra("gain", 20);
                setGainNative(gain);
                return START_STICKY;
            }
            if (intent.hasExtra("fileDescriptor")) {
                int fd = intent.getIntExtra("fileDescriptor", -1);
                if (fd >= 0) {
                    Log.i(TAG, "Starting SDR backend with fd=" + fd);
                    startSdrBackend(fd, this);
                }
            }
        }
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        Log.i(TAG, "Stopping SDR backend");
        stopSdrBackend();
        if (audioTrack != null) {
            audioTrack.stop();
            audioTrack.release();
            audioTrack = null;
        }
        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    // ---------- JniCallback implementation ----------
    @Override
    public void onSpectrumUpdate(final float[] spectrum, final int size) {
        // Broadcast spectrum to the activity for UI update
        Intent intent = new Intent(ACTION_SPECTRUM_UPDATE);
        intent.putExtra(EXTRA_SPECTRUM, spectrum);
        intent.putExtra(EXTRA_SIZE, size);
        sendBroadcast(intent);
    }

    @Override
    public void onAudioSamples(final short[] samples, final int count) {
        if (audioTrack != null && audioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
            audioTrack.write(samples, 0, count);
        }
    }

    @Override
    public void onNativeError(final String message) {
        Log.e(TAG, "Native error: " + message);
        runOnUiThread(() ->
            Toast.makeText(SdrService.this, "SDR Error: " + message, Toast.LENGTH_LONG).show()
        );
    }

    private void runOnUiThread(Runnable action) {
        new android.os.Handler(android.os.Looper.getMainLooper()).post(action);
    }

    // ---------- Notification helpers ----------
    private void createNotificationChannel() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            NotificationChannel channel = new NotificationChannel(
                    CHANNEL_ID,
                    "SDR Service",
                    NotificationManager.IMPORTANCE_LOW
            );
            NotificationManager manager = getSystemService(NotificationManager.class);
            if (manager != null) manager.createNotificationChannel(channel);
        }
    }

    private Notification createNotification() {
        return new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle("Quad RF SDR")
                .setContentText("Receiving IQ data...")
                .setSmallIcon(android.R.drawable.ic_menu_camera)
                .build();
    }
}
