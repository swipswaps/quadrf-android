package com.quadrf.usbhost.ui;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;

public class FrequencyControl extends LinearLayout {
    private TextView frequencyDisplay;
    private SeekBar frequencySlider;
    private long frequencyHz = 100000000; // 100 MHz default
    private FrequencyChangeListener listener;

    public interface FrequencyChangeListener {
        void onFrequencyChanged(long frequencyHz);
    }

    public FrequencyControl(Context context) {
        super(context);
        init();
    }

    public FrequencyControl(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        setOrientation(VERTICAL);
        // This would be inflated from a layout file in a real implementation
        frequencyDisplay = new TextView(getContext());
        frequencyDisplay.setText("100.000 MHz");
        addView(frequencyDisplay);

        frequencySlider = new SeekBar(getContext());
        frequencySlider.setMax(1000000); // 0 to 1000 MHz in kHz steps
        frequencySlider.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    frequencyHz = (long) progress * 1000; // convert kHz to Hz
                    updateDisplay();
                    if (listener != null) {
                        listener.onFrequencyChanged(frequencyHz);
                    }
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {}

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {}
        });
        addView(frequencySlider);
    }

    public void setFrequency(long frequencyHz) {
        this.frequencyHz = frequencyHz;
        frequencySlider.setProgress((int)(frequencyHz / 1000));
        updateDisplay();
    }

    public long getFrequency() {
        return frequencyHz;
    }

    public void setFrequencyChangeListener(FrequencyChangeListener listener) {
        this.listener = listener;
    }

    private void updateDisplay() {
        frequencyDisplay.setText(String.format("%.3f MHz", frequencyHz / 1e6));
    }
}
