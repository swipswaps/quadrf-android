package com.quadrf.usbhost.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

/*
 * Custom View for rendering a waterfall spectrogram.
 *
 * The waterfall displays frequency on the horizontal axis and time on the
 * vertical axis, with colour representing signal strength.
 *
 * Verbatim citation from VibeSDR implementation:
 * "Custom GPU waterfall and spectrum – rendered with a Skia runtime shader, with
 * in-shader temporal line synthesis for a smooth, high-frame-rate display."
 * Source: github.com/Stuey3D/VibeSDR
 */
public class WaterfallView extends View {
    private static final int MAX_ROWS = 256;
    private static final int MAX_COLS = 512;

    private float[][] data = new float[MAX_ROWS][MAX_COLS];
    private int currentRow = 0;
    private Bitmap bitmap;
    private Paint paint = new Paint();

    public WaterfallView(Context context) {
        super(context);
        init();
    }

    public WaterfallView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init();
    }

    private void init() {
        bitmap = Bitmap.createBitmap(MAX_COLS, MAX_ROWS, Bitmap.Config.ARGB_8888);
        paint.setStyle(Paint.Style.FILL);
    }

    public void updateData(float[] spectrum) {
        if (spectrum == null || spectrum.length > MAX_COLS) return;

        // Copy spectrum data to current row
        for (int i = 0; i < spectrum.length && i < MAX_COLS; i++) {
            data[currentRow][i] = spectrum[i];
        }

        currentRow = (currentRow + 1) % MAX_ROWS;
        invalidate();
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        // Render the waterfall from the data buffer
        int rowHeight = getHeight() / MAX_ROWS;
        int colWidth = getWidth() / MAX_COLS;

        for (int row = 0; row < MAX_ROWS; row++) {
            int dataRow = (currentRow + row) % MAX_ROWS;
            for (int col = 0; col < MAX_COLS; col++) {
                float value = data[dataRow][col];
                int color = valueToColor(value);
                paint.setColor(color);
                canvas.drawRect(col * colWidth, row * rowHeight,
                        (col + 1) * colWidth, (row + 1) * rowHeight, paint);
            }
        }
    }

    private int valueToColor(float value) {
        // Map float value [0,1] to a colour gradient (blue -> cyan -> green -> yellow -> red)
        int r, g, b;
        if (value < 0.25f) {
            float t = value / 0.25f;
            r = 0;
            g = (int)(t * 255);
            b = 255;
        } else if (value < 0.5f) {
            float t = (value - 0.25f) / 0.25f;
            r = 0;
            g = 255;
            b = (int)((1 - t) * 255);
        } else if (value < 0.75f) {
            float t = (value - 0.5f) / 0.25f;
            r = (int)(t * 255);
            g = 255;
            b = 0;
        } else {
            float t = (value - 0.75f) / 0.25f;
            r = 255;
            g = (int)((1 - t) * 255);
            b = 0;
        }
        return Color.rgb(r, g, b);
    }
}
