package org.clanbomber.game;

import android.content.res.AssetManager;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.os.Bundle;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.widget.Button;
import android.widget.FrameLayout;
import org.libsdl.app.SDLActivity;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public class ClanBomberActivity extends SDLActivity {
    private DirectionPad directionPad;
    @Override protected String[] getLibraries() {
        return new String[] {"SDL2", "SDL2_image", "SDL2_mixer", "SDL2_ttf", "main"};
    }

    private void copyAssets(AssetManager assets, String path, File target) throws IOException {
        String[] children = assets.list(path);
        if (children != null && children.length > 0) {
            if (!target.isDirectory() && !target.mkdirs()) throw new IOException("Cannot create " + target);
            for (String child : children) copyAssets(assets, path + "/" + child, new File(target, child));
        } else {
            try (InputStream in = assets.open(path); FileOutputStream out = new FileOutputStream(target)) {
                byte[] buffer = new byte[16384]; int count;
                while ((count = in.read(buffer)) != -1) out.write(buffer, 0, count);
            }
        }
    }

    @Override protected void onCreate(Bundle state) {
        try {
            copyAssets(getAssets(), "ClanBomber2", new File(getFilesDir(), "ClanBomber2"));
        } catch (IOException error) {
            throw new IllegalStateException("Unable to prepare game assets", error);
        }
        super.onCreate(state);
        FrameLayout controls = new FrameLayout(this);
        controls.setMotionEventSplittingEnabled(true);
        mLayout.addView(controls, new android.widget.RelativeLayout.LayoutParams(-1, -1));
        int size = Math.min((int)(56 * getResources().getDisplayMetrics().density),
                            getResources().getDisplayMetrics().heightPixels / 5);
        FrameLayout.LayoutParams padLayout = new FrameLayout.LayoutParams(3*size, 3*size, Gravity.LEFT | Gravity.BOTTOM);
        directionPad = new DirectionPad(size);
        controls.addView(directionPad, padLayout);
        addControl(controls, "BOMB / OK", KeyEvent.KEYCODE_ENTER, Gravity.RIGHT | Gravity.BOTTOM, size/3, size, size + size/2);
        addControl(controls, "BACK", KeyEvent.KEYCODE_DEL, Gravity.RIGHT | Gravity.TOP, size/4, size/4, size);
        addControl(controls, "PAUSE", KeyEvent.KEYCODE_P, Gravity.LEFT | Gravity.TOP, size/4, size/4, size);
    }

    @Override protected void onPause() {
        if (!SDLActivity.mBrokenLibraries) {
            if (directionPad != null) directionPad.setKey(0);
            SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_ENTER);
            SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_DEL);
            SDLActivity.onNativeKeyUp(KeyEvent.KEYCODE_P);
        }
        super.onPause();
    }

    private final class DirectionPad extends View {
        private final int cell;
        private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        private int key = 0;
        private int pointer = -1;

        DirectionPad(int cell) { super(ClanBomberActivity.this); this.cell = cell; }

        private void setKey(int next) {
            if (key == next) return;
            if (key != 0) SDLActivity.onNativeKeyUp(key);
            key = next;
            if (key != 0) SDLActivity.onNativeKeyDown(key);
            invalidate();
        }

        private void update(float x, float y) {
            float dx = x - 1.5f * cell, dy = y - 1.5f * cell;
            if (Math.max(Math.abs(dx), Math.abs(dy)) < cell * 0.22f) setKey(0);
            else if (Math.abs(dx) > Math.abs(dy))
                setKey(dx < 0 ? KeyEvent.KEYCODE_DPAD_LEFT : KeyEvent.KEYCODE_DPAD_RIGHT);
            else setKey(dy < 0 ? KeyEvent.KEYCODE_DPAD_UP : KeyEvent.KEYCODE_DPAD_DOWN);
        }

        @Override public boolean onTouchEvent(MotionEvent event) {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    pointer = event.getPointerId(0);
                    update(event.getX(), event.getY());
                    return true;
                case MotionEvent.ACTION_MOVE:
                    int index = event.findPointerIndex(pointer);
                    if (index >= 0) update(event.getX(index), event.getY(index));
                    else setKey(0);
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    pointer = -1;
                    setKey(0);
                    return true;
                case MotionEvent.ACTION_POINTER_UP:
                    if (event.getPointerId(event.getActionIndex()) == pointer) {
                        pointer = -1;
                        setKey(0);
                    }
                    return true;
                default: return true;
            }
        }

        @Override protected void onDraw(Canvas canvas) {
            super.onDraw(canvas);
            drawDirection(canvas, 1, 0, "▲", KeyEvent.KEYCODE_DPAD_UP);
            drawDirection(canvas, 0, 1, "◀", KeyEvent.KEYCODE_DPAD_LEFT);
            drawDirection(canvas, 1, 2, "▼", KeyEvent.KEYCODE_DPAD_DOWN);
            drawDirection(canvas, 2, 1, "▶", KeyEvent.KEYCODE_DPAD_RIGHT);
        }

        private void drawDirection(Canvas canvas, int column, int row, String label, int direction) {
            float left = column * cell, top = row * cell;
            paint.setColor(key == direction ? Color.argb(210, 40, 90, 200) : Color.argb(185, 210, 210, 210));
            canvas.drawRoundRect(left + 2, top + 2, left + cell - 2, top + cell - 2, 6, 6, paint);
            paint.setColor(Color.BLACK);
            paint.setTextAlign(Paint.Align.CENTER);
            paint.setTextSize(cell * 0.32f);
            canvas.drawText(label, left + cell / 2f, top + cell / 2f - (paint.ascent() + paint.descent()) / 2f, paint);
        }
    }

    private void addControl(FrameLayout parent, String label, int key, int gravity,
                            int horizontal, int vertical, int size) {
        Button button = new Button(this);
        button.setText(label);
        button.setTextSize(12);
        button.setPadding(0, 0, 0, 0);
        button.setAllCaps(false);
        button.setAlpha(0.72f);
        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(size, size, gravity);
        if ((gravity & Gravity.RIGHT) == Gravity.RIGHT) params.rightMargin = horizontal;
        else params.leftMargin = horizontal;
        if ((gravity & Gravity.BOTTOM) == Gravity.BOTTOM) params.bottomMargin = vertical;
        else params.topMargin = vertical;
        button.setOnTouchListener((View view, MotionEvent event) -> {
            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN: SDLActivity.onNativeKeyDown(key); return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL: SDLActivity.onNativeKeyUp(key); return true;
                default: return true;
            }
        });
        parent.addView(button, params);
    }
}
